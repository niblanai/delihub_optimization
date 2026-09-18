/**
 * DeliHub Password Reset - Cloudflare Worker
 * 
 * يعمل كـ Proxy آمن بين DeliHub (C++) و Brevo API
 * يخفي API key من الكود المصدري ويوفر Rate Limiting
 * 
 * Worker URL: https://erp-password-recovery.hosamwork2003.workers.dev/
 * KV Namespace Binding: PASSWORD_RESET_KV → Delihub
 * Secret: BREVO_API_KEY
 */

// ═══════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════
const CONFIG = {
  SENDER_EMAIL: 'hosamwork2003@gmail.com',
  BREVO_API_URL: 'https://api.brevo.com/v3/smtp/email',
  OTP_LENGTH: 6,
  OTP_EXPIRY_MINUTES: 10,
  MAX_ATTEMPTS: 5,
  RATE_LIMIT_WINDOW: 60, // seconds
  RATE_LIMIT_MAX_REQUESTS: 3
};

// ═══════════════════════════════════════════════════════════════════════════
// Main Router
// ═══════════════════════════════════════════════════════════════════════════
export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);
    const path = url.pathname;

    // CORS headers
    const corsHeaders = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type'
    };

    // Handle preflight
    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: corsHeaders });
    }

    try {
      let response;

      if (path === '/request-reset' && request.method === 'POST') {
        response = await handleRequestReset(request, env);
      } else if (path === '/verify-otp' && request.method === 'POST') {
        response = await handleVerifyOTP(request, env);
      } else if (path === '/test-email' && request.method === 'GET') {
        response = await handleTestEmail(url, env);
      } else {
        response = new Response(JSON.stringify({ error: 'Not found' }), {
          status: 404,
          headers: { 'Content-Type': 'application/json' }
        });
      }

      // Add CORS headers to response
      Object.entries(corsHeaders).forEach(([key, value]) => {
        response.headers.set(key, value);
      });

      return response;
    } catch (error) {
      return new Response(JSON.stringify({ 
        success: false, 
        error: error.message 
      }), {
        status: 500,
        headers: { ...corsHeaders, 'Content-Type': 'application/json' }
      });
    }
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Handler: Request Password Reset (Send OTP)
// ═══════════════════════════════════════════════════════════════════════════
async function handleRequestReset(request, env) {
  const { email } = await request.json();

  if (!email || !isValidEmail(email)) {
    return jsonResponse({ success: false, error: 'Invalid email address' }, 400);
  }

  // Check rate limiting
  const rateLimitKey = `rate_limit:${email}`;
  const rateLimitData = await env.PASSWORD_RESET_KV.get(rateLimitKey);
  
  if (rateLimitData) {
    const { count, timestamp } = JSON.parse(rateLimitData);
    const now = Date.now();
    const elapsed = (now - timestamp) / 1000;

    if (elapsed < CONFIG.RATE_LIMIT_WINDOW && count >= CONFIG.RATE_LIMIT_MAX_REQUESTS) {
      return jsonResponse({
        success: false,
        error: `Too many requests. Please wait ${Math.ceil(CONFIG.RATE_LIMIT_WINDOW - elapsed)} seconds.`
      }, 429);
    }
  }

  // Generate OTP
  const otp = generateOTP(CONFIG.OTP_LENGTH);
  const otpHash = await hashOTP(otp);

  // Store OTP hash in KV with expiry
  const otpKey = `otp:${email}`;
  const otpData = {
    hash: otpHash,
    attempts: CONFIG.MAX_ATTEMPTS,
    createdAt: Date.now()
  };

  const expirySeconds = CONFIG.OTP_EXPIRY_MINUTES * 60;
  await env.PASSWORD_RESET_KV.put(otpKey, JSON.stringify(otpData), {
    expirationTtl: expirySeconds
  });

  // Update rate limit
  const newRateLimitData = {
    count: rateLimitData ? JSON.parse(rateLimitData).count + 1 : 1,
    timestamp: Date.now()
  };
  await env.PASSWORD_RESET_KV.put(rateLimitKey, JSON.stringify(newRateLimitData), {
    expirationTtl: CONFIG.RATE_LIMIT_WINDOW
  });

  // Send OTP via Brevo
  const emailSent = await sendOTPEmail(env.BREVO_API_KEY, email, otp);

  if (!emailSent) {
    return jsonResponse({
      success: false,
      error: 'Failed to send email. Please try again later.'
    }, 500);
  }

  return jsonResponse({
    success: true,
    message: 'OTP sent successfully',
    expiresIn: CONFIG.OTP_EXPIRY_MINUTES
  });
}

// ═══════════════════════════════════════════════════════════════════════════
// Handler: Verify OTP
// ═══════════════════════════════════════════════════════════════════════════
async function handleVerifyOTP(request, env) {
  const { email, otp } = await request.json();

  if (!email || !otp) {
    return jsonResponse({ success: false, error: 'Email and OTP are required' }, 400);
  }

  // Retrieve stored OTP data
  const otpKey = `otp:${email}`;
  const storedData = await env.PASSWORD_RESET_KV.get(otpKey);

  if (!storedData) {
    return jsonResponse({
      success: false,
      error: 'OTP expired or not found'
    }, 404);
  }

  const { hash: storedHash, attempts } = JSON.parse(storedData);

  if (attempts <= 0) {
    await env.PASSWORD_RESET_KV.delete(otpKey);
    return jsonResponse({
      success: false,
      error: 'Maximum attempts exceeded',
      remainingAttempts: 0
    }, 403);
  }

  // Verify OTP
  const enteredHash = await hashOTP(otp);
  const isValid = enteredHash === storedHash;

  if (isValid) {
    // OTP is correct - delete it
    await env.PASSWORD_RESET_KV.delete(otpKey);
    return jsonResponse({
      success: true,
      message: 'OTP verified successfully'
    });
  } else {
    // OTP is incorrect - decrement attempts
    const newAttempts = attempts - 1;
    const updatedData = JSON.parse(storedData);
    updatedData.attempts = newAttempts;

    if (newAttempts > 0) {
      await env.PASSWORD_RESET_KV.put(otpKey, JSON.stringify(updatedData), {
        expirationTtl: CONFIG.OTP_EXPIRY_MINUTES * 60
      });
    } else {
      await env.PASSWORD_RESET_KV.delete(otpKey);
    }

    return jsonResponse({
      success: false,
      error: 'Invalid OTP',
      remainingAttempts: newAttempts
    }, 401);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Handler: Test Email (Development Only)
// ═══════════════════════════════════════════════════════════════════════════
async function handleTestEmail(url, env) {
  const email = url.searchParams.get('email');

  if (!email || !isValidEmail(email)) {
    return jsonResponse({ success: false, error: 'Invalid email address' }, 400);
  }

  const testOTP = '999999';
  const emailSent = await sendOTPEmail(env.BREVO_API_KEY, email, testOTP);

  if (!emailSent) {
    return jsonResponse({
      success: false,
      error: 'Failed to send test email'
    }, 500);
  }

  return jsonResponse({
    success: true,
    message: 'Test email sent successfully',
    note: 'OTP: 999999 (test mode)'
  });
}

// ═══════════════════════════════════════════════════════════════════════════
// Utility: Send OTP Email via Brevo
// ═══════════════════════════════════════════════════════════════════════════
async function sendOTPEmail(apiKey, recipientEmail, otp) {
  const emailPayload = {
    sender: {
      name: 'DeliHub Support',
      email: CONFIG.SENDER_EMAIL
    },
    to: [
      { email: recipientEmail }
    ],
    subject: 'رمز استعادة كلمة المرور - DeliHub',
    htmlContent: `
      <!DOCTYPE html>
      <html dir="rtl" lang="ar">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <style>
          body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: #f5f5f5;
            margin: 0;
            padding: 0;
            direction: rtl;
          }
          .container {
            max-width: 600px;
            margin: 40px auto;
            background: #ffffff;
            border-radius: 12px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            overflow: hidden;
          }
          .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 30px;
            text-align: center;
            color: white;
          }
          .header h1 {
            margin: 0;
            font-size: 28px;
          }
          .content {
            padding: 40px 30px;
            text-align: center;
          }
          .otp-box {
            background: #f0f4ff;
            border: 2px dashed #667eea;
            border-radius: 8px;
            padding: 30px;
            margin: 30px 0;
            font-size: 48px;
            font-weight: bold;
            color: #667eea;
            letter-spacing: 8px;
            font-family: 'Courier New', monospace;
          }
          .info {
            color: #666;
            font-size: 14px;
            line-height: 1.6;
            margin: 20px 0;
          }
          .warning {
            background: #fff3cd;
            border: 1px solid #ffc107;
            border-radius: 8px;
            padding: 15px;
            color: #856404;
            margin: 20px 0;
            font-size: 13px;
          }
          .footer {
            background: #f8f9fa;
            padding: 20px;
            text-align: center;
            color: #666;
            font-size: 12px;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <div class="header">
            <h1>🔐 استعادة كلمة المرور</h1>
          </div>
          <div class="content">
            <p style="font-size: 18px; color: #333;">
              لقد تلقينا طلباً لإعادة تعيين كلمة المرور الخاصة بك في <strong>DeliHub</strong>
            </p>
            
            <p class="info">
              استخدم رمز التحقق التالي لإكمال العملية:
            </p>
            
            <div class="otp-box">
              ${otp}
            </div>
            
            <p class="info">
              ⏰ رمز التحقق صالح لمدة <strong>${CONFIG.OTP_EXPIRY_MINUTES} دقائق</strong> فقط
            </p>
            
            <div class="warning">
              ⚠️ إذا لم تطلب إعادة تعيين كلمة المرور، يُرجى تجاهل هذا البريد الإلكتروني.
              <br>
              لن يتم تغيير كلمة المرور الخاصة بك إلا بعد إدخال الرمز في التطبيق.
            </div>
            
            <p class="info" style="margin-top: 30px;">
              هل تواجه مشكلة؟ تواصل معنا على:
              <br>
              <a href="mailto:${CONFIG.SENDER_EMAIL}" style="color: #667eea;">
                ${CONFIG.SENDER_EMAIL}
              </a>
            </p>
          </div>
          <div class="footer">
            <p>
              © 2026 DeliHub. جميع الحقوق محفوظة.
              <br>
              هذا بريد إلكتروني تلقائي، يُرجى عدم الرد عليه مباشرة.
            </p>
          </div>
        </div>
      </body>
      </html>
    `
  };

  try {
    const response = await fetch(CONFIG.BREVO_API_URL, {
      method: 'POST',
      headers: {
        'api-key': apiKey,
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(emailPayload)
    });

    return response.ok;
  } catch (error) {
    console.error('Brevo API error:', error);
    return false;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Utility Functions
// ═══════════════════════════════════════════════════════════════════════════

function generateOTP(length) {
  let otp = '';
  for (let i = 0; i < length; i++) {
    otp += Math.floor(Math.random() * 10);
  }
  return otp;
}

async function hashOTP(otp) {
  const encoder = new TextEncoder();
  const data = encoder.encode(otp);
  const hashBuffer = await crypto.subtle.digest('SHA-256', data);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
}

function isValidEmail(email) {
  const regex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
  return regex.test(email);
}

function jsonResponse(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { 'Content-Type': 'application/json' }
  });
}

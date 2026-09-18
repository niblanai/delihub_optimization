# =============================================================================
#  DeliHub — Release Build Script
#  الاستخدام:  powershell -ExecutionPolicy Bypass -File "E:\tifany\build_release.ps1"
#
#  ما يفعله هذا الـ script بالترتيب:
#    1. يتحقق من وجود الأدوات المطلوبة
#    2. يبني الـ exe بـ ninja
#    3. يمسح dist/ القديمة ويبنيها من الصفر
#    4. يشغّل windeployqt (Qt DLLs + plugins)
#    5. ينسخ 103 DLL من MSYS2 UCRT64 (libpq, libssl, libicudt, FFmpeg, ...)
#    6. ينسخ ملفات الـ assets (intro.mp4, logo.png, config.ini, ca-certificates.crt, qt.conf)
#    7. يتحقق من صحة البناء (architecture + dependencies + compatibility fixes)
#    8. يبني الـ installer بـ Inno Setup
# =============================================================================

Set-StrictMode -Version Latest
# Note: ErrorActionPreference stays at default "Continue" so that stderr output
# from external tools (windeployqt, ninja) does not trigger PowerShell exceptions.
# Failures are detected explicitly by checking exit codes and output files.
$ErrorActionPreference = "Continue"

# ── ثوابت المسارات ───────────────────────────────────────────────────────────
$ROOT   = "E:\tifany"
$BUILD  = "$ROOT\build"
$DIST   = "$ROOT\dist"
$MSYS   = "C:\msys64\ucrt64\bin"
$ISCC   = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
$ISS    = "$ROOT\installer\DeliHub_setup.iss"
$CACERT = "$ROOT\ca-certificates.crt"

# ── دوال مساعدة ──────────────────────────────────────────────────────────────
function Step([string]$msg) {
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Cyan
    Write-Host "  $msg" -ForegroundColor Cyan
    Write-Host "================================================================" -ForegroundColor Cyan
}
function OK([string]$msg)   { Write-Host "  [OK]  $msg" -ForegroundColor Green }
function WARN([string]$msg) { Write-Host "  [!!]  $msg" -ForegroundColor Yellow }
function FAIL([string]$msg) {
    Write-Host ""
    Write-Host "  [FAILED] $msg" -ForegroundColor Red
    Write-Host ""
    exit 1
}

# =============================================================================
# STEP 1 — التحقق من الأدوات
# =============================================================================
Step "1 / 8  Validating toolchain"

if (-not (Test-Path "$MSYS\ninja.exe"))       { FAIL "ninja.exe not found in $MSYS" }
if (-not (Test-Path "$MSYS\windeployqt.exe")) { FAIL "windeployqt.exe not found in $MSYS" }
if (-not (Test-Path "$MSYS\ntldd.exe"))       { FAIL "ntldd.exe not found — install: pacman -S mingw-w64-ucrt-x86_64-ntldd" }
if (-not (Test-Path $BUILD))                  { FAIL "Build dir missing: $BUILD  (run: cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release)" }
if (-not (Test-Path $CACERT))                 { FAIL "ca-certificates.crt not found at $CACERT`n  Download from: https://curl.se/ca/cacert.pem  then save as $CACERT" }

$env:PATH = "$MSYS;$env:PATH"
OK "All tools found"

# =============================================================================
# STEP 2 — بناء الـ exe
# =============================================================================
Step "2 / 8  Compiling DeliHub"

Push-Location $BUILD
& "$MSYS\ninja.exe" DeliHub
$ninjaExit = $LASTEXITCODE
Pop-Location

if ($ninjaExit -ne 0) { FAIL "Compilation failed (ninja exit code: $ninjaExit)" }
if (-not (Test-Path "$BUILD\DeliHub.exe")) { FAIL "DeliHub.exe not found after build" }

$exe = Get-Item "$BUILD\DeliHub.exe"
OK "DeliHub.exe  —  $([math]::Round($exe.Length/1KB,0)) KB  —  $($exe.LastWriteTime)"

# =============================================================================
# STEP 3 — مسح dist/ وإنشاؤها من جديد
# =============================================================================
Step "3 / 8  Rebuilding dist/"

if (Test-Path $DIST) {
    Remove-Item -Recurse -Force $DIST
    OK "Old dist/ deleted"
}
New-Item -ItemType Directory -Path $DIST | Out-Null
Copy-Item "$BUILD\DeliHub.exe" "$DIST\DeliHub.exe"
OK "dist/ created with fresh DeliHub.exe"

# =============================================================================
# STEP 4 — تشغيل windeployqt
# =============================================================================
Step "4 / 8  Running windeployqt"

$wdqOutput = & "$MSYS\windeployqt.exe" --release --compiler-runtime "$DIST\DeliHub.exe" 2>&1
# windeployqt prints warnings to stderr which PowerShell treats as errors — ignore exit code,
# verify success by checking that the platforms plugin was actually deployed.
if (-not (Test-Path "$DIST\platforms\qwindows.dll")) {
    FAIL "windeployqt did not deploy platforms\qwindows.dll — something went wrong"
}
OK "windeployqt completed"

# =============================================================================
# STEP 5 — نسخ الـ 103 DLL من MSYS2 UCRT64
# =============================================================================
Step "5 / 8  Copying MSYS2 UCRT64 dependencies (103 DLLs)"

$msysDlls = @(
    # MinGW C++ runtime
    "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",

    # Qt Core
    "libb2-1.dll", "libdouble-conversion.dll",
    "libicudt78.dll", "libicuin78.dll", "libicuuc78.dll",
    "libpcre2-16-0.dll", "zlib1.dll", "libzstd.dll",

    # Qt Gui / font rendering
    "libfreetype-6.dll", "libharfbuzz-0.dll", "libgraphite2.dll",
    "libpng16-16.dll", "libmd4c.dll",
    "libbrotlidec.dll", "libbrotlicommon.dll", "libbz2-1.dll",

    # Qt Gui / text / i18n
    "libglib-2.0-0.dll", "libintl-8.dll", "libiconv-2.dll", "libpcre2-8-0.dll",

    # PostgreSQL client + OpenSSL 3 (Supabase SSL)
    "libpq.dll", "libssl-3-x64.dll", "libcrypto-3-x64.dll",

    # SQLite (qsqlite.dll يحتاجها)
    "libsqlite3-0.dll",

    # FFmpeg — core libraries (REQUIRED for Qt6Multimedia + notification sound)
    "avcodec-62.dll", "avformat-62.dll", "avutil-60.dll",
    "swresample-5.dll", "swscale-8.dll",

    # FFmpeg — video codecs
    "libaom.dll", "libdav1d-7.dll", "librav1e.dll", "libSvtAv1Enc-4.dll",
    "libx264-165.dll", "libx265-216.dll", "libvpx-1.dll",
    "libtheoradec-2.dll", "libtheoraenc-2.dll", "libogg-0.dll",
    "libxvidcore.dll", "xvidcore.dll",

    # FFmpeg — audio codecs
    "libopus-0.dll", "libvorbis-0.dll", "libvorbisenc-2.dll",
    "libmp3lame-0.dll", "libspeex-1.dll", "libgsm.dll",
    "libopencore-amrnb-0.dll", "libopencore-amrwb-0.dll",
    "liblc3-1.dll",

    # FFmpeg — image codecs
    "libjxl.dll", "libjxl_cms.dll", "libjxl_threads.dll",
    "libhwy.dll", "liblcms2-2.dll",
    "libwebp-7.dll", "libwebpmux-3.dll", "libsharpyuv-0.dll",
    "libopenjp2-7.dll", "libjpeg-8.dll",
    "libtiff-6.dll", "libdeflate.dll", "libjbig-0.dll", "libLerc.dll",

    # FFmpeg — container / streaming
    "libbluray-3.dll", "libgme.dll", "libmodplug-1.dll",
    "librtmp-1.dll", "libsrt.dll", "libssh.dll", "libzvbi-0.dll",

    # FFmpeg — graphics / rendering (librsvg chain)
    "libcairo-2.dll", "libcairo-gobject-2.dll", "libpixman-1-0.dll",
    "libfontconfig-1.dll", "libexpat-1.dll",
    "libpango-1.0-0.dll", "libpangocairo-1.0-0.dll",
    "libpangoft2-1.0-0.dll", "libpangowin32-1.0-0.dll",
    "libfribidi-0.dll", "libthai-0.dll", "libdatrie-1.dll",
    "librsvg-2-2.dll", "libgdk_pixbuf-2.0-0.dll",
    "libgobject-2.0-0.dll", "libgmodule-2.0-0.dll",
    "libgio-2.0-0.dll", "libglib-2.0-0.dll",
    "libffi-8.dll", "libxml2-16.dll",

    # FFmpeg — TLS / crypto for streaming
    "libgnutls-30.dll", "libgmp-10.dll", "libhogweed-6.dll",
    "libnettle-8.dll", "libp11-kit-0.dll", "libtasn1-6.dll",
    "libidn2-0.dll", "libunistring-5.dll",

    # FFmpeg — misc
    "libva.dll", "libva_win32.dll", "libvpl-2.dll",
    "libsoxr.dll", "libgomp-1.dll",
    "liblzma-5.dll", "libbrotlienc.dll",
    "libshaderc_shared.dll"
)

$copied = 0
$skipped = @()
foreach ($dll in ($msysDlls | Select-Object -Unique)) {
    $src = "$MSYS\$dll"
    if (Test-Path $src) {
        Copy-Item $src "$DIST\$dll" -Force
        $copied++
    } else {
        $skipped += $dll
    }
}

OK "Copied $copied DLLs from MSYS2 UCRT64"
if ($skipped.Count -gt 0) {
    WARN "$($skipped.Count) DLL(s) not found in MSYS2 (may be renamed in newer version):"
    $skipped | ForEach-Object { Write-Host "       $_" -ForegroundColor Yellow }
}

# =============================================================================
# STEP 6 — نسخ ملفات الـ assets
# =============================================================================
Step "6 / 8  Copying runtime assets"

# CA certificate (مطلوب لـ sslmode=verify-full مع Supabase)
Copy-Item $CACERT "$DIST\ca-certificates.crt" -Force
OK "ca-certificates.crt  ($([math]::Round((Get-Item $CACERT).Length/1KB,0)) KB)"

# intro.mp4
foreach ($try in @("$ROOT\src\intro.mp4", "$BUILD\intro.mp4")) {
    if (Test-Path $try) { Copy-Item $try "$DIST\intro.mp4" -Force; OK "intro.mp4"; break }
}
if (-not (Test-Path "$DIST\intro.mp4")) { WARN "intro.mp4 not found — video splash will be skipped" }

# logo.png
foreach ($try in @("$ROOT\src\logo.png", "$BUILD\logo.png")) {
    if (Test-Path $try) { Copy-Item $try "$DIST\logo.png" -Force; OK "logo.png"; break }
}
if (-not (Test-Path "$DIST\logo.png")) { WARN "logo.png not found" }

# logo.ico (app icon for taskbar / exe)
foreach ($try in @("$ROOT\src\logo.ico", "$BUILD\logo.ico")) {
    if (Test-Path $try) { Copy-Item $try "$DIST\logo.ico" -Force; OK "logo.ico"; break }
}
if (-not (Test-Path "$DIST\logo.ico")) { WARN "logo.ico not found (taskbar icon may fall back to PNG)" }

# config.ini (template — app seeds ProgramData on first run)
if (Test-Path "$ROOT\config.ini") {
    Copy-Item "$ROOT\config.ini" "$DIST\config.ini" -Force
    OK "config.ini"
} else { WARN "config.ini not found in $ROOT" }

# notification.mp3
foreach ($try in @("$ROOT\src\notification.mp3", "$BUILD\notification.mp3")) {
    if (Test-Path $try) { Copy-Item $try "$DIST\notification.mp3" -Force; OK "notification.mp3"; break }
}
if (-not (Test-Path "$DIST\notification.mp3")) { WARN "notification.mp3 not found — notification sound will be silent" }

# qt.conf (windeployqt يكتبه في build dir)
if (Test-Path "$BUILD\qt.conf") {
    Copy-Item "$BUILD\qt.conf" "$DIST\qt.conf" -Force
    OK "qt.conf"
} elseif (Test-Path "$DIST\qt.conf") {
    OK "qt.conf (already in dist)"
} else { WARN "qt.conf not found" }

# qsvg.dll — SVG image format plugin (required for sidebar icons)
$qsvgSrc = "C:\msys64\ucrt64\share\qt6\plugins\imageformats\qsvg.dll"
if (Test-Path $qsvgSrc) {
    New-Item -ItemType Directory -Path "$DIST\imageformats" -Force | Out-Null
    Copy-Item $qsvgSrc "$DIST\imageformats\qsvg.dll" -Force
    OK "imageformats/qsvg.dll  ($([math]::Round((Get-Item $qsvgSrc).Length/1KB,0)) KB)"
} else { WARN "qsvg.dll not found — SVG icons will not render" }

# qsvgicon.dll — SVG icon engine plugin
$qsvgIconSrc = "C:\msys64\ucrt64\share\qt6\plugins\iconengines\qsvgicon.dll"
if (Test-Path $qsvgIconSrc) {
    New-Item -ItemType Directory -Path "$DIST\iconengines" -Force | Out-Null
    Copy-Item $qsvgIconSrc "$DIST\iconengines\qsvgicon.dll" -Force
    OK "iconengines/qsvgicon.dll"
}

# xls_reader — for Excel invoice import (uses bundled Python + xlrd)
# We ship a self-contained Python runtime: python.exe + libpython + stdlib + xlrd pyz
foreach ($xlsFile in @("E:\tifany\xls_reader.pyz", "E:\tifany\xls_reader.py")) {
    if (Test-Path $xlsFile) {
        $fname = [System.IO.Path]::GetFileName($xlsFile)
        Copy-Item $xlsFile "$DIST\$fname" -Force
        OK "$fname  ($([math]::Round((Get-Item $xlsFile).Length/1KB,0)) KB)"
    }
}

# Bundled Python runtime (needed by xls_reader.pyz — no system Python required)
$pyExeSrc = "C:\msys64\ucrt64\bin\python.exe"
$pyLibSrc = "C:\msys64\ucrt64\bin\libpython3.14.dll"
foreach ($f in @($pyExeSrc, $pyLibSrc)) {
    if (Test-Path $f) {
        Copy-Item $f "$DIST\$([System.IO.Path]::GetFileName($f))" -Force
        OK "$([System.IO.Path]::GetFileName($f))  ($([math]::Round((Get-Item $f).Length/1MB,1)) MB)"
    } else { WARN "$([System.IO.Path]::GetFileName($f)) not found in MSYS2" }
}

# python._pth — tells bundled Python where its stdlib is (python3.14\)
"python3.14`npython3.14\lib-dynload`nimport site" | Set-Content "$DIST\python._pth" -Encoding ASCII
OK "python._pth"

# python3.14 stdlib — minimal set needed by xls_reader.pyz + xlrd
$msysPyLib = "C:\msys64\ucrt64\lib\python3.14"
$distPyLib  = "$DIST\python3.14"
if (Test-Path $msysPyLib) {
    # Copy essential .py modules
    $pyModules = @(
        "__future__","_collections_abc","_sitebuiltins","_weakrefset","abc","bz2","codecs",
        "contextlib","copy","copyreg","datetime","enum","fnmatch","functools","genericpath",
        "glob","io","keyword","linecache","lzma","ntpath","operator","os","posixpath","pprint",
        "reprlib","shutil","site","stat","struct","threading","traceback","types","weakref","warnings"
    )
    $pyPkgs = @("collections","compression","encodings","importlib","json","pathlib","re","zipfile")

    New-Item -ItemType Directory -Path "$distPyLib\lib-dynload" -Force | Out-Null
    foreach ($mod in $pyModules) {
        $src = "$msysPyLib\$mod.py"
        if (Test-Path $src) { Copy-Item $src "$distPyLib\$mod.py" -Force }
    }
    foreach ($pkg in $pyPkgs) {
        if (Test-Path "$msysPyLib\$pkg") {
            Remove-Item "$distPyLib\$pkg" -Recurse -Force -ErrorAction SilentlyContinue
            Copy-Item "$msysPyLib\$pkg" "$distPyLib\$pkg" -Recurse -Force
        }
    }
    # Copy ALL .pyd extension modules (needed by zipfile, struct, json, etc.)
    Get-ChildItem "$msysPyLib\lib-dynload" -Filter "*.pyd" | ForEach-Object {
        Copy-Item $_.FullName "$distPyLib\lib-dynload\$($_.Name)" -Force
    }
    $pydCount = (Get-ChildItem "$distPyLib\lib-dynload" -Filter "*.pyd").Count
    $sz = [math]::Round((Get-ChildItem $distPyLib -Recurse | Measure-Object -Property Length -Sum).Sum/1MB, 1)
    OK "python3.14 stdlib ($pydCount pyd files, ${sz}MB total)"
} else { WARN "MSYS2 Python stdlib not found — Excel import will not work" }

foreach ($tool in @("pdftotext.exe", "pdftoppm.exe")) {
    $src = "C:\msys64\ucrt64\bin\$tool"
    if (Test-Path $src) {
        Copy-Item $src "$DIST\$tool" -Force
        OK "$tool  ($([math]::Round((Get-Item $src).Length/1KB,0)) KB)"
    } else { WARN "$tool not found in MSYS2 — PDF invoice import may be limited" }
}

# magick.exe (ImageMagick — PDF image preprocessing)
$magickSrc = "C:\msys64\ucrt64\bin\magick.exe"
if (Test-Path $magickSrc) {
    Copy-Item $magickSrc "$DIST\magick.exe" -Force
    OK "magick.exe  ($([math]::Round((Get-Item $magickSrc).Length/1KB,0)) KB)"
} else { WARN "magick.exe not found — PDF image enhancement disabled" }

# tesseract.exe + Arabic language data (OCR engine)
$tessSrc = "C:\msys64\ucrt64\bin\tesseract.exe"
if (Test-Path $tessSrc) {
    Copy-Item $tessSrc "$DIST\tesseract.exe" -Force
    OK "tesseract.exe  ($([math]::Round((Get-Item $tessSrc).Length/1KB,0)) KB)"
} else { WARN "tesseract.exe not found — PDF OCR will not work" }

$tessDataSrc = "C:\msys64\ucrt64\share\tessdata"
if (Test-Path $tessDataSrc) {
    New-Item -ItemType Directory -Path "$DIST\tessdata" -Force | Out-Null
    foreach ($lang in @("ara.traineddata", "eng.traineddata")) {
        $src = "$tessDataSrc\$lang"
        if (Test-Path $src) {
            Copy-Item $src "$DIST\tessdata\$lang" -Force
            OK "tessdata/$lang  ($([math]::Round((Get-Item $src).Length/1MB,1)) MB)"
        }
    }
} else { WARN "tessdata not found in MSYS2" }

# sidebar SVG icons — copy to dist/sidebar/ so installed app can find them
$sidebarSrc = "$ROOT\src\sidebar"
if (Test-Path $sidebarSrc) {
    New-Item -ItemType Directory -Path "$DIST\sidebar" -Force | Out-Null
    Copy-Item "$sidebarSrc\*.svg" "$DIST\sidebar\" -Force
    $svgCount = (Get-ChildItem "$DIST\sidebar\*.svg").Count
    OK "sidebar/ icons: $svgCount SVG files"
} else { WARN "src/sidebar not found — sidebar icons will not show in installed app" }
# These DLLs are needed by tesseract.exe, pdftoppm.exe, pdftotext.exe, magick.exe
# on machines that do NOT have MSYS2 installed.
# windeployqt only handles Qt DLLs — these must be copied manually.
$toolDlls = @(
    "libnspr4.dll",                    # Mozilla NSPR — libpoppler dep
    "libplc4.dll",
    "libplds4.dll",
    "nss3.dll",                        # Mozilla NSS — libpoppler dep
    "smime3.dll",
    "nssutil3.dll",
    "libtesseract-5.5.dll",            # Tesseract OCR engine
    "libleptonica-6.dll",          # Image processing for tesseract
    "libgif-7.dll",
    "libopenjp2-7.dll",
    "libwebp-7.dll",
    "libtiff-6.dll",
    "libarchive-13.dll",           # Archive lib (tesseract/curl dep)
    "liblz4.dll",
    "libcurl-4.dll",               # Network lib (tesseract)
    "libnghttp2-14.dll",
    "libnghttp3-9.dll",
    "libngtcp2-16.dll",
    "libngtcp2_crypto_ossl-0.dll",
    "libssh2-1.dll",
    "libpsl-5.dll",
    "libpoppler-162.dll",          # Poppler PDF library
    "libpoppler-cpp-3.dll",
    "libmagickcore-7.q16hdri-10.dll",  # ImageMagick core
    "libmagickwand-7.q16hdri-10.dll",  # ImageMagick wand API
    "libltdl-7.dll",               # libtool DLL loader
    "Qt6Svg.dll",                  # Qt SVG rendering (sidebar icons)
    "Qt6SvgWidgets.dll"
)
$dlCopied = 0
foreach ($dll in $toolDlls) {
    $src = "C:\msys64\ucrt64\bin\$dll"
    $dst = "$DIST\$dll"
    if (Test-Path $dst) { continue }   # already present
    if (Test-Path $src) {
        Copy-Item $src $dst -Force
        $dlCopied++
    } else {
        WARN "Tool DLL not found: $dll"
    }
}
OK "Tool runtime DLLs: $dlCopied newly copied"

# ── One-pass transitive DLL scan on dist files only ───────────────────────────
# Scan all DLLs currently in dist + tools, find any lib*.dll or nss*.dll
# that's referenced but missing from dist (and exists in MSYS2).
# Only scan files in dist (not MSYS2) to avoid infinite chain.
$allRefs2 = [System.Collections.Generic.HashSet[string]]::new()
Get-ChildItem "$DIST\*.dll","$DIST\pdftoppm.exe","$DIST\magick.exe","$DIST\pdftotext.exe","$DIST\tesseract.exe" -ErrorAction SilentlyContinue | ForEach-Object {
    (& "$msys\objdump.exe" -p $_.FullName 2>$null) | Select-String "DLL Name:" | ForEach-Object {
        $null = $allRefs2.Add(($_ -replace ".*DLL Name:\s*","").Trim().ToLower())
    }
}
$winSysKw2 = @("kernel32","ntdll","user32","advapi32","shell32","ole32","gdi32","ws2_32","bcrypt","crypt32","wintrust","version","shlwapi","msvcrt","ucrtbase","vcruntime","msvcp","cfgmgr32","setupapi","winmm","imm32","combase","comdlg32","secur32","rpcrt4","psapi","dbghelp","api-ms","ext-ms")
$extraNew = 0
foreach ($dll in ($allRefs2 | Sort-Object)) {
    $isWin = @($winSysKw2 | Where-Object { $dll -like "*$_*" }).Count -gt 0
    if ($isWin) { continue }
    if (Test-Path "$DIST\$dll") { continue }
    if (Test-Path "$msys\$dll") {
        Copy-Item "$msys\$dll" "$DIST\$dll" -Force
        $extraNew++
    }
}
if ($extraNew -gt 0) { OK "Transitive scan: $extraNew additional DLLs copied" }
else                  { OK "Transitive scan: all DLLs already present" }

# =============================================================================
# STEP 7 — التحقق من صحة البناء
# =============================================================================
Step "7 / 8  Validating deployment"

$anyFail = $false

# 7a. عدد الـ DLLs
$rootDlls  = (Get-ChildItem $DIST -Filter "*.dll" -File).Count
$totalDlls = (Get-ChildItem $DIST -Filter "*.dll" -Recurse).Count
Write-Host "  DLL count: $rootDlls root  |  $totalDlls total"
if ($totalDlls -lt 140) {
    WARN "Expected ~150+ DLLs, got $totalDlls — some may be missing"
    $anyFail = $true
} else { OK "DLL count OK ($totalDlls)" }

# 7b. Architecture — كل DLL يجب أن يكون x64
$badArch = 0
Get-ChildItem $DIST -Filter "*.dll" -Recurse | ForEach-Object {
    $b = [IO.File]::ReadAllBytes($_.FullName)
    $o = [BitConverter]::ToInt32($b, 0x3C)
    $m = [BitConverter]::ToUInt16($b, $o + 4)
    if ($m -ne 0x8664) {
        Write-Host "  [ARCH] REJECT: $($_.Name) = 0x$('{0:X4}' -f $m)" -ForegroundColor Red
        $badArch++
    }
}
if ($badArch -gt 0) { FAIL "$badArch non-x64 DLL(s) found — do not ship this build" }
OK "Architecture: all DLLs are x64 (0x8664)"

# 7c. Dependency resolution — لا شيء يشير لـ C:\msys64 من خارج dist
$ntlddOut = & "$MSYS\ntldd.exe" -R "$DIST\DeliHub.exe" 2>&1 | Out-String
$leaked = @()
foreach ($line in ($ntlddOut -split "`n")) {
    if ($line -match "C:\\msys64\\ucrt64\\bin\\([^ ]+\.dll)") {
        $dll = $Matches[1].Trim()
        if (-not (Test-Path "$DIST\$dll")) { $leaked += $dll }
    }
}
$leaked = @($leaked | Sort-Object -Unique)
if ($leaked.Count -gt 0) {
    Write-Host "  [DEP] Missing from dist:" -ForegroundColor Red
    $leaked | ForEach-Object { Write-Host "        $_" -ForegroundColor Red }
    FAIL "$($leaked.Count) required DLL(s) not in dist — deployment is incomplete"
}
OK "Dependencies: all MSYS2 deps inside dist/"

# 7d. Compatibility fixes — تحقق من وجود أو غياب الـ strings الصحيحة في الـ exe
$ascii = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes("$DIST\DeliHub.exe"))
$checks = @(
    @{ s="Writable data directory: ";   must=$true;  label="ProgramData path fix (Program Files write bug)" },
    @{ s="Seeded config from template"; must=$true;  label="config.ini first-run copy" },
    @{ s="keeping resolved path";       must=$true;  label="branch dbPath relative-path guard" },
    @{ s="sslmode=require";             must=$true;  label="SSL require for Supabase" },
    @{ s="ca-certificates.crt";         must=$true;  label="CA bundle reference" },
    @{ s="SQLite DB path resolved";      must=$false; label="OLD code — must be absent" }
)
$fixFail = 0
foreach ($c in $checks) {
    $found = $ascii.Contains($c.s)
    $ok    = ($c.must -and $found) -or (-not $c.must -and -not $found)
    if ($ok) {
        Write-Host "  [OK]   $($c.label)" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $($c.label)  |  `"$($c.s)`"" -ForegroundColor Red
        $fixFail++
    }
}
if ($fixFail -gt 0) { FAIL "$fixFail compatibility check(s) failed — wrong exe was built" }
OK "All compatibility fixes confirmed in exe"

# =============================================================================
# STEP 8 — بناء الـ installer
# =============================================================================
Step "8 / 8  Building installer (Inno Setup)"

if (-not (Test-Path $ISCC)) {
    WARN "Inno Setup not found at:`n  $ISCC"
    WARN "Install from https://jrsoftware.org/isdl.php then re-run this script"
} elseif (-not (Test-Path $ISS)) {
    WARN "installer\DeliHub_setup.iss not found at $ISS"
} else {
    $outDir = "$ROOT\installer_output"
    if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

    & $ISCC $ISS
    if ($LASTEXITCODE -ne 0) { FAIL "Inno Setup failed (exit $LASTEXITCODE)" }

    $built = Get-ChildItem $outDir -Filter "*.exe" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($built) {
        OK "Installer: $($built.Name)  —  $([math]::Round($built.Length/1MB,2)) MB  —  $($built.LastWriteTime)"
    }
}

# =============================================================================
# DONE
# =============================================================================
Write-Host ""
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  BUILD COMPLETE" -ForegroundColor Green
Write-Host "  Deployment folder : $DIST" -ForegroundColor Green
Write-Host "  Installer         : $ROOT\installer_output\DeliHub_v1.0.0_Setup.exe" -ForegroundColor Green
Write-Host "  Supports          : Windows 10 (1809+) and Windows 11  [x64 only]" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green

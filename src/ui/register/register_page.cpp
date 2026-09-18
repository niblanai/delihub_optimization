#include "register_page.h"
#include "session_detail_dialog.h"
#include "close_session_dialog.h"
#include "services/lang_manager.h"
#include "services/session_manager.h"
#include "services/theme_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "ui/tr_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QMouseEvent>
#include <QTimer>
#include <QFrame>

RegisterPage::RegisterPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_repo = useSqlite ? static_cast<IRegisterSessionRepository*>(new SQLiteRegisterSessionRepository)
                       : static_cast<IRegisterSessionRepository*>(new AccessRegisterSessionRepository);
    m_cashRepo = useSqlite ? static_cast<ICashMovementRepository*>(new SQLiteCashMovementRepository)
                           : static_cast<ICashMovementRepository*>(new AccessCashMovementRepository);
    m_orderRepo = useSqlite ? static_cast<IOrderRepository*>(new SQLiteOrderRepository)
                            : static_cast<IOrderRepository*>(new AccessOrderRepository);
    setupUi();
    
    // Load sessions after UI is setup
    QTimer::singleShot(100, this, [this]() {
        loadSessions();
    });
}

void RegisterPage::setupUi() {
    auto& themeMgr = ThemeManager::instance();
    auto tokens = themeMgr.tokens();
    
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0); root->setContentsMargins(0,0,0,0);

    auto* toolbar = new QFrame; toolbar->setObjectName("pageToolbar");
    auto* tbL = new QHBoxLayout(toolbar); tbL->setContentsMargins(16,10,16,10);
    
    m_currentLbl = new QLabel("No active session");
    m_currentLbl->setStyleSheet("font-weight: bold; color: #F59E0B;");
    
    m_openBtn    = new QPushButton(LangManager::instance().t("🔓 Open Session"));
    m_openBtn->setObjectName("primaryBtn");
    m_closeBtn   = new QPushButton(LangManager::instance().t("🔒 Close Session"));
    m_closeBtn->setObjectName("dangerBtn");
    m_closeBtn->setEnabled(false);
    m_barcodeBtn = new QPushButton(LangManager::instance().t("📊 Search Invoice Barcode"));
    
    tbL->addWidget(m_currentLbl);
    tbL->addStretch();
    tbL->addWidget(m_barcodeBtn);
    tbL->addWidget(m_openBtn);
    tbL->addWidget(m_closeBtn);
    root->addWidget(toolbar);

    // Sessions grid in scroll area
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    auto* gridContainer = new QWidget;
    m_sessionsGrid = new QGridLayout(gridContainer);
    m_sessionsGrid->setSpacing(16);
    m_sessionsGrid->setContentsMargins(16, 16, 16, 16);
    m_sessionsGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    
    scrollArea->setWidget(gridContainer);
    root->addWidget(scrollArea, 1);

    auto* sb = new QFrame; sb->setObjectName("statusBar");
    auto* sbl = new QHBoxLayout(sb); sbl->setContentsMargins(16,4,16,4);
    m_statusLbl = new QLabel("0 sessions");
    m_statusLbl->setObjectName("statusLabel");
    sbl->addWidget(m_statusLbl);
    root->addWidget(sb);

    connect(m_openBtn,   &QPushButton::clicked, this, &RegisterPage::onOpenSession);
    connect(m_closeBtn,  &QPushButton::clicked, this, &RegisterPage::onCloseSession);
    connect(m_barcodeBtn, &QPushButton::clicked, this, &RegisterPage::onBarcodeSearch);
}

void RegisterPage::refresh() { loadSessions(); }
void RegisterPage::retranslateUi() { retranslateWidget(this); }

RegisterSession RegisterPage::getCurrentOpenSession() {
    int currentUserId = SessionManager::instance().currentUser().id();
    return m_repo->getOpenSessionByUserId(currentUserId);
}

RegisterPage::SessionTotals RegisterPage::calculateSessionTotals(int sessionId) {
    SessionTotals totals;
    if (!m_orderRepo || sessionId <= 0) return totals;
    
    // Get all orders and filter by session
    QList<Order> allOrders = m_orderRepo->getAll();
    
    for (const auto& order : allOrders) {
        // Only count orders from this session
        if (order.registerSessionId() != sessionId) continue;
        
        totals.orderCount++;
        totals.total += order.grandTotal();
        
        // Check payment method
        if (order.paymentMethod() == "Cash") {
            totals.cash += order.grandTotal();
        } else if (order.paymentMethod() == "Card" || order.paymentMethod() == "Visa") {
            totals.card += order.grandTotal();
        }
    }
    
    return totals;
}

void RegisterPage::loadSessions() {
    // Clear existing cards
    while (m_sessionsGrid->count() > 0) {
        QLayoutItem* item = m_sessionsGrid->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    
    m_sessions = m_repo->getAll();
    const QString sym = ConfigManager::instance().currencySymbol();
    RegisterSession openSession = getCurrentOpenSession();
    
    // Count open sessions
    int openCount = 0;
    for (const auto& sess : m_sessions) {
        if (sess.isOpen()) openCount++;
    }
    
    if (openSession.id() > 0) {
        m_currentLbl->setText(QString("🟢 Session #%1 OPEN").arg(openSession.id()));
        m_currentLbl->setStyleSheet("font-weight: bold; color: #10B981;");
        m_closeBtn->setEnabled(true);
    } else {
        m_currentLbl->setText("🔴 No active session");
        m_currentLbl->setStyleSheet("font-weight: bold; color: #F59E0B;");
        m_closeBtn->setEnabled(false);
    }
    
    // Open button enabled unless 9 sessions are already open
    m_openBtn->setEnabled(openCount < 9);
    
    // Display max 9 sessions in 3x3 grid
    int displayCount = qMin(m_sessions.size(), 9);
    
    qDebug() << "Loading" << displayCount << "sessions to display";
    
    for (int i = 0; i < displayCount; ++i) {
        const auto& session = m_sessions[i];
        int row = i / 3;
        int col = i % 3;
        
        qDebug() << "Session" << session.id() << "- Open:" << session.isOpen() 
                 << "Opening cash:" << session.openingCash()
                 << "Opened at:" << session.openedAt().toString();
        
        // Create session card - INCREASED HEIGHT for better data visibility
        auto* card = new QFrame;
        card->setObjectName("sessionCard");  // ✅ Object name for styling
        card->setMinimumSize(350, 280);  // Increased from 200 to 280
        card->setMaximumSize(400, 350);  // Increased from 220 to 350
        card->setCursor(Qt::PointingHandCursor);
        
        bool isOpen = session.isOpen();
        QString bgColor = isOpen ? "#E0F2FE" : "#F3F4F6";
        QString borderColor = isOpen ? "#38BDF8" : "#D1D5DB";
        
        // ✅ Descendant selector forces all child QLabels transparent
        card->setStyleSheet(QString(
            "QFrame#sessionCard { background: %1; border: 2px solid %2; border-radius: 12px; padding: 12px; }"
            "QFrame#sessionCard:hover { border-color: #3B82F6; background: #EFF6FF; }"
            "QFrame#sessionCard * { background: transparent; }"  // Force all children transparent
        ).arg(bgColor, borderColor));
        
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setSpacing(10);
        cardLayout->setContentsMargins(16, 16, 16, 16);
        
        // Session number + status
        auto* headerLayout = new QHBoxLayout;
        auto* sessionNumLbl = new QLabel();
        sessionNumLbl->setText("Session #" + QString::number(session.id()));
        sessionNumLbl->setWordWrap(false);
        sessionNumLbl->setStyleSheet("font-size: 18px; font-weight: bold; color: #1F2937;");
        
        auto* statusLbl = new QLabel();
        statusLbl->setText(isOpen ? "OPEN" : "CLOSED");
        statusLbl->setWordWrap(false);
        statusLbl->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1;")
            .arg(isOpen ? "#10B981" : "#DC2626"));
        
        headerLayout->addWidget(sessionNumLbl);
        headerLayout->addStretch();
        headerLayout->addWidget(statusLbl);
        cardLayout->addLayout(headerLayout);
        
        // Separator line
        auto* sepLine = new QFrame;
        sepLine->setFrameShape(QFrame::HLine);
        sepLine->setStyleSheet("background: #D1D5DB;");
        sepLine->setFixedHeight(2);
        cardLayout->addWidget(sepLine);
        
        // Opened date
        QString openDateStr = session.openedAt().isValid() 
            ? session.openedAt().toString("dd/MM/yyyy hh:mm")
            : "N/A";
        auto* openedLbl = new QLabel();
        openedLbl->setText("Opened: " + openDateStr);
        openedLbl->setWordWrap(false);
        openedLbl->setStyleSheet("font-size: 13px; color: #6B7280; font-weight: 500;");
        cardLayout->addWidget(openedLbl);
        
        // Opening cash
        auto* openingCashLbl = new QLabel();
        openingCashLbl->setText(QString("Opening: %1 %2").arg(session.openingCash(), 0, 'f', 2).arg(sym));
        openingCashLbl->setWordWrap(false);
        openingCashLbl->setStyleSheet("font-size: 15px; color: #059669; font-weight: bold;");
        cardLayout->addWidget(openingCashLbl);
        
        // Calculate session totals from orders
        SessionTotals totals = calculateSessionTotals(session.id());
        
        // Current drawer amount (for open sessions)
        if (isOpen && totals.orderCount > 0) {
            double currentDrawer = session.openingCash() + totals.cash;
            
            auto* currentLbl = new QLabel();
            currentLbl->setText(QString("💵 Cash in drawer: %1 %2").arg(currentDrawer, 0, 'f', 2).arg(sym));
            currentLbl->setWordWrap(false);
            currentLbl->setStyleSheet("font-size: 14px; color: #2563EB; font-weight: bold;");
            cardLayout->addWidget(currentLbl);
            
            // Cash from orders
            auto* cashLbl = new QLabel();
            cashLbl->setText(QString("  Cash sales: %1 %2").arg(totals.cash, 0, 'f', 2).arg(sym));
            cashLbl->setWordWrap(false);
            cashLbl->setStyleSheet("font-size: 12px; color: #059669;");
            cardLayout->addWidget(cashLbl);
            
            // Card from orders
            auto* cardLbl = new QLabel();
            cardLbl->setText(QString("💳 Card sales: %1 %2").arg(totals.card, 0, 'f', 2).arg(sym));
            cardLbl->setWordWrap(false);
            cardLbl->setStyleSheet("font-size: 12px; color: #7C3AED;");
            cardLayout->addWidget(cardLbl);
            
            // Order count
            auto* ordersLbl = new QLabel();
            ordersLbl->setText(QString("📊 Orders: %1").arg(totals.orderCount));
            ordersLbl->setWordWrap(false);
            ordersLbl->setStyleSheet("font-size: 11px; color: #6B7280;");
            cardLayout->addWidget(ordersLbl);
        }
        
        // Closing cash (if closed)
        if (!isOpen) {
            QString closeDateStr = session.closedAt().isValid()
                ? session.closedAt().toString("dd/MM/yyyy hh:mm")
                : "N/A";
            auto* closedLbl = new QLabel();
            closedLbl->setText("Closed: " + closeDateStr);
            closedLbl->setWordWrap(false);
            closedLbl->setStyleSheet("font-size: 13px; color: #6B7280; font-weight: 500;");
            cardLayout->addWidget(closedLbl);
            
            auto* closingCashLbl = new QLabel();
            closingCashLbl->setText(QString("Closing: %1 %2").arg(session.closingCash(), 0, 'f', 2).arg(sym));
            closingCashLbl->setWordWrap(false);
            closingCashLbl->setStyleSheet("font-size: 15px; color: #DC2626; font-weight: bold;");
            cardLayout->addWidget(closingCashLbl);
            
            // Show totals for closed sessions too
            if (totals.orderCount > 0) {
                // Cash from orders
                auto* cashLbl = new QLabel();
                cashLbl->setText(QString("  Cash sales: %1 %2").arg(totals.cash, 0, 'f', 2).arg(sym));
                cashLbl->setWordWrap(false);
                cashLbl->setStyleSheet("font-size: 12px; color: #059669;");
                cardLayout->addWidget(cashLbl);
                
                // Card from orders
                auto* cardLbl = new QLabel();
                cardLbl->setText(QString("💳 Card sales: %1 %2").arg(totals.card, 0, 'f', 2).arg(sym));
                cardLbl->setWordWrap(false);
                cardLbl->setStyleSheet("font-size: 12px; color: #7C3AED;");
                cardLayout->addWidget(cardLbl);
                
                // Order count
                auto* ordersLbl = new QLabel();
                ordersLbl->setText(QString("📊 Orders: %1").arg(totals.orderCount));
                ordersLbl->setWordWrap(false);
                ordersLbl->setStyleSheet("font-size: 11px; color: #6B7280;");
                cardLayout->addWidget(ordersLbl);
            }
        }
        
        cardLayout->addStretch();
        
        // Double-click hint
        auto* hintLbl = new QLabel("💡 Double-click for details");
        hintLbl->setStyleSheet("font-size: 11px; color: #9CA3AF; font-style: italic;");
        hintLbl->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(hintLbl);
        
        // Store session ID as property
        card->setProperty("sessionId", session.id());
        card->installEventFilter(this);
        
        m_sessionsGrid->addWidget(card, row, col);
    }
    
    m_statusLbl->setText(QString("%1 session(s)").arg(m_sessions.size()));
}

void RegisterPage::onOpenSession() {
    // Count open sessions (max 9 allowed)
    int openCount = 0;
    QList<RegisterSession> allSessions = m_repo->getAll();
    for (const auto& sess : allSessions) {
        if (sess.isOpen()) openCount++;
    }
    
    if (openCount >= 9) {
        QMessageBox::warning(this, "Maximum Sessions Reached",
            "You can have maximum 9 open sessions.\n\nPlease close one before opening a new session.");
        return;
    }
    
    // Ask for session name and opening cash
    bool ok;
    int nextNum = allSessions.size() + 1;
    QString defaultName = QString("Session %1").arg(nextNum);
    
    QString sessionName = QInputDialog::getText(this, "Open Register Session",
        "Session Name:", QLineEdit::Normal, defaultName, &ok);
    
    if (!ok || sessionName.trimmed().isEmpty()) return;
    
    double openingCash = QInputDialog::getDouble(this, "Open Register Session",
        QString("Opening cash amount for \"%1\":").arg(sessionName),
        0.0, 0.0, 1000000.0, 2, &ok);
    
    if (!ok) return;
    
    int currentUserId = SessionManager::instance().currentUser().id();
    RegisterSession session;
    session.setUserId(currentUserId);
    session.setOpeningCash(openingCash);
    session.setOpenedAt(QDateTime::currentDateTime());
    session.setStatus(RegisterSession::Status::Open);
    // TODO: Add session name to RegisterSession model
    
    if (!m_repo->save(session)) {
        QMessageBox::critical(this, "Error", "Failed to open register session.\n\nCheck the log file for details.");
        return;
    }
    
    QMessageBox::information(this, "Success",
        QString("Session #%1 \"%2\" opened successfully!\n\nOpening cash: %3")
            .arg(session.id())
            .arg(sessionName)
            .arg(openingCash, 0, 'f', 2));
    
    loadSessions();
}

void RegisterPage::onCloseSession() {
    RegisterSession sess = getCurrentOpenSession();
    if (sess.id() <= 0) {
        QMessageBox::warning(this, "No Active Session", "No active session to close.");
        return;
    }
    
    // Open close session dialog
    CloseSessionDialog dialog(sess, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Update session
        sess.setStatus(RegisterSession::Status::Closed);
        sess.setClosedAt(QDateTime::currentDateTime());
        sess.setClosingCash(dialog.getClosingCash());
        
        if (!m_repo->save(sess)) {
            QMessageBox::critical(this, "Error", "Failed to close session.\n\nCheck the log file for details.");
            return;
        }
        
        QMessageBox::information(this, "Success",
            QString("Session #%1 closed successfully!").arg(sess.id()));
        
        loadSessions();
    }
}

void RegisterPage::onSessionCardDoubleClick(int sessionId) {
    // Find session
    RegisterSession selectedSession;
    for (const auto& sess : m_sessions) {
        if (sess.id() == sessionId) {
            selectedSession = sess;
            break;
        }
    }
    
    if (selectedSession.id() == 0) {
        QMessageBox::warning(this, "Error", "Session not found!");
        return;
    }
    
    // Open session detail dialog
    SessionDetailDialog dialog(selectedSession, this);
    dialog.exec();
}

bool RegisterPage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        QFrame* card = qobject_cast<QFrame*>(obj);
        if (card && card->property("sessionId").isValid()) {
            int sessionId = card->property("sessionId").toInt();
            onSessionCardDoubleClick(sessionId);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void RegisterPage::onBarcodeSearch() {
    QString barcode = QInputDialog::getText(this, 
        LangManager::instance().t("Search Invoice"), 
        LangManager::instance().t("Enter or scan invoice barcode:"));
    
    if (barcode.trimmed().isEmpty()) return;
    
    // Search for order with this barcode
    QList<Order> allOrders = m_orderRepo->getAll();
    Order foundOrder;
    
    for (const auto& order : allOrders) {
        if (order.invoiceBarcode() == barcode.trimmed()) {
            foundOrder = order;
            break;
        }
    }
    
    if (foundOrder.id() == 0) {
        QMessageBox::information(this, 
            LangManager::instance().t("Not Found"),
            LangManager::instance().t("No invoice found with barcode: %1").arg(barcode));
        return;
    }
    
    // Found! Open session detail dialog with this order's session
    if (foundOrder.registerSessionId() > 0) {
        // Get the session object
        RegisterSession session;
        for (const auto& s : m_sessions) {
            if (s.id() == foundOrder.registerSessionId()) {
                session = s;
                break;
            }
        }
        
        if (session.id() == 0) {
            // Session not in current list, fetch from DB
            QList<RegisterSession> allSessions = m_repo->getAll();
            for (const auto& s : allSessions) {
                if (s.id() == foundOrder.registerSessionId()) {
                    session = s;
                    break;
                }
            }
        }
        
        if (session.id() > 0) {
            // Open session detail dialog
            SessionDetailDialog dlg(session, this);
            dlg.exec();
        } else {
            QMessageBox::warning(this, 
                LangManager::instance().t("Session Not Found"),
                LangManager::instance().t("Unable to find session #%1").arg(foundOrder.registerSessionId()));
        }
    } else {
        // Show order details directly in message box
        QString details = QString(
            "Invoice #%1\n"
            "Barcode: %2\n"
            "Date: %3\n"
            "Customer: %4\n"
            "Total: %5\n"
            "Payment: %6\n"
            "Status: %7"
        ).arg(foundOrder.id())
         .arg(foundOrder.invoiceBarcode())
         .arg(foundOrder.dateTime().toString("yyyy-MM-dd hh:mm"))
         .arg(foundOrder.customerName().isEmpty() ? "Walk-in" : foundOrder.customerName())
         .arg(foundOrder.grandTotal(), 0, 'f', 2)
         .arg(foundOrder.paymentMethod())
         .arg(foundOrder.status());
        
        QMessageBox::information(this, 
            LangManager::instance().t("Invoice Found"), 
            details);
    }
}

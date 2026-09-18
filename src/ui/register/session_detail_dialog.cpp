#include "session_detail_dialog.h"
#include "services/theme_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFrame>

SessionDetailDialog::SessionDetailDialog(const RegisterSession& session, QWidget* parent)
    : QDialog(parent), m_session(session)
{
    setWindowTitle(QString("Session #%1 Details").arg(session.id()));
    resize(900, 600);
    
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_orderRepo = useSqlite ? static_cast<IOrderRepository*>(new SQLiteOrderRepository)
                            : static_cast<IOrderRepository*>(new AccessOrderRepository);
    
    setupUi();
    loadOrders();
}

void SessionDetailDialog::setupUi() {
    auto& themeMgr = ThemeManager::instance();
    auto tokens = themeMgr.tokens();
    const QString sym = ConfigManager::instance().currencySymbol();
    
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Session info box
    auto* infoBox = new QGroupBox("📊 Session Information");
    auto* infoLayout = new QVBoxLayout(infoBox);
    infoLayout->setSpacing(8);
    
    m_sessionInfoLbl = new QLabel;
    QString statusStr = m_session.isOpen() ? "🟢 OPEN" : "🔒 CLOSED";
    QString statusColor = m_session.isOpen() ? "#10B981" : "#6B7280";
    
    QString infoText = QString(
        "<b>Session #%1</b> - <span style='color: %2;'>%3</span><br>"
        "📅 <b>Opened:</b> %4<br>"
        "💰 <b>Opening Cash:</b> %5 %6"
    ).arg(m_session.id())
     .arg(statusColor, statusStr)
     .arg(m_session.openedAt().toString("dd/MM/yyyy HH:mm"))
     .arg(m_session.openingCash(), 0, 'f', 2)
     .arg(sym);
    
    if (!m_session.isOpen() && m_session.closedAt().isValid()) {
        infoText += QString("<br>🔒 <b>Closed:</b> %1<br>"
                           "💵 <b>Closing Cash:</b> %2 %3<br>"
                           "📊 <b>Difference:</b> %4 %5")
            .arg(m_session.closedAt().toString("dd/MM/yyyy HH:mm"))
            .arg(m_session.closingCash(), 0, 'f', 2)
            .arg(sym)
            .arg(m_session.difference(), 0, 'f', 2)
            .arg(sym);
    }
    
    m_sessionInfoLbl->setText(infoText);
    m_sessionInfoLbl->setStyleSheet("padding: 8px; background: #F9FAFB; border-radius: 6px;");
    infoLayout->addWidget(m_sessionInfoLbl);
    
    mainLayout->addWidget(infoBox);
    
    // Orders summary
    auto* summaryFrame = new QFrame;
    summaryFrame->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 6px; padding: 8px; }").arg(tokens.cardBg, tokens.border));
    auto* summaryLayout = new QHBoxLayout(summaryFrame);
    
    m_summaryLbl = new QLabel("📦 Orders: 0 | 💵 Total: 0.00");
    m_summaryLbl->setStyleSheet("font-weight: bold; font-size: 14px;");
    summaryLayout->addWidget(m_summaryLbl);
    
    mainLayout->addWidget(summaryFrame);
    
    // Orders table
    auto* ordersLabel = new QLabel("🧾 POS Orders");
    ordersLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    mainLayout->addWidget(ordersLabel);
    
    m_ordersTable = new QTableWidget(0, 6);
    m_ordersTable->setObjectName("dataTable");
    m_ordersTable->setHorizontalHeaderLabels({"Order #", "Customer", "Date/Time", "Items", "Payment", "Total"});
    m_ordersTable->horizontalHeader()->setStretchLastSection(true);
    m_ordersTable->verticalHeader()->setVisible(false);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ordersTable->setAlternatingRowColors(true);
    mainLayout->addWidget(m_ordersTable, 1);
    
    // Close button
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(buttons);
}

void SessionDetailDialog::loadOrders() {
    if (!m_orderRepo) return;
    
    // Get all orders for this session
    QList<Order> allOrders = m_orderRepo->getAll();
    QList<Order> sessionOrders;
    
    for (const auto& order : allOrders) {
        if (order.registerSessionId() == m_session.id()) {
            sessionOrders.append(order);
        }
    }
    
    const QString sym = ConfigManager::instance().currencySymbol();
    double totalAmount = 0.0;
    
    m_ordersTable->setRowCount(0);
    
    for (const auto& order : sessionOrders) {
        int row = m_ordersTable->rowCount();
        m_ordersTable->insertRow(row);
        
        auto cell = [](const QString& t, Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        
        m_ordersTable->setItem(row, 0, cell(QString::number(order.id())));
        m_ordersTable->setItem(row, 1, cell(order.customerName(), Qt::AlignLeft | Qt::AlignVCenter));
        m_ordersTable->setItem(row, 2, cell(order.dateTime().toString("dd/MM HH:mm")));
        m_ordersTable->setItem(row, 3, cell(QString::number(order.items().size())));
        m_ordersTable->setItem(row, 4, cell(order.paymentMethod()));
        m_ordersTable->setItem(row, 5, cell(QString("%1 %2").arg(order.grandTotal(), 0, 'f', 2).arg(sym), Qt::AlignRight | Qt::AlignVCenter));
        
        totalAmount += order.grandTotal();
    }
    
    m_summaryLbl->setText(QString("📦 Orders: %1 | 💵 Total: %2 %3")
        .arg(sessionOrders.size())
        .arg(totalAmount, 0, 'f', 2)
        .arg(sym));
}

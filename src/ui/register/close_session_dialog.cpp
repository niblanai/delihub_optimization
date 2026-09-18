#include "close_session_dialog.h"
#include "services/theme_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include <QFrame>
#include <QMessageBox>
#include <QRegularExpression>

CloseSessionDialog::CloseSessionDialog(const RegisterSession& session, QWidget* parent)
    : QDialog(parent), m_session(session)
{
    setWindowTitle(QString("Close Session #%1").arg(session.id()));
    resize(800, 600);
    
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_orderRepo = useSqlite ? static_cast<IOrderRepository*>(new SQLiteOrderRepository)
                            : static_cast<IOrderRepository*>(new AccessOrderRepository);
    
    setupUi();
    loadOrdersSummary();
}

void CloseSessionDialog::setupUi() {
    auto& themeMgr = ThemeManager::instance();
    auto tokens = themeMgr.tokens();
    const QString sym = ConfigManager::instance().currencySymbol();
    
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Session info
    auto* infoLbl = new QLabel(QString("<b>Session #%1</b> | Opened: %2 | Opening Cash: %3 %4")
        .arg(m_session.id())
        .arg(m_session.openedAt().toString("dd/MM/yyyy HH:mm"))
        .arg(m_session.openingCash(), 0, 'f', 2)
        .arg(sym));
    infoLbl->setStyleSheet("padding: 8px; background: #F3F4F6; border-radius: 6px;");
    mainLayout->addWidget(infoLbl);
    
    // Orders table
    auto* ordersLabel = new QLabel("🧾 Orders in this session:");
    ordersLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    mainLayout->addWidget(ordersLabel);
    
    m_ordersTable = new QTableWidget(0, 5);
    m_ordersTable->setObjectName("dataTable");
    m_ordersTable->setHorizontalHeaderLabels({"Order #", "Customer", "Payment", "Amount", "Time"});
    m_ordersTable->horizontalHeader()->setStretchLastSection(true);
    m_ordersTable->verticalHeader()->setVisible(false);
    m_ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ordersTable->setAlternatingRowColors(true);
    m_ordersTable->setMaximumHeight(200);
    mainLayout->addWidget(m_ordersTable);
    
    // Expected amounts (from system)
    auto* expectedBox = new QGroupBox("💰 Expected Amounts (from orders)");
    auto* expectedLayout = new QHBoxLayout(expectedBox);
    
    m_orderCountLbl = new QLabel("Orders: 0");
    m_orderCountLbl->setStyleSheet("font-size: 13px;");
    
    m_expectedCashLbl = new QLabel(QString("💵 Cash: 0.00 %1").arg(sym));
    m_expectedCashLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #059669;");
    
    m_expectedCardLbl = new QLabel(QString("💳 Card: 0.00 %1").arg(sym));
    m_expectedCardLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #3B82F6;");
    
    m_totalExpectedLbl = new QLabel(QString("📊 Total: 0.00 %1").arg(sym));
    m_totalExpectedLbl->setStyleSheet("font-size: 15px; font-weight: bold; color: #1F2937;");
    
    expectedLayout->addWidget(m_orderCountLbl);
    expectedLayout->addStretch();
    expectedLayout->addWidget(m_expectedCashLbl);
    expectedLayout->addWidget(m_expectedCardLbl);
    expectedLayout->addWidget(m_totalExpectedLbl);
    
    mainLayout->addWidget(expectedBox);
    
    // Actual amounts (user input)
    auto* actualBox = new QGroupBox("✍️ Actual Amounts (from drawer & machine)");
    auto* actualLayout = new QFormLayout(actualBox);
    actualLayout->setSpacing(12);
    
    m_actualCashSpin = new QDoubleSpinBox;
    m_actualCashSpin->setRange(0.0, 999999.99);
    m_actualCashSpin->setDecimals(2);
    m_actualCashSpin->setPrefix(sym + " ");
    m_actualCashSpin->setMinimumHeight(40);
    m_actualCashSpin->setStyleSheet("QDoubleSpinBox { font-size: 16px; padding: 6px; }");
    
    m_actualCardSpin = new QDoubleSpinBox;
    m_actualCardSpin->setRange(0.0, 999999.99);
    m_actualCardSpin->setDecimals(2);
    m_actualCardSpin->setPrefix(sym + " ");
    m_actualCardSpin->setMinimumHeight(40);
    m_actualCardSpin->setStyleSheet("QDoubleSpinBox { font-size: 16px; padding: 6px; }");
    
    actualLayout->addRow("💵 <b>Actual Cash in Drawer:</b>", m_actualCashSpin);
    actualLayout->addRow("💳 <b>Actual Card from Machine:</b>", m_actualCardSpin);
    
    mainLayout->addWidget(actualBox);
    
    // Variance (calculated)
    auto* varianceBox = new QGroupBox("📊 Variance (Difference)");
    auto* varianceLayout = new QVBoxLayout(varianceBox);
    varianceLayout->setSpacing(8);
    
    m_cashVarianceLbl = new QLabel(QString("💵 Cash Variance: 0.00 %1").arg(sym));
    m_cashVarianceLbl->setStyleSheet("font-size: 14px; font-weight: bold;");
    
    m_cardVarianceLbl = new QLabel(QString("💳 Card Variance: 0.00 %1").arg(sym));
    m_cardVarianceLbl->setStyleSheet("font-size: 14px; font-weight: bold;");
    
    m_totalVarianceLbl = new QLabel(QString("📊 Total Variance: 0.00 %1").arg(sym));
    m_totalVarianceLbl->setStyleSheet("font-size: 16px; font-weight: bold;");
    
    varianceLayout->addWidget(m_cashVarianceLbl);
    varianceLayout->addWidget(m_cardVarianceLbl);
    varianceLayout->addWidget(m_totalVarianceLbl);
    
    mainLayout->addWidget(varianceBox);
    
    // Buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText("🔒 Close Session");
    buttons->button(QDialogButtonBox::Save)->setObjectName("dangerBtn");
    buttons->button(QDialogButtonBox::Save)->setMinimumHeight(45);
    buttons->button(QDialogButtonBox::Cancel)->setMinimumHeight(45);
    
    connect(buttons, &QDialogButtonBox::accepted, this, &CloseSessionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttons);
    
    // Connect signals
    connect(m_actualCashSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CloseSessionDialog::onActualCashChanged);
    connect(m_actualCardSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CloseSessionDialog::onActualCardChanged);
}

void CloseSessionDialog::loadOrdersSummary() {
    if (!m_orderRepo) return;
    
    // Get all orders for this session
    QList<Order> allOrders = m_orderRepo->getAll();
    QList<Order> sessionOrders;
    
    m_totalCash = 0.0;
    m_totalCard = 0.0;
    m_totalAmount = 0.0;
    
    for (const auto& order : allOrders) {
        if (order.registerSessionId() == m_session.id()) {
            sessionOrders.append(order);
            
            // Parse payment method to calculate cash/card totals
            QString payment = order.paymentMethod();
            double amount = order.grandTotal();
            
            if (payment.contains("Split", Qt::CaseInsensitive)) {
                // Parse "Split (Cash: 250.00, Card: 250.00)"
                QRegularExpression cashRe("Cash:\\s*([0-9.]+)");
                QRegularExpression cardRe("Card:\\s*([0-9.]+)");
                
                auto cashMatch = cashRe.match(payment);
                auto cardMatch = cardRe.match(payment);
                
                if (cashMatch.hasMatch()) {
                    m_totalCash += cashMatch.captured(1).toDouble();
                }
                if (cardMatch.hasMatch()) {
                    m_totalCard += cardMatch.captured(1).toDouble();
                }
            } else if (payment.contains("Cash", Qt::CaseInsensitive)) {
                m_totalCash += amount;
            } else if (payment.contains("Card", Qt::CaseInsensitive) || payment.contains("Visa", Qt::CaseInsensitive)) {
                m_totalCard += amount;
            }
            
            m_totalAmount += amount;
        }
    }
    
    const QString sym = ConfigManager::instance().currencySymbol();
    
    // Fill table
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
        m_ordersTable->setItem(row, 2, cell(order.paymentMethod()));
        m_ordersTable->setItem(row, 3, cell(QString("%1 %2").arg(order.grandTotal(), 0, 'f', 2).arg(sym), Qt::AlignRight | Qt::AlignVCenter));
        m_ordersTable->setItem(row, 4, cell(order.dateTime().toString("HH:mm")));
    }
    
    // Update labels
    m_orderCountLbl->setText(QString("Orders: %1").arg(sessionOrders.size()));
    m_expectedCashLbl->setText(QString("💵 Cash: %1 %2").arg(m_totalCash, 0, 'f', 2).arg(sym));
    m_expectedCardLbl->setText(QString("💳 Card: %1 %2").arg(m_totalCard, 0, 'f', 2).arg(sym));
    m_totalExpectedLbl->setText(QString("📊 Total: %1 %2").arg(m_totalAmount, 0, 'f', 2).arg(sym));
    
    // Set initial actual values to expected
    m_actualCashSpin->setValue(m_totalCash);
    m_actualCardSpin->setValue(m_totalCard);
}

void CloseSessionDialog::onActualCashChanged() {
    calculateVariance();
}

void CloseSessionDialog::onActualCardChanged() {
    calculateVariance();
}

void CloseSessionDialog::calculateVariance() {
    const QString sym = ConfigManager::instance().currencySymbol();
    
    double actualCash = m_actualCashSpin->value();
    double actualCard = m_actualCardSpin->value();
    
    double cashVariance = actualCash - m_totalCash;
    double cardVariance = actualCard - m_totalCard;
    double totalVariance = cashVariance + cardVariance;
    
    // Cash variance
    QString cashColor = (cashVariance > 0) ? "#10B981" : (cashVariance < 0) ? "#EF4444" : "#6B7280";
    QString cashSign = (cashVariance > 0) ? "+" : "";
    m_cashVarianceLbl->setText(QString("💵 Cash Variance: %1%2 %3").arg(cashSign).arg(cashVariance, 0, 'f', 2).arg(sym));
    m_cashVarianceLbl->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1;").arg(cashColor));
    
    // Card variance
    QString cardColor = (cardVariance > 0) ? "#10B981" : (cardVariance < 0) ? "#EF4444" : "#6B7280";
    QString cardSign = (cardVariance > 0) ? "+" : "";
    m_cardVarianceLbl->setText(QString("💳 Card Variance: %1%2 %3").arg(cardSign).arg(cardVariance, 0, 'f', 2).arg(sym));
    m_cardVarianceLbl->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1;").arg(cardColor));
    
    // Total variance
    QString totalColor = (totalVariance > 0) ? "#10B981" : (totalVariance < 0) ? "#EF4444" : "#6B7280";
    QString totalSign = (totalVariance > 0) ? "+" : "";
    m_totalVarianceLbl->setText(QString("📊 Total Variance: %1%2 %3").arg(totalSign).arg(totalVariance, 0, 'f', 2).arg(sym));
    m_totalVarianceLbl->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(totalColor));
    
    // Store closing cash (opening + actual cash received)
    m_closingCash = m_session.openingCash() + actualCash;
}

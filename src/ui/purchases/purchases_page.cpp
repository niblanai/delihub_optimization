#include "purchases_page.h"
#include "purchase_invoice_dialog.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "ui/tr_helper.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFrame>
#include <QCoreApplication>
#include <QFile>

static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    QString devPath = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(devPath)) return devPath;
    return file;
}

PurchasesPage::PurchasesPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    m_repo = useSqlite ? static_cast<IPurchaseInvoiceRepository*>(new SQLitePurchaseInvoiceRepository)
                       : static_cast<IPurchaseInvoiceRepository*>(new AccessPurchaseInvoiceRepository);
    m_supplierRepo = useSqlite ? static_cast<ISupplierRepository*>(new SQLiteSupplierRepository)
                               : static_cast<ISupplierRepository*>(new AccessSupplierRepository);
    setupUi();
}

void PurchasesPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0); root->setContentsMargins(0,0,0,0);

    auto* toolbar = new QFrame; toolbar->setObjectName("pageToolbar");
    auto* tbL = new QHBoxLayout(toolbar); tbL->setContentsMargins(16,10,16,10);
    
    m_addBtn    = new QPushButton(LangManager::instance().t("＋ New Purchase"));
    m_addBtn->setObjectName("primaryBtn");
    m_viewBtn   = new QPushButton(LangManager::instance().t("View"));
    m_viewBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("view-svgrepo-com.svg"), 
                                           QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_viewBtn->setObjectName("secondaryBtn");
    m_viewBtn->setEnabled(false);
    m_deleteBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_deleteBtn->setObjectName("dangerBtn");
    m_deleteBtn->setEnabled(false);
    
    tbL->addStretch();
    tbL->addWidget(m_addBtn);
    tbL->addWidget(m_viewBtn);
    tbL->addWidget(m_deleteBtn);
    root->addWidget(toolbar);

    m_table = new QTableWidget(0, 6);
    m_table->setObjectName("dataTable");
    m_table->setHorizontalHeaderLabels({
        LangManager::instance().t("Invoice #"),
        LangManager::instance().t("Supplier"),
        LangManager::instance().t("Date"),
        LangManager::instance().t("Total"),
        LangManager::instance().t("Status"),
        LangManager::instance().t("Notes")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    auto* sb = new QFrame; sb->setObjectName("statusBar");
    auto* sbl = new QHBoxLayout(sb); sbl->setContentsMargins(16,4,16,4);
    m_statusLbl = new QLabel("0 invoices");
    m_statusLbl->setObjectName("statusLabel");
    sbl->addWidget(m_statusLbl);
    root->addWidget(sb);

    connect(m_addBtn,    &QPushButton::clicked, this, &PurchasesPage::onAdd);
    connect(m_viewBtn,   &QPushButton::clicked, this, &PurchasesPage::onView);
    connect(m_deleteBtn, &QPushButton::clicked, this, &PurchasesPage::onDelete);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &PurchasesPage::onSelectionChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int,int){ onView(); });
    
    // Load data on page init
    QTimer::singleShot(0, this, &PurchasesPage::loadInvoices);
}

void PurchasesPage::refresh() { loadInvoices(); }
void PurchasesPage::retranslateUi() { retranslateWidget(this); }

void PurchasesPage::loadInvoices() {
    m_invoices = m_repo->getAll();
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    
    const QString sym = ConfigManager::instance().currencySymbol();
    QList<Supplier> suppliers = m_supplierRepo->getAll();
    
    for (const auto& inv : m_invoices) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        
        auto cell = [](const QString& t, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = new QTableWidgetItem(t);
            it->setTextAlignment(a);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            return it;
        };
        
        QString supplierName = "—";
        for (const auto& s : suppliers) {
            if (s.id() == inv.supplierId()) {
                supplierName = s.name();
                break;
            }
        }
        
        m_table->setItem(row, 0, cell(inv.invoiceNumber(), Qt::AlignCenter));
        m_table->setItem(row, 1, cell(supplierName));
        m_table->setItem(row, 2, cell(inv.date().toString("yyyy-MM-dd"), Qt::AlignCenter));
        m_table->setItem(row, 3, cell(QString::number(inv.totalAmount(), 'f', 2) + " " + sym, Qt::AlignRight | Qt::AlignVCenter));
        
        auto* statusItem = cell(inv.status() == PurchaseInvoice::Status::Confirmed ? "✅ Confirmed" : "📝 Draft", Qt::AlignCenter);
        statusItem->setForeground(inv.status() == PurchaseInvoice::Status::Confirmed ? QColor("#4ADE80") : QColor("#FCD34D"));
        m_table->setItem(row, 4, statusItem);
        
        m_table->setItem(row, 5, cell(inv.notes()));
    }
    
    m_statusLbl->setText(QString("%1 purchase invoice(s)").arg(m_invoices.size()));
    m_table->setSortingEnabled(true);
    m_viewBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
}

void PurchasesPage::onAdd() {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    
    auto* productRepo = useSqlite ? static_cast<IProductRepository*>(new SQLiteProductRepository)
                                  : static_cast<IProductRepository*>(new AccessProductRepository);
    auto* stockMovementRepo = useSqlite ? static_cast<IStockMovementRepository*>(new SQLiteStockMovementRepository)
                                        : static_cast<IStockMovementRepository*>(new AccessStockMovementRepository);
    auto* costHistoryRepo = useSqlite ? static_cast<IProductCostHistoryRepository*>(new SQLiteProductCostHistoryRepository)
                                      : static_cast<IProductCostHistoryRepository*>(new AccessProductCostHistoryRepository);
    
    PurchaseInvoiceDialog dialog(m_repo, m_supplierRepo, productRepo, stockMovementRepo, costHistoryRepo, this);
    if (dialog.exec() == QDialog::Accepted) {
        loadInvoices();
    }
    
    delete productRepo;
    delete stockMovementRepo;
    delete costHistoryRepo;
}

void PurchasesPage::onView() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_invoices.size()) return;
    
    const auto& invoice = m_invoices[row];
    
    // Find supplier name
    QString supplierName = "N/A";
    for (const auto& s : m_supplierRepo->getAll()) {
        if (s.id() == invoice.supplierId()) {
            supplierName = s.name();
            break;
        }
    }
    
    // Build invoice details HTML
    QString sym = ConfigManager::instance().currencySymbol();
    QString html = QString(
        "<h3>📄 Purchase Invoice #%1</h3>"
        "<table cellpadding='4'>"
        "<tr><td><b>Supplier:</b></td><td>%2</td></tr>"
        "<tr><td><b>Invoice Number:</b></td><td>%3</td></tr>"
        "<tr><td><b>Date:</b></td><td>%4</td></tr>"
        "<tr><td><b>Total Amount:</b></td><td>%5 %6</td></tr>"
        "<tr><td><b>Paid Amount:</b></td><td>%7 %6</td></tr>"
        "<tr><td><b>Status:</b></td><td>%8</td></tr>"
        "<tr><td><b>Notes:</b></td><td>%9</td></tr>"
        "</table>"
    ).arg(invoice.id())
     .arg(supplierName)
     .arg(invoice.invoiceNumber().isEmpty() ? "N/A" : invoice.invoiceNumber())
     .arg(invoice.date().toString("dd/MM/yyyy"))
     .arg(invoice.totalAmount(), 0, 'f', 2)
     .arg(sym)
     .arg(invoice.paidAmount(), 0, 'f', 2)
     .arg(PurchaseInvoice::statusToString(invoice.status()))
     .arg(invoice.notes().isEmpty() ? "N/A" : invoice.notes());
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Purchase Invoice Details");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(html);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void PurchasesPage::onDelete() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_invoices.size()) return;
    
    const PurchaseInvoice& inv = m_invoices.at(row);
    
    if (inv.status() == PurchaseInvoice::Status::Confirmed) {
        QMessageBox::warning(this, "Cannot Delete", "Cannot delete confirmed invoices.\n\nYou must create a correction invoice instead.");
        return;
    }
    
    auto reply = QMessageBox::question(this, "Confirm",
        "Delete draft invoice \"" + inv.invoiceNumber() + "\"?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    
    if (!m_repo->remove(inv.id())) {
        QMessageBox::critical(this, "Error", "Failed to delete invoice.");
        return;
    }
    
    loadInvoices();
}

void PurchasesPage::onSelectionChanged() {
    bool has = m_table->currentRow() >= 0;
    m_viewBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
}

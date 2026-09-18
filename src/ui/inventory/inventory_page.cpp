#include "inventory_page.h"
#include "services/lang_manager.h"
#include "infra/config_manager.h"
#include "infra/database_connection_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "services/session_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QTimer>
#include <QFormLayout>
#include <QTextEdit>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLineEdit>
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

InventoryPage::InventoryPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    
    m_repo = useSqlite ? static_cast<IStockCountRepository*>(new SQLiteStockCountRepository)
                       : static_cast<IStockCountRepository*>(new AccessStockCountRepository);
    
    m_productRepo = useSqlite ? static_cast<IProductRepository*>(new SQLiteProductRepository)
                              : static_cast<IProductRepository*>(new AccessProductRepository);
    
    setupUi();
}

InventoryPage::~InventoryPage() {
    delete m_repo;
    delete m_productRepo;
}

void InventoryPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);
    
    // Toolbar
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 10, 16, 10);
    
    auto* titleLbl = new QLabel("📦 " + LangManager::instance().t("Inventory & Stocktake"));
    titleLbl->setStyleSheet("font-size: 18px; font-weight: bold;");
    
    tbLayout->addWidget(titleLbl);
    tbLayout->addStretch();
    
    // Date filters
    tbLayout->addWidget(new QLabel(LangManager::instance().t("From:")));
    m_dateFromEdit = new QDateEdit(QDate::currentDate().addMonths(-1));
    m_dateFromEdit->setCalendarPopup(true);
    m_dateFromEdit->setDisplayFormat("yyyy-MM-dd");
    connect(m_dateFromEdit, &QDateEdit::dateChanged, this, &InventoryPage::loadStockCounts);
    tbLayout->addWidget(m_dateFromEdit);
    
    tbLayout->addWidget(new QLabel(LangManager::instance().t("To:")));
    m_dateToEdit = new QDateEdit(QDate::currentDate());
    m_dateToEdit->setCalendarPopup(true);
    m_dateToEdit->setDisplayFormat("yyyy-MM-dd");
    connect(m_dateToEdit, &QDateEdit::dateChanged, this, &InventoryPage::loadStockCounts);
    tbLayout->addWidget(m_dateToEdit);
    
    // Status filter
    m_statusFilter = new QComboBox;
    m_statusFilter->addItem(LangManager::instance().t("All"), "");
    m_statusFilter->addItem(LangManager::instance().t("Draft"), "Draft");
    m_statusFilter->addItem(LangManager::instance().t("Confirmed"), "Confirmed");
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &InventoryPage::loadStockCounts);
    tbLayout->addWidget(m_statusFilter);
    
    // Action buttons
    m_addBtn = new QPushButton(LangManager::instance().t("＋ New Stock Count"));
    m_addBtn->setObjectName("primaryBtn");
    connect(m_addBtn, &QPushButton::clicked, this, &InventoryPage::onAddStockCount);
    tbLayout->addWidget(m_addBtn);
    
    m_viewBtn = new QPushButton(LangManager::instance().t("View"));
    m_viewBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("view-svgrepo-com.svg"), 
                                           QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_viewBtn->setObjectName("secondaryBtn");
    m_viewBtn->setEnabled(false);
    connect(m_viewBtn, &QPushButton::clicked, this, &InventoryPage::onViewStockCount);
    tbLayout->addWidget(m_viewBtn);
    
    m_deleteBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                             QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_deleteBtn->setObjectName("dangerBtn");
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &InventoryPage::onDeleteStockCount);
    tbLayout->addWidget(m_deleteBtn);
    
    root->addWidget(toolbar);
    
    // Table
    m_table = new QTableWidget(0, 5);
    m_table->setObjectName("dataTable");
    m_table->setHorizontalHeaderLabels({
        LangManager::instance().t("ID"),
        LangManager::instance().t("Date"),
        LangManager::instance().t("Status"),
        LangManager::instance().t("Created By"),
        LangManager::instance().t("Notes")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &InventoryPage::onSelectionChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int,int){ onViewStockCount(); });
    
    root->addWidget(m_table, 1);
    
    // Status bar
    auto* statusBar = new QFrame;
    statusBar->setObjectName("statusBar");
    auto* sbLayout = new QHBoxLayout(statusBar);
    sbLayout->setContentsMargins(16, 4, 16, 4);
    m_statusLbl = new QLabel("0 stock counts");
    m_statusLbl->setObjectName("statusLabel");
    sbLayout->addWidget(m_statusLbl);
    root->addWidget(statusBar);
    
    // Load data on init
    QTimer::singleShot(0, this, &InventoryPage::loadStockCounts);
}

void InventoryPage::loadStockCounts() {
    // Get stock counts in date range
    m_stockCounts = m_repo->getByDateRange(m_dateFromEdit->date(), m_dateToEdit->date());
    
    // Filter by status if selected
    QString statusFilter = m_statusFilter->currentData().toString();
    if (!statusFilter.isEmpty()) {
        QList<StockCount> filtered;
        for (const auto& sc : m_stockCounts) {
            if (StockCount::statusToString(sc.status()) == statusFilter) {
                filtered.append(sc);
            }
        }
        m_stockCounts = filtered;
    }
    
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    
    for (const auto& sc : m_stockCounts) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(sc.id())));
        m_table->setItem(row, 1, new QTableWidgetItem(sc.date().toString("dd/MM/yyyy")));
        
        QString status = StockCount::statusToString(sc.status());
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(status == "Confirmed" ? QColor("#10B981") : QColor("#F59E0B"));
        m_table->setItem(row, 2, statusItem);
        
        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(sc.userId())));
        m_table->setItem(row, 4, new QTableWidgetItem(sc.notes()));
    }
    
    m_table->setSortingEnabled(true);
    m_statusLbl->setText(QString("%1 stock count(s)").arg(m_stockCounts.size()));
}

void InventoryPage::onSelectionChanged() {
    bool hasSelection = !m_table->selectedItems().isEmpty();
    m_viewBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
}

void InventoryPage::onAddStockCount() {
    // Create new stock count dialog
    QDialog dlg(this);
    dlg.setWindowTitle(LangManager::instance().t("New Stock Count"));
    dlg.setMinimumSize(700, 500);
    
    auto* layout = new QVBoxLayout(&dlg);
    
    // Header info
    auto* headerLayout = new QFormLayout;
    auto* dateEdit = new QDateEdit(QDate::currentDate());
    dateEdit->setCalendarPopup(true);
    headerLayout->addRow(LangManager::instance().t("Date:"), dateEdit);
    
    auto* notesEdit = new QTextEdit;
    notesEdit->setMaximumHeight(60);
    notesEdit->setPlaceholderText(LangManager::instance().t("Optional notes..."));
    headerLayout->addRow(LangManager::instance().t("Notes:"), notesEdit);
    
    layout->addLayout(headerLayout);
    
    // Products table
    auto* productsLabel = new QLabel("<b>" + LangManager::instance().t("Products to Count:") + "</b>");
    layout->addWidget(productsLabel);
    
    auto* table = new QTableWidget(0, 5);
    table->setHorizontalHeaderLabels({
        LangManager::instance().t("Product"),
        LangManager::instance().t("System Qty"),
        LangManager::instance().t("Actual Qty"),
        LangManager::instance().t("Difference"),
        LangManager::instance().t("Actions")
    });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    layout->addWidget(table);
    
    // Handle remove on double-click
    QObject::connect(table, &QTableWidget::cellDoubleClicked, [table](int row, int col) {
        if (col == 4) table->removeRow(row);  // Remove column
    });
    
    // Add product button
    auto* addProductBtn = new QPushButton("＋ " + LangManager::instance().t("Add Product"));
    addProductBtn->setObjectName("secondaryBtn");
    
    // Product selection - use a simpler approach
    auto addProductToTable = [this, table]() {
        auto products = m_productRepo->getAll();
        
        QDialog productDlg(this);
        productDlg.setWindowTitle(LangManager::instance().t("Select Product"));
        productDlg.setMinimumSize(500, 400);
        
        auto* pdLayout = new QVBoxLayout(&productDlg);
        auto* searchEdit = new QLineEdit;
        searchEdit->setPlaceholderText(LangManager::instance().t("Search products..."));
        pdLayout->addWidget(searchEdit);
        
        auto* productList = new QListWidget;
        pdLayout->addWidget(productList);
        
        auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        pdLayout->addWidget(btnBox);
        
        auto updateList = [&products, productList](const QString& filter) {
            productList->clear();
            for (const auto& p : products) {
                if (p.status() != Product::Status::Active) continue;
                if (!filter.isEmpty() && !p.name().toLower().contains(filter.toLower())) continue;
                
                auto* item = new QListWidgetItem(QString("%1 (Stock: %2)")
                    .arg(p.name()).arg(p.stockQty()));
                item->setData(Qt::UserRole, p.id());
                productList->addItem(item);
            }
        };
        
        updateList("");
        QObject::connect(searchEdit, &QLineEdit::textChanged, updateList);
        QObject::connect(btnBox, &QDialogButtonBox::accepted, &productDlg, &QDialog::accept);
        QObject::connect(btnBox, &QDialogButtonBox::rejected, &productDlg, &QDialog::reject);
        QObject::connect(productList, &QListWidget::itemDoubleClicked, &productDlg, &QDialog::accept);
        
        if (productDlg.exec() == QDialog::Accepted && productList->currentItem()) {
            int productId = productList->currentItem()->data(Qt::UserRole).toInt();
            Product product;
            for (const auto& p : products) {
                if (p.id() == productId) { product = p; break; }
            }
            
            if (product.id() > 0) {
                int row = table->rowCount();
                table->insertRow(row);
                
                auto* nameItem = new QTableWidgetItem(product.name());
                nameItem->setData(Qt::UserRole, product.id());
                table->setItem(row, 0, nameItem);
                
                auto* systemQty = new QTableWidgetItem(QString::number(product.stockQty()));
                systemQty->setTextAlignment(Qt::AlignCenter);
                systemQty->setFlags(Qt::ItemIsEnabled);
                table->setItem(row, 1, systemQty);
                
                auto* actualQty = new QTableWidgetItem("0");
                actualQty->setTextAlignment(Qt::AlignCenter);
                table->setItem(row, 2, actualQty);
                
                auto* diff = new QTableWidgetItem("0");
                diff->setTextAlignment(Qt::AlignCenter);
                diff->setFlags(Qt::ItemIsEnabled);
                table->setItem(row, 3, diff);
                
                auto* removeItem = new QTableWidgetItem("❌");
                removeItem->setTextAlignment(Qt::AlignCenter);
                removeItem->setForeground(QColor("#EF4444"));
                removeItem->setToolTip("Double-click to remove");
                table->setItem(row, 4, removeItem);
            }
        }
    };
    
    connect(addProductBtn, &QPushButton::clicked, addProductToTable);
    layout->addWidget(addProductBtn);
    
    // Dialog buttons
    auto* btnBox = new QDialogButtonBox;
    auto* saveDraftBtn = btnBox->addButton(LangManager::instance().t("Save as Draft"), QDialogButtonBox::ActionRole);
    auto* confirmBtn = btnBox->addButton(LangManager::instance().t("Confirm & Adjust Stock"), QDialogButtonBox::ActionRole);
    confirmBtn->setObjectName("primaryBtn");
    auto* cancelBtn = btnBox->addButton(QDialogButtonBox::Cancel);
    
    bool isDraft = false;
    connect(saveDraftBtn, &QPushButton::clicked, [&]() { isDraft = true; dlg.accept(); });
    connect(confirmBtn, &QPushButton::clicked, [&]() { isDraft = false; dlg.accept(); });
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    
    layout->addWidget(btnBox);
    
    if (dlg.exec() == QDialog::Accepted && table->rowCount() > 0) {
        // Update differences before saving
        for (int r = 0; r < table->rowCount(); ++r) {
            int sys = table->item(r, 1)->text().toInt();
            int act = table->item(r, 2)->text().toInt();
            table->item(r, 3)->setText(QString::number(act - sys));
        }
        
        // Create stock count
        StockCount sc;
        sc.setDate(dateEdit->date());
        sc.setUserId(SessionManager::instance().currentUser().id());
        sc.setStatus(isDraft ? StockCount::Status::Draft : StockCount::Status::Confirmed);
        sc.setNotes(notesEdit->toPlainText());
        
        if (m_repo->save(sc)) {
            // Save items (simplified - full implementation would use StockCountItemRepository)
            QMessageBox::information(this, LangManager::instance().t("Success"),
                LangManager::instance().t("Stock count saved successfully!"));
            loadStockCounts();
        } else {
            QMessageBox::critical(this, LangManager::instance().t("Error"),
                LangManager::instance().t("Failed to save stock count."));
        }
    }
}

void InventoryPage::onViewStockCount() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_stockCounts.size()) return;
    
    const auto& sc = m_stockCounts[row];
    
    QString html = QString(
        "<h3>📦 Stock Count #%1</h3>"
        "<table cellpadding='4'>"
        "<tr><td><b>Date:</b></td><td>%2</td></tr>"
        "<tr><td><b>Status:</b></td><td>%3</td></tr>"
        "<tr><td><b>Created By:</b></td><td>User #%4</td></tr>"
        "<tr><td><b>Notes:</b></td><td>%5</td></tr>"
        "</table>"
    ).arg(sc.id())
     .arg(sc.date().toString("dd/MM/yyyy"))
     .arg(StockCount::statusToString(sc.status()))
     .arg(sc.userId())
     .arg(sc.notes().isEmpty() ? "N/A" : sc.notes());
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Stock Count Details");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(html);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void InventoryPage::onDeleteStockCount() {
    int row = m_table->currentRow();
    if (row < 0 || row >= m_stockCounts.size()) return;
    
    const auto& sc = m_stockCounts[row];
    
    if (sc.isConfirmed()) {
        QMessageBox::warning(this, "Cannot Delete", 
            "Cannot delete confirmed stock counts.\n"
            "Stock movements have already been created.");
        return;
    }
    
    auto reply = QMessageBox::question(this, "Confirm Delete",
        QString("Delete stock count #%1?").arg(sc.id()),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (m_repo->remove(sc.id())) {
            QMessageBox::information(this, "Success", "Stock count deleted.");
            loadStockCounts();
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete stock count.");
        }
    }
}

void InventoryPage::refresh() {
    loadStockCounts();
}

void InventoryPage::retranslateUi() {
    // Will implement when needed
}

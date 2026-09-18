#include "expenses_page.h"
#include "services/lang_manager.h"
#include "services/session_manager.h"
#include "services/audit_service.h"
#include "services/theme_manager.h"
#include "infra/database_connection_manager.h"
#include "infra/logger.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QHeaderView>
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

ExpensesPage::ExpensesPage(QWidget* parent) : QWidget(parent) {
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    
    m_expenseRepo = useSqlite ? static_cast<IExpenseRepository*>(new SQLiteExpenseRepository)
                              : static_cast<IExpenseRepository*>(new AccessExpenseRepository);
    m_categoryRepo = useSqlite ? static_cast<IExpenseCategoryRepository*>(new SQLiteExpenseCategoryRepository)
                               : static_cast<IExpenseCategoryRepository*>(new AccessExpenseCategoryRepository);
    
    setupUi();
}

void ExpensesPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);
    
    auto* tabs = new QTabWidget;
    
    // ═══ Expenses Tab ═══
    auto* expensesTab = new QWidget;
    auto* expLayout = new QVBoxLayout(expensesTab);
    
    // Toolbar
    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QHBoxLayout(toolbar);
    
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(LangManager::instance().t("Search..."));
    m_searchEdit->setMaximumWidth(250);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ExpensesPage::onSearch);
    
    m_categoryFilterCombo = new QComboBox;
    m_categoryFilterCombo->addItem(LangManager::instance().t("All Categories"), 0);
    connect(m_categoryFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ExpensesPage::onSearch);
    
    m_dateFromEdit = new QDateEdit(QDate::currentDate().addMonths(-1));
    m_dateFromEdit->setCalendarPopup(true);
    m_dateFromEdit->setDisplayFormat("yyyy-MM-dd");
    connect(m_dateFromEdit, &QDateEdit::dateChanged, this, &ExpensesPage::onSearch);
    
    m_dateToEdit = new QDateEdit(QDate::currentDate());
    m_dateToEdit->setCalendarPopup(true);
    m_dateToEdit->setDisplayFormat("yyyy-MM-dd");
    connect(m_dateToEdit, &QDateEdit::dateChanged, this, &ExpensesPage::onSearch);
    
    m_addExpenseBtn = new QPushButton(LangManager::instance().t("＋ Add Expense"));
    m_addExpenseBtn->setObjectName("primaryBtn");
    connect(m_addExpenseBtn, &QPushButton::clicked, this, &ExpensesPage::onAddExpense);
    
    m_editExpenseBtn = new QPushButton(LangManager::instance().t("Edit"));
    m_editExpenseBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                                   QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editExpenseBtn->setObjectName("secondaryBtn");
    m_editExpenseBtn->setEnabled(false);
    connect(m_editExpenseBtn, &QPushButton::clicked, this, &ExpensesPage::onEditExpense);
    
    m_deleteExpenseBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteExpenseBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                     QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_deleteExpenseBtn->setObjectName("dangerBtn");
    m_deleteExpenseBtn->setEnabled(false);
    connect(m_deleteExpenseBtn, &QPushButton::clicked, this, &ExpensesPage::onDeleteExpense);
    
    tbLayout->addWidget(m_searchEdit);
    tbLayout->addWidget(new QLabel(LangManager::instance().t("Category:")));
    tbLayout->addWidget(m_categoryFilterCombo);
    tbLayout->addWidget(new QLabel(LangManager::instance().t("From:")));
    tbLayout->addWidget(m_dateFromEdit);
    tbLayout->addWidget(new QLabel(LangManager::instance().t("To:")));
    tbLayout->addWidget(m_dateToEdit);
    tbLayout->addStretch();
    tbLayout->addWidget(m_addExpenseBtn);
    tbLayout->addWidget(m_editExpenseBtn);
    tbLayout->addWidget(m_deleteExpenseBtn);
    
    expLayout->addWidget(toolbar);
    
    // Table
    m_expensesTable = new QTableWidget;
    m_expensesTable->setObjectName("dataTable");
    m_expensesTable->setColumnCount(6);
    m_expensesTable->setHorizontalHeaderLabels({
        LangManager::instance().t("Date"),
        LangManager::instance().t("Category"),
        LangManager::instance().t("Amount"),
        LangManager::instance().t("Description"),
        LangManager::instance().t("Receipt"),
        LangManager::instance().t("Created By")
    });
    m_expensesTable->horizontalHeader()->setStretchLastSection(true);
    m_expensesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_expensesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_expensesTable, &QTableWidget::itemSelectionChanged, [this]() {
        bool hasSelection = !m_expensesTable->selectedItems().isEmpty();
        m_editExpenseBtn->setEnabled(hasSelection);
        m_deleteExpenseBtn->setEnabled(hasSelection);
    });
    
    expLayout->addWidget(m_expensesTable);
    
    // Total
    m_totalLabel = new QLabel;
    m_totalLabel->setStyleSheet("font-size: 16px; font-weight: bold; padding: 10px;");
    expLayout->addWidget(m_totalLabel);
    
    tabs->addTab(expensesTab, LangManager::instance().t("💸 Expenses"));
    
    // ═══ Categories Tab ═══
    auto* categoriesTab = new QWidget;
    auto* catLayout = new QVBoxLayout(categoriesTab);
    
    auto* catToolbar = new QFrame;
    catToolbar->setObjectName("pageToolbar");
    auto* catTbLayout = new QHBoxLayout(catToolbar);
    
    m_addCategoryBtn = new QPushButton(LangManager::instance().t("＋ Add Category"));
    m_addCategoryBtn->setObjectName("primaryBtn");
    connect(m_addCategoryBtn, &QPushButton::clicked, this, &ExpensesPage::onAddCategory);
    
    m_editCategoryBtn = new QPushButton(LangManager::instance().t("Edit"));
    m_editCategoryBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("edit-svgrepo-com.svg"), 
                                                    QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_editCategoryBtn->setObjectName("secondaryBtn");
    m_editCategoryBtn->setEnabled(false);
    connect(m_editCategoryBtn, &QPushButton::clicked, this, &ExpensesPage::onEditCategory);
    
    m_deleteCategoryBtn = new QPushButton(LangManager::instance().t("Delete"));
    m_deleteCategoryBtn->setIcon(SvgIconHelper::icon(sidebarSvgPath("delete-recycle-bin-trash-can-svgrepo-com.svg"), 
                                                      QColor(ThemeManager::instance().tokens().textPrimary), 18));
    m_deleteCategoryBtn->setObjectName("dangerBtn");
    m_deleteCategoryBtn->setEnabled(false);
    connect(m_deleteCategoryBtn, &QPushButton::clicked, this, &ExpensesPage::onDeleteCategory);
    
    catTbLayout->addStretch();
    catTbLayout->addWidget(m_addCategoryBtn);
    catTbLayout->addWidget(m_editCategoryBtn);
    catTbLayout->addWidget(m_deleteCategoryBtn);
    
    catLayout->addWidget(catToolbar);
    
    m_categoriesTable = new QTableWidget;
    m_categoriesTable->setObjectName("dataTable");
    m_categoriesTable->setColumnCount(2);
    m_categoriesTable->setHorizontalHeaderLabels({
        LangManager::instance().t("Name"),
        LangManager::instance().t("Description")
    });
    m_categoriesTable->horizontalHeader()->setStretchLastSection(true);
    m_categoriesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_categoriesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_categoriesTable, &QTableWidget::itemSelectionChanged, [this]() {
        bool hasSelection = !m_categoriesTable->selectedItems().isEmpty();
        m_editCategoryBtn->setEnabled(hasSelection);
        m_deleteCategoryBtn->setEnabled(hasSelection);
    });
    
    catLayout->addWidget(m_categoriesTable);
    
    tabs->addTab(categoriesTab, LangManager::instance().t("📂 Categories"));
    
    root->addWidget(tabs);
    
    // Load data on page init
    QTimer::singleShot(0, this, &ExpensesPage::refresh);
}

void ExpensesPage::refresh() {
    loadCategories();
    loadExpenses();
}

void ExpensesPage::loadCategories() {
    m_categories = m_categoryRepo->getAll();
    
    // Update combo
    m_categoryFilterCombo->clear();
    m_categoryFilterCombo->addItem(LangManager::instance().t("All Categories"), 0);
    for (const auto& cat : m_categories) {
        m_categoryFilterCombo->addItem(cat.name(), cat.id());
    }
    
    // Update table
    m_categoriesTable->setRowCount(0);
    for (const auto& cat : m_categories) {
        int row = m_categoriesTable->rowCount();
        m_categoriesTable->insertRow(row);
        m_categoriesTable->setItem(row, 0, new QTableWidgetItem(cat.name()));
        m_categoriesTable->setItem(row, 1, new QTableWidgetItem(cat.description()));
    }
}

void ExpensesPage::loadExpenses() {
    m_expenses = m_expenseRepo->getAll();
    onSearch(); // Apply filters
}

void ExpensesPage::onSearch() {
    m_expensesTable->setRowCount(0);
    
    QString searchText = m_searchEdit->text().trimmed().toLower();
    int categoryId = m_categoryFilterCombo->currentData().toInt();
    QDate dateFrom = m_dateFromEdit->date();
    QDate dateTo = m_dateToEdit->date();
    
    double total = 0.0;
    
    for (const auto& exp : m_expenses) {
        // Apply filters
        if (categoryId > 0 && exp.categoryId() != categoryId) continue;
        if (exp.date() < dateFrom || exp.date() > dateTo) continue;
        if (!searchText.isEmpty() && !exp.description().toLower().contains(searchText)) continue;
        
        int row = m_expensesTable->rowCount();
        m_expensesTable->insertRow(row);
        
        auto* dateItem = new QTableWidgetItem(exp.date().toString("yyyy-MM-dd"));
        dateItem->setData(Qt::UserRole, exp.id());  // Store expense ID
        m_expensesTable->setItem(row, 0, dateItem);
        
        QString catName = "";
        for (const auto& cat : m_categories) {
            if (cat.id() == exp.categoryId()) {
                catName = cat.name();
                break;
            }
        }
        m_expensesTable->setItem(row, 1, new QTableWidgetItem(catName));
        m_expensesTable->setItem(row, 2, new QTableWidgetItem(QString::number(exp.amount(), 'f', 2)));
        m_expensesTable->setItem(row, 3, new QTableWidgetItem(exp.description()));
        m_expensesTable->setItem(row, 4, new QTableWidgetItem(exp.paymentMethod()));  // Show payment method
        m_expensesTable->setItem(row, 5, new QTableWidgetItem(QString::number(exp.userId())));
        
        total += exp.amount();
    }
    
    m_totalLabel->setText(QString("%1: %2 %3")
        .arg(LangManager::instance().t("Total"))
        .arg(QString::number(total, 'f', 2))
        .arg(LangManager::instance().t("ج.م")));
}

void ExpensesPage::onAddExpense() {
    QDialog dlg(this);
    dlg.setWindowTitle(LangManager::instance().t("Add Expense"));
    dlg.setMinimumWidth(500);
    
    auto* layout = new QFormLayout(&dlg);
    
    auto* categoryCombo = new QComboBox;
    for (const auto& cat : m_categories) {
        categoryCombo->addItem(cat.name(), cat.id());
    }
    
    auto* amountSpin = new QDoubleSpinBox;
    amountSpin->setRange(0, 999999);
    amountSpin->setDecimals(2);
    amountSpin->setSuffix(" " + LangManager::instance().t("ج.م"));
    
    auto* dateEdit = new QDateEdit(QDate::currentDate());
    dateEdit->setCalendarPopup(true);
    
    // Payment method
    auto* paymentCombo = new QComboBox;
    paymentCombo->addItem(LangManager::instance().t("Cash"), "Cash");
    paymentCombo->addItem(LangManager::instance().t("Card"), "Card");
    paymentCombo->addItem(LangManager::instance().t("Other"), "Other");
    
    auto* paymentOtherEdit = new QLineEdit;
    paymentOtherEdit->setPlaceholderText(LangManager::instance().t("Specify payment method..."));
    paymentOtherEdit->setEnabled(false);
    
    connect(paymentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int) {
        paymentOtherEdit->setEnabled(paymentCombo->currentData().toString() == "Other");
    });
    
    auto* descEdit = new QTextEdit;
    descEdit->setMaximumHeight(100);
    
    layout->addRow(LangManager::instance().t("Category:"), categoryCombo);
    layout->addRow(LangManager::instance().t("Amount:"), amountSpin);
    layout->addRow(LangManager::instance().t("Date:"), dateEdit);
    layout->addRow(LangManager::instance().t("Payment Method:"), paymentCombo);
    layout->addRow("", paymentOtherEdit);
    layout->addRow(LangManager::instance().t("Description:"), descEdit);
    
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addRow(btnBox);
    
    if (dlg.exec() == QDialog::Accepted) {
        QString paymentMethod = paymentCombo->currentData().toString();
        if (paymentMethod == "Other" && !paymentOtherEdit->text().trimmed().isEmpty()) {
            paymentMethod = paymentOtherEdit->text().trimmed();
        }
        
        Expense exp;
        exp.setCategoryId(categoryCombo->currentData().toInt());
        exp.setAmount(amountSpin->value());
        exp.setDate(dateEdit->date());
        exp.setPaymentMethod(paymentMethod);
        exp.setDescription(descEdit->toPlainText());
        exp.setUserId(SessionManager::instance().currentUser().id());
        
        if (m_expenseRepo->save(exp)) {
            AuditService::instance().logCreate("Expense", exp.id(),
                QString("Amount: %1").arg(exp.amount()));
            refresh();
        } else {
            QMessageBox::critical(this, LangManager::instance().t("Error"),
                LangManager::instance().t("Failed to save expense."));
        }
    }
}

void ExpensesPage::onEditExpense() {
    int row = m_expensesTable->currentRow();
    if (row < 0) return;
    
    int expenseId = m_expensesTable->item(row, 0)->data(Qt::UserRole).toInt();
    Expense existing;
    for (const auto& e : m_expenses) {
        if (e.id() == expenseId) {
            existing = e;
            break;
        }
    }
    if (existing.id() == 0) return;
    
    QDialog dlg(this);
    dlg.setWindowTitle(LangManager::instance().t("Edit Expense"));
    dlg.setMinimumWidth(500);
    
    auto* layout = new QFormLayout(&dlg);
    
    auto* categoryCombo = new QComboBox;
    for (const auto& cat : m_categories) {
        categoryCombo->addItem(cat.name(), cat.id());
    }
    int catIdx = categoryCombo->findData(existing.categoryId());
    if (catIdx >= 0) categoryCombo->setCurrentIndex(catIdx);
    
    auto* amountSpin = new QDoubleSpinBox;
    amountSpin->setRange(0, 999999);
    amountSpin->setDecimals(2);
    amountSpin->setSuffix(" " + LangManager::instance().t("ج.م"));
    amountSpin->setValue(existing.amount());
    
    auto* dateEdit = new QDateEdit(existing.date());
    dateEdit->setCalendarPopup(true);
    
    // Payment method
    auto* paymentCombo = new QComboBox;
    paymentCombo->addItem(LangManager::instance().t("Cash"), "Cash");
    paymentCombo->addItem(LangManager::instance().t("Card"), "Card");
    paymentCombo->addItem(LangManager::instance().t("Other"), "Other");
    
    // Set current payment method
    QString currentPayment = existing.paymentMethod();
    if (currentPayment == "Cash" || currentPayment == "Card") {
        int pmIdx = paymentCombo->findData(currentPayment);
        if (pmIdx >= 0) paymentCombo->setCurrentIndex(pmIdx);
    } else {
        paymentCombo->setCurrentIndex(paymentCombo->findData("Other"));
    }
    
    auto* paymentOtherEdit = new QLineEdit;
    paymentOtherEdit->setPlaceholderText(LangManager::instance().t("Specify payment method..."));
    if (currentPayment != "Cash" && currentPayment != "Card") {
        paymentOtherEdit->setText(currentPayment);
        paymentOtherEdit->setEnabled(true);
    } else {
        paymentOtherEdit->setEnabled(false);
    }
    
    connect(paymentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int) {
        paymentOtherEdit->setEnabled(paymentCombo->currentData().toString() == "Other");
    });
    
    auto* descEdit = new QTextEdit;
    descEdit->setMaximumHeight(100);
    descEdit->setPlainText(existing.description());
    
    layout->addRow(LangManager::instance().t("Category:"), categoryCombo);
    layout->addRow(LangManager::instance().t("Amount:"), amountSpin);
    layout->addRow(LangManager::instance().t("Date:"), dateEdit);
    layout->addRow(LangManager::instance().t("Payment Method:"), paymentCombo);
    layout->addRow("", paymentOtherEdit);
    layout->addRow(LangManager::instance().t("Description:"), descEdit);
    
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addRow(btnBox);
    
    if (dlg.exec() == QDialog::Accepted) {
        QString paymentMethod = paymentCombo->currentData().toString();
        if (paymentMethod == "Other" && !paymentOtherEdit->text().trimmed().isEmpty()) {
            paymentMethod = paymentOtherEdit->text().trimmed();
        }
        
        existing.setCategoryId(categoryCombo->currentData().toInt());
        existing.setAmount(amountSpin->value());
        existing.setDate(dateEdit->date());
        existing.setPaymentMethod(paymentMethod);
        existing.setDescription(descEdit->toPlainText());
        
        if (m_expenseRepo->save(existing)) {
            Logger::instance().info("Expense updated: " + QString::number(existing.id()));
            refresh();
        } else {
            QMessageBox::critical(this, LangManager::instance().t("Error"),
                LangManager::instance().t("Failed to update expense."));
        }
    }
}

void ExpensesPage::onDeleteExpense() {
    int row = m_expensesTable->currentRow();
    if (row < 0) return;
    
    int expenseId = m_expensesTable->item(row, 0)->data(Qt::UserRole).toInt();
    Expense expense;
    for (const auto& e : m_expenses) {
        if (e.id() == expenseId) {
            expense = e;
            break;
        }
    }
    if (expense.id() == 0) return;
    
    auto reply = QMessageBox::question(this, 
        LangManager::instance().t("Confirm Delete"),
        QString("Delete expense of %1?").arg(expense.amount()),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes && m_expenseRepo->remove(expense.id())) {
        AuditService::instance().logDelete("Expense", expense.id(), 
            QString("Amount: %1").arg(expense.amount()));
        refresh();
    }
}

void ExpensesPage::onAddCategory() {
    QDialog dlg(this);
    dlg.setWindowTitle(LangManager::instance().t("Add Category"));
    
    auto* layout = new QFormLayout(&dlg);
    
    auto* nameEdit = new QLineEdit;
    auto* descEdit = new QLineEdit;
    
    layout->addRow(LangManager::instance().t("Name:"), nameEdit);
    layout->addRow(LangManager::instance().t("Description:"), descEdit);
    
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addRow(btnBox);
    
    if (dlg.exec() == QDialog::Accepted) {
        ExpenseCategory cat;
        cat.setName(nameEdit->text());
        cat.setDescription(descEdit->text());
        
        if (m_categoryRepo->save(cat)) {
            refresh();
        } else {
            QMessageBox::critical(this, LangManager::instance().t("Error"),
                LangManager::instance().t("Failed to save category."));
        }
    }
}

void ExpensesPage::onEditCategory() {
    QMessageBox::information(this, "TODO", "Edit category - to be implemented");
}

void ExpensesPage::onDeleteCategory() {
    QMessageBox::information(this, "TODO", "Delete category - to be implemented");
}

void ExpensesPage::retranslateUi() {
    // TODO: Retranslate all UI elements
}

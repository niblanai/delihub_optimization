#ifndef EXPENSES_PAGE_H
#define EXPENSES_PAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QDateEdit>
#include <QComboBox>
#include <QTabWidget>
#include <QLabel>
#include "ui/translatable_page.h"
#include "data/irepositories.h"
#include "core/expense.h"
#include "core/expense_category.h"

class ExpensesPage : public QWidget, public TranslatablePage {
    Q_OBJECT
public:
    explicit ExpensesPage(QWidget* parent = nullptr);
    void refresh();
    void retranslateUi() override;

private slots:
    void onAddExpense();
    void onEditExpense();
    void onDeleteExpense();
    void onAddCategory();
    void onEditCategory();
    void onDeleteCategory();
    void onSearch();

private:
    void setupUi();
    void loadExpenses();
    void loadCategories();
    
    // Repositories
    IExpenseRepository* m_expenseRepo = nullptr;
    IExpenseCategoryRepository* m_categoryRepo = nullptr;
    
    // Data
    QList<Expense> m_expenses;
    QList<ExpenseCategory> m_categories;
    
    // UI - Expenses Tab
    QLineEdit* m_searchEdit = nullptr;
    QDateEdit* m_dateFromEdit = nullptr;
    QDateEdit* m_dateToEdit = nullptr;
    QComboBox* m_categoryFilterCombo = nullptr;
    QPushButton* m_addExpenseBtn = nullptr;
    QPushButton* m_editExpenseBtn = nullptr;
    QPushButton* m_deleteExpenseBtn = nullptr;
    QTableWidget* m_expensesTable = nullptr;
    QLabel* m_totalLabel = nullptr;
    
    // UI - Categories Tab
    QPushButton* m_addCategoryBtn = nullptr;
    QPushButton* m_editCategoryBtn = nullptr;
    QPushButton* m_deleteCategoryBtn = nullptr;
    QTableWidget* m_categoriesTable = nullptr;
};

#endif // EXPENSES_PAGE_H

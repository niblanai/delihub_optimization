#include "promotions_page.h"
#include "promotion_dialog.h"
#include "services/lang_manager.h"
#include "infra/database_connection_manager.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include <QHeaderView>
#include <QMessageBox>

PromotionsPage::PromotionsPage(QWidget* parent)
    : QWidget(parent)
{
    const bool useSqlite = DatabaseConnectionManager::instance().isSqliteFallbackActive()
                           || DatabaseConnectionManager::instance().connectionType() == "QSQLITE";
    
    m_promotionRepo = useSqlite ? static_cast<IPromotionRepository*>(new SQLitePromotionRepository)
                                : static_cast<IPromotionRepository*>(new AccessPromotionRepository);
    m_productRepo = useSqlite ? static_cast<IProductRepository*>(new SQLiteProductRepository)
                              : static_cast<IProductRepository*>(new AccessProductRepository);
    m_categoryRepo = useSqlite ? static_cast<ICategoryRepository*>(new SQLiteCategoryRepository)
                               : static_cast<ICategoryRepository*>(new AccessCategoryRepository);
    
    setupUi();
    loadPromotions();
    retranslateUi();
}

void PromotionsPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // Top bar with search and filter
    auto* topLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search promotions...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PromotionsPage::onFilterChanged);
    
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("All", -1);
    m_filterCombo->addItem("Active", static_cast<int>(Promotion::Status::Active));
    m_filterCombo->addItem("Inactive", static_cast<int>(Promotion::Status::Inactive));
    m_filterCombo->addItem("Scheduled", static_cast<int>(Promotion::Status::Scheduled));
    m_filterCombo->addItem("Expired", static_cast<int>(Promotion::Status::Expired));
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PromotionsPage::onFilterChanged);
    
    topLayout->addWidget(m_searchEdit);
    topLayout->addWidget(m_filterCombo);
    topLayout->addStretch();
    
    mainLayout->addLayout(topLayout);

    // Table
    m_tableView = new QTableView(this);
    m_model = new PromotionTableModel(this);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setAlternatingRowColors(true);
    
    connect(m_tableView, &QTableView::doubleClicked, this, &PromotionsPage::onEditPromotion);
    
    mainLayout->addWidget(m_tableView);

    // Bottom buttons
    auto* btnLayout = new QHBoxLayout();
    
    m_btnAdd = new QPushButton(this);
    m_btnEdit = new QPushButton(this);
    m_btnDelete = new QPushButton(this);
    m_btnRefresh = new QPushButton(this);
    
    connect(m_btnAdd, &QPushButton::clicked, this, &PromotionsPage::onAddPromotion);
    connect(m_btnEdit, &QPushButton::clicked, this, &PromotionsPage::onEditPromotion);
    connect(m_btnDelete, &QPushButton::clicked, this, &PromotionsPage::onDeletePromotion);
    connect(m_btnRefresh, &QPushButton::clicked, this, &PromotionsPage::onRefresh);
    
    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnEdit);
    btnLayout->addWidget(m_btnDelete);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnRefresh);
    
    mainLayout->addLayout(btnLayout);
}

void PromotionsPage::loadPromotions() {
    if (!m_promotionRepo) return;
    
    QList<Promotion> promotions = m_promotionRepo->getAll();
    m_model->setPromotions(promotions);
}

void PromotionsPage::onAddPromotion() {
    PromotionDialog dialog(m_promotionRepo, m_productRepo, m_categoryRepo, this);
    if (dialog.exec() == QDialog::Accepted) {
        loadPromotions();
    }
}

void PromotionsPage::onEditPromotion() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) return;
    
    Promotion promo = m_model->getPromotion(index.row());
    if (promo.id == 0) return;
    
    PromotionDialog dialog(m_promotionRepo, m_productRepo, m_categoryRepo, this);
    dialog.setPromotion(promo);
    
    if (dialog.exec() == QDialog::Accepted) {
        loadPromotions();
    }
}

void PromotionsPage::onDeletePromotion() {
    QModelIndex index = m_tableView->currentIndex();
    if (!index.isValid()) return;
    
    Promotion promo = m_model->getPromotion(index.row());
    if (promo.id == 0) return;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        LangManager::tr("delete_promotion"),
        LangManager::tr("delete_promotion_confirm").arg(promo.title),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        if (m_promotionRepo->remove(promo.id)) {
            loadPromotions();
            QMessageBox::information(this, 
                                   LangManager::tr("success"), 
                                   LangManager::tr("promotion_deleted"));
        } else {
            QMessageBox::critical(this, 
                                LangManager::tr("error"), 
                                LangManager::tr("promotion_delete_failed"));
        }
    }
}

void PromotionsPage::onRefresh() {
    loadPromotions();
}

void PromotionsPage::onFilterChanged() {
    // TODO: Implement filtering logic
    loadPromotions();
}

void PromotionsPage::retranslateUi() {
    m_btnAdd->setText(LangManager::tr("add_promotion"));
    m_btnEdit->setText(LangManager::tr("edit_promotion"));
    m_btnDelete->setText(LangManager::tr("delete_promotion"));
    m_btnRefresh->setText(LangManager::tr("refresh"));
    m_searchEdit->setPlaceholderText(LangManager::tr("search_promotions"));
}

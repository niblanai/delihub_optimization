#include "missing_product_dialog.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QCoreApplication>
#include <QFile>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

MissingProductDialog::MissingProductDialog(const QList<Product>& products,
                                           const QList<MissingProduct>& existingMissing,
                                           QWidget* parent)
    : QDialog(parent), m_products(products), m_existingMissing(existingMissing)
{
    setupUi();
}

void MissingProductDialog::setupUi() {
    setWindowTitle("Report Missing Product");
    setMinimumWidth(440);
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    // ── Product search group ──────────────────────────────────────────────────
    auto* searchGroup = new QGroupBox("Find Product from Catalog *");
    auto* sg = new QVBoxLayout(searchGroup);
    sg->setSpacing(6);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Type product name or barcode...");
    m_searchEdit->setObjectName("searchEdit");
    
    // Add search icon
    QAction* searchAction = new QAction(m_searchEdit);
    searchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                               QColor("#9CA3AF"), 16));
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);
    
    sg->addWidget(m_searchEdit);

    // Floating popup — parented to dialog so it overlays correctly
    m_searchPopup = new QListWidget(this);
    m_searchPopup->setWindowFlags(Qt::ToolTip);
    m_searchPopup->setFocusPolicy(Qt::NoFocus);
    m_searchPopup->setMaximumHeight(160);
    m_searchPopup->setVisible(false);
    m_searchPopup->setObjectName("dataTable");

    // Selected product display (read-only)
    auto* selFrame = new QFrame;
    selFrame->setObjectName("kpiCard");
    auto* selLayout = new QFormLayout(selFrame);
    selLayout->setSpacing(4);
    selLayout->setContentsMargins(10, 8, 10, 8);

    m_productNameLbl = new QLabel("—");
    m_productNameLbl->setStyleSheet("font-weight:bold; color:#E2E8F0;");
    m_barcodeValLbl  = new QLabel("—");
    m_barcodeValLbl->setStyleSheet("color:#94A3B8; font-family:monospace;");
    m_stockValLbl    = new QLabel("—");
    m_stockValLbl->setStyleSheet("color:#4ADE80; font-weight:bold;");
    selLayout->addRow("Name:",          m_productNameLbl);
    selLayout->addRow("Barcode:",       m_barcodeValLbl);
    selLayout->addRow("Current Stock:", m_stockValLbl);
    sg->addWidget(selFrame);

    root->addWidget(searchGroup);

    // ── Quantity + Unit ───────────────────────────────────────────────────────
    auto* qtyGroup  = new QGroupBox("Quantity Required");
    auto* qtyLayout = new QHBoxLayout(qtyGroup);
    qtyLayout->setSpacing(12);

    m_qtySpin = new QSpinBox;
    m_qtySpin->setRange(1, 99999);
    m_qtySpin->setValue(1);
    m_qtySpin->setFixedWidth(100);
    qtyLayout->addWidget(m_qtySpin);

    m_unitCombo = new QComboBox;
    m_unitCombo->addItem("قطعة",    "قطعة");
    m_unitCombo->addItem("علبة",     "علبة");
    m_unitCombo->addItem("كرتونة",  "كرتونة");
    m_unitCombo->addItem("كيلو",     "كيلو");
    m_unitCombo->addItem("لتر",      "لتر");
    m_unitCombo->addItem("باكيت",    "باكيت");
    m_unitCombo->setMinimumWidth(120);
    qtyLayout->addWidget(m_unitCombo);
    qtyLayout->addStretch();

    root->addWidget(qtyGroup);

    // ── Buttons ───────────────────────────────────────────────────────────────
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    root->addWidget(m_buttons);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_searchEdit,  &QLineEdit::textChanged,
            this, &MissingProductDialog::onSearchChanged);
    connect(m_searchPopup, &QListWidget::itemClicked,
            this, &MissingProductDialog::onProductSelected);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &MissingProductDialog::validate);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void MissingProductDialog::onSearchChanged(const QString& text) {
    m_searchPopup->clear();
    QString lower = text.trimmed().toLower();
    if (lower.isEmpty()) { m_searchPopup->hide(); return; }

    int count = 0;
    for (const auto& p : m_products) {
        bool nameMatch    = p.name().toLower().contains(lower);
        bool barcodeMatch = !p.barcode().isEmpty() && p.barcode().contains(text.trimmed());
        if (nameMatch || barcodeMatch) {
            QString label = p.barcode().isEmpty()
                ? p.name()
                : QString("%1  [%2]").arg(p.name(), p.barcode());
            auto* it = new QListWidgetItem(label);
            it->setData(Qt::UserRole,     p.id());
            it->setData(Qt::UserRole + 1, p.name());
            it->setData(Qt::UserRole + 2, p.barcode());
            it->setData(Qt::UserRole + 3, p.stockQty());   // Issue 1
            m_searchPopup->addItem(it);
            if (++count >= 10) break;
        }
    }
    if (count > 0) {
        QPoint pos = m_searchEdit->mapToGlobal(QPoint(0, m_searchEdit->height()));
        m_searchPopup->move(pos);
        m_searchPopup->setFixedWidth(qMax(m_searchEdit->width(), 300));
        m_searchPopup->raise();
        m_searchPopup->show();
    } else {
        m_searchPopup->hide();
    }
}

void MissingProductDialog::onProductSelected(QListWidgetItem* item) {
    if (!item) return;
    m_productId    = item->data(Qt::UserRole).toInt();
    QString name    = item->data(Qt::UserRole + 1).toString();
    QString barcode = item->data(Qt::UserRole + 2).toString();
    int     stockQty= item->data(Qt::UserRole + 3).toInt();

    m_searchEdit->blockSignals(true);
    m_searchEdit->setText(name);
    m_searchEdit->blockSignals(false);
    m_searchPopup->hide();

    m_productNameLbl->setText(name.isEmpty()    ? "—" : name);
    m_barcodeValLbl->setText(barcode.isEmpty()  ? "—" : barcode);
    // Issue 1: show current stock from catalog
    m_stockValLbl->setText(QString::number(stockQty));
}

void MissingProductDialog::setMissingProduct(const MissingProduct& mp) {
    m_editId    = mp.id;
    m_productId = mp.productId;

    // Pre-fill search box with product name
    m_searchEdit->blockSignals(true);
    m_searchEdit->setText(mp.productName);
    m_searchEdit->blockSignals(false);

    m_productNameLbl->setText(mp.productName.isEmpty() ? "—" : mp.productName);
    m_barcodeValLbl->setText(mp.barcode.isEmpty() ? "—" : mp.barcode);

    // Issue 1: populate current stock from catalog if productId is known
    if (mp.productId > 0) {
        for (const auto& p : m_products) {
            if (p.id() == mp.productId) {
                m_stockValLbl->setText(QString::number(p.stockQty()));
                break;
            }
        }
    } else {
        m_stockValLbl->setText(mp.currentQty > 0 ? QString::number(mp.currentQty) : "—");
    }

    m_qtySpin->setValue(qMax(1, mp.quantityNeeded));

    // Select unit
    int idx = m_unitCombo->findData(mp.unitLabel);
    if (idx >= 0)
        m_unitCombo->setCurrentIndex(idx);
}

MissingProduct MissingProductDialog::getMissingProduct() const {
    MissingProduct item;
    item.id             = m_editId;
    item.productId      = m_productId;
    item.productName    = m_productNameLbl->text() == "—" ? m_searchEdit->text().trimmed()
                                                           : m_productNameLbl->text();
    item.barcode        = m_barcodeValLbl->text() == "—"  ? QString()
                                                           : m_barcodeValLbl->text();
    item.quantityNeeded = m_qtySpin->value();
    item.unitLabel      = m_unitCombo->currentData().toString();
    item.dateAdded      = QDate::currentDate();
    item.purchased      = false;
    item.source         = "manual";
    // BUG 1 FIX: read stock value back from the label so it's saved to the DB
    QString stockText = m_stockValLbl ? m_stockValLbl->text() : "—";
    item.currentQty = (stockText != "—" && !stockText.isEmpty())
        ? stockText.toInt() : 0;
    return item;
}

void MissingProductDialog::validate() {
    if (m_productId <= 0 && m_searchEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation",
            "Please search for and select a product from the catalog.");
        m_searchEdit->setFocus();
        return;
    }

    // ── Check: already exists in Missing Products list? ───────────────────────
    if (m_editId == 0 && m_productId > 0) {
        for (const auto& existing : m_existingMissing) {
            if (existing.productId == m_productId && !existing.purchased) {
                // Found a pending request for the same product
                auto reply = QMessageBox::question(
                    this,
                    "Product Already in Missing List",
                    QString("\"<b>%1</b>\" is already in the Missing Products list "
                            "with Qty: %2 %3.<br><br>"
                            "What would you like to do?")
                        .arg(existing.productName.isEmpty()
                             ? m_productNameLbl->text() : existing.productName)
                        .arg(existing.quantityNeeded)
                        .arg(existing.unitLabel),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    QMessageBox::No);

                // Yes = Replace existing | No = Add another anyway | Cancel = abort
                if (reply == QMessageBox::Cancel) return;  // don't close dialog

                if (reply == QMessageBox::Yes) {
                    m_conflictAction     = ConflictAction::Replace;
                    m_conflictExistingId = existing.id;
                } else {
                    m_conflictAction = ConflictAction::Add;
                }
                break;
            }
        }
    }

    accept();
}

#include "product_quick_edit_dialog.h"
#include "infra/logger.h"
#include "services/audit_service.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

ProductQuickEditDialog::ProductQuickEditDialog(int productId,
                                               IProductRepository* repo,
                                               QWidget* parent)
    : QDialog(parent), m_productId(productId), m_repo(repo)
{
    // Load fresh from DB before building UI so fields are pre-filled
    if (m_repo) m_product = m_repo->getById(productId);
    setupUi();
}

void ProductQuickEditDialog::setupUi() {
    setWindowTitle(m_product.id() > 0
        ? QString("Quick Edit — %1").arg(m_product.name().isEmpty()
              ? QString("Product #%1").arg(m_productId) : m_product.name())
        : "Quick Edit Product");
    setMinimumWidth(400);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    // Hint banner — shown when the name is empty so the user knows why they
    // ended up here
    if (m_product.name().isEmpty()) {
        auto* hint = new QLabel(
            "⚠️  This product has no name on record.\n"
            "Please fill in the name to identify it correctly.");
        hint->setWordWrap(true);
        hint->setStyleSheet(
            "background:#451A03; color:#FCD34D; border-radius:6px;"
            "padding:8px 12px; font-size:12px;");
        layout->addWidget(hint);
    }

    auto* form = new QFormLayout;
    form->setSpacing(10);

    m_nameEdit = new QLineEdit(m_product.name());
    m_nameEdit->setPlaceholderText("Product name...");
    form->addRow("Name *", m_nameEdit);

    m_barcodeEdit = new QLineEdit(m_product.barcode());
    m_barcodeEdit->setPlaceholderText("Barcode (optional)...");
    form->addRow("Barcode", m_barcodeEdit);

    m_qtySpin = new QSpinBox;
    m_qtySpin->setRange(0, 999999);
    m_qtySpin->setValue(m_product.stockQty());
    form->addRow("Stock Qty", m_qtySpin);

    layout->addLayout(form);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &ProductQuickEditDialog::onSave);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ProductQuickEditDialog::onSave() {
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Validation", "Product name cannot be empty.");
        m_nameEdit->setFocus();
        return;
    }

    // Capture before-state for audit
    QString oldName    = m_product.name();
    QString oldBarcode = m_product.barcode();
    int     oldQty     = m_product.stockQty();

    m_product.setName(name);
    m_product.setBarcode(m_barcodeEdit->text().trimmed());
    m_product.setStockQty(m_qtySpin->value());

    if (!m_repo->save(m_product)) {
        QMessageBox::critical(this, "Save Failed",
            "Could not update the product. Please try again.");
        return;
    }

    m_saved = true;
    Logger::instance().info(QString("ProductQuickEdit: product #%1 updated"
        " name='%2' barcode='%3' qty=%4")
        .arg(m_productId).arg(name)
        .arg(m_product.barcode()).arg(m_product.stockQty()));

    // Audit log: only log if something actually changed
    if (name != oldName || m_product.barcode() != oldBarcode || m_product.stockQty() != oldQty) {
        QString details = QString(
            R"({"before":{"name":"%1","barcode":"%2","stockQty":%3},"after":{"name":"%4","barcode":"%5","stockQty":%6}})")
            .arg(oldName, oldBarcode).arg(oldQty)
            .arg(name, m_product.barcode()).arg(m_product.stockQty());
        AuditService::instance().logUpdate("Product", m_productId, details);
    }

    accept();
}

#include "ui/orders/invoice_review_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
InvoiceReviewDialog::InvoiceReviewDialog(const QList<InvoiceItem>& parsed,
                                          IProductRepository*        productRepo,
                                          QWidget*                   parent)
    : QDialog(parent)
{
    setWindowTitle("📄 مراجعة الفاتورة");
    setMinimumWidth(620);
    setMinimumHeight(380);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_items = matchItems(parsed, productRepo);
    setupUi(parsed, productRepo);
}

// ─────────────────────────────────────────────────────────────────────────────
// Match PDF names against the product database
// ─────────────────────────────────────────────────────────────────────────────
QList<ReviewedItem> InvoiceReviewDialog::matchItems(const QList<InvoiceItem>& parsed,
                                                     IProductRepository*        productRepo)
{
    // Load all products once for matching.
    const QList<Product> allProducts = productRepo->getAll();

    // Build a map: product name (lower) → Product for fast lookup.
    QMap<QString, Product> nameMap;
    for (const Product& p : allProducts)
        nameMap[p.name().trimmed().toLower()] = p;

    QList<ReviewedItem> result;
    for (const InvoiceItem& item : parsed) {
        ReviewedItem ri;
        ri.pdfName = item.name;
        ri.qty     = item.qty;
        ri.productId   = 0;
        ri.matchedName = "";

        // 1. Exact match (case-insensitive, trimmed)
        const QString key = item.name.trimmed().toLower();
        if (nameMap.contains(key)) {
            const Product& p = nameMap[key];
            ri.productId   = p.id();
            ri.matchedName = p.name();
        } else {
            // 2. Partial / contains match — find first product whose name
            //    contains the PDF name or vice-versa.
            for (const Product& p : allProducts) {
                const QString pnLow = p.name().trimmed().toLower();
                if (pnLow.contains(key) || key.contains(pnLow)) {
                    ri.productId   = p.id();
                    ri.matchedName = p.name();
                    break;
                }
            }
        }

        result.append(ri);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// UI
// ─────────────────────────────────────────────────────────────────────────────
void InvoiceReviewDialog::setupUi(const QList<InvoiceItem>&, IProductRepository*) {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    // ── Title / hint ─────────────────────────────────────────────────────────
    auto* hint = new QLabel("راجع المنتجات المستخرجة من الفاتورة ثم اضغط \"إضافة للطلب\".");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #94A3B8; font-size: 12px;");
    root->addWidget(hint);

    // ── Table ─────────────────────────────────────────────────────────────────
    // Columns: حالة | اسم المنتج (PDF) | المنتج المطابق | الكمية
    m_table = new QTableWidget(0, 4, this);
    m_table->setObjectName("dataTable");
    m_table->setHorizontalHeaderLabels({"", "اسم المنتج في الفاتورة", "المنتج المطابق", "الكمية"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(false);
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);

    int matched = 0;
    for (const ReviewedItem& ri : m_items) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        bool ok = ri.productId > 0;
        if (ok) ++matched;

        // Column 0: status icon
        auto* statusItem = new QTableWidgetItem(ok ? "✅" : "⚠️");
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, statusItem);

        // Column 1: PDF name
        auto* pdfNameItem = new QTableWidgetItem(ri.pdfName);
        pdfNameItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 1, pdfNameItem);

        // Column 2: matched product name (or warning)
        QString matchText = ok ? ri.matchedName : "⚠️ لم يُطابَق — سيُتجاهل";
        auto* matchItem = new QTableWidgetItem(matchText);
        matchItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (!ok) matchItem->setForeground(QBrush(QColor("#F87171")));
        m_table->setItem(row, 2, matchItem);

        // Column 3: editable quantity (QDoubleSpinBox)
        auto* qtySpin = new QDoubleSpinBox;
        qtySpin->setMinimum(0.001);
        qtySpin->setMaximum(99999);
        qtySpin->setDecimals(3);
        qtySpin->setValue(ri.qty);
        qtySpin->setEnabled(ok);
        qtySpin->setAlignment(Qt::AlignCenter);
        qtySpin->setProperty("rowIndex", row);
        // Keep m_items in sync when user edits quantity
        connect(qtySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, row](double v) {
                    if (row < m_items.size()) m_items[row].qty = v;
                });
        m_table->setCellWidget(row, 3, qtySpin);

        m_table->setRowHeight(row, 38);
    }

    root->addWidget(m_table, 1);

    // ── Stats label ───────────────────────────────────────────────────────────
    m_statsLabel = new QLabel;
    int total    = m_items.size();
    int unmatched = total - matched;
    QString statsText = QString("تم استخراج %1 منتج — ✅ %2 مطابق  ⚠️ %3 غير مطابق (سيُتجاهل)")
                            .arg(total).arg(matched).arg(unmatched);
    m_statsLabel->setText(statsText);
    m_statsLabel->setStyleSheet("font-size: 12px; color: #94A3B8;");
    root->addWidget(m_statsLabel);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* btnBox = new QDialogButtonBox(this);
    auto* addBtn = new QPushButton("✅  إضافة للطلب");
    addBtn->setObjectName("primaryBtn");
    addBtn->setDefault(true);
    auto* cancelBtn = new QPushButton("إلغاء");
    cancelBtn->setObjectName("secondaryBtn");
    btnBox->addButton(addBtn,    QDialogButtonBox::AcceptRole);
    btnBox->addButton(cancelBtn, QDialogButtonBox::RejectRole);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btnBox);
}

// ─────────────────────────────────────────────────────────────────────────────
// Return only matched items
// ─────────────────────────────────────────────────────────────────────────────
QList<ReviewedItem> InvoiceReviewDialog::acceptedItems() const {
    QList<ReviewedItem> result;
    for (const ReviewedItem& ri : m_items)
        if (ri.productId > 0) result.append(ri);
    return result;
}

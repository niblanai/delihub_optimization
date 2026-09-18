#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QLabel>
#include "services/invoice_parser.h"
#include "data/irepositories.h"

// ─────────────────────────────────────────────────────────────────────────────
// InvoiceReviewDialog
// Shows the items extracted from a PDF invoice so the user can verify/edit
// before they are added to the order.
//
// Each row shows:
//   ✅ / ⚠️  |  اسم المنتج (from PDF)  |  الكمية  |  حالة المطابقة
//
// "حالة المطابقة": whether the name was found exactly in the product DB.
// ─────────────────────────────────────────────────────────────────────────────

struct ReviewedItem {
    QString pdfName;   // name as read from the PDF
    double  qty;       // quantity from PDF
    int     productId; // matched product id (0 = not found)
    QString matchedName; // product name as stored in DB (empty if not found)
};

class InvoiceReviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit InvoiceReviewDialog(const QList<InvoiceItem>& parsed,
                                  IProductRepository*        productRepo,
                                  QWidget*                   parent = nullptr);

    // Items accepted by the user (only rows with productId > 0 are included).
    QList<ReviewedItem> acceptedItems() const;

private:
    void setupUi(const QList<InvoiceItem>& parsed, IProductRepository* productRepo);
    QList<ReviewedItem> matchItems(const QList<InvoiceItem>& parsed,
                                   IProductRepository*        productRepo);

    QTableWidget* m_table  = nullptr;
    QLabel*       m_statsLabel = nullptr;

    QList<ReviewedItem> m_items; // all items (matched + unmatched)
};

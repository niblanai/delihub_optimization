#ifndef RECEIPT_PREVIEW_DIALOG_H
#define RECEIPT_PREVIEW_DIALOG_H

#include <QDialog>
#include <QTextBrowser>
#include <QPushButton>
#include "core/order.h"
#include "core/register_session.h"
#include "core/receipt_template.h"

class ReceiptPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReceiptPreviewDialog(const Order& order, const RegisterSession& session,
                                 const QString& paymentMethod, QWidget* parent = nullptr);

private slots:
    void onPrint();

private:
    void setupUi();
    QString generateHtml();

    Order m_order;
    RegisterSession m_session;
    QString m_paymentMethod;
    QString m_html;   // the exact HTML fed to m_browser — reused for printing so
                       // print output matches the preview instead of round-tripping
                       // through QTextBrowser::toHtml(), which re-serializes the
                       // document using Qt's own simplified CSS subset and can lose
                       // layout (e.g. it doesn't understand display:flex at all).
    QTextBrowser* m_browser = nullptr;
    QPushButton* m_printBtn = nullptr;
};

#endif // RECEIPT_PREVIEW_DIALOG_H

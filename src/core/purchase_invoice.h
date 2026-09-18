#ifndef PURCHASE_INVOICE_H
#define PURCHASE_INVOICE_H

#include <QString>
#include <QDate>

class PurchaseInvoice {
public:
    enum class Status { Draft, Confirmed };

    PurchaseInvoice() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int supplierId() const { return m_supplierId; }
    void setSupplierId(int sid) { m_supplierId = sid; }

    QString invoiceNumber() const { return m_invoiceNumber; }
    void setInvoiceNumber(const QString& num) { m_invoiceNumber = num; }

    QDate date() const { return m_date; }
    void setDate(const QDate& d) { m_date = d; }

    double totalAmount() const { return m_totalAmount; }
    void setTotalAmount(double amount) { m_totalAmount = amount; }

    double paidAmount() const { return m_paidAmount; }
    void setPaidAmount(double amount) { m_paidAmount = amount; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& n) { m_notes = n; }

    int userId() const { return m_userId; }
    void setUserId(int uid) { m_userId = uid; }

    Status status() const { return m_status; }
    void setStatus(Status s) { m_status = s; }

    // Helpers
    bool isDraft() const { return m_status == Status::Draft; }
    bool isConfirmed() const { return m_status == Status::Confirmed; }

    static QString statusToString(Status s) {
        return (s == Status::Confirmed) ? "Confirmed" : "Draft";
    }
    static Status stringToStatus(const QString& s) {
        return (s.compare("Confirmed", Qt::CaseInsensitive) == 0) ? Status::Confirmed : Status::Draft;
    }

private:
    int     m_id            = 0;
    int     m_supplierId    = 0;
    QString m_invoiceNumber;
    QDate   m_date;
    double  m_totalAmount   = 0.0;
    double  m_paidAmount    = 0.0;
    QString m_notes;
    int     m_userId        = 0;
    Status  m_status        = Status::Draft;
};

#endif // PURCHASE_INVOICE_H

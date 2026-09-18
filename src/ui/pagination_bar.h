#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QComboBox>

// ── Reusable pagination bar ───────────────────────────────────────────────────
// Usage: create once, wire to pageChanged signal, call setTotalItems() after loading.
class PaginationBar : public QWidget {
    Q_OBJECT
public:
    explicit PaginationBar(QWidget* parent = nullptr) : QWidget(parent) {
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(16, 4, 16, 4);
        lay->setSpacing(8);

        m_prevBtn = new QPushButton("‹");
        m_prevBtn->setObjectName("iconBtn");
        m_prevBtn->setFixedSize(28, 28);
        m_prevBtn->setToolTip("Previous page");

        m_nextBtn = new QPushButton("›");
        m_nextBtn->setObjectName("iconBtn");
        m_nextBtn->setFixedSize(28, 28);
        m_nextBtn->setToolTip("Next page");

        m_infoLabel = new QLabel;
        m_infoLabel->setObjectName("statusLabel");

        // Page size selector
        auto* psLabel = new QLabel("Rows:");
        psLabel->setObjectName("statusLabel");
        m_pageSizeCombo = new QComboBox;
        m_pageSizeCombo->setFixedWidth(72);
        m_pageSizeCombo->addItem("25",  25);
        m_pageSizeCombo->addItem("50",  50);
        m_pageSizeCombo->addItem("100", 100);
        m_pageSizeCombo->addItem("200", 200);

        lay->addWidget(m_prevBtn);
        lay->addWidget(m_infoLabel);
        lay->addWidget(m_nextBtn);
        lay->addStretch();
        lay->addWidget(psLabel);
        lay->addWidget(m_pageSizeCombo);

        connect(m_prevBtn, &QPushButton::clicked, this, [this](){ setPage(m_page - 1); });
        connect(m_nextBtn, &QPushButton::clicked, this, [this](){ setPage(m_page + 1); });
        connect(m_pageSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() { m_page = 0; updateUi(); emit pageChanged(offset(), pageSize()); });
    }

    void setTotalItems(int total) {
        m_total = total;
        if (m_page > lastPage()) m_page = lastPage();
        updateUi();
    }

    int offset()   const { return m_page * pageSize(); }
    int pageSize() const { return m_pageSizeCombo->currentData().toInt(); }
    int page()     const { return m_page; }

    // Call this when data is loaded from the outside (e.g. search resets page)
    void resetToFirst() { m_page = 0; updateUi(); }

signals:
    void pageChanged(int offset, int pageSize);

private:
    void setPage(int p) {
        p = qMax(0, qMin(p, lastPage()));
        if (p == m_page) return;
        m_page = p;
        updateUi();
        emit pageChanged(offset(), pageSize());
    }

    int lastPage() const {
        int n = pageSize();
        return n > 0 ? qMax(0, (m_total - 1) / n) : 0;
    }

    void updateUi() {
        int from = m_total == 0 ? 0 : offset() + 1;
        int to   = qMin(offset() + pageSize(), m_total);
        m_infoLabel->setText(QString("Showing %1–%2 of %3").arg(from).arg(to).arg(m_total));
        m_prevBtn->setEnabled(m_page > 0);
        m_nextBtn->setEnabled(m_page < lastPage());
    }

    QPushButton* m_prevBtn     = nullptr;
    QPushButton* m_nextBtn     = nullptr;
    QLabel*      m_infoLabel   = nullptr;
    QComboBox*   m_pageSizeCombo = nullptr;
    int m_page  = 0;
    int m_total = 0;
};

#include "numeric_keypad.h"
#include <QVBoxLayout>

NumericKeypad::NumericKeypad(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Display (read-only)
    m_display = new QLineEdit;
    m_display->setReadOnly(true);
    m_display->setAlignment(Qt::AlignRight);
    m_display->setPlaceholderText("0.00");
    m_display->setStyleSheet(
        "QLineEdit {"
        "  padding: 8px;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  border: 2px solid #D1D5DB;"
        "  border-radius: 6px;"
        "  background: #F9FAFB;"
        "  color: #1F2937;"
        "}"
    );
    layout->addWidget(m_display);
    
    // Keypad grid (4 rows x 4 columns)
    auto* grid = new QGridLayout;
    grid->setSpacing(3);
    grid->setContentsMargins(0, 0, 0, 0);
    
    // Row 1: 1, 2, 3, Qty
    grid->addWidget(makeButton("1"), 0, 0);
    grid->addWidget(makeButton("2"), 0, 1);
    grid->addWidget(makeButton("3"), 0, 2);
    auto* qtyBtn = makeButton("Qty", "#DCFCE7");
    connect(qtyBtn, &QPushButton::clicked, this, &NumericKeypad::qtyClicked);
    grid->addWidget(qtyBtn, 0, 3);
    
    // Row 2: 4, 5, 6, %
    grid->addWidget(makeButton("4"), 1, 0);
    grid->addWidget(makeButton("5"), 1, 1);
    grid->addWidget(makeButton("6"), 1, 2);
    auto* pctBtn = makeButton("%", "#DCFCE7");
    connect(pctBtn, &QPushButton::clicked, this, &NumericKeypad::percentClicked);
    grid->addWidget(pctBtn, 1, 3);
    
    // Row 3: 7, 8, 9, Price
    grid->addWidget(makeButton("7"), 2, 0);
    grid->addWidget(makeButton("8"), 2, 1);
    grid->addWidget(makeButton("9"), 2, 2);
    auto* priceBtn = makeButton("Price", "#DCFCE7");
    connect(priceBtn, &QPushButton::clicked, this, &NumericKeypad::priceClicked);
    grid->addWidget(priceBtn, 2, 3);
    
    // Row 4: +/-, 0, ., ⌫ (backspace)
    auto* signBtn = makeButton("+/-", "#FEF3C7");
    connect(signBtn, &QPushButton::clicked, this, &NumericKeypad::onToggleSign);
    grid->addWidget(signBtn, 3, 0);
    
    grid->addWidget(makeButton("0"), 3, 1);
    
    auto* dotBtn = makeButton(".", "#FED7AA");
    connect(dotBtn, &QPushButton::clicked, this, &NumericKeypad::onDecimal);
    grid->addWidget(dotBtn, 3, 2);
    
    auto* backBtn = makeButton("⌫", "#FECACA");
    connect(backBtn, &QPushButton::clicked, this, &NumericKeypad::onBackspace);
    grid->addWidget(backBtn, 3, 3);
    
    layout->addLayout(grid);
    
    setMaximumHeight(220);
    setMinimumHeight(220);
}

QPushButton* NumericKeypad::makeButton(const QString& text, const QString& bgColor) {
    auto* btn = new QPushButton(text);
    btn->setMinimumHeight(36);
    btn->setMaximumHeight(36);
    btn->setMinimumWidth(70);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1;"
        "  color: #1F2937;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "  border: 1px solid #E5E7EB;"
        "  border-radius: 5px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  background: #F3F4F6;"
        "  border-color: #9CA3AF;"
        "}"
        "QPushButton:pressed {"
        "  background: #E5E7EB;"
        "}"
    ).arg(bgColor));
    
    // Connect number buttons
    if (text.length() == 1 && text[0].isDigit()) {
        connect(btn, &QPushButton::clicked, this, &NumericKeypad::onNumberClick);
    }
    
    return btn;
}

void NumericKeypad::onNumberClick() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    
    QString current = m_display->text();
    if (current == "0" || current.isEmpty()) {
        m_display->setText(btn->text());
    } else {
        m_display->setText(current + btn->text());
    }
    emit valueChanged(m_display->text());
}

void NumericKeypad::onBackspace() {
    QString current = m_display->text();
    if (current.isEmpty()) return;
    
    current.chop(1);
    m_display->setText(current.isEmpty() ? "0" : current);
    emit valueChanged(m_display->text());
}

void NumericKeypad::onToggleSign() {
    QString current = m_display->text();
    if (current.isEmpty() || current == "0") return;
    
    if (current.startsWith("-")) {
        m_display->setText(current.mid(1));
    } else {
        m_display->setText("-" + current);
    }
    emit valueChanged(m_display->text());
}

void NumericKeypad::onDecimal() {
    QString current = m_display->text();
    if (current.isEmpty()) {
        m_display->setText("0.");
    } else if (!current.contains(".")) {
        m_display->setText(current + ".");
    }
    emit valueChanged(m_display->text());
}

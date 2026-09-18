#pragma once
#include <QWidget>
#include <QPushButton>
#include <QGridLayout>
#include <QLineEdit>

class NumericKeypad : public QWidget {
    Q_OBJECT
public:
    explicit NumericKeypad(QWidget* parent = nullptr);
    
    QString value() const { return m_display->text(); }
    void setValue(const QString& val) { m_display->setText(val); }
    void clear() { m_display->clear(); }
    
signals:
    void valueChanged(const QString& value);
    void qtyClicked();
    void percentClicked();
    void priceClicked();
    
private slots:
    void onNumberClick();
    void onBackspace();
    void onToggleSign();
    void onDecimal();
    
private:
    QLineEdit* m_display;
    
    QPushButton* makeButton(const QString& text, const QString& bgColor = "#FFFFFF");
};

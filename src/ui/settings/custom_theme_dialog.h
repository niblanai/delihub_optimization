#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QMap>

class CustomThemeDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomThemeDialog(QWidget* parent = nullptr);

private slots:
    void onColorFieldClicked(const QString& fieldName);
    void onSave();
    void onCancel();
    void onLoadFromLight();
    void onLoadFromDark();
    void updatePreview();

private:
    void setupUi();
    void loadCurrentColors();
    void saveColors();
    QString pickColor(const QString& currentColor);
    
    // Color fields mapped by name
    QMap<QString, QLineEdit*> m_colorFields;
    QMap<QString, QPushButton*> m_colorButtons;
    
    // Preview widgets
    QWidget* m_previewArea;
    QLabel* m_previewLabel;
    
    // Buttons
    QPushButton* m_btnSave;
    QPushButton* m_btnCancel;
    QPushButton* m_btnLoadLight;
    QPushButton* m_btnLoadDark;
};

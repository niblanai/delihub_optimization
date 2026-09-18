#include "custom_theme_dialog.h"
#include "infra/config_manager.h"
#include "services/theme_manager.h"
#include "services/lang_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QScrollArea>

CustomThemeDialog::CustomThemeDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadCurrentColors();
    updatePreview();
}

void CustomThemeDialog::setupUi() {
    setWindowTitle(LangManager::instance().t("custom_theme_editor"));
    setMinimumSize(900, 700);
    
    auto* mainLayout = new QHBoxLayout(this);
    
    // ── Left side: Color editors (scrollable) ─────────────────────────────────
    auto* leftScroll = new QScrollArea;
    leftScroll->setWidgetResizable(true);
    leftScroll->setMinimumWidth(450);
    
    auto* leftWidget = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftWidget);
    
    // Helper lambda to add a color group
    auto addColorGroup = [&](const QString& title, const QStringList& fields) {
        auto* group = new QGroupBox(title);
        auto* grid = new QGridLayout(group);
        grid->setColumnStretch(1, 1);
        
        int row = 0;
        for (const QString& field : fields) {
            // Label
            auto* label = new QLabel(field.mid(field.lastIndexOf('_') + 1) + ":");
            grid->addWidget(label, row, 0);
            
            // Color field (read-only, shows hex)
            auto* lineEdit = new QLineEdit;
            lineEdit->setReadOnly(true);
            lineEdit->setMaximumWidth(100);
            grid->addWidget(lineEdit, row, 1);
            m_colorFields[field] = lineEdit;
            
            // Color picker button
            auto* btn = new QPushButton("🎨");
            btn->setFixedSize(36, 36);
            btn->setToolTip(LangManager::instance().t("pick_color"));
            connect(btn, &QPushButton::clicked, [=]() { onColorFieldClicked(field); });
            grid->addWidget(btn, row, 2);
            m_colorButtons[field] = btn;
            
            row++;
        }
        
        leftLayout->addWidget(group);
    };
    
    // Load presets buttons
    auto* presetLayout = new QHBoxLayout;
    m_btnLoadLight = new QPushButton("📋 " + LangManager::instance().t("load_from_light"));
    m_btnLoadDark = new QPushButton("📋 " + LangManager::instance().t("load_from_dark"));
    connect(m_btnLoadLight, &QPushButton::clicked, this, &CustomThemeDialog::onLoadFromLight);
    connect(m_btnLoadDark, &QPushButton::clicked, this, &CustomThemeDialog::onLoadFromDark);
    presetLayout->addWidget(m_btnLoadLight);
    presetLayout->addWidget(m_btnLoadDark);
    leftLayout->addLayout(presetLayout);
    
    // ── SVG Icon Mode ─────────────────────────────────────────────────────────
    auto* iconModeGroup = new QGroupBox(LangManager::instance().t("svg_icon_mode"));
    auto* iconModeLayout = new QVBoxLayout(iconModeGroup);
    
    auto* iconModeLabel = new QLabel(LangManager::instance().t("svg_icon_mode_hint"));
    iconModeLabel->setWordWrap(true);
    iconModeLabel->setObjectName("hintLabel");
    iconModeLayout->addWidget(iconModeLabel);
    
    auto* iconModeBtnLayout = new QHBoxLayout;
    auto* iconLightBtn = new QPushButton("☀️ Light Icons");
    auto* iconDarkBtn = new QPushButton("🌙 Dark Icons");
    auto* iconCustomBtn = new QPushButton("🎨 Custom Color");
    
    iconLightBtn->setCheckable(true);
    iconDarkBtn->setCheckable(true);
    iconCustomBtn->setCheckable(true);
    iconLightBtn->setObjectName("secondaryBtn");
    iconDarkBtn->setObjectName("secondaryBtn");
    iconCustomBtn->setObjectName("secondaryBtn");
    
    QString currentMode = ConfigManager::instance().svgIconMode();
    if (currentMode == "light") iconLightBtn->setChecked(true);
    else if (currentMode == "dark") iconDarkBtn->setChecked(true);
    else iconCustomBtn->setChecked(true);
    
    connect(iconLightBtn, &QPushButton::clicked, [=]() {
        iconLightBtn->setChecked(true);
        iconDarkBtn->setChecked(false);
        iconCustomBtn->setChecked(false);
        ConfigManager::instance().setSvgIconMode("light");
        updatePreview();  // Refresh preview with new icon mode
    });
    
    connect(iconDarkBtn, &QPushButton::clicked, [=]() {
        iconLightBtn->setChecked(false);
        iconDarkBtn->setChecked(true);
        iconCustomBtn->setChecked(false);
        ConfigManager::instance().setSvgIconMode("dark");
        updatePreview();  // Refresh preview with new icon mode
    });
    
    connect(iconCustomBtn, &QPushButton::clicked, [=]() {
        iconLightBtn->setChecked(false);
        iconDarkBtn->setChecked(false);
        iconCustomBtn->setChecked(true);
        
        // Pick a custom color for icons
        QString currentCustom = ConfigManager::instance().svgIconMode();
        if (currentCustom == "light" || currentCustom == "dark") {
            currentCustom = "#FFFFFF";
        }
        QString newColor = pickColor(currentCustom);
        if (!newColor.isEmpty()) {
            ConfigManager::instance().setSvgIconMode(newColor);
            updatePreview();  // Refresh preview with new icon mode
        }
    });
    
    iconModeBtnLayout->addWidget(iconLightBtn);
    iconModeBtnLayout->addWidget(iconDarkBtn);
    iconModeBtnLayout->addWidget(iconCustomBtn);
    iconModeLayout->addLayout(iconModeBtnLayout);
    
    leftLayout->addWidget(iconModeGroup);
    
    // Surface colors
    addColorGroup(LangManager::instance().t("surfaces"), {
        "windowBg", "sidebarBg", "cardBg", "cardHover", "inputBg", "dialogBg"
    });
    
    // Borders
    addColorGroup(LangManager::instance().t("borders"), {
        "border", "borderFocus"
    });
    
    // Text colors
    addColorGroup(LangManager::instance().t("text_colors"), {
        "textPrimary", "textSecondary", "textDisabled", "textOnPrimary"
    });
    
    // Brand / Primary colors
    addColorGroup(LangManager::instance().t("primary_colors"), {
        "primary", "primaryHover", "primaryPressed"
    });
    
    // Semantic colors
    addColorGroup(LangManager::instance().t("semantic_colors"), {
        "success", "successBg", "warning", "warningBg", "danger", "dangerBg"
    });
    
    // Table colors
    addColorGroup(LangManager::instance().t("table_colors"), {
        "tableHeaderBg", "tableHeaderText", "tableRowAlt", 
        "tableRowHover", "tableSelected", "tableBorder"
    });
    
    // Navigation colors
    addColorGroup(LangManager::instance().t("navigation_colors"), {
        "navBg", "navHover", "navSelected", "navSelectedText", "navText"
    });
    
    // Scrollbar colors
    addColorGroup(LangManager::instance().t("scrollbar_colors"), {
        "scrollHandle", "scrollTrack"
    });
    
    leftLayout->addStretch();
    leftScroll->setWidget(leftWidget);
    mainLayout->addWidget(leftScroll, 1);
    
    // ── Right side: Preview ───────────────────────────────────────────────────
    auto* rightLayout = new QVBoxLayout;
    
    m_previewLabel = new QLabel("<b>" + LangManager::instance().t("preview") + "</b>");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(m_previewLabel);
    
    m_previewArea = new QWidget;
    m_previewArea->setMinimumSize(350, 500);
    rightLayout->addWidget(m_previewArea, 1);
    
    // Save/Cancel buttons
    auto* btnLayout = new QHBoxLayout;
    m_btnSave = new QPushButton(LangManager::instance().t("save"));
    m_btnSave->setObjectName("primaryBtn");
    m_btnCancel = new QPushButton(LangManager::instance().t("cancel"));
    
    connect(m_btnSave, &QPushButton::clicked, this, &CustomThemeDialog::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &CustomThemeDialog::onCancel);
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnCancel);
    rightLayout->addLayout(btnLayout);
    
    mainLayout->addLayout(rightLayout, 1);
}

void CustomThemeDialog::loadCurrentColors() {
    ConfigManager& cfg = ConfigManager::instance();
    
    m_colorFields["windowBg"]->setText(cfg.customTheme_windowBg());
    m_colorFields["sidebarBg"]->setText(cfg.customTheme_sidebarBg());
    m_colorFields["cardBg"]->setText(cfg.customTheme_cardBg());
    m_colorFields["cardHover"]->setText(cfg.customTheme_cardHover());
    m_colorFields["inputBg"]->setText(cfg.customTheme_inputBg());
    m_colorFields["dialogBg"]->setText(cfg.customTheme_dialogBg());
    
    m_colorFields["border"]->setText(cfg.customTheme_border());
    m_colorFields["borderFocus"]->setText(cfg.customTheme_borderFocus());
    
    m_colorFields["textPrimary"]->setText(cfg.customTheme_textPrimary());
    m_colorFields["textSecondary"]->setText(cfg.customTheme_textSecondary());
    m_colorFields["textDisabled"]->setText(cfg.customTheme_textDisabled());
    m_colorFields["textOnPrimary"]->setText(cfg.customTheme_textOnPrimary());
    
    m_colorFields["primary"]->setText(cfg.customTheme_primary());
    m_colorFields["primaryHover"]->setText(cfg.customTheme_primaryHover());
    m_colorFields["primaryPressed"]->setText(cfg.customTheme_primaryPressed());
    
    m_colorFields["success"]->setText(cfg.customTheme_success());
    m_colorFields["successBg"]->setText(cfg.customTheme_successBg());
    m_colorFields["warning"]->setText(cfg.customTheme_warning());
    m_colorFields["warningBg"]->setText(cfg.customTheme_warningBg());
    m_colorFields["danger"]->setText(cfg.customTheme_danger());
    m_colorFields["dangerBg"]->setText(cfg.customTheme_dangerBg());
    
    m_colorFields["tableHeaderBg"]->setText(cfg.customTheme_tableHeaderBg());
    m_colorFields["tableHeaderText"]->setText(cfg.customTheme_tableHeaderText());
    m_colorFields["tableRowAlt"]->setText(cfg.customTheme_tableRowAlt());
    m_colorFields["tableRowHover"]->setText(cfg.customTheme_tableRowHover());
    m_colorFields["tableSelected"]->setText(cfg.customTheme_tableSelected());
    m_colorFields["tableBorder"]->setText(cfg.customTheme_tableBorder());
    
    m_colorFields["navBg"]->setText(cfg.customTheme_navBg());
    m_colorFields["navHover"]->setText(cfg.customTheme_navHover());
    m_colorFields["navSelected"]->setText(cfg.customTheme_navSelected());
    m_colorFields["navSelectedText"]->setText(cfg.customTheme_navSelectedText());
    m_colorFields["navText"]->setText(cfg.customTheme_navText());
    
    m_colorFields["scrollHandle"]->setText(cfg.customTheme_scrollHandle());
    m_colorFields["scrollTrack"]->setText(cfg.customTheme_scrollTrack());
    
    // Update button backgrounds
    for (auto it = m_colorFields.constBegin(); it != m_colorFields.constEnd(); ++it) {
        QString color = it.value()->text();
        if (m_colorButtons.contains(it.key())) {
            m_colorButtons[it.key()]->setStyleSheet(
                QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(color)
            );
        }
    }
}

void CustomThemeDialog::onColorFieldClicked(const QString& fieldName) {
    if (!m_colorFields.contains(fieldName)) return;
    
    QString currentColor = m_colorFields[fieldName]->text();
    QString newColor = pickColor(currentColor);
    
    if (!newColor.isEmpty() && newColor != currentColor) {
        m_colorFields[fieldName]->setText(newColor);
        m_colorButtons[fieldName]->setStyleSheet(
            QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(newColor)
        );
        updatePreview();
    }
}

QString CustomThemeDialog::pickColor(const QString& currentColor) {
    QColor initial = QColor(currentColor);
    QColor color = QColorDialog::getColor(initial, this, LangManager::instance().t("select_color"));
    
    if (color.isValid()) {
        return color.name().toUpper();
    }
    return QString();
}

void CustomThemeDialog::onLoadFromLight() {
    auto tokens = DesignTokens::light();
    
    m_colorFields["windowBg"]->setText(tokens.windowBg);
    m_colorFields["sidebarBg"]->setText(tokens.sidebarBg);
    m_colorFields["cardBg"]->setText(tokens.cardBg);
    m_colorFields["cardHover"]->setText(tokens.cardHover);
    m_colorFields["inputBg"]->setText(tokens.inputBg);
    m_colorFields["dialogBg"]->setText(tokens.dialogBg);
    m_colorFields["border"]->setText(tokens.border);
    m_colorFields["borderFocus"]->setText(tokens.borderFocus);
    m_colorFields["textPrimary"]->setText(tokens.textPrimary);
    m_colorFields["textSecondary"]->setText(tokens.textSecondary);
    m_colorFields["textDisabled"]->setText(tokens.textDisabled);
    m_colorFields["textOnPrimary"]->setText(tokens.textOnPrimary);
    m_colorFields["primary"]->setText(tokens.primary);
    m_colorFields["primaryHover"]->setText(tokens.primaryHover);
    m_colorFields["primaryPressed"]->setText(tokens.primaryPressed);
    m_colorFields["success"]->setText(tokens.success);
    m_colorFields["successBg"]->setText(tokens.successBg);
    m_colorFields["warning"]->setText(tokens.warning);
    m_colorFields["warningBg"]->setText(tokens.warningBg);
    m_colorFields["danger"]->setText(tokens.danger);
    m_colorFields["dangerBg"]->setText(tokens.dangerBg);
    m_colorFields["tableHeaderBg"]->setText(tokens.tableHeaderBg);
    m_colorFields["tableHeaderText"]->setText(tokens.tableHeaderText);
    m_colorFields["tableRowAlt"]->setText(tokens.tableRowAlt);
    m_colorFields["tableRowHover"]->setText(tokens.tableRowHover);
    m_colorFields["tableSelected"]->setText(tokens.tableSelected);
    m_colorFields["tableBorder"]->setText(tokens.tableBorder);
    m_colorFields["navBg"]->setText(tokens.navBg);
    m_colorFields["navHover"]->setText(tokens.navHover);
    m_colorFields["navSelected"]->setText(tokens.navSelected);
    m_colorFields["navSelectedText"]->setText(tokens.navSelectedText);
    m_colorFields["navText"]->setText(tokens.navText);
    m_colorFields["scrollHandle"]->setText(tokens.scrollHandle);
    m_colorFields["scrollTrack"]->setText(tokens.scrollTrack);
    
    // Update button backgrounds
    for (auto it = m_colorFields.constBegin(); it != m_colorFields.constEnd(); ++it) {
        QString color = it.value()->text();
        if (m_colorButtons.contains(it.key())) {
            m_colorButtons[it.key()]->setStyleSheet(
                QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(color)
            );
        }
    }
    
    updatePreview();
}

void CustomThemeDialog::onLoadFromDark() {
    auto tokens = DesignTokens::dark();
    
    m_colorFields["windowBg"]->setText(tokens.windowBg);
    m_colorFields["sidebarBg"]->setText(tokens.sidebarBg);
    m_colorFields["cardBg"]->setText(tokens.cardBg);
    m_colorFields["cardHover"]->setText(tokens.cardHover);
    m_colorFields["inputBg"]->setText(tokens.inputBg);
    m_colorFields["dialogBg"]->setText(tokens.dialogBg);
    m_colorFields["border"]->setText(tokens.border);
    m_colorFields["borderFocus"]->setText(tokens.borderFocus);
    m_colorFields["textPrimary"]->setText(tokens.textPrimary);
    m_colorFields["textSecondary"]->setText(tokens.textSecondary);
    m_colorFields["textDisabled"]->setText(tokens.textDisabled);
    m_colorFields["textOnPrimary"]->setText(tokens.textOnPrimary);
    m_colorFields["primary"]->setText(tokens.primary);
    m_colorFields["primaryHover"]->setText(tokens.primaryHover);
    m_colorFields["primaryPressed"]->setText(tokens.primaryPressed);
    m_colorFields["success"]->setText(tokens.success);
    m_colorFields["successBg"]->setText(tokens.successBg);
    m_colorFields["warning"]->setText(tokens.warning);
    m_colorFields["warningBg"]->setText(tokens.warningBg);
    m_colorFields["danger"]->setText(tokens.danger);
    m_colorFields["dangerBg"]->setText(tokens.dangerBg);
    m_colorFields["tableHeaderBg"]->setText(tokens.tableHeaderBg);
    m_colorFields["tableHeaderText"]->setText(tokens.tableHeaderText);
    m_colorFields["tableRowAlt"]->setText(tokens.tableRowAlt);
    m_colorFields["tableRowHover"]->setText(tokens.tableRowHover);
    m_colorFields["tableSelected"]->setText(tokens.tableSelected);
    m_colorFields["tableBorder"]->setText(tokens.tableBorder);
    m_colorFields["navBg"]->setText(tokens.navBg);
    m_colorFields["navHover"]->setText(tokens.navHover);
    m_colorFields["navSelected"]->setText(tokens.navSelected);
    m_colorFields["navSelectedText"]->setText(tokens.navSelectedText);
    m_colorFields["navText"]->setText(tokens.navText);
    m_colorFields["scrollHandle"]->setText(tokens.scrollHandle);
    m_colorFields["scrollTrack"]->setText(tokens.scrollTrack);
    
    // Update button backgrounds
    for (auto it = m_colorFields.constBegin(); it != m_colorFields.constEnd(); ++it) {
        QString color = it.value()->text();
        if (m_colorButtons.contains(it.key())) {
            m_colorButtons[it.key()]->setStyleSheet(
                QString("background-color: %1; border: 2px solid #888; border-radius: 4px;").arg(color)
            );
        }
    }
    
    updatePreview();
}

void CustomThemeDialog::updatePreview() {
    // Create a preview with sample UI elements
    QString windowBg = m_colorFields["windowBg"]->text();
    QString cardBg = m_colorFields["cardBg"]->text();
    QString border = m_colorFields["border"]->text();
    QString textPrimary = m_colorFields["textPrimary"]->text();
    QString textSecondary = m_colorFields["textSecondary"]->text();
    QString primary = m_colorFields["primary"]->text();
    QString primaryHover = m_colorFields["primaryHover"]->text();
    QString inputBg = m_colorFields["inputBg"]->text();
    QString navBg = m_colorFields["navBg"]->text();
    QString navSelected = m_colorFields["navSelected"]->text();
    QString navText = m_colorFields["navText"]->text();
    
    QString previewStyle = QString(R"(
        QWidget {
            background-color: %1;
            color: %2;
        }
        QLabel {
            color: %2;
        }
        QPushButton {
            background-color: %3;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: %4;
        }
        QLineEdit {
            background-color: %5;
            border: 2px solid %6;
            border-radius: 6px;
            padding: 8px;
            color: %2;
        }
        QFrame#sampleCard {
            background-color: %7;
            border: 1px solid %6;
            border-radius: 10px;
            padding: 12px;
        }
        QFrame#sampleNav {
            background-color: %8;
            border: 1px solid %6;
            border-radius: 8px;
            padding: 8px;
        }
        QLabel#navItem {
            background-color: %9;
            color: %10;
            padding: 8px;
            border-radius: 6px;
        }
    )")
    .arg(windowBg, textPrimary, primary, primaryHover, inputBg, border, cardBg, navBg, navSelected, navText);
    
    // Rebuild preview widget
    delete m_previewArea;
    m_previewArea = new QWidget;
    m_previewArea->setMinimumSize(350, 500);
    m_previewArea->setStyleSheet(previewStyle);
    
    auto* previewLayout = new QVBoxLayout(m_previewArea);
    
    // Sample card
    auto* card = new QFrame;
    card->setObjectName("sampleCard");
    auto* cardLayout = new QVBoxLayout(card);
    
    auto* title = new QLabel("<b>" + LangManager::instance().t("sample_card") + "</b>");
    cardLayout->addWidget(title);
    
    auto* desc = new QLabel(LangManager::instance().t("this_is_preview"));
    desc->setStyleSheet("color: " + textSecondary + ";");
    cardLayout->addWidget(desc);
    
    auto* input = new QLineEdit;
    input->setPlaceholderText(LangManager::instance().t("sample_input"));
    cardLayout->addWidget(input);
    
    auto* btn = new QPushButton(LangManager::instance().t("sample_button"));
    cardLayout->addWidget(btn);
    
    previewLayout->addWidget(card);
    
    // Sample navigation
    auto* nav = new QFrame;
    nav->setObjectName("sampleNav");
    auto* navLayout = new QVBoxLayout(nav);
    
    auto* navLabel = new QLabel("<b>" + LangManager::instance().t("navigation") + "</b>");
    navLayout->addWidget(navLabel);
    
    auto* navItem = new QLabel("● " + LangManager::instance().t("selected_item"));
    navItem->setObjectName("navItem");
    navLayout->addWidget(navItem);
    
    previewLayout->addWidget(nav);
    previewLayout->addStretch();
    
    // Update layout
    QVBoxLayout* rightLayout = qobject_cast<QVBoxLayout*>(
        qobject_cast<QHBoxLayout*>(layout())->itemAt(1)->layout()
    );
    if (rightLayout) {
        rightLayout->replaceWidget(rightLayout->itemAt(1)->widget(), m_previewArea);
    }
}

void CustomThemeDialog::onSave() {
    saveColors();
    
    // Apply custom theme
    ConfigManager::instance().setTheme("custom");
    ThemeManager::instance().applyTheme(ThemeType::Custom);
    
    // Icon mode is already saved in ConfigManager by the button click handlers
    // Trigger a theme change signal to refresh all icons across the application
    emit ThemeManager::instance().themeChanged(ThemeType::Custom);
    
    QMessageBox::information(this, 
        LangManager::instance().t("success"),
        LangManager::instance().t("custom_theme_saved"));
    
    accept();
}

void CustomThemeDialog::onCancel() {
    reject();
}

void CustomThemeDialog::saveColors() {
    ConfigManager& cfg = ConfigManager::instance();
    
    cfg.setCustomTheme_windowBg(m_colorFields["windowBg"]->text());
    cfg.setCustomTheme_sidebarBg(m_colorFields["sidebarBg"]->text());
    cfg.setCustomTheme_cardBg(m_colorFields["cardBg"]->text());
    cfg.setCustomTheme_cardHover(m_colorFields["cardHover"]->text());
    cfg.setCustomTheme_inputBg(m_colorFields["inputBg"]->text());
    cfg.setCustomTheme_dialogBg(m_colorFields["dialogBg"]->text());
    
    cfg.setCustomTheme_border(m_colorFields["border"]->text());
    cfg.setCustomTheme_borderFocus(m_colorFields["borderFocus"]->text());
    
    cfg.setCustomTheme_textPrimary(m_colorFields["textPrimary"]->text());
    cfg.setCustomTheme_textSecondary(m_colorFields["textSecondary"]->text());
    cfg.setCustomTheme_textDisabled(m_colorFields["textDisabled"]->text());
    cfg.setCustomTheme_textOnPrimary(m_colorFields["textOnPrimary"]->text());
    
    cfg.setCustomTheme_primary(m_colorFields["primary"]->text());
    cfg.setCustomTheme_primaryHover(m_colorFields["primaryHover"]->text());
    cfg.setCustomTheme_primaryPressed(m_colorFields["primaryPressed"]->text());
    
    cfg.setCustomTheme_success(m_colorFields["success"]->text());
    cfg.setCustomTheme_successBg(m_colorFields["successBg"]->text());
    cfg.setCustomTheme_warning(m_colorFields["warning"]->text());
    cfg.setCustomTheme_warningBg(m_colorFields["warningBg"]->text());
    cfg.setCustomTheme_danger(m_colorFields["danger"]->text());
    cfg.setCustomTheme_dangerBg(m_colorFields["dangerBg"]->text());
    
    cfg.setCustomTheme_tableHeaderBg(m_colorFields["tableHeaderBg"]->text());
    cfg.setCustomTheme_tableHeaderText(m_colorFields["tableHeaderText"]->text());
    cfg.setCustomTheme_tableRowAlt(m_colorFields["tableRowAlt"]->text());
    cfg.setCustomTheme_tableRowHover(m_colorFields["tableRowHover"]->text());
    cfg.setCustomTheme_tableSelected(m_colorFields["tableSelected"]->text());
    cfg.setCustomTheme_tableBorder(m_colorFields["tableBorder"]->text());
    
    cfg.setCustomTheme_navBg(m_colorFields["navBg"]->text());
    cfg.setCustomTheme_navHover(m_colorFields["navHover"]->text());
    cfg.setCustomTheme_navSelected(m_colorFields["navSelected"]->text());
    cfg.setCustomTheme_navSelectedText(m_colorFields["navSelectedText"]->text());
    cfg.setCustomTheme_navText(m_colorFields["navText"]->text());
    
    cfg.setCustomTheme_scrollHandle(m_colorFields["scrollHandle"]->text());
    cfg.setCustomTheme_scrollTrack(m_colorFields["scrollTrack"]->text());
}

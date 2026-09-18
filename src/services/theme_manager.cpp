#include "services/theme_manager.h"
#include <QApplication>
#include <QSettings>

// ─────────────────────────────────────────────────────────────────────────────
ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

void ThemeManager::applyTheme(const QString& name) {
    if      (name == "light")  applyTheme(ThemeType::Light);
    else if (name == "custom") applyTheme(ThemeType::Custom);
    else                       applyTheme(ThemeType::Dark);
}

void ThemeManager::applyTheme(ThemeType type) {
    m_current = type;
    m_tokens  = DesignTokens::forTheme(type);

    if (qApp) qApp->setStyleSheet(fullStyle());

    // Persist choice
    QSettings s;
    s.setValue("theme", currentThemeName());

    emit themeChanged(type);
}

QString ThemeManager::currentThemeName() const {
    switch (m_current) {
        case ThemeType::Light:  return "light";
        case ThemeType::Custom: return "custom";
        default:                return "dark";
    }
}

// ── Helper: generate QSS from token value ─────────────────────────────────────
#define T(x) (m_tokens.x)

// ── 1. Global colors / resets ─────────────────────────────────────────────────
QString ThemeManager::colorsStyle() const {
    return QString(R"(
QWidget {
    background-color: %1;
    color: %2;
    font-family: "Inter", "Segoe UI Variable", "Segoe UI", "Cairo", sans-serif;
    font-size: %3px;
    outline: none;
}
QMainWindow, QDialog {
    background-color: %4;
}
QToolTip {
    background-color: %5;
    color: %2;
    border: 1px solid %6;
    border-radius: 6px;
    padding: 4px 8px;
    font-size: %7px;
}
)")
    .arg(T(windowBg), T(textPrimary)).arg(T(fontBody))
    .arg(T(windowBg), T(cardBg), T(border)).arg(T(fontSm));
}

// ── 2. Buttons ────────────────────────────────────────────────────────────────
QString ThemeManager::buttonsStyle() const {
    return QString(R"(
QPushButton {
    background-color: %1;
    color: %2;
    border: none;
    border-radius: %3px;
    padding: 8px 16px;
    font-size: %4px;
    font-weight: 600;
    min-height: 36px;
}
QPushButton:hover    { background-color: %5; }
QPushButton:pressed  { background-color: %6; }
QPushButton:disabled { background-color: %7; color: %8; }
QPushButton:checked  { background-color: %5; }

QPushButton#primaryBtn {
    background-color: %1;
    color: %2;
    border-radius: %3px;
}
QPushButton#primaryBtn:hover   { background-color: %5; }
QPushButton#primaryBtn:pressed { background-color: %6; }

QPushButton#secondaryBtn {
    background-color: %9;
    color: %10;
    border: 1px solid %11;
}
QPushButton#secondaryBtn:hover   { background-color: %12; }
QPushButton#secondaryBtn:pressed { background-color: %11; }

QPushButton#dangerBtn {
    background-color: %13;
    color: #FFFFFF;
}
QPushButton#dangerBtn:hover   { background-color: #DC2626; }
QPushButton#dangerBtn:pressed { background-color: #B91C1C; }

QPushButton#successBtn {
    background-color: %14;
    color: #FFFFFF;
}
QPushButton#successBtn:hover   { background-color: #16A34A; }
QPushButton#successBtn:pressed { background-color: #15803D; }

QPushButton#iconBtn {
    background-color: transparent;
    border: 1px solid %11;
    border-radius: 6px;
    padding: 2px;
    min-height: 24px;
    min-width: 24px;
    font-size: 10px;
    font-weight: bold;
    color: %10;
}
QPushButton#iconBtn:hover { background-color: %12; }
)")
    .arg(T(primary), T(textOnPrimary)).arg(T(radiusMd))
    .arg(T(fontBody))
    .arg(T(primaryHover), T(primaryPressed))
    .arg(T(border), T(textDisabled))
    .arg(T(cardBg), T(textPrimary), T(border), T(cardHover))
    .arg(T(danger), T(success));
}

// ── 3. Inputs ─────────────────────────────────────────────────────────────────
QString ThemeManager::inputsStyle() const {
    return QString(R"(
QLineEdit, QDoubleSpinBox, QSpinBox, QTextEdit, QPlainTextEdit {
    background-color: %1;
    border: 1.5px solid %2;
    border-radius: %3px;
    padding: 6px 12px;
    color: %4;
    font-size: %5px;
    min-height: %6px;
    selection-background-color: %7;
}
QLineEdit:hover, QDoubleSpinBox:hover, QSpinBox:hover {
    border-color: %8;
}
QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus,
QTextEdit:focus, QPlainTextEdit:focus {
    border-color: %9;
    background-color: %1;
}
QLineEdit:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled {
    background-color: %10;
    color: %11;
}
/* ── Table cell widgets: suppress the tall form-input style ──────────────────
   The global min-height + padding causes QSpinBox/QLineEdit/QDoubleSpinBox
   placed via setCellWidget() to paint outside their geometry box (overflowing
   into the row below).  A descendant-selector rule with lower height and
   tighter padding fixes the painted area to stay within the row geometry.   */
QTableWidget QSpinBox,
QTableWidget QDoubleSpinBox,
QTableWidget QLineEdit {
    min-height: 0px;
    padding: 2px 4px;
}
/* Task 14: same fix for QPushButton placed via setCellWidget() —
   the global min-height:36px on QPushButton overrides setFixedHeight(30),
   causing the Convert button to misalign inside the 44px row cell. */
QTableWidget QPushButton {
    min-height: 0px;
    padding: 2px 8px;
}
QComboBox {
    background-color: %1;
    border: 1.5px solid %2;
    border-radius: %3px;
    padding: 6px 12px;
    color: %4;
    font-size: %5px;
    min-height: %6px;
}
QComboBox:hover  { border-color: %8; }
QComboBox:focus  { border-color: %9; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: none; }
QComboBox QAbstractItemView {
    background-color: %12;
    border: 1px solid %2;
    border-radius: %3px;
    selection-background-color: %7;
    color: %4;
    padding: 4px;
}
QDateEdit, QDateTimeEdit, QTimeEdit {
    background-color: %1;
    border: 1.5px solid %2;
    border-radius: %3px;
    padding: 6px 12px;
    color: %4;
    font-size: %5px;
    min-height: %6px;
}
QDateEdit:focus, QDateTimeEdit:focus { border-color: %9; }
QCheckBox {
    color: %4;
    font-size: %5px;
    spacing: 8px;
}
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 1.5px solid %2;
    border-radius: 4px;
    background-color: %1;
}
QCheckBox::indicator:checked {
    background-color: %13;
    border-color: %13;
}
QCheckBox::indicator:hover { border-color: %8; }
QRadioButton {
    color: %4;
    font-size: %5px;
    spacing: 8px;
}
)")
    .arg(T(inputBg), T(border)).arg(T(radiusMd))
    .arg(T(textPrimary)).arg(T(fontBody)).arg(T(inputHeight))
    .arg(T(tableSelected))
    .arg(T(border))         // hover border (slightly lighter)
    .arg(T(borderFocus))    // focus border
    .arg(T(cardHover), T(textDisabled))
    .arg(T(cardBg))         // ComboBox dropdown bg
    .arg(T(primary));       // checkbox checked bg
}

// ── 4. Tables ─────────────────────────────────────────────────────────────────
QString ThemeManager::tableStyle() const {
    return QString(R"(
QTableView, QTableWidget {
    background-color: %1;
    alternate-background-color: %2;
    gridline-color: %3;
    border: 1px solid %3;
    border-radius: %4px;
    font-size: %5px;
    selection-background-color: %6;
    selection-color: %7;
    outline: none;
}
QTableView::item, QTableWidget::item {
    padding: 8px 10px;
    min-height: %8px;
    border: none;
}
QTableView::item:hover, QTableWidget::item:hover {
    background-color: %9;
}
QTableView::item:selected, QTableWidget::item:selected {
    background-color: %6;
    color: %7;
}
QHeaderView::section {
    background-color: %10;
    color: %11;
    padding: 10px 12px;
    font-size: %12px;
    font-weight: 700;
    border: none;
    border-bottom: 2px solid %3;
    min-height: %13px;
}
QHeaderView::section:hover {
    background-color: %9;
}
QHeaderView { border: none; }
)")
    .arg(T(cardBg), T(tableRowAlt), T(tableBorder)).arg(T(radiusMd))
    .arg(T(fontBody))
    .arg(T(tableSelected), T(textPrimary))
    .arg(T(tableRowHeight))
    .arg(T(tableRowHover))
    .arg(T(tableHeaderBg), T(tableHeaderText)).arg(T(fontSm))
    .arg(T(tableHeaderHeight));
}

// ── 5. Sidebar ────────────────────────────────────────────────────────────────
QString ThemeManager::sidebarStyle() const {
    return QString(R"(
/* Sidebar scroll area transparency */
#navScrollArea,
#navScrollArea > QWidget,
#navScrollArea > QWidget > QWidget,
#navContainer {
    background-color: %1;
    border: none;
}

/* ── Nav buttons ─────────────────────────────────────────────────────────── */
#sidebar QPushButton#navBtn {
    background-color: transparent;
    color: %3;
    border: none;
    border-left: 4px solid transparent;
    border-radius: 0px;
    text-align: left;
    padding: 0px 10px;
    font-size: %5px;
    font-weight: 500;
    min-height: 44px;
    max-height: 44px;
}
#sidebar QPushButton#navBtn:hover {
    background-color: %6;
    color: %7;
    border-left: 4px solid transparent;
}
#sidebar QPushButton#navBtn:checked {
    background-color: %8;
    color: %10;
    border-left: 4px solid %4;
    font-weight: 600;
}
#sidebar QPushButton#navBtn:hover:checked {
    background-color: %8;
    color: %10;
    border-left: 4px solid %4;
}

/* ── Metallic premium separator ─────────────────────────────────────────── */
QFrame#sidebarMetalSep {
    min-height: 1px;
    max-height: 1px;
    border: none;
    margin: 4px 0;
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0   rgba(255,255,255,0),
        stop:0.2 rgba(255,255,255,18),
        stop:0.5 rgba(255,255,255,45),
        stop:0.8 rgba(255,255,255,18),
        stop:1   rgba(255,255,255,0));
}
/* ── Old separator (still used for banner etc) ───────────────────────────── */
QFrame#sidebarSep {
    background-color: %2;
    max-height: 1px;
    min-height: 1px;
    border: none;
    margin: 0px;
}
/* ── Premium thin scrollbar inside sidebar ───────────────────────────────── */
#navScrollArea QScrollBar:vertical {
    background: transparent;
    width: 4px;
    margin: 4px 0;
    border-radius: 2px;
}
#navScrollArea QScrollBar::handle:vertical {
    background: rgba(255,255,255,35);
    border-radius: 2px;
    min-height: 24px;
}
#navScrollArea QScrollBar::handle:vertical:hover {
    background: rgba(255,255,255,65);
}
#navScrollArea QScrollBar::add-line:vertical,
#navScrollArea QScrollBar::sub-line:vertical {
    height: 0px;
    background: transparent;
}
#navScrollArea QScrollBar::add-page:vertical,
#navScrollArea QScrollBar::sub-page:vertical {
    background: transparent;
}
/* ── Sidebar drop shadow via border ──────────────────────────────────────── */
#sidebar {
    background-color: %1;
    border-right: 1px solid %2;
    border-top-right-radius: 12px;
    border-bottom-right-radius: 12px;
}
#clockLabel {
    font-size: 16px;
    font-weight: bold;
    color: %4;
    letter-spacing: 2px;
    padding: 4px 0;
}
#brandLabel {
    font-size: 18px;
    font-weight: bold;
    color: %4;
    padding: 0 4px;
}
#sidebarHint {
    font-size: 11px;
    color: %10;
    margin-top: 2px;
}
/* ── User info labels — no background box, clean text ───────────────────── */
#currentUserLabel {
    font-size: 12px;
    font-weight: 600;
    color: %3;
    background: transparent;
    padding: 2px 0;
}
#sidebarRoleLabel {
    font-size: 11px;
    font-weight: 400;
    color: %3;
    background: transparent;
    padding: 1px 0;
    opacity: 0.75;
}
#sidebarBranchLabel {
    font-size: 10px;
    color: %3;
    background: transparent;
    padding: 1px 0;
    opacity: 0.65;
}
/* ── Version label ───────────────────────────────────────────────────────── */
#versionLabel {
    font-size: 10px;
    color: %10;
    background: transparent;
}
)")
    .arg(T(sidebarBg), T(border), T(navText))
    .arg(T(primary))
    .arg(T(fontBody))
    .arg(T(navHover), T(textPrimary))
    .arg(T(navSelected))
    .arg(T(navSelectedText))
    .arg(T(textSecondary));
}

// ── 6. Cards / KPI ────────────────────────────────────────────────────────────
QString ThemeManager::cardsStyle() const {
    return QString(R"(
QFrame#kpiCard {
    background-color: %1;
    border: 1px solid %2;
    border-radius: %3px;
    padding: 0px;
}
QFrame#kpiCard:hover {
    border-color: %4;
    background-color: %5;
}
QFrame#pageToolbar {
    background-color: %6;
    border-bottom: 1px solid %2;
}
QFrame#filterBar {
    background-color: %6;
    border-bottom: 1px solid %2;
}
QFrame#statusBar {
    background-color: %6;
    border-top: 1px solid %2;
}
QLabel#statusLabel {
    color: %7;
    font-size: %8px;
}
QLabel#pageTitle {
    font-size: %9px;
    font-weight: 700;
    color: %10;
}
QLabel#hintLabel {
    color: %7;
    font-size: %8px;
}
QLabel#cardSectionTitle {
    font-size: %9px;
    font-weight: 600;
    color: %10;
}
QGroupBox {
    border: 1px solid %2;
    border-radius: %11px;
    margin-top: 10px;
    padding: 12px 8px 8px 8px;
    font-size: %8px;
    font-weight: 600;
    color: %7;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
    color: %7;
}
QTabWidget::pane {
    border: 1px solid %2;
    border-radius: %11px;
    background-color: %1;
}
QTabBar::tab {
    background-color: %6;
    color: %7;
    border: none;
    border-bottom: 2px solid transparent;
    padding: 10px 18px;
    font-size: %8px;
    font-weight: 500;
}
QTabBar::tab:selected {
    color: %4;
    border-bottom: 2px solid %4;
    font-weight: 700;
    background-color: %1;
}
QTabBar::tab:hover { color: %10; background-color: %5; }
)")
    .arg(T(cardBg), T(border)).arg(T(radiusLg))
    .arg(T(primary), T(cardHover))
    .arg(T(windowBg))
    .arg(T(textSecondary)).arg(T(fontSm))
    .arg(T(fontH2))
    .arg(T(textPrimary))
    .arg(T(radiusSm))
    // appended — Dashboard semantic labels, fully theme-aware
    + QString(R"(
QLabel#dashSectionLabel {
    color: %1;
    font-size: 11px;
}
QLabel#dashKpiTitle {
    color: %1;
    font-size: 11px;
}
QLabel#dashKpiValue {
    color: %2;
    font-size: 26px;
    font-weight: 500;
}
QLabel#dashKpiValueGreen  { color: %3; font-size: 26px; font-weight: 500; }
QLabel#dashKpiValueAmber  { color: %4; font-size: 26px; font-weight: 500; }
QLabel#dashKpiValueDanger { color: %5; font-size: 26px; font-weight: 500; }
/* ── Hamburger (menu) button — floats over the page ─────────────────────── */
QPushButton#hamburgerBtn {
    background-color: transparent;
    border: none;
    border-radius: 10px;
    padding: 0px;
    font-size: 22px;
    font-weight: bold;
    color: %10;
}
QPushButton#hamburgerBtn:hover {
    background-color: %1;
    border: 1px solid %2;
}
)")
    .arg(T(textSecondary), T(textPrimary),
         T(success), T(warning), T(danger));
}

// ── 7. Dialogs ────────────────────────────────────────────────────────────────
QString ThemeManager::dialogsStyle() const {
    return QString(R"(
QDialog {
    background-color: %1;
}
QDialog QLabel {
    color: %2;
}
QDialogButtonBox QPushButton {
    min-width: 90px;
}
QMessageBox {
    background-color: %1;
}
QMessageBox QLabel { color: %2; }
QScrollArea {
    background-color: %3;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background-color: %3;
}
)")
    .arg(T(dialogBg), T(textPrimary), T(windowBg));
}

// ── 8. Scrollbars ─────────────────────────────────────────────────────────────
QString ThemeManager::scrollStyle() const {
    return QString(R"(
QScrollBar:vertical {
    background: transparent;
    width: 5px;
    border-radius: 2px;
    margin: 2px 0;
}
QScrollBar::handle:vertical {
    background: %2;
    min-height: 24px;
    border-radius: 2px;
}
QScrollBar::handle:vertical:hover { background: %3; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; background: transparent; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal {
    background: transparent;
    height: 5px;
    border-radius: 2px;
    margin: 0 2px;
}
QScrollBar::handle:horizontal {
    background: %2;
    min-width: 24px;
    border-radius: 2px;
}
QScrollBar::handle:horizontal:hover { background: %3; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; background: transparent; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
)")
    .arg(T(scrollTrack), T(scrollHandle), T(textSecondary));
}

// ── Full combined stylesheet ───────────────────────────────────────────────────
QString ThemeManager::fullStyle() const {
    return colorsStyle()
         + buttonsStyle()
         + inputsStyle()
         + tableStyle()
         + sidebarStyle()
         + cardsStyle()
         + dialogsStyle()
         + scrollStyle()
         + notesStyle();
}

// ── 9. Notes page ─────────────────────────────────────────────────────────────
QString ThemeManager::notesStyle() const {
    return QString(R"(

/* ── Notes board (scroll area + inner widget) — follow windowBg token ────── */
QScrollArea#notesBoard,
QScrollArea#notesBoard > QWidget,
QScrollArea#notesBoard > QWidget > QWidget,
QWidget#notesBoardInner {
    background-color: %1;
}

/* ── Attachment chip: small pill showing a file/image attachment ─────────── */
QFrame#attachmentChip {
    background-color: %2;
    border: 1px solid %3;
    border-radius: %4px;
    padding: 2px 6px;
}
QFrame#attachmentChip QLabel {
    color: %5;
    font-size: %6px;
}
QPushButton#attachmentChipRemove {
    background-color: transparent;
    border: none;
    color: %7;
    font-size: %6px;
    min-height: 0px;
    padding: 0px 2px;
}
QPushButton#attachmentChipRemove:hover {
    color: %8;
}

/* ── Pagination bar ──────────────────────────────────────────────────────── */
QFrame#paginationBar {
    background-color: %9;
    border-top: 1px solid %3;
}
QLabel#paginationLabel {
    color: %5;
    font-size: %6px;
}

/* ── Note editor toolbar ────────────────────────────────────────────────── */
QFrame#noteEditorToolbar {
    background-color: %2;
    border-bottom: 2px solid %3;
    min-height: 44px;
}
QFrame#noteEditorMeta {
    background-color: %2;
    border-bottom: 1px solid %3;
    min-height: 48px;
}
QFrame#noteEditorButtons {
    background-color: %2;
    border-top: 1px solid %3;
    min-height: 52px;
}
/* Ensure iconBtn inside the note toolbar has a visible background */
QFrame#noteEditorToolbar QPushButton#iconBtn {
    background-color: %2;
    border: 1px solid %3;
    min-height: 28px;
    min-width: 28px;
}
QFrame#noteEditorToolbar QPushButton#iconBtn:hover {
    background-color: %3;
}
/* toolbarSep: thin vertical divider between toolbar groups */
QWidget#toolbarSep {
    background-color: %3;
}

/* ── Mention panel inside Note editor ───────────────────────────────────── */
QFrame#mentionPanel {
    background-color: %9;
    border-left: 1px solid %3;
}
QFrame#mentionPanel QLabel#mentionHeader {
    font-weight: bold;
    font-size: %6px;
}

)")
    .arg(T(windowBg))        // %1 board bg
    .arg(T(cardBg))          // %2 chip / toolbar bg
    .arg(T(border))          // %3 border
    .arg(T(radiusSm))        // %4 chip radius
    .arg(T(textSecondary))   // %5 chip label / pagination text
    .arg(T(fontSm))          // %6 small font
    .arg(T(textDisabled))    // %7 remove btn normal
    .arg(T(danger))          // %8 remove btn hover
    .arg(T(windowBg));       // %9 pagination bar / mention panel bg
}

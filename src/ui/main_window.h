#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsBlurEffect>
#include <QScrollArea>
#include <QList>
#include <QEvent>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

signals:
    void logoutRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();
    void setupSidebar();
    void setupFallbackBanner();
    void applyStylesheet();
    void navigateTo(int pageIndex);

    // Overlay drawer open/close
    void openSidebar();
    void closeSidebar();
    void updateNavButtonLabels();
    void refreshNavIcons();

    // ── Widgets ───────────────────────────────────────────────────────────────
    QWidget*        m_central      = nullptr;
    QFrame*         m_banner       = nullptr;
    QWidget*        m_pages        = nullptr;   // container for pages + hamburger
    QWidget*        m_pagesWrapper = nullptr;   // wraps m_pages — carries the blur effect
    QStackedWidget* m_pageStack    = nullptr;
    QPushButton*    m_hamburgerBtn = nullptr;   // ☰ button top-left of pages

    // Overlay drawer
    QWidget*        m_overlay      = nullptr;   // semi-transparent backdrop
    QWidget*        m_sidebar      = nullptr;   // the drawer panel itself
    bool            m_sidebarOpen  = false;

    // Animations
    QPropertyAnimation* m_slideAnim = nullptr;
    QPropertyAnimation* m_blurAnim  = nullptr;   // animates m_pagesBlur->blurRadius

    // Real blur effect applied to m_pagesWrapper while the sidebar is open
    QGraphicsBlurEffect* m_pagesBlur = nullptr;

    // Nav buttons (index matches page stack index)
    QPushButton* m_btnDashboard = nullptr;
    QPushButton* m_btnCustomers = nullptr;
    QPushButton* m_btnProducts  = nullptr;
    QPushButton* m_btnOrders    = nullptr;
    QPushButton* m_btnScheduled = nullptr;
    QPushButton* m_btnReports   = nullptr;
    QPushButton* m_btnReturns   = nullptr;
    QPushButton* m_btnCoupons   = nullptr;
    QPushButton* m_btnUsers     = nullptr;
    QPushButton* m_btnSettings  = nullptr;
    QPushButton* m_btnAuditLog  = nullptr;
    QPushButton* m_btnNotes     = nullptr;
    // Phase 1 buttons
    QPushButton* m_btnSuppliers  = nullptr;
    QPushButton* m_btnPurchases  = nullptr;
    QPushButton* m_btnRegister   = nullptr;
    QPushButton* m_btnExpenses   = nullptr;
    QPushButton* m_btnInventory  = nullptr;

    struct NavEntry { QPushButton* btn; QString svgPath; QString icon; QString label; };
    QList<NavEntry> m_navEntries;

    int m_currentPage = 0;

    // Sidebar width (fixed, not collapsible anymore)
    static constexpr int SIDEBAR_W = 260;
};

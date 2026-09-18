#ifndef CHARTS_WIDGET_H
#define CHARTS_WIDGET_H

#include <QWidget>
#include <QMap>
#include <QString>
#include <QFrame>
#include <QLabel>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChart>

// ─────────────────────────────────────────────────────────────────────────────
// ChartsWidget — three chart cards in a horizontal row:
//   [Sales – 2fr]  [Top Products – 2fr]  [Orders by Status – 1fr]
//
// Each chart lives inside its own themed card (kpiCard background/border).
// Colors are always read from ThemeManager::instance().tokens() so all
// 7 themes (Light, Dark, Teal, Luxury, Emerald, Abyss, Noir) are covered.
// ─────────────────────────────────────────────────────────────────────────────
class ChartsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartsWidget(QWidget* parent = nullptr);

    void setSalesData      (const QMap<QString, double>& dayToRevenue);
    void setTopProductsData(const QMap<QString, int>&    productToQty);
    void setStatusData     (const QMap<QString, int>&    statusToCount);

private:
    void setupUi();

    // Inner containers — each is a card frame holding title + QChartView
    QFrame*  m_salesCard       = nullptr;
    QFrame*  m_productsCard    = nullptr;
    QFrame*  m_statusCard      = nullptr;

    // Cached data for theme-change redraws
    QMap<QString, double> m_salesData;
    QMap<QString, int>    m_productsData;
    QMap<QString, int>    m_statusData;

    // Helper: build a card frame with the same look as the KPI cards above
    static QFrame* makeChartCard(const QString& title, QWidget* parent = nullptr);
};

#endif // CHARTS_WIDGET_H

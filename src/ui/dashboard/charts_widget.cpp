#include "charts_widget.h"
#include "services/theme_manager.h"
#include "services/design_tokens.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolTip>
#include <QCursor>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QChart>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QDateTime>
#include <QFont>
#include <QPen>

// ─────────────────────────────────────────────────────────────────────────────
// makeChartCard — builds a card that visually matches the KPI metric cards
// above it on the Dashboard. Uses the same "kpiCard" objectName so the
// ThemeManager's QSS rule for #kpiCard applies the correct background,
// border and radius for whatever theme is active.
// ─────────────────────────────────────────────────────────────────────────────
QFrame* ChartsWidget::makeChartCard(const QString& title, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("kpiCard");          // ← picks up ThemeManager card QSS

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(0);

    // Card title — uses theme token for primary text; no hardcoded colour
    auto* titleLbl = new QLabel(title, card);
    titleLbl->setObjectName("chartCardTitle");
    // Font only, colour comes from ThemeManager QSS via objectName or
    // falls back to the inherited widget foreground (= textPrimary in QSS)
    titleLbl->setStyleSheet(
        "QLabel#chartCardTitle {"
        "  font-size:13px; font-weight:500;"
        "  margin-bottom:10px;"
        "  background:transparent;"
        "}");
    lay->addWidget(titleLbl);

    return card;
}

// ─────────────────────────────────────────────────────────────────────────────
ChartsWidget::ChartsWidget(QWidget* parent) : QWidget(parent) {
    setupUi();

    // Re-draw all three charts when the user switches theme so axis/grid/
    // label colours update from the new DesignTokens.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeType) {
        if (!m_salesData.isEmpty())    setSalesData(m_salesData);
        if (!m_productsData.isEmpty()) setTopProductsData(m_productsData);
        if (!m_statusData.isEmpty())   setStatusData(m_statusData);
    });
}

void ChartsWidget::setupUi() {
    auto* root = new QHBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(0, 0, 0, 0);

    // Build the three card wrappers
    m_salesCard    = makeChartCard("Sales — Last 30 Days", this);
    m_productsCard = makeChartCard("Top Products",          this);
    m_statusCard   = makeChartCard("Orders by Status",      this);

    // 2fr : 2fr : 1fr — pie chart is narrower
    root->addWidget(m_salesCard,    2);
    root->addWidget(m_productsCard, 2);
    root->addWidget(m_statusCard,   1);

    // Minimum card height so charts have breathing room
    for (auto* c : {m_salesCard, m_productsCard, m_statusCard})
        c->setMinimumHeight(230);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: remove all QChartView children from a card layout so we can
// rebuild the chart without duplicating widgets on theme change / data refresh.
// ─────────────────────────────────────────────────────────────────────────────
static void clearChartViews(QFrame* card) {
    if (!card || !card->layout()) return;
    auto* lay = static_cast<QVBoxLayout*>(card->layout());
    // Remove everything after the title label (index 0)
    while (lay->count() > 1) {
        auto* item = lay->takeAt(1);
        if (item->widget()) { item->widget()->deleteLater(); }
        delete item;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sales Line Chart
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWidget::setSalesData(const QMap<QString, double>& data) {
    if (!m_salesCard) return;
    m_salesData = data;
    clearChartViews(m_salesCard);

    const DesignTokens& tk = ThemeManager::instance().tokens();
    const QColor axisColor (tk.chartAxisText);
    const QColor gridColor (tk.chartGrid);
    const QColor lineColor (tk.chartLine);
    const QColor legendColor(tk.textSecondary);

    auto* series = new QLineSeries;
    series->setName("Revenue");
    series->setPen(QPen(lineColor, 2));
    // Light fill under the line
    series->setBrush(QColor(lineColor.red(), lineColor.green(), lineColor.blue(), 30));

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        qint64 ms = QDateTime::fromString(it.key(), "yyyy-MM-dd").toMSecsSinceEpoch();
        series->append(ms, it.value());
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setBackgroundBrush(Qt::transparent);
    chart->setBackgroundRoundness(0);
    chart->legend()->setLabelColor(legendColor);
    chart->legend()->setFont(QFont("Segoe UI", 9));
    chart->setMargins(QMargins(0, 0, 0, 0));

    auto* axisX = new QDateTimeAxis;
    axisX->setFormat("MM/dd");
    axisX->setLabelsColor(axisColor);
    axisX->setGridLineColor(gridColor);
    axisX->setLabelsFont(QFont("Segoe UI", 8));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setLabelsColor(axisColor);
    axisY->setGridLineColor(gridColor);
    axisY->setLabelsFont(QFont("Segoe UI", 8));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    auto* view = new QChartView(chart, m_salesCard);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet("background: transparent;");
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    static_cast<QVBoxLayout*>(m_salesCard->layout())->addWidget(view, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Top Products Bar Chart
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWidget::setTopProductsData(const QMap<QString, int>& data) {
    if (!m_productsCard) return;
    m_productsData = data;
    clearChartViews(m_productsCard);

    const DesignTokens& tk = ThemeManager::instance().tokens();
    const QColor axisColor(tk.chartAxisText);
    const QColor gridColor(tk.chartGrid);

    auto* barSet = new QBarSet("Qty Sold");
    barSet->setColor(QColor(tk.chartLine));

    QStringList categories;
    QStringList fullNames;
    int count = 0;
    for (auto it = data.constBegin(); it != data.constEnd() && count < 6; ++it, ++count) {
        *barSet << it.value();
        fullNames << it.key();
        // Truncate axis label — full name shown via tooltip
        categories << it.key().left(10);
    }

    auto* series = new QBarSeries;
    series->append(barSet);
    series->setLabelsVisible(true);
    series->setLabelsPosition(QAbstractBarSeries::LabelsInsideEnd);

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setBackgroundBrush(Qt::transparent);
    chart->setBackgroundRoundness(0);
    chart->legend()->hide();
    chart->setMargins(QMargins(0, 0, 0, 0));

    auto* axisX = new QBarCategoryAxis;
    axisX->append(categories);
    axisX->setLabelsColor(axisColor);
    axisX->setGridLineColor(gridColor);
    axisX->setLabelsFont(QFont("Segoe UI", 8));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setLabelsColor(axisColor);
    axisY->setGridLineColor(gridColor);
    axisY->setLabelsFont(QFont("Segoe UI", 8));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    auto* view = new QChartView(chart, m_productsCard);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet("background: transparent;");
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view->setMouseTracking(true);

    // Tooltip — show full product name + qty on bar hover
    QObject::connect(barSet, &QBarSet::hovered,
        view, [view, barSet, fullNames](bool status, int index) {
            if (status && index >= 0 && index < fullNames.size()) {
                QString tip = QString("<b>%1</b><br>Qty Sold: %2")
                    .arg(fullNames.at(index))
                    .arg(static_cast<int>(barSet->at(index)));
                QToolTip::showText(QCursor::pos(), tip, view);
            } else {
                QToolTip::hideText();
            }
        });

    static_cast<QVBoxLayout*>(m_productsCard->layout())->addWidget(view, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Orders by Status Pie Chart
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWidget::setStatusData(const QMap<QString, int>& data) {
    if (!m_statusCard) return;
    m_statusData = data;
    clearChartViews(m_statusCard);

    const DesignTokens& tk = ThemeManager::instance().tokens();
    // Legend text uses textPrimary so it reads on every theme background
    const QColor legendTextColor(tk.textPrimary);

    static const QMap<QString, QColor> sliceColors = {
        {"Pending",          QColor("#FCD34D")},
        {"Out for Delivery", QColor("#38BDF8")},
        {"Delivered",        QColor("#4ADE80")},
        {"Cancelled",        QColor("#F87171")},
    };

    auto* series = new QPieSeries;
    // Smaller pie so the custom legend below has room
    series->setPieSize(0.62);
    series->setHoleSize(0.0);

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        auto* slice = series->append(it.key(), it.value());
        const QColor sliceCol = sliceColors.value(it.key(), QColor("#94A3B8"));
        slice->setColor(sliceCol);
        slice->setLabelVisible(false);
        // Tooltip on hover — capture label/value/color as values not via slice ptr
        const QString sliceLabel = it.key();
        const int     sliceValue = it.value();
        QObject::connect(slice, &QPieSlice::hovered, [sliceLabel, sliceValue](bool state) {
            if (state) {
                QToolTip::showText(QCursor::pos(),
                    QString("<b>%1</b><br>%2 orders").arg(sliceLabel).arg(sliceValue));
            } else {
                QToolTip::hideText();
            }
        });
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setBackgroundBrush(Qt::transparent);
    chart->setBackgroundRoundness(0);
    chart->legend()->setVisible(false);   // we draw our own legend below
    chart->setMargins(QMargins(0, 0, 0, 0));

    auto* view = new QChartView(chart, m_statusCard);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet("background: transparent;");
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view->setMaximumHeight(160);   // leave room for legend

    auto* lay = static_cast<QVBoxLayout*>(m_statusCard->layout());
    lay->addWidget(view, 1);

    // ── Custom legend — full labels, wrap-friendly, theme-aware ──────────────
    // We build it as a widget with a FlowLayout-style QHBoxLayout that wraps.
    // Each item: coloured 8×8 swatch + full label text.
    auto* legendWidget = new QWidget(m_statusCard);
    legendWidget->setStyleSheet("background: transparent;");
    auto* legendLay = new QHBoxLayout(legendWidget);
    legendLay->setContentsMargins(0, 6, 0, 0);
    legendLay->setSpacing(14);
    legendLay->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const QString statusName = it.key();
        const QColor  swatchCol  = sliceColors.value(statusName, QColor("#94A3B8"));

        auto* item = new QWidget(legendWidget);
        item->setStyleSheet("background: transparent;");
        auto* ilay = new QHBoxLayout(item);
        ilay->setContentsMargins(0, 0, 0, 0);
        ilay->setSpacing(5);

        auto* swatch = new QFrame(item);
        swatch->setFixedSize(8, 8);
        swatch->setStyleSheet(
            QString("background:%1; border-radius:2px;").arg(swatchCol.name()));

        auto* lbl = new QLabel(statusName, item);
        lbl->setStyleSheet(
            QString("font-size:11px; font-weight:400;"
                    " color:%1; background:transparent;")
                .arg(legendTextColor.name()));

        ilay->addWidget(swatch);
        ilay->addWidget(lbl);
        legendLay->addWidget(item);
    }

    lay->addWidget(legendWidget);
}

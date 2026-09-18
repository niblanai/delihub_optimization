#include "scheduled_order_dialog.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPoint>
#include <QScrollArea>
#include <QFrame>
#include <QStackedWidget>
#include <QCoreApplication>
#include <QFile>

// Helper to resolve sidebar SVG paths
static QString sidebarSvgPath(const QString& file) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/sidebar/" + file;
    if (QFile::exists(p)) return p;
    p = appDir + "/../src/sidebar/" + file;
    if (QFile::exists(p)) return p;
    return {};
}

static constexpr int IC_PRODUCT = 0;
static constexpr int IC_QTY     = 1;
static constexpr int IC_PRICE   = 2;   // Task 15: read-only unit price
static constexpr int IC_COUNT   = 3;

static const char* kDayNames[7] = { "Mon","Tue","Wed","Thu","Fri","Sat","Sun" };

ScheduledOrderDialog::ScheduledOrderDialog(const QList<Customer>& customers,
                                           const QList<Product>&  products,
                                           QWidget* parent)
    : QDialog(parent), m_customers(customers), m_products(products)
{
    setupUi();
}

void ScheduledOrderDialog::setupUi() {
    auto& L = LangManager::instance();
    setWindowTitle(L.t("Scheduled / Recurring Order"));
    setMinimumWidth(600);
    setMinimumHeight(580);
    setModal(true);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(0);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContents = new QWidget;
    auto* mainLayout = new QVBoxLayout(scrollContents);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 8);

    scrollArea->setWidget(scrollContents);
    outerLayout->addWidget(scrollArea, 1);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* headerGroup = new QGroupBox(L.t("Order Details"));
    auto* formLayout  = new QFormLayout(headerGroup);
    formLayout->setSpacing(10);

    // Hidden combo — kept for getScheduledOrder/setScheduledOrder compatibility
    m_customerCombo = new QComboBox;
    m_customerCombo->setVisible(false);
    m_customerCombo->addItem("— Select Customer —", 0);
    for (const auto& c : m_customers) m_customerCombo->addItem(c.name(), c.id());

    // Searchable customer field
    m_customerSearchEdit = new QLineEdit;
    m_customerSearchEdit->setPlaceholderText(L.t("Type customer name..."));
    m_customerSearchEdit->setObjectName("searchEdit");
    
    // Add search icon
    QAction* custSearchAction = new QAction(m_customerSearchEdit);
    custSearchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                                   QColor("#9CA3AF"), 16));
    m_customerSearchEdit->addAction(custSearchAction, QLineEdit::LeadingPosition);

    // Floating popup
    m_customerPopup = new QListWidget(this);
    m_customerPopup->setWindowFlags(Qt::ToolTip);
    m_customerPopup->setFocusPolicy(Qt::NoFocus);
    m_customerPopup->setMaximumHeight(180);
    m_customerPopup->setVisible(false);
    m_customerPopup->setObjectName("dataTable");

    m_customerSelectedLbl = new QLabel;
    m_customerSelectedLbl->setStyleSheet("color:#94A3B8; font-size:11px;");
    m_customerSelectedLbl->setVisible(false);

    auto* custWidget = new QWidget;
    auto* custLayout = new QVBoxLayout(custWidget);
    custLayout->setSpacing(4);
    custLayout->setContentsMargins(0,0,0,0);
    custLayout->addWidget(m_customerSearchEdit);
    custLayout->addWidget(m_customerSelectedLbl);
    formLayout->addRow(L.t("Customer *"), custWidget);

    m_timeEdit = new QTimeEdit(QTime(9, 0));
    m_timeEdit->setDisplayFormat("hh:mm AP");
    formLayout->addRow(L.t("Delivery Time *"), m_timeEdit);

    mainLayout->addWidget(headerGroup);

    // ── Task 5: Repeat toggle ─────────────────────────────────────────────────
    auto* repeatGroup  = new QGroupBox(L.t("Schedule Type"));
    auto* repeatVLayout = new QVBoxLayout(repeatGroup);
    repeatVLayout->setSpacing(8);

    m_repeatToggle = new QCheckBox(L.t("🔁  Repeat weekly (recurring)"));
    m_repeatToggle->setChecked(true);
    m_repeatToggle->setStyleSheet("font-weight:bold; color:#E2E8F0;");
    repeatVLayout->addWidget(m_repeatToggle);

    // Stacked: page 0 = weekdays, page 1 = single date
    m_modeStack = new QStackedWidget;

    // Page 0 — weekday checkboxes
    auto* daysWidget = new QWidget;
    auto* daysLayout = new QHBoxLayout(daysWidget);
    daysLayout->setSpacing(6);
    daysLayout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 7; ++i) {
        m_dayChecks[i] = new QCheckBox(kDayNames[i]);
        // Use theme-neutral styling — no hardcoded text colour so it adapts
        // to Dark, Light and Teal themes automatically.
        m_dayChecks[i]->setStyleSheet(
            "QCheckBox { padding: 6px 10px; border: 1px solid #334155;"
            " border-radius: 6px; }"
            "QCheckBox:checked { background: #0EA5E9; border-color: #0EA5E9;"
            " color: #ffffff; font-weight: bold; }"
            "QCheckBox::indicator { width: 0; height: 0; }");
        daysLayout->addWidget(m_dayChecks[i]);
    }
    m_modeStack->addWidget(daysWidget);   // index 0

    // Page 1 — single date
    auto* dateWidget = new QWidget;
    auto* dateLayout = new QFormLayout(dateWidget);
    dateLayout->setContentsMargins(0, 0, 0, 0);
    m_oneDateEdit = new QDateEdit(QDate::currentDate());
    m_oneDateEdit->setCalendarPopup(true);
    m_oneDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_oneDateEdit->setMinimumDate(QDate::currentDate());
    auto* dateHint = new QLabel(L.t("Order will be created once on this date."));
    dateHint->setStyleSheet("color:#94A3B8; font-size:11px;");
    dateLayout->addRow(L.t("Date *"), m_oneDateEdit);
    dateLayout->addRow("",        dateHint);
    m_modeStack->addWidget(dateWidget);   // index 1

    repeatVLayout->addWidget(m_modeStack);
    mainLayout->addWidget(repeatGroup);

    connect(m_repeatToggle, &QCheckBox::toggled, this, &ScheduledOrderDialog::onRepeatToggled);

    // ── Reminder & Auto-Create Settings ──────────────────────────────────────
    auto* reminderGroup  = new QGroupBox(L.t("Reminder & Auto-Create Settings"));
    auto* reminderForm   = new QFormLayout(reminderGroup);
    reminderForm->setSpacing(10);

    m_remindMinSpin = new QSpinBox;
    m_remindMinSpin->setRange(5, 1440);
    m_remindMinSpin->setValue(60);
    m_remindMinSpin->setSuffix(L.t("  min before delivery"));
    reminderForm->addRow(L.t("⏰ Remind me:"), m_remindMinSpin);

    m_remindRepeatSpin = new QSpinBox;
    m_remindRepeatSpin->setRange(0, 120);
    m_remindRepeatSpin->setValue(0);
    m_remindRepeatSpin->setSpecialValueText(L.t("Once only (no repeat)"));
    m_remindRepeatSpin->setSuffix(L.t("  min  (repeat interval)"));
    reminderForm->addRow(L.t("🔁 Repeat every:"), m_remindRepeatSpin);

    m_autoCreateChk = new QCheckBox(L.t(
        "Automatically create a pending order at reminder time\n"
        "(order will appear in Orders tab — fill remaining details there)"));
    reminderForm->addRow(L.t("🤖 Auto-create:"), m_autoCreateChk);

    mainLayout->addWidget(reminderGroup);

    // ── Items table ───────────────────────────────────────────────────────────
    auto* itemsGroup  = new QGroupBox(L.t("Fixed Products for this Order"));
    auto* itemsLayout = new QVBoxLayout(itemsGroup);

    auto* btnRow = new QHBoxLayout;
    m_addItemBtn = new QPushButton(L.t("＋ Add Product"));
    m_addItemBtn->setObjectName("addBtn");
    m_removeItemBtn = new QPushButton(L.t("－ Remove"));
    m_removeItemBtn->setObjectName("removeBtn");
    m_removeItemBtn->setEnabled(false);
    btnRow->addWidget(m_addItemBtn);
    btnRow->addWidget(m_removeItemBtn);
    btnRow->addStretch();
    itemsLayout->addLayout(btnRow);

    m_itemsTable = new QTableWidget(0, IC_COUNT);
    m_itemsTable->setObjectName("dataTable");
    m_itemsTable->setHorizontalHeaderLabels({
        L.t("Product (name or barcode)"),
        L.t("Qty"),
        L.t("Unit Price")
    });
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_PRODUCT, QHeaderView::Stretch);
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_QTY,     QHeaderView::Fixed);
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_PRICE,   QHeaderView::Fixed);
    m_itemsTable->setColumnWidth(IC_QTY,   80);
    m_itemsTable->setColumnWidth(IC_PRICE, 110);
    m_itemsTable->verticalHeader()->setVisible(false);
    m_itemsTable->verticalHeader()->setDefaultSectionSize(44);
    m_itemsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_itemsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_itemsTable->setMinimumHeight(140);
    itemsLayout->addWidget(m_itemsTable);
    mainLayout->addWidget(itemsGroup);

    // Buttons
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    m_buttons->button(QDialogButtonBox::Save)->setText(L.t("Save"));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(L.t("Cancel"));
    outerLayout->addWidget(m_buttons);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_addItemBtn,    &QPushButton::clicked, this, &ScheduledOrderDialog::onAddItem);
    connect(m_removeItemBtn, &QPushButton::clicked, this, &ScheduledOrderDialog::onRemoveItem);
    connect(m_itemsTable, &QTableWidget::itemSelectionChanged, this, [this](){
        m_removeItemBtn->setEnabled(m_itemsTable->currentRow() >= 0);
    });
    connect(m_itemsTable, &QTableWidget::currentCellChanged, this,
        [this](int row, int, int, int){
            m_removeItemBtn->setEnabled(row >= 0);
        });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &ScheduledOrderDialog::validate);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ── Customer search connections ────────────────────────────────────────
    connect(m_customerSearchEdit, &QLineEdit::textChanged,
            this, &ScheduledOrderDialog::onCustomerSearchChanged);
    connect(m_customerPopup, &QListWidget::itemClicked,
            this, &ScheduledOrderDialog::onCustomerSelected);
}

// Customer search
void ScheduledOrderDialog::onCustomerSearchChanged(const QString& text) {
    m_customerPopup->clear();
    QString lower = text.trimmed().toLower();
    if (lower.isEmpty()) { m_customerPopup->hide(); return; }

    int count = 0;
    for (const auto& c : m_customers) {
        if (c.name().toLower().contains(lower)) {
            auto* it = new QListWidgetItem(c.name());
            it->setData(Qt::UserRole,     c.id());
            it->setData(Qt::UserRole + 1, c.name());
            m_customerPopup->addItem(it);
            if (++count >= 12) break;
        }
    }
    if (count > 0) {
        QPoint pos = m_customerSearchEdit->mapToGlobal(
            QPoint(0, m_customerSearchEdit->height()));
        m_customerPopup->move(pos);
        m_customerPopup->setFixedWidth(qMax(m_customerSearchEdit->width(), 280));
        m_customerPopup->raise();
        m_customerPopup->show();
    } else {
        m_customerPopup->hide();
    }
}

void ScheduledOrderDialog::onCustomerSelected(QListWidgetItem* item) {
    if (!item) return;
    m_customerId = item->data(Qt::UserRole).toInt();
    QString name = item->data(Qt::UserRole + 1).toString();

    m_customerSearchEdit->blockSignals(true);
    m_customerSearchEdit->setText(name);
    m_customerSearchEdit->blockSignals(false);
    m_customerPopup->hide();

    m_customerSelectedLbl->setText(QString("✓ Customer: %1").arg(name));
    m_customerSelectedLbl->setVisible(true);

    // Sync hidden combo
    int idx = m_customerCombo->findData(m_customerId);
    if (idx >= 0) m_customerCombo->setCurrentIndex(idx);
}

// Task 5: toggle between recurring weekdays and single date
void ScheduledOrderDialog::onRepeatToggled(bool checked) {
    m_modeStack->setCurrentIndex(checked ? 0 : 1);
    m_repeatToggle->setText(checked ? "🔁  Repeat weekly (recurring)" : "📅  One-time (single date)");
}

void ScheduledOrderDialog::rebuildItemRow(int row, int productId, int qty) {
    auto* searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Type name or barcode...");
    searchEdit->setProperty("productId", productId);
    searchEdit->setFixedHeight(30);
    if (productId > 0)
        for (const auto& p : m_products)
            if (p.id() == productId) { searchEdit->setText(p.name()); break; }

    auto* popup = new QListWidget(this);
    popup->setWindowFlags(Qt::ToolTip);
    popup->setFocusPolicy(Qt::NoFocus);
    popup->setMaximumHeight(130);
    popup->setVisible(false);
    popup->setObjectName("dataTable");

    connect(searchEdit, &QLineEdit::textChanged, this,
        [this, searchEdit, popup](const QString& txt) {
            popup->clear();
            QString lower = txt.trimmed().toLower();
            if (lower.isEmpty()) { popup->hide(); return; }
            int count = 0;
            for (const auto& p : m_products) {
                if (p.name().toLower().startsWith(lower)
                    || (!p.barcode().isEmpty() && p.barcode().startsWith(txt.trimmed()))) {
                    auto* it = new QListWidgetItem(
                        p.barcode().isEmpty() ? p.name()
                            : QString("%1  [%2]").arg(p.name(), p.barcode()));
                    it->setData(Qt::UserRole, p.id());
                    popup->addItem(it);
                    if (++count >= 8) break;
                }
            }
            if (count > 0) {
                QPoint pos = searchEdit->mapToGlobal(QPoint(0, searchEdit->height()));
                popup->move(pos);
                popup->setFixedWidth(qMax(searchEdit->width(), 200));
                popup->raise(); popup->show();
            } else {
                popup->hide();
            }
        });

    connect(popup, &QListWidget::itemClicked, this,
        [this, row, searchEdit, popup](QListWidgetItem* it) {
            int pid = it->data(Qt::UserRole).toInt();
            QString txt = it->text();
            int bracket = txt.indexOf("  [");
            searchEdit->blockSignals(true);
            searchEdit->setText(bracket >= 0 ? txt.left(bracket) : txt);
            searchEdit->setProperty("productId", pid);
            searchEdit->blockSignals(false);
            popup->hide();
            // Task 15: update price cell
            for (const auto& p : m_products) {
                if (p.id() == pid) {
                    if (auto* pi = m_itemsTable->item(row, IC_PRICE))
                        pi->setText(QString::number(p.price(), 'f', 2));
                    break;
                }
            }
        });

    m_itemsTable->setCellWidget(row, IC_PRODUCT, searchEdit);

    auto* qtySpin = new QSpinBox;
    qtySpin->setRange(1, 9999);
    qtySpin->setValue(qty);
    qtySpin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    qtySpin->setFixedHeight(30);
    m_itemsTable->setCellWidget(row, IC_QTY, qtySpin);

    // Task 15: read-only unit price column — auto-filled from product catalog
    auto* priceItem = new QTableWidgetItem("—");
    priceItem->setTextAlignment(Qt::AlignCenter);
    priceItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);  // not editable
    priceItem->setForeground(QBrush(QColor("#94A3B8")));
    if (productId > 0) {
        for (const auto& p : m_products) {
            if (p.id() == productId) {
                priceItem->setText(QString::number(p.price(), 'f', 2));
                break;
            }
        }
    }
    m_itemsTable->setItem(row, IC_PRICE, priceItem);
}

void ScheduledOrderDialog::onAddItem() {
    int row = m_itemsTable->rowCount();
    m_itemsTable->insertRow(row);
    rebuildItemRow(row);
    m_itemsTable->selectRow(row);
    m_removeItemBtn->setEnabled(true);
}

void ScheduledOrderDialog::onRemoveItem() {
    int row = m_itemsTable->currentRow();
    if (row >= 0) {
        m_itemsTable->removeRow(row);
        m_removeItemBtn->setEnabled(m_itemsTable->rowCount() > 0 &&
                                    m_itemsTable->currentRow() >= 0);
    }
}

void ScheduledOrderDialog::setScheduledOrder(const ScheduledOrder& order) {
    m_orderId    = order.id;
    m_customerId = order.customerId;
    int idx = m_customerCombo->findData(order.customerId);
    if (idx >= 0) m_customerCombo->setCurrentIndex(idx);
    // Pre-fill search field with customer name
    if (!order.customerName.isEmpty()) {
        m_customerSearchEdit->blockSignals(true);
        m_customerSearchEdit->setText(order.customerName);
        m_customerSearchEdit->blockSignals(false);
        m_customerSelectedLbl->setText(QString("✓ Customer: %1").arg(order.customerName));
        m_customerSelectedLbl->setVisible(true);
    }
    if (order.time.isValid()) m_timeEdit->setTime(order.time);

    // Task 5: restore repeat mode
    m_repeatToggle->setChecked(order.isRecurring);
    m_modeStack->setCurrentIndex(order.isRecurring ? 0 : 1);
    m_repeatToggle->setText(order.isRecurring
        ? "🔁  Repeat weekly (recurring)"
        : "📅  One-time (single date)");

    for (int i = 0; i < 7; ++i)
        m_dayChecks[i]->setChecked(order.isScheduledFor(i + 1));

    if (!order.isRecurring && order.oneTimeDate.isValid())
        m_oneDateEdit->setDate(order.oneTimeDate);

    m_remindMinSpin->setValue(order.remindMinutesBefore > 0 ? order.remindMinutesBefore : 60);
    m_remindRepeatSpin->setValue(order.remindRepeatInterval);
    m_autoCreateChk->setChecked(order.autoCreateOrder);

    m_itemsTable->setRowCount(0);
    for (const auto& item : order.items) {
        int row = m_itemsTable->rowCount();
        m_itemsTable->insertRow(row);
        rebuildItemRow(row, item.productId, item.quantity);
    }
}

ScheduledOrder ScheduledOrderDialog::getScheduledOrder() const {
    ScheduledOrder s;
    s.id         = m_orderId;
    s.customerId = m_customerId;
    s.customerName = m_customerSearchEdit->text().trimmed();
    s.time = m_timeEdit->time();

    // Task 5: mode
    s.isRecurring = m_repeatToggle->isChecked();
    if (s.isRecurring) {
        QStringList days;
        for (int i = 0; i < 7; ++i)
            if (m_dayChecks[i]->isChecked()) days << QString::number(i + 1);
        s.weekdays = days.join(',');
    } else {
        s.weekdays    = QString();
        s.oneTimeDate = m_oneDateEdit->date();
    }

    s.remindMinutesBefore  = m_remindMinSpin->value();
    s.remindRepeatInterval = m_remindRepeatSpin->value();
    s.autoCreateOrder      = m_autoCreateChk->isChecked();

    for (int r = 0; r < m_itemsTable->rowCount(); ++r) {
        auto* se    = qobject_cast<QLineEdit*>(m_itemsTable->cellWidget(r, IC_PRODUCT));
        auto* qSpin = qobject_cast<QSpinBox*>( m_itemsTable->cellWidget(r, IC_QTY));
        if (!se || !qSpin) continue;

        int pid = se->property("productId").toInt();
        if (pid == 0) {
            QString name = se->text().trimmed();
            for (const auto& p : m_products)
                if (p.name().compare(name, Qt::CaseInsensitive) == 0) { pid = p.id(); break; }
        }
        if (pid == 0) continue;

        OrderItem item;
        item.productId   = pid;
        item.quantity    = qSpin->value();
        item.unitPrice   = 0.0;
        for (const auto& p : m_products)
            if (p.id() == pid) {
                item.productName = p.name();
                item.unitPrice   = p.price();   // Task 15: store price from catalog
                break;
            }
        s.items.append(item);
    }
    return s;
}

void ScheduledOrderDialog::validate() {
    if (m_customerId == 0) {
        QMessageBox::warning(this, "Validation", "Please search for and select a customer.");
        m_customerSearchEdit->setFocus();
        return;
    }
    if (m_repeatToggle->isChecked()) {
        bool anyDay = false;
        for (int i = 0; i < 7; ++i) if (m_dayChecks[i]->isChecked()) { anyDay = true; break; }
        if (!anyDay) {
            QMessageBox::warning(this, "Validation", "Please select at least one weekday.");
            return;
        }
    } else {
        if (!m_oneDateEdit->date().isValid()) {
            QMessageBox::warning(this, "Validation", "Please select a valid date.");
            return;
        }
    }
    if (m_itemsTable->rowCount() == 0) {
        QMessageBox::warning(this, "Validation", "Please add at least one product.");
        return;
    }
    accept();
}

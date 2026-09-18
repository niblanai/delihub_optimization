#include "order_dialog.h"
#include "infra/config_manager.h"
#include "services/lang_manager.h"
#include "services/session_manager.h"
#include "services/invoice_parser.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "ui/orders/invoice_review_dialog.h"
#include "core/delivery_driver.h"
#include "core/coupon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QSpinBox>
#include <QFrame>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QTimer>
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
#include <QFileDialog>
#include <cmath>

// ── BarcodeLineEdit ───────────────────────────────────────────────────────────
// Wraps a normal QLineEdit but tracks inter-keystroke timing.  When a burst of
// characters arrives faster than a human can type (each char within
// kScannerMaxMs ms) AND is terminated by Enter/Return, the accumulated text is
// treated as a completed barcode scan and scanComplete() is emitted.
// For normal (slow) typing the widget behaves exactly like a plain QLineEdit.
class BarcodeLineEdit : public QLineEdit {
    Q_OBJECT
signals:
    void scanComplete(const QString& barcode);
    void keyPress(int key);    // Up/Down/Enter/Escape for popup navigation
public:
    explicit BarcodeLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {}

    // Called by the popup show/hide logic so we know whether to block Enter
    void setPopupVisible(bool v) { m_popupVisible = v; }
    bool popupVisible() const    { return m_popupVisible; }

protected:
    void keyPressEvent(QKeyEvent* ev) override {
        int k = ev->key();

        // Navigation keys always go to popup handler
        if (k == Qt::Key_Up || k == Qt::Key_Down || k == Qt::Key_Escape) {
            emit keyPress(k);
            ev->accept();
            return;
        }

        qint64 elapsed = m_lastKey.isValid() ? m_lastKey.elapsed() : 999;
        m_lastKey.restart();

        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            // Scanner burst: treat as barcode scan
            QString txt = text().trimmed();
            if (!txt.isEmpty() && m_scannerBurst) {
                emit scanComplete(txt);
                m_scannerBurst = false;
                ev->accept();
                return;
            }
            m_scannerBurst = false;

            if (m_popupVisible) {
                // Popup is open → commit selection, DO NOT propagate to QDialog
                emit keyPress(Qt::Key_Return);
                ev->accept();   // block QDialog from receiving this Enter
                return;
            }
            // Popup closed → let QDialog handle Enter → Save
            QLineEdit::keyPressEvent(ev);
            return;
        }

        if (elapsed < kScannerMaxMs) m_scannerBurst = true;
        else                          m_scannerBurst = false;
        QLineEdit::keyPressEvent(ev);
    }
private:
    static constexpr qint64 kScannerMaxMs = 35;
    QElapsedTimer m_lastKey;
    bool          m_scannerBurst  = false;
    bool          m_popupVisible  = false;
};
#include "order_dialog.moc"

static constexpr int IC_PRODUCT   = 0;
static constexpr int IC_BARCODE   = 1;
static constexpr int IC_QTY       = 2;
static constexpr int IC_UNITPRICE = 3;
static constexpr int IC_TOTAL     = 4;
static constexpr int IC_COUNT     = 5;

OrderDialog::OrderDialog(const QList<Customer>&      customers,
                         const QList<Product>&        products,
                         const QStringList&           statuses,
                         const QList<DeliveryDriver>& drivers,
                         const QMap<int,double>&      regionFeeMap,
                         ICouponRepository*           couponRepo,
                         QWidget* parent)
    : QDialog(parent), m_customers(customers), m_products(products),
      m_drivers(drivers), m_statuses(statuses), m_regionFeeMap(regionFeeMap),
      m_couponRepo(couponRepo)
{
    setupUi();
}

void OrderDialog::setupUi() {
    auto& L = LangManager::instance();
    setWindowTitle(L.t("Order"));
    setMinimumWidth(700);
    setMinimumHeight(640);
    setModal(true);

    const QString sym = ConfigManager::instance().currencySymbol();

    // ── Root: scrollable content ──────────────────────────────────────────────
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(0);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Scroll area for everything except the buttons
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* scrollContents = new QWidget;
    auto* root = new QVBoxLayout(scrollContents);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 4);
    scrollArea->setWidget(scrollContents);
    outerLayout->addWidget(scrollArea, 1);

    // ── Customer search ──────────────────────────────────────────────────────
    auto* custGroup  = new QGroupBox(L.t("Customer"));
    auto* custLayout = new QVBoxLayout(custGroup);

    m_customerSearch = new QLineEdit;
    m_customerSearch->setPlaceholderText(L.t("Type to search customer by name..."));
    m_customerSearch->setObjectName("searchEdit");
    
    // Add search icon
    QAction* custSearchAction = new QAction(m_customerSearch);
    custSearchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                                   QColor("#9CA3AF"), 16));
    m_customerSearch->addAction(custSearchAction, QLineEdit::LeadingPosition);
    
    custLayout->addWidget(m_customerSearch);

    m_customerList = new QListWidget;
    m_customerList->setMaximumHeight(100);
    m_customerList->setVisible(false);
    custLayout->addWidget(m_customerList);

    m_customerLabel = new QLabel(L.t("No customer selected"));
    m_customerLabel->setStyleSheet("color:#4ADE80; font-weight:bold; font-size:13px;");
    custLayout->addWidget(m_customerLabel);

    root->addWidget(custGroup);

    // ── Order Details ────────────────────────────────────────────────────────
    auto* detailGroup = new QGroupBox(L.t("Order Details"));
    auto* form        = new QFormLayout(detailGroup);
    form->setSpacing(10);

    m_dateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    m_dateTimeEdit->setDisplayFormat("yyyy-MM-dd  hh:mm");
    m_dateTimeEdit->setCalendarPopup(true);
    {
        bool canEdit = SessionManager::instance().currentRole().canEditOrderDate;
        m_dateTimeEdit->setReadOnly(!canEdit);
        m_dateTimeEdit->setEnabled(canEdit);
        if (!canEdit)
            m_dateTimeEdit->setToolTip(L.t("Your role does not allow changing the order date/time."));
    }
    form->addRow(L.t("Date & Time *"), m_dateTimeEdit);

    m_deliveryFeeSpin = new QDoubleSpinBox;
    m_deliveryFeeSpin->setRange(0.0, 99999.99);
    m_deliveryFeeSpin->setDecimals(2);
    m_deliveryFeeSpin->setSuffix("  " + sym);
    form->addRow(L.t("Delivery Fee"), m_deliveryFeeSpin);

    m_statusCombo = new QComboBox;
    for (const QString& s : m_statuses)
        m_statusCombo->addItem(L.t(s), s);   // display = translated, value = English key
    form->addRow(L.t("Status"), m_statusCombo);

    m_cancelReasonLbl  = new QLabel(L.t("Cancel Reason"));
    m_cancelReasonEdit = new QLineEdit;
    m_cancelReasonEdit->setPlaceholderText(L.t("Reason for cancellation..."));
    form->addRow(m_cancelReasonLbl, m_cancelReasonEdit);
    m_cancelReasonLbl->setVisible(false);
    m_cancelReasonEdit->setVisible(false);

    // Delivery Driver selector
    m_driverCombo = new QComboBox;
    m_driverCombo->addItem(L.t("— No Driver Assigned —"), 0);
    for (const auto& d : m_drivers)
        if (d.active())
            m_driverCombo->addItem(
                QString("%1  📞 %2").arg(d.name(), d.phone()), d.id());
    form->addRow(L.t("Delivery Driver"), m_driverCombo);

    // Payment Method
    m_paymentCombo = new QComboBox;
    for (const QString& pm : Order::paymentMethods())
        m_paymentCombo->addItem(pm, pm);
    form->addRow(L.t("💳 Payment Method *"), m_paymentCombo);

    // "Other" detail field — hidden by default
    m_paymentOtherLbl  = new QLabel(L.t("Specify method:"));
    m_paymentOtherEdit = new QLineEdit;
    m_paymentOtherEdit->setPlaceholderText(L.t("e.g. Bank transfer, Instapay..."));
    m_paymentOtherEdit->setMinimumHeight(32);
    form->addRow(m_paymentOtherLbl, m_paymentOtherEdit);
    m_paymentOtherLbl->setVisible(false);
    m_paymentOtherEdit->setVisible(false);

    connect(m_paymentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int) {
            bool isOther = (m_paymentCombo->currentData().toString() == "Other");
            m_paymentOtherLbl->setVisible(isOther);
            m_paymentOtherEdit->setVisible(isOther);
        });

    // Discount
    m_discountSpin = new QDoubleSpinBox;
    m_discountSpin->setRange(0.0, 99999.0);
    m_discountSpin->setDecimals(2);
    m_discountSpin->setSuffix("  " + ConfigManager::instance().currencySymbol());
    m_discountSpin->setSpecialValueText(L.t("No discount"));
    form->addRow(L.t("🏷 Discount"), m_discountSpin);

    m_discountReasonEdit = new QLineEdit;
    m_discountReasonEdit->setPlaceholderText(L.t("Reason for discount (e.g. Loyalty, Damage)..."));
    form->addRow(L.t("Discount Reason"), m_discountReasonEdit);

    // Task 12: Activate Coupon button + hidden coupon row
    m_activateCouponBtn = new QPushButton(L.t("🏷  Activate Coupon"));
    m_activateCouponBtn->setObjectName("secondaryBtn");
    m_activateCouponBtn->setCheckable(false);
    form->addRow("", m_activateCouponBtn);

    // Coupon input row (hidden by default, revealed on button click)
    m_couponRow = new QWidget;
    auto* couponLayout = new QHBoxLayout(m_couponRow);
    couponLayout->setContentsMargins(0, 0, 0, 0);
    couponLayout->setSpacing(8);
    m_couponCodeEdit = new QLineEdit;
    m_couponCodeEdit->setPlaceholderText(L.t("Enter coupon code..."));
    m_couponCodeEdit->setMaxLength(50);
    m_applyCouponBtn = new QPushButton(L.t("Apply"));
    m_applyCouponBtn->setObjectName("primaryBtn");
    m_applyCouponBtn->setFixedWidth(80);
    m_couponStatusLabel = new QLabel;
    m_couponStatusLabel->setWordWrap(true);
    couponLayout->addWidget(m_couponCodeEdit, 1);
    couponLayout->addWidget(m_applyCouponBtn);
    couponLayout->addWidget(m_couponStatusLabel, 1);
    m_couponRow->setVisible(false);
    form->addRow(L.t("Coupon Code:"), m_couponRow);

    // Toggle coupon row visibility
    connect(m_activateCouponBtn, &QPushButton::clicked, this, [this]() {
        m_couponRow->setVisible(!m_couponRow->isVisible());
        m_activateCouponBtn->setText(m_couponRow->isVisible()
            ? "🏷  Hide Coupon" : "🏷  Activate Coupon");
    });

    // Apply coupon logic
    connect(m_applyCouponBtn, &QPushButton::clicked, this, [this]() {
        if (!m_couponRepo) {
            m_couponStatusLabel->setStyleSheet("color:#F87171;");
            m_couponStatusLabel->setText("Coupon system not available.");
            return;
        }
        QString code = m_couponCodeEdit->text().trimmed().toUpper();
        if (code.isEmpty()) {
            m_couponStatusLabel->setStyleSheet("color:#F87171;");
            m_couponStatusLabel->setText("Please enter a code.");
            return;
        }
        Coupon c = m_couponRepo->getByCode(code);
        if (c.id() <= 0) {
            m_couponStatusLabel->setStyleSheet("color:#F87171;");
            m_couponStatusLabel->setText("Code not found.");
            m_appliedCouponId = 0;
            return;
        }
        if (!c.isValid()) {
            m_couponStatusLabel->setStyleSheet("color:#F87171;");
            QString reason = !c.isActive() ? "inactive"
                : (c.expiryDate().isValid() && c.expiryDate() < QDate::currentDate()) ? "expired"
                : "usage limit reached";
            m_couponStatusLabel->setText(QString("Coupon %1.").arg(reason));
            m_appliedCouponId = 0;
            return;
        }
        // Calculate discount on current subtotal
        double sub = 0.0;
        for (int r = 0; r < m_itemsTable->rowCount(); ++r) {
            auto* ps = qobject_cast<QDoubleSpinBox*>(m_itemsTable->cellWidget(r, IC_UNITPRICE));
            auto* qs = qobject_cast<QSpinBox*>(       m_itemsTable->cellWidget(r, IC_QTY));
            if (ps && qs) sub += ps->value() * qs->value();
        }
        double disc = c.calculateDiscount(sub);
        if (disc < 0.001) {
            m_couponStatusLabel->setStyleSheet("color:#F87171;");
            m_couponStatusLabel->setText("Coupon gives 0 discount on current items.");
            return;
        }
        // Apply: set discount spin + reason
        m_discountSpin->setValue(disc);
        m_discountReasonEdit->setText(
            QString("Coupon %1 (%2%3)")
                .arg(c.code())
                .arg(c.type() == Coupon::Type::Percentage
                     ? QString::number(c.value(), 'f', 1) + "% off"
                     : ConfigManager::instance().currencySymbol() + " " + QString::number(c.value(), 'f', 2)));
        m_appliedCouponId = c.id();
        m_couponStatusLabel->setStyleSheet("color:#4ADE80;");
        m_couponStatusLabel->setText(
            QString("✓ Applied: -%1 %2")
                .arg(QString::number(disc, 'f', 2),
                     ConfigManager::instance().currencySymbol()));
        recalcTotals();
    });
    // Also allow Enter to apply
    connect(m_couponCodeEdit, &QLineEdit::returnPressed, m_applyCouponBtn, &QPushButton::click);

    root->addWidget(detailGroup);

    // ── Items Table ──────────────────────────────────────────────────────────
    auto* itemsGroup  = new QGroupBox(L.t("Order Items"));
    auto* itemsLayout = new QVBoxLayout(itemsGroup);

    auto* btnRow = new QHBoxLayout;
    m_addItemBtn = new QPushButton(L.t("＋ Add Item"));
    m_addItemBtn->setObjectName("addBtn");
    m_removeItemBtn = new QPushButton(L.t("－ Remove"));
    m_removeItemBtn->setObjectName("removeBtn");
    m_removeItemBtn->setEnabled(false);
    m_uploadInvoiceBtn = new QPushButton("📄  " + L.t("رفع فاتورة PDF"));
    m_uploadInvoiceBtn->setObjectName("secondaryBtn");
    m_uploadInvoiceBtn->setToolTip(L.t("استيراد المنتجات من ملف PDF فاتورة"));
    m_uploadXlsBtn = new QPushButton("📊  " + L.t("رفع Excel"));
    m_uploadXlsBtn->setObjectName("secondaryBtn");
    m_uploadXlsBtn->setToolTip(L.t("استيراد المنتجات من ملف Excel (.xls)"));
    btnRow->addWidget(m_addItemBtn);
    btnRow->addWidget(m_removeItemBtn);
    btnRow->addWidget(m_uploadInvoiceBtn);
    btnRow->addWidget(m_uploadXlsBtn);
    btnRow->addStretch();
    itemsLayout->addLayout(btnRow);

    m_itemsTable = new QTableWidget(0, IC_COUNT);
    m_itemsTable->setObjectName("dataTable");
    m_itemsTable->setHorizontalHeaderLabels({
        L.t("Product (name or barcode)"),
        L.t("Barcode"),
        L.t("Qty"),
        L.t("Unit Price"),
        L.t("Line Total")
    });
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_PRODUCT,   QHeaderView::Stretch);
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_BARCODE,   QHeaderView::Interactive);
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_QTY,       QHeaderView::Fixed);
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_UNITPRICE, QHeaderView::Fixed);
    m_itemsTable->horizontalHeader()->setSectionResizeMode(IC_TOTAL,     QHeaderView::Fixed);
    m_itemsTable->setColumnWidth(IC_BARCODE,   110);
    m_itemsTable->setColumnWidth(IC_QTY,        70);
    m_itemsTable->setColumnWidth(IC_UNITPRICE, 120);
    m_itemsTable->setColumnWidth(IC_TOTAL,     120);
    m_itemsTable->verticalHeader()->setVisible(false);
    m_itemsTable->verticalHeader()->setDefaultSectionSize(44);
    m_itemsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);  // T5: lock row height
    m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_itemsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_itemsTable->setMinimumHeight(140);
    itemsLayout->addWidget(m_itemsTable);
    root->addWidget(itemsGroup);

    // ── Totals ───────────────────────────────────────────────────────────────
    auto* totalsGroup  = new QGroupBox;
    auto* totalsLayout = new QHBoxLayout(totalsGroup);
    totalsLayout->setSpacing(24);

    auto makeTotal = [&](const QString& label) -> QLabel* {
        auto* col   = new QVBoxLayout;
        auto* title = new QLabel(label);
        title->setObjectName("hintLabel");
        title->setAlignment(Qt::AlignCenter);
        auto* val = new QLabel("0.00 " + sym);
        val->setAlignment(Qt::AlignCenter);
        val->setStyleSheet("font-size:15px; font-weight:bold; color:#38BDF8;");
        col->addWidget(title);
        col->addWidget(val);
        totalsLayout->addLayout(col);
        return val;
    };
    m_subtotalLabel   = makeTotal(L.t("Subtotal"));
    totalsLayout->addWidget([]{auto*s=new QFrame;s->setFrameShape(QFrame::VLine);s->setStyleSheet("color:#334155;");return s;}());
    m_deliveryLabel   = makeTotal(L.t("Delivery Fee"));
    totalsLayout->addWidget([]{auto*s=new QFrame;s->setFrameShape(QFrame::VLine);s->setStyleSheet("color:#334155;");return s;}());
    m_grandTotalLabel = makeTotal(L.t("Grand Total"));
    m_grandTotalLabel->setStyleSheet("font-size:17px; font-weight:bold; color:#4ADE80;");
    root->addWidget(totalsGroup);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    m_buttons->button(QDialogButtonBox::Save)->setText(L.t("Save"));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(L.t("Cancel"));
    root->addWidget(m_buttons);
    connect(m_customerSearch, &QLineEdit::textChanged,
            this, &OrderDialog::onCustomerSearch);
    connect(m_customerList, &QListWidget::itemClicked,
            this, &OrderDialog::onCustomerSelected);
    connect(m_addItemBtn,       &QPushButton::clicked, this, &OrderDialog::onAddItem);
    connect(m_uploadInvoiceBtn, &QPushButton::clicked, this, &OrderDialog::onUploadInvoice);
    connect(m_uploadXlsBtn,     &QPushButton::clicked, this, &OrderDialog::onUploadXls);
    connect(m_removeItemBtn, &QPushButton::clicked, this, &OrderDialog::onRemoveItem);
    connect(m_deliveryFeeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OrderDialog::recalcTotals);
    // Recalc when discount changes
    if (m_discountSpin)
        connect(m_discountSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &OrderDialog::recalcTotals);
    connect(m_itemsTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]{ m_removeItemBtn->setEnabled(!m_itemsTable->selectedItems().isEmpty()); });
    // Use currentIndexChanged + currentData() so the English key is always compared,
    // not the translated display text which breaks when Arabic is active.
    connect(m_statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                onStatusChanged(m_statusCombo->currentData().toString());
            });
    connect(m_buttons, &QDialogButtonBox::accepted, this, &OrderDialog::validate);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ── Customer search / select ─────────────────────────────────────────────────
void OrderDialog::onCustomerSearch(const QString& text) {
    m_customerList->clear();
    const QString lower = text.trimmed().toLower();
    if (lower.isEmpty()) {
        m_customerList->setVisible(false);
        return;
    }
    bool any = false;
    for (const auto& c : m_customers) {
        bool nameMatch = c.name().toLower().contains(lower);
        // phone match
        bool phoneMatch = false;
        for (const auto& ph : c.phones())
            if (ph.number.contains(lower)) { phoneMatch = true; break; }
        // address match
        bool addrMatch = false;
        for (const auto& a : c.addresses())
            if (a.text.toLower().contains(lower)) { addrMatch = true; break; }

        if (nameMatch || phoneMatch || addrMatch) {
            // Build display label showing what matched
            QString label = c.name();
            if (!nameMatch) {
                if (phoneMatch)
                    for (const auto& ph : c.phones())
                        if (ph.number.contains(lower)) { label += "  📞 " + ph.number; break; }
                if (addrMatch)
                    for (const auto& a : c.addresses())
                        if (a.text.toLower().contains(lower)) { label += "  📍 " + a.text; break; }
            }
            auto* item = new QListWidgetItem(label);
            item->setData(Qt::UserRole,     c.id());
            item->setData(Qt::UserRole + 1, c.name());
            m_customerList->addItem(item);
            any = true;
        }
    }
    m_customerList->setVisible(any);
}

void OrderDialog::onCustomerSelected(QListWidgetItem* item) {
    if (!item) return;
    m_customerId = item->data(Qt::UserRole).toInt();
    QString name = item->data(Qt::UserRole + 1).toString();
    if (name.isEmpty()) name = item->text();
    m_customerLabel->setText("✔  " + name);
    m_customerSearch->blockSignals(true);
    m_customerSearch->setText(name);
    m_customerSearch->blockSignals(false);
    m_customerList->setVisible(false);

    // Auto-fill preferred payment method from customer profile
    for (const auto& c : m_customers) {
        if (c.id() == m_customerId) {
            // Payment method
            if (!c.preferredPaymentMethod().isEmpty()) {
                int pi = m_paymentCombo->findData(c.preferredPaymentMethod());
                if (pi >= 0) m_paymentCombo->setCurrentIndex(pi);
            }
            // Task 2: auto-fill delivery fee from customer's region
            // Only set when the fee spin is currently 0 (don't clobber existing value on edit)
            if (m_deliveryFeeSpin->value() < 0.001 && c.regionId() > 0 && m_regionFeeMap.contains(c.regionId())) {
                double fee = m_regionFeeMap.value(c.regionId(), 0.0);
                if (fee > 0.001)
                    m_deliveryFeeSpin->setValue(fee);
            }
            break;
        }
    }
}

// ── Status changed ───────────────────────────────────────────────────────────
void OrderDialog::onStatusChanged(const QString& status) {
    bool isCancelled = (status == "Cancelled");
    m_cancelReasonLbl->setVisible(isCancelled);
    m_cancelReasonEdit->setVisible(isCancelled);
}

// ── Item row ─────────────────────────────────────────────────────────────────
void OrderDialog::rebuildItemRow(int row, int productId, int qty, double unitPrice,
                                  const QString& /*barcode*/) {
    m_itemsTable->blockSignals(true);

    // ── Product search with live dropdown popup + scanner auto-capture ────────
    auto* searchEdit = new BarcodeLineEdit;
    searchEdit->setPlaceholderText("Type name or barcode…  (scanner auto-detects)");
    searchEdit->setProperty("productId", productId);
    searchEdit->setFixedHeight(30);
    if (productId > 0)
        for (const auto& p : m_products)
            if (p.id() == productId) { searchEdit->setText(p.name()); break; }

    // Popup list — SubWindow flag: stays above the dialog but doesn't
    // steal mouse capture when the user drags the scrollbar.
    auto* popup = new QListWidget;
    popup->setParent(this, Qt::SubWindow | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_ShowWithoutActivating, true);
    popup->setAttribute(Qt::WA_DeleteOnClose, false);
    popup->setFocusPolicy(Qt::NoFocus);
    popup->setMinimumHeight(240);
    popup->setMaximumHeight(300);
    popup->setMinimumWidth(300);
    popup->setVisible(false);
    popup->setObjectName("dataTable");
    // SingleSelection but NOT on hover — only on explicit click
    popup->setSelectionMode(QAbstractItemView::SingleSelection);
    popup->setMouseTracking(false);
    // Issue #3: The default QListWidget highlights items on hover because
    // QAbstractItemView::setMouseTracking is true internally.  The stylesheet
    // hover rule below overrides that visual highlight without disabling click-selection.
    popup->setStyleSheet(
        "QListWidget { border: 2px solid #334155; background: #1E293B; }"
        "QListWidget::item { padding: 6px 8px; min-height: 28px; color: #E2E8F0; }"
        "QListWidget::item:hover { background: #334155; color: #E2E8F0; }"
        "QListWidget::item:selected { background: #1D4ED8; color: #fff; }");

    // updatePopupPos: recomputes global position and moves the popup window.
    // Called once on show and by a QTimer while visible so scroll doesn't misplace it.
    auto updatePopupPos = [this, searchEdit, popup]() {
        if (!popup->isVisible()) return;
        // mapToGlobal gives screen coords; for a SubWindow child widget move()
        // expects parent-relative coords — convert back with mapFromGlobal.
        QPoint globalPos = searchEdit->mapToGlobal(QPoint(0, searchEdit->height()));
        QPoint parentPos = this->mapFromGlobal(globalPos);
        popup->move(parentPos);
        popup->setFixedWidth(qMax(searchEdit->width(), 300));
    };

    // Timer keeps the popup aligned while scrolling (fires every 50ms when visible)
    auto* posTimer = new QTimer(this);
    posTimer->setInterval(50);
    connect(posTimer, &QTimer::timeout, this, updatePopupPos);
    connect(popup, &QListWidget::destroyed, posTimer, &QTimer::stop);

    auto showPopup = [updatePopupPos, popup, posTimer, searchEdit]() {
        updatePopupPos();
        popup->raise();
        popup->show();
        posTimer->start();
        searchEdit->setPopupVisible(true);
    };

    // Issue #1 fix: track whether popup was dismissed by a click so we don't
    // re-show it immediately when textChanged fires after the selection.
    bool* suppressPopup = new bool(false);

    // Helper: commit the currently selected popup item (shared by Enter key and click)
    auto commitItem = [this, row, searchEdit, popup, suppressPopup, posTimer]() {
        QListWidgetItem* it = popup->currentItem();
        if (!it) return;
        int pid = it->data(Qt::UserRole).toInt();
        *suppressPopup = true;
        posTimer->stop();
        popup->hide();
        searchEdit->setPopupVisible(false);
        searchEdit->blockSignals(true);
        QString txt = it->text();
        int bracket = txt.indexOf("  [");
        searchEdit->setText(bracket >= 0 ? txt.left(bracket) : txt);
        searchEdit->setProperty("productId", pid);
        searchEdit->blockSignals(false);
        auto* qtySpin = qobject_cast<QSpinBox*>(m_itemsTable->cellWidget(row, IC_QTY));
        if (qtySpin) qtySpin->setFocus();
        else m_addItemBtn->setFocus();
        QTimer::singleShot(200, [suppressPopup]{ *suppressPopup = false; });
        auto* ps = qobject_cast<QDoubleSpinBox*>(m_itemsTable->cellWidget(row, IC_UNITPRICE));
        auto* bc = m_itemsTable->item(row, IC_BARCODE);
        for (const auto& p : m_products) {
            if (p.id() == pid) {
                if (ps) ps->setValue(p.price());
                if (bc) bc->setText(p.barcode());
                break;
            }
        }
        recalcTotals();
    };

    // Up/Down arrows navigate popup; Enter commits if popup visible, else fall through to Save
    connect(searchEdit, &BarcodeLineEdit::keyPress, this,
        [popup, commitItem](int key) {
            if (key == Qt::Key_Down && popup->isVisible()) {
                int next = qMin(popup->currentRow() + 1, popup->count() - 1);
                if (popup->currentRow() < 0 && popup->count() > 0) next = 0;
                popup->setCurrentRow(next);
                return;
            }
            if (key == Qt::Key_Up && popup->isVisible()) {
                int prev = qMax(popup->currentRow() - 1, 0);
                popup->setCurrentRow(prev);
                return;
            }
            if ((key == Qt::Key_Return || key == Qt::Key_Enter) && popup->isVisible()) {
                if (popup->currentItem()) commitItem();
                return;
            }
            if (key == Qt::Key_Escape && popup->isVisible()) {
                popup->hide();
                return;
            }
            // If popup is NOT visible: fall through — QDialog handles Enter → Save
        });

    // Fill popup when text changes
    connect(searchEdit, &QLineEdit::textChanged, this,
        [this, searchEdit, popup, showPopup, suppressPopup, posTimer](const QString& txt) {
            if (*suppressPopup) return;
            popup->clear();
            QString lower = txt.trimmed().toLower();
            int count = 0;
            for (const auto& p : m_products) {
                bool match = lower.isEmpty() ? false
                    : (p.name().toLower().contains(lower)
                       || (!p.barcode().isEmpty()
                           && p.barcode().toLower().contains(lower)));
                if (match) {
                    auto* it = new QListWidgetItem(
                        p.barcode().isEmpty() ? p.name()
                            : QString("%1  [%2]").arg(p.name(), p.barcode()));
                    it->setData(Qt::UserRole, p.id());
                    popup->addItem(it);
                    if (++count >= 12) break;
                }
            }
            if (count > 0) showPopup();
            else { posTimer->stop(); popup->hide(); searchEdit->setPopupVisible(false); }
        });

    // Hide popup when search edit loses focus
    connect(searchEdit, &QLineEdit::editingFinished, this,
        [popup, posTimer, searchEdit]() {
            posTimer->stop();
            popup->hide();
            searchEdit->setPopupVisible(false);
        });

    // Select from popup — click triggers commitItem
    connect(popup, &QListWidget::itemClicked, this,
        [commitItem](QListWidgetItem*) { commitItem(); });

    // ── Scanner auto-select ────────────────────────────────────────────────────
    connect(searchEdit, &BarcodeLineEdit::scanComplete, this,
        [this, row, searchEdit, popup, suppressPopup, posTimer](const QString& barcode) {
            posTimer->stop();
            popup->hide();
            int pid = 0;
            QString pname;
            for (const auto& p : m_products) {
                if (!p.barcode().isEmpty() &&
                    p.barcode().compare(barcode, Qt::CaseInsensitive) == 0) {
                    pid   = p.id(); pname = p.name(); break;
                }
            }
            if (pid == 0) return;

            *suppressPopup = true;
            searchEdit->blockSignals(true);
            searchEdit->setText(pname);
            searchEdit->setProperty("productId", pid);
            searchEdit->blockSignals(false);
            QTimer::singleShot(200, [suppressPopup]{ *suppressPopup = false; });

            auto* ps = qobject_cast<QDoubleSpinBox*>(m_itemsTable->cellWidget(row, IC_UNITPRICE));
            auto* bc = m_itemsTable->item(row, IC_BARCODE);
            for (const auto& p : m_products) {
                if (p.id() == pid) {
                    if (ps) ps->setValue(p.price());
                    if (bc) bc->setText(p.barcode());
                    break;
                }
            }
            recalcTotals();
        });

    // Cleanup: when the row is removed the searchEdit is destroyed — hide + delete popup
    connect(searchEdit, &QObject::destroyed, this,
        [popup, posTimer, suppressPopup]() {
            posTimer->stop();
            popup->hide();
            popup->deleteLater();
            delete suppressPopup;
        });
    m_itemsTable->setCellWidget(row, IC_PRODUCT, searchEdit);

    // Barcode display (read-only)
    auto* barcodeItem = new QTableWidgetItem;
    barcodeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    barcodeItem->setTextAlignment(Qt::AlignCenter);
    if (productId > 0)
        for (const auto& p : m_products)
            if (p.id() == productId) { barcodeItem->setText(p.barcode()); break; }
    m_itemsTable->setItem(row, IC_BARCODE, barcodeItem);

    // Qty spin
    auto* qtySpin = new QSpinBox;
    qtySpin->setRange(1, 9999);
    qtySpin->setValue(qty);
    qtySpin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    qtySpin->setFixedHeight(30);
    m_itemsTable->setCellWidget(row, IC_QTY, qtySpin);

    // Unit price spin — Task 15: read-only (auto-filled from product, not user-editable)
    auto* priceSpin = new QDoubleSpinBox;
    priceSpin->setRange(0.0, 99999.99);
    priceSpin->setDecimals(2);
    priceSpin->setValue(unitPrice);
    priceSpin->setSuffix("  " + ConfigManager::instance().currencySymbol());
    priceSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    priceSpin->setReadOnly(true);   // Task 15: price set from product catalog only
    priceSpin->setFixedHeight(30);
    priceSpin->setToolTip("Unit price is set from the Product Directory and cannot be edited here.");
    priceSpin->setStyleSheet(
        "QDoubleSpinBox { background:#1e293b; color:#94A3B8; border-color:#334155; }");
    m_itemsTable->setCellWidget(row, IC_UNITPRICE, priceSpin);

    // Line total
    auto* totalItem = new QTableWidgetItem;
    totalItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    totalItem->setTextAlignment(Qt::AlignCenter);
    m_itemsTable->setItem(row, IC_TOTAL, totalItem);

    m_itemsTable->blockSignals(false);

    // Live recalc on qty/price change
    auto recalc = [this, row]() {
        auto* ps = qobject_cast<QDoubleSpinBox*>(m_itemsTable->cellWidget(row, IC_UNITPRICE));
        auto* qs = qobject_cast<QSpinBox*>(       m_itemsTable->cellWidget(row, IC_QTY));
        if (!ps || !qs) return;
        double lt = qs->value() * ps->value();
        const QString sym = ConfigManager::instance().currencySymbol();
        if (auto* it = m_itemsTable->item(row, IC_TOTAL))
            it->setText(QString("%1 %2").arg(QString::number(lt, 'f', 2), sym));
        recalcTotals();
    };
    connect(qtySpin,   QOverload<int>::of(&QSpinBox::valueChanged),          this, recalc);
    connect(priceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, recalc);
    recalc();
}

void OrderDialog::onAddItem() {
    int row = m_itemsTable->rowCount();
    m_itemsTable->insertRow(row);
    rebuildItemRow(row);
}

void OrderDialog::onRemoveItem() {
    int row = m_itemsTable->currentRow();
    if (row < 0) return;
    m_itemsTable->removeRow(row);
    recalcTotals();
}

// ─────────────────────────────────────────────────────────────────────────────
// Excel Invoice Import (.xls)
// ─────────────────────────────────────────────────────────────────────────────
void OrderDialog::onUploadXls() {
    const QString xlsPath = QFileDialog::getOpenFileName(
        this,
        "اختر ملف Excel الفاتورة",
        QString(),
        "Excel Files (*.xls *.xlsx);;All Files (*)");
    if (xlsPath.isEmpty()) return;

    QString errorMsg;
    QList<InvoiceItem> parsed = InvoiceParser::parseXls(xlsPath, errorMsg);
    if (parsed.isEmpty()) {
        QMessageBox::warning(this, "فشل قراءة ملف Excel",
            errorMsg.isEmpty() ? "لم يتم العثور على منتجات في الملف." : errorMsg);
        return;
    }

    // Reuse the same review + matching flow as PDF upload
    onUploadInvoiceWithItems(parsed);
}

// ─────────────────────────────────────────────────────────────────────────────
// PDF Invoice Import
// ─────────────────────────────────────────────────────────────────────────────
void OrderDialog::onUploadInvoice() {
    const QString pdfPath = QFileDialog::getOpenFileName(
        this,
        "اختر ملف الفاتورة",
        QString(),
        "PDF Files (*.pdf);;All Files (*)");
    if (pdfPath.isEmpty()) return;

    QString errorMsg;
    QList<InvoiceItem> parsed = InvoiceParser::parse(pdfPath, errorMsg);
    if (parsed.isEmpty()) {
        QMessageBox::warning(this, "فشل قراءة الفاتورة",
            errorMsg.isEmpty()
                ? "لم يتم العثور على منتجات في الفاتورة."
                : errorMsg);
        return;
    }

    onUploadInvoiceWithItems(parsed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Shared review + matching flow for both PDF and XLS imports
// ─────────────────────────────────────────────────────────────────────────────
void OrderDialog::onUploadInvoiceWithItems(const QList<InvoiceItem>& parsed) {
    // Build name→id map for fast exact matching
    QMap<QString, int> nameToId;
    for (const Product& p : m_products)
        nameToId[p.name().trimmed().toLower()] = p.id();

    // Pre-match items so the review dialog can show results without a repo.
    // ── Inline matching — tiered priority ────────────────────────────────
    // Priority 1: exact match (case-insensitive, trimmed)
    // Priority 2: 90%+ similarity (Levenshtein-based)
    // Priority 3: 80%+ similarity
    // Priority 4: one contains the other (partial)
    // We prefer longer matches to avoid "كاكا" beating "كورونا كاكاو خام 90 جرام"

    // Helper: simple character-level similarity ratio
    auto similarity = [](const QString& a, const QString& b) -> double {
        if (a.isEmpty() || b.isEmpty()) return 0.0;
        if (a == b) return 1.0;
        // Count matching characters using longest common substring approach
        int aLen = a.length(), bLen = b.length();
        int maxLen = qMax(aLen, bLen);
        // Count how many chars of a appear in b (order-aware)
        int matches = 0;
        int bPos = 0;
        for (QChar c : a) {
            int found = b.indexOf(c, bPos);
            if (found >= 0) { matches++; bPos = found + 1; }
        }
        return (double)matches / maxLen;
    };

    struct MatchedRow {
        QString pdfName;
        double  qty;
        int     productId;
        QString matchedName;
    };
    QList<MatchedRow> rows;

    for (const InvoiceItem& item : parsed) {
        MatchedRow r;
        r.pdfName   = item.name;
        r.qty       = item.qty;
        r.productId = 0;

        const QString key = item.name.trimmed().toLower();

        // Pass 1: exact match
        if (nameToId.contains(key)) {
            r.productId = nameToId[key];
        }

        // Pass 2-4: scored match — find best match above threshold
        if (r.productId == 0) {
            int    bestId    = 0;
            double bestScore = 0.0;
            int    bestLen   = 0;   // prefer longer product names for same score

            for (const Product& p : m_products) {
                const QString pnLow = p.name().trimmed().toLower();
                double sc = 0.0;

                // Exact match after normalization
                if (pnLow == key) {
                    sc = 1.0;
                }
                // OCR name contains full product name (product name ⊆ OCR)
                else if (key.contains(pnLow) && pnLow.length() >= 4) {
                    // Score by how much of the OCR name is covered
                    sc = 0.85 + 0.1 * ((double)pnLow.length() / qMax(key.length(), 1));
                }
                // Product name contains full OCR name (OCR name ⊆ product)
                else if (pnLow.contains(key) && key.length() >= 4) {
                    sc = 0.80 + 0.1 * ((double)key.length() / qMax(pnLow.length(), 1));
                }
                // Similarity score
                else {
                    sc = similarity(key, pnLow);
                }

                // Prefer higher score AND longer product name (avoids "كاكا" vs "كورونا كاكاو...")
                bool better = (sc > bestScore)
                           || (sc >= bestScore - 0.01 && p.name().length() > bestLen);
                if (sc >= 0.70 && better) {
                    bestScore = sc;
                    bestId    = p.id();
                    bestLen   = p.name().length();
                }
            }
            r.productId = bestId;
        }

        // Get matched name
        for (const Product& p : m_products)
            if (p.id() == r.productId) { r.matchedName = p.name(); break; }

        rows.append(r);
    }

    // 4. Show review dialog (build ReviewedItem list manually)
    // Re-use the review dialog's table for display — build ReviewedItems
    QList<ReviewedItem> reviewItems;
    for (const MatchedRow& mr : rows) {
        ReviewedItem ri;
        ri.pdfName     = mr.pdfName;
        ri.qty         = mr.qty;
        ri.productId   = mr.productId;
        ri.matchedName = mr.matchedName;
        reviewItems.append(ri);
    }

    // We need to pass InvoiceItems + productRepo to InvoiceReviewDialog,
    // but here we have m_products directly.  Build a thin pass-through:
    // pass the already-matched ReviewedItems by constructing a custom dialog.
    // Since InvoiceReviewDialog does its own matching internally, we pass
    // the parsed items and a null-safe repo wrapper that returns m_products.

    // ── Simple approach: show a plain review table without repo ────────────
    // Build the dialog using parsed items, doing matching from m_products.
    // We extend InvoiceReviewDialog to also accept a pre-built product list.
    // For simplicity we show a QDialog directly here:

    auto* reviewDlg = new QDialog(this);
    reviewDlg->setWindowTitle("📄 مراجعة الفاتورة");
    reviewDlg->setMinimumWidth(640);
    reviewDlg->setMinimumHeight(400);
    // Do NOT set WA_DeleteOnClose — we need to read qtySpins after exec() returns.

    auto* vlay = new QVBoxLayout(reviewDlg);
    vlay->setContentsMargins(16, 16, 16, 16);
    vlay->setSpacing(10);

    auto* hint = new QLabel("راجع المنتجات المستخرجة من الفاتورة ثم اضغط \"إضافة للطلب\".");
    hint->setStyleSheet("color:#94A3B8; font-size:12px;");
    vlay->addWidget(hint);

    // Table: status | PDF name | matched | qty (editable)
    auto* tbl = new QTableWidget(0, 4, reviewDlg);
    tbl->setObjectName("dataTable");
    tbl->setHorizontalHeaderLabels({"", "اسم الصنف في الفاتورة", "المنتج المطابق", "الكمية"});
    tbl->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tbl->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tbl->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tbl->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tbl->verticalHeader()->setVisible(false);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setShowGrid(false);
    tbl->setSelectionMode(QAbstractItemView::NoSelection);

    int matched = 0;
    QList<QDoubleSpinBox*> qtySpins;

    for (const MatchedRow& mr : rows) {
        int r = tbl->rowCount();
        tbl->insertRow(r);
        bool ok = (mr.productId > 0);
        if (ok) ++matched;

        auto* statusIt = new QTableWidgetItem(ok ? "✅" : "⚠️");
        statusIt->setTextAlignment(Qt::AlignCenter);
        tbl->setItem(r, 0, statusIt);

        auto* nameIt = new QTableWidgetItem(mr.pdfName);
        nameIt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tbl->setItem(r, 1, nameIt);

        QString matchTxt = ok ? mr.matchedName : "لم يُطابَق — سيُتجاهل";
        auto* matchIt = new QTableWidgetItem(matchTxt);
        matchIt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (!ok) matchIt->setForeground(QBrush(QColor("#F87171")));
        tbl->setItem(r, 2, matchIt);

        auto* spin = new QDoubleSpinBox;
        spin->setMinimum(0.001);
        spin->setMaximum(99999);
        spin->setDecimals(3);
        spin->setValue(mr.qty);
        spin->setEnabled(ok);
        spin->setAlignment(Qt::AlignCenter);
        tbl->setCellWidget(r, 3, spin);
        qtySpins.append(spin);
        tbl->setRowHeight(r, 38);
    }
    vlay->addWidget(tbl, 1);

    // Stats
    auto* statsLbl = new QLabel(
        QString("✅ %1 مطابق   ⚠️ %2 غير مطابق (سيُتجاهل)")
            .arg(matched).arg(rows.size() - matched));
    statsLbl->setStyleSheet("font-size:12px; color:#94A3B8;");
    vlay->addWidget(statsLbl);

    // Buttons
    auto* btnBox    = new QDialogButtonBox(reviewDlg);
    auto* addBtn    = new QPushButton("✅  إضافة للطلب");
    addBtn->setObjectName("primaryBtn");
    addBtn->setDefault(true);
    auto* cancelBtn = new QPushButton("إلغاء");
    cancelBtn->setObjectName("secondaryBtn");
    btnBox->addButton(addBtn,    QDialogButtonBox::AcceptRole);
    btnBox->addButton(cancelBtn, QDialogButtonBox::RejectRole);
    connect(btnBox, &QDialogButtonBox::accepted, reviewDlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, reviewDlg, &QDialog::reject);
    vlay->addWidget(btnBox);

    if (reviewDlg->exec() != QDialog::Accepted) {
        reviewDlg->deleteLater();
        return;
    }

    // Snapshot qty values NOW while reviewDlg and its child widgets are still alive.
    // Must be done BEFORE deleteLater() because WA_DeleteOnClose was removed to
    // prevent the dialog (and its QDoubleSpinBox children) from being destroyed
    // during exec() — which would leave qtySpins[] as dangling pointers.
    QList<double> acceptedQtys;
    for (auto* spin : qtySpins)
        acceptedQtys.append(spin ? spin->value() : 1.0);

    reviewDlg->deleteLater(); // safe to schedule deletion now

    // 5. Add accepted rows to the order items table
    for (int i = 0; i < rows.size(); ++i) {
        const MatchedRow& mr = rows[i];
        if (mr.productId <= 0) continue;
        if (i >= acceptedQtys.size()) continue;

        double qty = acceptedQtys[i];

        // Find unit price from m_products
        double unitPrice = 0.0;
        for (const Product& p : m_products)
            if (p.id() == mr.productId) { unitPrice = p.price(); break; }

        int row = m_itemsTable->rowCount();
        m_itemsTable->insertRow(row);
        int qtyInt = qMax(1, static_cast<int>(std::round(qty)));
        rebuildItemRow(row, mr.productId, qtyInt, unitPrice);

        // rebuildItemRow already calls qtySpin->setValue(qty) internally,
        // but we set again to be sure the rounded value is correct.
        if (auto* qtySpin = qobject_cast<QSpinBox*>(m_itemsTable->cellWidget(row, IC_QTY)))
            qtySpin->setValue(qtyInt);
    }
    recalcTotals();
}

// ── Keyboard shortcuts ────────────────────────────────────────────────────────
// Numpad +  → add product row
// Numpad -  → remove selected/last product row
void OrderDialog::keyPressEvent(QKeyEvent* event) {
    // Numpad Plus (key = Qt::Key_Plus with numpad modifier, or Qt::Key_Insert on some layouts)
    if (event->key() == Qt::Key_Plus
        && (event->modifiers() == Qt::NoModifier
            || event->modifiers() == Qt::KeypadModifier)) {
        onAddItem();
        event->accept();
        return;
    }
    // Numpad Minus
    if (event->key() == Qt::Key_Minus
        && (event->modifiers() == Qt::NoModifier
            || event->modifiers() == Qt::KeypadModifier)) {
        onRemoveItem();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void OrderDialog::recalcTotals() {
    const QString sym = ConfigManager::instance().currencySymbol();
    double sub = 0.0;
    for (int r = 0; r < m_itemsTable->rowCount(); ++r) {
        auto* ps = qobject_cast<QDoubleSpinBox*>(m_itemsTable->cellWidget(r, IC_UNITPRICE));
        auto* qs = qobject_cast<QSpinBox*>(       m_itemsTable->cellWidget(r, IC_QTY));
        if (ps && qs) sub += ps->value() * qs->value();
    }
    double del      = m_deliveryFeeSpin->value();
    double discount = m_discountSpin ? m_discountSpin->value() : 0.0;
    double gt       = sub + del - discount;
    if (gt < 0) gt = 0;
    m_subtotalLabel->setText(  QString("%1 %2").arg(QString::number(sub,      'f', 2), sym));
    m_deliveryLabel->setText(  QString("%1 %2").arg(QString::number(del,      'f', 2), sym));
    m_grandTotalLabel->setText(QString("%1 %2").arg(QString::number(gt,       'f', 2), sym));
    // Show discount inline
    if (discount > 0.0)
        m_grandTotalLabel->setToolTip(QString("Discount: -%1 %2").arg(QString::number(discount,'f',2), sym));
}

void OrderDialog::setOrder(const Order& order) {
    m_orderId    = order.id();
    m_customerId = order.customerId();

    // Show customer name in search box
    m_customerSearch->setText(order.customerName());
    m_customerLabel->setText("✔  " + order.customerName());

    m_dateTimeEdit->setDateTime(order.dateTime().isValid()
        ? order.dateTime() : QDateTime::currentDateTime());
    m_deliveryFeeSpin->setValue(order.deliveryFee());

    int si = m_statusCombo->findData(order.status());
    if (si >= 0) m_statusCombo->setCurrentIndex(si);
    m_cancelReasonEdit->setText(order.cancelReason());
    onStatusChanged(order.status());

    // Driver
    if (m_driverCombo) {
        int di = m_driverCombo->findData(order.driverId());
        if (di >= 0) m_driverCombo->setCurrentIndex(di);
    }

    // Payment + Discount
    if (m_paymentCombo) {
        int pi = m_paymentCombo->findData(order.paymentMethod().isEmpty() ? "Cash" : order.paymentMethod());
        if (pi >= 0) m_paymentCombo->setCurrentIndex(pi);
    }
    if (m_paymentOtherEdit) {
        m_paymentOtherEdit->setText(order.paymentOtherDetail());
        bool isOther = (order.paymentMethod() == "Other");
        m_paymentOtherLbl->setVisible(isOther);
        m_paymentOtherEdit->setVisible(isOther);
    }
    if (m_discountSpin) m_discountSpin->setValue(order.discountAmount());
    if (m_discountReasonEdit) m_discountReasonEdit->setText(order.discountReason());

    m_itemsTable->setRowCount(0);
    for (const auto& item : order.items()) {
        int row = m_itemsTable->rowCount();
        m_itemsTable->insertRow(row);
        rebuildItemRow(row, item.productId, item.quantity, item.unitPrice);
    }
    recalcTotals();
}

Order OrderDialog::getOrder() const {
    Order o;
    o.setId(m_orderId);
    o.setCustomerId(m_customerId);

    // Find customer name from ID
    for (const auto& c : m_customers)
        if (c.id() == m_customerId) { o.setCustomerName(c.name()); break; }

    o.setDateTime(m_dateTimeEdit->dateTime());
    o.setDeliveryFee(m_deliveryFeeSpin->value());
    o.setStatus(m_statusCombo->currentData().toString());
    o.setCancelReason(m_statusCombo->currentData().toString() == "Cancelled"
        ? m_cancelReasonEdit->text().trimmed() : QString());

    // Driver
    if (m_driverCombo) {
        int did = m_driverCombo->currentData().toInt();
        o.setDriverId(did);
        if (did > 0) {
            int idx = m_driverCombo->currentIndex();
            QString txt = m_driverCombo->itemText(idx);
            int sep = txt.indexOf("  📞");
            o.setDriverName(sep >= 0 ? txt.left(sep) : txt);
        }
    }

    // Payment + Discount
    if (m_paymentCombo)
        o.setPaymentMethod(m_paymentCombo->currentData().toString());
    if (m_paymentOtherEdit && m_paymentCombo->currentData().toString() == "Other")
        o.setPaymentOtherDetail(m_paymentOtherEdit->text().trimmed());
    if (m_discountSpin)
        o.setDiscountAmount(m_discountSpin->value());
    if (m_discountReasonEdit)
        o.setDiscountReason(m_discountReasonEdit->text().trimmed());

    QList<OrderItem> items;
    for (int r = 0; r < m_itemsTable->rowCount(); ++r) {
        auto* se   = qobject_cast<QLineEdit*>(    m_itemsTable->cellWidget(r, IC_PRODUCT));
        auto* qs   = qobject_cast<QSpinBox*>(     m_itemsTable->cellWidget(r, IC_QTY));
        auto* ps   = qobject_cast<QDoubleSpinBox*>(m_itemsTable->cellWidget(r, IC_UNITPRICE));
        if (!se || !qs || !ps) continue;

        // Resolve product ID from the widget property (set during search)
        int pid = se->property("productId").toInt();
        if (pid == 0) {
            // Fallback: match by name
            QString name = se->text().trimmed();
            for (const auto& p : m_products)
                if (p.name().compare(name, Qt::CaseInsensitive) == 0) { pid = p.id(); break; }
        }
        if (pid == 0) continue;

        OrderItem item;
        item.productId   = pid;
        item.quantity    = qs->value();
        item.unitPrice   = ps->value();
        for (const auto& p : m_products)
            if (p.id() == pid) { item.productName = p.name(); break; }
        items.append(item);
    }
    o.setItems(items);
    return o;
}

void OrderDialog::validate() {
    if (m_customerId == 0) {
        QMessageBox::warning(this, "Validation", "Please select a customer.");
        m_customerSearch->setFocus();
        return;
    }
    if (m_itemsTable->rowCount() == 0) {
        QMessageBox::warning(this, "Validation", "Please add at least one item.");
        return;
    }
    if (m_statusCombo->currentData().toString() == "Cancelled" &&
        m_cancelReasonEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation",
            "Please enter a reason for cancellation.");
        m_cancelReasonEdit->setFocus();
        return;
    }
    if (m_paymentCombo->currentData().toString() == "Other" &&
        m_paymentOtherEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation",
            "Please specify the payment method when 'Other' is selected.");
        m_paymentOtherEdit->setFocus();
        return;
    }
    accept();
}

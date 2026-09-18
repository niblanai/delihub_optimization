#include "customer_dialog.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QLabel>
#include <QMessageBox>
#include <QListWidgetItem>
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

CustomerDialog::CustomerDialog(const QList<Region>& regions,
                               const QList<Product>& allProducts,
                               QWidget* parent)
    : QDialog(parent), m_regions(regions)
{
    setupUi(regions, allProducts);
}

void CustomerDialog::setupUi(const QList<Region>& regions, const QList<Product>& allProducts) {
    auto& L = LangManager::instance();
    setWindowTitle(L.t("Customer"));
    setMinimumWidth(520);
    setModal(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    auto* tabs = new QTabWidget;

    // ── Tab 1: Basic Info ─────────────────────────────────────────────────
    auto* infoWidget = new QWidget;
    auto* formLayout = new QFormLayout(infoWidget);
    formLayout->setSpacing(10);
    formLayout->setContentsMargins(12, 12, 12, 12);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(L.t("Full name..."));
    formLayout->addRow(L.t("Name *"), m_nameEdit);

    m_regionCombo = new QComboBox;
    m_regionCombo->addItem(L.t("— No Region —"), 0);
    for (const auto& r : regions)
        m_regionCombo->addItem(r.name, r.id);
    formLayout->addRow(L.t("Region"), m_regionCombo);

    connect(m_regionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CustomerDialog::onRegionChanged);

    m_distanceSpin = new QDoubleSpinBox;
    m_distanceSpin->setRange(0.0, 9999.9);
    m_distanceSpin->setDecimals(1);
    m_distanceSpin->setSuffix(" km");
    formLayout->addRow(L.t("Distance"), m_distanceSpin);

    m_statusCombo = new QComboBox;
    m_statusCombo->addItem(L.t("Active"),    static_cast<int>(Customer::Status::Active));
    m_statusCombo->addItem(L.t("Hesitant"),  static_cast<int>(Customer::Status::Hesitant));
    m_statusCombo->addItem(L.t("Inactive"),  static_cast<int>(Customer::Status::Inactive));
    m_statusCombo->addItem(L.t("Suspended"), static_cast<int>(Customer::Status::Suspended));
    formLayout->addRow(L.t("Status"), m_statusCombo);

    m_debtSpin = new QDoubleSpinBox;
    m_debtSpin->setRange(0.0, 999999.99);
    m_debtSpin->setDecimals(2);
    m_debtSpin->setPrefix(L.t("EGP "));
    m_debtSpin->setSpecialValueText(L.t("No debt"));
    formLayout->addRow(L.t("Debt"), m_debtSpin);

    m_notesEdit = new QTextEdit;
    m_notesEdit->setMaximumHeight(80);
    m_notesEdit->setPlaceholderText(L.t("Optional notes..."));
    formLayout->addRow(L.t("Notes"), m_notesEdit);

    m_paymentPrefCombo = new QComboBox;
    m_paymentPrefCombo->addItem(L.t("— Not specified —"), "");
    m_paymentPrefCombo->addItem("💵 Cash", "Cash");
    m_paymentPrefCombo->addItem("💳 Visa", "Visa");
    m_paymentPrefCombo->addItem(L.t("🔖 Other (specify)"), "Other");
    formLayout->addRow(L.t("Preferred Payment"), m_paymentPrefCombo);

    m_paymentOtherLbl  = new QLabel(L.t("Please specify:"));
    m_paymentOtherEdit = new QLineEdit;
    m_paymentOtherEdit->setPlaceholderText(L.t("e.g. Bank transfer, Instapay..."));
    formLayout->addRow(m_paymentOtherLbl, m_paymentOtherEdit);
    m_paymentOtherLbl->setVisible(false);
    m_paymentOtherEdit->setVisible(false);

    connect(m_paymentPrefCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int) {
            bool isOther = (m_paymentPrefCombo->currentData().toString() == "Other");
            m_paymentOtherLbl->setVisible(isOther);
            m_paymentOtherEdit->setVisible(isOther);
        });

    tabs->addTab(infoWidget, L.t("ℹ️  Info"));

    // ── Tab 2: Phones ─────────────────────────────────────────────────────
    auto* phoneWidget = new QWidget;
    auto* phoneLayout = new QVBoxLayout(phoneWidget);
    phoneLayout->setSpacing(8);
    phoneLayout->setContentsMargins(12, 12, 12, 12);

    m_phoneList = new QListWidget;
    m_phoneList->setSelectionMode(QAbstractItemView::SingleSelection);
    phoneLayout->addWidget(m_phoneList);

    auto* phoneInputRow = new QHBoxLayout;
    m_phoneInput = new QLineEdit;
    m_phoneInput->setPlaceholderText(L.t("Phone number..."));
    m_addPhoneBtn = new QPushButton(L.t("Add"));
    m_addPhoneBtn->setObjectName("addBtn");
    m_removePhoneBtn = new QPushButton(L.t("Remove"));
    m_removePhoneBtn->setObjectName("removeBtn");
    phoneInputRow->addWidget(m_phoneInput, 1);
    phoneInputRow->addWidget(m_addPhoneBtn);
    phoneInputRow->addWidget(m_removePhoneBtn);
    phoneLayout->addLayout(phoneInputRow);

    connect(m_addPhoneBtn, &QPushButton::clicked, this, &CustomerDialog::onAddPhone);
    connect(m_removePhoneBtn, &QPushButton::clicked, this, &CustomerDialog::onRemovePhone);
    connect(m_phoneInput, &QLineEdit::returnPressed, this, &CustomerDialog::onAddPhone);

    tabs->addTab(phoneWidget, L.t("📞  Phones"));

    // ── Tab 3: Addresses ─────────────────────────────────────────────────
    auto* addrWidget = new QWidget;
    auto* addrLayout = new QVBoxLayout(addrWidget);
    addrLayout->setSpacing(8);
    addrLayout->setContentsMargins(12, 12, 12, 12);

    m_addrList = new QListWidget;
    m_addrList->setSelectionMode(QAbstractItemView::SingleSelection);
    addrLayout->addWidget(m_addrList);

    auto* addrInputRow = new QHBoxLayout;
    m_addrInput = new QLineEdit;
    m_addrInput->setPlaceholderText(L.t("Address..."));
    m_addAddrBtn = new QPushButton(L.t("Add"));
    m_addAddrBtn->setObjectName("addBtn");
    m_removeAddrBtn = new QPushButton(L.t("Remove"));
    m_removeAddrBtn->setObjectName("removeBtn");
    addrInputRow->addWidget(m_addrInput, 1);
    addrInputRow->addWidget(m_addAddrBtn);
    addrInputRow->addWidget(m_removeAddrBtn);
    addrLayout->addLayout(addrInputRow);

    connect(m_addAddrBtn, &QPushButton::clicked, this, &CustomerDialog::onAddAddress);
    connect(m_removeAddrBtn, &QPushButton::clicked, this, &CustomerDialog::onRemoveAddress);
    connect(m_addrInput, &QLineEdit::returnPressed, this, &CustomerDialog::onAddAddress);

    tabs->addTab(addrWidget, L.t("📍  Addresses"));

    // ── Tab 4: Favorite Products ─────────────────────────────────────────
    auto* favWidget = new QWidget;
    auto* favLayout = new QVBoxLayout(favWidget);
    favLayout->setSpacing(8);
    favLayout->setContentsMargins(12, 12, 12, 12);

    auto* favSearchEdit = new QLineEdit;
    favSearchEdit->setPlaceholderText(L.t("Search by name or barcode..."));
    favSearchEdit->setObjectName("searchEdit");
    
    // Add search icon
    QAction* favSearchAction = new QAction(favSearchEdit);
    favSearchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                                  QColor("#9CA3AF"), 16));
    favSearchEdit->addAction(favSearchAction, QLineEdit::LeadingPosition);
    
    favLayout->addWidget(favSearchEdit);

    m_favList = new QListWidget;
    m_favList->setStyleSheet("QListWidget::item { padding: 6px 8px; font-size:13px; }");
    for (const auto& p : allProducts) {
        QString label = p.barcode().isEmpty()
            ? p.name()
            : QString("%1   [%2]").arg(p.name(), p.barcode());
        auto* item = new QListWidgetItem(label);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, p.id());
        item->setData(Qt::UserRole + 1, p.name());
        item->setData(Qt::UserRole + 2, p.barcode());
        m_favList->addItem(item);
    }
    favLayout->addWidget(m_favList);

    QObject::connect(favSearchEdit, &QLineEdit::textChanged, m_favList,
        [this](const QString& text) {
            QString lower = text.trimmed().toLower();
            for (int i = 0; i < m_favList->count(); ++i) {
                auto* it = m_favList->item(i);
                bool match = lower.isEmpty()
                    || it->data(Qt::UserRole + 1).toString().toLower().contains(lower)
                    || it->data(Qt::UserRole + 2).toString().toLower().contains(lower);
                it->setHidden(!match);
            }
        });

    tabs->addTab(favWidget, L.t("⭐  Favorites"));

    mainLayout->addWidget(tabs);

    // ── Buttons ────────────────────────────────────────────────────────────
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Save)->setObjectName("saveBtn");
    m_buttons->button(QDialogButtonBox::Save)->setText(L.t("Save"));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(L.t("Cancel"));
    mainLayout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &CustomerDialog::validate);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ──────────────────────────────────────────────────────────────────────────────

void CustomerDialog::setCustomer(const Customer& c) {
    m_customerId = c.id();
    m_nameEdit->setText(c.name());
    m_distanceSpin->setValue(c.distanceKm());
    m_notesEdit->setPlainText(c.notes());

    // Region
    int regionIdx = m_regionCombo->findData(c.regionId());
    if (regionIdx >= 0) m_regionCombo->setCurrentIndex(regionIdx);

    // Status
    int statusIdx = m_statusCombo->findData(static_cast<int>(c.status()));
    if (statusIdx >= 0) m_statusCombo->setCurrentIndex(statusIdx);

    // Debt
    m_debtSpin->setValue(c.debt());

    // Preferred payment
    int pi = m_paymentPrefCombo->findData(c.preferredPaymentMethod());
    m_paymentPrefCombo->setCurrentIndex(pi >= 0 ? pi : 0);
    m_paymentOtherEdit->setText(c.preferredPaymentOther());
    bool isOther = (c.preferredPaymentMethod() == "Other");
    m_paymentOtherLbl->setVisible(isOther);
    m_paymentOtherEdit->setVisible(isOther);

    // Phones
    m_phoneList->clear();
    for (const auto& ph : c.phones())
        m_phoneList->addItem(ph.number);

    // Addresses
    m_addrList->clear();
    for (const auto& a : c.addresses())
        m_addrList->addItem(a.text);

    // Favorites
    const QList<int>& favIds = c.favoriteProductIds();
    for (int i = 0; i < m_favList->count(); ++i) {
        QListWidgetItem* item = m_favList->item(i);
        bool isFav = favIds.contains(item->data(Qt::UserRole).toInt());
        item->setCheckState(isFav ? Qt::Checked : Qt::Unchecked);
    }
}

Customer CustomerDialog::getCustomer() const {
    Customer c;
    c.setId(m_customerId);
    c.setName(m_nameEdit->text().trimmed());
    c.setDistanceKm(m_distanceSpin->value());
    c.setNotes(m_notesEdit->toPlainText().trimmed());
    c.setRegionId(m_regionCombo->currentData().toInt());
    c.setStatus(static_cast<Customer::Status>(m_statusCombo->currentData().toInt()));
    c.setDebt(m_debtSpin->value());
    c.setFirstContactDate(QDateTime::currentDateTime());

    // Preferred payment
    QString pm = m_paymentPrefCombo->currentData().toString();
    c.setPreferredPaymentMethod(pm);
    c.setPreferredPaymentOther(pm == "Other" ? m_paymentOtherEdit->text().trimmed() : QString());

    // Phones
    QList<Phone> phones;
    for (int i = 0; i < m_phoneList->count(); ++i)
        phones.append(Phone{m_phoneList->item(i)->text()});
    c.setPhones(phones);

    // Addresses
    QList<Address> addresses;
    for (int i = 0; i < m_addrList->count(); ++i)
        addresses.append(Address{m_addrList->item(i)->text()});
    c.setAddresses(addresses);

    // Favorites
    QList<int> favIds;
    for (int i = 0; i < m_favList->count(); ++i) {
        QListWidgetItem* item = m_favList->item(i);
        if (item->checkState() == Qt::Checked)
            favIds.append(item->data(Qt::UserRole).toInt());
    }
    c.setFavoriteProductIds(favIds);

    return c;
}

void CustomerDialog::onAddPhone() {
    QString num = m_phoneInput->text().trimmed();
    if (!num.isEmpty()) {
        m_phoneList->addItem(num);
        m_phoneInput->clear();
    }
}

void CustomerDialog::onRemovePhone() {
    qDeleteAll(m_phoneList->selectedItems());
}

void CustomerDialog::onAddAddress() {
    QString addr = m_addrInput->text().trimmed();
    if (!addr.isEmpty()) {
        m_addrList->addItem(addr);
        m_addrInput->clear();
    }
}

void CustomerDialog::onRemoveAddress() {
    qDeleteAll(m_addrList->selectedItems());
}

void CustomerDialog::onToggleFavorite(QListWidgetItem* /*item*/) {
    // toggled via checkbox — no extra action needed
}

// Task 1: auto-fill distance from the selected region
void CustomerDialog::onRegionChanged(int /*comboIndex*/) {
    int regionId = m_regionCombo->currentData().toInt();
    if (regionId <= 0) return;
    for (const auto& r : m_regions) {
        if (r.id == regionId) {
            // Only auto-fill if distance is currently 0 (don't clobber user-entered value)
            if (m_distanceSpin->value() < 0.01)
                m_distanceSpin->setValue(r.distanceKm);
            break;
        }
    }
}

void CustomerDialog::validate() {
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation", "Customer name is required.");
        m_nameEdit->setFocus();
        return;
    }

    // ── Duplicate detection (only when adding a new customer, id == 0) ────────
    if (m_customerId == 0 && !m_existingCustomers.isEmpty()) {
        QString newName = m_nameEdit->text().trimmed().toLower();

        // Collect phones being added (normalised: no spaces, lowercase)
        QStringList newPhones;
        for (int i = 0; i < m_phoneList->count(); ++i) {
            QString ph = m_phoneList->item(i)->text().trimmed().remove(' ').toLower();
            if (!ph.isEmpty()) newPhones.append(ph);
        }

        // Collect addresses being added (trimmed, lowercase)
        QStringList newAddrs;
        for (int i = 0; i < m_addrList->count(); ++i) {
            QString addr = m_addrList->item(i)->text().trimmed().toLower();
            if (!addr.isEmpty()) newAddrs.append(addr);
        }

        struct Conflict {
            QString existingName;
            int     existingId;
            QString reason;
        };
        QList<Conflict> conflicts;

        for (const auto& c : m_existingCustomers) {
            QString existName = c.name().trimmed().toLower();

            // 1. Name match (exact after trim+lower)
            if (!newName.isEmpty() && existName == newName) {
                conflicts.append({ c.name(), c.id(), "Same name" });
                continue; // no need to check phone/address for same customer
            }

            // 2. Phone match
            bool phoneMatched = false;
            for (const auto& ph : c.phones()) {
                QString norm = ph.number.trimmed().remove(' ').toLower();
                if (!norm.isEmpty() && newPhones.contains(norm)) {
                    conflicts.append({ c.name(), c.id(),
                        QString("Phone number \"%1\" matches").arg(ph.number.trimmed()) });
                    phoneMatched = true;
                    break;
                }
            }
            if (phoneMatched) continue;

            // 3. Address match
            for (const auto& addr : c.addresses()) {
                QString normAddr = addr.text.trimmed().toLower();
                if (!normAddr.isEmpty() && newAddrs.contains(normAddr)) {
                    conflicts.append({ c.name(), c.id(),
                        QString("Address \"%1\" matches").arg(addr.text.trimmed()) });
                    break;
                }
            }
        }

        if (!conflicts.isEmpty()) {
            QDialog dlg(this);
            dlg.setWindowTitle("Duplicate Customer Detected");
            dlg.setMinimumWidth(500);
            dlg.setModal(true);

            auto* lay = new QVBoxLayout(&dlg);
            lay->setSpacing(12);
            lay->setContentsMargins(20, 16, 20, 16);

            auto* hdr = new QLabel(
                "<b style='color:#F87171;'>⚠ Similar customer(s) already exist</b>");
            lay->addWidget(hdr);

            auto* grid = new QGridLayout;
            grid->setSpacing(6);
            grid->setColumnStretch(1, 1);
            grid->setColumnStretch(2, 2);

            auto addHdr = [&](int col, const QString& t) {
                auto* l = new QLabel(t);
                l->setStyleSheet("font-weight:bold; color:#94A3B8;");
                grid->addWidget(l, 0, col);
            };
            addHdr(0, "#");
            addHdr(1, "Existing Customer");
            addHdr(2, "Conflict Reason");

            for (int i = 0; i < conflicts.size(); ++i) {
                auto* num  = new QLabel(QString::number(i + 1));
                auto* name = new QLabel(conflicts[i].existingName);
                auto* rsn  = new QLabel(conflicts[i].reason);
                num->setStyleSheet("color:#E2E8F0;");
                name->setStyleSheet("color:#E2E8F0;");
                rsn->setStyleSheet("color:#F87171;");
                grid->addWidget(num,  i + 1, 0);
                grid->addWidget(name, i + 1, 1);
                grid->addWidget(rsn,  i + 1, 2);
            }
            lay->addLayout(grid);

            auto* sep = new QFrame;
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("color:#334155;");
            lay->addWidget(sep);

            auto* note = new QLabel(
                "<span style='color:#94A3B8;font-size:11px;'>"
                "Close to go back and review, or create the customer anyway.</span>");
            note->setWordWrap(true);
            lay->addWidget(note);

            auto* btnRow = new QHBoxLayout;
            auto* closeBtn  = new QPushButton("✕  Close");
            auto* createBtn = new QPushButton("✓  Create Anyway");
            closeBtn->setObjectName("secondaryBtn");
            createBtn->setObjectName("primaryBtn");
            btnRow->addWidget(closeBtn);
            btnRow->addStretch();
            btnRow->addWidget(createBtn);
            lay->addLayout(btnRow);

            bool createAnyway = false;
            QObject::connect(closeBtn,  &QPushButton::clicked, &dlg, [&](){ dlg.reject(); });
            QObject::connect(createBtn, &QPushButton::clicked, &dlg, [&](){ createAnyway = true; dlg.accept(); });

            dlg.exec();
            if (!createAnyway) return; // stay open — don't accept CustomerDialog
        }
    }

    accept();
}

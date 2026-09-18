#include "notes_page.h"
#include "services/lang_manager.h"
#include "services/theme_manager.h"
#include "ui/svg_icon_helper.h"
#include "infra/database_connection_manager.h"
#include "infra/config_manager.h"
#include "infra/logger.h"
#include "data/sqlite_repositories.h"
#include "data/access_repositories.h"
#include "data/access_repositories.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextListFormat>
#include <QComboBox>
#include <QListWidget>
#include <QMessageBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QDateTime>
#include <QSizePolicy>
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
#include <QGraphicsDropShadowEffect>
#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <QImageReader>
#include <QApplication>
#include <QFont>
#include <QDir>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>
#include <QFile>
#include <QPixmap>
#include <QTimer>

// ═══════════════════════════════════════════════════════════════════════════
// NoteCardWidget
// ═══════════════════════════════════════════════════════════════════════════
NoteCardWidget::NoteCardWidget(const Note& note, QWidget* parent)
    : QWidget(parent), m_noteId(note.id), m_pinned(note.pinned), m_note(note)
{
    const NoteColor nc = noteColorForKey(note.color);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString(
        "NoteCardWidget { background-color: %1;"
        "  border: 2px solid rgba(0,0,0,0.18); border-radius: 6px; }"
    ).arg(nc.bg));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumWidth(160);
    setToolTip("Double-click to open full view");

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(10);
    shadow->setOffset(2, 3);
    shadow->setColor(QColor(0, 0, 0, 70));
    setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 10, 10, 8);

    // ── Pin label ─────────────────────────────────────────────────────────────
    if (note.pinned) {
        auto* pinLbl = new QLabel("📌");
        pinLbl->setStyleSheet("background:transparent;border:none;font-size:11pt;");
        pinLbl->setAlignment(Qt::AlignRight);
        layout->addWidget(pinLbl);
    }

    // ── Title ─────────────────────────────────────────────────────────────────
    if (!note.title.isEmpty()) {
        auto* titleLbl = new QLabel(note.title);
        titleLbl->setStyleSheet(QString(
            "font-weight:bold;font-size:10pt;color:%1;"
            "background:transparent;border:none;").arg(nc.text));
        titleLbl->setWordWrap(true);
        titleLbl->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        layout->addWidget(titleLbl);
    }

    // ── Rich-text preview — show first ~3 lines with formatting ──────────────
    // #3: Use QTextBrowser so HTML formatting is rendered, not stripped
    if (!note.content.isEmpty()) {
        auto* preview = new QTextBrowser;
        preview->setHtml(note.content);
        preview->setReadOnly(true);
        preview->setMaximumHeight(72);
        preview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        preview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        preview->setFrameShape(QFrame::NoFrame);
        // Match card background
        preview->setStyleSheet(QString(
            "QTextBrowser { background: %1; color: %2;"
            " border: none; font-size: 9pt; padding: 0px; }"
        ).arg(nc.bg, nc.text));
        // Disable mouse interaction so double-click reaches the card
        preview->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        layout->addWidget(preview, 1);
    }
    layout->addStretch();

    // ── Attachment indicator ─────────────────────────────────────────────────
    // #4: show thumbnail for images, 📄 icon for files
    QStringList paths = note.attachmentList();
    if (!paths.isEmpty()) {
        auto* attRow = new QHBoxLayout;
        attRow->setSpacing(4);
        int shown = 0;
        for (const QString& p : paths) {
            if (shown >= 3) break;
            QFileInfo fi(p);
            QImageReader reader(p);
            if (reader.canRead()) {
                // Image thumbnail
                auto* thumb = new QLabel;
                QPixmap pm(p);
                if (!pm.isNull())
                    thumb->setPixmap(pm.scaled(36, 36,
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
                thumb->setStyleSheet("background:transparent;border:1px solid rgba(0,0,0,0.2);border-radius:3px;");
                thumb->setToolTip(fi.fileName());
                thumb->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                attRow->addWidget(thumb);
            } else {
                // File icon
                auto* iconLbl = new QLabel("📄");
                iconLbl->setStyleSheet(QString("color:%1;font-size:18pt;"
                    "background:transparent;border:none;").arg(nc.text));
                iconLbl->setToolTip(fi.fileName());
                iconLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                attRow->addWidget(iconLbl);
            }
            ++shown;
        }
        if (paths.size() > 3) {
            auto* moreLbl = new QLabel(QString("+%1").arg(paths.size() - 3));
            moreLbl->setStyleSheet(QString("color:%1;font-size:8pt;"
                "background:transparent;border:none;").arg(nc.text));
            moreLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            attRow->addWidget(moreLbl);
        }
        attRow->addStretch();
        layout->addLayout(attRow);
    }

    // ── Separator ─────────────────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background:rgba(0,0,0,0.15);border:none;");
    layout->addWidget(sep);

    // ── Footer ────────────────────────────────────────────────────────────────
    auto* footer = new QHBoxLayout;
    footer->setSpacing(3);
    QString ts = note.updatedAt.isValid()
        ? note.updatedAt.toString("MMM d · hh:mm")
        : note.createdAt.toString("MMM d · hh:mm");
    auto* dateLbl = new QLabel(ts);
    dateLbl->setStyleSheet(QString(
        "color:%1;font-size:8pt;background:transparent;border:none;").arg(nc.text));
    footer->addWidget(dateLbl, 1);

    auto* pinBtn  = new QPushButton(note.pinned ? "📍" : "📌");
    auto* editBtn = new QPushButton("✏️");
    auto* delBtn  = new QPushButton("🗑");
    for (auto* b : {pinBtn, editBtn, delBtn}) {
        b->setFixedSize(24, 24);
        b->setObjectName("iconBtn");
        footer->addWidget(b);
    }
    layout->addLayout(footer);

    connect(pinBtn,  &QPushButton::clicked, this, [this]{ emit pinToggled(m_noteId, !m_pinned); });
    connect(editBtn, &QPushButton::clicked, this, [this]{ emit editRequested(m_noteId); });
    connect(delBtn,  &QPushButton::clicked, this, [this]{ emit deleteRequested(m_noteId); });
}

void NoteCardWidget::mouseDoubleClickEvent(QMouseEvent*) {
    emit viewRequested(m_noteId);
}

// ═══════════════════════════════════════════════════════════════════════════
// NoteViewDialog — full read-only view (double-click on card)
// ═══════════════════════════════════════════════════════════════════════════
NoteViewDialog::NoteViewDialog(const Note& note, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(note.title.isEmpty() ? "Note" : note.title);
    setMinimumSize(640, 480);

    const NoteColor nc = noteColorForKey(note.color);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // Header bar
    auto* header = new QFrame;
    header->setStyleSheet(QString("background:%1;border-bottom:2px solid rgba(0,0,0,0.2);").arg(nc.bg));
    auto* hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(16, 12, 16, 12);

    auto* titleLbl = new QLabel(note.title.isEmpty() ? "(Untitled)" : note.title);
    titleLbl->setStyleSheet(QString(
        "font-size:14pt;font-weight:bold;color:%1;background:transparent;border:none;").arg(nc.text));
    hLayout->addWidget(titleLbl, 1);

    auto* editBtn = new QPushButton("✏️  Edit");
    editBtn->setObjectName("primaryBtn");
    hLayout->addWidget(editBtn);
    root->addWidget(header);

    // Content
    auto* browser = new QTextBrowser;
    browser->setHtml(note.content);
    browser->setReadOnly(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setStyleSheet(QString(
        "QTextBrowser { background:%1; color:%2; font-size:11pt; padding:16px; border:none; }")
        .arg(nc.bg, nc.text));
    root->addWidget(browser, 1);

    // Attachments section
    QStringList paths = note.attachmentList();
    if (!paths.isEmpty()) {
        auto* attFrame = new QFrame;
        attFrame->setStyleSheet(QString(
            "background:%1;border-top:1px solid rgba(0,0,0,0.15);").arg(nc.bg));
        auto* aLayout = new QHBoxLayout(attFrame);
        aLayout->setContentsMargins(12, 8, 12, 8);
        aLayout->setSpacing(8);

        auto* attTitle = new QLabel("📎 Attachments:");
        attTitle->setStyleSheet(QString(
            "font-weight:bold;color:%1;background:transparent;border:none;").arg(nc.text));
        aLayout->addWidget(attTitle);

        for (const QString& p : paths) {
            QFileInfo fi(p);
            QImageReader reader(p);
            auto* chip = new QPushButton(
                reader.canRead() ? ("🖼 " + fi.fileName()) : ("📄 " + fi.fileName()));
            chip->setObjectName("secondaryBtn");
            chip->setToolTip("Double-click to open: " + p);
            // Open on double-click
            connect(chip, &QPushButton::clicked, this, [p]{
                if (QFile::exists(p))
                    QDesktopServices::openUrl(QUrl::fromLocalFile(p));
                else
                    QMessageBox::warning(nullptr, "File Not Found",
                        "Cannot find: " + p);
            });
            aLayout->addWidget(chip);
        }
        aLayout->addStretch();
        root->addWidget(attFrame);
    }

    // Close button
    auto* btnBar = new QFrame;
    btnBar->setObjectName("noteEditorButtons");
    auto* bLayout = new QHBoxLayout(btnBar);
    bLayout->addStretch();
    auto* closeBtn = new QPushButton("Close");
    closeBtn->setObjectName("secondaryBtn");
    bLayout->addWidget(closeBtn);
    root->addWidget(btnBar);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(editBtn,  &QPushButton::clicked, this, [this, &note]{
        emit editRequested(note.id);
        accept();
    });
}

// ═══════════════════════════════════════════════════════════════════════════
// NoteEditorDialog
// ═══════════════════════════════════════════════════════════════════════════
// #6: lazy-init customers+products — only built when dialog first opens,
//     not at app startup (speeds up New Note)
NoteEditorDialog::NoteEditorDialog(const QList<Customer>& customers,
                                    const QList<Product>&  products,
                                    QWidget* parent)
    : QDialog(parent), m_customers(customers), m_products(products)
{
    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeType){ recolorIcons(); });
}

QString NoteEditorDialog::attachmentsDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                   + "/DeliHub/attachments";
    QDir().mkpath(base);
    return base;
}

void NoteEditorDialog::setupUi() {
    setWindowTitle("Note Editor");
    setMinimumSize(780, 580);
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // ── FORMAT TOOLBAR ────────────────────────────────────────────────────────
    auto* toolbar = new QFrame;
    toolbar->setObjectName("noteEditorToolbar");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 6, 8, 6);
    tbLayout->setSpacing(4);

    auto mkBtn = [](const QString& tip) -> QPushButton* {
        auto* b = new QPushButton;
        b->setToolTip(tip);
        b->setFixedSize(34, 30);
        b->setCursor(Qt::PointingHandCursor);
        b->setObjectName("iconBtn");
        b->setStyleSheet(
            "QPushButton#iconBtn{"
            "  background:#334155;color:#E2E8F0;"
            "  border:1px solid #475569;border-radius:5px;"
            "  font-size:13pt;font-weight:bold;min-height:0;"
            "}"
            "QPushButton#iconBtn:hover{background:#475569;}"
            "QPushButton#iconBtn:pressed{background:#0EA5E9;color:#fff;}");
        return b;
    };

    m_boldBtn      = mkBtn("Bold");
    m_italicBtn    = mkBtn("Italic");
    m_underlineBtn = mkBtn("Underline");
    m_strikeBtn    = mkBtn("Strikethrough");
    m_colorBtn     = mkBtn("Text color");
    m_hlBtn        = mkBtn("Highlight");
    m_quoteBtn     = mkBtn("Block quote");
    m_bulletBtn    = mkBtn("Bullet list");
    m_attachBtn    = mkBtn("Attach file or image");

    auto mkSep = [&]() -> QWidget* {
        auto* s = new QWidget; s->setFixedSize(1, 22);
        s->setObjectName("toolbarSep"); return s;
    };

    m_fontSizeCombo = new QComboBox;
    m_fontSizeCombo->setFixedSize(64, 30);
    for (int s : {8,9,10,11,12,14,16,18,20,24,28,32,36,48})
        m_fontSizeCombo->addItem(QString::number(s));
    m_fontSizeCombo->setCurrentText("11");

    tbLayout->addWidget(m_boldBtn);
    tbLayout->addWidget(m_italicBtn);
    tbLayout->addWidget(m_underlineBtn);
    tbLayout->addWidget(m_strikeBtn);
    tbLayout->addWidget(mkSep());
    tbLayout->addWidget(m_colorBtn);
    tbLayout->addWidget(m_hlBtn);
    tbLayout->addWidget(mkSep());
    tbLayout->addWidget(m_quoteBtn);
    tbLayout->addWidget(m_bulletBtn);
    tbLayout->addWidget(mkSep());
    tbLayout->addWidget(m_attachBtn);
    tbLayout->addWidget(mkSep());
    auto* sizeLbl = new QLabel("Size:");
    sizeLbl->setObjectName("hintLabel");
    tbLayout->addWidget(sizeLbl);
    tbLayout->addWidget(m_fontSizeCombo);
    tbLayout->addStretch();
    root->addWidget(toolbar);

    recolorIcons();

    // ── ATTACHMENT ROW ────────────────────────────────────────────────────────
    m_attachRow = new QWidget;
    m_attachRow->setVisible(false);
    auto* arLayout = new QHBoxLayout(m_attachRow);
    arLayout->setContentsMargins(8, 4, 8, 4);
    arLayout->setSpacing(6);
    root->addWidget(m_attachRow);

    // ── META ROW ──────────────────────────────────────────────────────────────
    auto* metaBar = new QFrame;
    metaBar->setObjectName("noteEditorMeta");
    auto* metaLayout = new QHBoxLayout(metaBar);
    metaLayout->setContentsMargins(12, 8, 12, 8);
    metaLayout->setSpacing(10);

    m_titleEdit = new QLineEdit;
    m_titleEdit->setPlaceholderText("Note title…");
    m_titleEdit->setObjectName("searchEdit");
    metaLayout->addWidget(m_titleEdit, 1);

    auto* colorLbl = new QLabel("Color:");
    colorLbl->setObjectName("hintLabel");
    metaLayout->addWidget(colorLbl);

    m_colorCombo = new QComboBox;
    m_colorCombo->setMinimumWidth(130);
    for (const auto& c : noteColorPalette())
        m_colorCombo->addItem(c.label, c.key);
    metaLayout->addWidget(m_colorCombo);

    m_pinBtn = new QPushButton("📌 Pin");
    m_pinBtn->setObjectName("secondaryBtn");
    m_pinBtn->setCheckable(true);
    metaLayout->addWidget(m_pinBtn);
    root->addWidget(metaBar);

    // ── MAIN AREA ─────────────────────────────────────────────────────────────
    auto* mainArea = new QHBoxLayout;
    mainArea->setSpacing(0);
    mainArea->setContentsMargins(0, 0, 0, 0);

    m_editor = new QTextEdit;
    m_editor->setAcceptRichText(true);
    m_editor->setPlaceholderText("Write your note here…");
    m_editor->setObjectName("noteEditorBody");
    mainArea->addWidget(m_editor, 1);

    // ── Mention panel ─────────────────────────────────────────────────────────
    auto* mentionPanel = new QFrame;
    mentionPanel->setObjectName("mentionPanel");
    mentionPanel->setFixedWidth(220);
    auto* mpLayout = new QVBoxLayout(mentionPanel);
    mpLayout->setSpacing(6);
    mpLayout->setContentsMargins(10, 12, 10, 10);

    auto* custHdr = new QLabel("👥  Mention Customer");
    custHdr->setObjectName("mentionHeader");
    mpLayout->addWidget(custHdr);

    auto* custSearch = new QLineEdit;
    custSearch->setPlaceholderText("Name or phone…");
    custSearch->setObjectName("searchEdit");
    custSearch->setFixedHeight(30);
    mpLayout->addWidget(custSearch);

    m_custMentionList = new QListWidget;
    m_custMentionList->setMaximumHeight(120);
    m_custMentionList->setObjectName("dataTable");
    for (const auto& c : m_customers) {
        QString phone = c.phones().isEmpty() ? "" : c.phones().first().number;
        QString disp  = phone.isEmpty() ? "👤  " + c.name()
                                        : "👤  " + c.name() + "  📞 " + phone;
        auto* it = new QListWidgetItem(disp);
        it->setData(Qt::UserRole,     c.id());
        it->setData(Qt::UserRole + 1, phone);
        m_custMentionList->addItem(it);
    }
    mpLayout->addWidget(m_custMentionList);

    auto* custMentionBtn = new QPushButton("@ Insert Mention");
    custMentionBtn->setObjectName("primaryBtn");
    custMentionBtn->setFixedHeight(28);
    mpLayout->addWidget(custMentionBtn);

    auto* div = new QFrame;
    div->setFrameShape(QFrame::HLine);
    div->setObjectName("sidebarSep");
    mpLayout->addWidget(div);

    auto* prodHdr = new QLabel("📦  Mention Product");
    prodHdr->setObjectName("mentionHeader");
    mpLayout->addWidget(prodHdr);

    auto* prodSearch = new QLineEdit;
    prodSearch->setPlaceholderText("Name or barcode…");
    prodSearch->setObjectName("searchEdit");
    prodSearch->setFixedHeight(30);
    mpLayout->addWidget(prodSearch);

    m_prodMentionList = new QListWidget;
    m_prodMentionList->setMaximumHeight(120);
    m_prodMentionList->setObjectName("dataTable");
    for (const auto& p : m_products) {
        QString disp = p.barcode().isEmpty() ? "📦  " + p.name()
                                             : "📦  " + p.name() + "  [" + p.barcode() + "]";
        auto* it = new QListWidgetItem(disp);
        it->setData(Qt::UserRole,     p.id());
        it->setData(Qt::UserRole + 1, p.barcode());
        m_prodMentionList->addItem(it);
    }
    mpLayout->addWidget(m_prodMentionList);

    auto* prodMentionBtn = new QPushButton("@ Insert Mention");
    prodMentionBtn->setObjectName("primaryBtn");
    prodMentionBtn->setFixedHeight(28);
    mpLayout->addWidget(prodMentionBtn);
    mpLayout->addStretch();
    mainArea->addWidget(mentionPanel);
    root->addLayout(mainArea, 1);

    // ── BUTTONS ───────────────────────────────────────────────────────────────
    auto* btnBar = new QFrame;
    btnBar->setObjectName("noteEditorButtons");
    auto* bLayout = new QHBoxLayout(btnBar);
    bLayout->setContentsMargins(14, 10, 14, 10);
    bLayout->addStretch();
    auto* cancelBtn = new QPushButton("Cancel");
    cancelBtn->setObjectName("secondaryBtn");
    auto* saveBtn   = new QPushButton("💾  Save Note");
    saveBtn->setObjectName("primaryBtn");
    bLayout->addWidget(cancelBtn);
    bLayout->addWidget(saveBtn);
    root->addWidget(btnBar);

    // ── CONNECTIONS ───────────────────────────────────────────────────────────
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn,   &QPushButton::clicked, this, [this]{
        if (m_titleEdit->text().trimmed().isEmpty()
            && m_editor->toPlainText().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Empty Note",
                "Please add a title or some content.");
            return;
        }
        accept();
    });

    // Format buttons
    connect(m_boldBtn,      &QPushButton::clicked, this, [this]{
        QTextCharFormat f; f.setFontWeight(m_editor->fontWeight()==QFont::Bold ? QFont::Normal:QFont::Bold);
        m_editor->mergeCurrentCharFormat(f); m_editor->setFocus();});
    connect(m_italicBtn,    &QPushButton::clicked, this, [this]{
        m_editor->setFontItalic(!m_editor->fontItalic()); m_editor->setFocus();});
    connect(m_underlineBtn, &QPushButton::clicked, this, [this]{
        m_editor->setFontUnderline(!m_editor->fontUnderline()); m_editor->setFocus();});
    connect(m_strikeBtn,    &QPushButton::clicked, this, [this]{
        QTextCharFormat f; f.setFontStrikeOut(!m_editor->currentCharFormat().fontStrikeOut());
        m_editor->mergeCurrentCharFormat(f); m_editor->setFocus();});
    connect(m_colorBtn, &QPushButton::clicked, this, [this]{
        QColor col = QColorDialog::getColor(m_editor->textColor(), this, "Text Color");
        if (col.isValid()) { QTextCharFormat f; f.setForeground(col); m_editor->mergeCurrentCharFormat(f); }
        m_editor->setFocus();});
    connect(m_hlBtn, &QPushButton::clicked, this, [this]{
        static const QList<QColor> hl={QColor("#FEF08A"),QColor("#BBF7D0"),QColor("#BAE6FD"),QColor("#DDD6FE"),Qt::transparent};
        static int i=0; QTextCharFormat f; f.setBackground(hl[i%hl.size()]); m_editor->mergeCurrentCharFormat(f); ++i; m_editor->setFocus();});
    connect(m_quoteBtn, &QPushButton::clicked, this, [this]{
        QTextCursor cur=m_editor->textCursor(); QTextBlockFormat bf=cur.blockFormat();
        bool on=bf.leftMargin()>0; bf.setLeftMargin(on?0:24); cur.setBlockFormat(bf);
        QTextCharFormat cf; cf.setFontItalic(!on); m_editor->mergeCurrentCharFormat(cf); m_editor->setFocus();});
    connect(m_bulletBtn, &QPushButton::clicked, this, [this]{
        QTextCursor cur=m_editor->textCursor(); QTextListFormat lf;
        lf.setStyle(QTextListFormat::ListDisc); cur.createList(lf); m_editor->setFocus();});
    connect(m_fontSizeCombo, &QComboBox::currentTextChanged, this, [this](const QString& sz){
        bool ok; int pt=sz.toInt(&ok);
        if(ok&&pt>0){QTextCharFormat f;f.setFontPointSize(pt);m_editor->mergeCurrentCharFormat(f);}
        m_editor->setFocus();});

    // Attach menu
    auto* attachMenu = new QMenu(this);
    auto* actFile  = attachMenu->addAction("📄  Attach file");
    auto* actImage = attachMenu->addAction("🖼  Attach image");
    connect(m_attachBtn, &QPushButton::clicked, this, [this, attachMenu]{
        attachMenu->exec(m_attachBtn->mapToGlobal(QPoint(0, m_attachBtn->height())));});
    connect(actFile,  &QAction::triggered, this, [this]{
        QString p = QFileDialog::getOpenFileName(this,"Attach File","","All Files (*)");
        if (!p.isEmpty()) addAttachment(p, false);});
    connect(actImage, &QAction::triggered, this, [this]{
        QString p = QFileDialog::getOpenFileName(this,"Attach Image","",
            "Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp)");
        if (!p.isEmpty()) addAttachment(p, true);});

    // Mention search — customer by name or phone
    connect(custSearch, &QLineEdit::textChanged, this, [this](const QString& t){
        const QString lo=t.toLower();
        for(int i=0;i<m_custMentionList->count();++i){
            auto* it=m_custMentionList->item(i);
            bool nm=it->text().toLower().contains(lo);
            bool ph=it->data(Qt::UserRole+1).toString().contains(lo);
            it->setHidden(lo.isEmpty()?false:!(nm||ph));}});
    // Product by name or barcode
    connect(prodSearch, &QLineEdit::textChanged, this, [this](const QString& t){
        const QString lo=t.toLower();
        for(int i=0;i<m_prodMentionList->count();++i){
            auto* it=m_prodMentionList->item(i);
            bool nm=it->text().toLower().contains(lo);
            bool bc=it->data(Qt::UserRole+1).toString().toLower().contains(lo);
            it->setHidden(lo.isEmpty()?false:!(nm||bc));}});

    // Insert @customer
    connect(custMentionBtn, &QPushButton::clicked, this, [this]{
        auto* it=m_custMentionList->currentItem();
        if(!it){QMessageBox::information(this,"Select Customer","Select a customer first.");return;}
        QString d=it->text().mid(it->text().indexOf(' ')+1).trimmed();
        QString name=d.contains("  📞")?d.left(d.indexOf("  📞")).trimmed():d;
        QTextCursor cur=m_editor->textCursor();
        QTextCharFormat fm; fm.setFontWeight(QFont::Bold);
        fm.setForeground(QColor(ThemeManager::instance().tokens().primary));
        cur.insertText("@"+name,fm);
        QTextCharFormat rst; rst.setFontWeight(QFont::Normal);
        rst.setForeground(QColor(ThemeManager::instance().tokens().textPrimary));
        cur.insertText(" ",rst); m_editor->setTextCursor(cur); m_editor->setFocus();});

    // Insert #product
    connect(prodMentionBtn, &QPushButton::clicked, this, [this]{
        auto* it=m_prodMentionList->currentItem();
        if(!it){QMessageBox::information(this,"Select Product","Select a product first.");return;}
        QString d=it->text().mid(it->text().indexOf(' ')+1).trimmed();
        QString name=d.contains("  [")?d.left(d.indexOf("  [")).trimmed():d;
        QTextCursor cur=m_editor->textCursor();
        QTextCharFormat fm; fm.setFontWeight(QFont::Bold);
        fm.setForeground(QColor(ThemeManager::instance().tokens().success));
        cur.insertText("#"+name,fm);
        QTextCharFormat rst; rst.setFontWeight(QFont::Normal);
        rst.setForeground(QColor(ThemeManager::instance().tokens().textPrimary));
        cur.insertText(" ",rst); m_editor->setTextCursor(cur); m_editor->setFocus();});
}

void NoteEditorDialog::recolorIcons() {
    struct { QPushButton* btn; const char* g; } map[]={
        {m_boldBtn,     "𝐁"},{m_italicBtn,    "𝘐"},{m_underlineBtn,"U̲"},
        {m_strikeBtn,   "S̶"},{m_colorBtn,    "A"}, {m_hlBtn,      "▩"},
        {m_quoteBtn,    "❝"},{m_bulletBtn,   "☰"},
    };
    for(auto& e:map){if(e.btn){e.btn->setText(QString::fromUtf8(e.g));e.btn->setIcon(QIcon());}}
    if(m_attachBtn){m_attachBtn->setText("📎");m_attachBtn->setIcon(QIcon());}
}

// #4: copy attachment to stable attachments dir, store dest path
void NoteEditorDialog::addAttachment(const QString& srcPath, bool /*isImage*/) {
    if (m_attachPaths.contains(srcPath)) return;

    // Copy to our attachments folder so it's always accessible
    QString destDir = attachmentsDir();
    QFileInfo fi(srcPath);
    QString destPath = destDir + "/" + fi.fileName();

    // Avoid overwriting existing — add numeric suffix if needed
    if (QFile::exists(destPath) && destPath != srcPath) {
        int n = 1;
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix().isEmpty() ? "" : "." + fi.suffix();
        while (QFile::exists(destPath)) {
            destPath = destDir + "/" + base + "_" + QString::number(n++) + ext;
        }
    }

    if (srcPath != destPath) {
        if (!QFile::copy(srcPath, destPath)) {
            // Fall back to original path if copy fails
            destPath = srcPath;
        }
    }

    m_attachPaths.append(destPath);
    rebuildAttachmentRow();
}

void NoteEditorDialog::rebuildAttachmentRow() {
    auto* layout = qobject_cast<QHBoxLayout*>(m_attachRow->layout());
    if (layout) {
        while (QLayoutItem* item = layout->takeAt(0)) {
            if (item->widget()) { item->widget()->deleteLater(); }
            delete item;
        }
    }
    m_attachRow->setVisible(!m_attachPaths.isEmpty());
    if (m_attachPaths.isEmpty()) return;

    for (int i = 0; i < m_attachPaths.size(); ++i) {
        const QString& path = m_attachPaths[i];
        QFileInfo fi(path);

        auto* chip = new QFrame;
        chip->setObjectName("attachmentChip");
        auto* cL = new QHBoxLayout(chip);
        cL->setContentsMargins(6, 2, 4, 2);
        cL->setSpacing(4);

        QImageReader reader(path);
        if (reader.canRead()) {
            auto* thumb = new QLabel;
            QPixmap pm(path);
            if (!pm.isNull())
                thumb->setPixmap(pm.scaled(32, 32,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
            cL->addWidget(thumb);
        } else {
            cL->addWidget(new QLabel("📄"));
        }

        auto* nameLbl = new QLabel(fi.fileName());
        nameLbl->setMaximumWidth(130);
        // Click to open
        nameLbl->setCursor(Qt::PointingHandCursor);
        nameLbl->setToolTip("Click to open file");
        connect(nameLbl, &QLabel::linkActivated, [path]{
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));});
        cL->addWidget(nameLbl);

        auto* rmBtn = new QPushButton("✕");
        rmBtn->setObjectName("attachmentChipRemove");
        rmBtn->setFixedSize(18, 18);
        int idx = i;
        connect(rmBtn, &QPushButton::clicked, this, [this, idx]{
            if (idx < m_attachPaths.size()) {
                m_attachPaths.removeAt(idx);
                rebuildAttachmentRow();
            }
        });
        cL->addWidget(rmBtn);
        layout->addWidget(chip);
    }
    layout->addStretch();
}

void NoteEditorDialog::setNote(const Note& note) {
    m_noteId  = note.id;
    m_pinned  = note.pinned;
    m_titleEdit->setText(note.title);
    if (!note.content.isEmpty()) m_editor->setHtml(note.content);
    int ci = m_colorCombo->findData(note.color);
    if (ci >= 0) m_colorCombo->setCurrentIndex(ci);
    m_pinBtn->setChecked(note.pinned);
    m_pinBtn->setText(note.pinned ? "📍 Pinned" : "📌 Pin");
    m_attachPaths = note.attachmentList();
    rebuildAttachmentRow();
}

Note NoteEditorDialog::getNote() const {
    Note n;
    n.id      = m_noteId;
    n.title   = m_titleEdit->text().trimmed();
    n.content = m_editor->toHtml();
    n.color   = m_colorCombo->currentData().toString();
    n.pinned  = m_pinBtn->isChecked();
    n.setAttachmentList(m_attachPaths);
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════
// NotesPage
// ═══════════════════════════════════════════════════════════════════════════
NotesPage::NotesPage(QWidget* parent) : QWidget(parent)
{
    const bool useSqlite =
        DatabaseConnectionManager::instance().isSqliteFallbackActive() ||
        DatabaseConnectionManager::instance().connectionType() == "QSQLITE";

    if (useSqlite) {
        m_noteRepo     = new SQLiteNoteRepository;
        m_customerRepo = new SQLiteCustomerRepository;
        m_productRepo  = new SQLiteProductRepository;
    } else {
        m_noteRepo     = new AccessNoteRepository;
        m_customerRepo = new AccessCustomerRepository;
        m_productRepo  = new AccessProductRepository;
    }

    setupUi();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeType){ onThemeChanged(); });
}

void NotesPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    auto* toolbar = new QFrame;
    toolbar->setObjectName("pageToolbar");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 10, 16, 10);
    tbLayout->setSpacing(12);

    auto* titleLbl = new QLabel(LangManager::instance().t("📝  Notes  /  ملاحظات"));
    titleLbl->setObjectName("pageTitle");

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(LangManager::instance().t("Search notes…"));
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setMinimumWidth(240);
    
    // Add search icon
    QAction* searchAction = new QAction(m_searchEdit);
    searchAction->setIcon(SvgIconHelper::icon(sidebarSvgPath("search-svgrepo-com.svg"), 
                                               QColor("#9CA3AF"), 16));
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);

    m_newNoteBtn = new QPushButton(LangManager::instance().t("＋  New Note"));
    m_newNoteBtn->setObjectName("primaryBtn");

    tbLayout->addWidget(titleLbl);
    tbLayout->addStretch();
    tbLayout->addWidget(m_searchEdit);
    tbLayout->addWidget(m_newNoteBtn);
    root->addWidget(toolbar);

    m_boardScroll = new QScrollArea;
    m_boardScroll->setObjectName("notesBoard");
    m_boardScroll->setWidgetResizable(true);
    m_boardScroll->setFrameShape(QFrame::NoFrame);
    m_boardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_boardWidget = new QWidget;
    m_boardWidget->setObjectName("notesBoardInner");
    m_boardScroll->setWidget(m_boardWidget);
    root->addWidget(m_boardScroll, 1);
    applyBoardBackground();

    // Pagination bar
    m_paginationBar = new QFrame;
    m_paginationBar->setObjectName("paginationBar");
    auto* pLayout = new QHBoxLayout(m_paginationBar);
    pLayout->setContentsMargins(16, 6, 16, 6);
    pLayout->setSpacing(10);

    m_prevPageBtn = new QPushButton("◀  Previous");
    m_prevPageBtn->setObjectName("secondaryBtn");
    m_prevPageBtn->setEnabled(false);

    m_pageLabel = new QLabel("Page 1 of 1");
    m_pageLabel->setObjectName("paginationLabel");
    m_pageLabel->setAlignment(Qt::AlignCenter);

    m_nextPageBtn = new QPushButton("Next  ▶");
    m_nextPageBtn->setObjectName("secondaryBtn");
    m_nextPageBtn->setEnabled(false);

    pLayout->addWidget(m_prevPageBtn);
    pLayout->addStretch();
    pLayout->addWidget(m_pageLabel);
    pLayout->addStretch();
    pLayout->addWidget(m_nextPageBtn);
    root->addWidget(m_paginationBar);

    auto* statusBar = new QFrame;
    statusBar->setObjectName("statusBar");
    auto* sl = new QHBoxLayout(statusBar);
    sl->setContentsMargins(16, 4, 16, 4);
    m_statusLabel = new QLabel("0 notes");
    m_statusLabel->setObjectName("statusLabel");
    sl->addWidget(m_statusLabel);
    root->addWidget(statusBar);

    connect(m_newNoteBtn,  &QPushButton::clicked, this, &NotesPage::onNewNote);
    connect(m_searchEdit,  &QLineEdit::textChanged, this, [this](const QString& t){
        m_searchKeyword = t.trimmed(); m_currentPage = 0; onSearch(m_searchKeyword);});
    connect(m_prevPageBtn, &QPushButton::clicked, this, [this]{
        if (m_currentPage > 0) { --m_currentPage; loadNotes(); }});
    connect(m_nextPageBtn, &QPushButton::clicked, this, [this]{
        ++m_currentPage; loadNotes();});
}

// #5: responsive column count based on available width
int NotesPage::calcColumns() const {
    int w = m_boardScroll->viewport()->width();
    if (w <= 0) w = m_boardScroll->width();
    if (w <= 0) w = 800;
    // Each card ~220px + 20px gap
    int cols = qMax(1, (w - 24) / (220 + 20));
    return qMin(cols, 6);  // max 6 columns
}

void NotesPage::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    // Rebuild board on resize to reflow cards into new column count
    if (!m_notes.isEmpty()) {
        QList<Note> page = m_notes.mid(m_currentPage * kPageSize, kPageSize);
        rebuildBoard(page);
    }
}

void NotesPage::applyBoardBackground() {
    const auto& cfg = ConfigManager::instance();
    if (cfg.notesBgMode() == "image") {
        QString imgPath = cfg.notesBgImagePath();
        if (!imgPath.isEmpty() && QFile::exists(imgPath)) {
            m_boardWidget->setStyleSheet(QString(
                "QWidget#notesBoardInner {"
                "  background-image: url(\"%1\");"
                "  background-repeat: no-repeat;"
                "  background-position: center;"
                "  background-attachment: fixed;"
                "}"
            ).arg(imgPath.replace('\\', '/')));
            m_boardWidget->setAutoFillBackground(true);
            m_boardScroll->setStyleSheet(
                "QScrollArea#notesBoard{background:transparent;}"
                "QScrollArea#notesBoard>QWidget{background:transparent;}");
            return;
        }
    }
    m_boardWidget->setStyleSheet("");
    m_boardWidget->setAutoFillBackground(false);
    m_boardScroll->setStyleSheet("");
}

void NotesPage::onThemeChanged() { applyBoardBackground(); }

void NotesPage::refresh() {
    // #6: Load customers+products lazily here (not in constructor)
    // so New Note dialog opens instantly after first refresh
    if (m_customers.isEmpty()) m_customers = m_customerRepo->getAll();
    if (m_products.isEmpty())  m_products  = m_productRepo->getAll();
    loadNotes();
}

void NotesPage::loadNotes() {
    QList<Note> all = m_noteRepo->getAll();

    QList<Note> filtered;
    if (m_searchKeyword.isEmpty()) {
        filtered = all;
    } else {
        const QString lo = m_searchKeyword.toLower();
        for (const auto& n : all) {
            QTextEdit tmp; tmp.setHtml(n.content);
            if (n.title.toLower().contains(lo) || tmp.toPlainText().toLower().contains(lo))
                filtered.append(n);
        }
    }
    m_notes = filtered;

    int totalPages = qMax(1, (filtered.size() + kPageSize - 1) / kPageSize);
    if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;

    int off = m_currentPage * kPageSize;
    QList<Note> page = filtered.mid(off, kPageSize);

    rebuildBoard(page);

    m_prevPageBtn->setEnabled(m_currentPage > 0);
    m_nextPageBtn->setEnabled(m_currentPage < totalPages - 1);
    m_pageLabel->setText(
        QString("Page %1 of %2").arg(m_currentPage + 1).arg(totalPages));
    m_statusLabel->setText(QString("%1 note(s)").arg(filtered.size()));
}

void NotesPage::onSearch(const QString&) { m_currentPage = 0; loadNotes(); }

void NotesPage::rebuildBoard(const QList<Note>& notes) {
    if (QLayout* old = m_boardWidget->layout()) {
        while (QLayoutItem* it = old->takeAt(0)) {
            if (QWidget* w = it->widget()) { w->setParent(nullptr); delete w; }
            delete it;
        }
        delete old;
    }
    const auto orphans =
        m_boardWidget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* w : orphans) { w->setParent(nullptr); delete w; }

    auto* grid = new QGridLayout(m_boardWidget);
    const int cols = calcColumns();
    const int gap  = 16;
    grid->setSpacing(gap);
    grid->setContentsMargins(gap, gap, gap, gap);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    if (notes.isEmpty()) {
        auto* lbl = new QLabel(
            m_searchKeyword.isEmpty()
                ? "No notes yet.\n\nClick  ＋ New Note  to get started."
                : QString("No notes match \"%1\".").arg(m_searchKeyword));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setObjectName("hintLabel");
        grid->addWidget(lbl, 0, 0, 1, qMax(1, cols));
        return;
    }

    int r = 0, c = 0;
    for (const auto& note : notes) {
        auto* card = new NoteCardWidget(note, m_boardWidget);
        connect(card, &NoteCardWidget::editRequested,   this, &NotesPage::onEditNote);
        connect(card, &NoteCardWidget::deleteRequested, this, &NotesPage::onDeleteNote);
        connect(card, &NoteCardWidget::pinToggled,      this, &NotesPage::onPinToggled);
        connect(card, &NoteCardWidget::viewRequested,   this, &NotesPage::onViewNote);
        grid->addWidget(card, r, c);
        if (++c >= cols) { c = 0; ++r; }
    }
}

void NotesPage::onNewNote() {
    // #6: m_customers/m_products already loaded in refresh(); open dialog immediately
    NoteEditorDialog dlg(m_customers, m_products, this);
    if (dlg.exec() != QDialog::Accepted) return;
    Note note = dlg.getNote();
    if (!m_noteRepo->save(note)) {
        QMessageBox::critical(this, "Error", "Failed to save note.");
        return;
    }
    Logger::instance().info("Note created ID=" + QString::number(note.id));
    loadNotes();
}

void NotesPage::onEditNote(int noteId) {
    Note note;
    bool found = false;
    for (const auto& n : m_notes) {
        if (n.id == noteId) { note = n; found = true; break; }
    }
    if (!found) note = m_noteRepo->getById(noteId);

    NoteEditorDialog dlg(m_customers, m_products, this);
    dlg.setNote(note);
    if (dlg.exec() != QDialog::Accepted) return;
    Note updated = dlg.getNote();
    if (!m_noteRepo->save(updated)) {
        QMessageBox::critical(this, "Error", "Failed to update note.");
        return;
    }
    loadNotes();
}

void NotesPage::onViewNote(int noteId) {
    Note note;
    for (const auto& n : m_notes) {
        if (n.id == noteId) { note = n; break; }
    }
    if (note.id == 0) note = m_noteRepo->getById(noteId);

    NoteViewDialog* dlg = new NoteViewDialog(note, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &NoteViewDialog::editRequested, this, &NotesPage::onEditNote);
    dlg->exec();
}

void NotesPage::onDeleteNote(int noteId) {
    auto reply = QMessageBox::question(this, "Delete Note",
        "Delete this note permanently?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    m_noteRepo->remove(noteId);
    loadNotes();
}

void NotesPage::onPinToggled(int noteId, bool pinned) {
    Note note = m_noteRepo->getById(noteId);
    if (note.id == 0) return;
    note.pinned = pinned;
    m_noteRepo->save(note);
    loadNotes();
}

#include "ui/settings/receipt_designer_widget.h"
#include "services/receipt_printer.h"
#include "infra/config_manager.h"
#include "infra/logger.h"

#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QFrame>
#include <QDialog>
#include <QTextBrowser>

using namespace ReceiptTemplateNS;

namespace {
// Every block type the toolbox offers, in the order it's listed.
const QVector<BlockType> kAllBlockTypes = {
    BlockType::Logo, BlockType::CompanyName, BlockType::Address, BlockType::Phone,
    BlockType::InvoiceNumber, BlockType::DateTime, BlockType::Cashier,
    BlockType::ItemsTable, BlockType::Totals, BlockType::Barcode,
    BlockType::Text, BlockType::Divider, BlockType::Spacer,
};

// Recursively empties a layout, hiding+deleting every widget it contains —
// including widgets nested inside SUB-layouts (like the QFormLayout used for
// each block's property rows).
//
// The bug this fixes: QLayoutItem::widget() is null for an item that wraps a
// nested QLayout rather than a plain widget. The old cleanup loop only ever
// checked child->widget(), so for the properties form specifically — always
// added via m_propertiesLayout->addLayout(form) — that check was always
// false. Only the QFormLayout wrapper got deleted; every QSpinBox/QComboBox/
// QCheckBox/QLabel that had been added to it via form->addRow() was never
// removed or destroyed. They stayed alive as orphaned children of
// m_propertiesContainer, unmanaged by any layout, sitting at their last
// position — which is exactly the overlapping/stacked fields in the
// screenshots. They also silently accumulate on every single block switch,
// which is what eventually crashes the app after enough switches.
void clearLayoutRecursive(QLayout* layout) {
    if (!layout) return;
    QLayoutItem* child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (QWidget* w = child->widget()) {
            w->hide();          // take it off screen immediately — deleteLater()
                                 // only runs on the next event loop pass, so without
                                 // this it stays visible (still stacked) until then.
            w->deleteLater();
        } else if (QLayout* nested = child->layout()) {
            clearLayoutRecursive(nested);   // recurse into the form, etc.
        }
        delete child;
    }
}
}

ReceiptDesignerWidget::ReceiptDesignerWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    reload();
}

void ReceiptDesignerWidget::reload() {
    m_loading = true;

    const int paperMm = ConfigManager::instance().receiptPaperWidthMm();
    m_paperSizeCombo->setCurrentIndex(paperMm == 58 ? 1 : 0);

    const QString json = ConfigManager::instance().receiptTemplateJson(paperMm);
    m_template = json.isEmpty() ? ReceiptTemplate::defaultTemplate(paperMm)
                                : ReceiptTemplate::fromJson(json);

    onRefreshPrinters();
    const QString savedPrinter = ConfigManager::instance().receiptPrinterName();
    const int idx = m_printerCombo->findData(savedPrinter);
    m_printerCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_loading = false;
    rebuildBlockList();
    clearPropertiesPanel();
    refreshPreview();
}

void ReceiptDesignerWidget::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // ── Top bar: paper size + printer ────────────────────────────────────────
    auto* topBar = new QFrame;
    topBar->setObjectName("pageToolbar");
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 8, 12, 8);

    topLayout->addWidget(new QLabel("Paper size:"));
    m_paperSizeCombo = new QComboBox;
    m_paperSizeCombo->addItem("80mm", 80);
    m_paperSizeCombo->addItem("58mm", 58);
    connect(m_paperSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ReceiptDesignerWidget::onPaperSizeChanged);
    topLayout->addWidget(m_paperSizeCombo);

    topLayout->addSpacing(24);
    topLayout->addWidget(new QLabel("Printer:"));
    m_printerCombo = new QComboBox;
    m_printerCombo->setMinimumWidth(200);
    topLayout->addWidget(m_printerCombo, 1);
    auto* refreshPrintersBtn = new QPushButton("🔄");
    refreshPrintersBtn->setToolTip("Refresh printer list");
    refreshPrintersBtn->setFixedWidth(36);
    connect(refreshPrintersBtn, &QPushButton::clicked, this, &ReceiptDesignerWidget::onRefreshPrinters);
    topLayout->addWidget(refreshPrintersBtn);
    connect(m_printerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_loading) return;
        ConfigManager::instance().setReceiptPrinterName(m_printerCombo->currentData().toString());
    });

    root->addWidget(topBar);

    // ── Main working area: toolbox | block list | properties ─────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);

    // Toolbox
    auto* toolboxScroll = new QScrollArea;
    toolboxScroll->setWidgetResizable(true);
    toolboxScroll->setFixedWidth(190);
    auto* toolboxWidget = new QWidget;
    buildToolbox(toolboxWidget);
    toolboxScroll->setWidget(toolboxWidget);
    splitter->addWidget(toolboxScroll);

    // Block list
    auto* listPanel = new QWidget;
    auto* listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->addWidget(new QLabel("<b>Receipt layout</b>  (drag to reorder)"));
    m_blockList = new QListWidget;
    m_blockList->setDragDropMode(QAbstractItemView::InternalMove);
    m_blockList->setDefaultDropAction(Qt::MoveAction);
    connect(m_blockList, &QListWidget::currentRowChanged, this, [this](int) { onBlockSelectionChanged(); });
    connect(m_blockList->model(), &QAbstractItemModel::rowsMoved, this, &ReceiptDesignerWidget::onBlockRowsMoved);
    listLayout->addWidget(m_blockList, 1);
    m_removeBlockBtn = new QPushButton("🗑  Remove selected block");
    connect(m_removeBlockBtn, &QPushButton::clicked, this, &ReceiptDesignerWidget::onRemoveSelectedBlock);
    listLayout->addWidget(m_removeBlockBtn);
    splitter->addWidget(listPanel);

    // Properties
    auto* propsScroll = new QScrollArea;
    propsScroll->setWidgetResizable(true);
    propsScroll->setMinimumWidth(260);
    m_propertiesContainer = new QWidget;
    m_propertiesLayout = new QVBoxLayout(m_propertiesContainer);
    m_propertiesHintLabel = new QLabel("Select a block to edit its properties.");
    m_propertiesHintLabel->setWordWrap(true);
    m_propertiesHintLabel->setStyleSheet("color:#888; padding:12px;");
    m_propertiesLayout->addWidget(m_propertiesHintLabel);
    m_propertiesLayout->addStretch();
    propsScroll->setWidget(m_propertiesContainer);
    splitter->addWidget(propsScroll);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);
    root->addWidget(splitter, 1);

    // ── Bottom actions ────────────────────────────────────────────────────────
    auto* actions = new QHBoxLayout;
    m_resetBtn = new QPushButton("↺  Reset to Default");
    connect(m_resetBtn, &QPushButton::clicked, this, &ReceiptDesignerWidget::onResetTemplate);
    
    m_previewBtn = new QPushButton("👁  Show Preview");
    m_previewBtn->setObjectName("secondaryBtn");
    connect(m_previewBtn, &QPushButton::clicked, this, &ReceiptDesignerWidget::onShowPreview);
    
    m_testPrintBtn = new QPushButton("🖨  Test Print");
    connect(m_testPrintBtn, &QPushButton::clicked, this, &ReceiptDesignerWidget::onTestPrint);
    m_saveBtn = new QPushButton("💾  Save Template");
    m_saveBtn->setObjectName("primaryBtn");
    connect(m_saveBtn, &QPushButton::clicked, this, &ReceiptDesignerWidget::onSaveTemplate);
    actions->addWidget(m_resetBtn);
    actions->addWidget(m_previewBtn);
    actions->addWidget(m_testPrintBtn);
    actions->addStretch();
    actions->addWidget(m_saveBtn);
    root->addLayout(actions);
}

void ReceiptDesignerWidget::buildToolbox(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setSpacing(6);
    layout->addWidget(new QLabel("<b>Add block</b>"));
    for (BlockType t : kAllBlockTypes) {
        auto* btn = new QPushButton(blockTypeIcon(t) + "  " + blockTypeLabel(t));
        btn->setStyleSheet("text-align:left; padding:6px 8px;");
        connect(btn, &QPushButton::clicked, this, [this, t]() { onAddBlock(t); });
        layout->addWidget(btn);
    }
    layout->addStretch();
}

void ReceiptDesignerWidget::onPaperSizeChanged(int) {
    if (m_loading) return;
    const int mm = m_paperSizeCombo->currentData().toInt();

    // Templates are kept separately per paper size, so switching sizes loads
    // that size's own saved layout (or a sensible default) instead of
    // stretching/squashing the other size's blocks.
    const QString json = ConfigManager::instance().receiptTemplateJson(mm);
    m_template = json.isEmpty() ? ReceiptTemplate::defaultTemplate(mm) : ReceiptTemplate::fromJson(json);
    m_template.paperWidthMm = mm;

    ConfigManager::instance().setReceiptPaperWidthMm(mm);

    rebuildBlockList();
    clearPropertiesPanel();
    refreshPreview();
}

void ReceiptDesignerWidget::onAddBlock(BlockType type) {
    ReceiptBlock block;
    block.type = type;
    block.properties = defaultProperties(type);
    m_template.blocks.append(block);
    rebuildBlockList();
    m_blockList->setCurrentRow(m_blockList->count() - 1);
    refreshPreview();
}

void ReceiptDesignerWidget::rebuildBlockList() {
    m_loading = true;
    m_blockList->clear();
    for (const auto& b : m_template.blocks) {
        auto* item = new QListWidgetItem(blockTypeIcon(b.type) + "  " + blockTypeLabel(b.type));
        m_blockList->addItem(item);
    }
    m_loading = false;
}

void ReceiptDesignerWidget::onBlockRowsMoved() {
    if (m_loading) return;
    syncTemplateFromList();
    refreshPreview();
}

void ReceiptDesignerWidget::syncTemplateFromList() {
    // The list widget reordered its rows via drag & drop; rebuild m_template's
    // block order to match by matching each row's label back to a block.
    // Labels aren't unique (e.g. two Text blocks), so instead we track order
    // by keeping a shadow list of block pointers via Qt::UserRole indices.
    // Simpler and robust: read Qt::UserRole+1 "original index" set at build time.
    QVector<ReceiptBlock> reordered;
    reordered.reserve(m_template.blocks.size());
    for (int i = 0; i < m_blockList->count(); ++i) {
        const int originalIndex = m_blockList->item(i)->data(Qt::UserRole).toInt();
        if (originalIndex >= 0 && originalIndex < m_template.blocks.size())
            reordered.append(m_template.blocks[originalIndex]);
    }
    if (reordered.size() == m_template.blocks.size())
        m_template.blocks = reordered;
}

void ReceiptDesignerWidget::onBlockSelectionChanged() {
    if (m_loading) return;
    const int row = m_blockList->currentRow();
    showPropertiesFor(row);
}

void ReceiptDesignerWidget::onRemoveSelectedBlock() {
    const int row = m_blockList->currentRow();
    if (row < 0 || row >= m_template.blocks.size()) return;
    m_template.blocks.remove(row);
    rebuildBlockList();
    clearPropertiesPanel();
    refreshPreview();
}

void ReceiptDesignerWidget::clearPropertiesPanel() {
    clearLayoutRecursive(m_propertiesLayout);
    m_propertiesHintLabel = new QLabel("Select a block to edit its properties.");
    m_propertiesHintLabel->setWordWrap(true);
    m_propertiesHintLabel->setStyleSheet("color:#888; padding:12px;");
    m_propertiesLayout->addWidget(m_propertiesHintLabel);
    m_propertiesLayout->addStretch();
}

void ReceiptDesignerWidget::showPropertiesFor(int blockIndex) {
    clearPropertiesPanel();
    if (blockIndex < 0 || blockIndex >= m_template.blocks.size()) return;

    // clearPropertiesPanel() just added the placeholder hint label + stretch
    // (for the "nothing selected" state) — remove those two since we're
    // populating real content instead. They're always simple top-level
    // items (a QLabel and a QSpacerItem, no nested layout), so the plain
    // helper is fine here.
    clearLayoutRecursive(m_propertiesLayout);

    // NOTE: 'blockIndex' (not a reference into m_template.blocks) is what the
    // property-edit lambdas below capture. m_template.blocks is a QVector and
    // can reallocate (e.g. adding another block from the toolbox), which would
    // dangle a captured reference/iterator — an index re-looked-up on every
    // edit stays valid regardless.
    const ReceiptBlock& block = m_template.blocks[blockIndex];
    auto* title = new QLabel(QString("<b>%1 %2</b>").arg(blockTypeIcon(block.type), blockTypeLabel(block.type)));
    m_propertiesLayout->addWidget(title);

    auto* form = new QFormLayout;
    const QVariantMap& p = block.properties;   // read-only snapshot for initial widget values

    auto commit = [this]() { if (!m_loading) refreshPreview(); };

    form->setSpacing(8);
    form->setContentsMargins(4, 4, 4, 4);
    
    switch (block.type) {
    case BlockType::Logo: {
        auto* heightSpin = new QDoubleSpinBox; 
        heightSpin->setRange(5, 40); 
        heightSpin->setSuffix(" mm");
        heightSpin->setValue(p.value("heightMm", 15.0).toDouble());
        connect(heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, blockIndex, commit](double v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["heightMm"] = v; 
            commit(); 
        });
        form->addRow("Height:", heightSpin);

        // Logo width is auto-constrained to 60% of paper width (40-45mm @ 80mm, 27-30mm @ 58mm)
        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["align"] = v; 
            commit(); 
        });
        form->addRow("Alignment:", alignCombo);

        auto* browseBtn = new QPushButton("Browse for receipt logo…");
        connect(browseBtn, &QPushButton::clicked, this, &ReceiptDesignerWidget::onBrowseLogo);
        form->addRow("", browseBtn);

        auto* note = new QLabel("Logo width is automatically 60% of receipt width.\nThis logo is separate from PDF invoices.\nPDF logo is set in Settings → Company.");
        note->setWordWrap(true);
        note->setStyleSheet("color:#888; font-size:11px;");
        form->addRow(note);
        break;
    }
    case BlockType::CompanyName: {
        auto* sizeSpin = new QSpinBox; 
        sizeSpin->setRange(8, 32); 
        sizeSpin->setSuffix(" pt");
        sizeSpin->setValue(p.value("fontSizePt", 16).toInt());
        connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, blockIndex, commit](int v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["fontSizePt"] = v; 
            commit(); 
        });
        form->addRow("Font size:", sizeSpin);

        auto* boldChk = new QCheckBox("Bold");
        boldChk->setChecked(p.value("bold", true).toBool());
        connect(boldChk, &QCheckBox::toggled, this, [this, blockIndex, commit](bool v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["bold"] = v; 
            commit(); 
        });
        form->addRow("", boldChk);

        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["align"] = v; 
            commit(); 
        });
        form->addRow("Alignment:", alignCombo);
        break;
    }
    case BlockType::Phone:
    case BlockType::InvoiceNumber: {
        auto* labelEdit = new QLineEdit(p.value("label", "").toString());
        connect(labelEdit, &QLineEdit::textChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["label"] = v; 
            commit(); 
        });
        form->addRow("Label:", labelEdit);

        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["align"] = v; 
            commit(); 
        });
        form->addRow("Alignment:", alignCombo);
        break;
    }
    case BlockType::DateTime: {
        auto* formatEdit = new QLineEdit(p.value("format", "yyyy-MM-dd hh:mm").toString());
        connect(formatEdit, &QLineEdit::textChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["format"] = v; 
            commit(); 
        });
        form->addRow("Format:", formatEdit);

        auto* hint = new QLabel("e.g. yyyy-MM-dd hh:mm, dd/MM/yyyy, hh:mm:ss");
        hint->setStyleSheet("color:#888; font-size:11px;");
        form->addRow("", hint);

        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["align"] = v; 
            commit(); 
        });
        form->addRow("Alignment:", alignCombo);
        break;
    }
    case BlockType::ItemsTable: {
        auto* qtyChk = new QCheckBox("Show quantity column");
        qtyChk->setChecked(p.value("showQty", true).toBool());
        connect(qtyChk, &QCheckBox::toggled, this, [this, blockIndex, commit](bool v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["showQty"] = v; commit(); });
        form->addRow(qtyChk);

        auto* priceChk = new QCheckBox("Show unit price column");
        priceChk->setChecked(p.value("showPrice", true).toBool());
        connect(priceChk, &QCheckBox::toggled, this, [this, blockIndex, commit](bool v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["showPrice"] = v; commit(); });
        form->addRow(priceChk);
        break;
    }
    case BlockType::Totals: {
        auto* discChk = new QCheckBox("Show discount line");
        discChk->setChecked(p.value("showDiscount", true).toBool());
        connect(discChk, &QCheckBox::toggled, this, [this, blockIndex, commit](bool v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["showDiscount"] = v; commit(); });
        form->addRow(discChk);

        auto* delChk = new QCheckBox("Show delivery fee line");
        delChk->setChecked(p.value("showDelivery", false).toBool());
        connect(delChk, &QCheckBox::toggled, this, [this, blockIndex, commit](bool v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["showDelivery"] = v; commit(); });
        form->addRow(delChk);

        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["align"] = v; 
            commit(); 
        });
        form->addRow("Alignment:", alignCombo);
        break;
    }
    case BlockType::Barcode: {
        auto* heightSpin = new QDoubleSpinBox; 
        heightSpin->setRange(6, 30); 
        heightSpin->setSuffix(" mm");
        heightSpin->setValue(p.value("heightMm", 12.0).toDouble());
        connect(heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, blockIndex, commit](double v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["heightMm"] = v; 
            commit(); 
        });
        form->addRow("Height:", heightSpin);

        auto* textChk = new QCheckBox("Show code as text underneath");
        textChk->setChecked(p.value("showText", true).toBool());
        connect(textChk, &QCheckBox::toggled, this, [this, blockIndex, commit](bool v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["showText"] = v; 
            commit(); 
        });
        form->addRow("", textChk);

        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["align"] = v; 
            commit(); 
        });
        form->addRow("Alignment:", alignCombo);

        auto* note = new QLabel("Width always matches the paper automatically.");
        note->setStyleSheet("color:#888; font-size:11px;");
        form->addRow("", note);
        break;
    }
    case BlockType::Text: {
        auto* textEdit = new QPlainTextEdit(p.value("text", "").toString());
        textEdit->setFixedHeight(70);
        connect(textEdit, &QPlainTextEdit::textChanged, this, [this, blockIndex, textEdit, commit]() {
            if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["text"] = textEdit->toPlainText();
            commit();
        });
        form->addRow("Text:", textEdit);

        auto* sizeSpin = new QSpinBox; sizeSpin->setRange(7, 28); sizeSpin->setSuffix(" pt");
        sizeSpin->setValue(p.value("fontSizePt", 11).toInt());
        connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, blockIndex, commit](int v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["fontSizePt"] = v; commit(); });
        form->addRow("Font size:", sizeSpin);

        auto* boldChk = new QCheckBox("Bold");
        boldChk->setChecked(p.value("bold", false).toBool());
        connect(boldChk, &QCheckBox::toggled, this, [this, blockIndex, commit](bool v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["bold"] = v; commit(); });
        form->addRow(boldChk);

        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["align"] = v; commit(); });
        form->addRow("Alignment:", alignCombo);
        break;
    }
    case BlockType::Divider: {
        auto* styleCombo = new QComboBox;
        styleCombo->addItems({"dashed", "solid"});
        styleCombo->setCurrentText(p.value("style", "dashed").toString());
        connect(styleCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["style"] = v; commit(); });
        form->addRow("Line style:", styleCombo);
        break;
    }
    case BlockType::Spacer: {
        auto* heightSpin = new QDoubleSpinBox; heightSpin->setRange(1, 30); heightSpin->setSuffix(" mm");
        heightSpin->setValue(p.value("heightMm", 4.0).toDouble());
        connect(heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, blockIndex, commit](double v) { if (blockIndex < m_template.blocks.size()) m_template.blocks[blockIndex].properties["heightMm"] = v; commit(); });
        form->addRow("Height:", heightSpin);
        break;
    }
    case BlockType::Address:
    case BlockType::Cashier:
    default: {
        auto* alignCombo = new QComboBox;
        alignCombo->addItems({"left", "center", "right"});
        alignCombo->setCurrentText(p.value("align", "center").toString());
        connect(alignCombo, &QComboBox::currentTextChanged, this, [this, blockIndex, commit](const QString& v) { 
            if (blockIndex < m_template.blocks.size()) 
                m_template.blocks[blockIndex].properties["align"] = v; 
            commit(); 
        });
        form->addRow("Alignment:", alignCombo);

        auto* note = new QLabel("This block shows the live value from the current invoice.");
        note->setWordWrap(true);
        note->setStyleSheet("color:#888;");
        form->addRow("", note);
        break;
    }
    }

    m_propertiesLayout->addLayout(form);
    m_propertiesLayout->addStretch();
}

void ReceiptDesignerWidget::onPropertyEdited() {
    if (m_loading) return;
    refreshPreview();
}

void ReceiptDesignerWidget::refreshPreview() {
    // Update block list item data for drag-reorder tracking
    for (int i = 0; i < m_blockList->count(); ++i)
        m_blockList->item(i)->setData(Qt::UserRole, i);
    
    // Update live preview if dialog is visible
    updateLivePreview();
}

void ReceiptDesignerWidget::updateLivePreview() {
    if (!m_previewDialog || !m_previewDialog->isVisible() || !m_previewBrowser)
        return;
    
    const QString html = ReceiptPrinter::generatePreviewHtml(m_template);
    m_previewBrowser->setHtml(html);
}

void ReceiptDesignerWidget::onShowPreview() {
    // If already open, just bring to front
    if (m_previewDialog && m_previewDialog->isVisible()) {
        m_previewDialog->raise();
        m_previewDialog->activateWindow();
        return;
    }
    
    // Create modeless dialog (floating window that stays open)
    if (!m_previewDialog) {
        m_previewDialog = new QDialog(this);
        m_previewDialog->setWindowTitle("Receipt Preview");
        m_previewDialog->setModal(false);  // Allow interaction with main window
        m_previewDialog->setAttribute(Qt::WA_DeleteOnClose, false);  // Don't delete when closed
        
        // Set dialog size based on paper width
        int paperMm = m_template.paperWidthMm;
        int dialogWidth = paperMm == 58 ? 280 : 380;
        m_previewDialog->setFixedSize(dialogWidth, 700);
        
        auto* layout = new QVBoxLayout(m_previewDialog);
        layout->setContentsMargins(0, 0, 0, 0);
        
        m_previewBrowser = new QTextBrowser;
        m_previewBrowser->setStyleSheet("background:#fff; border:2px solid #333;");
        layout->addWidget(m_previewBrowser);
        
        auto* closeBtn = new QPushButton("Close");
        closeBtn->setObjectName("secondaryBtn");
        connect(closeBtn, &QPushButton::clicked, m_previewDialog, &QDialog::hide);
        
        auto* btnLayout = new QHBoxLayout;
        btnLayout->addStretch();
        btnLayout->addWidget(closeBtn);
        layout->addLayout(btnLayout);
    }
    
    // Update dialog size in case paper width changed
    int paperMm = m_template.paperWidthMm;
    int dialogWidth = paperMm == 58 ? 280 : 380;
    m_previewDialog->setFixedSize(dialogWidth, 700);
    
    // Update content and show
    const QString html = ReceiptPrinter::generatePreviewHtml(m_template);
    m_previewBrowser->setHtml(html);
    m_previewDialog->show();
}

void ReceiptDesignerWidget::onSaveTemplate() {
    ConfigManager::instance().setReceiptTemplateJson(m_template.paperWidthMm,
        QString::fromUtf8(m_template.toJson().toJson(QJsonDocument::Compact)));
    ConfigManager::instance().setReceiptPaperWidthMm(m_template.paperWidthMm);
    Logger::instance().info(QString("Receipt template saved (%1mm, %2 blocks)")
                            .arg(m_template.paperWidthMm).arg(m_template.blocks.size()));
    QMessageBox::information(this, "Receipt Designer",
        QString("Template saved for %1mm paper.").arg(m_template.paperWidthMm));
}

void ReceiptDesignerWidget::onResetTemplate() {
    if (QMessageBox::question(this, "Reset Template",
            "Discard all changes and restore the default layout for this paper size?")
        != QMessageBox::Yes) return;

    m_template = ReceiptTemplate::defaultTemplate(m_template.paperWidthMm);
    rebuildBlockList();
    clearPropertiesPanel();
    refreshPreview();
}

void ReceiptDesignerWidget::onTestPrint() {
    const QString printerName = m_printerCombo->currentData().toString();
    const QString html = ReceiptPrinter::generatePreviewHtml(m_template);
    if (!ReceiptPrinter::printHtml(html, m_template.paperWidthMm, printerName, this)) {
        QMessageBox::warning(this, "Test Print",
            "Could not print. Check that the selected printer is connected and try again.");
        return;
    }
    QMessageBox::information(this, "Test Print", "Test receipt sent to the printer.");
}

void ReceiptDesignerWidget::onBrowseLogo() {
    const QString path = QFileDialog::getOpenFileName(this, "Select Receipt Logo",
                                                       QString(), "Images (*.png *.jpg *.jpeg)");
    if (path.isEmpty()) return;
    ConfigManager::instance().setReceiptLogoPath(path);
    refreshPreview();  // This will update live preview if open
    QMessageBox::information(this, "Receipt Logo Updated",
        "Receipt logo saved. It will appear on thermal receipts that include a Logo block.\n"
        "This is separate from the PDF invoice logo.");
}

void ReceiptDesignerWidget::onRefreshPrinters() {
    const QString current = m_printerCombo->currentData().toString();
    m_printerCombo->blockSignals(true);
    m_printerCombo->clear();
    m_printerCombo->addItem("(Show print dialog each time)", "");
    for (const QString& name : ReceiptPrinter::getAvailableThermalPrinters())
        m_printerCombo->addItem(name, name);
    const int idx = m_printerCombo->findData(current);
    m_printerCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_printerCombo->blockSignals(false);
}

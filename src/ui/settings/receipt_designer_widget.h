#pragma once

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QDialog>
#include <QVBoxLayout>
#include "core/receipt_template.h"

// ─────────────────────────────────────────────────────────────────────────────
// Settings tab: "Receipt Designer".
//
//   [Toolbox]  ──drag/Add──▶  [Block list]  ◀──select──▶  [Properties panel]
//                                    │
//                                    ▼
//                              [Live preview]
//
// The block list is a QListWidget with internal move enabled — that IS Qt's
// native drag & drop (you can pick a row up and drop it at a new position),
// so blocks are dragged into place exactly as asked, just without a full
// custom QGraphicsScene canvas. Selecting a block swaps the properties panel
// to that block's editable fields; every edit re-renders the preview live.
// ─────────────────────────────────────────────────────────────────────────────
class ReceiptDesignerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReceiptDesignerWidget(QWidget* parent = nullptr);

    // Called by SettingsPage when the tab is shown, in case config changed.
    void reload();

private slots:
    void onPaperSizeChanged(int index);
    void onAddBlock(ReceiptTemplateNS::BlockType type);
    void onBlockSelectionChanged();
    void onBlockRowsMoved();
    void onRemoveSelectedBlock();
    void onPropertyEdited();
    void onSaveTemplate();
    void onResetTemplate();
    void onTestPrint();
    void onShowPreview();
    void onBrowseLogo();
    void onRefreshPrinters();

private:
    void setupUi();
    void buildToolbox(QWidget* parent);
    void rebuildBlockList();
    void showPropertiesFor(int blockIndex);
    void clearPropertiesPanel();
    void refreshPreview();
    void updateLivePreview();  // Updates the floating preview if visible
    void syncTemplateFromList();   // reorders m_template.blocks to match the list widget

    ReceiptTemplateNS::ReceiptTemplate m_template;
    bool m_loading = false;   // guards against feedback loops while populating widgets

    // ── Left: toolbox ────────────────────────────────────────────────────────
    QComboBox* m_paperSizeCombo = nullptr;

    // ── Middle: block list ───────────────────────────────────────────────────
    QListWidget* m_blockList = nullptr;
    QPushButton* m_removeBlockBtn = nullptr;

    // ── Right: properties (stacked, one page per block being edited) ────────
    QWidget* m_propertiesContainer = nullptr;
    QVBoxLayout* m_propertiesLayout = nullptr;
    QLabel* m_propertiesHintLabel = nullptr;

    // ── Printer selection ────────────────────────────────────────────────────
    QComboBox* m_printerCombo = nullptr;

    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QPushButton* m_testPrintBtn = nullptr;
    QPushButton* m_previewBtn = nullptr;

    // ── Modeless preview dialog (live floating window) ──────────────────────
    QDialog* m_previewDialog = nullptr;
    QTextBrowser* m_previewBrowser = nullptr;
};

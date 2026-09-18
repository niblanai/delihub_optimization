#pragma once

#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QLineEdit>
#include <QLabel>
#include <QDialog>
#include <QTextEdit>
#include <QComboBox>
#include <QListWidget>
#include <QMenu>
#include <QResizeEvent>
#include "data/irepositories.h"
#include "core/note.h"
#include "services/theme_manager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Sticky note color palette (fixed — independent of app theme)
// ─────────────────────────────────────────────────────────────────────────────
struct NoteColor {
    QString key;
    QString bg;
    QString text;
    QString label;
};

inline QList<NoteColor> noteColorPalette() {
    return {
        { "yellow",   "#FFF599", "#6B5D00", "🟡 Yellow"  },
        { "pink",     "#FFB3C6", "#7A1F3D", "🩷 Pink"     },
        { "orange",   "#FFCC80", "#7A4A00", "🟠 Orange"   },
        { "green",    "#C8F5B0", "#2E5C1A", "🟢 Green"    },
        { "blue",     "#AEE3F7", "#0D4D66", "🔵 Blue"     },
        { "purple",   "#E3C6FF", "#4A2170", "🟣 Purple"   },
    };
}

inline NoteColor noteColorForKey(const QString& key) {
    for (const auto& c : noteColorPalette())
        if (c.key == key) return c;
    return noteColorPalette().first();
}

// ─────────────────────────────────────────────────────────────────────────────
// NoteCardWidget — one sticky note on the board
// ─────────────────────────────────────────────────────────────────────────────
class NoteCardWidget : public QWidget {
    Q_OBJECT
public:
    explicit NoteCardWidget(const Note& note, QWidget* parent = nullptr);
    int  noteId()   const { return m_noteId; }
    bool isPinned() const { return m_pinned; }

signals:
    void editRequested(int noteId);
    void deleteRequested(int noteId);
    void pinToggled(int noteId, bool pinned);
    void viewRequested(int noteId);   // double-click → full view

protected:
    void mouseDoubleClickEvent(QMouseEvent*) override;

private:
    int    m_noteId = 0;
    bool   m_pinned = false;
    Note   m_note;
};

// ─────────────────────────────────────────────────────────────────────────────
// NoteViewDialog — read-only full-view of a note (double-click on card)
// ─────────────────────────────────────────────────────────────────────────────
class NoteViewDialog : public QDialog {
    Q_OBJECT
public:
    explicit NoteViewDialog(const Note& note, QWidget* parent = nullptr);
signals:
    void editRequested(int noteId);
};

// ─────────────────────────────────────────────────────────────────────────────
// NoteEditorDialog — rich text editor
// ─────────────────────────────────────────────────────────────────────────────
class NoteEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit NoteEditorDialog(const QList<Customer>& customers,
                               const QList<Product>&  products,
                               QWidget* parent = nullptr);

    void setNote(const Note& note);
    Note getNote() const;

private:
    void setupUi();
    void recolorIcons();
    void addAttachment(const QString& srcPath, bool isImage);
    void rebuildAttachmentRow();
    static QString attachmentsDir();   // ProgramData/DeliHub/attachments/

    QPushButton* m_boldBtn      = nullptr;
    QPushButton* m_italicBtn    = nullptr;
    QPushButton* m_underlineBtn = nullptr;
    QPushButton* m_strikeBtn    = nullptr;
    QPushButton* m_colorBtn     = nullptr;
    QPushButton* m_hlBtn        = nullptr;
    QPushButton* m_quoteBtn     = nullptr;
    QPushButton* m_bulletBtn    = nullptr;
    QPushButton* m_attachBtn    = nullptr;
    QComboBox*   m_fontSizeCombo= nullptr;

    QLineEdit*   m_titleEdit = nullptr;
    QComboBox*   m_colorCombo= nullptr;
    QPushButton* m_pinBtn    = nullptr;
    QTextEdit*   m_editor    = nullptr;

    QListWidget* m_custMentionList = nullptr;
    QListWidget* m_prodMentionList = nullptr;

    QWidget*    m_attachRow   = nullptr;
    QStringList m_attachPaths;

    const QList<Customer>& m_customers;
    const QList<Product>&  m_products;
    int  m_noteId = 0;
    bool m_pinned = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// NotesPage — the sticky-note corkboard
// ─────────────────────────────────────────────────────────────────────────────
class NotesPage : public QWidget {
    Q_OBJECT
public:
    explicit NotesPage(QWidget* parent = nullptr);
    void refresh();

protected:
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onNewNote();
    void onSearch(const QString& text);
    void onEditNote(int noteId);
    void onDeleteNote(int noteId);
    void onPinToggled(int noteId, bool pinned);
    void onViewNote(int noteId);
    void onThemeChanged();

private:
    void setupUi();
    void loadNotes();
    void applyBoardBackground();
    void rebuildBoard(const QList<Note>& notes);
    int  calcColumns() const;   // responsive column count

    INoteRepository*     m_noteRepo     = nullptr;
    ICustomerRepository* m_customerRepo = nullptr;
    IProductRepository*  m_productRepo  = nullptr;

    QList<Note>     m_notes;
    QList<Customer> m_customers;
    QList<Product>  m_products;

    QPushButton* m_newNoteBtn = nullptr;
    QLineEdit*   m_searchEdit = nullptr;

    QScrollArea* m_boardScroll = nullptr;
    QWidget*     m_boardWidget = nullptr;

    QFrame*      m_paginationBar = nullptr;
    QPushButton* m_prevPageBtn   = nullptr;
    QPushButton* m_nextPageBtn   = nullptr;
    QLabel*      m_pageLabel     = nullptr;
    int          m_currentPage   = 0;
    static constexpr int kPageSize = 12;   // #2: 12 per page

    QLabel* m_statusLabel = nullptr;
    QString m_searchKeyword;
};

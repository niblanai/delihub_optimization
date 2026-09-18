#pragma once
#include <QString>
#include <QDateTime>
#include <QStringList>

// A sticky note on the corkboard.
// content is stored as HTML (rich text) so formatting is preserved.
struct Note {
    int       id          = 0;
    QString   title;
    QString   content;      // HTML rich text
    QString   color;        // palette key, e.g. "yellow", "pink", "green" …
    bool      pinned       = false;
    QString   attachments; // semicolon-separated file paths
    QDateTime createdAt;
    QDateTime updatedAt;

    // Convenience: parse/serialise attachments list
    QStringList attachmentList() const {
        if (attachments.isEmpty()) return {};
        return attachments.split(';', Qt::SkipEmptyParts);
    }
    void setAttachmentList(const QStringList& list) {
        attachments = list.join(';');
    }
};

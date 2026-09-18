#ifndef BRANCH_MANAGER_H
#define BRANCH_MANAGER_H

#include <QString>
#include <QList>
#include <QSettings>

struct BranchInfo {
    int     id;
    QString name;
    QString dbPath;          // SQLite path  OR  "postgres://..." for Supabase
    bool    isCloud = false; // true = Supabase/PostgreSQL
};

// Manages branch definitions from config.ini
// Each branch entry looks like:
//   [Branch_1]
//   Name=Cairo Main
//   DBPath=\\server\DeliHub\cairo.db   (local SQLite)
//     -OR-
//   DBPath=postgresql://postgres:pass@db.xxx.supabase.co:5432/postgres
//   IsCloud=true
//
// Owner branch (BranchId=0) can see all branches.
class BranchManager {
public:
    static BranchManager& instance();

    void               loadBranches(const QString& configPath,
                                   const QString& writableDataDir = QString());
    QList<BranchInfo>  branches() const;
    BranchInfo         branchById(int id) const;

    // Currently selected branch for this session
    void              setActiveBranch(int id);
    int               activeBranchId() const;
    BranchInfo        activeBranch() const;

    // Add / remove branch (persisted to config)
    void saveBranch(const BranchInfo& b);
    void removeBranch(int id);

private:
    BranchManager() = default;
    void persistBranches();

    QList<BranchInfo> m_branches;
    int               m_activeBranchId = -1;
    QString           m_configPath;
    QString           m_writableDataDir;  // set once at startup by main()
};

#endif // BRANCH_MANAGER_H

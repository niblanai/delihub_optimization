#include "branch_manager.h"
#include "infra/logger.h"
#include <QSettings>
#include <QRegularExpression>
#include <QFileInfo>

BranchManager& BranchManager::instance() {
    static BranchManager inst;
    return inst;
}

void BranchManager::loadBranches(const QString& configPath,
                                 const QString& writableDataDir) {
    m_configPath      = configPath;
    m_writableDataDir = writableDataDir;
    m_branches.clear();

    QSettings s(configPath, QSettings::IniFormat);
    for (const QString& group : s.childGroups()) {
        if (!group.startsWith("Branch_")) continue;
        s.beginGroup(group);
        BranchInfo b;
        b.id       = group.mid(7).toInt();   // "Branch_3" → 3
        b.name     = s.value("Name", "Branch " + QString::number(b.id)).toString();
        b.dbPath   = s.value("DBPath", "").toString();
        b.isCloud  = s.value("IsCloud", false).toBool()
                     || b.dbPath.startsWith("postgresql://")
                     || b.dbPath.startsWith("postgres://");
        s.endGroup();

        // If this is a local SQLite branch with a bare/relative filename,
        // and we have a writable data directory, upgrade it to an absolute
        // path so the branch always opens the right file after install.
        if (!b.isCloud && !b.dbPath.isEmpty()
                && !QFileInfo(b.dbPath).isAbsolute()
                && !writableDataDir.isEmpty()) {
            b.dbPath = writableDataDir + "/" + b.dbPath;
        }

        if (!b.name.isEmpty())
            m_branches.append(b);
    }

    // Always ensure at least one default branch
    if (m_branches.isEmpty()) {
        BranchInfo def;
        def.id      = 1;
        def.name    = "Main Branch";
        // Use the writable data directory for the default DB path if provided.
        // If not provided (empty), fall back to bare relative path (portable mode).
        def.dbPath  = m_writableDataDir.isEmpty()
                      ? "delivery_system.db"
                      : m_writableDataDir + "/delivery_system.db";
        def.isCloud = false;
        m_branches.append(def);
        persistBranches();
    }

    Logger::instance().info(QString("Loaded %1 branch(es) from config").arg(m_branches.size()));
}

QList<BranchInfo> BranchManager::branches() const { return m_branches; }

BranchInfo BranchManager::branchById(int id) const {
    for (const auto& b : m_branches)
        if (b.id == id) return b;
    return m_branches.isEmpty() ? BranchInfo{} : m_branches.first();
}

void BranchManager::setActiveBranch(int id)  { m_activeBranchId = id; }
int  BranchManager::activeBranchId()   const { return m_activeBranchId; }

BranchInfo BranchManager::activeBranch() const {
    return branchById(m_activeBranchId);
}

void BranchManager::saveBranch(const BranchInfo& b) {
    for (auto& existing : m_branches)
        if (existing.id == b.id) { existing = b; persistBranches(); return; }
    m_branches.append(b);
    persistBranches();
}

void BranchManager::removeBranch(int id) {
    m_branches.removeIf([id](const BranchInfo& b){ return b.id == id; });
    persistBranches();
}

void BranchManager::persistBranches() {
    if (m_configPath.isEmpty()) return;
    QSettings s(m_configPath, QSettings::IniFormat);
    // Remove old branch groups
    for (const QString& grp : s.childGroups())
        if (grp.startsWith("Branch_")) s.remove(grp);
    // Write current branches
    for (const auto& b : m_branches) {
        s.beginGroup("Branch_" + QString::number(b.id));
        s.setValue("Name",    b.name);
        s.setValue("DBPath",  b.dbPath);
        s.setValue("IsCloud", b.isCloud);
        s.endGroup();
    }
    s.sync();
}

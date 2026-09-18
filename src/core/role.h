#ifndef ROLE_H
#define ROLE_H

#include <QString>

struct Role {
    int     id   = 0;
    QString name;

    // ── Legacy flags (kept for compatibility) ─────────────────────────────────
    bool canManageUsers    = false;
    bool canViewReports    = false;

    // ── Granular per-section permissions ─────────────────────────────────────
    // Customers
    bool canAddCustomer    = false;
    bool canEditCustomer   = false;
    bool canDeleteCustomer = false;

    // Products
    bool canAddProduct     = false;
    bool canEditProduct    = false;
    bool canDeleteProduct  = false;

    // Orders
    bool canAddOrder       = false;
    bool canEditOrder      = false;
    bool canEditOrderDate  = false;
    bool canCancelOrder    = false;
    bool canDeleteOrders   = false;

    // Dashboard
    bool canAccessDashboard = true;   // default ON so existing roles keep access

    // Regions (admin-level)
    bool canManageRegions  = false;

    // Settings
    bool canAccessSettings = false;

    // ── Phase 1: New Permissions ──────────────────────────────────────────────
    // Purchases
    bool canManagePurchases = false;

    // POS (Point of Sale)
    bool canAccessPOS = false;
    bool canDeleteFromPOS = false;  // Authorize deletion from POS cart

    // Register/Till
    bool canManageRegister = false;

    // Expenses
    bool canManageExpenses = false;

    // Inventory/Stocktake
    bool canManageInventory = false;

    bool operator==(const Role& other) const { return id == other.id; }
};

#endif // ROLE_H

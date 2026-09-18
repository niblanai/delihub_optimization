# Delivery Management System — C++ / Qt / OOP Implementation Specification

## 0. Instructions to the agent

You are building a real, maintainable desktop application — not a toy demo. Follow the architecture below exactly. If any requirement is ambiguous or you must make an assumption not covered here, **stop and ask for clarification instead of guessing**, or clearly mark the assumption in your output/comments. Do not invent database fields, table names, or business rules that are not specified below. Work in the phased order given in Section 9 — do not attempt everything in one pass.

---

## 1. Project Overview

A desktop delivery-service management system for a business that runs delivery orders out of one or more branches. It manages customers, their delivery orders, products, low-stock tracking, recurring/scheduled orders for regular clients, and generates reports and exports. The database must be shareable across branches on a network.

---

## 2. Technology Stack

- **Language:** C++17 or later
- **GUI framework:** Qt 6 (Widgets), using Qt's Model/View framework (`QAbstractTableModel`) for all list/search views (customers, products, orders) to get efficient live search/filtering for free
- **Database:** Microsoft Access (`.accdb`), accessed via ODBC through Qt SQL (`QODBC` driver)
- **Excel export:** QXlsx (or libxlsxwriter) — **do not use COM Automation**; it would require Microsoft Excel to be installed on every machine that runs the app, which is an unnecessary and fragile dependency
- **PDF / printing:** Qt's own `QPrinter` / `QPdfWriter` / `QTextDocument` for invoices and printed reports — no external reporting engine needed
- **Build system:** CMake

---

## 3. Mandatory Layered Architecture

Do not mix data access, business rules, and UI code inside the same class. Structure the codebase into these layers:

```
/src
  /core        -> Domain models only. No Qt SQL, no UI code, no ODBC calls.
  /data        -> Repository classes. ALL SQL/ODBC code lives here, behind interfaces.
  /services    -> Business logic, calculations, report generation, exports, scheduling, auth, audit.
  /ui          -> Qt widgets/windows/dialogs. No direct SQL calls anywhere in this layer.
  /infra       -> Config loading, logging, input validation, backup/restore.
/resources     -> icons, .qss stylesheets, invoice/report templates, shop logo
/db            -> Access schema creation scripts, seed data
/tests         -> unit tests for services and repositories (e.g. Catch2 or GoogleTest)
```

**Why:** every Repository is defined behind an interface (e.g. `ICustomerRepository`, implemented by `AccessCustomerRepository`). This means that if the business later needs to move off Microsoft Access (Access has real concurrency/corruption risk once several branches write to a shared network file at once) to something like SQL Server Express or PostgreSQL, only the `/data` layer changes — the domain, service, and UI layers stay untouched. Build the abstraction from day one even though the initial concrete implementation targets Access.

---

## 4. Domain Model Classes (`/core`)

Use real inheritance/polymorphism, not just naming:

- `Person` (abstract base: id, name, phone list, address list) → `Customer`, `User`
- `Customer` : `Person` — plus: distance from shop (km), region, notes, first-contact date, last-order date, status enum (`Active`, `Inactive`, `Suspended`), list of favorite/usual products
- `Phone`, `Address` — value objects owned by a `Customer` (a customer can have multiple of each)
- `Product` — id, name, price, category, status (`Active`, `Hidden`)
- `Category`
- `ProductPriceHistory` — records the price a product had at the time each order was created, so past invoices never change if a product's price changes later
- `Order` — customer reference, date/time, list of `OrderItem`, delivery fee (manually entered per order, per the original requirement), subtotal (before delivery fee, auto-computed from items), grand total (subtotal + delivery fee, auto-computed)
- `OrderItem` — product reference, quantity, unit price at time of order
- `ScheduledOrder` — customer reference, list of fixed weekdays, fixed time, list of `ScheduledOrderItem` (fixed products/quantities) — for recurring clients such as nearby companies
- `MissingProduct` — product name, quantity needed, branch, date added, purchased flag
- `Branch`
- `User` : `Person` — role (`Admin`, `Manager`, `Employee`)
- `AuditLogEntry` — who, what action, what entity, before/after (where relevant), timestamp

---

## 5. Data Access Layer (`/data`)

One repository interface + one Access-backed implementation per entity above (`ICustomerRepository`/`AccessCustomerRepository`, `IOrderRepository`/`AccessOrderRepository`, etc.), plus:

- `DatabaseConnectionManager` — owns the ODBC connection, reads the connection string from config (see Section 8)
- All multi-step writes (e.g., creating an order + its order items) must run inside a **transaction** so a partial failure never leaves inconsistent data
- Add database indexes on `Customers.Name`, `Phones.Number`, and `Products.Name` to keep live search fast as data grows

---

## 6. Service Layer (`/services`) — business logic and patterns to apply

- `OrderService` — creates/updates orders, computes subtotal and grand total automatically from order items + the manually entered delivery fee
- `PricingService` — implements the **Strategy pattern** for delivery-fee calculation: a `ManualFeeStrategy` (current requirement — the user types the fee per order) and room to add a `DistanceBasedFeeStrategy` later without touching `Order`/`OrderService`
- `SchedulingService` — manages recurring `ScheduledOrder`s; on app startup, checks which scheduled orders are due today and raises a notification
- `ReportService` — abstract `Report` base class with a virtual `generate()`, and concrete subclasses using a **Factory pattern**: `CustomerReport`, `OrderReport`, `SalesReport` (daily/weekly/monthly/yearly), `ProductReport`, `DeliveryFeeReport`, `RegionReport`. This makes the OOP polymorphism real rather than just structural.
- `ExportService` — `ExcelExporter` (QXlsx/libxlsxwriter) and `PdfExporter` (QPdfWriter/QTextDocument, including a printable customer invoice with the shop logo)
- `AuthService` — login and role-based permission checks (`Admin`/`Manager`/`Employee`)
- `AuditService` — wraps repository write calls (decorator-style) to automatically log who added/edited/deleted what and when, instead of hand-adding logging calls at every call site
- `NotificationService` — implements an **Observer pattern**: emits events for "scheduled order due today", "low stock item", "customer inactive for N days", and any UI panel (e.g., the dashboard) can subscribe without the service knowing about the UI

---

## 7. UI Pages (`/ui`, Qt Widgets)

1. **Dashboard** — customer count, today's order count, today's total sales, today's total delivery fees, new customers count, scheduled orders due today, best-selling product, top customer
2. **Customers** — add/edit customer (name, primary + additional phones, primary + additional addresses, distance in km, region, notes, status); live search by name/phone/partial number/region/address; shows each customer's favorite/usual products (surfaced first when creating a new order for them)
3. **Orders** — search customer by name/phone/code; on selection show name, address, last order date, order count; create new order: date/time, product lines (product, qty, unit price), delivery fee field, auto-computed subtotal and grand total
4. **Products** — add/edit product (name, price, category, status); price changes are recorded into `ProductPriceHistory`
5. **Missing Products** — log low-stock items (product, quantity needed, branch, date, purchased yes/no) — a running list instead of pen and paper
6. **Scheduled Orders** — configure recurring orders per regular customer (weekdays, time, fixed product list); dashboard alert when one is due today
7. **Reports** — customer report (totals, new, active, inactive), order report (count, average/highest/lowest order value), sales report (daily/weekly/monthly/yearly), top/least ordering customers, customers inactive 1 month / 6 months, product report (best/least selling, never sold), delivery-fee report (by day/month), region report (customers and sales per region); every report has **Export to Excel** and **Export to PDF**
8. **Advanced search** — e.g. customers who ordered more than N times, customers inactive for N months, customers in a given region, customers whose average invoice exceeds a given amount
9. **Users & permissions** — Admin/Manager/Employee roles with different access levels
10. **Audit log view** — who added/edited/deleted what, and when
11. **Settings** — branch info, DB connection settings, backup/restore
12. **Login dialog**

---

## 8. Database Schema (Access, via ODBC)

`Customers`, `CustomerPhones`, `CustomerAddresses`, `Products`, `Categories`, `ProductPriceHistory`, `Orders`, `OrderItems`, `ScheduledOrders`, `ScheduledOrderItems`, `MissingProducts`, `Branches`, `Users`, `Roles`, `AuditLogs`, `Regions`.

Keep this relational (no repeated/duplicated data across tables) exactly as listed — do not collapse everything into one flat table.

---

## 9. Non-functional requirements

- Config file (not hardcoded) for the DB connection string and current branch ID, so each branch's install can point at the shared Access file location
- Input validation on all forms before writing to the database
- User-friendly error messages (never raw ODBC/SQL errors in the UI)
- File-based logging of errors and key actions
- Backup / restore button for the Access database file
- All monetary and date calculations must be unit-tested in `/tests`, independent of the UI

---

## 10. Phased delivery plan (build in this order, confirm each phase works before moving on)

- **Phase 0:** CMake project skeleton, folder structure from Section 3, config loading, DB connection manager, empty Access schema created from Section 8
- **Phase 1:** Domain models + repositories + unit tests for Customers and Products
- **Phase 2:** Customers UI (add/edit/search, multiple phones/addresses, live search)
- **Phase 3:** Products UI + Missing Products tracking
- **Phase 4:** Orders — customer search/select, order creation with auto-computed totals, order history per customer
- **Phase 5:** Scheduled/recurring orders + due-today notifications
- **Phase 6:** Reports module (all report types from Section 7) + Excel/PDF export
- **Phase 7:** Users, roles/permissions, audit logging
- **Phase 8:** Branches, backup/restore, Dashboard, polish, and full pass of unit tests

---

## 11. Acceptance criteria

- Every monetary total (subtotal, grand total) is always computed automatically from the current data — never manually re-typed
- Switching the concrete repository implementation (e.g. Access → another engine) requires no changes outside `/data`
- No SQL/ODBC code exists outside `/data`
- Every report can be exported to both Excel and PDF
- Old invoices remain unchanged even after a product's price is later updated

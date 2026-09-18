# DeliHub — Delivery Management System

Complete delivery and inventory management system built with Qt6 and C++.

## 🚀 Features

### Core Modules
- **Dashboard**: Real-time analytics and reports
- **Customers**: Customer management with regions and contact details
- **Products**: Product catalog with barcode support, suppliers, and magazines/promotions
- **Orders**: Order processing and tracking
- **Scheduled Orders**: Recurring order management
- **Returns**: Return and refund processing
- **Coupons**: Discount and promotion codes
- **Reports**: Comprehensive business reports

### Phase 1 Features (Completed)
- **Suppliers**: Supplier management
- **Purchases**: Purchase invoice tracking
- **Register/Till**: Cash register with session management
- **Expenses**: Business expense tracking
- **Inventory**: Stock management and adjustments

### Administrative
- **Users**: User accounts, roles, and permissions
- **Settings**: System configuration, branches, themes, backups
- **Audit Log**: Complete audit trail
- **Notes**: Internal notes and memos

### Additional Features
- **Multi-branch**: Support for multiple branches (local + cloud databases)
- **POS Interface**: Dedicated point-of-sale interface
- **Attendance System**: Employee check-in/check-out with barcode scanner
- **Multi-language**: Arabic and English support
- **Custom Themes**: Light, Dark, and fully customizable themes
- **Barcode Printing**: Product barcode label generation
- **Receipt Designer**: Visual receipt template editor
- **Invoice Import**: Import invoices from Excel and PDF
- **Email Notifications**: Brevo integration for email
- **Auto Backup**: Scheduled database backups

## 🛠️ Technology Stack

- **Language**: C++17
- **Framework**: Qt 6.11.1
- **Database**: MS Access (ODBC) with SQLite fallback
- **Build System**: CMake 3.29+
- **Compiler**: MSYS2 UCRT64 GCC 16.1.0
- **Platform**: Windows x64

## 📋 Prerequisites

### Required Tools
- **MSYS2 UCRT64**: [Download](https://www.msys2.org/)
- **CMake 3.29+**: Install via MSYS2
- **Qt 6.11.1**: Install via MSYS2 `ucrt64/mingw-w64-ucrt64-qt6`
- **C++ Compiler**: GCC 16+ (included in MSYS2 UCRT64)

### MSYS2 Packages
```bash
pacman -S mingw-w64-ucrt64-gcc
pacman -S mingw-w64-ucrt64-cmake
pacman -S mingw-w64-ucrt64-ninja
pacman -S mingw-w64-ucrt64-qt6
```

## 🏗️ Building the Project

### Debug Build
```bash
# Configure
cmake -S . -B build_debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build_debug --config Debug

# Run
./build_debug/DeliHub.exe
```

### Release Build
```bash
# Use the automated build script
pwsh build_release.ps1
```

This creates a complete distribution in `dist/` with all dependencies.

## 📂 Project Structure

```
E:/tifany/
├── src/                      # Source code
│   ├── core/                 # Domain models
│   ├── data/                 # Repositories (SQLite + Access)
│   ├── infra/                # Infrastructure (config, database, backup)
│   ├── services/             # Business logic
│   ├── ui/                   # Qt UI pages and dialogs
│   └── sidebar/              # SVG icons
├── CMakeLists.txt            # CMake configuration
├── build_release.ps1         # Release build script
├── *.sql                     # Database schemas
└── README.md                 # This file
```

## 🎨 Custom Theme System

DeliHub includes a powerful theme customization system:
- **Light Theme**: Professional light color scheme
- **Dark Theme**: Easy on the eyes dark mode
- **Custom Theme**: 34 customizable color fields + SVG icon mode

Access via: **Settings → Customize Theme**

### SVG Icon Modes
- **Light Mode**: Black icons (for light backgrounds)
- **Dark Mode**: White icons (for dark backgrounds)
- **Custom**: Pick any color for icons

## 🗄️ Database Setup

### First Run
On first launch, the application will:
1. Prompt for database configuration
2. Create initial schema
3. Guide you through admin account setup

### Database Modes
- **Access Database**: Connect to shared MS Access database via ODBC
- **SQLite Fallback**: Local database if Access connection fails

### Branch Management
Multiple branches supported:
- Local branches (SQLite)
- Cloud branches (shared Access database)

Configure in **Settings → Branches**

## 🔐 Security Features

- Role-based access control
- Password encryption
- Audit logging for all operations
- Barcode-based employee authentication
- Session management

## 📊 Reports

Available reports:
- Sales summary
- Product sales
- Customer purchases
- Revenue by period
- Top selling products
- Returns analysis
- Expense breakdown

## 🌍 Internationalization

Supported languages:
- **Arabic** (RTL layout)
- **English** (LTR layout)

Switch language: **Login Dialog → Language selector**

## 🔧 Configuration

Configuration file: `config.ini` (created on first run)

Key settings:
- Database connection
- Theme selection
- Language preference
- Branch configuration
- Custom theme colors
- SVG icon mode
- Auto backup settings

## 📦 Dependencies (Auto-managed by CMake)

- Qt6 Core, Widgets, SQL, Charts, Multimedia, SVG
- QXlsx (bundled, for Excel support)

Runtime dependencies (included in release build):
- Qt6 DLLs
- MSYS2 UCRT64 runtime libraries
- FFmpeg (for video splash screen)
- ImageMagick & Tesseract (for PDF/OCR invoice import)
- Python 3.14 (bundled, for xls_reader.pyz)

## 🚦 First Time Setup

1. **Clone the repository**
   ```bash
   git clone <your-repo-url>
   cd tifany
   ```

2. **Install dependencies** (MSYS2 UCRT64)
   ```bash
   pacman -S mingw-w64-ucrt64-gcc mingw-w64-ucrt64-cmake \
             mingw-w64-ucrt64-ninja mingw-w64-ucrt64-qt6
   ```

3. **Build the project**
   ```bash
   cmake -S . -B build_debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build_debug
   ```

4. **Run the application**
   ```bash
   ./build_debug/DeliHub.exe
   ```

5. **Create admin account**
   - Follow the first-run wizard
   - Create an admin account
   - Configure your branch

## 📝 Development Guidelines

### Code Style
- Use Qt6 modern signal/slot syntax (lambdas, not SIGNAL/SLOT macros)
- Follow Qt naming conventions: camelCase for functions, PascalCase for classes
- Use `auto` where type is obvious from context
- Add comments for complex business logic

### Adding a New Page
1. Create files in `src/ui/<feature>/`
2. Inherit from `TranslatablePage`
3. Implement `setupUi()`, `refresh()`, `retranslateUi()`
4. Add translations to `lang_manager.cpp`
5. Register in `main_window.cpp`

### Database Access
- Use repository interfaces in `data/irepositories.h`
- Implement for both SQLite and Access
- Never write SQL in UI code

### UI Icons
- Place SVGs in `src/sidebar/`
- Use `SvgIconHelper::icon(path, size)` for automatic theming
- Icons auto-copy to build directory

## 🐛 Troubleshooting

### Build Issues
- **Qt6 not found**: Ensure MSYS2 UCRT64 Qt6 is installed and in PATH
- **CMake version**: Requires CMake 3.29+
- **Compiler errors**: Use MSYS2 UCRT64 terminal (not MinGW64)

### Runtime Issues
- **DLLs missing**: Build with release script or copy from MSYS2 ucrt64/bin
- **Database errors**: Check ODBC driver installed for Access
- **Theme not loading**: Delete config.ini and restart

### Database Issues
- **Access connection fails**: Check ODBC driver (32-bit or 64-bit match)
- **SQLite fallback**: Check file permissions in app directory

## 📄 License

[Your License Here]

## 👥 Contributors

[Your Name/Team]

## 📧 Contact

[Your Contact Info]

## 🙏 Acknowledgments

- Qt Framework
- MSYS2 Project
- QXlsx Library
- SVG Icons from svgrepo.com

---

**Version**: 2.3.0  
**Last Updated**: 2026-09-17

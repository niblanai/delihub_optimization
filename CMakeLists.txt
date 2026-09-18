cmake_minimum_required(VERSION 3.16)
project(DeliHub VERSION 2.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 configuration
find_package(Qt6 REQUIRED COMPONENTS Core Gui GuiPrivate Widgets Sql PrintSupport Multimedia MultimediaWidgets Charts Svg SvgWidgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Fetch QXlsx for Excel Exports
include(FetchContent)
FetchContent_Declare(
    QXlsx
    GIT_REPOSITORY https://github.com/QtExcel/QXlsx.git
    GIT_TAG        v1.4.9
)
FetchContent_GetProperties(QXlsx)
if(NOT qxlsx_POPULATED)
    FetchContent_Populate(QXlsx)
    add_subdirectory(${qxlsx_SOURCE_DIR}/QXlsx ${qxlsx_BINARY_DIR})
    get_target_property(QXLSX_DEFS QXlsx COMPILE_DEFINITIONS)
    if(QXLSX_DEFS)
        list(REMOVE_ITEM QXLSX_DEFS "QT_NO_CAST_FROM_ASCII" "QT_NO_CAST_TO_ASCII")
        set_target_properties(QXlsx PROPERTIES COMPILE_DEFINITIONS "${QXLSX_DEFS}")
    endif()
endif()

# Sources
set(SOURCES
    src/main.cpp
    src/infra/logger.cpp
    src/infra/config_manager.cpp
    src/infra/backup_manager.cpp
    src/infra/database_connection_manager.cpp
    src/infra/schema_deployer.cpp
    src/data/sqlite_repositories.cpp
    src/data/access_repositories.cpp
    src/ui/main_window.cpp
    src/ui/customers/customer_table_model.cpp
    src/ui/customers/customer_dialog.cpp
    src/ui/customers/customers_page.cpp
    src/ui/products/product_table_model.cpp
    src/ui/products/product_dialog.cpp
    src/ui/products/missing_product_dialog.cpp
    src/ui/products/product_quick_edit_dialog.cpp
    src/ui/products/barcode_print_dialog.cpp
    src/ui/products/product_suppliers_dialog.cpp
    src/ui/products/magazine_dialog.cpp
    src/ui/products/products_page.cpp
    src/ui/orders/order_table_model.cpp
    src/ui/orders/order_dialog.cpp
    src/ui/orders/orders_page.cpp
    src/services/invoice_parser.cpp
    src/ui/orders/invoice_review_dialog.cpp
    src/services/order_service.cpp
    src/services/notification_service.cpp
    src/services/scheduling_service.cpp
    src/services/notification_sound_player.cpp
    src/ui/scheduled_orders/scheduled_order_dialog.cpp
    src/ui/scheduled_orders/scheduled_orders_page.cpp
    src/services/auth_service.cpp
    src/services/audit_service.cpp
    src/services/archive_manager.cpp
    src/ui/reports/reports_page.cpp
    src/ui/users/users_page.cpp
    src/ui/dashboard/dashboard_page.cpp
    src/ui/settings/settings_page.cpp
    src/ui/settings/custom_theme_dialog.cpp
    src/services/session_manager.cpp
    src/services/receipt_printer.cpp
    src/services/theme_manager.cpp
    src/services/design_tokens.cpp
    src/services/lang_manager.cpp
    src/services/branch_manager.cpp
    src/services/invoice_generator.cpp
    src/ui/login/login_dialog.cpp
    src/ui/login/attendance_dialog.cpp
    src/ui/login/forgot_password_dialog.cpp
    src/services/brevo_email_service.cpp
    src/services/otp_manager.cpp
    src/ui/intro_splash.cpp
    src/ui/dashboard/charts_widget.cpp
    src/ui/notifications_panel.cpp
    src/infra/auto_backup_scheduler.cpp
    src/ui/coupons/coupons_page.cpp
    src/ui/returns/returns_page.cpp
    src/ui/audit_log/audit_log_page.cpp
    src/ui/pagination_bar.cpp
    src/ui/notes/notes_page.cpp
    # Phase 1: New UI pages
    src/ui/suppliers/suppliers_page.cpp
    src/ui/purchases/purchases_page.cpp
    src/ui/purchases/purchase_invoice_dialog.cpp
    src/ui/register/register_page.cpp
    src/ui/register/session_detail_dialog.cpp
    src/ui/register/close_session_dialog.cpp
    src/ui/expenses/expenses_page.cpp
    src/ui/inventory/inventory_page.cpp
    src/ui/pos/pos_window.cpp
    src/ui/pos/pos_payment_dialog.cpp
    src/ui/pos/receipt_preview_dialog.cpp
    src/ui/pos/numeric_keypad.cpp
    # Barcode & ID Card
    src/services/barcode_generator.cpp
    src/ui/id_card_renderer.cpp
    src/services/barcode_auth.cpp

    # Receipt Designer
    src/core/receipt_template.cpp
    src/ui/settings/receipt_designer_widget.cpp
    
    # Promotions (Task #6)
    src/ui/promotions/promotions_page.cpp
    src/ui/promotions/promotion_table_model.cpp
    src/ui/promotions/promotion_dialog.cpp
)

set(HEADERS
    src/infra/logger.h
    src/infra/config_manager.h
    src/infra/backup_manager.h
    src/infra/database_connection_manager.h
    src/infra/schema_deployer.h
    src/data/irepositories.h
    src/data/sqlite_repositories.h
    src/data/access_repositories.h
    src/ui/main_window.h
    src/ui/customers/customer_table_model.h
    src/ui/customers/customer_dialog.h
    src/ui/customers/customers_page.h
    src/ui/products/product_table_model.h
    src/ui/products/product_dialog.h
    src/ui/products/missing_product_dialog.h
    src/ui/products/product_quick_edit_dialog.h
    src/ui/products/products_page.h
    src/ui/orders/order_table_model.h
    src/ui/orders/order_dialog.h
    src/ui/orders/orders_page.h
    src/ui/orders/invoice_review_dialog.h
    src/services/invoice_parser.h
    src/services/order_service.h
    src/services/notification_service.h
    src/services/scheduling_service.h
    src/services/notification_sound_player.h
    src/ui/scheduled_orders/scheduled_order_dialog.h
    src/ui/scheduled_orders/scheduled_orders_page.h
    src/services/auth_service.h
    src/services/audit_service.h
    src/services/archive_manager.h
    src/services/design_tokens.h
    src/ui/reports/reports_page.h
    src/ui/users/users_page.h
    src/ui/dashboard/dashboard_page.h
    src/ui/settings/settings_page.h
    src/services/session_manager.h
    src/services/receipt_printer.h
    src/services/theme_manager.h
    src/services/lang_manager.h
    src/services/branch_manager.h
    src/services/invoice_generator.h
    src/ui/login/login_dialog.h
    src/ui/login/attendance_dialog.h
    src/ui/intro_splash.h
    src/ui/dashboard/charts_widget.h
    src/ui/notifications_panel.h
    src/infra/auto_backup_scheduler.h
    src/ui/coupons/coupons_page.h
    src/ui/returns/returns_page.h
    src/ui/audit_log/audit_log_page.h
    src/ui/pagination_bar.h
    src/ui/arabic_sort_proxy.h
    src/ui/notes/notes_page.h
    src/core/note.h
    # Phase 1: New UI headers
    src/ui/suppliers/suppliers_page.h
    src/ui/purchases/purchases_page.h
    src/ui/purchases/purchase_invoice_dialog.h
    src/ui/register/register_page.h
    src/ui/register/session_detail_dialog.h
    src/ui/register/close_session_dialog.h
    src/ui/expenses/expenses_page.h
    src/ui/inventory/inventory_page.h
    src/ui/pos/pos_window.h
    src/ui/pos/pos_payment_dialog.h
    src/ui/pos/receipt_preview_dialog.h
    src/ui/pos/numeric_keypad.h
    # Barcode & ID Card headers
    src/services/barcode_generator.h
    src/ui/id_card_renderer.h
    src/services/barcode_auth.h

    # Receipt Designer
    src/core/receipt_template.h
    src/ui/settings/receipt_designer_widget.h
    
    # Promotions (Task #6)
    src/ui/promotions/promotions_page.h
    src/ui/promotions/promotion_table_model.h
    src/ui/promotions/promotion_dialog.h
)

# Executable
add_executable(DeliHub ${SOURCES} ${HEADERS} src/app_icon.rc src/resources.qrc)

target_link_libraries(DeliHub PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::PrintSupport
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Charts
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::Network
    QXlsx::QXlsx
    dbghelp
)

target_include_directories(DeliHub PRIVATE src)

# Copy sidebar icons to build directory
add_custom_command(TARGET DeliHub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/sidebar
    $<TARGET_FILE_DIR:DeliHub>/sidebar
    COMMENT "Copying sidebar icons to build directory"
)

# Optional Unit Testing Target
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    # Fetch Catch2 v3
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    set(TEST_SOURCES
        tests/unit_tests.cpp
        src/infra/logger.cpp
        src/infra/config_manager.cpp
        src/infra/backup_manager.cpp
        src/infra/database_connection_manager.cpp
        src/data/sqlite_repositories.cpp
        src/services/branch_manager.cpp
    )

    add_executable(unit_tests ${TEST_SOURCES})
    target_link_libraries(unit_tests PRIVATE
        Qt6::Core
        Qt6::Sql
        Catch2::Catch2
    )
    target_include_directories(unit_tests PRIVATE src)

    # Diagnostic tool to verify Qt SQL works standalone
    add_executable(diag_sql tests/diag_main.cpp)
    target_link_libraries(diag_sql PRIVATE Qt6::Core Qt6::Sql)
    target_include_directories(diag_sql PRIVATE src)
endif()

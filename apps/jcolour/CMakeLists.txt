cmake_minimum_required(VERSION 3.20)
project(jcolour-ui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
# External libraries
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LCMS2 REQUIRED lcms2)
pkg_check_modules(TIFF REQUIRED libtiff)

# Use full library paths to avoid linker issues with -l flags
find_library(LCMS2_FULL_PATH lcms2 PATHS /opt/homebrew/opt/little-cms2/lib /opt/homebrew/lib NO_DEFAULT_PATH REQUIRED)
find_library(TIFF_FULL_PATH tiff PATHS /opt/homebrew/lib NO_DEFAULT_PATH REQUIRED)

# Engine libraries - use absolute path
get_filename_component(JCOLOUR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_subdirectory(${JCOLOUR_ROOT}/engine engine-build)

# ============================================================
# jcolour UI Application
# ============================================================

set(JCOLOUR_SOURCES
    # Main
    main.cpp
    mainwindow.cpp
    mainwindow.h

    # New shell (ColorProof-inspired layout)
    job.h
    job_list_model.cpp
    job_list_model.h
    theme.cpp
    theme.h
    workflow_sidebar.cpp
    workflow_sidebar.h
    workspace_center.cpp
    workspace_center.h
    job_inspector.cpp
    job_inspector.h
    status_indicators.cpp
    status_indicators.h
    translations.cpp
    translations.h
    settings_dialog.cpp
    settings_dialog.h

    # Workflow selector
    workflow_selector.cpp
    workflow_selector.h

    # B&W Mode
    bw_mode_widget.cpp
    bw_mode_widget.h
    split_tone_wheel.cpp
    split_tone_wheel.h
    black_linearization_panel.cpp
    black_linearization_panel.h
    g7_wizard.cpp
    g7_wizard.h
    gamut_advisor_widget.cpp
    gamut_advisor_widget.h
    ab_softproof_view.cpp
    ab_softproof_view.h
    # Engine modules not in any jcolour-* lib (self-contained / lcms2-only)
    ../../engine/src/g7_validation.cpp
    ../../engine/src/intent_advisor.cpp
    curve_editor.cpp
    curve_editor.h
    ink_channel_widget.cpp
    ink_channel_widget.h

    # Preview & Soft Proofing (NEW)
    preview_widget.cpp
    preview_widget.h

    # Presets
    preset_manager.cpp
    preset_manager.h

    # Paper-state spine (engine ParamDb + state machine + epson hardware)
    paper_state.cpp
    paper_state.h

    # Calibration
    calibration.cpp
    calibration.h

    # Paper-feed (GMG 米尺測試) — pure math for 走紙長度校正
    paper_feed.cpp
    paper_feed.h

    # Queue & Batch
    queue.cpp
    queue.h

    # Nesting
    nesting.cpp
    nesting.h

    # Spot Color (蜂巢 UI - Enhanced)
    spot_color.cpp
    spot_color.h

    # Test Print (NEW)
    test_print.cpp
    test_print.h

    # Step & Repeat (NEW)
    step_repeat.cpp
    step_repeat.h

    # Contact Sheet / 批量联系表 (NEW)
    contact_sheet.cpp
    contact_sheet.h

    # Edition Tracking (NEW)
    edition_tracking.cpp
    edition_tracking.h
    edition_exif.cpp
    edition_exif.h

    # Ink Lot Tracker / 墨盒批次跟踪 (NEW)
    ink_lot_tracker.cpp
    ink_lot_tracker.h

    # Monitor Calibration / 监视器校准检测 (NEW)
    monitor_calibration.cpp
    monitor_calibration.h

    # Profile Drag & Drop / 第三方Profile拖入 (NEW)
    profile_drag_drop.cpp
    profile_drag_drop.h

    # StoreKit Manager / MAS订阅 (NEW)
    storekit_manager.cpp
    storekit_manager.h

    # Splash screen (logo + module showcase)
    splash_screen.cpp
    splash_screen.h

    # Printer manager (System Manager › Printers tab)
    printer_manager.cpp
    printer_manager.h

    # Shared vector icon set (Lucide-style line icons, painted with QPainter)
    ui/icons.cpp
    ui/icons.h

    # New unified shell: Sidebar (4 items) + Jobs print-type bar
    ui/main_sidebar.cpp
    ui/main_sidebar.h
    ui/print_inspector.cpp
    ui/print_inspector.h
    ui/print_type_bar.cpp
    ui/print_type_bar.h
    ui/page_dashboard.cpp
    ui/page_dashboard.h
    ui/page_history.cpp
    ui/page_history.h
    ui/page_color_management.cpp
    ui/page_color_management.h
    ui/page_calibration_library.cpp
    ui/page_calibration_library.h
    ui/calibration_page.cpp
    ui/calibration_page.h
    ui/calibration_stepper.cpp
    ui/calibration_stepper.h
    ui/measure_widget.cpp
    ui/measure_widget.h
    ui/print_gate_bar.cpp
    ui/print_gate_bar.h
    ui/page_ink_management.cpp
    ui/page_ink_management.h
    ui/page_measurement_devices.cpp
    ui/page_measurement_devices.h

    # Native shell rewrite (increment 1): print-environment manager,
    # wired to the engine calibration state machine.
    ui/print_env_manager.cpp
    ui/print_env_manager.h

    # Native shell rewrite (increment 2): the CorelDraw/ErgoSoft-style
    # layout window (design truth: design/jcolour_optimized.html).
    ui/layout_window.cpp
    ui/layout_window.h

    # Qt resource bundle (logo, etc.)
    resources/resources.qrc
)

set(JCOLOUR_HEADERS
    mainwindow.h
    job.h
    job_list_model.h
    theme.h
    workflow_sidebar.h
    workspace_center.h
    job_inspector.h
    status_indicators.h
    bw_mode_widget.h
    split_tone_wheel.h
    black_linearization_panel.h
    g7_wizard.h
    gamut_advisor_widget.h
    ab_softproof_view.h
    curve_editor.h
    ink_channel_widget.h
    preview_widget.h
    preset_manager.h
    calibration.h
    queue.h
    nesting.h
    spot_color.h
    test_print.h
    step_repeat.h
    contact_sheet.h
    edition_tracking.h
    ink_lot_tracker.h
    monitor_calibration.h
    profile_drag_drop.h
    storekit_manager.h
    splash_screen.h
    printer_manager.h
)

add_executable(jcolour-ui
    ${JCOLOUR_SOURCES}
    ${JCOLOUR_HEADERS}
)

# Debug-only: unlock all features at startup. Pre-release: comment this out
# (or pass -DJCOLOUR_DEBUG_UNLOCK=0 to override).
target_compile_definitions(jcolour-ui PRIVATE JCOLOUR_DEBUG_UNLOCK=1)

target_link_libraries(jcolour-ui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    jcolour-spectral
    jcolour-icc
    jcolour-pjl
    jcolour-calibration
    jcolour-epson
    jcolour-argyll
    jcolour-i1sdk
    ${LCMS2_FULL_PATH}
    ${TIFF_FULL_PATH}
    "-framework OpenGL"
    "-F/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/System/Library/Frameworks"
    "-framework AGL"
)

target_include_directories(jcolour-ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../engine/src
    ../engine/calibration
    ../engine/workflow
    ../engine/epson
    ../engine/argyll
    ${LCMS2_INCLUDE_DIRS}
    ${TIFF_INCLUDE_DIRS}
)

# Enable sandbox entitlements for macOS
if(APPLE)
    set_target_properties(jcolour-ui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jcolour.app"
    )
endif()

# Install
install(TARGETS jcolour-ui DESTINATION bin)
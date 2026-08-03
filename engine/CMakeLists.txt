cmake_minimum_required(VERSION 3.20)
project(jcolour-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LCMS2 REQUIRED lcms2)
pkg_check_modules(TIFF REQUIRED libtiff)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# Find full SQLite library path
find_library(SQLITE3_FULL_PATH sqlite3
    PATHS /opt/homebrew/Cellar/sqlite/3.49.2/lib /opt/homebrew/lib
    NO_DEFAULT_PATH REQUIRED)

# CUPS optional (macOS has system CUPS, Linux needs libcups)
pkg_check_modules(CUPS QUIET libcups)
if(NOT CUPS_FOUND)
    # Try system CUPS framework on macOS
    find_package(CUPS)
endif()

# ------------------------------------------------------------------
# Build options
# ------------------------------------------------------------------
# The in-house ESC/P2 emitter (engine/epson/job.cpp) is a Stage-3 path that is
# NOT yet correct for the P9580 (its weave/drop/language params still need to be
# pinned from real-machine .prn capture). Production printing goes through the
# process-isolated Gutenprint backend (jcolour-stp-backend). Keep this OFF until
# the emitter is pinned; turning it ON lets build_hardware_job() drive hardware.
option(JCOLOUR_ENABLE_INHOUSE_ESCP2
    "Allow the in-house Stage-3 ESC/P2 emitter to drive hardware (default OFF)"
    OFF)

# Compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wextra -g)
else()
    add_compile_options(-O3 -DNDEBUG)
endif()

# ============================================================
# Libraries
# ============================================================

# Spectral core (36-channel spectrum, illuminants, XYZ, Lab)
add_library(jcolour-spectral STATIC
    src/spectral.cpp
    src/delta_e.cpp
)
target_link_libraries(jcolour-spectral PUBLIC m)
target_include_directories(jcolour-spectral PUBLIC src)

# K-M forward model
add_library(jcolour-km STATIC
    src/km.cpp
)
target_link_libraries(jcolour-km PUBLIC jcolour-spectral)
target_include_directories(jcolour-km PUBLIC src)

# CYNSN forward model
add_library(jcolour-cynsn STATIC
    src/cynsn.cpp
)
target_link_libraries(jcolour-cynsn PUBLIC jcolour-km)
target_include_directories(jcolour-cynsn PUBLIC src)

# ICC/LCMS2 wrapper
add_library(jcolour-icc STATIC
    src/color_pipeline.cpp
    src/tiff_io.cpp
)

# Find full library paths for linking
find_library(LCMS2_FULL_PATH lcms2 PATHS /opt/homebrew/opt/little-cms2/lib /opt/homebrew/lib NO_DEFAULT_PATH REQUIRED)
find_library(TIFF_FULL_PATH tiff PATHS /opt/homebrew/lib NO_DEFAULT_PATH REQUIRED)

target_link_libraries(jcolour-icc PUBLIC
    jcolour-spectral
    ${LCMS2_FULL_PATH}
    ${TIFF_FULL_PATH}
)
target_include_directories(jcolour-icc PUBLIC
    ${LCMS2_INCLUDE_DIRS}
    ${TIFF_INCLUDE_DIRS}
    src
)

# Softproof engine (paper white simulation, ΔE OOG mask, Or/Gr/V meter)
add_library(jcolour-softproof STATIC
    src/softproof.cpp
)
target_link_libraries(jcolour-softproof PUBLIC
    ${LCMS2_FULL_PATH}
)
target_include_directories(jcolour-softproof PUBLIC
    ${LCMS2_INCLUDE_DIRS}
    src
)

# PJL/Epson direct
add_library(jcolour-pjl STATIC
    src/pjl.cpp
)
if(CUPS_FOUND)
    target_link_libraries(jcolour-pjl PUBLIC ${CUPS_LIBRARIES})
    target_include_directories(jcolour-pjl PUBLIC ${CUPS_INCLUDE_DIRS} src)
else()
    target_include_directories(jcolour-pjl PUBLIC src)
endif()

# i1 SDK wrapper
add_library(jcolour-i1sdk STATIC
    src/i1sdk.cpp
    src/i1pro3_driver.cpp
)
target_include_directories(jcolour-i1sdk PUBLIC src)
target_link_libraries(jcolour-i1sdk PUBLIC
    "-framework IOKit"
    "-framework CoreFoundation"
)

# Calibration: PrintCondition state machine, ParamDb (SQLite), pre-print
# consistency check, AutoCal self-test, self-test charts. This is the
# "paper state" spine consumed by both the app's print and calibration paths.
# self_test.cpp pulls in wf1_measure/wf1_chart for chart→measure simulation.
add_library(jcolour-calibration STATIC
    calibration/state_machine.cpp
    calibration/db.cpp
    calibration/consistency.cpp
    calibration/self_test.cpp
    calibration/charts.cpp
    calibration/cmykogv_target.cpp
    calibration/black_split.cpp
    calibration/target_measure.cpp
    workflow/wf1_measure.cpp
    workflow/wf1_chart.cpp
)
target_link_libraries(jcolour-calibration PUBLIC ${SQLITE3_FULL_PATH})
target_include_directories(jcolour-calibration PUBLIC
    calibration
    workflow
    src
    epson
    ${SQLITE3_INCLUDE_DIRS}
)

# Epson direct hardware: discovery (Bonjour), status, nozzle-check, ink/lot
# read, head cleaning, raw PJL job send. Real network I/O (sockets + dns_sd).
add_library(jcolour-epson STATIC
    epson/epson.cpp
    epson/beo.cpp
    epson/job.cpp
)
target_include_directories(jcolour-epson PUBLIC epson src)
if(JCOLOUR_ENABLE_INHOUSE_ESCP2)
    target_compile_definitions(jcolour-epson PUBLIC JCOLOUR_ENABLE_INHOUSE_ESCP2=1)
endif()

# Argyll subprocess wrapper: local ICC baking (targen/colprof/profcheck) and
# the i1Pro3 measurement backend (spotread/chartread). Shells out via fork/execv
# to locally-installed ArgyllCMS — no proprietary SDK, no cloud.
add_library(jcolour-argyll STATIC
    argyll/argyll.cpp
)
target_include_directories(jcolour-argyll PUBLIC argyll)

# ============================================================
# Executables
# ============================================================

# CLI tool (Spike 1)
add_executable(jcolour-convert src/main.cpp)
target_link_libraries(jcolour-convert PRIVATE
    jcolour-spectral
    jcolour-icc
)
target_include_directories(jcolour-convert PRIVATE src)

# ------------------------------------------------------------------
# jcolour-stp-backend — process-isolated Gutenprint bridge.
#
# GPL ISOLATION: libgutenprint is GPL. It is linked into THIS executable only.
# The engine/app talk to it over a process boundary (manifest in, ESC/P2 out),
# so no closed-source object ever links GPL code. When libgutenprint is not
# installed, the backend still builds as a stub that reports "not configured".
# ------------------------------------------------------------------
# Gutenprint ships its pkg-config module as "gutenprint" (older trees used
# "libgutenprint"); accept either. Point PKG_CONFIG_PATH at the install prefix
# if Gutenprint lives outside the default search path (e.g. $HOME/.local).
pkg_check_modules(GUTENPRINT QUIET gutenprint)
if(NOT GUTENPRINT_FOUND)
    pkg_check_modules(GUTENPRINT QUIET libgutenprint)
endif()
add_executable(jcolour-stp-backend
    stp_backend/backend_main.cpp
    stp_backend/manifest.cpp
    stp_backend/stp_render.cpp
)
target_include_directories(jcolour-stp-backend PRIVATE stp_backend)
if(GUTENPRINT_FOUND)
    target_compile_definitions(jcolour-stp-backend PRIVATE JCOLOUR_HAVE_GUTENPRINT=1)
    target_include_directories(jcolour-stp-backend PRIVATE ${GUTENPRINT_INCLUDE_DIRS})
    target_link_directories(jcolour-stp-backend PRIVATE ${GUTENPRINT_LIBRARY_DIRS})
    target_link_libraries(jcolour-stp-backend PRIVATE ${GUTENPRINT_LIBRARIES})
    message(STATUS "jcolour-stp-backend: libgutenprint found (real render path)")
else()
    message(STATUS "jcolour-stp-backend: libgutenprint NOT found (building stub)")
endif()

# ============================================================
# Testing (Catch2)
# ============================================================

enable_testing()
find_package(Catch2 3 QUIET PATHS /opt/homebrew/lib/cmake/Catch2)

if(Catch2_FOUND)
    # Unit tests for each module
    add_executable(test-km
        tests/test_km.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-km PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-km PRIVATE src)
    add_test(NAME km COMMAND test-km)

    add_executable(test-cynsn
        tests/test_cynsn.cpp
        src/cynsn.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-cynsn PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-cynsn PRIVATE src)
    add_test(NAME cynsn COMMAND test-cynsn)

    add_executable(test-forward-factory
        tests/test_forward_factory.cpp
        src/forward_factory.cpp
        src/cynsn.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-forward-factory PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-forward-factory PRIVATE src)
    add_test(NAME forward-factory COMMAND test-forward-factory)

    add_executable(test-sqp
        tests/test_sqp.cpp
        src/sqp.cpp
    )
    target_link_libraries(test-sqp PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-sqp PRIVATE src)
    add_test(NAME sqp COMMAND test-sqp)

    add_executable(test-jacobian
        tests/test_jacobian.cpp
        src/jacobian.cpp
    )
    target_link_libraries(test-jacobian PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-jacobian PRIVATE src)
    add_test(NAME jacobian COMMAND test-jacobian)

    add_executable(test-cost
        tests/test_cost.cpp
        src/cost.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-cost PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-cost PRIVATE src)
    add_test(NAME cost COMMAND test-cost)

    add_executable(test-constraints
        tests/test_constraints.cpp
        src/constraints.cpp
        src/sqp.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-constraints PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-constraints PRIVATE src)
    add_test(NAME constraints COMMAND test-constraints)

    add_executable(test-halftone
        tests/test_halftone.cpp
        src/halftone.cpp
    )
    target_link_libraries(test-halftone PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-halftone PRIVATE src)
    add_test(NAME halftone COMMAND test-halftone)

    add_executable(test-color-pipeline
        tests/test_color_pipeline.cpp
        src/color_pipeline.cpp
        src/tiff_io.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-color-pipeline PRIVATE
        Catch2::Catch2WithMain
        ${LCMS2_FULL_PATH}
        ${TIFF_FULL_PATH}
        m
    )
    target_include_directories(test-color-pipeline PRIVATE
        ${LCMS2_INCLUDE_DIRS}
        ${TIFF_INCLUDE_DIRS}
        src
    )
    add_test(NAME color-pipeline COMMAND test-color-pipeline)

    add_executable(test-channel-layout
        tests/test_channel_layout.cpp
    )
    target_link_libraries(test-channel-layout PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-channel-layout PRIVATE src)
    add_test(NAME channel-layout COMMAND test-channel-layout)

    add_executable(test-softproof
        tests/test_softproof.cpp
        src/softproof.cpp
    )
    target_link_libraries(test-softproof PRIVATE
        Catch2::Catch2WithMain
        ${LCMS2_FULL_PATH}
        m
    )
    target_include_directories(test-softproof PRIVATE
        ${LCMS2_INCLUDE_DIRS}
        src
    )
    add_test(NAME softproof COMMAND test-softproof)

    add_executable(test-argyll
        tests/test_argyll.cpp
        argyll/argyll.cpp
    )
    target_link_libraries(test-argyll PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-argyll PRIVATE argyll)
    add_test(NAME argyll COMMAND test-argyll)

    add_executable(test-epson
        tests/test_epson.cpp
        epson/epson.cpp
    )
    target_link_libraries(test-epson PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-epson PRIVATE epson)
    add_test(NAME epson COMMAND test-epson)

    add_executable(test-job
        tests/test_job.cpp
        epson/job.cpp
        epson/epson.cpp
    )
    target_link_libraries(test-job PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-job PRIVATE epson)
    add_test(NAME job COMMAND test-job)

    # stp-backend manifest parse/serialize (GPL-free part of the backend).
    add_executable(test-stp-manifest
        tests/test_stp_manifest.cpp
        stp_backend/manifest.cpp
    )
    target_link_libraries(test-stp-manifest PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-stp-manifest PRIVATE stp_backend)
    add_test(NAME stp-manifest COMMAND test-stp-manifest)

    add_executable(test-state-machine
        tests/test_state_machine.cpp
        calibration/state_machine.cpp
    )
    target_link_libraries(test-state-machine PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-state-machine PRIVATE calibration)
    add_test(NAME state-machine COMMAND test-state-machine)

    add_executable(test-charts
        tests/test_charts.cpp
        calibration/charts.cpp
    )
    target_link_libraries(test-charts PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-charts PRIVATE calibration)
    add_test(NAME charts COMMAND test-charts)

    add_executable(test-cmykogv-target
        tests/test_cmykogv_target.cpp
        calibration/cmykogv_target.cpp
        epson/job.cpp
    )
    target_link_libraries(test-cmykogv-target PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-cmykogv-target PRIVATE calibration epson src)
    add_test(NAME cmykogv-target COMMAND test-cmykogv-target)

    add_executable(test-black-split
        tests/test_black_split.cpp
        calibration/black_split.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-black-split PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-black-split PRIVATE calibration src)
    add_test(NAME black-split COMMAND test-black-split)

    add_executable(test-ogv-recruit
        tests/test_ogv_recruit.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-ogv-recruit PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-ogv-recruit PRIVATE src)
    add_test(NAME ogv-recruit COMMAND test-ogv-recruit)

    add_executable(test-ogv-meter
        tests/test_ogv_meter.cpp
        src/ogv_meter.cpp
    )
    target_link_libraries(test-ogv-meter PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-ogv-meter PRIVATE src)
    add_test(NAME ogv-meter COMMAND test-ogv-meter)

    add_executable(test-target-measure
        tests/test_target_measure.cpp
        calibration/target_measure.cpp
        calibration/cmykogv_target.cpp
        epson/job.cpp
        src/km.cpp
        src/spectral.cpp
    )
    target_link_libraries(test-target-measure PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-target-measure PRIVATE calibration epson src)
    add_test(NAME target-measure COMMAND test-target-measure)

    add_executable(test-gamut-mapping
        tests/test_gamut_mapping.cpp
        src/gamut_mapping.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-gamut-mapping PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-gamut-mapping PRIVATE src)
    add_test(NAME gamut-mapping COMMAND test-gamut-mapping)

    add_executable(test-self-test
        tests/test_self_test.cpp
        calibration/self_test.cpp
        calibration/charts.cpp
        workflow/wf1_measure.cpp
        workflow/wf1_chart.cpp
    )
    target_link_libraries(test-self-test PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-self-test PRIVATE calibration workflow src)
    add_test(NAME self-test COMMAND test-self-test)

    add_executable(test-wf1-chart
        tests/test_wf1_chart.cpp
        workflow/wf1_chart.cpp
    )
    target_link_libraries(test-wf1-chart PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf1-chart PRIVATE workflow)
    add_test(NAME wf1-chart COMMAND test-wf1-chart)

    add_executable(test-wf1-measure
        tests/test_wf1_measure.cpp
        workflow/wf1_measure.cpp
        workflow/wf1_chart.cpp
    )
    target_link_libraries(test-wf1-measure PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf1-measure PRIVATE workflow)
    add_test(NAME wf1-measure COMMAND test-wf1-measure)

    add_executable(test-wf1-ink-limit
        tests/test_wf1_ink_limit.cpp
        workflow/wf1_ink_limit.cpp
        workflow/wf1_measure.cpp
        workflow/wf1_chart.cpp
    )
    target_link_libraries(test-wf1-ink-limit PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf1-ink-limit PRIVATE workflow)
    add_test(NAME wf1-ink-limit COMMAND test-wf1-ink-limit)

    add_executable(test-wf1-lut
        tests/test_wf1_lut.cpp
        workflow/wf1_lut.cpp
        workflow/wf1_measure.cpp
        workflow/wf1_chart.cpp
        workflow/wf1_ink_limit.cpp
    )
    target_link_libraries(test-wf1-lut PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf1-lut PRIVATE workflow)
    add_test(NAME wf1-lut COMMAND test-wf1-lut)

    add_executable(test-wf1-print
        tests/test_wf1_print.cpp
        workflow/wf1_print.cpp
        workflow/wf1_lut.cpp
        workflow/wf1_measure.cpp
        workflow/wf1_chart.cpp
        workflow/wf1_ink_limit.cpp
    )
    target_link_libraries(test-wf1-print PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf1-print PRIVATE workflow src epson)
    add_test(NAME wf1-print COMMAND test-wf1-print)

    add_executable(test-wf1-param
        tests/test_wf1_param.cpp
        workflow/wf1_param.cpp
    )
    target_link_libraries(test-wf1-param PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf1-param PRIVATE workflow)
    add_test(NAME wf1-param COMMAND test-wf1-param)

    add_executable(test-wf1-end-to-end
        tests/test_wf1_end_to_end.cpp
        workflow/wf1_end_to_end.cpp
        workflow/wf1_chart.cpp
        workflow/wf1_measure.cpp
        workflow/wf1_ink_limit.cpp
        workflow/wf1_lut.cpp
        workflow/wf1_param.cpp
    )
    target_link_libraries(test-wf1-end-to-end PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf1-end-to-end PRIVATE workflow)
    add_test(NAME wf1-end-to-end COMMAND test-wf1-end-to-end)

    add_executable(test-db
        tests/test_db.cpp
        calibration/db.cpp
    )
    target_link_libraries(test-db PRIVATE Catch2::Catch2WithMain ${SQLITE3_FULL_PATH})
    target_include_directories(test-db PRIVATE calibration ${SQLITE3_INCLUDE_DIRS})
    add_test(NAME db COMMAND test-db)

    add_executable(test-wf4-chart
        tests/test_wf4_chart.cpp
        workflow/wf4_chart.cpp
    )
    target_link_libraries(test-wf4-chart PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf4-chart PRIVATE workflow)
    add_test(NAME wf4-chart COMMAND test-wf4-chart)

    add_executable(test-wf4-measure
        tests/test_wf4_measure.cpp
        workflow/wf4_measure.cpp
        workflow/wf4_chart.cpp
    )
    target_link_libraries(test-wf4-measure PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf4-measure PRIVATE workflow src)
    add_test(NAME wf4-measure COMMAND test-wf4-measure)

    add_executable(test-wf4-profile
        tests/test_wf4_profile.cpp
        workflow/wf4_profile.cpp
        argyll/argyll.cpp
    )
    target_link_libraries(test-wf4-profile PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf4-profile PRIVATE workflow src argyll)
    add_test(NAME wf4-profile COMMAND test-wf4-profile)

    add_executable(test-wf4-transform
        tests/test_wf4_transform.cpp
        workflow/wf4_transform.cpp
        src/color_pipeline.cpp
        src/tiff_io.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf4-transform PRIVATE Catch2::Catch2WithMain ${LCMS2_FULL_PATH} ${TIFF_FULL_PATH} m)
    target_include_directories(test-wf4-transform PRIVATE workflow src ${TIFF_INCLUDE_DIRS})
    add_test(NAME wf4-transform COMMAND test-wf4-transform)

    add_executable(test-wf4-print
        tests/test_wf4_print.cpp
        workflow/wf4_print.cpp
        workflow/wf4_transform.cpp
        src/halftone.cpp
        src/color_pipeline.cpp
        src/tiff_io.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf4-print PRIVATE Catch2::Catch2WithMain ${LCMS2_FULL_PATH} ${TIFF_FULL_PATH} m)
    target_include_directories(test-wf4-print PRIVATE workflow src ${TIFF_INCLUDE_DIRS})
    add_test(NAME wf4-print COMMAND test-wf4-print)

    add_executable(test-wf2-chart
        tests/test_wf2_chart.cpp
        workflow/wf2_chart.cpp
    )
    target_link_libraries(test-wf2-chart PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf2-chart PRIVATE workflow)
    add_test(NAME wf2-chart COMMAND test-wf2-chart)

    add_executable(test-wf2-self-test
        tests/test_wf2_self_test.cpp
        workflow/wf2_self_test.cpp
        workflow/wf2_chart.cpp
        workflow/wf2_lut.cpp
        workflow/wf2_measure.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf2-self-test PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-wf2-self-test PRIVATE workflow src)
    add_test(NAME wf2-self-test COMMAND test-wf2-self-test)

    add_executable(test-wf4-self-test
        tests/test_wf4_self_test.cpp
        workflow/wf4_self_test.cpp
        workflow/wf4_chart.cpp
        src/g7_validation.cpp
    )
    target_link_libraries(test-wf4-self-test PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-wf4-self-test PRIVATE workflow src)
    add_test(NAME wf4-self-test COMMAND test-wf4-self-test)

    add_executable(test-wf3-measure
        tests/test_wf3_measure.cpp
        workflow/wf3_measure.cpp
        workflow/wf3_chart.cpp
    )
    target_link_libraries(test-wf3-measure PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-wf3-measure PRIVATE workflow src)
    add_test(NAME wf3-measure COMMAND test-wf3-measure)

    add_executable(test-wf3-lut
        tests/test_wf3_lut.cpp
        workflow/wf3_lut.cpp
        workflow/wf3_measure.cpp
        workflow/wf3_chart.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf3-lut PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-wf3-lut PRIVATE workflow src)
    add_test(NAME wf3-lut COMMAND test-wf3-lut)

    add_executable(test-wf6-chart
        tests/test_wf6_chart.cpp
        workflow/wf6_chart.cpp
        workflow/wf4_chart.cpp
        src/gamut_mapping.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf6-chart PRIVATE
        Catch2::Catch2WithMain
        ${LCMS2_FULL_PATH}
        m
    )
    target_include_directories(test-wf6-chart PRIVATE
        workflow src
        ${LCMS2_INCLUDE_DIRS}
    )
    add_test(NAME wf6-chart COMMAND test-wf6-chart)

    add_executable(test-g7-validation
        tests/test_g7_validation.cpp
        src/g7_validation.cpp
        src/profile_library.cpp
    )
    target_link_libraries(test-g7-validation PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-g7-validation PRIVATE src)
    add_test(NAME g7-validation COMMAND test-g7-validation)

    add_executable(test-tiling
        tests/test_tiling.cpp
        src/tiling.cpp
    )
    target_link_libraries(test-tiling PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-tiling PRIVATE src)
    add_test(NAME tiling COMMAND test-tiling)

    add_executable(test-profile-library
        tests/test_profile_library.cpp
        src/profile_library.cpp
    )
    target_link_libraries(test-profile-library PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-profile-library PRIVATE src)
    add_test(NAME profile-library COMMAND test-profile-library)

    add_executable(test-wf2-measure
        tests/test_wf2_measure.cpp
        workflow/wf2_measure.cpp
        workflow/wf2_chart.cpp
        src/km.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf2-measure PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-wf2-measure PRIVATE workflow src)
    add_test(NAME wf2-measure COMMAND test-wf2-measure)

    add_executable(test-wf2-lut
        tests/test_wf2_lut.cpp
        workflow/wf2_lut.cpp
    )
    target_link_libraries(test-wf2-lut PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf2-lut PRIVATE workflow src)
    add_test(NAME wf2-lut COMMAND test-wf2-lut)

    add_executable(test-wf2-print
        tests/test_wf2_print.cpp
        workflow/wf2_print.cpp
        workflow/wf2_lut.cpp
    )
    target_link_libraries(test-wf2-print PRIVATE Catch2::Catch2WithMain)
    target_include_directories(test-wf2-print PRIVATE workflow src)
    add_test(NAME wf2-print COMMAND test-wf2-print)

    add_executable(test-wf3-print
        tests/test_wf3_print.cpp
        workflow/wf3_print.cpp
        workflow/wf3_lut.cpp
        workflow/wf3_measure.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf3-print PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-wf3-print PRIVATE workflow src)
    add_test(NAME wf3-print COMMAND test-wf3-print)

    add_executable(test-wf3-self-test
        tests/test_wf3_self_test.cpp
        workflow/wf3_self_test.cpp
        workflow/wf3_lut.cpp
        workflow/wf3_measure.cpp
        src/delta_e.cpp
        src/spectral.cpp
    )
    target_link_libraries(test-wf3-self-test PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-wf3-self-test PRIVATE workflow src)
    add_test(NAME wf3-self-test COMMAND test-wf3-self-test)

    add_executable(test-wf6-print
        tests/test_wf6_print.cpp
        workflow/wf6_print.cpp
        workflow/wf6_chart.cpp
        workflow/wf6_gamut.cpp
        workflow/wf4_chart.cpp
        src/halftone.cpp
        src/color_pipeline.cpp
        src/tiff_io.cpp
        src/spectral.cpp
        src/delta_e.cpp
        src/gamut_mapping.cpp
    )
    target_link_libraries(test-wf6-print PRIVATE Catch2::Catch2WithMain ${LCMS2_FULL_PATH} ${TIFF_FULL_PATH} m)
    target_include_directories(test-wf6-print PRIVATE workflow src ${TIFF_INCLUDE_DIRS})
    add_test(NAME wf6-print COMMAND test-wf6-print)

    add_executable(test-wf6-self-test
        tests/test_wf6_self_test.cpp
        workflow/wf6_self_test.cpp
        workflow/wf6_chart.cpp
        workflow/wf6_gamut.cpp
        workflow/wf4_chart.cpp
        src/gamut_mapping.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-wf6-self-test PRIVATE
        Catch2::Catch2WithMain
        ${LCMS2_FULL_PATH}
        m
    )
    target_include_directories(test-wf6-self-test PRIVATE
        workflow src
        ${LCMS2_INCLUDE_DIRS}
    )
    add_test(NAME wf6-self-test COMMAND test-wf6-self-test)

    add_executable(test-consistency
        tests/test_consistency.cpp
        calibration/consistency.cpp
        calibration/db.cpp
    )
    target_link_libraries(test-consistency PRIVATE Catch2::Catch2WithMain ${SQLITE3_FULL_PATH})
    target_include_directories(test-consistency PRIVATE calibration ${SQLITE3_INCLUDE_DIRS})
    add_test(NAME consistency COMMAND test-consistency)

    add_executable(test-spectral
        tests/test_spectral.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-spectral PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-spectral PRIVATE src)
    add_test(NAME spectral COMMAND test-spectral)

    add_executable(test-delta-e
        tests/test_delta_e.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-delta-e PRIVATE Catch2::Catch2WithMain m)
    target_include_directories(test-delta-e PRIVATE src)
    add_test(NAME delta-e COMMAND test-delta-e)

    add_executable(test-tiff-io
        tests/test_tiff_io.cpp
        src/spectral.cpp
        src/delta_e.cpp
        src/tiff_io.cpp
    )
    target_link_libraries(test-tiff-io PRIVATE
        Catch2::Catch2WithMain
        ${TIFF_FULL_PATH}
        m
    )
    target_include_directories(test-tiff-io PRIVATE
        ${TIFF_INCLUDE_DIRS}
        src
    )
    add_test(NAME tiff-io COMMAND test-tiff-io)

    add_executable(test-pjl
        tests/test_pjl.cpp
        src/pjl.cpp
    )
    target_link_libraries(test-pjl PRIVATE
        Catch2::Catch2WithMain
        ${CUPS_LIBRARIES}
    )
    target_include_directories(test-pjl PRIVATE
        ${CUPS_INCLUDE_DIRS}
        src
    )
    add_test(NAME pjl COMMAND test-pjl)

    add_executable(test-i1sdk
        tests/test_i1sdk.cpp
        src/i1sdk.cpp
        src/i1pro3_driver.cpp
        src/spectral.cpp
        src/delta_e.cpp
    )
    target_link_libraries(test-i1sdk PRIVATE Catch2::Catch2WithMain m
        "-framework IOKit" "-framework CoreFoundation")
    target_include_directories(test-i1sdk PRIVATE src)
    add_test(NAME i1sdk COMMAND test-i1sdk)

    add_executable(test-integration
        tests/test_integration.cpp
        src/spectral.cpp
        src/delta_e.cpp
        src/color_pipeline.cpp
        src/tiff_io.cpp
        src/pjl.cpp
        src/i1sdk.cpp
        src/i1pro3_driver.cpp
    )
    target_link_libraries(test-integration PRIVATE
        Catch2::Catch2WithMain
        ${LCMS2_FULL_PATH}
        ${TIFF_FULL_PATH}
        ${CUPS_LIBRARIES}
        "-framework IOKit"
        "-framework CoreFoundation"
        m
    )
    target_include_directories(test-integration PRIVATE
        ${LCMS2_INCLUDE_DIRS}
        ${TIFF_INCLUDE_DIRS}
        ${CUPS_INCLUDE_DIRS}
        src
    )
    add_test(NAME integration COMMAND test-integration)

    # All tests combined
    add_executable(test-all
        tests/test_spectral.cpp
        tests/test_delta_e.cpp
        tests/test_tiff_io.cpp
        tests/test_pjl.cpp
        tests/test_i1sdk.cpp
        tests/test_integration.cpp
        src/spectral.cpp
        src/delta_e.cpp
        src/tiff_io.cpp
        src/pjl.cpp
        src/i1sdk.cpp
        src/i1pro3_driver.cpp
        src/color_pipeline.cpp
    )
    target_link_libraries(test-all PRIVATE
        Catch2::Catch2WithMain
        ${LCMS2_FULL_PATH}
        ${TIFF_FULL_PATH}
        ${CUPS_LIBRARIES}
        "-framework IOKit"
        "-framework CoreFoundation"
        m
    )
    target_include_directories(test-all PRIVATE
        ${LCMS2_INCLUDE_DIRS}
        ${TIFF_INCLUDE_DIRS}
        ${CUPS_INCLUDE_DIRS}
        src
    )
    add_test(NAME all-tests COMMAND test-all)
endif()

# ============================================================
# Install
# ============================================================

install(TARGETS jcolour-convert DESTINATION bin)

cmake_minimum_required(VERSION 3.21)

project(sysquill
    VERSION 0.1.0
    DESCRIPTION "A live system monitor for KDE Plasma"
    HOMEPAGE_URL "https://github.com/narl3yyy-svg/Sysquill"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

set(APP_ID "io.github.narl3yyy_svg.Sysquill")
set(HELPER_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/sysquill")
set(HELPER_PATH "${HELPER_INSTALL_DIR}/sysquill-helper")

# ---------------------------------------------------------------------------
# Code shared by the app and the privileged helper
add_library(sysquill_common STATIC
    src/common/procscan.cpp
    src/common/procscan.h)
target_include_directories(sysquill_common PUBLIC src/common)
target_link_libraries(sysquill_common PUBLIC Qt6::Core)
set_target_properties(sysquill_common PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# The app
qt_add_executable(sysquill
    src/main.cpp
    src/format.h
    src/snapshot.h
    src/sampler.cpp
    src/sampler.h
    src/systemmonitor.cpp
    src/systemmonitor.h
    src/processmodel.cpp
    src/processmodel.h
    src/sparkgraph.cpp
    src/sparkgraph.h)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(sysquill
    URI SysquillUi
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Sidebar.qml
        qml/Card.qml
        qml/Stat.qml
        qml/UsageBar.qml
        qml/GraphBox.qml
        qml/LegendDot.qml
        qml/PageHeader.qml
        qml/ThemeIcon.qml
        qml/OverviewPage.qml
        qml/ProcessesPage.qml
        qml/CpuPage.qml
        qml/MemoryPage.qml
        qml/DisksPage.qml
        qml/NetworkPage.qml
        qml/GpuPage.qml
        qml/SensorsPage.qml
    RESOURCES
        data/${APP_ID}.svg)

target_compile_definitions(sysquill PRIVATE
    SYSQUILL_VERSION="${PROJECT_VERSION}"
    SYSQUILL_APP_ID="${APP_ID}"
    SYSQUILL_HELPER_PATH="${HELPER_PATH}")

target_include_directories(sysquill PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(sysquill PRIVATE
    sysquill_common
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2)

# ---------------------------------------------------------------------------
# Small helper that runs as root (through pkexec) only when the user asks
add_executable(sysquill-helper src/helper/main.cpp)
target_link_libraries(sysquill-helper PRIVATE sysquill_common)

# ---------------------------------------------------------------------------
# Install
install(TARGETS sysquill RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS sysquill-helper RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/sysquill)

configure_file(data/sysquill.policy.in ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.policy @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.policy
        DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES data/${APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/${APP_ID}.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/${APP_ID}.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

# ---------------------------------------------------------------------------
# .deb package (cpack -G DEB)
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "sysquill")
set(CPACK_PACKAGE_VENDOR "Sysquill")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Live system monitor (task manager) for KDE Plasma")
set(CPACK_PACKAGE_DESCRIPTION "Sysquill shows CPU, memory, disks, network, Intel GPU, sensors and processes live, with a searchable process list that can end processes.")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sysquill <https://github.com/narl3yyy-svg/Sysquill>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/narl3yyy-svg/Sysquill")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtqml-workerscript, qt6-svg-plugins, pkexec | policykit-1, pciutils")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qml6-module-org-kde-desktop, kf6-breeze-icon-theme | breeze-icon-theme")
include(CPack)

project(dds10-thumbnailer-kde)
cmake_minimum_required(VERSION 3.16.0)

set(QT_MIN_VERSION "5.15.2")
set(KF5_MIN_VERSION "5.78.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.78")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

#set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FeatureSummary)
#include(WriteBasicConfigVersionFile)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS KIO)

set(dds10thumbnail_SRCS thumbnailer_dds10.cpp)
add_library(dds10thumbnail MODULE ${dds10thumbnail_SRCS})
target_link_libraries(dds10thumbnail PRIVATE KF5::KIOWidgets Qt::Gui)

install(TARGETS dds10thumbnail DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES thumbnailer_dds10.desktop DESTINATION ${SERVICES_INSTALL_DIR})

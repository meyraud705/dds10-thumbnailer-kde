cmake_minimum_required(VERSION 3.16.0)
project(dds10-thumbnailer-kde)

set(QT_MIN_VERSION "6.0.0")
set(KF5_MIN_VERSION "5.100.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.100")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

#set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FeatureSummary)
#include(WriteBasicConfigVersionFile)
include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui)
find_package(KF6 ${KF5_MIN_VERSION} REQUIRED COMPONENTS KIO)

kcoreaddons_add_plugin(dds10thumbnail SOURCES thumbnailer_dds10.cpp INSTALL_NAMESPACE "kf6/thumbcreator")
target_link_libraries(dds10thumbnail PRIVATE KF6::KIOGui Qt::Gui)

cmake_minimum_required(VERSION 3.16)
project(katebookmarksplugin)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(QT_MIN_VERSION "6.5.0")
set(KF6_DEP_VERSION "6.0.0")
set(QT_MAJOR_VERSION "6")

find_package(ECM ${KF6_DEP_VERSION} QUIET REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEClangFormat)

include(ECMOptionalAddSubdirectory)
include(ECMAddAppIcon)
include(ECMInstallIcons)
include(ECMDeprecationSettings)

find_package(KF6 REQUIRED COMPONENTS TextEditor I18n)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

kcoreaddons_add_plugin(katebookmarksplugin
    INSTALL_NAMESPACE "kf6/ktexteditor")

target_sources(katebookmarksplugin PRIVATE
    KateBookmarksPlugin.cpp
)

target_link_libraries(katebookmarksplugin
    KF6::TextEditor
    Qt6::Core
    Qt6::Widgets
)

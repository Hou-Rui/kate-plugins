cmake_minimum_required(VERSION 3.16)
project(kateplugins)

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

add_subdirectory(src)
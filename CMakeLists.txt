cmake_minimum_required(VERSION 3.18)

project(WPEBackend-android)

find_package(PkgConfig)
pkg_check_modules(WPE REQUIRED IMPORTED_TARGET wpe-1.0)
pkg_check_modules(GLib REQUIRED IMPORTED_TARGET
    "gio-2.0>=2.40" "gobject-2.0>=2.40" "gthread-2.0>=2.40" "gmodule-2.0>=2.40")

set(WPE_ANDROID_PUBLIC_HDRS
    include/wpe-android/view-backend.h
)

add_library(WPEBackend-android SHARED
    src/android.cpp
    src/ipc.cpp
    src/renderer-backend-egl.cpp
    src/renderer-host.cpp
    src/view-backend.cpp
)
target_include_directories(WPEBackend-android PRIVATE
    include
)
target_link_libraries(WPEBackend-android
    PkgConfig::GLib
    PkgConfig::WPE
    android
    c++_shared
    log
    EGL
    GLESv2
)
set_target_properties(WPEBackend-android PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED TRUE
)

set(INSTALL_INC_DIR "${CMAKE_INSTALL_INCLUDEDIR}/wpe-android" CACHE PATH "Installation directory for headers")

install(TARGETS WPEBackend-android
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${WPE_ANDROID_PUBLIC_HDRS} DESTINATION ${INSTALL_INC_DIR})

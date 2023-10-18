project(WPEBackend-android)

cmake_minimum_required(VERSION 3.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(PkgConfig)
pkg_check_modules(WPE wpe-1.0 REQUIRED)

find_package(GLIB 2.40.0 REQUIRED COMPONENTS gio gobject gthread gmodule)

set(WPE_ANDROID_PUBLIC_HDRS
    "include/wpe-android/view-backend.h"
)

set(WPE_ANDROID_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
    "include"
)

set(WPE_ANDROID_LIBRARIES
    ${GLIB_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${WPE_LIBRARIES}
    android
    c++_shared
    log
    EGL
    GLESv2
)

set(WPE_ANDROID_SOURCES
    src/android.cpp
    src/ipc.cpp
    src/renderer-backend-egl.cpp
    src/renderer-host.cpp
    src/view-backend.cpp
)

add_library(WPEBackend-android SHARED ${WPE_ANDROID_SOURCES})
target_include_directories(WPEBackend-android PRIVATE ${WPE_ANDROID_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-android ${WPE_ANDROID_LIBRARIES})

set(INSTALL_INC_DIR "${CMAKE_INSTALL_INCLUDEDIR}/wpe-android" CACHE PATH "Installation directory for headers")

install(TARGETS WPEBackend-android
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${WPE_ANDROID_PUBLIC_HDRS} DESTINATION ${INSTALL_INC_DIR})

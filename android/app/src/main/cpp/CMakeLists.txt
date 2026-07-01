cmake_minimum_required(VERSION 3.22.1)
project(penguindash)

# Penguin Dash Android native build.
# A0: NativeActivity + android_native_app_glue + an EGL GLES2 clear-screen loop.
# A1+ will add the ETR engine source list and swap the clear loop for the game loop.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# native_app_glue ships with the NDK; compile it in and keep its entry symbol.
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue)
# NativeActivity resolves ANativeActivity_onCreate at load time — force-keep it.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(penguindash SHARED
    native_main.cpp)
target_include_directories(penguindash PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv2-lib GLESv2)

target_link_libraries(penguindash
    native_app_glue
    ${android-lib} ${egl-lib} ${glesv2-lib} ${log-lib})

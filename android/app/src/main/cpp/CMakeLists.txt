cmake_minimum_required(VERSION 3.22.1)
project(penguindash)

# Penguin Dash Android native build.
# A0: NativeActivity + EGL GLES2 clear-screen (native_main.cpp).
# A1: compile the ETR engine (src/*.cpp) into libpenguindash.so against the
#     SFML-compat shim (sfml_compat/ first on the include path) + GLES2. SFML is
#     replaced, not cross-compiled. main.cpp is excluded — its init/loop is driven
#     from android_main (A1c). Audio/font/texture are shim-stubbed until A1d/A3.

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine source root (repo src/), five levels up from this cpp/ dir.
get_filename_component(ETRSRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src" ABSOLUTE)

file(GLOB ENGINE_SRC "${ETRSRC}/*.cpp")
list(REMOVE_ITEM ENGINE_SRC "${ETRSRC}/main.cpp")   # entry replaced by android_main

# native_app_glue ships with the NDK; compile it in and keep its entry symbol.
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue)
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(penguindash SHARED
    native_main.cpp
    sfml_compat.cpp
    ${ENGINE_SRC})

target_include_directories(penguindash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/sfml_compat   # our <SFML/...> shim — must precede any real SFML
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party    # stb_image / stb_truetype (A1d)
    ${ETRSRC}
    ${ANDROID_NDK}/sources/android/native_app_glue)

# ETR bakes its data dir at build time (autotools --datadir). On Android data
# comes from APK assets (A4); give it a placeholder so game_config compiles.
target_compile_definitions(penguindash PRIVATE ETR_DATA_DIR="/android_asset")

# Quiet the loudest legacy-code warnings so real errors stand out during bring-up.
target_compile_options(penguindash PRIVATE
    -Wno-narrowing -Wno-deprecated-declarations -fsigned-char)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv2-lib GLESv2)

target_link_libraries(penguindash
    native_app_glue
    ${android-lib} ${egl-lib} ${glesv2-lib} ${log-lib})

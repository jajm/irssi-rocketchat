cmake_minimum_required(VERSION 3.0)

project(irssi-rocketchat C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets>=4.1)
pkg_check_modules(JANSSON REQUIRED jansson>=2.11)

set(IRSSI_INCLUDE_DIR "/usr/include/irssi" CACHE PATH "Irssi development files path")
set(IRSSI_INCLUDE_DIRS
    ${IRSSI_INCLUDE_DIR}
    ${IRSSI_INCLUDE_DIR}/src
    ${IRSSI_INCLUDE_DIR}/src/core
    ${IRSSI_INCLUDE_DIR}/src/fe-common/core)

set(IRSSI_MODULEDIR "${CMAKE_INSTALL_PREFIX}/lib/irssi/modules" CACHE PATH "Irssi modules directory (where to install modules)")

file(GLOB CORE_SRCS src/core/*.c)
add_library(rocketchat_core MODULE ${CORE_SRCS})

target_include_directories(rocketchat_core PRIVATE
    ${IRSSI_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${JANSSON_INCLUDE_DIRS})
target_compile_definitions(rocketchat_core PRIVATE -DUOFF_T_LONG)
target_compile_options(rocketchat_core PRIVATE -Wall -Werror -Wno-error=deprecated-declarations)
target_link_libraries(rocketchat_core
    ${GLIB_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    ${JANSSON_LIBRARIES})

file(GLOB FE_COMMON_SRCS src/fe-common/*.c)
add_library(fe_rocketchat MODULE ${FE_COMMON_SRCS})
target_include_directories(fe_rocketchat PRIVATE
    src/core
    ${IRSSI_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${JANSSON_INCLUDE_DIRS})
target_compile_definitions(fe_rocketchat PRIVATE -DUOFF_T_LONG)
target_compile_options(fe_rocketchat PRIVATE -Wall -Werror -Wno-error=deprecated-declarations)
target_link_libraries(fe_rocketchat
    ${GLIB_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    ${JANSSON_LIBRARIES})

install(TARGETS rocketchat_core fe_rocketchat
    LIBRARY DESTINATION ${IRSSI_MODULEDIR})

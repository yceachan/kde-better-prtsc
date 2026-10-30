cmake_minimum_required(VERSION 3.20)
project(snipaste_kwin_patch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SKIP_RPATH ON)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    message(FATAL_ERROR "This prototype targets Linux x86_64 / Snipaste Qt 6.2.4.")
endif()

set(SNIPASTE_APPDIR "${CMAKE_SOURCE_DIR}/.runtime/squashfs-root" CACHE PATH "Extracted Snipaste AppImage")
set(QT624_HEADERS "$ENV{HOME}/.cache/kde-better-prtsc/qt624/usr/include/x86_64-linux-gnu/qt6" CACHE PATH "Qt 6.2.4 headers")
file(STRINGS "${QT624_HEADERS}/QtCore/qconfig.h" qt_version REGEX "^#define QT_VERSION_STR ")
if(NOT qt_version STREQUAL "#define QT_VERSION_STR \"6.2.4\"")
    message(FATAL_ERROR "Qt 6.2.4 headers required; run scripts/prepare.sh first.")
endif()

foreach(module Core DBus)
    set(qt_library "${SNIPASTE_APPDIR}/usr/lib/libQt6${module}.so.6")
    if(NOT EXISTS "${qt_library}")
        message(FATAL_ERROR "Missing ${qt_library}; run scripts/prepare.sh first.")
    endif()
    add_library(Qt624${module} SHARED IMPORTED)
    set_target_properties(Qt624${module} PROPERTIES
        IMPORTED_LOCATION "${qt_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${QT624_HEADERS};${QT624_HEADERS}/Qt${module}")
endforeach()
set_property(TARGET Qt624DBus PROPERTY INTERFACE_LINK_LIBRARIES Qt624Core)

add_library(snipaste-kwin SHARED src/interpose.cpp)
target_link_libraries(snipaste-kwin PRIVATE Qt624DBus ${CMAKE_DL_LIBS})
target_compile_options(snipaste-kwin PRIVATE -Wall -Wextra -Werror)
target_link_options(snipaste-kwin PRIVATE
    "LINKER:--version-script=${CMAKE_SOURCE_DIR}/src/exports.map" "LINKER:-z,defs")
set_property(TARGET snipaste-kwin APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/src/exports.map")

include(CTest)
if(BUILD_TESTING)
    add_library(echo-transport SHARED tests/echo_transport.cpp)
    target_link_libraries(echo-transport PRIVATE Qt624DBus)
    target_link_options(echo-transport PRIVATE
        "LINKER:--version-script=${CMAKE_SOURCE_DIR}/src/exports.map")
    add_executable(interposition-test tests/interposition.cpp)
    target_link_libraries(interposition-test PRIVATE echo-transport Qt624DBus)
    target_compile_options(interposition-test PRIVATE -fPIC -Wall -Wextra -Werror)
    add_executable(kwin-probe tests/kwin_probe.cpp)
    target_link_libraries(kwin-probe PRIVATE Qt624DBus)
    target_compile_options(kwin-probe PRIVATE -fPIC -Wall -Wextra -Werror)
    set(test_libraries "${CMAKE_BINARY_DIR}:${SNIPASTE_APPDIR}/usr/lib")
    add_test(NAME transport-baseline COMMAND ${CMAKE_COMMAND} -E env
        "LD_LIBRARY_PATH=${test_libraries}" "LD_PRELOAD="
        $<TARGET_FILE:interposition-test> baseline)
    add_test(NAME transport-patched COMMAND ${CMAKE_COMMAND} -E env
        "LD_LIBRARY_PATH=${test_libraries}" "LD_PRELOAD=$<TARGET_FILE:snipaste-kwin>"
        $<TARGET_FILE:interposition-test> patched)
endif()

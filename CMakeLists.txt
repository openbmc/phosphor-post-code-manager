cmake_minimum_required(VERSION 2.8.10 FATAL_ERROR)
project(post-code-manager CXX)
option (
    ENABLE_BIOS_POST_CODE_LOG
    "Enable Bios Post Code Logging"
    OFF
)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(GNUInstallDirs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(DBUS_OBJECT_NAME "xyz/openbmc_project/State/Boot/PostCode0")
set(DBUS_INTF_NAME "xyz.openbmc_project.State.Boot.PostCode")
set(MAX_BOOT_CYCLE_COUNT 100 CACHE STRING "Max boot count Value")
if(NOT MAX_BOOT_CYCLE_COUNT MATCHES "^([1-9][0-9]?|100)$")
    message(FATAL_ERROR "Maximum boot cycle count must be a number from 1 to 100")
endif()

add_definitions(-DDBUS_OBJECT_NAME="/${DBUS_OBJECT_NAME}")
add_definitions(-DDBUS_INTF_NAME="${DBUS_INTF_NAME}")
add_definitions(-DMAX_BOOT_CYCLE_COUNT=${MAX_BOOT_CYCLE_COUNT})

set(SRC_FILES src/post_code.cpp
    src/main.cpp )
set ( SERVICE_FILES
    service_files/xyz.openbmc_project.State.Boot.PostCode.service
    service_files/xyz.openbmc_project.State.Boot.PostCode@.service )

# import sdbusplus
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUSPLUSPLUS sdbusplus REQUIRED)
include_directories(${SDBUSPLUSPLUS_INCLUDE_DIRS})
link_directories(${SDBUSPLUSPLUS_LIBRARY_DIRS})
find_program(SDBUSPLUSPLUS sdbus++)

# import phosphor-logging
find_package(PkgConfig REQUIRED)
pkg_check_modules(LOGGING phosphor-logging REQUIRED)
include_directories(${LOGGING_INCLUDE_DIRS})
link_directories(${LOGGING_LIBRARY_DIRS})

# phosphor-dbus-interfaces
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUSINTERFACE phosphor-dbus-interfaces REQUIRED)
include_directories(${DBUSINTERFACE_INCLUDE_DIRS})
link_directories(${DBUSINTERFACE_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${DBUSINTERFACE_LIBRARIES} )
target_link_libraries(${PROJECT_NAME} "${SDBUSPLUSPLUS_LIBRARIES} -lstdc++fs -lphosphor_dbus")

install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
target_compile_definitions (
    ${PROJECT_NAME} PRIVATE $<$<BOOL:${ENABLE_BIOS_POST_CODE_LOG}>: -DENABLE_BIOS_POST_CODE_LOG>
)
install (FILES ${SERVICE_FILES} DESTINATION /lib/systemd/system/)

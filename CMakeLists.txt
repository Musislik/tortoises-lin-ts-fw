cmake_minimum_required(VERSION 3.20)

project(Tortoises-Lin-TS LANGUAGES C)

# Path to the SysConfig CLI tool
set(SYSCONFIG_DIR "D:/ti/sysconfig_1.26.2" CACHE PATH "Path to SysConfig CLI")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk")
    message(WARNING "The 'sdk' folder was not found. Do not forget to run the 'tools/update_sdk_deps.ps1' script before compiling.")
endif()

file(GLOB_RECURSE DRIVERLIB_SOURCES "sdk/ti/driverlib/*.c")
set(STARTUP_SOURCE "startup/startup_mspm0c110x_ticlang.c")

set(APP_SOURCES
    main.c
    config.c
    filter.c
    telemetry.c
    adc.c
    ${STARTUP_SOURCE}
    ${DRIVERLIB_SOURCES}
)

set(SYSCFG_FILE "Tortoises-Temp-Sensor.syscfg")
set(SYSCFG_OUT_DIR ${CMAKE_BINARY_DIR}/syscfg)
file(MAKE_DIRECTORY ${SYSCFG_OUT_DIR})

set(MSPM0_SDK_DIR "D:/mspm0_sdk_2_10_00_04" CACHE PATH "Path to MSPM0 SDK")

add_custom_command(
    OUTPUT
        ${SYSCFG_OUT_DIR}/ti_msp_dl_config.c
        ${SYSCFG_OUT_DIR}/ti_msp_dl_config.h
        ${SYSCFG_OUT_DIR}/device.opt
        ${SYSCFG_OUT_DIR}/device.cmd.genlibs
    COMMAND ${SYSCONFIG_DIR}/sysconfig_cli.bat
        -s ${MSPM0_SDK_DIR}/.metadata/product.json
        -o ${SYSCFG_OUT_DIR}
        --compiler ticlang
        ${CMAKE_CURRENT_SOURCE_DIR}/${SYSCFG_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SYSCFG_FILE}
    COMMENT "Running SysConfig to generate configuration from Tortoises-Temp-Sensor.syscfg..."
)

list(APPEND APP_SOURCES ${SYSCFG_OUT_DIR}/ti_msp_dl_config.c)

set(CMAKE_EXECUTABLE_SUFFIX ".out")

add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SYSCFG_OUT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/third_party/CMSIS/Core/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    __MSPM0C1103__
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m0plus
    -mfloat-abi=soft
    -mthumb
    -O2
)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m0plus
    -mfloat-abi=soft
    -mthumb
    -Wl,-m,${PROJECT_NAME}.map
    ${CMAKE_CURRENT_SOURCE_DIR}/startup/mspm0c1103.cmd
    -Wl,-i${CMAKE_CURRENT_SOURCE_DIR}/sdk
    -Wl,-i${SYSCFG_OUT_DIR}
    -Wl,-l${SYSCFG_OUT_DIR}/device.cmd.genlibs
)

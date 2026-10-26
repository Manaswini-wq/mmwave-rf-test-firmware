cmake_minimum_required(VERSION 3.16)

# ===========================================================================
# mmWave RF Test Firmware - CMake Build Configuration
# ===========================================================================
project(mmwave_rf_test
    VERSION 1.0.0
    DESCRIPTION "RF Test Firmware for TI mmWave Radar Sensors"
    LANGUAGES C ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Platform configuration
# ---------------------------------------------------------------------------
option(TARGET_DEVICE "Target mmWave device" "IWR6843")
option(BUILD_TESTS "Build unit tests for host" OFF)
option(ENABLE_DEBUG "Enable debug symbols and reduced optimization" OFF)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/iwr6843_r5f.lds)

# ---------------------------------------------------------------------------
# Toolchain flags (when using arm-none-eabi toolchain file)
# ---------------------------------------------------------------------------
if(NOT BUILD_TESTS)
    set(MCU_FLAGS "-mcpu=cortex-r5 -mthumb -mfloat-abi=hard -mfpu=vfpv3-d16")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MCU_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter")

    if(ENABLE_DEBUG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map")
endif()

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
set(RF_TEST_SRCS
    src/rf_tests/tx_power_test.c
    src/rf_tests/rx_sensitivity_test.c
    src/rf_tests/phase_noise_test.c
    src/rf_tests/freq_accuracy_test.c
    src/rf_tests/noise_figure_test.c
    src/rf_tests/tx_evm_test.c
)

set(CALIBRATION_SRCS
    src/calibration/tx_power_flatness_cal.c
    src/calibration/rx_gain_phase_cal.c
    src/calibration/lo_leakage_nulling.c
    src/calibration/inter_channel_cal.c
)

set(DRIVER_SRCS
    src/drivers/synth_driver.c
    src/drivers/pga_driver.c
    src/drivers/if_filter_driver.c
    src/drivers/adc_driver.c
    src/drivers/dac_driver.c
    src/drivers/tx_frontend.c
    src/drivers/rx_frontend.c
)

set(ROM_SRCS
    src/rom/rom_boot_hooks.c
    src/rom/factory_trim.c
)

set(DATA_SRCS
    src/data/capture_engine.c
    src/data/dma_manager.c
)

set(PROTOCOL_SRCS
    src/protocol/uart_cmd_handler.c
    src/protocol/test_protocol.c
)

set(ALL_SRCS
    src/main.c
    ${RF_TEST_SRCS}
    ${CALIBRATION_SRCS}
    ${DRIVER_SRCS}
    ${ROM_SRCS}
    ${DATA_SRCS}
    ${PROTOCOL_SRCS}
)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Firmware target
# ---------------------------------------------------------------------------
if(NOT BUILD_TESTS)
    add_executable(${PROJECT_NAME}.elf ${ALL_SRCS})
    target_include_directories(${PROJECT_NAME}.elf PRIVATE ${INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
        DEVICE_IWR6843
        PLATFORM_R5F
        VERSION="${PROJECT_VERSION}"
    )
    target_link_libraries(${PROJECT_NAME}.elf PRIVATE m gcc)

    # Generate binary output
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
        COMMENT "Generating binary: ${PROJECT_NAME}.bin"
    )

    # Print size after build
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_SIZE} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf
        COMMENT "Firmware size:"
    )
endif()

# ---------------------------------------------------------------------------
# Host-side unit tests
# ---------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_runner
        tests/unit/test_tx_power_cal.c
        tests/unit/test_rx_gain_cal.c
        tests/unit/test_lo_nulling.c
        tests/unit/test_capture_engine.c
        ${RF_TEST_SRCS}
        ${CALIBRATION_SRCS}
        ${DRIVER_SRCS}
        ${DATA_SRCS}
        ${PROTOCOL_SRCS}
    )
    target_include_directories(test_runner PRIVATE ${INCLUDE_DIRS})
    target_compile_definitions(test_runner PRIVATE
        UNIT_TEST
        HOST_BUILD
        DEVICE_IWR6843
    )
    target_link_libraries(test_runner PRIVATE m)

    add_test(NAME unit_tests COMMAND test_runner)
endif()

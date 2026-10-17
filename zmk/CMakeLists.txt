if(CONFIG_USB_DC_B91 OR CONFIG_BT_HCI_B91)
  zephyr_include_directories(include)
endif()

add_subdirectory_ifdef(CONFIG_USB_DC_B91 drivers/usb)
add_subdirectory_ifdef(CONFIG_BT_HCI_B91 drivers/bluetooth)
add_subdirectory_ifdef(CONFIG_BATTERY_B91_ADC drivers/sensor)
add_subdirectory_ifdef(CONFIG_WDT_B91 drivers/watchdog)
add_subdirectory_ifdef(CONFIG_LED_STRIP_B91_SPI drivers/led_strip)

if(CONFIG_BT_HCI_B91)
  zephyr_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/lib/liblt_9518_zephyr.a)
endif()

if(CONFIG_BOOT_DIAG)
  zephyr_library_named(boot_diag)
  zephyr_library_sources(src/boot_diag.c)
endif()

if(CONFIG_IMG_MANAGER AND CONFIG_WDT_B91)
  zephyr_library_named(mcuboot_confirm)
  zephyr_library_sources(src/mcuboot_confirm.c)
endif()

if(CONFIG_POWEROFF)
  zephyr_library_named(poweroff_b91)
  zephyr_library_sources(src/poweroff.c)
endif()

if(CONFIG_FLASH_MGMT)
  zephyr_library_named(flash_mgmt)
  zephyr_library_sources(src/flash_mgmt.c)
endif()

if(CONFIG_UART_MCUMGR AND CONFIG_USB_DC_B91)
  zephyr_library_named(uart_mcumgr_kick)
  zephyr_library_sources(src/uart_mcumgr_kick.c)
endif()

if(CONFIG_USB_DC_B91 AND CONFIG_ZMK_USB)
  zephyr_library_named(usb_cdc_recover)
  zephyr_library_sources(src/usb_cdc_recover.c)
endif()


if(CONFIG_RAINY_RGB)
  zephyr_library_named(rainy_rgb)
  zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
  zephyr_library_sources(
    src/behaviors/behavior_rainy_rgb.c
    src/rainy_rgb/color.c
    src/rainy_rgb/effects.c
    src/rainy_rgb/led_map.c
    src/rainy_rgb/reactive.c
    src/rainy_rgb/engine.c
    src/rainy_rgb/state.c
    src/rainy_rgb/overlay.c
    src/rainy_rgb/zmk_adapter.c
  )
endif()

if(CONFIG_OPENRGB)
  zephyr_library_named(openrgb)
  zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
  zephyr_library_sources(src/openrgb.c)
endif()

if(CONFIG_CRUSH80_KEYMAP_REV)
  zephyr_library_named(keymap_rev)
  zephyr_library_include_directories(${ZEPHYR_BASE}/../zmk-src/app/include)
  zephyr_library_sources(src/keymap_rev.c)
endif()

if(CONFIG_VIA)
  zephyr_library_named(via)
  zephyr_library_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/via
    ${ZEPHYR_BASE}/../zmk-src/app/include
  )
  zephyr_library_sources(src/via/via.c src/via/via_xlat.c src/via/via_macro.c)
endif()

if(CONFIG_ZMK_BLE)
  zephyr_library_named(crush80_ble_name)
  zephyr_library_sources(src/crush80_ble_name.c)
  zephyr_library_include_directories(
    ${ZEPHYR_BASE}/../zmk-src/app/include
  )
endif()

# The module is built from a copy inside the west workspace, which is not a git
# repository, so build.sh passes the version of the real repository.
if(DEFINED ENV{CRUSH80_GIT_VERSION})
  set(BUILD_GIT_VERSION "$ENV{CRUSH80_GIT_VERSION}")
else()
  execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE BUILD_GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
if(NOT BUILD_GIT_VERSION)
  set(BUILD_GIT_VERSION "unknown")
endif()
zephyr_compile_definitions(BUILD_GIT_VERSION="${BUILD_GIT_VERSION}")

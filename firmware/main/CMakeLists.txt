idf_component_register(
    SRCS
        "main.c"
        "cfg/config.c"
        "cfg/config_rx.c"
        "usb/usb_descriptors.c"
        "usb/hid_keyboard.c"
        "usb/raw_hid.c"
        "usb/keymap.c"
        "ui/deck_ui.c"
        "ui/theme.c"
        "ui/feedback.c"
        "ui/imu_gestures.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_timer
        json
        spiffs
)

# Deck configuration and icons live on the "storage" partition and are flashed
# together with the app, so a config change does not need a firmware rebuild.
spiffs_create_partition_image(storage ../../storage FLASH_IN_PROJECT)

/* Fail at link time if firmware overlaps Bluetooth bond storage. */
ASSERT(__flash_binary_end <= __receiver_flash_limit,
       "Firmware overlaps reserved configuration / Bluetooth flash banks")

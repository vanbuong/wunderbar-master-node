/*
 * Place .wb_wifi_cred at absolute flash address 0x0007E000 (MK24 flash @ 0).
 * Included via zephyr_linker_sources(SECTIONS ...).
 */
. = 0x0007E000;
.wb_wifi_cred : {
	KEEP(*(.wb_wifi_cred))
}

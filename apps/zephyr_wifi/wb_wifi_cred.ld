/*
 * Absolute flash slot at 0x0007E000 (shared with FreeRTOS / patch_wifi_cred.py).
 * Must use "section ADDR :" + "> FLASH" — a bare ". = ADDR" without FLASH
 * output is ignored by ld and the section packs at the end of the image.
 */
.wb_wifi_cred 0x0007E000 :
{
	KEEP(*(.wb_wifi_cred))
} > FLASH

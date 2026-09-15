/*
 * USB CDC descriptors for WunderBar master (TinyUSB).
 * SPDX-License-Identifier: MIT
 */

#include "tusb.h"
#include "fsl_device_registers.h"

#include <string.h>

#define USB_VID 0xCafe
#define USB_PID 0x4001
#define USB_BCD 0x0200

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_fs_configuration;
}

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "Relayr",
    "WunderBar FreeRTOS",
    NULL, /* filled from SIM unique ID */
    "CDC ACM",
};

static uint16_t _desc_str[33];

static size_t usb_serial_from_sim(uint16_t *out, size_t max_chars)
{
    static const char hex[] = "0123456789ABCDEF";
    uint32_t uid[4] = {SIM->UIDH, SIM->UIDMH, SIM->UIDML, SIM->UIDL};
    uint8_t *bytes = (uint8_t *)uid;
    size_t n = 16U;
    size_t i;

    if (n > max_chars / 2U) {
        n = max_chars / 2U;
    }

    for (i = 0; i < n; i++) {
        out[i * 2U] = (uint16_t)hex[(bytes[i] >> 4) & 0xFU];
        out[i * 2U + 1U] = (uint16_t)hex[bytes[i] & 0xFU];
    }
    return n * 2U;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t chr_count;

    switch (index) {
    case STRID_LANGID:
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
        break;

    case STRID_SERIAL:
        chr_count = usb_serial_from_sim(_desc_str + 1, 32);
        break;

    default:
        if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }
        {
            const char *str = string_desc_arr[index];
            size_t i;
            chr_count = strlen(str);
            if (chr_count > 32) {
                chr_count = 32;
            }
            for (i = 0; i < chr_count; i++) {
                _desc_str[1 + i] = (uint16_t)str[i];
            }
        }
        break;
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}

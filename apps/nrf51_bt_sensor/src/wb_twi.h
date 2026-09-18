/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef WB_TWI_H
#define WB_TWI_H

#include <stdint.h>
#include <stdbool.h>

uint32_t wb_twi_init(void);
uint32_t wb_twi_write(uint8_t addr7, const uint8_t *data, uint8_t len);
uint32_t wb_twi_write_reg(uint8_t addr7, uint8_t reg, uint8_t val);
uint32_t wb_twi_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len);

#endif /* WB_TWI_H */

/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Early board bring-up aligned with Zephyr's k6x soc_early_init_hook():
 *   - release PMC I/O isolation (ACKISO)
 *   - disable the Kinetis SYSMPU (required for USB BDT RAM access)
 * then clocks, pins, and USB FS clock when the USB CDC image is built.
 */

#include "board.h"
#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_device_registers.h"

#ifndef LOG_BACKEND_RTT
#include "tusb.h"
#endif

#ifndef LOG_BACKEND_RTT
void BOARD_InitUsb(void)
{
    /* Unlock and enable the on-chip USB voltage regulator. */
    SIM->SOPT1CFG |= SIM_SOPT1CFG_URWE_MASK;
    SIM->SOPT1 |= SIM_SOPT1_USBREGEN_MASK;

    /* Crystal-less 48 MHz USB clock (IRC48M + CLK_RECOVER). TinyUSB's KHCI
     * driver restores CLK_RECOVER after a USB reset. */
    (void)CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcIrc48M, 48000000U);

    /* Must be numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (2)
     * so the TinyUSB ISR can call FreeRTOS FromISR APIs. */
    NVIC_SetPriority(USB0_IRQn, 3);
}
#endif

void BOARD_InitHardware(void)
{
    uint32_t temp_reg;

    /* Release I/O power hold so GPIO (LED) can actually drive pins. */
    PMC->REGSC |= PMC_REGSC_ACKISO_MASK;

    /*
     * K64/K24 have NXP SYSMPU, not the optional ARM PMSAv7 MPU. Leave it
     * disabled so USB (and other bus masters) can reach SRAM.
     */
    temp_reg = SYSMPU->CESR;
    temp_reg &= ~SYSMPU_CESR_VLD_MASK;
    temp_reg |= SYSMPU_CESR_SPERR_MASK;
    SYSMPU->CESR = temp_reg;

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
#ifndef LOG_BACKEND_RTT
    BOARD_InitUsb();
#endif
}

#ifndef LOG_BACKEND_RTT
void USB0_IRQHandler(void)
{
    tud_int_handler(0);
}
#endif

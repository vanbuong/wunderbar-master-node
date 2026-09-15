/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * WunderBar master clock configuration for MK24FN1M0VDC12.
 *
 * Hardware:
 *   OSC0 : 12 MHz crystal (Y1) on EXTAL0/XTAL0, external C17/C18 = 12 pF
 *   RTC  : 32.768 kHz crystal (X8) on EXTAL32/XTAL32, external C104/C105 = 12 pF
 *
 * Target (MCG PEE):
 *   Core / System : 120 MHz
 *   Bus           :  60 MHz
 *   FlexBus       :  40 MHz
 *   Flash         :  24 MHz
 *
 * PLL math: 12 MHz / 3 * 30 = 120 MHz
 *   PRDIV register = 2  (divide by PRDIV+1 = 3)
 *   VDIV  register = 6  (multiply by VDIV+24 = 30)
 *
 * Correct SDK package: MCUXpresso SDK for FRDM-K64F / MK64FN1M0xxx12
 * (SDK 2.x also lists MK24FN1M0VDC12 in the same device family).
 * Download from https://mcuxpresso.nxp.com/en/builder  (board: FRDM-K64F)
 * or use a legacy SDK zip that contains devices/MK64F12 (or MK24F12).
 */

#include "fsl_smc.h"
#include "clock_config.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define MCG_IRCLK_DISABLE                 0U
#define MCG_PLL_DISABLE                   0U
#define OSC_CAP0P                         0U
#define OSC_ER_CLK_DISABLE                0U
#define SIM_OSC32KSEL_RTC32KCLK_CLK       2U
#define SIM_PLLFLLSEL_MCGPLLCLK_CLK       1U
#define SIM_PLLFLLSEL_IRC48MCLK_CLK       3U
#define RTC_OSC_CAP_LOAD_0PF              0U
#define RTC_RTC32KCLK_PERIPHERALS_ENABLED 1U

/*******************************************************************************
 * Variables
 ******************************************************************************/
extern uint32_t SystemCoreClock;

/*******************************************************************************
 * Code
 ******************************************************************************/
static void CLOCK_CONFIG_SetFllExtRefDiv(uint8_t frdiv)
{
    MCG->C1 = ((MCG->C1 & ~MCG_C1_FRDIV_MASK) | MCG_C1_FRDIV(frdiv));
}

static void CLOCK_CONFIG_SetRtcClock(uint32_t capLoad, uint8_t enablePeriphs)
{
    /* . The RTC oscillator uses the external 32.768 kHz crystal (X8).
     * On-chip load caps are left at 0 pF because C104/C105 are already 12 pF. */
    RTC->CR = (RTC->CR & ~(RTC_CR_SC2P_MASK | RTC_CR_SC4P_MASK | RTC_CR_SC8P_MASK |
                           RTC_CR_SC16P_MASK)) |
              capLoad;
    RTC->CR |= RTC_CR_OSCE_MASK;
    if (enablePeriphs != 0U) {
        /* Keep RTC32KCLK available to peripherals via SIM_SOPT1[OSC32KSEL]. */
        (void)enablePeriphs;
    }
}

void BOARD_InitBootClocks(void)
{
    BOARD_BootClockRUN();
}

const mcg_config_t mcgConfig_BOARD_BootClockRUN = {
    .mcgMode = kMCG_ModePEE,
    .irclkEnableMode = kMCG_IrclkEnable,
    .ircs = kMCG_IrcFast,
    .fcrdiv = 0x1U, /* Fast IRC / 2 */
    .frdiv = 0x0U,  /* FLL FRDIV (unused in PEE system path) */
    .drs = kMCG_DrsLow,
    .dmx32 = kMCG_Dmx32Default,
    .oscsel = kMCG_OscselOsc,
    .pll0Config =
        {
            .enableMode = MCG_PLL_DISABLE,
            .prdiv = 0x2U, /* /3  -> 4 MHz PLL ref */
            .vdiv = 0x6U,  /* x30 -> 120 MHz VCO/out */
        },
};

const sim_clock_config_t simConfig_BOARD_BootClockRUN = {
    .pllFllSel = SIM_PLLFLLSEL_MCGPLLCLK_CLK,
    .er32kSrc = SIM_OSC32KSEL_RTC32KCLK_CLK,
    /* OUTDIV1=/1, OUTDIV2=/2, OUTDIV3=/3, OUTDIV4=/5 */
    .clkdiv1 = 0x1240000U,
};

const osc_config_t oscConfig_BOARD_BootClockRUN = {
    .freq = BOARD_XTAL0_CLK_HZ,
    /* External 12 pF capacitors already fitted; do not add on-chip load. */
    .capLoad = (OSC_CAP0P),
    .workMode = kOSC_ModeOscLowPower,
    .oscerConfig =
        {
            .enableMode = kOSC_ErClkEnable,
        },
};

void BOARD_BootClockRUN(void)
{
    CLOCK_SetSimSafeDivs();

    /* Enable 32.768 kHz RTC oscillator first (safe while still on FEI). */
    CLOCK_CONFIG_SetRtcClock(RTC_OSC_CAP_LOAD_0PF, RTC_RTC32KCLK_PERIPHERALS_ENABLED);

    CLOCK_InitOsc0(&oscConfig_BOARD_BootClockRUN);
    CLOCK_SetXtal0Freq(oscConfig_BOARD_BootClockRUN.freq);
#if defined(CLOCK_SetXtal32Freq)
    CLOCK_SetXtal32Freq(BOARD_XTAL32K_CLK_HZ);
#endif

    CLOCK_SetInternalRefClkConfig(mcgConfig_BOARD_BootClockRUN.irclkEnableMode,
                                  mcgConfig_BOARD_BootClockRUN.ircs,
                                  mcgConfig_BOARD_BootClockRUN.fcrdiv);
    CLOCK_CONFIG_SetFllExtRefDiv(mcgConfig_BOARD_BootClockRUN.frdiv);

    CLOCK_BootToPeeMode(mcgConfig_BOARD_BootClockRUN.oscsel, kMCG_PllClkSelPll0,
                        &mcgConfig_BOARD_BootClockRUN.pll0Config);

    CLOCK_SetSimConfig(&simConfig_BOARD_BootClockRUN);
    SystemCoreClock = BOARD_BOOTCLOCKRUN_CORE_CLOCK;
}

const mcg_config_t mcgConfig_BOARD_BootClockVLPR = {
    .mcgMode = kMCG_ModeBLPI,
    .irclkEnableMode = MCG_IRCLK_DISABLE,
    .ircs = kMCG_IrcFast,
    .fcrdiv = 0x0U,
    .frdiv = 0x0U,
    .drs = kMCG_DrsLow,
    .dmx32 = kMCG_Dmx32Default,
    .oscsel = kMCG_OscselOsc,
    .pll0Config =
        {
            .enableMode = MCG_PLL_DISABLE,
            .prdiv = 0x0U,
            .vdiv = 0x0U,
        },
};

const sim_clock_config_t simConfig_BOARD_BootClockVLPR = {
    .pllFllSel = SIM_PLLFLLSEL_IRC48MCLK_CLK,
    .er32kSrc = SIM_OSC32KSEL_RTC32KCLK_CLK,
    .clkdiv1 = 0x40000U,
};

const osc_config_t oscConfig_BOARD_BootClockVLPR = {
    .freq = 0U,
    .capLoad = (OSC_CAP0P),
    .workMode = kOSC_ModeExt,
    .oscerConfig =
        {
            .enableMode = OSC_ER_CLK_DISABLE,
        },
};

void BOARD_BootClockVLPR(void)
{
    CLOCK_SetSimSafeDivs();
    CLOCK_BootToBlpiMode(mcgConfig_BOARD_BootClockVLPR.fcrdiv, mcgConfig_BOARD_BootClockVLPR.ircs,
                         mcgConfig_BOARD_BootClockVLPR.irclkEnableMode);
    CLOCK_SetSimConfig(&simConfig_BOARD_BootClockVLPR);
    SMC_SetPowerModeProtection(SMC, kSMC_AllowPowerModeAll);
#if (defined(FSL_FEATURE_SMC_HAS_LPWUI) && FSL_FEATURE_SMC_HAS_LPWUI)
    SMC_SetPowerModeVlpr(SMC, false);
#else
    SMC_SetPowerModeVlpr(SMC);
#endif
    while (SMC_GetPowerModeState(SMC) != kSMC_PowerStateVlpr) {
    }
    SystemCoreClock = BOARD_BOOTCLOCKVLPR_CORE_CLOCK;
}

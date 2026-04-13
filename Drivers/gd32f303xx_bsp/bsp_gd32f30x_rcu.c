/**
 * @file bsp_gd32f30x_rcu.c
 * @author your name (you@domain.com)
 * @brief reset and clock unit
 * @version 0.4
 * @date 2025-07-29
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "gd32f30x_rcu.h"
#include "gd32f30x_pmu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_misc.h"
#include "gd32f30x_exti.h"
#include  "bsp_gd32f30x_rcu.h"
#include "bsp_assert.h"

void bsp_reinit_rcu(enum bsp_sys_clock_value sys_clock_value)
{
    switch (sys_clock_value)
    {
    case SYS_CLOCK_120M:
        break;
    case SYS_CLOCK_108M:
        break;
    case SYS_CLOCK_72M:
        break;
    case SYS_CLOCK_48M:
        break;
    case SYS_CLOCK_8M:
        break;
    default:
        break;
    }
}

/*!
    \brief      configure the system clock to 48M by PLL which selects HXTAL(8M) as its clock source
    \param[in]  none
    \param[out] none
    \retval     none
*/
//static void system_clock_48m_hxtal(void)
//{
//    uint32_t timeout = 0U;
//    uint32_t stab_flag = 0U;

//    /* enable HXTAL */
//    RCU_CTL |= RCU_CTL_HXTALEN;

//    /* wait until HXTAL is stable or the startup time is longer than HXTAL_STARTUP_TIMEOUT */
//    do{
//        timeout++;
//        stab_flag = (RCU_CTL & RCU_CTL_HXTALSTB);
//    }while((0U == stab_flag) && (HXTAL_STARTUP_TIMEOUT != timeout));

//    /* if fail */
//    bsp_assert(0U != (RCU_CTL & RCU_CTL_HXTALSTB));

//    RCU_APB1EN |= RCU_APB1EN_PMUEN;
//    PMU_CTL |= PMU_CTL_LDOVS;

//    /* HXTAL is stable */
//    /* AHB = SYSCLK */
//    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
//    /* APB2 = AHB/1 */
//    RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;
//    /* APB1 = AHB/2 */
//    RCU_CFG0 |= RCU_APB1_CKAHB_DIV2;

//#if (defined(GD32F30X_HD) || defined(GD32F30X_XD))
//    /* select HXTAL/2 as clock source */
//    RCU_CFG0 &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PREDV0);
//    RCU_CFG0 |= (RCU_PLLSRC_HXTAL_IRC48M | RCU_CFG0_PREDV0);

//    /* CK_PLL = (CK_HXTAL/2) * 12 = 48 MHz */
//    RCU_CFG0 &= ~(RCU_CFG0_PLLMF | RCU_CFG0_PLLMF_4 | RCU_CFG0_PLLMF_5);
//    RCU_CFG0 |= RCU_PLL_MUL12;

//#elif defined(GD32F30X_CL)
//    /* CK_PLL = (CK_PREDIV0) * 12 = 48 MHz */ 
//    RCU_CFG0 &= ~(RCU_CFG0_PLLMF | RCU_CFG0_PLLMF_4 | RCU_CFG0_PLLMF_5);
//    RCU_CFG0 |= (RCU_PLLSRC_HXTAL_IRC48M | RCU_PLL_MUL12);

//    /* CK_PREDIV0 = (CK_HXTAL)/5 *8 /10 = 4 MHz */ 
//    RCU_CFG1 &= ~(RCU_CFG1_PLLPRESEL | RCU_CFG1_PREDV0SEL | RCU_CFG1_PLL1MF | RCU_CFG1_PREDV1 | RCU_CFG1_PREDV0);
//    RCU_CFG1 |= (RCU_PLLPRESRC_HXTAL | RCU_PREDV0SRC_CKPLL1 | RCU_PLL1_MUL8 | RCU_PREDV1_DIV3 | RCU_PREDV0_DIV8);

//    /* enable PLL1 */
//    RCU_CTL |= RCU_CTL_PLL1EN;
//    /* wait till PLL1 is ready */
//    while((RCU_CTL & RCU_CTL_PLL1STB) == 0){
//    }
//#endif /* GD32F30X_HD and GD32F30X_XD */

//    /* enable PLL */
//    RCU_CTL |= RCU_CTL_PLLEN;

//    /* wait until PLL is stable */
//    while(0U == (RCU_CTL & RCU_CTL_PLLSTB)){
//    }

//    /* enable the high-drive to extend the clock frequency to 120 MHz */
//    PMU_CTL |= PMU_CTL_HDEN;
//    while(0U == (PMU_CS & PMU_CS_HDRF)){
//    }

//    /* select the high-drive mode */
//    PMU_CTL |= PMU_CTL_HDS;
//    while(0U == (PMU_CS & PMU_CS_HDSRF)){
//    }

//    /* select PLL as system clock */
//    RCU_CFG0 &= ~RCU_CFG0_SCS;
//    RCU_CFG0 |= RCU_CKSYSSRC_PLL;

//    /* wait until PLL is selected as system clock */
//    while(0U == (RCU_CFG0 & RCU_SCSS_PLL)){
//    }
//}

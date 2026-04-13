/*!
    \file  gd32f30x_it.c
    \brief interrupt service routines

    \version 2021-03-23, V2.0.0, demo for GD32F30x
*/

/*
    Copyright (c) 2021, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f30x_it.h"
#include "bsp_gd32f30x_hhctl.h"
#include "bsp_i2c_gd32f30x.h"
#include "bsp_gd32f30x_rtc.h"
#include "bsp_gd32f30x_rcu.h"
#include "bsp_gd32f30x_hhctl_funkey.h"
#include "bsp_atbm6431.h"

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{

    //		__set_FAULTMASK(1);
    //		NVIC_SystemReset();
    /* if Hard Fault exception occurs, go to infinite loop */
    while (1)
    {
        __NOP();
        return;
    }
}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
    /* if Memory Manage exception occurs, go to infinite loop */
    while (1)
    {
    }
}

/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
    /* if Bus Fault exception occurs, go to infinite loop */
    while (1)
    {
    }
}

/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
    /* if Usage Fault exception occurs, go to infinite loop */
    while (1)
    {
    }
}

/*!
    \brief      this function handles SVC exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SVC_Handler(void)
{
}

/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
}

/*!
    \brief      this function handles PendSV exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void PendSV_Handler(void)
{
}

/*!
    \brief      this function handles SysTick exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SysTick_Handler(void)
{
    bsp_SysTimDelayCallback();
}

void TIMER3_IRQHandler(void)
{
//    bsp_KnobTimIRQCallback();
}

void TIMER4_IRQHandler(void)
{
    if (RESET != timer_interrupt_flag_get(TIMER4, TIMER_INT_FLAG_UP))
    {
        timer_interrupt_flag_clear(TIMER4, TIMER_INT_FLAG_UP);
#ifdef WIFI_H02W
       bsp_ModbusUartTimIRQCallback(&hMBWifiMaster);
#else
        bsp_ModbusUartTimIRQCallback(&hMBMaster);
#endif
    }
}

void USART2_IRQHandler(void)
{
    bsp_atbm6431_isr();
}

void USART1_IRQHandler(void)
{
    if (RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE))
    {
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);

        bsp_ModbusUartRxIRQCallback(&hMBMaster, (uint8_t)usart_data_receive(USART1));
    }

    if (RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_TC))
    {
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_TC);

        MB_RxState(); /* ÊÍ·Å485×ÜÏß */
    }
}

void I2C0_EV_IRQHandler(void)
{
    hi2c0.i2c_ops.isr(&hi2c0);
}

/*!
    \brief      this function handles external lines 10 to 15 interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI10_15_IRQHandler(void)
{
    if (RESET != exti_interrupt_flag_get(EXTI_15))
    {
        exti_interrupt_flag_clear(EXTI_15);
    }
    exti_interrupt_flag_clear(EXTI_15);
}

/**
 * @file bsp_gd32f30x_adc.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.5
 * @date 2025-08-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "stdint.h"
#include "gd32f30x_adc.h"
#include "bsp_gd32f30x_adc.h"
#include "bsp_gd32f303xx_systick.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_dma.h"
#include "bsp_drdusb.h"
static uint32_t adc_value[4] = {1, 1, 1, 1};
static uint8_t adc_init_flag = 0;
static uint32_t Bat1Volt = 0;

static void adc_dma_init(void)
{
    rcu_periph_clock_enable(RCU_DMA0);    
    /* ADC_DMA_channel configuration */
    dma_parameter_struct dma_data_parameter;
    
    /* ADC_DMA_channel deinit */
    dma_deinit(DMA0, DMA_CH0);
    
    /* initialize DMA single data mode */
    dma_data_parameter.periph_addr = (uint32_t)(&ADC_RDATA(ADC0));
    dma_data_parameter.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr = (uint32_t)(adc_value);
    dma_data_parameter.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_32BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_32BIT;
    dma_data_parameter.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number = 4;
    dma_data_parameter.priority = DMA_PRIORITY_HIGH;  
    dma_init(DMA0, DMA_CH0, &dma_data_parameter);

    dma_circulation_enable(DMA0, DMA_CH0);
    /* enable DMA channel */
    dma_channel_enable(DMA0, DMA_CH0);   
}

/**
 * @brief 配置为单次扫描模式，软件触发
 * 
 */
void bsp_adc_init(void)
{       
	/* Enable ADC I/O clock */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_0);   /* BAT3V IO */
    gpio_init(GPIOC, GPIO_MODE_AIN, GPIO_OSPEED_MAX, GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5); /* BAT3V7|CC2|CC1 */

    adc_dma_init();

    rcu_periph_clock_enable(RCU_ADC0);
    rcu_adc_clock_config(RCU_CKADC_CKAHB_DIV5); /* 120Mhz/5 = 24Mhz */
    /* reset ADC */
    adc_deinit(ADC0);
    adc_mode_config(ADC_MODE_FREE);
    /* ADC scan mode function enable */
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    /* 配置ADC采样精度 */
    adc_resolution_config(ADC0, ADC_RESOLUTION_12B);
    /* 配置ADC数据对齐方式 */
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    /* config ADC Channel */
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 4);
    adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_10, ADC_SAMPLETIME_239POINT5);   /* PC0, BAT3V7 */
    adc_regular_channel_config(ADC0, 1, ADC_CHANNEL_0,  ADC_SAMPLETIME_239POINT5);   /* PA0, BAT3V */
    adc_regular_channel_config(ADC0, 2, ADC_CHANNEL_15, ADC_SAMPLETIME_55POINT5);   /* PC5, CC1 */
    adc_regular_channel_config(ADC0, 3, ADC_CHANNEL_14, ADC_SAMPLETIME_55POINT5);   /* PC4, CC2 */
    /* ADC trigger config */
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_2_EXTTRIG_REGULAR_NONE);
	/* ADC over sample */
	adc_oversample_mode_config(ADC0, ADC_OVERSAMPLING_ALL_CONVERT, ADC_OVERSAMPLING_SHIFT_7B, ADC_OVERSAMPLING_RATIO_MUL128);
	adc_oversample_mode_enable(ADC0);
    /* ADC calibration */
    adc_enable(ADC0);
    bsp_delay(1);
    adc_calibration_enable(ADC0);
    adc_dma_mode_enable(ADC0);
	adc_init_flag = 1;
}

#define BAT1_RATIO 2.0f
#define BAT2_RATIO 2.0f

/**
 * @brief 读取3.7V锂电池电量
 * 
 * @return uint32_t 返回值需除以4095.0得到标准电压值
 */
uint32_t bsp_readBat1Volt(void)
{
    return (uint32_t)(Bat1Volt*3.29*BAT1_RATIO);
}

/*                              0     1     10    20    30    40    50    60    70    80    90    100 */
// static float Bat1_SOCLUT[] = {3.30, 3.40, 3.65, 3.71, 3.73, 3.76, 3.79, 3.84, 3.90, 3.97, 4.08, 4.20};
// static float Bat1_SOCLUT[] = {3.30, 3.40, 3.65, 3.71, 3.73, 3.76, 3.79, 3.84, 3.90, 3.97, 4.08, 4.20};
// static float Bat1_SOCLUT[12] = {3.10, 3.11, 3.20, 3.30, 3.40, 3.50, 3.60, 3.70, 3.80, 3.90, 4.00, 4.10}; /* before 20250819 */
/*                              0     5     10*    20    30    40    50    60*    70    80    90    100* */ 
static float Bat1_SOCLUT[12] = {3.20, 3.50, 3.60, 3.64, 3.68, 3.72, 3.76, 3.80, 3.88, 3.96, 4.04, 4.12};    /* 20250819 */
/**
 * @brief 返回电池电量百分比
 * 
 * @return uint8_t 未做拟合，仅返回区间估计值
 */
uint8_t bsp_readBat1VoltPercent(void)
{
	uint8_t i;
    float Vbat = bsp_readBat1Volt()/4095.0;
    for (i = 0; i < 12; i++) {
        if (Vbat < Bat1_SOCLUT[i]) {
            break;
        } 
    }

    if (i == 0 || i == 1) {
        return 0;
    } else if (i == 2) {
        return 5;
    } else if (i == 12) { 
        return 100;
    } else {
        return (i-2)*10;
    }
}

#define SAMPLE_TIMES 30
static uint32_t sample_volt1[SAMPLE_TIMES];
static uint32_t Bat1VoltChargeLatch = 0;
static void Bat1VoltFilter(void)
{
    static uint8_t v_cnt = 0;
    static uint8_t w_cnt = 0;
    static uint32_t last_stand_volt1 = 0;
    static uint32_t stand_volt1 = 0;
    static uint8_t stand_cnt = 0;
    static uint8_t flag_cmp = 0;

    if (w_cnt >= SAMPLE_TIMES) {
        w_cnt = 0;
        if (flag_cmp) {
            if (v_cnt >= SAMPLE_TIMES/2) {
                if (hdrdusb.vbus_valid) {
                    if (Bat1VoltChargeLatch == 0) {
                        Bat1VoltChargeLatch = adc_value[0];
						stand_cnt = 0;
                    } else {
                        if (Bat1Volt == 0) {
                            Bat1Volt = last_stand_volt1;
                        } else {
                            Bat1Volt += last_stand_volt1 - Bat1VoltChargeLatch;
                        }
						Bat1VoltChargeLatch = last_stand_volt1;
                    }
                } else {
                    if (Bat1VoltChargeLatch != 0) {
                        Bat1VoltChargeLatch = 0;
						stand_cnt = 0;
                    } else {
                        Bat1Volt = last_stand_volt1;
                    }
                }
            } 
        }
        if (stand_cnt != 0) {
            last_stand_volt1 = stand_volt1;
            v_cnt = 0;
            flag_cmp = 1;
        } else {
            flag_cmp = 0;
        }
        stand_cnt = 0;
    }
    if (flag_cmp) {
        if (sample_volt1[w_cnt] == last_stand_volt1) {
            v_cnt++;
        }
    }
    sample_volt1[w_cnt] = adc_value[0];
    if (stand_cnt == 0) {
        stand_volt1 = adc_value[0];
        stand_cnt++;
    } else if (stand_volt1 == adc_value[0]) {
        stand_cnt++;
    } else {
        stand_cnt--;
    }
    w_cnt++;

}

/**
* @brief 读取CR2032纽扣电池电量
* 
* @return uint8_t 
*/
uint32_t bsp_readBat2Volt(void)
{
    return (uint32_t)(adc_value[1]*3.29*BAT2_RATIO);
}

uint32_t bsp_GetCC1State(void)
{
    return (uint32_t)(adc_value[2]*3.29);
}

uint32_t bsp_GetCC2State(void)
{
    return (uint32_t)(adc_value[3]*3.29);
}

void bsp_adc_run_per10ms(void)
{
	if (adc_init_flag) {
		adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
		Bat1VoltFilter();
	}
}

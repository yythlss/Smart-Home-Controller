#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/uart.h>
#include <esp_adc/adc_oneshot.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 如果使用 Duplex I2S 模式，请注释下面一行
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif


#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_47
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_40
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39

// 删除原来的 OLED 屏幕 GPIO 定义（原 GPIO41=SDA, GPIO42=SCL）
// 这两脚现用于 TJC 串口屏的 UART2 通信

// ===== TJC 串口屏 UART 定义 =====
#define TJC_UART_PORT          UART_NUM_2
#define TJC_UART_TX_PIN        GPIO_NUM_41   // 接屏幕 RX
#define TJC_UART_RX_PIN        GPIO_NUM_42   // 接屏幕 TX
#define TJC_UART_BAUD_RATE     9600

// ===== 传感器引脚定义 =====
// 注意：DHT11 原计划 GPIO4 被音频 I2S 占用，改为 GPIO18
#define DHT11_GPIO              GPIO_NUM_18   // DHT11 温湿度传感器 DATA
#define MQ135_ADC_UNIT          ADC_UNIT_1    // MQ135 ADC 单元
#define MQ135_ADC_CHANNEL       ADC_CHANNEL_0 // MQ135 接 GPIO1 → ADC1_CH0

// GL5528 light sensor module: power it from 3.3V and connect AO to GPIO2.
// Do not connect a 5V analog output to this ADC input.
#define AMBIENT_LIGHT_ADC_UNIT       ADC_UNIT_1
#define AMBIENT_LIGHT_ADC_CHANNEL    ADC_CHANNEL_1 // GPIO2 -> ADC1_CH1
// 本模块实测为遮光时 AO 原始值升高、照亮时原始值降低。
// 现场重新标定时，按“遮光值填 DARK、照亮值填 BRIGHT”的语义填写即可。
#define AMBIENT_LIGHT_DARK_RAW       3300
#define AMBIENT_LIGHT_BRIGHT_RAW     300

// HLK-LD2450 radar owns UART1. UART2 remains reserved for the TJC HMI.
#define LD2450_UART_PORT             UART_NUM_1
#define LD2450_UART_RX_PIN           GPIO_NUM_11 // Connect to LD2450 TX
#define LD2450_UART_TX_PIN           GPIO_NUM_12 // Connect to LD2450 RX
#define LD2450_UART_BAUD_RATE        256000
#define LD2450_POLL_INTERVAL_MS      100

// ===== 智能家居执行器引脚定义 =====
#define SMART_HOME_PURIFIER_LED_GPIO    GPIO_NUM_13
#define SMART_HOME_HUMIDIFIER_LED_GPIO  GPIO_NUM_14
#define SMART_HOME_FRESH_AIR_SERVO_GPIO GPIO_NUM_21

// ===== 智能家居 PWM 定义 =====
#define SMART_HOME_LEDC_SPEED_MODE      LEDC_LOW_SPEED_MODE
#define SMART_HOME_LED_TIMER            LEDC_TIMER_1
#define SMART_HOME_SERVO_TIMER          LEDC_TIMER_2
#define SMART_HOME_PURIFIER_LED_CHANNEL LEDC_CHANNEL_0
#define SMART_HOME_HUMIDIFIER_CHANNEL   LEDC_CHANNEL_1
#define SMART_HOME_SERVO_CHANNEL        LEDC_CHANNEL_2
#define SMART_HOME_LED_PWM_HZ           5000
#define SMART_HOME_SERVO_PWM_HZ         50

// 传感器采集间隔（毫秒）
#define SENSOR_READ_INTERVAL_MS 5000

// Cached DHT11 values are only considered valid for this duration.
#define DHT11_CACHE_MAX_AGE_MS 30000

// Optional local HTTP API protection. Leave empty for backwards-compatible
// trusted-LAN demonstrations; set a non-empty value before untrusted use.
#define SMART_HOME_API_TOKEN ""
#define SMART_HOME_CORS_ORIGIN "*"

#endif // _BOARD_CONFIG_H_

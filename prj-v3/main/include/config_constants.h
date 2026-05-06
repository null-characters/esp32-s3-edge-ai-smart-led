/**
 * @file config_constants.h
 * @brief 统一配置常量定义 - 集中管理所有硬编码参数
 *
 * 遵循 Karpathy Guidelines:
 * - 所有魔法数字必须有命名常量
 * - 配置参数集中管理，便于调试和维护
 */

#ifndef CONFIG_CONSTANTS_H
#define CONFIG_CONSTANTS_H

#include <math.h>

/* ============================================================================
 * 色温与亮度常量
 * ============================================================================ */
#define COLOR_TEMP_WARM_K           2700    /*!< 暖色温 (K) */
#define COLOR_TEMP_COOL_K           6500    /*!< 冷色温 (K) */
#define COLOR_TEMP_DEFAULT_K        4000    /*!< 默认色温 (K) */
#define COLOR_TEMP_STEP_K           300     /*!< 色温步进 (K) */

#define BRIGHTNESS_MIN              0       /*!< 最小亮度 (%) */
#define BRIGHTNESS_MAX              100     /*!< 最大亮度 (%) */
#define BRIGHTNESS_DEFAULT          50      /*!< 默认亮度 (%) */
#define BRIGHTNESS_STEP             10      /*!< 亮度步进 (%) */

/* ============================================================================
 * 音频处理常量
 * ============================================================================ */
#define AFE_TASK_STACK_SIZE         (16 * 1024)  /*!< AFE 任务栈大小 */
#define AFE_TASK_PRIORITY           5             /*!< AFE 任务优先级 */

#define WAKENET_THRESHOLD_DEFAULT   0.5f         /*!< 唤醒词默认阈值 */
#define WAKENET_THRESHOLD_MIN       0.4f         /*!< 唤醒词最小阈值 */
#define WAKENET_THRESHOLD_MAX       0.9999f      /*!< 唤醒词最大阈值 */

#define CMD_TIMEOUT_DEFAULT_MS      5000         /*!< 命令超时默认值 (ms) */
#define CMD_TIMEOUT_MIN_MS          1000         /*!< 命令超时最小值 (ms) */
#define CMD_TIMEOUT_MAX_MS          30000        /*!< 命令超时最大值 (ms) */

#define VOICE_CONFIDENCE_THRESHOLD  0.5f         /*!< 语音置信度阈值 */

/* Mel 频率范围 */
#define MEL_LOW_FREQ_HZ             0.0f         /*!< Mel 低频 (Hz) */
#define MEL_HIGH_FREQ_HZ            8000.0f      /*!< Mel 高频 (Hz) */
#define LOG_FLOOR_VALUE             1e-10f       /*!< 对数下限值 */

/* ============================================================================
 * 任务与超时常量
 * ============================================================================ */
#define DEFAULT_MUTEX_TIMEOUT_MS    1000         /*!< 默认 mutex 超时 (ms) */
#define DEFAULT_QUEUE_TIMEOUT_MS    1000         /*!< 默认队列超时 (ms) */
#define TASK_STOP_WAIT_MS           200          /*!< 任务停止等待 (ms) */

/* ============================================================================
 * 雷达与环境检测常量
 * ============================================================================ */
#define RADAR_EXPIRE_MULTIPLIER     2            /*!< 雷达过期倍数 */
#define MS_PER_MINUTE               60000        /*!< 每分钟毫秒数 */

#define RADAR_UART_BAUD_RATE        256000       /*!< 雷达默认波特率 */
#define RADAR_UART_TX_PIN           8            /*!< 雷达 TX 引脚 */
#define RADAR_UART_RX_PIN           9            /*!< 雷达 RX 引脚 */

/* 归一化参数 */
#define RADAR_DISTANCE_MIN_M        0.0f         /*!< 最小距离 (m) */
#define RADAR_DISTANCE_MAX_M        6.0f         /*!< 最大距离 (m) */
#define RADAR_ENERGY_MIN            0.0f         /*!< 最小能量 */
#define RADAR_ENERGY_MAX            100.0f       /*!< 最大能量 */

/* ============================================================================
 * WiFi 常量
 * ============================================================================ */
#define WIFI_MAX_RETRY              5            /*!< WiFi 最大重试次数 */
#define WIFI_RETRY_DELAY_MS         1000         /*!< WiFi 重试延迟 (ms) */

/* ============================================================================
 * LED 状态常量
 * ============================================================================ */
#define STATUS_LED_TASK_STACK_SIZE  2048         /*!< 状态 LED 任务栈 */
#define STATUS_LED_TASK_PRIORITY    5            /*!< 状态 LED 任务优先级 */
#define STATUS_LED_UPDATE_INTERVAL_MS 20         /*!< 状态 LED 更新间隔 (ms) */

/* ============================================================================
 * 模型加载常量
 * ============================================================================ */
#define MODEL_CHUNK_SIZE            4096         /*!< 模型分块大小 */

/* ============================================================================
 * TTL 租约常量
 * ============================================================================ */
#define TTL_DEFAULT_MS              (2 * 60 * 60 * 1000)  /*!< 默认租约 2 小时 */
#define TTL_SHORT_MS                (30 * 60 * 1000)      /*!< 短租约 30 分钟 */
#define TTL_LONG_MS                 (4 * 60 * 60 * 1000)  /*!< 长租约 4 小时 */
#define TTL_ENV_EXIT_MS             (10 * 60 * 1000)      /*!< 环境退出 TTL (ms) - 10分钟 */

#endif /* CONFIG_CONSTANTS_H */

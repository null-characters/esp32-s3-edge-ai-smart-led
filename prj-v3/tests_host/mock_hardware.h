/**
 * @file mock_hardware.h
 * @brief 硬件外设模拟层
 * 
 * 模拟麦克风、雷达、LED 等硬件设备
 */

#ifndef MOCK_HARDWARE_H
#define MOCK_HARDWARE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

/* ================================================================
 * 音频数据模拟
 * ================================================================ */

/**
 * @brief 生成模拟的音频数据
 * 
 * @param buffer 输出缓冲区
 * @param samples 采样点数
 * @param sample_rate 采样率
 * @param frequency 模拟频率 (Hz)
 * @param noise_level 噪声级别 (0.0-1.0)
 */
static inline void mock_generate_audio(int16_t *buffer, int samples, 
                                        int sample_rate, float frequency, 
                                        float noise_level) {
    for (int i = 0; i < samples; i++) {
        /* 正弦波 + 噪声 */
        float signal = sinf(2.0f * M_PI * frequency * i / sample_rate);
        float noise = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * noise_level;
        buffer[i] = (int16_t)((signal + noise) * 32767.0f * 0.5f);
    }
}

/**
 * @brief 生成模拟的 MFCC 特征
 * 
 * @param mfcc 输出 MFCC 缓冲区 (40x32)
 * @param class_type 场景类型 (0-4)
 */
static inline void mock_generate_mfcc(float *mfcc, int class_type) {
    /* 基于场景类型生成不同的 MFCC 模式 */
    float base_freq = 100.0f + class_type * 50.0f;
    
    for (int i = 0; i < 40; i++) {
        for (int j = 0; j < 32; j++) {
            /* 模拟频谱包络 */
            float envelope = expf(-i * 0.05f) * (1.0f + 0.3f * sinf(j * 0.2f));
            mfcc[i * 32 + j] = envelope * (1.0f + class_type * 0.1f);
        }
    }
}

/* ================================================================
 * 雷达数据模拟
 * ================================================================ */

/**
 * @brief 模拟 LD2410 雷达数据帧
 */
typedef struct {
    uint8_t header[4];        /* 0xF4 0xF3 0xF2 0xF1 */
    uint8_t type;             /* 帧类型 */
    uint8_t target_state;     /* 目标状态: 0=无, 1=静止, 2=运动 */
    int16_t distance;         /* 距离 (cm) */
    uint8_t energy;           /* 能量值 */
    uint8_t tail[4];          /* 0xF8 0xF7 0xF6 0xF5 */
} mock_radar_frame_t;

/**
 * @brief 生成模拟的雷达数据
 * 
 * @param frame 输出帧
 * @param state 目标状态 (0=无, 1=静止, 2=运动)
 * @param distance 距离 (cm)
 */
static inline void mock_generate_radar(mock_radar_frame_t *frame, 
                                        uint8_t state, int16_t distance) {
    frame->header[0] = 0xF4;
    frame->header[1] = 0xF3;
    frame->header[2] = 0xF2;
    frame->header[3] = 0xF1;
    
    frame->type = 0x01;
    frame->target_state = state;
    frame->distance = distance;
    frame->energy = (state > 0) ? 50 + rand() % 50 : 0;
    
    frame->tail[0] = 0xF8;
    frame->tail[1] = 0xF7;
    frame->tail[2] = 0xF6;
    frame->tail[3] = 0xF5;
}

/**
 * @brief 模拟雷达特征向量
 * 
 * @param features 输出特征 (8 维)
 * @param state 目标状态
 */
static inline void mock_generate_radar_features(float *features, uint8_t state) {
    features[0] = (state > 0) ? 1.0f : 0.0f;  /* 有无人 */
    features[1] = (state == 2) ? 1.0f : 0.0f; /* 是否运动 */
    features[2] = (state > 0) ? 150.0f : 0.0f; /* 距离 */
    features[3] = (state > 0) ? 0.8f : 0.0f;  /* 能量 */
    features[4] = 0.0f;  /* 保留 */
    features[5] = 0.0f;
    features[6] = 0.0f;
    features[7] = 0.0f;
}

/* ================================================================
 * LED 控制模拟
 * ================================================================ */

typedef struct {
    uint8_t brightness;   /* 亮度 0-100 */
    uint16_t color_temp;  /* 色温 2700K-6500K */
    bool is_on;           /* 开关状态 */
} mock_led_state_t;

static mock_led_state_t g_mock_led = {0, 4000, false};

/**
 * @brief 设置 LED 亮度
 */
static inline void mock_led_set_brightness(uint8_t brightness) {
    g_mock_led.brightness = brightness;
}

/**
 * @brief 设置 LED 色温
 */
static inline void mock_led_set_color_temp(uint16_t temp) {
    g_mock_led.color_temp = temp;
}

/**
 * @brief 设置 LED 开关
 */
static inline void mock_led_set_power(bool on) {
    g_mock_led.is_on = on;
}

/**
 * @brief 获取 LED 状态
 */
static inline const mock_led_state_t* mock_led_get_state(void) {
    return &g_mock_led;
}

/* ================================================================
 * 场景预设模拟
 * ================================================================ */

typedef struct {
    const char *name;
    uint8_t brightness;
    uint16_t color_temp;
} mock_scene_t;

static const mock_scene_t MOCK_SCENES[] = {
    {"阅读", 80, 4000},
    {"观影", 20, 3000},
    {"用餐", 60, 3500},
    {"休息", 10, 2700},
    {"活动", 70, 5000},
};

static inline int mock_scene_count(void) {
    return sizeof(MOCK_SCENES) / sizeof(MOCK_SCENES[0]);
}

static inline const mock_scene_t* mock_scene_get(int index) {
    if (index >= 0 && index < mock_scene_count()) {
        return &MOCK_SCENES[index];
    }
    return NULL;
}

/* ================================================================
 * 语音命令模拟
 * ================================================================ */

typedef enum {
    MOCK_CMD_NONE = 0,
    MOCK_CMD_TURN_ON,
    MOCK_CMD_TURN_OFF,
    MOCK_CMD_BRIGHT_UP,
    MOCK_CMD_BRIGHT_DOWN,
    MOCK_CMD_WARM,
    MOCK_CMD_COOL,
    MOCK_CMD_STATUS,
    MOCK_CMD_AUTO,
    MOCK_CMD_MAX,
} mock_voice_cmd_t;

/**
 * @brief 模拟语音命令处理
 */
static inline void mock_process_voice_cmd(mock_voice_cmd_t cmd) {
    switch (cmd) {
        case MOCK_CMD_TURN_ON:
            mock_led_set_power(true);
            break;
        case MOCK_CMD_TURN_OFF:
            mock_led_set_power(false);
            break;
        case MOCK_CMD_BRIGHT_UP:
            if (g_mock_led.brightness < 90) g_mock_led.brightness += 10;
            break;
        case MOCK_CMD_BRIGHT_DOWN:
            if (g_mock_led.brightness > 10) g_mock_led.brightness -= 10;
            break;
        case MOCK_CMD_WARM:
            g_mock_led.color_temp = 3000;
            break;
        case MOCK_CMD_COOL:
            g_mock_led.color_temp = 5000;
            break;
        case MOCK_CMD_AUTO:
            /* 切换到自动模式 */
            break;
        default:
            break;
    }
}

#endif /* MOCK_HARDWARE_H */

/**
 * @file Lighting_Event.h
 * @brief 统一照明控制事件格式
 * 
 * 定义语音命令、自动控制、仲裁器之间传递的统一事件结构
 * 支持 TTL 租约机制，防止语音锁死
 */

#ifndef LIGHTING_EVENT_H
#define LIGHTING_EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ================================================================
 * 事件来源定义
 * ================================================================ */

/**
 * @brief 事件来源类型
 */
typedef enum {
    EVENT_SOURCE_NONE = 0,       /* 无效事件 */
    EVENT_SOURCE_VOICE = 1,      /* 语音命令 */
    EVENT_SOURCE_AUTO = 2,       /* 自动控制 (雷达+TFLM) */
    EVENT_SOURCE_DEFAULT = 3,    /* 默认行为 */
    EVENT_SOURCE_TIMER = 4,      /* 定时器触发 */
    EVENT_SOURCE_NETWORK = 5,    /* 网络命令 */
} event_source_t;

/**
 * @brief 事件类型
 */
typedef enum {
    EVENT_TYPE_NONE = 0,
    EVENT_TYPE_SCENE = 1,        /* 场景切换 */
    EVENT_TYPE_BRIGHTNESS = 2,   /* 亮度调节 */
    EVENT_TYPE_COLOR_TEMP = 3,   /* 色温调节 */
    EVENT_TYPE_POWER = 4,        /* 电源开关 */
    EVENT_TYPE_QUERY = 5,        /* 状态查询 */
    EVENT_TYPE_MODE_SWITCH = 6,  /* 模式切换 */
} event_type_t;

/* ================================================================
 * 场景配置
 * ================================================================ */

/**
 * @brief 预定义场景 ID
 */
typedef enum {
    SCENE_NONE = 0,
    SCENE_FOCUS = 1,      /* 专注模式: 冷光, 中等亮度 */
    SCENE_MEETING = 2,    /* 会议模式: 自然光, 高亮度 */
    SCENE_PRESENT = 3,    /* 演示模式: 暖光, 低亮度 */
    SCENE_RELAX = 4,      /* 休息模式: 暖光, 低亮度 */
    SCENE_NIGHT = 5,      /* 夜间模式: 暖光, 极低亮度 */
    SCENE_CUSTOM = 255,   /* 自定义场景 */
} scene_id_t;

/**
 * @brief 场景配置结构
 */
typedef struct {
    scene_id_t id;            /* 场景 ID */
    const char *name;         /* 场景名称 */
    uint8_t brightness;       /* 亮度 (0-100) */
    uint16_t color_temp;      /* 色温 (2700-6500K) */
    bool power;               /* 电源状态 */
} scene_config_t;

/* ================================================================
 * 控制参数结构
 * ================================================================ */

/**
 * @brief 亮度调节参数
 */
typedef struct {
    uint8_t value;            /* 目标亮度值 (0-100) */
    bool relative;            /* 是否相对调节 */
    int8_t delta;             /* 相对调节增量 (-100 ~ +100) */
} brightness_config_t;

/**
 * @brief 色温调节参数
 */
typedef struct {
    uint16_t value;           /* 目标色温值 (2700-6500K) */
    bool relative;            /* 是否相对调节 (暖/冷切换) */
    int8_t direction;         /* 方向: -1 暖, +1 冷 */
} color_temp_config_t;

/**
 * @brief 电源控制参数
 */
typedef struct {
    bool power;               /* 开关状态 */
    uint32_t delay_ms;        /* 延迟时间 (用于定时关灯) */
} power_config_t;

/* ================================================================
 * 统一事件结构
 * ================================================================ */

/**
 * @brief 统一照明控制事件
 * 
 * 这是语音命令、自动控制、仲裁器之间传递的标准事件格式
 */
typedef struct {
    /* 事件元信息 */
    event_source_t source;         /* 事件来源 */
    event_type_t type;             /* 事件类型 */
    uint64_t timestamp_ms;         /* 时间戳 (毫秒) */
    
    /* TTL 租约 (仅语音事件有效) */
    uint32_t ttl_ms;               /* 租约时长 (毫秒), 0 表示无租约 */
    bool has_lease;                /* 是否带有租约 */
    
    /* 事件数据联合体 */
    union {
        scene_config_t scene;       /* 场景配置 */
        brightness_config_t brightness; /* 亮度配置 */
        color_temp_config_t color_temp;  /* 色温配置 */
        power_config_t power;       /* 电源配置 */
        int query_type;             /* 查询类型 */
        int mode;                   /* 模式值 */
    } data;
    
    /* 事件标识 */
    int command_id;                /* 原始命令 ID (语音事件) */
    float confidence;              /* 置信度 */
    
} Lighting_Event_t;

/* ================================================================
 * 预定义场景
 * ================================================================ */

/* 专注模式: 冷光 (4000K), 中等亮度 (60%) */
static const scene_config_t SCENE_FOCUS_CONFIG = {
    .id = SCENE_FOCUS,
    .name = "专注模式",
    .brightness = 60,
    .color_temp = 4000,
    .power = true,
};

/* 会议模式: 自然光 (5000K), 高亮度 (80%) */
static const scene_config_t SCENE_MEETING_CONFIG = {
    .id = SCENE_MEETING,
    .name = "会议模式",
    .brightness = 80,
    .color_temp = 5000,
    .power = true,
};

/* 演示模式: 暖光 (3000K), 低亮度 (30%) */
static const scene_config_t SCENE_PRESENT_CONFIG = {
    .id = SCENE_PRESENT,
    .name = "演示模式",
    .brightness = 30,
    .color_temp = 3000,
    .power = true,
};

/* 休息模式: 暖光 (2700K), 低亮度 (40%) */
static const scene_config_t SCENE_RELAX_CONFIG = {
    .id = SCENE_RELAX,
    .name = "休息模式",
    .brightness = 40,
    .color_temp = 2700,
    .power = true,
};

/* 夜间模式: 暖光 (2700K), 极低亮度 (10%) */
static const scene_config_t SCENE_NIGHT_CONFIG = {
    .id = SCENE_NIGHT,
    .name = "夜间模式",
    .brightness = 10,
    .color_temp = 2700,
    .power = true,
};

/* ================================================================
 * TTL 租约常量
 * ================================================================ */

#define TTL_DEFAULT_MS        (2 * 60 * 60 * 1000)  /* 默认 2 小时 */
#define TTL_SHORT_MS          (30 * 60 * 1000)      /* 短租约 30 分钟 */
#define TTL_LONG_MS           (4 * 60 * 60 * 1000)  /* 长租约 4 小时 */
#define TTL_ENV_EXIT_MS       (10 * 60 * 1000)      /* 环境退出阈值 10 分钟 */

/* ================================================================
 * 辅助函数 (内联)
 * ================================================================ */

/**
 * @brief 获取场景配置
 * @param id 场景 ID
 * @return 场景配置指针
 */
static inline const scene_config_t* get_scene_config(scene_id_t id)
{
    switch (id) {
        case SCENE_FOCUS:   return &SCENE_FOCUS_CONFIG;
        case SCENE_MEETING: return &SCENE_MEETING_CONFIG;
        case SCENE_PRESENT: return &SCENE_PRESENT_CONFIG;
        case SCENE_RELAX:   return &SCENE_RELAX_CONFIG;
        case SCENE_NIGHT:   return &SCENE_NIGHT_CONFIG;
        default:            return NULL;
    }
}

/**
 * @brief 创建语音事件 (带 TTL 租约)
 * @param type 事件类型
 * @param ttl_ms TTL 租约时长 (毫秒)
 * @return 事件结构
 */
static inline Lighting_Event_t create_voice_event(event_type_t type, uint32_t ttl_ms)
{
    Lighting_Event_t event = {
        .source = EVENT_SOURCE_VOICE,
        .type = type,
        .timestamp_ms = 0,  /* 由发送者填充 */
        .ttl_ms = ttl_ms,
        .has_lease = (ttl_ms > 0),
        .command_id = 0,
        .confidence = 0.0f,
    };
    return event;
}

/**
 * @brief 创建自动控制事件 (无 TTL 租约)
 * @param type 事件类型
 * @return 事件结构
 */
static inline Lighting_Event_t create_auto_event(event_type_t type)
{
    Lighting_Event_t event = {
        .source = EVENT_SOURCE_AUTO,
        .type = type,
        .timestamp_ms = 0,
        .ttl_ms = 0,
        .has_lease = false,
        .command_id = -1,
        .confidence = 0.0f,
    };
    return event;
}

#endif /* LIGHTING_EVENT_H */

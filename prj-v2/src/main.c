/**
 * @file main.c
 * @brief ESP32-S3 Edge AI Gateway V2 主程序
 * 
 * 多模态 AI 升级方案：
 * - Phase 1: 毫米波雷达接入 (LD2410)
 * - Phase 2: 声音分类 (INMP441 + MFCC)
 * - Phase 3: 多模态融合推理
 * - Phase 4: 场景智能控制
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/i2s.h>

#include "radar/ld2410_driver.h"
#include "audio/inmp441_driver.h"
#include "audio/mfcc.h"
#include "multimodal/multimodal_infer.h"
#include "scene/scene_recognition.h"

/* 基础模块 */
#include "uart_driver.h"
#include "lighting.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* 线程配置 */
#define RADAR_THREAD_STACK_SIZE   2048
#define AUDIO_THREAD_STACK_SIZE    4096
#define INFERENCE_THREAD_STACK_SIZE 8192
#define CONTROL_THREAD_STACK_SIZE  2048

#define RADAR_THREAD_PRIORITY      5
#define AUDIO_THREAD_PRIORITY       4
#define INFERENCE_THREAD_PRIORITY   6
#define CONTROL_THREAD_PRIORITY     7

/* 推理间隔 */
#define INFERENCE_INTERVAL_MS  1000

/* 全局数据 */
static radar_features_t g_radar_features;
static mfcc_features_t g_mfcc_features;
static multimodal_input_t g_multimodal_input;
static multimodal_output_t g_multimodal_output;
static scene_id_t g_current_scene = SCENE_UNKNOWN;

/* 互斥锁 */
static struct k_mutex features_mutex;
static struct k_mutex output_mutex;

/* 信号量 */
static struct k_sem radar_ready;
static struct k_sem audio_ready;
static struct k_sem inference_trigger;

/* 线程定义 */
K_THREAD_DEFINE(radar_thread, RADAR_THREAD_STACK_SIZE, 
                radar_thread_fn, NULL, NULL, NULL,
                RADAR_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(audio_thread, AUDIO_THREAD_STACK_SIZE,
                 audio_thread_fn, NULL, NULL, NULL,
                 AUDIO_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(inference_thread, INFERENCE_THREAD_STACK_SIZE,
                 inference_thread_fn, NULL, NULL, NULL,
                 INFERENCE_THREAD_PRIORITY, 0, 0);

K_THREAD_DEFINE(control_thread, CONTROL_THREAD_STACK_SIZE,
                 control_thread_fn, NULL, NULL, NULL,
                 CONTROL_THREAD_PRIORITY, 0, 0);

/**
 * @brief 雷达数据采集线程
 */
void radar_thread_fn(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Radar thread started");
    
    while (1) {
        /* 读取雷达特征 */
        radar_features_t features;
        int ret = ld2410_get_features(&features);
        
        if (ret == 0) {
            k_mutex_lock(&features_mutex, K_FOREVER);
            g_radar_features = features;
            k_mutex_unlock(&features_mutex);
            
            k_sem_give(&radar_ready);
            
            LOG_DBG("Radar: dist=%.2f, vel=%.2f, energy=%.2f",
                    features.distance_norm, features.velocity_norm,
                    features.energy_norm);
        }
        
        k_msleep(100);  /* 10Hz 采样 */
    }
}

/**
 * @brief 音频采集线程
 */
void audio_thread_fn(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Audio thread started");
    
    /* 音频缓冲区 (1秒音频) */
    int16_t audio_buffer[AUDIO_SAMPLE_RATE];
    
    while (1) {
        /* 采集音频帧 */
        audio_frame_t frame;
        int ret = inmp441_read_frame(&frame);
        
        if (ret == 0 && frame.vad_active) {
            /* 累积音频数据 (1秒) */
            static int sample_count = 0;
            
            if (sample_count < AUDIO_SAMPLE_RATE) {
                memcpy(&audio_buffer[sample_count], frame.samples,
                       frame.num_samples * sizeof(int16_t));
                sample_count += frame.num_samples;
            }
            
            /* 达到1秒时提取 MFCC */
            if (sample_count >= AUDIO_SAMPLE_RATE) {
                mfcc_features_t mfcc;
                mfcc_extract(audio_buffer, AUDIO_SAMPLE_RATE, &mfcc);
                
                k_mutex_lock(&features_mutex, K_FOREVER);
                g_mfcc_features = mfcc;
                k_mutex_unlock(&features_mutex);
                
                k_sem_give(&audio_ready);
                
                sample_count = 0;
                LOG_DBG("MFCC extracted");
            }
        }
        
        k_msleep(10);  /* 100Hz 采样 */
    }
}

/**
 * @brief 多模态推理线程
 */
void inference_thread_fn(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Inference thread started");
    
    while (1) {
        /* 等待数据就绪 */
        k_sem_take(&radar_ready, K_MSEC(500));
        k_sem_take(&audio_ready, K_MSEC(500));
        
        /* 组装输入 */
        k_mutex_lock(&features_mutex, K_FOREVER);
        
        /* MFCC 数据 */
        memcpy(g_multimodal_input.mfcc, g_mfcc_features.data,
               SOUND_MODEL_INPUT_SIZE * sizeof(float));
        
        /* 雷达特征 */
        g_multimodal_input.radar = g_radar_features;
        
        k_mutex_unlock(&features_mutex);
        
        /* 环境特征 */
        float hour = (k_uptime_get() / 3600000) % 24;
        g_multimodal_input.hour_sin = sinf(2 * M_PI * hour / 24);
        g_multimodal_input.hour_cos = cosf(2 * M_PI * hour / 24);
        g_multimodal_input.sunset_proximity = 0;  /* TODO: 从 v1 模块获取 */
        g_multimodal_input.presence = 1;          /* TODO: 从雷达判断 */
        
        /* 执行推理 */
        multimodal_output_t output;
        int ret = multimodal_infer(&g_multimodal_input, &output);
        
        if (ret == 0) {
            k_mutex_lock(&output_mutex, K_FOREVER);
            g_multimodal_output = output;
            k_mutex_unlock(&output_mutex);
            
            /* 场景识别 */
            scene_id_t scene;
            float confidence;
            scene_recognize(&output, &scene, &confidence);
            
            if (scene_check_switch(scene, confidence)) {
                scene_start_transition(scene, 0);
                g_current_scene = scene;
            }
            
            LOG_INF("Inference: ct=%uK, br=%u%%, scene=%s (%.2f)",
                    output.color_temp, output.brightness,
                    scene_get_name(scene), confidence);
        }
        
        k_msleep(INFERENCE_INTERVAL_MS);
    }
}

/**
 * @brief 照明控制线程
 */
void control_thread_fn(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Control thread started");
    
    while (1) {
        /* 更新场景过渡 */
        uint16_t color_temp;
        uint8_t brightness;
        
        int ret = scene_update_transition(0, &color_temp, &brightness);
        
        if (ret == 0) {
            /* 发送控制命令 */
            uart_send_set_both(color_temp, brightness);
            
            LOG_DBG("Control: ct=%uK, br=%u%%", color_temp, brightness);
        }
        
        k_msleep(100);  /* 10Hz 控制频率 */
    }
}

/**
 * @brief 主函数
 */
int main(void)
{
    LOG_INF("ESP32-S3 Edge AI Gateway V2 starting...");
    LOG_INF("Multimodal AI: Radar + Microphone");
    
    /* 初始化互斥锁 */
    k_mutex_init(&features_mutex);
    k_mutex_init(&output_mutex);
    
    /* 初始化信号量 */
    k_sem_init(&radar_ready, 0, 1);
    k_sem_init(&audio_ready, 0, 1);
    k_sem_init(&inference_trigger, 0, 1);
    
    /* Phase 1: 初始化雷达驱动 */
    int ret = ld2410_init();
    if (ret < 0) {
        LOG_WRN("Radar init failed (hardware not ready)");
    }
    
    /* Phase 2: 初始化麦克风驱动 */
    ret = inmp441_init();
    if (ret < 0) {
        LOG_WRN("Microphone init failed (hardware not ready)");
    }
    
    /* 初始化 MFCC */
    mfcc_init();
    
    /* Phase 3: 初始化多模态推理 */
    ret = multimodal_init();
    if (ret < 0) {
        LOG_ERR("Multimodal init failed");
        return ret;
    }
    
    /* Phase 4: 初始化场景识别 */
    scene_init();
    
    /* 保留 v1 模块初始化 */
    uart_driver_init();
    lighting_init();
    
    /* 启动音频采集 */
    inmp441_start();
    
    LOG_INF("All modules initialized");
    LOG_INF("Waiting for hardware...");
    
    /* 主循环 */
    while (1) {
        k_msleep(1000);
        
        /* 打印状态 */
        LOG_INF("Scene: %s, CT: %uK, BR: %u%%",
                scene_get_name(g_current_scene),
                g_multimodal_output.color_temp,
                g_multimodal_output.brightness);
    }
    
    return 0;
}
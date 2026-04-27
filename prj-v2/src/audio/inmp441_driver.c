/**
 * @file inmp441_driver.c
 * @brief INMP441 MEMS 麦克风驱动实现
 * 
 * Phase 2: 声音分类模块
 * 任务 AU-004 ~ AU-007
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>
#include "inmp441_driver.h"

LOG_MODULE_REGISTER(inmp441, LOG_LEVEL_INF);

/* I2S 设备 */
static const struct device *i2s_dev;

/* 音频缓冲区 */
static audio_buffer_t audio_buffer;

/* VAD 状态 */
static vad_state_t vad_state;

/* DMA 缓冲区 */
#define DMA_BUF_COUNT   4
#define DMA_BUF_SIZE    1024
static int16_t dma_buf[DMA_BUF_COUNT][DMA_BUF_SIZE];
static int active_buf = 0;

/* 采集状态 */
static volatile bool capturing = false;

/**
 * @brief I2S 接收回调
 */
static void i2s_rx_callback(const struct device *dev, 
                            void *buf, size_t len, 
                            void *user_data)
{
    if (!capturing) {
        return;
    }
    
    int16_t *samples = (int16_t *)buf;
    size_t num_samples = len / sizeof(int16_t);
    
    k_mutex_lock(&audio_buffer.lock, K_FOREVER);
    
    /* 写入环形缓冲区 */
    for (size_t i = 0; i < num_samples && audio_buffer.count < AUDIO_BUFFER_SAMPLES; i++) {
        audio_buffer.buffer[audio_buffer.head] = samples[i];
        audio_buffer.head = (audio_buffer.head + 1) % AUDIO_BUFFER_SAMPLES;
        audio_buffer.count++;
    }
    
    /* 缓冲区满时覆盖旧数据 */
    if (audio_buffer.count > AUDIO_BUFFER_SAMPLES) {
        audio_buffer.tail = audio_buffer.head;
        audio_buffer.count = AUDIO_BUFFER_SAMPLES;
    }
    
    k_mutex_unlock(&audio_buffer.lock);
    
    /* 启动下一次接收 */
    active_buf = (active_buf + 1) % DMA_BUF_COUNT;
    i2s_read(dev, dma_buf[active_buf], DMA_BUF_SIZE * sizeof(int16_t), K_NO_WAIT);
}

int inmp441_init(void)
{
    /* 获取 I2S 设备 */
    i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
    if (!device_is_ready(i2s_dev)) {
        LOG_ERR("I2S0 device not ready");
        return -ENODEV;
    }
    
    /* 配置 I2S: 16kHz, 16-bit, 单声道 */
    struct i2s_config i2s_cfg = {
        .word_size = AUDIO_BITS_PER_SAMPLE,
        .channels = AUDIO_CHANNELS,
        .format = I2S_FMT_DATA_FORMAT_I2S,
        .options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER,
        .frame_clk_freq = AUDIO_SAMPLE_RATE,
        .block_size = DMA_BUF_SIZE * sizeof(int16_t),
        .timeout = 1000,
    };
    
    int ret = i2s_configure(i2s_dev, I2S_DIR_RX, &i2s_cfg);
    if (ret < 0) {
        LOG_ERR("I2S configure failed: %d", ret);
        return ret;
    }
    
    /* 初始化音频缓冲区 */
    audio_buffer.head = 0;
    audio_buffer.tail = 0;
    audio_buffer.count = 0;
    k_mutex_init(&audio_buffer.lock);
    
    /* 初始化 VAD */
    vad_state.active = false;
    vad_state.energy_threshold = VAD_ENERGY_THRESHOLD;
    vad_state.last_active_time = 0;
    vad_state.noise_floor = 0.001f;
    
    LOG_INF("INMP441 microphone driver initialized");
    return 0;
}

void inmp441_start(void)
{
    if (capturing) {
        return;
    }
    
    capturing = true;
    
    /* 启动 I2S 接收 */
    for (int i = 0; i < DMA_BUF_COUNT; i++) {
        i2s_read(i2s_dev, dma_buf[i], DMA_BUF_SIZE * sizeof(int16_t), K_NO_WAIT);
    }
    
    LOG_INF("Audio capture started");
}

void inmp441_stop(void)
{
    capturing = false;
    LOG_INF("Audio capture stopped");
}

int inmp441_read_frame(audio_frame_t *frame)
{
    k_mutex_lock(&audio_buffer.lock, K_FOREVER);
    
    if (audio_buffer.count < AUDIO_FRAME_SAMPLES) {
        k_mutex_unlock(&audio_buffer.lock);
        return -EAGAIN;
    }
    
    /* 读取一帧数据 */
    for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
        frame->samples[i] = audio_buffer.buffer[audio_buffer.tail];
        audio_buffer.tail = (audio_buffer.tail + 1) % AUDIO_BUFFER_SAMPLES;
        audio_buffer.count--;
    }
    
    frame->num_samples = AUDIO_FRAME_SAMPLES;
    frame->timestamp = k_uptime_get();
    
    k_mutex_unlock(&audio_buffer.lock);
    
    /* 计算 RMS 能量 */
    frame->rms_energy = inmp441_calc_rms(frame->samples, frame->num_samples);
    
    /* VAD 检测 */
    frame->vad_active = inmp441_vad_detect(frame);
    
    return 0;
}

int inmp441_get_samples(int16_t *samples, int max_samples)
{
    k_mutex_lock(&audio_buffer.lock, K_FOREVER);
    
    int count = (audio_buffer.count < max_samples) ? audio_buffer.count : max_samples;
    
    for (int i = 0; i < count; i++) {
        samples[i] = audio_buffer.buffer[audio_buffer.tail];
        audio_buffer.tail = (audio_buffer.tail + 1) % AUDIO_BUFFER_SAMPLES;
    }
    audio_buffer.count -= count;
    
    k_mutex_unlock(&audio_buffer.lock);
    
    return count;
}

float inmp441_calc_rms(const int16_t *samples, int num_samples)
{
    if (num_samples <= 0) {
        return 0;
    }
    
    float sum_sq = 0;
    for (int i = 0; i < num_samples; i++) {
        float sample = samples[i] / 32768.0f;  /* 归一化到 [-1, 1] */
        sum_sq += sample * sample;
    }
    
    return sqrtf(sum_sq / num_samples);
}

bool inmp441_vad_detect(const audio_frame_t *frame)
{
    float energy = frame->rms_energy;
    uint32_t now = k_uptime_get();
    
    /* 更新噪声底估计 */
    if (!vad_state.active && energy < vad_state.energy_threshold) {
        /* 平滑更新噪声底 */
        vad_state.noise_floor = 0.9f * vad_state.noise_floor + 0.1f * energy;
    }
    
    /* 自适应阈值 */
    float threshold = vad_state.noise_floor * 10;
    if (threshold < VAD_ENERGY_THRESHOLD) {
        threshold = VAD_ENERGY_THRESHOLD;
    }
    
    /* 能量检测 */
    if (energy > threshold) {
        vad_state.active = true;
        vad_state.last_active_time = now;
        return true;
    }
    
    /* 拖尾处理 */
    if (vad_state.active) {
        if (now - vad_state.last_active_time < VAD_HANGOVER_MS) {
            return true;
        }
        vad_state.active = false;
    }
    
    return false;
}

bool inmp441_get_vad_state(void)
{
    return vad_state.active;
}

void inmp441_update_noise_floor(float energy)
{
    vad_state.noise_floor = 0.95f * vad_state.noise_floor + 0.05f * energy;
}
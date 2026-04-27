/**
 * @file mfcc.c
 * @brief MFCC 特征提取实现
 * 
 * Phase 2: 声音分类模块
 * 任务 AU-008 ~ AU-013
 * 
 * 依赖: ESP-DSP 库 (FFT)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>
#include "mfcc.h"

/* ESP-DSP FFT 头文件 */
#ifdef CONFIG_DSP
#include <esp_dsp.h>
#endif

LOG_MODULE_REGISTER(mfcc, LOG_LEVEL_INF);

/* MFCC 状态 */
static mfcc_state_t mfcc_state;

/* 预加重系数 */
#define PRE_EMPH_ALPHA  0.97f

/* Mel 滤波器参数 */
#define MEL_LOW_FREQ    0.0f
#define MEL_HIGH_FREQ   8000.0f

/**
 * @brief 初始化汉明窗
 */
static void init_hamming_window(void)
{
    for (int i = 0; i < MFCC_FRAME_SIZE; i++) {
        mfcc_state.window[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (MFCC_FRAME_SIZE - 1));
    }
}

/**
 * @brief 初始化 Mel 滤波器组
 */
static void init_mel_filters(void)
{
    float mel_low = mfcc_hz_to_mel(MEL_LOW_FREQ);
    float mel_high = mfcc_hz_to_mel(MEL_HIGH_FREQ);
    float mel_step = (mel_high - mel_low) / (MFCC_MEL_FILTERS + 1);
    
    /* 计算每个滤波器的中心频率 */
    float mel_centers[MFCC_MEL_FILTERS + 2];
    for (int i = 0; i < MFCC_MEL_FILTERS + 2; i++) {
        mel_centers[i] = mel_low + i * mel_step;
    }
    
    /* 转换为线性频率 */
    float freq_centers[MFCC_MEL_FILTERS + 2];
    for (int i = 0; i < MFCC_MEL_FILTERS + 2; i++) {
        freq_centers[i] = mfcc_mel_to_hz(mel_centers[i]);
    }
    
    /* 转换为 FFT bin 索引 */
    int bin_centers[MFCC_MEL_FILTERS + 2];
    for (int i = 0; i < MFCC_MEL_FILTERS + 2; i++) {
        bin_centers[i] = (int)floorf((MFCC_FFT_SIZE + 1) * freq_centers[i] / MFCC_SAMPLE_RATE);
    }
    
    /* 创建三角滤波器 */
    for (int i = 0; i < MFCC_MEL_FILTERS; i++) {
        int left = bin_centers[i];
        int center = bin_centers[i + 1];
        int right = bin_centers[i + 2];
        
        for (int j = 0; j < MFCC_FFT_SIZE / 2 + 1; j++) {
            if (j < left || j > right) {
                mfcc_state.mel_filters[i][j] = 0;
            } else if (j < center) {
                mfcc_state.mel_filters[i][j] = (float)(j - left) / (center - left);
            } else {
                mfcc_state.mel_filters[i][j] = (float)(right - j) / (right - center);
            }
        }
    }
}

int mfcc_init(void)
{
    if (mfcc_state.initialized) {
        return 0;
    }
    
    /* 初始化预加重状态 */
    mfcc_state.pre_emph_last = 0;
    
    /* 初始化汉明窗 */
    init_hamming_window();
    
    /* 初始化 Mel 滤波器 */
    init_mel_filters();
    
#ifdef CONFIG_DSP
    /* 初始化 ESP-DSP FFT */
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, MFCC_FFT_SIZE);
    if (ret != ESP_OK) {
        LOG_WRN("ESP-DSP FFT init failed, using fallback");
    }
#endif
    
    mfcc_state.initialized = true;
    LOG_INF("MFCC extractor initialized");
    return 0;
}

int mfcc_extract(const int16_t *samples, int num_samples, mfcc_features_t *features)
{
    if (!mfcc_state.initialized) {
        mfcc_init();
    }
    
    /* 1. 预加重 */
    float pre_emph[MFCC_SAMPLE_RATE];  /* 1秒音频 */
    int pre_len = (num_samples < MFCC_SAMPLE_RATE) ? num_samples : MFCC_SAMPLE_RATE;
    mfcc_pre_emphasis(samples, pre_emph, pre_len, PRE_EMPH_ALPHA);
    
    /* 2. 分帧 */
    float frames[MFCC_NUM_FRAMES][MFCC_FRAME_SIZE];
    int num_frames;
    mfcc_frame_signal(pre_emph, pre_len, frames, &num_frames);
    
    /* 限制帧数 */
    if (num_frames > MFCC_NUM_FRAMES) {
        num_frames = MFCC_NUM_FRAMES;
    }
    
    features->num_frames = num_frames;
    features->num_coeffs = MFCC_NUM_COEFFS;
    
    /* 3. 逐帧处理 */
    for (int f = 0; f < num_frames; f++) {
        /* 应用汉明窗 */
        mfcc_apply_window(frames[f], MFCC_FRAME_SIZE);
        
        /* 计算 FFT */
        float spectrum[MFCC_FFT_SIZE];
        mfcc_fft(frames[f], spectrum, MFCC_FFT_SIZE);
        
        /* 计算功率谱 */
        float power[MFCC_FFT_SIZE / 2 + 1];
        for (int i = 0; i < MFCC_FFT_SIZE / 2 + 1; i++) {
            power[i] = spectrum[i] * spectrum[i];
        }
        
        /* 应用 Mel 滤波器 */
        float mel_spectrum[MFCC_MEL_FILTERS];
        mfcc_apply_mel_filters(power, mel_spectrum, MFCC_FFT_SIZE / 2 + 1);
        
        /* 取对数 */
        for (int i = 0; i < MFCC_MEL_FILTERS; i++) {
            mel_spectrum[i] = logf(mel_spectrum[i] + 1e-10f);
        }
        
        /* DCT 变换 */
        float mfcc[MFCC_NUM_COEFFS];
        mfcc_dct(mel_spectrum, mfcc, MFCC_MEL_FILTERS);
        
        /* 保存结果 */
        for (int i = 0; i < MFCC_NUM_COEFFS; i++) {
            features->data[i][f] = mfcc[i];
        }
    }
    
    /* 填充剩余帧 */
    for (int f = num_frames; f < MFCC_NUM_FRAMES; f++) {
        for (int i = 0; i < MFCC_NUM_COEFFS; i++) {
            features->data[i][f] = 0;
        }
    }
    
    return 0;
}

void mfcc_pre_emphasis(const int16_t *input, float *output, int len, float alpha)
{
    output[0] = input[0] / 32768.0f;
    for (int i = 1; i < len; i++) {
        output[i] = input[i] / 32768.0f - alpha * (input[i-1] / 32768.0f);
    }
}

void mfcc_frame_signal(const float *signal, int signal_len, float frames[][MFCC_FRAME_SIZE], int *num_frames)
{
    int num = (signal_len - MFCC_FRAME_SIZE) / MFCC_FRAME_SHIFT + 1;
    if (num < 0) num = 0;
    if (num > MFCC_NUM_FRAMES) num = MFCC_NUM_FRAMES;
    
    *num_frames = num;
    
    for (int i = 0; i < num; i++) {
        int start = i * MFCC_FRAME_SHIFT;
        for (int j = 0; j < MFCC_FRAME_SIZE; j++) {
            if (start + j < signal_len) {
                frames[i][j] = signal[start + j];
            } else {
                frames[i][j] = 0;
            }
        }
    }
}

void mfcc_apply_window(float *frame, int len)
{
    for (int i = 0; i < len && i < MFCC_FRAME_SIZE; i++) {
        frame[i] *= mfcc_state.window[i];
    }
}

void mfcc_fft(const float *input, float *output, int len)
{
#ifdef CONFIG_DSP
    /* 使用 ESP-DSP FFT */
    float *fft_input = mfcc_state.fft_buffer;
    
    /* 填充实部和虚部 */
    for (int i = 0; i < len; i++) {
        fft_input[2*i] = (i < MFCC_FRAME_SIZE) ? input[i] : 0;  /* 实部 */
        fft_input[2*i+1] = 0;                                    /* 虚部 */
    }
    
    /* 执行 FFT */
    dsps_fft2r_fc32(fft_input, len);
    dsps_bit_rev_fc32(fft_input, len);
    
    /* 提取幅度谱 */
    for (int i = 0; i < len / 2 + 1; i++) {
        float re = fft_input[2*i];
        float im = fft_input[2*i+1];
        output[i] = sqrtf(re*re + im*im);
    }
#else
    /* 简化 FFT (仅用于测试) */
    for (int k = 0; k < len / 2 + 1; k++) {
        float sum = 0;
        for (int n = 0; n < len; n++) {
            float angle = 2.0f * M_PI * k * n / len;
            float sample = (n < MFCC_FRAME_SIZE) ? input[n] : 0;
            sum += sample * cosf(angle);
        }
        output[k] = fabsf(sum);
    }
#endif
}

float mfcc_hz_to_mel(float freq)
{
    return 2595.0f * log10f(1.0f + freq / 700.0f);
}

float mfcc_mel_to_hz(float mel)
{
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

void mfcc_apply_mel_filters(const float *power_spectrum, float *mel_spectrum, int len)
{
    for (int i = 0; i < MFCC_MEL_FILTERS; i++) {
        float sum = 0;
        for (int j = 0; j < len; j++) {
            sum += power_spectrum[j] * mfcc_state.mel_filters[i][j];
        }
        mel_spectrum[i] = sum;
    }
}

void mfcc_dct(const float *input, float *output, int len)
{
    /* DCT-II */
    for (int k = 0; k < MFCC_NUM_COEFFS; k++) {
        float sum = 0;
        for (int n = 0; n < len; n++) {
            sum += input[n] * cosf(M_PI * k * (n + 0.5f) / len);
        }
        output[k] = sum;
    }
}
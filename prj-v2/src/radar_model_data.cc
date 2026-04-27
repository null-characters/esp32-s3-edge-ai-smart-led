/*
 * Radar Scene Classifier Model Data - Placeholder
 * Auto-generated placeholder, run radar_train_model.py to generate real data
 *
 * Model: 8 → 16 → 12 → 6 MLP Classifier
 * Input: 8-dim radar features (INT8)
 * Output: 6-class scene probabilities (INT8)
 *
 * Scenes:
 *   0 = empty (无人)
 *   1 = breathing (静坐呼吸)
 *   2 = fan (风扇摇头)
 *   3 = walking (行走)
 *   4 = multiple_people (多人)
 *   5 = entering_leaving (进入/离开)
 */

#include <stdint.h>

// Placeholder: 256 bytes of zeros
// Real model will be ~2KB after training
const unsigned int g_radar_model_data_len = 256;
const alignas(16) unsigned char g_radar_model_data[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* ... placeholder data ... */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

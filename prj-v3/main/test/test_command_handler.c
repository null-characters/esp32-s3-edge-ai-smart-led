/**
 * @file test_command_handler.c
 * @brief 命令处理器单元测试 (TDD RED Phase)
 * 
 * 测试内容：
 * - 命令词 ID 验证
 * - 命令类别判断
 * - 场景切换功能
 * - 亮度/色温调节
 */

#include <unity.h>
#include "command_handler.h"
#include "command_words.h"

/* ================================================================
 * T1: 命令词 ID 验证测试
 * ================================================================ */

TEST_CASE("command_id_validation", "command_handler")
{
    /* 有效命令 ID */
    TEST_ASSERT_TRUE(command_is_valid(CMD_SCENE_FOCUS));
    TEST_ASSERT_TRUE(command_is_valid(CMD_BRIGHT_UP));
    TEST_ASSERT_TRUE(command_is_valid(CMD_CCT_WARM));
    TEST_ASSERT_TRUE(command_is_valid(CMD_POWER_ON));
    TEST_ASSERT_TRUE(command_is_valid(CMD_MODE_AUTO));
    TEST_ASSERT_TRUE(command_is_valid(CMD_QUERY_BRIGHTNESS));
    TEST_ASSERT_TRUE(command_is_valid(CMD_CANCEL_ACTION));
    TEST_ASSERT_TRUE(command_is_valid(CMD_SPECIAL_HELP));
    
    /* 无效命令 ID */
    TEST_ASSERT_FALSE(command_is_valid(CMD_INVALID));
    TEST_ASSERT_FALSE(command_is_valid(999));
    TEST_ASSERT_FALSE(command_is_valid(-1));
}

/* ================================================================
 * T2: 命令类别判断测试
 * ================================================================ */

TEST_CASE("command_category_scene", "command_handler")
{
    TEST_ASSERT_EQUAL(CMD_CATEGORY_SCENE, command_get_category(CMD_SCENE_FOCUS));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_SCENE, command_get_category(CMD_SCENE_MEETING));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_SCENE, command_get_category(CMD_SCENE_PRESENTATION));
}

TEST_CASE("command_category_brightness", "command_handler")
{
    TEST_ASSERT_EQUAL(CMD_CATEGORY_BRIGHT, command_get_category(CMD_BRIGHT_UP));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_BRIGHT, command_get_category(CMD_BRIGHT_DOWN));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_BRIGHT, command_get_category(CMD_BRIGHT_MAX));
}

TEST_CASE("command_category_cct", "command_handler")
{
    TEST_ASSERT_EQUAL(CMD_CATEGORY_CCT, command_get_category(CMD_CCT_WARM));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_CCT, command_get_category(CMD_CCT_COOL));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_CCT, command_get_category(CMD_CCT_NATURAL));
}

TEST_CASE("command_category_power", "command_handler")
{
    TEST_ASSERT_EQUAL(CMD_CATEGORY_POWER, command_get_category(CMD_POWER_ON));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_POWER, command_get_category(CMD_POWER_OFF));
}

TEST_CASE("command_category_invalid", "command_handler")
{
    TEST_ASSERT_EQUAL(CMD_CATEGORY_INVALID, command_get_category(CMD_INVALID));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_INVALID, command_get_category(0));
    TEST_ASSERT_EQUAL(CMD_CATEGORY_INVALID, command_get_category(999));
}

/* ================================================================
 * T3: 命令处理器初始化测试
 * ================================================================ */

TEST_CASE("command_handler_init", "command_handler")
{
    int ret = command_handler_init();
    TEST_ASSERT_EQUAL(0, ret);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL(MODE_AUTO, state->mode);
}

/* ================================================================
 * T4: 场景切换测试
 * ================================================================ */

TEST_CASE("scene_preset_valid", "command_handler")
{
    const scene_preset_t *preset = command_handler_get_scene_preset(CMD_SCENE_FOCUS);
    TEST_ASSERT_NOT_NULL(preset);
    TEST_ASSERT_EQUAL_STRING("专注模式", preset->name);
    TEST_ASSERT_EQUAL(60, preset->params.brightness);
    TEST_ASSERT_EQUAL(5000, preset->params.color_temp);
}

TEST_CASE("scene_preset_invalid", "command_handler")
{
    const scene_preset_t *preset = command_handler_get_scene_preset(999);
    TEST_ASSERT_NULL(preset);
}

TEST_CASE("scene_apply", "command_handler")
{
    command_handler_init();
    
    int ret = command_handler_apply_scene(CMD_SCENE_FOCUS);
    TEST_ASSERT_EQUAL(0, ret);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_EQUAL(CMD_SCENE_FOCUS, state->current_scene);
    TEST_ASSERT_EQUAL(60, state->light.brightness);
    TEST_ASSERT_EQUAL(5000, state->light.color_temp);
}

/* ================================================================
 * T5: 亮度调节测试
 * ================================================================ */

TEST_CASE("brightness_adjust_up", "command_handler")
{
    command_handler_init();
    
    /* 初始亮度 50% */
    const system_state_t *state = command_handler_get_state();
    int initial_brightness = state->light.brightness;
    
    /* 调亮 */
    command_result_t result = command_handler_process(CMD_BRIGHT_UP);
    TEST_ASSERT_EQUAL(CMD_RESULT_OK, result);
    
    state = command_handler_get_state();
    TEST_ASSERT_EQUAL(initial_brightness + 10, state->light.brightness);
}

TEST_CASE("brightness_adjust_max", "command_handler")
{
    command_handler_init();
    
    command_result_t result = command_handler_process(CMD_BRIGHT_MAX);
    TEST_ASSERT_EQUAL(CMD_RESULT_OK, result);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_EQUAL(100, state->light.brightness);
}

/* ================================================================
 * T6: 色温调节测试
 * ================================================================ */

TEST_CASE("cct_adjust_warm", "command_handler")
{
    command_handler_init();
    
    command_result_t result = command_handler_process(CMD_CCT_WARM);
    TEST_ASSERT_EQUAL(CMD_RESULT_OK, result);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_EQUAL(2700, state->light.color_temp);
}

TEST_CASE("cct_adjust_cool", "command_handler")
{
    command_handler_init();
    
    command_result_t result = command_handler_process(CMD_CCT_COOL);
    TEST_ASSERT_EQUAL(CMD_RESULT_OK, result);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_EQUAL(6500, state->light.color_temp);
}

/* ================================================================
 * T7: 开关控制测试
 * ================================================================ */

TEST_CASE("power_control_on", "command_handler")
{
    command_handler_init();
    
    command_result_t result = command_handler_process(CMD_POWER_ON);
    TEST_ASSERT_EQUAL(CMD_RESULT_OK, result);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_TRUE(state->light.main_light_on);
    TEST_ASSERT_TRUE(state->light.aux_light_on);
}

TEST_CASE("power_control_off", "command_handler")
{
    command_handler_init();
    
    command_handler_process(CMD_POWER_ON);
    command_result_t result = command_handler_process(CMD_POWER_OFF);
    TEST_ASSERT_EQUAL(CMD_RESULT_OK, result);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_FALSE(state->light.main_light_on);
    TEST_ASSERT_FALSE(state->light.aux_light_on);
}

/* ================================================================
 * T8: 模式切换测试
 * ================================================================ */

TEST_CASE("mode_switch_manual", "command_handler")
{
    command_handler_init();
    
    command_result_t result = command_handler_process(CMD_MODE_MANUAL);
    TEST_ASSERT_EQUAL(CMD_RESULT_OK, result);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_EQUAL(MODE_MANUAL, state->mode);
}

TEST_CASE("restore_auto", "command_handler")
{
    command_handler_init();
    
    command_handler_process(CMD_MODE_MANUAL);
    int ret = command_handler_restore_auto();
    TEST_ASSERT_EQUAL(0, ret);
    
    const system_state_t *state = command_handler_get_state();
    TEST_ASSERT_EQUAL(MODE_AUTO, state->mode);
    TEST_ASSERT_FALSE(state->voice_active);
}

/* ================================================================
 * T9: 无效命令测试
 * ================================================================ */

TEST_CASE("invalid_command", "command_handler")
{
    command_handler_init();
    
    command_result_t result = command_handler_process(CMD_INVALID);
    TEST_ASSERT_EQUAL(CMD_RESULT_INVALID, result);
    
    result = command_handler_process(999);
    TEST_ASSERT_EQUAL(CMD_RESULT_INVALID, result);
}

/* ================================================================
 * T10: 命令词表测试
 * ================================================================ */

TEST_CASE("command_words_count", "command_handler")
{
    /* 验证命令词总数 >= 50 */
    TEST_ASSERT_TRUE(COMMAND_WORDS_COUNT >= 50);
}
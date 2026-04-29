/**
 * @file command_words.c
 * @brief 语音命令词定义
 * 
 * 定义支持的语音命令词 ID、字符串和映射关系
 * 支持 50+ 条命令词，覆盖场景切换、亮度/色温调节、模式切换等
 */

#include "command_words.h"

/* ================================================================
 * 命令词表 (按类别组织)
 * ================================================================ */

const command_word_t g_command_words[] = {
    /* 场景切换 */
    {CMD_SCENE_FOCUS,       "da kai zhuan zhu mo shi",    "打开专注模式"},
    {CMD_SCENE_FOCUS,       "zhuan zhu mo shi",           "专注模式"},
    {CMD_SCENE_MEETING,     "da kai hui yi mo shi",       "打开会议模式"},
    {CMD_SCENE_MEETING,     "hui yi mo shi",              "会议模式"},
    {CMD_SCENE_PRESENTATION, "da kai yan shi mo shi",     "打开演示模式"},
    {CMD_SCENE_PRESENTATION, "yan shi mo shi",            "演示模式"},
    {CMD_SCENE_RELAX,       "da kai xiu xi mo shi",       "打开休息模式"},
    {CMD_SCENE_RELAX,       "xiu xi mo shi",              "休息模式"},
    {CMD_SCENE_BRIGHT,      "da kai ming liang mo shi",   "打开明亮模式"},
    {CMD_SCENE_BRIGHT,      "ming liang mo shi",          "明亮模式"},
    {CMD_SCENE_NIGHT,       "da kai ye jian mo shi",      "打开夜间模式"},
    {CMD_SCENE_NIGHT,       "ye jian mo shi",             "夜间模式"},
    
    /* 亮度调节 */
    {CMD_BRIGHT_UP,         "diao liang yi dian",         "调亮一点"},
    {CMD_BRIGHT_UP,         "liang yi dian",              "亮一点"},
    {CMD_BRIGHT_DOWN,       "diao an yi dian",            "调暗一点"},
    {CMD_BRIGHT_DOWN,       "an yi dian",                 "暗一点"},
    {CMD_BRIGHT_MAX,        "zui liang",                  "最亮"},
    {CMD_BRIGHT_MAX,        "diao dao zui liang",         "调到最亮"},
    {CMD_BRIGHT_MIN,        "zui an",                     "最暗"},
    {CMD_BRIGHT_MIN,        "diao dao zui an",            "调到最暗"},
    {CMD_BRIGHT_20,         "liang du bai fen zhi er shi", "亮度百分之二十"},
    {CMD_BRIGHT_40,         "liang du bai fen zhi si shi", "亮度百分之四十"},
    {CMD_BRIGHT_60,         "liang du bai fen zhi liu shi", "亮度百分之六十"},
    {CMD_BRIGHT_80,         "liang du bai fen zhi ba shi", "亮度百分之八十"},
    {CMD_BRIGHT_100,        "liang du bai fen zhi yi bai", "亮度百分之一百"},
    
    /* 色温调节 */
    {CMD_CCT_WARM,         "huan cheng nuan guang",      "换成暖光"},
    {CMD_CCT_WARM,         "nuan guang",                  "暖光"},
    {CMD_CCT_COOL,         "huan cheng leng guang",      "换成冷光"},
    {CMD_CCT_COOL,         "leng guang",                  "冷光"},
    {CMD_CCT_WHITE,        "huan cheng bai guang",       "换成白光"},
    {CMD_CCT_WHITE,        "bai guang",                   "白光"},
    {CMD_CCT_NATURAL,      "huan cheng zi ran guang",    "换成自然光"},
    {CMD_CCT_NATURAL,      "zi ran guang",               "自然光"},
    {CMD_CCT_WARMER,       "nuan yi dian",               "暖一点"},
    {CMD_CCT_COOLER,       "leng yi dian",               "冷一点"},
    
    /* 开关控制 */
    {CMD_POWER_ON,         "da kai deng",                "打开灯"},
    {CMD_POWER_ON,         "kai deng",                    "开灯"},
    {CMD_POWER_OFF,        "guan bi deng",               "关闭灯"},
    {CMD_POWER_OFF,        "guan deng",                   "关灯"},
    {CMD_POWER_MAIN_ON,    "da kai zhu deng",            "打开主灯"},
    {CMD_POWER_MAIN_OFF,   "guan bi zhu deng",           "关闭主灯"},
    {CMD_POWER_AUX_ON,     "da kai fu zhu deng",         "打开辅助灯"},
    {CMD_POWER_AUX_OFF,    "guan bi fu zhu deng",        "关闭辅助灯"},
    {CMD_POWER_ALL_ON,     "quan bu da kai",             "全部打开"},
    {CMD_POWER_ALL_OFF,    "quan bu guan bi",            "全部关闭"},
    
    /* 模式切换 */
    {CMD_MODE_AUTO,        "qie huan zi dong mo shi",    "切换自动模式"},
    {CMD_MODE_AUTO,        "zi dong mo shi",             "自动模式"},
    {CMD_MODE_MANUAL,      "qie huan shou dong mo shi",  "切换手动模式"},
    {CMD_MODE_MANUAL,      "shou dong mo shi",           "手动模式"},
    {CMD_MODE_VOICE,       "yu yin kong zhi mo shi",     "语音控制模式"},
    
    /* 查询状态 */
    {CMD_QUERY_BRIGHTNESS, "xian zai liang du duo shao", "现在亮度多少"},
    {CMD_QUERY_BRIGHTNESS, "cha xun liang du",           "查询亮度"},
    {CMD_QUERY_CCT,        "xian zai se wen duo shao",   "现在色温多少"},
    {CMD_QUERY_CCT,        "cha xun se wen",             "查询色温"},
    {CMD_QUERY_MODE,       "xian zai shi shen me mo shi", "现在是什么模式"},
    {CMD_QUERY_MODE,       "cha xun mo shi",             "查询模式"},
    {CMD_QUERY_SCENE,      "xian zai shi shen me chang jing", "现在是什么场景"},
    {CMD_QUERY_SCENE,      "cha xun chang jing",         "查询场景"},
    {CMD_QUERY_ALL,        "hui bao zhuang tai",         "汇报状态"},
    {CMD_QUERY_ALL,        "cha xun suo you zhuang tai", "查询所有状态"},
    
    /* 取消/恢复 */
    {CMD_CANCEL_ACTION,    "qu xiao",                    "取消"},
    {CMD_CANCEL_ACTION,    "qu xiao cao zuo",            "取消操作"},
    {CMD_RESTORE_AUTO,     "hui fu zi dong",             "恢复自动"},
    {CMD_RESTORE_AUTO,     "hui fu zi dong kong zhi",   "恢复自动控制"},
    {CMD_UNDO,             "che xiao",                   "撤销"},
    {CMD_UNDO,             "che xiao shang yi bu",       "撤销上一步"},
    
    /* 特殊命令 */
    {CMD_SPECIAL_HELP,     "ni neng zuo shen me",        "你能做什么"},
    {CMD_SPECIAL_HELP,     "bang zhu",                    "帮助"},
    {CMD_SPECIAL_SLEEP,    "xiu mian",                   "休眠"},
    {CMD_SPECIAL_SLEEP,    "jin ru xiu mian",            "进入休眠"},
};

/* 命令词总数 */
const size_t g_command_words_count = sizeof(g_command_words) / sizeof(g_command_words[0]);

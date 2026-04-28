/**
 * @file command_words.h
 * @brief 语音命令词定义
 * 
 * 定义支持的语音命令词 ID、字符串和映射关系
 * 支持 50+ 条命令词，覆盖场景切换、亮度/色温调节、模式切换等
 */

#ifndef COMMAND_WORDS_H
#define COMMAND_WORDS_H

#include <stdint.h>

/* ================================================================
 * 命令词 ID 定义
 * ================================================================ */

/* 场景切换命令 (100-109) */
#define CMD_SCENE_BASE          100
#define CMD_SCENE_FOCUS         100  /* "打开专注模式" / "专注模式" */
#define CMD_SCENE_MEETING       101  /* "打开会议模式" / "会议模式" */
#define CMD_SCENE_PRESENTATION  102  /* "打开演示模式" / "演示模式" */
#define CMD_SCENE_RELAX         103  /* "打开休息模式" / "休息模式" */
#define CMD_SCENE_BRIGHT        104  /* "打开明亮模式" / "明亮模式" */
#define CMD_SCENE_NIGHT         105  /* "打开夜间模式" / "夜间模式" */

/* 亮度调节命令 (200-219) */
#define CMD_BRIGHT_BASE         200
#define CMD_BRIGHT_UP           200  /* "调亮一点" / "亮一点" */
#define CMD_BRIGHT_DOWN         201  /* "调暗一点" / "暗一点" */
#define CMD_BRIGHT_MAX          202  /* "最亮" / "调到最亮" */
#define CMD_BRIGHT_MIN          203  /* "最暗" / "调到最暗" */
#define CMD_BRIGHT_20           204  /* "亮度百分之二十" */
#define CMD_BRIGHT_40           205  /* "亮度百分之四十" */
#define CMD_BRIGHT_60           206  /* "亮度百分之六十" */
#define CMD_BRIGHT_80           207  /* "亮度百分之八十" */
#define CMD_BRIGHT_100          208  /* "亮度百分之一百" */

/* 色温调节命令 (300-319) */
#define CMD_CCT_BASE            300
#define CMD_CCT_WARM            300  /* "换成暖光" / "暖光" */
#define CMD_CCT_COOL            301  /* "换成冷光" / "冷光" */
#define CMD_CCT_WHITE           302  /* "换成白光" / "白光" */
#define CMD_CCT_NATURAL         303  /* "换成自然光" / "自然光" */
#define CMD_CCT_WARMER          304  /* "暖一点" */
#define CMD_CCT_COOLER         305  /* "冷一点" */

/* 开关控制命令 (400-419) */
#define CMD_POWER_BASE          400
#define CMD_POWER_ON            400  /* "打开灯" / "开灯" */
#define CMD_POWER_OFF           401  /* "关闭灯" / "关灯" */
#define CMD_POWER_MAIN_ON       402  /* "打开主灯" */
#define CMD_POWER_MAIN_OFF      403  /* "关闭主灯" */
#define CMD_POWER_AUX_ON        404  /* "打开辅助灯" */
#define CMD_POWER_AUX_OFF       405  /* "关闭辅助灯" */
#define CMD_POWER_ALL_ON        406  /* "全部打开" */
#define CMD_POWER_ALL_OFF       407  /* "全部关闭" */

/* 模式切换命令 (500-519) */
#define CMD_MODE_BASE           500
#define CMD_MODE_AUTO           500  /* "切换自动模式" / "自动模式" */
#define CMD_MODE_MANUAL         501  /* "切换手动模式" / "手动模式" */
#define CMD_MODE_VOICE          502  /* "语音控制模式" */

/* 查询状态命令 (600-619) */
#define CMD_QUERY_BASE          600
#define CMD_QUERY_BRIGHTNESS    600  /* "现在亮度多少" / "查询亮度" */
#define CMD_QUERY_CCT           601  /* "现在色温多少" / "查询色温" */
#define CMD_QUERY_MODE          602  /* "现在是什么模式" / "查询模式" */
#define CMD_QUERY_SCENE         603  /* "现在是什么场景" / "查询场景" */
#define CMD_QUERY_ALL           604  /* "汇报状态" / "查询所有状态" */

/* 取消/恢复命令 (700-719) */
#define CMD_CANCEL_BASE         700
#define CMD_CANCEL_ACTION       700  /* "取消" / "取消操作" */
#define CMD_RESTORE_AUTO        701  /* "恢复自动" / "恢复自动控制" */
#define CMD_UNDO                702  /* "撤销" / "撤销上一步" */

/* 特殊命令 (900-999) */
#define CMD_SPECIAL_BASE        900
#define CMD_SPECIAL_HELP        900  /* "你能做什么" / "帮助" */
#define CMD_SPECIAL_SLEEP       901  /* "休眠" / "进入休眠" */

/* 无效命令 */
#define CMD_INVALID             0

/* ================================================================
 * 命令词字符串定义
 * ================================================================ */

/**
 * @brief 命令词条目结构
 */
typedef struct {
    int command_id;         /* 命令 ID */
    const char *phrase;     /* 命令词字符串 (拼音) */
    const char *desc;       /* 命令描述 */
} command_word_t;

/**
 * @brief 命令词表 (按类别组织)
 */
static const command_word_t g_command_words[] = {
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
#define COMMAND_WORDS_COUNT (sizeof(g_command_words) / sizeof(g_command_words[0]))

/* ================================================================
 * 命令类别枚举
 * ================================================================ */

typedef enum {
    CMD_CATEGORY_SCENE,    /* 场景切换 */
    CMD_CATEGORY_BRIGHT,   /* 亮度调节 */
    CMD_CATEGORY_CCT,      /* 色温调节 */
    CMD_CATEGORY_POWER,    /* 开关控制 */
    CMD_CATEGORY_MODE,     /* 模式切换 */
    CMD_CATEGORY_QUERY,    /* 查询状态 */
    CMD_CATEGORY_CANCEL,   /* 取消/恢复 */
    CMD_CATEGORY_SPECIAL,  /* 特殊命令 */
    CMD_CATEGORY_INVALID,  /* 无效类别 */
} command_category_t;

/**
 * @brief 获取命令类别
 * @param command_id 命令 ID
 * @return 命令类别
 */
static inline command_category_t command_get_category(int command_id) {
    if (command_id >= CMD_SCENE_BASE && command_id < CMD_SCENE_BASE + 10) {
        return CMD_CATEGORY_SCENE;
    } else if (command_id >= CMD_BRIGHT_BASE && command_id < CMD_BRIGHT_BASE + 20) {
        return CMD_CATEGORY_BRIGHT;
    } else if (command_id >= CMD_CCT_BASE && command_id < CMD_CCT_BASE + 20) {
        return CMD_CATEGORY_CCT;
    } else if (command_id >= CMD_POWER_BASE && command_id < CMD_POWER_BASE + 20) {
        return CMD_CATEGORY_POWER;
    } else if (command_id >= CMD_MODE_BASE && command_id < CMD_MODE_BASE + 20) {
        return CMD_CATEGORY_MODE;
    } else if (command_id >= CMD_QUERY_BASE && command_id < CMD_QUERY_BASE + 20) {
        return CMD_CATEGORY_QUERY;
    } else if (command_id >= CMD_CANCEL_BASE && command_id < CMD_CANCEL_BASE + 20) {
        return CMD_CATEGORY_CANCEL;
    } else if (command_id >= CMD_SPECIAL_BASE && command_id < CMD_SPECIAL_BASE + 100) {
        return CMD_CATEGORY_SPECIAL;
    }
    return CMD_CATEGORY_INVALID;
}

/**
 * @brief 检查命令是否有效
 * @param command_id 命令 ID
 * @return true 有效, false 无效
 */
static inline bool command_is_valid(int command_id) {
    return command_get_category(command_id) != CMD_CATEGORY_INVALID;
}

#endif /* COMMAND_WORDS_H */

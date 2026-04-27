# 最小任务单元 - Phase 4: 端侧学习模块

> **Phase**: 端侧学习模块 (可选)
> **范围**: 任务 LL-001 ~ LL-023
> **预计工时**: 4周

---

## 任务组1: 用户反馈 (LL-001 ~ LL-004)

| 任务ID | 任务名称 | 执行要点 | 验收标准 |
|--------|----------|----------|----------|
| LL-001 | 反馈接口设计 | 按钮/旋钮输入 | 接口可用 |
| LL-002 | 反馈数据结构 | feedback_event | 结构体定义清晰 |
| LL-003 | 反馈存储 | PSRAM 缓冲区 | 存储正常 |
| LL-004 | 反馈统计 | 偏差趋势分析 | 统计正确 |

---

## 任务组2: 数据收集 (LL-005 ~ LL-007)

| 任务ID | 任务名称 | 执行要点 | 验收标准 |
|--------|----------|----------|----------|
| LL-005 | 偏差数据标记 | AI 输出 vs 用户期望 | 标记正确 |
| LL-006 | 数据清洗 | 过滤异常值 | 数据质量高 |
| LL-007 | 数据集划分 | 训练/验证集 80/20 | 划分合理 |

---

## 任务组3: AIfES 集成 (LL-008 ~ LL-011)

| 任务ID | 任务名称 | 执行要点 | 验收标准 |
|--------|----------|----------|----------|
| LL-008 | AIfES 移植 | 编译配置 | 编译通过 |
| LL-009 | 内存分配 | 训练内存池 <64KB | 分配成功 |
| LL-010 | 前向传播验证 | 与 TFLM 一致 | 输出一致 |
| LL-011 | 反向传播验证 | 梯度计算正确 | 梯度验证通过 |

---

## 任务组4: 微调训练 (LL-012 ~ LL-015)

| 任务ID | 任务名称 | 执行要点 | 验收标准 |
|--------|----------|----------|----------|
| LL-012 | 冻结层配置 | 冻结除最后一层 | 配置正确 |
| LL-013 | 训练循环实现 | SGD 优化器 | 训练可运行 |
| LL-014 | 早停机制 | 防止过拟合 | 早停生效 |
| LL-015 | 训练验证 | 准确率测试 | 准确率 >80% |

---

## 任务组5: 模型管理 (LL-016 ~ LL-019)

| 任务ID | 任务名称 | 执行要点 | 验收标准 |
|--------|----------|----------|----------|
| LL-016 | 模型版本号 | 版本管理 | 版本号正确 |
| LL-017 | 模型备份 | 旧版本保存 | 备份可用 |
| LL-018 | 模型回滚 | 回滚机制 | 回滚成功 |
| LL-019 | Flash 存储 | NVS 持久化 | 存储正确 |

---

## 任务组6: 夜间训练 (LL-020 ~ LL-023)

| 任务ID | 任务名称 | 执行要点 | 验收标准 |
|--------|----------|----------|----------|
| LL-020 | 训练触发条件 | 数据量 >50 条 | 触发正确 |
| LL-021 | 时间窗口 | 凌晨 2-5 点 | 时间正确 |
| LL-022 | 训练监控 | 进度日志 | 日志完整 |
| LL-023 | 训练完成通知 | 结果记录 | 通知正常 |

---

## 附录: 端侧学习接口

### edge_learning.h
```c
#ifndef EDGE_LEARNING_H
#define EDGE_LEARNING_H

typedef struct {
    uint32_t timestamp;
    float features[19];
    float ai_output[2];
    float user_correction[2];
    uint8_t feedback_type;
} feedback_event_t;

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint32_t checksum;
} model_version_t;

int feedback_init(void);
int feedback_log(uint8_t type, float user_ct, float user_br);

int aifes_init(void);
int aifes_train(float *samples, float *labels, int count);
int aifes_validate(float *samples, float *labels, int count, float *accuracy);

int model_save(uint8_t slot);
int model_load(uint8_t slot);
int model_get_version(model_version_t *ver);
int model_rollback(void);

#endif
```

---

## 技术方案说明

### 推荐方案: 最后一层微调

**原因**:
- 实现简单，风险低
- 计算量小，适合 MCU
- 参数少，不易过拟合

**流程**:
1. 冻结特征提取层 (雷达/声音模型)
2. 只训练融合层最后一层
3. 使用 SGD 优化器
4. 早停防止过拟合

### AIfES 框架 (可选升级)

**特点**:
- 专为 MCU 设计的微型训练库
- 支持反向传播、SGD、Adam
- 成熟度高，适合生产环境

**集成步骤**:
1. 添加 AIfES 模块到 west.yml
2. 配置训练内存池
3. 实现前向/反向传播
4. 集成到夜间训练任务

---

*文档版本: 1.0 | 创建日期: 2026-04-27*

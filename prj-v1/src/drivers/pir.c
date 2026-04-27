/*
 * PIR Sensor Driver Implementation
 * Phase 1: HW-005 ~ HW-008
 *
 * GPIO4 - PIR人体传感器
 * - 初始化: 输入模式 + 内部上拉
 * - 轮询读取: gpio_pin_get()
 * - 中断驱动: GPIO_INT_EDGE_BOTH (双边沿触发)
 * - 软件消抖: 50ms防抖窗口
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "pir.h"

LOG_MODULE_REGISTER(pir, CONFIG_LOG_DEFAULT_LEVEL);

/* ================================================================
 * PIR硬件定义: GPIO0 引脚4
 * ================================================================ */
#define PIR_GPIO_NODE  DT_NODELABEL(gpio0)
#define PIR_GPIO_PIN   4

/* 软件消抖参数 */
#define PIR_DEBOUNCE_MS 50

/* ================================================================
 * 内部状态
 * ================================================================ */
static const struct device *pir_dev = NULL;

/* 消抖状态 */
static bool            debounced_status = false;
static uint64_t        last_change_time = 0;
static struct k_spinlock pir_lock;

/* 中断回调中使用: 原始电平变化 */
static struct gpio_callback pir_cb_data;

/* ================================================================
 * 中断回调 (HW-007)
 * 双边沿触发时调用, 在系统工作队列上下文中处理
 * ================================================================ */
static void pir_isr_handler(const struct device *dev,
			    struct gpio_callback *cb, uint32_t pins)
{
	/* 在中断上下文中, 仅更新raw值, 消抖由pir_get_status()处理 */
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	/* 实际消抖在pir_get_status()轮询时完成;
	 * 中断仅用于即时唤醒/标记变化 */
	LOG_DBG("PIR interrupt triggered on pin mask 0x%08x", pins);
}

/* ================================================================
 * GPIO初始化 (HW-005)
 * ================================================================ */
int pir_init(void)
{
	int ret;

	pir_dev = DEVICE_DT_GET(PIR_GPIO_NODE);
	if (!device_is_ready(pir_dev)) {
		LOG_ERR("PIR GPIO device not ready");
		return -ENODEV;
	}
	LOG_INF("PIR GPIO device ready: %s", pir_dev->name);

	/* 配置GPIO4: 输入模式 + 内部上拉 */
	ret = gpio_pin_configure(pir_dev, PIR_GPIO_PIN,
				 GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO4 (err %d)", ret);
		return ret;
	}
	LOG_INF("PIR GPIO4 configured: input + pull-up");

	/* 读取初始状态 */
	debounced_status = (gpio_pin_get(pir_dev, PIR_GPIO_PIN) == 1);
	last_change_time = k_uptime_get();
	LOG_INF("PIR initial status: %s",
		debounced_status ? "PRESENCE" : "NO_PRESENCE");

	/* 配置GPIO中断: 双边沿触发 (HW-007) */
	ret = gpio_pin_interrupt_configure(pir_dev, PIR_GPIO_PIN,
					   GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		LOG_ERR("Failed to configure PIR interrupt (err %d)", ret);
		return ret;
	}

	/* 注册并启用中断回调 */
	gpio_init_callback(&pir_cb_data, pir_isr_handler, BIT(PIR_GPIO_PIN));
	ret = gpio_add_callback(pir_dev, &pir_cb_data);
	if (ret < 0) {
		LOG_ERR("Failed to add PIR callback (err %d)", ret);
		return ret;
	}
	LOG_INF("PIR interrupt enabled (edge-both)");

	return 0;
}

/* ================================================================
 * 获取消抖后的状态 (HW-006 + HW-008)
 * ================================================================ */
bool pir_get_status(void)
{
	if (!pir_dev) {
		return false;
	}

	bool raw = (gpio_pin_get(pir_dev, PIR_GPIO_PIN) == 1);

	k_spinlock_key_t key = k_spin_lock(&pir_lock);

	/* 软件消抖: 状态未变化则直接返回 */
	if (raw == debounced_status) {
		k_spin_unlock(&pir_lock, key);
		return debounced_status;
	}

	/* 状态变化, 检查消抖时间窗口 */
	uint64_t now = k_uptime_get();
	if ((now - last_change_time) < PIR_DEBOUNCE_MS) {
		/* 在防抖窗口内, 忽略此次变化 */
		k_spin_unlock(&pir_lock, key);
		return debounced_status;
	}

	/* 确认状态变化 */
	debounced_status = raw;
	last_change_time = now;
	k_spin_unlock(&pir_lock, key);

	LOG_INF("PIR status changed: %s",
		debounced_status ? "PRESENCE" : "NO_PRESENCE");

	return debounced_status;
}

/* ================================================================
 * 获取原始引脚电平(未消抖)
 * ================================================================ */
bool pir_get_raw(void)
{
	if (!pir_dev) {
		return false;
	}
	return (gpio_pin_get(pir_dev, PIR_GPIO_PIN) == 1);
}

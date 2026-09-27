#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#define HALLS_DEVICE_NAME "halls"

enum {
	KEYCODE_SLIDER_UP = 594,
	KEYCODE_SLIDER_DOWN,
};

enum {
	STATE_SLIDER_UP = 0,
	STATE_SLIDER_DOWN,
	STATE_SLIDER_SLIDING,
};

struct halls_data {
	struct delayed_work notify_work;
	struct wakeup_source *wakelock;
	struct input_dev *input;

	int first_gpio;
	int second_gpio;
	int first_irq;
	int second_irq;
	bool gpio_requested;
	bool irq_requested;
};

extern int elliptic_set_hall_state(int state);
static void halls_send_input(struct halls_data *hdata, int state) {
	int keycode;

	if (state == STATE_SLIDER_UP)
		keycode = KEYCODE_SLIDER_UP;
	else if (state == STATE_SLIDER_DOWN)
		keycode = KEYCODE_SLIDER_DOWN;
	else
		return;

	input_report_key(hdata->input, keycode, 1);
	input_sync(hdata->input);
	input_report_key(hdata->input, keycode, 0);
	input_sync(hdata->input);
}

static void halls_notify_work_func(struct work_struct *work) {
	struct halls_data *hdata = container_of(to_delayed_work(work),
			struct halls_data, notify_work);
	int first_value, second_value;
	int state;

	first_value = gpio_get_value_cansleep(hdata->first_gpio);
	second_value = gpio_get_value_cansleep(hdata->second_gpio);

	if (first_value == 1 && second_value == 0)
		state = STATE_SLIDER_UP;
	else if (first_value == 0 && second_value == 1)
		state = STATE_SLIDER_DOWN;
	else
		state = STATE_SLIDER_SLIDING;

	elliptic_set_hall_state(state);
	halls_send_input(hdata, state);
}

static irqreturn_t halls_irq_handler(int irqno, void *dev_id) {
	struct halls_data *hdata = dev_id;
	__pm_wakeup_event(hdata->wakelock, 20);
	schedule_delayed_work(&hdata->notify_work, 0);
	return 0;
}

static int halls_configure_gpio(struct halls_data *hdata, int gpio,
		const char *label) {
	int ret;

	ret = gpio_request(gpio, label);
	if (ret) {
		pr_err("%s: failed to request GPIO %d\n", __func__, gpio);
		return ret;
	}
	hdata->gpio_requested = true;

	ret = gpio_direction_input(gpio);
	if (ret) {
		pr_err("%s: failed to set GPIO %d direction to input\n", __func__, gpio);
		return ret;
	}

	return 0;
}

static int halls_configure_irq(struct halls_data *hdata, int gpio, int *irq,
		const char *label) {
	int ret;

	*irq = gpio_to_irq(gpio);
	if (*irq < 0) {
		pr_err("%s: failed to map GPIO %d to IRQ\n", __func__, gpio);
		return *irq;
	}

	ret = request_irq(*irq, halls_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
			| IRQF_ONESHOT, label, hdata);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d IRQ\n", __func__, gpio);
		*irq = -1;
		return ret;
	}
	hdata->irq_requested = true;

	irq_set_irq_wake(*irq, 1);

	return 0;
}

static void halls_free_resources(struct halls_data *hdata) {
	if (hdata->irq_requested) {
		irq_set_irq_wake(hdata->first_irq, 0);
		free_irq(hdata->first_irq, hdata);
		irq_set_irq_wake(hdata->second_irq, 0);
		free_irq(hdata->second_irq, hdata);
		hdata->irq_requested = false;
	}
	if (hdata->gpio_requested) {
		gpio_free(hdata->first_gpio);
		gpio_free(hdata->second_gpio);
		hdata->gpio_requested = false;
	}
	if (hdata->wakelock) {
		__pm_wakeup_event(hdata->wakelock, 0);
		wakeup_source_unregister(hdata->wakelock);
		hdata->wakelock = NULL;
	}
}

static int halls_probe(struct platform_device *pdev) {
	struct device_node *np = pdev->dev.of_node;
	struct halls_data *hdata;
	enum of_gpio_flags flags = 0;
	int ret;

	if (!np)
		return -ENODEV;

	hdata = kzalloc(sizeof(*hdata), GFP_KERNEL);
	if (!hdata) {
		pr_err("%s: failed to allocate memory for driver data\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, hdata);

	hdata->input = input_allocate_device();
	if (!hdata->input) {
		pr_err("%s: failed to allocate memory for input device\n", __func__);
		ret = -ENOMEM;
		goto free_halls;
	}

	hdata->input->name = HALLS_DEVICE_NAME;
	input_set_drvdata(hdata->input, hdata);

	set_bit(EV_KEY, hdata->input->evbit);
	set_bit(KEYCODE_SLIDER_UP, hdata->input->keybit);
	set_bit(KEYCODE_SLIDER_DOWN, hdata->input->keybit);

	ret = input_register_device(hdata->input);
	if (ret) {
		pr_err("%s: failed to register input device\n", __func__);
		goto free_input;
	}

	/*
	 * The hall state is derived from the raw pin level, so the device tree
	 * has to describe both GPIOs as GPIO_ACTIVE_HIGH.
	 */
	ret = of_get_named_gpio_flags(np, "qcom,hall-gpios", 0, &flags);
	if (ret < 0) {
		pr_err("%s: failed to get first hall GPIO\n", __func__);
		ret = -EINVAL;
		goto unregister_input;
	}
	hdata->first_gpio = ret;
	if (flags & OF_GPIO_ACTIVE_LOW)
		pr_warn("%s: first hall GPIO should be GPIO_ACTIVE_HIGH\n", __func__);

	ret = of_get_named_gpio_flags(np, "qcom,hall-gpios", 1, &flags);
	if (ret < 0) {
		pr_err("%s: failed to get second hall GPIO\n", __func__);
		ret = -EINVAL;
		goto unregister_input;
	}
	hdata->second_gpio = ret;
	if (flags & OF_GPIO_ACTIVE_LOW)
		pr_warn("%s: second hall GPIO should be GPIO_ACTIVE_HIGH\n", __func__);

	ret = halls_configure_gpio(hdata, hdata->first_gpio, "halls-first");
	if (ret)
		goto unregister_input;

	ret = halls_configure_gpio(hdata, hdata->second_gpio, "halls-second");
	if (ret)
		goto free_gpios;

	INIT_DELAYED_WORK(&hdata->notify_work, halls_notify_work_func);

	hdata->wakelock = wakeup_source_register("halls-ws");
	if (!hdata->wakelock) {
		pr_err("%s: failed to register wakeup source\n", __func__);
		ret = -EINVAL;
		goto free_gpios;
	}

	ret = halls_configure_irq(hdata, hdata->first_gpio, &hdata->first_irq,
			"halls-first");
	if (ret)
		goto free_wakelock;

	ret = halls_configure_irq(hdata, hdata->second_gpio, &hdata->second_irq,
			"halls-second");
	if (ret) {
		free_irq(hdata->first_irq, hdata);
		hdata->irq_requested = false;
		goto free_wakelock;
	}

	return 0;

free_wakelock:
	wakeup_source_unregister(hdata->wakelock);
	hdata->wakelock = NULL;
free_gpios:
	hdata->gpio_requested = false;
	gpio_free(hdata->first_gpio);
	gpio_free(hdata->second_gpio);
unregister_input:
	input_unregister_device(hdata->input);
free_input:
	input_free_device(hdata->input);
free_halls:
	kfree(hdata);
	return ret;
}

static int halls_remove(struct platform_device *pdev) {
	struct halls_data *hdata = platform_get_drvdata(pdev);

	if (!hdata)
		return 0;

	cancel_delayed_work_sync(&hdata->notify_work);
	halls_free_resources(hdata);
	input_unregister_device(hdata->input);
	input_free_device(hdata->input);
	kfree(hdata);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id halls_of_match[] = {
	{ .compatible = "qcom,halls" },
	{ }
};

static struct platform_driver halls_driver = {
	.probe = halls_probe,
	.remove = halls_remove,
	.driver = {
		.name = HALLS_DEVICE_NAME,
		.of_match_table = halls_of_match,
	},
};

module_platform_driver(halls_driver);

MODULE_LICENSE("GPL v2");

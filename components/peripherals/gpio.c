#include "peripherals/gpio.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

/* CONN_1_NODE is the devicetree node identifier for the "led0" alias. */
#define CONN_1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec connection1_enabled = GPIO_DT_SPEC_GET(CONN_1_NODE, gpios);  // GPIO P.04

/* BOOT_PIN is the devicetree node identifier for the "led1" alias. */
#define BOOT_PIN DT_ALIAS(led2)
static const struct gpio_dt_spec boot_active = GPIO_DT_SPEC_GET(BOOT_PIN, gpios);

/* CONN_0_NODE is the devicetree node identifier for the "led3" alias. */
#define CONN_0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec connection0_enabled = GPIO_DT_SPEC_GET(CONN_0_NODE, gpios);  // GPIO P.03

int32_t gpio_init()
{
  if (!device_is_ready(connection1_enabled.port))
  {
    return -EIO;
  }

  if (!device_is_ready(boot_active.port))
  {
    return -EIO;
  }

  if (!device_is_ready(connection0_enabled.port))
  {
    return -EIO;
  }

	int32_t err = gpio_pin_configure_dt(&connection1_enabled, GPIO_OUTPUT_INACTIVE);
	if (err < 0)
	{
		return err;
	}

  err = gpio_pin_configure_dt(&boot_active, GPIO_OUTPUT_INACTIVE);
  if (err < 0)
  {
    return err;
  }

  err = gpio_pin_configure_dt(&connection0_enabled, GPIO_OUTPUT_INACTIVE);
  if (err < 0)
  {
    return err;
  }

  gpio_pin_set_dt(&connection1_enabled, GPIO_LOW_STATE);
  gpio_pin_set_dt(&boot_active, GPIO_LOW_STATE);
  gpio_pin_set_dt(&connection0_enabled, GPIO_LOW_STATE);

  return 0;
}

void gpio_set(gpio_node_t node, gpio_state_t state)
{
  switch (node)
  {
    case GPIO_ID1_NODE:
    {
      gpio_pin_set_dt(&connection1_enabled, state);
      break;
    }
    case GPIO_BOOT_NODE:
    {
      gpio_pin_set_dt(&boot_active, state);
      break;
    }
    case GPIO_ID0_NODE:
    {
      gpio_pin_set_dt(&connection0_enabled, state);
      break;
    }
  }
}
// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: duerapp_key.c
 * Auth: zhong shuai (zhongshuai@baidu.com)
 * Desc: Show how to report key event
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "lightduer_types.h"
#include "lightduer_key.h"
#include "duerapp_config.h"

/**
 * Brief:
 * This test code shows how to repo key to key handler framework
 *
 * GPIO status:
 * GPIO18: output I2C:SDA
 * GPIO19: output
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Test:
 * Connect GPIO18 with GPIO4
 * Connect GPIO19 with GPIO5
 * Generate pulses on GPIO18/19, that triggers interrupt on GPIO4/5
 *
 */

#define GPIO_OUTPUT_IO_0        18
#define GPIO_OUTPUT_IO_1        19
#define GPIO_OUTPUT_PIN_SEL     ((1 << GPIO_OUTPUT_IO_0) | (1 << GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_IO_0         4
#define GPIO_INPUT_IO_1         5
#define GPIO_INPUT_PIN_SEL      ((1 << GPIO_INPUT_IO_0) | (1 << GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT   0

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    esp_err_t ret;
    uint32_t gpio_num = (uint32_t)arg;

    ret = duer_report_key_event(STOP_KEY, gpio_num);
    if (ret != DUER_OK) {
        // Error handle
    }
}

void initialize_gpio()
{
    gpio_config_t io_conf;
    // Disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    // Set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // Bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    // Disable pull-down mode
    io_conf.pull_down_en = 0;
    // Disable pull-up mode
    io_conf.pull_up_en = 0;
    // Configure GPIO with the given settings
    gpio_config(&io_conf);

    // Interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    // Bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    // Set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // Enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // Change gpio intrrupt type for one pin
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    // Install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // Hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *) GPIO_INPUT_IO_0);
    // Hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *) GPIO_INPUT_IO_1);

    // Remove isr handler for gpio number.
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    // Hook isr handler for specific gpio pin again
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *) GPIO_INPUT_IO_0);
}

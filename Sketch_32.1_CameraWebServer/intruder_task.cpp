/*
intruder_task.cpp
simple FreeRTOS queue for managing intruder detection without blocking the CPU: 
sound the buzzer, pulse the red led, and send an alert on WhatsApp
*/ 
#include "intruder_task.h"
#include "hardware_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


static QueueHandle_t intruderQueue = NULL;

static void intruder_task(void *arg) {
    uint32_t msg;
    while (true) {
        if (xQueueReceive(intruderQueue, &msg, portMAX_DELAY)) {
            hardware_buzz();           
            hardware_led_pulse(&intruder_led, 5000); 
            sendIntruderAlert();          
        }
    }
}

void intruder_task_init(void) {
    if (intruderQueue) return;
    intruderQueue = xQueueCreate(4, sizeof(uint32_t));
    xTaskCreatePinnedToCore(intruder_task, "intruder_task", 8192, NULL, 4, NULL, 1);
}

bool intruder_queue_send(uint32_t msg) {
    if (!intruderQueue) return false;
    return xQueueSend(intruderQueue, &msg, 0) == pdTRUE;
}

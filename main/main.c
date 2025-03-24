/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint PIN_TRIGGER = 2;
const uint PIN_ECHO = 3;

QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;
SemaphoreHandle_t xSemaphoreTrigger;

void pin_callback(uint gpio, uint32_t events) {
    static uint32_t start_time = 0;
    uint32_t time = to_us_since_boot(get_absolute_time());
    
    if (events == 0x8) { 
        start_time = time;
    } else {
        uint32_t diff = time - start_time;
        xQueueSendFromISR(xQueueTime, &diff, 0);
        xSemaphoreGiveFromISR(xSemaphoreTrigger, 0);  
    }
}

void trigger_task(void *p){
    printf("Trigger Task\n");
    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);

    while (true) {
        gpio_put(PIN_TRIGGER, 1);
        vTaskDelay(pdMS_TO_TICKS(10)); 
        gpio_put(PIN_TRIGGER, 0);
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

void echo_task(void *p){
    printf("Echo Task\n");
    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PIN_ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);

    int duracao = 0;
    while (true) {
        if (xQueueReceive(xQueueTime, &duracao, portMAX_DELAY) == pdTRUE) {
            float distancia = (duracao * 0.0343) / 2;
            printf("Distancia: %f cm\n", distancia);
            xQueueSend(xQueueDistance, &distancia, 0);
        }
    }
}

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void oled_task(void *p){
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    float distancia = 0;
    while (true) {
        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (xQueueReceive(xQueueDistance, &distancia, pdMS_TO_TICKS(100))== pdTRUE) {
                gfx_clear_buffer(&disp);
                char dist_str[14];
                if (distancia > 400){
                    gfx_draw_string(&disp, 0, 0, 1, "Erro");
                } else {
                    snprintf(dist_str, sizeof(dist_str), "Dist: %.2f cm", distancia);
                    printf("%s\n", dist_str);
                    gfx_draw_string(&disp, 0, 0, 1, dist_str);
                    int progresso = (int) distancia * 128 / 200;
                    gfx_draw_line(&disp, 0, 27, progresso, 27);
                }
                gfx_show(&disp);
            } 
        }
    }
}

int main() {
    stdio_init_all();

    // Cria as filas
    xQueueTime = xQueueCreate(32, sizeof(int64_t)); // Fila para o tempo do pulso Echo
    xQueueDistance = xQueueCreate(32, sizeof(float)); // Fila para a distância em cm

    // Cria o semáforo
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    // Cria as tarefas
    xTaskCreate(trigger_task, "Trigger", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4096, NULL, 1, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    while (true)
        ;
}
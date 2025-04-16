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

// #include "tusb.h"

const int BTN_LARANJA = 22;
const int BTN_AZUL = 21;
const int BTN_AMARELO = 20;
const int BTN_VERMELHO = 19;
const int BTN_VERDE = 18;

// Flags para cada botão
volatile bool btn_laranja_flag = false;
volatile bool btn_azul_flag = false;
volatile bool btn_amarelo_flag = false;
volatile bool btn_vermelho_flag = false;
volatile bool btn_verde_flag = false;

void btn_note_callback(uint gpio, uint32_t events)
{
    if (events & GPIO_IRQ_EDGE_FALL)
    {
        if (gpio == BTN_LARANJA)
        {
            btn_laranja_flag = true;
        }
        else if (gpio == BTN_AZUL)
        {
            btn_azul_flag = true;
        }
        else if (gpio == BTN_AMARELO)
        {
            btn_amarelo_flag = true;
        }
        else if (gpio == BTN_VERMELHO)
        {
            btn_vermelho_flag = true;
        }
        else if (gpio == BTN_VERDE)
        {
            btn_verde_flag = true;
        }
    }
    else if (events & GPIO_IRQ_EDGE_RISE)
    {
        if (gpio == BTN_LARANJA)
        {
            btn_laranja_flag = false;
        }
        else if (gpio == BTN_AZUL)
        {
            btn_azul_flag = false;
        }
        else if (gpio == BTN_AMARELO)
        {
            btn_amarelo_flag = false;
        }
        else if (gpio == BTN_VERMELHO)
        {
            btn_vermelho_flag = false;
        }
        else if (gpio == BTN_VERDE)
        {
            btn_verde_flag = false;
        }
    }
}

// inicializa todos os botoes
void init_buttons()
{
    gpio_init(BTN_LARANJA);
    gpio_set_dir(BTN_LARANJA, GPIO_IN);
    gpio_pull_up(BTN_LARANJA);
    gpio_init(BTN_AZUL);
    gpio_set_dir(BTN_AZUL, GPIO_IN);
    gpio_pull_up(BTN_AZUL);
    gpio_init(BTN_AMARELO);
    gpio_set_dir(BTN_AMARELO, GPIO_IN);
    gpio_pull_up(BTN_AMARELO);
    gpio_init(BTN_VERMELHO);
    gpio_set_dir(BTN_VERMELHO, GPIO_IN);
    gpio_pull_up(BTN_VERMELHO);
    gpio_init(BTN_VERDE);
    gpio_set_dir(BTN_VERDE, GPIO_IN);
    gpio_pull_up(BTN_VERDE);
}

void init_callbacks()
{
    gpio_set_irq_enabled_with_callback(BTN_LARANJA, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_AZUL, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_AMARELO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_VERMELHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_VERDE, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
}

void task_debug(void *p) {
    while (true) {
        printf("Verde:%d Vermelho:%d Amarelo:%d Azul:%d Laranja:%d\n",
            btn_verde_flag, btn_vermelho_flag, btn_amarelo_flag,
            btn_azul_flag, btn_laranja_flag);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


int main()
{
    stdio_init_all();
    printf("Iniciando controle Clone Hero...\n");

    // Inicializa os botões e interrupções
    init_buttons();
    init_callbacks();

    xTaskCreate(task_debug, "debug", 256, NULL, 1, NULL);
    vTaskStartScheduler();


    while (true)
        ; // nunca chega aqui
}

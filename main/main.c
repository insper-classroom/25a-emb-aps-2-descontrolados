/*
 * Clone Hero Controller with Buttons and Joystick
 * Combines button input with analog joystick control
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include <stdio.h>

// UART configuration
#define UART_ID uart0
#define BAUD_RATE 115200

// Button definitions
const int BTN_LARANJA = 22;
const int BTN_AZUL = 21;
const int BTN_AMARELO = 20;
const int BTN_VERMELHO = 18;
const int BTN_VERDE = 19;

// Pins for joystick X and Y axes
const int X_AXIS_PIN = 26;       
const int Y_AXIS_PIN = 27;

// Button flags
volatile bool btn_laranja_flag = false;
volatile bool btn_azul_flag = false;
volatile bool btn_amarelo_flag = false;
volatile bool btn_vermelho_flag = false;
volatile bool btn_verde_flag = false;

// ADC data structure
typedef struct adc {
    int axis;               
    int val;                
} adc_t;

// Queue for ADC values
QueueHandle_t xQueueADC;

// Button interrupt callback
void btn_note_callback(uint gpio, uint32_t events)
{
    if (events & GPIO_IRQ_EDGE_FALL)
    {
        if (gpio == BTN_LARANJA)
            btn_laranja_flag = true;
        else if (gpio == BTN_AZUL)
            btn_azul_flag = true;
        else if (gpio == BTN_AMARELO)
            btn_amarelo_flag = true;
        else if (gpio == BTN_VERMELHO)
            btn_vermelho_flag = true;
        else if (gpio == BTN_VERDE)
            btn_verde_flag = true;
    }
    else if (events & GPIO_IRQ_EDGE_RISE)
    {
        if (gpio == BTN_LARANJA)
            btn_laranja_flag = false;
        else if (gpio == BTN_AZUL)
            btn_azul_flag = false;
        else if (gpio == BTN_AMARELO)
            btn_amarelo_flag = false;
        else if (gpio == BTN_VERMELHO)
            btn_vermelho_flag = false;
        else if (gpio == BTN_VERDE)
            btn_verde_flag = false;
    }
}

// Initialize all buttons
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

// Setup button interrupts
void init_callbacks()
{
    gpio_set_irq_enabled_with_callback(BTN_LARANJA, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_AZUL, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_AMARELO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_VERMELHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled_with_callback(BTN_VERDE, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
}

// Convert ADC value from 0-4095 to useful range
int convert_adc_value(uint16_t adc_val) {
    // Convert from 0-4095 to -2047 to 2047
    int centered = adc_val - 2047;
    
    int scaled_value = centered / 8; // Scale adjustment

    // Create a dead zone in the center
    if (scaled_value > -30 && scaled_value < 30) {
        scaled_value = 0;
    }

    return scaled_value;
}

// Task to monitor button status and send via serial printf
void task_button_serial(void *p) {
    bool last_state[5] = {0};

    while (true) {
        bool current_state[5] = {
            btn_verde_flag,
            btn_vermelho_flag,
            btn_amarelo_flag,
            btn_azul_flag,
            btn_laranja_flag
        };

        const char* letras[5] = {"A", "S", "J", "K", "L"};

        for (int i = 0; i < 5; i++) {
            if (current_state[i] != last_state[i]) {
                printf("%s:%s\n", letras[i], current_state[i] ? "DOWN" : "UP");
                last_state[i] = current_state[i];
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task to read X axis and send to queue
void x_task(void *p) {
    uint16_t x_values[5] = {0};
    int x_index = 0;
    
    adc_t adc_data;
    adc_data.axis = 0; // X axis
    
    while (1) {
        adc_select_input(0);
        x_values[x_index] = adc_read();
        x_index = (x_index + 1) % 5;
        
        // Simple moving average filter
        uint32_t sum = 0;
        for (int i = 0; i < 5; i++) {
            sum += x_values[i];
        }
        uint16_t x_filtered = sum / 5;
        
        adc_data.val = convert_adc_value(x_filtered);
        
        if (adc_data.val != 0) {
            xQueueSend(xQueueADC, &adc_data, 0);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task to read Y axis and send to queue
void y_task(void *p) {
    uint16_t y_values[5] = {0};
    int y_index = 0;
    
    adc_t adc_data;
    adc_data.axis = 1; // Y axis
    
    while (1) {
        adc_select_input(1);
        y_values[y_index] = adc_read();
        y_index = (y_index + 1) % 5;
        
        // Simple moving average filter
        uint32_t sum = 0;
        for (int i = 0; i < 5; i++) {
            sum += y_values[i];
        }
        uint16_t y_filtered = sum / 5;
        
        adc_data.val = convert_adc_value(y_filtered);
        
        if (adc_data.val != 0) {
            xQueueSend(xQueueADC, &adc_data, 0);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task to send ADC data over UART
void uart_task(void *p) {
    adc_t adc_data;

    while (1) {
        if (xQueueReceive(xQueueADC, &adc_data, portMAX_DELAY)) {
            int valor = adc_data.val;
            uint8_t val_1 = (valor >> 8) & 0xFF;  // MSB
            uint8_t val_0 = valor & 0xFF;         // LSB

            uart_putc_raw(UART_ID, adc_data.axis);
            uart_putc_raw(UART_ID, val_0);
            uart_putc_raw(UART_ID, val_1);
            uart_putc_raw(UART_ID, 255);  // End marker
        }
    }
}

int main()
{
    stdio_init_all();
    printf("Iniciando controle Clone Hero com botoes e joystick...\n");

    // Initialize ADC for joystick
    adc_init();
    adc_gpio_init(X_AXIS_PIN);
    adc_gpio_init(Y_AXIS_PIN);
    
    // Initialize buttons and their interrupts
    init_buttons();
    init_callbacks();
    
    // Initialize UART for joystick data
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    
    // Create queue for ADC values
    xQueueADC = xQueueCreate(64, sizeof(adc_t));
    
    if (xQueueADC == NULL) {
        printf("Erro ao criar a fila ADC!\n");
        while(1); 
    }
    
    // Create tasks
    xTaskCreate(task_button_serial, "Button Serial", 512, NULL, 1, NULL);
    xTaskCreate(x_task, "X Axis Task", 256, NULL, 1, NULL);
    xTaskCreate(y_task, "Y Axis Task", 256, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART Task", 256, NULL, 2, NULL);
    
    vTaskStartScheduler();
    
    while (true) {
        ; // Should never reach here
    }
}
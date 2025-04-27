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
#include <stdlib.h>
#include "hardware/i2c.h"
#include "mpu6050.h"
#include "Fusion.h"
#include "hc06.h"

#define SAMPLE_PERIOD (0.01f)
// UART configuration
#define UART_ID HC06_UART_ID
#define BAUD_RATE 115200

// Button definitions
const int BTN_LARANJA = 22;
const int BTN_AZUL = 21;
const int BTN_AMARELO = 20;
const int BTN_VERMELHO = 19;
const int BTN_VERDE = 18;
const int BTN_JOYSTICK = 16; // Joystick button

// Pins for joystick X and Y axes
const int X_AXIS_PIN = 26;       
const int Y_AXIS_PIN = 27;

// Button flags
volatile bool btn_laranja_flag = false;
volatile bool btn_azul_flag = false;
volatile bool btn_amarelo_flag = false;
volatile bool btn_vermelho_flag = false;
volatile bool btn_verde_flag = false;
volatile bool btn_joystick_flag = false;

// MPU6050 I2C address
const int MPU_ADDRESS = 0x68;
const int I2C_SDA_GPIO = 8;
const int I2C_SCL_GPIO = 9;

// ADC data structure
typedef struct adc {
    int axis;               
    int val;                
} adc_t;

// Queue for ADC values
QueueHandle_t xQueueADC;
SemaphoreHandle_t xSemaphoreEvent;

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
        else if (gpio == BTN_JOYSTICK)
            btn_joystick_flag = true;
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
        else if (gpio == BTN_JOYSTICK)
            btn_joystick_flag = false;
    }
    xSemaphoreGiveFromISR(xSemaphoreEvent, 0);
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

    gpio_init(BTN_JOYSTICK);
    gpio_set_dir(BTN_JOYSTICK, GPIO_IN);
    gpio_pull_up(BTN_JOYSTICK);
}

// Setup button interrupts
void init_callbacks()
{
    gpio_set_irq_enabled_with_callback(BTN_LARANJA, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_note_callback);
    gpio_set_irq_enabled(BTN_AZUL, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_AMARELO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_VERMELHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_VERDE, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_JOYSTICK, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
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

// MPU
static void mpu6050_reset() {
    // Two byte reset. First byte register, second byte data
    // There are a load more options to set up the device in different ways that could be added here
    uint8_t buf[] = {0x6B, 0x00};
    i2c_write_blocking(i2c_default, MPU_ADDRESS, buf, 2, false);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    // For this particular device, we send the device the register we want to read
    // first, then subsequently read from the device. The register is auto incrementing
    // so we don't need to keep sending the register we want, just the first.

    uint8_t buffer[6];

    // Start reading acceleration registers from register 0x3B for 6 bytes
    uint8_t val = 0x3B;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &val, 1, true); // true to keep master control of bus
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);

    for (int i = 0; i < 3; i++) {
        accel[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);
    }

    // Now gyro data from reg 0x43 for 6 bytes
    // The register is auto incrementing on each read
    val = 0x43;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &val, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);  // False - finished with bus

    for (int i = 0; i < 3; i++) {
        gyro[i] = (buffer[i * 2] << 8 | buffer[(i * 2) + 1]);;
    }

    // Now temperature from reg 0x41 for 2 bytes
    // The register is auto incrementing on each read
    val = 0x41;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &val, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 2, false);  // False - finished with bus

    *temp = buffer[0] << 8 | buffer[1];
}

void hc06_send_text(const char* text) {
    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        while (!uart_is_writable(HC06_UART_ID)) {
            taskYIELD(); // Deixa o FreeRTOS trocar de task enquanto espera
        }
        uart_putc_raw(HC06_UART_ID, text[i]);
    }
}

// Task to monitor button status and send via serial printf
void task_button_serial(void *p) {
    bool last_state[6] = {0};
    const char* letras[6] = {"A", "S", "J", "K", "L", "CLICK"};

    while (true) {
        if (xSemaphoreTake(xSemaphoreEvent, portMAX_DELAY)){
            bool current_state[6] = {
                btn_verde_flag,
                btn_vermelho_flag,
                btn_amarelo_flag,
                btn_azul_flag,
                btn_laranja_flag, 
                btn_joystick_flag
            };

            for (int i = 0; i < 6; i++) {
                if (current_state[i] != last_state[i]) {
                    char buffer[16];
                    snprintf(buffer, sizeof(buffer), "%s:%s\n", letras[i], current_state[i] ? "DOWN" : "UP");
                    hc06_send_text(buffer); // ENVIA PELO BLUETOOTH
                    last_state[i] = current_state[i];
                }
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
            xSemaphoreGive(xSemaphoreEvent); 
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
            xSemaphoreGive(xSemaphoreEvent); 
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void mpu6050_task(void *p) {
    // configuracao do I2C
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_GPIO);
    gpio_pull_up(I2C_SCL_GPIO);

    FusionAhrs ahrs;
    FusionAhrsInitialise(&ahrs);

     
    int16_t acceleration[3], gyro[3], temp;
  
    while (true) { 
  
        mpu6050_read_raw(acceleration, gyro, &temp);
        FusionVector gyroscope = {
            .axis.x = gyro[0] / 131.0f, // Conversão para graus/s
            .axis.y = gyro[1] / 131.0f,
            .axis.z = gyro[2] / 131.0f,
        };
  
        FusionVector accelerometer = {
            .axis.x = acceleration[0] / 16384.0f, // Conversão para g
            .axis.y = acceleration[1] / 16384.0f,
            .axis.z = acceleration[2] / 16384.0f,
        };      
  
        FusionAhrsUpdateNoMagnetometer(&ahrs, gyroscope, accelerometer, SAMPLE_PERIOD);
        const FusionEuler euler = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));
        // printf("Roll %0.1f, Pitch %0.1f, Yaw %0.1f\n", euler.angle.roll, euler.angle.pitch, euler.angle.yaw); 
        
        // criar os structs pra enviar pra fila
  
        adc_t acel;
        acel.axis = 2;
        acel.val = accelerometer.axis.x*100;
        static int contador_zeros = 0;
        //printf("Acel: %d\n", acel.val);

        if (acel.val == 0) {
            contador_zeros++;
            if (contador_zeros > 50) { 
                //printf("Sensor travado! Resetando MPU6050...\n");
                mpu6050_reset(); 
                contador_zeros = 0;
            }
        } else {
            contador_zeros = 0;
        }

        
        if (abs(acel.val) > 60) {
            printf("SPACE\n");
            xQueueSend(xQueueADC, &acel, 0);
            xSemaphoreGive(xSemaphoreEvent); 
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void hc06_task(void *p) {
    uart_init(HC06_UART_ID, HC06_BAUD_RATE);
    gpio_set_function(HC06_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(HC06_RX_PIN, GPIO_FUNC_UART);
    hc06_init("gabi", "1234");

    adc_t adc_data;
    while (1) {
        if (xQueueReceive(xQueueADC, &adc_data, portMAX_DELAY))
        {
            uint8_t vec[4];
            vec[0] = 0xFF; 
            vec[1] = (uint8_t)adc_data.axis;
            vec[2] = (uint8_t)(adc_data.val & 0xFF);
            vec[3] = (uint8_t)((adc_data.val >> 8) & 0xFF);
            uart_write_blocking(HC06_UART_ID, vec, 4); 
        }
    }
}


int main()
{
    stdio_init_all();
    // printf("Iniciando controle Clone Hero com botoes e joystick...\n");

    // Initialize ADC for joystick
    adc_init();
    adc_gpio_init(X_AXIS_PIN);
    adc_gpio_init(Y_AXIS_PIN);
    
    // Initialize buttons and their interrupts
    init_buttons();
    init_callbacks();

    //cria semaforo
    xSemaphoreEvent = xSemaphoreCreateBinary();
    // Create queue for ADC values
    xQueueADC = xQueueCreate(64, sizeof(adc_t));
    
    if (xQueueADC == NULL) {
        //printf("Erro ao criar a fila ADC!\n");
        while(1); 
    }
    // Create tasks
    xTaskCreate(task_button_serial, "Button Serial", 512, NULL, 1, NULL);
    xTaskCreate(x_task, "X Axis Task", 256, NULL, 1, NULL);
    xTaskCreate(y_task, "Y Axis Task", 256, NULL, 1, NULL);
    xTaskCreate(mpu6050_task, "mpu6050_Task", 8192, NULL, 1, NULL);
    // printf("Start bluetooth task\n");
    xTaskCreate(hc06_task, "UART_Task", 4096, NULL, 1, NULL);

    vTaskStartScheduler();
    
    while (true) {
        ; // Should never reach here
    }
}
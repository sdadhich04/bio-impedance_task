#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "driver/usb_serial_jtag.h"
#include "AD5933.h"
// #include "LogWriter.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mac.h"


/*
 *   Set up correct i2c pins on esp32 version
 */
// #define I2C_MASTER_SCL_IO 9  // for old esp32 board
// #define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 20  // for new esp32c6 board
#define I2C_MASTER_SDA_IO 21 


#define MHz_6 6000000
#define INTERNAL_CLOCK_FREQ 16000000

#define START_FREQ 33000
#define NUM_INCR 1
#define FREQ_INCR 1000

#define CALIBRATION_NUM_INCR 3

#define BUFFER_SIZE 1024

double gain_factor;
//double system_phase;
double gain_factor_range[CALIBRATION_NUM_INCR];
double system_phase_range[CALIBRATION_NUM_INCR];
// int16_t real[NUM_INCR];
// int16_t imag[NUM_INCR];

i2c_master_bus_config_t i2c_master_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

void HandleSerialInput();

void print_uarr(uint8_t* arr, uint8_t n) {
    for (int i = 0; i < n; i++) {
        //ESP_LOGI("arr contents: ", "[%d] = %x", i, arr[i]);
        printf("arr contents: [%d] = %x\n", i, arr[i]);
    }
}

void print_arr(signed short* arr, uint8_t n) {
    for (int i = 0; i < n; i++) {
        //ESP_LOGI("arr contents: ", "[%d] = %d", i, arr[i]);
        printf("arr contents: [%d] = %d\n", i, arr[i]);
    }
}

void init_freq_arr(uint8_t n, uint16_t start, uint16_t step, uint16_t* arr) {
    for (int i = 0; i < n; i++) {
        arr[i] = start + step * i;
    }
}

void print_double_arr(double* arr, int n) {
    for (int i = 0; i < n; i++) {
        ESP_LOGI("arr contents: ", "[%d] = %f", i, arr[i]);
        //printf("arr contents: [%d] = %x\n", i, arr[i]);
    }
}

void app_main(void)
{
    printf("starting up\n");
    // init master bus
    i2c_master_bus_handle_t bus_handle;

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_config, &bus_handle));

    // init AD5933
    AD5933_init_i2c_device(bus_handle);

    // reset
    AD5933_set_reg_value(AD5933_REG_CONTROL_LB, 0x18);
    
    // init calibration settings
    AD5933_init_settings(START_FREQ, INTERNAL_CLOCK_FREQ, FREQ_INCR, CALIBRATION_NUM_INCR, AD5933_RANGE_2000mVpp, AD5933_PGA_1, 25);
    
    int16_t calib_real[CALIBRATION_NUM_INCR];
    int16_t calib_imag[CALIBRATION_NUM_INCR];

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // start frequency sweep
    AD5933_start_freq_sweep(calib_real, calib_imag);
    // calculate gain_factor based on first point recorded
    gain_factor = gain_factor_calibration(2200, calc_magnitude(calib_real[0], calib_imag[0]));
    // print the gain_factor
    ESP_LOGI("gain factor", "gainfactor: %f", gain_factor);

    // system phase calibration
    system_phase_calibration(system_phase_range, calib_real, calib_imag, CALIBRATION_NUM_INCR);
    ESP_LOGI("system phases", "listed below");
    print_double_arr(system_phase_range, CALIBRATION_NUM_INCR);
    // done with calibration

    // loop forever collecting values and logging them
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    AD5933_init_settings(START_FREQ, INTERNAL_CLOCK_FREQ, FREQ_INCR, NUM_INCR, AD5933_RANGE_2000mVpp, AD5933_PGA_1, 25);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    int16_t real_arr[NUM_INCR];
    int16_t imag_arr[NUM_INCR];

    while (1) {
        ESP_LOGI("log", "Starting new sweep");
        AD5933_start_freq_sweep(real_arr, imag_arr);
        for (int i = 0; i < NUM_INCR; i ++){
            ESP_LOGI("log", "Uncompensated real: %d, imag: %d", real_arr[i], imag_arr[i]);
            double impedance = AD5933_calculate_impedance(gain_factor, real_arr[i], imag_arr[i]);
            ESP_LOGI("log", "impedance magnitude: %f", impedance);
            
            // phase calculations
            bool phase_error;
            double phase = arctan_phase_angle(real_arr[i], imag_arr[i], &phase_error);
            double comp_real, comp_imag;
            compensated_real_and_imag(impedance, phase, system_phase_range[i], &comp_real, &comp_imag);
            
            ESP_LOGI("log", "Calculated phase: %f", phase);
            ESP_LOGI("log", "System phase compensated real: %f, imag: %f", comp_real, comp_imag);
        }
        ESP_LOGI("log",  "Pausing");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}

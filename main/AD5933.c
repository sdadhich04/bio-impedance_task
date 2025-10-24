#include "AD5933.h"
// #include "driver/i2c_master.h"
// #include "driver/i2c_types.h"
#include "esp_log.h"
#include "math.h"
#include "freertos/FreeRTOS.h"


i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7, // change based on device
    .device_address = 0x0D, // change based on device
    .scl_speed_hz = 10000, // change based on device
    .scl_wait_us = 100, // im not sure what goes here tbh but its just timeout
    .flags.disable_ack_check = 0
};

i2c_master_dev_handle_t AD5933_dev_handle;

esp_err_t AD5933_init_i2c_device(i2c_master_bus_handle_t bus_handle){

    return i2c_master_bus_add_device(bus_handle, &dev_cfg, &AD5933_dev_handle);
}

void AD5933_set_reg_value(uint8_t reg_addr, uint8_t reg_val) {
    uint8_t write_buff[2] = {reg_addr, reg_val};
    i2c_master_transmit(AD5933_dev_handle, write_buff, 2, -1);
}

void AD5933_set_ptr_reg(uint8_t reg_addr){
    uint8_t pointer_cmd[2] = {AD5933_ADDR_POINTER, reg_addr};
    i2c_master_transmit(AD5933_dev_handle, pointer_cmd, 2, -1);
}

void AD5933_set_reg_block(uint8_t reg_addr, uint8_t* data, uint8_t size) {
    uint8_t write_buff[2] = {0, 0};
    for (int i = 0; i < size; i ++) {
        write_buff[0] = reg_addr + i;
        write_buff[1] = data[i];
        i2c_master_transmit(AD5933_dev_handle, write_buff, 2, -1);
    }
}

void AD5933_write_block(uint8_t reg_addr, uint8_t* data, uint8_t size) {
    

    uint8_t write_size = 2 + size;

    uint8_t* write_buff = calloc(write_size, sizeof(uint8_t));

    write_buff[0] = AD5933_BLOCK_WRITE;
    write_buff[1] = size;

    // copy data to write_buff
    for (int i = 0; i < size; i++) { 
        write_buff[i + 2] = data[i];
    }

    uint8_t pointer_cmd[2] = {AD5933_ADDR_POINTER, reg_addr};
    i2c_master_transmit(AD5933_dev_handle, pointer_cmd, 2, -1);

    i2c_master_transmit(AD5933_dev_handle, write_buff, write_size, -1);

    free(write_buff);
}

void AD5933_read_reg(uint8_t* read_buff) {
    i2c_master_receive(AD5933_dev_handle, read_buff, 1, -1);
}

void AD5933_read_reg_block(uint8_t* read_buff, uint8_t num_bytes) {
    uint8_t write_buff[2] = {AD5933_BLOCK_READ, num_bytes};
    i2c_master_transmit_receive(AD5933_dev_handle, write_buff, 2, read_buff, num_bytes, -1);
}

void AD5933_init_settings(double start_freq, double clock_freq, double freq_incr,
    uint16_t num_incr, uint8_t range, uint8_t PGA, uint16_t num_cycles) {
    // Start frequency
    uint8_t start_freq_bytes[3] = {0};
    uint32_t start_freq_code = (uint32_t)((start_freq / (clock_freq / 4)) * (1 << 27));

    start_freq_bytes[0] = (start_freq_code >> 16) & 0xFF;
    start_freq_bytes[1] = (start_freq_code >> 8) & 0xFF;
    start_freq_bytes[2] = start_freq_code & 0xFF;
    
    AD5933_write_block(AD5933_REG_FREQ_START, start_freq_bytes, 3);

    // Freq Incr 
    uint8_t freq_incr_bytes[3] = {0};
    uint32_t freq_incr_code = (uint32_t)((freq_incr / (clock_freq / 4)) * (1 << 27));

    freq_incr_bytes[0] = freq_incr_code >> 16 & 0xFF;
    freq_incr_bytes[1] = freq_incr_code >> 8 & 0xFF;
    freq_incr_bytes[2] = freq_incr_code & 0xFF;

    AD5933_write_block(AD5933_REG_FREQ_INC, freq_incr_bytes, 3);

    // Num Incr
    // 16 bits written, first 7 0s
    uint8_t num_incr_bytes[2] = {0};
    
    num_incr_bytes[0] = (num_incr >> 8) & 0xFF;
    num_incr_bytes[1] = num_incr & 0xFF;

    AD5933_write_block(AD5933_REG_INC_NUM, num_incr_bytes, 2);

    // set cycling times
    uint8_t num_cycles_bytes[2] = {0};
    num_cycles_bytes[0] = (num_cycles >> 8) & 0xFF;
    num_cycles_bytes[1] = num_cycles & 0xFF;
    AD5933_write_block(AD5933_REG_SETTLING_CYCLES, num_cycles_bytes, 2);

    // Control set to standby
    AD5933_set_reg_value(AD5933_REG_CONTROL_HB, AD5933_CONTROL_FUNCTION(AD5933_FUNCTION_STANDBY) | AD5933_CONTROL_RANGE(range) | PGA);
    AD5933_set_reg_value(AD5933_REG_CONTROL_LB, 0x00);
}


void AD5933_start_freq_sweep(signed short* real_arr, signed short* imag_arr) {
    uint8_t ctrl_hb_low;
    AD5933_set_ptr_reg(AD5933_REG_CONTROL_HB);
    AD5933_read_reg(&ctrl_hb_low);
    ctrl_hb_low &= 0x0F;

    uint8_t status = 0;
    int freq_index = 0;
    
    signed short real;
    signed short imag;

    // first ensure init sequence is completed (registers are set up): finish method above then call
    //AD5933_init_settings();

    // set control register to standby by issuing "enter standby mode command to control register" ( or a reset command?)
    AD5933_set_reg_value(AD5933_REG_CONTROL_HB, AD5933_CONTROL_FUNCTION(AD5933_FUNCTION_STANDBY) | ctrl_hb_low); // ask about output voltage range, PGA gain
    // make sure clock is set

    // set control register to initialize with start frequency
    AD5933_set_reg_value(AD5933_REG_CONTROL_HB, AD5933_CONTROL_FUNCTION(AD5933_FUNCTION_INIT_START_FREQ) | ctrl_hb_low); // ask about output voltage range, PGA gain

    // wait a period of time equal to settling time

    // send start frequency sweep mode command
    AD5933_set_reg_value(AD5933_REG_CONTROL_HB, AD5933_CONTROL_FUNCTION(AD5933_FUNCTION_START_SWEEP) | ctrl_hb_low);
    
    
    AD5933_set_ptr_reg(AD5933_REG_STATUS);
    // while frequency sweep not complete (poll status register)
    do {
        // means its not complete, so continue

        // poll status register to check if dft conversion is complete:
        // while dft conversion not complete{}
        // send increment or repeat frequency command 
        while ((status & AD5933_STAT_DATA_VALID) == 0) {
            AD5933_read_reg(&status);
            ESP_LOGI("waiting", "status - %X", status);
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
        // HERE READ FROM REGISTERS, load into big array
        // just load real and imaginary data, calculate shit later
        
        uint8_t data[2] = {0,0};
        AD5933_set_ptr_reg(AD5933_REG_REAL_DATA);
        AD5933_read_reg_block(data, 2);

        real = (signed short)((data[0] << 8) | data[1]);
        //ESP_LOGI("debug", "made it");
        AD5933_set_ptr_reg(AD5933_REG_IMAG_DATA);
        AD5933_read_reg_block(data, 2);
        
        imag = (signed short)((data[0] << 8) | data[1]);
        
        // ESP_LOGI("real", "%d", real);
        // ESP_LOGI("imag", "%d", imag);

        real_arr[freq_index] = real;
        imag_arr[freq_index] = imag;
        
        // increment frequency

        AD5933_set_reg_value(AD5933_REG_CONTROL_HB, AD5933_CONTROL_FUNCTION(AD5933_FUNCTION_INC_FREQ) | ctrl_hb_low);
        
        AD5933_set_ptr_reg(AD5933_REG_STATUS);
        AD5933_read_reg(&status);
        freq_index++;
    } while ((status & AD5933_STAT_SWEEP_DONE) == 0);
}

// one point gain factor calibration
double gain_factor_calibration(double Z_calibration, double magnitude) {
    return (1 / (double)Z_calibration) / magnitude;
}

double calc_magnitude(int16_t real, int16_t imag) {
    return sqrt((double)((real * real) + (imag * imag)));
}

double AD5933_calculate_impedance(double gainFactor, signed short real, signed short imag) {
    double magnitude = 0;
    double impedance = 0;

    double doubleRealData = (double)real;
    double doubleImagData = (double)imag;
    magnitude = sqrt((doubleRealData * doubleRealData) + (doubleImagData * doubleImagData));
    impedance = 1.0 / (magnitude * gainFactor);
    //impedance = (magnitude * gainFactor);
    return(impedance);    
}

double arctan_phase_angle(double R, double I, bool* undefined) {
    
    // edge case when R = 0
    if (R == 0) {
        if (I == 0) {
            *undefined = true;
            return 0;
        }
        *undefined = false;
        return (I < 0) ? -180 : 180;
    }
    // if R is not undefined it must be defined
    *undefined = false;
    double tan_IR = atan(I / R) * (180 / M_PI);
    if (R > 0) {
        if (I > 0){
            return tan_IR;
        }
        else {
            return 360 + tan_IR;
        }
    }
    // negative real is same calculation
    else {
        return tan_IR + 180;
    }
}

void compensated_real_and_imag(double impedance, double phase, double system_phase, double* comp_real, double* comp_imag) {
    *comp_real = impedance * cos(phase - system_phase);
    *comp_imag = impedance * sin(phase - system_phase);
}

// sets system_phase array values
void system_phase_calibration(double* system_phases, int16_t* real, int16_t* imag, int num_incr) {
    for (int i = 0; i < num_incr; i++) {
        bool undefined;
        system_phases[i] = arctan_phase_angle(real[i], imag[i], &undefined);
    }
}

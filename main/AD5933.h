#ifndef AD5933_H_
#define AD5933_H_

#include "driver/i2c_master.h"
#include "driver/i2c_types.h"


/*! AD5933 Address */
#define AD5933_ADDRESS          ((uint8_t)0x0D)

/*! AD5933_CONTROL_RANGE(x) options */
#define AD5933_RANGE_2000mVpp   ((uint8_t)0x0)
#define AD5933_RANGE_200mVpp    ((uint8_t)0x1)
#define AD5933_RANGE_400mVpp    ((uint8_t)0x2)
#define AD5933_RANGE_1000mVpp   ((uint8_t)0x3)

/*! AD5933_CONTROL_PGA options */
#define AD5933_PGA_1        ((uint8_t)0x1)
#define AD5933_PGA_5        ((uint8_t)0x0)

/*! AD5933 Registers */
#define AD5933_REG_CONTROL_HB       ((uint8_t)0x80)    /*!< HB of the Control register */
#define AD5933_REG_CONTROL_LB       ((uint8_t)0x81)    /*!< LB of the Control register */
#define AD5933_REG_FREQ_START       ((uint8_t)0x82)    /*!< Start frequency */
#define AD5933_REG_FREQ_INC         ((uint8_t)0x85)    /*!< Frequency increment */
#define AD5933_REG_INC_NUM          ((uint8_t)0x88)    /*!< Number of increments */
#define AD5933_REG_SETTLING_CYCLES  ((uint8_t)0x8A)    /*!< Number of settling time cycles */
#define AD5933_REG_STATUS           ((uint8_t)0x8F)    /*!< Status */
#define AD5933_REG_TEMP_DATA        ((uint8_t)0x92)    /*!< Temperature data */
#define AD5933_REG_REAL_DATA        ((uint8_t)0x94)    /*!< Real data MSB*/
#define AD5933_REG_REAL_DATA_2      ((uint8_t)0x95)    /*!< Real Data LSB*/
#define AD5933_REG_IMAG_DATA        ((uint8_t)0x96)    /*!< Imaginary data MSB*/
#define AD5933_REG_IMAG_DATA_2      ((uint8_t)0x97)    /*!<Imaginary data LSB*/

/*! AD5933 Block Commands */
#define AD5933_BLOCK_WRITE      ((uint8_t)0xA0)
#define AD5933_BLOCK_READ       ((uint8_t)0xA1)
#define AD5933_ADDR_POINTER     ((uint8_t)0xB0)

/*! AD5933_CONTROL_FUNCTION(x) options */
#define AD5933_FUNCTION_INIT_START_FREQ     ((uint8_t)0x1)
#define AD5933_FUNCTION_START_SWEEP         ((uint8_t)0x2)
#define AD5933_FUNCTION_INC_FREQ            ((uint8_t)0x3)
#define AD5933_FUNCTION_REPEAT_FREQ         ((uint8_t)0x4)
#define AD5933_FUNCTION_MEASURE_TEMP        ((uint8_t)0x9)
#define AD5933_FUNCTION_POWER_DOWN          ((uint8_t)0xA)
#define AD5933_FUNCTION_STANDBY             ((uint8_t)0xB)

/*! AD5933_REG_CONTROL_HB Bits */
#define AD5933_CONTROL_FUNCTION(x)  ((x) << 4)     
#define AD5933_CONTROL_RANGE(x)     ((x) << 1)

/*! AD5933_REG_STATUS Bits */
#define AD5933_STAT_TEMP_VALID  (0x1 << 0)
#define AD5933_STAT_DATA_VALID  (0x1 << 1)
#define AD5933_STAT_SWEEP_DONE  (0x1 << 2)

#define CLOCK_FREQ 

extern i2c_device_config_t dev_cfg;

extern i2c_master_dev_handle_t AD5933_dev_handle;

/*
  Function to init the i2c communication bus on the esp32,
  this MUST be called before any other AD5933 related work.
  Takes a bus handle as a parameter, which must be declared
  and initialized using i2c_new_master_bus(&i2c_master_config, &bus_handle)
  before calling this function
*/
esp_err_t AD5933_init_i2c_device(i2c_master_bus_handle_t bus_handle);
// i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

void AD5933_set_reg_value(uint8_t reg_addr, uint8_t reg_val);

void AD5933_set_ptr_reg(uint8_t reg_addr);

void AD5933_set_reg_block(uint8_t reg_addr, uint8_t* data, uint8_t size);

void AD5933_write_block(uint8_t reg_addr, uint8_t* data, uint8_t size);

// set pointer register to reg_addr before calling this method (allows for repeated calls)
void AD5933_read_reg(uint8_t* read_buff);

void AD5933_read_reg_block(uint8_t* read_buff, uint8_t num_bytes);

/*
  Actually starts the frequency sweep. Only call this after the 
  AD5933 has been initialized.
  Parameters:
   - real_arr: array to store real parts of impedances (should be of length num_incr)
   - imag_arr: array to store imaginary parts of impedances (should be of length num_incr)
*/
void AD5933_start_freq_sweep(signed short* real_arr, signed short* imag_arr);

/* 
  Initializes the setting registers on the AD5933 based on the provided parameters
  Parameters:
   - start_freq: the starting frequency of the frequency sweep
   - clock_freq: frequency of the clock used for the AD5933 (if using AD5933 internal clock,
                 then use 16MHz, otherwise set to external clock freq)
   - freq_incr:  size of each increment of frequency in the frequency sweep
   - num_incr:   number of incremenets in the frequency sweep
   - range:      voltage range of output signal; see macros defined in AD5933.h
                 (AD5933_RANGE_2000mVpp, etc)
   - PGA:        Programmable Gain Amplifier, set to AD5933_PGA_1 or AD5933_PGA_5
                 (currently we set to 1)
   - num_cycles: (From datasheet) number of output excitation cycles that are allowed to pass 
                 through the unknown impedance, after receipt of a start frequency sweep, 
                 increment frequency, or repeat frequency command, before the ADC is triggered to
                 perform a conversion of the response signal
*/
void AD5933_init_settings(double start_freq, double clock_freq,
    double freq_incr, uint16_t num_incr, uint8_t range, uint8_t PGA,
    uint16_t num_cycles);

/*
  Calculates impedance based on given gainFactor, real part and imaginary part
*/
double AD5933_calculate_impedance(double gainFactor, signed short real, signed short imag);

/*
  Calculates gain factor based on calibration impedance (target impedance)
  and recorded magnitude.
  This returns the gain factor value, so the global gain_factor value must
  be set to the returned value
*/
double gain_factor_calibration(double Z_calibration, double magnitude);

/*
  Calculates magnitude of vector with real and imaginary parts
*/
double calc_magnitude(int16_t real, int16_t imag);

/*
  Calculated system phase values over a range of frequencies.
  Use based on frequencies corresponding to real and imag arrays
*/
void system_phase_calibration(double* system_phases, int16_t* real, int16_t* imag, int num_incr);

double arctan_phase_angle(double R, double I, bool* undefined);

void compensated_real_and_imag(double impedance, double phase, double system_phase, double* comp_real, double* comp_imag);

#endif
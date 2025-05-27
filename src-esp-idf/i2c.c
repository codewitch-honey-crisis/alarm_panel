#include "i2c.h"
#include "driver/i2c.h"
int i2c_master_init(void)
{
    int i2c_master_port = I2C_PORT;

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_SPEED,
    };

    // Configure I2C parameters
    i2c_param_config(i2c_master_port, &i2c_conf);

    // Install I2C driver
    return ESP_OK!=i2c_driver_install(i2c_master_port, i2c_conf.mode, 0, 0, 0);
}


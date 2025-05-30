#include "i2c.h"
#include <memory.h>
#include "driver/i2c_master.h"
int i2c_master_init(void)
{
    i2c_master_bus_config_t i2c_mst_config;
    memset(&i2c_mst_config,0,sizeof(i2c_mst_config));
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = I2C_PORT;
    i2c_mst_config.scl_io_num = (gpio_num_t)I2C_SCL;
    i2c_mst_config.sda_io_num = (gpio_num_t)I2C_SDA;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t bus;
    if(ESP_OK!=i2c_new_master_bus(&i2c_mst_config, &bus)) {
        return -1;
    }
    return 0;
}


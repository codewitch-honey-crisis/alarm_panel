#include "power.h"
#include "i2c.h"
#ifdef M5STACK_CORE2
#include "driver/i2c_master.h"
#include "task.h"
#include <memory.h>

static i2c_master_dev_handle_t power_i2c_handle = NULL;

static int power_read8(uint8_t addr) {
    uint8_t res;
    if(ESP_OK!=i2c_master_transmit_receive(power_i2c_handle,&addr,1,&res,1,1000)) {
        return -1;
    }
    return res;
}
static int power_write8(uint8_t addr, uint8_t value) {
    uint8_t buf[] = {addr,value};
    if(ESP_OK!=i2c_master_transmit(power_i2c_handle,buf,sizeof(buf),-1)) {
        return -1;
    }
    return 0;
}
static uint8_t power_calc_voltage_data(uint16_t value, uint16_t maxv, uint16_t minv,
                                 uint16_t step) {
    uint8_t data = 0;
    if (value > maxv) value = maxv;
    if (value > minv) data = (value - minv) / step;
    return data;
}
static void power_ldo_voltage(uint8_t number, float voltage) {
    uint16_t value = voltage * 1000;
    // value = (value > 3300) ? 15 : (value / 100) - 18;
    value = power_calc_voltage_data(value, 3300, 1800, 100) & 0x0F;
    switch (number) {
        // uint8_t reg, data;
        case 2:
            power_write8(0x28, (power_read8(0x28) & 0X0F) | (value << 4));
            break;
        case 3:
            power_write8(0x28, (power_read8(0x28) & 0XF0) | value);
            break;
    }
}
static void power_ldo_enable(uint8_t number, char state) {
    uint8_t mark = 0x01;
    if ((number < 2) || (number > 3)) return;

    mark <<= number;
    if (state) {
        power_write8(0x12, (power_read8(0x12) | mark));
    } else {
        power_write8(0x12, (power_read8(0x12) & (~mark)));
    }
}
static void power_dc_voltage(uint8_t number, float voltage) {
    uint8_t addr;
    uint16_t value = voltage * 1000;
    if (number > 2) return;
    // value = (value < 700) ? 0 : (value - 700) / 25;
    switch (number) {
        case 1:
            addr = 0x25;
            value = power_calc_voltage_data(value, 2275, 700, 25) & 0x3f;
            break;
        case 2:
            addr = 0x27;
            value = power_calc_voltage_data(value, 3500, 700, 25) & 0x7f;
            break;
        default: // 0
            addr = 0x26;
            value = power_calc_voltage_data(value, 3500, 700, 25) & 0x7f;
            break;
        
    }
    power_write8(addr, (power_read8(addr) & 0X80) | (value & 0X7F));
}
static void power_charge_current(uint8_t value) {
    uint8_t data = power_read8(0x33);
    data &= 0xf0;
    data = data | (((int)value) & 0x0f);
    power_write8(0x33, data);
}
static void power_dcd3_enable(char state) {
    uint8_t buf = power_read8(0x12);
    if (state == true)
        buf = (1 << 1) | buf;
    else
        buf = ~(1 << 1) & buf;
    power_write8(0x12, buf);
}
static void power_led_enable(char value) {
    uint8_t reg_addr = 0x94;
    uint8_t data;
    data = power_read8(reg_addr);

    if (value) {
        data = data & 0XFD;
    } else {
        data |= 0X02;
    }

    power_write8(reg_addr, data);
}
static void power_lcd_reset_enable(char state) {
    uint8_t reg_addr = 0x96;
    uint8_t gpio_bit = 0x02;
    uint8_t data;
    data = power_read8(reg_addr);

    if (state) {
        data |= gpio_bit;
    } else {
        data &= ~gpio_bit;
    }

    power_write8(reg_addr, data);
}
static float power_battery_voltage(void) {
    uint8_t buf[2];
    uint8_t addr = 0x78;
    if(ESP_OK!=i2c_master_transmit_receive(power_i2c_handle,&addr,1,buf,2,1000)) {
        return -1;
    }
    static const float ADCLSB = 1.1f / 1000.f;
    uint16_t data12 = ((buf[0] << 4) + buf[1]);
    return data12 * ADCLSB;
}
// Select source for BUS_5V
// 0 : use internal boost
// 1 : powered externally
void power_bus_external_power_enable(char value) {
    uint8_t data;
    if (!value) {
        // Set GPIO to 3.3V (LDO OUTPUT mode)
        data = power_read8(0x91);
        power_write8(0x91, (data & 0x0F) | 0xF0);
        // Set GPIO0 to LDO OUTPUT, pullup N_VBUSEN to disable VBUS supply from BUS_5V
        data = power_read8(0x90);
        power_write8(0x90, (data & 0xF8) | 0x02);
        // Set EXTEN to enable 5v boost
        data = power_read8(0x10);
        power_write8(0x10, data | 0x04);
    } else {
        // Set EXTEN to disable 5v boost
        data = power_read8(0x10);
        power_write8(0x10, data & ~0x04);
        // Set GPIO0 to float, using enternal pulldown resistor to enable VBUS supply from BUS_5V
        data = power_read8(0x90);
        power_write8(0x90, (data & 0xF8) | 0x07);
    }
}
// for AXP192 power management
int power_init() {
    if(power_i2c_handle!=NULL) {
        return 0;
    }
    i2c_device_config_t dev_cfg;
    memset(&dev_cfg,0,sizeof(dev_cfg));
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = 0x34;
    dev_cfg.scl_speed_hz = 400*1000;
    i2c_master_bus_handle_t bus_handle;
    if(ESP_OK!=i2c_master_get_bus_handle((i2c_port_num_t)I2C_PORT,&bus_handle)) {
        return -1;
    }
    if(ESP_OK!=i2c_master_bus_add_device(bus_handle,&dev_cfg,&power_i2c_handle)) {
        return -1;
    }
    power_write8(0x30, (power_read8(0x30) & 0x04) | 0x02);
    power_write8(0x92, power_read8(0x92) & 0xF8);
    power_write8(0x93, power_read8(0x93) & 0xF8);
    power_write8(0x35, (power_read8(0x35) & 0x1c) | 0xa2);
    power_dc_voltage(0,3.350f); // MCU
    power_dc_voltage(2, 2.8f); // LCD
    power_ldo_voltage(2,3.30f); // Periph power voltage preset (LCD_logic, SD card)
    power_ldo_voltage(3, 2.000);  // Vibrator power voltage preset
    power_ldo_enable(2,1);
    power_dcd3_enable(1); // LCD backlight
    power_led_enable(1);
    power_charge_current(0);// 100mA
    // power GPIO4
    power_write8(0x95,(power_read8(0x95)&0x72)|0x84); 
    power_write8(0x36,0x4C);
    power_write8(0x82,0xFF);
    power_lcd_reset_enable(0);
    task_delay(100);
    power_lcd_reset_enable(1);
    task_delay(100);
    power_write8(0x10, power_read8(0x10) | 0X04); // peripheral power on
    // axp: check v-bus status
    if (power_read8(0x00) & 0x08) {
        power_write8(0x30, power_read8(0x30) | 0x80);
        // if v-bus can use, disable M-Bus 5V output to input
        power_bus_external_power_enable(1);
    } else {
        // if not, enable M-Bus 5V output
        power_bus_external_power_enable(0);
    } 
    return 0;
}
int power_battery_level() {
    const float voltage = power_battery_voltage();
    const float percentage =
        (voltage < 3.248088) ? (0) : (voltage - 3.120712) * 100;
    return (percentage <= 100) ? percentage : 100;
}
int power_ac() {
    return (power_read8(0x00) & 0x80) ? 1 : 0;
}
#else
int power_init() {
    // do nothing
}
int power_battery_level() {
    return 0;
}
int power_ac() {
    return 1;
}
#endif
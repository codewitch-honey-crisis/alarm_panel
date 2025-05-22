#include "power.hpp"
#include "i2c.hpp"
#ifdef M5STACK_CORE2
#include <esp_i2c.hpp>        // i2c initialization
#include <m5core2_power.hpp>  // AXP192 power management (core2)
using namespace esp_idf;
void power_init() {
    // for AXP192 power management
    static m5core2_power power(esp_i2c<I2C_PORT, I2C_SDA, I2C_SCL>::instance);
    // draw a little less power
    power.initialize();
    power.lcd_voltage(3.0);
}
#endif
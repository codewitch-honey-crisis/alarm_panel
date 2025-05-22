#include "power.hpp"
#include "i2c.hpp"
#ifdef M5STACK_CORE2
#include <esp_i2c.hpp>        // i2c initialization
#include <m5core2_power.hpp>  // AXP192 power management (core2)
using namespace esp_idf;
// for AXP192 power management
static m5core2_power power(esp_i2c<I2C_PORT, I2C_SDA, I2C_SCL>::instance);    
void power_init() {
    // draw a little less power
    power.initialize();
    power.lcd_voltage(3.0);
}
float power_battery_level() {
    return power.battery_level();
}
bool power_ac() {
    return power.ac_in();
}
#else
void power_init() {
    // do nothing
}
float power_battery_level() {
    return 0.f;
}
bool power_ac() {
    return true;
}
#endif
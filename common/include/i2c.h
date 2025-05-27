#ifndef I2C_H
#define I2C_H
#ifdef M5STACK_CORE2
#define I2C_PORT 1
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_SPEED 200*1000
#endif
#ifdef FREENOVE_DEVKIT
#define I2C_PORT 0
#define I2C_SCL 1
#define I2C_SDA 2
#define I2C_SPEED 200*1000
#endif
#endif // I2C_H
#ifdef __cplusplus
extern "C" {
#endif
int i2c_master_init();
#ifdef __cplusplus
}
#endif
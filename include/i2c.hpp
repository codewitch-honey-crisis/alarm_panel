#pragma once
#ifdef M5STACK_CORE2
#define I2C_PORT 1
#define I2C_SDA 21
#define I2C_SCL 22
#endif
#ifdef FREENOVE_DEVKIT
#define I2C_PORT 0
#define I2C_SCL 1
#define I2C_SDA 2
#endif
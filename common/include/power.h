#ifndef POWER_H
#define POWER_H
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initializes the power management
void power_init(void);
/// @brief Gets the battery level
/// @return 0-100 indicating the percentage of charge
int power_battery_level(void);
/// @brief Indicates whether the device is on AC power
/// @return Non-zero if on AC power, otherwise zero
int power_ac(void);
#ifdef __cplusplus
}
#endif
#endif // POWER_H
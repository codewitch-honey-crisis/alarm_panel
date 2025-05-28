#ifndef POWER_H
#define POWER_H
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initializes the power management
/// @returns 0 on success, otherwise nonzero
int power_init(void);
/// @brief Gets the battery level
/// @return 0-100 indicating the percentage of charge, or less than zero if error
int power_battery_level(void);
/// @brief Indicates whether the device is on AC power
/// @return Returns a positive integer if on AC power, otherwise zero if on battery power. If an error occurs, the return value will be less than zero.
int power_ac(void);
#ifdef __cplusplus
}
#endif
#endif // POWER_H
#ifndef POWER_H
#define POWER_H
#ifdef __cplusplus
extern "C" {
#endif
void power_init();
int power_battery_level();
int power_ac();
#ifdef __cplusplus
}
#endif
#endif // POWER_H
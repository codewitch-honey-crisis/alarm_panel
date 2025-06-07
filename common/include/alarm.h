#ifndef ALARM_H
#define ALARM_H
#include <stddef.h>
#include <stdint.h>
#include "config.h"
#ifdef __cplusplus
extern "C" {
#endif
/// @brief The values of each alarm
extern char alarm_values[];
/// @brief Initialize the alarms
/// @return 0 if success, non-zero on error
int alarm_init(void);
/// @brief Turn an alarm off or on
/// @param alarm the alarm
/// @param on enable or disable the alarm
/// @return 0 on success, non-zero on error
int alarm_enable(size_t alarm, char on);
/// @brief Locks the alarm data with a mutex
void alarm_lock(void);
/// @brief Unlocks the alarm data with a mutex
void alarm_unlock(void);
/// @brief Unpacks alarm values into an array
/// @param data Up to 32 alarms, packed as bits
/// @param out_values The array that holds the result
/// @param length The size of the out buffer
void alarm_unpack_values(uint32_t data, char* out_values, size_t out_length);
/// @brief Pack alarm values into 32 bits
/// @param values The values to pack
/// @param length The count of values to pack
/// @return A uint32_t with the bits for up to 32 alarms
uint32_t alarm_pack_values(char* values, size_t length);
#ifdef __cplusplus
}
#endif
#endif // ALARM_H
#ifndef UI_H
#define UI_H
#ifdef __cplusplus
extern "C" {
#endif
/// @brief Initialize the user interface
void ui_init(void);
/// @brief Update the user interface
void ui_update(void);
/// @brief Update the switches to the current alarm values
/// @param lock Non-zero to lock the alarm mutex
void ui_update_switches(char lock);
/// @brief Indicates whether the web link is visible or not
/// @return Non-zero if visible, otherwise zero
int ui_web_link_visible(void);
/// @brief Sets the address for the web link
/// @param addr The address string
void ui_web_link(const char* addr);
#ifdef __cplusplus
}
#endif
#endif // UI_H
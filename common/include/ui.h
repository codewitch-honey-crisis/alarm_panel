#ifndef UI_H
#define UI_H
#ifdef __cplusplus
extern "C" {
#endif
void ui_init();
void ui_update();
void ui_update_switches(char lock);
int ui_web_link_visible();
void ui_web_link(const char* addr);
#ifdef __cplusplus
}
#endif
#endif // UI_H
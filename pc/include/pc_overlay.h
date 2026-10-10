/* pc_overlay.h - FPS counter + in-game settings menu */
#ifndef PC_OVERLAY_H
#define PC_OVERLAY_H

#ifdef __cplusplus
extern "C" {
#endif

void pc_overlay_init(void);
void pc_overlay_shutdown(void);
void pc_overlay_update(double fps, double speed);
void pc_overlay_draw(void);
void pc_overlay_boot_splash(const char* msg);
void pc_overlay_boot_error_frame(const char* const* lines, int n_lines);

/* Settings menu (Select / Tab) */
void pc_overlay_menu_toggle(void);
/* 1 while the Controls tab waits for a key to bind (menu key must not toggle) */
int  pc_overlay_capturing(void);
/* Short centered message drawn over the game for `ms` milliseconds */
void pc_overlay_toast(const char* msg, Uint32 ms);

#ifdef __cplusplus
}
#endif

#endif /* PC_OVERLAY_H */

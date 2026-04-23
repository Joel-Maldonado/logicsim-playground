#ifndef TOPBAR_H
#define TOPBAR_H

#include "app.h"
#include "workspace_layout.h"

#define TOPBAR_TAB_COUNT 3

typedef struct {
    Rectangle tabs[TOPBAR_TAB_COUNT];
    Rectangle sim_buttons[3];
    Rectangle help_button;
} TopbarLayout;

TopbarLayout topbar_compute_layout(const WorkspaceFrame *frame);
bool topbar_handle_click(AppContext *app, const TopbarLayout *layout, Vector2 mouse_pos, bool *shortcuts_open);
void topbar_draw(const AppContext *app, const TopbarLayout *layout, Vector2 mouse_pos, bool shortcuts_open, int window_width);

#endif // TOPBAR_H

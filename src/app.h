#ifndef APP_H
#define APP_H

#include "app_types.h"

void app_init(AppContext *app);
void app_rebuild_derived_state(AppContext *app);
LogicNode* app_add_node(AppContext *app, NodeType type, Vector2 pos);
LogicNode* app_add_named_node(AppContext *app, NodeType type, const char *name, Vector2 pos);
void app_set_mode(AppContext *app, AppMode mode);
void app_set_tool(AppContext *app, AppTool tool);
void app_set_panel_focus(AppContext *app, AppPanelFocus panel);
void app_select_row(AppContext *app, uint32_t row_index);
void app_clear_graph(AppContext *app);
void app_set_source_path(AppContext *app, const char *path);
void app_set_source_status(AppContext *app, const char *status);
void app_update_solver(AppContext *app);
void app_solver_set_input(AppContext *app, const char *input);
void app_solver_insert_char(AppContext *app, int codepoint);
void app_solver_backspace(AppContext *app);

#endif // APP_H

#ifndef UI_H
#define UI_H

#include "app.h"

typedef struct {
    Rectangle panel_rect;
    Rectangle status_rect;
    Rectangle compare_rect;
    Rectangle equation_rect;
    Rectangle truth_table_rect;
    Rectangle kmap_rect;
    Rectangle why_rect;
    uint32_t visible_truth_rows;
    uint32_t hidden_truth_rows;
    bool show_compare;
    bool show_kmap;
    uint8_t _padding[2];
} UiContextPanelLayout;

typedef struct {
    Rectangle panel_rect;
    Rectangle input_rect;
    Rectangle preview_rect;
    Rectangle result_rect;
    Rectangle steps_rect;
} UiSolverLayout;

UiContextPanelLayout ui_measure_context_panel(const AppContext *app, Rectangle panel);
UiSolverLayout ui_measure_solver_workspace(Rectangle panel);
float ui_solver_steps_content_height(const AppContext *app);
bool ui_context_truth_table_row_rect(const AppContext *app, const UiContextPanelLayout *layout, uint32_t row_index, Rectangle *row_rect);
bool ui_toolbox_item_rect(Rectangle panel, int slot, Rectangle *item_rect);
int ui_toolbox_slot_at(Rectangle panel, Vector2 mouse_pos);
void ui_draw_circuit(AppContext *app, Rectangle canvas);
void ui_draw_truth_table(AppContext *app, Rectangle panel);
void ui_draw_expression(AppContext *app, Rectangle panel);
void ui_draw_kmap(AppContext *app, Rectangle panel);
void ui_draw_toolbox(AppContext *app, Rectangle panel);
void ui_draw_status_strip(AppContext *app, Rectangle panel);
void ui_draw_waveforms(AppContext *app, Rectangle panel);
void ui_draw_context_panel(AppContext *app, Rectangle panel);
void ui_draw_solver_workspace(AppContext *app, Rectangle panel);
void ui_draw_solver_side_panel(AppContext *app, Rectangle panel);
void ui_draw_placement_ghost(AppContext *app, Rectangle canvas, Vector2 mouse_pos);
LogicPin* ui_get_pin_at(AppContext *app, Rectangle canvas, Vector2 mouse_pos);
LogicPin* ui_get_wire_at(AppContext *app, Rectangle canvas, Vector2 mouse_pos);

#endif // UI_H

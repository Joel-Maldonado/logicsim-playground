#include "topbar.h"
#include "app_commands.h"
#include "draw_util.h"
#include <string.h>

#define TOPBAR_TAB_START_X 130.0f
#define TOPBAR_TAB_WIDTH 88.0f
#define TOPBAR_TAB_HEIGHT 28.0f
#define TOPBAR_TAB_GAP 4.0f
#define TOPBAR_BUTTON_WIDTH 80.0f
#define TOPBAR_BUTTON_HEIGHT 28.0f
#define TOPBAR_BUTTON_GAP 10.0f
#define TOPBAR_HELP_WIDTH 30.0f
#define TOPBAR_HELP_MARGIN_RIGHT 20.0f
#define TOPBAR_HELP_GAP 14.0f

static bool topbar_sim_controls_visible(AppMode mode) {
    return mode != MODE_SOLVER;
}

static AppMode topbar_mode_for_tab(int index) {
    if (index == 1) {
        return MODE_COMPARE;
    }
    if (index == 2) {
        return MODE_SOLVER;
    }

    return MODE_BUILD;
}

static EditorCommand topbar_command_for_tab(int index) {
    if (index == 1) {
        return EDITOR_COMMAND_MODE_COMPARE;
    }
    if (index == 2) {
        return EDITOR_COMMAND_MODE_SOLVER;
    }

    return EDITOR_COMMAND_MODE_BUILD;
}

static const char *topbar_tab_label(int index) {
    if (index == 1) {
        return "COMPARE";
    }
    if (index == 2) {
        return "SOLVER";
    }

    return "EDIT";
}

TopbarLayout topbar_compute_layout(const WorkspaceFrame *frame) {
    TopbarLayout layout;
    float button_start_x;
    int index;

    memset(&layout, 0, sizeof(layout));
    for (index = 0; index < TOPBAR_TAB_COUNT; index++) {
        layout.tabs[index] = (Rectangle){
            TOPBAR_TAB_START_X + ((float)index * (TOPBAR_TAB_WIDTH + TOPBAR_TAB_GAP)),
            11.0f,
            TOPBAR_TAB_WIDTH,
            TOPBAR_TAB_HEIGHT
        };
    }

    layout.help_button = (Rectangle){
        (float)frame->window_width - TOPBAR_HELP_MARGIN_RIGHT - TOPBAR_HELP_WIDTH,
        11.0f,
        TOPBAR_HELP_WIDTH,
        TOPBAR_BUTTON_HEIGHT
    };

    button_start_x = layout.help_button.x - TOPBAR_HELP_GAP - (3.0f * TOPBAR_BUTTON_WIDTH) - (2.0f * TOPBAR_BUTTON_GAP);
    for (index = 0; index < 3; index++) {
        layout.sim_buttons[index] = (Rectangle){
            button_start_x + ((float)index * (TOPBAR_BUTTON_WIDTH + TOPBAR_BUTTON_GAP)),
            11.0f,
            TOPBAR_BUTTON_WIDTH,
            TOPBAR_BUTTON_HEIGHT
        };
    }

    return layout;
}

bool topbar_handle_click(AppContext *app, const TopbarLayout *layout, Vector2 mouse_pos, bool *shortcuts_open) {
    int index;

    if (CheckCollisionPointRec(mouse_pos, layout->help_button)) {
        *shortcuts_open = !(*shortcuts_open);
        return true;
    }

    for (index = 0; index < TOPBAR_TAB_COUNT; index++) {
        if (CheckCollisionPointRec(mouse_pos, layout->tabs[index])) {
            app_queue_command(app, topbar_command_for_tab(index));
            return true;
        }
    }

    if (!topbar_sim_controls_visible(app->mode)) {
        return false;
    }
    if (CheckCollisionPointRec(mouse_pos, layout->sim_buttons[0])) {
        app_queue_command(app, EDITOR_COMMAND_SIM_TOGGLE);
        return true;
    }
    if (CheckCollisionPointRec(mouse_pos, layout->sim_buttons[1])) {
        app_queue_command(app, EDITOR_COMMAND_SIM_STEP);
        return true;
    }
    if (CheckCollisionPointRec(mouse_pos, layout->sim_buttons[2])) {
        app_queue_command(app, EDITOR_COMMAND_SIM_RESET);
        return true;
    }

    return false;
}

void topbar_draw(const AppContext *app, const TopbarLayout *layout, Vector2 mouse_pos, bool shortcuts_open, int window_width) {
    Color violet;
    Color dim_label;
    Color active_label;
    Color idle_fill;
    int index;

    DrawRectangle(0, 0, window_width, pixel(WORKSPACE_TOPBAR_HEIGHT), (Color){ 20, 20, 20, 255 });
    draw_line_at(0.0f, WORKSPACE_TOPBAR_HEIGHT, (float)window_width, WORKSPACE_TOPBAR_HEIGHT, (Color){ 50, 50, 50, 255 });
    draw_text_at("LOGICSIM", 20.0f, 16.0f, 20, LIGHTGRAY);

    violet = (Color){ 200, 170, 255, 255 };
    dim_label = (Color){ 150, 150, 150, 255 };
    active_label = (Color){ 240, 240, 240, 255 };
    idle_fill = (Color){ 46, 46, 46, 255 };

    for (index = 0; index < TOPBAR_TAB_COUNT; index++) {
        Rectangle tab;
        bool is_active;
        bool is_hover;
        const char *label;
        float label_x;

        tab = layout->tabs[index];
        is_active = topbar_mode_for_tab(index) == app->mode;
        is_hover = CheckCollisionPointRec(mouse_pos, tab);
        label = topbar_tab_label(index);

        if (is_hover && !is_active) {
            DrawRectangleRec(tab, (Color){ 30, 30, 30, 255 });
        }
        label_x = tab.x + (tab.width - text_width(label, 13)) * 0.5f;
        draw_text_at(label, label_x, tab.y + 8.0f, 13, is_active ? active_label : dim_label);
        if (is_active) {
            DrawRectangle(pixel(tab.x + 8.0f), pixel(tab.y + tab.height), pixel(tab.width - 16.0f), 2, violet);
        }
    }

    if (topbar_sim_controls_visible(app->mode)) {
        Rectangle run_rect;
        Rectangle step_rect;
        Rectangle reset_rect;
        const char *run_label;
        Color run_color;
        float run_label_x;
        float step_label_x;
        float reset_label_x;

        run_rect = layout->sim_buttons[0];
        step_rect = layout->sim_buttons[1];
        reset_rect = layout->sim_buttons[2];

        run_label = app->simulation.active ? "STOP" : "RUN";
        run_color = app->simulation.active ? (Color){ 200, 60, 60, 255 } : (Color){ 245, 185, 50, 255 };

        DrawRectangleRounded(run_rect, 0.24f, 8, run_color);
        DrawRectangleRounded(step_rect, 0.24f, 8, idle_fill);
        DrawRectangleRounded(reset_rect, 0.24f, 8, idle_fill);

        run_label_x = run_rect.x + (run_rect.width - text_width(run_label, 13)) * 0.5f;
        step_label_x = step_rect.x + (step_rect.width - text_width("STEP", 13)) * 0.5f;
        reset_label_x = reset_rect.x + (reset_rect.width - text_width("RESET", 13)) * 0.5f;

        draw_text_at(run_label, run_label_x, run_rect.y + 8.0f, 13, WHITE);
        draw_text_at("STEP", step_label_x, step_rect.y + 8.0f, 13, LIGHTGRAY);
        draw_text_at("RESET", reset_label_x, reset_rect.y + 8.0f, 13, LIGHTGRAY);
    }

    {
        Rectangle help_rect;
        bool help_hover;

        help_rect = layout->help_button;
        help_hover = CheckCollisionPointRec(mouse_pos, help_rect);
        if (shortcuts_open || help_hover) {
            DrawRectangleRounded(help_rect, 0.5f, 8, (Color){ 38, 38, 38, 255 });
        }
        DrawRectangleRoundedLinesEx(help_rect, 0.5f, 8, 1.0f, (Color){ 70, 70, 70, 255 });
        draw_text_at("?", help_rect.x + (help_rect.width - text_width("?", 14)) * 0.5f, help_rect.y + 7.0f, 14, LIGHTGRAY);
    }
}

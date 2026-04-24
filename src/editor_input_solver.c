#include "editor_input_internal.h"
#include "app_commands.h"
#include "ui.h"
#include "raylib.h"

static int solver_mouse_cursor_for_handle(WorkspaceResizeHandle handle) {
    if (handle == WORKSPACE_RESIZE_HANDLE_SIDE_PANEL) {
        return MOUSE_CURSOR_RESIZE_EW;
    }

    return MOUSE_CURSOR_DEFAULT;
}

static void solver_process_command_queue(AppContext *app) {
    EditorCommand command;

    while (app_pop_command(app, &command)) {
        app_handle_command(app, command);
    }
}

static Rectangle solver_workspace_rect(const WorkspaceFrame *frame) {
    return (Rectangle){
        0.0f,
        frame->toolbox_rect.y,
        frame->side_panel_rect.x,
        frame->side_panel_rect.height
    };
}

static Rectangle solver_input_field_rect(UiSolverLayout layout) {
    return (Rectangle){ layout.input_rect.x, layout.input_rect.y + 20.0f, layout.input_rect.width, 44.0f };
}

static void process_solver_keyboard(AppContext *app) {
    int codepoint;

    if (!app->solver.input_focused) {
        if (IsKeyPressed(KEY_B)) {
            app_queue_command(app, EDITOR_COMMAND_MODE_BUILD);
        }
        if (IsKeyPressed(KEY_C)) {
            app_queue_command(app, EDITOR_COMMAND_MODE_COMPARE);
        }
        return;
    }

    codepoint = GetCharPressed();
    while (codepoint > 0) {
        app_solver_insert_char(app, codepoint);
        codepoint = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        app_solver_backspace(app);
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        app_update_solver(app);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        app->solver.input_focused = false;
    }
}

void editor_input_process_solver_frame(
    AppContext *app,
    EditorInputState *state,
    WorkspaceLayoutPrefs *layout_prefs,
    WorkspaceFrame *frame,
    WorkspaceResizeHandles *resize_handles,
    const TopbarLayout *topbar_layout
) {
    WorkspaceResizeHandle hovered_resize_handle;
    Vector2 mouse_pos;
    Rectangle workspace_rect;
    UiSolverLayout solver_layout;
    Rectangle input_field;
    float wheel_move;
    bool mouse_left_pressed;
    bool mouse_left_down;
    bool mouse_left_released;

    mouse_pos = GetMousePosition();
    hovered_resize_handle = workspace_layout_hit_test_handle(resize_handles, mouse_pos);
    if (hovered_resize_handle != WORKSPACE_RESIZE_HANDLE_SIDE_PANEL) {
        hovered_resize_handle = WORKSPACE_RESIZE_HANDLE_NONE;
    }

    mouse_left_pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    mouse_left_down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    mouse_left_released = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    if (state->active_resize_handle != WORKSPACE_RESIZE_HANDLE_NONE &&
        state->active_resize_handle != WORKSPACE_RESIZE_HANDLE_SIDE_PANEL) {
        state->active_resize_handle = WORKSPACE_RESIZE_HANDLE_NONE;
    }

    if (state->active_resize_handle != WORKSPACE_RESIZE_HANDLE_NONE) {
        SetMouseCursor(solver_mouse_cursor_for_handle(state->active_resize_handle));
    } else {
        SetMouseCursor(solver_mouse_cursor_for_handle(hovered_resize_handle));
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        mouse_left_pressed &&
        hovered_resize_handle == WORKSPACE_RESIZE_HANDLE_SIDE_PANEL) {
        state->active_resize_handle = hovered_resize_handle;
    }

    if (mouse_left_pressed && hovered_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE) {
        topbar_handle_click(app, topbar_layout, mouse_pos, &state->shortcuts_open);
    }

    if (IsKeyPressed(KEY_SLASH)) {
        state->shortcuts_open = !state->shortcuts_open;
    } else if (state->shortcuts_open && IsKeyPressed(KEY_ESCAPE)) {
        state->shortcuts_open = false;
    }

    process_solver_keyboard(app);
    solver_process_command_queue(app);

    if (state->active_resize_handle != WORKSPACE_RESIZE_HANDLE_NONE) {
        if (mouse_left_down &&
            workspace_layout_apply_drag(
                layout_prefs,
                state->active_resize_handle,
                mouse_pos,
                frame->window_width,
                frame->window_height
            )) {
            state->layout_save_pending = true;
            *frame = workspace_layout_compute_frame(layout_prefs, frame->window_width, frame->window_height);
            *resize_handles = workspace_layout_compute_handles(frame);
        }
        if (mouse_left_released) {
            state->active_resize_handle = WORKSPACE_RESIZE_HANDLE_NONE;
            if (state->layout_save_pending) {
                workspace_layout_save_prefs(layout_prefs);
                state->layout_save_pending = false;
            }
        }
    }

    workspace_rect = solver_workspace_rect(frame);
    solver_layout = ui_measure_solver_workspace(workspace_rect);
    input_field = solver_input_field_rect(solver_layout);
    if (mouse_left_pressed && !CheckCollisionPointRec(mouse_pos, frame->topbar_rect)) {
        app->solver.input_focused = CheckCollisionPointRec(mouse_pos, input_field);
    }

    wheel_move = GetMouseWheelMove();
    if (wheel_move != 0.0f && CheckCollisionPointRec(mouse_pos, solver_layout.steps_rect)) {
        float max_scroll;

        max_scroll = ui_solver_steps_content_height(app) - solver_layout.steps_rect.height;
        if (max_scroll < 0.0f) {
            max_scroll = 0.0f;
        }
        app->solver.steps_scroll -= wheel_move * 34.0f;
        if (app->solver.steps_scroll < 0.0f) {
            app->solver.steps_scroll = 0.0f;
        } else if (app->solver.steps_scroll > max_scroll) {
            app->solver.steps_scroll = max_scroll;
        }
    }
}

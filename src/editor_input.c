#include "editor_input.h"
#include "editor_input_internal.h"
#include "app_analysis.h"
#include "app_canvas.h"
#include "app_commands.h"
#include "ui.h"
#include "raylib.h"
#include <string.h>

static int mouse_cursor_for_handle(WorkspaceResizeHandle handle) {
    if (handle == WORKSPACE_RESIZE_HANDLE_TOOLBOX || handle == WORKSPACE_RESIZE_HANDLE_SIDE_PANEL) {
        return MOUSE_CURSOR_RESIZE_EW;
    }
    if (handle == WORKSPACE_RESIZE_HANDLE_WAVE_PANEL) {
        return MOUSE_CURSOR_RESIZE_NS;
    }

    return MOUSE_CURSOR_DEFAULT;
}

static LogicNode *find_node_at(AppContext *app, Vector2 mouse_pos) {
    int index;

    for (index = (int)app->graph.node_count - 1; index >= 0; index--) {
        LogicNode *node;

        node = &app->graph.nodes[index];
        if (!logic_node_is_active(node)) {
            continue;
        }
        if (CheckCollisionPointRec(mouse_pos, node->rect)) {
            return node;
        }
    }

    return NULL;
}

static void queue_keyboard_commands(AppContext *app) {
    bool shift_down;

    shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_V)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_SELECT);
    }
    if (IsKeyPressed(KEY_ONE)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_INPUT);
    }
    if (IsKeyPressed(KEY_TWO)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_OUTPUT);
    }
    if (IsKeyPressed(KEY_THREE)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_AND);
    }
    if (IsKeyPressed(KEY_FOUR)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_OR);
    }
    if (IsKeyPressed(KEY_FIVE)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_NOT);
    }
    if (IsKeyPressed(KEY_SIX)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_XOR);
    }
    if (IsKeyPressed(KEY_SEVEN)) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_CLOCK);
    }
    if (IsKeyPressed(KEY_B)) {
        app_queue_command(app, EDITOR_COMMAND_MODE_BUILD);
    }
    if (IsKeyPressed(KEY_C)) {
        app_queue_command(app, EDITOR_COMMAND_MODE_COMPARE);
    }
    if (IsKeyPressed(KEY_R)) {
        app_queue_command(app, EDITOR_COMMAND_SIM_RESET);
    }
    if (IsKeyPressed(KEY_SPACE)) {
        app_queue_command(app, EDITOR_COMMAND_SIM_TOGGLE);
    }
    if (IsKeyPressed(KEY_PERIOD)) {
        app_queue_command(app, EDITOR_COMMAND_SIM_STEP);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        app_queue_command(app, EDITOR_COMMAND_CANCEL);
    }
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        app_queue_command(app, EDITOR_COMMAND_DELETE_SELECTION);
    }
    if (IsKeyPressed(KEY_TAB)) {
        app_queue_command(app, shift_down ? EDITOR_COMMAND_SELECT_PREVIOUS_NODE : EDITOR_COMMAND_SELECT_NEXT_NODE);
    }
    if (IsKeyPressed(KEY_LEFT)) {
        app_queue_command(app, EDITOR_COMMAND_MOVE_SELECTION_LEFT);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        app_queue_command(app, EDITOR_COMMAND_MOVE_SELECTION_RIGHT);
    }
    if (IsKeyPressed(KEY_UP)) {
        app_queue_command(
            app,
            (app->selection.selected_node && app->selection.focused_panel == APP_PANEL_CANVAS) ?
                EDITOR_COMMAND_MOVE_SELECTION_UP :
                EDITOR_COMMAND_SELECT_PREVIOUS_ROW
        );
    }
    if (IsKeyPressed(KEY_DOWN)) {
        app_queue_command(
            app,
            (app->selection.selected_node && app->selection.focused_panel == APP_PANEL_CANVAS) ?
                EDITOR_COMMAND_MOVE_SELECTION_DOWN :
                EDITOR_COMMAND_SELECT_NEXT_ROW
        );
    }
}

static void process_command_queue(AppContext *app) {
    EditorCommand command;

    while (app_pop_command(app, &command)) {
        app_handle_command(app, command);
    }
}

static bool point_is_toolbar(Vector2 mouse_pos, const WorkspaceFrame *frame) {
    return CheckCollisionPointRec(mouse_pos, frame->topbar_rect) || CheckCollisionPointRec(mouse_pos, frame->toolbox_rect);
}

static bool select_truth_row_at(AppContext *app, Rectangle side_panel_rect, Vector2 mouse_pos) {
    UiContextPanelLayout layout;
    uint32_t row_index;

    if (!CheckCollisionPointRec(mouse_pos, side_panel_rect) || !app->analysis.truth_table) {
        return false;
    }

    layout = ui_measure_context_panel(app, side_panel_rect);
    for (row_index = 0U; row_index < layout.visible_truth_rows; row_index++) {
        Rectangle row_rect;

        if (!ui_context_truth_table_row_rect(app, &layout, row_index, &row_rect)) {
            continue;
        }
        if (CheckCollisionPointRec(mouse_pos, row_rect)) {
            app_select_row(app, row_index);
            return true;
        }
    }

    return false;
}

static void handle_toolbox_click(AppContext *app, Rectangle toolbox_rect, Vector2 mouse_pos) {
    static const AppTool slot_tools[] = {
        APP_TOOL_INPUT,
        APP_TOOL_OUTPUT,
        APP_TOOL_AND,
        APP_TOOL_OR,
        APP_TOOL_NOT,
        APP_TOOL_XOR,
        APP_TOOL_CLOCK,
    };
    static const EditorCommand slot_commands[] = {
        EDITOR_COMMAND_TOOL_INPUT,
        EDITOR_COMMAND_TOOL_OUTPUT,
        EDITOR_COMMAND_TOOL_AND,
        EDITOR_COMMAND_TOOL_OR,
        EDITOR_COMMAND_TOOL_NOT,
        EDITOR_COMMAND_TOOL_XOR,
        EDITOR_COMMAND_TOOL_CLOCK,
    };
    int slot;

    if (!CheckCollisionPointRec(mouse_pos, toolbox_rect)) {
        return;
    }

    slot = ui_toolbox_slot_at(toolbox_rect, mouse_pos);
    if (slot < 0 || slot >= (int)(sizeof(slot_tools) / sizeof(slot_tools[0]))) {
        return;
    }

    if (app->active_tool == slot_tools[slot]) {
        app_queue_command(app, EDITOR_COMMAND_TOOL_SELECT);
    } else {
        app_queue_command(app, slot_commands[slot]);
    }
}

void editor_input_init(EditorInputState *state) {
    memset(state, 0, sizeof(*state));
}

void editor_input_process_frame(
    AppContext *app,
    EditorInputState *state,
    WorkspaceLayoutPrefs *layout_prefs,
    WorkspaceFrame *frame,
    WorkspaceResizeHandles *resize_handles,
    const TopbarLayout *topbar_layout,
    SourceWatch *source_watch
) {
    WorkspaceResizeHandle hovered_resize_handle;
    Vector2 mouse_pos;
    Vector2 world_mouse_pos;
    Vector2 zoom_anchor;
    bool canvas_hovered;
    bool canvas_pan_blocking_interactions;
    bool mouse_left_pressed;
    bool mouse_left_down;
    bool mouse_left_released;
    bool mouse_right_pressed;

    mouse_pos = GetMousePosition();
    hovered_resize_handle = workspace_layout_hit_test_handle(resize_handles, mouse_pos);
    canvas_hovered = CheckCollisionPointRec(mouse_pos, frame->canvas_rect);
    zoom_anchor = canvas_hovered ? mouse_pos :
        (Vector2){
            frame->canvas_rect.x + (frame->canvas_rect.width * 0.5f),
            frame->canvas_rect.y + (frame->canvas_rect.height * 0.5f)
        };
    canvas_pan_blocking_interactions = false;
    mouse_left_pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    mouse_left_down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    mouse_left_released = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    mouse_right_pressed = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);

    if (app->mode == MODE_SOLVER) {
        editor_input_process_solver_frame(app, state, layout_prefs, frame, resize_handles, topbar_layout);
        return;
    }

    if (state->active_resize_handle != WORKSPACE_RESIZE_HANDLE_NONE) {
        SetMouseCursor(mouse_cursor_for_handle(state->active_resize_handle));
    } else if (state->canvas_pan_active) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
    } else {
        SetMouseCursor(mouse_cursor_for_handle(hovered_resize_handle));
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        mouse_left_pressed &&
        hovered_resize_handle != WORKSPACE_RESIZE_HANDLE_NONE) {
        state->active_resize_handle = hovered_resize_handle;
        app->canvas.drag_node = NULL;
        app_cancel_wire_drag(app);
    }

    if (!state->shortcuts_open) {
        queue_keyboard_commands(app);
    }
    if (IsKeyPressed(KEY_SLASH)) {
        state->shortcuts_open = !state->shortcuts_open;
    } else if (state->shortcuts_open && IsKeyPressed(KEY_ESCAPE)) {
        state->shortcuts_open = false;
    }
    if (!state->shortcuts_open && IsKeyPressed(KEY_HOME)) {
        app_reset_canvas_view(app);
    }
    if (!state->shortcuts_open &&
        (canvas_hovered || app->selection.focused_panel == APP_PANEL_CANVAS) &&
        (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))) {
        app_zoom_canvas_at(app, frame->canvas_rect, zoom_anchor, 1.12f);
    }
    if (!state->shortcuts_open &&
        (canvas_hovered || app->selection.focused_panel == APP_PANEL_CANVAS) &&
        (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))) {
        app_zoom_canvas_at(app, frame->canvas_rect, zoom_anchor, 1.0f / 1.12f);
    }
    if (mouse_left_pressed && hovered_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE) {
        if (!topbar_handle_click(app, topbar_layout, mouse_pos, &state->shortcuts_open)) {
            handle_toolbox_click(app, frame->toolbox_rect, mouse_pos);
        }
    }
    process_command_queue(app);

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

    if (source_watch->active) {
        source_watch_reload_if_changed(source_watch, app, frame->canvas_rect);
    }

    if (state->canvas_pan_active) {
        canvas_pan_blocking_interactions = true;
        if (mouse_left_down) {
            Vector2 pan_delta;
            float dx;
            float dy;

            pan_delta = (Vector2){
                mouse_pos.x - state->pan_last_mouse.x,
                mouse_pos.y - state->pan_last_mouse.y
            };
            if (pan_delta.x != 0.0f || pan_delta.y != 0.0f) {
                dx = mouse_pos.x - state->click_start_pos.x;
                dy = mouse_pos.y - state->click_start_pos.y;
                if (!state->canvas_pan_moved && ((dx * dx) + (dy * dy) > 16.0f)) {
                    state->canvas_pan_moved = true;
                }
                app_pan_canvas(app, pan_delta);
                state->pan_last_mouse = mouse_pos;
            }
        } else {
            state->canvas_pan_active = false;
        }
    }

    world_mouse_pos = app_canvas_screen_to_world(app, frame->canvas_rect, mouse_pos);
    if (app->interaction.wire_drag_active) {
        app_update_wire_drag(app, ui_get_pin_at(app, frame->canvas_rect, mouse_pos), world_mouse_pos);
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        !canvas_pan_blocking_interactions &&
        mouse_left_pressed &&
        !point_is_toolbar(mouse_pos, frame) &&
        !app_tool_places_node(app->active_tool)) {
        LogicPin *pin;

        pin = ui_get_pin_at(app, frame->canvas_rect, mouse_pos);
        if (pin && CheckCollisionPointRec(mouse_pos, frame->canvas_rect)) {
            app_begin_wire_drag(app, pin, world_mouse_pos);
        } else if (CheckCollisionPointRec(mouse_pos, frame->canvas_rect)) {
            LogicPin *wire_sink;
            LogicNode *hit_node;

            app->canvas.drag_node = NULL;
            app_set_panel_focus(app, APP_PANEL_CANVAS);
            wire_sink = ui_get_wire_at(app, frame->canvas_rect, mouse_pos);
            hit_node = find_node_at(app, world_mouse_pos);
            if (wire_sink) {
                app_cancel_wire_drag(app);
                app_select_wire_by_sink(app, wire_sink);
            } else if (hit_node) {
                app_cancel_wire_drag(app);
                app->canvas.drag_node = hit_node;
                app->selection.selected_node = hit_node;
                app->selection.selected_wire_sink = NULL;
                app->canvas.drag_offset = (Vector2){
                    world_mouse_pos.x - hit_node->pos.x,
                    world_mouse_pos.y - hit_node->pos.y
                };
                state->click_start_pos = mouse_pos;
                state->click_moved = false;
            } else {
                app_cancel_wire_drag(app);
                state->canvas_pan_active = true;
                state->canvas_pan_moved = false;
                state->pan_last_mouse = mouse_pos;
                state->click_start_pos = mouse_pos;
            }
        } else {
            select_truth_row_at(app, frame->side_panel_rect, mouse_pos);
        }
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        !canvas_pan_blocking_interactions &&
        mouse_left_released &&
        app_tool_places_node(app->active_tool) &&
        CheckCollisionPointRec(mouse_pos, frame->canvas_rect)) {
        LogicNode *new_node;

        new_node = app_add_node(
            app,
            app_node_type_for_tool(app->active_tool),
            app_snap_node_position(world_mouse_pos, app_node_type_for_tool(app->active_tool))
        );
        if (new_node) {
            app->selection.selected_node = new_node;
            app_set_panel_focus(app, APP_PANEL_CANVAS);
            app_rebuild_derived_state(app);
        }
        app_set_tool(app, APP_TOOL_SELECT);
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        !canvas_pan_blocking_interactions &&
        mouse_left_released &&
        app->interaction.wire_drag_active) {
        app_commit_wire_drag(app, ui_get_pin_at(app, frame->canvas_rect, mouse_pos));
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        !canvas_pan_blocking_interactions &&
        mouse_left_down &&
        app->canvas.drag_node) {
        float dx;
        float dy;

        dx = mouse_pos.x - state->click_start_pos.x;
        dy = mouse_pos.y - state->click_start_pos.y;
        if (!state->click_moved && ((dx * dx) + (dy * dy) > 16.0f)) {
            state->click_moved = true;
        }
        if (state->click_moved) {
            Vector2 dragged_position;

            dragged_position = (Vector2){
                world_mouse_pos.x - app->canvas.drag_offset.x,
                world_mouse_pos.y - app->canvas.drag_offset.y
            };
            app->canvas.drag_node->pos = app_snap_live_node_position(app, app->canvas.drag_node, dragged_position);
            app->canvas.drag_node->rect.x = app->canvas.drag_node->pos.x;
            app->canvas.drag_node->rect.y = app->canvas.drag_node->pos.y;
        }
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        !canvas_pan_blocking_interactions &&
        mouse_left_released) {
        if (app->canvas.drag_node && !state->click_moved && app->canvas.drag_node->type == NODE_INPUT) {
            app_toggle_input_value(app, app->canvas.drag_node);
        }
        app->canvas.drag_node = NULL;
        state->click_moved = false;
    }

    if (state->active_resize_handle == WORKSPACE_RESIZE_HANDLE_NONE &&
        mouse_left_released &&
        !state->canvas_pan_active &&
        CheckCollisionPointRec(mouse_pos, frame->canvas_rect) &&
        !app_tool_places_node(app->active_tool) &&
        !app->canvas.drag_node &&
        !app->interaction.wire_drag_active &&
        ui_get_pin_at(app, frame->canvas_rect, mouse_pos) == NULL &&
        ui_get_wire_at(app, frame->canvas_rect, mouse_pos) == NULL &&
        find_node_at(app, world_mouse_pos) == NULL &&
        !state->canvas_pan_moved) {
        app->selection.selected_node = NULL;
        app->selection.selected_wire_sink = NULL;
    }

    if (mouse_left_released && !state->canvas_pan_active) {
        state->canvas_pan_moved = false;
    }

    if (mouse_right_pressed) {
        state->canvas_pan_active = false;
        state->canvas_pan_moved = false;
        app_cancel_interaction(app);
    }

    app_update_simulation(app);
    app_compute_view_context(app);
}

WorkspaceResizeHandle editor_input_active_resize_handle(const EditorInputState *state) {
    return state->active_resize_handle;
}

bool editor_input_shortcuts_open(const EditorInputState *state) {
    return state->shortcuts_open;
}

#include "app_commands.h"
#include "app_analysis.h"
#include "app_canvas.h"
#include "app_internal.h"
#include "node_catalog.h"
#include "raylib.h"
#include <string.h>

bool app_pin_is_input(const LogicPin *pin) {
    return pin &&
        pin->node &&
        pin->index < pin->node->input_count &&
        &pin->node->inputs[pin->index] == pin;
}

bool app_pin_is_output(const LogicPin *pin) {
    return pin &&
        pin->node &&
        pin->index < pin->node->output_count &&
        &pin->node->outputs[pin->index] == pin;
}

bool app_sink_has_connection(const AppContext *app, const LogicPin *pin) {
    uint32_t net_index;

    if (!app_pin_is_input(pin)) {
        return false;
    }

    for (net_index = 0U; net_index < app->graph.net_count; net_index++) {
        uint8_t sink_index;

        for (sink_index = 0U; sink_index < app->graph.nets[net_index].sink_count; sink_index++) {
            if (app->graph.nets[net_index].sinks[sink_index] == pin) {
                return true;
            }
        }
    }

    return false;
}

void app_queue_command(AppContext *app, EditorCommand command) {
    if (!app || command == EDITOR_COMMAND_NONE || app->commands.count >= APP_PENDING_COMMANDS) {
        return;
    }

    app->commands.pending_commands[app->commands.count++] = command;
}

bool app_pop_command(AppContext *app, EditorCommand *command) {
    uint8_t index;

    if (!app || !command || app->commands.count == 0U) {
        return false;
    }

    *command = app->commands.pending_commands[0];
    for (index = 1U; index < app->commands.count; index++) {
        app->commands.pending_commands[index - 1U] = app->commands.pending_commands[index];
    }
    app->commands.count--;
    return true;
}

void app_cancel_interaction(AppContext *app) {
    app->interaction.wiring_active = false;
    app->interaction.active_pin = NULL;
    app->canvas.drag_node = NULL;
    app->selection.selected_wire_sink = NULL;
    app_cancel_wire_drag(app);
    app->active_tool = APP_TOOL_SELECT;
}

void app_select_wire_by_sink(AppContext *app, LogicPin *sink) {
    if (!app) {
        return;
    }

    app->selection.selected_wire_sink = sink;
    if (sink) {
        app->selection.selected_node = NULL;
        app->canvas.drag_node = NULL;
        app_set_panel_focus(app, APP_PANEL_CANVAS);
    }
}

bool app_delete_selected_wire(AppContext *app) {
    LogicPin *sink;

    if (!app || !app->selection.selected_wire_sink) {
        return false;
    }

    sink = app->selection.selected_wire_sink;
    app->selection.selected_wire_sink = NULL;
    if (!logic_disconnect_sink(&app->graph, sink)) {
        return false;
    }

    app_rebuild_derived_state(app);
    return true;
}

void app_step_simulation(AppContext *app) {
    logic_tick(&app->graph);
    app_record_waveforms(app);
    app->simulation.last_tick_time = GetTime();
}

void app_reset_simulation(AppContext *app) {
    if (!app) {
        return;
    }

    app->simulation.active = false;
    app->simulation.last_tick_time = 0.0;
    app->simulation.waveform_index = 0U;
    memset(app->simulation.waveforms, 0, sizeof(app->simulation.waveforms));
    logic_evaluate(&app->graph);
    app_compute_view_context(app);
}

void app_update_simulation(AppContext *app) {
    double now;
    double interval;

    if (!app->simulation.active || app->simulation.speed <= 0.0f) {
        return;
    }

    now = GetTime();
    interval = 1.0 / (double)app->simulation.speed;
    if (app->simulation.last_tick_time <= 0.0) {
        app->simulation.last_tick_time = now;
    }

    while ((now - app->simulation.last_tick_time) >= interval) {
        logic_tick(&app->graph);
        app_record_waveforms(app);
        app->simulation.last_tick_time += interval;
    }
}

bool app_delete_selected_node(AppContext *app) {
    if (!app || !app->selection.selected_node) {
        return false;
    }

    if (app->interaction.active_pin && app->interaction.active_pin->node == app->selection.selected_node) {
        app->interaction.active_pin = NULL;
        app->interaction.wiring_active = false;
    }
    if (app->interaction.wire_drag_pin && app->interaction.wire_drag_pin->node == app->selection.selected_node) {
        app_cancel_wire_drag(app);
    }

    if (!logic_remove_node(&app->graph, app->selection.selected_node)) {
        return false;
    }

    app->selection.selected_node = NULL;
    app->selection.selected_wire_sink = NULL;
    app->canvas.drag_node = NULL;
    app_rebuild_derived_state(app);
    return true;
}

bool app_select_next_node(AppContext *app, int direction) {
    uint32_t index;
    uint32_t start_index;

    if (!app || app->graph.node_count == 0U) {
        return false;
    }

    if (direction >= 0) {
        start_index = 0U;
        if (app->selection.selected_node) {
            start_index = (uint32_t)(app->selection.selected_node - app->graph.nodes) + 1U;
        }

        for (index = 0U; index < app->graph.node_count; index++) {
            uint32_t node_index;

            node_index = (start_index + index) % app->graph.node_count;
            if (!logic_node_is_active(&app->graph.nodes[node_index])) {
                continue;
            }
            app->selection.selected_node = &app->graph.nodes[node_index];
            app->selection.selected_wire_sink = NULL;
            app->selection.focused_panel = APP_PANEL_CANVAS;
            return true;
        }
    } else {
        start_index = app->graph.node_count - 1U;
        if (app->selection.selected_node) {
            start_index = (uint32_t)(app->selection.selected_node - app->graph.nodes);
            start_index = (start_index == 0U) ? app->graph.node_count - 1U : start_index - 1U;
        }

        for (index = 0U; index < app->graph.node_count; index++) {
            uint32_t node_index;

            node_index = (start_index + app->graph.node_count - index) % app->graph.node_count;
            if (!logic_node_is_active(&app->graph.nodes[node_index])) {
                continue;
            }
            app->selection.selected_node = &app->graph.nodes[node_index];
            app->selection.selected_wire_sink = NULL;
            app->selection.focused_panel = APP_PANEL_CANVAS;
            return true;
        }
    }

    return false;
}

bool app_move_selected_node(AppContext *app, int grid_dx, int grid_dy) {
    Vector2 moved_position;

    if (!app || !app->selection.selected_node) {
        return false;
    }

    moved_position = (Vector2){
        app->selection.selected_node->pos.x + ((float)grid_dx * APP_GRID_SIZE),
        app->selection.selected_node->pos.y + ((float)grid_dy * APP_GRID_SIZE)
    };
    app->selection.selected_node->pos = app_snap_live_node_position(app, app->selection.selected_node, moved_position);
    app->selection.selected_node->rect.x = app->selection.selected_node->pos.x;
    app->selection.selected_node->rect.y = app->selection.selected_node->pos.y;
    app->selection.focused_panel = APP_PANEL_CANVAS;
    return true;
}

LogicNode *app_create_node_for_tool(AppContext *app, AppTool tool) {
    LogicNode *node;
    NodeType type;

    if (!app || !app_tool_places_node(tool)) {
        return NULL;
    }

    type = app_node_type_for_tool(tool);
    node = app_add_named_node(app, type, NULL, app_default_node_position(app, type));
    if (!node) {
        return NULL;
    }

    app->selection.selected_node = node;
    app->selection.selected_wire_sink = NULL;
    app->active_tool = APP_TOOL_SELECT;
    app_cancel_wire_drag(app);
    app_rebuild_derived_state(app);
    app_set_panel_focus(app, APP_PANEL_CANVAS);
    return node;
}

bool app_select_node_by_index(AppContext *app, uint32_t node_index) {
    if (!app || node_index >= app->graph.node_count || !logic_node_is_active(&app->graph.nodes[node_index])) {
        return false;
    }

    app->selection.selected_node = &app->graph.nodes[node_index];
    app->selection.selected_wire_sink = NULL;
    app->canvas.drag_node = NULL;
    app_set_panel_focus(app, APP_PANEL_CANVAS);
    return true;
}

bool app_activate_pin_by_index(AppContext *app, uint32_t node_index, bool is_output_pin, uint8_t pin_index) {
    LogicNode *node;
    LogicPin *pin;

    if (!app || node_index >= app->graph.node_count) {
        return false;
    }

    node = &app->graph.nodes[node_index];
    if (!logic_node_is_active(node)) {
        return false;
    }

    if (is_output_pin) {
        if (pin_index >= node->output_count) {
            return false;
        }
        pin = &node->outputs[pin_index];
    } else {
        if (pin_index >= node->input_count) {
            return false;
        }
        pin = &node->inputs[pin_index];
    }

    app->active_tool = APP_TOOL_SELECT;
    app_set_panel_focus(app, APP_PANEL_CANVAS);
    app->selection.selected_node = node;
    app->selection.selected_wire_sink = NULL;

    if (!app->interaction.wiring_active) {
        app->interaction.wiring_active = true;
        app->interaction.active_pin = pin;
        app_cancel_wire_drag(app);
        return true;
    }

    if (!app_connect_pins(app, app->interaction.active_pin, pin)) {
        app->interaction.active_pin = pin;
        return false;
    }

    app->interaction.wiring_active = false;
    app->interaction.active_pin = NULL;
    return true;
}

bool app_connect_pins(AppContext *app, LogicPin *first_pin, LogicPin *second_pin) {
    LogicPin *source_pin;
    LogicPin *sink_pin;

    if (!app || !first_pin || !second_pin) {
        return false;
    }

    if (app_pin_is_output(first_pin) && app_pin_is_input(second_pin)) {
        source_pin = first_pin;
        sink_pin = second_pin;
    } else if (app_pin_is_output(second_pin) && app_pin_is_input(first_pin)) {
        source_pin = second_pin;
        sink_pin = first_pin;
    } else {
        return false;
    }

    if (!logic_connect(&app->graph, source_pin, sink_pin)) {
        return false;
    }

    app_rebuild_derived_state(app);
    return true;
}

bool app_begin_wire_drag(AppContext *app, LogicPin *pin, Vector2 pointer_pos) {
    if (!app || !pin || (!app_pin_is_input(pin) && !app_pin_is_output(pin))) {
        return false;
    }

    app->active_tool = APP_TOOL_SELECT;
    app->selection.selected_node = pin->node;
    app->selection.selected_wire_sink = NULL;
    app->selection.focused_panel = APP_PANEL_CANVAS;
    app->interaction.wiring_active = false;
    app->interaction.active_pin = NULL;
    app->interaction.wire_drag_active = true;
    app->interaction.wire_drag_pin = pin;
    app->interaction.wire_hover_pin = NULL;
    app->interaction.wire_drag_replacing_sink = false;
    app->interaction.wire_drag_pos = pointer_pos;
    return true;
}

void app_update_wire_drag(AppContext *app, LogicPin *hover_pin, Vector2 pointer_pos) {
    if (!app || !app->interaction.wire_drag_active) {
        return;
    }

    app->interaction.wire_drag_pos = pointer_pos;
    app->interaction.wire_hover_pin = hover_pin;
    app->interaction.wire_drag_replacing_sink =
        hover_pin &&
        app_pin_is_input(hover_pin) &&
        hover_pin != app->interaction.wire_drag_pin &&
        app_sink_has_connection(app, hover_pin);
}

bool app_commit_wire_drag(AppContext *app, LogicPin *pin) {
    bool connected;

    if (!app || !app->interaction.wire_drag_active || !app->interaction.wire_drag_pin) {
        return false;
    }

    connected = false;
    if (pin) {
        connected = app_connect_pins(app, app->interaction.wire_drag_pin, pin);
    }

    app_cancel_wire_drag(app);
    return connected;
}

void app_cancel_wire_drag(AppContext *app) {
    if (!app) {
        return;
    }

    app->interaction.wire_drag_active = false;
    app->interaction.wire_drag_pin = NULL;
    app->interaction.wire_hover_pin = NULL;
    app->interaction.wire_drag_replacing_sink = false;
    app->interaction.wire_drag_pos = (Vector2){ 0.0f, 0.0f };
}

bool app_select_truth_row_by_index(AppContext *app, uint32_t row_index) {
    if (!app || !app->analysis.truth_table || row_index >= app->analysis.truth_table->row_count) {
        return false;
    }

    app_select_row(app, row_index);
    return true;
}

void app_handle_command(AppContext *app, EditorCommand command) {
    if (!app) {
        return;
    }

    switch (command) {
        case EDITOR_COMMAND_MODE_BUILD:
            app_set_mode(app, MODE_BUILD);
            break;
        case EDITOR_COMMAND_MODE_COMPARE:
            app_set_mode(app, MODE_COMPARE);
            break;
        case EDITOR_COMMAND_MODE_SOLVER:
            app_set_mode(app, MODE_SOLVER);
            break;
        case EDITOR_COMMAND_TOOL_SELECT:
            app_set_tool(app, APP_TOOL_SELECT);
            break;
        case EDITOR_COMMAND_TOOL_INPUT:
            app_set_tool(app, APP_TOOL_INPUT);
            break;
        case EDITOR_COMMAND_TOOL_OUTPUT:
            app_set_tool(app, APP_TOOL_OUTPUT);
            break;
        case EDITOR_COMMAND_TOOL_AND:
            app_set_tool(app, APP_TOOL_AND);
            break;
        case EDITOR_COMMAND_TOOL_OR:
            app_set_tool(app, APP_TOOL_OR);
            break;
        case EDITOR_COMMAND_TOOL_NOT:
            app_set_tool(app, APP_TOOL_NOT);
            break;
        case EDITOR_COMMAND_TOOL_XOR:
            app_set_tool(app, APP_TOOL_XOR);
            break;
        case EDITOR_COMMAND_TOOL_CLOCK:
            app_set_tool(app, APP_TOOL_CLOCK);
            break;
        case EDITOR_COMMAND_SIM_TOGGLE:
            app->simulation.active = !app->simulation.active;
            app->simulation.last_tick_time = GetTime();
            break;
        case EDITOR_COMMAND_SIM_STEP:
            app->simulation.active = false;
            app_step_simulation(app);
            break;
        case EDITOR_COMMAND_SIM_RESET:
            app_reset_simulation(app);
            break;
        case EDITOR_COMMAND_CANCEL:
            app_cancel_interaction(app);
            break;
        case EDITOR_COMMAND_DELETE_SELECTION:
            if (app->selection.selected_wire_sink) {
                app_delete_selected_wire(app);
            } else {
                app_delete_selected_node(app);
            }
            break;
        case EDITOR_COMMAND_SELECT_NEXT_NODE:
            app_select_next_node(app, 1);
            break;
        case EDITOR_COMMAND_SELECT_PREVIOUS_NODE:
            app_select_next_node(app, -1);
            break;
        case EDITOR_COMMAND_MOVE_SELECTION_LEFT:
            app_move_selected_node(app, -1, 0);
            break;
        case EDITOR_COMMAND_MOVE_SELECTION_RIGHT:
            app_move_selected_node(app, 1, 0);
            break;
        case EDITOR_COMMAND_MOVE_SELECTION_UP:
            app_move_selected_node(app, 0, -1);
            break;
        case EDITOR_COMMAND_MOVE_SELECTION_DOWN:
            app_move_selected_node(app, 0, 1);
            break;
        case EDITOR_COMMAND_SELECT_PREVIOUS_ROW:
            if (app->analysis.truth_table && app->selection.selected_row > 0U) {
                app_select_row(app, app->selection.selected_row - 1U);
            }
            break;
        case EDITOR_COMMAND_SELECT_NEXT_ROW:
            if (app->analysis.truth_table && app->selection.selected_row + 1U < app->analysis.truth_table->row_count) {
                app_select_row(app, app->selection.selected_row + 1U);
            }
            break;
        case EDITOR_COMMAND_NONE:
        default:
            break;
    }
}

AppTool app_tool_from_node_type(NodeType type) {
    return node_catalog_tool(type);
}

NodeType app_node_type_for_tool(AppTool tool) {
    return node_catalog_type_for_tool(tool);
}

bool app_tool_places_node(AppTool tool) {
    return node_catalog_tool_places_node(tool);
}

const char *app_mode_label(AppMode mode) {
    if (mode == MODE_SOLVER) {
        return "Solver";
    }
    if (mode == MODE_COMPARE) {
        return "Compare";
    }

    return "Edit";
}

const char *app_tool_label(AppTool tool) {
    switch (tool) {
        case APP_TOOL_INPUT:
            return "Input";
        case APP_TOOL_OUTPUT:
            return "Output";
        case APP_TOOL_AND:
            return "AND";
        case APP_TOOL_OR:
            return "OR";
        case APP_TOOL_NOT:
            return "NOT";
        case APP_TOOL_XOR:
            return "XOR";
        case APP_TOOL_CLOCK:
            return "Clock";
        case APP_TOOL_SELECT:
        default:
            return "Select";
    }
}

#include "app.h"
#include "app_analysis.h"
#include "app_canvas.h"
#include "app_commands.h"
#include "app_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_NAME_BUFFER_SIZE 32
#define APP_SOLVER_SEED_EXPRESSION "!AB + !(A + B) + !AC + AB"

static uint32_t app_count_nodes_of_type(const AppContext *app, NodeType type) {
    uint32_t count;
    uint32_t index;

    count = 0U;
    for (index = 0U; index < app->graph.node_count; index++) {
        if (app->graph.nodes[index].type == type) {
            count++;
        }
    }

    return count;
}

static void app_build_input_name(uint32_t index, char *buffer, size_t buffer_size) {
    char letter;
    uint32_t suffix;

    letter = (char)('A' + (char)(index % 26U));
    suffix = index / 26U;
    if (suffix == 0U) {
        snprintf(buffer, buffer_size, "%c", letter);
        return;
    }

    snprintf(buffer, buffer_size, "%c%u", letter, suffix);
}

static void app_build_output_name(uint32_t index, char *buffer, size_t buffer_size) {
    if (index == 0U) {
        snprintf(buffer, buffer_size, "Z");
        return;
    }

    snprintf(buffer, buffer_size, "Z%u", index);
}

static const char *app_gate_name_prefix(NodeType type) {
    switch (type) {
        case NODE_INPUT:
            return "IN";
        case NODE_OUTPUT:
            return "OUT";
        case NODE_GATE_AND:
            return "AND";
        case NODE_GATE_OR:
            return "OR";
        case NODE_GATE_NOT:
            return "NOT";
        case NODE_GATE_XOR:
            return "XOR";
        case NODE_GATE_NAND:
            return "NAND";
        case NODE_GATE_NOR:
            return "NOR";
        case NODE_GATE_DFF:
            return "DFF";
        case NODE_GATE_LATCH:
            return "LATCH";
        case NODE_GATE_CLOCK:
            return "CLK";
        default:
            return "NODE";
    }
}

static void app_default_node_name(const AppContext *app, NodeType type, char *buffer, size_t buffer_size) {
    uint32_t count;

    count = app_count_nodes_of_type(app, type);
    if (type == NODE_INPUT) {
        app_build_input_name(count, buffer, buffer_size);
        return;
    }
    if (type == NODE_OUTPUT) {
        app_build_output_name(count, buffer, buffer_size);
        return;
    }

    snprintf(buffer, buffer_size, "%s%u", app_gate_name_prefix(type), count + 1U);
}

static LogicValue app_node_waveform_value(const LogicNode *node) {
    if (node->type == NODE_OUTPUT && node->input_count > 0U) {
        return node->inputs[0].value;
    }
    if (node->output_count > 0U) {
        return node->outputs[0].value;
    }
    if (node->input_count > 0U) {
        return node->inputs[0].value;
    }

    return LOGIC_UNKNOWN;
}

LogicNode *app_primary_output_node(AppContext *app) {
    uint32_t index;

    for (index = 0U; index < app->graph.node_count; index++) {
        if (app->graph.nodes[index].type == NODE_OUTPUT) {
            return &app->graph.nodes[index];
        }
    }

    return NULL;
}

void app_record_waveforms(AppContext *app) {
    uint32_t index;

    for (index = 0U; index < app->graph.node_count; index++) {
        LogicNode *node;

        node = &app->graph.nodes[index];
        if (node->type == (NodeType)-1) {
            continue;
        }
        app->simulation.waveforms[index][app->simulation.waveform_index] = app_node_waveform_value(node);
    }

    app->simulation.waveform_index = (app->simulation.waveform_index + 1U) % WAVEFORM_SAMPLES;
}

void app_init(AppContext *app) {
    memset(app, 0, sizeof(*app));
    logic_init_graph(&app->graph);
    app->mode = MODE_BUILD;
    app->active_tool = APP_TOOL_SELECT;
    app->selection.focused_panel = APP_PANEL_CANVAS;
    app->simulation.speed = 2.0f;
    app->comparison.status = APP_COMPARE_NO_TARGET;
    app_reset_canvas_view(app);
    app_set_source_status(app, "Canvas editing");
    app_solver_set_input(app, APP_SOLVER_SEED_EXPRESSION);
}

void app_update_logic(AppContext *app) {
    LogicNode *output_node;
    LogicValue saved_inputs[MAX_PINS];
    LogicNode *saved_input_nodes[MAX_PINS];
    uint8_t saved_count;
    uint32_t index;

    app->simulation.waveform_index = 0U;
    memset(app->simulation.waveforms, 0, sizeof(app->simulation.waveforms));
    output_node = app_primary_output_node(app);

    saved_count = 0U;
    for (index = 0U; index < app->graph.node_count && saved_count < MAX_PINS; index++) {
        LogicNode *node;

        node = &app->graph.nodes[index];
        if (node->type == NODE_INPUT && node->output_count > 0U) {
            saved_input_nodes[saved_count] = node;
            saved_inputs[saved_count] = node->outputs[0].value;
            saved_count++;
        }
    }

    if (app->analysis.truth_table) {
        logic_free_truth_table(app->analysis.truth_table);
    }
    app->analysis.truth_table = logic_generate_truth_table(&app->graph);

    for (index = 0U; index < saved_count; index++) {
        saved_input_nodes[index]->outputs[0].value = saved_inputs[index];
    }
    logic_evaluate(&app->graph);

    free(app->analysis.expression);
    app->analysis.expression = NULL;
    if (output_node) {
        app->analysis.expression = logic_generate_expression(&app->graph, output_node);
    }

    app_update_kmap_grouping(app);
    app_compute_view_context(app);
    app_compare_if_needed(app);
    app_record_waveforms(app);
}

LogicNode *app_add_node(AppContext *app, NodeType type, Vector2 pos) {
    return app_add_named_node(app, type, NULL, pos);
}

LogicNode *app_add_named_node(AppContext *app, NodeType type, const char *custom_name, Vector2 pos) {
    const char *resolved_name;
    char generated_name[APP_NAME_BUFFER_SIZE];
    int width;
    int height;
    LogicNode *node;

    app_node_dimensions(type, &width, &height);
    if (custom_name) {
        resolved_name = custom_name;
    } else {
        app_default_node_name(app, type, generated_name, sizeof(generated_name));
        resolved_name = generated_name;
    }

    node = logic_add_node(&app->graph, type, resolved_name);
    if (!node) {
        return NULL;
    }

    node->pos = pos;
    node->rect = (Rectangle){ pos.x, pos.y, (float)width, (float)height };
    if (type == NODE_INPUT && node->output_count > 0U) {
        node->outputs[0].value = LOGIC_HIGH;
    }

    return node;
}

void app_set_mode(AppContext *app, AppMode mode) {
    app->mode = mode;
    if (mode == MODE_SOLVER) {
        app_cancel_interaction(app);
        app->simulation.active = false;
    } else {
        app->solver.input_focused = false;
    }
    app_compute_view_context(app);
    app_compare_if_needed(app);
}

void app_set_tool(AppContext *app, AppTool tool) {
    app->active_tool = tool;
    app->interaction.wiring_active = false;
    app->interaction.active_pin = NULL;
    app_cancel_wire_drag(app);
}

void app_set_panel_focus(AppContext *app, AppPanelFocus panel) {
    app->selection.focused_panel = panel;
}

void app_select_row(AppContext *app, uint32_t row_index) {
    if (!app->analysis.truth_table || row_index >= app->analysis.truth_table->row_count) {
        return;
    }

    app->selection.selected_row = row_index;
    app->selection.focused_panel = APP_PANEL_TRUTH_TABLE;
}

void app_clear_graph(AppContext *app) {
    uint32_t index;

    if (!app) {
        return;
    }

    for (index = 0U; index < app->graph.node_count; index++) {
        free(app->graph.nodes[index].name);
        app->graph.nodes[index].name = NULL;
    }

    logic_init_graph(&app->graph);
    if (app->analysis.truth_table) {
        logic_free_truth_table(app->analysis.truth_table);
        app->analysis.truth_table = NULL;
    }
    free(app->analysis.expression);
    app->analysis.expression = NULL;
    free(app->analysis.simplified_expression);
    app->analysis.simplified_expression = NULL;
    app->canvas.drag_node = NULL;
    app->selection.selected_node = NULL;
    app->selection.selected_wire_sink = NULL;
    app->interaction.active_pin = NULL;
    app->interaction.wire_drag_pin = NULL;
    app->interaction.wire_hover_pin = NULL;
    app->interaction.wiring_active = false;
    app->interaction.wire_drag_active = false;
    app->interaction.wire_drag_replacing_sink = false;
    app->simulation.active = false;
    app->simulation.last_tick_time = 0.0;
    app->simulation.waveform_index = 0U;
    memset(app->simulation.waveforms, 0, sizeof(app->simulation.waveforms));
    app->selection.selected_row = 0U;
    app->analysis.kmap_group_count = 0U;
    app->comparison.status = APP_COMPARE_NO_TARGET;
    app->comparison.equivalent = false;
}

void app_set_source_path(AppContext *app, const char *path) {
    if (!app) {
        return;
    }

    if (!path) {
        app->source.path[0] = '\0';
        return;
    }

    snprintf(app->source.path, sizeof(app->source.path), "%s", path);
}

void app_set_source_status(AppContext *app, const char *status) {
    if (!app) {
        return;
    }

    if (!status) {
        app->source.status[0] = '\0';
        return;
    }

    snprintf(app->source.status, sizeof(app->source.status), "%s", status);
}

void app_update_solver(AppContext *app) {
    if (!app) {
        return;
    }

    bool_solver_solve(app->solver.input, &app->solver.result);
}

void app_solver_set_input(AppContext *app, const char *input) {
    if (!app) {
        return;
    }

    snprintf(app->solver.input, sizeof(app->solver.input), "%s", input ? input : "");
    app->solver.steps_scroll = 0.0f;
    app_update_solver(app);
}

void app_solver_insert_char(AppContext *app, int codepoint) {
    size_t length;
    char ch;

    if (!app || codepoint < 32 || codepoint > 126) {
        return;
    }

    length = strlen(app->solver.input);
    if (length >= BOOL_SOLVER_INPUT_MAX) {
        return;
    }

    ch = (char)codepoint;
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)toupper((unsigned char)ch);
    }

    app->solver.input[length] = ch;
    app->solver.input[length + 1U] = '\0';
    app->solver.steps_scroll = 0.0f;
    app_update_solver(app);
}

void app_solver_backspace(AppContext *app) {
    size_t length;

    if (!app) {
        return;
    }

    length = strlen(app->solver.input);
    if (length == 0U) {
        return;
    }

    app->solver.input[length - 1U] = '\0';
    app->solver.steps_scroll = 0.0f;
    app_update_solver(app);
}

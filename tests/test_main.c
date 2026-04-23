#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/app.h"
#include "../src/app_analysis.h"
#include "../src/app_canvas.h"
#include "../src/app_commands.h"
#include "../src/bool_solver.h"
#include "../src/circuit_file.h"
#include "../src/draw_util.h"
#include "../src/logic.h"
#include "../src/topbar.h"
#include "../src/ui.h"
#include "../src/ui_geometry.h"
#include "../src/workspace_layout.h"

static void test_gate_and(void) {
    LogicValue inputs_low[] = { LOGIC_LOW, LOGIC_LOW };
    LogicValue inputs_mixed[] = { LOGIC_LOW, LOGIC_HIGH };
    LogicValue inputs_high[] = { LOGIC_HIGH, LOGIC_HIGH };

    assert(logic_eval_gate(NODE_GATE_AND, inputs_low, 2) == LOGIC_LOW);
    assert(logic_eval_gate(NODE_GATE_AND, inputs_mixed, 2) == LOGIC_LOW);
    assert(logic_eval_gate(NODE_GATE_AND, inputs_high, 2) == LOGIC_HIGH);
    printf("test_gate_and passed!\n");
}

static void test_gate_or(void) {
    LogicValue inputs_low[] = { LOGIC_LOW, LOGIC_LOW };
    LogicValue inputs_mixed[] = { LOGIC_LOW, LOGIC_HIGH };
    LogicValue inputs_high[] = { LOGIC_HIGH, LOGIC_HIGH };

    assert(logic_eval_gate(NODE_GATE_OR, inputs_low, 2) == LOGIC_LOW);
    assert(logic_eval_gate(NODE_GATE_OR, inputs_mixed, 2) == LOGIC_HIGH);
    assert(logic_eval_gate(NODE_GATE_OR, inputs_high, 2) == LOGIC_HIGH);
    printf("test_gate_or passed!\n");
}

static void test_simple_circuit(void) {
    LogicGraph graph;
    LogicNode *a;
    LogicNode *b;
    LogicNode *and_gate;
    LogicNode *output;

    logic_init_graph(&graph);
    a = logic_add_node(&graph, NODE_INPUT, "A");
    b = logic_add_node(&graph, NODE_INPUT, "B");
    and_gate = logic_add_node(&graph, NODE_GATE_AND, "AND1");
    output = logic_add_node(&graph, NODE_OUTPUT, "Z");

    logic_connect(&graph, &a->outputs[0], &and_gate->inputs[0]);
    logic_connect(&graph, &b->outputs[0], &and_gate->inputs[1]);
    logic_connect(&graph, &and_gate->outputs[0], &output->inputs[0]);

    a->outputs[0].value = LOGIC_LOW;
    b->outputs[0].value = LOGIC_LOW;
    logic_evaluate(&graph);
    assert(output->inputs[0].value == LOGIC_LOW);

    a->outputs[0].value = LOGIC_HIGH;
    b->outputs[0].value = LOGIC_LOW;
    logic_evaluate(&graph);
    assert(output->inputs[0].value == LOGIC_LOW);

    a->outputs[0].value = LOGIC_HIGH;
    b->outputs[0].value = LOGIC_HIGH;
    logic_evaluate(&graph);
    assert(output->inputs[0].value == LOGIC_HIGH);
    printf("test_simple_circuit passed!\n");
}

static void test_truth_table(void) {
    LogicGraph graph;
    LogicNode *a;
    LogicNode *b;
    LogicNode *and_gate;
    LogicNode *output;
    TruthTable *table;

    logic_init_graph(&graph);
    a = logic_add_node(&graph, NODE_INPUT, "A");
    b = logic_add_node(&graph, NODE_INPUT, "B");
    and_gate = logic_add_node(&graph, NODE_GATE_AND, "AND1");
    output = logic_add_node(&graph, NODE_OUTPUT, "Z");

    logic_connect(&graph, &a->outputs[0], &and_gate->inputs[0]);
    logic_connect(&graph, &b->outputs[0], &and_gate->inputs[1]);
    logic_connect(&graph, &and_gate->outputs[0], &output->inputs[0]);

    table = logic_generate_truth_table(&graph);
    assert(table != NULL);
    assert(table->row_count == 4U);
    assert(table->input_count == 2U);
    assert(table->output_count == 1U);
    assert(table->data[0] == LOGIC_LOW);
    assert(table->data[1] == LOGIC_LOW);
    assert(table->data[2] == LOGIC_LOW);
    assert(table->data[9] == LOGIC_HIGH);
    assert(table->data[10] == LOGIC_HIGH);
    assert(table->data[11] == LOGIC_HIGH);

    logic_free_truth_table(table);
    printf("test_truth_table passed!\n");
}

static void test_expression(void) {
    LogicGraph graph;
    LogicNode *a;
    LogicNode *b;
    LogicNode *and_gate;
    LogicNode *output;
    char *expression;

    logic_init_graph(&graph);
    a = logic_add_node(&graph, NODE_INPUT, "A");
    b = logic_add_node(&graph, NODE_INPUT, "B");
    and_gate = logic_add_node(&graph, NODE_GATE_AND, "AND1");
    output = logic_add_node(&graph, NODE_OUTPUT, "Z");

    logic_connect(&graph, &a->outputs[0], &and_gate->inputs[0]);
    logic_connect(&graph, &b->outputs[0], &and_gate->inputs[1]);
    logic_connect(&graph, &and_gate->outputs[0], &output->inputs[0]);

    expression = logic_generate_expression(&graph, output);
    assert(expression != NULL);
    assert(strcmp(expression, "(A AND B)") == 0);

    free(expression);
    printf("test_expression passed!\n");
}

static void test_bool_solver_simplifies_sample_expression(void) {
    BoolSolverResult result;

    assert(bool_solver_solve("!AB + !(A + B) + !AC + AB", &result));
    assert(strcmp(result.simplified_expression, "!A + B") == 0 || strcmp(result.simplified_expression, "B + !A") == 0);
    assert(result.step_count >= 5U);
    assert(strstr(result.steps[1].title, "De Morgan") != NULL);
    assert(strcmp(result.variables, "A B C") == 0);
    printf("test_bool_solver_simplifies_sample_expression passed!\n");
}

static void test_bool_solver_parser_precedence_and_implicit_and(void) {
    BoolSolverResult result;

    assert(bool_solver_solve("A + BC", &result));
    assert(strcmp(result.simplified_expression, "A + BC") == 0);

    assert(bool_solver_solve("!AB", &result));
    assert(strcmp(result.simplified_expression, "!AB") == 0);
    printf("test_bool_solver_parser_precedence_and_implicit_and passed!\n");
}

static void test_bool_solver_grouped_not_and_constants(void) {
    BoolSolverResult result;

    assert(bool_solver_solve("!(A+B)", &result));
    assert(strcmp(result.simplified_expression, "!A!B") == 0);

    assert(bool_solver_solve("A + 1", &result));
    assert(strcmp(result.simplified_expression, "1") == 0);

    assert(bool_solver_solve("A0", &result));
    assert(strcmp(result.simplified_expression, "0") == 0);
    printf("test_bool_solver_grouped_not_and_constants passed!\n");
}

static void test_bool_solver_reports_invalid_input(void) {
    BoolSolverResult result;

    assert(!bool_solver_solve("A +", &result));
    assert(result.error[0] != '\0');

    assert(!bool_solver_solve("ABCDEFGHI", &result));
    assert(strstr(result.error, "at most") != NULL);
    printf("test_bool_solver_reports_invalid_input passed!\n");
}

static void test_reconnect_replaces_existing_input(void) {
    LogicGraph graph;
    LogicNode *a;
    LogicNode *b;
    LogicNode *output;

    logic_init_graph(&graph);
    a = logic_add_node(&graph, NODE_INPUT, "A");
    b = logic_add_node(&graph, NODE_INPUT, "B");
    output = logic_add_node(&graph, NODE_OUTPUT, "Z");

    assert(logic_connect(&graph, &a->outputs[0], &output->inputs[0]));
    assert(logic_connect(&graph, &b->outputs[0], &output->inputs[0]));

    a->outputs[0].value = LOGIC_LOW;
    b->outputs[0].value = LOGIC_HIGH;
    logic_evaluate(&graph);
    assert(output->inputs[0].value == LOGIC_HIGH);
    assert(graph.net_count == 1U);
    printf("test_reconnect_replaces_existing_input passed!\n");
}

static void test_remove_node_removes_attached_nets(void) {
    LogicGraph graph;
    LogicNode *a;
    LogicNode *b;
    LogicNode *and_gate;
    LogicNode *output;

    logic_init_graph(&graph);
    a = logic_add_node(&graph, NODE_INPUT, "A");
    b = logic_add_node(&graph, NODE_INPUT, "B");
    and_gate = logic_add_node(&graph, NODE_GATE_AND, "AND1");
    output = logic_add_node(&graph, NODE_OUTPUT, "Z");

    assert(logic_connect(&graph, &a->outputs[0], &and_gate->inputs[0]));
    assert(logic_connect(&graph, &b->outputs[0], &and_gate->inputs[1]));
    assert(logic_connect(&graph, &and_gate->outputs[0], &output->inputs[0]));
    assert(graph.net_count == 3U);

    assert(logic_remove_node(&graph, and_gate));
    assert(and_gate->type == (NodeType)-1);
    assert(graph.net_count == 0U);
    printf("test_remove_node_removes_attached_nets passed!\n");
}

static void write_text_file(const char *path, const char *text) {
    FILE *file;

    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fputs(text, file) >= 0);
    fclose(file);
}

static LogicNode *find_node_by_name(AppContext *app, const char *name) {
    uint32_t i;

    for (i = 0; i < app->graph.node_count; i++) {
        LogicNode *node;

        node = &app->graph.nodes[i];
        if (node->type == (NodeType)-1 || !node->name) {
            continue;
        }
        if (strcmp(node->name, name) == 0) {
            return node;
        }
    }

    return NULL;
}

static LogicValue table_output_value(const AppContext *app, uint32_t row_index, uint32_t output_index) {
    uint32_t cols;

    assert(app->analysis.truth_table != NULL);
    cols = (uint32_t)app->analysis.truth_table->input_count + (uint32_t)app->analysis.truth_table->output_count;
    return app->analysis.truth_table->data[(row_index * cols) + (uint32_t)app->analysis.truth_table->input_count + output_index];
}

static const LogicNet *find_incoming_net_for_sink(const AppContext *app, const LogicPin *sink_pin) {
    uint32_t net_index;

    for (net_index = 0U; net_index < app->graph.net_count; net_index++) {
        uint8_t sink_index;

        for (sink_index = 0U; sink_index < app->graph.nets[net_index].sink_count; sink_index++) {
            if (app->graph.nets[net_index].sinks[sink_index] == sink_pin) {
                return &app->graph.nets[net_index];
            }
        }
    }

    return NULL;
}

static bool node_is_snapped_to_grid(const LogicNode *node) {
    float snapped_x;
    float snapped_y;

    snapped_x = roundf(node->pos.x / 20.0f) * 20.0f;
    snapped_y = roundf(node->pos.y / 20.0f) * 20.0f;
    return fabsf(node->pos.x - snapped_x) < 0.001f && fabsf(node->pos.y - snapped_y) < 0.001f;
}

static bool rectangles_overlap(Rectangle left, Rectangle right) {
    return left.x < right.x + right.width &&
        left.x + left.width > right.x &&
        left.y < right.y + right.height &&
        left.y + left.height > right.y;
}

typedef struct {
    const LogicPin *source;
    const LogicPin *sink;
    Vector2 start;
    Vector2 mid_1;
    Vector2 mid_2;
    Vector2 end;
} OrthogonalWirePath;

static OrthogonalWirePath orthogonal_wire_path_for_sink(const AppContext *app, const LogicPin *sink_pin) {
    const LogicNet *incoming;
    OrthogonalWirePath path;

    incoming = find_incoming_net_for_sink(app, sink_pin);
    assert(incoming != NULL);
    assert(incoming->source != NULL);

    path.source = incoming->source;
    path.sink = sink_pin;
    path.start = ui_output_pin_position(incoming->source);
    path.end = ui_input_pin_position(sink_pin);
    path.mid_1 = (Vector2){ (path.start.x + path.end.x) * 0.5f, path.start.y };
    path.mid_2 = (Vector2){ (path.start.x + path.end.x) * 0.5f, path.end.y };
    return path;
}

static bool value_is_strictly_between(float value, float a, float b) {
    float min_value;
    float max_value;

    min_value = a < b ? a : b;
    max_value = a < b ? b : a;
    return value > min_value + 0.001f && value < max_value - 0.001f;
}

static bool orthogonal_paths_cross(OrthogonalWirePath left, OrthogonalWirePath right) {
    bool left_vertical_hits_right_first;
    bool left_vertical_hits_right_last;
    bool right_vertical_hits_left_first;
    bool right_vertical_hits_left_last;

    left_vertical_hits_right_first =
        value_is_strictly_between(left.mid_1.x, right.start.x, right.mid_1.x) &&
        value_is_strictly_between(right.start.y, left.mid_1.y, left.mid_2.y);
    left_vertical_hits_right_last =
        value_is_strictly_between(left.mid_1.x, right.mid_2.x, right.end.x) &&
        value_is_strictly_between(right.end.y, left.mid_1.y, left.mid_2.y);
    right_vertical_hits_left_first =
        value_is_strictly_between(right.mid_1.x, left.start.x, left.mid_1.x) &&
        value_is_strictly_between(left.start.y, right.mid_1.y, right.mid_2.y);
    right_vertical_hits_left_last =
        value_is_strictly_between(right.mid_1.x, left.mid_2.x, left.end.x) &&
        value_is_strictly_between(left.end.y, right.mid_1.y, right.mid_2.y);

    return left_vertical_hits_right_first ||
        left_vertical_hits_right_last ||
        right_vertical_hits_left_first ||
        right_vertical_hits_left_last;
}

static void assert_loaded_layout_is_readable(const AppContext *app) {
    uint32_t left_index;

    for (left_index = 0U; left_index < app->graph.node_count; left_index++) {
        const LogicNode *left_node;
        uint32_t right_index;

        left_node = &app->graph.nodes[left_index];
        if (left_node->type == (NodeType)-1) {
            continue;
        }

        assert(node_is_snapped_to_grid(left_node));
        for (right_index = left_index + 1U; right_index < app->graph.node_count; right_index++) {
            const LogicNode *right_node;

            right_node = &app->graph.nodes[right_index];
            if (right_node->type == (NodeType)-1) {
                continue;
            }

            assert(!rectangles_overlap(left_node->rect, right_node->rect));
        }
    }

    for (left_index = 0U; left_index < app->graph.net_count; left_index++) {
        const LogicNet *net;
        uint8_t sink_index;

        net = &app->graph.nets[left_index];
        assert(net->source != NULL);
        for (sink_index = 0U; sink_index < net->sink_count; sink_index++) {
            LogicPin *sink_pin;
            Vector2 start;
            Vector2 end;

            sink_pin = net->sinks[sink_index];
            assert(sink_pin != NULL);
            start = ui_output_pin_position(net->source);
            end = ui_input_pin_position(sink_pin);
            assert(start.x < end.x);
        }
    }

    for (left_index = 0U; left_index < app->graph.node_count; left_index++) {
        const LogicNode *node;

        node = &app->graph.nodes[left_index];
        if (node->type == (NodeType)-1) {
            continue;
        }

        if (node->type == NODE_INPUT || node->type == NODE_GATE_CLOCK) {
            uint32_t net_index;

            for (net_index = 0U; net_index < app->graph.net_count; net_index++) {
                const LogicNet *net;
                uint8_t sink_index;

                net = &app->graph.nets[net_index];
                if (!net->source || net->source->node != node) {
                    continue;
                }
                for (sink_index = 0U; sink_index < net->sink_count; sink_index++) {
                    assert(node->pos.x < net->sinks[sink_index]->node->pos.x);
                }
            }
        }

        if (node->type == NODE_OUTPUT) {
            const LogicNet *incoming;

            incoming = find_incoming_net_for_sink(app, &node->inputs[0]);
            assert(incoming != NULL);
            assert(incoming->source != NULL);
            assert(incoming->source->node->pos.x < node->pos.x);
        }
    }
}

static void assert_wire_paths_do_not_cross(const AppContext *app) {
    LogicPin *sink_pins[MAX_NETS * MAX_PINS];
    uint32_t sink_count;
    uint32_t net_index;
    uint32_t left_index;

    sink_count = 0U;
    for (net_index = 0U; net_index < app->graph.net_count; net_index++) {
        uint8_t sink_index;

        for (sink_index = 0U; sink_index < app->graph.nets[net_index].sink_count; sink_index++) {
            sink_pins[sink_count++] = app->graph.nets[net_index].sinks[sink_index];
        }
    }

    for (left_index = 0U; left_index < sink_count; left_index++) {
        OrthogonalWirePath left_path;
        uint32_t right_index;

        left_path = orthogonal_wire_path_for_sink(app, sink_pins[left_index]);
        for (right_index = left_index + 1U; right_index < sink_count; right_index++) {
            OrthogonalWirePath right_path;

            right_path = orthogonal_wire_path_for_sink(app, sink_pins[right_index]);
            if (left_path.source->node == right_path.source->node ||
                left_path.sink->node == right_path.sink->node) {
                continue;
            }
            assert(!orthogonal_paths_cross(left_path, right_path));
        }
    }
}

static void assert_fanout_mid_columns_share_a_trunk(const AppContext *app, LogicNode *source_node) {
    float mid_x[MAX_PINS];
    uint32_t mid_count;
    uint32_t net_index;

    assert(source_node != NULL);
    mid_count = 0U;
    for (net_index = 0U; net_index < app->graph.net_count; net_index++) {
        const LogicNet *net;
        uint8_t sink_index;

        net = &app->graph.nets[net_index];
        if (!net->source || net->source->node != source_node) {
            continue;
        }

        for (sink_index = 0U; sink_index < net->sink_count; sink_index++) {
            OrthogonalWirePath path;

            path = orthogonal_wire_path_for_sink(app, net->sinks[sink_index]);
            mid_x[mid_count++] = path.mid_1.x;
        }
    }

    if (mid_count > 1U) {
        uint32_t mid_index;

        for (mid_index = 1U; mid_index < mid_count; mid_index++) {
            assert(fabsf(mid_x[0] - mid_x[mid_index]) < 0.001f);
        }
    }
}

static Rectangle named_nodes_bounds(AppContext *app, const char * const *names, uint32_t name_count) {
    Rectangle bounds;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    uint32_t name_index;

    min_x = 0.0f;
    min_y = 0.0f;
    max_x = 0.0f;
    max_y = 0.0f;
    for (name_index = 0U; name_index < name_count; name_index++) {
        LogicNode *node;

        node = find_node_by_name(app, names[name_index]);
        assert(node != NULL);
        if (name_index == 0U) {
            min_x = node->rect.x;
            min_y = node->rect.y;
            max_x = node->rect.x + node->rect.width;
            max_y = node->rect.y + node->rect.height;
            continue;
        }

        if (node->rect.x < min_x) {
            min_x = node->rect.x;
        }
        if (node->rect.y < min_y) {
            min_y = node->rect.y;
        }
        if (node->rect.x + node->rect.width > max_x) {
            max_x = node->rect.x + node->rect.width;
        }
        if (node->rect.y + node->rect.height > max_y) {
            max_y = node->rect.y + node->rect.height;
        }
    }

    bounds = (Rectangle){ min_x, min_y, max_x - min_x, max_y - min_y };
    return bounds;
}

static void assert_named_node_positions_match(
    AppContext *left_app,
    AppContext *right_app,
    const char * const *names,
    uint32_t name_count
) {
    uint32_t name_index;

    for (name_index = 0U; name_index < name_count; name_index++) {
        LogicNode *left_node;
        LogicNode *right_node;

        left_node = find_node_by_name(left_app, names[name_index]);
        right_node = find_node_by_name(right_app, names[name_index]);
        assert(left_node != NULL);
        assert(right_node != NULL);
            assert(fabsf(left_node->pos.x - right_node->pos.x) < 0.001f);
            assert(fabsf(left_node->pos.y - right_node->pos.y) < 0.001f);
    }
}

static void test_app_default_names(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *z;
    LogicNode *z1;
    LogicNode *and_gate;
    LogicNode *clock;

    app_init(&app);
    a = app_add_node(&app, NODE_INPUT, (Vector2){ 100.0f, 100.0f });
    b = app_add_node(&app, NODE_INPUT, (Vector2){ 100.0f, 160.0f });
    z = app_add_node(&app, NODE_OUTPUT, (Vector2){ 220.0f, 100.0f });
    z1 = app_add_node(&app, NODE_OUTPUT, (Vector2){ 220.0f, 160.0f });
    and_gate = app_add_node(&app, NODE_GATE_AND, (Vector2){ 160.0f, 130.0f });
    clock = app_add_node(&app, NODE_GATE_CLOCK, (Vector2){ 160.0f, 200.0f });

    assert(strcmp(a->name, "A") == 0);
    assert(strcmp(b->name, "B") == 0);
    assert(strcmp(z->name, "Z") == 0);
    assert(strcmp(z1->name, "Z1") == 0);
    assert(strcmp(and_gate->name, "AND1") == 0);
    assert(strcmp(clock->name, "CLK1") == 0);

    app_clear_graph(&app);
    printf("test_app_default_names passed!\n");
}

static void test_interactive_construction_flow(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *out;
    LogicNode *and_gate;
    char *explanation;

    app_init(&app);
    a = app_add_node(&app, NODE_INPUT, (Vector2){ 140.0f, 160.0f });
    out = app_add_node(&app, NODE_OUTPUT, (Vector2){ 520.0f, 220.0f });
    assert(app_connect_pins(&app, &a->outputs[0], &out->inputs[0]));

    b = app_add_node(&app, NODE_INPUT, (Vector2){ 140.0f, 260.0f });
    and_gate = app_add_node(&app, NODE_GATE_AND, (Vector2){ 320.0f, 220.0f });
    assert(app_connect_pins(&app, &a->outputs[0], &and_gate->inputs[0]));
    assert(app_connect_pins(&app, &b->outputs[0], &and_gate->inputs[1]));
    assert(app_connect_pins(&app, &and_gate->outputs[0], &out->inputs[0]));

    assert(strcmp(app.analysis.expression, "(A AND B)") == 0);
    assert(table_output_value(&app, 0U, 0U) == LOGIC_LOW);
    assert(table_output_value(&app, 1U, 0U) == LOGIC_LOW);
    assert(table_output_value(&app, 2U, 0U) == LOGIC_LOW);
    assert(table_output_value(&app, 3U, 0U) == LOGIC_HIGH);

    app_select_row(&app, 2U);
    app_apply_selected_row_to_inputs(&app);
    assert(and_gate->outputs[0].value == LOGIC_LOW);
    assert(out->inputs[0].value == LOGIC_LOW);
    assert(app.selection.view.row_valid);
    assert(app.selection.view.live_row_index == 2U);
    explanation = app_get_node_explanation(&app, and_gate);
    assert(explanation != NULL);
    assert(strstr(explanation, "at least one input is 0") != NULL);
    free(explanation);

    app_select_row(&app, 3U);
    app_apply_selected_row_to_inputs(&app);
    assert(and_gate->outputs[0].value == LOGIC_HIGH);
    assert(out->inputs[0].value == LOGIC_HIGH);
    assert(app.selection.view.row_valid);
    assert(app.selection.view.live_row_index == 3U);

    assert(app_toggle_input_value(&app, a));
    assert(a->outputs[0].value == LOGIC_LOW);
    assert(and_gate->outputs[0].value == LOGIC_LOW);
    assert(app.selection.view.row_valid);
    assert(app.selection.view.live_row_index == 1U);

    app_clear_graph(&app);
    printf("test_interactive_construction_flow passed!\n");
}

static void test_compare_mode_without_target(void) {
    AppContext app;

    app_init(&app);
    assert(app_add_node(&app, NODE_INPUT, (Vector2){ 100.0f, 100.0f }) != NULL);
    assert(app_add_node(&app, NODE_OUTPUT, (Vector2){ 220.0f, 100.0f }) != NULL);
    assert(app_connect_pins(&app, &app.graph.nodes[0].outputs[0], &app.graph.nodes[1].inputs[0]));

    app_set_mode(&app, MODE_COMPARE);
    assert(app.comparison.status == APP_COMPARE_NO_TARGET);
    assert(!app.comparison.equivalent);

    app_clear_graph(&app);
    printf("test_compare_mode_without_target passed!\n");
}

static void test_circuit_file_load(void) {
    AppContext app;
    char temp_path[] = "/tmp/mlvd-test-XXXXXX";
    int fd;
    bool loaded;
    char error_message[128];
    LogicNode *output;

    fd = mkstemp(temp_path);
    assert(fd >= 0);
    close(fd);

    write_text_file(
        temp_path,
        "input A at 140,160\n"
        "input B at 140,260\n"
        "and G1 at 300,220\n"
        "output Z at 500,240\n"
        "wire A -> G1.in0\n"
        "wire B -> G1.in1\n"
        "wire G1.out0 -> Z.in0\n"
    );

    app_init(&app);
    loaded = circuit_file_load(&app, temp_path, error_message, sizeof(error_message));
    assert(loaded);
    assert(app.graph.node_count == 4U);
    assert(app.analysis.truth_table != NULL);
    assert(app.analysis.truth_table->row_count == 4U);

    output = find_node_by_name(&app, "Z");
    assert(output != NULL);
    assert(strcmp(output->name, "Z") == 0);
    assert(strcmp(app.analysis.expression, "(A AND B)") == 0);
    assert_loaded_layout_is_readable(&app);
    assert_wire_paths_do_not_cross(&app);

    unlink(temp_path);
    app_clear_graph(&app);
    printf("test_circuit_file_load passed!\n");
}

static void test_circuit_file_load_ignores_explicit_positions(void) {
    AppContext first_app;
    AppContext second_app;
    char first_path[] = "/tmp/mlvd-test-layout-a-XXXXXX";
    char second_path[] = "/tmp/mlvd-test-layout-b-XXXXXX";
    const char *names[] = { "A", "G1", "Z" };
    int fd;
    char error_message[128];

    fd = mkstemp(first_path);
    assert(fd >= 0);
    close(fd);
    fd = mkstemp(second_path);
    assert(fd >= 0);
    close(fd);

    write_text_file(
        first_path,
        "input A at 143,213\n"
        "and G1 at 349,227\n"
        "output Z at 517,241\n"
        "wire A -> G1.in0\n"
        "wire G1.out0 -> Z.in0\n"
    );
    write_text_file(
        second_path,
        "input A at 860,520\n"
        "and G1 at 120,80\n"
        "output Z at 40,940\n"
        "wire A -> G1.in0\n"
        "wire G1.out0 -> Z.in0\n"
    );

    app_init(&first_app);
    assert(circuit_file_load(&first_app, first_path, error_message, sizeof(error_message)));
    app_init(&second_app);
    assert(circuit_file_load(&second_app, second_path, error_message, sizeof(error_message)));

    assert_named_node_positions_match(&first_app, &second_app, names, 3U);
    assert_loaded_layout_is_readable(&first_app);
    assert_loaded_layout_is_readable(&second_app);

    unlink(first_path);
    unlink(second_path);
    app_clear_graph(&first_app);
    app_clear_graph(&second_app);
    printf("test_circuit_file_load_ignores_explicit_positions passed!\n");
}

static void test_circuit_file_load_failure_keeps_existing_graph(void) {
    AppContext app;
    char temp_path[] = "/tmp/mlvd-test-invalid-XXXXXX";
    int fd;
    bool loaded;
    char error_message[128];

    fd = mkstemp(temp_path);
    assert(fd >= 0);
    close(fd);

    write_text_file(
        temp_path,
        "input A\n"
        "output Z\n"
        "wire Missing -> Z.in0\n"
    );

    app_init(&app);
    assert(app_add_named_node(&app, NODE_INPUT, "ExistingIn", (Vector2){ 100.0f, 100.0f }) != NULL);
    assert(app_add_named_node(&app, NODE_OUTPUT, "ExistingOut", (Vector2){ 200.0f, 100.0f }) != NULL);
    assert(logic_connect(&app.graph, &app.graph.nodes[0].outputs[0], &app.graph.nodes[1].inputs[0]));
    app_update_logic(&app);

    loaded = circuit_file_load(&app, temp_path, error_message, sizeof(error_message));
    assert(!loaded);
    assert(strstr(error_message, "unknown node") != NULL);
    assert(app.graph.node_count == 2U);
    assert(strcmp(app.graph.nodes[0].name, "ExistingIn") == 0);

    unlink(temp_path);
    app_clear_graph(&app);
    printf("test_circuit_file_load_failure_keeps_existing_graph passed!\n");
}

static void test_example_circuits_load(void) {
    AppContext app;
    LogicNode *b;
    LogicNode *gate;
    LogicNode *xor_gate;
    LogicNode *output;
    LogicNode *carry;
    char error_message[128];
    char *expression;
    Vector2 moved_pin_pos;

    app_init(&app);
    assert(circuit_file_load(&app, "examples/and_gate.circ", error_message, sizeof(error_message)));
    assert(app.graph.node_count == 4U);
    assert(app.graph.net_count == 3U);
    assert_loaded_layout_is_readable(&app);
    assert_wire_paths_do_not_cross(&app);
    gate = find_node_by_name(&app, "G1");
    assert(gate != NULL);
    output = find_node_by_name(&app, "Z");
    assert(output != NULL);
    assert(gate->pos.x < output->pos.x);
    expression = logic_generate_expression(&app.graph, output);
    assert(expression != NULL);
    assert(strcmp(expression, "(A AND B)") == 0);
    free(expression);

    b = find_node_by_name(&app, "B");
    assert(b != NULL);
    app.selection.selected_node = b;
    assert(app_move_selected_node(&app, 0, 1));
    assert(app.graph.nets[1].source == &b->outputs[0]);
    moved_pin_pos = ui_output_pin_position(&b->outputs[0]);
    assert(fabsf(moved_pin_pos.x - (b->pos.x + b->rect.width)) < 0.001f);
    assert(fabsf(moved_pin_pos.y - (b->pos.y + app_node_pin_offset_y(b, true, 0U))) < 0.001f);
    app_clear_graph(&app);

    app_init(&app);
    assert(circuit_file_load(&app, "examples/half_adder.circ", error_message, sizeof(error_message)));
    assert(app.graph.node_count == 6U);
    assert(app.graph.net_count == 4U);
    assert(app.analysis.truth_table != NULL);
    assert(app.analysis.truth_table->row_count == 4U);
    assert_loaded_layout_is_readable(&app);
    assert_wire_paths_do_not_cross(&app);
    assert_fanout_mid_columns_share_a_trunk(&app, find_node_by_name(&app, "A"));
    assert_fanout_mid_columns_share_a_trunk(&app, find_node_by_name(&app, "B"));
    xor_gate = find_node_by_name(&app, "XOR1");
    gate = find_node_by_name(&app, "AND1");
    assert(xor_gate != NULL);
    assert(gate != NULL);
    output = find_node_by_name(&app, "SUM");
    assert(output != NULL);
    carry = find_node_by_name(&app, "CARRY");
    assert(carry != NULL);
    assert(xor_gate->pos.y < gate->pos.y);
    assert(output->pos.y < carry->pos.y);
    assert(fabsf(
        (xor_gate->rect.y + (xor_gate->rect.height * 0.5f)) -
        (gate->rect.y + (gate->rect.height * 0.5f))
    ) >= 120.0f);
    assert(fabsf(
        (output->rect.y + (output->rect.height * 0.5f)) -
        (carry->rect.y + (carry->rect.height * 0.5f))
    ) >= 100.0f);
    assert(output->pos.x > xor_gate->pos.x);
    assert(carry->pos.x > gate->pos.x);
    assert(table_output_value(&app, 0U, 0U) == LOGIC_LOW);
    assert(table_output_value(&app, 0U, 1U) == LOGIC_LOW);
    assert(table_output_value(&app, 1U, 0U) == LOGIC_HIGH);
    assert(table_output_value(&app, 1U, 1U) == LOGIC_LOW);
    assert(table_output_value(&app, 2U, 0U) == LOGIC_HIGH);
    assert(table_output_value(&app, 2U, 1U) == LOGIC_LOW);
    assert(table_output_value(&app, 3U, 0U) == LOGIC_LOW);
    assert(table_output_value(&app, 3U, 1U) == LOGIC_HIGH);

    output = find_node_by_name(&app, "SUM");
    assert(output != NULL);
    expression = logic_generate_expression(&app.graph, output);
    assert(expression != NULL);
    assert(strcmp(expression, "(A XOR B)") == 0);
    free(expression);

    expression = logic_generate_expression(&app.graph, carry);
    assert(expression != NULL);
    assert(strcmp(expression, "(A AND B)") == 0);
    free(expression);

    app_clear_graph(&app);
    printf("test_example_circuits_load passed!\n");
}

static void test_circuit_file_load_uses_declaration_order_for_symmetric_layers(void) {
    AppContext sum_first_app;
    AppContext carry_first_app;
    char sum_first_path[] = "/tmp/mlvd-test-order-a-XXXXXX";
    char carry_first_path[] = "/tmp/mlvd-test-order-b-XXXXXX";
    int fd;
    char error_message[128];
    LogicNode *sum_first_xor;
    LogicNode *sum_first_and;
    LogicNode *sum_first_sum;
    LogicNode *sum_first_carry;
    LogicNode *carry_first_xor;
    LogicNode *carry_first_and;
    LogicNode *carry_first_sum;
    LogicNode *carry_first_carry;

    fd = mkstemp(sum_first_path);
    assert(fd >= 0);
    close(fd);
    fd = mkstemp(carry_first_path);
    assert(fd >= 0);
    close(fd);

    write_text_file(
        sum_first_path,
        "input A\n"
        "input B\n"
        "xor XOR1\n"
        "and AND1\n"
        "output SUM\n"
        "output CARRY\n"
        "wire A -> XOR1.in0\n"
        "wire B -> XOR1.in1\n"
        "wire A -> AND1.in0\n"
        "wire B -> AND1.in1\n"
        "wire XOR1.out0 -> SUM.in0\n"
        "wire AND1.out0 -> CARRY.in0\n"
    );
    write_text_file(
        carry_first_path,
        "input A\n"
        "input B\n"
        "and AND1\n"
        "xor XOR1\n"
        "output CARRY\n"
        "output SUM\n"
        "wire A -> XOR1.in0\n"
        "wire B -> AND1.in1\n"
        "wire A -> AND1.in0\n"
        "wire B -> XOR1.in1\n"
        "wire AND1.out0 -> CARRY.in0\n"
        "wire XOR1.out0 -> SUM.in0\n"
    );

    app_init(&sum_first_app);
    assert(circuit_file_load(&sum_first_app, sum_first_path, error_message, sizeof(error_message)));
    app_init(&carry_first_app);
    assert(circuit_file_load(&carry_first_app, carry_first_path, error_message, sizeof(error_message)));

    sum_first_xor = find_node_by_name(&sum_first_app, "XOR1");
    sum_first_and = find_node_by_name(&sum_first_app, "AND1");
    sum_first_sum = find_node_by_name(&sum_first_app, "SUM");
    sum_first_carry = find_node_by_name(&sum_first_app, "CARRY");
    carry_first_xor = find_node_by_name(&carry_first_app, "XOR1");
    carry_first_and = find_node_by_name(&carry_first_app, "AND1");
    carry_first_sum = find_node_by_name(&carry_first_app, "SUM");
    carry_first_carry = find_node_by_name(&carry_first_app, "CARRY");

    assert(sum_first_xor != NULL);
    assert(sum_first_and != NULL);
    assert(sum_first_sum != NULL);
    assert(sum_first_carry != NULL);
    assert(carry_first_xor != NULL);
    assert(carry_first_and != NULL);
    assert(carry_first_sum != NULL);
    assert(carry_first_carry != NULL);

    assert(sum_first_xor->pos.y < sum_first_and->pos.y);
    assert(sum_first_sum->pos.y < sum_first_carry->pos.y);
    assert(carry_first_and->pos.y < carry_first_xor->pos.y);
    assert(carry_first_carry->pos.y < carry_first_sum->pos.y);
    assert_loaded_layout_is_readable(&sum_first_app);
    assert_loaded_layout_is_readable(&carry_first_app);
    assert_wire_paths_do_not_cross(&sum_first_app);
    assert_wire_paths_do_not_cross(&carry_first_app);

    unlink(sum_first_path);
    unlink(carry_first_path);
    app_clear_graph(&sum_first_app);
    app_clear_graph(&carry_first_app);
    printf("test_circuit_file_load_uses_declaration_order_for_symmetric_layers passed!\n");
}

static void test_circuit_file_load_packs_disconnected_components(void) {
    AppContext app;
    char temp_path[] = "/tmp/mlvd-test-components-XXXXXX";
    const char *left_component[] = { "A", "B", "G1", "Z" };
    const char *right_component[] = { "C", "D", "G2", "Y" };
    Rectangle left_bounds;
    Rectangle right_bounds;
    int fd;
    char error_message[128];

    fd = mkstemp(temp_path);
    assert(fd >= 0);
    close(fd);

    write_text_file(
        temp_path,
        "input A\n"
        "input B\n"
        "and G1\n"
        "output Z\n"
        "input C\n"
        "input D\n"
        "xor G2\n"
        "output Y\n"
        "wire A -> G1.in0\n"
        "wire B -> G1.in1\n"
        "wire G1.out0 -> Z.in0\n"
        "wire C -> G2.in0\n"
        "wire D -> G2.in1\n"
        "wire G2.out0 -> Y.in0\n"
    );

    app_init(&app);
    assert(circuit_file_load(&app, temp_path, error_message, sizeof(error_message)));
    assert_loaded_layout_is_readable(&app);
    assert_wire_paths_do_not_cross(&app);

    left_bounds = named_nodes_bounds(&app, left_component, 4U);
    right_bounds = named_nodes_bounds(&app, right_component, 4U);
    assert(left_bounds.y + left_bounds.height + 40.0f <= right_bounds.y ||
        right_bounds.y + right_bounds.height + 40.0f <= left_bounds.y);

    unlink(temp_path);
    app_clear_graph(&app);
    printf("test_circuit_file_load_packs_disconnected_components passed!\n");
}

static void test_circuit_file_load_handles_feedback_cycles(void) {
    AppContext app;
    char temp_path[] = "/tmp/mlvd-test-cycle-XXXXXX";
    int fd;
    char error_message[128];
    LogicNode *left;
    LogicNode *right;

    fd = mkstemp(temp_path);
    assert(fd >= 0);
    close(fd);

    write_text_file(
        temp_path,
        "not N1\n"
        "not N2\n"
        "wire N1.out0 -> N2.in0\n"
        "wire N2.out0 -> N1.in0\n"
    );

    app_init(&app);
    assert(circuit_file_load(&app, temp_path, error_message, sizeof(error_message)));
    left = find_node_by_name(&app, "N1");
    right = find_node_by_name(&app, "N2");
    assert(left != NULL);
    assert(right != NULL);
    assert(node_is_snapped_to_grid(left));
    assert(node_is_snapped_to_grid(right));
    assert(!rectangles_overlap(left->rect, right->rect));
    assert(fabsf(left->pos.x - right->pos.x) >= 0.001f || fabsf(left->pos.y - right->pos.y) >= 0.001f);

    unlink(temp_path);
    app_clear_graph(&app);
    printf("test_circuit_file_load_handles_feedback_cycles passed!\n");
}

static void test_connected_nodes_can_snap_to_straight_wire_alignment(void) {
    AppContext app;
    LogicNode *gate;
    LogicNode *output;
    Vector2 snapped;
    Vector2 gate_pin;
    Vector2 output_pin;

    app_init(&app);
    gate = app_add_named_node(&app, NODE_GATE_AND, "G1", (Vector2){ 340.0f, 220.0f });
    output = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 560.0f, 240.0f });

    assert(gate != NULL);
    assert(output != NULL);
    assert(app_connect_pins(&app, &gate->outputs[0], &output->inputs[0]));

    snapped = app_snap_live_node_position(&app, output, (Vector2){ 560.0f, 236.0f });
    assert(snapped.x == 560.0f);
    assert(snapped.y == 240.0f);

    output->pos = snapped;
    output->rect.x = snapped.x;
    output->rect.y = snapped.y;
    gate_pin = ui_output_pin_position(&gate->outputs[0]);
    output_pin = ui_input_pin_position(&output->inputs[0]);
    assert(fabsf(gate_pin.y - output_pin.y) < 0.001f);

    app_clear_graph(&app);
    printf("test_connected_nodes_can_snap_to_straight_wire_alignment passed!\n");
}

static void test_multi_input_gate_can_snap_to_connected_inputs_centerline(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *gate;
    Vector2 snapped;
    Vector2 a_pin;
    Vector2 b_pin;
    Vector2 gate_in_0;
    Vector2 gate_in_1;
    float source_midpoint;
    float gate_midpoint;

    app_init(&app);
    a = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 180.0f, 200.0f });
    b = app_add_named_node(&app, NODE_INPUT, "B", (Vector2){ 180.0f, 300.0f });
    gate = app_add_named_node(&app, NODE_GATE_AND, "G1", (Vector2){ 440.0f, 220.0f });

    assert(a != NULL);
    assert(b != NULL);
    assert(gate != NULL);
    assert(app_connect_pins(&app, &a->outputs[0], &gate->inputs[0]));
    assert(app_connect_pins(&app, &b->outputs[0], &gate->inputs[1]));

    snapped = app_snap_live_node_position(&app, gate, (Vector2){ 440.0f, 232.0f });
    assert(snapped.x == 440.0f);
    assert(snapped.y == 230.0f);

    gate->pos = snapped;
    gate->rect.x = snapped.x;
    gate->rect.y = snapped.y;

    a_pin = ui_output_pin_position(&a->outputs[0]);
    b_pin = ui_output_pin_position(&b->outputs[0]);
    gate_in_0 = ui_input_pin_position(&gate->inputs[0]);
    gate_in_1 = ui_input_pin_position(&gate->inputs[1]);

    source_midpoint = (a_pin.y + b_pin.y) * 0.5f;
    gate_midpoint = (gate_in_0.y + gate_in_1.y) * 0.5f;
    assert(fabsf(gate_midpoint - source_midpoint) < 0.001f);

    app_clear_graph(&app);
    printf("test_multi_input_gate_can_snap_to_connected_inputs_centerline passed!\n");
}

static void test_view_context_matches_live_state(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *out;
    LogicNode *and_gate;

    app_init(&app);
    a = app_add_node(&app, NODE_INPUT, (Vector2){ 140.0f, 160.0f });
    b = app_add_node(&app, NODE_INPUT, (Vector2){ 140.0f, 260.0f });
    and_gate = app_add_node(&app, NODE_GATE_AND, (Vector2){ 320.0f, 220.0f });
    out = app_add_node(&app, NODE_OUTPUT, (Vector2){ 520.0f, 220.0f });
    assert(app_connect_pins(&app, &a->outputs[0], &and_gate->inputs[0]));
    assert(app_connect_pins(&app, &b->outputs[0], &and_gate->inputs[1]));
    assert(app_connect_pins(&app, &and_gate->outputs[0], &out->inputs[0]));

    a->outputs[0].value = LOGIC_HIGH;
    b->outputs[0].value = LOGIC_HIGH;
    logic_evaluate(&app.graph);
    app_compute_view_context(&app);

    assert(app.selection.view.row_valid);
    assert(app.selection.view.live_row_index == 3U);

    app.selection.selected_row = 0U;
    app_compute_view_context(&app);
    assert(app.selection.view.live_row_index == 3U);
    assert(a->outputs[0].value == LOGIC_HIGH);
    assert(b->outputs[0].value == LOGIC_HIGH);

    app.selection.selected_node = and_gate;
    app_compute_view_context(&app);
    assert(app.selection.view.output_valid);
    assert(app.selection.view.live_output == LOGIC_HIGH);

    app_clear_graph(&app);
    printf("test_view_context_matches_live_state passed!\n");
}

static void test_equation_resolved(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *and_gate;
    LogicNode *out;
    char buf[256];

    app_init(&app);
    a = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    b = app_add_named_node(&app, NODE_INPUT, "B", (Vector2){ 140.0f, 260.0f });
    and_gate = app_add_named_node(&app, NODE_GATE_AND, "AND1", (Vector2){ 320.0f, 220.0f });
    out = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });
    assert(app_connect_pins(&app, &a->outputs[0], &and_gate->inputs[0]));
    assert(app_connect_pins(&app, &b->outputs[0], &and_gate->inputs[1]));
    assert(app_connect_pins(&app, &and_gate->outputs[0], &out->inputs[0]));

    a->outputs[0].value = LOGIC_HIGH;
    b->outputs[0].value = LOGIC_HIGH;
    logic_evaluate(&app.graph);

    assert(logic_format_equation_resolved(&app.graph, out, buf, sizeof(buf)));
    assert(strcmp(buf, "Z = (A AND B) = (1 AND 1) = 1") == 0);

    assert(logic_format_equation_resolved(&app.graph, and_gate, buf, sizeof(buf)));
    assert(strcmp(buf, "AND1 = (A AND B) = (1 AND 1) = 1") == 0);

    a->outputs[0].value = LOGIC_LOW;
    logic_evaluate(&app.graph);
    assert(logic_format_equation_resolved(&app.graph, out, buf, sizeof(buf)));
    assert(strcmp(buf, "Z = (A AND B) = (0 AND 1) = 0") == 0);

    app_clear_graph(&app);
    printf("test_equation_resolved passed!\n");
}

static void test_equation_symbolic_and_values(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *and_gate;
    LogicNode *out;
    char buf[256];

    app_init(&app);
    a = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    b = app_add_named_node(&app, NODE_INPUT, "B", (Vector2){ 140.0f, 260.0f });
    and_gate = app_add_named_node(&app, NODE_GATE_AND, "AND1", (Vector2){ 320.0f, 220.0f });
    out = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });
    assert(app_connect_pins(&app, &a->outputs[0], &and_gate->inputs[0]));
    assert(app_connect_pins(&app, &b->outputs[0], &and_gate->inputs[1]));
    assert(app_connect_pins(&app, &and_gate->outputs[0], &out->inputs[0]));

    a->outputs[0].value = LOGIC_HIGH;
    b->outputs[0].value = LOGIC_HIGH;
    logic_evaluate(&app.graph);

    assert(logic_format_equation_symbolic(&app.graph, out, buf, sizeof(buf)));
    assert(strcmp(buf, "Z = (A AND B)") == 0);

    assert(logic_format_equation_values(&app.graph, out, buf, sizeof(buf)));
    assert(strcmp(buf, "(1 AND 1) -> 1") == 0);

    assert(logic_format_equation_symbolic(&app.graph, and_gate, buf, sizeof(buf)));
    assert(strcmp(buf, "AND1 = (A AND B)") == 0);

    assert(logic_format_equation_values(&app.graph, and_gate, buf, sizeof(buf)));
    assert(strcmp(buf, "(1 AND 1) -> 1") == 0);

    app_clear_graph(&app);
    printf("test_equation_symbolic_and_values passed!\n");
}

static void test_equation_values_with_unknown_inputs(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *and_gate;
    LogicNode *out;
    char buf[256];

    app_init(&app);
    a = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    and_gate = app_add_named_node(&app, NODE_GATE_AND, "AND1", (Vector2){ 320.0f, 220.0f });
    out = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });
    assert(app_connect_pins(&app, &a->outputs[0], &and_gate->inputs[0]));
    assert(app_connect_pins(&app, &and_gate->outputs[0], &out->inputs[0]));

    a->outputs[0].value = LOGIC_HIGH;
    logic_evaluate(&app.graph);

    assert(logic_format_equation_symbolic(&app.graph, out, buf, sizeof(buf)));
    assert(strcmp(buf, "Z = (A AND ?)") == 0);

    assert(logic_format_equation_values(&app.graph, out, buf, sizeof(buf)));
    assert(strcmp(buf, "(1 AND ?) -> ?") == 0);

    app_clear_graph(&app);
    printf("test_equation_values_with_unknown_inputs passed!\n");
}

static void test_text_fit_with_ellipsis_truncates(void) {
    char fitted[64];
    float max_width;

    max_width = text_width("ALPHA...", 13);
    assert(text_fit_with_ellipsis("ALPHABET SOUP", 13, max_width, fitted, sizeof(fitted)));
    assert(strcmp(fitted, "ALPHA...") == 0);
    assert(text_width(fitted, 13) <= max_width);

    printf("test_text_fit_with_ellipsis_truncates passed!\n");
}

static void test_text_wrap_with_ellipsis_respects_max_lines(void) {
    WrappedTextLayout layout;
    char wrapped[128];
    float max_width;
    float height;

    max_width = text_width("ALPHA BETA", 12) + 1.0f;
    layout = text_wrap_with_ellipsis("ALPHA BETA GAMMA", 12, max_width, 2U, wrapped, sizeof(wrapped));
    assert(layout.line_count == 2U);
    assert(!layout.truncated);
    assert(strcmp(wrapped, "ALPHA BETA\nGAMMA") == 0);

    layout = text_wrap_with_ellipsis("ALPHA BETA GAMMA", 12, max_width, 1U, wrapped, sizeof(wrapped));
    assert(layout.line_count == 1U);
    assert(layout.truncated);
    assert(strstr(wrapped, "...") != NULL);
    assert(text_width(wrapped, 12) <= max_width);

    height = text_wrapped_height("ALPHA BETA GAMMA", 12, max_width, 3.0f, 2U);
    assert(fabsf(height - 27.0f) < 0.01f);

    printf("test_text_wrap_with_ellipsis_respects_max_lines passed!\n");
}

static void test_snap_node_position_keeps_single_pin_nodes_on_grid(void) {
    Vector2 snapped;

    snapped = app_snap_node_position((Vector2){ 143.0f, 213.0f }, NODE_OUTPUT);
    assert(snapped.x == 140.0f);
    assert(snapped.y == 200.0f);

    printf("test_snap_node_position_keeps_single_pin_nodes_on_grid passed!\n");
}

static void test_snap_node_position_centers_tall_gates(void) {
    Vector2 gate_snapped;

    gate_snapped = app_snap_node_position((Vector2){ 143.0f, 213.0f }, NODE_GATE_AND);

    assert(gate_snapped.x == 140.0f);
    assert(gate_snapped.y == 200.0f);
    assert(((int)gate_snapped.y % 20) == 0);

    {
        LogicNode gate;
        Vector2 in0;
        Vector2 in1;

        memset(&gate, 0, sizeof(gate));
        gate.pos = gate_snapped;
        gate.rect = (Rectangle){ gate_snapped.x, gate_snapped.y, 80.0f, 80.0f };
        gate.input_count = 2;
        gate.output_count = 1;
        gate.inputs[0].node = &gate;
        gate.inputs[0].index = 0;
        gate.inputs[1].node = &gate;
        gate.inputs[1].index = 1;

        in0 = ui_input_pin_position(&gate.inputs[0]);
        in1 = ui_input_pin_position(&gate.inputs[1]);
        assert(in0.y == 220.0f);
        assert(in1.y == 260.0f);
        assert(((int)in0.y % 20) == 0);
        assert(((int)in1.y % 20) == 0);
    }

    printf("test_snap_node_position_centers_tall_gates passed!\n");
}

static void test_delete_selected_wire(void) {
    AppContext app;
    LogicNode *input;
    LogicNode *output;

    app_init(&app);
    input = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    output = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });

    assert(input != NULL);
    assert(output != NULL);
    assert(app_connect_pins(&app, &input->outputs[0], &output->inputs[0]));
    assert(app.graph.net_count == 1U);

    app_select_wire_by_sink(&app, &output->inputs[0]);
    assert(app.selection.selected_wire_sink == &output->inputs[0]);
    assert(app.selection.selected_node == NULL);
    assert(app_delete_selected_wire(&app));
    assert(app.selection.selected_wire_sink == NULL);
    assert(app.graph.net_count == 0U);
    assert(!app_delete_selected_wire(&app));

    app_clear_graph(&app);
    printf("test_delete_selected_wire passed!\n");
}

static void test_ui_get_wire_at_uses_rendered_wire_path(void) {
    AppContext app;
    Rectangle canvas;
    LogicNode *input;
    LogicNode *output;
    LogicPin *hit_wire;

    app_init(&app);
    canvas = (Rectangle){ 132.0f, 50.0f, 720.0f, 480.0f };
    input = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    output = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });

    assert(input != NULL);
    assert(output != NULL);
    assert(app_connect_pins(&app, &input->outputs[0], &output->inputs[0]));

    hit_wire = ui_get_wire_at(
        &app,
        canvas,
        app_canvas_world_to_screen(&app, canvas, (Vector2){ 280.0f, 180.0f })
    );
    assert(hit_wire == &output->inputs[0]);

    app_clear_graph(&app);
    printf("test_ui_get_wire_at_uses_rendered_wire_path passed!\n");
}

static void assert_rect_inside(Rectangle inner, Rectangle outer) {
    assert(inner.x >= outer.x);
    assert(inner.y >= outer.y);
    assert(inner.width >= 0.0f);
    assert(inner.height >= 0.0f);
    assert(inner.x + inner.width <= outer.x + outer.width);
    assert(inner.y + inner.height <= outer.y + outer.height);
}

static void assert_float_close(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.001f);
}

static void test_canvas_coordinate_transform_round_trip(void) {
    Rectangle canvas;
    Vector2 origin;
    Vector2 screen;
    Vector2 world;
    Vector2 round_trip;

    canvas = (Rectangle){ 132.0f, 50.0f, 720.0f, 480.0f };
    origin = (Vector2){ 96.0f, 84.0f };
    screen = (Vector2){ 404.0f, 278.0f };
    world = app_canvas_screen_to_world_at(origin, 1.75f, canvas, screen);
    round_trip = app_canvas_world_to_screen_at(origin, 1.75f, canvas, world);

    assert_float_close(round_trip.x, screen.x);
    assert_float_close(round_trip.y, screen.y);
    printf("test_canvas_coordinate_transform_round_trip passed!\n");
}

static void test_canvas_zoom_anchor_keeps_world_point_stable(void) {
    Rectangle canvas;
    Vector2 origin;
    Vector2 anchor;
    Vector2 anchor_world;
    Vector2 zoomed_origin;
    Vector2 anchor_world_after_zoom;

    canvas = (Rectangle){ 132.0f, 50.0f, 720.0f, 480.0f };
    origin = (Vector2){ 40.0f, 30.0f };
    anchor = (Vector2){ 392.0f, 266.0f };
    anchor_world = app_canvas_screen_to_world_at(origin, 1.0f, canvas, anchor);
    zoomed_origin = app_canvas_origin_after_zoom(origin, 1.0f, 2.1f, canvas, anchor);
    anchor_world_after_zoom = app_canvas_screen_to_world_at(zoomed_origin, 2.1f, canvas, anchor);

    assert_float_close(anchor_world_after_zoom.x, anchor_world.x);
    assert_float_close(anchor_world_after_zoom.y, anchor_world.y);
    printf("test_canvas_zoom_anchor_keeps_world_point_stable passed!\n");
}

static void test_frame_graph_in_canvas_centers_loaded_circuit(void) {
    AppContext app;
    Rectangle canvas;
    Vector2 graph_center;
    Vector2 screen_center;
    char error_message[128];
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    uint32_t node_index;
    bool found_node;

    app_init(&app);
    assert(circuit_file_load(&app, "examples/and_gate.circ", error_message, sizeof(error_message)));

    canvas = (Rectangle){ 132.0f, 50.0f, 720.0f, 480.0f };
    assert(app_frame_graph_in_canvas(&app, canvas));

    found_node = false;
    min_x = 0.0f;
    min_y = 0.0f;
    max_x = 0.0f;
    max_y = 0.0f;

    for (node_index = 0U; node_index < app.graph.node_count; node_index++) {
        LogicNode *node;

        node = &app.graph.nodes[node_index];
        if (node->type == (NodeType)-1) {
            continue;
        }

        if (!found_node) {
            min_x = node->rect.x;
            min_y = node->rect.y;
            max_x = node->rect.x + node->rect.width;
            max_y = node->rect.y + node->rect.height;
            found_node = true;
            continue;
        }

        if (node->rect.x < min_x) {
            min_x = node->rect.x;
        }
        if (node->rect.y < min_y) {
            min_y = node->rect.y;
        }
        if (node->rect.x + node->rect.width > max_x) {
            max_x = node->rect.x + node->rect.width;
        }
        if (node->rect.y + node->rect.height > max_y) {
            max_y = node->rect.y + node->rect.height;
        }
    }

    assert(found_node);

    graph_center = (Vector2){
        (min_x + max_x) * 0.5f,
        (min_y + max_y) * 0.5f
    };
    screen_center = app_canvas_world_to_screen(&app, canvas, graph_center);

    assert(app.canvas.zoom < 1.0f);
    assert(app.canvas.zoom > 0.9f);
    assert_float_close(screen_center.x, canvas.x + (canvas.width * 0.5f));
    assert_float_close(screen_center.y, canvas.y + (canvas.height * 0.5f));

    app_clear_graph(&app);
    printf("test_frame_graph_in_canvas_centers_loaded_circuit passed!\n");
}

static void test_canvas_zoom_clamps_to_supported_range(void) {
    assert_float_close(app_canvas_clamp_zoom(0.1f), APP_CANVAS_MIN_ZOOM);
    assert_float_close(app_canvas_clamp_zoom(1.4f), 1.4f);
    assert_float_close(app_canvas_clamp_zoom(4.2f), APP_CANVAS_MAX_ZOOM);
    printf("test_canvas_zoom_clamps_to_supported_range passed!\n");
}

static void test_canvas_pan_updates_origin_predictably(void) {
    AppContext app;

    app_init(&app);
    app.canvas.origin = (Vector2){ 120.0f, 80.0f };
    app.canvas.zoom = 2.0f;
    app_pan_canvas(&app, (Vector2){ 40.0f, -20.0f });

    assert_float_close(app.canvas.origin.x, 100.0f);
    assert_float_close(app.canvas.origin.y, 90.0f);
    printf("test_canvas_pan_updates_origin_predictably passed!\n");
}

static void test_ui_get_wire_at_tracks_canvas_viewport(void) {
    AppContext app;
    Rectangle canvas;
    LogicNode *input;
    LogicNode *output;
    LogicPin *hit_wire;
    Vector2 hit_screen_pos;

    app_init(&app);
    canvas = (Rectangle){ 132.0f, 50.0f, 720.0f, 480.0f };
    input = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    output = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });

    assert(input != NULL);
    assert(output != NULL);
    assert(app_connect_pins(&app, &input->outputs[0], &output->inputs[0]));

    app.canvas.origin = (Vector2){ 60.0f, 40.0f };
    app.canvas.zoom = 1.6f;
    hit_screen_pos = app_canvas_world_to_screen(&app, canvas, (Vector2){ 280.0f, 180.0f });
    hit_wire = ui_get_wire_at(&app, canvas, hit_screen_pos);

    assert(hit_wire == &output->inputs[0]);
    app_clear_graph(&app);
    printf("test_ui_get_wire_at_tracks_canvas_viewport passed!\n");
}

static void test_ui_get_pin_at_tracks_canvas_zoom(void) {
    AppContext app;
    Rectangle canvas;
    LogicNode *input;
    LogicPin *hit_pin;
    Vector2 hit_screen_pos;

    app_init(&app);
    canvas = (Rectangle){ 132.0f, 50.0f, 720.0f, 480.0f };
    input = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });

    assert(input != NULL);

    app.canvas.origin = (Vector2){ 20.0f, 10.0f };
    app.canvas.zoom = 2.0f;
    hit_screen_pos = app_canvas_world_to_screen(&app, canvas, (Vector2){ 200.0f, 180.0f });
    hit_pin = ui_get_pin_at(&app, canvas, hit_screen_pos);

    assert(hit_pin == &input->outputs[0]);
    app_clear_graph(&app);
    printf("test_ui_get_pin_at_tracks_canvas_zoom passed!\n");
}

static void test_canvas_snap_uses_world_coordinates_after_navigation(void) {
    AppContext app;
    Rectangle canvas;
    Vector2 screen_pos;
    Vector2 world_pos;
    Vector2 snapped;

    app_init(&app);
    canvas = (Rectangle){ 132.0f, 50.0f, 720.0f, 480.0f };
    app.canvas.origin = (Vector2){ 80.0f, 40.0f };
    app.canvas.zoom = 2.0f;
    screen_pos = (Vector2){ 402.0f, 274.0f };
    world_pos = app_canvas_screen_to_world(&app, canvas, screen_pos);
    snapped = app_snap_node_position(world_pos, NODE_GATE_AND);

    assert_float_close(snapped.x, 200.0f);
    assert_float_close(snapped.y, 140.0f);
    printf("test_canvas_snap_uses_world_coordinates_after_navigation passed!\n");
}

static void test_ui_measure_context_panel_stays_within_min_window_bounds(void) {
    AppContext app;
    Rectangle side_panel;
    UiContextPanelLayout layout;

    app_init(&app);
    side_panel = (Rectangle){ 770.0f, 50.0f, 330.0f, 646.0f };
    layout = ui_measure_context_panel(&app, side_panel);

    assert_rect_inside(layout.status_rect, side_panel);
    assert_rect_inside(layout.equation_rect, side_panel);
    assert_rect_inside(layout.truth_table_rect, side_panel);
    assert_rect_inside(layout.why_rect, side_panel);
    assert(layout.compare_rect.width == 0.0f);
    assert(layout.kmap_rect.width == 0.0f);
    assert(!layout.show_compare);
    assert(!layout.show_kmap);

    app_clear_graph(&app);
    printf("test_ui_measure_context_panel_stays_within_min_window_bounds passed!\n");
}

static void test_ui_measure_context_panel_allocates_compare_and_kmap_sections(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *out;
    Rectangle side_panel;
    UiContextPanelLayout layout;

    app_init(&app);
    a = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    b = app_add_named_node(&app, NODE_INPUT, "B", (Vector2){ 140.0f, 260.0f });
    out = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });

    assert(a != NULL);
    assert(b != NULL);
    assert(out != NULL);
    assert(app_connect_pins(&app, &a->outputs[0], &out->inputs[0]));
    app_set_mode(&app, MODE_COMPARE);

    side_panel = (Rectangle){ 770.0f, 50.0f, 330.0f, 646.0f };
    layout = ui_measure_context_panel(&app, side_panel);

    assert(layout.show_compare);
    assert(layout.show_kmap);
    assert(layout.compare_rect.height > 0.0f);
    assert(layout.kmap_rect.height > 0.0f);
    assert_rect_inside(layout.compare_rect, side_panel);
    assert_rect_inside(layout.kmap_rect, side_panel);
    assert_rect_inside(layout.why_rect, side_panel);
    assert(layout.truth_table_rect.height > 0.0f);

    app_clear_graph(&app);
    printf("test_ui_measure_context_panel_allocates_compare_and_kmap_sections passed!\n");
}

static void test_ui_measure_context_panel_handles_narrow_widths_with_long_labels(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *xor_gate;
    LogicNode *or_gate;
    LogicNode *out;
    float widths[] = { 280.0f, 330.0f, 360.0f };
    size_t width_index;

    app_init(&app);
    a = app_add_named_node(&app, NODE_INPUT, "VERY_LONG_INPUT_SIGNAL_A", (Vector2){ 140.0f, 160.0f });
    b = app_add_named_node(&app, NODE_INPUT, "VERY_LONG_INPUT_SIGNAL_B", (Vector2){ 140.0f, 260.0f });
    xor_gate = app_add_named_node(&app, NODE_GATE_XOR, "XOR_STAGE_WITH_LONG_NAME", (Vector2){ 320.0f, 220.0f });
    or_gate = app_add_named_node(&app, NODE_GATE_OR, "OR_STAGE_WITH_LONG_NAME", (Vector2){ 500.0f, 220.0f });
    out = app_add_named_node(&app, NODE_OUTPUT, "VERY_LONG_OUTPUT_SIGNAL_Z", (Vector2){ 700.0f, 220.0f });

    assert(a != NULL);
    assert(b != NULL);
    assert(xor_gate != NULL);
    assert(or_gate != NULL);
    assert(out != NULL);
    assert(app_connect_pins(&app, &a->outputs[0], &xor_gate->inputs[0]));
    assert(app_connect_pins(&app, &b->outputs[0], &xor_gate->inputs[1]));
    assert(app_connect_pins(&app, &xor_gate->outputs[0], &or_gate->inputs[0]));
    assert(app_connect_pins(&app, &a->outputs[0], &or_gate->inputs[1]));
    assert(app_connect_pins(&app, &or_gate->outputs[0], &out->inputs[0]));

    for (width_index = 0U; width_index < sizeof(widths) / sizeof(widths[0]); width_index++) {
        Rectangle side_panel;
        UiContextPanelLayout layout;

        side_panel = (Rectangle){ 1100.0f - widths[width_index], 50.0f, widths[width_index], 646.0f };
        layout = ui_measure_context_panel(&app, side_panel);

        assert_rect_inside(layout.status_rect, side_panel);
        assert_rect_inside(layout.equation_rect, side_panel);
        assert_rect_inside(layout.truth_table_rect, side_panel);
        assert_rect_inside(layout.why_rect, side_panel);
    }

    app_clear_graph(&app);
    printf("test_ui_measure_context_panel_handles_narrow_widths_with_long_labels passed!\n");
}

static void test_ui_context_truth_table_row_rect_matches_first_visible_row(void) {
    AppContext app;
    LogicNode *a;
    LogicNode *b;
    LogicNode *out;
    Rectangle side_panel;
    UiContextPanelLayout layout;
    Rectangle row_rect;

    app_init(&app);
    a = app_add_named_node(&app, NODE_INPUT, "A", (Vector2){ 140.0f, 160.0f });
    b = app_add_named_node(&app, NODE_INPUT, "B", (Vector2){ 140.0f, 260.0f });
    out = app_add_named_node(&app, NODE_OUTPUT, "Z", (Vector2){ 520.0f, 220.0f });

    assert(a != NULL);
    assert(b != NULL);
    assert(out != NULL);
    assert(app_connect_pins(&app, &a->outputs[0], &out->inputs[0]));

    side_panel = (Rectangle){ 770.0f, 50.0f, 330.0f, 646.0f };
    layout = ui_measure_context_panel(&app, side_panel);

    assert(layout.visible_truth_rows > 0U);
    assert(ui_context_truth_table_row_rect(&app, &layout, 0U, &row_rect));
    assert_float_close(row_rect.x, layout.truth_table_rect.x + 10.0f);
    assert_float_close(row_rect.y, layout.truth_table_rect.y + 59.0f);
    assert_float_close(row_rect.width, layout.truth_table_rect.width - 20.0f);
    assert_float_close(row_rect.height, 20.0f);

    app_clear_graph(&app);
    printf("test_ui_context_truth_table_row_rect_matches_first_visible_row passed!\n");
}

static void test_workspace_layout_clamps_panel_sizes(void) {
    WorkspaceLayoutPrefs prefs;
    WorkspaceFrame frame;

    workspace_layout_init_defaults(&prefs);
    prefs.toolbox_width = 400.0f;
    prefs.side_panel_width = 900.0f;
    prefs.wave_height = 900.0f;

    workspace_layout_sanitize_prefs(&prefs, WORKSPACE_WINDOW_MIN_WIDTH, WORKSPACE_WINDOW_MIN_HEIGHT);
    frame = workspace_layout_compute_frame(&prefs, WORKSPACE_WINDOW_MIN_WIDTH, WORKSPACE_WINDOW_MIN_HEIGHT);

    assert(prefs.toolbox_width >= WORKSPACE_TOOLBOX_MIN_WIDTH);
    assert(prefs.toolbox_width <= WORKSPACE_TOOLBOX_MAX_WIDTH);
    assert(prefs.side_panel_width >= WORKSPACE_SIDE_PANEL_MIN_WIDTH);
    assert(prefs.side_panel_width <= WORKSPACE_SIDE_PANEL_MAX_WIDTH);
    assert(prefs.wave_height >= WORKSPACE_WAVE_MIN_HEIGHT);
    assert(prefs.wave_height <= WORKSPACE_WAVE_MAX_HEIGHT);
    assert(frame.canvas_rect.width >= WORKSPACE_CENTER_MIN_WIDTH);
    assert(frame.canvas_rect.height >= WORKSPACE_CANVAS_MIN_HEIGHT);

    printf("test_workspace_layout_clamps_panel_sizes passed!\n");
}

static void test_workspace_layout_drag_updates_each_panel(void) {
    WorkspaceLayoutPrefs prefs;
    WorkspaceFrame frame;

    workspace_layout_init_defaults(&prefs);

    assert(workspace_layout_apply_drag(
        &prefs,
        WORKSPACE_RESIZE_HANDLE_TOOLBOX,
        (Vector2){ 150.0f, 0.0f },
        1440,
        900
    ));
    assert_float_close(prefs.toolbox_width, 150.0f);

    assert(workspace_layout_apply_drag(
        &prefs,
        WORKSPACE_RESIZE_HANDLE_SIDE_PANEL,
        (Vector2){ 1000.0f, 0.0f },
        1440,
        900
    ));
    assert_float_close(prefs.side_panel_width, 440.0f);

    assert(workspace_layout_apply_drag(
        &prefs,
        WORKSPACE_RESIZE_HANDLE_WAVE_PANEL,
        (Vector2){ 0.0f, 540.0f },
        1440,
        900
    ));

    frame = workspace_layout_compute_frame(&prefs, 1440, 900);
    assert_float_close(frame.toolbox_rect.width, prefs.toolbox_width);
    assert_float_close(frame.side_panel_rect.width, prefs.side_panel_width);
    assert_float_close(frame.wave_rect.height, prefs.wave_height);

    printf("test_workspace_layout_drag_updates_each_panel passed!\n");
}

static void test_workspace_layout_save_and_load_round_trip(void) {
    WorkspaceLayoutPrefs saved;
    WorkspaceLayoutPrefs loaded;
    char temp_path[] = "/tmp/mlvd-layout-XXXXXX";
    int fd;

    fd = mkstemp(temp_path);
    assert(fd >= 0);
    close(fd);
    unlink(temp_path);

    assert(setenv("MLVD_LAYOUT_PATH", temp_path, 1) == 0);

    workspace_layout_init_defaults(&saved);
    saved.toolbox_width = 142.0f;
    saved.side_panel_width = 418.0f;
    saved.wave_height = 228.0f;

    assert(workspace_layout_save_prefs(&saved));

    workspace_layout_init_defaults(&loaded);
    loaded.toolbox_width = 0.0f;
    loaded.side_panel_width = 0.0f;
    loaded.wave_height = 0.0f;
    assert(workspace_layout_load_prefs(&loaded));
    assert_float_close(loaded.toolbox_width, saved.toolbox_width);
    assert_float_close(loaded.side_panel_width, saved.side_panel_width);
    assert_float_close(loaded.wave_height, saved.wave_height);

    unsetenv("MLVD_LAYOUT_PATH");
    unlink(temp_path);
    printf("test_workspace_layout_save_and_load_round_trip passed!\n");
}

static void test_toolbox_items_scale_with_panel_width(void) {
    Rectangle narrow_panel;
    Rectangle wide_panel;
    Rectangle narrow_item;
    Rectangle wide_item;
    Vector2 hit_point;

    narrow_panel = (Rectangle){ 0.0f, 50.0f, WORKSPACE_TOOLBOX_MIN_WIDTH, 646.0f };
    wide_panel = (Rectangle){ 0.0f, 50.0f, WORKSPACE_TOOLBOX_MAX_WIDTH, 646.0f };

    assert(ui_toolbox_item_rect(narrow_panel, 0, &narrow_item));
    assert(ui_toolbox_item_rect(wide_panel, 0, &wide_item));
    assert(wide_item.height > narrow_item.height);
    assert(wide_item.width > narrow_item.width);
    assert(wide_item.height <= 58.0f);

    hit_point = (Vector2){
        wide_item.x + (wide_item.width * 0.5f),
        wide_item.y + (wide_item.height * 0.5f)
    };
    assert(ui_toolbox_slot_at(wide_panel, hit_point) == 0);

    printf("test_toolbox_items_scale_with_panel_width passed!\n");
}

static void test_solver_mode_layout_and_input_state(void) {
    AppContext app;
    UiSolverLayout layout;
    Rectangle panel;

    assert(TOPBAR_TAB_COUNT == 3);

    app_init(&app);
    app_set_mode(&app, MODE_SOLVER);
    assert(app.mode == MODE_SOLVER);
    assert(!app.simulation.active);
    assert(app.solver.result.ok);

    app_solver_set_input(&app, "");
    app_solver_insert_char(&app, 'a');
    app_solver_insert_char(&app, '+');
    app_solver_insert_char(&app, '1');
    assert(strcmp(app.solver.input, "A+1") == 0);
    assert(strcmp(app.solver.result.simplified_expression, "1") == 0);
    app_solver_backspace(&app);
    assert(strcmp(app.solver.input, "A+") == 0);
    assert(!app.solver.result.ok);

    panel = (Rectangle){ 0.0f, 50.0f, 900.0f, 826.0f };
    layout = ui_measure_solver_workspace(panel);
    assert(layout.input_rect.y > panel.y);
    assert(layout.preview_rect.y > layout.input_rect.y);
    assert(layout.result_rect.y > layout.preview_rect.y);
    assert(layout.steps_rect.y > layout.result_rect.y);
    assert(layout.steps_rect.y + layout.steps_rect.height <= panel.y + panel.height);
    assert(ui_solver_steps_content_height(&app) > 0.0f);

    printf("test_solver_mode_layout_and_input_state passed!\n");
}

int main(void) {
    test_gate_and();
    test_gate_or();
    test_simple_circuit();
    test_truth_table();
    test_expression();
    test_bool_solver_simplifies_sample_expression();
    test_bool_solver_parser_precedence_and_implicit_and();
    test_bool_solver_grouped_not_and_constants();
    test_bool_solver_reports_invalid_input();
    test_reconnect_replaces_existing_input();
    test_remove_node_removes_attached_nets();
    test_app_default_names();
    test_interactive_construction_flow();
    test_compare_mode_without_target();
    test_circuit_file_load();
    test_circuit_file_load_ignores_explicit_positions();
    test_circuit_file_load_failure_keeps_existing_graph();
    test_example_circuits_load();
    test_circuit_file_load_uses_declaration_order_for_symmetric_layers();
    test_circuit_file_load_packs_disconnected_components();
    test_circuit_file_load_handles_feedback_cycles();
    test_connected_nodes_can_snap_to_straight_wire_alignment();
    test_multi_input_gate_can_snap_to_connected_inputs_centerline();
    test_view_context_matches_live_state();
    test_equation_resolved();
    test_equation_symbolic_and_values();
    test_equation_values_with_unknown_inputs();
    test_text_fit_with_ellipsis_truncates();
    test_text_wrap_with_ellipsis_respects_max_lines();
    test_snap_node_position_keeps_single_pin_nodes_on_grid();
    test_snap_node_position_centers_tall_gates();
    test_delete_selected_wire();
    test_ui_get_wire_at_uses_rendered_wire_path();
    test_canvas_coordinate_transform_round_trip();
    test_canvas_zoom_anchor_keeps_world_point_stable();
    test_frame_graph_in_canvas_centers_loaded_circuit();
    test_canvas_zoom_clamps_to_supported_range();
    test_canvas_pan_updates_origin_predictably();
    test_ui_get_wire_at_tracks_canvas_viewport();
    test_ui_get_pin_at_tracks_canvas_zoom();
    test_canvas_snap_uses_world_coordinates_after_navigation();
    test_ui_measure_context_panel_stays_within_min_window_bounds();
    test_ui_measure_context_panel_allocates_compare_and_kmap_sections();
    test_ui_measure_context_panel_handles_narrow_widths_with_long_labels();
    test_ui_context_truth_table_row_rect_matches_first_visible_row();
    test_workspace_layout_clamps_panel_sizes();
    test_workspace_layout_drag_updates_each_panel();
    test_workspace_layout_save_and_load_round_trip();
    test_toolbox_items_scale_with_panel_width();
    test_solver_mode_layout_and_input_state();
    printf("All tests passed!\n");
    return 0;
}

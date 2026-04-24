#include "logic.h"
#include "node_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void logic_node_init_pins(LogicNode *node) {
    uint8_t i;

    for (i = 0; i < MAX_PINS; i++) {
        node->inputs[i].node = node;
        node->inputs[i].index = i;
        node->inputs[i].value = LOGIC_UNKNOWN;
        node->outputs[i].node = node;
        node->outputs[i].index = i;
        node->outputs[i].value = LOGIC_UNKNOWN;
    }
}

static LogicNet *logic_find_incoming_net(LogicGraph *graph, LogicPin *pin) {
    uint32_t i;

    for (i = 0; i < graph->net_count; i++) {
        uint8_t j;

        for (j = 0; j < graph->nets[i].sink_count; j++) {
            if (graph->nets[i].sinks[j] == pin) {
                return &graph->nets[i];
            }
        }
    }

    return NULL;
}

static void logic_append_text(char *buf, size_t *pos, size_t size, const char *text) {
    size_t available;
    int written;

    if (*pos >= size) {
        return;
    }

    available = size - *pos;
    written = snprintf(buf + *pos, available, "%s", text);
    if (written <= 0) {
        return;
    }

    if ((size_t)written >= available) {
        *pos = size - 1U;
        return;
    }

    *pos += (size_t)written;
}

static bool logic_net_contains_sink(const LogicNet *net, const LogicPin *sink) {
    uint8_t i;

    for (i = 0; i < net->sink_count; i++) {
        if (net->sinks[i] == sink) {
            return true;
        }
    }

    return false;
}

static bool logic_net_references_node(const LogicNet *net, const LogicNode *node) {
    uint8_t i;

    if (net->source && net->source->node == node) {
        return true;
    }

    for (i = 0; i < net->sink_count; i++) {
        if (net->sinks[i] && net->sinks[i]->node == node) {
            return true;
        }
    }

    return false;
}

static void logic_remove_net_at(LogicGraph *graph, uint32_t index) {
    uint32_t i;

    if (index >= graph->net_count) {
        return;
    }

    for (i = index + 1U; i < graph->net_count; i++) {
        graph->nets[i - 1U] = graph->nets[i];
    }

    graph->net_count--;
    memset(&graph->nets[graph->net_count], 0, sizeof(LogicNet));
}

typedef enum {
    LOGIC_VISIT_UNSEEN = 0,
    LOGIC_VISIT_ACTIVE = 1,
    LOGIC_VISIT_DONE = 2
} LogicVisitState;

static void visit(LogicGraph *graph, LogicNode *node, uint8_t *visit_state, LogicNode **sorted, uint32_t *count) {
    size_t node_index;
    uint8_t i;

    if (!logic_node_is_active(node)) {
        return;
    }

    node_index = (size_t)(node - graph->nodes);
    if (visit_state[node_index] == LOGIC_VISIT_DONE || visit_state[node_index] == LOGIC_VISIT_ACTIVE) {
        return;
    }

    visit_state[node_index] = LOGIC_VISIT_ACTIVE;
    for (i = 0; i < node->input_count; i++) {
        LogicPin *pin;
        LogicNet *incoming;

        pin = &node->inputs[i];
        incoming = logic_find_incoming_net(graph, pin);
        if (incoming) {
            visit(graph, incoming->source->node, visit_state, sorted, count);
        }
    }

    visit_state[node_index] = LOGIC_VISIT_DONE;
    sorted[(*count)++] = node;
}

static const char *logic_value_digit(LogicValue value) {
    if (value == LOGIC_HIGH) {
        return "1";
    }
    if (value == LOGIC_LOW) {
        return "0";
    }
    if (value == LOGIC_ERROR) {
        return "E";
    }
    return "?";
}

static void build_node_expr(LogicGraph *graph, LogicNode *node, char *buf, size_t *pos, size_t size, bool use_values);

static LogicValue logic_node_result_value(const LogicNode *node) {
    if (node->type == NODE_OUTPUT) {
        return node->inputs[0].value;
    }
    if (node->output_count > 0) {
        return node->outputs[0].value;
    }

    return LOGIC_UNKNOWN;
}

static void build_pin_expr(LogicGraph *graph, LogicPin *sink_pin, char *buf, size_t *pos, size_t size, bool use_values) {
    LogicNet *incoming;

    incoming = logic_find_incoming_net(graph, sink_pin);
    if (!incoming || !incoming->source) {
        logic_append_text(buf, pos, size, "?");
        return;
    }
    build_node_expr(graph, incoming->source->node, buf, pos, size, use_values);
}

static void build_node_expr(LogicGraph *graph, LogicNode *node, char *buf, size_t *pos, size_t size, bool use_values) {
    if (node->type == NODE_INPUT || node->type == NODE_GATE_CLOCK) {
        if (use_values) {
            logic_append_text(buf, pos, size, logic_value_digit(node->outputs[0].value));
        } else {
            logic_append_text(buf, pos, size, node->name ? node->name : (node->type == NODE_INPUT ? "IN" : "CLK"));
        }
        return;
    }

    if (node->type == NODE_OUTPUT) {
        build_pin_expr(graph, &node->inputs[0], buf, pos, size, use_values);
        return;
    }

    logic_append_text(buf, pos, size, "(");
    if (node->type == NODE_GATE_NOT) {
        logic_append_text(buf, pos, size, "NOT ");
        build_pin_expr(graph, &node->inputs[0], buf, pos, size, use_values);
    } else {
        const char *op;
        uint8_t i;

        op = node_catalog_operator_text(node->type);
        for (i = 0; i < node->input_count; i++) {
            if (i > 0) {
                logic_append_text(buf, pos, size, op);
            }
            build_pin_expr(graph, &node->inputs[i], buf, pos, size, use_values);
        }
    }
    logic_append_text(buf, pos, size, ")");
}

void logic_init_graph(LogicGraph *graph) {
    memset(graph, 0, sizeof(LogicGraph));
}

LogicNode* logic_add_node(LogicGraph *graph, NodeType type, const char *name) {
    LogicNode *node;

    if (graph->node_count >= MAX_NODES) {
        return NULL;
    }

    node = &graph->nodes[graph->node_count++];
    memset(node, 0, sizeof(LogicNode));
    node->type = type;
    if (name) {
        node->name = strdup(name);
    }

    logic_node_init_pins(node);
    node->input_count = node_catalog_input_count(type);
    node->output_count = node_catalog_output_count(type);
    return node;
}

LogicNet* logic_add_net(LogicGraph *graph) {
    LogicNet *net;

    if (graph->net_count >= MAX_NETS) {
        return NULL;
    }

    net = &graph->nets[graph->net_count++];
    memset(net, 0, sizeof(LogicNet));
    return net;
}

bool logic_connect(LogicGraph *graph, LogicPin *src, LogicPin *sink) {
    LogicNet *net;
    uint32_t i;

    if (!src || !sink || !src->node || !sink->node) {
        return false;
    }
    if (!logic_node_is_active(src->node) || !logic_node_is_active(sink->node)) {
        return false;
    }
    if (src->node->output_count == 0 || sink->node->input_count == 0) {
        return false;
    }
    if (src == sink) {
        return false;
    }

    net = NULL;
    for (i = 0; i < graph->net_count; i++) {
        if (graph->nets[i].source == src) {
            net = &graph->nets[i];
            break;
        }
    }

    if (logic_disconnect_sink(graph, sink)) {
        net = NULL;
        for (i = 0; i < graph->net_count; i++) {
            if (graph->nets[i].source == src) {
                net = &graph->nets[i];
                break;
            }
        }
    }

    if (!net) {
        net = logic_add_net(graph);
        if (!net) {
            return false;
        }
        net->source = src;
    }

    if (net->sink_count >= MAX_PINS) {
        return false;
    }
    if (logic_net_contains_sink(net, sink)) {
        return true;
    }

    net->sinks[net->sink_count++] = sink;
    return true;
}

bool logic_disconnect_sink(LogicGraph *graph, LogicPin *sink) {
    uint32_t i;

    if (!sink) {
        return false;
    }

    for (i = 0; i < graph->net_count; i++) {
        LogicNet *net;
        uint8_t j;

        net = &graph->nets[i];
        for (j = 0; j < net->sink_count; j++) {
            if (net->sinks[j] != sink) {
                continue;
            }

            for (; j + 1U < net->sink_count; j++) {
                net->sinks[j] = net->sinks[j + 1U];
            }
            net->sink_count--;
            net->sinks[net->sink_count] = NULL;

            if (net->sink_count == 0) {
                logic_remove_net_at(graph, i);
            }
            return true;
        }
    }

    return false;
}

bool logic_remove_node(LogicGraph *graph, LogicNode *node) {
    uint32_t i;

    if (!node || !logic_node_is_active(node)) {
        return false;
    }

    for (i = 0; i < graph->net_count;) {
        if (logic_net_references_node(&graph->nets[i], node)) {
            logic_remove_net_at(graph, i);
            continue;
        }
        i++;
    }

    free(node->name);
    node->name = NULL;
    node->type = NODE_INVALID;
    node->input_count = 0;
    node->output_count = 0;
    node->state = LOGIC_UNKNOWN;
    node->prev_state = LOGIC_UNKNOWN;
    node->evaluated = false;
    node->state_changed = false;
    node->inputs_changed = false;
    node->rect = (Rectangle){ 0 };
    node->pos = (Vector2){ 0 };

    return true;
}

LogicValue logic_eval_gate(NodeType type, LogicValue inputs[], uint8_t count) {
    uint8_t i;

    if (!inputs || count == 0U) {
        return LOGIC_UNKNOWN;
    }
    if (count < node_catalog_input_count(type)) {
        return LOGIC_UNKNOWN;
    }

    for (i = 0; i < count; i++) {
        if (inputs[i] == LOGIC_UNKNOWN) {
            return LOGIC_UNKNOWN;
        }
        if (inputs[i] == LOGIC_ERROR) {
            return LOGIC_ERROR;
        }
    }

    if (type == NODE_GATE_AND) {
        for (i = 0; i < count; i++) {
            if (inputs[i] == LOGIC_LOW) {
                return LOGIC_LOW;
            }
        }
        return LOGIC_HIGH;
    }

    if (type == NODE_GATE_OR) {
        for (i = 0; i < count; i++) {
            if (inputs[i] == LOGIC_HIGH) {
                return LOGIC_HIGH;
            }
        }
        return LOGIC_LOW;
    }

    if (type == NODE_GATE_NOT) {
        return (inputs[0] == LOGIC_HIGH) ? LOGIC_LOW : LOGIC_HIGH;
    }

    if (type == NODE_GATE_XOR) {
        return (inputs[0] != inputs[1]) ? LOGIC_HIGH : LOGIC_LOW;
    }

    if (type == NODE_GATE_NAND) {
        for (i = 0; i < count; i++) {
            if (inputs[i] == LOGIC_LOW) {
                return LOGIC_HIGH;
            }
        }
        return LOGIC_LOW;
    }

    if (type == NODE_GATE_NOR) {
        for (i = 0; i < count; i++) {
            if (inputs[i] == LOGIC_HIGH) {
                return LOGIC_LOW;
            }
        }
        return LOGIC_HIGH;
    }

    return LOGIC_UNKNOWN;
}

uint32_t logic_topological_sort(LogicGraph *graph, LogicNode **sorted_nodes) {
    uint8_t visit_state[MAX_NODES] = {0};
    uint32_t count;
    uint32_t i;

    count = 0;
    for (i = 0; i < graph->node_count; i++) {
        visit(graph, &graph->nodes[i], visit_state, sorted_nodes, &count);
    }
    return count;
}

bool logic_node_is_active(const LogicNode *node) {
    return node && node->type != NODE_INVALID;
}

void logic_evaluate(LogicGraph *graph) {
    LogicNode *sorted[MAX_NODES];
    uint32_t count;
    uint32_t i;

    count = logic_topological_sort(graph, sorted);
    for (i = 0; i < count; i++) {
        LogicNode *node;
        LogicValue inputs[MAX_PINS];
        uint8_t j;

        node = sorted[i];
        if (!logic_node_is_active(node)) {
            continue;
        }
        if (node->type == NODE_INPUT) {
            continue;
        }

        for (j = 0U; j < MAX_PINS; j++) {
            inputs[j] = LOGIC_UNKNOWN;
        }

        for (j = 0; j < node->input_count; j++) {
            LogicValue value;
            LogicPin *pin;
            LogicNet *incoming;

            value = LOGIC_UNKNOWN;
            pin = &node->inputs[j];
            incoming = logic_find_incoming_net(graph, pin);
            if (incoming) {
                value = incoming->source->value;
            }
            inputs[j] = value;
        }

        if (node->type == NODE_OUTPUT) {
            node->inputs[0].value = inputs[0];
        } else if (node->type == NODE_GATE_DFF) {
            node->outputs[0].value = node->state;
        } else if (node->type == NODE_GATE_LATCH) {
            if (inputs[1] == LOGIC_HIGH) {
                node->state = inputs[0];
            }
            node->outputs[0].value = node->state;
        } else if (node->type != NODE_GATE_CLOCK) {
            node->outputs[0].value = logic_eval_gate(node->type, inputs, node->input_count);
        }
    }
}

void logic_tick(LogicGraph *graph) {
    uint32_t i;

    for (i = 0; i < graph->node_count; i++) {
        LogicNode *node;

        node = &graph->nodes[i];
        if (!logic_node_is_active(node)) {
            continue;
        }
        if (node->type == NODE_GATE_DFF) {
            LogicValue clk;
            LogicValue d;
            bool d_changed;
            uint32_t k;

            clk = LOGIC_UNKNOWN;
            d = LOGIC_UNKNOWN;
            d_changed = false;
            for (k = 0; k < graph->net_count; k++) {
                uint8_t l;

                for (l = 0; l < graph->nets[k].sink_count; l++) {
                    if (graph->nets[k].sinks[l] == &node->inputs[0]) {
                        d = graph->nets[k].source->value;
                        if (graph->nets[k].source->node->state_changed) {
                            d_changed = true;
                        }
                    }
                    if (graph->nets[k].sinks[l] == &node->inputs[1]) {
                        clk = graph->nets[k].source->value;
                    }
                }
            }

            node->state_changed = false;
            if (node->prev_state == LOGIC_LOW && clk == LOGIC_HIGH) {
                node->state = d_changed ? LOGIC_ERROR : d;
                node->state_changed = true;
            }
            node->prev_state = clk;
        } else if (node->type == NODE_GATE_CLOCK) {
            node->state = (node->state == LOGIC_HIGH) ? LOGIC_LOW : LOGIC_HIGH;
            node->outputs[0].value = node->state;
            node->state_changed = true;
        } else {
            node->state_changed = false;
        }
    }

    logic_evaluate(graph);
}

TruthTable* logic_generate_truth_table(LogicGraph *graph) {
    TruthTable *table;
    uint32_t cols;
    uint32_t r;

    table = (TruthTable *)calloc(1, sizeof(TruthTable));
    if (!table) {
        return NULL;
    }

    for (r = 0; r < graph->node_count; r++) {
        LogicNode *node;

        node = &graph->nodes[r];
        if (node->type == NODE_INPUT && table->input_count < MAX_PINS) {
            table->inputs[table->input_count++] = node;
        } else if (node->type == NODE_OUTPUT && table->output_count < MAX_PINS) {
            table->outputs[table->output_count++] = node;
        }
    }

    table->row_count = 1U << table->input_count;
    cols = (uint32_t)table->input_count + (uint32_t)table->output_count;
    table->data = (LogicValue *)calloc(table->row_count * cols, sizeof(LogicValue));
    if (!table->data) {
        free(table);
        return NULL;
    }

    for (r = 0; r < table->row_count; r++) {
        uint8_t i;

        for (i = 0; i < table->input_count; i++) {
            uint8_t bit_index;
            uint32_t bit_mask;
            LogicValue value;

            bit_index = (uint8_t)(table->input_count - 1U - i);
            bit_mask = 1U << bit_index;
            value = (r & bit_mask) ? LOGIC_HIGH : LOGIC_LOW;
            table->inputs[i]->outputs[0].value = value;
            table->data[(r * cols) + (uint32_t)i] = value;
        }

        logic_evaluate(graph);

        for (i = 0; i < table->output_count; i++) {
            table->data[(r * cols) + (uint32_t)table->input_count + (uint32_t)i] =
                table->outputs[i]->inputs[0].value;
        }
    }

    return table;
}

char* logic_generate_expression(LogicGraph *graph, LogicNode *output_node) {
    char *buf;
    size_t pos;

    if (!output_node || output_node->type != NODE_OUTPUT) {
        return NULL;
    }

    buf = (char *)calloc(1024, sizeof(char));
    if (!buf) {
        return NULL;
    }

    pos = 0;
    build_pin_expr(graph, &output_node->inputs[0], buf, &pos, 1024U, false);
    return buf;
}

bool logic_format_equation_symbolic(LogicGraph *graph, LogicNode *node, char *out, size_t len) {
    size_t pos;
    LogicValue value;
    const char *name;

    if (!out || len == 0U || !node) {
        return false;
    }

    out[0] = '\0';
    pos = 0U;
    name = node->name ? node->name : "out";

    if (node->type == NODE_INPUT || node->type == NODE_GATE_CLOCK) {
        value = logic_node_result_value(node);
        logic_append_text(out, &pos, len, name);
        logic_append_text(out, &pos, len, " = ");
        logic_append_text(out, &pos, len, logic_value_digit(value));
        return true;
    }

    logic_append_text(out, &pos, len, name);
    logic_append_text(out, &pos, len, " = ");
    build_node_expr(graph, node, out, &pos, len, false);
    return true;
}

bool logic_format_equation_values(LogicGraph *graph, LogicNode *node, char *out, size_t len) {
    size_t pos;
    LogicValue value;

    if (!out || len == 0U || !node) {
        return false;
    }

    out[0] = '\0';
    pos = 0U;
    value = logic_node_result_value(node);

    if (node->type == NODE_INPUT || node->type == NODE_GATE_CLOCK) {
        logic_append_text(out, &pos, len, logic_value_digit(value));
        return true;
    }

    build_node_expr(graph, node, out, &pos, len, true);
    logic_append_text(out, &pos, len, " -> ");
    logic_append_text(out, &pos, len, logic_value_digit(value));
    return true;
}

bool logic_format_equation_resolved(LogicGraph *graph, LogicNode *node, char *out, size_t len) {
    size_t pos;
    LogicValue value;
    const char *name;

    if (!out || len == 0U || !node) {
        return false;
    }

    out[0] = '\0';
    pos = 0;

    name = node->name ? node->name : "out";

    if (node->type == NODE_INPUT || node->type == NODE_GATE_CLOCK) {
        value = (node->output_count > 0) ? node->outputs[0].value : LOGIC_UNKNOWN;
        logic_append_text(out, &pos, len, name);
        logic_append_text(out, &pos, len, " = ");
        logic_append_text(out, &pos, len, logic_value_digit(value));
        return true;
    }

    if (node->type == NODE_OUTPUT) {
        value = node->inputs[0].value;
    } else if (node->output_count > 0) {
        value = node->outputs[0].value;
    } else {
        value = LOGIC_UNKNOWN;
    }

    logic_append_text(out, &pos, len, name);
    logic_append_text(out, &pos, len, " = ");
    build_node_expr(graph, node, out, &pos, len, false);
    logic_append_text(out, &pos, len, " = ");
    build_node_expr(graph, node, out, &pos, len, true);
    logic_append_text(out, &pos, len, " = ");
    logic_append_text(out, &pos, len, logic_value_digit(value));
    return true;
}

void logic_free_truth_table(TruthTable *table) {
    if (table) {
        free(table->data);
        free(table);
    }
}

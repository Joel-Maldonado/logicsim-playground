#include "circuit_file.h"
#include "app_analysis.h"
#include "app_canvas.h"
#include "circuit_layout.h"
#include "node_catalog.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARSED_NAME_MAX 64

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

typedef struct {
    NodeType type;
    char name[PARSED_NAME_MAX];
    Vector2 position;
    bool has_position;
} CircuitDocumentNode;

typedef struct {
    char node_name[PARSED_NAME_MAX];
    bool explicit_pin;
    bool is_output_pin;
    uint8_t pin_index;
} CircuitEndpoint;

typedef struct {
    CircuitEndpoint source;
    CircuitEndpoint sink;
} CircuitDocumentWire;

typedef struct {
    CircuitDocumentNode nodes[MAX_NODES];
    CircuitDocumentWire wires[MAX_NETS];
    uint32_t node_count;
    uint32_t wire_count;
} CircuitDocument;

typedef struct {
    AppMode mode;
    AppTool active_tool;
    AppPanelFocus focused_panel;
    LogicGraph *target_graph;
    float simulation_speed;
    bool live_reload;
    char source_path[APP_SOURCE_PATH_MAX];
} AppLoadSettings;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static void set_error(char *error_message, size_t error_message_size, const char *message, unsigned int line_number) {
    if (!error_message || error_message_size == 0U) {
        return;
    }

    if (line_number == 0U) {
        snprintf(error_message, error_message_size, "%s", message);
        return;
    }

    snprintf(error_message, error_message_size, "Line %u: %s", line_number, message);
}

static char *trim_whitespace(char *text) {
    size_t length;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    length = strlen(text);
    while (length > 0U && isspace((unsigned char)text[length - 1U])) {
        text[length - 1U] = '\0';
        length--;
    }

    return text;
}

static void strip_comment(char *text) {
    char *comment;

    comment = strchr(text, '#');
    if (comment) {
        *comment = '\0';
    }
}

static CircuitDocumentNode *find_document_node(CircuitDocument *document, const char *name) {
    uint32_t index;

    for (index = 0U; index < document->node_count; index++) {
        if (strcmp(document->nodes[index].name, name) == 0) {
            return &document->nodes[index];
        }
    }

    return NULL;
}

static int32_t find_document_node_index(const CircuitDocument *document, const char *name) {
    uint32_t index;

    for (index = 0U; index < document->node_count; index++) {
        if (strcmp(document->nodes[index].name, name) == 0) {
            return (int32_t)index;
        }
    }

    return -1;
}

static bool parse_endpoint(const char *text, CircuitEndpoint *endpoint) {
    char buffer[PARSED_NAME_MAX];
    char *dot;
    char *endptr;
    long index;

    memset(endpoint, 0, sizeof(*endpoint));
    if (strlen(text) >= sizeof(buffer)) {
        return false;
    }

    snprintf(buffer, sizeof(buffer), "%s", text);
    dot = strchr(buffer, '.');
    if (!dot) {
        snprintf(endpoint->node_name, sizeof(endpoint->node_name), "%s", buffer);
        endpoint->pin_index = 0U;
        return true;
    }

    *dot = '\0';
    dot++;
    snprintf(endpoint->node_name, sizeof(endpoint->node_name), "%s", buffer);

    if (strncmp(dot, "out", 3) == 0) {
        endpoint->is_output_pin = true;
        dot += 3;
    } else if (strncmp(dot, "in", 2) == 0) {
        endpoint->is_output_pin = false;
        dot += 2;
    } else {
        return false;
    }

    if (*dot == '\0') {
        return false;
    }

    index = strtol(dot, &endptr, 10);
    if (*endptr != '\0' || index < 0L || index > 255L) {
        return false;
    }

    endpoint->explicit_pin = true;
    endpoint->pin_index = (uint8_t)index;
    return true;
}

static bool parse_position_clause(const char *text, Vector2 *position) {
    float x;
    float y;

    if (sscanf(text, "at %f , %f", &x, &y) == 2) {
        position->x = x;
        position->y = y;
        return true;
    }
    if (sscanf(text, "at %f,%f", &x, &y) == 2) {
        position->x = x;
        position->y = y;
        return true;
    }

    return false;
}

static bool parse_node_line(
    CircuitDocument *document,
    char *line,
    unsigned int line_number,
    char *error_message,
    size_t error_message_size
) {
    char *kind_token;
    char *name_token;
    char *rest;
    NodeType type;
    CircuitDocumentNode *node;

    kind_token = strtok(line, " \t");
    name_token = strtok(NULL, " \t");
    rest = strtok(NULL, "");

    if (!kind_token || !name_token) {
        set_error(error_message, error_message_size, "node declaration needs a type and name", line_number);
        return false;
    }
    if (!node_catalog_type_from_token(kind_token, &type)) {
        set_error(error_message, error_message_size, "unknown node type", line_number);
        return false;
    }
    if (find_document_node(document, name_token)) {
        set_error(error_message, error_message_size, "duplicate node name", line_number);
        return false;
    }
    if (document->node_count >= MAX_NODES) {
        set_error(error_message, error_message_size, "too many nodes", line_number);
        return false;
    }

    node = &document->nodes[document->node_count++];
    memset(node, 0, sizeof(*node));
    node->type = type;
    snprintf(node->name, sizeof(node->name), "%s", name_token);

    if (!rest) {
        return true;
    }

    rest = trim_whitespace(rest);
    if (*rest == '\0') {
        return true;
    }
    if (!parse_position_clause(rest, &node->position)) {
        set_error(error_message, error_message_size, "expected optional position like 'at 140,160'", line_number);
        return false;
    }
    node->has_position = true;
    return true;
}

static bool parse_wire_line(
    CircuitDocument *document,
    char *line,
    unsigned int line_number,
    char *error_message,
    size_t error_message_size
) {
    char *source_token;
    char *arrow_token;
    char *sink_token;
    CircuitDocumentWire *wire;

    source_token = strtok(line, " \t");
    arrow_token = strtok(NULL, " \t");
    sink_token = strtok(NULL, " \t");

    if (!source_token || !arrow_token || !sink_token || strcmp(arrow_token, "->") != 0) {
        set_error(error_message, error_message_size, "wire declaration must look like 'wire A -> G1.in0'", line_number);
        return false;
    }
    if (document->wire_count >= MAX_NETS) {
        set_error(error_message, error_message_size, "too many wires", line_number);
        return false;
    }

    wire = &document->wires[document->wire_count++];
    memset(wire, 0, sizeof(*wire));
    if (!parse_endpoint(source_token, &wire->source)) {
        set_error(error_message, error_message_size, "invalid source endpoint", line_number);
        return false;
    }
    if (!parse_endpoint(sink_token, &wire->sink)) {
        set_error(error_message, error_message_size, "invalid sink endpoint", line_number);
        return false;
    }

    return true;
}

static bool parse_circuit_text(const char *text, CircuitDocument *document, char *error_message, size_t error_message_size) {
    char *buffer;
    char *cursor;
    unsigned int line_number;

    memset(document, 0, sizeof(*document));
    buffer = strdup(text);
    if (!buffer) {
        set_error(error_message, error_message_size, "out of memory", 0U);
        return false;
    }

    cursor = buffer;
    line_number = 0U;
    while (cursor && *cursor != '\0') {
        char *line_end;
        char *line;

        line_end = strchr(cursor, '\n');
        if (line_end) {
            *line_end = '\0';
        }

        line_number++;
        line = trim_whitespace(cursor);
        strip_comment(line);
        line = trim_whitespace(line);
        if (*line != '\0') {
            if (strncmp(line, "wire ", 5) == 0) {
                if (!parse_wire_line(document, line + 5, line_number, error_message, error_message_size)) {
                    free(buffer);
                    return false;
                }
            } else if (!parse_node_line(document, line, line_number, error_message, error_message_size)) {
                free(buffer);
                return false;
            }
        }

        cursor = line_end ? line_end + 1 : NULL;
    }

    free(buffer);
    return true;
}

static bool resolve_source_pin(const CircuitDocumentNode *node, const CircuitEndpoint *endpoint, uint8_t *pin_index) {
    uint8_t output_count;

    output_count = node_catalog_output_count(node->type);
    if (endpoint->explicit_pin) {
        if (!endpoint->is_output_pin || endpoint->pin_index >= output_count) {
            return false;
        }
        *pin_index = endpoint->pin_index;
        return true;
    }
    if (output_count != 1U) {
        return false;
    }

    *pin_index = 0U;
    return true;
}

static bool resolve_sink_pin(const CircuitDocumentNode *node, const CircuitEndpoint *endpoint, uint8_t *pin_index) {
    uint8_t input_count;

    input_count = node_catalog_input_count(node->type);
    if (endpoint->explicit_pin) {
        if (endpoint->is_output_pin || endpoint->pin_index >= input_count) {
            return false;
        }
        *pin_index = endpoint->pin_index;
        return true;
    }
    if (input_count != 1U) {
        return false;
    }

    *pin_index = 0U;
    return true;
}

static bool build_layout_spec(
    const CircuitDocument *document,
    CircuitLayoutNode *layout_nodes,
    CircuitLayoutEdge *layout_edges,
    uint32_t *layout_edge_count,
    char *error_message,
    size_t error_message_size
) {
    uint32_t node_index;
    uint32_t wire_index;

    for (node_index = 0U; node_index < document->node_count; node_index++) {
        uint8_t input_count;
        uint8_t output_count;

        input_count = node_catalog_input_count(document->nodes[node_index].type);
        output_count = node_catalog_output_count(document->nodes[node_index].type);
        layout_nodes[node_index].type = document->nodes[node_index].type;
        layout_nodes[node_index].name = document->nodes[node_index].name;
        layout_nodes[node_index].input_count = input_count;
        layout_nodes[node_index].output_count = output_count;
    }

    *layout_edge_count = 0U;
    for (wire_index = 0U; wire_index < document->wire_count; wire_index++) {
        const CircuitDocumentWire *wire;
        int32_t source_index;
        int32_t sink_index;
        uint8_t source_pin_index;
        uint8_t sink_pin_index;

        wire = &document->wires[wire_index];
        source_index = find_document_node_index(document, wire->source.node_name);
        sink_index = find_document_node_index(document, wire->sink.node_name);
        if (source_index < 0 || sink_index < 0) {
            set_error(error_message, error_message_size, "wire references an unknown node", 0U);
            return false;
        }
        if (!resolve_source_pin(&document->nodes[(uint32_t)source_index], &wire->source, &source_pin_index)) {
            set_error(error_message, error_message_size, "wire source pin is invalid", 0U);
            return false;
        }
        if (!resolve_sink_pin(&document->nodes[(uint32_t)sink_index], &wire->sink, &sink_pin_index)) {
            set_error(error_message, error_message_size, "wire sink pin is invalid", 0U);
            return false;
        }

        layout_edges[*layout_edge_count].source_node_index = (uint32_t)source_index;
        layout_edges[*layout_edge_count].source_pin_index = source_pin_index;
        layout_edges[*layout_edge_count].sink_node_index = (uint32_t)sink_index;
        layout_edges[*layout_edge_count].sink_pin_index = sink_pin_index;
        (*layout_edge_count)++;
    }

    return true;
}

static void capture_load_settings(const AppContext *app, AppLoadSettings *settings) {
    settings->mode = app->mode;
    settings->active_tool = app->active_tool;
    settings->focused_panel = app->selection.focused_panel;
    settings->target_graph = app->comparison.target_graph;
    settings->simulation_speed = app->simulation.speed;
    settings->live_reload = app->source.live_reload;
    snprintf(settings->source_path, sizeof(settings->source_path), "%s", app->source.path);
}

static void restore_load_settings(AppContext *app, const AppLoadSettings *settings) {
    app->mode = settings->mode;
    app->active_tool = settings->active_tool;
    app->selection.focused_panel = settings->focused_panel;
    app->comparison.target_graph = settings->target_graph;
    app->simulation.speed = settings->simulation_speed;
    app->source.live_reload = settings->live_reload;
    app_set_source_path(app, settings->source_path);
}

static void reset_graph_after_failed_load(AppContext *app, const AppLoadSettings *settings) {
    app_clear_graph(app);
    restore_load_settings(app, settings);
}

static bool apply_document(
    AppContext *app,
    const CircuitDocument *document,
    char *error_message,
    size_t error_message_size
) {
    CircuitLayoutNode layout_nodes[MAX_NODES];
    CircuitLayoutEdge layout_edges[MAX_NETS];
    Vector2 layout_positions[MAX_NODES];
    AppLoadSettings settings;
    uint32_t layout_edge_count;
    uint32_t node_index;
    uint32_t wire_index;

    if (!build_layout_spec(document, layout_nodes, layout_edges, &layout_edge_count, error_message, error_message_size)) {
        return false;
    }
    if (!circuit_layout_resolve_positions(
            layout_nodes,
            document->node_count,
            layout_edges,
            layout_edge_count,
            layout_positions
        )) {
        set_error(error_message, error_message_size, "could not lay out circuit", 0U);
        return false;
    }

    capture_load_settings(app, &settings);
    app_clear_graph(app);
    restore_load_settings(app, &settings);

    for (node_index = 0U; node_index < document->node_count; node_index++) {
        Vector2 position;

        position = app_snap_node_position(layout_positions[node_index], document->nodes[node_index].type);
        if (!app_add_named_node(app, document->nodes[node_index].type, document->nodes[node_index].name, position)) {
            set_error(error_message, error_message_size, "could not add node to graph", 0U);
            reset_graph_after_failed_load(app, &settings);
            return false;
        }
    }

    for (wire_index = 0U; wire_index < layout_edge_count; wire_index++) {
        LogicNode *source_node;
        LogicNode *sink_node;
        LogicPin *source_pin;
        LogicPin *sink_pin;

        source_node = &app->graph.nodes[layout_edges[wire_index].source_node_index];
        sink_node = &app->graph.nodes[layout_edges[wire_index].sink_node_index];
        source_pin = &source_node->outputs[layout_edges[wire_index].source_pin_index];
        sink_pin = &sink_node->inputs[layout_edges[wire_index].sink_pin_index];

        if (!logic_connect(&app->graph, source_pin, sink_pin)) {
            set_error(error_message, error_message_size, "could not connect wire", 0U);
            reset_graph_after_failed_load(app, &settings);
            return false;
        }
    }

    app_rebuild_derived_state(app);
    return true;
}

bool circuit_file_load(AppContext *app, const char *path, char *error_message, size_t error_message_size) {
    FILE *file;
    long size;
    char *buffer;
    CircuitDocument document;
    bool parsed;

    if (!app || !path) {
        set_error(error_message, error_message_size, "missing app or path", 0U);
        return false;
    }

    file = fopen(path, "rb");
    if (!file) {
        set_error(error_message, error_message_size, "could not open circuit file", 0U);
        return false;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        set_error(error_message, error_message_size, "could not seek circuit file", 0U);
        return false;
    }

    size = ftell(file);
    if (size < 0L) {
        fclose(file);
        set_error(error_message, error_message_size, "could not read circuit file size", 0U);
        return false;
    }
    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        set_error(error_message, error_message_size, "could not rewind circuit file", 0U);
        return false;
    }

    buffer = (char *)calloc((size_t)size + 1U, sizeof(char));
    if (!buffer) {
        fclose(file);
        set_error(error_message, error_message_size, "out of memory", 0U);
        return false;
    }

    if (size > 0L && fread(buffer, (size_t)size, 1U, file) != 1U) {
        free(buffer);
        fclose(file);
        set_error(error_message, error_message_size, "could not read circuit file", 0U);
        return false;
    }
    fclose(file);

    parsed = parse_circuit_text(buffer, &document, error_message, error_message_size);
    free(buffer);
    if (!parsed) {
        return false;
    }

    return apply_document(app, &document, error_message, error_message_size);
}

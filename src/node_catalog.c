#include "node_catalog.h"
#include <string.h>

static const NodeCatalogEntry node_catalog[] = {
    { NODE_INPUT, APP_TOOL_INPUT, "input", "IN", "IN", NULL, 60, 0U, 1U, 1U, true },
    { NODE_OUTPUT, APP_TOOL_OUTPUT, "output", "OUT", "OUT", NULL, 60, 1U, 0U, 1U, true },
    { NODE_GATE_AND, APP_TOOL_AND, "and", NULL, "AND", " AND ", 80, 2U, 1U, 2U, true },
    { NODE_GATE_OR, APP_TOOL_OR, "or", NULL, "OR", " OR ", 80, 2U, 1U, 2U, true },
    { NODE_GATE_NOT, APP_TOOL_NOT, "not", NULL, "NOT", NULL, 60, 1U, 1U, 1U, true },
    { NODE_GATE_XOR, APP_TOOL_XOR, "xor", NULL, "XOR", " XOR ", 80, 2U, 1U, 2U, true },
    { NODE_GATE_NAND, APP_TOOL_SELECT, "nand", "NAND", "NAND", " NAND ", 80, 2U, 1U, 2U, false },
    { NODE_GATE_NOR, APP_TOOL_SELECT, "nor", "NOR", "NOR", " NOR ", 80, 2U, 1U, 2U, false },
    { NODE_GATE_DFF, APP_TOOL_SELECT, "dff", "DFF", "DFF", NULL, 80, 2U, 1U, 2U, false },
    { NODE_GATE_LATCH, APP_TOOL_SELECT, "latch", "LATCH", "LATCH", NULL, 80, 2U, 1U, 2U, false },
    { NODE_GATE_CLOCK, APP_TOOL_CLOCK, "clock", "CLK", "CLK", NULL, 60, 0U, 1U, 1U, true },
};

const NodeCatalogEntry *node_catalog_entry(NodeType type) {
    size_t index;

    for (index = 0U; index < sizeof(node_catalog) / sizeof(node_catalog[0]); index++) {
        if (node_catalog[index].type == type) {
            return &node_catalog[index];
        }
    }

    return NULL;
}

bool node_catalog_type_from_token(const char *token, NodeType *type) {
    size_t index;

    if (!token || !type) {
        return false;
    }

    for (index = 0U; index < sizeof(node_catalog) / sizeof(node_catalog[0]); index++) {
        if (strcmp(token, node_catalog[index].token) == 0) {
            *type = node_catalog[index].type;
            return true;
        }
    }

    return false;
}

uint8_t node_catalog_input_count(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry ? entry->input_count : 0U;
}

uint8_t node_catalog_output_count(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry ? entry->output_count : 0U;
}

uint8_t node_catalog_pin_rows(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry ? entry->pin_rows : 1U;
}

int node_catalog_width(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry ? entry->width : 80;
}

const char *node_catalog_label(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry ? entry->label : NULL;
}

const char *node_catalog_name_prefix(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry ? entry->name_prefix : "NODE";
}

const char *node_catalog_operator_text(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry && entry->operator_text ? entry->operator_text : " ? ";
}

AppTool node_catalog_tool(NodeType type) {
    const NodeCatalogEntry *entry;

    entry = node_catalog_entry(type);
    return entry && entry->has_tool ? entry->tool : APP_TOOL_SELECT;
}

NodeType node_catalog_type_for_tool(AppTool tool) {
    size_t index;

    for (index = 0U; index < sizeof(node_catalog) / sizeof(node_catalog[0]); index++) {
        if (node_catalog[index].has_tool && node_catalog[index].tool == tool) {
            return node_catalog[index].type;
        }
    }

    return NODE_INVALID;
}

bool node_catalog_tool_places_node(AppTool tool) {
    return node_catalog_type_for_tool(tool) != NODE_INVALID;
}

bool node_catalog_has_waveform(NodeType type) {
    return type == NODE_INPUT || type == NODE_OUTPUT || type == NODE_GATE_CLOCK;
}

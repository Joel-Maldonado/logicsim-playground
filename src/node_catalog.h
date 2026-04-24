#ifndef NODE_CATALOG_H
#define NODE_CATALOG_H

#include <stdbool.h>
#include <stdint.h>
#include "app_types.h"

typedef struct {
    NodeType type;
    AppTool tool;
    const char *token;
    const char *label;
    const char *name_prefix;
    const char *operator_text;
    int width;
    uint8_t input_count;
    uint8_t output_count;
    uint8_t pin_rows;
    bool has_tool;
} NodeCatalogEntry;

const NodeCatalogEntry *node_catalog_entry(NodeType type);
bool node_catalog_type_from_token(const char *token, NodeType *type);
uint8_t node_catalog_input_count(NodeType type);
uint8_t node_catalog_output_count(NodeType type);
uint8_t node_catalog_pin_rows(NodeType type);
int node_catalog_width(NodeType type);
const char *node_catalog_label(NodeType type);
const char *node_catalog_name_prefix(NodeType type);
const char *node_catalog_operator_text(NodeType type);
AppTool node_catalog_tool(NodeType type);
NodeType node_catalog_type_for_tool(AppTool tool);
bool node_catalog_tool_places_node(AppTool tool);
bool node_catalog_has_waveform(NodeType type);

#endif // NODE_CATALOG_H

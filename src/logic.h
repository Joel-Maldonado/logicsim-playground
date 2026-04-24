#ifndef LOGIC_H
#define LOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    LOGIC_LOW = 0,
    LOGIC_HIGH = 1,
    LOGIC_UNKNOWN = 2,
    LOGIC_ERROR = 3
} LogicValue;

typedef enum {
    NODE_INVALID = -1,
    NODE_INPUT,
    NODE_OUTPUT,
    NODE_GATE_AND,
    NODE_GATE_OR,
    NODE_GATE_NOT,
    NODE_GATE_XOR,
    NODE_GATE_NAND,
    NODE_GATE_NOR,
    NODE_GATE_DFF,
    NODE_GATE_LATCH,
    NODE_GATE_CLOCK
} NodeType;

#define MAX_PINS 8
#define MAX_NODES 1024
#define MAX_NETS 2048

#include "raylib.h"

typedef struct LogicNode LogicNode;
typedef struct LogicNet LogicNet;

typedef struct {
    LogicNode *node;
    LogicValue value;
    uint8_t index;
    uint8_t _padding[3];
} LogicPin;

struct LogicNet {
    LogicPin *source;
    LogicPin *sinks[MAX_PINS];
    uint8_t sink_count;
    uint8_t _padding[7];
};

struct LogicNode {
    char *name;
    LogicPin inputs[MAX_PINS];
    LogicPin outputs[MAX_PINS];
    Rectangle rect;
    Vector2 pos;
    NodeType type;
    LogicValue state;
    LogicValue prev_state;
    uint8_t input_count;
    uint8_t output_count;
    bool evaluated;
    bool state_changed;
    bool inputs_changed;
    uint8_t _padding[7];
};

typedef struct {
    LogicNode nodes[MAX_NODES];
    LogicNet nets[MAX_NETS];
    uint32_t node_count;
    uint32_t net_count;
} LogicGraph;

typedef struct {
    LogicNode *inputs[MAX_PINS];
    LogicNode *outputs[MAX_PINS];
    LogicValue *data; // Row-major: [row][input... output...]
    uint32_t row_count;
    uint8_t input_count;
    uint8_t output_count;
    uint8_t _padding[2];
} TruthTable;

// Core Logic Engine API
void logic_init_graph(LogicGraph *graph);
LogicNode* logic_add_node(LogicGraph *graph, NodeType type, const char *name);
LogicNet* logic_add_net(LogicGraph *graph);
bool logic_connect(LogicGraph *graph, LogicPin *src, LogicPin *sink);
bool logic_disconnect_sink(LogicGraph *graph, LogicPin *sink);
bool logic_remove_node(LogicGraph *graph, LogicNode *node);
bool logic_node_is_active(const LogicNode *node);
void logic_evaluate(LogicGraph *graph);
void logic_tick(LogicGraph *graph);
LogicValue logic_eval_gate(NodeType type, LogicValue inputs[], uint8_t count);

// Topological Sort
uint32_t logic_topological_sort(LogicGraph *graph, LogicNode **sorted_nodes);

// Truth Table API
TruthTable* logic_generate_truth_table(LogicGraph *graph);
void logic_free_truth_table(TruthTable *table);

// Expression API
char* logic_generate_expression(LogicGraph *graph, LogicNode *output_node);
bool logic_format_equation_symbolic(LogicGraph *graph, LogicNode *node, char *out, size_t len);
bool logic_format_equation_values(LogicGraph *graph, LogicNode *node, char *out, size_t len);
bool logic_format_equation_resolved(LogicGraph *graph, LogicNode *node, char *out, size_t len);

#endif // LOGIC_H

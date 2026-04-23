#ifndef APP_H
#define APP_H

#include "bool_solver.h"
#include "logic.h"

typedef enum {
    MODE_BUILD,
    MODE_COMPARE,
    MODE_SOLVER
} AppMode;

typedef enum {
    APP_TOOL_SELECT,
    APP_TOOL_INPUT,
    APP_TOOL_OUTPUT,
    APP_TOOL_AND,
    APP_TOOL_OR,
    APP_TOOL_NOT,
    APP_TOOL_XOR,
    APP_TOOL_CLOCK
} AppTool;

typedef enum {
    APP_PANEL_CANVAS,
    APP_PANEL_TRUTH_TABLE
} AppPanelFocus;

typedef enum {
    APP_COMPARE_NO_TARGET,
    APP_COMPARE_EQUIVALENT,
    APP_COMPARE_MISMATCH
} AppCompareStatus;

typedef enum {
    EDITOR_COMMAND_NONE,
    EDITOR_COMMAND_MODE_BUILD,
    EDITOR_COMMAND_MODE_COMPARE,
    EDITOR_COMMAND_MODE_SOLVER,
    EDITOR_COMMAND_TOOL_SELECT,
    EDITOR_COMMAND_TOOL_INPUT,
    EDITOR_COMMAND_TOOL_OUTPUT,
    EDITOR_COMMAND_TOOL_AND,
    EDITOR_COMMAND_TOOL_OR,
    EDITOR_COMMAND_TOOL_NOT,
    EDITOR_COMMAND_TOOL_XOR,
    EDITOR_COMMAND_TOOL_CLOCK,
    EDITOR_COMMAND_SIM_TOGGLE,
    EDITOR_COMMAND_SIM_STEP,
    EDITOR_COMMAND_SIM_RESET,
    EDITOR_COMMAND_CANCEL,
    EDITOR_COMMAND_DELETE_SELECTION,
    EDITOR_COMMAND_SELECT_NEXT_NODE,
    EDITOR_COMMAND_SELECT_PREVIOUS_NODE,
    EDITOR_COMMAND_MOVE_SELECTION_LEFT,
    EDITOR_COMMAND_MOVE_SELECTION_RIGHT,
    EDITOR_COMMAND_MOVE_SELECTION_UP,
    EDITOR_COMMAND_MOVE_SELECTION_DOWN,
    EDITOR_COMMAND_SELECT_PREVIOUS_ROW,
    EDITOR_COMMAND_SELECT_NEXT_ROW
} EditorCommand;

#define MAX_KMAP_GROUPS 8
#define APP_SOURCE_PATH_MAX 512
#define APP_STATUS_MESSAGE_MAX 128
#define APP_CANVAS_MIN_ZOOM 0.4f
#define APP_CANVAS_MAX_ZOOM 3.0f

typedef struct {
    char term[16];
    Color color;
    uint8_t cell_mask;
    uint8_t _padding[3];
} KMapGroup;

#define WAVEFORM_SAMPLES 100
#define APP_PENDING_COMMANDS 32

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

typedef struct {
    Vector2 origin;
    float zoom;
    LogicNode *drag_node;
    Vector2 drag_offset;
} AppCanvasState;

typedef struct {
    LogicNode *selected_node;
    LogicValue live_output;
    uint32_t live_row_index;
    bool row_valid;
    bool output_valid;
    uint8_t _padding[6];
} ViewContext;

typedef struct {
    LogicNode *selected_node;
    LogicPin *selected_wire_sink;
    AppPanelFocus focused_panel;
    uint32_t selected_row;
    ViewContext view;
} AppSelectionState;

typedef struct {
    LogicValue waveforms[MAX_NODES][WAVEFORM_SAMPLES];
    double last_tick_time;
    float speed;
    uint32_t waveform_index;
    bool active;
    uint8_t _padding[3];
} AppSimulationState;

typedef struct {
    TruthTable *truth_table;
    char *expression;
    char *simplified_expression;
    KMapGroup kmap_groups[MAX_KMAP_GROUPS];
    uint8_t kmap_group_count;
    uint8_t _padding[7];
} AppAnalysisState;

typedef struct {
    LogicGraph *target_graph;
    LogicNode *divergence_node;
    AppCompareStatus status;
    bool equivalent;
    uint8_t _padding[3];
    uint32_t first_failing_row;
} AppComparisonState;

typedef struct {
    char path[APP_SOURCE_PATH_MAX];
    char status[APP_STATUS_MESSAGE_MAX];
    bool live_reload;
    uint8_t _padding[7];
} AppSourceState;

typedef struct {
    LogicPin *active_pin;
    LogicPin *wire_drag_pin;
    LogicPin *wire_hover_pin;
    bool wiring_active;
    bool wire_drag_active;
    bool wire_drag_replacing_sink;
    uint8_t _padding[5];
    Vector2 wire_drag_pos;
} AppInteractionState;

typedef struct {
    EditorCommand pending_commands[APP_PENDING_COMMANDS];
    uint8_t count;
} AppCommandQueue;

typedef struct {
    char input[BOOL_SOLVER_INPUT_MAX + 1U];
    BoolSolverResult result;
    float steps_scroll;
    bool input_focused;
    uint8_t _padding[3];
} AppSolverState;

typedef struct {
    LogicGraph graph;
    AppMode mode;
    AppTool active_tool;
    AppCanvasState canvas;
    AppSelectionState selection;
    AppSimulationState simulation;
    AppAnalysisState analysis;
    AppComparisonState comparison;
    AppSourceState source;
    AppInteractionState interaction;
    AppCommandQueue commands;
    AppSolverState solver;
} AppContext;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

void app_init(AppContext *app);
void app_update_logic(AppContext *app);
LogicNode* app_add_node(AppContext *app, NodeType type, Vector2 pos);
LogicNode* app_add_named_node(AppContext *app, NodeType type, const char *name, Vector2 pos);
void app_set_mode(AppContext *app, AppMode mode);
void app_set_tool(AppContext *app, AppTool tool);
void app_set_panel_focus(AppContext *app, AppPanelFocus panel);
void app_select_row(AppContext *app, uint32_t row_index);
void app_clear_graph(AppContext *app);
void app_set_source_path(AppContext *app, const char *path);
void app_set_source_status(AppContext *app, const char *status);
void app_update_solver(AppContext *app);
void app_solver_set_input(AppContext *app, const char *input);
void app_solver_insert_char(AppContext *app, int codepoint);
void app_solver_backspace(AppContext *app);

#endif // APP_H

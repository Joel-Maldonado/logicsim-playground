#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <stddef.h>
#include "draw_util.h"
#include "node_catalog.h"
#include "ui_geometry.h"
#include "ui.h"

#define UI_TOOLBOX_ITEM_COUNT 7

static const Color UI_SELECT_VIOLET = { 200, 170, 255, 255 };

static const float CONTEXT_PANEL_PADDING = 12.0f;
static const float CONTEXT_PANEL_GAP = 10.0f;
static const float CONTEXT_STATUS_HEIGHT = 62.0f;
static const float CONTEXT_COMPARE_HEIGHT = 76.0f;
static const float CONTEXT_EQUATION_HEIGHT = 90.0f;
static const float CONTEXT_TRUTH_MIN_HEIGHT = 120.0f;
static const float CONTEXT_WHY_MIN_HEIGHT = 80.0f;
static const float CONTEXT_WHY_FLOOR_HEIGHT = 48.0f;
static const float CONTEXT_KMAP_HEIGHT = 140.0f;
static const float TRUTH_TABLE_HEADER_Y = 38.0f;
static const float TRUTH_TABLE_BODY_Y = 62.0f;
static const float TRUTH_TABLE_BOTTOM_PADDING = 10.0f;
static const float TRUTH_TABLE_ROW_HEIGHT = 22.0f;
static const float TRUTH_TABLE_ROW_RECT_Y_OFFSET = -3.0f;
static const float TRUTH_TABLE_ROW_RECT_HEIGHT = 20.0f;
static const float TRUTH_TABLE_ROW_X_PADDING = 10.0f;
static const float TRUTH_TABLE_COLUMN_X_PADDING = 16.0f;
static const float TRUTH_TABLE_VALUE_X_PADDING = 20.0f;
static const float WAVEFORM_HEADER_HEIGHT = 40.0f;
static const float WAVEFORM_BOTTOM_PADDING = 12.0f;
static const float WAVEFORM_ROW_HEIGHT = 40.0f;
static const float SOLVER_PANEL_PADDING = 24.0f;
static const float SOLVER_SECTION_GAP = 14.0f;
static const float SOLVER_INPUT_HEIGHT = 76.0f;
static const float SOLVER_PREVIEW_HEIGHT = 92.0f;
static const float SOLVER_RESULT_HEIGHT = 98.0f;
static const float SOLVER_STEP_HEIGHT = 88.0f;
static const float SOLVER_STEPS_HEADER_HEIGHT = 48.0f;

static inline Rectangle ui_make_rect(float x, float y, float width, float height) {
    if (width < 0.0f) {
        width = 0.0f;
    }
    if (height < 0.0f) {
        height = 0.0f;
    }

    return (Rectangle){ x, y, width, height };
}

static inline Color ui_logic_color(LogicValue value) {
    if (value == LOGIC_HIGH) {
        return (Color){ 245, 185, 50, 255 };
    }
    if (value == LOGIC_LOW) {
        return (Color){ 110, 110, 110, 255 };
    }
    if (value == LOGIC_ERROR) {
        return (Color){ 220, 60, 60, 255 };
    }
    return (Color){ 140, 140, 160, 255 };
}

static inline const char *ui_node_label(NodeType type) {
    return node_catalog_label(type);
}

static inline bool ui_node_has_waveform(const LogicNode *node) {
    if (!logic_node_is_active(node)) {
        return false;
    }

    return node_catalog_has_waveform(node->type);
}

static inline uint32_t ui_waveform_visible_rows(Rectangle panel) {
    float available_height;

    available_height = panel.height - WAVEFORM_HEADER_HEIGHT - WAVEFORM_BOTTOM_PADDING;
    if (available_height <= 0.0f) {
        return 0U;
    }

    return (uint32_t)(available_height / WAVEFORM_ROW_HEIGHT);
}

void ui_build_selection_label(const AppContext *app, char *buffer, size_t buffer_size);
void ui_toolbox_metrics(Rectangle panel, float *outer_margin, float *item_gap, float *item_height);
bool ui_truth_table_row_rect_in_panel(const AppContext *app, Rectangle panel, uint32_t row_index, Rectangle *row_rect);
uint32_t ui_truth_table_visible_rows_in_panel(const AppContext *app, Rectangle panel);

#endif // UI_INTERNAL_H

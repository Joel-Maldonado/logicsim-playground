#include "ui_internal.h"
#include "app_canvas.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void ui_build_selection_label(const AppContext *app, char *buffer, size_t buffer_size) {
    const LogicNode *node;

    if (!app || !buffer || buffer_size == 0U) {
        return;
    }

    if (app->selection.selected_wire_sink &&
        app->selection.selected_wire_sink->node &&
        app->selection.selected_wire_sink->node->type != (NodeType)-1) {
        node = app->selection.selected_wire_sink->node;
        snprintf(
            buffer,
            buffer_size,
            "Wire -> %s.in%u",
            node->name ? node->name : "Node",
            app->selection.selected_wire_sink->index
        );
        return;
    }

    if (app->selection.selected_node && app->selection.selected_node->type != (NodeType)-1) {
        snprintf(
            buffer,
            buffer_size,
            "%s",
            app->selection.selected_node->name ? app->selection.selected_node->name : "Node"
        );
        return;
    }

    snprintf(buffer, buffer_size, "None");
}

void ui_toolbox_metrics(Rectangle panel, float *outer_margin, float *item_gap, float *item_height) {
    float margin;
    float gap;
    float height;
    float available_height;
    float desired_total;
    float scale;

    margin = panel.width * 0.07f;
    if (margin < 8.0f) {
        margin = 8.0f;
    } else if (margin > 14.0f) {
        margin = 14.0f;
    }

    height = panel.width * 0.30f;
    if (height < 44.0f) {
        height = 44.0f;
    } else if (height > 58.0f) {
        height = 58.0f;
    }

    gap = height * 0.16f;
    if (gap < 6.0f) {
        gap = 6.0f;
    } else if (gap > 10.0f) {
        gap = 10.0f;
    }

    available_height = panel.height - (margin * 2.0f);
    if (available_height < 0.0f) {
        available_height = 0.0f;
    }

    desired_total = (height * (float)UI_TOOLBOX_ITEM_COUNT) + (gap * (float)(UI_TOOLBOX_ITEM_COUNT - 1));
    if (desired_total > available_height && desired_total > 0.0f) {
        scale = available_height / desired_total;
        height *= scale;
        gap *= scale;
    }

    if (outer_margin) {
        *outer_margin = margin;
    }
    if (item_gap) {
        *item_gap = gap;
    }
    if (item_height) {
        *item_height = height;
    }
}

bool ui_truth_table_row_rect_in_panel(const AppContext *app, Rectangle panel, uint32_t row_index, Rectangle *row_rect) {
    Rectangle rect;
    float max_y;

    if (!app || !app->analysis.truth_table || row_index >= app->analysis.truth_table->row_count) {
        return false;
    }

    rect = ui_make_rect(
        panel.x + TRUTH_TABLE_ROW_X_PADDING,
        panel.y + TRUTH_TABLE_BODY_Y + ((float)row_index * TRUTH_TABLE_ROW_HEIGHT) + TRUTH_TABLE_ROW_RECT_Y_OFFSET,
        panel.width - (TRUTH_TABLE_ROW_X_PADDING * 2.0f),
        TRUTH_TABLE_ROW_RECT_HEIGHT
    );
    max_y = panel.y + panel.height - TRUTH_TABLE_BOTTOM_PADDING;
    if (rect.y + rect.height > max_y) {
        return false;
    }

    if (row_rect) {
        *row_rect = rect;
    }

    return true;
}

uint32_t ui_truth_table_visible_rows_in_panel(const AppContext *app, Rectangle panel) {
    uint32_t row_index;

    if (!app || !app->analysis.truth_table) {
        return 0U;
    }

    for (row_index = 0U; row_index < app->analysis.truth_table->row_count; row_index++) {
        if (!ui_truth_table_row_rect_in_panel(app, panel, row_index, NULL)) {
            return row_index;
        }
    }

    return app->analysis.truth_table->row_count;
}

bool ui_toolbox_item_rect(Rectangle panel, int slot, Rectangle *item_rect) {
    float outer_margin;
    float item_gap;
    float item_height;

    if (slot < 0 || slot >= UI_TOOLBOX_ITEM_COUNT) {
        return false;
    }

    ui_toolbox_metrics(panel, &outer_margin, &item_gap, &item_height);
    if (item_height <= 0.0f) {
        return false;
    }

    if (item_rect) {
        *item_rect = ui_make_rect(
            panel.x + outer_margin,
            panel.y + outer_margin + ((float)slot * (item_height + item_gap)),
            panel.width - (outer_margin * 2.0f),
            item_height
        );
    }

    return true;
}

int ui_toolbox_slot_at(Rectangle panel, Vector2 mouse_pos) {
    int slot;

    for (slot = 0; slot < UI_TOOLBOX_ITEM_COUNT; slot++) {
        Rectangle item_rect;

        if (!ui_toolbox_item_rect(panel, slot, &item_rect)) {
            continue;
        }
        if (CheckCollisionPointRec(mouse_pos, item_rect)) {
            return slot;
        }
    }

    return -1;
}

UiSolverLayout ui_measure_solver_workspace(Rectangle panel) {
    UiSolverLayout layout;
    float content_x;
    float content_y;
    float content_width;
    float bottom;

    memset(&layout, 0, sizeof(layout));
    layout.panel_rect = panel;

    content_x = panel.x + SOLVER_PANEL_PADDING;
    content_y = panel.y + SOLVER_PANEL_PADDING + 34.0f;
    content_width = panel.width - (SOLVER_PANEL_PADDING * 2.0f);
    if (content_width < 0.0f) {
        content_width = 0.0f;
    }
    bottom = panel.y + panel.height - SOLVER_PANEL_PADDING;

    layout.input_rect = ui_make_rect(content_x, content_y, content_width, SOLVER_INPUT_HEIGHT);
    content_y += SOLVER_INPUT_HEIGHT + SOLVER_SECTION_GAP;
    layout.preview_rect = ui_make_rect(content_x, content_y, content_width, SOLVER_PREVIEW_HEIGHT);
    content_y += SOLVER_PREVIEW_HEIGHT + SOLVER_SECTION_GAP;
    layout.result_rect = ui_make_rect(content_x, content_y, content_width, SOLVER_RESULT_HEIGHT);
    content_y += SOLVER_RESULT_HEIGHT + SOLVER_SECTION_GAP;
    layout.steps_rect = ui_make_rect(content_x, content_y, content_width, bottom - content_y);
    return layout;
}

float ui_solver_steps_content_height(const AppContext *app) {
    uint8_t step_count;

    step_count = 1U;
    if (app && app->solver.result.ok && app->solver.result.step_count > 0U) {
        step_count = app->solver.result.step_count;
    }

    return SOLVER_STEPS_HEADER_HEIGHT + ((float)step_count * SOLVER_STEP_HEIGHT) + 16.0f;
}

UiContextPanelLayout ui_measure_context_panel(const AppContext *app, Rectangle panel) {
    UiContextPanelLayout layout;
    float bottom;
    float cursor_y;
    float remaining_height;
    float section_width;
    float kmap_total_height;
    float why_height;
    float truth_height;

    memset(&layout, 0, sizeof(layout));
    layout.panel_rect = panel;
    layout.show_compare = app && app->mode == MODE_COMPARE;
    layout.show_kmap = app && app->analysis.truth_table && app->analysis.truth_table->input_count == 2;

    section_width = panel.width - (CONTEXT_PANEL_PADDING * 2.0f);
    if (section_width < 0.0f) {
        section_width = 0.0f;
    }

    cursor_y = panel.y + CONTEXT_PANEL_PADDING;
    bottom = panel.y + panel.height - CONTEXT_PANEL_PADDING;

    layout.status_rect = ui_make_rect(panel.x + CONTEXT_PANEL_PADDING, cursor_y, section_width, CONTEXT_STATUS_HEIGHT);
    cursor_y += CONTEXT_STATUS_HEIGHT + CONTEXT_PANEL_GAP;

    if (layout.show_compare) {
        layout.compare_rect = ui_make_rect(panel.x + CONTEXT_PANEL_PADDING, cursor_y, section_width, CONTEXT_COMPARE_HEIGHT);
        cursor_y += CONTEXT_COMPARE_HEIGHT + CONTEXT_PANEL_GAP;
    }

    layout.equation_rect = ui_make_rect(panel.x + CONTEXT_PANEL_PADDING, cursor_y, section_width, CONTEXT_EQUATION_HEIGHT);
    cursor_y += CONTEXT_EQUATION_HEIGHT + CONTEXT_PANEL_GAP;

    remaining_height = bottom - cursor_y;
    if (remaining_height < 0.0f) {
        remaining_height = 0.0f;
    }

    kmap_total_height = layout.show_kmap ? (CONTEXT_KMAP_HEIGHT + CONTEXT_PANEL_GAP) : 0.0f;
    why_height = CONTEXT_WHY_MIN_HEIGHT;
    truth_height = remaining_height - kmap_total_height - CONTEXT_PANEL_GAP - why_height;

    if (truth_height < CONTEXT_TRUTH_MIN_HEIGHT) {
        why_height -= CONTEXT_TRUTH_MIN_HEIGHT - truth_height;
        if (why_height < CONTEXT_WHY_FLOOR_HEIGHT) {
            why_height = CONTEXT_WHY_FLOOR_HEIGHT;
        }
        truth_height = remaining_height - kmap_total_height - CONTEXT_PANEL_GAP - why_height;
    }

    if (truth_height < 0.0f) {
        truth_height = 0.0f;
        why_height = remaining_height - kmap_total_height - CONTEXT_PANEL_GAP;
        if (why_height < 0.0f) {
            why_height = 0.0f;
        }
    }

    layout.truth_table_rect = ui_make_rect(panel.x + CONTEXT_PANEL_PADDING, cursor_y, section_width, truth_height);
    cursor_y += layout.truth_table_rect.height + CONTEXT_PANEL_GAP;

    if (layout.show_kmap) {
        layout.kmap_rect = ui_make_rect(panel.x + CONTEXT_PANEL_PADDING, cursor_y, section_width, CONTEXT_KMAP_HEIGHT);
        cursor_y += CONTEXT_KMAP_HEIGHT + CONTEXT_PANEL_GAP;
    }

    layout.why_rect = ui_make_rect(panel.x + CONTEXT_PANEL_PADDING, cursor_y, section_width, bottom - cursor_y);
    layout.visible_truth_rows = ui_truth_table_visible_rows_in_panel(app, layout.truth_table_rect);
    if (app && app->analysis.truth_table && app->analysis.truth_table->row_count > layout.visible_truth_rows) {
        layout.hidden_truth_rows = app->analysis.truth_table->row_count - layout.visible_truth_rows;
    }

    return layout;
}

bool ui_context_truth_table_row_rect(const AppContext *app, const UiContextPanelLayout *layout, uint32_t row_index, Rectangle *row_rect) {
    if (!layout) {
        return false;
    }

    return ui_truth_table_row_rect_in_panel(app, layout->truth_table_rect, row_index, row_rect);
}

LogicPin* ui_get_wire_at(AppContext *app, Rectangle canvas, Vector2 mouse_pos) {
    LogicGraph *graph;
    LogicPin *best_sink;
    float best_distance;
    float hit_radius;
    uint32_t i;
    Vector2 world_pos;

    graph = &app->graph;
    best_sink = NULL;
    world_pos = app_canvas_screen_to_world(app, canvas, mouse_pos);
    hit_radius = 7.0f / app_canvas_clamp_zoom(app->canvas.zoom);
    best_distance = hit_radius;

    for (i = 0; i < graph->net_count; i++) {
        LogicNet *net;
        uint8_t sink_index;

        net = &graph->nets[i];
        if (!net->source || !net->source->node || net->source->node->type == (NodeType)-1) {
            continue;
        }
        for (sink_index = 0; sink_index < net->sink_count; sink_index++) {
            LogicPin *sink_pin;
            UiWirePath path;
            float distance;

            sink_pin = net->sinks[sink_index];
            if (!sink_pin || !sink_pin->node || sink_pin->node->type == (NodeType)-1) {
                continue;
            }

            path = ui_orthogonal_wire_path(ui_pin_position(net->source), ui_input_pin_position(sink_pin));
            distance = ui_point_to_wire_distance(world_pos, path);
            if (distance < best_distance) {
                best_distance = distance;
                best_sink = sink_pin;
            }
        }
    }

    return best_sink;
}

LogicPin* ui_get_pin_at(AppContext *app, Rectangle canvas, Vector2 mouse_pos) {
    LogicGraph *graph;
    float hit_radius;
    uint32_t i;
    Vector2 world_pos;

    graph = &app->graph;
    world_pos = app_canvas_screen_to_world(app, canvas, mouse_pos);
    hit_radius = 18.0f / app_canvas_clamp_zoom(app->canvas.zoom);
    for (i = 0; i < graph->node_count; i++) {
        LogicNode *node;
        uint8_t pin_index;

        node = &graph->nodes[i];
        if (node->type == (NodeType)-1) {
            continue;
        }

        for (pin_index = 0; pin_index < node->input_count; pin_index++) {
            Vector2 pin_pos;

            pin_pos = ui_input_pin_position(&node->inputs[pin_index]);
            if (CheckCollisionPointCircle(world_pos, pin_pos, hit_radius)) {
                return &node->inputs[pin_index];
            }
        }

        for (pin_index = 0; pin_index < node->output_count; pin_index++) {
            Vector2 pin_pos;

            pin_pos = ui_output_pin_position(&node->outputs[pin_index]);
            if (CheckCollisionPointCircle(world_pos, pin_pos, hit_radius)) {
                return &node->outputs[pin_index];
            }
        }
    }

    return NULL;
}

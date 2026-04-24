#include "ui_internal.h"
#include "app_canvas.h"
#include "app_commands.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static float grid_start(float value, float step) {
    float snapped;

    snapped = (float)((int)(value / step)) * step;
    if (snapped > value) {
        snapped -= step;
    }

    return snapped;
}

static int fit_text_font_size(const char *text, float max_width, int preferred_size, int min_size) {
    int font_size;

    if (!text || max_width <= 0.0f) {
        return min_size;
    }

    font_size = preferred_size;
    while (font_size > min_size && text_width(text, font_size) > max_width) {
        font_size--;
    }

    return font_size;
}

static void draw_circle_at(float x, float y, float radius, Color color) {
    DrawCircle(pixel(x), pixel(y), radius, color);
}

static void draw_circle_lines_at(float x, float y, float radius, Color color) {
    DrawCircleLines(pixel(x), pixel(y), radius, color);
}

static void draw_glow_circle(Vector2 center, float radius, Color color) {
    Color outer;
    Color inner;

    outer = color;
    outer.a = 42;
    inner = color;
    inner.a = 120;
    DrawCircleV(center, radius + 6.0f, outer);
    DrawCircleV(center, radius + 2.0f, inner);
}

static const char *toolbox_shortcut_label(AppTool tool) {
    switch (tool) {
        case APP_TOOL_INPUT:
            return "1";
        case APP_TOOL_OUTPUT:
            return "2";
        case APP_TOOL_AND:
            return "3";
        case APP_TOOL_OR:
            return "4";
        case APP_TOOL_NOT:
            return "5";
        case APP_TOOL_XOR:
            return "6";
        case APP_TOOL_CLOCK:
            return "7";
        case APP_TOOL_SELECT:
            return "V";
        default:
            return "V";
    }
}

static void draw_arc_lines(Vector2 center, float radius, float start_angle, float end_angle, int segments, float thick, Color color) {
    float step;
    int i;

    if (segments < 2) {
        segments = 2;
    }

    step = (end_angle - start_angle) / (float)segments;
    for (i = 0; i < segments; i++) {
        float angle_1;
        float angle_2;
        Vector2 point_1;
        Vector2 point_2;

        angle_1 = (start_angle + ((float)i * step)) * DEG2RAD;
        angle_2 = (start_angle + ((float)(i + 1) * step)) * DEG2RAD;
        point_1 = (Vector2){ center.x + (cosf(angle_1) * radius), center.y + (sinf(angle_1) * radius) };
        point_2 = (Vector2){ center.x + (cosf(angle_2) * radius), center.y + (sinf(angle_2) * radius) };
        DrawLineEx(point_1, point_2, thick, color);
    }
}

static void draw_orthogonal_wire(Vector2 start, Vector2 end, Color color, float thick) {
    Vector2 mid_1;
    Vector2 mid_2;

    mid_1 = (Vector2){ (start.x + end.x) / 2.0f, start.y };
    mid_2 = (Vector2){ (start.x + end.x) / 2.0f, end.y };

    DrawLineEx(start, mid_1, thick, color);
    DrawLineEx(mid_1, mid_2, thick, color);
    DrawLineEx(mid_2, end, thick, color);
}

static void draw_panel_shell(Rectangle panel, const char *title, bool focused) {
    DrawRectangleRounded(panel, 0.14f, 10, (Color){ 27, 27, 27, 255 });
    DrawRectangleRoundedLinesEx(
        panel,
        0.14f,
        10,
        1.0f,
        focused ? (Color){ 0, 191, 255, 140 } : (Color){ 54, 54, 54, 255 }
    );
    draw_text_at(title, panel.x + 14.0f, panel.y + 14.0f, 11, GRAY);
}

static void draw_clock_glyph(Rectangle rect, LogicValue value) {
    float cx;
    float cy;
    float glyph_w;
    float glyph_h;
    float stroke;
    float left;
    float right;
    float top;
    float bottom;
    float step;
    float x0;
    float x1;
    float x2;
    float x3;
    float x4;
    float led_radius;
    float led_x;
    float led_y;
    Color glyph_color;
    Color led_color;
    Color led_ring;
    Vector2 points[10];
    int i;

    cx = rect.x + (rect.width / 2.0f);
    cy = rect.y + (rect.height * 0.55f);
    glyph_w = rect.width * 0.62f;
    glyph_h = rect.height * 0.40f;
    stroke = rect.height * 0.07f;
    if (stroke < 1.75f) {
        stroke = 1.75f;
    }
    if (stroke > 3.5f) {
        stroke = 3.5f;
    }

    left = cx - (glyph_w / 2.0f);
    right = cx + (glyph_w / 2.0f);
    top = cy - (glyph_h / 2.0f);
    bottom = cy + (glyph_h / 2.0f);

    step = glyph_w / 4.0f;
    x0 = left;
    x1 = x0 + (step * 0.5f);
    x2 = x0 + (step * 1.5f);
    x3 = x0 + (step * 2.5f);
    x4 = x0 + (step * 3.5f);

    if (value == LOGIC_HIGH) {
        glyph_color = (Color){ 245, 185, 50, 245 };
    } else if (value == LOGIC_LOW) {
        glyph_color = (Color){ 180, 185, 200, 230 };
    } else {
        glyph_color = (Color){ 205, 210, 225, 220 };
    }

    points[0] = (Vector2){ x0, bottom };
    points[1] = (Vector2){ x1, bottom };
    points[2] = (Vector2){ x1, top };
    points[3] = (Vector2){ x2, top };
    points[4] = (Vector2){ x2, bottom };
    points[5] = (Vector2){ x3, bottom };
    points[6] = (Vector2){ x3, top };
    points[7] = (Vector2){ x4, top };
    points[8] = (Vector2){ x4, bottom };
    points[9] = (Vector2){ right, bottom };

    for (i = 0; i < 9; i++) {
        DrawLineEx(points[i], points[i + 1], stroke, glyph_color);
    }
    for (i = 1; i < 9; i++) {
        draw_circle_at(points[i].x, points[i].y, stroke / 2.0f, glyph_color);
    }

    led_radius = rect.height * 0.065f;
    if (led_radius < 2.0f) {
        led_radius = 2.0f;
    }
    led_x = rect.x + rect.width - (led_radius * 2.4f);
    led_y = rect.y + (led_radius * 2.4f);
    if (value == LOGIC_HIGH) {
        led_color = (Color){ 245, 185, 50, 255 };
        led_ring = (Color){ 255, 220, 120, 255 };
    } else if (value == LOGIC_LOW) {
        led_color = (Color){ 70, 70, 80, 255 };
        led_ring = (Color){ 120, 120, 135, 200 };
    } else {
        led_color = (Color){ 90, 90, 105, 255 };
        led_ring = (Color){ 140, 140, 160, 200 };
    }
    draw_circle_at(led_x, led_y, led_radius, led_color);
    draw_circle_lines_at(led_x, led_y, led_radius, led_ring);
}

static void draw_gate_symbol(NodeType type, Rectangle rect, Color color, Color border_color, float border_thick) {
    float x;
    float y;
    float w;
    float h;

    x = rect.x;
    y = rect.y;
    w = rect.width;
    h = rect.height;

    if (type == NODE_GATE_AND) {
        DrawRectangleRec((Rectangle){ x, y, w / 2.0f, h }, color);
        DrawCircleSector((Vector2){ x + (w / 2.0f), y + (h / 2.0f) }, h / 2.0f, -90.0f, 90.0f, 20, color);
        DrawLineEx((Vector2){ x, y }, (Vector2){ x + (w / 2.0f), y }, border_thick, border_color);
        DrawLineEx((Vector2){ x, y + h }, (Vector2){ x + (w / 2.0f), y + h }, border_thick, border_color);
        DrawLineEx((Vector2){ x, y }, (Vector2){ x, y + h }, border_thick, border_color);
        draw_arc_lines((Vector2){ x + (w / 2.0f), y + (h / 2.0f) }, h / 2.0f, -90.0f, 90.0f, 20, border_thick, border_color);
        return;
    }

    if (type == NODE_GATE_OR || type == NODE_GATE_XOR) {
        Vector2 input_top;
        Vector2 input_bottom;
        Vector2 output;
        Vector2 inner_mid;
        int segments;
        int i;

        input_top = (Vector2){ x, y };
        input_bottom = (Vector2){ x, y + h };
        output = (Vector2){ x + w, y + (h / 2.0f) };
        inner_mid = (Vector2){ x + (w * 0.25f), y + (h / 2.0f) };
        segments = 20;

        for (i = 0; i < segments; i++) {
            float t0;
            float t1;
            Vector2 top_a;
            Vector2 top_b;
            Vector2 bottom_a;
            Vector2 bottom_b;
            Vector2 inner_a;
            Vector2 inner_b;

            t0 = (float)i / (float)segments;
            t1 = (float)(i + 1) / (float)segments;
            top_a = GetSplinePointBezierQuad(input_top, (Vector2){ x + (w * 0.4f), y }, output, t0);
            top_b = GetSplinePointBezierQuad(input_top, (Vector2){ x + (w * 0.4f), y }, output, t1);
            bottom_a = GetSplinePointBezierQuad(input_bottom, (Vector2){ x + (w * 0.4f), y + h }, output, t0);
            bottom_b = GetSplinePointBezierQuad(input_bottom, (Vector2){ x + (w * 0.4f), y + h }, output, t1);
            inner_a = GetSplinePointBezierQuad(input_top, inner_mid, input_bottom, t0);
            inner_b = GetSplinePointBezierQuad(input_top, inner_mid, input_bottom, t1);

            DrawTriangle(output, inner_a, inner_b, color);
            DrawTriangle(output, inner_b, inner_a, color);
            DrawTriangle(input_top, top_a, top_b, color);
            DrawTriangle(input_top, top_b, top_a, color);
            DrawTriangle(input_bottom, bottom_a, bottom_b, color);
            DrawTriangle(input_bottom, bottom_b, bottom_a, color);
        }

        if (border_thick > 0.0f) {
            DrawSplineSegmentBezierQuadratic(input_top, (Vector2){ x + (w * 0.4f), y }, output, border_thick, border_color);
            DrawSplineSegmentBezierQuadratic(input_bottom, (Vector2){ x + (w * 0.4f), y + h }, output, border_thick, border_color);
            DrawSplineSegmentBezierQuadratic(input_top, inner_mid, input_bottom, border_thick, border_color);

            if (type == NODE_GATE_XOR) {
                Vector2 xor_1;
                Vector2 xor_2;
                Vector2 xor_3;

                xor_1 = (Vector2){ x - 6.0f, y };
                xor_2 = (Vector2){ x + (w * 0.25f) - 6.0f, y + (h / 2.0f) };
                xor_3 = (Vector2){ x - 6.0f, y + h };
                DrawSplineSegmentBezierQuadratic(xor_1, xor_2, xor_3, border_thick, border_color);
            }
        }
        return;
    }

    if (type == NODE_GATE_NOT) {
        float bubble_radius;
        float tip_x;
        Vector2 point_1;
        Vector2 point_2;
        Vector2 point_3;

        bubble_radius = h * 0.15f;
        tip_x = x + w - (bubble_radius * 2.0f);
        point_1 = (Vector2){ x, y };
        point_2 = (Vector2){ tip_x, y + (h / 2.0f) };
        point_3 = (Vector2){ x, y + h };
        DrawTriangle(point_1, point_3, point_2, color);
        DrawTriangleLines(point_1, point_3, point_2, border_color);
        draw_circle_at(tip_x + bubble_radius, y + (h / 2.0f), bubble_radius, color);
        draw_circle_lines_at(tip_x + bubble_radius, y + (h / 2.0f), border_thick, border_color);
        return;
    }

    if (type == NODE_INPUT) {
        float tab_width;
        Vector2 tab[3];

        DrawRectangleRounded(rect, 0.35f, 10, color);
        DrawRectangleRoundedLinesEx(rect, 0.35f, 10, border_thick, border_color);
        tab_width = h * 0.25f;
        tab[0] = (Vector2){ x + w, y + (h * 0.35f) };
        tab[1] = (Vector2){ x + w, y + (h * 0.65f) };
        tab[2] = (Vector2){ x + w + tab_width, y + (h / 2.0f) };
        DrawTriangle(tab[0], tab[1], tab[2], color);
        DrawLineEx(tab[0], tab[2], border_thick, border_color);
        DrawLineEx(tab[1], tab[2], border_thick, border_color);
        return;
    }

    if (type == NODE_OUTPUT) {
        float notch_width;
        Vector2 notch[3];

        DrawRectangleRounded(rect, 0.35f, 10, color);
        DrawRectangleRoundedLinesEx(rect, 0.35f, 10, border_thick, border_color);
        notch_width = h * 0.25f;
        notch[0] = (Vector2){ x, y + (h * 0.35f) };
        notch[1] = (Vector2){ x, y + (h * 0.65f) };
        notch[2] = (Vector2){ x - notch_width, y + (h / 2.0f) };
        DrawTriangle(notch[0], notch[2], notch[1], color);
        DrawLineEx(notch[0], notch[2], border_thick, border_color);
        DrawLineEx(notch[1], notch[2], border_thick, border_color);
        return;
    }

    if (type == NODE_GATE_DFF) {
        Vector2 triangle[3];
        float font_scale;
        int label_size;

        DrawRectangleRounded(rect, 0.12f, 10, color);
        DrawRectangleRoundedLinesEx(rect, 0.12f, 10, border_thick, border_color);

        font_scale = h / 60.0f;
        label_size = (int)(11.0f * font_scale);
        if (label_size < 8) {
            label_size = 8;
        }
        draw_text_at("D", x + (w * 0.12f), y + (h * 0.18f), label_size, (Color){ 230, 230, 230, 220 });
        draw_text_at("Q", x + (w * 0.78f), y + (h * 0.18f), label_size, (Color){ 230, 230, 230, 220 });

        triangle[0] = (Vector2){ x, y + (h * 0.62f) };
        triangle[1] = (Vector2){ x + (h * 0.18f), y + (h * 0.75f) };
        triangle[2] = (Vector2){ x, y + (h * 0.88f) };
        DrawTriangle(triangle[0], triangle[2], triangle[1], (Color){ 180, 180, 200, 220 });
        DrawLineEx(triangle[0], triangle[1], border_thick, border_color);
        DrawLineEx(triangle[1], triangle[2], border_thick, border_color);
        return;
    }

    if (type == NODE_GATE_CLOCK) {
        DrawRectangleRounded(rect, 0.22f, 10, color);
        DrawRectangleRoundedLinesEx(rect, 0.22f, 10, border_thick, border_color);
        return;
    }

    DrawRectangleRounded(rect, 0.2f, 10, color);
    DrawRectangleRoundedLinesEx(rect, 0.2f, 10, border_thick, border_color);
}

void ui_draw_circuit(AppContext *app, Rectangle canvas) {
    static const float grid_size = 20.0f;
    Camera2D camera;
    LogicGraph *graph;
    Vector2 world_min;
    Vector2 world_max;
    uint32_t i;
    float x;
    float y;

    graph = &app->graph;
    camera = app_canvas_camera(app, canvas);
    world_min = app_canvas_screen_to_world(app, canvas, (Vector2){ canvas.x, canvas.y });
    world_max = app_canvas_screen_to_world(
        app,
        canvas,
        (Vector2){ canvas.x + canvas.width, canvas.y + canvas.height }
    );

    BeginMode2D(camera);

    for (x = grid_start(world_min.x, grid_size); x <= world_max.x + grid_size; x += grid_size) {
        draw_line_at(x, world_min.y, x, world_max.y, (Color){ 30, 30, 30, 255 });
    }
    for (y = grid_start(world_min.y, grid_size); y <= world_max.y + grid_size; y += grid_size) {
        draw_line_at(world_min.x, y, world_max.x, y, (Color){ 30, 30, 30, 255 });
    }

    for (i = 0; i < graph->net_count; i++) {
        LogicNet *net;
        Vector2 start;
        Color wire_color;
        uint8_t sink_index;

        net = &graph->nets[i];
        if (!net->source || !logic_node_is_active(net->source->node)) {
            continue;
        }

        start = ui_pin_position(net->source);
        wire_color = ui_logic_color(net->source->value);

        for (sink_index = 0; sink_index < net->sink_count; sink_index++) {
            LogicPin *sink_pin;
            Vector2 end;
            bool is_selected;

            sink_pin = net->sinks[sink_index];
            if (!logic_node_is_active(sink_pin->node)) {
                continue;
            }

            end = ui_input_pin_position(sink_pin);
            is_selected = app->selection.selected_wire_sink == sink_pin;
            if (is_selected) {
                draw_orthogonal_wire(start, end, UI_SELECT_VIOLET, 5.0f);
                draw_glow_circle(start, 6.0f, UI_SELECT_VIOLET);
                draw_glow_circle(end, 6.0f, UI_SELECT_VIOLET);
            } else {
                draw_orthogonal_wire(start, end, wire_color, 3.0f);
            }
        }
    }

    if (app->interaction.wire_drag_active && app->interaction.wire_drag_pin) {
        Vector2 start;
        Vector2 end;
        Color wire_color;
        bool valid_target;

        if (app->interaction.wire_drag_pin->node->output_count > 0) {
            start = ui_output_pin_position(app->interaction.wire_drag_pin);
        } else {
            start = ui_input_pin_position(app->interaction.wire_drag_pin);
        }

        end = app->interaction.wire_drag_pos;
        valid_target = false;
        if (app->interaction.wire_hover_pin) {
            valid_target =
                (app->interaction.wire_drag_pin->node->output_count > 0 && app->interaction.wire_hover_pin->node->input_count > 0) ||
                (app->interaction.wire_drag_pin->node->input_count > 0 && app->interaction.wire_hover_pin->node->output_count > 0);
            wire_color = valid_target ? UI_SELECT_VIOLET : (Color){ 200, 50, 50, 255 };
            end = ui_pin_position(app->interaction.wire_hover_pin);
        } else {
            wire_color = (Color){ 0, 191, 255, 220 };
        }

        draw_orthogonal_wire(start, end, wire_color, 3.0f);
        draw_glow_circle(start, 6.0f, UI_SELECT_VIOLET);
        if (app->interaction.wire_hover_pin) {
            draw_glow_circle(end, valid_target ? 7.0f : 6.0f, wire_color);
        }
    }

    for (i = 0; i < graph->node_count; i++) {
        LogicNode *node;
        Color background;
        Color border;
        bool selected;
        float border_thick;

        node = &graph->nodes[i];
        if (!logic_node_is_active(node)) {
            continue;
        }

        background = (node->type == NODE_INPUT || node->type == NODE_OUTPUT) ?
            (Color){ 56, 56, 56, 255 } :
            (Color){ 40, 40, 40, 255 };
        selected = app->selection.selected_node == node;
        border = selected ? UI_SELECT_VIOLET : (Color){ 100, 100, 100, 255 };
        border_thick = selected ? 3.0f : 2.0f;

        draw_gate_symbol(node->type, node->rect, background, border, border_thick);

        if (node->type == NODE_GATE_CLOCK) {
            LogicValue live = node->output_count > 0 ? node->outputs[0].value : LOGIC_UNKNOWN;
            draw_clock_glyph(node->rect, live);
        }

        if (node->type == NODE_INPUT || node->type == NODE_OUTPUT) {
            LogicValue live;
            const char *digit;
            int text_size;
            int digit_width;
            float text_x;
            float text_y;
            Color digit_color;

            if (node->type == NODE_OUTPUT && node->input_count > 0) {
                live = node->inputs[0].value;
            } else if (node->output_count > 0) {
                live = node->outputs[0].value;
            } else {
                live = LOGIC_UNKNOWN;
            }

            if (live == LOGIC_HIGH) {
                digit = "1";
            } else if (live == LOGIC_LOW) {
                digit = "0";
            } else if (live == LOGIC_ERROR) {
                digit = "E";
            } else {
                digit = "?";
            }

            text_size = 28;
            digit_width = MeasureText(digit, text_size);
            text_x = node->pos.x + ((node->rect.width - (float)digit_width) / 2.0f);
            text_y = node->pos.y + ((node->rect.height - (float)text_size) / 2.0f);
            digit_color = (live == LOGIC_HIGH || live == LOGIC_LOW)
                ? ui_logic_color(live)
                : (Color){ 200, 200, 200, 255 };
            draw_text_at(digit, text_x, text_y, text_size, digit_color);
        } else if (node->type != NODE_GATE_CLOCK) {
            const char *label;

            label = ui_node_label(node->type);
            if (label) {
                int text_size;
                int label_width;
                float text_x;
                float text_y;

                text_size = 12;
                label_width = MeasureText(label, text_size);
                text_x = node->pos.x + ((node->rect.width - (float)label_width) / 2.0f);
                text_y = node->pos.y + ((node->rect.height - (float)text_size) / 2.0f);
                draw_text_at(label, text_x, text_y, text_size, LIGHTGRAY);
            }
        }

        if (node->name) {
            draw_text_at(node->name, node->pos.x, node->pos.y - 15.0f, 10, GRAY);
        }

        {
            uint8_t pin_index;

            for (pin_index = 0; pin_index < node->input_count; pin_index++) {
                Vector2 pin_pos;
                bool is_hovered;
                Color outline;

                pin_pos = ui_input_pin_position(&node->inputs[pin_index]);
                is_hovered = app->interaction.wire_drag_pin == &node->inputs[pin_index] ||
                    app->interaction.wire_hover_pin == &node->inputs[pin_index];
                outline = (Color){ 30, 30, 30, 255 };
                if (app->interaction.wire_drag_pin == &node->inputs[pin_index]) {
                    draw_glow_circle(pin_pos, 6.0f, UI_SELECT_VIOLET);
                    outline = UI_SELECT_VIOLET;
                } else if (app->interaction.wire_hover_pin == &node->inputs[pin_index]) {
                    draw_glow_circle(
                        pin_pos,
                        app->interaction.wire_drag_replacing_sink ? 8.0f : 6.0f,
                        UI_SELECT_VIOLET
                    );
                    outline = UI_SELECT_VIOLET;
                }
                draw_circle_at(pin_pos.x, pin_pos.y, is_hovered ? 7.0f : 6.0f, ui_logic_color(node->inputs[pin_index].value));
                draw_circle_lines_at(pin_pos.x, pin_pos.y, is_hovered ? 7.0f : 6.0f, outline);
            }

            for (pin_index = 0; pin_index < node->output_count; pin_index++) {
                Vector2 pin_pos;
                bool is_hovered;
                Color outline;

                pin_pos = ui_output_pin_position(&node->outputs[pin_index]);
                is_hovered = app->interaction.wire_drag_pin == &node->outputs[pin_index] ||
                    app->interaction.wire_hover_pin == &node->outputs[pin_index];
                outline = (Color){ 30, 30, 30, 255 };
                if (app->interaction.wire_drag_pin == &node->outputs[pin_index]) {
                    draw_glow_circle(pin_pos, 6.0f, UI_SELECT_VIOLET);
                    outline = UI_SELECT_VIOLET;
                } else if (app->interaction.wire_hover_pin == &node->outputs[pin_index]) {
                    draw_glow_circle(pin_pos, 6.0f, (Color){ 200, 50, 50, 230 });
                    outline = (Color){ 200, 50, 50, 255 };
                }
                draw_circle_at(pin_pos.x, pin_pos.y, is_hovered ? 7.0f : 6.0f, ui_logic_color(node->outputs[pin_index].value));
                draw_circle_lines_at(pin_pos.x, pin_pos.y, is_hovered ? 7.0f : 6.0f, outline);
            }
        }
    }

    EndMode2D();

    if (app->interaction.wire_drag_active || app_tool_places_node(app->active_tool)) {
        Rectangle hint_rect;
        const char *line_1;
        const char *line_2;

        hint_rect = (Rectangle){ canvas.x + 20.0f, canvas.y + 20.0f, 388.0f, 58.0f };
        DrawRectangleRounded(hint_rect, 0.2f, 10, (Color){ 18, 18, 18, 228 });
        DrawRectangleRoundedLinesEx(hint_rect, 0.2f, 10, 1.0f, (Color){ 0, 191, 255, 110 });

        if (app->interaction.wire_drag_active) {
            line_1 = app->interaction.wire_drag_replacing_sink ? "Drag wire: replace existing sink" : "Drag wire";
            line_2 = "Release on a compatible pin to connect. Release on empty space or press Esc to cancel.";
        } else {
            line_1 = "Placement mode";
            line_2 = "Click the canvas to place the selected node on the grid.";
        }

        draw_text_at(line_1, hint_rect.x + 12.0f, hint_rect.y + 10.0f, 12, WHITE);
        draw_text_at(line_2, hint_rect.x + 12.0f, hint_rect.y + 28.0f, 11, LIGHTGRAY);
    }
}

void ui_draw_truth_table(AppContext *app, Rectangle panel) {
    uint32_t cols;
    uint32_t output_index;
    uint32_t row_index;
    float col_width;
    float header_width_limit;
    uint32_t visible_rows;
    uint32_t hidden_rows;
    int text_y;

    draw_panel_shell(panel, "TRUTH TABLE", app->selection.focused_panel == APP_PANEL_TRUTH_TABLE);

    if (!app->analysis.truth_table) {
        draw_text_at("Add inputs and outputs to generate a table.", panel.x + 14.0f, panel.y + 42.0f, 13, GRAY);
        return;
    }

    text_y = pixel(panel.y + TRUTH_TABLE_HEADER_Y);
    cols = (uint32_t)app->analysis.truth_table->input_count + (uint32_t)app->analysis.truth_table->output_count;
    col_width = panel.width - 32.0f;
    if (col_width < 0.0f) {
        col_width = 0.0f;
    }
    col_width /= (float)((cols > 0U) ? cols : 1U);
    header_width_limit = col_width - 18.0f;
    if (header_width_limit < 0.0f) {
        header_width_limit = 0.0f;
    }
    visible_rows = ui_truth_table_visible_rows_in_panel(app, panel);
    hidden_rows = app->analysis.truth_table->row_count - visible_rows;

    for (row_index = 0; row_index < app->analysis.truth_table->input_count; row_index++) {
        char label[64];

        text_fit_with_ellipsis(
            app->analysis.truth_table->inputs[row_index]->name,
            13,
            header_width_limit,
            label,
            sizeof(label)
        );
        draw_text_at(
            label,
            panel.x + TRUTH_TABLE_COLUMN_X_PADDING + ((float)row_index * col_width),
            (float)text_y,
            13,
            LIGHTGRAY
        );
    }
    for (output_index = 0; output_index < app->analysis.truth_table->output_count; output_index++) {
        char label[64];

        text_fit_with_ellipsis(
            app->analysis.truth_table->outputs[output_index]->name,
            13,
            header_width_limit,
            label,
            sizeof(label)
        );
        draw_text_at(
            label,
            panel.x + TRUTH_TABLE_COLUMN_X_PADDING + ((float)(app->analysis.truth_table->input_count + output_index) * col_width),
            (float)text_y,
            13,
            LIGHTGRAY
        );
    }

    text_y = pixel(panel.y + TRUTH_TABLE_BODY_Y);
    for (row_index = 0; row_index < visible_rows; row_index++) {
        Color row_color;
        int row_y;
        uint32_t col_index;
        bool is_live;
        bool is_selected;
        Rectangle row_rect;

        if (!ui_truth_table_row_rect_in_panel(app, panel, row_index, &row_rect)) {
            break;
        }

        is_live = app->selection.view.row_valid && app->selection.view.live_row_index == row_index;
        is_selected = app->selection.selected_row == row_index;
        row_y = text_y + (int)((float)row_index * TRUTH_TABLE_ROW_HEIGHT);

        if (is_live) {
            DrawRectangleRounded(row_rect, 0.2f, 10, (Color){ 245, 185, 50, 40 });
            draw_text_at(">", panel.x + 2.0f, (float)row_y, 14, (Color){ 245, 185, 50, 255 });
        }
        if (is_selected && !is_live) {
            DrawRectangleRoundedLinesEx(row_rect, 0.2f, 10, 1.0f, UI_SELECT_VIOLET);
        }

        if (is_live) {
            row_color = (Color){ 245, 185, 50, 255 };
        } else if (is_selected) {
            row_color = UI_SELECT_VIOLET;
        } else {
            row_color = WHITE;
        }

        for (col_index = 0; col_index < cols; col_index++) {
            const char *value_text;

            value_text = (app->analysis.truth_table->data[(row_index * cols) + col_index] == LOGIC_HIGH) ? "1" : "0";
            draw_text_at(
                value_text,
                panel.x + TRUTH_TABLE_VALUE_X_PADDING + ((float)col_index * col_width),
                (float)row_y,
                14,
                row_color
            );
        }
    }

    if (hidden_rows > 0U) {
        char overflow_text[32];

        snprintf(overflow_text, sizeof(overflow_text), "+%u more", hidden_rows);
        draw_text_at(overflow_text, panel.x + 14.0f, panel.y + panel.height - 18.0f, 10, GRAY);
    }
}

void ui_draw_expression(AppContext *app, Rectangle panel) {
    draw_text_at("RAW", panel.x, panel.y, 11, GRAY);

    if (!app->analysis.expression) {
        draw_text_at("No output expression yet.", panel.x, panel.y + 18.0f, 13, GRAY);
        return;
    }

    draw_text_at(app->analysis.expression, panel.x, panel.y + 18.0f, 13, LIGHTGRAY);
    if (app->analysis.simplified_expression) {
        draw_text_at("SIMPLIFIED", panel.x, panel.y + 42.0f, 11, GRAY);
        draw_text_at(app->analysis.simplified_expression, panel.x, panel.y + 60.0f, 15, UI_SELECT_VIOLET);
    }
}

void ui_draw_kmap(AppContext *app, Rectangle panel) {
    uint32_t cols;
    int size;
    int origin_x;
    int origin_y;
    int b;

    if (!app->analysis.truth_table || app->analysis.truth_table->input_count != 2) {
        draw_text_at("Available when the circuit has exactly two inputs.", panel.x, panel.y + 18.0f, 13, GRAY);
        return;
    }

    size = 34;
    origin_x = pixel(panel.x) + 44;
    origin_y = pixel(panel.y) + 28;
    draw_text_at("B \\ A", (float)(origin_x - 40), (float)(origin_y - 20), 12, GRAY);
    draw_text_at("0", (float)(origin_x + (size / 2)), (float)(origin_y - 20), 12, GRAY);
    draw_text_at("1", (float)(origin_x + size + (size / 2)), (float)(origin_y - 20), 12, GRAY);
    draw_text_at("0", (float)(origin_x - 20), (float)(origin_y + (size / 2)), 12, GRAY);
    draw_text_at("1", (float)(origin_x - 20), (float)(origin_y + size + (size / 2)), 12, GRAY);

    cols = (uint32_t)app->analysis.truth_table->input_count + (uint32_t)app->analysis.truth_table->output_count;
    for (b = 0; b < 2; b++) {
        int a;

        for (a = 0; a < 2; a++) {
            int row_index;
            LogicValue value;
            Rectangle cell_rect;
            const char *value_text;

            row_index = (a << 1) | b;
            value = app->analysis.truth_table->data[((uint32_t)row_index * cols) + 2U];
            cell_rect = (Rectangle){
                (float)(origin_x + (a * size)),
                (float)(origin_y + (b * size)),
                (float)size,
                (float)size
            };
            DrawRectangleLinesEx(cell_rect, 1.0f, (Color){ 80, 80, 80, 255 });
            if (app->selection.view.row_valid && app->selection.view.live_row_index == (uint32_t)row_index) {
                DrawRectangleRec(cell_rect, (Color){ 245, 185, 50, 50 });
            }
            if (app->selection.selected_row == (uint32_t)row_index &&
                (!app->selection.view.row_valid || app->selection.view.live_row_index != (uint32_t)row_index)) {
                DrawRectangleLinesEx(cell_rect, 2.0f, UI_SELECT_VIOLET);
            }
            value_text = (value == LOGIC_HIGH) ? "1" : "0";
            draw_text_at(value_text, cell_rect.x + ((float)size / 2.0f) - 5.0f, cell_rect.y + ((float)size / 2.0f) - 7.0f, 15, WHITE);
        }
    }

    {
        uint8_t group_index;

        for (group_index = 0; group_index < app->analysis.kmap_group_count; group_index++) {
            KMapGroup *group;
            float min_x;
            float min_y;
            float max_x;
            float max_y;
            int cell;

            group = &app->analysis.kmap_groups[group_index];
            min_x = 10000.0f;
            min_y = 10000.0f;
            max_x = 0.0f;
            max_y = 0.0f;

            for (cell = 0; cell < 4; cell++) {
                uint8_t cell_mask;

                cell_mask = (uint8_t)(1U << (uint32_t)cell);
                if ((group->cell_mask & cell_mask) != 0U) {
                    int a;
                    int b_index;
                    float x;
                    float y;

                    a = cell >> 1;
                    b_index = cell & 1;
                    x = (float)(origin_x + (a * size));
                    y = (float)(origin_y + (b_index * size));
                    if (x < min_x) {
                        min_x = x;
                    }
                    if (y < min_y) {
                        min_y = y;
                    }
                    if (x + (float)size > max_x) {
                        max_x = x + (float)size;
                    }
                    if (y + (float)size > max_y) {
                        max_y = y + (float)size;
                    }
                }
            }

            {
                Rectangle rect;

                rect = (Rectangle){ min_x + 3.0f, min_y + 3.0f, (max_x - min_x) - 6.0f, (max_y - min_y) - 6.0f };
                DrawRectangleRounded(rect, 0.3f, 10, group->color);
                DrawRectangleRoundedLines(rect, 0.3f, 10, (Color){ group->color.r, group->color.g, group->color.b, 255 });
            }
        }
    }
}

static const char *logic_digit_text(LogicValue value) {
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

static void draw_section_header(Rectangle section, const char *label) {
    draw_text_at(label, section.x + 14.0f, section.y + 10.0f, 11, (Color){ 150, 150, 150, 255 });
}

static void draw_section_shell(Rectangle section) {
    DrawRectangleRounded(section, 0.14f, 10, (Color){ 27, 27, 27, 255 });
    DrawRectangleRoundedLinesEx(section, 0.14f, 10, 1.0f, (Color){ 54, 54, 54, 255 });
}

static void draw_context_status(AppContext *app, Rectangle rect) {
    LogicNode *node;
    char title[96];
    char line[192];
    char fitted_inputs[192];
    size_t written;
    uint8_t input_index;
    LogicValue out_value;
    float section_left;
    float section_right;
    float text_width_limit;

    draw_section_shell(rect);
    section_left = rect.x + 16.0f;
    section_right = rect.x + rect.width - 16.0f;
    text_width_limit = rect.width - 32.0f;

    node = app->selection.view.selected_node;
    if (!node) {
        draw_section_header(rect, "CONTEXT");
        draw_wrapped_text_block(
            "Select a gate to inspect its live state and boolean form.",
            section_left,
            rect.y + 28.0f,
            text_width_limit,
            12,
            4.0f,
            2U,
            (Color){ 170, 170, 170, 255 }
        );
        return;
    }

    text_fit_with_ellipsis(node->name ? node->name : ui_node_label(node->type), 16, text_width_limit, title, sizeof(title));
    draw_text_at(
        title,
        section_left,
        rect.y + 10.0f,
        16,
        WHITE
    );

    written = 0U;
    line[0] = '\0';
    if (node->input_count > 0) {
        written += (size_t)snprintf(line + written, sizeof(line) - written, "In: ");
        for (input_index = 0; input_index < node->input_count && written + 8U < sizeof(line); input_index++) {
            LogicNet *net;
            LogicPin *pin;
            LogicValue value;

            pin = &node->inputs[input_index];
            net = NULL;
            {
                uint32_t net_index;
                uint8_t sink_index;

                for (net_index = 0; net_index < app->graph.net_count && !net; net_index++) {
                    for (sink_index = 0; sink_index < app->graph.nets[net_index].sink_count; sink_index++) {
                        if (app->graph.nets[net_index].sinks[sink_index] == pin) {
                            net = &app->graph.nets[net_index];
                            break;
                        }
                    }
                }
            }
            value = (net && net->source) ? net->source->value : pin->value;
            written += (size_t)snprintf(
                line + written,
                sizeof(line) - written,
                "%s%s",
                (input_index > 0) ? "  " : "",
                logic_digit_text(value)
            );
        }
    }

    if (node->type == NODE_INPUT || node->type == NODE_GATE_CLOCK) {
        out_value = (node->output_count > 0) ? node->outputs[0].value : LOGIC_UNKNOWN;
    } else if (node->type == NODE_OUTPUT) {
        out_value = node->inputs[0].value;
    } else {
        out_value = (node->output_count > 0) ? node->outputs[0].value : LOGIC_UNKNOWN;
    }

    if (node->input_count > 0) {
        char out_line[32];
        Color out_color;
        float out_width;
        float out_x;
        float inputs_width_limit;

        snprintf(out_line, sizeof(out_line), "Out: %s", logic_digit_text(out_value));
        out_color = (out_value == LOGIC_HIGH) ? (Color){ 245, 185, 50, 255 } : LIGHTGRAY;
        out_width = text_width(out_line, 12);
        out_x = section_right - out_width;
        inputs_width_limit = out_x - section_left - 12.0f;
        if (inputs_width_limit < 0.0f) {
            inputs_width_limit = 0.0f;
        }

        text_fit_with_ellipsis(line, 12, inputs_width_limit, fitted_inputs, sizeof(fitted_inputs));
        if (fitted_inputs[0] != '\0') {
            draw_text_at(fitted_inputs, section_left, rect.y + 32.0f, 12, LIGHTGRAY);
        }
        draw_text_at(out_line, out_x, rect.y + 32.0f, 12, out_color);
        return;
    }

    {
        char out_line[32];
        Color out_color;
        float out_width;

        snprintf(out_line, sizeof(out_line), "Out: %s", logic_digit_text(out_value));
        out_color = (out_value == LOGIC_HIGH) ? (Color){ 245, 185, 50, 255 } : LIGHTGRAY;
        out_width = text_width(out_line, 12);
        draw_text_at(out_line, section_right - out_width, rect.y + 32.0f, 12, out_color);
    }
}

static void draw_context_equation(AppContext *app, Rectangle rect) {
    LogicNode *node;
    char symbolic[512];
    char values[512];
    float text_x;
    float text_width_limit;
    float text_y;

    draw_section_shell(rect);
    draw_section_header(rect, "EQUATION");

    node = app->selection.view.selected_node;
    if (!node || node->type == NODE_INPUT || node->type == NODE_GATE_CLOCK) {
        draw_wrapped_text_block(
            node ? "Simple signal - no equation to decompose." : "Select a gate to see its boolean equation.",
            rect.x + 16.0f,
            rect.y + 32.0f,
            rect.width - 32.0f,
            12,
            4.0f,
            2U,
            GRAY
        );
        return;
    }

    if (!logic_format_equation_symbolic(&app->graph, node, symbolic, sizeof(symbolic))) {
        return;
    }
    if (!logic_format_equation_values(&app->graph, node, values, sizeof(values))) {
        values[0] = '\0';
    }

    text_x = rect.x + 16.0f;
    text_y = rect.y + 30.0f;
    text_width_limit = rect.width - 32.0f;
    text_y = draw_wrapped_text_block(symbolic, text_x, text_y, text_width_limit, 13, 3.0f, 2U, WHITE);

    if (values[0] != '\0' && strcmp(symbolic, values) != 0) {
        Color values_color;

        values_color = (Color){ 168, 164, 188, 255 };
        draw_wrapped_text_block(values, text_x, text_y + 5.0f, text_width_limit, 12, 3.0f, 1U, values_color);
    }
}

static const char *gate_why_line(NodeType type, LogicValue value) {
    if (value == LOGIC_UNKNOWN || value == LOGIC_ERROR) {
        return "Output is undefined - connect all inputs to evaluate.";
    }
    if (type == NODE_GATE_AND) {
        return (value == LOGIC_HIGH)
            ? "Output is 1 because all inputs are 1."
            : "Output is 0 because at least one input is 0.";
    }
    if (type == NODE_GATE_OR) {
        return (value == LOGIC_HIGH)
            ? "Output is 1 because at least one input is 1."
            : "Output is 0 because all inputs are 0.";
    }
    if (type == NODE_GATE_NOT) {
        return (value == LOGIC_HIGH)
            ? "Output is 1 because the input is 0."
            : "Output is 0 because the input is 1.";
    }
    if (type == NODE_GATE_XOR) {
        return (value == LOGIC_HIGH)
            ? "Output is 1 because inputs differ."
            : "Output is 0 because inputs are equal.";
    }
    if (type == NODE_GATE_NAND) {
        return (value == LOGIC_HIGH)
            ? "Output is 1 because at least one input is 0."
            : "Output is 0 because all inputs are 1.";
    }
    if (type == NODE_GATE_NOR) {
        return (value == LOGIC_HIGH)
            ? "Output is 1 because all inputs are 0."
            : "Output is 0 because at least one input is 1.";
    }
    if (type == NODE_OUTPUT) {
        return (value == LOGIC_HIGH)
            ? "Output signal is 1."
            : "Output signal is 0.";
    }
    if (type == NODE_INPUT) {
        return "Input signal - click to toggle.";
    }
    return "Current output reflects the live input values.";
}

static void draw_context_why(AppContext *app, Rectangle rect) {
    LogicNode *node;
    LogicValue value;

    draw_section_shell(rect);
    draw_section_header(rect, "WHY");

    node = app->selection.view.selected_node;
    if (!node) {
        draw_wrapped_text_block(
            "Selection explains its output here.",
            rect.x + 16.0f,
            rect.y + 32.0f,
            rect.width - 32.0f,
            12,
            4.0f,
            2U,
            GRAY
        );
        return;
    }

    if (node->type == NODE_OUTPUT) {
        value = node->inputs[0].value;
    } else {
        value = (node->output_count > 0) ? node->outputs[0].value : LOGIC_UNKNOWN;
    }

    draw_wrapped_text_block(
        gate_why_line(node->type, value),
        rect.x + 16.0f,
        rect.y + 32.0f,
        rect.width - 32.0f,
        13,
        4.0f,
        3U,
        LIGHTGRAY
    );
}

static void draw_context_compare(AppContext *app, Rectangle rect) {
    draw_section_shell(rect);
    draw_section_header(rect, "COMPARE");

    if (app->comparison.status == APP_COMPARE_NO_TARGET) {
        draw_text_at("No reference circuit loaded.", rect.x + 14.0f, rect.y + 32.0f, 13, LIGHTGRAY);
        draw_text_at("Load a target design before comparing.", rect.x + 14.0f, rect.y + 50.0f, 12, GRAY);
        return;
    }
    if (app->comparison.equivalent) {
        draw_text_at("Designs are equivalent.", rect.x + 14.0f, rect.y + 32.0f, 14, (Color){ 76, 175, 80, 255 });
        return;
    }

    {
        char line[64];

        draw_text_at("Mismatch detected.", rect.x + 14.0f, rect.y + 32.0f, 14, (Color){ 220, 60, 60, 255 });
        snprintf(line, sizeof(line), "First failing row: %u", app->comparison.first_failing_row);
        draw_text_at(line, rect.x + 14.0f, rect.y + 52.0f, 12, GRAY);
    }
}

static const char *boolean_operand_end(const char *cursor) {
    int depth;

    if (!cursor || *cursor == '\0') {
        return cursor;
    }

    if (*cursor == '!') {
        return boolean_operand_end(cursor + 1);
    }

    if (*cursor != '(') {
        return cursor + 1;
    }

    depth = 0;
    while (*cursor != '\0') {
        if (*cursor == '(') {
            depth++;
        } else if (*cursor == ')') {
            depth--;
            if (depth == 0) {
                return cursor + 1;
            }
        }
        cursor++;
    }

    return cursor;
}

static float boolean_expression_width_range(const char *start, const char *end, int font_size) {
    const char *cursor;
    float width;

    cursor = start;
    width = 0.0f;
    while (cursor && *cursor != '\0' && cursor < end) {
        if (*cursor == '!') {
            const char *operand_start;
            const char *operand_end;

            operand_start = cursor + 1;
            operand_end = boolean_operand_end(operand_start);
            width += boolean_expression_width_range(operand_start, operand_end, font_size);
            cursor = operand_end;
        } else {
            char text[2];

            text[0] = *cursor;
            text[1] = '\0';
            width += text_width(text, font_size);
            cursor++;
        }
    }

    return width;
}

static float boolean_expression_width(const char *expression, int font_size) {
    if (!expression) {
        return 0.0f;
    }

    return boolean_expression_width_range(expression, expression + strlen(expression), font_size);
}

static float draw_boolean_expression_range(
    const char *start,
    const char *end,
    float x,
    float y,
    int font_size,
    Color color
) {
    const char *cursor;

    cursor = start;
    while (cursor && *cursor != '\0' && cursor < end) {
        if (*cursor == '!') {
            const char *operand_start;
            const char *operand_end;
            float overline_start;
            float overline_end;
            float overline_y;

            operand_start = cursor + 1;
            operand_end = boolean_operand_end(operand_start);
            overline_start = x;
            x = draw_boolean_expression_range(operand_start, operand_end, x, y, font_size, color);
            overline_end = x;
            overline_y = y - 4.0f;
            DrawLineEx((Vector2){ overline_start, overline_y }, (Vector2){ overline_end, overline_y }, 2.0f, color);
            cursor = operand_end;
        } else {
            char text[2];

            text[0] = *cursor;
            text[1] = '\0';
            draw_text_at(text, x, y, font_size, color);
            x += text_width(text, font_size);
            cursor++;
        }
    }

    return x;
}

static int fit_boolean_expression_font_size(const char *expression, float max_width, int preferred_size, int min_size) {
    int font_size;

    font_size = preferred_size;
    while (font_size > min_size && boolean_expression_width(expression, font_size) > max_width) {
        font_size--;
    }

    return font_size;
}

static float draw_boolean_expression(const char *expression, float x, float y, float max_width, int preferred_size, int min_size, Color color) {
    int font_size;

    if (!expression || expression[0] == '\0') {
        return x;
    }

    font_size = fit_boolean_expression_font_size(expression, max_width, preferred_size, min_size);
    return draw_boolean_expression_range(expression, expression + strlen(expression), x, y, font_size, color);
}

static void draw_solver_input(AppContext *app, Rectangle rect) {
    Rectangle field;
    Color border;
    float text_x;
    float text_y;
    float max_width;

    draw_text_at("EXPRESSION", rect.x, rect.y, 11, GRAY);
    field = ui_make_rect(rect.x, rect.y + 20.0f, rect.width, 44.0f);
    border = app->solver.input_focused ? (Color){ 200, 170, 255, 220 } : (Color){ 70, 70, 70, 255 };
    DrawRectangleRounded(field, 0.12f, 8, (Color){ 31, 31, 31, 255 });
    DrawRectangleRoundedLinesEx(field, 0.12f, 8, 1.0f, border);

    text_x = field.x + 14.0f;
    text_y = field.y + 13.0f;
    max_width = field.width - 28.0f;
    if (app->solver.input[0] == '\0') {
        draw_text_at("!AB + !(A + B) + !AC + AB", text_x, text_y, 14, (Color){ 120, 120, 120, 255 });
    } else {
        char fitted[BOOL_SOLVER_INPUT_MAX + 1U];

        text_fit_with_ellipsis(app->solver.input, 14, max_width, fitted, sizeof(fitted));
        draw_text_at(fitted, text_x, text_y, 14, LIGHTGRAY);
        if (app->solver.input_focused && ((int)(GetTime() * 2.0) % 2) == 0) {
            float caret_x;

            caret_x = text_x + text_width(fitted, 14) + 2.0f;
            if (caret_x < field.x + field.width - 10.0f) {
                DrawLineEx((Vector2){ caret_x, field.y + 10.0f }, (Vector2){ caret_x, field.y + field.height - 10.0f }, 1.0f, UI_SELECT_VIOLET);
            }
        }
    }
}

static void draw_solver_preview(AppContext *app, Rectangle rect) {
    draw_section_shell(rect);
    draw_section_header(rect, "PREVIEW");
    if (app->solver.input[0] == '\0') {
        draw_text_at("Enter an expression.", rect.x + 16.0f, rect.y + 42.0f, 13, GRAY);
        return;
    }

    draw_boolean_expression(app->solver.input, rect.x + 16.0f, rect.y + 44.0f, rect.width - 32.0f, 28, 14, LIGHTGRAY);
}

static void draw_solver_result(AppContext *app, Rectangle rect) {
    draw_section_shell(rect);
    draw_section_header(rect, "SIMPLIFIED");
    if (!app->solver.result.ok) {
        draw_wrapped_text_block(
            app->solver.result.error[0] ? app->solver.result.error : "Could not simplify the expression.",
            rect.x + 16.0f,
            rect.y + 38.0f,
            rect.width - 32.0f,
            13,
            4.0f,
            3U,
            (Color){ 220, 90, 90, 255 }
        );
        return;
    }

    draw_boolean_expression(
        app->solver.result.simplified_expression,
        rect.x + 16.0f,
        rect.y + 42.0f,
        rect.width - 32.0f,
        32,
        16,
        UI_SELECT_VIOLET
    );
}

static void draw_solver_steps(AppContext *app, Rectangle rect) {
    float content_y;
    float max_scroll;
    uint8_t step_index;

    draw_section_shell(rect);
    draw_text_at("Steps", rect.x + 16.0f, rect.y + 14.0f, 24, WHITE);

    max_scroll = ui_solver_steps_content_height(app) - rect.height;
    if (max_scroll < 0.0f) {
        max_scroll = 0.0f;
    }
    if (app->solver.steps_scroll > max_scroll) {
        app->solver.steps_scroll = max_scroll;
    }

    if (!begin_scissor_rect(ui_make_rect(rect.x + 1.0f, rect.y + SOLVER_STEPS_HEADER_HEIGHT, rect.width - 2.0f, rect.height - SOLVER_STEPS_HEADER_HEIGHT - 1.0f))) {
        return;
    }

    content_y = rect.y + SOLVER_STEPS_HEADER_HEIGHT - app->solver.steps_scroll;
    if (!app->solver.result.ok) {
        draw_line_at(rect.x + 14.0f, content_y, rect.x + rect.width - 14.0f, content_y, (Color){ 64, 64, 64, 255 });
        draw_text_at("Waiting for a valid expression", rect.x + 16.0f, content_y + 18.0f, 15, GRAY);
        EndScissorMode();
        return;
    }

    for (step_index = 0U; step_index < app->solver.result.step_count; step_index++) {
        const BoolSolverStep *step;
        float step_top;

        step = &app->solver.result.steps[step_index];
        step_top = content_y + ((float)step_index * SOLVER_STEP_HEIGHT);
        draw_line_at(rect.x + 14.0f, step_top, rect.x + rect.width - 14.0f, step_top, (Color){ 64, 64, 64, 255 });
        draw_wrapped_text_block(
            step->title,
            rect.x + 16.0f,
            step_top + 16.0f,
            rect.width - 32.0f,
            15,
            3.0f,
            2U,
            (Color){ 150, 150, 150, 255 }
        );
        draw_boolean_expression(
            step->expression,
            rect.x + 16.0f,
            step_top + 48.0f,
            rect.width - 32.0f,
            24,
            12,
            LIGHTGRAY
        );
    }

    EndScissorMode();
}

void ui_draw_solver_workspace(AppContext *app, Rectangle panel) {
    UiSolverLayout layout;

    DrawRectangleRec(panel, (Color){ 24, 24, 24, 255 });
    layout = ui_measure_solver_workspace(panel);

    draw_text_at("BOOLEAN ALGEBRA SOLVER", layout.panel_rect.x + SOLVER_PANEL_PADDING, layout.panel_rect.y + 16.0f, 18, LIGHTGRAY);
    draw_solver_input(app, layout.input_rect);
    draw_solver_preview(app, layout.preview_rect);
    draw_solver_result(app, layout.result_rect);
    draw_solver_steps(app, layout.steps_rect);
}

static void draw_solver_side_section(Rectangle rect, const char *title, const char *body, Color body_color, uint32_t max_lines) {
    draw_section_shell(rect);
    draw_section_header(rect, title);
    draw_wrapped_text_block(body, rect.x + 16.0f, rect.y + 32.0f, rect.width - 32.0f, 12, 4.0f, max_lines, body_color);
}

void ui_draw_solver_side_panel(AppContext *app, Rectangle panel) {
    Rectangle section;
    float x;
    float y;
    float width;
    const char *status;
    Color status_color;

    x = panel.x + CONTEXT_PANEL_PADDING;
    y = panel.y + CONTEXT_PANEL_PADDING;
    width = panel.width - (CONTEXT_PANEL_PADDING * 2.0f);
    if (width < 0.0f) {
        width = 0.0f;
    }

    status = app->solver.result.ok ? "Verified equivalent by truth table." :
        (app->solver.result.error[0] ? app->solver.result.error : "Expression has not been solved.");
    status_color = app->solver.result.ok ? (Color){ 120, 210, 150, 255 } : (Color){ 220, 90, 90, 255 };

    section = ui_make_rect(x, y, width, 78.0f);
    draw_solver_side_section(section, "STATUS", status, status_color, 2U);
    y += section.height + CONTEXT_PANEL_GAP;

    section = ui_make_rect(x, y, width, 88.0f);
    draw_solver_side_section(section, "VARIABLES", app->solver.result.variables[0] ? app->solver.result.variables : "None", LIGHTGRAY, 2U);
    y += section.height + CONTEXT_PANEL_GAP;

    section = ui_make_rect(x, y, width, 130.0f);
    draw_solver_side_section(
        section,
        "NOTATION",
        "!A means NOT A. Adjacent variables multiply, + is OR, and parentheses group expressions.",
        LIGHTGRAY,
        4U
    );
    y += section.height + CONTEXT_PANEL_GAP;

    section = ui_make_rect(x, y, width, panel.y + panel.height - CONTEXT_PANEL_PADDING - y);
    draw_solver_side_section(
        section,
        "ALGORITHM USED",
        app->solver.result.algorithm[0] ? app->solver.result.algorithm :
            "Parse the expression, normalize NOT, build minterms, combine implicants, cover essentials, and verify.",
        LIGHTGRAY,
        10U
    );
}

void ui_draw_context_panel(AppContext *app, Rectangle panel) {
    UiContextPanelLayout layout;

    layout = ui_measure_context_panel(app, panel);

    if (layout.status_rect.width > 0.0f && layout.status_rect.height > 0.0f) {
        draw_context_status(app, layout.status_rect);
    }
    if (layout.show_compare && layout.compare_rect.width > 0.0f && layout.compare_rect.height > 0.0f) {
        draw_context_compare(app, layout.compare_rect);
    }
    if (layout.equation_rect.width > 0.0f && layout.equation_rect.height > 0.0f) {
        draw_context_equation(app, layout.equation_rect);
    }
    if (layout.truth_table_rect.width > 0.0f && layout.truth_table_rect.height > 0.0f) {
        ui_draw_truth_table(app, layout.truth_table_rect);
    }
    if (layout.show_kmap && layout.kmap_rect.width > 0.0f && layout.kmap_rect.height > 0.0f) {
        Rectangle kmap_inner;

        draw_section_shell(layout.kmap_rect);
        draw_section_header(layout.kmap_rect, "K-MAP");
        kmap_inner = ui_make_rect(
            layout.kmap_rect.x + 14.0f,
            layout.kmap_rect.y + 26.0f,
            layout.kmap_rect.width - 28.0f,
            layout.kmap_rect.height - 40.0f
        );
        ui_draw_kmap(app, kmap_inner);
    }
    if (layout.why_rect.width > 0.0f && layout.why_rect.height > 0.0f) {
        draw_context_why(app, layout.why_rect);
    }
}

void ui_draw_toolbox(AppContext *app, Rectangle panel) {
    static const char *labels[] = { "INPUT", "OUTPUT", "AND", "OR", "NOT", "XOR", "CLOCK" };
    static const AppTool tools[] = {
        APP_TOOL_INPUT,
        APP_TOOL_OUTPUT,
        APP_TOOL_AND,
        APP_TOOL_OR,
        APP_TOOL_NOT,
        APP_TOOL_XOR,
        APP_TOOL_CLOCK
    };
    int tool_index;

    DrawRectangleRec(panel, (Color){ 30, 30, 30, 255 });
    draw_line_at(panel.x + panel.width, panel.y, panel.x + panel.width, panel.y + panel.height, (Color){ 50, 50, 50, 255 });

    for (tool_index = 0; tool_index < (int)(sizeof(labels) / sizeof(labels[0])); tool_index++) {
        Rectangle item_rect;
        Vector2 mouse_pos;
        bool hovered;
        bool selected;
        Color button_color;
        float content_padding;
        float max_text_width;
        float text_x;
        float text_gap;
        int label_font_size;
        int shortcut_font_size;
        float symbol_width;
        float symbol_height;
        Rectangle symbol_rect;
        NodeType tool_type;

        if (!ui_toolbox_item_rect(panel, tool_index, &item_rect)) {
            continue;
        }
        mouse_pos = GetMousePosition();
        hovered = CheckCollisionPointRec(mouse_pos, item_rect);
        selected = app->active_tool == tools[tool_index];
        button_color = hovered ? (Color){ 50, 50, 50, 255 } : (Color){ 38, 38, 38, 255 };

        DrawRectangleRounded(item_rect, 0.2f, 10, button_color);
        DrawRectangleRoundedLinesEx(item_rect, 0.2f, 10, 1.0f, (Color){ 70, 70, 70, 255 });
        if (selected) {
            DrawRectangle(pixel(item_rect.x), pixel(item_rect.y + 6.0f), 3, pixel(item_rect.height - 12.0f), UI_SELECT_VIOLET);
        }

        content_padding = item_rect.height * 0.24f;
        if (content_padding < 10.0f) {
            content_padding = 10.0f;
        } else if (content_padding > 18.0f) {
            content_padding = 18.0f;
        }

        symbol_height = item_rect.height * 0.34f;
        symbol_width = symbol_height * 1.33f;
        if (tools[tool_index] == APP_TOOL_INPUT || tools[tool_index] == APP_TOOL_OUTPUT || tools[tool_index] == APP_TOOL_NOT) {
            symbol_width = symbol_height * 1.5f;
        }

        text_gap = item_rect.width * 0.08f;
        if (text_gap < 10.0f) {
            text_gap = 10.0f;
        } else if (text_gap > 18.0f) {
            text_gap = 18.0f;
        }
        text_x = item_rect.x + content_padding + symbol_width + text_gap;
        max_text_width = (item_rect.x + item_rect.width) - content_padding - text_x;
        if (max_text_width < 0.0f) {
            max_text_width = 0.0f;
        }

        label_font_size = (int)(item_rect.height * 0.24f);
        if (label_font_size < 9) {
            label_font_size = 9;
        } else if (label_font_size > 16) {
            label_font_size = 16;
        }
        label_font_size = fit_text_font_size(labels[tool_index], max_text_width, label_font_size, 8);
        shortcut_font_size = label_font_size - 1;
        if (shortcut_font_size < 8) {
            shortcut_font_size = 8;
        }
        shortcut_font_size = fit_text_font_size(toolbox_shortcut_label(tools[tool_index]), max_text_width, shortcut_font_size, 8);

        tool_type = app_node_type_for_tool(tools[tool_index]);
        symbol_rect = (Rectangle){
            item_rect.x + content_padding,
            item_rect.y + ((item_rect.height - symbol_height) / 2.0f),
            symbol_width,
            symbol_height
        };
        draw_gate_symbol(
            tool_type,
            symbol_rect,
            (Color){ 200, 200, 200, 255 },
            (Color){ 255, 255, 255, 200 },
            1.0f
        );
        if (tool_type == NODE_GATE_CLOCK) {
            draw_clock_glyph(symbol_rect, LOGIC_UNKNOWN);
        }

        draw_text_at(
            labels[tool_index],
            text_x,
            item_rect.y + (item_rect.height * 0.18f),
            label_font_size,
            selected ? WHITE : LIGHTGRAY
        );
        draw_text_at(
            toolbox_shortcut_label(tools[tool_index]),
            text_x,
            item_rect.y + item_rect.height - (float)shortcut_font_size - (item_rect.height * 0.16f),
            shortcut_font_size,
            GRAY
        );
    }
}

void ui_draw_placement_ghost(AppContext *app, Rectangle canvas, Vector2 mouse_pos) {
    Camera2D camera;
    Vector2 snapped;
    Vector2 world_pos;
    NodeType type;
    float width;
    float height;
    Rectangle ghost;

    if (!app_tool_places_node(app->active_tool)) {
        return;
    }
    if (!CheckCollisionPointRec(mouse_pos, canvas)) {
        return;
    }

    world_pos = app_canvas_screen_to_world(app, canvas, mouse_pos);
    type = app_node_type_for_tool(app->active_tool);
    width = 80.0f;
    height = 60.0f;
    if (type == NODE_INPUT || type == NODE_OUTPUT || type == NODE_GATE_NOT || type == NODE_GATE_CLOCK) {
        width = 60.0f;
        height = 40.0f;
    }

    snapped = app_snap_node_position(world_pos, type);
    ghost = (Rectangle){ snapped.x, snapped.y, width, height };

    camera = app_canvas_camera(app, canvas);
    BeginMode2D(camera);
    draw_gate_symbol(type, ghost, (Color){ 200, 170, 255, 80 }, (Color){ 200, 170, 255, 180 }, 1.0f);
    if (type == NODE_GATE_CLOCK) {
        draw_clock_glyph(ghost, LOGIC_UNKNOWN);
    }
    EndMode2D();
}

void ui_draw_status_strip(AppContext *app, Rectangle panel) {
    char selected_text[96];
    char source_text[APP_SOURCE_PATH_MAX + APP_STATUS_MESSAGE_MAX + 16];
    char compare_text[32];
    char selection_label[64];
    const char *compare_label;

    DrawRectangleRec(panel, (Color){ 24, 24, 24, 255 });
    draw_line_at(panel.x, panel.y, panel.x + panel.width, panel.y, (Color){ 42, 42, 42, 255 });

    ui_build_selection_label(app, selection_label, sizeof(selection_label));

    compare_label = "None";
    if (app->comparison.status == APP_COMPARE_EQUIVALENT) {
        compare_label = "Equivalent";
    } else if (app->comparison.status == APP_COMPARE_MISMATCH) {
        compare_label = "Mismatch";
    }

    snprintf(
        selected_text,
        sizeof(selected_text),
        "Tool %s   Mode %s   Selected %s",
        app_tool_label(app->active_tool),
        app_mode_label(app->mode),
        selection_label
    );
    snprintf(compare_text, sizeof(compare_text), "Compare %s", compare_label);

    if (app->source.path[0] != '\0') {
        snprintf(
            source_text,
            sizeof(source_text),
            "Source %s   %s",
            app->source.path,
            app->source.status
        );
    } else {
        snprintf(source_text, sizeof(source_text), "%s", app->source.status);
    }

    draw_text_at(selected_text, panel.x + 14.0f, panel.y + 7.0f, 11, LIGHTGRAY);
    draw_text_at(compare_text, panel.x + panel.width - 118.0f, panel.y + 7.0f, 11, app->comparison.status == APP_COMPARE_MISMATCH ? (Color){ 200, 50, 50, 255 } : GRAY);
    draw_text_at(source_text, panel.x + 14.0f, panel.y + 20.0f, 10, GRAY);
    draw_text_at("Shortcuts  V select  1-7 tools  Tab cycle  Space run  . step  Del delete  Esc cancel", panel.x + 14.0f, panel.y + 33.0f, 10, GRAY);
}

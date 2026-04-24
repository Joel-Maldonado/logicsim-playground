#include "app_canvas.h"
#include "node_catalog.h"
#include <string.h>

#define APP_PIN_ALIGN_TOLERANCE 12.0f

static float app_absf(float value) {
    return value < 0.0f ? -value : value;
}

static float app_minf(float left, float right) {
    return left < right ? left : right;
}

static uint32_t app_count_nodes_of_type(const AppContext *app, NodeType type) {
    uint32_t count;
    uint32_t index;

    count = 0U;
    for (index = 0U; index < app->graph.node_count; index++) {
        if (app->graph.nodes[index].type == type) {
            count++;
        }
    }

    return count;
}

static void app_consider_vertical_alignment(float candidate_y, float desired_y, float *best_y, float *best_distance) {
    float distance;

    distance = app_absf(candidate_y - desired_y);
    if (distance > APP_PIN_ALIGN_TOLERANCE || distance >= *best_distance) {
        return;
    }

    *best_y = candidate_y;
    *best_distance = distance;
}

void app_node_dimensions(NodeType type, int *width, int *height) {
    uint8_t pin_rows;

    if (!width || !height) {
        return;
    }

    pin_rows = node_catalog_pin_rows(type);
    *height = (int)((float)APP_GRID_SIZE * 2.0f * (float)pin_rows);
    *width = node_catalog_width(type);
}

float app_node_pin_offset_y(const LogicNode *node, bool is_output_pin, uint8_t pin_index) {
    uint8_t pin_count;
    float pitch;

    if (!node) {
        return 0.0f;
    }

    pin_count = is_output_pin ? node->output_count : node->input_count;
    if (pin_count <= 1U) {
        return node->rect.height * 0.5f;
    }

    pitch = (node->rect.height - (APP_GRID_SIZE * 2.0f)) / (float)(pin_count - 1U);
    return APP_GRID_SIZE + ((float)pin_index * pitch);
}

Vector2 app_default_node_position(const AppContext *app, NodeType type) {
    uint32_t count;

    count = app_count_nodes_of_type(app, type);
    switch (type) {
        case NODE_INPUT:
            return (Vector2){ 140.0f, 160.0f + ((float)count * 100.0f) };
        case NODE_OUTPUT:
            return (Vector2){ 520.0f, 220.0f + ((float)count * 100.0f) };
        case NODE_GATE_AND:
        case NODE_GATE_OR:
        case NODE_GATE_NOT:
        case NODE_GATE_XOR:
        case NODE_GATE_NAND:
        case NODE_GATE_NOR:
        case NODE_GATE_DFF:
        case NODE_GATE_LATCH:
        case NODE_GATE_CLOCK:
            return (Vector2){ 320.0f, 200.0f + ((float)count * 100.0f) };
        case NODE_INVALID:
        default:
            return (Vector2){ 320.0f, 200.0f };
    }
}

Vector2 app_snap_node_position(Vector2 position, NodeType type) {
    (void)type;

    return (Vector2){
        (float)((int)(position.x / APP_GRID_SIZE) * (int)APP_GRID_SIZE),
        (float)((int)(position.y / APP_GRID_SIZE) * (int)APP_GRID_SIZE)
    };
}

Vector2 app_snap_live_node_position(const AppContext *app, const LogicNode *node, Vector2 position) {
    Vector2 snapped;
    float best_y;
    float best_distance;
    float input_pin_sum_y;
    float input_offset_sum;
    float output_pin_sum_y;
    float output_offset_sum;
    uint32_t input_alignment_count;
    uint32_t output_alignment_count;
    uint32_t net_index;

    snapped = app_snap_node_position(position, node ? node->type : NODE_INPUT);
    if (!app || !logic_node_is_active(node)) {
        return snapped;
    }

    best_y = snapped.y;
    best_distance = APP_PIN_ALIGN_TOLERANCE + 1.0f;
    input_pin_sum_y = 0.0f;
    input_offset_sum = 0.0f;
    output_pin_sum_y = 0.0f;
    output_offset_sum = 0.0f;
    input_alignment_count = 0U;
    output_alignment_count = 0U;

    for (net_index = 0U; net_index < app->graph.net_count; net_index++) {
        const LogicNet *net;
        uint8_t sink_index;

        net = &app->graph.nets[net_index];
        if (net->source && net->source->node == node) {
            float source_offset;

            source_offset = app_node_pin_offset_y(node, true, net->source->index);
            for (sink_index = 0U; sink_index < net->sink_count; sink_index++) {
                LogicPin *sink_pin;
                float candidate_y;

                sink_pin = net->sinks[sink_index];
                if (!sink_pin || !logic_node_is_active(sink_pin->node)) {
                    continue;
                }

                candidate_y = sink_pin->node->pos.y + app_node_pin_offset_y(sink_pin->node, false, sink_pin->index) - source_offset;
                app_consider_vertical_alignment(candidate_y, position.y, &best_y, &best_distance);
                output_pin_sum_y += sink_pin->node->pos.y + app_node_pin_offset_y(sink_pin->node, false, sink_pin->index);
                output_offset_sum += source_offset;
                output_alignment_count++;
            }
        }

        if (!net->source || !net->source->node || net->source->node->type == NODE_INVALID) {
            continue;
        }

        for (sink_index = 0U; sink_index < net->sink_count; sink_index++) {
            const LogicNode *source_node;
            LogicPin *sink_pin;
            float candidate_y;

            source_node = net->source->node;
            sink_pin = net->sinks[sink_index];
            if (!sink_pin || sink_pin->node != node) {
                continue;
            }

            candidate_y =
                source_node->pos.y +
                app_node_pin_offset_y(source_node, true, net->source->index) -
                app_node_pin_offset_y(node, false, sink_pin->index);
            app_consider_vertical_alignment(candidate_y, position.y, &best_y, &best_distance);
            input_pin_sum_y += source_node->pos.y + app_node_pin_offset_y(source_node, true, net->source->index);
            input_offset_sum += app_node_pin_offset_y(node, false, sink_pin->index);
            input_alignment_count++;
        }
    }

    if (input_alignment_count > 1U) {
        float centered_input_y;

        centered_input_y = (input_pin_sum_y / (float)input_alignment_count) - (input_offset_sum / (float)input_alignment_count);
        app_consider_vertical_alignment(centered_input_y, position.y, &best_y, &best_distance);
    }

    if (output_alignment_count > 1U) {
        float centered_output_y;

        centered_output_y = (output_pin_sum_y / (float)output_alignment_count) - (output_offset_sum / (float)output_alignment_count);
        app_consider_vertical_alignment(centered_output_y, position.y, &best_y, &best_distance);
    }

    snapped.y = best_y;
    return snapped;
}

float app_canvas_clamp_zoom(float zoom) {
    if (zoom < APP_CANVAS_MIN_ZOOM) {
        return APP_CANVAS_MIN_ZOOM;
    }
    if (zoom > APP_CANVAS_MAX_ZOOM) {
        return APP_CANVAS_MAX_ZOOM;
    }

    return zoom;
}

Vector2 app_canvas_screen_to_world_at(Vector2 origin, float zoom, Rectangle canvas_rect, Vector2 screen_pos) {
    float safe_zoom;

    safe_zoom = app_canvas_clamp_zoom(zoom);
    return (Vector2){
        origin.x + ((screen_pos.x - canvas_rect.x) / safe_zoom),
        origin.y + ((screen_pos.y - canvas_rect.y) / safe_zoom)
    };
}

Vector2 app_canvas_world_to_screen_at(Vector2 origin, float zoom, Rectangle canvas_rect, Vector2 world_pos) {
    float safe_zoom;

    safe_zoom = app_canvas_clamp_zoom(zoom);
    return (Vector2){
        canvas_rect.x + ((world_pos.x - origin.x) * safe_zoom),
        canvas_rect.y + ((world_pos.y - origin.y) * safe_zoom)
    };
}

Vector2 app_canvas_origin_after_pan(Vector2 origin, float zoom, Vector2 screen_delta) {
    float safe_zoom;

    safe_zoom = app_canvas_clamp_zoom(zoom);
    return (Vector2){
        origin.x - (screen_delta.x / safe_zoom),
        origin.y - (screen_delta.y / safe_zoom)
    };
}

Vector2 app_canvas_origin_after_zoom(
    Vector2 origin,
    float current_zoom,
    float new_zoom,
    Rectangle canvas_rect,
    Vector2 screen_anchor
) {
    Vector2 anchor_world;
    float clamped_zoom;

    anchor_world = app_canvas_screen_to_world_at(origin, current_zoom, canvas_rect, screen_anchor);
    clamped_zoom = app_canvas_clamp_zoom(new_zoom);
    return (Vector2){
        anchor_world.x - ((screen_anchor.x - canvas_rect.x) / clamped_zoom),
        anchor_world.y - ((screen_anchor.y - canvas_rect.y) / clamped_zoom)
    };
}

void app_reset_canvas_view(AppContext *app) {
    if (!app) {
        return;
    }

    app->canvas.origin = (Vector2){ 0.0f, 0.0f };
    app->canvas.zoom = 1.0f;
}

bool app_frame_graph_in_canvas(AppContext *app, Rectangle canvas_rect) {
    static const float world_padding = 80.0f;
    bool found_node;
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    uint32_t node_index;
    float width;
    float height;
    float zoom_x;
    float zoom_y;
    float zoom;
    Vector2 center;

    if (!app || canvas_rect.width <= 0.0f || canvas_rect.height <= 0.0f) {
        return false;
    }

    found_node = false;
    min_x = 0.0f;
    min_y = 0.0f;
    max_x = 0.0f;
    max_y = 0.0f;
    for (node_index = 0U; node_index < app->graph.node_count; node_index++) {
        LogicNode *node;

        node = &app->graph.nodes[node_index];
        if (!logic_node_is_active(node)) {
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

    if (!found_node) {
        return false;
    }

    width = (max_x - min_x) + (world_padding * 2.0f);
    height = (max_y - min_y) + (world_padding * 2.0f);
    if (width <= 0.0f || height <= 0.0f) {
        return false;
    }

    zoom_x = canvas_rect.width / width;
    zoom_y = canvas_rect.height / height;
    zoom = app_minf(zoom_x, zoom_y);
    if (zoom > 1.0f) {
        zoom = 1.0f;
    }
    app->canvas.zoom = app_canvas_clamp_zoom(zoom);

    center = (Vector2){ (min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f };
    app->canvas.origin = (Vector2){
        center.x - (canvas_rect.width * 0.5f) / app->canvas.zoom,
        center.y - (canvas_rect.height * 0.5f) / app->canvas.zoom
    };
    return true;
}

Camera2D app_canvas_camera(const AppContext *app, Rectangle canvas_rect) {
    Camera2D camera;

    memset(&camera, 0, sizeof(camera));
    if (!app) {
        return camera;
    }

    camera.offset = (Vector2){ canvas_rect.x, canvas_rect.y };
    camera.target = app->canvas.origin;
    camera.rotation = 0.0f;
    camera.zoom = app_canvas_clamp_zoom(app->canvas.zoom);
    return camera;
}

Vector2 app_canvas_screen_to_world(const AppContext *app, Rectangle canvas_rect, Vector2 screen_pos) {
    if (!app) {
        return (Vector2){ 0.0f, 0.0f };
    }

    return app_canvas_screen_to_world_at(app->canvas.origin, app->canvas.zoom, canvas_rect, screen_pos);
}

Vector2 app_canvas_world_to_screen(const AppContext *app, Rectangle canvas_rect, Vector2 world_pos) {
    if (!app) {
        return (Vector2){ 0.0f, 0.0f };
    }

    return app_canvas_world_to_screen_at(app->canvas.origin, app->canvas.zoom, canvas_rect, world_pos);
}

void app_pan_canvas(AppContext *app, Vector2 screen_delta) {
    if (!app) {
        return;
    }

    app->canvas.origin = app_canvas_origin_after_pan(app->canvas.origin, app->canvas.zoom, screen_delta);
}

void app_zoom_canvas_at(AppContext *app, Rectangle canvas_rect, Vector2 screen_anchor, float zoom_factor) {
    float next_zoom;
    float delta;

    if (!app || zoom_factor <= 0.0f) {
        return;
    }

    next_zoom = app_canvas_clamp_zoom(app->canvas.zoom * zoom_factor);
    delta = next_zoom - app->canvas.zoom;
    if (delta < 0.0f) {
        delta = -delta;
    }
    if (delta < 0.0001f) {
        return;
    }

    app->canvas.origin = app_canvas_origin_after_zoom(
        app->canvas.origin,
        app->canvas.zoom,
        next_zoom,
        canvas_rect,
        screen_anchor
    );
    app->canvas.zoom = next_zoom;
}

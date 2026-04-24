#include "ui_internal.h"
#include <stdio.h>

void ui_draw_waveforms(AppContext *app, Rectangle panel) {
    char overflow_text[32];
    uint32_t node_index;
    uint32_t visible_rows;
    uint32_t samples;
    float step;
    float plot_width;
    int row_height;
    int origin_x;
    int count;
    int total_rows;

    DrawRectangleRec(panel, (Color){ 24, 24, 24, 255 });
    DrawRectangleLinesEx(panel, 1.0f, (Color){ 50, 50, 50, 255 });
    draw_text_at("WAVEFORMS", panel.x + 15.0f, panel.y + 10.0f, 14, GRAY);

    row_height = 40;
    origin_x = pixel(panel.x) + 80;
    samples = WAVEFORM_SAMPLES;
    plot_width = panel.width - 100.0f;
    if (plot_width < 0.0f) {
        plot_width = 0.0f;
    }
    step = plot_width / (float)samples;
    count = 0;
    total_rows = 0;
    visible_rows = ui_waveform_visible_rows(panel);

    for (node_index = 0; node_index < app->graph.node_count; node_index++) {
        if (ui_node_has_waveform(&app->graph.nodes[node_index])) {
            total_rows++;
        }
    }

    if (total_rows > (int)visible_rows) {
        snprintf(overflow_text, sizeof(overflow_text), "+%u more", (uint32_t)(total_rows - (int)visible_rows));
        draw_text_at(
            overflow_text,
            panel.x + panel.width - 14.0f - text_width(overflow_text, 10),
            panel.y + 14.0f,
            10,
            GRAY
        );
    }

    for (node_index = 0; node_index < app->graph.node_count; node_index++) {
        LogicNode *node;

        node = &app->graph.nodes[node_index];
        if (!ui_node_has_waveform(node)) {
            continue;
        }
        if ((uint32_t)count >= visible_rows) {
            break;
        }

        {
            int y;
            int tick;
            uint32_t sample_index;

            y = pixel(panel.y) + 40 + (count * row_height);
            draw_text_at(node->name ? node->name : "Node", panel.x + 15.0f, (float)(y + 10), 12, LIGHTGRAY);
            draw_line_at((float)origin_x, (float)(y + 25), (float)origin_x + ((float)samples * step), (float)(y + 25), (Color){ 50, 50, 50, 255 });
            for (tick = 0; tick < (int)samples; tick += 20) {
                draw_line_at(
                    (float)origin_x + ((float)tick * step),
                    (float)y + 2.0f,
                    (float)origin_x + ((float)tick * step),
                    (float)y + 28.0f,
                    (Color){ 38, 38, 38, 255 }
                );
            }

            for (sample_index = 0; sample_index + 1U < samples; sample_index++) {
                uint32_t waveform_slot_1;
                uint32_t waveform_slot_2;
                LogicValue value_1;
                LogicValue value_2;
                float height_1;
                float height_2;
                Color line_color;

                waveform_slot_1 = (app->simulation.waveform_index + sample_index) % samples;
                waveform_slot_2 = (app->simulation.waveform_index + sample_index + 1U) % samples;
                value_1 = app->simulation.waveforms[node_index][waveform_slot_1];
                value_2 = app->simulation.waveforms[node_index][waveform_slot_2];
                height_1 = (value_1 == LOGIC_HIGH) ? 5.0f : 25.0f;
                height_2 = (value_2 == LOGIC_HIGH) ? 5.0f : 25.0f;
                line_color = (value_1 == LOGIC_HIGH) ? (Color){ 76, 175, 80, 255 } : (Color){ 85, 85, 85, 255 };

                DrawLineEx(
                    (Vector2){ (float)origin_x + ((float)sample_index * step), (float)y + height_1 },
                    (Vector2){ (float)origin_x + ((float)(sample_index + 1U) * step), (float)y + height_1 },
                    2.0f,
                    line_color
                );
                if (value_1 != value_2) {
                    DrawLineEx(
                        (Vector2){ (float)origin_x + ((float)(sample_index + 1U) * step), (float)y + height_1 },
                        (Vector2){ (float)origin_x + ((float)(sample_index + 1U) * step), (float)y + height_2 },
                        2.0f,
                        line_color
                    );
                }
            }
        }

        count++;
    }
}

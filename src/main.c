#include "raylib.h"
#include "app.h"
#include "app_analysis.h"
#include "app_canvas.h"
#include "app_commands.h"
#include "draw_util.h"
#include "editor_input.h"
#include "source_watch.h"
#include "topbar.h"
#include "ui.h"
#include "workspace_layout.h"
#include <stdio.h>

static void draw_resize_seam(Rectangle rect, WorkspaceResizeHandle handle) {
    Color seam_color;

    if (handle == WORKSPACE_RESIZE_HANDLE_NONE) {
        return;
    }

    seam_color = (Color){ 52, 52, 52, 200 };
    if (handle == WORKSPACE_RESIZE_HANDLE_WAVE_PANEL) {
        DrawRectangle(pixel(rect.x), pixel(rect.y + (rect.height * 0.5f)), pixel(rect.width), 1, seam_color);
    } else {
        DrawRectangle(pixel(rect.x + (rect.width * 0.5f)), pixel(rect.y), 1, pixel(rect.height), seam_color);
    }
}

static void draw_resize_handle(Rectangle rect, WorkspaceResizeHandle handle, bool hovered, bool active) {
    Color grip_color;
    Rectangle grip;
    float grip_length;
    float grip_thickness;

    if (handle == WORKSPACE_RESIZE_HANDLE_NONE || (!hovered && !active)) {
        return;
    }

    grip = rect;
    grip_length = 92.0f;
    grip_thickness = active ? 4.0f : 3.0f;
    if (handle == WORKSPACE_RESIZE_HANDLE_WAVE_PANEL) {
        grip.x += (rect.width - grip_length) * 0.5f;
        grip.width = grip_length;
        if (grip.width > rect.width - 24.0f) {
            grip.width = rect.width - 24.0f;
            grip.x = rect.x;
        }
        if (grip.width < 24.0f) {
            grip.width = rect.width;
            grip.x = rect.x;
        }
        grip.y += (rect.height - grip_thickness) * 0.5f;
        grip.height = grip_thickness;
    } else {
        grip.y += (rect.height - grip_length) * 0.5f;
        grip.height = grip_length;
        if (grip.height > rect.height - 24.0f) {
            grip.height = rect.height - 24.0f;
            grip.y = rect.y;
        }
        if (grip.height < 24.0f) {
            grip.height = rect.height;
            grip.y = rect.y;
        }
        grip.x += (rect.width - grip_thickness) * 0.5f;
        grip.width = grip_thickness;
    }

    grip_color = active ? (Color){ 200, 170, 255, 220 } : (Color){ 170, 170, 170, 140 };
    DrawRectangleRounded(grip, 1.0f, 12, grip_color);
}

static Rectangle solver_workspace_rect(const WorkspaceFrame *frame) {
    return (Rectangle){
        0.0f,
        frame->toolbox_rect.y,
        frame->side_panel_rect.x,
        frame->side_panel_rect.height
    };
}

static void draw_footer(const AppContext *app, Rectangle footer_rect) {
    char footer_text[APP_SOURCE_PATH_MAX + 96];
    const char *file_label;
    uint32_t live_count;
    uint32_t node_index;
    char sim_segment[48];
    char selected_label[64];
    char selected_segment[80];

    DrawRectangleRec(footer_rect, (Color){ 20, 20, 20, 255 });
    draw_line_at(footer_rect.x, footer_rect.y, footer_rect.x + footer_rect.width, footer_rect.y, (Color){ 50, 50, 50, 255 });

    file_label = app->source.path[0] ? app->source.path : "untitled.circ";
    live_count = 0U;
    for (node_index = 0U; node_index < app->graph.node_count; node_index++) {
        if (app->graph.nodes[node_index].type != (NodeType)-1) {
            live_count++;
        }
    }

    sim_segment[0] = '\0';
    snprintf(sim_segment, sizeof(sim_segment), "  |  sim @ %.0f Hz", (double)app->simulation.speed);

    selected_label[0] = '\0';
    if (app->selection.selected_wire_sink && app->selection.selected_wire_sink->node &&
        app->selection.selected_wire_sink->node->type != (NodeType)-1) {
        snprintf(
            selected_label,
            sizeof(selected_label),
            "wire -> %s.in%u",
            app->selection.selected_wire_sink->node->name ? app->selection.selected_wire_sink->node->name : "node",
            app->selection.selected_wire_sink->index
        );
    } else if (app->selection.selected_node && app->selection.selected_node->type != (NodeType)-1 &&
        app->selection.selected_node->name) {
        snprintf(selected_label, sizeof(selected_label), "%s", app->selection.selected_node->name);
    }

    selected_segment[0] = '\0';
    if (selected_label[0] != '\0') {
        snprintf(selected_segment, sizeof(selected_segment), "  |  selected %s", selected_label);
    }

    snprintf(footer_text, sizeof(footer_text), "FILE: %s  |  %u gates%s%s", file_label, live_count, sim_segment, selected_segment);
    draw_text_at(footer_text, footer_rect.x + 12.0f, footer_rect.y + 6.0f, 11, (Color){ 150, 150, 150, 255 });
}

static void draw_shortcuts_overlay(const WorkspaceFrame *frame, Vector2 mouse_pos, bool *shortcuts_open) {
    Rectangle card;
    float card_w;
    float card_h;
    const char *lines[] = {
        "KEYBOARD SHORTCUTS",
        "",
        "V          Select tool",
        "1-7        Pick a tool (input, gate, ...)",
        "B / C      Mode: Edit / Compare",
        "Top tabs   Switch to Edit, Compare, or Solver",
        "Space      Run / Stop simulation",
        "Drag empty canvas  Pan canvas",
        ".          Step one tick",
        "R          Reset simulation",
        "Tab        Cycle selection (Shift for reverse)",
        "Arrows     Move selected node on grid",
        "+ / -      Zoom canvas",
        "Home       Reset canvas view",
        "Del        Delete selected node or wire",
        "Esc        Cancel or close this panel",
        "/          Toggle this panel",
    };
    int line_count;
    int line_index;

    DrawRectangle(0, 0, frame->window_width, frame->window_height, (Color){ 0, 0, 0, 160 });

    card_w = 520.0f;
    card_h = 372.0f;
    card = (Rectangle){
        ((float)frame->window_width - card_w) * 0.5f,
        ((float)frame->window_height - card_h) * 0.5f,
        card_w,
        card_h,
    };
    DrawRectangleRounded(card, 0.04f, 8, (Color){ 30, 30, 30, 255 });
    DrawRectangleRoundedLinesEx(card, 0.04f, 8, 1.0f, (Color){ 80, 80, 80, 255 });

    line_count = (int)(sizeof(lines) / sizeof(lines[0]));
    for (line_index = 0; line_index < line_count; line_index++) {
        Color color;

        color = (line_index == 0) ? (Color){ 240, 240, 240, 255 } : (Color){ 180, 180, 180, 255 };
        draw_text_at(lines[line_index], card.x + 22.0f, card.y + 22.0f + ((float)line_index * 20.0f), line_index == 0 ? 13 : 12, color);
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse_pos, card)) {
        *shortcuts_open = false;
    }
}

int main(int argc, char **argv) {
    AppContext app;
    EditorInputState input_state;
    WorkspaceLayoutPrefs layout_prefs;
    WorkspaceFrame frame_layout;
    WorkspaceResizeHandles resize_handles;
    TopbarLayout topbar_layout;
    SourceWatch source_watch;
    const char *load_path;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WORKSPACE_WINDOW_START_WIDTH, WORKSPACE_WINDOW_START_HEIGHT, "LogicSim");
    SetWindowMinSize(WORKSPACE_WINDOW_MIN_WIDTH, WORKSPACE_WINDOW_MIN_HEIGHT);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    workspace_layout_init_defaults(&layout_prefs);
    workspace_layout_load_prefs(&layout_prefs);
    workspace_layout_sanitize_prefs(&layout_prefs, GetScreenWidth(), GetScreenHeight());
    frame_layout = workspace_layout_compute_frame(&layout_prefs, GetScreenWidth(), GetScreenHeight());
    resize_handles = workspace_layout_compute_handles(&frame_layout);
    topbar_layout = topbar_compute_layout(&frame_layout);

    app_init(&app);
    app_update_logic(&app);
    editor_input_init(&input_state);
    source_watch_init(&source_watch);

    load_path = source_watch_parse_load_path(argc, argv);
    if (load_path && source_watch_load_circuit(&app, load_path, "Loaded from file", frame_layout.canvas_rect)) {
        source_watch_start(&source_watch, load_path);
        app.source.live_reload = true;
    }

    while (!WindowShouldClose()) {
        WorkspaceResizeHandle hovered_resize_handle;
        Vector2 mouse_pos;

        workspace_layout_sanitize_prefs(&layout_prefs, GetScreenWidth(), GetScreenHeight());
        frame_layout = workspace_layout_compute_frame(&layout_prefs, GetScreenWidth(), GetScreenHeight());
        resize_handles = workspace_layout_compute_handles(&frame_layout);
        topbar_layout = topbar_compute_layout(&frame_layout);

        editor_input_process_frame(
            &app,
            &input_state,
            &layout_prefs,
            &frame_layout,
            &resize_handles,
            &topbar_layout,
            &source_watch
        );

        mouse_pos = GetMousePosition();
        hovered_resize_handle = workspace_layout_hit_test_handle(&resize_handles, mouse_pos);

        BeginDrawing();
        ClearBackground((Color){ 24, 24, 24, 255 });

        topbar_draw(&app, &topbar_layout, mouse_pos, editor_input_shortcuts_open(&input_state), frame_layout.window_width);

        if (app.mode == MODE_SOLVER) {
            Rectangle solver_rect;

            solver_rect = solver_workspace_rect(&frame_layout);
            if (begin_scissor_rect(solver_rect)) {
                ui_draw_solver_workspace(&app, solver_rect);
                EndScissorMode();
            }

            DrawRectangleRec(frame_layout.side_panel_rect, (Color){ 23, 23, 23, 255 });
            if (begin_scissor_rect(frame_layout.side_panel_rect)) {
                ui_draw_solver_side_panel(&app, frame_layout.side_panel_rect);
                EndScissorMode();
            }

            draw_resize_seam(resize_handles.side_panel, WORKSPACE_RESIZE_HANDLE_SIDE_PANEL);
            draw_resize_handle(
                resize_handles.side_panel,
                WORKSPACE_RESIZE_HANDLE_SIDE_PANEL,
                hovered_resize_handle == WORKSPACE_RESIZE_HANDLE_SIDE_PANEL,
                editor_input_active_resize_handle(&input_state) == WORKSPACE_RESIZE_HANDLE_SIDE_PANEL
            );
        } else {
            if (begin_scissor_rect(frame_layout.canvas_rect)) {
                ui_draw_circuit(&app, frame_layout.canvas_rect);
                ui_draw_placement_ghost(&app, frame_layout.canvas_rect, mouse_pos);
                EndScissorMode();
            }
            ui_draw_toolbox(&app, frame_layout.toolbox_rect);
            if (begin_scissor_rect(frame_layout.wave_rect)) {
                ui_draw_waveforms(&app, frame_layout.wave_rect);
                EndScissorMode();
            }

            DrawRectangleRec(frame_layout.side_panel_rect, (Color){ 23, 23, 23, 255 });
            if (begin_scissor_rect(frame_layout.side_panel_rect)) {
                ui_draw_context_panel(&app, frame_layout.side_panel_rect);
                EndScissorMode();
            }

            draw_resize_seam(resize_handles.toolbox, WORKSPACE_RESIZE_HANDLE_TOOLBOX);
            draw_resize_seam(resize_handles.side_panel, WORKSPACE_RESIZE_HANDLE_SIDE_PANEL);
            draw_resize_seam(resize_handles.wave_panel, WORKSPACE_RESIZE_HANDLE_WAVE_PANEL);

            draw_resize_handle(
                resize_handles.toolbox,
                WORKSPACE_RESIZE_HANDLE_TOOLBOX,
                hovered_resize_handle == WORKSPACE_RESIZE_HANDLE_TOOLBOX,
                editor_input_active_resize_handle(&input_state) == WORKSPACE_RESIZE_HANDLE_TOOLBOX
            );
            draw_resize_handle(
                resize_handles.side_panel,
                WORKSPACE_RESIZE_HANDLE_SIDE_PANEL,
                hovered_resize_handle == WORKSPACE_RESIZE_HANDLE_SIDE_PANEL,
                editor_input_active_resize_handle(&input_state) == WORKSPACE_RESIZE_HANDLE_SIDE_PANEL
            );
            draw_resize_handle(
                resize_handles.wave_panel,
                WORKSPACE_RESIZE_HANDLE_WAVE_PANEL,
                hovered_resize_handle == WORKSPACE_RESIZE_HANDLE_WAVE_PANEL,
                editor_input_active_resize_handle(&input_state) == WORKSPACE_RESIZE_HANDLE_WAVE_PANEL
            );
        }

        draw_footer(&app, frame_layout.footer_rect);
        if (editor_input_shortcuts_open(&input_state)) {
            bool shortcuts_open;

            shortcuts_open = true;
            draw_shortcuts_overlay(&frame_layout, mouse_pos, &shortcuts_open);
            if (!shortcuts_open) {
                input_state.shortcuts_open = false;
            }
        }

        EndDrawing();
    }

    workspace_layout_save_prefs(&layout_prefs);
    CloseWindow();
    return 0;
}

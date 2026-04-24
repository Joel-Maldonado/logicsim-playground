#include "app_analysis.h"
#include "app_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_apply_selected_row_to_inputs(AppContext *app) {
    uint32_t column_count;
    uint8_t input_index;

    if (!app || !app->analysis.truth_table) {
        return;
    }

    if (app->selection.selected_row >= app->analysis.truth_table->row_count) {
        app->selection.selected_row = 0U;
    }

    column_count = (uint32_t)app->analysis.truth_table->input_count + (uint32_t)app->analysis.truth_table->output_count;
    for (input_index = 0U; input_index < app->analysis.truth_table->input_count; input_index++) {
        app->analysis.truth_table->inputs[input_index]->outputs[0].value =
            app->analysis.truth_table->data[(app->selection.selected_row * column_count) + (uint32_t)input_index];
    }

    logic_evaluate(&app->graph);
    app_compute_view_context(app);
}

bool app_toggle_input_value(AppContext *app, LogicNode *node) {
    LogicValue next;

    if (!app || !node || node->type != NODE_INPUT || node->output_count == 0U) {
        return false;
    }

    next = (node->outputs[0].value == LOGIC_HIGH) ? LOGIC_LOW : LOGIC_HIGH;
    node->outputs[0].value = next;
    logic_evaluate(&app->graph);
    app_compute_view_context(app);
    return true;
}

void app_compute_view_context(AppContext *app) {
    ViewContext *view;
    uint32_t row;
    uint8_t input_index;

    if (!app) {
        return;
    }

    view = &app->selection.view;
    view->selected_node = app->selection.selected_node;
    view->row_valid = false;
    view->output_valid = false;
    view->live_row_index = 0U;
    view->live_output = LOGIC_UNKNOWN;

    if (logic_node_is_active(app->selection.selected_node)) {
        LogicNode *node;

        node = app->selection.selected_node;
        if (node->type == NODE_OUTPUT && node->input_count > 0U) {
            view->live_output = node->inputs[0].value;
            view->output_valid = true;
        } else if (node->output_count > 0U) {
            view->live_output = node->outputs[0].value;
            view->output_valid = true;
        }
    }

    if (!app->analysis.truth_table || app->analysis.truth_table->input_count == 0U) {
        return;
    }

    row = 0U;
    for (input_index = 0U; input_index < app->analysis.truth_table->input_count; input_index++) {
        LogicNode *input_node;
        uint8_t bit_index;

        input_node = app->analysis.truth_table->inputs[input_index];
        if (!input_node || input_node->output_count == 0U) {
            return;
        }
        if (input_node->outputs[0].value != LOGIC_LOW && input_node->outputs[0].value != LOGIC_HIGH) {
            return;
        }

        bit_index = (uint8_t)(app->analysis.truth_table->input_count - 1U - input_index);
        if (input_node->outputs[0].value == LOGIC_HIGH) {
            row |= (1U << bit_index);
        }
    }

    if (row < app->analysis.truth_table->row_count) {
        view->live_row_index = row;
        view->row_valid = true;
    }
}

void app_compare_with_target(AppContext *app, LogicGraph *target) {
    TruthTable *target_table;
    uint32_t column_count;
    uint32_t target_column_count;
    uint32_t row_index;
    uint8_t output_index;

    app->comparison.target_graph = target;
    app->comparison.equivalent = true;
    app->comparison.status = APP_COMPARE_EQUIVALENT;
    app->comparison.divergence_node = NULL;
    app->comparison.first_failing_row = 0U;

    target_table = logic_generate_truth_table(target);
    if (!target_table || !app->analysis.truth_table) {
        if (target_table) {
            logic_free_truth_table(target_table);
        }
        app->comparison.status = APP_COMPARE_NO_TARGET;
        app->comparison.equivalent = false;
        return;
    }

    column_count = (uint32_t)app->analysis.truth_table->input_count + (uint32_t)app->analysis.truth_table->output_count;
    target_column_count = (uint32_t)target_table->input_count + (uint32_t)target_table->output_count;

    if (app->analysis.truth_table->input_count != target_table->input_count ||
        app->analysis.truth_table->output_count != target_table->output_count) {
        app->comparison.equivalent = false;
        app->comparison.status = APP_COMPARE_MISMATCH;
    } else {
        for (row_index = 0U; row_index < app->analysis.truth_table->row_count; row_index++) {
            bool row_matches;

            row_matches = true;
            for (output_index = 0U; output_index < app->analysis.truth_table->output_count; output_index++) {
                if (app->analysis.truth_table->data[(row_index * column_count) + (uint32_t)app->analysis.truth_table->input_count + (uint32_t)output_index] !=
                    target_table->data[(row_index * target_column_count) + (uint32_t)target_table->input_count + (uint32_t)output_index]) {
                    row_matches = false;
                    break;
                }
            }
            if (!row_matches) {
                app->comparison.equivalent = false;
                app->comparison.status = APP_COMPARE_MISMATCH;
                app->comparison.first_failing_row = row_index;
                break;
            }
        }
    }

    logic_free_truth_table(target_table);
}

void app_compare_if_needed(AppContext *app) {
    if (!app->comparison.target_graph) {
        app->comparison.equivalent = false;
        app->comparison.status = APP_COMPARE_NO_TARGET;
        app->comparison.divergence_node = NULL;
        app->comparison.first_failing_row = 0U;
        return;
    }

    app_compare_with_target(app, app->comparison.target_graph);
}

char *app_get_node_explanation(AppContext *app, LogicNode *node) {
    char *buffer;
    LogicValue output_value;
    const char *output_text;

    (void)app;
    if (!logic_node_is_active(node)) {
        return NULL;
    }

    buffer = (char *)calloc(256U, sizeof(char));
    if (!buffer) {
        return NULL;
    }

    output_value = (node->output_count > 0U) ? node->outputs[0].value : node->inputs[0].value;
    output_text = (output_value == LOGIC_HIGH) ? "1" : "0";

    if (node->type == NODE_GATE_AND) {
        snprintf(buffer, 256U, (output_value == LOGIC_HIGH)
            ? "Output is 1 because all inputs are 1."
            : "Output is 0 because at least one input is 0.");
    } else if (node->type == NODE_GATE_OR) {
        snprintf(buffer, 256U, (output_value == LOGIC_HIGH)
            ? "Output is 1 because at least one input is 1."
            : "Output is 0 because all inputs are 0.");
    } else if (node->type == NODE_GATE_NOT) {
        snprintf(buffer, 256U, "NOT gate inverts input to %s.", output_text);
    } else if (node->type == NODE_GATE_DFF) {
        if (output_value == LOGIC_ERROR) {
            snprintf(buffer, 256U, "POSSIBLE RACE CONDITION: Input D changed at the exact same tick as the rising edge of CLK.");
        } else {
            snprintf(buffer, 256U, "DFF stores state %s (latched on rising edge of CLK).", output_text);
        }
    } else if (node->type == NODE_GATE_CLOCK) {
        snprintf(buffer, 256U, "Clock toggles on every tick.");
    } else if (node->type == NODE_INPUT) {
        snprintf(buffer, 256U, "Input node fixed at %s.", output_text);
    } else if (node->type == NODE_OUTPUT) {
        snprintf(buffer, 256U, "Output reflects its incoming pin %s.", output_text);
    } else if (node->type == NODE_GATE_XOR) {
        snprintf(buffer, 256U, "XOR gate outputs 1 if inputs differ.");
    } else if (node->type == NODE_GATE_NAND) {
        snprintf(buffer, 256U, "NAND gate outputs 0 only if all inputs are 1.");
    } else if (node->type == NODE_GATE_NOR) {
        snprintf(buffer, 256U, "NOR gate outputs 1 only if all inputs are 0.");
    } else if (node->type == NODE_GATE_LATCH) {
        snprintf(buffer, 256U, "LATCH stores state while enabled.");
    }

    return buffer;
}

void app_update_kmap_grouping(AppContext *app) {
    static const Color colors[] = {
        { 255, 0, 0, 100 },
        { 0, 255, 0, 100 },
        { 0, 0, 255, 100 },
        { 255, 165, 0, 100 }
    };
    static const uint8_t groups2_masks[] = { 0x5U, 0xAU, 0x3U, 0xCU };
    static const char *groups2_terms[] = { "B'", "B", "A'", "A" };
    static const char *groups1_terms[] = { "A'B'", "A'B", "AB'", "AB" };
    char buffer[256];
    uint32_t column_count;
    uint32_t table_bits;
    uint32_t index;
    uint8_t covered;

    app->analysis.kmap_group_count = 0U;
    free(app->analysis.simplified_expression);
    app->analysis.simplified_expression = NULL;
    memset(buffer, 0, sizeof(buffer));

    if (!app->analysis.truth_table || app->analysis.truth_table->input_count != 2U) {
        return;
    }

    column_count = (uint32_t)app->analysis.truth_table->input_count + (uint32_t)app->analysis.truth_table->output_count;
    table_bits = 0U;
    covered = 0U;
    for (index = 0U; index < 4U; index++) {
        uint32_t cell_index;

        cell_index = (index * column_count) + 2U;
        if (app->analysis.truth_table->data[cell_index] == LOGIC_HIGH) {
            table_bits |= (1U << index);
        }
    }

    if (table_bits == 0U) {
        app->analysis.simplified_expression = strdup("0");
        return;
    }
    if (table_bits == 0xFU) {
        KMapGroup *group;

        group = &app->analysis.kmap_groups[app->analysis.kmap_group_count++];
        group->cell_mask = 0xFU;
        strcpy(group->term, "1");
        group->color = colors[0];
        app->analysis.simplified_expression = strdup("1");
        return;
    }

    for (index = 0U; index < 4U; index++) {
        uint32_t uncovered_mask;

        if ((table_bits & groups2_masks[index]) != groups2_masks[index]) {
            continue;
        }

        uncovered_mask = table_bits & (uint32_t)groups2_masks[index] & (uint32_t)(uint8_t)(~covered);
        if (uncovered_mask != 0U) {
            KMapGroup *group;

            group = &app->analysis.kmap_groups[app->analysis.kmap_group_count++];
            group->cell_mask = groups2_masks[index];
            strcpy(group->term, groups2_terms[index]);
            group->color = colors[app->analysis.kmap_group_count % 4U];
            covered = (uint8_t)(covered | groups2_masks[index]);
        }
    }

    for (index = 0U; index < 4U; index++) {
        uint8_t cell_mask;

        cell_mask = (uint8_t)(1U << index);
        if ((table_bits & (uint32_t)cell_mask) == 0U || (covered & cell_mask) != 0U) {
            continue;
        }

        {
            KMapGroup *group;

            group = &app->analysis.kmap_groups[app->analysis.kmap_group_count++];
            group->cell_mask = cell_mask;
            strcpy(group->term, groups1_terms[index]);
            group->color = colors[app->analysis.kmap_group_count % 4U];
            covered = (uint8_t)(covered | cell_mask);
        }
    }

    for (index = 0U; index < app->analysis.kmap_group_count; index++) {
        if (index > 0U) {
            strcat(buffer, " OR ");
        }
        strcat(buffer, app->analysis.kmap_groups[index].term);
    }
    app->analysis.simplified_expression = strdup(buffer);
}

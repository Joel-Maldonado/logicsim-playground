#include "circuit_layout.h"
#include "node_catalog.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define LAYOUT_GRID 20.0f
#define LAYOUT_LAYER_GAP 200.0f
#define LAYOUT_VERTICAL_GAP 60.0f
#define LAYOUT_FANOUT_STAGGER 40.0f
#define LAYOUT_COMPONENT_GAP 180.0f
#define LAYOUT_CANVAS_LEFT 160.0f
#define LAYOUT_CANVAS_TOP 160.0f
#define LAYOUT_ORDER_SWEEPS 6U
#define LAYOUT_POSITION_PASSES 8U

typedef struct {
    uint32_t source_index;
    uint32_t sink_index;
    uint8_t source_pin_index;
    uint8_t sink_pin_index;
    uint8_t _padding[2];
} LayoutEdge;

typedef struct {
    uint32_t source_scc;
    uint32_t sink_scc;
} SccEdge;

typedef struct {
    uint32_t node_indices[MAX_NODES];
    uint32_t node_count;
} GraphComponent;

typedef struct {
    const CircuitLayoutNode *nodes;
    uint32_t node_indices[MAX_NODES];
    LayoutEdge edges[MAX_NETS];
    uint32_t node_count;
    uint32_t edge_count;
    int32_t local_index_by_global[MAX_NODES];
    uint32_t indegree[MAX_NODES];
    uint32_t outdegree[MAX_NODES];
    int widths[MAX_NODES];
    int heights[MAX_NODES];
    uint32_t scc_of_node[MAX_NODES];
    uint32_t scc_member_count[MAX_NODES];
    uint32_t node_rank[MAX_NODES];
    uint32_t ordered_nodes[MAX_NODES];
    uint32_t layer_offsets[MAX_NODES + 1U];
    float node_y[MAX_NODES];
    float node_x_offset[MAX_NODES];
    uint32_t max_rank;
} LayoutComponent;

typedef struct {
    float barycenter;
    bool has_barycenter;
    uint8_t _padding[3];
} BarycenterKey;

static float layout_snap(float value) {
    return floorf((value / LAYOUT_GRID) + 0.5f) * LAYOUT_GRID;
}

static bool layout_node_is_input_side(NodeType type) {
    return type == NODE_INPUT || type == NODE_GATE_CLOCK;
}

static bool layout_node_is_output_side(NodeType type) {
    return type == NODE_OUTPUT;
}

static int layout_node_role_priority(NodeType type) {
    if (layout_node_is_input_side(type)) {
        return 0;
    }
    if (layout_node_is_output_side(type)) {
        return 2;
    }

    return 1;
}

static void collect_graph_components(
    uint32_t node_count,
    const CircuitLayoutEdge *edges,
    uint32_t edge_count,
    GraphComponent *components,
    uint32_t *component_count
) {
    int32_t component_by_node[MAX_NODES];
    uint32_t queue[MAX_NODES];
    uint32_t next_component;
    uint32_t node_index;

    memset(component_by_node, 0xff, sizeof(component_by_node));
    next_component = 0U;

    for (node_index = 0U; node_index < node_count; node_index++) {
        GraphComponent *component;
        uint32_t queue_head;
        uint32_t queue_tail;

        if (component_by_node[node_index] >= 0) {
            continue;
        }

        component = &components[next_component];
        memset(component, 0, sizeof(*component));

        queue_head = 0U;
        queue_tail = 0U;
        queue[queue_tail++] = node_index;
        component_by_node[node_index] = (int32_t)next_component;

        while (queue_head < queue_tail) {
            uint32_t current;
            uint32_t edge_index;

            current = queue[queue_head++];
            component->node_indices[component->node_count++] = current;

            for (edge_index = 0U; edge_index < edge_count; edge_index++) {
                uint32_t neighbor;

                neighbor = UINT32_MAX;
                if (edges[edge_index].source_node_index == current) {
                    neighbor = edges[edge_index].sink_node_index;
                } else if (edges[edge_index].sink_node_index == current) {
                    neighbor = edges[edge_index].source_node_index;
                }

                if (neighbor == UINT32_MAX || component_by_node[neighbor] >= 0) {
                    continue;
                }

                component_by_node[neighbor] = (int32_t)next_component;
                queue[queue_tail++] = neighbor;
            }
        }

        next_component++;
    }
    *component_count = next_component;
}

static float layout_pin_offset(uint8_t pin_count, float node_height, uint8_t pin_index) {
    float pitch;

    if (pin_count <= 1U) {
        return node_height * 0.5f;
    }

    pitch = (node_height - (LAYOUT_GRID * 2.0f)) / (float)(pin_count - 1U);
    return LAYOUT_GRID + ((float)pin_index * pitch);
}

static int layout_compare_local_nodes(const LayoutComponent *component, uint32_t left, uint32_t right) {
    uint32_t left_global;
    uint32_t right_global;
    int role_delta;

    left_global = component->node_indices[left];
    right_global = component->node_indices[right];

    if (component->node_rank[left] != component->node_rank[right]) {
        return component->node_rank[left] < component->node_rank[right] ? -1 : 1;
    }

    role_delta =
        layout_node_role_priority(component->nodes[left_global].type) -
        layout_node_role_priority(component->nodes[right_global].type);
    if (role_delta != 0) {
        return role_delta;
    }

    if (component->indegree[left] != component->indegree[right]) {
        return component->indegree[left] > component->indegree[right] ? -1 : 1;
    }
    if (component->outdegree[left] != component->outdegree[right]) {
        return component->outdegree[left] > component->outdegree[right] ? -1 : 1;
    }
    if (component->nodes[left_global].input_count != component->nodes[right_global].input_count) {
        return component->nodes[left_global].input_count > component->nodes[right_global].input_count ? -1 : 1;
    }

    if (left_global < right_global) {
        return -1;
    }
    if (left_global > right_global) {
        return 1;
    }

    return 0;
}

static void layout_component_build(
    LayoutComponent *component,
    const CircuitLayoutNode *nodes,
    const CircuitLayoutEdge *edges,
    uint32_t edge_count,
    const GraphComponent *graph_component
) {
    uint32_t local_index;
    uint32_t edge_index;

    memset(component, 0, sizeof(*component));
    component->nodes = nodes;
    component->node_count = graph_component->node_count;
    memset(component->local_index_by_global, 0xff, sizeof(component->local_index_by_global));

    for (local_index = 0U; local_index < component->node_count; local_index++) {
        uint32_t global_index;

        global_index = graph_component->node_indices[local_index];
        component->node_indices[local_index] = global_index;
        component->local_index_by_global[global_index] = (int32_t)local_index;
        component->widths[local_index] = node_catalog_width(nodes[global_index].type);
        component->heights[local_index] = (int)(LAYOUT_GRID * 2.0f * node_catalog_pin_rows(nodes[global_index].type));
    }

    for (edge_index = 0U; edge_index < edge_count; edge_index++) {
        int32_t source_local;
        int32_t sink_local;
        LayoutEdge *local_edge;

        source_local = component->local_index_by_global[edges[edge_index].source_node_index];
        sink_local = component->local_index_by_global[edges[edge_index].sink_node_index];
        if (source_local < 0 || sink_local < 0) {
            continue;
        }

        local_edge = &component->edges[component->edge_count++];
        local_edge->source_index = (uint32_t)source_local;
        local_edge->source_pin_index = edges[edge_index].source_pin_index;
        local_edge->sink_index = (uint32_t)sink_local;
        local_edge->sink_pin_index = edges[edge_index].sink_pin_index;
        component->indegree[(uint32_t)sink_local]++;
        component->outdegree[(uint32_t)source_local]++;
    }
}

static const char *layout_component_scc_name(
    const LayoutComponent *component,
    const uint32_t *scc_of_node,
    uint32_t scc_index
) {
    const char *name;
    uint32_t local_index;

    name = NULL;
    for (local_index = 0U; local_index < component->node_count; local_index++) {
        if (scc_of_node[local_index] != scc_index) {
            continue;
        }

        if (!name || strcmp(component->nodes[component->node_indices[local_index]].name, name) < 0) {
            name = component->nodes[component->node_indices[local_index]].name;
        }
    }

    return name;
}

static void layout_component_strongconnect(
    LayoutComponent *component,
    uint32_t local_node,
    int32_t *indices,
    int32_t *lowlink,
    bool *on_stack,
    uint32_t *stack,
    uint32_t *stack_size,
    int32_t *next_index,
    uint32_t *scc_count
) {
    uint32_t edge_index;

    indices[local_node] = *next_index;
    lowlink[local_node] = *next_index;
    (*next_index)++;
    stack[(*stack_size)++] = local_node;
    on_stack[local_node] = true;

    for (edge_index = 0U; edge_index < component->edge_count; edge_index++) {
        uint32_t neighbor;

        if (component->edges[edge_index].source_index != local_node) {
            continue;
        }

        neighbor = component->edges[edge_index].sink_index;
        if (indices[neighbor] < 0) {
            layout_component_strongconnect(
                component,
                neighbor,
                indices,
                lowlink,
                on_stack,
                stack,
                stack_size,
                next_index,
                scc_count
            );
            if (lowlink[neighbor] < lowlink[local_node]) {
                lowlink[local_node] = lowlink[neighbor];
            }
            continue;
        }

        if (on_stack[neighbor] && indices[neighbor] < lowlink[local_node]) {
            lowlink[local_node] = indices[neighbor];
        }
    }

    if (lowlink[local_node] == indices[local_node]) {
        while (*stack_size > 0U) {
            uint32_t member;

            member = stack[--(*stack_size)];
            on_stack[member] = false;
            component->scc_of_node[member] = *scc_count;
            component->scc_member_count[*scc_count]++;
            if (member == local_node) {
                break;
            }
        }
        (*scc_count)++;
    }
}

static uint32_t layout_component_find_sccs(LayoutComponent *component) {
    int32_t indices[MAX_NODES];
    int32_t lowlink[MAX_NODES];
    bool on_stack[MAX_NODES];
    uint32_t stack[MAX_NODES];
    uint32_t stack_size;
    int32_t next_index;
    uint32_t scc_count;
    uint32_t local_index;

    memset(indices, 0xff, sizeof(indices));
    memset(lowlink, 0, sizeof(lowlink));
    memset(on_stack, 0, sizeof(on_stack));
    memset(component->scc_member_count, 0, sizeof(component->scc_member_count));

    stack_size = 0U;
    next_index = 0;
    scc_count = 0U;

    for (local_index = 0U; local_index < component->node_count; local_index++) {
        if (indices[local_index] >= 0) {
            continue;
        }

        layout_component_strongconnect(
            component,
            local_index,
            indices,
            lowlink,
            on_stack,
            stack,
            &stack_size,
            &next_index,
            &scc_count
        );
    }

    return scc_count;
}

static int layout_compare_scc(
    const LayoutComponent *component,
    const uint32_t *scc_of_node,
    uint32_t left_scc,
    uint32_t right_scc
) {
    const char *left_name;
    const char *right_name;

    left_name = layout_component_scc_name(component, scc_of_node, left_scc);
    right_name = layout_component_scc_name(component, scc_of_node, right_scc);

    return strcmp(left_name, right_name);
}

static void layout_component_assign_ranks(LayoutComponent *component) {
    SccEdge condensed_edges[MAX_NETS];
    uint32_t scc_indegree[MAX_NODES];
    uint32_t scc_start_rank[MAX_NODES];
    uint32_t scc_span[MAX_NODES];
    uint32_t scc_count;
    uint32_t condensed_edge_count;
    uint32_t edge_index;
    uint32_t scc_index;
    uint32_t processed_sccs;
    bool processed[MAX_NODES];
    bool has_output;
    uint32_t rightmost_output_rank;
    uint32_t local_index;

    scc_count = layout_component_find_sccs(component);
    memset(scc_indegree, 0, sizeof(scc_indegree));
    memset(scc_start_rank, 0, sizeof(scc_start_rank));
    memset(scc_span, 0, sizeof(scc_span));
    memset(processed, 0, sizeof(processed));
    condensed_edge_count = 0U;

    for (scc_index = 0U; scc_index < scc_count; scc_index++) {
        scc_span[scc_index] = component->scc_member_count[scc_index];
    }

    for (edge_index = 0U; edge_index < component->edge_count; edge_index++) {
        uint32_t source_scc;
        uint32_t sink_scc;
        bool already_present;
        uint32_t condensed_index;

        source_scc = component->scc_of_node[component->edges[edge_index].source_index];
        sink_scc = component->scc_of_node[component->edges[edge_index].sink_index];
        if (source_scc == sink_scc) {
            continue;
        }

        already_present = false;
        for (condensed_index = 0U; condensed_index < condensed_edge_count; condensed_index++) {
            if (condensed_edges[condensed_index].source_scc == source_scc &&
                condensed_edges[condensed_index].sink_scc == sink_scc) {
                already_present = true;
                break;
            }
        }
        if (already_present) {
            continue;
        }

        condensed_edges[condensed_edge_count].source_scc = source_scc;
        condensed_edges[condensed_edge_count].sink_scc = sink_scc;
        condensed_edge_count++;
        scc_indegree[sink_scc]++;
    }

    for (processed_sccs = 0U; processed_sccs < scc_count; processed_sccs++) {
        uint32_t next_scc;

        next_scc = UINT32_MAX;
        for (scc_index = 0U; scc_index < scc_count; scc_index++) {
            if (processed[scc_index] || scc_indegree[scc_index] != 0U) {
                continue;
            }
            if (next_scc == UINT32_MAX ||
                layout_compare_scc(component, component->scc_of_node, scc_index, next_scc) < 0) {
                next_scc = scc_index;
            }
        }

        if (next_scc == UINT32_MAX) {
            break;
        }

        processed[next_scc] = true;
        for (edge_index = 0U; edge_index < condensed_edge_count; edge_index++) {
            uint32_t sink_scc;
            uint32_t candidate_rank;

            if (condensed_edges[edge_index].source_scc != next_scc) {
                continue;
            }

            sink_scc = condensed_edges[edge_index].sink_scc;
            candidate_rank = scc_start_rank[next_scc] + scc_span[next_scc];
            if (candidate_rank > scc_start_rank[sink_scc]) {
                scc_start_rank[sink_scc] = candidate_rank;
            }
            if (scc_indegree[sink_scc] > 0U) {
                scc_indegree[sink_scc]--;
            }
        }
    }

    for (scc_index = 0U; scc_index < scc_count; scc_index++) {
        uint32_t members[MAX_NODES];
        uint32_t member_count;
        uint32_t member_index;

        member_count = 0U;
        for (local_index = 0U; local_index < component->node_count; local_index++) {
            if (component->scc_of_node[local_index] == scc_index) {
                members[member_count++] = local_index;
            }
        }

        for (member_index = 1U; member_index < member_count; member_index++) {
            uint32_t current;
            uint32_t insert_at;

            current = members[member_index];
            insert_at = member_index;
            while (insert_at > 0U && layout_compare_local_nodes(component, current, members[insert_at - 1U]) < 0) {
                members[insert_at] = members[insert_at - 1U];
                insert_at--;
            }
            members[insert_at] = current;
        }

        for (member_index = 0U; member_index < member_count; member_index++) {
            component->node_rank[members[member_index]] = scc_start_rank[scc_index] + member_index;
        }
    }

    has_output = false;
    rightmost_output_rank = 0U;
    component->max_rank = 0U;
    for (local_index = 0U; local_index < component->node_count; local_index++) {
        uint32_t global_index;

        global_index = component->node_indices[local_index];
        if (layout_node_is_output_side(component->nodes[global_index].type)) {
            if (!has_output || component->node_rank[local_index] > rightmost_output_rank) {
                rightmost_output_rank = component->node_rank[local_index];
            }
            has_output = true;
        }
        if (component->node_rank[local_index] > component->max_rank) {
            component->max_rank = component->node_rank[local_index];
        }
    }

    if (has_output) {
        for (local_index = 0U; local_index < component->node_count; local_index++) {
            uint32_t global_index;

            global_index = component->node_indices[local_index];
            if (layout_node_is_output_side(component->nodes[global_index].type)) {
                component->node_rank[local_index] = rightmost_output_rank;
            }
            if (component->node_rank[local_index] > component->max_rank) {
                component->max_rank = component->node_rank[local_index];
            }
        }
    }
}

static void layout_component_rebuild_layers(LayoutComponent *component) {
    uint32_t local_nodes[MAX_NODES];
    uint32_t rank_counts[MAX_NODES];
    uint32_t next_slot[MAX_NODES];
    uint32_t local_index;
    uint32_t rank_index;

    memset(rank_counts, 0, sizeof(rank_counts));
    for (local_index = 0U; local_index < component->node_count; local_index++) {
        local_nodes[local_index] = local_index;
        rank_counts[component->node_rank[local_index]]++;
    }

    for (local_index = 1U; local_index < component->node_count; local_index++) {
        uint32_t current;
        uint32_t insert_at;

        current = local_nodes[local_index];
        insert_at = local_index;
        while (insert_at > 0U && layout_compare_local_nodes(component, current, local_nodes[insert_at - 1U]) < 0) {
            local_nodes[insert_at] = local_nodes[insert_at - 1U];
            insert_at--;
        }
        local_nodes[insert_at] = current;
    }

    component->layer_offsets[0] = 0U;
    for (rank_index = 0U; rank_index <= component->max_rank; rank_index++) {
        component->layer_offsets[rank_index + 1U] = component->layer_offsets[rank_index] + rank_counts[rank_index];
        next_slot[rank_index] = component->layer_offsets[rank_index];
    }

    for (local_index = 0U; local_index < component->node_count; local_index++) {
        uint32_t node_index;
        uint32_t rank;

        node_index = local_nodes[local_index];
        rank = component->node_rank[node_index];
        component->ordered_nodes[next_slot[rank]++] = node_index;
    }
}

static void layout_component_update_layer_positions(const LayoutComponent *component, uint32_t *layer_position) {
    uint32_t rank_index;

    for (rank_index = 0U; rank_index <= component->max_rank; rank_index++) {
        uint32_t start;
        uint32_t end;
        uint32_t order_index;

        start = component->layer_offsets[rank_index];
        end = component->layer_offsets[rank_index + 1U];
        for (order_index = start; order_index < end; order_index++) {
            layer_position[component->ordered_nodes[order_index]] = order_index - start;
        }
    }
}

static BarycenterKey layout_component_barycenter_key(
    const LayoutComponent *component,
    uint32_t local_node,
    const uint32_t *layer_position,
    bool use_predecessors
) {
    BarycenterKey key;
    uint32_t edge_index;
    float sum;
    uint32_t count;

    key.has_barycenter = false;
    key.barycenter = 0.0f;
    sum = 0.0f;
    count = 0U;

    for (edge_index = 0U; edge_index < component->edge_count; edge_index++) {
        uint32_t neighbor;

        if (use_predecessors) {
            if (component->edges[edge_index].sink_index != local_node) {
                continue;
            }
            neighbor = component->edges[edge_index].source_index;
            if (component->node_rank[neighbor] >= component->node_rank[local_node]) {
                continue;
            }
        } else {
            if (component->edges[edge_index].source_index != local_node) {
                continue;
            }
            neighbor = component->edges[edge_index].sink_index;
            if (component->node_rank[neighbor] <= component->node_rank[local_node]) {
                continue;
            }
        }

        sum += (float)layer_position[neighbor];
        count++;
    }

    if (count > 0U) {
        key.has_barycenter = true;
        key.barycenter = sum / (float)count;
    }

    return key;
}

static int layout_compare_layer_nodes(
    const LayoutComponent *component,
    uint32_t left,
    uint32_t right,
    BarycenterKey left_key,
    BarycenterKey right_key
) {
    if (left_key.has_barycenter != right_key.has_barycenter) {
        return left_key.has_barycenter ? -1 : 1;
    }

    if (left_key.has_barycenter && right_key.has_barycenter) {
        if (left_key.barycenter < right_key.barycenter - 0.001f) {
            return -1;
        }
        if (left_key.barycenter > right_key.barycenter + 0.001f) {
            return 1;
        }
    }

    return layout_compare_local_nodes(component, left, right);
}

static void layout_component_sweep_order(LayoutComponent *component, bool use_predecessors) {
    uint32_t layer_position[MAX_NODES];
    uint32_t rank_index;
    uint32_t start_rank;
    uint32_t end_rank;
    int step;

    memset(layer_position, 0, sizeof(layer_position));
    layout_component_update_layer_positions(component, layer_position);
    if (use_predecessors) {
        start_rank = 1U;
        end_rank = component->max_rank + 1U;
        step = 1;
    } else {
        start_rank = component->max_rank;
        end_rank = 0U;
        step = -1;
    }

    rank_index = start_rank;
    while ((step > 0 && rank_index < end_rank) || (step < 0 && rank_index > end_rank)) {
        uint32_t start;
        uint32_t end;
        uint32_t layer_size;
        uint32_t order_index;
        BarycenterKey keys[MAX_NODES];

        start = component->layer_offsets[rank_index];
        end = component->layer_offsets[rank_index + 1U];
        layer_size = end - start;
        if (layer_size > 1U) {
            for (order_index = start; order_index < end; order_index++) {
                keys[order_index - start] = layout_component_barycenter_key(
                    component,
                    component->ordered_nodes[order_index],
                    layer_position,
                    use_predecessors
                );
            }

            for (order_index = 1U; order_index < layer_size; order_index++) {
                uint32_t current_node;
                BarycenterKey current_key;
                uint32_t insert_at;

                current_node = component->ordered_nodes[start + order_index];
                current_key = keys[order_index];
                insert_at = order_index;
                while (insert_at > 0U &&
                       layout_compare_layer_nodes(
                           component,
                           current_node,
                           component->ordered_nodes[start + insert_at - 1U],
                           current_key,
                           keys[insert_at - 1U]
                       ) < 0) {
                    component->ordered_nodes[start + insert_at] = component->ordered_nodes[start + insert_at - 1U];
                    keys[insert_at] = keys[insert_at - 1U];
                    insert_at--;
                }
                component->ordered_nodes[start + insert_at] = current_node;
                keys[insert_at] = current_key;
            }

            for (order_index = start; order_index < end; order_index++) {
                layer_position[component->ordered_nodes[order_index]] = order_index - start;
            }
        }

        rank_index = (uint32_t)((int)rank_index + step);
    }
}

static float layout_component_desired_y(const LayoutComponent *component, uint32_t local_node) {
    float sum;
    uint32_t weight;
    uint32_t edge_index;
    float own_input_offset;
    float own_output_offset;

    sum = 0.0f;
    weight = 0U;

    for (edge_index = 0U; edge_index < component->edge_count; edge_index++) {
        const LayoutEdge *edge;
        float neighbor_offset;

        edge = &component->edges[edge_index];
        if (edge->sink_index == local_node) {
            own_input_offset = layout_pin_offset(
                component->nodes[component->node_indices[local_node]].input_count,
                (float)component->heights[local_node],
                edge->sink_pin_index
            );
            neighbor_offset = layout_pin_offset(
                component->nodes[component->node_indices[edge->source_index]].output_count,
                (float)component->heights[edge->source_index],
                edge->source_pin_index
            );
            sum += component->node_y[edge->source_index] + neighbor_offset - own_input_offset;
            weight++;
        }

        if (edge->source_index == local_node) {
            own_output_offset = layout_pin_offset(
                component->nodes[component->node_indices[local_node]].output_count,
                (float)component->heights[local_node],
                edge->source_pin_index
            );
            neighbor_offset = layout_pin_offset(
                component->nodes[component->node_indices[edge->sink_index]].input_count,
                (float)component->heights[edge->sink_index],
                edge->sink_pin_index
            );
            sum += component->node_y[edge->sink_index] + neighbor_offset - own_output_offset;
            weight++;
        }
    }

    if (weight == 0U) {
        return component->node_y[local_node];
    }

    return sum / (float)weight;
}

static void layout_component_place_layer(LayoutComponent *component, uint32_t rank_index, const float *desired_y) {
    uint32_t start;
    uint32_t end;
    uint32_t order_index;
    float shift_sum;
    float layer_shift;

    start = component->layer_offsets[rank_index];
    end = component->layer_offsets[rank_index + 1U];
    if (start == end) {
        return;
    }

    for (order_index = start; order_index < end; order_index++) {
        uint32_t local_node;
        float y;

        local_node = component->ordered_nodes[order_index];
        y = layout_snap(desired_y[local_node]);
        if (order_index > start) {
            uint32_t previous_node;
            float min_y;

            previous_node = component->ordered_nodes[order_index - 1U];
            min_y = component->node_y[previous_node] + (float)component->heights[previous_node] + LAYOUT_VERTICAL_GAP;
            if (y < min_y) {
                y = min_y;
            }
        }
        component->node_y[local_node] = y;
    }

    shift_sum = 0.0f;
    for (order_index = start; order_index < end; order_index++) {
        uint32_t local_node;

        local_node = component->ordered_nodes[order_index];
        shift_sum += desired_y[local_node] - component->node_y[local_node];
    }

    layer_shift = layout_snap(shift_sum / (float)(end - start));
    if (fabsf(layer_shift) < 0.001f) {
        return;
    }

    for (order_index = start; order_index < end; order_index++) {
        uint32_t local_node;

        local_node = component->ordered_nodes[order_index];
        component->node_y[local_node] += layer_shift;
    }

    for (order_index = start + 1U; order_index < end; order_index++) {
        uint32_t previous_node;
        uint32_t local_node;
        float min_y;

        previous_node = component->ordered_nodes[order_index - 1U];
        local_node = component->ordered_nodes[order_index];
        min_y = component->node_y[previous_node] + (float)component->heights[previous_node] + LAYOUT_VERTICAL_GAP;
        if (component->node_y[local_node] < min_y) {
            component->node_y[local_node] = min_y;
        }
    }
}

static void layout_component_place_nodes(LayoutComponent *component) {
    float desired_y[MAX_NODES];
    uint32_t rank_index;
    uint32_t order_index;
    uint32_t pass_index;

    for (rank_index = 0U; rank_index <= component->max_rank; rank_index++) {
        float cursor_y;
        uint32_t start;
        uint32_t end;

        cursor_y = 0.0f;
        start = component->layer_offsets[rank_index];
        end = component->layer_offsets[rank_index + 1U];
        for (order_index = start; order_index < end; order_index++) {
            uint32_t local_node;

            local_node = component->ordered_nodes[order_index];
            component->node_y[local_node] = cursor_y;
            cursor_y += (float)component->heights[local_node] + LAYOUT_VERTICAL_GAP;
        }
    }

    for (pass_index = 0U; pass_index < LAYOUT_POSITION_PASSES; pass_index++) {
        for (order_index = 0U; order_index < component->node_count; order_index++) {
            desired_y[order_index] = layout_component_desired_y(component, order_index);
        }
        for (rank_index = 0U; rank_index <= component->max_rank; rank_index++) {
            layout_component_place_layer(component, rank_index, desired_y);
        }
    }
}

static float layout_component_fanout_offset(
    const LayoutComponent *component,
    uint32_t local_node,
    const uint32_t *layer_position
) {
    uint32_t target_rank;
    float offset_sum;
    uint32_t contributing_groups;
    uint32_t source_index;

    target_rank = component->node_rank[local_node];
    offset_sum = 0.0f;
    contributing_groups = 0U;

    for (source_index = 0U; source_index < component->node_count; source_index++) {
        uint32_t predecessor_index;
        bool same_predecessors;
        bool saw_predecessor;

        if (source_index == local_node || component->node_rank[source_index] != target_rank) {
            continue;
        }

        same_predecessors = true;
        saw_predecessor = false;
        for (predecessor_index = 0U; predecessor_index < component->node_count; predecessor_index++) {
            uint32_t edge_index;
            bool local_has_predecessor;
            bool sibling_has_predecessor;

            if (component->node_rank[predecessor_index] >= target_rank) {
                continue;
            }

            local_has_predecessor = false;
            sibling_has_predecessor = false;
            for (edge_index = 0U; edge_index < component->edge_count; edge_index++) {
                if (component->edges[edge_index].source_index != predecessor_index) {
                    continue;
                }
                if (component->edges[edge_index].sink_index == local_node) {
                    local_has_predecessor = true;
                }
                if (component->edges[edge_index].sink_index == source_index) {
                    sibling_has_predecessor = true;
                }
            }

            if (!local_has_predecessor && !sibling_has_predecessor) {
                continue;
            }

            saw_predecessor = true;
            if (local_has_predecessor != sibling_has_predecessor) {
                same_predecessors = false;
                break;
            }
        }

        if (same_predecessors && saw_predecessor) {
            return 0.0f;
        }
    }

    for (source_index = 0U; source_index < component->node_count; source_index++) {
        uint32_t sinks[MAX_NODES];
        uint32_t sink_count;
        uint32_t edge_index;
        uint32_t sink_index;

        if (component->node_rank[source_index] >= target_rank) {
            continue;
        }

        sink_count = 0U;
        for (edge_index = 0U; edge_index < component->edge_count; edge_index++) {
            if (component->edges[edge_index].source_index != source_index) {
                continue;
            }

            sink_index = component->edges[edge_index].sink_index;
            if (component->node_rank[sink_index] != target_rank) {
                continue;
            }

            {
                bool already_listed;
                uint32_t listed_index;

                already_listed = false;
                for (listed_index = 0U; listed_index < sink_count; listed_index++) {
                    if (sinks[listed_index] == sink_index) {
                        already_listed = true;
                        break;
                    }
                }
                if (already_listed) {
                    continue;
                }
            }

            sinks[sink_count++] = sink_index;
        }

        if (sink_count < 2U) {
            continue;
        }

        for (edge_index = 1U; edge_index < sink_count; edge_index++) {
            uint32_t current_sink;
            uint32_t insert_at;

            current_sink = sinks[edge_index];
            insert_at = edge_index;
            while (insert_at > 0U &&
                   layer_position[current_sink] < layer_position[sinks[insert_at - 1U]]) {
                sinks[insert_at] = sinks[insert_at - 1U];
                insert_at--;
            }
            sinks[insert_at] = current_sink;
        }

        for (sink_index = 0U; sink_index < sink_count; sink_index++) {
            float centered_slot;

            if (sinks[sink_index] != local_node) {
                continue;
            }

            centered_slot = (float)sink_index - (((float)sink_count - 1.0f) * 0.5f);
            offset_sum -= centered_slot * LAYOUT_FANOUT_STAGGER;
            contributing_groups++;
            break;
        }
    }

    if (contributing_groups == 0U) {
        return 0.0f;
    }

    return layout_snap(offset_sum / (float)contributing_groups);
}

static float layout_component_finalize_positions(
    LayoutComponent *component,
    float component_top,
    Vector2 *positions
) {
    uint32_t layer_position[MAX_NODES];
    float base_x[MAX_NODES];
    float min_offset[MAX_NODES];
    float max_offset[MAX_NODES];
    float max_width[MAX_NODES];
    uint32_t rank_index;
    float min_x;
    float min_y;
    float max_y;
    float shift_x;
    float shift_y;
    uint32_t local_index;

    memset(layer_position, 0, sizeof(layer_position));
    layout_component_update_layer_positions(component, layer_position);
    memset(base_x, 0, sizeof(base_x));
    memset(min_offset, 0, sizeof(min_offset));
    memset(max_offset, 0, sizeof(max_offset));
    memset(max_width, 0, sizeof(max_width));

    for (rank_index = 0U; rank_index <= component->max_rank; rank_index++) {
        uint32_t start;
        uint32_t end;
        uint32_t order_index;

        start = component->layer_offsets[rank_index];
        end = component->layer_offsets[rank_index + 1U];
        min_offset[rank_index] = 0.0f;
        max_offset[rank_index] = 0.0f;
        max_width[rank_index] = 0.0f;

        for (order_index = start; order_index < end; order_index++) {
            uint32_t local_node;
            float offset;

            local_node = component->ordered_nodes[order_index];
            offset = layout_component_fanout_offset(component, local_node, layer_position);
            component->node_x_offset[local_node] = offset;
            if (offset < min_offset[rank_index]) {
                min_offset[rank_index] = offset;
            }
            if (offset > max_offset[rank_index]) {
                max_offset[rank_index] = offset;
            }
            if ((float)component->widths[local_node] > max_width[rank_index]) {
                max_width[rank_index] = (float)component->widths[local_node];
            }
        }
    }

    base_x[0] = -min_offset[0];
    for (rank_index = 1U; rank_index <= component->max_rank; rank_index++) {
        base_x[rank_index] =
            base_x[rank_index - 1U] +
            max_width[rank_index - 1U] +
            max_offset[rank_index - 1U] -
            min_offset[rank_index] +
            LAYOUT_LAYER_GAP;
    }

    min_x = FLT_MAX;
    min_y = FLT_MAX;
    max_y = -FLT_MAX;
    for (local_index = 0U; local_index < component->node_count; local_index++) {
        uint32_t global_index;
        float node_x;
        float node_y;
        float node_bottom;

        global_index = component->node_indices[local_index];
        node_x = base_x[component->node_rank[local_index]] + component->node_x_offset[local_index];
        node_y = component->node_y[local_index];
        positions[global_index] = (Vector2){ node_x, node_y };

        if (node_x < min_x) {
            min_x = node_x;
        }
        if (node_y < min_y) {
            min_y = node_y;
        }
        node_bottom = node_y + (float)component->heights[local_index];
        if (node_bottom > max_y) {
            max_y = node_bottom;
        }
    }

    shift_x = LAYOUT_CANVAS_LEFT - min_x;
    shift_y = component_top - min_y;
    for (local_index = 0U; local_index < component->node_count; local_index++) {
        uint32_t global_index;

        global_index = component->node_indices[local_index];
        positions[global_index].x += shift_x;
        positions[global_index].y += shift_y;
    }

    return max_y + shift_y;
}

static float layout_resolve_component(
    const CircuitLayoutNode *nodes,
    const CircuitLayoutEdge *edges,
    uint32_t edge_count,
    const GraphComponent *graph_component,
    float component_top,
    Vector2 *positions
) {
    LayoutComponent component;
    uint32_t sweep_index;

    layout_component_build(&component, nodes, edges, edge_count, graph_component);
    layout_component_assign_ranks(&component);
    layout_component_rebuild_layers(&component);

    for (sweep_index = 0U; sweep_index < LAYOUT_ORDER_SWEEPS; sweep_index++) {
        layout_component_sweep_order(&component, true);
        layout_component_sweep_order(&component, false);
    }

    layout_component_place_nodes(&component);
    return layout_component_finalize_positions(&component, component_top, positions);
}

bool circuit_layout_resolve_positions(
    const CircuitLayoutNode *nodes,
    uint32_t node_count,
    const CircuitLayoutEdge *edges,
    uint32_t edge_count,
    Vector2 *positions
) {
    GraphComponent components[MAX_NODES];
    uint32_t component_count;
    float component_top;
    uint32_t component_index;

    if (!nodes || !positions) {
        return false;
    }
    if (node_count == 0U) {
        return true;
    }

    collect_graph_components(node_count, edges, edge_count, components, &component_count);
    component_top = LAYOUT_CANVAS_TOP;
    for (component_index = 0U; component_index < component_count; component_index++) {
        float component_bottom;

        component_bottom = layout_resolve_component(
            nodes,
            edges,
            edge_count,
            &components[component_index],
            component_top,
            positions
        );
        component_top = component_bottom + LAYOUT_COMPONENT_GAP;
    }

    return true;
}

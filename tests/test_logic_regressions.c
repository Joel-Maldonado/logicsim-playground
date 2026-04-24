#include <assert.h>
#include <stdio.h>
#include "test_logic_regressions.h"
#include "../src/logic.h"

static void test_gate_rejects_missing_inputs(void) {
    LogicValue one_input[] = { LOGIC_HIGH };

    assert(logic_eval_gate(NODE_GATE_XOR, one_input, 1U) == LOGIC_UNKNOWN);
    assert(logic_eval_gate(NODE_GATE_NOT, NULL, 0U) == LOGIC_UNKNOWN);
    printf("test_gate_rejects_missing_inputs passed!\n");
}

static void test_connected_input_uses_source_value(void) {
    LogicGraph graph;
    LogicNode *input;
    LogicNode *output;

    logic_init_graph(&graph);
    input = logic_add_node(&graph, NODE_INPUT, "A");
    output = logic_add_node(&graph, NODE_OUTPUT, "Z");

    assert(logic_connect(&graph, &input->outputs[0], &output->inputs[0]));
    input->outputs[0].value = LOGIC_LOW;
    logic_evaluate(&graph);
    assert(output->inputs[0].value == LOGIC_LOW);

    input->outputs[0].value = LOGIC_HIGH;
    logic_evaluate(&graph);
    assert(output->inputs[0].value == LOGIC_HIGH);
    printf("test_connected_input_uses_source_value passed!\n");
}

void run_logic_regression_tests(void) {
    test_gate_rejects_missing_inputs();
    test_connected_input_uses_source_value();
}

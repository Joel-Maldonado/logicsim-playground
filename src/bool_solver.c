#include "bool_solver.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOOL_AST_MAX_NODES 192
#define BOOL_SOLVER_MAX_IMPLICANTS 8192

typedef enum {
    BOOL_AST_CONST,
    BOOL_AST_VAR,
    BOOL_AST_NOT,
    BOOL_AST_AND,
    BOOL_AST_OR
} BoolAstType;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

typedef struct {
    BoolAstType type;
    int left;
    int right;
    char var;
    bool value;
    uint8_t _padding[3];
} BoolAstNode;

typedef struct {
    const char *input;
    size_t pos;
    size_t length;
    BoolAstNode nodes[BOOL_AST_MAX_NODES];
    uint32_t variable_mask;
    uint16_t node_count;
    char error[BOOL_SOLVER_ERROR_MAX];
} BoolParser;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

typedef struct {
    uint8_t value;
    uint8_t mask;
} BoolCube;

static void append_text(char *buffer, size_t buffer_size, const char *text) {
    size_t length;
    size_t available;
    int written;

    if (!buffer || buffer_size == 0U || !text) {
        return;
    }

    length = strlen(buffer);
    if (length >= buffer_size - 1U) {
        return;
    }

    available = buffer_size - length;
    written = snprintf(buffer + length, available, "%s", text);
    if (written < 0 || (size_t)written < available) {
        return;
    }

    if (buffer_size > 4U) {
        buffer[buffer_size - 4U] = '.';
        buffer[buffer_size - 3U] = '.';
        buffer[buffer_size - 2U] = '.';
        buffer[buffer_size - 1U] = '\0';
    }
}

static void parser_set_error(BoolParser *parser, const char *message) {
    if (!parser || parser->error[0] != '\0') {
        return;
    }

    snprintf(parser->error, sizeof(parser->error), "%s", message);
}

static void skip_spaces(BoolParser *parser) {
    while (parser->pos < parser->length && isspace((unsigned char)parser->input[parser->pos])) {
        parser->pos++;
    }
}

static char peek_char(BoolParser *parser) {
    skip_spaces(parser);
    if (parser->pos >= parser->length) {
        return '\0';
    }

    return parser->input[parser->pos];
}

static bool consume_char(BoolParser *parser, char expected) {
    skip_spaces(parser);
    if (parser->pos >= parser->length || parser->input[parser->pos] != expected) {
        return false;
    }

    parser->pos++;
    return true;
}

static int add_node(BoolParser *parser, BoolAstType type, int left, int right, char var, bool value) {
    BoolAstNode *node;

    if (parser->node_count >= BOOL_AST_MAX_NODES) {
        parser_set_error(parser, "Expression is too complex for the educational solver.");
        return -1;
    }

    node = &parser->nodes[parser->node_count];
    memset(node, 0, sizeof(*node));
    node->type = type;
    node->left = left;
    node->right = right;
    node->var = var;
    node->value = value;
    parser->node_count++;
    return (int)(parser->node_count - 1U);
}

static uint8_t bit_count(uint32_t value) {
    uint8_t count;

    count = 0U;
    while (value != 0U) {
        count = (uint8_t)(count + (uint8_t)(value & 1U));
        value >>= 1U;
    }

    return count;
}

static bool char_starts_primary(char ch) {
    return ch == '!' || ch == '(' || ch == '0' || ch == '1' || isalpha((unsigned char)ch);
}

static int parse_or(BoolParser *parser);

static int parse_primary(BoolParser *parser) {
    char ch;
    int node;

    ch = peek_char(parser);
    if (ch == '(') {
        parser->pos++;
        node = parse_or(parser);
        if (node < 0) {
            return -1;
        }
        if (!consume_char(parser, ')')) {
            if (parser->error[0] == '\0') {
                snprintf(parser->error, sizeof(parser->error), "Expected ')' at position %zu.", parser->pos + 1U);
            }
            return -1;
        }
        return node;
    }

    if (ch == '0' || ch == '1') {
        parser->pos++;
        return add_node(parser, BOOL_AST_CONST, -1, -1, '\0', ch == '1');
    }

    if (isalpha((unsigned char)ch)) {
        char var;
        uint32_t bit;

        parser->pos++;
        var = (char)toupper((unsigned char)ch);
        bit = 1U << (uint32_t)(var - 'A');
        if ((parser->variable_mask & bit) == 0U && bit_count(parser->variable_mask) >= BOOL_SOLVER_MAX_VARS) {
            if (parser->error[0] == '\0') {
                snprintf(parser->error, sizeof(parser->error), "Use at most %u variables.", (unsigned int)BOOL_SOLVER_MAX_VARS);
            }
            return -1;
        }
        parser->variable_mask |= bit;
        return add_node(parser, BOOL_AST_VAR, -1, -1, var, false);
    }

    if (ch == '\0') {
        parser_set_error(parser, "Expected a variable, constant, or group at the end of the expression.");
    } else {
        if (parser->error[0] == '\0') {
            snprintf(parser->error, sizeof(parser->error), "Unexpected '%c' at position %zu.", ch, parser->pos + 1U);
        }
    }
    return -1;
}

static int parse_unary(BoolParser *parser) {
    int child;

    if (consume_char(parser, '!')) {
        child = parse_unary(parser);
        if (child < 0) {
            return -1;
        }
        return add_node(parser, BOOL_AST_NOT, child, -1, '\0', false);
    }

    return parse_primary(parser);
}

static int parse_and(BoolParser *parser) {
    int left;

    left = parse_unary(parser);
    if (left < 0) {
        return -1;
    }

    for (;;) {
        char ch;
        int right;

        ch = peek_char(parser);
        if (ch == '*' || ch == '.') {
            parser->pos++;
            right = parse_unary(parser);
        } else if (char_starts_primary(ch)) {
            right = parse_unary(parser);
        } else {
            break;
        }

        if (right < 0) {
            return -1;
        }
        left = add_node(parser, BOOL_AST_AND, left, right, '\0', false);
        if (left < 0) {
            return -1;
        }
    }

    return left;
}

static int parse_or(BoolParser *parser) {
    int left;

    left = parse_and(parser);
    if (left < 0) {
        return -1;
    }

    while (consume_char(parser, '+')) {
        int right;

        right = parse_and(parser);
        if (right < 0) {
            return -1;
        }
        left = add_node(parser, BOOL_AST_OR, left, right, '\0', false);
        if (left < 0) {
            return -1;
        }
    }

    return left;
}

static bool parse_expression(const char *input, BoolParser *parser, int *root) {
    memset(parser, 0, sizeof(*parser));
    parser->input = input;
    parser->length = strlen(input);

    if (parser->length == 0U) {
        snprintf(parser->error, sizeof(parser->error), "Enter a boolean expression.");
        return false;
    }
    if (parser->length > BOOL_SOLVER_INPUT_MAX) {
        snprintf(parser->error, sizeof(parser->error), "Use at most %u characters.", (unsigned int)BOOL_SOLVER_INPUT_MAX);
        return false;
    }

    *root = parse_or(parser);
    if (*root < 0) {
        return false;
    }

    if (peek_char(parser) != '\0') {
        if (parser->error[0] == '\0') {
            snprintf(parser->error, sizeof(parser->error), "Unexpected '%c' at position %zu.", parser->input[parser->pos], parser->pos + 1U);
        }
        return false;
    }

    return parser->error[0] == '\0';
}

static void parser_variables(const BoolParser *parser, char vars[BOOL_SOLVER_MAX_VARS], uint8_t *var_count) {
    char var;

    *var_count = 0U;
    for (var = 'A'; var <= 'Z'; var++) {
        uint32_t bit;

        bit = 1U << (uint32_t)(var - 'A');
        if ((parser->variable_mask & bit) != 0U) {
            vars[*var_count] = var;
            *var_count = (uint8_t)(*var_count + 1U);
        }
    }
}

static int variable_index(const char vars[BOOL_SOLVER_MAX_VARS], uint8_t var_count, char var) {
    uint8_t index;

    for (index = 0U; index < var_count; index++) {
        if (vars[index] == var) {
            return (int)index;
        }
    }

    return -1;
}

static bool eval_ast(
    const BoolParser *parser,
    int node_index,
    const char vars[BOOL_SOLVER_MAX_VARS],
    uint8_t var_count,
    uint16_t assignment
) {
    const BoolAstNode *node;

    node = &parser->nodes[node_index];
    switch (node->type) {
        case BOOL_AST_CONST:
            return node->value;
        case BOOL_AST_VAR: {
            int index;
            uint16_t bit;

            index = variable_index(vars, var_count, node->var);
            if (index < 0) {
                return false;
            }
            bit = (uint16_t)(1U << (uint8_t)(var_count - 1U - (uint8_t)index));
            return (assignment & bit) != 0U;
        }
        case BOOL_AST_NOT:
            return !eval_ast(parser, node->left, vars, var_count, assignment);
        case BOOL_AST_AND:
            return eval_ast(parser, node->left, vars, var_count, assignment) &&
                eval_ast(parser, node->right, vars, var_count, assignment);
        case BOOL_AST_OR:
            return eval_ast(parser, node->left, vars, var_count, assignment) ||
                eval_ast(parser, node->right, vars, var_count, assignment);
        default:
            break;
    }

    return false;
}

static int ast_precedence(const BoolAstNode *node) {
    if (node->type == BOOL_AST_OR) {
        return 1;
    }
    if (node->type == BOOL_AST_AND) {
        return 2;
    }
    if (node->type == BOOL_AST_NOT) {
        return 3;
    }

    return 4;
}

static void render_ast_node(const BoolParser *parser, int node_index, int parent_precedence, char *out, size_t out_size) {
    const BoolAstNode *node;
    int precedence;
    bool needs_parens;

    node = &parser->nodes[node_index];
    precedence = ast_precedence(node);
    needs_parens = precedence < parent_precedence;
    if (needs_parens) {
        append_text(out, out_size, "(");
    }

    switch (node->type) {
        case BOOL_AST_CONST:
            append_text(out, out_size, node->value ? "1" : "0");
            break;
        case BOOL_AST_VAR: {
            char text[2];

            text[0] = node->var;
            text[1] = '\0';
            append_text(out, out_size, text);
            break;
        }
        case BOOL_AST_NOT:
            append_text(out, out_size, "!");
            render_ast_node(parser, node->left, precedence, out, out_size);
            break;
        case BOOL_AST_AND:
            render_ast_node(parser, node->left, precedence, out, out_size);
            render_ast_node(parser, node->right, precedence, out, out_size);
            break;
        case BOOL_AST_OR:
            render_ast_node(parser, node->left, precedence, out, out_size);
            append_text(out, out_size, " + ");
            render_ast_node(parser, node->right, precedence, out, out_size);
            break;
        default:
            break;
    }

    if (needs_parens) {
        append_text(out, out_size, ")");
    }
}

static void render_nnf_node(
    const BoolParser *parser,
    int node_index,
    bool negated,
    int parent_precedence,
    char *out,
    size_t out_size
) {
    const BoolAstNode *node;

    node = &parser->nodes[node_index];
    if (node->type == BOOL_AST_NOT) {
        render_nnf_node(parser, node->left, !negated, parent_precedence, out, out_size);
        return;
    }

    if (node->type == BOOL_AST_CONST) {
        append_text(out, out_size, node->value != negated ? "1" : "0");
        return;
    }

    if (node->type == BOOL_AST_VAR) {
        char text[3];

        if (negated) {
            text[0] = '!';
            text[1] = node->var;
            text[2] = '\0';
        } else {
            text[0] = node->var;
            text[1] = '\0';
        }
        append_text(out, out_size, text);
        return;
    }

    {
        BoolAstType effective_type;
        int precedence;
        bool needs_parens;

        effective_type = node->type;
        if (negated && node->type == BOOL_AST_AND) {
            effective_type = BOOL_AST_OR;
        } else if (negated && node->type == BOOL_AST_OR) {
            effective_type = BOOL_AST_AND;
        }

        precedence = (effective_type == BOOL_AST_OR) ? 1 : 2;
        needs_parens = precedence < parent_precedence;
        if (needs_parens) {
            append_text(out, out_size, "(");
        }

        render_nnf_node(parser, node->left, negated, precedence, out, out_size);
        if (effective_type == BOOL_AST_OR) {
            append_text(out, out_size, " + ");
        }
        render_nnf_node(parser, node->right, negated, precedence, out, out_size);

        if (needs_parens) {
            append_text(out, out_size, ")");
        }
    }
}

static bool cube_covers(BoolCube cube, uint16_t minterm) {
    return (((uint8_t)minterm ^ cube.value) & (uint8_t)(~cube.mask)) == 0U;
}

static bool cubes_equal(BoolCube left, BoolCube right) {
    return left.value == right.value && left.mask == right.mask;
}

static bool add_unique_cube(BoolCube *cubes, uint16_t *count, BoolCube cube, uint16_t capacity) {
    uint16_t index;

    for (index = 0U; index < *count; index++) {
        if (cubes_equal(cubes[index], cube)) {
            return true;
        }
    }

    if (*count >= capacity) {
        return false;
    }

    cubes[*count] = cube;
    *count = (uint16_t)(*count + 1U);
    return true;
}

static bool combine_cubes(BoolCube left, BoolCube right, BoolCube *combined) {
    uint8_t diff;

    if (left.mask != right.mask) {
        return false;
    }

    diff = (uint8_t)((left.value ^ right.value) & (uint8_t)(~left.mask));
    if (bit_count(diff) != 1U) {
        return false;
    }

    combined->mask = (uint8_t)(left.mask | diff);
    combined->value = (uint8_t)(left.value & (uint8_t)(~diff));
    return true;
}

static uint8_t cube_literal_count(BoolCube cube, uint8_t var_count) {
    uint8_t all_mask;

    all_mask = (uint8_t)((1U << var_count) - 1U);
    return (uint8_t)(var_count - bit_count(cube.mask & all_mask));
}

static int cube_first_literal_index(BoolCube cube, uint8_t var_count) {
    uint8_t index;

    for (index = 0U; index < var_count; index++) {
        uint8_t bit;

        bit = (uint8_t)(1U << (uint8_t)(var_count - 1U - index));
        if ((cube.mask & bit) == 0U) {
            return (int)index;
        }
    }

    return (int)var_count;
}

static bool cube_less(BoolCube left, BoolCube right, uint8_t var_count) {
    uint8_t left_literals;
    uint8_t right_literals;
    int left_first;
    int right_first;

    left_literals = cube_literal_count(left, var_count);
    right_literals = cube_literal_count(right, var_count);
    if (left_literals != right_literals) {
        return left_literals < right_literals;
    }

    left_first = cube_first_literal_index(left, var_count);
    right_first = cube_first_literal_index(right, var_count);
    if (left_first != right_first) {
        return left_first < right_first;
    }
    if (left.mask != right.mask) {
        return left.mask > right.mask;
    }

    return left.value < right.value;
}

static void sort_cubes(BoolCube *cubes, uint16_t count, uint8_t var_count) {
    uint16_t index;

    for (index = 1U; index < count; index++) {
        BoolCube cube;
        uint16_t cursor;

        cube = cubes[index];
        cursor = index;
        while (cursor > 0U && cube_less(cube, cubes[cursor - 1U], var_count)) {
            cubes[cursor] = cubes[cursor - 1U];
            cursor--;
        }
        cubes[cursor] = cube;
    }
}

static bool build_prime_implicants(
    const uint16_t *minterms,
    uint16_t minterm_count,
    BoolCube *primes,
    uint16_t *prime_count,
    char *error,
    size_t error_size
) {
    BoolCube *current;
    BoolCube *next;
    bool *used;
    uint16_t current_count;
    bool ok;

    current = (BoolCube *)calloc(BOOL_SOLVER_MAX_IMPLICANTS, sizeof(*current));
    next = (BoolCube *)calloc(BOOL_SOLVER_MAX_IMPLICANTS, sizeof(*next));
    used = (bool *)calloc(BOOL_SOLVER_MAX_IMPLICANTS, sizeof(*used));
    if (!current || !next || !used) {
        snprintf(error, error_size, "Could not allocate solver workspace.");
        free(current);
        free(next);
        free(used);
        return false;
    }

    current_count = 0U;
    ok = true;
    for (uint16_t index = 0U; index < minterm_count; index++) {
        BoolCube cube;

        cube.value = (uint8_t)minterms[index];
        cube.mask = 0U;
        if (!add_unique_cube(current, &current_count, cube, BOOL_SOLVER_MAX_IMPLICANTS)) {
            ok = false;
            break;
        }
    }

    *prime_count = 0U;
    while (ok && current_count > 0U) {
        uint16_t next_count;

        memset(used, 0, BOOL_SOLVER_MAX_IMPLICANTS * sizeof(*used));
        next_count = 0U;

        for (uint16_t left = 0U; left < current_count; left++) {
            for (uint16_t right = (uint16_t)(left + 1U); right < current_count; right++) {
                BoolCube combined;

                if (!combine_cubes(current[left], current[right], &combined)) {
                    continue;
                }
                used[left] = true;
                used[right] = true;
                if (!add_unique_cube(next, &next_count, combined, BOOL_SOLVER_MAX_IMPLICANTS)) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                break;
            }
        }

        for (uint16_t index = 0U; ok && index < current_count; index++) {
            if (!used[index] && !add_unique_cube(primes, prime_count, current[index], BOOL_SOLVER_MAX_IMPLICANTS)) {
                ok = false;
            }
        }

        {
            BoolCube *swap;

            swap = current;
            current = next;
            next = swap;
            current_count = next_count;
        }
    }

    if (!ok) {
        snprintf(error, error_size, "Expression generated too many implicants to display clearly.");
    }

    free(current);
    free(next);
    free(used);
    return ok;
}

static void render_cube(
    BoolCube cube,
    const char vars[BOOL_SOLVER_MAX_VARS],
    uint8_t var_count,
    char *out,
    size_t out_size
) {
    uint8_t literal_count;

    literal_count = 0U;
    for (uint8_t index = 0U; index < var_count; index++) {
        uint8_t bit;
        char text[3];

        bit = (uint8_t)(1U << (uint8_t)(var_count - 1U - index));
        if ((cube.mask & bit) != 0U) {
            continue;
        }

        if ((cube.value & bit) == 0U) {
            text[0] = '!';
            text[1] = vars[index];
            text[2] = '\0';
        } else {
            text[0] = vars[index];
            text[1] = '\0';
        }
        append_text(out, out_size, text);
        literal_count++;
    }

    if (literal_count == 0U) {
        append_text(out, out_size, "1");
    }
}

static void render_cube_list(
    const BoolCube *cubes,
    uint16_t cube_count,
    const char vars[BOOL_SOLVER_MAX_VARS],
    uint8_t var_count,
    char *out,
    size_t out_size
) {
    uint16_t limit;

    out[0] = '\0';
    if (cube_count == 0U) {
        append_text(out, out_size, "0");
        return;
    }

    limit = cube_count > 24U ? 24U : cube_count;
    for (uint16_t index = 0U; index < limit; index++) {
        if (index > 0U) {
            append_text(out, out_size, " + ");
        }
        render_cube(cubes[index], vars, var_count, out, out_size);
    }

    if (limit < cube_count) {
        append_text(out, out_size, " + ...");
    }
}

static bool select_cover(
    const BoolCube *primes,
    uint16_t prime_count,
    const uint16_t *minterms,
    uint16_t minterm_count,
    uint8_t var_count,
    BoolCube *selected,
    uint16_t *selected_count
) {
    bool prime_selected[BOOL_SOLVER_MAX_IMPLICANTS];
    bool covered[256];

    memset(prime_selected, 0, sizeof(prime_selected));
    memset(covered, 0, sizeof(covered));
    *selected_count = 0U;

    for (uint16_t minterm_index = 0U; minterm_index < minterm_count; minterm_index++) {
        uint16_t owner;
        uint16_t cover_count;

        owner = 0U;
        cover_count = 0U;
        for (uint16_t prime_index = 0U; prime_index < prime_count; prime_index++) {
            if (cube_covers(primes[prime_index], minterms[minterm_index])) {
                owner = prime_index;
                cover_count++;
            }
        }

        if (cover_count == 1U && !prime_selected[owner]) {
            prime_selected[owner] = true;
            selected[*selected_count] = primes[owner];
            *selected_count = (uint16_t)(*selected_count + 1U);
        }
    }

    for (uint16_t selected_index = 0U; selected_index < *selected_count; selected_index++) {
        for (uint16_t minterm_index = 0U; minterm_index < minterm_count; minterm_index++) {
            if (cube_covers(selected[selected_index], minterms[minterm_index])) {
                covered[minterm_index] = true;
            }
        }
    }

    for (;;) {
        uint16_t best_index;
        uint16_t best_cover_count;
        bool has_uncovered;

        has_uncovered = false;
        for (uint16_t minterm_index = 0U; minterm_index < minterm_count; minterm_index++) {
            if (!covered[minterm_index]) {
                has_uncovered = true;
                break;
            }
        }
        if (!has_uncovered) {
            sort_cubes(selected, *selected_count, var_count);
            return true;
        }

        best_index = 0U;
        best_cover_count = 0U;
        for (uint16_t prime_index = 0U; prime_index < prime_count; prime_index++) {
            uint16_t cover_count;

            if (prime_selected[prime_index]) {
                continue;
            }

            cover_count = 0U;
            for (uint16_t minterm_index = 0U; minterm_index < minterm_count; minterm_index++) {
                if (!covered[minterm_index] && cube_covers(primes[prime_index], minterms[minterm_index])) {
                    cover_count++;
                }
            }

            if (cover_count > best_cover_count ||
                (cover_count == best_cover_count && cover_count > 0U && cube_less(primes[prime_index], primes[best_index], var_count))) {
                best_index = prime_index;
                best_cover_count = cover_count;
            }
        }

        if (best_cover_count == 0U) {
            break;
        }

        prime_selected[best_index] = true;
        selected[*selected_count] = primes[best_index];
        *selected_count = (uint16_t)(*selected_count + 1U);
        for (uint16_t minterm_index = 0U; minterm_index < minterm_count; minterm_index++) {
            if (cube_covers(primes[best_index], minterms[minterm_index])) {
                covered[minterm_index] = true;
            }
        }
    }

    return false;
}

static bool selected_cover_eval(const BoolCube *selected, uint16_t selected_count, uint16_t assignment) {
    for (uint16_t index = 0U; index < selected_count; index++) {
        if (cube_covers(selected[index], assignment)) {
            return true;
        }
    }

    return false;
}

static void add_step(BoolSolverResult *result, const char *title, const char *expression) {
    BoolSolverStep *step;

    if (result->step_count >= BOOL_SOLVER_MAX_STEPS) {
        return;
    }

    step = &result->steps[result->step_count];
    snprintf(step->title, sizeof(step->title), "%s", title);
    snprintf(step->expression, sizeof(step->expression), "%s", expression ? expression : "");
    result->step_count++;
}

void bool_solver_result_clear(BoolSolverResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
}

bool bool_solver_solve(const char *input, BoolSolverResult *result) {
    BoolParser parser;
    char vars[BOOL_SOLVER_MAX_VARS];
    char start_expr[BOOL_SOLVER_EXPR_MAX];
    char nnf_expr[BOOL_SOLVER_EXPR_MAX];
    char canonical_expr[BOOL_SOLVER_EXPR_MAX];
    char primes_expr[BOOL_SOLVER_EXPR_MAX];
    uint16_t minterms[256];
    BoolCube *primes;
    BoolCube *selected;
    uint16_t minterm_count;
    uint16_t prime_count;
    uint16_t selected_count;
    uint8_t var_count;
    uint16_t row_count;
    int root;
    bool ok;

    if (!result) {
        return false;
    }
    bool_solver_result_clear(result);

    if (!input) {
        snprintf(result->error, sizeof(result->error), "Enter a boolean expression.");
        return false;
    }

    if (!parse_expression(input, &parser, &root)) {
        snprintf(result->error, sizeof(result->error), "%s", parser.error);
        return false;
    }

    parser_variables(&parser, vars, &var_count);
    result->variable_count = var_count;
    result->variables[0] = '\0';
    for (uint8_t index = 0U; index < var_count; index++) {
        char text[3];

        text[0] = vars[index];
        text[1] = (index + 1U < var_count) ? ' ' : '\0';
        text[2] = '\0';
        append_text(result->variables, sizeof(result->variables), text);
    }

    start_expr[0] = '\0';
    nnf_expr[0] = '\0';
    canonical_expr[0] = '\0';
    primes_expr[0] = '\0';
    render_ast_node(&parser, root, 0, start_expr, sizeof(start_expr));
    render_nnf_node(&parser, root, false, 0, nnf_expr, sizeof(nnf_expr));
    add_step(result, "Start", start_expr);
    add_step(result, "Apply: De Morgan Theorem / Double Negation", nnf_expr);

    row_count = (uint16_t)(1U << var_count);
    minterm_count = 0U;
    for (uint16_t row = 0U; row < row_count; row++) {
        if (eval_ast(&parser, root, vars, var_count, row)) {
            minterms[minterm_count] = row;
            minterm_count++;
        }
    }

    if (minterm_count == 0U) {
        snprintf(result->simplified_expression, sizeof(result->simplified_expression), "0");
        add_step(result, "Convert to Sum of Products", "0");
        add_step(result, "Result", result->simplified_expression);
        result->ok = true;
        snprintf(
            result->algorithm,
            sizeof(result->algorithm),
            "Parsed the expression, pushed NOT inward with De Morgan laws, built the truth table, and found no true minterms."
        );
        return true;
    }

    if (minterm_count == row_count) {
        snprintf(result->simplified_expression, sizeof(result->simplified_expression), "1");
        add_step(result, "Convert to Sum of Products", "1");
        add_step(result, "Result", result->simplified_expression);
        result->ok = true;
        snprintf(
            result->algorithm,
            sizeof(result->algorithm),
            "Parsed the expression, pushed NOT inward with De Morgan laws, built the truth table, and found every row true."
        );
        return true;
    }

    selected = (BoolCube *)calloc(BOOL_SOLVER_MAX_IMPLICANTS, sizeof(*selected));
    primes = (BoolCube *)calloc(BOOL_SOLVER_MAX_IMPLICANTS, sizeof(*primes));
    if (!selected || !primes) {
        snprintf(result->error, sizeof(result->error), "Could not allocate solver workspace.");
        free(selected);
        free(primes);
        return false;
    }

    for (uint16_t index = 0U; index < minterm_count; index++) {
        BoolCube cube;

        cube.value = (uint8_t)minterms[index];
        cube.mask = 0U;
        selected[index] = cube;
    }
    render_cube_list(selected, minterm_count, vars, var_count, canonical_expr, sizeof(canonical_expr));
    add_step(result, "Convert to Sum of Products", canonical_expr);

    prime_count = 0U;
    ok = build_prime_implicants(minterms, minterm_count, primes, &prime_count, result->error, sizeof(result->error));
    if (!ok) {
        free(selected);
        free(primes);
        return false;
    }

    sort_cubes(primes, prime_count, var_count);
    render_cube_list(primes, prime_count, vars, var_count, primes_expr, sizeof(primes_expr));
    add_step(result, "Apply: Combining Law", primes_expr);

    selected_count = 0U;
    if (!select_cover(primes, prime_count, minterms, minterm_count, var_count, selected, &selected_count)) {
        snprintf(result->error, sizeof(result->error), "Could not cover all true minterms.");
        free(selected);
        free(primes);
        return false;
    }

    render_cube_list(selected, selected_count, vars, var_count, result->simplified_expression, sizeof(result->simplified_expression));
    add_step(result, "Apply: Absorption / Essential Coverage", result->simplified_expression);

    for (uint16_t row = 0U; row < row_count; row++) {
        bool original_value;
        bool simplified_value;

        original_value = eval_ast(&parser, root, vars, var_count, row);
        simplified_value = selected_cover_eval(selected, selected_count, row);
        if (original_value != simplified_value) {
            snprintf(result->error, sizeof(result->error), "Simplification failed equivalence verification.");
            free(selected);
            free(primes);
            return false;
        }
    }

    add_step(result, "Result", result->simplified_expression);
    snprintf(
        result->algorithm,
        sizeof(result->algorithm),
        "Parser -> AST -> NOT normalization with De Morgan and double-negation laws -> truth-table minterms -> Quine-McCluskey prime implicants -> essential-prime coverage -> truth-table verification."
    );
    result->ok = true;

    free(selected);
    free(primes);
    return true;
}

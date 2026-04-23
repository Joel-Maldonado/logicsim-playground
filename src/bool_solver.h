#ifndef BOOL_SOLVER_H
#define BOOL_SOLVER_H

#include <stdbool.h>
#include <stdint.h>

#define BOOL_SOLVER_INPUT_MAX 256
#define BOOL_SOLVER_MAX_VARS 8
#define BOOL_SOLVER_MAX_STEPS 8
#define BOOL_SOLVER_EXPR_MAX 1024
#define BOOL_SOLVER_TITLE_MAX 96
#define BOOL_SOLVER_ERROR_MAX 192
#define BOOL_SOLVER_ALGORITHM_MAX 512

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

typedef struct {
    char title[BOOL_SOLVER_TITLE_MAX];
    char expression[BOOL_SOLVER_EXPR_MAX];
} BoolSolverStep;

typedef struct {
    BoolSolverStep steps[BOOL_SOLVER_MAX_STEPS];
    char simplified_expression[BOOL_SOLVER_EXPR_MAX];
    char variables[32];
    char algorithm[BOOL_SOLVER_ALGORITHM_MAX];
    char error[BOOL_SOLVER_ERROR_MAX];
    uint8_t variable_count;
    uint8_t step_count;
    bool ok;
    uint8_t _padding[5];
} BoolSolverResult;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

void bool_solver_result_clear(BoolSolverResult *result);
bool bool_solver_solve(const char *input, BoolSolverResult *result);

#endif // BOOL_SOLVER_H

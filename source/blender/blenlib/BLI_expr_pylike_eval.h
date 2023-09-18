/* SPDX-FileCopyrightText: 2018 Blender Authors, Alexander Gavrilov. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque structure containing pre-parsed data for evaluation. */
typedef struct ExprPyLike_Parsed ExprPyLike_Parsed;

/** Expression evaluation return code. */
typedef enum eExprPyLike_EvalStatus {
  EXPR_PYLIKE_SUCCESS = 0,
  /* Computation errors; result is still set, but may be NaN */
  EXPR_PYLIKE_DIV_BY_ZERO,
  EXPR_PYLIKE_MATH_ERROR,
  /* Expression dependent errors or bugs; result is 0 */
  EXPR_PYLIKE_INVALID,
  EXPR_PYLIKE_FATAL_ERROR,
} eExprPyLike_EvalStatus;

/**
 * Free the parsed data; NULL argument is ok.
 */
void BLI_expr_pylike_free(struct ExprPyLike_Parsed *expr);
/**
 * Check if the parsing result is valid for evaluation.
 */
bool BLI_expr_pylike_is_valid(struct ExprPyLike_Parsed *expr);
/**
 * Check if the parsed expression always evaluates to the same value.
 */
bool BLI_expr_pylike_is_constant(struct ExprPyLike_Parsed *expr);
/**
 * Check if the parsed expression uses the parameter with the given index.
 */
bool BLI_expr_pylike_is_using_param(struct ExprPyLike_Parsed *expr, int index);
/**
 * Compile the expression and return the result.
 *
 * Parse the expression for evaluation later.
 * Returns non-NULL even on failure; use is_valid to check.
 */
ExprPyLike_Parsed *BLI_expr_pylike_parse(const char *expression,
                                         const char **param_names,
                                         int param_names_len);
/**
 * Evaluate the expression with the given parameters.
 * The order and number of parameters must match the names given to parse.
 */
eExprPyLike_EvalStatus BLI_expr_pylike_eval(struct ExprPyLike_Parsed *expr,
                                            const double *param_values,
                                            int param_values_len,
                                            double *r_result);

#ifdef __cplusplus
}
#endif

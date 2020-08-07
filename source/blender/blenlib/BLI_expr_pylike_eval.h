/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 Blender Foundation, Alexander Gavrilov
 * All rights reserved.
 */

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

void BLI_expr_pylike_free(struct ExprPyLike_Parsed *expr);
bool BLI_expr_pylike_is_valid(struct ExprPyLike_Parsed *expr);
bool BLI_expr_pylike_is_constant(struct ExprPyLike_Parsed *expr);
bool BLI_expr_pylike_is_using_param(struct ExprPyLike_Parsed *expr, int index);
ExprPyLike_Parsed *BLI_expr_pylike_parse(const char *expression,
                                         const char **param_names,
                                         int param_names_len);
eExprPyLike_EvalStatus BLI_expr_pylike_eval(struct ExprPyLike_Parsed *expr,
                                            const double *param_values,
                                            int param_values_len,
                                            double *r_result);

#ifdef __cplusplus
}
#endif

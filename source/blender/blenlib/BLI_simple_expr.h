/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Alexander Gavrilov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_SIMPLE_EXPR_H__
#define __BLI_SIMPLE_EXPR_H__

/** \file BLI_simple_expr.h
 *  \ingroup bli
 *  \author Alexander Gavrilov
 *  \since 2018
 *
 * Simple evaluator for a subset of Python expressions that can be
 * computed using purely double precision floating point values.
 *
 * Supported subset:
 *
 *  - Identifiers use only ASCII characters.
 *  - Literals:
 *      floating point and decimal integer.
 *  - Constants:
 *      pi, True, False
 *  - Operators:
 *      +, -, *, /, ==, !=, <, <=, >, >=, and, or, not, ternary if
 *  - Functions:
 *      radians, degrees,
 *      abs, fabs, floor, ceil, trunc, int,
 *      sin, cos, tan, asin, acos, atan, atan2,
 *      exp, log, sqrt, pow, fmod
 *
 * The implementation has no global state and can be used multithreaded.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque structure containing pre-parsed data for evaluation. */
typedef struct ParsedSimpleExpr ParsedSimpleExpr;

/** Simple expression evaluation return code. */
typedef enum eSimpleExpr_EvalStatus {
	SIMPLE_EXPR_SUCCESS = 0,
	/* Computation errors; result is still set, but may be NaN */
	SIMPLE_EXPR_DIV_BY_ZERO,
	SIMPLE_EXPR_MATH_ERROR,
	/* Expression dependent errors or bugs; result is 0 */
	SIMPLE_EXPR_INVALID,
	SIMPLE_EXPR_FATAL_ERROR,
} eSimpleExpr_EvalStatus;

/** Free the parsed data; NULL argument is ok. */
void BLI_simple_expr_free(struct ParsedSimpleExpr *expr);

/** Check if the parsing result is valid for evaluation. */
bool BLI_simple_expr_is_valid(struct ParsedSimpleExpr *expr);

/** Check if the parsed expression always evaluates to the same value. */
bool BLI_simple_expr_is_constant(struct ParsedSimpleExpr *expr);

/** Parse the expression for evaluation later.
 *  Returns non-NULL even on failure; use is_valid to check.
 */
ParsedSimpleExpr *BLI_simple_expr_parse(const char *expression, int num_params, const char **param_names);

/** Evaluate the expression with the given parameters.
 *  The order and number of parameters must match the names given to parse.
 */
eSimpleExpr_EvalStatus BLI_simple_expr_evaluate(struct ParsedSimpleExpr *expr, double *result, int num_params, const double *params);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_SIMPLE_EXPR_H__*/

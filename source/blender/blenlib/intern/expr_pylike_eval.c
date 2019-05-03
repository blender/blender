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

/** \file
 * \ingroup bli
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
 *      min, max, radians, degrees,
 *      abs, fabs, floor, ceil, trunc, int,
 *      sin, cos, tan, asin, acos, atan, atan2,
 *      exp, log, sqrt, pow, fmod
 *
 * The implementation has no global state and can be used multithreaded.
 */

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <float.h>
#include <ctype.h>
#include <stdlib.h>
#include <fenv.h>

#include "MEM_guardedalloc.h"

#include "BLI_expr_pylike_eval.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"
#include "BLI_alloca.h"

#ifdef _MSC_VER
#  pragma fenv_access(on)
#endif

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

typedef enum eOpCode {
  /* Double constant: (-> dval) */
  OPCODE_CONST,
  /* 1 argument function call: (a -> func1(a)) */
  OPCODE_FUNC1,
  /* 2 argument function call: (a b -> func2(a,b)) */
  OPCODE_FUNC2,
  /* Parameter access: (-> params[ival]) */
  OPCODE_PARAMETER,
  /* Minimum of multiple inputs: (a b c... -> min); ival = arg count */
  OPCODE_MIN,
  /* Maximum of multiple inputs: (a b c... -> max); ival = arg count */
  OPCODE_MAX,
  /* Jump (pc += jmp_offset) */
  OPCODE_JMP,
  /* Pop and jump if zero: (a -> ); JUMP IF NOT a */
  OPCODE_JMP_ELSE,
  /* Jump if nonzero, or pop: (a -> a JUMP) IF a ELSE (a -> ) */
  OPCODE_JMP_OR,
  /* Jump if zero, or pop: (a -> a JUMP) IF NOT a ELSE (a -> )  */
  OPCODE_JMP_AND,
  /* For comparison chaining: (a b -> 0 JUMP) IF NOT func2(a,b) ELSE (a b -> b) */
  OPCODE_CMP_CHAIN,
} eOpCode;

typedef double (*UnaryOpFunc)(double);
typedef double (*BinaryOpFunc)(double, double);

typedef struct ExprOp {
  eOpCode opcode;

  int jmp_offset;

  union {
    int ival;
    double dval;
    void *ptr;
    UnaryOpFunc func1;
    BinaryOpFunc func2;
  } arg;
} ExprOp;

struct ExprPyLike_Parsed {
  int ops_count;
  int max_stack;

  ExprOp ops[];
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

/** Free the parsed data; NULL argument is ok. */
void BLI_expr_pylike_free(ExprPyLike_Parsed *expr)
{
  if (expr != NULL) {
    MEM_freeN(expr);
  }
}

/** Check if the parsing result is valid for evaluation. */
bool BLI_expr_pylike_is_valid(ExprPyLike_Parsed *expr)
{
  return expr != NULL && expr->ops_count > 0;
}

/** Check if the parsed expression always evaluates to the same value. */
bool BLI_expr_pylike_is_constant(ExprPyLike_Parsed *expr)
{
  return expr != NULL && expr->ops_count == 1 && expr->ops[0].opcode == OPCODE_CONST;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stack Machine Evaluation
 * \{ */

/**
 * Evaluate the expression with the given parameters.
 * The order and number of parameters must match the names given to parse.
 */
eExprPyLike_EvalStatus BLI_expr_pylike_eval(ExprPyLike_Parsed *expr,
                                            const double *param_values,
                                            int param_values_len,
                                            double *r_result)
{
  *r_result = 0.0;

  if (!BLI_expr_pylike_is_valid(expr)) {
    return EXPR_PYLIKE_INVALID;
  }

#define FAIL_IF(condition) \
  if (condition) { \
    return EXPR_PYLIKE_FATAL_ERROR; \
  } \
  ((void)0)

  /* Check the stack requirement is at least remotely sane and allocate on the actual stack. */
  FAIL_IF(expr->max_stack <= 0 || expr->max_stack > 1000);

  double *stack = BLI_array_alloca(stack, expr->max_stack);

  /* Evaluate expression. */
  ExprOp *ops = expr->ops;
  int sp = 0, pc;

  feclearexcept(FE_ALL_EXCEPT);

  for (pc = 0; pc >= 0 && pc < expr->ops_count; pc++) {
    switch (ops[pc].opcode) {
      /* Arithmetic */
      case OPCODE_CONST:
        FAIL_IF(sp >= expr->max_stack);
        stack[sp++] = ops[pc].arg.dval;
        break;
      case OPCODE_PARAMETER:
        FAIL_IF(sp >= expr->max_stack || ops[pc].arg.ival >= param_values_len);
        stack[sp++] = param_values[ops[pc].arg.ival];
        break;
      case OPCODE_FUNC1:
        FAIL_IF(sp < 1);
        stack[sp - 1] = ops[pc].arg.func1(stack[sp - 1]);
        break;
      case OPCODE_FUNC2:
        FAIL_IF(sp < 2);
        stack[sp - 2] = ops[pc].arg.func2(stack[sp - 2], stack[sp - 1]);
        sp--;
        break;
      case OPCODE_MIN:
        FAIL_IF(sp < ops[pc].arg.ival);
        for (int j = 1; j < ops[pc].arg.ival; j++, sp--) {
          CLAMP_MAX(stack[sp - 2], stack[sp - 1]);
        }
        break;
      case OPCODE_MAX:
        FAIL_IF(sp < ops[pc].arg.ival);
        for (int j = 1; j < ops[pc].arg.ival; j++, sp--) {
          CLAMP_MIN(stack[sp - 2], stack[sp - 1]);
        }
        break;

      /* Jumps */
      case OPCODE_JMP:
        pc += ops[pc].jmp_offset;
        break;
      case OPCODE_JMP_ELSE:
        FAIL_IF(sp < 1);
        if (!stack[--sp]) {
          pc += ops[pc].jmp_offset;
        }
        break;
      case OPCODE_JMP_OR:
      case OPCODE_JMP_AND:
        FAIL_IF(sp < 1);
        if (!stack[sp - 1] == !(ops[pc].opcode == OPCODE_JMP_OR)) {
          pc += ops[pc].jmp_offset;
        }
        else {
          sp--;
        }
        break;

      /* For chaining comparisons, i.e. "a < b < c" as "a < b and b < c" */
      case OPCODE_CMP_CHAIN:
        FAIL_IF(sp < 2);
        /* If comparison fails, return 0 and jump to end. */
        if (!ops[pc].arg.func2(stack[sp - 2], stack[sp - 1])) {
          stack[sp - 2] = 0.0;
          pc += ops[pc].jmp_offset;
        }
        /* Otherwise keep b on the stack and proceed. */
        else {
          stack[sp - 2] = stack[sp - 1];
        }
        sp--;
        break;

      default:
        return EXPR_PYLIKE_FATAL_ERROR;
    }
  }

  FAIL_IF(sp != 1 || pc != expr->ops_count);

#undef FAIL_IF

  *r_result = stack[0];

  /* Detect floating point evaluation errors. */
  int flags = fetestexcept(FE_DIVBYZERO | FE_INVALID);
  if (flags) {
    return (flags & FE_INVALID) ? EXPR_PYLIKE_MATH_ERROR : EXPR_PYLIKE_DIV_BY_ZERO;
  }

  return EXPR_PYLIKE_SUCCESS;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Built-In Operations
 * \{ */

static double op_negate(double arg)
{
  return -arg;
}

static double op_mul(double a, double b)
{
  return a * b;
}

static double op_div(double a, double b)
{
  return a / b;
}

static double op_add(double a, double b)
{
  return a + b;
}

static double op_sub(double a, double b)
{
  return a - b;
}

static double op_radians(double arg)
{
  return arg * M_PI / 180.0;
}

static double op_degrees(double arg)
{
  return arg * 180.0 / M_PI;
}

static double op_not(double a)
{
  return a ? 0.0 : 1.0;
}

static double op_eq(double a, double b)
{
  return a == b ? 1.0 : 0.0;
}

static double op_ne(double a, double b)
{
  return a != b ? 1.0 : 0.0;
}

static double op_lt(double a, double b)
{
  return a < b ? 1.0 : 0.0;
}

static double op_le(double a, double b)
{
  return a <= b ? 1.0 : 0.0;
}

static double op_gt(double a, double b)
{
  return a > b ? 1.0 : 0.0;
}

static double op_ge(double a, double b)
{
  return a >= b ? 1.0 : 0.0;
}

typedef struct BuiltinConstDef {
  const char *name;
  double value;
} BuiltinConstDef;

static BuiltinConstDef builtin_consts[] = {
    {"pi", M_PI}, {"True", 1.0}, {"False", 0.0}, {NULL, 0.0}};

typedef struct BuiltinOpDef {
  const char *name;
  eOpCode op;
  void *funcptr;
} BuiltinOpDef;

static BuiltinOpDef builtin_ops[] = {
    {"radians", OPCODE_FUNC1, op_radians},
    {"degrees", OPCODE_FUNC1, op_degrees},
    {"abs", OPCODE_FUNC1, fabs},
    {"fabs", OPCODE_FUNC1, fabs},
    {"floor", OPCODE_FUNC1, floor},
    {"ceil", OPCODE_FUNC1, ceil},
    {"trunc", OPCODE_FUNC1, trunc},
    {"int", OPCODE_FUNC1, trunc},
    {"sin", OPCODE_FUNC1, sin},
    {"cos", OPCODE_FUNC1, cos},
    {"tan", OPCODE_FUNC1, tan},
    {"asin", OPCODE_FUNC1, asin},
    {"acos", OPCODE_FUNC1, acos},
    {"atan", OPCODE_FUNC1, atan},
    {"atan2", OPCODE_FUNC2, atan2},
    {"exp", OPCODE_FUNC1, exp},
    {"log", OPCODE_FUNC1, log},
    {"sqrt", OPCODE_FUNC1, sqrt},
    {"pow", OPCODE_FUNC2, pow},
    {"fmod", OPCODE_FUNC2, fmod},
    {NULL, OPCODE_CONST, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Expression Parser State
 * \{ */

#define MAKE_CHAR2(a, b) (((a) << 8) | (b))

#define CHECK_ERROR(condition) \
  if (!(condition)) { \
    return false; \
  } \
  ((void)0)

/* For simplicity simple token types are represented by their own character;
 * these are special identifiers for multi-character tokens. */
#define TOKEN_ID MAKE_CHAR2('I', 'D')
#define TOKEN_NUMBER MAKE_CHAR2('0', '0')
#define TOKEN_GE MAKE_CHAR2('>', '=')
#define TOKEN_LE MAKE_CHAR2('<', '=')
#define TOKEN_NE MAKE_CHAR2('!', '=')
#define TOKEN_EQ MAKE_CHAR2('=', '=')
#define TOKEN_AND MAKE_CHAR2('A', 'N')
#define TOKEN_OR MAKE_CHAR2('O', 'R')
#define TOKEN_NOT MAKE_CHAR2('N', 'O')
#define TOKEN_IF MAKE_CHAR2('I', 'F')
#define TOKEN_ELSE MAKE_CHAR2('E', 'L')

static const char *token_eq_characters = "!=><";
static const char *token_characters = "~`!@#$%^&*+-=/\\?:;<>(){}[]|.,\"'";

typedef struct KeywordTokenDef {
  const char *name;
  short token;
} KeywordTokenDef;

static KeywordTokenDef keyword_list[] = {
    {"and", TOKEN_AND},
    {"or", TOKEN_OR},
    {"not", TOKEN_NOT},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {NULL, TOKEN_ID},
};

typedef struct ExprParseState {
  int param_names_len;
  const char **param_names;

  /* Original expression */
  const char *expr;
  const char *cur;

  /* Current token */
  short token;
  char *tokenbuf;
  double tokenval;

  /* Opcode buffer */
  int ops_count, max_ops, last_jmp;
  ExprOp *ops;

  /* Stack space requirement tracking */
  int stack_ptr, max_stack;
} ExprParseState;

/* Reserve space for the specified number of operations in the buffer. */
static ExprOp *parse_alloc_ops(ExprParseState *state, int count)
{
  if (state->ops_count + count > state->max_ops) {
    state->max_ops = power_of_2_max_i(state->ops_count + count);
    state->ops = MEM_reallocN(state->ops, state->max_ops * sizeof(ExprOp));
  }

  ExprOp *op = &state->ops[state->ops_count];
  state->ops_count += count;
  return op;
}

/* Add one operation and track stack usage. */
static ExprOp *parse_add_op(ExprParseState *state, eOpCode code, int stack_delta)
{
  /* track evaluation stack depth */
  state->stack_ptr += stack_delta;
  CLAMP_MIN(state->stack_ptr, 0);
  CLAMP_MIN(state->max_stack, state->stack_ptr);

  /* allocate the new instruction */
  ExprOp *op = parse_alloc_ops(state, 1);
  memset(op, 0, sizeof(ExprOp));
  op->opcode = code;
  return op;
}

/* Add one jump operation and return an index for parse_set_jump. */
static int parse_add_jump(ExprParseState *state, eOpCode code)
{
  parse_add_op(state, code, -1);
  return state->last_jmp = state->ops_count;
}

/* Set the jump offset in a previously added jump operation. */
static void parse_set_jump(ExprParseState *state, int jump)
{
  state->last_jmp = state->ops_count;
  state->ops[jump - 1].jmp_offset = state->ops_count - jump;
}

/* Add a function call operation, applying constant folding when possible. */
static bool parse_add_func(ExprParseState *state, eOpCode code, int args, void *funcptr)
{
  ExprOp *prev_ops = &state->ops[state->ops_count];
  int jmp_gap = state->ops_count - state->last_jmp;

  feclearexcept(FE_ALL_EXCEPT);

  switch (code) {
    case OPCODE_FUNC1:
      CHECK_ERROR(args == 1);

      if (jmp_gap >= 1 && prev_ops[-1].opcode == OPCODE_CONST) {
        UnaryOpFunc func = funcptr;

        double result = func(prev_ops[-1].arg.dval);

        if (fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0) {
          prev_ops[-1].arg.dval = result;
          return true;
        }
      }
      break;

    case OPCODE_FUNC2:
      CHECK_ERROR(args == 2);

      if (jmp_gap >= 2 && prev_ops[-2].opcode == OPCODE_CONST &&
          prev_ops[-1].opcode == OPCODE_CONST) {
        BinaryOpFunc func = funcptr;

        double result = func(prev_ops[-2].arg.dval, prev_ops[-1].arg.dval);

        if (fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0) {
          prev_ops[-2].arg.dval = result;
          state->ops_count--;
          state->stack_ptr--;
          return true;
        }
      }
      break;

    default:
      BLI_assert(false);
      return false;
  }

  parse_add_op(state, code, 1 - args)->arg.ptr = funcptr;
  return true;
}

/* Extract the next token from raw characters. */
static bool parse_next_token(ExprParseState *state)
{
  /* Skip whitespace. */
  while (isspace(*state->cur)) {
    state->cur++;
  }

  /* End of string. */
  if (*state->cur == 0) {
    state->token = 0;
    return true;
  }

  /* Floating point numbers. */
  if (isdigit(*state->cur) || (state->cur[0] == '.' && isdigit(state->cur[1]))) {
    char *end, *out = state->tokenbuf;
    bool is_float = false;

    while (isdigit(*state->cur)) {
      *out++ = *state->cur++;
    }

    if (*state->cur == '.') {
      is_float = true;
      *out++ = *state->cur++;

      while (isdigit(*state->cur)) {
        *out++ = *state->cur++;
      }
    }

    if (ELEM(*state->cur, 'e', 'E')) {
      is_float = true;
      *out++ = *state->cur++;

      if (ELEM(*state->cur, '+', '-')) {
        *out++ = *state->cur++;
      }

      CHECK_ERROR(isdigit(*state->cur));

      while (isdigit(*state->cur)) {
        *out++ = *state->cur++;
      }
    }

    *out = 0;

    /* Forbid C-style octal constants. */
    if (!is_float && state->tokenbuf[0] == '0') {
      for (char *p = state->tokenbuf + 1; *p; p++) {
        if (*p != '0') {
          return false;
        }
      }
    }

    state->token = TOKEN_NUMBER;
    state->tokenval = strtod(state->tokenbuf, &end);
    return (end == out);
  }

  /* ?= tokens */
  if (state->cur[1] == '=' && strchr(token_eq_characters, state->cur[0])) {
    state->token = MAKE_CHAR2(state->cur[0], state->cur[1]);
    state->cur += 2;
    return true;
  }

  /* Special characters (single character tokens) */
  if (strchr(token_characters, *state->cur)) {
    state->token = *state->cur++;
    return true;
  }

  /* Identifiers */
  if (isalpha(*state->cur) || ELEM(*state->cur, '_')) {
    char *out = state->tokenbuf;

    while (isalnum(*state->cur) || ELEM(*state->cur, '_')) {
      *out++ = *state->cur++;
    }

    *out = 0;

    for (int i = 0; keyword_list[i].name; i++) {
      if (STREQ(state->tokenbuf, keyword_list[i].name)) {
        state->token = keyword_list[i].token;
        return true;
      }
    }

    state->token = TOKEN_ID;
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recursive Descent Parser
 * \{ */

static bool parse_expr(ExprParseState *state);

static int parse_function_args(ExprParseState *state)
{
  if (!parse_next_token(state) || state->token != '(' || !parse_next_token(state)) {
    return -1;
  }

  int arg_count = 0;

  for (;;) {
    if (!parse_expr(state)) {
      return -1;
    }

    arg_count++;

    switch (state->token) {
      case ',':
        if (!parse_next_token(state)) {
          return -1;
        }
        break;

      case ')':
        if (!parse_next_token(state)) {
          return -1;
        }
        return arg_count;

      default:
        return -1;
    }
  }
}

static bool parse_unary(ExprParseState *state)
{
  int i;

  switch (state->token) {
    case '+':
      return parse_next_token(state) && parse_unary(state);

    case '-':
      CHECK_ERROR(parse_next_token(state) && parse_unary(state));
      parse_add_func(state, OPCODE_FUNC1, 1, op_negate);
      return true;

    case '(':
      return parse_next_token(state) && parse_expr(state) && state->token == ')' &&
             parse_next_token(state);

    case TOKEN_NUMBER:
      parse_add_op(state, OPCODE_CONST, 1)->arg.dval = state->tokenval;
      return parse_next_token(state);

    case TOKEN_ID:
      /* Parameters: search in reverse order in case of duplicate names -
       * the last one should win. */
      for (i = state->param_names_len - 1; i >= 0; i--) {
        if (STREQ(state->tokenbuf, state->param_names[i])) {
          parse_add_op(state, OPCODE_PARAMETER, 1)->arg.ival = i;
          return parse_next_token(state);
        }
      }

      /* Ordinary builtin constants. */
      for (i = 0; builtin_consts[i].name; i++) {
        if (STREQ(state->tokenbuf, builtin_consts[i].name)) {
          parse_add_op(state, OPCODE_CONST, 1)->arg.dval = builtin_consts[i].value;
          return parse_next_token(state);
        }
      }

      /* Ordinary builtin functions. */
      for (i = 0; builtin_ops[i].name; i++) {
        if (STREQ(state->tokenbuf, builtin_ops[i].name)) {
          int args = parse_function_args(state);

          return parse_add_func(state, builtin_ops[i].op, args, builtin_ops[i].funcptr);
        }
      }

      /* Specially supported functions. */
      if (STREQ(state->tokenbuf, "min")) {
        int cnt = parse_function_args(state);
        CHECK_ERROR(cnt > 0);

        parse_add_op(state, OPCODE_MIN, 1 - cnt)->arg.ival = cnt;
        return true;
      }

      if (STREQ(state->tokenbuf, "max")) {
        int cnt = parse_function_args(state);
        CHECK_ERROR(cnt > 0);

        parse_add_op(state, OPCODE_MAX, 1 - cnt)->arg.ival = cnt;
        return true;
      }

      return false;

    default:
      return false;
  }
}

static bool parse_mul(ExprParseState *state)
{
  CHECK_ERROR(parse_unary(state));

  for (;;) {
    switch (state->token) {
      case '*':
        CHECK_ERROR(parse_next_token(state) && parse_unary(state));
        parse_add_func(state, OPCODE_FUNC2, 2, op_mul);
        break;

      case '/':
        CHECK_ERROR(parse_next_token(state) && parse_unary(state));
        parse_add_func(state, OPCODE_FUNC2, 2, op_div);
        break;

      default:
        return true;
    }
  }
}

static bool parse_add(ExprParseState *state)
{
  CHECK_ERROR(parse_mul(state));

  for (;;) {
    switch (state->token) {
      case '+':
        CHECK_ERROR(parse_next_token(state) && parse_mul(state));
        parse_add_func(state, OPCODE_FUNC2, 2, op_add);
        break;

      case '-':
        CHECK_ERROR(parse_next_token(state) && parse_mul(state));
        parse_add_func(state, OPCODE_FUNC2, 2, op_sub);
        break;

      default:
        return true;
    }
  }
}

static BinaryOpFunc parse_get_cmp_func(short token)
{
  switch (token) {
    case TOKEN_EQ:
      return op_eq;
    case TOKEN_NE:
      return op_ne;
    case '>':
      return op_gt;
    case TOKEN_GE:
      return op_ge;
    case '<':
      return op_lt;
    case TOKEN_LE:
      return op_le;
    default:
      return NULL;
  }
}

static bool parse_cmp_chain(ExprParseState *state, BinaryOpFunc cur_func)
{
  BinaryOpFunc next_func = parse_get_cmp_func(state->token);

  if (next_func) {
    parse_add_op(state, OPCODE_CMP_CHAIN, -1)->arg.func2 = cur_func;
    int jump = state->last_jmp = state->ops_count;

    CHECK_ERROR(parse_next_token(state) && parse_add(state));
    CHECK_ERROR(parse_cmp_chain(state, next_func));

    parse_set_jump(state, jump);
  }
  else {
    parse_add_func(state, OPCODE_FUNC2, 2, cur_func);
  }

  return true;
}

static bool parse_cmp(ExprParseState *state)
{
  CHECK_ERROR(parse_add(state));

  BinaryOpFunc func = parse_get_cmp_func(state->token);

  if (func) {
    CHECK_ERROR(parse_next_token(state) && parse_add(state));

    return parse_cmp_chain(state, func);
  }

  return true;
}

static bool parse_not(ExprParseState *state)
{
  if (state->token == TOKEN_NOT) {
    CHECK_ERROR(parse_next_token(state) && parse_not(state));
    parse_add_func(state, OPCODE_FUNC1, 1, op_not);
    return true;
  }

  return parse_cmp(state);
}

static bool parse_and(ExprParseState *state)
{
  CHECK_ERROR(parse_not(state));

  if (state->token == TOKEN_AND) {
    int jump = parse_add_jump(state, OPCODE_JMP_AND);

    CHECK_ERROR(parse_next_token(state) && parse_and(state));

    parse_set_jump(state, jump);
  }

  return true;
}

static bool parse_or(ExprParseState *state)
{
  CHECK_ERROR(parse_and(state));

  if (state->token == TOKEN_OR) {
    int jump = parse_add_jump(state, OPCODE_JMP_OR);

    CHECK_ERROR(parse_next_token(state) && parse_or(state));

    parse_set_jump(state, jump);
  }

  return true;
}

static bool parse_expr(ExprParseState *state)
{
  /* Temporarily set the constant expression evaluation barrier */
  int prev_last_jmp = state->last_jmp;
  int start = state->last_jmp = state->ops_count;

  CHECK_ERROR(parse_or(state));

  if (state->token == TOKEN_IF) {
    /* Ternary IF expression in python requires swapping the
     * main body with condition, so stash the body opcodes. */
    int size = state->ops_count - start;
    int bytes = size * sizeof(ExprOp);

    ExprOp *body = MEM_mallocN(bytes, "driver if body");
    memcpy(body, state->ops + start, bytes);

    state->last_jmp = state->ops_count = start;
    state->stack_ptr--;

    /* Parse condition. */
    if (!parse_next_token(state) || !parse_or(state) || state->token != TOKEN_ELSE ||
        !parse_next_token(state)) {
      MEM_freeN(body);
      return false;
    }

    int jmp_else = parse_add_jump(state, OPCODE_JMP_ELSE);

    /* Add body back. */
    memcpy(parse_alloc_ops(state, size), body, bytes);
    MEM_freeN(body);

    state->stack_ptr++;

    int jmp_end = parse_add_jump(state, OPCODE_JMP);

    /* Parse the else block. */
    parse_set_jump(state, jmp_else);

    CHECK_ERROR(parse_expr(state));

    parse_set_jump(state, jmp_end);
  }
  /* If no actual jumps happened, restore previous barrier */
  else if (state->last_jmp == start) {
    state->last_jmp = prev_last_jmp;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Parsing Function
 * \{ */

/**
 * Compile the expression and return the result.
 *
 * Parse the expression for evaluation later.
 * Returns non-NULL even on failure; use is_valid to check.
 */
ExprPyLike_Parsed *BLI_expr_pylike_parse(const char *expression,
                                         const char **param_names,
                                         int param_names_len)
{
  /* Prepare the parser state. */
  ExprParseState state;
  memset(&state, 0, sizeof(state));

  state.cur = state.expr = expression;

  state.param_names_len = param_names_len;
  state.param_names = param_names;

  state.tokenbuf = MEM_mallocN(strlen(expression) + 1, __func__);

  state.max_ops = 16;
  state.ops = MEM_mallocN(state.max_ops * sizeof(ExprOp), __func__);

  /* Parse the expression. */
  ExprPyLike_Parsed *expr;

  if (parse_next_token(&state) && parse_expr(&state) && state.token == 0) {
    BLI_assert(state.stack_ptr == 1);

    int bytesize = sizeof(ExprPyLike_Parsed) + state.ops_count * sizeof(ExprOp);

    expr = MEM_mallocN(bytesize, "ExprPyLike_Parsed");
    expr->ops_count = state.ops_count;
    expr->max_stack = state.max_stack;

    memcpy(expr->ops, state.ops, state.ops_count * sizeof(ExprOp));
  }
  else {
    /* Always return a non-NULL object so that parse failure can be cached. */
    expr = MEM_callocN(sizeof(ExprPyLike_Parsed), "ExprPyLike_Parsed(empty)");
  }

  MEM_freeN(state.tokenbuf);
  MEM_freeN(state.ops);
  return expr;
}

/** \} */

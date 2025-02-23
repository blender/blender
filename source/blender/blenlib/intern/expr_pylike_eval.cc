/* SPDX-FileCopyrightText: 2018 Blender Authors, Alexander Gavrilov. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 * The implementation has no global state and can be used multi-threaded.
 */

#include <cctype>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <variant>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_expr_pylike_eval.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#ifdef _MSC_VER
#  pragma fenv_access(on)
#endif

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

enum eOpCode {
  /** Double constant: `(-> dval)`. */
  OPCODE_CONST,
  /** 1 argument function call: `(a -> func1(a))`. */
  OPCODE_FUNC1,
  /** 2 argument function call: `(a b -> func2(a,b))`. */
  OPCODE_FUNC2,
  /** 3 argument function call: `(a b c -> func3(a,b,c))`. */
  OPCODE_FUNC3,
  /** Parameter access: `(-> params[ival])`. */
  OPCODE_PARAMETER,
  /** Minimum of multiple inputs: `(a b c... -> min); ival = arg count`. */
  OPCODE_MIN,
  /** Maximum of multiple inputs: `(a b c... -> max); ival = arg count`. */
  OPCODE_MAX,
  /** Jump `(pc += jmp_offset)` */
  OPCODE_JMP,
  /** Pop and jump if zero: `(a -> ); JUMP IF NOT a`. */
  OPCODE_JMP_ELSE,
  /** Jump if nonzero, or pop: `(a -> a JUMP) IF a ELSE (a -> )`. */
  OPCODE_JMP_OR,
  /** Jump if zero, or pop: `(a -> a JUMP) IF NOT a ELSE (a -> )`. */
  OPCODE_JMP_AND,
  /** For comparison chaining: `(a b -> 0 JUMP) IF NOT func2(a,b) ELSE (a b -> b)`. */
  OPCODE_CMP_CHAIN,
};

using UnaryOpFunc = double (*)(double);
using BinaryOpFunc = double (*)(double, double);
using TernaryOpFunc = double (*)(double, double, double);

struct ExprOp {
  eOpCode opcode;

  int jmp_offset;

  union {
    int ival;
    double dval;
    void *ptr;
    UnaryOpFunc func1;
    BinaryOpFunc func2;
    TernaryOpFunc func3;
  } arg;
};

struct ExprPyLike_Parsed {
  blender::Vector<ExprOp> ops;
  int max_stack;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void BLI_expr_pylike_free(ExprPyLike_Parsed *expr)
{
  MEM_delete(expr);
}

bool BLI_expr_pylike_is_valid(const ExprPyLike_Parsed *expr)
{
  return expr != nullptr && expr->ops.size() > 0;
}

bool BLI_expr_pylike_is_constant(const ExprPyLike_Parsed *expr)
{
  return expr != nullptr && expr->ops.size() == 1 && expr->ops[0].opcode == OPCODE_CONST;
}

bool BLI_expr_pylike_is_using_param(const ExprPyLike_Parsed *expr, int index)
{
  int i;

  if (expr == nullptr) {
    return false;
  }

  for (i = 0; i < expr->ops.size(); i++) {
    if (expr->ops[i].opcode == OPCODE_PARAMETER && expr->ops[i].arg.ival == index) {
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stack Machine Evaluation
 * \{ */

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
  ExprOp *ops = expr->ops.data();
  int sp = 0, pc;

  feclearexcept(FE_ALL_EXCEPT);

  for (pc = 0; pc >= 0 && pc < expr->ops.size(); pc++) {
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
      case OPCODE_FUNC3:
        FAIL_IF(sp < 3);
        stack[sp - 3] = ops[pc].arg.func3(stack[sp - 3], stack[sp - 2], stack[sp - 1]);
        sp -= 2;
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

  FAIL_IF(sp != 1 || pc != expr->ops.size());

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

static double op_log2(double a, double b)
{
  return log(a) / log(b);
}

static double op_lerp(double a, double b, double x)
{
  return a * (1.0 - x) + b * x;
}

static double op_clamp(double arg)
{
  CLAMP(arg, 0.0, 1.0);
  return arg;
}

static double op_clamp3(double arg, double minv, double maxv)
{
  CLAMP(arg, minv, maxv);
  return arg;
}

static double op_smoothstep(double a, double b, double x)
{
  double t = (x - a) / (b - a);
  CLAMP(t, 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
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

struct BuiltinConstDef {
  const char *name;
  double value;
};

static BuiltinConstDef builtin_consts[] = {
    {"pi", M_PI}, {"True", 1.0}, {"False", 0.0}, {nullptr, 0.0}};

struct BuiltinOpDef {
  const char *name;
  std::variant<UnaryOpFunc, BinaryOpFunc, TernaryOpFunc> funcptr;

  /* Returns the required argument count of the given function call code. */
  int arg_count()
  {
    if (std::holds_alternative<UnaryOpFunc>(funcptr)) {
      return 1;
    }
    if (std::holds_alternative<BinaryOpFunc>(funcptr)) {
      return 2;
    }
    if (std::holds_alternative<TernaryOpFunc>(funcptr)) {
      return 3;
    }

    BLI_assert_msg(0, "unexpected function pointer");
    return -1;
  }
};

#ifdef _MSC_VER
/* Prevent MSVC from inlining calls to ceil/floor so the table below can get a function pointer to
 * them. */
#  pragma function(ceil)
#  pragma function(floor)
#endif

static BuiltinOpDef builtin_ops[] = {
    {"radians", UnaryOpFunc(op_radians)},
    {"degrees", UnaryOpFunc(op_degrees)},
    {"abs", UnaryOpFunc(fabs)},
    {"fabs", UnaryOpFunc(fabs)},
    {"floor", UnaryOpFunc(floor)},
    {"ceil", UnaryOpFunc(ceil)},
    {"trunc", UnaryOpFunc(trunc)},
    {"round", UnaryOpFunc(round)},
    {"int", UnaryOpFunc(trunc)},
    {"sin", UnaryOpFunc(sin)},
    {"cos", UnaryOpFunc(cos)},
    {"tan", UnaryOpFunc(tan)},
    {"asin", UnaryOpFunc(asin)},
    {"acos", UnaryOpFunc(acos)},
    {"atan", UnaryOpFunc(atan)},
    {"atan2", BinaryOpFunc(atan2)},
    {"exp", UnaryOpFunc(exp)},
    {"log", UnaryOpFunc(log)},
    {"log", BinaryOpFunc(op_log2)},
    {"sqrt", UnaryOpFunc(sqrt)},
    {"pow", BinaryOpFunc(pow)},
    {"fmod", BinaryOpFunc(fmod)},
    {"lerp", TernaryOpFunc(op_lerp)},
    {"clamp", UnaryOpFunc(op_clamp)},
    {"clamp", TernaryOpFunc(op_clamp3)},
    {"smoothstep", TernaryOpFunc(op_smoothstep)},
    {nullptr},
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

struct KeywordTokenDef {
  const char *name;
  short token;
};

static KeywordTokenDef keyword_list[] = {
    {"and", TOKEN_AND},
    {"or", TOKEN_OR},
    {"not", TOKEN_NOT},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {nullptr, TOKEN_ID},
};

struct ExprParseState {
  int param_names_len = 0;
  const char **param_names = nullptr;

  /* Original expression */
  const char *expr = nullptr;
  const char *cur = nullptr;

  /* Current token */
  short token = 0;
  blender::Vector<char> tokenbuf;
  double tokenval = 0.0;

  /* Opcode buffer */
  int last_jmp = 0;
  blender::Vector<ExprOp> ops;

  /* Stack space requirement tracking */
  int stack_ptr = 0;
  int max_stack = 0;
};

/* Add one operation and track stack usage. */
static ExprOp *parse_add_op(ExprParseState *state, eOpCode code, int stack_delta)
{
  /* track evaluation stack depth */
  state->stack_ptr += stack_delta;
  CLAMP_MIN(state->stack_ptr, 0);
  CLAMP_MIN(state->max_stack, state->stack_ptr);

  /* allocate the new instruction */
  ExprOp op{code};
  state->ops.append(op);
  return &state->ops.last();
}

/* Add one jump operation and return an index for parse_set_jump. */
static int parse_add_jump(ExprParseState *state, eOpCode code)
{
  parse_add_op(state, code, -1);
  return state->last_jmp = state->ops.size();
}

/* Set the jump offset in a previously added jump operation. */
static void parse_set_jump(ExprParseState *state, int jump)
{
  state->last_jmp = state->ops.size();
  state->ops[jump - 1].jmp_offset = state->ops.size() - jump;
}

/* Add a function call operation, applying constant folding when possible. */
static bool parse_add_func(ExprParseState *state,
                           std::variant<UnaryOpFunc, BinaryOpFunc, TernaryOpFunc> funcptr)
{
  ExprOp *prev_ops = state->ops.end();
  int jmp_gap = state->ops.size() - state->last_jmp;

  feclearexcept(FE_ALL_EXCEPT);

  if (std::holds_alternative<UnaryOpFunc>(funcptr)) {
    UnaryOpFunc func = std::get<UnaryOpFunc>(funcptr);

    if (jmp_gap >= 1 && prev_ops[-1].opcode == OPCODE_CONST) {
      /* volatile because some compilers overly aggressive optimize this call out.
       * see D6012 for details. */
      volatile double result = func(prev_ops[-1].arg.dval);

      if (fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0) {
        prev_ops[-1].arg.dval = result;
        return true;
      }
    }

    parse_add_op(state, OPCODE_FUNC1, 0)->arg.func1 = func;
  }
  else if (std::holds_alternative<BinaryOpFunc>(funcptr)) {
    BinaryOpFunc func = std::get<BinaryOpFunc>(funcptr);

    if (jmp_gap >= 2 && prev_ops[-2].opcode == OPCODE_CONST && prev_ops[-1].opcode == OPCODE_CONST)
    {
      /* volatile because some compilers overly aggressive optimize this call out.
       * see D6012 for details. */
      volatile double result = func(prev_ops[-2].arg.dval, prev_ops[-1].arg.dval);

      if (fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0) {
        prev_ops[-2].arg.dval = result;
        state->ops.resize(state->ops.size() - 1);
        state->stack_ptr--;
        return true;
      }
    }

    parse_add_op(state, OPCODE_FUNC2, -1)->arg.func2 = func;
  }
  else if (std::holds_alternative<TernaryOpFunc>(funcptr)) {
    TernaryOpFunc func = std::get<TernaryOpFunc>(funcptr);

    if (jmp_gap >= 3 && prev_ops[-3].opcode == OPCODE_CONST &&
        prev_ops[-2].opcode == OPCODE_CONST && prev_ops[-1].opcode == OPCODE_CONST)
    {
      /* volatile because some compilers overly aggressive optimize this call out.
       * see D6012 for details. */
      volatile double result = func(
          prev_ops[-3].arg.dval, prev_ops[-2].arg.dval, prev_ops[-1].arg.dval);

      if (fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0) {
        prev_ops[-3].arg.dval = result;
        state->ops.resize(state->ops.size() - 2);
        state->stack_ptr -= 2;
        return true;
      }
    }

    parse_add_op(state, OPCODE_FUNC3, -2)->arg.func3 = func;
  }
  else {
    BLI_assert(false);
    return false;
  }

  return true;
}

/* Extract the next token from raw characters. */
static bool parse_next_token(ExprParseState *state)
{
  /* Skip white-space. */
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
    char *end, *out = state->tokenbuf.data();
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
      for (char *p = state->tokenbuf.data() + 1; *p; p++) {
        if (*p != '0') {
          return false;
        }
      }
    }

    state->token = TOKEN_NUMBER;
    state->tokenval = strtod(state->tokenbuf.data(), &end);
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
    char *out = state->tokenbuf.data();

    while (isalnum(*state->cur) || ELEM(*state->cur, '_')) {
      *out++ = *state->cur++;
    }

    *out = 0;

    for (int i = 0; keyword_list[i].name; i++) {
      if (STREQ(state->tokenbuf.data(), keyword_list[i].name)) {
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
      parse_add_func(state, op_negate);
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
        if (STREQ(state->tokenbuf.data(), state->param_names[i])) {
          parse_add_op(state, OPCODE_PARAMETER, 1)->arg.ival = i;
          return parse_next_token(state);
        }
      }

      /* Ordinary builtin constants. */
      for (i = 0; builtin_consts[i].name; i++) {
        if (STREQ(state->tokenbuf.data(), builtin_consts[i].name)) {
          parse_add_op(state, OPCODE_CONST, 1)->arg.dval = builtin_consts[i].value;
          return parse_next_token(state);
        }
      }

      /* Ordinary builtin functions. */
      for (i = 0; builtin_ops[i].name; i++) {
        if (STREQ(state->tokenbuf.data(), builtin_ops[i].name)) {
          int args = parse_function_args(state);

          /* Search for other arg count versions if necessary. */
          if (args != builtin_ops[i].arg_count()) {
            for (int j = i + 1; builtin_ops[j].name; j++) {
              if (builtin_ops[j].arg_count() == args &&
                  STREQ(builtin_ops[j].name, builtin_ops[i].name))
              {
                i = j;
                break;
              }
            }
          }

          CHECK_ERROR(builtin_ops[i].name && builtin_ops[i].arg_count() == args);

          return parse_add_func(state, builtin_ops[i].funcptr);
        }
      }

      /* Specially supported functions. */
      if (STREQ(state->tokenbuf.data(), "min")) {
        int count = parse_function_args(state);
        CHECK_ERROR(count > 0);

        parse_add_op(state, OPCODE_MIN, 1 - count)->arg.ival = count;
        return true;
      }

      if (STREQ(state->tokenbuf.data(), "max")) {
        int count = parse_function_args(state);
        CHECK_ERROR(count > 0);

        parse_add_op(state, OPCODE_MAX, 1 - count)->arg.ival = count;
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
        parse_add_func(state, op_mul);
        break;

      case '/':
        CHECK_ERROR(parse_next_token(state) && parse_unary(state));
        parse_add_func(state, op_div);
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
        parse_add_func(state, op_add);
        break;

      case '-':
        CHECK_ERROR(parse_next_token(state) && parse_mul(state));
        parse_add_func(state, op_sub);
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
      return nullptr;
  }
}

static bool parse_cmp_chain(ExprParseState *state, BinaryOpFunc cur_func)
{
  BinaryOpFunc next_func = parse_get_cmp_func(state->token);

  if (next_func) {
    parse_add_op(state, OPCODE_CMP_CHAIN, -1)->arg.func2 = cur_func;
    int jump = state->last_jmp = state->ops.size();

    CHECK_ERROR(parse_next_token(state) && parse_add(state));
    CHECK_ERROR(parse_cmp_chain(state, next_func));

    parse_set_jump(state, jump);
  }
  else {
    parse_add_func(state, cur_func);
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
    parse_add_func(state, op_not);
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
  int start = state->last_jmp = state->ops.size();

  CHECK_ERROR(parse_or(state));

  if (state->token == TOKEN_IF) {
    /* Ternary IF expression in python requires swapping the
     * main body with condition, so stash the body opcodes. */
    const int size = state->ops.size() - start;

    blender::Vector<ExprOp> body(size);
    std::copy_n(state->ops.data() + start, size, body.data());

    state->ops.resize(start);
    state->last_jmp = start;
    state->stack_ptr--;

    /* Parse condition. */
    if (!parse_next_token(state) || !parse_or(state) || state->token != TOKEN_ELSE ||
        !parse_next_token(state))
    {
      return false;
    }

    int jmp_else = parse_add_jump(state, OPCODE_JMP_ELSE);

    /* Add body back. */
    const size_t body_offset = state->ops.size();
    state->ops.resize(body_offset + size);
    std::copy_n(body.data(), size, state->ops.data() + body_offset);
    body.clear_and_shrink();

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

ExprPyLike_Parsed *BLI_expr_pylike_parse(const char *expression,
                                         const char **param_names,
                                         int param_names_len)
{
  /* Prepare the parser state. */
  ExprParseState state;

  state.cur = state.expr = expression;

  state.param_names_len = param_names_len;
  state.param_names = param_names;

  state.tokenbuf.resize(strlen(expression) + 1);

  /* Parse the expression. */
  ExprPyLike_Parsed *expr = MEM_new<ExprPyLike_Parsed>("ExprPyLike_Parsed(empty)");

  if (parse_next_token(&state) && parse_expr(&state) && state.token == 0) {
    BLI_assert(state.stack_ptr == 1);

    expr->max_stack = state.max_stack;
    expr->ops = std::move(state.ops);
  }
  else {
    /* Always return a non-nullptr object so that parse failure can be cached. */
  }

  return expr;
}

/** \} */

/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include <string.h>

#include "BLI_expr_pylike_eval.h"
#include "BLI_math.h"

#define TRUE_VAL 1.0
#define FALSE_VAL 0.0

static void expr_pylike_parse_fail_test(const char *str)
{
  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse(str, NULL, 0);

  EXPECT_FALSE(BLI_expr_pylike_is_valid(expr));

  BLI_expr_pylike_free(expr);
}

static void expr_pylike_const_test(const char *str, double value, bool force_const)
{
  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse(str, NULL, 0);

  if (force_const) {
    EXPECT_TRUE(BLI_expr_pylike_is_constant(expr));
  }
  else {
    EXPECT_TRUE(BLI_expr_pylike_is_valid(expr));
    EXPECT_FALSE(BLI_expr_pylike_is_constant(expr));
  }

  double result;
  eExprPyLike_EvalStatus status = BLI_expr_pylike_eval(expr, NULL, 0, &result);

  EXPECT_EQ(status, EXPR_PYLIKE_SUCCESS);
  EXPECT_EQ(result, value);

  BLI_expr_pylike_free(expr);
}

static ExprPyLike_Parsed *parse_for_eval(const char *str, bool nonconst)
{
  const char *names[1] = {"x"};
  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse(str, names, ARRAY_SIZE(names));

  EXPECT_TRUE(BLI_expr_pylike_is_valid(expr));

  if (nonconst) {
    EXPECT_FALSE(BLI_expr_pylike_is_constant(expr));
  }

  return expr;
}

static void verify_eval_result(ExprPyLike_Parsed *expr, double x, double value)
{
  double result;
  eExprPyLike_EvalStatus status = BLI_expr_pylike_eval(expr, &x, 1, &result);

  EXPECT_EQ(status, EXPR_PYLIKE_SUCCESS);
  EXPECT_EQ(result, value);
}

static void expr_pylike_eval_test(const char *str, double x, double value)
{
  ExprPyLike_Parsed *expr = parse_for_eval(str, true);
  verify_eval_result(expr, x, value);
  BLI_expr_pylike_free(expr);
}

static void expr_pylike_error_test(const char *str, double x, eExprPyLike_EvalStatus error)
{
  ExprPyLike_Parsed *expr = parse_for_eval(str, false);

  double result;
  eExprPyLike_EvalStatus status = BLI_expr_pylike_eval(expr, &x, 1, &result);

  EXPECT_EQ(status, error);

  BLI_expr_pylike_free(expr);
}

#define TEST_PARSE_FAIL(name, str) \
  TEST(expr_pylike, ParseFail_##name) \
  { \
    expr_pylike_parse_fail_test(str); \
  }

TEST_PARSE_FAIL(Empty, "")
TEST_PARSE_FAIL(ConstHex, "0x0")
TEST_PARSE_FAIL(ConstOctal, "01")
TEST_PARSE_FAIL(Tail, "0 0")
TEST_PARSE_FAIL(ConstFloatExp, "0.5e+")
TEST_PARSE_FAIL(BadId, "Pi")
TEST_PARSE_FAIL(BadArgCount0, "sqrt")
TEST_PARSE_FAIL(BadArgCount1, "sqrt()")
TEST_PARSE_FAIL(BadArgCount2, "sqrt(1,2)")
TEST_PARSE_FAIL(BadArgCount3, "pi()")
TEST_PARSE_FAIL(BadArgCount4, "max()")
TEST_PARSE_FAIL(BadArgCount5, "min()")

TEST_PARSE_FAIL(Truncated1, "(1+2")
TEST_PARSE_FAIL(Truncated2, "1 if 2")
TEST_PARSE_FAIL(Truncated3, "1 if 2 else")
TEST_PARSE_FAIL(Truncated4, "1 < 2 <")
TEST_PARSE_FAIL(Truncated5, "1 +")
TEST_PARSE_FAIL(Truncated6, "1 *")
TEST_PARSE_FAIL(Truncated7, "1 and")
TEST_PARSE_FAIL(Truncated8, "1 or")
TEST_PARSE_FAIL(Truncated9, "sqrt(1")
TEST_PARSE_FAIL(Truncated10, "fmod(1,")

/* Constant expression with working constant folding */
#define TEST_CONST(name, str, value) \
  TEST(expr_pylike, Const_##name) \
  { \
    expr_pylike_const_test(str, value, true); \
  }

/* Constant expression but constant folding is not supported */
#define TEST_RESULT(name, str, value) \
  TEST(expr_pylike, Result_##name) \
  { \
    expr_pylike_const_test(str, value, false); \
  }

/* Expression with an argument */
#define TEST_EVAL(name, str, x, value) \
  TEST(expr_pylike, Eval_##name) \
  { \
    expr_pylike_eval_test(str, x, value); \
  }

TEST_CONST(Zero, "0", 0.0)
TEST_CONST(Zero2, "00", 0.0)
TEST_CONST(One, "1", 1.0)
TEST_CONST(OneF, "1.0", 1.0)
TEST_CONST(OneF2, "1.", 1.0)
TEST_CONST(OneE, "1e0", 1.0)
TEST_CONST(TenE, "1.e+1", 10.0)
TEST_CONST(Half, ".5", 0.5)

TEST_CONST(Pi, "pi", M_PI)
TEST_CONST(True, "True", TRUE_VAL)
TEST_CONST(False, "False", FALSE_VAL)

TEST_CONST(Sqrt, "sqrt(4)", 2.0)
TEST_EVAL(Sqrt, "sqrt(x)", 4.0, 2.0)

TEST_CONST(FMod, "fmod(3.5, 2)", 1.5)
TEST_EVAL(FMod, "fmod(x, 2)", 3.5, 1.5)

TEST_CONST(Pow, "pow(4, 0.5)", 2.0)
TEST_EVAL(Pow, "pow(4, x)", 0.5, 2.0)

TEST_CONST(Log2_1, "log(4, 2)", 2.0)

TEST_CONST(Round1, "round(-0.5)", -1.0)
TEST_CONST(Round2, "round(-0.4)", 0.0)
TEST_CONST(Round3, "round(0.4)", 0.0)
TEST_CONST(Round4, "round(0.5)", 1.0)

TEST_CONST(Clamp1, "clamp(-0.1)", 0.0)
TEST_CONST(Clamp2, "clamp(0.5)", 0.5)
TEST_CONST(Clamp3, "clamp(1.5)", 1.0)
TEST_CONST(Clamp4, "clamp(0.5, 0.2, 0.3)", 0.3)
TEST_CONST(Clamp5, "clamp(0.0, 0.2, 0.3)", 0.2)

TEST_CONST(Lerp1, "lerp(-10,10,-1)", -30.0)
TEST_CONST(Lerp2, "lerp(-10,10,0.25)", -5.0)
TEST_CONST(Lerp3, "lerp(-10,10,1)", 10.0)
TEST_EVAL(Lerp1, "lerp(-10,10,x)", 0, -10.0)
TEST_EVAL(Lerp2, "lerp(-10,10,x)", 0.75, 5.0)

TEST_CONST(Smoothstep1, "smoothstep(-10,10,-20)", 0.0)
TEST_CONST(Smoothstep2, "smoothstep(-10,10,-10)", 0.0)
TEST_CONST(Smoothstep3, "smoothstep(-10,10,10)", 1.0)
TEST_CONST(Smoothstep4, "smoothstep(-10,10,20)", 1.0)
TEST_CONST(Smoothstep5, "smoothstep(-10,10,-5)", 0.15625)
TEST_EVAL(Smoothstep1, "smoothstep(-10,10,x)", 5, 0.84375)

TEST_RESULT(Min1, "min(3,1,2)", 1.0)
TEST_RESULT(Max1, "max(3,1,2)", 3.0)
TEST_RESULT(Min2, "min(1,2,3)", 1.0)
TEST_RESULT(Max2, "max(1,2,3)", 3.0)
TEST_RESULT(Min3, "min(2,3,1)", 1.0)
TEST_RESULT(Max3, "max(2,3,1)", 3.0)

TEST_CONST(UnaryPlus, "+1", 1.0)

TEST_CONST(UnaryMinus, "-1", -1.0)
TEST_EVAL(UnaryMinus, "-x", 1.0, -1.0)

TEST_CONST(BinaryPlus, "1+2", 3.0)
TEST_EVAL(BinaryPlus, "x+2", 1, 3.0)

TEST_CONST(BinaryMinus, "1-2", -1.0)
TEST_EVAL(BinaryMinus, "1-x", 2, -1.0)

TEST_CONST(BinaryMul, "2*3", 6.0)
TEST_EVAL(BinaryMul, "x*3", 2, 6.0)

TEST_CONST(BinaryDiv, "3/2", 1.5)
TEST_EVAL(BinaryDiv, "3/x", 2, 1.5)

TEST_CONST(Arith1, "1 + -2 * 3", -5.0)
TEST_CONST(Arith2, "(1 + -2) * 3", -3.0)
TEST_CONST(Arith3, "-1 + 2 * 3", 5.0)
TEST_CONST(Arith4, "3 * (-2 + 1)", -3.0)

TEST_EVAL(Arith1, "1 + -x * 3", 2, -5.0)

TEST_CONST(Eq1, "1 == 1.0", TRUE_VAL)
TEST_CONST(Eq2, "1 == 2.0", FALSE_VAL)
TEST_CONST(Eq3, "True == 1", TRUE_VAL)
TEST_CONST(Eq4, "False == 0", TRUE_VAL)

TEST_EVAL(Eq1, "1 == x", 1.0, TRUE_VAL)
TEST_EVAL(Eq2, "1 == x", 2.0, FALSE_VAL)

TEST_CONST(NEq1, "1 != 1.0", FALSE_VAL)
TEST_CONST(NEq2, "1 != 2.0", TRUE_VAL)

TEST_EVAL(NEq1, "1 != x", 1.0, FALSE_VAL)
TEST_EVAL(NEq2, "1 != x", 2.0, TRUE_VAL)

TEST_CONST(Lt1, "1 < 1", FALSE_VAL)
TEST_CONST(Lt2, "1 < 2", TRUE_VAL)
TEST_CONST(Lt3, "2 < 1", FALSE_VAL)

TEST_CONST(Le1, "1 <= 1", TRUE_VAL)
TEST_CONST(Le2, "1 <= 2", TRUE_VAL)
TEST_CONST(Le3, "2 <= 1", FALSE_VAL)

TEST_CONST(Gt1, "1 > 1", FALSE_VAL)
TEST_CONST(Gt2, "1 > 2", FALSE_VAL)
TEST_CONST(Gt3, "2 > 1", TRUE_VAL)

TEST_CONST(Ge1, "1 >= 1", TRUE_VAL)
TEST_CONST(Ge2, "1 >= 2", FALSE_VAL)
TEST_CONST(Ge3, "2 >= 1", TRUE_VAL)

TEST_CONST(Cmp1, "3 == 1 + 2", TRUE_VAL)

TEST_EVAL(Cmp1, "3 == x + 2", 1, TRUE_VAL)
TEST_EVAL(Cmp1b, "3 == x + 2", 1.5, FALSE_VAL)

TEST_RESULT(CmpChain1, "1 < 2 < 3", TRUE_VAL)
TEST_RESULT(CmpChain2, "1 < 2 == 2", TRUE_VAL)
TEST_RESULT(CmpChain3, "1 < 2 > -1", TRUE_VAL)
TEST_RESULT(CmpChain4, "1 < 2 < 2 < 3", FALSE_VAL)
TEST_RESULT(CmpChain5, "1 < 2 <= 2 < 3", TRUE_VAL)

TEST_EVAL(CmpChain1a, "1 < x < 3", 2, TRUE_VAL)
TEST_EVAL(CmpChain1b, "1 < x < 3", 1, FALSE_VAL)
TEST_EVAL(CmpChain1c, "1 < x < 3", 3, FALSE_VAL)

TEST_CONST(Not1, "not 2", FALSE_VAL)
TEST_CONST(Not2, "not 0", TRUE_VAL)
TEST_CONST(Not3, "not not 2", TRUE_VAL)

TEST_EVAL(Not1, "not x", 2, FALSE_VAL)
TEST_EVAL(Not2, "not x", 0, TRUE_VAL)

TEST_RESULT(And1, "2 and 3", 3.0)
TEST_RESULT(And2, "0 and 3", 0.0)

TEST_RESULT(Or1, "2 or 3", 2.0)
TEST_RESULT(Or2, "0 or 3", 3.0)

TEST_RESULT(Bool1, "2 or 3 and 4", 2.0)
TEST_RESULT(Bool2, "not 2 or 3 and 4", 4.0)

TEST(expr_pylike, Eval_Ternary1)
{
  ExprPyLike_Parsed *expr = parse_for_eval("x / 2 if x < 4 else x - 2 if x < 8 else x*2 - 12",
                                           true);

  for (int i = 0; i <= 10; i++) {
    double x = i;
    double v = (x < 4) ? (x / 2) : (x < 8) ? (x - 2) : (x * 2 - 12);

    verify_eval_result(expr, x, v);
  }

  BLI_expr_pylike_free(expr);
}

TEST(expr_pylike, MultipleArgs)
{
  const char *names[3] = {"x", "y", "x"};
  double values[3] = {1.0, 2.0, 3.0};

  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse("x*10 + y", names, ARRAY_SIZE(names));

  EXPECT_TRUE(BLI_expr_pylike_is_valid(expr));

  double result;
  eExprPyLike_EvalStatus status = BLI_expr_pylike_eval(expr, values, 3, &result);

  EXPECT_EQ(status, EXPR_PYLIKE_SUCCESS);
  EXPECT_EQ(result, 32.0);

  BLI_expr_pylike_free(expr);
}

TEST(expr_pylike, UsingParam)
{
  const char *names[3] = {"x", "y", "z"};

  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse("x + z", names, ARRAY_SIZE(names));

  EXPECT_TRUE(BLI_expr_pylike_is_using_param(expr, 0));
  EXPECT_FALSE(BLI_expr_pylike_is_using_param(expr, 1));
  EXPECT_TRUE(BLI_expr_pylike_is_using_param(expr, 2));

  BLI_expr_pylike_free(expr);
}

#define TEST_ERROR(name, str, x, code) \
  TEST(expr_pylike, Error_##name) \
  { \
    expr_pylike_error_test(str, x, code); \
  }

TEST_ERROR(DivZero1, "0 / 0", 0.0, EXPR_PYLIKE_MATH_ERROR)
TEST_ERROR(DivZero2, "1 / 0", 0.0, EXPR_PYLIKE_DIV_BY_ZERO)
TEST_ERROR(DivZero3, "1 / x", 0.0, EXPR_PYLIKE_DIV_BY_ZERO)
TEST_ERROR(DivZero4, "1 / x", 1.0, EXPR_PYLIKE_SUCCESS)

TEST_ERROR(SqrtDomain1, "sqrt(-1)", 0.0, EXPR_PYLIKE_MATH_ERROR)
TEST_ERROR(SqrtDomain2, "sqrt(x)", -1.0, EXPR_PYLIKE_MATH_ERROR)
TEST_ERROR(SqrtDomain3, "sqrt(x)", 0.0, EXPR_PYLIKE_SUCCESS)

TEST_ERROR(PowDomain1, "pow(-1, 0.5)", 0.0, EXPR_PYLIKE_MATH_ERROR)
TEST_ERROR(PowDomain2, "pow(-1, x)", 0.5, EXPR_PYLIKE_MATH_ERROR)
TEST_ERROR(PowDomain3, "pow(-1, x)", 2.0, EXPR_PYLIKE_SUCCESS)

TEST_ERROR(Mixed1, "sqrt(x) + 1 / max(0, x)", -1.0, EXPR_PYLIKE_MATH_ERROR)
TEST_ERROR(Mixed2, "sqrt(x) + 1 / max(0, x)", 0.0, EXPR_PYLIKE_DIV_BY_ZERO)
TEST_ERROR(Mixed3, "sqrt(x) + 1 / max(0, x)", 1.0, EXPR_PYLIKE_SUCCESS)

TEST(expr_pylike, Error_Invalid)
{
  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse("", NULL, 0);
  double result;

  EXPECT_EQ(BLI_expr_pylike_eval(expr, NULL, 0, &result), EXPR_PYLIKE_INVALID);

  BLI_expr_pylike_free(expr);
}

TEST(expr_pylike, Error_ArgumentCount)
{
  ExprPyLike_Parsed *expr = parse_for_eval("x", false);
  double result;

  EXPECT_EQ(BLI_expr_pylike_eval(expr, NULL, 0, &result), EXPR_PYLIKE_FATAL_ERROR);

  BLI_expr_pylike_free(expr);
}

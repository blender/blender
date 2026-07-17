/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

#include "expression.hh"

namespace blender::gpu::tests {

using namespace std;
using namespace shader::parser;

static int test_expression(std::string str)
{
  ExpressionParser parser;
  parser.lexical_analysis(str);
  try {
    return parser.eval();
  }
  catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 9999999;
  }
}

TEST(shader_tool, ExpressionParser)
{
  /* --- Basic arithmetic --- */
  EXPECT_EQ(test_expression("1+2+3"), 6);
  EXPECT_EQ(test_expression("1*2+3"), 5);
  EXPECT_EQ(test_expression("1+2*3"), 7);
  EXPECT_EQ(test_expression("10-3-2"), 5);
  EXPECT_EQ(test_expression("10-(3-2)"), 9);
  EXPECT_EQ(test_expression("20/5/2"), 2);

  /* --- Parenthesis --- */
  EXPECT_EQ(test_expression("(1+2)*3"), 9);
  EXPECT_EQ(test_expression("((2+3)*4)"), 20);

  /* --- Unary operators --- */
  EXPECT_EQ(test_expression("-1+2"), 1);
  EXPECT_EQ(test_expression("~0"), ~0);
  EXPECT_EQ(test_expression("!0"), 1);
  EXPECT_EQ(test_expression("!5"), 0);

  /* --- Bitwise operators --- */
  EXPECT_EQ(test_expression("1|2"), 3);
  EXPECT_EQ(test_expression("3&1"), 1);
  EXPECT_EQ(test_expression("1^3"), 2);
  /* Not supported yet. */
  // EXPECT_EQ(test_expression("1 << 3"), 8);
  // EXPECT_EQ(test_expression("8 >> 2"), 2);

  /* --- Bitwise vs arithmetic precedence --- */
  /* Not supported yet. */
  // EXPECT_EQ(test_expression("1 + 2 << 2"), 12); /* (1+2)<<2 */
  // EXPECT_EQ(test_expression("1 << 2 + 1"), 8);  /* 1<<(2+1) */

  /* --- Comparison operators --- */
  EXPECT_EQ(test_expression("1 < 2"), 1);
  EXPECT_EQ(test_expression("2 <= 2"), 1);
  EXPECT_EQ(test_expression("3 > 5"), 0);
  EXPECT_EQ(test_expression("3 != 4"), 1);
  EXPECT_EQ(test_expression("3 == 3"), 1);

  /* --- Logical operators --- */
  EXPECT_EQ(test_expression("1 && 1"), 1);
  EXPECT_EQ(test_expression("1 && 0"), 0);
  EXPECT_EQ(test_expression("0 || 1"), 1);
  EXPECT_EQ(test_expression("0 || 0"), 0);
  EXPECT_EQ(test_expression("0 || 0 || 1"), 1);

  /* --- Logical precedence --- */
  EXPECT_EQ(test_expression("0 || 1 && 0"), 0); /* && before || */
  EXPECT_EQ(test_expression("(0 || 1) && 0"), 0);

  /* --- Ternary operator --- */
  EXPECT_EQ(test_expression("1 ? 2 : 3"), 2);
  EXPECT_EQ(test_expression("0 ? 2 : 3"), 3);
  EXPECT_EQ(test_expression("1 ? 0 ? 2 : 3 : 4"), 3);
  EXPECT_EQ(test_expression("0 ? 1 : 2 ? 3 : 4"), 3);

  /* --- Mixed complex expressions --- */
  EXPECT_EQ(test_expression("(1+2*3) == 7 && (4|1) == 5"), 1);
  EXPECT_EQ(test_expression("!((3<1) == 0)"), 0);
  EXPECT_EQ(test_expression("!0 && !0"), 1);
  EXPECT_EQ(test_expression("!1 && !0"), 0);
  EXPECT_EQ(test_expression("!!1 && !0"), 1);

  /* --- Deep Ternary Nesting --- */
  EXPECT_EQ(test_expression("1 ? 10 + 5 : 20"), 15);
  EXPECT_EQ(test_expression("0 ? 1 : 0 ? 2 : 3"), 3);
  EXPECT_EQ(test_expression("1 ? (0 ? 1 : 2) : 3"), 2);
  EXPECT_EQ(test_expression("10 + (1 ? 5 : 0) * 2"), 20);

  /* --- Unary Chains --- */
  EXPECT_EQ(test_expression("! ~ -1"), 1);
  EXPECT_EQ(test_expression("-5 * -2"), 10);

  /* --- Precedence Boundary Tests --- */
  EXPECT_EQ(test_expression("1 == 1 | 2"), 3);
  EXPECT_EQ(test_expression("1 + 2 < 4"), 1);
  EXPECT_EQ(test_expression("1 | 2 && 0"), 0);

  /* --- Complex Boolean Logic --- */
  EXPECT_EQ(test_expression("!((1 + 2 == 3) && (4 * 5 <= 20) || (0 ? 1 : 0))"), 0);

  /* --- The Kitchen Sink --- */
  EXPECT_EQ(test_expression("(10 - 2 * 3 == 4) ? 50 : 100 + !0"), 50);
}

}  // namespace blender::gpu::tests

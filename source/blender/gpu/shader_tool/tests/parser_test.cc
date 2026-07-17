/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader::parser;
using namespace shader;
using namespace std;

TEST(shader_tool, Parser)
{
  using Parser = IntermediateForm<FullLexer, FullParser>;
  using Lexer = IntermediateForm<FullLexer, DummyParser>;

  ErrorHandler err_handler;

  {
    string input = R"(
1;
1.0;
2e10;
2e10f;
2.e10f;
2.0e-1f;
2.0e-1;
2.0e-1f;
0xFF;
0xFFu;
0+8;
)";
    string expect = R"(
1;1;1;1;1;1;1;1;1;1;1+1;)";
    EXPECT_EQ(Lexer(input, err_handler).token_types_str(), expect);
  }
  {
    string input = R"(
[[a(0,1,b), c, d(t)]]
)";
    string expect = R"(
[[A(1,1,A),A,A(A)]])";
    string scopes = R"(GABbcmmmbbcm)";
    EXPECT_EQ(Parser(input, err_handler).token_types_str(), expect);
    EXPECT_EQ(Parser(input, err_handler).scope_types_str, scopes);
  }
  {
    string input = R"(
struct T {
    int t = 1;
};
class B {
    T t;
};
)";
    string expect = R"(
sA{AA=1;};SA{AA;};)";
    EXPECT_EQ(Lexer(input, err_handler).token_types_str(), expect);
  }
  {
    string input = R"(
a /* Comment */
//
a
/* //
*/
a
// a
a
)";
    string expect = R"(
AZZAZAZA)";
    EXPECT_EQ(Lexer(input, err_handler).token_types_str(), expect);
  }
  {
    string input = R"(
namespace T {}
namespace T::U::V {}
)";
    string expect = R"(
nA{}nA::A::A{})";
    string expect_scopes = R"(GNN)";
    EXPECT_EQ(Parser(input, err_handler).token_types_str(), expect);
    EXPECT_EQ(Parser(input, err_handler).scope_types_str, expect_scopes);
  }
  {
    string input = R"(
void f(int t = 0) {
  int i = 0, u = 2, v = {1.0f};
  {
    v = i = u, v++;
    if (v == i) {
      return;
    }
  }
}
)";
    string expect = R"(
AA(AA=1){AA=1,A=1,A={1};{A=A=A,AP;i(AEA){r;}}})";
    EXPECT_EQ(Lexer(input, err_handler).token_types_str(), expect);
  }
  {
    Parser parser("float i;", err_handler);
    parser.insert_after(Token(parser, 0), "A ");
    parser.insert_after(Token(parser, 0), "B  ");
    EXPECT_EQ(parser.result_get(), "float A B  i;");
  }
  {
    string input = R"(
A
#line 100
B
)";
    Parser parser(input, err_handler);
    string expect = R"(
A#A1A)";
    EXPECT_EQ(parser.token_types_str(), expect);

    Token A = Token(parser, 1);
    Token B = Token(parser, 5);

    EXPECT_EQ(A.str(), "A");
    EXPECT_EQ(B.str(), "B");
    EXPECT_EQ(A.line_number(), 2);
    EXPECT_EQ(B.line_number(), 100);
  }
  {
    string input = R"(
const bool foo;
[[a]] int bar[0];
)";

    string expect = R"(
match(, const, bool, , foo, , ;)
match([a], , int, , bar, [0], ;)
)";

    Parser parser(input, err_handler);

    string result = "\n";
    parser().foreach_declaration([&](Scope attributes,
                                     Token const_tok,
                                     Token type,
                                     Scope template_scope,
                                     Token name,
                                     Scope array,
                                     Token decl_end) {
      result += "match(";
      result += string(attributes.str()) + ", ";
      result += string(const_tok.str()) + ", ";
      result += string(type.str()) + ", ";
      result += string(template_scope.str()) + ", ";
      result += string(name.str()) + ", ";
      result += string(array.str()) + ", ";
      result += string(decl_end.str()) + ")\n";
    });

    EXPECT_EQ(expect, result);
  }
}

TEST(shader_tool, Cleanup)
{
  {
    string input = R"(
#line 2
int b = 0;

#if 0

int a = 1;
#elif 1
#line 321
#line 321
int a = 0;
#endif
)";
    string expect = R"(
int b = 0;

#if 0
#elif 1
#line 321
int a = 0;
#endif
)";
    string error;
    string output = process_test_string(input, error);
    EXPECT_EQ(output, expect);
    EXPECT_EQ(error, "");
  }
}

}  // namespace blender::gpu::tests

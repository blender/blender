/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "shader_tool_testing.hh"

namespace blender::gpu::tests {

using namespace shader;
using namespace std;

struct TestParser {
  using Parser = SourceProcessor::Parser;
  using ErrorHandler = SourceProcessor::ErrorHandler;

  string source;
  string result;
  ErrorHandler err_handler;
  Parser parser;

  TestParser(const string &src) : source(src), parser(err_handler)
  {
    parser.language = Language::BSL;
    try {
      parser.set_str(source);
    }
    catch (...) {
      EXPECT_TRUE(err_handler.err.has_value());
    }
  }

  string error() const
  {
    return err_handler.err.has_value() ? err_handler.err.value().message : "";
  }

  parser::ast::LocalScope root() const
  {
    return parser.root();
  }
};

TEST(bsl_grammar, EmptyTranslationUnit)
{
  TestParser parser("   \n\n   ");
  EXPECT_EQ(parser.error(), "");

  EXPECT_TRUE(parser.root().is_valid());
}

TEST(bsl_grammar, PreprocessorDirectives)
{
  TestParser parser(
      "#define MY_MACRO 1\n"
      "#ifndef TEST\n"
      "#define TEST_VAL 2.0\n"
      "#endif\n");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, NamespaceDeclarations)
{
  TestParser parser(
      "namespace blender::gpu::shader {\n"
      "  struct InnerStruct {};\n"
      "}");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, UsingStatements)
{
  TestParser parser(
      "using namespace blender::gpu;\n"
      "using MyType = other::Namespace::Type;");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, StandardStructDeclaration)
{
  TestParser parser(
      "struct MyStruct {\n"
      "  int field_a;\n"
      "  float field_b;\n"
      "};");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, StructWithAccessSpecifiers)
{
  TestParser parser(
      "class MyClass {\n"
      " public:\n"
      "  int public_var;\n"
      " private:\n"
      "  float private_var;\n"
      "};");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, MemberKeywordsAndAttributes)
{
  // Verifies C++ / explicit host-shared patterns allowed by the grammar
  TestParser parser(
      "struct HostShared {\n"
      "  static const int constant_val = 42;\n"
      "  constexpr float pi = 3.14159;\n"
      "};");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, PipelineDeclarations)
{
  TestParser parser(
      "PipelineGraphic MyGraphicsPipeline(int sub_pass, float scale);\n"
      "PipelineCompute MyComputePipeline(int work_groups);");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, TemplateDeclarationsAndInstantiations)
{
  TestParser parser(
      "template <typename T, int N>\n"
      "struct BufferArray {\n"
      "  T data[N];\n"
      "};\n"
      "template struct BufferArray<float, 4>;");  // Explicit instantiation
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, FunctionDeclarationsAndDefinitions)
{
  TestParser parser(
      "void compute_data(const int input, float &output) {\n"
      "  output = float(input) * 2.0;\n"
      "}");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, ConditionalStatements)
{
  TestParser parser(
      "void check_value(int x) {\n"
      "  if constexpr (true) {\n"
      "    x += 1;\n"
      "  } else if (x == 0) {\n"
      "    x = -1;\n"
      "  } else {\n"
      "    x = 0;\n"
      "  }\n"
      "}");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, LoopConstructs)
{
  TestParser parser(
      "void loops() {\n"
      "  for (int i = 0; i < 10; i++) { break; }\n"
      "  while (false) { continue; }\n"
      "  do { int x = 0; } while (false);\n"
      "}");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, SwitchCaseParsing)
{
  TestParser parser(
      "void handle_switch(int val) {\n"
      "  switch(val) {\n"
      "    case 1:\n"
      "      return;\n"
      "    case -2:\n"
      "      break;\n"
      "    default:\n"
      "      break;\n"
      "  }\n"
      "}");
  EXPECT_EQ(parser.error(), "");
}

TEST(bsl_grammar, Attributes)
{
  TestParser parser("[[nodiscard]] int structural_func();");
  EXPECT_EQ(parser.error(), "");

  TestParser unsupported("[[vendor::attribute(1, 2)]] int structural_func();");
  EXPECT_EQ(unsupported.error(), "Unexpected token \":\", expected attribute");
}

TEST(bsl_grammar, Enum)
{
  {
    TestParser parser(
        "enum class Color : int {\n"
        "  Red = 0,\n"
        "  Green = 1,\n"
        "  Blue\n"
        "};");
    EXPECT_EQ(parser.error(), "");
  }
  {
    TestParser parser(
        "enum InvalidEnum {\n"
        "  ValueA\n"
        "};");
    EXPECT_EQ(parser.error(), "enum declaration must explicitly use an underlying type");
  }
}

TEST(bsl_grammar, ForwardClassDeclarations)
{
  TestParser parser("struct ForwardDeclared;");
  EXPECT_EQ(parser.error(), "Forward declaration of classes is not supported");
}

TEST(bsl_grammar, UnexpectedTokens)
{
  TestParser parser("namespace A { @invalid_token; }");
  EXPECT_EQ(parser.error(), "Unexpected token \"@\": Expecting declaration");
}

}  // namespace blender::gpu::tests

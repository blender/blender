/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "ast.hh"
#include "token.hh"

#include <sstream>

namespace bsl {

using Token = blender::gpu::shader::parser::Token;

using namespace blender::gpu::shader::parser;
using namespace std;

/* TODO using fmt directly.
 * Using std::format is not possible on macos because of our minimum target. */
namespace mini_fmt {

namespace detail {
// Convert any streamable argument into a std::string
template<typename T> std::string to_string_arg(T &&arg)
{
  std::ostringstream ss;
  ss << std::forward<T>(arg);
  return ss.str();
}

// Expand variadic arguments into a vector of strings
template<typename... Args> std::vector<std::string> args_to_strings(Args &&...args)
{
  return {to_string_arg(std::forward<Args>(args))...};
}
}  // namespace detail

template<typename... Args> std::string format(const std::string &fmt, Args &&...args)
{
  std::vector<std::string> str_args = detail::args_to_strings(std::forward<Args>(args)...);
  std::string result;
  result.reserve(fmt.size() + 16 * sizeof...(args));

  size_t auto_idx = 0;
  size_t i = 0;
  size_t len = fmt.size();

  while (i < len) {
    if (fmt[i] == '{' && i + 1 < len) {
      // Case 1: Sequential placeholder "{}"
      if (fmt[i + 1] == '}') {
        if (auto_idx < str_args.size()) {
          result += str_args[auto_idx++];
        }
        i += 2;
        continue;
      }

      // Case 2: Positional placeholder "{N}"
      size_t end_pos = fmt.find('}', i + 1);
      if (end_pos != std::string::npos) {
        std::string num_str = fmt.substr(i + 1, end_pos - (i + 1));

        // Parse integer index manually to avoid std::stoi dependency
        bool valid = !num_str.empty();
        size_t idx = 0;
        for (char c : num_str) {
          if (c < '0' || c > '9') {
            valid = false;
            break;
          }
          idx = idx * 10 + (c - '0');
        }

        if (valid && idx < str_args.size()) {
          result += str_args[idx];
          i = end_pos + 1;
          continue;
        }
      }
    }

    result += fmt[i++];
  }

  return result;
}

}  // namespace mini_fmt

enum class Diag {
  CompilerErrorADLOnTypesNotAllowed,
  CompilerErrorMemberFuncNoClass,
  CompilerErrorChildScopeNotFound,

  AnonymousUnionNotSupportedAtNamespaceScope,

  ArrayMultidimensionalImplicitSize,
  ArrayMultidimensionalNotSupported,
  ArrayNeedsSizeOrInitializer,
  ArraySizeMustBeGreaterThanZero,
  ArraySizeNotConstantExpression,
  ArrayTypeOperand,

  AttributeOnlyEntryPointsCanUseIn,
  AttributeOnlyEntryPointsCanUseOut,
  AttributeNotAllowedInComp,

  AutoTypeMultipleExpressions,
  AutoTypeNestedInitializer,
  AutoTypeCannotBeDeduced,

  ConstexprDivisionByZero,
  ConstexprGlobalNonStatic,
  ConstexprIfConditionNotConstexpr,
  ConstexprMemberNonStatic,
  ConstexprShiftNegative,
  ConstexprShiftTooLarge,
  ConstexprVarMustBeInitializedByConstantExpr,
  ConstexprVarMustBeIntOrUint,
  ConstexprVarMustNotBeArray,

  ResourceTableDeclarationAlreadyOfType,

  EmptyClassNotSupportedInBuffer,
  EntryPointVoidReturn,

  ExpectedArrayMemberInitializer,
  ExpectedColon,
  ExpectedEntryPointResourceAttribute,
  ExpectedImageFormat,
  ExpectedNParametersForAttribute,
  ExpectedOneTypename,
  ExpectedParenthesis,
  ExpectedRangeParametersForAttribute,
  ExpectedScalarInitializer,

  ExpectedResourceDefinitionOfType,
  ExpectedResourceOfEitherType,
  ExpectedResourceOfType,
  ExpectedResourceTableInitializer,

  ExpectedComputeShaderEntryPoint,
  ExpectedFragmentShaderEntryPoint,
  ExpectedVertexShaderEntryPoint,

  ExprCannotContainFunctionCalls,
  ExprNotIntegralConstant,

  FieldDesignatorNotFound,
  FieldDesignatorOutOfOrder,

  FunctionCallNotAllowedInReferenceDefinition,

  ImplicitPadding12BytesArrayElement,
  ImplicitPaddingArrayElementStd140,
  ImplicitPaddingAtEndOfStruct,
  ImplicitPaddingBeforeMember,

  InitializerListRequiresExplicitType,

  InvalidBinaryOperands,
  InvalidExprTypeChecker,
  InvalidInlinedFunctionLocation,
  InvalidNumberLiteral,
  InvalidResourceTypeForResourceTableClass,
  InvalidSubscript,
  InvalidUnaryArgumentType,

  MissingParameterForCall,

  MultipleConditionAttributes,

  OperatorCalledIsNotFunction,
  OperatorTokenInvalid,
  OperatorInvalidTernary,

  OverloadAmbiguous,
  OverloadNotFound,

  NonConstVariableInExpr,

  NoteDeclarationUnionRequested,
  NoteInDeclarationOfVariable,
  NoteInInliningOf,
  NoteInStorageBufferDeclaration,
  NoteInUniformBufferDeclaration,
  NoteInstantiationClassTemplateRequested,
  NoteInstantiationFuncTemplateRequested,
  NotePreviousDefinition,

  ParserTrailingInput,

  InliningRecursive,

  Redefinition,
  RedefinitionOfEntryPointFunction,
  RedefinitionTemplate,

  ReferenceCannotBindNonConstSubscript,
  ReferenceCannotBindSideEffect,
  ReferenceCannotBindTemporary,
  ReferenceMixedVarDecl,
  ReferenceTypeMismatch,
  ReferenceVariableRequiresInitializer,

  ResourceAttributesOnlyOnEntryPointArgs,
  ResourceTableConstructorNotAllowed,
  ResourceTableMustBeReference,
  ResourcesNotAllowedInAnonymousStruct,
  ResourceOutOfClassDeclaration,

  ReturnOutsideFunction,

  StaticBranchConstexprIfNotAllowed,
  StaticBranchMissingPrevious,
  StaticBranchRequiredByPrevious,
  StaticDataMemberNotAllowed,

  StructuredBindingNotEnoughNames,
  StructuredBindingTooManyNames,

  SubscriptNotInt,

  TemplateInvalidArg,
  TemplateMissingArgument,
  TemplateMissingExplicitArguments,
  TemplateMissingParentClassInstantiation,
  TemplateMissingInstantiation,
  TemplateMissingDefinition,
  TemplateMissingParameters,
  TemplateMissingParametersForType,
  TemplateParameterCountMismatch,
  TemplateParameterNotConstexpr,

  TypeNotAllowedInStorageBuffer,
  TypeNotAllowedInUniformAndStorageBuffer,
  TypeNotAllowedInUniformBufferImplicitPadding,

  TypeNotConvertibleToBool,

  UnionArrayUnsupported,

  UnknownAttribute,
  UnknownClassInstantiation,
  UnknownFunction,
  UnknownIdentifier,
  UnknownMember,
  UnknownTypeName,
  UnknownTypeNameWithArg,
  UnknownVariable,

  UnrolledLoopCommaOperator,
  UnrolledLoopInvalidExpression,
  UnrolledLoopMissingAssignment,
  UnrolledLoopMissingInit,
  UnrolledLoopMultipleVariables,
  UnrolledLoopMustAssignToVar,
  UnrolledLoopNonIntVar,
  UnrolledLoopNotConstexpr,
  UnrolledLoopTooManyIterations,

  UnsupportedSymbolInExpr,

  UsingNamespaceNotSupported,
};

static inline std::string_view diagnostic_message_get(Diag diag)
{
  switch (diag) {
    case Diag::AnonymousUnionNotSupportedAtNamespaceScope:
      return "Anonymous unions at namespace or global scope are not supported";
    case Diag::ArrayMultidimensionalImplicitSize:
      return "Multi-dimensional arrays cannot have implicit size";
    case Diag::ArrayMultidimensionalNotSupported:
      return "Multi-dimensional arrays are not supported";
    case Diag::ArrayNeedsSizeOrInitializer:
      return "Definition of variable with array type needs an explicit size or an initializer";
    case Diag::ArraySizeMustBeGreaterThanZero:
      return "Array size must be greater than zero";
    case Diag::ArraySizeNotConstantExpression:
      return "Array size is not a constant expression";
    case Diag::ArrayTypeOperand:
      return "Invalid operand of array type";
    case Diag::AttributeNotAllowedInComp:
      return "'{}' attribute is only allowed in vertex and fragment shaders";
    case Diag::AttributeOnlyEntryPointsCanUseIn:
      return "Only entry points can use 'in' attribute";
    case Diag::AttributeOnlyEntryPointsCanUseOut:
      return "Only entry points can use 'out' attribute";
    case Diag::AutoTypeCannotBeDeduced:
      return "Cannot deduce actual type for variable '{}' with type 'auto'";
    case Diag::AutoTypeMultipleExpressions:
      return "Initializer for variable '{}' with type 'auto' contains multiple expressions";
    case Diag::AutoTypeNestedInitializer:
      return "Cannot deduce type for variable '{}' with type 'auto' from nested initializer list";
    case Diag::CompilerErrorADLOnTypesNotAllowed:
      return "Cannot use ADL on types";
    case Diag::CompilerErrorMemberFuncNoClass:
      return "Compiler error: member function has no class";
    case Diag::CompilerErrorChildScopeNotFound:
      return "Compiler error: non-existing child scope";
    case Diag::ConstexprDivisionByZero:
      return "Division by zero during constexpr evaluation";
    case Diag::ConstexprGlobalNonStatic:
      return "Non-static global variable cannot be constexpr; did you intend to make it static?";
    case Diag::ConstexprIfConditionNotConstexpr:
      return "Constexpr if condition is not a constant expression";
    case Diag::ConstexprMemberNonStatic:
      return "Non-static data member cannot be constexpr; did you intend to make it static?";
    case Diag::ConstexprShiftNegative:
      return "Negative shift count {}";
    case Diag::ConstexprShiftTooLarge:
      return "Shift count {} >= width of type 32";
    case Diag::ConstexprVarMustBeInitializedByConstantExpr:
      return "Constexpr variable '{}' must be initialized by a constant expression";
    case Diag::ConstexprVarMustBeIntOrUint:
      return "Constexpr variable must be of type 'int', 'uint', 'bool' or 'float'";
    case Diag::ConstexprVarMustNotBeArray:
      return "Array variables cannot be constexpr";
    case Diag::EmptyClassNotSupportedInBuffer:
      return "Empty classes are not supported in storage or uniform buffers";
    case Diag::EntryPointVoidReturn:
      return "Entry point functions must return 'void'";
    case Diag::ExpectedArrayMemberInitializer:
      return "Expected array member initializer but got designated initializer";
    case Diag::ExpectedColon:
      return "Expected colon ':'";
    case Diag::ExpectedComputeShaderEntryPoint:
      return "Expected compute shader entry point function";
    case Diag::ExpectedEntryPointResourceAttribute:
      return "Expected entry point resource attribute but got '{}'";
    case Diag::ExpectedFragmentShaderEntryPoint:
      return "Expected fragment shader entry point function";
    case Diag::ExpectedImageFormat:
      return "Expected image format";
    case Diag::ExpectedNParametersForAttribute:
      return "Expected {} parameter(s) for attribute '{}'";
    case Diag::ExpectedOneTypename:
      return "Expecting one typename";
    case Diag::ExpectedParenthesis:
      return "Expected closing parenthesis";
    case Diag::ExpectedRangeParametersForAttribute:
      return "Expected between 1 and {} parameter(s) for attribute '{}'";
    case Diag::ExpectedResourceDefinitionOfType:
      return "Expected resource definition of type '{}' but got '{}'";
    case Diag::ExpectedResourceOfEitherType:
      return "Expected resource of type '{}' or '{}' but got '{}'";
    case Diag::ExpectedResourceOfType:
      return "Expected resource of type '{}' but got '{}'";
    case Diag::ExpectedResourceTableInitializer:
      return "Expected resource table initializer";
    case Diag::ExpectedScalarInitializer:
      return "Excess elements in scalar initializer";
    case Diag::ExpectedVertexShaderEntryPoint:
      return "Expected vertex shader entry point function";
    case Diag::ExprCannotContainFunctionCalls:
      return "{} cannot contain function calls";
    case Diag::ExprNotIntegralConstant:
      return "Expression is not an integral constant expression";
    case Diag::FieldDesignatorNotFound:
      return "Field designator '{}' does not refer to any field in type '{}'";
    case Diag::FieldDesignatorOutOfOrder:
      return "Field designator '{}' needs to be specified in declaration order";
    case Diag::FunctionCallNotAllowedInReferenceDefinition:
      return "Function calls are not allowed in reference definition";
    case Diag::ImplicitPadding12BytesArrayElement:
      return "Implicit padding of 4 bytes per array element, do not use 12 bytes types in arrays";
    case Diag::ImplicitPaddingArrayElementStd140:
      return "Implicit padding of {} bytes per array element, use a 16 bytes type or use a "
             "storage buffer";
    case Diag::ImplicitPaddingAtEndOfStruct:
      return "Implicit {} bytes padding at the end of '{}', add manual padding";
    case Diag::ImplicitPaddingBeforeMember:
      return "Implicit {} bytes padding before '{}', add manual padding";
    case Diag::InitializerListRequiresExplicitType:
      return "Initializer list requires explicit type";
    case Diag::InliningRecursive:
      return "Cannot inline recursive call";
    case Diag::InvalidBinaryOperands:
      return "Invalid operands to binary expression ('{}' {} '{}')";
    case Diag::InvalidExprTypeChecker:
      return "Invalid expression in type checker";
    case Diag::InvalidInlinedFunctionLocation:
      return "Cannot inline function call inside a compound expression";
    case Diag::InvalidNumberLiteral:
      return "Unsupported literal";
    case Diag::InvalidResourceTypeForResourceTableClass:
      return "Invalid resource of type '{}' for resource table class";
    case Diag::InvalidSubscript:
      return "Subscripted value is not an array, pointer, or vector";
    case Diag::InvalidUnaryArgumentType:
      return "Invalid argument type '{0}' to unary expression ({0}'{1}')";
    case Diag::MissingParameterForCall:
      return "Missing parameter for call to '{}'";
    case Diag::MultipleConditionAttributes:
      return "Only one condition attribute is allowed";
    case Diag::NonConstVariableInExpr:
      return "Read of non-const variable is not allowed in a {}";
    case Diag::NoteDeclarationUnionRequested:
      return "In declaration of union '{}' requested here";
    case Diag::NoteInDeclarationOfVariable:
      return "In declaration of '{}' of type '{}'";
    case Diag::NoteInInliningOf:
      return "In inlining of function '{}' requested here";
    case Diag::NoteInStorageBufferDeclaration:
      return "In this storage buffer declaration";
    case Diag::NoteInUniformBufferDeclaration:
      return "In this uniform buffer declaration";
    case Diag::NoteInstantiationClassTemplateRequested:
      return "In instantiation of class template specialization '{}' requested here";
    case Diag::NoteInstantiationFuncTemplateRequested:
      return "In instantiation of function template specialization '{}' requested here";
    case Diag::NotePreviousDefinition:
      return "Previous definition of '{}' is here";
    case Diag::OperatorCalledIsNotFunction:
      return "Called object type '{}' is not a function";
    case Diag::OperatorInvalidTernary:
      return "Incompatible operand types ('{}' and '{}')";
    case Diag::OperatorTokenInvalid:
      return "Invalid operator token '{}'";
    case Diag::OverloadAmbiguous:
      return "Call to '{}' is ambiguous for parameters '{}' with candidates: {}";
    case Diag::OverloadNotFound:
      return "No matching function for call to '{}' for parameters {} with candidates: {}";
    case Diag::ParserTrailingInput:
      return "Trailing input";
    case Diag::Redefinition:
      return "Redefinition of '{}'";
    case Diag::RedefinitionOfEntryPointFunction:
      return "Redefinition of entry point function '{}'";
    case Diag::RedefinitionTemplate:
      return "Redefinition of template '{}'";
    case Diag::ReferenceCannotBindNonConstSubscript:
      return "Reference cannot bind to expression of non-const subscript index";
    case Diag::ReferenceCannotBindSideEffect:
      return "Reference cannot bind to an expression with side effects";
    case Diag::ReferenceCannotBindTemporary:
      return "Reference cannot bind to a temporary";
    case Diag::ReferenceMixedVarDecl:
      return "Variable declaration must declare either no reference or only reference";
    case Diag::ReferenceTypeMismatch:
      return "Reference to type '{}' cannot bind to a value of unrelated type '{}'";
    case Diag::ReferenceVariableRequiresInitializer:
      return "Declaration of reference variable '{}' requires an initializer";
    case Diag::ResourceAttributesOnlyOnEntryPointArgs:
      return "Resource attributes are only supported on entry point arguments";
    case Diag::ResourceOutOfClassDeclaration:
      return "Out of class resource declaration";
    case Diag::ResourceTableConstructorNotAllowed:
      return "Calling resource table constructor '{0}' is not allowed; use "
             "'resource_table_get({0})' instead";
    case Diag::ResourceTableDeclarationAlreadyOfType:
      return "Declaration is already of type '{}'";
    case Diag::ResourceTableMustBeReference:
      return "Resource table must be declared as reference";
    case Diag::ResourcesNotAllowedInAnonymousStruct:
      return "Resources cannot be declared in an anonymous struct";
    case Diag::ReturnOutsideFunction:
      return "Return statement outside of a function";
    case Diag::StaticBranchConstexprIfNotAllowed:
      return "'if' statement cannot be 'constexpr' inside a static branch";
    case Diag::StaticBranchMissingPrevious:
      return "Statement cannot be tagged as 'static_branch' because previous statement also isn't";
    case Diag::StaticBranchRequiredByPrevious:
      return "Statement must be tagged as 'static_branch' because previous statement is";
    case Diag::StaticDataMemberNotAllowed:
      return "Static data member '{}' not allowed in {} {}";
    case Diag::StructuredBindingNotEnoughNames:
      return "Not enough names in structure binding";
    case Diag::StructuredBindingTooManyNames:
      return "Too many names in structure binding";
    case Diag::SubscriptNotInt:
      return "Array subscript is not an integer";
    case Diag::TemplateInvalidArg:
      return "Invalid argument for template parameter '{}'";
    case Diag::TemplateMissingArgument:
      return "Missing argument for template parameter '{}'";
    case Diag::TemplateMissingDefinition:
      return "Missing template definition";
    case Diag::TemplateMissingExplicitArguments:
      return "Missing explicit template arguments";
    case Diag::TemplateMissingInstantiation:
      return "Missing explicit instantiation of template '{}'";
    case Diag::TemplateMissingParameters:
      return "Missing template parameters";
    case Diag::TemplateMissingParametersForType:
      return "Missing template parameters for '{}'";
    case Diag::TemplateMissingParentClassInstantiation:
      return "Missing instantiation of parent class for instantiation of '{}'";
    case Diag::TemplateParameterCountMismatch:
      return "Template parameter count must match declaration argument count";
    case Diag::TemplateParameterNotConstexpr:
      return "Non-type template argument is not a constant expression";
    case Diag::TypeNotAllowedInStorageBuffer:
      return "Type '{}' is not allowed in storage buffer{}";
    case Diag::TypeNotAllowedInUniformAndStorageBuffer:
      return "Type '{}' is not allowed in uniform and storage buffer{}";
    case Diag::TypeNotAllowedInUniformBufferImplicitPadding:
      return "Type '{}' is not allowed in uniform buffer because it contains implicit padding";
    case Diag::TypeNotConvertibleToBool:
      return "Value of type '{}' is not contextually convertible to 'bool'";
    case Diag::UnionArrayUnsupported:
      return "Unsupported array at union base scope; wrap it inside a named struct";
    case Diag::UnknownAttribute:
      return "Unknown attribute '{}'";
    case Diag::UnknownClassInstantiation:
      return "Compiler error: Can't find class symbol";
    case Diag::UnknownFunction:
      return "Unknown function name";
    case Diag::UnknownIdentifier:
      return "Use of undeclared identifier '{}'";
    case Diag::UnknownMember:
      return "Unknown member '{}'";
    case Diag::UnknownTypeName:
      return "Unknown type name";
    case Diag::UnknownTypeNameWithArg:
      return "Unknown type name '{}'";
    case Diag::UnknownVariable:
      return "Unknown variable '{}'";
    case Diag::UnrolledLoopCommaOperator:
      return "Comma operator is not allowed in unrolled loop statement";
    case Diag::UnrolledLoopInvalidExpression:
      return "Expected '++{0}', '--{0}', '{0}++', '{0}--' or '{0} = expr', '{0} += expr', '{0} -= "
             "expr', '{0} /= expr', '{0} *= expr' for unrolled loop expression";
    case Diag::UnrolledLoopMissingAssignment:
      return "Loop variable needs to be assigned a value (using assignment) for unrolled loops";
    case Diag::UnrolledLoopMissingInit:
      return "Init statement needs to define the loop variable for unrolled loops";
    case Diag::UnrolledLoopMultipleVariables:
      return "Multiple variables declared in unrolled loop";
    case Diag::UnrolledLoopMustAssignToVar:
      return "Unrolled loop expression must assign to '{}'";
    case Diag::UnrolledLoopNonIntVar:
      return "Loop variable needs to be an integer type for unrolled loops";
    case Diag::UnrolledLoopNotConstexpr:
      return "Non-constant expression in unrolled loop control";
    case Diag::UnrolledLoopTooManyIterations:
      return "Loop unrolling generates too many iterations (over 64)";
    case Diag::UnsupportedSymbolInExpr:
      return "Unsupported symbol '{}' inside {}";
    case Diag::UsingNamespaceNotSupported:
      return "Using namespace statements are not supported";
  }
  return "Compiler error: Invalid diagnostic value";
}

struct AstNodeException {
  ast::Node node;
  Diag diag;
  string param1;
  string param2;
  string param3;

  AstNodeException(ast::Node node,
                   Diag diag,
                   const string &param1 = "",
                   const string &param2 = "",
                   const string &param3 = "")
      : node(node), diag(diag), param1(param1), param2(param2), param3(param3)
  {
  }
};

struct TokenException {
  Token tok;
  Diag diag;
  string param1;
  string param2;
  string param3;

  TokenException(Token tok,
                 Diag diag,
                 const string &param1 = "",
                 const string &param2 = "",
                 const string &param3 = "")
      : tok(tok), diag(diag), param1(param1), param2(param2), param3(param3)
  {
  }
};

struct NodeErrorHandler {
  ErrorHandler &err_handler;

  NodeErrorHandler(ErrorHandler &handler) : err_handler(handler) {}

  void error(Token tok, const string &msg)
  {
    if (err_handler.err) {
      return;
    }
    vector<pair<Token, string>> errors;
    errors.emplace_back(tok, msg);

    for (auto it = note_stack.rbegin(); it != note_stack.rend(); ++it) {
      auto [note_node, note_msg] = *it;
      errors.emplace_back(note_node, note_msg);
    }
    err_handler.report(errors);
  }

  void error(ast::Node node, const string &msg)
  {
    error(node.front(), msg);
  }

  /* Pipe error if existing. */
  void error(std::optional<AstNodeException> &err)
  {
    if (err) {
      error(err->node, err->diag, err->param1, err->param2, err->param3);
    }
  }
  void error(std::optional<TokenException> &err)
  {
    if (err) {
      error(err->tok, err->diag, err->param1, err->param2, err->param3);
    }
  }

  template<typename... Args> void error(Token tok, Diag code, Args &&...args)
  {
    std::string_view format_str = diagnostic_message_get(code);
    std::string message = mini_fmt::format(std::string(format_str), std::forward<Args>(args)...);
    error(tok, message);
  }
  template<typename... Args> void error(ast::Node node, Diag code, Args &&...args)
  {
    error(node.front(), code, std::forward<Args>(args)...);
  }

  /* Stack of notes to add to the error. */
  vector<tuple<Token, string>> note_stack = {};

  struct ScopedNote {
    vector<tuple<Token, string>> &note_stack;

    ScopedNote(Token tok, const string &msg, vector<tuple<Token, string>> &note_stack)
        : note_stack(note_stack)
    {
      note_stack.emplace_back(tok, msg);
    }

    ~ScopedNote()
    {
      note_stack.pop_back();
    }
  };

  template<typename... Args> [[nodiscard]] ScopedNote note(Token tok, Diag code, Args &&...args)
  {
    std::string_view format_str = diagnostic_message_get(code);
    std::string message = mini_fmt::format(std::string(format_str), std::forward<Args>(args)...);
    return ScopedNote(tok, message, note_stack);
  }

  template<typename... Args>
  [[nodiscard]] ScopedNote note(ast::Node node, Diag code, Args &&...args)
  {
    std::string_view format_str = diagnostic_message_get(code);
    std::string message = mini_fmt::format(std::string(format_str), std::forward<Args>(args)...);
    return ScopedNote(node.front(), message, note_stack);
  }

#define NOTE(node, diag, id) auto n = note(node, diag, id)
};

template<typename T, typename ExceptionT = AstNodeException> struct Result {
  T value;
  std::optional<ExceptionT> err;

  T unwrap(NodeErrorHandler *handler)
  {
    handler->error(err);
    return value;
  }

  /* Transfer error if not yet set. */
  T unwrap(std::optional<AstNodeException> &other_err)
  {
    if (!other_err) {
      other_err = err;
    }
    return value;
  }
};

template<typename T, typename U, typename ExceptionT = AstNodeException> struct ResultPair {
  T first;
  U second;
  std::optional<ExceptionT> err;

  pair<T, U> unwrap(NodeErrorHandler *handler)
  {
    handler->error(err);
    return {first, second};
  }

  /* Transfer error if not yet set. */
  pair<T, U> unwrap(std::optional<AstNodeException> &other_err)
  {
    if (!other_err) {
      other_err = err;
    }
    return {first, second};
  }
};

}  // namespace bsl

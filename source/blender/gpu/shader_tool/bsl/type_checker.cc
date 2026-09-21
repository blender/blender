/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "type_checker.hh"
#include "pratt_parser.hh"
#include "symbol_table.hh"

#include <bit>

namespace bsl {

/**
 * Type resolution parser.
 * Will evaluate each operand type and return the type of the expression result.
 */
struct ExpressionTypeParser
    : public PrattParser<ExpressionTypeParser, ExpressionResult, ast::Node> {
  using Base = PrattParser<ExpressionTypeParser, ExpressionResult, ast::Node>;
  using ErrorType = Base::ErrorType;
  using BindingPower = Base::BindingPower;

 private:
  std::optional<AstNodeException> err;
  const SymbolTable *table;
  const SymbolScope *scope;
  ast::Node start;
  ast::Node node;

  /* The expression is supposed to be bound to a reference variable. */
  bool is_reference;

 public:
  ExpressionTypeParser(const SymbolTable *table,
                       const SymbolScope *scope,
                       ast::Node expr_first_child,
                       bool is_reference = false)
      : table(table),
        scope(scope),
        start(expr_first_child),
        node(expr_first_child),
        is_reference(is_reference)
  {
  }

  Result<ExpressionResult> eval()
  {
    ExpressionResult result = expr(0);

    if (peek() != Invalid && !err) {
      error(consume(), Diag::ParserTrailingInput);
    }
    return {result, err};
  }

  /**
   * CRTP Implementation
   */

  ExpressionResult identifier(const ast::Node &t)
  {
    switch (t.type()) {
      case ast::NodeType::Constructor: {
        ast::Constructor ctor = t;
        return scope->lookup_class(*table, ctor.identifier()).unwrap(err);
      }
      case ast::NodeType::FuncCall: {
        ast::FuncCall call = t;

        /* Legacy BSL. */
        if (call.identifier().str() == "resource_table_get") {
          auto [param] = call.parameters().splat_1();
          if (ast::LocalVar var = param.child_first(); var.is_valid()) {
            return scope->lookup_class(*table, var.identifier()).unwrap(err);
          }
          error(call.parameters(), Diag::ExpectedOneTypename);
          return {table->err_cls, ConstexprError(), false};
        }
        if (call.identifier().str() == "interface_get" ||
            call.identifier().str() == "sampler_get" || call.identifier().str() == "buffer_get")
        {
          auto [param1, param2] = call.parameters().splat_2();
          if (ast::LocalVar var = param1.child_first(), mem = param2.child_first();
              var.is_valid() && mem.is_valid())
          {
            SymbolClass *cls = scope->lookup_class(*table, var.identifier()).unwrap(err);
            SymbolVariable *v = cls->lookup_variable(*table, mem.identifier());
            if (v->is_error) {
              error(mem, Diag::UnknownVariable, string(mem.str()));
            }
            return v;
          }
          error(call.parameters(), Diag::ExpectedOneTypename);
          return {table->err_cls, ConstexprError(), false};
        }

        if (is_reference) {
          error(t, Diag::FunctionCallNotAllowedInReferenceDefinition);
        }
        auto sym = scope->lookup_function(*table, call.identifier(), call.parameters(), *scope);
        SymbolFunction *fn = sym.unwrap(err);
        if (fn->is_constexpr && !fn->is_error) {
          /* Implement builtin type casts. */
          auto [param1, param2, param3, param4] = call.parameters().splat_4();
          int param_count = call.parameters().child_count();
          if (param_count <= 0 || param_count > 4) {
            /* Not a constexpr. */
          }
          else if (param_count == 4) {
            ExpressionResult res1 = sub_expression(t, param1, 0);
            ExpressionResult res2 = sub_expression(t, param2, 0);
            ExpressionResult res3 = sub_expression(t, param3, 0);
            ExpressionResult res4 = sub_expression(t, param4, 0);

            if (!res1.is_constexpr() || res1.array_dim != 0 || !res2.is_constexpr() ||
                res2.array_dim != 0 || !res3.is_constexpr() || res3.array_dim != 0 ||
                !res4.is_constexpr() || res4.array_dim != 0)
            {
              /* Not a constexpr. */
            }
            else if (fn->identifier == "float4") {
              return {table->float4_cls,
                      ConstexprValue(
                          ConstexprValue::FLOAT4, res1.value, res2.value, res3.value, res4.value)};
            }
            else if (fn->identifier == "int4") {
              return {table->int4_cls,
                      ConstexprValue(
                          ConstexprValue::INT4, res1.value, res2.value, res3.value, res4.value)};
            }
            else if (fn->identifier == "uint4") {
              return {table->uint4_cls,
                      ConstexprValue(
                          ConstexprValue::UINT4, res1.value, res2.value, res3.value, res4.value)};
            }
            else if (fn->identifier == "bool4") {
              return {table->bool4_cls,
                      ConstexprValue(
                          ConstexprValue::BOOL4, res1.value, res2.value, res3.value, res4.value)};
            }
          }
          else if (param_count == 3) {
            ExpressionResult res1 = sub_expression(t, param1, 0);
            ExpressionResult res2 = sub_expression(t, param2, 0);
            ExpressionResult res3 = sub_expression(t, param3, 0);

            if (!res1.is_constexpr() || res1.array_dim != 0 || !res2.is_constexpr() ||
                res2.array_dim != 0 || !res3.is_constexpr() || res3.array_dim != 0)
            {
              /* Not a constexpr. */
            }
            else if (fn->identifier == "float3") {
              return {table->float3_cls,
                      ConstexprValue(ConstexprValue::FLOAT3, res1.value, res2.value, res3.value)};
            }
            else if (fn->identifier == "int3") {
              return {table->int3_cls,
                      ConstexprValue(ConstexprValue::INT3, res1.value, res2.value, res3.value)};
            }
            else if (fn->identifier == "uint3") {
              return {table->uint3_cls,
                      ConstexprValue(ConstexprValue::UINT3, res1.value, res2.value, res3.value)};
            }
            else if (fn->identifier == "bool3") {
              return {table->bool3_cls,
                      ConstexprValue(ConstexprValue::BOOL3, res1.value, res2.value, res3.value)};
            }
          }
          else if (param_count == 2) {
            ExpressionResult res1 = sub_expression(t, param1, 0);
            ExpressionResult res2 = sub_expression(t, param2, 0);

            if (!res1.is_constexpr() || res1.array_dim != 0 || !res2.is_constexpr() ||
                res2.array_dim != 0)
            {
              /* Not a constexpr. */
            }
            else if (fn->identifier == "float2") {
              return {table->float2_cls,
                      ConstexprValue(ConstexprValue::FLOAT2, res1.value, res2.value)};
            }
            else if (fn->identifier == "int2") {
              return {table->int2_cls,
                      ConstexprValue(ConstexprValue::INT2, res1.value, res2.value)};
            }
            else if (fn->identifier == "uint2") {
              return {table->uint2_cls,
                      ConstexprValue(ConstexprValue::UINT2, res1.value, res2.value)};
            }
            else if (fn->identifier == "bool2") {
              return {table->bool2_cls,
                      ConstexprValue(ConstexprValue::BOOL2, res1.value, res2.value)};
            }
            else if (res1.type->comp_len > 1 && res1.type->comp_len == res2.type->comp_len) {
              if (fn->identifier == "lessThan") {
                return {table->make_type(table->bool_cls, res1.type->comp_len),
                        res1.value < res2.value};
              }
              if (fn->identifier == "lessThanEqual") {
                return {table->make_type(table->bool_cls, res1.type->comp_len),
                        res1.value <= res2.value};
              }
              if (fn->identifier == "greaterThan") {
                return {table->make_type(table->bool_cls, res1.type->comp_len),
                        res1.value > res2.value};
              }
              if (fn->identifier == "greaterThanEqual") {
                return {table->make_type(table->bool_cls, res1.type->comp_len),
                        res1.value >= res2.value};
              }
              if (fn->identifier == "equal") {
                return {table->make_type(table->bool_cls, res1.type->comp_len),
                        res1.value == res2.value};
              }
              if (fn->identifier == "notEqual") {
                return {table->make_type(table->bool_cls, res1.type->comp_len),
                        res1.value != res2.value};
              }
            }
          }
          else if (param_count == 1) {
            ExpressionResult res = sub_expression(t, param1, 0);
            if (!res.is_constexpr() || res.array_dim != 0) {
              /* Not a constexpr. */
            }
            else if (res.type == table->float4_cls || res.type == table->int4_cls ||
                     res.type == table->bool4_cls || res.type == table->uint4_cls)
            {
              if (fn->identifier == "float4") {
                return {table->float4_cls, ConstexprValue(ConstexprValue::FLOAT4, res.value)};
              }
              if (fn->identifier == "int4") {
                return {table->int4_cls, ConstexprValue(ConstexprValue::INT4, res.value)};
              }
              if (fn->identifier == "uint4") {
                return {table->uint4_cls, ConstexprValue(ConstexprValue::UINT4, res.value)};
              }
              if (fn->identifier == "bool4") {
                return {table->bool4_cls, ConstexprValue(ConstexprValue::BOOL4, res.value)};
              }
            }
            else if (res.type == table->float3_cls || res.type == table->int3_cls ||
                     res.type == table->bool3_cls || res.type == table->uint3_cls)
            {
              if (fn->identifier == "float3") {
                return {table->float3_cls, ConstexprValue(ConstexprValue::FLOAT3, res.value)};
              }
              if (fn->identifier == "int3") {
                return {table->int3_cls, ConstexprValue(ConstexprValue::INT3, res.value)};
              }
              if (fn->identifier == "uint3") {
                return {table->uint3_cls, ConstexprValue(ConstexprValue::UINT3, res.value)};
              }
              if (fn->identifier == "bool3") {
                return {table->bool3_cls, ConstexprValue(ConstexprValue::BOOL3, res.value)};
              }
            }
            else if (res.type == table->float2_cls || res.type == table->int2_cls ||
                     res.type == table->bool2_cls || res.type == table->uint2_cls)
            {
              if (fn->identifier == "float2") {
                return {table->float2_cls, ConstexprValue(ConstexprValue::FLOAT2, res.value)};
              }
              if (fn->identifier == "int2") {
                return {table->int2_cls, ConstexprValue(ConstexprValue::INT2, res.value)};
              }
              if (fn->identifier == "uint2") {
                return {table->uint2_cls, ConstexprValue(ConstexprValue::UINT2, res.value)};
              }
              if (fn->identifier == "bool2") {
                return {table->bool2_cls, ConstexprValue(ConstexprValue::BOOL2, res.value)};
              }
            }
            else if (res.type == table->float_cls || res.type == table->int_cls ||
                     res.type == table->bool_cls || res.type == table->uint_cls)
            {
              if (fn->identifier == "float4") {
                return {table->float4_cls, ConstexprValue(ConstexprValue::FLOAT4, res.value)};
              }
              if (fn->identifier == "int4") {
                return {table->int4_cls, ConstexprValue(ConstexprValue::INT4, res.value)};
              }
              if (fn->identifier == "uint4") {
                return {table->uint4_cls, ConstexprValue(ConstexprValue::UINT4, res.value)};
              }
              if (fn->identifier == "bool4") {
                return {table->bool4_cls, ConstexprValue(ConstexprValue::BOOL4, res.value)};
              }
              if (fn->identifier == "float3") {
                return {table->float3_cls, ConstexprValue(ConstexprValue::FLOAT3, res.value)};
              }
              if (fn->identifier == "int3") {
                return {table->int3_cls, ConstexprValue(ConstexprValue::INT3, res.value)};
              }
              if (fn->identifier == "uint3") {
                return {table->uint3_cls, ConstexprValue(ConstexprValue::UINT3, res.value)};
              }
              if (fn->identifier == "bool3") {
                return {table->bool3_cls, ConstexprValue(ConstexprValue::BOOL3, res.value)};
              }
              if (fn->identifier == "float2") {
                return {table->float2_cls, ConstexprValue(ConstexprValue::FLOAT2, res.value)};
              }
              if (fn->identifier == "int2") {
                return {table->int2_cls, ConstexprValue(ConstexprValue::INT2, res.value)};
              }
              if (fn->identifier == "uint2") {
                return {table->uint2_cls, ConstexprValue(ConstexprValue::UINT2, res.value)};
              }
              if (fn->identifier == "bool2") {
                return {table->bool2_cls, ConstexprValue(ConstexprValue::BOOL2, res.value)};
              }
              if (fn->identifier == "float") {
                return {table->float_cls, ConstexprValue(ConstexprValue::FLOAT1, res.value)};
              }
              if (fn->identifier == "int") {
                return {table->int_cls, ConstexprValue(ConstexprValue::INT1, res.value)};
              }
              if (fn->identifier == "uint") {
                return {table->uint_cls, ConstexprValue(ConstexprValue::UINT1, res.value)};
              }
              if (fn->identifier == "bool") {
                return {table->bool_cls, ConstexprValue(ConstexprValue::BOOL1, res.value)};
              }
              if (fn->identifier == "floatBitsToUint") {
                return {table->uint_cls,
                        ConstexprValue(std::bit_cast<uint32_t>(res.value.comp_cast<float>(0)))};
              }
              if (fn->identifier == "floatBitsToInt") {
                return {table->int_cls,
                        ConstexprValue(std::bit_cast<int32_t>(res.value.comp_cast<float>(0)))};
              }
              if (fn->identifier == "uintBitsToFloat") {
                return {table->uint_cls,
                        ConstexprValue(std::bit_cast<float>(res.value.comp_cast<uint32_t>(0)))};
              }
              if (fn->identifier == "intBitsToFloat") {
                return {table->float_cls,
                        ConstexprValue(std::bit_cast<float>(res.value.comp_cast<int32_t>(0)))};
              }
            }
          }
        }
        return fn->return_type;
      }
      case ast::NodeType::LocalVar: {
        ast::LocalVar var = t;
        SymbolVariable *sym = scope->lookup_variable(*table, var.identifier());
        if (sym) {
          return sym;
        }
        error(t, Diag::UnknownMember, std::string(var.identifier().str()));
        return table->err_cls;
      }
      default:
        error(t, Diag::InvalidExprTypeChecker);
        return table->err_cls;
    }
  }

  ExpressionResult number_literal(const ast::Node &t)
  {
    SymbolClass *type = table->get_literal_type(t.str());

    ConstexprValue value;
    try {
      std::string str_val(t.str());

      if (type == table->float_cls) {
        value = ConstexprValue(std::stof(str_val));
      }
      else if (type == table->int_cls) {
        value = ConstexprValue(std::stoi(str_val, nullptr, 0));
      }
      else if (type == table->uint_cls) {
        value = ConstexprValue(uint32_t(std::stoull(str_val, nullptr, 0)));
      }
      else if (type == table->bool_cls) {
        value = ConstexprValue(str_val == "true");
      }
      else {
        throw std::runtime_error("Invalid Type");
      }
    }
    catch (...) {
      error(t, Diag::InvalidNumberLiteral);
      return table->err_cls;
    }
    return {type, value, /* is_temporary */ false};
  }

  ExpressionResult string_literal(const ast::Node & /*t*/)
  {
    return table->str_cls;
  }

  ExpressionResult parenthesis(const ast::Node &t, BindingPower p)
  {
    return sub_expression(t, ast::ExprSub(t).expr(), p);
  }

  ExpressionResult member(ExpressionResult left, const ast::Node &t)
  {
    SymbolClass *l = left.type;
    ast::Node right = consume();
    switch (right.type()) {
      case ast::NodeType::FuncCall: {
        ast::FuncCall call = right;
        /* The exception is for legacy BSL. */
        if (is_reference && call.identifier().str() != "resource_table_get" &&
            call.identifier().str() != "interface_get" &&
            call.identifier().str() != "sampler_get" && call.identifier().str() != "buffer_get")
        {
          error(t, Diag::FunctionCallNotAllowedInReferenceDefinition);
        }
        auto sym = l->lookup_function(*table, call.identifier(), call.parameters(), *scope);
        return sym.unwrap(err)->return_type;
      }
      case ast::NodeType::LocalVar: {
        ast::LocalVar var = right;
        SymbolVariable *sym = l->lookup_variable(*table, var.identifier());
        if (sym) {
          return sym;
        }
        error(t, Diag::UnknownMember, std::string(var.identifier().str()));
        return table->err_cls;
      }
      default:
        error(t, Diag::InvalidExprTypeChecker);
        this->node = ast::Node{};
        return table->err_cls;
    }
  }

  ExpressionResult function_call(ExpressionResult left, const ast::Node &t)
  {
    if (is_reference) {
      error(t, Diag::FunctionCallNotAllowedInReferenceDefinition);
    }
    error(t, Diag::OperatorCalledIsNotFunction, left.type->identifier);
    return table->err_cls;
  }

  ExpressionResult subscript(ExpressionResult left, const ast::Node &t)
  {
    /* Subscript operator. */
    if (ast::Subscript sub = t; sub.is_valid()) {
      if (left.array_dim > 0) {
        ExpressionResult arg = subscript_parameter(sub);

        if (table->get_conversion_rank(arg.type, table->int_cls) < MatchRank::Promotion) {
          error(t, Diag::SubscriptNotInt);
        }
        if (is_reference && !(arg.flags & ExprFlag::IsConstant)) {
          error(t, Diag::ReferenceCannotBindNonConstSubscript);
        }

        left.array_dim -= 1;
        return left;
      }
      /* Call subscript operator */
      if (left.type->operator_subscript) {
        ExpressionResult arg = subscript_parameter(sub);
        if (table->get_conversion_rank(arg.type, table->int_cls) < MatchRank::Promotion) {
          error(t, Diag::SubscriptNotInt);
        }
        return left.type->operator_subscript->return_type;
      }
    }
    error(t, Diag::InvalidSubscript);
    return table->err_cls;
  }

  ExpressionResult subscript_parameter(ast::Subscript &sub)
  {
    return sub_expression(sub, sub.expr(), 0);
  }

  ExpressionResult prefix(const ast::Node &t, BindingPower p)
  {
    using namespace builtin;
    ExpressionResult right = expr(p);

    if (right.array_dim > 0) {
      error(node, Diag::ArrayTypeOperand);
      return table->err_cls;
    }

    TokenType op_type = t.front().type();

    ClassId type_id = unary_prefix_operator_return_type(op_type, right.type);
    if (type_id == builtin::Invalid) {
      error(t, Diag::InvalidUnaryArgumentType, to_str(op_type), right.type->identifier);
      return table->err_cls;
    }

    SymbolClass *type = table->to_class(type_id);

    if (is_reference && (op_type == Increment || op_type == Decrement)) {
      error(t, Diag::ReferenceCannotBindSideEffect);
      return table->err_cls;
    }

#ifdef _MSC_VER
/* Silence warning about bool operand.
 * They cannot happen because they are caught by the above check */
#  pragma warning(push)
#  pragma warning(disable : 4805)
#endif

    ExprFlag flags = ExprFlag(right.flags);
    if (flags & ExprFlag::IsConstexpr) {
      switch (op_type) {
        /* Note that side effects are not propagated. */
        case Increment:
          return {type, ++right.value};
        case Decrement:
          return {type, --right.value};
        case Plus:
          return {type, +right.value};
        case Minus:
          return {type, -right.value};
        case Not:
          /* Note that '!' token is of MultiTok class and can contain many unary '!'. */
          return {type, (t.str().size() & 1) ? !right.value : !!right.value};
        case BitwiseNot:
          return {type, ~right.value};
        default:
          assert(0); /* Unreachable. */
          return table->err_cls;
      }
    }
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
    return {type, flags};
  }

  ExpressionResult suffix(ExpressionResult left, const ast::Node &t)
  {
    using namespace builtin;
    if (left.array_dim > 0) {
      error(node, Diag::ArrayTypeOperand);
      return table->err_cls;
    }

    TokenType op_type = t.front().type();

    ClassId type_id = unary_suffix_operator_return_type(left.type, op_type);
    if (type_id == builtin::Invalid) {
      error(t, Diag::InvalidUnaryArgumentType, to_str(op_type), left.type->identifier);
      return table->err_cls;
    }

    SymbolClass *type = table->to_class(type_id);

    if (is_reference && (op_type == Increment || op_type == Decrement)) {
      error(t, Diag::ReferenceCannotBindSideEffect);
      return table->err_cls;
    }

#ifdef _MSC_VER
/* Silence warning about bool operand.
 * They cannot happen because they are caught by the above check */
#  pragma warning(push)
#  pragma warning(disable : 4805)
#endif

    ExprFlag flags = ExprFlag(left.flags);
    if (flags & ExprFlag::IsConstexpr) {
      switch (op_type) {
        /* Note that side effects are not propagated. */
        case Increment:
          return {type, left.value++};
        case Decrement:
          return {type, left.value--};
        default:
          assert(0); /* Unreachable. */
          return table->err_cls;
      }
    }

#ifdef _MSC_VER
#  pragma warning(pop)
#endif
    return {type, flags};
  }

  ExpressionResult comma(ExpressionResult /*lhs*/, const ast::Node & /*t*/, BindingPower p)
  {
    using namespace builtin;
    ExpressionResult rhs = expr(p);
    return rhs;
  }

  ExpressionResult binary(ExpressionResult lhs, const ast::Node &t, BindingPower p)
  {
    using namespace builtin;
    ExpressionResult rhs = expr(p);

    if (lhs.array_dim > 0 || rhs.array_dim > 0) {
      error(node, Diag::ArrayTypeOperand);
      return table->err_cls;
    }

    TokenType op_type = t.front().type();

    ClassId type_id = binary_operator_return_type(lhs.type, op_type, rhs.type);
    if (type_id == builtin::Invalid) {
      error(t,
            Diag::InvalidBinaryOperands,
            lhs.type->identifier,
            to_str(op_type),
            rhs.type->identifier);
      return table->err_cls;
    }

    SymbolClass *type = table->to_class(type_id);
    ExprFlag flags = ExprFlag(lhs.flags & rhs.flags);

    /* For debugging. */
    // std::cout << t.prev().str() << t.str() << t.next().str() << std::endl;

#ifdef _MSC_VER
/* Silence warning about bool operand.
 * They cannot happen because they are caught by the above check */
#  pragma warning(push)
#  pragma warning(disable : 4805)
#endif

    if (flags & ExprFlag::IsConstexpr) {
      switch (op_type) {
        case Multiply:
          return {type, lhs.value * rhs.value};
        case Divide:
          return {type, [&]() {
                    if (contains_zero(rhs.value)) {
                      error(t, Diag::ConstexprDivisionByZero);
                      return lhs.value * rhs.value;
                    }
                    return lhs.value / rhs.value;
                  }()};
        case Modulo:
          return {type, [&]() {
                    if (contains_zero(rhs.value)) {
                      error(t, Diag::ConstexprDivisionByZero, constexpr_to_string(rhs.value));
                      return decltype(lhs.value % rhs.value)(0);
                    }
                    return lhs.value % rhs.value;
                  }()};
        case LShift:
          return {type, [&]() {
                    if (anyLessThan(rhs.value, 0)) {
                      error(t, Diag::ConstexprShiftNegative, constexpr_to_string(rhs.value));
                      return decltype(lhs.value << rhs.value)(0);
                    }
                    if (anyGreaterThanEqual(rhs.value, 32)) {
                      error(t, Diag::ConstexprShiftTooLarge, constexpr_to_string(rhs.value));
                      return decltype(lhs.value << rhs.value)(0);
                    }
                    return lhs.value << rhs.value;
                  }()};
        case RShift:
          return {type, [&]() {
                    if (anyLessThan(rhs.value, 0)) {
                      error(t, Diag::ConstexprShiftNegative, constexpr_to_string(rhs.value));
                      return decltype(lhs.value >> rhs.value)(0);
                    }
                    if (anyGreaterThanEqual(rhs.value, 32)) {
                      error(t, Diag::ConstexprShiftTooLarge, constexpr_to_string(rhs.value));
                      return decltype(lhs.value >> rhs.value)(0);
                    }
                    return lhs.value >> rhs.value;
                  }()};
        case Plus:
          return {type, lhs.value + rhs.value};
        case Minus:
          return {type, lhs.value - rhs.value};
        case LThan:
          return {type, lhs.value < rhs.value};
        case LEqual:
          return {type, lhs.value <= rhs.value};
        case GThan:
          return {type, lhs.value > rhs.value};
        case GEqual:
          return {type, lhs.value >= rhs.value};
        case Equal:
          return {type, lhs.value == rhs.value};
        case NotEqual:
          return {type, lhs.value != rhs.value};
        case And:
          return {type, lhs.value & rhs.value};
        case Xor:
          return {type, lhs.value ^ rhs.value};
        case Or:
          return {type, lhs.value | rhs.value};
        case LogicalAnd:
          return {type, lhs.value && rhs.value};
        case LogicalOr:
          return {type, lhs.value || rhs.value};
        default:
          assert(0); /* Unreachable. */
          return table->err_cls;
      }
    }

#ifdef _MSC_VER
#  pragma warning(pop)
#endif
    return {type, flags};
  }

  ExpressionResult ternary(ExpressionResult left,
                           const ast::Node &t,
                           BindingPower p_first,
                           BindingPower p_second)
  {
    ExpressionResult tval = sub_expression(node, node, p_first);
    if (consume().front().type() != Colon) {
      error(ErrorType::ExpectedColon, Colon);
      return table->err_cls;
    }
    ExpressionResult fval = expr(p_second);

    if (left.type != table->bool_cls && left.type != table->bool32_t_cls) {
      error(t, Diag::TypeNotConvertibleToBool, left.type->original);
      return table->err_cls;
    }
    if (tval.type != fval.type) {
      error(t, Diag::OperatorInvalidTernary, tval.type->original, fval.type->original);
      return table->err_cls;
    }

    if (left.flags & ExprFlag::IsConstexpr) {
      return {tval.type, left.value.comp_as<int>(0) ? tval.value : fval.value};
    }
    /* Operand types match. We can return either. */
    return {tval.type, ExprFlag(tval.flags & fval.flags)};
  }

  ExpressionResult error(ErrorType err, lexit::TokenType type)
  {
    switch (err) {
      case ErrorType::ExpectedParenthesis:
        error(node, Diag::ExpectedParenthesis);
        break;
      case ErrorType::ExpectedColon:
        error(node, Diag::ExpectedColon);
        break;
      case ErrorType::InvalidOperator:
        error(node, Diag::OperatorTokenInvalid, to_str(type));
        break;
      case ErrorType::InvalidExpression:
        error(node, Diag::InvalidExprTypeChecker);
        break;
    }
    return table->err_cls;
  }

  TokenType peek() const
  {
    return node.is_valid() ? node.front().type() : Invalid;
  }

  ast::Node consume()
  {
    ast::Node n = node;
    node = node.next();
    return n;
  }

  TokenType to_type(const ast::Node &t) const
  {
    ast::NodeType type = t.type();
    return type != ast::NodeType::Invalid ?
               (type == ast::NodeType::IdQualified || type == ast::NodeType::FuncCall ?
                    Word :
                    t.front().type()) :
               Invalid;
  }

 private:
  /* Helper to allow evaluating nested AST node inside the expression. */
  ExpressionResult sub_expression(ast::Node node, ast::Expr expression, BindingPower p)
  {
    /* Enter sub-expression */
    this->node = expression.child_first();
    /* Parse the whole parenthesis expression. */
    ExpressionResult v = expr(p);
    /* Exit sub-expression. */
    this->node = node.next();
    return v;
  }

  void error(ast::Node node,
             Diag diag,
             const std::string &msg = "",
             const std::string &msg2 = "",
             const std::string &msg3 = "")
  {
    /* Only log the first error. */
    if (!err) {
      err = {node, diag, msg, msg2, msg3};
    }
  }

  /* Pipe error if existing. */
  void error(std::optional<AstNodeException> &err)
  {
    if (err) {
      error(err->node, err->diag, err->param1, err->param2, err->param3);
    }
  }
};

Result<ExpressionResult> SymbolTable::expr_type_analysis(const SymbolScope &scope,
                                                         ast::Node start_node,
                                                         bool is_reference) const
{
  assert(start_node.is_valid());
  /* Caller must not pass the Expr node but it's first child. */
  assert(start_node.type() != ast::NodeType::Expr);
  return ExpressionTypeParser(this, &scope, start_node, is_reference).eval();
}

}  // namespace bsl

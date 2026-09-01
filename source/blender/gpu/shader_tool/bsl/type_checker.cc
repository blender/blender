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
          return {table->err_cls, 1, false};
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
          return {table->err_cls, 1, false};
        }

        if (is_reference) {
          error(t, Diag::FunctionCallNotAllowedInReferenceDefinition);
        }
        auto sym = scope->lookup_function(*table, call.identifier(), call.parameters(), *scope);
        SymbolFunction *fn = sym.unwrap(err);
        if (fn->is_builtin) {
          /* Implement builtin type casts. */
          ast::Expr param = call.parameters().splat_1().arg1;
          if (!param.is_valid()) {
            /* Not a constexpr. */
          }
          else {
            ExpressionResult res = sub_expression(t, param, 0);
            if (!res.is_constexpr() || res.array_dim != 0) {
              /* Not a constexpr. */
            }
            else if (fn->identifier == "float") {
              return std::visit(
                  [&](auto val) -> ExpressionResult { return {table->float_cls, float(val)}; },
                  res.value);
            }
            else if (fn->identifier == "int") {
              return std::visit(
                  [&](auto val) -> ExpressionResult { return {table->int_cls, int(val)}; },
                  res.value);
            }
            else if (fn->identifier == "uint") {
              return std::visit(
                  [&](auto val) -> ExpressionResult { return {table->uint_cls, uint32_t(val)}; },
                  res.value);
            }
            else if (fn->identifier == "bool") {
              return std::visit(
                  [&](auto val) -> ExpressionResult { return {table->bool_cls, bool(val)}; },
                  res.value);
            }
            else if (fn->identifier == "floatBitsToUint") {
              return std::visit(
                  [&](auto val) -> ExpressionResult {
                    return {table->uint_cls, std::bit_cast<uint32_t>(float(val))};
                  },
                  res.value);
            }
            else if (fn->identifier == "floatBitsToInt") {
              return std::visit(
                  [&](auto val) -> ExpressionResult {
                    return {table->int_cls, std::bit_cast<int>(float(val))};
                  },
                  res.value);
            }
            else if (fn->identifier == "uintBitsToFloat") {
              return std::visit(
                  [&](auto val) -> ExpressionResult {
                    return {table->uint_cls, std::bit_cast<float>(uint32_t(val))};
                  },
                  res.value);
            }
            else if (fn->identifier == "intBitsToFloat") {
              return std::visit(
                  [&](auto val) -> ExpressionResult {
                    return {table->int_cls, std::bit_cast<float>(int(val))};
                  },
                  res.value);
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
        value = std::stof(str_val);
      }
      else if (type == table->int_cls) {
        value = std::stoi(str_val, nullptr, 0);
      }
      else if (type == table->uint_cls) {
        value = uint32_t(std::stoull(str_val, nullptr, 0));
      }
      else if (type == table->bool_cls) {
        value = str_val == "true";
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

    ExprFlag flags = ExprFlag(right.flags);
    if (flags & ExprFlag::IsConstexpr) {
      switch (op_type) {
        /* Note that side effects are not propagated. */
        case Increment:
          return {type, apply_unary_arithmetic(right, [](auto r) { return ++r; })};
        case Decrement:
          return {type, apply_unary_arithmetic(right, [](auto r) { return --r; })};
        case Plus:
          return {type, apply_arithmetic(right, [](auto r) { return +r; })};
        case Minus:
          return {type, apply_arithmetic(right, [](auto r) { return -r; })};
        case Not:
          /* Note that '!' token is of MultiTok class and can contain many unary '!'. */
          return {type,
                  (t.str().size() & 1) ? apply_arithmetic(right, [](auto r) { return !r; }) :
                                         apply_arithmetic(right, [](auto r) { return !!r; })};
        case BitwiseNot:
          return {type, apply_integral(right, [](auto r) { return ~r; })};
        default:
          assert(0); /* Unreachable. */
          return table->err_cls;
      }
    }
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

    ExprFlag flags = ExprFlag(left.flags);
    if (flags & ExprFlag::IsConstexpr) {
      switch (op_type) {
        /* Note that side effects are not propagated. */
        case Increment:
          return {type, apply_unary_arithmetic(left, [](auto r) { return r++; })};
        case Decrement:
          return {type, apply_unary_arithmetic(left, [](auto r) { return r--; })};
        default:
          assert(0); /* Unreachable. */
          return table->err_cls;
      }
    }
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

    if (flags & ExprFlag::IsConstexpr) {
      switch (op_type) {
        case Multiply:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l * r; })};
        case Divide:
          return {type, apply_arithmetic(lhs, rhs, [&](auto l, auto r) {
                    if (r == 0) {
                      error(t, Diag::ConstexprDivisionByZero);
                      return l * r;
                    }
                    return l / r;
                  })};
        case Modulo:
          return {type, apply_integral(lhs, rhs, [&](auto l, auto r) {
                    if (r == 0) {
                      error(t, Diag::ConstexprDivisionByZero, to_string(r));
                      return decltype(l % r)(0);
                    }
                    return l % r;
                  })};
        case LShift:
          return {type, apply_integral(lhs, rhs, [&](auto l, auto r) {
                    if (int(r) < 0) {
                      error(t, Diag::ConstexprShiftNegative, to_string(r));
                      return decltype(l << r)(0);
                    }
                    if (int(r) >= 32) {
                      error(t, Diag::ConstexprShiftTooLarge, to_string(r));
                      return decltype(l << r)(0);
                    }
                    return l << r;
                  })};
        case RShift:
          return {type, apply_integral(lhs, rhs, [&](auto l, auto r) {
                    if (int(r) < 0) {
                      error(t, Diag::ConstexprShiftNegative, to_string(r));
                      return decltype(l >> r)(0);
                    }
                    if (int(r) >= 32) {
                      error(t, Diag::ConstexprShiftTooLarge, to_string(r));
                      return decltype(l >> r)(0);
                    }
                    return l >> r;
                  })};
        case Plus:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l + r; })};
        case Minus:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l - r; })};
        case LThan:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l < r; })};
        case LEqual:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l <= r; })};
        case GThan:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l > r; })};
        case GEqual:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l >= r; })};
        case Equal:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l == r; })};
        case NotEqual:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l != r; })};
        case And:
          return {type, apply_integral(lhs, rhs, [](auto l, auto r) { return l & r; })};
        case Xor:
          return {type, apply_integral(lhs, rhs, [](auto l, auto r) { return l ^ r; })};
        case Or:
          return {type, apply_integral(lhs, rhs, [](auto l, auto r) { return l | r; })};
        case LogicalAnd:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l && r; })};
        case LogicalOr:
          return {type, apply_arithmetic(lhs, rhs, [](auto l, auto r) { return l || r; })};
        default:
          assert(0); /* Unreachable. */
          return table->err_cls;
      }
    }
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
      return {tval.type, std::get<bool>(left.value) ? tval.value : fval.value};
    }
    /* Operand types match. We can return either. */
    return {tval.type, tval.flags & fval.flags};
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

  /*
   * Helper to evaluate operations valid for all types (Arithmetic).
   * C++ decltype(a op b) natively dictates the correct promoted return type
   * which perfectly forwards into std::variant.
   */
  template<typename Func>
  ConstexprValue apply_arithmetic(const ExpressionResult &lhs,
                                  const ExpressionResult &rhs,
                                  Func func) const
  {
    return std::visit(
        [func](auto &&a, auto &&b) -> ConstexprValue { return func(a, b); }, lhs.value, rhs.value);
  }
  template<typename Func>
  ConstexprValue apply_arithmetic(const ExpressionResult &rhs, Func func) const
  {
    return std::visit([func](auto &&b) -> ConstexprValue { return func(b); }, rhs.value);
  }

  /* Helper to evaluate bitwise and modulo operations (requires integral types). */
  template<typename Func>
  ConstexprValue apply_integral(const ExpressionResult &lhs,
                                const ExpressionResult &rhs,
                                Func func) const
  {
    return std::visit(
        [func](auto &&a, auto &&b) -> ConstexprValue {
          using T = std::decay_t<decltype(a)>;
          using U = std::decay_t<decltype(b)>;

          if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
            return func(a, b);
          }
          else {
            /* Invalid operands: operation require integral types. */
            assert(0);
            return 0;
          }
        },
        lhs.value,
        rhs.value);
  }
  template<typename Func>
  ConstexprValue apply_integral(const ExpressionResult &rhs, Func func) const
  {
    return std::visit(
        [func](auto &&b) -> ConstexprValue {
          using T = std::decay_t<decltype(b)>;

          if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            return func(b);
          }
          else {
            /* Invalid operands: operation require integral types. */
            assert(0);
            return 0;
          }
        },
        rhs.value);
  }

  template<typename Func>
  ConstexprValue apply_unary_arithmetic(const ExpressionResult &rhs, Func func) const
  {
    return std::visit(
        [func](auto &&b) -> ConstexprValue {
          using T = std::decay_t<decltype(b)>;

          if constexpr (!std::is_same_v<T, bool>) {
            return func(b);
          }
          else {
            /* Invalid operands: operation require float of integral types. */
            assert(0);
            return 0;
          }
        },
        rhs.value);
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

/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "diagnostic.hh"
#include "metadata.hh"
#include "pratt_parser.hh"
#include "processor.hh"
#include "symbol_table.hh"

#include <algorithm>
#include <sstream>

namespace bsl {
using namespace std;
using namespace blender::gpu::shader;
using namespace blender::gpu::shader::parser;
using namespace blender::gpu::shader::parser::ast;

static string to_uppercase(string str)
{
  for (char &c : str) {
    c = toupper(c);
  }
  return str;
}

struct CodegenContext : NodeErrorHandler {
  SymbolTable &table;
  metadata::Source &metadata;

  struct CodegenResult {
    string str;

    SymbolClass *type;
    /* Array dimension of the current temporary. */
    int8_t array_dim = 0;

    CodegenResult(SymbolVariable *var, const string &trivia)
        : str((var->is_constexpr ? var->value_str() : var->identifier) + trivia),
          type(var->type),
          array_dim(var->array_dimensions)
    {
    }

    CodegenResult(const ast::Node &node, SymbolClass *cls, int8_t array_dim = 0)
        : str(node.str_full()), type(cls), array_dim(array_dim)
    {
    }

    CodegenResult(const string &str, SymbolClass *cls, int8_t array_dim = 0)
        : str(str), type(cls), array_dim(array_dim)
    {
    }
  };

  /**
   * Expression parser for code generation using Pratt parsing.
   * Evaluates operand types and emits tokens in source order.
   */
  struct CodegenExpressionParser
      : public PrattParser<CodegenExpressionParser, CodegenResult, ast::Node> {
    using Base = PrattParser<CodegenExpressionParser, CodegenResult, ast::Node>;
    using ErrorType = Base::ErrorType;
    using BindingPower = Base::BindingPower;

   private:
    CodegenContext &ctx;
    const SymbolScope &scope;
    /* Point of instantiation of the containing function. */
    optional<SourceLocation> poi;
    ast::Node node;

   public:
    CodegenExpressionParser(CodegenContext &ctx, const SymbolScope &scope, ast::Node start_node)
        : ctx(ctx), scope(scope), node(start_node)
    {
      if (const SymbolFunction *fn = scope.parent_function(); fn) {
        poi = fn->poi;
      }
    }

    CodegenResult eval()
    {
      if (!node.is_valid()) {
        return {node, ctx.table.err_cls};
      }
      return expr(0);
    }

    /**
     * CRTP Overrides
     */

    CodegenResult identifier(const ast::Node &t)
    {
      switch (t.type()) {
        case ast::NodeType::Constructor:
          return constructor(t);
        case ast::NodeType::FuncCall:
          return function(t, scope);
        case ast::NodeType::LocalVar:
          return variable(LocalVar(t).identifier(), scope);
        default:
          return {t, ctx.table.err_cls};
      }
      return {t, ctx.table.err_cls};
    }

    CodegenResult number_literal(const ast::Node &t)
    {
      return {t, ctx.table.get_literal_type(t.str())};
    }

    CodegenResult string_literal(const ast::Node &t)
    {
      return {t, ctx.table.str_cls};
    }

    CodegenResult parenthesis(const ast::Node &t, BindingPower /*p*/)
    {
      CodegenResult result = ctx.expr(ExprSub(t).expr(), scope);
      string front(t.front().str_with_whitespace());
      string back(t.back().str_with_whitespace());
      return {front + result.str + back, result.type};
    }

    CodegenResult member(CodegenResult left, const ast::Node &t)
    {
      ast::Node right = consume();
      switch (right.type()) {
        case ast::NodeType::FuncCall: {
          CodegenResult rhs = function(right, *left.type);
          return {left.str + string(t.str_full()) + rhs.str, rhs.type};
        }
        case ast::NodeType::LocalVar: {
          CodegenResult rhs = variable(LocalVar(right).identifier(), *left.type);
          if (left.type->is_srt()) {
            /* Remove access to SRT type (makes the resource global). */
            return {left.str + as_whitespace(t.str_full()) + rhs.str, rhs.type, rhs.array_dim};
          }
          return {left.str + string(t.str_full()) + rhs.str, rhs.type, rhs.array_dim};
        }
        default:
          return {t, ctx.table.err_cls};
      }
    }

    CodegenResult subscript(CodegenResult left, const ast::Node &t)
    {
      /* Subscript operator. */
      if (ast::Subscript sub = t; sub.is_valid()) {
        CodegenResult param = ctx.expr(sub.expr(), scope);

        string_view front = t.front().str_with_whitespace();
        string_view back = t.back().str_with_whitespace();

        if (left.array_dim > 0) {
          left.str += string(front) + param.str + string(back);
          left.array_dim -= 1;
          return left;
        }
        /* Call subscript operator */
        if (left.type->operator_subscript) {
#if 0 /* Future operator overloading */
          if (true) {
            left.str = SymbolTable::subscript_operator_id + '(' + left.str + ',' + param.str + ')';
            return {left.str, left.type->operator_subscript->return_type};
          }
#endif
          left.str += string(front) + param.str + string(back);
          return {left.str, left.type->operator_subscript->return_type};
        }
      }
      ctx.error(t, Diag::InvalidSubscript);
      return {t, ctx.table.err_cls};
    }

    CodegenResult function_call(CodegenResult left, const ast::Node &t)
    {
      ctx.error(t, Diag::OperatorCalledIsNotFunction, left.type ? left.type->identifier : "");
      return {t, ctx.table.err_cls};
    }

    CodegenResult prefix(const ast::Node &t, BindingPower p)
    {
      using namespace builtin;
      TokenType op_type = t.front().type();
      CodegenResult right = expr(p);

      ClassId type = unary_prefix_operator_return_type(op_type, right.type);
      return {string(t.str_full()) + right.str, ctx.table.to_class(type)};
    }

    CodegenResult suffix(CodegenResult left, const ast::Node &t)
    {
      using namespace builtin;
      TokenType op_type = t.front().type();

      ClassId type = unary_suffix_operator_return_type(left.type, op_type);
      return {left.str + string(t.str_full()), ctx.table.to_class(type)};
    }

    CodegenResult comma(CodegenResult lhs, const ast::Node &t, BindingPower p)
    {
      using namespace builtin;
      CodegenResult rhs = expr(p);
      return {lhs.str + string(t.str_full()) + rhs.str, rhs.type};
    }

    CodegenResult binary(CodegenResult lhs, const ast::Node &t, BindingPower p)
    {
      using namespace builtin;
      TokenType op_type = t.front().type();

      CodegenResult rhs = expr(p);

      ClassId type = binary_operator_return_type(lhs.type, op_type, rhs.type);
#if 0 /* Future operator overloading */
      if (true) {
        const string op_name = ctx.table.binary_type_to_id.find(op_type)->second;
        return {op_name + '(' + lhs.str + ',' + rhs.str + ')', type};
      }
#endif
      return {lhs.str + string(t.str_full()) + rhs.str, ctx.table.to_class(type)};
    }

    CodegenResult ternary(CodegenResult left,
                          const ast::Node &t,
                          BindingPower /*p_first*/,
                          BindingPower p_second)
    {
      ast::Node question = t;
      CodegenResult tval = ctx.expr(consume(), scope);
      ast::Node colon = consume();
      CodegenResult fval = expr(p_second);
      return {left.str + string(question.str_full()) + tval.str + string(colon.str_full()) +
                  fval.str,
              tval.type};
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

    CodegenResult error(ErrorType err, lexit::TokenType type)
    {
      switch (err) {
        case ErrorType::ExpectedParenthesis:
          ctx.error(node, Diag::ExpectedParenthesis);
          break;
        case ErrorType::ExpectedColon:
          ctx.error(node, Diag::ExpectedColon);
          break;
        case ErrorType::InvalidOperator:
          ctx.error(node, Diag::OperatorTokenInvalid, to_str(type));
          break;
        case ErrorType::InvalidExpression:
          ctx.error(node, Diag::InvalidExprTypeChecker);
          break;
      }
      return {node, ctx.table.err_cls};
    }

   private:
    CodegenResult constructor(Constructor construct)
    {
      SymbolClass *cls = scope.lookup_class(ctx.table, construct.identifier()).unwrap(&ctx);
      if (cls->is_srt()) {
        ctx.error(construct, Diag::ResourceTableConstructorNotAllowed, cls->identifier);
      }

      return ctx.initializer_list(construct.initializer_list(), scope, cls);
    }

    CodegenResult variable(IdQualified id, const SymbolScope &symbol_scope)
    {
      assert(id.is_valid());
      string whitespace = trivia(id);

      SymbolVariable *var = symbol_scope.lookup_variable(ctx.table, id);
      if (var->is_error) {
        ctx.error(id, Diag::UnknownVariable, id.str());
        return {var, whitespace};
      }

      if (var->is_constexpr) {
        return {var, whitespace};
      }

      if (var->type->is_srt()) {
        bool followed_by_dot = id.back().next() == '.';
        bool followed_by_member_call = id.parent().next().next() == NodeType::FuncCall;
        if (followed_by_dot && !followed_by_member_call) {
          /* Remove access to SRT type (makes the resource global). */
          return {as_whitespace(id.str_full()), var->type};
        }
        /* SRT type is used in an expression (likely as function argument). Call the ctor. */
        return {var->type->identifier + "_ctor_()" + whitespace, var->type};
      }

      if (var->reference_value.is_valid()) {
        return {string(var->reference_value.str()) + whitespace, var->type, var->array_dimensions};
      }

      bool preceded_by_dot = id.front().prev() == '.';
      if (!var->is_static && !preceded_by_dot && var->parent->as_class() != nullptr) {
        return {"this_." + var->identifier + whitespace, var->type, var->array_dimensions};
      }
      return {var, whitespace};
    }

    CodegenResult function(const ast::FuncCall &call, const SymbolScope &symbol_scope)
    {
      assert(call.is_valid());

      IdQualified id = call.identifier();

      /* If accessing a function in a different class, we need to set the location at the point of
       * instantiation. Otherwise we cannot use CRTP patterns. */
      SourceLocation loc = (&symbol_scope != &scope) ? poi.value_or(id.front()) : id.front();

      SymbolFunction *sym = ctx.id_func_resolved_lookup(
          id, call.parameters(), symbol_scope, scope, loc);

      if (sym->is_error) {
        ctx.error(call, Diag::UnknownFunction);
        return {"", sym->return_type};
      }

      if (sym->is_inline) {
        ctx.error(call, Diag::InvalidInlinedFunctionLocation);
      }

      FuncParamList list = call.parameters();
      std::string params_str;
      if (sym->fn_type == SymbolFunction::MEMBER && list.parent().prev().back() != '.') {
        params_str += "this_";
        if (!list.is_empty()) {
          params_str += ", ";
        }
      }

      int param_count = 0;
      for (Expr param : list.children_of_type<Expr>()) {
        params_str += ctx.expr(param, scope).str;
        params_str += ctx.opt_str(param.back().next(), Comma);
        ++param_count;
      }

      /* Default arguments. */
      for (; param_count < sym->arg_defaults.size(); ++param_count) {
        if (sym->arg_defaults[param_count].is_valid()) {
          if (param_count != 0) {
            params_str += ", ";
          }
          /* Note: We evaluate the expression from where it was defined, in the function scope. */
          params_str += ctx.expr(sym->arg_defaults[param_count], *sym).str;
        }
        else {
          ctx.error(list, Diag::MissingParameterForCall, sym->identifier);
        }
      }

      params_str = std::string(list.front().str_with_whitespace()) + params_str +
                   std::string(list.back().str_with_whitespace()) + trivia(call);

      return {sym->identifier + trivia(id) + params_str, sym->return_type};
    }
  };

  struct StringBuilder {
    Token curr;
    stringstream ss;

    StringBuilder &operator<<(Token tok)
    {
      ss << tok.str_with_whitespace();
      curr = curr.next();
      return *this;
    }

    StringBuilder &operator<<(const string &str)
    {
      ss << str;
      curr = curr.next();
      return *this;
    }

    StringBuilder &operator<<(const string_view &str)
    {
      ss << str;
      curr = curr.next();
      return *this;
    }

    StringBuilder &operator<<(const Node &node)
    {
      ss << node.front().buf_->substr(node.front(), node.back(), true);
      curr = node.back().next();
      return *this;
    }

    StringBuilder &operator<<(const StringBuilder &sb)
    {
      ss << sb.ss.str();
      curr = curr.next();
      return *this;
    }
  } builder;

  CodegenContext(SourceProcessor::Parser &parser,
                 SymbolTable &symbols,
                 metadata::Source &metadata,
                 SourceProcessor::ErrorHandler &error_handler)
      : NodeErrorHandler(error_handler),
        table(symbols),
        metadata(metadata),
        builder(Token::invalid(&parser))
  {
  }

  string process(SymbolScope &symbol, LocalScope node)
  {
    local_scope(node, symbol);
    return builder.ss.str();
  }

 private:
  static string trivia(Token t)
  {
    string_view no_ws = t.str();
    return string(t.str_with_whitespace().substr(no_ws.size()));
  }

  static string trivia(Node node)
  {
    return trivia(node.back());
  }

  static string as_whitespace(string_view str)
  {
    size_t lines = count(str.begin(), str.end(), '\n');
    size_t spaces = str.find_last_of("\n");
    if (spaces != string::npos) {
      spaces = str.length() - (spaces + 1);
    }
    else {
      spaces = str.length();
    }
    return string(lines, '\n') + string(spaces, ' ');
  }

  void skip_node(Node node)
  {
    if (node.is_valid()) {
      const TokenBuffer *buf = node.front().buf_;
      string_view str = buf->substr(node.front(), node.back(), true);
      builder.ss << as_whitespace(str);
      builder.curr = node.back().next();
    }
  }

  /* Only go to next token if matching an optional token. */
  template<typename... Args> bool skip_if(Args... expected)
  {
    if (((builder.curr == TokenType(expected)) || ...)) {
      string_view str = builder.curr.str_with_whitespace();
      builder << as_whitespace(str);
      return true;
    }
    return false;
  }

  void line(Token tok)
  {
    builder.ss << "\n#line " + to_string(tok.line_number()) + "\n";
  }

  void line(Node node)
  {
    line(node.front());
  }

  void jump_to(Token tok)
  {
    line(tok);
    builder.ss << string(tok.char_number(), ' ');
    builder.curr = tok;
  }

  /* Only go to next token if matching an optional token. */
  template<typename... Args> bool match_if(Args... expected)
  {
    if (((builder.curr == TokenType(expected)) || ...)) {
      builder << builder.curr;
      return true;
    }
    return false;
  }

  void local_scope(LocalScope node, SymbolScope &symbol)
  {
    assert(node.is_valid());

    /* Check if scope started with a bracket, otherwise we could emit a contained scope last token
     * because of the last jump_to. */
    bool has_bracket = false;
    if (symbol.parent != nullptr) {
      if (symbol.type == SymbolScope::NAMESPACE) {
        skip_if('{');
      }
      else {
        has_bracket = match_if('{');
      }
    }

    /* Resource guards for local SRT variables. Can be phased out once we remove the need for
     * resource_table_get.  */
    string cond = !symbol.as_function() ? condition(&symbol) : "";
    if (!cond.empty()) {
      builder.ss << string("\n#if ") + cond + "\n";
      jump_to(builder.curr);
    }

    if (node.child_first().is_valid()) {
      jump_to(node.child_first().front());
    }

    int local_scope_id = 0;
    for (Node node : node.children_range()) {
      switch (node.type()) {
        case NodeType::Preprocessor:
          builder << node;
          break;
        case NodeType::Namespace:
          namespace_decl(ast::Namespace(node), symbol);
          break;
        case NodeType::VarDecl:
          var_decl(VarDecl(node), symbol);
          break;
        case NodeType::ClassDecl:
          class_decl(ClassDecl(node), symbol);
          break;
        case NodeType::FuncDecl:
          func_decl(FuncDecl(node), symbol);
          break;
        case NodeType::TemplateDecl:
          skip_node(node);
          break;
        case NodeType::TemplateInst:
          template_inst(TemplateInst(node), symbol);
          break;
        case NodeType::TemplateSpec:
          template_spec(TemplateSpec(node), symbol);
          break;
        case NodeType::UsingStmt:
          skip_node(node);
          break;
        case NodeType::ForLoop:
          for_loop(node, *symbol.child_scope(local_scope_id++));
          break;
        case NodeType::WhileLoop:
          while_loop(node, *symbol.child_scope(local_scope_id++));
          break;
        case NodeType::DoWhileLoop:
          do_while_loop(node, *symbol.child_scope(local_scope_id++));
          break;
        case NodeType::SwitchStmt:
          switch_statement(node, symbol, local_scope_id);
          break;
        case NodeType::ReturnStmt:
          return_statement(node, symbol);
          break;
        case NodeType::StructuredBinding:
          structured_binding(node, symbol);
          break;
        case NodeType::IfStmt:
          if (Condition(node.child_first()).attributes().contains_attr("static_branch")) {
            static_if_statement(node, symbol, local_scope_id);
          }
          else {
            if_statement(node, symbol, local_scope_id);
          }
          break;
        case NodeType::ElseIfStmt:
        case NodeType::ElseStmt:
          /* Should have been processed by the if statement. */
          break;
        case NodeType::LocalStmt:
          local_statement(node, symbol);
          break;
        case NodeType::LocalScope:
          local_scope(LocalScope(node), *symbol.child_scope(local_scope_id++));
          break;
        case NodeType::PipelineDecl:
          pipeline_decl(node, symbol);
          break;
        default:
          builder << node;
          break;
      }
    }

    if (!cond.empty()) {
      builder.ss << string("\n#endif\n");
    }

    if (symbol.parent != nullptr) {
      jump_to(node.back());
      if (symbol.type == SymbolScope::NAMESPACE) {
        skip_if('}');
      }
      else if (has_bracket) {
        match_if('}');
      }
    }
  }

  void pipeline_decl(PipelineDecl stmt, SymbolScope &scope)
  {
    SymbolVariable *pipeline = scope.lookup_variable(table, stmt.identifier());

    const bool is_graphic = stmt.type().str() == "PipelineGraphic";

    FuncParamList params = stmt.parameters();

    jump_to(stmt.front());
    builder << stmt.type();
    builder << pipeline->identifier;
    builder.curr = params.front();
    match_if('(');

    Expr constants_init = Node{};

    if (is_graphic) {
      Expr vert_fn = params.child_first();
      if (!vert_fn.is_valid() || vert_fn.child_count() != 1) {
        error(params, Diag::ExpectedVertexShaderEntryPoint);
        return;
      }
      /* Resolve vertex function. */
      auto *fn =
          scope.lookup_function(table, LocalVar(vert_fn.child_first()).identifier()).unwrap(this);
      if (fn->entry_point_type != SymbolFunction::EntryPointType::VERT) {
        error(vert_fn, Diag::ExpectedVertexShaderEntryPoint);
        return;
      }
      builder << fn->identifier;
      builder.curr = vert_fn.back().next();
      match_if(',');

      Expr frag_fn = vert_fn.next();
      if (!frag_fn.is_valid() || frag_fn.child_count() != 1) {
        error(frag_fn, Diag::ExpectedFragmentShaderEntryPoint);
        return;
      }
      /* Resolve fragment function. */
      auto *fn2 =
          scope.lookup_function(table, LocalVar(frag_fn.child_first()).identifier()).unwrap(this);
      if (fn2->entry_point_type != SymbolFunction::EntryPointType::FRAG) {
        error(frag_fn, Diag::ExpectedFragmentShaderEntryPoint);
        return;
      }
      builder << fn2->identifier;
      builder.curr = frag_fn.back().next();
      match_if(',');

      constants_init = frag_fn.next();
    }
    else {
      Expr comp_fn = params.child_first();
      if (!comp_fn.is_valid() || comp_fn.child_count() != 1) {
        error(comp_fn, Diag::ExpectedComputeShaderEntryPoint);
        return;
      }
      /* Resolve compute function. */
      auto *fn =
          scope.lookup_function(table, LocalVar(comp_fn.child_first()).identifier()).unwrap(this);
      if (fn->entry_point_type != SymbolFunction::EntryPointType::COMP) {
        error(comp_fn, Diag::ExpectedComputeShaderEntryPoint);
        return;
      }
      builder << fn->identifier;
      builder.curr = comp_fn.back().next();
      match_if(',');

      constants_init = comp_fn.next();
    }

    while (constants_init.is_valid()) {
      if (Constructor ctor(constants_init.child_first()); ctor.is_valid()) {
        auto *cls = scope.lookup_class(table, ctor.identifier()).unwrap(this);
        builder << cls->identifier;
        builder << ctor.initializer_list();
      }
      else {
        error(constants_init, Diag::ExpectedResourceTableInitializer);
      }
      constants_init = constants_init.next();
    }

    builder.curr = params.back();
    match_if(')');
    match_if(';');
  }

  void return_statement(ReturnStmt stmt, SymbolScope &scope)
  {
    if (SymbolFunction *fn = scope.parent_function(); fn) {
      string ret_val;
      if (auto [r_fn, call] = get_inlined_function(stmt.expression(), scope); r_fn) {
        ret_val = inline_function(r_fn, call, scope);
        /* Resume. */
        jump_to(stmt.front());
      }

      if (fn->is_inline) {
        SymbolVariable *var = fn->lookup_variable(SymbolFunction::inline_fn_ret_id);
        builder.ss << var->identifier << " = ";
      }
      else {
        match_if(Return);
      }

      if (!ret_val.empty()) {
        builder.ss << ret_val;
      }
      else if (InitializerList list(stmt.expression().child_first()); list.is_valid()) {
        builder.ss << initializer_list(list, scope, fn->return_type).str;
      }
      else {
        builder.ss << expr(stmt.expression(), scope).str;
      }
      builder.curr = stmt.expression().back().next();

      if (fn->is_inline) {
        builder.ss << "; break";
      }
      match_if(';');
    }
    else {
      error(stmt, Diag::ReturnOutsideFunction);
    }
  }

  void structured_binding(StructuredBinding decl, SymbolScope &scope)
  {
    SymbolVariable *tmp_var = scope.lookup_variable(decl.tmp_id());

    AssignStmt assign = decl.assign();

    Token decl_front = decl.front();
    Token assign_front = assign.front();

    int pad = assign_front.char_number() - decl_front.char_number();

    jump_to(decl_front);
    /* Replace 'auto [a, b]' by the tmp variable declaration. */
    string decl_str = tmp_var->type->identifier + " " + tmp_var->identifier;
    /* Add padding until the assign statement. */
    builder.ss << decl_str + string(max(0, pad - int(decl_str.size())), ' ');

    builder.curr = assign_front;
    assignment(assign, scope, tmp_var);
    match_if(';');
  }

  void switch_statement(SwitchStmt stmt, SymbolScope &scope, int &local_scope_id)
  {
    match_if(Switch);
    condition(stmt.condition(), scope, false, false);
    match_if('{');

    for (Node node : stmt.children_range()) {
      switch (node.type()) {
        case NodeType::Preprocessor:
          builder << node;
          break;
        case NodeType::SwitchCase: {
          SwitchCase switch_case(node);
          jump_to(switch_case.front());
          match_if(Case, Default);
          if (!switch_case.is_default_case()) {
            Node val = switch_case.value();
            if (val.type() == NodeType::IdQualified) {
              id_var_resolved(val, scope);
            }
            else {
              builder << val;
            }
            builder.curr = val.back().next();
          }
          match_if(Colon);
          local_scope(switch_case.body(), *scope.child_scope(local_scope_id++));
          break;
        }
        case NodeType::Condition:
          break; /* Already processed. */
        default:
          assert(0);
      }
    }
    jump_to(stmt.back());
    match_if('}');
  }

  void loop_unroll(ForLoop stmt, SymbolScope &scope)
  {
    VarDecl init_stmt = stmt.condition().child_first();
    LocalStmt cond_stmt = init_stmt.next();
    LocalStmt end_stmt = cond_stmt.next();
    Expr cond_expr = cond_stmt.expr();
    Expr end_expr = end_stmt.expr();
    if (!init_stmt.is_valid()) {
      error(stmt, Diag::UnrolledLoopMissingInit);
      return;
    }
    Declarator decl = init_stmt.child_first(NodeType::Declarator);
    SymbolVariable *var = scope.lookup_variable(table, decl.identifier());
    if (var->is_error) {
      error(decl.identifier(), Diag::UnknownVariable, decl.identifier().str());
      return;
    }
    /* Check if only a single var is declared. */
    if (decl.next(NodeType::Declarator).is_valid()) {
      error(init_stmt.type(), Diag::UnrolledLoopMultipleVariables);
      return;
    }
    /* Check if loop variable is of integer type. */
    SymbolClass *cls = var->type;
    if (cls != table.int_cls && cls != table.uint_cls) {
      error(init_stmt.type(), Diag::UnrolledLoopNonIntVar);
      return;
    }
    /* Check if loop variable is init by constexpr. */
    Expr init_expr = decl.initial_value().expr();
    if (!init_expr.is_valid()) {
      error(decl, Diag::UnrolledLoopMissingAssignment);
      return;
    }
    auto result = table.expr_type_analysis(scope, init_expr.child_first()).unwrap(this);
    if (!result.is_constexpr()) {
      error(decl, Diag::UnrolledLoopNotConstexpr);
      return;
    }
    ConstexprValue val = result.value;
    /* Check if loop statement assign to and only to the loop variable. */
    for (Node node : end_expr.children_range()) {
      if (node.type() == NodeType::Op && node.front() == ',') {
        error(end_stmt, Diag::UnrolledLoopCommaOperator);
      }
    }
    if (this->err_handler.err) {
      return;
    }

    /* Replace loop variable resolved value with constexpr value. */
    var->is_constexpr = true;
    var->value = val;
    /* Run a small virtual machine. For each iteration, check the condition, and run the loop
     * statement. */
    for (int i = 0;; i++) {
      auto result = table.expr_type_analysis(scope, cond_expr.child_first()).unwrap(this);
      if (!result.is_constexpr()) {
        error(cond_expr, Diag::UnrolledLoopNotConstexpr);
        break;
      }
      /* Break if condition is false. */
      if (value_as<int>(result.value) == 0) {
        break;
      }
      /* Break if too many iterations. */
      if (i >= 64) {
        error(stmt, Diag::UnrolledLoopTooManyIterations);
        break;
      }
      /* Generate the loop body. */
      jump_to(stmt.body().front());
      local_scope(stmt.body(), scope);

      /* Evaluate the loop statement. */
      var->value = eval_constexpr_with_side_effects(var, end_expr, scope);
      if (err_handler.err) {
        break;
      }
    }

    /* Restore. */
    var->is_constexpr = false;

    jump_to(stmt.back().next());
  }

  ConstexprValue eval_constexpr_with_side_effects(SymbolVariable *var,
                                                  Expr expr,
                                                  SymbolScope &scope)
  {
    if (expr.child_count() == 2) {
      /* ExpressionParser will not evaluate increment and decrement operators.
       * We have to handle it ourselves. */
      TokenType op_type = Invalid;
      LocalVar local_var = expr.child_first();
      if (expr.child_last().type() == NodeType::Op) {
        local_var = expr.child_first();
        op_type = expr.back().type();
      }
      else if (expr.child_first().type() == NodeType::Op) {
        local_var = expr.child_last();
        op_type = expr.front().type();
      }

      if (local_var.is_valid()) {
        if (scope.lookup_variable(table, local_var.identifier()) != var) {
          error(expr, Diag::UnrolledLoopMustAssignToVar, var->identifier);
          return 0;
        }
        /* Assign with the correct type cast. */
        switch (op_type) {
          case Decrement:
            return visit([](auto &&v) -> ConstexprValue { return decay_t<decltype(v)>(v - 1); },
                         var->value);
          case Increment:
            return visit([](auto &&v) -> ConstexprValue { return decay_t<decltype(v)>(v + 1); },
                         var->value);
          default:
            break;
        }
      }
    }
    else {
      /* ExpressionParser will not evaluate assignment operator (=,+=,-=, ..)s.
       * We have to handle it ourselves. */
      LocalVar local_var = expr.child_first();
      if (local_var.is_valid()) {
        if (scope.lookup_variable(table, local_var.identifier()) != var) {
          error(expr, Diag::UnrolledLoopMustAssignToVar, var->identifier);
          return 0;
        }
        Node op = local_var.next();
        if (op.type() == NodeType::Op) {
          Node node = op.next();

          auto result = table.expr_type_analysis(scope, node).unwrap(this);
          if (!result.is_constexpr()) {
            error(expr, Diag::UnrolledLoopNotConstexpr, var->identifier);
            return 0;
          }

          /* Assign with the correct type cast. */
          switch (op.front().type()) {
            case Assign:
              return visit(
                  [](auto &&a, auto &&b) -> ConstexprValue { return decay_t<decltype(a)>(b); },
                  var->value,
                  result.value);
            case AssignAdd:
              return visit(
                  [](auto &&a, auto &&b) -> ConstexprValue { return decay_t<decltype(a)>(a + b); },
                  var->value,
                  result.value);
            case AssignSub:
              return visit(
                  [](auto &&a, auto &&b) -> ConstexprValue { return decay_t<decltype(a)>(a - b); },
                  var->value,
                  result.value);
            case AssignMul:
              return visit(
                  [](auto &&a, auto &&b) -> ConstexprValue { return decay_t<decltype(a)>(a * b); },
                  var->value,
                  result.value);
            case AssignDiv:
              if (visit([](auto &&a) -> bool { return a == 0; }, result.value)) {
                error(expr, Diag::ConstexprDivisionByZero);
                return 0;
              }
              return visit(
                  [](auto &&a, auto &&b) -> ConstexprValue { return decay_t<decltype(a)>(a / b); },
                  var->value,
                  result.value);
            default:
              break;
          }
        }
      }
    }
    error(expr, Diag::UnrolledLoopInvalidExpression, var->identifier);
    return 0;
  }

  void for_loop(ForLoop stmt, SymbolScope &scope)
  {
    if (stmt.condition().attributes().contains_attr("unroll")) {
      loop_unroll(stmt, scope);
      return;
    }
    match_if(For);
    condition(stmt.condition(), scope, true, true);
    local_scope(stmt.body(), scope);
  }

  void while_loop(WhileLoop stmt, SymbolScope &scope)
  {
    match_if(While);
    condition(stmt.condition(), scope, false, true);
    local_scope(stmt.body(), scope);
  }

  void do_while_loop(DoWhileLoop stmt, SymbolScope &scope)
  {
    match_if(Do);
    local_scope(stmt.body(), scope);
    match_if(While);
    condition(stmt.condition(), scope, false, true);
    match_if(';');
  }

  void local_statement(LocalStmt stmt, SymbolScope &scope)
  {
    skip_node(stmt.attributes());

    if (auto [fn, call] = get_inlined_function(stmt.expr(), scope); fn) {
      inline_function(fn, call, scope);
      return;
    }

    builder.ss << expr(stmt.expr(), scope).str;
    builder.curr = stmt.expr().back().next();
    match_if(';');
  }

  void namespace_decl(ast::Namespace ns, SymbolScope &scope)
  {
    SymbolScope *sym = &scope;
    for (Id id : ns.identifier().children_of_type<Id>()) {
      sym = sym->scopes[string(id.str())];
    }
    skip_if(TokenType::Namespace);
    skip_node(ns.identifier());
    local_scope(ns.body(), *sym);
  }

  void enum_decl(ClassDecl decl, SymbolScope &scope)
  {
    SymbolScope &body_scope = *scope.scopes[string(decl.identifier().str())];
    SymbolClass *enum_cls = body_scope.as_class();
    LocalScope body = decl.body();

    IdQualified type = decl.parent_class();

    if (enum_cls) {
      /* TODO(fclem): Remove. Compatibility with previous BSL version. */
      builder.ss << "#define " << enum_cls->identifier << " " << type.str() << "\n";
    }

    jump_to(decl.front());

    /* Treat enum values as static variables. */
    for (EnumValue val : body.children_of_type<EnumValue>()) {
      line(val.front());
      string type_str = string("static constexpr ") + string(type.str());
      builder << type_str;
      /* Pretty align. */
      builder.ss << string(
          max(int64_t(1), int64_t(val.front().char_number()) - int64_t(type_str.size())), ' ');

      SymbolVariable *var = id_var_decl_resolved(val.identifier(), body_scope);
      builder << " = " + var->value_str();
      builder << string(";");
    }
  }

  void class_decl(ClassDecl decl,
                  SymbolScope &scope,
                  int nested_id = 0,
                  /* Resolved, templated class instance. */
                  SymbolClass *inst_cls = nullptr)
  {
    if (decl.is_enum()) {
      enum_decl(decl, scope);
      return;
    }

    SymbolClass *cls_ptr = inst_cls;
    if (!cls_ptr) {
      if (decl.identifier().is_valid()) {
        cls_ptr = scope.lookup_class(table, decl.identifier()).unwrap(this);
      }
      else {
        cls_ptr = scope.lookup_class("a" + to_string(nested_id));
      }
    }
    SymbolClass &cls = *cls_ptr;
    if (cls.is_error) {
      error(decl, Diag::UnknownClassInstantiation);
      return;
    }

    LocalScope body = decl.body();

    {
      /* First emit nested classes. */
      int class_id = 0;
      for (ClassDecl decl : body.children_of_type<ClassDecl>()) {
        class_decl(decl, cls, class_id);
        ++class_id;
      }
    }

    if (cls.is_srt()) {
      generate_srt_placeholder_macros(cls);
    }

    /* Emit static variables. */
    for (VarDecl decl : body.children_of_type<VarDecl>()) {
      if (decl.type().is_static()) {
        var_decl(decl, cls);
      }
    }

    /* Rollback to front. */
    jump_to(decl.front());

    /* Then emit the class itself. */
    builder << "struct" + trivia(builder.curr);

    skip_node(decl.attributes());

    if (cls.is_anonymous) {
      builder.ss << cls.identifier << " ";
    }
    else {
      builder << cls.identifier + trivia(decl.identifier());
    }

    builder.curr = body.front();
    match_if('{');

    if (cls.is_union) {
      union_members_decl(cls);
    }
    else {
      class_members_decl(decl, cls);
    }

    jump_to(body.back());
    match_if('}');
    match_if(';');

    /* Don't do host shared structures. */
    if (!decl.attributes().contains_attr("host_shared")) {
      line(body.front());
      builder.ss << class_default_constructor(decl, cls) + "\n";
    }

    if (cls.is_union) {
      builder.ss << "\n";
      NOTE(decl, Diag::NoteDeclarationUnionRequested, string(decl.identifier().str()));

      /* Emit getter and setters. */
      for (SymbolVariable *var : cls.non_static_variables_in_declaration_order()) {
        builder.ss << union_getter(cls, *var) + "\n";
        builder.ss << union_setter(cls, *var) + "\n";
      }
    }
    else {
      /* Emit function prototypes. */
      if (body.child_first(NodeType::FuncDecl).is_valid()) {
        /* Prototypes are not needed with MSL wrapper class. */
        builder.ss << "\n#ifndef GPU_METAL\n";
        for (FuncDecl decl : body.children_of_type<FuncDecl>()) {
          func_forward_decl(decl, cls);
        }
        builder.ss << "#endif\n";
      }
      /* Emit function members. */
      for (FuncDecl decl : body.children_of_type<FuncDecl>()) {
        func_decl(decl, cls);
      }
    }
  }

  void class_members_decl(ClassDecl decl, SymbolClass &cls)
  {
    LocalScope body = decl.body();
    /* Emit member variables. */
    int member_count = 0;
    int class_id = 0;
    for (Node node : body.children_range()) {
      switch (node.type()) {
        case NodeType::VarDecl: {
          VarDecl decl = node;
          if (!decl.type().is_static()) {
            member_count += var_decl(decl, cls);
          }
          break;
        }
        case NodeType::ClassDecl: {
          ClassDecl decl = node;
          if (decl.is_anonymous() && !decl.is_enum()) {
            /* Anonymous class are instantiated as regular members. */
            jump_to(decl.front());
            string id = "a" + to_string(class_id);
            id_type_resolved(id, cls);
            builder.ss << " " << id << "_;";
            member_count++;
          }
          ++class_id;
          break;
        }
        default:
          break;
      }
    }

    if (member_count == 0) {
      /* Add padding member. Empty class are invalid in GLSL. */
      builder.ss << "int _pad;";
    }

    if (cls.is_srt()) {
      parse_class_metadata(cls, body);
    }
  }

  void generate_srt_placeholder_macros(SymbolClass &cls)
  {
    string access_macros = "#pragma resource_access ";
    for (const auto &[k, v] : cls.variables) {
      const SymbolVariable &member = *v.second;
      if (member.type->is_srt()) {
        access_macros += " access_" + cls.identifier + "_" + member.identifier + "()";
        access_macros += " " + member.type->identifier + "_ctor_()";
      }
      else {
        access_macros += " access_" + cls.identifier + "_" + member.identifier + "()";
        access_macros += " " + member.identifier + "";
      }
    }

    builder.ss << "\n" + access_macros + "\n";
    builder.ss << "\n#pragma resource_defines " + cls.identifier + "\n";
  }

  string size_to_float_vec_type_str(size_t member_size)
  {
    if (member_size == 4) {
      return "float";
    }
    if (member_size == 8) {
      return "float2";
    }
    if (member_size == 12) {
      return "float3";
    }
    if (member_size == 16) {
      return "float4";
    }
    return "ERROR";
  }

  void union_members_decl(SymbolClass &cls)
  {
    for (int i = 0; i < cls.size; i += 16) {
      int member_size = min(16, cls.size - i);
      builder.ss << size_to_float_vec_type_str(member_size) << " _" << to_string(i / 16) << "; ";
    }
  }

  string union_ctor(SymbolClass &cls)
  {
    string members;
    for (int i = 0; i < cls.size; i += 16) {
      int member_size = min(16, cls.size - i);
      SymbolClass *type = cls.root_scope()->lookup_class(size_to_float_vec_type_str(member_size));
      members += "r._" + to_string(i / 16) + "=" + default_value(*type) + ";";
    }
    return members;
  }

  string default_value(const SymbolClass &type)
  {
    if (type.identifier == "float") {
      return "0.0f";
    }
    if (type.identifier == "uint" || type.identifier == "uchar") {
      return "0u";
    }
    if (type.identifier == "int" || type.identifier == "char") {
      return "0";
    }
    if (type.identifier == "bool") {
      return "false";
    }
    if (type.is_builtin()) {
      return type.identifier + "(0)";
    }
    return type.identifier + "_ctor_()";
  };

  /* Return temp variable name for a given index. */
  static string get_temp_name(size_t index)
  {
    string name;
    /* Shift to 1-based for the bijective base-26 math. */
    size_t n = index + 1;
    while (n > 0) {
      n--; /* Map 0-25 to 'a'-'z'. */
      name += char('a' + (n % 26));
      n /= 26;
    }
    return name;
  }

  string class_default_constructor(ClassDecl decl, SymbolClass &cls)
  {
    const string &cls_id = cls.identifier;

    string members;
    if (cls.is_union) {
      members = union_ctor(cls);
    }
    else {
      int class_id = 0;
      for (Node node : decl.body().children_range()) {
        switch (node.type()) {
          case NodeType::VarDecl: {
            VarDecl decl = node;
            if (!decl.type().is_static()) {
              SymbolClass *type = id_type_lookup_resolved(decl.type().identifier(), cls);
              for (Declarator d : decl.children_of_type<Declarator>()) {
                SymbolVariable *var = cls.lookup_variable(table, d.identifier());
                if (var->is_static) {
                  continue;
                }
                string access;
                string close;
                int dim = 0;
                Subscript sub = d.array().sub();
                while (sub.is_valid()) {
                  string var = get_temp_name(dim++);
                  string len = string(sub.expr().str());
                  members += "for(int " + var + " =0;" + var + " < " + len + ";++" + var + ") {";
                  close += "}";
                  access += "[" + var + "]";
                  sub = sub.next();
                }
                members += "r." + var->original + access + "=" + default_value(*type) + ";";
                members += close;
              }
            }
            break;
          }
          case NodeType::ClassDecl: {
            ClassDecl decl = node;
            if (decl.is_anonymous() && !decl.is_enum()) {
              /* Anonymous class are instantiated as regular members. */
              jump_to(decl.front());
              string type_id = "a" + to_string(class_id);
              SymbolClass *type = cls.lookup_class(type_id);
              string member_id = type_id + "_";

              members += "r." + member_id + "=" + default_value(*type) + ";";
            }
            ++class_id;
            break;
          }
          default:
            break;
        }
      }

      if (members.empty()) {
        /* Empty struct will have a padding int. */
        members += "r._pad=0;";
      }
    }

    return cls_id + " " + cls_id + "_ctor_() {" + cls_id + " r;" + members + "return r;}";
  }

  string union_data_access(const size_t member_offset,
                           const size_t member_size,
                           const size_t union_size)
  {
    string access = "_" + to_string(member_offset / 16);

    if (member_size == 12) {
      access += ".xyz";
    }
    else if (member_size == 8) {
      access += ((member_offset % 16) == 0) ? ".xy" : ".zw";
    }
    else if (member_size == 4) {
      switch (member_offset % 16) {
        case 0:
          /* Special case if last member is a scalar. */
          access += ((union_size - member_offset) == 4) ? "" : ".x";
          break;
        case 4:
          access += ".y";
          break;
        case 8:
          access += ".z";
          break;
        case 12:
          access += ".w";
          break;
      }
    }
    return access;
  }

  string member_from_float(const SymbolClass &member_type, const string &access)
  {
    /* Account for trivial types. */
    const string &type = member_type.identifier;

    if (type.starts_with("uint")) {
      return "floatBitsToUint(" + access + ")";
    }
    if (type.starts_with("int")) {
      return "floatBitsToInt(" + access + ")";
    }
    if (type == "bool32_t") {
      return access + " != 0";
    }
    return access;
  }

  string member_to_float(const SymbolClass &member_type, const string &access)
  {
    /* Account for trivial types. */
    const string &type = member_type.identifier;

    if (type.starts_with("uint")) {
      return "uintBitsToFloat(" + access + ")";
    }
    if (type.starts_with("int")) {
      return "intBitsToFloat(" + access + ")";
    }
    if (type == "bool32_t") {
      return "intBitsToFloat(int(" + access + "))";
    }
    return access;
  }

  string union_getter(SymbolClass &union_type, SymbolVariable &member)
  {
    SymbolClass &member_type = *member.type;
    string union_type_id = union_type.identifier;
    string fn_type_id = member_type.identifier;
    string fn_name = "_" + member.original;
    string fn_args = "(" + union_type_id + " this_)";
    string fn_body = "{\n";
    if (member_type.is_builtin()) {
      if (member.array_dimensions > 0) {
        error(member.loc.tok, Diag::UnionArrayUnsupported);
      }
      string access = "this_." +
                      union_data_access(member.offset, member_type.size, union_type.size);
      fn_body += "  return " + member_from_float(*member.type, access) + ";\n";
    }
    else {
      /* Declare return variable of the same type as the accessed member. */
      fn_body += "  " + fn_type_id + " r;\n";
      for (const auto &var : member_type.get_all_members_flat().unwrap(this)) {
        string to_var = "r." + var.path;
        string access = "this_." +
                        union_data_access(var.absolute_offset, var.type->size, union_type.size);
        fn_body += "  " + to_var + " = " + member_from_float(*var.type, access) + ";\n";
      }
      fn_body += "  return r;\n";
    }
    fn_body += "}";
    return fn_type_id + " " + fn_name + fn_args + " " + fn_body;
  };

  string union_setter(SymbolClass &union_type, SymbolVariable &member)
  {
    SymbolClass &member_type = *member.type;
    string union_type_id = union_type.identifier;
    string member_type_id = member_type.identifier;
    string fn_name = "_" + member.original + "_set_";
    string fn_args = "(" + union_type_id + " &this_, " + member_type_id + " v)";
    string fn_body = "{\n";
    if (member_type.is_builtin()) {
      if (member.array_dimensions > 0) {
        error(member.loc.tok, Diag::UnionArrayUnsupported);
      }
      string to_var = "this_." +
                      union_data_access(member.offset, member_type.size, union_type.size);
      string access = "v." + member.original;
      fn_body += "  " + to_var + " = " + member_to_float(*member.type, access) + ";\n";
    }
    else {
      for (const auto &var : member_type.get_all_members_flat().unwrap(this)) {
        string to_var = "this_." +
                        union_data_access(var.absolute_offset, var.type->size, union_type.size);
        string access = "v." + var.path;
        fn_body += "  " + to_var + " = " + member_to_float(*var.type, access) + ";\n";
      }
    }
    fn_body += "}";
    return "void " + fn_name + fn_args + " " + fn_body;
  };

  void func_forward_decl(FuncDecl decl, SymbolScope &scope)
  {
    string id(decl.identifier().str());
    if (auto it = scope.functions.find(id); it != scope.functions.end()) {
      SymbolScope &body_scope = *it->second.second;
      // jump_to(decl.front()); /* Adds too many directives. */
      builder.curr = decl.front();
      skip_node(decl.attributes());
      skip_if(Static);
      id_type(decl.return_type(), scope);
      id_func_resolved(decl.identifier(), decl.arguments(), scope, scope);
      func_arg_list(decl.arguments(), body_scope, false, true);
      builder << string(";\n");
    }
    else {
      assert(0);
    }
  }

  string condition(SymbolScope *scope)
  {
    string str;
    for (auto &[k, var] : scope->variables) {
      if (var.second->type->is_srt() &&
          var.second->type->srt_type != ResourceTableType::VERTEX_OUT)
      {
        str += " && defined(CREATE_INFO_" + var.second->type->identifier + ")";
      }
    }
    return str.empty() ? str : str.substr(4);
  }

  string condition(SymbolFunction *fn)
  {
    string str;
    if (fn->is_entry_point()) {
      str += " && defined(ENTRY_POINT_" + fn->identifier + ")";
    }
    if (string scope_str = condition(static_cast<SymbolScope *>(fn)); !scope_str.empty()) {
      str += " && " + scope_str;
    }
    return str.empty() ? str : str.substr(4);
  }

  void func_decl(FuncDecl decl,
                 SymbolScope &scope,
                 /* Resolved, templated function instance. */
                 SymbolFunction *inst_fn = nullptr)
  {
    LocalScope body = decl.body();

    if (inst_fn == nullptr) {
      inst_fn = id_func_resolved_lookup(decl.identifier(), decl.arguments(), scope, scope);
    }

    if (inst_fn->is_inline) {
      /* Do not emit inline function. */
      return;
    }

    if (inst_fn->fn_type == SymbolFunction::MEMBER && decl.is_template()) {
      if (SymbolClass *cls = inst_fn->parent_class(); cls) {
        /* Leave a breadcrum for a mutation pass which will move the following forward declaration
         * all the way up at the class declaration. This is needed to allow other member functions
         * to call the templated function. */
        builder.ss << "#pragma member_forward_decl " << inst_fn->parent_class()->identifier
                   << "\n";
        builder.ss << inst_fn->return_type->identifier << " " << inst_fn->identifier;
        builder.curr = decl.arguments().front();
        func_arg_list(decl.arguments(), *inst_fn, false);
        builder.ss << ";\n";
      }
      else {
        error(decl, Diag::CompilerErrorMemberFuncNoClass);
      }
    }

    string cond = condition(inst_fn);
    if (!cond.empty()) {
      builder.ss << string("#if ") + cond + "\n";
    }

    jump_to(decl.front());
    skip_node(decl.attributes());
    skip_if(Static);
    /* Note that we match the id type from inside the function scope in order to match template
     * argument aliases.  */
    id_type(decl.return_type(), *inst_fn);

    builder.curr = decl.identifier().back();
    builder << inst_fn->identifier + trivia(decl.identifier());

    if (inst_fn->is_entry_point()) {
      builder.curr = decl.arguments().back();
      builder << string("()\n");
    }
    else {
      func_arg_list(decl.arguments(), *inst_fn);
    }

    local_scope(body, *inst_fn);

    if (!cond.empty()) {
      builder.ss << string("#endif\n");
    }

    if (inst_fn->is_entry_point()) {
      entry_point_arg_list_metadata(decl, *inst_fn);
    }
  }

  pair<SymbolFunction *, FuncCall> get_inlined_function(Expr expr, SymbolScope &scope)
  {
    FuncCall call(expr.child_first());
    if (!call.is_valid()) {
      return {nullptr, {}};
    }
    if (!expr.has_single_child() || expr.parent().parent().type() == NodeType::Condition) {
      return {nullptr, {}};
    }

    SymbolFunction *fn = id_func_resolved_lookup(
        call.identifier(), call.parameters(), scope, scope);
    if (!fn || !fn->is_inline) {
      return {nullptr, {}};
    }
    return {fn, call};
  }

  vector<SymbolFunction *> inlining_stack;

  string inline_function(SymbolFunction *fn, FuncCall call, SymbolScope &scope)
  {
    string uuid = to_string(call.front().index_);
    string ret_val = "_r" + uuid;

    if (std::ranges::find(inlining_stack, fn) != inlining_stack.end()) {
      error(call, Diag::InliningRecursive);
      return "";
    }

    inlining_stack.push_back(fn);
    NOTE(call, Diag::NoteInInliningOf, fn->original);

    jump_to(fn->decl.front());

    /* Communicate the name of the return variable to the return statements. */
    if (auto it = fn->variables.find(SymbolFunction::inline_fn_ret_id); it != fn->variables.end())
    {
      SymbolVariable *var = it->second.second;
      var->identifier = ret_val;
      /* Create return variable if any. */
      builder.ss << fn->return_type->identifier << " " << ret_val << ";\n";
    }

    /* Create local scope. */
    builder.ss << "do {\n";
    jump_to(fn->decl.front());

    /* Declare argument copies. */
    int arg_i = 0;
    Expr param = call.parameters().child_first();
    vector<string> original_identifiers;
    for (FuncArg arg : fn->decl.arguments().children_range()) {
      SymbolVariable *var = fn->lookup_variable(table, arg.identifier());

      if (!param.is_valid() && (arg.is_reference() || !fn->arg_defaults[arg_i].is_valid())) {
        error(call, Diag::MissingParameterForCall, var->identifier);
        continue;
      }

      original_identifiers.emplace_back(var->identifier);

      if (arg.is_reference() || var->type->is_opaque) {
        /* Check if reference has no side effect. */
        table.expr_type_analysis(scope, param.child_first(), true).unwrap(this);
        var->identifier = expr(param, scope).str;
      }
      else {
        /* Make parameter name unique to avoid shadowing. */
        string id = "_" + string(arg.identifier().str()) + uuid;
        /* Change the mangled name of the argument. */
        var->identifier = id;

        Expr init_expr = (param.is_valid()) ? param : fn->arg_defaults[arg_i];
        builder.ss << fn->arg_types[arg_i]->identifier << " " << id;
        builder.ss << " = " << expr(init_expr, scope).str << "; ";
      }
      param = param.next();
      ++arg_i;
    }
    builder.ss << "\n";

    local_scope(fn->decl.body(), *fn);

    /* Create argument assignments. */
    builder.ss << "} while (false);\n";

    inlining_stack.pop_back();

    {
      /* Restore argument identifiers. */
      int arg_i = 0;
      for (FuncArg arg : fn->decl.arguments().children_range()) {
        SymbolVariable *var = fn->lookup_variable(table, arg.identifier());
        var->identifier = original_identifiers[arg_i++];
      }
    }

    return ret_val;
  }

  void entry_point_arg_list_metadata(FuncDecl decl, SymbolFunction &fn)
  {
    /* For now, just emit good old create info macros. */
    string create_info_decl;

    for (Attr attr : decl.attributes().children_of_type<Attr>()) {
      string_view id = attr.identifier().str();
      if (id == "clip_control") {
        create_info_decl += "BUILTINS(BuiltinBits::CLIP_CONTROL)\n";
      }
      else if (id == "texture_atomic") {
        create_info_decl += "BUILTINS(BuiltinBits::TEXTURE_ATOMIC)\n";
      }
      else if (id == "early_fragment_tests") {
        create_info_decl += "EARLY_FRAGMENT_TEST(true)\n";
      }
      else if (id == "local_size") {
        create_info_decl += "LOCAL_GROUP_SIZE" + string(attr.parameters().str()) + "\n";
      }
      else if (id == "metal_max_total_threads_per_threadgroup") {
        create_info_decl += "MTL_MAX_TOTAL_THREADS_PER_THREADGROUP" +
                            string(attr.parameters().str()) + "\n";
      }
    }

    for (FuncArg arg : decl.arguments().children_of_type<FuncArg>()) {
      auto attr = resource_type_from_attributes(arg.attributes()).unwrap(this);

      SymbolVariable *var = fn.lookup_variable(table, arg.identifier());

      const string &resolved_type = var->type->identifier;

      switch (attr.res_type) {
        case ResourceType::VERTEX_ID:
        case ResourceType::INSTANCE_ID:
        case ResourceType::POINT_SIZE:
        case ResourceType::CLIP_DISTANCES:
        case ResourceType::LAYER:
        case ResourceType::VIEWPORT_INDEX:
        case ResourceType::FRAG_COORD:
        case ResourceType::POINT_COORD:
        case ResourceType::FRONT_FACING:
        case ResourceType::GLOBAL_INVOCATION_ID:
        case ResourceType::LOCAL_INVOCATION_ID:
        case ResourceType::LOCAL_INVOCATION_INDEX:
        case ResourceType::WORK_GROUP_ID:
        case ResourceType::BASE_INSTANCE:
        case ResourceType::NUM_WORK_GROUP:
        case ResourceType::INSTANCE_INDEX:
        case ResourceType::FRAG_STENCIL_REF:
          create_info_decl += "BUILTINS(BuiltinBits::" + to_str(attr.res_type) + ")\n";
          break;
        case ResourceType::FRAG_DEPTH:
          for (Attr attr : arg.attributes().children_of_type<Attr>()) {
            string_view id = attr.identifier().str();
            if (id == "frag_depth") {
              create_info_decl += "DEPTH_WRITE(DepthWrite::" +
                                  to_uppercase(string(attr.parameters().splat_1().arg1.str())) +
                                  ")\n";
            }
          }
          break;
        case ResourceType::RESOURCE_TABLE: {
          string res_condition_lambda = parse_condition(arg.attributes());
          if (res_condition_lambda.empty()) {
            create_info_decl += "ADDITIONAL_INFO(" + resolved_type + ")\n";
          }
          else {
            create_info_decl += ".additional_info_with_condition(\"" + resolved_type + "\"" +
                                res_condition_lambda + ")\n";
          }
          break;
        }
        case ResourceType::SUBPASS_IN:
        case ResourceType::OUT:
        case ResourceType::IN:
          if (var->type->srt_type == ResourceTableType::VERTEX_OUT) {
            create_info_decl += "VERTEX_OUT(" + resolved_type + "_t)\n";
          }
          else {
            create_info_decl += "ADDITIONAL_INFO(" + resolved_type + ")\n";
          }
          break;
        case ResourceType::POSITION:
        case ResourceType::LEGACY_INFO:
        case ResourceType::LEGACY_IFACE:
        case ResourceType::COMPILATION_CONST:
        case ResourceType::SPECIALIZATION_CONST:
        case ResourceType::PUSH_CONST:
        case ResourceType::SAMPLER:
        case ResourceType::IMAGE:
        case ResourceType::UNIFORM_BUF:
        case ResourceType::STORAGE_BUF:
        case ResourceType::ACCELERATION_STRUCTURE:
        case ResourceType::SHARED:
        case ResourceType::VERT_ATTR_IN:
        case ResourceType::VERT_ATTR_OUT:
        case ResourceType::FRAG_IN:
        case ResourceType::FRAG_OUT:
        case ResourceType::CLIP_CONTROL:
        case ResourceType::CONDITION:
        case ResourceType::FREQUENCY:
        case ResourceType::DUAL_SOURCE_INDEX:
        case ResourceType::RASTER_ORDER_GROUP:
        case ResourceType::NONE:
          break;
      }
    }

    if (create_info_decl.empty()) {
      create_info_decl += "DEFINE(\"EMPTY_CREATE_INFO\")\n";
    }

    create_info_decl = "GPU_SHADER_CREATE_INFO(" + fn.identifier + "_infos_)\n" +
                       create_info_decl + "GPU_SHADER_CREATE_END()\n";
    metadata.create_infos_declarations.emplace_back(create_info_decl);
  }

  string parse_condition(AttrList attributes)
  {
    string cond;
    for (Attr attr : attributes.children_of_type<Attr>()) {
      if (attr.identifier().str() == "condition") {
        if (!cond.empty()) {
          error(attr.identifier(), Diag::MultipleConditionAttributes);
          break;
        }
        for (LocalVar var : attributes.children_of_type<LocalVar>()) {
          string id = string(var.identifier().str());
          cond += "int " + id + " = ShaderCreateInfo::find_constant(constants, \"" + id + "\"); ";
        }
        cond += "return " + string(attr.parameters().str()) + ";";
      }
    }
    if (!cond.empty()) {
      cond = ", [](blender::Span<CompilationConstant> constants) { " + cond + "}";
    }
    return cond;
  }

  void func_arg_list(FuncArgList list,
                     SymbolScope &scope,
                     bool with_trivia = true,
                     bool no_arg_name = false)
  {
    match_if('(');

    FuncDecl decl = list.parent();
    if (decl.is_method() && !decl.is_static()) {
      ClassDecl cls = decl.parent_class();
      if (decl.is_const()) {
        builder.ss << "const ";
      }
      SymbolClass *cls_resolved = id_type_resolved(cls.identifier(), scope);
      if (cls_resolved->is_srt()) {
        /* WORKAROUND: Do not pass SRT by reference otherwise we get a compilation error on metal
         * we cannot bind to a temporary when the caller is just calling a constructor as the
         * parameter. */
        builder.ss << "this_";
      }
      else {
        builder.ss << "&this_";
      }

      if (!list.is_empty()) {
        builder.ss << ", ";
      }
      builder.curr = list.front().next();
    }

    if (builder.curr != ')') {
      int i = 0;
      for (FuncArg arg : list.children_of_type<FuncArg>()) {
        skip_node(arg.attributes());
        id_type(arg.type(), scope);
        declarator(arg.declarator(), scope, true, no_arg_name ? i++ : -1);
        match_if(',');
      }
    }

    if (with_trivia) {
      match_if(')');
      skip_if(TokenType::Const);
    }
    else {
      builder << string(")");
      if (builder.curr == TokenType::Const) {
        builder.curr.next();
      }
    }
  }

  void template_inst(TemplateInst temp_decl, SymbolScope &scope)
  {
    if (temp_decl.is_class()) {
      ClassDecl decl = temp_decl.decl();
      auto *base_cls = scope.lookup_class_base(table, decl.identifier());
      /* Instantiation should have been checked already. */
      auto [temp_cls, full_id, _] = base_cls->template_data->lookup_inst(
          temp_decl.parameters(), scope, table);
      NOTE(decl.identifier(), Diag::NoteInstantiationClassTemplateRequested, full_id);
      Node decl_tmp = base_cls->template_data->decl.decl();
      class_decl(decl_tmp, scope, 0, temp_cls);
    }
    else {
      FuncForwardDecl decl = temp_decl.decl();
      auto *base_fn = scope.lookup_function_base(table, decl.identifier());
      /* Instantiation should have been checked already. */
      auto [temp_fn, full_id, _] = base_fn->template_data->lookup_inst(
          temp_decl.parameters(), scope, table);
      NOTE(decl.identifier(), Diag::NoteInstantiationFuncTemplateRequested, full_id);
      Node decl_tmp = base_fn->template_data->decl.decl();
      func_decl(decl_tmp, *temp_fn->parent, temp_fn);
    }
  }

  void template_spec(TemplateSpec temp_spec, SymbolScope &scope)
  {
    if (temp_spec.is_class()) {
      ClassDecl decl = temp_spec.decl();
      auto *base_cls = scope.lookup_class_base(table, decl.identifier());
      /* Instantiation should have been checked already. */
      auto [temp_cls, full_id, _] = base_cls->template_data->lookup_inst(
          temp_spec.parameters(), scope, table);
      NOTE(decl.identifier(), Diag::NoteInstantiationClassTemplateRequested, full_id);
      class_decl(decl, scope, 0, temp_cls);
    }
    else {
      FuncDecl decl = temp_spec.decl();
      auto *base_fn = scope.lookup_function_base(table, decl.identifier());
      /* Instantiation should have been checked already. */
      auto [temp_fn, full_id, _] = base_fn->template_data->lookup_inst(
          temp_spec.parameters(), scope, table);
      NOTE(decl.identifier(), Diag::NoteInstantiationFuncTemplateRequested, full_id);
      func_decl(decl, *temp_fn->parent, temp_fn);
    }
  }

  /* Match if/else if/else statements */
  void if_statement(Node decl,
                    SymbolScope &parent_scope,
                    int &local_scope_id,
                    bool preceeded_by_true_if_constexpr = false,
                    bool preceeded_by_regular_if = false)
  {
    SymbolScope *scope = parent_scope.child_scope(local_scope_id++);
    if (scope == nullptr) {
      error(decl, Diag::CompilerErrorChildScopeNotFound);
      return;
    }

    jump_to(decl.front());

    Condition cond(decl.child_first());

    bool is_constexpr = cond.is_valid() && cond.front().prev() == Constexpr;

    bool constexpr_value = true;
    if (cond.is_valid()) {
      LocalStmt stmt = cond.child_first();
      auto result = table.expr_type_analysis(*scope, stmt.expr().child_first()).unwrap(this);

      constexpr_value = value_as<int>(result.value) != 0;

      if (is_constexpr && !result.is_constexpr()) {
        /* Report error if expression couldn't be evaluated. */
        error(stmt, Diag::ConstexprIfConditionNotConstexpr);
      }
      else if (result.is_constexpr()) {
        /* Expression was evaluated successfully. Treat the statement as constexpr. */
        is_constexpr = true;
      }

      if (cond.attributes().contains_attr("static_branch")) {
        error(decl, Diag::StaticBranchMissingPrevious);
      }
    }

    /* Generate a 'else' statement only if there is a non-constexpr if above in the chain,
     * and if no 'if constexpr' evaluated to true above in the chain,
     * and if not a 'if constexpr' itself. */
    if (preceeded_by_regular_if && !preceeded_by_true_if_constexpr &&
        (!is_constexpr || constexpr_value == true))
    {
      match_if(Else);
    }
    else {
      skip_if(Else);
    }

    /* Generate a if statement only if this is not a 'if constexpr',
     * and if no 'if constexpr' evaluated to true above in the chain. */
    if (!is_constexpr && !preceeded_by_true_if_constexpr) {
      match_if(If);
      skip_if(Constexpr);
      if (cond.is_valid()) {
        condition(cond, *scope, false, true);
      }
    }
    else {
      skip_if(If);
      skip_if(Constexpr);
      skip_node(cond);
    }

    LocalScope body = decl.child_last(NodeType::LocalScope);

    if (!preceeded_by_true_if_constexpr && (!is_constexpr || constexpr_value == true)) {
      local_scope(body, *scope);
    }

    /* Process preprocessor directive that can be added between if statements. */
    Node node = body.next();
    while (node.type() == NodeType::Preprocessor) {
      builder << node;
      node = node.next();
    }

    Node next = decl.next();
    if (next.type() == NodeType::ElseIfStmt || next.type() == NodeType::ElseStmt) {
      if_statement(next,
                   parent_scope,
                   local_scope_id,
                   preceeded_by_true_if_constexpr || (is_constexpr && constexpr_value == true),
                   preceeded_by_regular_if || !is_constexpr);
    }
    else {
      jump_to(next.front());
    }
  }

  void static_if_statement(Node decl,
                           SymbolScope &parent_scope,
                           int &local_scope_id,
                           bool first = true)
  {
    SymbolScope *scope = parent_scope.child_scope(local_scope_id++);
    if (scope == nullptr) {
      error(decl, Diag::CompilerErrorChildScopeNotFound);
      return;
    }

    Condition cond(decl.child_first());

    if (cond.is_valid()) {
      if (!cond.attributes().contains_attr("static_branch")) {
        error(decl, Diag::StaticBranchRequiredByPrevious);
      }
      if (cond.front().prev() == Constexpr) {
        error(decl, Diag::StaticBranchConstexprIfNotAllowed);
      }
      LocalStmt stmt(cond.child_first());
      int unused_count = 0;
      /* Checks that condition is made of compilation constants or constexpr. */
      auto cond_str = table
                          .expr_to_string(
                              parent_scope, stmt.expr().child_first(), unused_count, true)
                          .unwrap(this);
      builder.ss << (first ? "#if " : "#elif ") + cond_str + "\n";
    }
    else {
      builder.ss << "#else\n";
    }

    skip_if(If);
    skip_if(Constexpr);
    skip_node(cond);

    LocalScope body = decl.child_last(NodeType::LocalScope);
    jump_to(body.front());

    local_scope(body, *scope);

    /* Process preprocessor directive that can be added between if statements. */
    Node node = body.next();
    while (node.type() == NodeType::Preprocessor) {
      builder << node;
      node = node.next();
    }

    jump_to(body.back().next());

    Node next = decl.next();
    if (next.type() == NodeType::ElseIfStmt || next.type() == NodeType::ElseStmt) {
      static_if_statement(next, parent_scope, local_scope_id, false);
    }
    else {
      /* Pretty align. */
      builder.ss << string(decl.front().char_number(), ' ') + "#endif\n";
      jump_to(next.front());
    }
  }

  void condition(Condition cond, SymbolScope &scope, bool is_for_loop, bool do_implicit_conversion)
  {
    match_if('(');
    for (Node child : cond.children_range()) {
      if (child.type() == NodeType::VarDecl) {
        var_decl(child, scope, false);
      }
      else if (child.type() == NodeType::LocalStmt) {
        local_statement(child, scope);

        /* In for loops, only the center statement is a condition. */
        if (do_implicit_conversion &&
            (!is_for_loop || (child.prev().is_valid() && child.next().is_valid())))
        {
          Node start = LocalStmt(child).expr().child_first();
          auto *type = table.expr_type_analysis(scope, start).unwrap(this).type;
          if (type == table.int_cls) {
            builder.ss << "!=0";
          }
          else if (type == table.uint_cls) {
            builder.ss << "!=0u";
          }
          else if (type != table.bool_cls && type != table.bool32_t_cls) {
            error(child, Diag::TypeNotConvertibleToBool, type->original);
          }
        }
      }
      else {
        /* TODO: structured bindings. */
        assert(0);
      }
      match_if(';');
    }
    match_if(')');
    skip_node(AttrList(cond.next()));
  }

  CodegenResult expr(Expr node, const SymbolScope &scope)
  {
    assert(node.is_valid());
    return CodegenExpressionParser(*this, scope, node.child_first()).eval();
  }

  /* Return true if declaration was issued. */
  bool var_decl(VarDecl decl, SymbolScope &scope, bool jump = true)
  {
    auto attr = resource_type_from_attributes(decl.attributes()).unwrap(this);

    if (attr.res_type != ResourceType::NONE) {
      /* Resources are defined in global space at runtime. */
      skip_node(decl);
      return false;
    }

    if (scope.as_class() == nullptr && decl.is_reference()) {
      /* Local references are simply pasted. */
      skip_node(decl);
      return false;
    }

    if (jump) {
      jump_to(decl.front());
    }

    skip_node(decl.attributes());

    if (decl.type().identifier().str() == "auto") {
      Declarator d = decl.child_first(NodeType::Declarator);
      auto *cls = table.resolve_auto_type(scope, d).unwrap(this);
      builder << cls->identifier + trivia(decl.type().identifier());
    }
    else {
      id_type(decl.type(), scope);
    }
    for (Declarator d : decl.children_of_type<Declarator>()) {
      declarator(d, scope);
      match_if(',');
    }
    match_if(';');
    return true;
  }

  void id_type(IdType type, SymbolScope &scope)
  {
    match_if(Static);
    match_if(TokenType::Const, Constexpr);
    skip_if(Struct, Class, Enum);
    id_type_resolved(type.identifier(), scope);
  }

  void array_decl(ArrayDecl array, Declarator decl, SymbolScope &scope)
  {
    if (!array.is_valid()) {
      return;
    }

    if (array.dimensions() == 1) {
      if (array.sub().is_empty()) {
        AssignStmt stmt = decl.initial_value();
        InitializerList list = stmt.initializer_list();
        if (!list.is_valid()) {
          error(decl, Diag::ArrayNeedsSizeOrInitializer);
        }
        int size = list.child_count();
        builder << builder.curr;
        builder.ss << to_string(size);
        builder << builder.curr;
        if (size == 0) {
          error(decl, Diag::ArraySizeMustBeGreaterThanZero);
        }
        return;
      }
    }

    for (Subscript sub : array.children_of_type<Subscript>()) {
      if (sub.is_empty()) {
        error(decl, Diag::ArrayMultidimensionalImplicitSize);
      }
      else {
        auto [result, err] = table.expr_type_analysis(scope, sub.expr().child_first());
        /* Note: Do not report error. The size might be defined by compilation constant. */
        if (!err && result.is_constexpr()) {
          if (value_as<uint32_t>(result.value) == 0) {
            error(decl, Diag::ArraySizeMustBeGreaterThanZero);
          }
        }
      }
      builder << sub;
    }
  }

  /* If skip_name_match_index is not -1, will not try to match the declared argument name and
   * declare empty . */
  void declarator(Declarator decl,
                  SymbolScope &scope,
                  bool skip_value = false,
                  int skip_name_match_index = -1)
  {
    ArrayDecl array = decl.array();
    SymbolVariable *var = scope.lookup_variable(table, decl.identifier());

    const bool par = match_if('(');
    if (var->type->is_srt()) {
      /* WORKAROUND: Do not pass SRT by reference because it causes issue on metal when the caller
       * is just calling the constructor in place. */
      skip_if('&');
    }
    else {
      match_if('&');
    }

    if (skip_name_match_index != -1) {
      /* Do not try to match the identifier for forward declarations. */
      builder << "_" + to_string(skip_name_match_index);
    }
    else {
      id_var_decl_resolved(decl.identifier(), scope);
    }

    if (par) {
      match_if(')');
    }

    array_decl(array, decl, scope);

    if (!var->is_error && var->is_constexpr) {
      builder.curr = decl.back();
      builder << "= " + var->value_str();
      return;
    }

    if (skip_value) {
      skip_node(decl.initial_value());
    }
    else if (decl.initial_value().is_valid()) {
      assignment(decl.initial_value(), scope, var);
    }
    else if (decl.initializer_list().is_valid()) {
      builder.ss << "=" + initializer_list(
                              decl.initializer_list(), scope, var->type, array.dimensions())
                              .str;
      builder.curr = decl.back().next();
    }
  }

  void assignment(AssignStmt stmt, SymbolScope &scope, SymbolVariable *var)
  {

    if (auto [fn, call] = get_inlined_function(stmt.expr(), scope); fn) {
      /* Check if this part of a declarator sequence before inlining. */
      if (Declarator d(stmt.parent()), d_next(d.next()); d_next.is_valid()) {
        /* Could be supported, but keep it simple for now. Continuing will trigger an error. */
      }
      else {
        /* Need to stop the declaration here. */
        builder.ss << ";";
        string val = inline_function(fn, call, scope);
        /* Resume. */
        jump_to(var->loc.tok);
        builder.ss << var->identifier << " = " << val;
        builder.curr = stmt.back().next();
        return;
      }
    }

    match_if('=');
    builder.ss << init_expression_or_initializer_list(
                      stmt.child_first(), scope, var->type, var->array_dimensions)
                      .str;
    builder.curr = stmt.back().next();
  }

  CodegenResult init_expression_or_initializer_list(Node node,
                                                    const SymbolScope &scope,
                                                    SymbolClass *cls,
                                                    int array_dim = 0)
  {
    if (node == NodeType::InitializerList) {
      return initializer_list(node, scope, cls, array_dim);
    }
    if (node == NodeType::Expr) {
      return expr(node, scope);
    }
    assert(0);
    return {"", cls};
  }

  CodegenResult initializer_list(InitializerList list,
                                 const SymbolScope &scope,
                                 SymbolClass *cls,
                                 int array_dim = 0)
  {
    const bool is_empty = list.is_empty();
    if (array_dim > 0) {
      /* Array initializer is the only place where we don't need to prepend the type. */
    }
    else if (cls == nullptr || cls->is_error) {
      /* TODO(fclem): Long term, we can lookup what is the expected type of the expression and pass
       * it down the initializer tree. */
      error(list, Diag::InitializerListRequiresExplicitType);
      return {"", cls};
    }

    string open, close;
    if (array_dim > 0) {
      /* Only use aggregate syntax for arrays. */
      open = '{', close = '}';
    }
    else if (cls->is_builtin()) {
      /* Use constructor syntax for builtin types. */
      open = cls->identifier + "(", close = ')';
    }
    else if (is_empty) {
      /* Call default constructor. */
      if (cls->is_builtin()) {
        open = cls->identifier + "(0", close = ')';
      }
      else {
        open = cls->identifier + "_ctor_(", close = ')';
      }
    }
    else {
      /* NOTE: Converting to `_ctor(id) param _rotc()` messes with the core syntax and prevent
       * using AST for the next lowering passes. Instead convert to _ctor(id, params) and lower it
       * to final macro later. */
      open = "_ctor(" + cls->identifier + ",";
      close = ")";
    }
    open += trivia(list.front());
    close += trivia(list.back());

    string content;
    DesignatedInitializer designated(list.child_first());
    if (designated.is_valid()) {
      if (array_dim > 0) {
        error(designated, Diag::ExpectedArrayMemberInitializer);
      }
      /* Iterate over the list of members, zero initializing the omitted members, until we find
       * the given initializer. If member is out of order or not existing, raise an error. */
      for (SymbolVariable *var : cls->non_static_variables_in_declaration_order()) {
        if (designated.is_valid() && var->identifier == designated.identifier().str()) {
          AssignStmt stmt = designated.assign();
          content += init_expression_or_initializer_list(stmt.child_first(), scope, var->type).str;
          content += opt_str(designated.back().next(), Comma);
          designated = designated.next();
        }
        else {
          content += default_value(*var->type) + ",";
        }
      }

      if (designated.is_valid()) {
        if (auto *var = cls->lookup_variable(string(designated.identifier().str()));
            var && !var->is_error)
        {
          error(designated, Diag::FieldDesignatorOutOfOrder, designated.identifier().str());
        }
        else {
          error(designated,
                Diag::FieldDesignatorNotFound,
                designated.identifier().str(),
                cls->identifier);
        }
      }
    }
    else {
      for (Node child : list.children_range()) {
        content += init_expression_or_initializer_list(child.child_last(),
                                                       scope,
                                                       array_dim > 0 ? cls : nullptr /* TODO */,
                                                       array_dim - 1)
                       .str;
        content += opt_str(child.back().next(), Comma);
      }
    }
    return {open + content + close, cls};
  }

  string_view opt_str(Token tok, TokenType type)
  {
    return tok == type ? tok.str_with_whitespace() : string_view();
  }

  SymbolClass *id_type_lookup_resolved(IdQualified id, const SymbolScope &scope)
  {
    assert(id.is_valid());
    auto *cls = scope.lookup_class(table, id).unwrap(this);
    if (cls->template_data && !id.template_params().is_valid()) {
      error(id, Diag::TemplateMissingExplicitArguments);
    }
    else if (cls->is_error) {
      error(id, Diag::UnknownTypeNameWithArg);
    }
    return cls;
  }

  SymbolClass *id_type_resolved(IdQualified id, const SymbolScope &scope)
  {
    auto *cls_resolved = id_type_lookup_resolved(id, scope);
    builder.curr = id.back();
    builder << cls_resolved->identifier + trivia(id);
    return cls_resolved;
  }

  /* Lookup only in the give scope. */
  SymbolClass *id_type_resolved(string id, const SymbolScope &scope)
  {
    auto it = scope.classes.find(id);
    SymbolClass *cls = it->second.second;
    builder.ss << cls->identifier;
    return cls;
  }

  template<typename ArgOrParamList>
  SymbolFunction *id_func_resolved_lookup(IdQualified id,
                                          ArgOrParamList params,
                                          const SymbolScope &scope,
                                          const SymbolScope &param_scope,
                                          optional<SourceLocation> loc = nullopt)
  {
    assert(id.is_valid());
    return scope.lookup_function(table, id, params, param_scope, loc).unwrap(this);
  }

  template<typename ArgOrParamList>
  SymbolFunction *id_func_resolved(IdQualified id,
                                   ArgOrParamList params,
                                   const SymbolScope &scope,
                                   const SymbolScope &param_scope,
                                   optional<SourceLocation> loc = nullopt)
  {
    SymbolFunction *func = id_func_resolved_lookup(id, params, scope, param_scope, loc);
    builder.curr = id.back();
    builder << func->identifier + trivia(id);
    return func;
  }

  /* Resolve a variable in an expression. */
  SymbolVariable *id_var_resolved(IdQualified id, const SymbolScope &scope)
  {
    assert(id.is_valid());
    SymbolVariable *var = scope.lookup_variable(table, id);
    if (var->is_error) {
      error(id, Diag::UnknownVariable, string(id.str()));
      builder << id;
      return var;
    }

    if (var->is_constexpr) {
      builder.ss << var->value_str() + trivia(id);
      return var;
    }

    if (var->type->is_srt()) {
      if (id.back().next() == TokenType::Dot && id.parent().next().next() != NodeType::FuncCall) {
        /* Remove access to SRT type (makes the resource global). */
        skip_node(id);
      }
      else {
        /* SRT type is used in an expression (likely as function argument). Call the ctor. */
        builder << var->type->identifier + "_ctor_()" + trivia(id);
      }
      return var;
    }

    if (var->reference_value.is_valid()) {
      builder << string(var->reference_value.str()) + trivia(id);
      return var;
    }

    bool preceded_by_dot = id.front().prev() == '.';
    if (!var->is_static && !preceded_by_dot && var->parent->as_class() != nullptr) {
      builder.ss << "this_.";
    }

    builder.curr = id.back();
    builder << var->identifier + trivia(id);
    return var;
  }

  /* Resolve a variable in a declaration. */
  SymbolVariable *id_var_decl_resolved(IdQualified id, const SymbolScope &scope)
  {
    assert(id.is_valid());
    SymbolVariable *var = scope.lookup_variable(table, id);
    if (var->is_error) {
      error(id, Diag::UnknownVariable, string(id.str()));
      builder << id;
      return var;
    }

    if (var->is_constexpr && var->array_dimensions > 0) {
      error(id, Diag::ConstexprVarMustNotBeArray);
    }

    /* Note we only resolve static variable. */
    if (var->is_static) {
      builder.curr = id.back();
      builder << var->identifier + trivia(id);
    }
    else {
      builder << id;
    }
    return var;
  }

  /* -------------------------------------------------------------------- */
  /** \name Metadata Parsing
   *
   * \{ */

  void parse_class_metadata(SymbolClass &cls, LocalScope body)
  {
    string &cls_id = cls.identifier;
    metadata::ResourceTable srt;
    metadata::VertexInputs vertex_in;
    metadata::StageInterface vertex_out;
    metadata::FragmentOutputs fragment_out;
    metadata::FragmentInputs fragment_in;
    srt.name = cls_id;
    vertex_in.name = cls_id;
    vertex_out.name = cls_id;
    fragment_out.name = cls_id;
    fragment_in.name = cls_id;

    for (VarDecl decl : body.children_of_type<VarDecl>()) {
      Declarator d = decl.child_first(NodeType::Declarator);
      SymbolVariable *var = cls.lookup_variable(table, d.identifier());
      const string &type_name = var->type->identifier;
      const string &var_name = var->identifier;
      switch (cls.srt_type) {
        case ResourceTableType::RESOURCE_TABLE:
          srt.emplace_back(parse_resource(decl.attributes(), type_name, var_name, d.array(), cls));
          break;
        case ResourceTableType::VERTEX_IN:
          vertex_in.emplace_back(parse_vertex_input(decl.attributes(), type_name, var_name));
          break;
        case ResourceTableType::VERTEX_OUT:
          vertex_out.emplace_back(parse_vertex_output(decl.attributes(), type_name, var_name));
          break;
        case ResourceTableType::FRAGMENT_OUT:
          fragment_out.emplace_back(parse_fragment_output(decl.attributes(), type_name, var_name));
          break;
        case ResourceTableType::FRAGMENT_IN:
          fragment_in.emplace_back(parse_fragment_input(decl.attributes(), type_name, var_name));
          break;
        default:
          break;
      }
    }

    switch (cls.srt_type) {
      case ResourceTableType::RESOURCE_TABLE:
        metadata.resource_tables.emplace_back(srt);
        break;
      case ResourceTableType::VERTEX_IN:
        metadata.vertex_inputs.emplace_back(vertex_in);
        break;
      case ResourceTableType::VERTEX_OUT:
        metadata.stage_interfaces.emplace_back(vertex_out);
        break;
      case ResourceTableType::FRAGMENT_OUT:
        metadata.fragment_outputs.emplace_back(fragment_out);
        break;
      case ResourceTableType::FRAGMENT_IN:
        metadata.fragment_inputs.emplace_back(fragment_in);
        break;
      case ResourceTableType::ENTRY_POINT:
      case ResourceTableType::NONE:
        break;
    }
  }

  metadata::ParsedVertInput parse_vertex_input(AttrList attributes,
                                               const string &type,
                                               const string &name)
  {
    auto attr = resource_type_from_attributes(attributes).unwrap(this);
    return {
        .line = 0,
        .var_type = type,
        .var_name = name,
        .slot = string(attr.param1.str()),
    };
  }

  metadata::ParsedAttribute parse_vertex_output(AttrList attributes,
                                                const string &type,
                                                const string &name)
  {
    auto attr = resource_type_from_attributes(attributes).unwrap(this);
    return {
        .line = 0,
        .var_type = type,
        .var_name = name,
        .interpolation_mode = string(attr.attr.str()),
    };
  }

  metadata::ParsedFragOuput parse_fragment_output(AttrList attributes,
                                                  const string &type,
                                                  const string &name)
  {
    auto attr = resource_type_from_attributes(attributes).unwrap(this);
    return {
        .line = 0,
        .var_type = type,
        .var_name = name,
        .slot = string(attr.param1.str()),
        .dual_source = string(attr.param2.str()),
        .raster_order_group = string(attr.raster_order_group.str()),
    };
  }

  metadata::ParsedFragInput parse_fragment_input(AttrList attributes,
                                                 const string &type,
                                                 const string &name)
  {
    auto attr = resource_type_from_attributes(attributes).unwrap(this);
    return {
        .line = 0,
        .var_type = type,
        .var_name = name,
        .slot = string(attr.param1.str()),
        .image_type = string(attr.param2.str()),
        .raster_order_group = string(attr.raster_order_group.str()),
    };
  }

  string parse_image_format(SymbolClass &cls, Expr expr)
  {
    Node node = expr.child_first();
    if (LocalVar var = node; var.is_valid()) {
      SymbolVariable *sym = cls.lookup_variable(table, var.identifier());
      if (sym->is_error) {
        /* This could be a macro. Don't make an error. */
        return string(expr.str());
      }
      int enum_val = value_as<int>(sym->value);
      if (auto it = table.image_formats.find(enum_val); it != table.image_formats.end()) {
        return it->second;
      }
    }
    error(expr, Diag::ExpectedImageFormat);
    return "0";
  }

  string parse_frequency(Expr frequency)
  {
    return frequency.is_valid() ? string(frequency.str()) : string("PASS");
  }

  string array_size_to_string(ast::ArrayDecl array, const SymbolScope &scope)
  {
    if (!array.is_valid()) {
      return "";
    }

    string str;
    for (Subscript sub : array.children_of_type<ast::Subscript>()) {
      if (sub.expr().is_valid()) {
        auto [result, err] = table.expr_type_analysis(scope, sub.expr().child_first());
        if (result.is_constexpr()) {
          if (result.type == table.int_cls || result.type == table.uint_cls) {
            str += '[' + to_string(value_as<int>(result.value)) + ']';
          }
          else {
            error(sub.expr(), Diag::SubscriptNotInt);
          }
        }
        else {
          error(sub.expr(), Diag::ArraySizeNotConstantExpression);
        }
      }
      else {
        str += string(sub.str());
      }
    }
    return str;
  }

  metadata::ParsedResource parse_resource(AttrList attributes,
                                          const string &type,
                                          const string &name,
                                          ArrayDecl array,
                                          SymbolClass &cls)
  {
    auto attr = resource_type_from_attributes(attributes).unwrap(this);
    switch (attr.res_type) {
      case ResourceType::LEGACY_INFO:
      case ResourceType::RESOURCE_TABLE:
      case ResourceType::COMPILATION_CONST:
      case ResourceType::PUSH_CONST:
      case ResourceType::SHARED:
        return {.line = 0,
                .var_type = type,
                .var_name = name,
                .var_array = array_size_to_string(array, cls),
                .res_type = string(attr.attr.identifier().str()),
                .res_condition = attr.parse_condition()};
      case ResourceType::SPECIALIZATION_CONST:
        return {.line = 0,
                .var_type = type,
                .var_name = name,
                .var_array = array_size_to_string(array, cls),
                .res_type = string(attr.attr.identifier().str()),
                .res_value = string(attr.param1.str()),
                .res_condition = attr.parse_condition()};
      case ResourceType::SAMPLER:
        return {.line = 0,
                .var_type = type,
                .var_name = name,
                .var_array = array_size_to_string(array, cls),
                .res_type = string(attr.attr.identifier().str()),
                .res_slot = string(attr.param1.str()),
                .res_condition = attr.parse_condition(),
                .res_frequency = parse_frequency(attr.frequency)};
      case ResourceType::UNIFORM_BUF:
        return {.line = 0,
                .var_type = type,
                .var_name = name,
                .var_array = array_size_to_string(array, cls),
                .res_type = string(attr.attr.identifier().str()),
                .res_slot = string(attr.param1.str()),
                .res_condition = attr.parse_condition(),
                .res_frequency = parse_frequency(attr.frequency)};
      case ResourceType::STORAGE_BUF:
        return {.line = 0,
                .var_type = type,
                .var_name = name,
                .var_array = array_size_to_string(array, cls),
                .res_type = string(attr.attr.identifier().str()),
                .res_slot = string(attr.param1.str()),
                .res_qualifier = string(attr.param2.str()),
                .res_condition = attr.parse_condition(),
                .res_frequency = parse_frequency(attr.frequency)};
      case ResourceType::IMAGE:
        return {.line = 0,
                .var_type = type,
                .var_name = name,
                .var_array = array_size_to_string(array, cls),
                .res_type = string(attr.attr.identifier().str()),
                .res_slot = string(attr.param1.str()),
                .res_qualifier = string(attr.param2.str()),
                .res_format = parse_image_format(cls, attr.param3),
                .res_condition = attr.parse_condition(),
                .res_frequency = parse_frequency(attr.frequency)};
        break;
      default:
        assert(0);
        break;
    }
    return {};
  }

  /** \} */
};

}  // namespace bsl

namespace blender::gpu::shader {

/* Lower namespaces by adding namespace prefix to all the contained structs and functions. */
void SourceProcessor::lower_bsl_to_il(Parser &parser, bsl::SymbolTable &symbols)
{
  using namespace parser;
  bsl::CodegenContext ctx(parser, symbols, metadata_, error_handler);
  std::string str = ctx.process(*symbols.root, parser.root());

  if (error_handler.err.has_value()) {
    throw ParserException();
  }

  parser.set_str(str);
}
}  // namespace blender::gpu::shader

/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 *
 * Grammar parsing and AST building of shader sources.
 * This need to be flexible enough to take as input MSL, GLSL, BSL and our intermediate macro heavy
 * language.
 */

#include "scope.hh"
#include "token.hh"
#include "token_stream.hh"

namespace blender::gpu::shader::parser {

/*
 * Simple Recursive Descent Parser that creates AST nodes.
 *
 * Follows the Blender Shading Language Specification.
 */
struct BSLParser {
  using NodeType = ast::NodeType;
  using Nodes = ast::Nodes;
  using Node = ast::NodeData;
  using TokenID = ast::TokenID;
  using NodeID = ast::NodeID;

  Token curr;

  ParserBase &parser;

  Nodes nodes;
  NodeID curr_node = -1;

  struct NodeScope {
    BSLParser *parser;
    NodeScope(BSLParser *p, NodeType type) : parser(p)
    {
      parser->push_node(type);
    }
    ~NodeScope()
    {
      parser->pop_node();
    }
  };

  void push_node(NodeType type)
  {
    NodeID id = nodes.size();
    nodes.emplace_back(nodes, id, curr_node, curr, type);
    curr_node = id;
  }

  void pop_node()
  {
    assert(curr_node != -1);
    nodes[curr_node].back = curr.index_ - 1;
    curr_node = nodes[curr_node].parent;
  }

  ErrorHandler &error_handler;

  BSLParser(ParserBase &parser, Token tok, ErrorHandler &error_handler)
      : curr(tok), parser(parser), error_handler(error_handler)
  {
  }

#define NODE(type) NodeScope node(this, NodeType::type);

  void translation_unit()
  {
    NODE(Namespace);

    /* Skip first whitespace token if it exists. */
    if (peek() == NewLine || peek() == Space) {
      next();
    }
    external_decl();
    match(EndOfFile);
  }

  void external_decl()
  {
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor_directive();
          break;
        case Namespace:
          namespace_decl();
          break;
        case Using:
          using_statement();
          break;
        case Class:
        case Struct:
          struct_decl();
          break;
        case Enum:
          enum_decl();
          break;
        case Template:
          template_decl_or_spec_or_inst();
          break;
        case SquareOpen: /* Attr. */
        case Const:
        case Constexpr:
        case Static:
        case Inline:
        case Word:
          pipeline_or_func_or_var_decl();
          break;
        case EndOfFile:
        case BracketClose: /* For namespaces. */
          return;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\": Expecting declaration");
          return;
      }
    }
  }

  /* Example: `#define A\n`. */
  void preprocessor_directive()
  {
    NODE(Preprocessor);
    const LexerBase &lex = parser;

    int tok_id = curr.index_;
    while (true) {
      const TokenType type = lex.types_[tok_id];
      if (type == EndOfFile) {
        tok_id--;
        break;
      }
      std::string_view tok_str = lex[tok_id].str_with_whitespace();
      size_t new_line = -1;
      while ((new_line = tok_str.find("\n", new_line + 1)) != std::string::npos) {
        if (new_line == 0 || tok_str[new_line - 1] != '\\') {
          break;
        }
      }
      if (new_line != std::string::npos) {
        break;
      }
      tok_id++;
    }
    curr = lex[tok_id + 1];
  }

  /* Example: `namespace A::B {}`. */
  void namespace_decl()
  {
    NODE(Namespace);
    match(Namespace);
    qualified_id();
    match('{');
    external_decl();
    match('}');
  }

  /* Example : `struct [[a]] A {}`. */
  void struct_decl(bool is_template_inst = false)
  {
    NODE(ClassDecl);
    match(Struct, Class);
    /* Optional attributes. */
    attribute_optional();

    if (peek() == '{' && !is_template_inst) {
      /* Nameless struct */
    }
    else {
      /* Note we allow `struct A::B` syntax because it is used during namespace lowering. */
      qualified_id(is_template_inst);
      if (match_if(';')) {
        if (is_template_inst) {
          return; /* Template explicit instantiation. */
        }
        error("Forward declaration of classes is not supported");
        return;
      }
      if (peek() == Word) {
        /* Struct keyword usage in variable declaration. */
        /* Supported because of explicit host shared struct members and C++ shared code. */
        next();
        return;
      }
    }
    match('{');
    members_decl();
    match('}');
    match(';');
  }

  void members_decl()
  {
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor_directive();
          break;
        case Private:
        case Public: {
          NODE(AccessSpecifier);
          next();
          match(':');
          break;
        }
        case Union:
          union_decl();
          break;
        case Template:
          template_declaration();
          break;
        case Class:
        case Struct:
        case Enum:
        case SquareOpen:
        case Const:
        case Static:
        case Constexpr:
        case Word:
          func_or_var_decl(true);
          break;
        case BracketClose:
          return;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\"");
          return;
      }
    }
  }

  void pipeline_or_func_or_var_decl()
  {
    auto state = save_state();
    if (pipeline_decl()) {
      return;
    }
    restore_state(state);
    func_or_var_decl();
  }

  bool pipeline_decl()
  {
    std::string_view tok_str = curr.str();
    if (!tok_str.starts_with("Pipeline")) {
      return false;
    }

    /* TODO(fclem): Maybe introduce a pipeline keyword. */
    if (tok_str != "PipelineGraphic" && tok_str != "PipelineCompute") {
      return false;
    }

    NODE(PipelineDecl);
    /* PipelineGraphic / PipelineCompute. */
    unqualified_id();
    /* Name. */
    unqualified_id();
    function_parameter_list();
    match(';');
    return true;
  }

  void func_or_var_decl(bool is_member = false)
  {
    auto state = save_state();
    if (var_decl(is_member)) {
      return;
    }
    restore_state(state);
    func_decl();
  }

  bool var_decl(bool is_member = false, bool expect_semicolon = true)
  {
    NODE(VarDecl);
    attribute_optional();
    {
      NODE(IdType);
      if (peek() == Word && curr.str() == "constant") {
        /* MSL constant keyword. Used for enum values. */
        next();
      }
      match_if(Static);
      match_if(Const, Constexpr);
      if (is_member) {
        /* Supported because of explicit host shared struct members. */
        match_if(Struct, Class, Enum);
      }
      qualified_id();
    }
    do {
      declarator();
    } while (match_if(','));
    /* Check if current token is valid. Otherwise we could be at end of file after the last
     * semicolon. */
    bool valid = peek() == ';' || peek() == ')' || peek() == '}';
    if (expect_semicolon) {
      valid = match(';').is_valid();
    }
    return valid;
  }

  void declarator()
  {
    NODE(Declarator);
    bool par = match_if('(');
    /* NOTE: Require Ampersand after parenthesis to avoid mistaking the decl with a func call. */
    if (par || peek() == '&') {
      NODE(Reference);
      match('&');
    }
    unqualified_id_optional();

    if (par) {
      match(')');
    }

    if (peek() == '{') {
      initializer_list();
    }
    else {
      subscript_optional();
      assignment_optional();
    }
  }

  void func_decl(bool is_template_inst = false)
  {
    NODE(FuncDecl);
    attribute_optional();
    if (peek() == Static) {
      NODE(StaticStmt);
      match_if(Static);
    }
    match_if(Inline);
    {
      NODE(IdType);
      qualified_id();
    }
    qualified_id(is_template_inst);
    function_argument_list();
    match_if(Const);
    if (match_if(';')) {
      /* Template instantiation or forward declaration. */
      nodes[curr_node].type = NodeType::FuncForwardDecl;
      return;
    }
    if (peek() == '{') {
      local_scope();
    }
  }

  /* Example: `enum [[a]] A : a {}`. */
  void enum_decl()
  {
    NODE(ClassDecl);
    match(Enum);
    /* Optional class qualifier. */
    match_if(Class);
    /* Optional attributes. */
    attribute_optional();
    /* Optional name. */
    qualified_id_optional();
    if (peek() != ':') {
      error("enum declaration must explicitly use an underlying type");
      return;
    }
    /* Underlying type. */
    match(':');
    qualified_id();

    enum_values();
    match(';');
  }

  void enum_values()
  {
    NODE(LocalScope);
    match('{');
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor_directive();
          break;
        case Word: {
          NODE(EnumValue);
          unqualified_id();
          assignment_optional();
          match_if(',');
          break;
        }
        case BracketClose:
          match('}');
          return;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\": Expecting enum value");
          return;
      }
    }
  }

  void assignment_optional()
  {
    if (peek() == '=') {
      assignment();
    }
  }

  void assignment()
  {
    NODE(AssignStmt);
    match('=');
    init_expression_or_initializer_list();
  }

  void init_expression_or_initializer_list()
  {
    if (peek() == '{') {
      initializer_list();
    }
    else {
      expression(true);
    }
  }

  void initializer_list()
  {
    NODE(InitializerList);
    match('{');
    const bool designated = (peek() == '.');

    while (true) {
      switch (peek()) {
        case BracketClose:
          match('}');
          return;
        case Dot:
        case This:
        case Word:
        case Number:
        case Minus:
        case Plus:
        case ParOpen:
          if (designated) {
            NODE(DesignatedInitializer);
            match('.');
            unqualified_id();
            assignment();
          }
          else {
            NODE(Initializer);
            init_expression_or_initializer_list();
          }
          match_if(',');
          break;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\": Expecting initializer");
          return;
      }
    }
  }

  void function_argument_list()
  {
    NODE(FuncArgList);
    match('(');
    if (match_if(')')) {
      return;
    }
    do {
      NODE(FuncArg);
      attribute_optional();
      {
        NODE(IdType);
        if (peek() == Const) {
          NODE(Const);
          match(Const);
        }
        qualified_id();
      }
      declarator();
    } while (match_if(','));
    match(')');
  }

  void function_parameter_list()
  {
    NODE(FuncParamList);
    match('(');
    if (match_if(')')) {
      return;
    }
    do {
      expression(true);
    } while (match_if(','));
    match(')');
  }

  void template_decl_or_spec_or_inst()
  {
    if (peek_next(1) != TemplateOpen) {
      template_instantiation();
    }
    else if (peek_next(2) == TemplateClose) {
      template_specialization();
    }
    else {
      template_declaration();
    }
  }

  void template_instantiation()
  {
    NODE(TemplateInst);
    match(Template);
    switch (peek()) {
      case Class:
      case Struct:
        struct_decl(true);
        break;
      case Word:
        func_decl(true);
        break;
      default:
        error("Unexpected token \"" + to_str(peek()) + "\", expected template instantiation");
        return;
    }
  }

  void template_specialization()
  {
    NODE(TemplateSpec);
    match(Template);
    template_argument_list();
    switch (peek()) {
      case Class:
      case Struct:
        struct_decl(true);
        break;
      case Word:
        func_decl(true);
        break;
      default:
        error("Unexpected token \"" + to_str(peek()) + "\", expected template specialization");
        return;
    }
  }

  void template_declaration()
  {
    NODE(TemplateDecl);
    match(Template);
    match(TemplateOpen);
    while (true) {
      switch (peek()) {
        case Enum:
          match(Enum);
          [[fallthrough]];
        case Word: {
          {
            NODE(TemplateArg);
            unqualified_id();
            unqualified_id();
          }
          match_if(',');
          break;
        }
        case TemplateClose:
          match(TemplateClose);
          return;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\"");
          return;
      }
    }
  }

  void template_argument_list()
  {
    NODE(TemplateArgList);
    match(TemplateOpen);
    while (true) {
      switch (peek()) {
        case Enum:
          match(Enum);
          [[fallthrough]];
        case Word:
          unqualified_id();
          unqualified_id();
          break;
        case TemplateClose:
          match(TemplateClose);
          return;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\"");
          return;
      }
    }
  }

  bool template_parameter_list_optional()
  {
    if (peek() == TemplateOpen) {
      template_parameter_list();
      return true;
    }
    return false;
  }

  void template_parameter_list()
  {
    NODE(TemplateParamList);
    match(TemplateOpen);
    while (true) {
      switch (peek()) {
        case Number: {
          {
            NODE(NumConst);
            match(Number);
          }
          match_if(',');
          break;
        }
        case Struct:
          match(Struct);
          break;
        case Enum:
          match(Enum);
          break;
        case Word:
          /* TODO(fclem): Differentiate true/false from IDs. Does it matter? */
          qualified_id();
          match_if(',');
          break;
        case TemplateClose:
          match(TemplateClose);
          return;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\"");
          return;
      }
    }
  }

  void expression(bool break_on_comma = false)
  {
    NODE(Expr);
    bool is_first_statement = true;
    while (true) {
      switch (peek()) {
        case SquareOpen:
          subscript();
          break;
        case Template: {
          {
            NODE(TemplateExplicit);
            match(Template);
            {
              NODE(FuncCall);
              qualified_id(true);
              function_parameter_list();
            }
          }
          break;
        }
        case Colon:
          if (peek_next(1) != ':') {
            /* This is the end of a scope. */
            return;
          }
          /* This is the start of an identifier. */
          [[fallthrough]];
        case Word:
          function_call_or_id_or_initializer();
          break;
        case ParOpen:
          match('(');
          expression();
          match(')');
          break;
        case String: {
          NODE(StringConst);
          match(String);
          break;
        }
        case This: {
          NODE(LocalVar);
          match(This);
          break;
        }
        case Number: {
          NODE(NumConst);
          match(Number);
          break;
        }
        case Question: {
          {
            NODE(Op);
            match('?');
          }
          expression();
          {
            NODE(Op);
            match(':');
          }
          break;
        }
        case Minus: {
          NODE(Op);
          next();
          if (peek() == '>') {
            nodes[curr_node].type = NodeType::OpDeref;
            next();
          }
          break;
        }
        case Assign:
        case Ampersand:
        case BitwiseNot:
        case Decrement:
        case Dot:
        case Divide:
        case Equal:
        case GEqual:
        case GThan:
        case Increment:
        case LEqual:
        case LogicalAnd:
        case LogicalOr:
        case LThan:
        case Modulo:
        case Multiply:
        case Not:
        case NotEqual:
        case Or:
        case Plus:
        case Xor: {
          NODE(Op);
          next();
          /* TODO Complete expression parsing. */
          break;
        }
        case Comma: {
          if (break_on_comma) {
            return;
          }
          /* Comma operator. */
          NODE(Op);
          match(',');
          break;
        }
        case BracketClose:
        case SquareClose:
        case TemplateClose:
        case SemiColon:
        case ParClose:
          return;
        case BracketOpen:
          if (is_first_statement) {
            initializer_list();
            break;
          }
          [[fallthrough]];
        default:
          error("Unexpected token \"" + to_str(peek()) + "\"");
          return;
      }
      is_first_statement = false;
    }
  }

  void function_call_or_id_or_initializer()
  {
    NODE(LocalVar);
    qualified_id();
    switch (peek()) {
      case ParOpen:
        nodes[curr_node].type = NodeType::FuncCall;
        function_parameter_list();
        break;
      case BracketOpen:
        nodes[curr_node].type = NodeType::Constructor;
        initializer_list();
        break;
      default:
        break;
    }
  }

  void local_scope(bool optional_brackets = false)
  {
    NODE(LocalScope);
    bool has_brackets = true;
    if (optional_brackets) {
      has_brackets = match_if('{');
    }
    else {
      match('{');
    }
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor_directive();
          break;
        case BracketOpen:
          local_scope();
          break;
        case For:
          for_loop();
          break;
        case While:
          while_loop();
          break;
        case Do:
          do_while_loop();
          break;
        case Switch:
          switch_statement();
          break;
        case If:
          if_statement();
          break;
        case Continue:
          match(Continue);
          match(';');
          break;
        case Break:
          match(Break);
          match(';');
          break;
        case Return: {
          NODE(ReturnStmt);
          match(Return);
          expression();
          match(';');
          break;
        }
        case Using:
          using_statement();
          break;
        case Default:
        case Case:
        case BracketClose:
          if (has_brackets) {
            match('}');
          }
          return;
        case EndOfFile:
          error("Unexpected token \"" + to_str(peek()) + "\"");
          return;
        default:
          local_statement();
          break;
      }
    }
  }

  void local_statement(bool expect_semicolon = true)
  {
    auto state = save_state();
    if (var_decl(false, expect_semicolon)) {
      return;
    }
    /* Restore state after failing to parse a declaration. */
    restore_state(state);
    /* TODO: structured bindings. */

    NODE(LocalStmt);
    attribute_optional();
    expression();
    if (expect_semicolon) {
      match(';');
    }
  }

  void using_statement()
  {
    NODE(UsingStmt);
    match(Using);
    qualified_id();
    if (match_if(Assign)) {
      qualified_id();
    }
    match(';');
  }

  void subscript_optional()
  {
    if (peek() == '[') {
      subscript();
    }
  }

  void subscript()
  {
    NODE(Subscript);
    match('[');
    if (peek() == '[') {
      error("Unexpected attribute specifier");
      return;
    }
    if (!match_if(']')) {
      expression(true);
      match(']');
    }
    subscript_optional();
  }

  void attribute_optional()
  {
    if (peek() == '[') {
      attribute();
    }
  }

  void attribute()
  {
    NODE(AttrList);
    match('[');
    match('[');

    while (true) {
      switch (peek()) {
        case SquareClose:
          match(']');
          match(']');
          /* Attrs can be chained. Nest them so we can make AST traversal easier. */
          if (peek() == '[') {
            attribute();
          }
          return;
        case Word: {
          {
            NODE(Attr);
            unqualified_id();
            if (peek() == '(') {
              function_parameter_list();
            }
          }
          match_if(',');
          break;
        }
        default:
          error("Unexpected token \"" + to_str(peek()) + "\"");
          return;
      }
    }
  }

  /* Example: `union {}`. */
  void union_decl()
  {
    NODE(ClassDecl);
    match(Union);
    match('{');
    members_decl();
    match('}');
    match(';');
  }

  void if_statement()
  {
    {
      NODE(IfStmt);
      match(If);
      match_if(Constexpr);
      condition(1);
      local_scope();
    }
    while (peek() == Else) {
      if (peek_next(1) == If) {
        NODE(ElseIfStmt);
        match(Else);
        match(If);
        condition(1);
        local_scope();
      }
      else {
        NODE(ElseStmt);
        match(Else);
        local_scope();
        break;
      }
    }
  }

  void switch_statement()
  {
    NODE(Switch);
    match(Switch);
    condition(1);
    match('{');
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor_directive();
          break;
        case Case: {
          NODE(SwitchCase);
          match(Case);
          if (!match_if(Number)) {
            qualified_id();
          }
          match(Colon);
          local_scope(true);
          break;
        }
        case Default: {
          NODE(SwitchCase);
          match(Default);
          match(Colon);
          local_scope(true);
          break;
        }
        case BracketClose:
          match('}');
          return;
        default:
          error("Unexpected token \"" + to_str(peek()) + "\" expected switch case");
          return;
      }
    }
  }

  void for_loop()
  {
    NODE(ForLoop);
    match(For);
    condition(3);
    local_scope();
  }

  void while_loop()
  {
    NODE(WhileLoop);
    match(While);
    condition(1);
    local_scope();
  }

  void do_while_loop()
  {
    NODE(DoWhileLoop);
    match(Do);
    local_scope();
    match(While);
    condition(1);
    match(';');
  }

  void condition(int arg_needed)
  {
    NODE(Condition);
    match('(');
    for (int i = 0; i < arg_needed; i++) {
      local_statement(false);
      if (i != arg_needed - 1) {
        match(';');
      }
    }
    match(')');
    attribute_optional();
  }

  void unqualified_id_optional()
  {
    if (peek() == Word) {
      unqualified_id();
    }
  }

  /* Example: `a3234`. */
  void unqualified_id()
  {
    NODE(Id);
    match(Word);
  }

  bool qualified_id_optional()
  {
    if (peek() == Word || (peek() == ':' && peek_next(1) == ':')) {
      qualified_id();
      return true;
    }
    return false;
  }

  /* Example: `A<1>::B<2>`. */
  void qualified_id(bool must_end_with_template_arg = false)
  {
    NODE(IdQualified);
    /* Global scope specifier. */
    if (peek() == ':') {
      NODE(NamespaceSeparator);
      match(':');
      match(':');
    }
    match(Word);
    bool ends_with_template = template_parameter_list_optional();
    while (peek() == ':' && peek_next(1) == ':') {
      {
        NODE(NamespaceSeparator);
        match(':');
        match(':');
      }
      unqualified_id();
      ends_with_template = template_parameter_list_optional();
    }

    if (must_end_with_template_arg && !ends_with_template) {
      error("Expected Template arguments");
    }
  }

#undef NODE

 private:
  TokenType peek() const
  {
    return curr.type();
  }

  TokenType peek_next(int i) const
  {
    return curr.next(i).type();
  }

  void error(const std::string &str)
  {
    error_handler.report(curr, str);
    /* Set token to EndOfFile/Invalid. */
    curr = Token(parser);
  }

  Token match(char expected)
  {
    if (curr != TokenType(expected)) {
      error("Syntax Error: Expected token \"" + to_str(TokenType(expected)) + "\" but got \"" +
            to_str(curr.type()) + "\"");
    }
    Token tok = curr;
    next();
    return tok;
  }

  Token match(char expected, char expected2)
  {
    if (curr != TokenType(expected) && curr != TokenType(expected2)) {
      error("Syntax Error: Expected token \"" + to_str(TokenType(expected)) + "\" or \"" +
            to_str(TokenType(expected2)) + "\" but got \"" + to_str(curr.type()) + "\"");
    }
    Token tok = curr;
    next();
    return tok;
  }

  /* Only go to next token if matching an optional token. */
  template<typename... Args> bool match_if(Args... expected)
  {
    if (((curr == TokenType(expected)) || ...)) {
      next();
      return true;
    }
    return false;
  }

  Token next()
  {
    return curr = curr.next();
  }

  struct ParserState {
    Token curr;
    NodeID curr_node;
    size_t node_count;
    Node node;
  };

  ParserState save_state()
  {
    return {curr, curr_node, nodes.size(), nodes[curr_node]};
  }

  void restore_state(const ParserState &state)
  {
    curr = state.curr;
    curr_node = state.curr_node;
    nodes.resize(state.node_count);
    nodes[curr_node] = state.node;
    error_handler.reset();
  }
};

void ParserBase::parse_bsl(ErrorHandler &err_handler)
{
  LexerBase &lex = *this;

  lex.identify_template_tokens();

  BSLParser p(*this, lex[0], err_handler);
  p.translation_unit();

  this->ast_nodes = std::move(p.nodes);

  if (err_handler.err.has_value()) {
    print_ast();
  }

  lex.reset_template_tokens();
}

void ParserBase::print_ast() const
{
  ast::Node(this, 0).print_ast();
}

ast::Namespace ParserBase::root() const
{
  return ast::Namespace(ast::Node(this, 0));
}

}  // namespace blender::gpu::shader::parser

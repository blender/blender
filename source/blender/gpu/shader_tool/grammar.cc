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

static std::string to_string(TokenType type)
{
  switch (type) {
    case Word:
      return "Word";
    case Number:
      return "Number";
    case TemplateOpen:
      return "<";
    case TemplateClose:
      return ">";
    case NewLine:
      return "NewLine";
    case LogicalAnd:
      return "&&";
    case Break:
      return "break";
    case Const:
      return "const";
    case Constexpr:
      return "constexpr";
    case Do:
      return "do";
    case Decrement:
      return "decrement";
    case NotEqual:
      return "!=";
    case Equal:
      return "==";
    case For:
      return "for";
    case While:
      return "while";
    case LogicalOr:
      return "||";
    case GEqual:
      return ">=";
    case Switch:
      return "switch";
    case Case:
      return "case";
    case If:
      return "if";
    case Else:
      return "else";
    case Elif:
      return "elif";
    case Endif:
      return "endif";
    case Ifdef:
      return "ifdef";
    case Ifndef:
      return "ifndef";
    case Inline:
      return "inline";
    case LEqual:
      return "<=";
    case Static:
      return "static";
    case Enum:
      return "enum";
    case Namespace:
      return "namespace";
    case Define:
      return "define";
    case Union:
      return "union";
    case Continue:
      return "continue";
    case Line:
      return "line";
    case Increment:
      return "++";
    case Pragma:
      return "pragma";
    case DoubleHash:
      return "##";
    case Return:
      return "return";
    case Struct:
      return "struct";
    case Class:
      return "class";
    case Template:
      return "template";
    case This:
      return "this";
    case Using:
      return "using";
    case Undef:
      return "undef";
    case Private:
      return "private";
    case Public:
      return "public";
    default:
      return std::string(1, char(type));
  }
}

#define EXPRESSION_TOKENS \
  Ampersand: \
  case BitwiseNot: \
  case Colon: \
  case Decrement: \
  case Divide: \
  case Dot: \
  case Equal: \
  case GEqual: \
  case GThan: \
  case Increment: \
  case LEqual: \
  case LogicalAnd: \
  case LogicalOr: \
  case LThan: \
  case Minus: \
  case Modulo: \
  case Multiply: \
  case Not: \
  case NotEqual: \
  case Or: \
  case Plus: \
  case Question: \
  case Xor

/*
 * Simple Recursive Descent Parser that creates AST nodes.
 * For now, the AST nodes are just ranges of token (called Scopes) with a specific semantic
 * attached.
 *
 * Since shader code transformations are happening on code still contains macros and preprocessor
 * directives, this parser need to be much less pedantic regarding the target syntax.
 */
struct ScopeParser {
  Token curr;

  ParserBase &parser;

  using Node = Scope;
  Node curr_node = Node(parser);

  std::string error_str;
  Token error_tok;

  ScopeParser(ParserBase &parser, Token tok) : curr(tok), parser(parser), error_tok(parser) {}

  void translation_unit()
  {
    open_scope(curr, ScopeType::Global);
    /* Skip first whitespace token if it exists. */
    if (peek() == NewLine || peek() == Space) {
      next();
    }
    external_declaration();
    close_scope(parser.back(), ScopeType::Global);
    match(EndOfFile);
  }

  void external_declaration()
  {
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case SquareOpen:
          attribute_or_subscript();
          break;
        case Namespace:
          namespace_declaration();
          break;
        case Class:
        case Struct:
          struct_declaration();
          break;
        case Enum:
          enum_declaration();
          break;
        case ParOpen:
          function_definition();
          break;
        case TemplateOpen:
          template_argument_list();
          break;
        case Template:
          template_definition();
          break;
        case Using:
          next();
          match_if(Namespace);
          qualified_id();
          break;
        case Assign:
          assignment();
          break;
        case BracketOpen:
          /* For C++/HLSL compatibility. */
          local_scope(ScopeType::Local);
          break;
        case Const:
        case Colon:
        case Constexpr:
        case SemiColon:
        case Inline:
        case Static:   /* For C++ compatibility. */
        case NotEqual: /* For MSL matrix operators. */
        case Minus:    /* For MSL matrix operators. */
        case Word:
          next();
          break;
        case EndOfFile:
        case BracketClose:
          return;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\": Expecting declaration");
          break;
      }
    }
  }

  /* Example: `struct [[a]] A {}`. */
  void struct_declaration()
  {
    match(Struct, Class);
    /* Optional attributes. */
    if (peek() == '[') {
      attribute();
    }

    if (peek() == '{') {
      /* Nameless struct */
    }
    else {
      /* Note we allow `struct A::B` syntax because it is used during namespace lowering. */
      qualified_id();
      if (peek() == ';') {
        /* Allowed because of shared C++ files which have C++ code not yet rejected. */
        //  error("Forward declaration of classes is not supported");
        return;
      }
      if (peek() == Word) {
        /* Struct keyword usage in variable declaration. */
        /* Supported because of explicit host shared struct members and C++ shared code. */
        next();
        return;
      }
    }
    /* For specialization. */
    if (peek() == lexit::TemplateOpen) {
      template_argument_list();
      if (peek() == ';') {
        /* Template explicit instantiation. */
        return;
      }
    }
    open_scope(curr, ScopeType::Struct);
    match('{');
    member_declaration();
    close_scope(curr, ScopeType::Struct);
    match('}');
    /* Support C-style anonymous struct for C compatibility. */
    match_if(Word);
    match(';');
  }

  void member_declaration()
  {
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case SquareOpen:
          attribute_or_subscript();
          break;
        case Private:
        case Public:
          next();
          match(':');
          break;
        case Class:
        case Struct:
          // error("Nested class declaration is not supported");
          // return;
          /* Supported because of explicit host shared struct members and C++ shared code. */
          struct_declaration();
          break;
        case Enum:
          // error("Nested enum declaration not supported");
          // return;
          /* Supported because of explicit host shared struct members. */
          next();
          break;
        case Union:
          union_declaration();
          break;
        case ParOpen:
          function_definition();
          break;
        case TemplateOpen:
          template_argument_list();
          break;
        case Template:
          template_definition();
          break;
        case BracketClose:
          return;
        case Assign:
          assignment();
          break;
        case Using:
        case Const:
        case Constexpr:
        case Static:
        case SemiColon:
        case Colon:
        case Ampersand: /* For references. */
        case Inline:    /* For MSL / C++. */
        case Number:    /* For C++ bit-flags. */
        case Star:      /* For C++ pointers. */
        case Comma:     /* For C++ constructor. */
        case Equal:     /* For C++ operator. */
        case Word:
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  /* Example: `enum [[a]] A : a {}`. */
  void enum_declaration()
  {
    match(Enum);
    /* Optional class qualifier. */
    match_if(Class);
    /* Optional attributes. */
    if (peek() == '[') {
      attribute();
    }
    /* Note we allow `struct A::B` syntax because it is used during namespace lowering. */
    match(Word);
    if (match_if(':')) {
      /* Underlying type. */
      match(Word);
    }

    open_scope(curr, ScopeType::Local);
    match('{');
    enum_values();
    close_scope(curr, ScopeType::Local);
    match('}');
  }

  void enum_values()
  {
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case ParOpen:
          local_parenthesis();
          break;
        case BracketClose:
          return;
        case Assign:
          assignment();
          break;
        case Comma:
        case Word:
        case Number:
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  /* Example: `union {}`. */
  void union_declaration()
  {
    match(Union);
    open_scope(curr, ScopeType::Local);
    match('{');
    member_declaration();
    close_scope(curr, ScopeType::Local);
    match('}');
  }

  void template_definition()
  {
    match(Template);

    if (peek() == Word || peek() == Struct) {
      /* Template instantiation. */
      return;
    }
    template_argument_list();
  }

  void template_explicit_call()
  {
    if ((curr.prev() != '.') && (curr.prev(1) != '>' && curr.prev(2) != '-')) {
      /* Expected a method call. */
      error("Expected explicit template method call");
    }
    else {
      match(Template);
    }
  }

  void template_argument_list()
  {
    open_scope(curr, ScopeType::Template);
    match(TemplateOpen);

    bool in_argument = false;
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case TemplateOpen:
          template_argument_list();
          break;
        case TemplateClose:
          if (in_argument) {
            in_argument = false;
            close_scope(curr.prev(), ScopeType::TemplateArg);
          }
          close_scope(curr, ScopeType::Template);
          match(TemplateClose);
          return;
        case Comma:
          if (in_argument) {
            in_argument = false;
            close_scope(curr.prev(), ScopeType::TemplateArg);
          }
          next();
          break;
        case Assign:
          if (!in_argument) {
            /* Expecting at least a type before an assignment. */
            match(Word);
          }
          /* For MSL/C++ compatibility. */
          assignment();
          break;
        case Star:   /* For C++ shared headers. */
        case Class:  /* Used during union processing. */
        case Struct: /* Used during union processing. */
        case Colon:  /* Could be removed if using qualified_id(). */
        case LThan:
        case Plus:
        case Minus:
        case Divide:
        case Modulo:
        case GThan:
        case LEqual:
        case GEqual:
        case Equal:
        case Const:
        case Word:
        case Number:
        case Enum:
          if (!in_argument) {
            open_scope(curr, ScopeType::TemplateArg);
            in_argument = true;
          }
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  /* Example: `A::T<t> A::B<T>(){}`. */
  void function_definition()
  {
    function_argument_list();
    match_if(Const);
    if (match_if(';')) {
      /* Template instantiation or forward declaration. */
      return;
    }
    if (peek() == '{') {
      local_scope(ScopeType::Function);
      return;
    }
    /* Function call.
     *  Could eventually become an error but is currently used by create info macros. */
  }

  void assignment()
  {
    open_scope(curr, ScopeType::Assignment);
    match('=');

    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case Template:
          template_explicit_call();
          break;
        case TemplateOpen:
          template_argument_list();
          break;
        case ParOpen:
          function_call_or_local_parenthesis();
          break;
        case SquareOpen:
          subscript();
          break;
        case BracketOpen:
          local_scope(ScopeType::Local);
          break;
        case Assign:
          close_scope(curr.prev(), ScopeType::Assignment);
          assignment();
          return;
        case ParClose:
        case BracketClose:
        case TemplateClose:
        case Comma:
        case SemiColon:
          close_scope(curr.prev(), ScopeType::Assignment);
          return;
        case This:
        case Word:
        case Number:
        case EXPRESSION_TOKENS:
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  void local_scope(ScopeType type)
  {
    open_scope(curr, type);
    match('{');

    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case Template:
          template_explicit_call();
          break;
        case TemplateOpen:
          template_argument_list();
          break;
        case BracketOpen:
          local_scope(ScopeType::Local);
          break;
        case BracketClose:
          close_scope(curr, type);
          match('}');
          return;
        case ParOpen:
          function_call_or_local_parenthesis();
          break;
        case SquareOpen:
          attribute_or_subscript();
          break;
        case Assign:
          assignment();
          break;
        case For:
          for_loop();
          break;
        case While:
          while_loop();
          break;
        case Switch:
          switch_statement();
          break;
        case If:
          next();
          condition(1, ScopeType::Local);
          break;
        case Else:
          next();
          if (peek() == If) {
            break;
          }
          local_scope(ScopeType::Local);
          break;
        case Using:
        case This:
        case Case: /* For switch cases. */
        case Comma:
        case Break:
        case Const:
        case Constexpr:
        case Continue:
        case Return:
        case SemiColon:
        case Word:
        case Number:
        case EXPRESSION_TOKENS:
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  void switch_statement()
  {
    match(Switch);
    condition(1, ScopeType::SwitchArg);
    local_scope(ScopeType::SwitchBody);
  }

  void for_loop()
  {
    match(For);
    condition(3, ScopeType::LoopArgs);
    local_scope(ScopeType::LoopBody);
  }

  void while_loop()
  {
    match(While);
    condition(1, ScopeType::LoopArgs);
    local_scope(ScopeType::LoopBody);
  }

  void condition(int arg_needed, ScopeType type)
  {
    open_scope(curr, type);
    match('(');

    int arg_count = 0;
    bool in_argument = false;
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case ParOpen:
          if (!in_argument) {
            if (type == ScopeType::LoopArgs) {
              open_scope(curr, ScopeType::LoopArg);
            }
            in_argument = true;
          }
          function_call_or_local_parenthesis();
          break;
        case ParClose:
          ++arg_count;
          if (in_argument) {
            in_argument = false;
            if (type == ScopeType::LoopArgs) {
              close_scope(curr.prev(), ScopeType::LoopArg);
            }
          }
          close_scope(curr, type);
          if (arg_count < arg_needed) {
            /* Error about missing semicolon. */
            error("Missing loop or conditional statement");
            return;
          }
          match(')');
          /* Optional attribute. */
          if (peek() == '[') {
            attribute();
          }
          return;
        case SemiColon:
          ++arg_count;
          if (arg_count == arg_needed) {
            /* Error about extra semicolon. */
            error("Extraneous loop or conditional statement");
            return;
          }
          if (in_argument && type == ScopeType::LoopArgs) {
            in_argument = false;
            close_scope(curr.prev(), ScopeType::LoopArg);
          }
          next();
          break;
        case SquareOpen:
          subscript();
          break;
        case Comma:
        case This:
        case Word:
        case Number:
        case Assign: /* Because LEqual and GEqual might not be parsed. */
        case EXPRESSION_TOKENS:
          if (!in_argument) {
            if (type == ScopeType::LoopArgs) {
              open_scope(curr, ScopeType::LoopArg);
            }
            in_argument = true;
          }
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  void function_argument_list()
  {
    open_scope(curr, ScopeType::FunctionArgs);
    match('(');

    bool in_argument = false;
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case ParOpen:
          if (!in_argument) {
            /* This could be enabled once we get rid of all global level macros. */
            // /* Expecting at least a type before a function call. */
            // match(Word);
            open_scope(curr, ScopeType::FunctionArg);
            in_argument = true;
          }
          function_call_or_local_parenthesis();
          break;
        case BracketOpen:
          if (!in_argument) {
            /* Expecting at least a type before a function call. */
            match(Word);
          }
          /* For initializer list constructor of parameters. */
          local_scope(ScopeType::Local);
          break;
        case TemplateOpen:
          template_argument_list();
          break;
        case ParClose:
          if (in_argument) {
            in_argument = false;
            close_scope(curr.prev(), ScopeType::FunctionArg);
          }
          close_scope(curr, ScopeType::FunctionArgs);
          match(')');
          return;
        case Comma:
          if (in_argument) {
            in_argument = false;
            close_scope(curr.prev(), ScopeType::FunctionArg);
          }
          next();
          break;
        case SquareOpen:
          if (!in_argument) {
            open_scope(curr, ScopeType::FunctionArg);
            in_argument = true;
          }
          attribute_or_subscript();
          break;
        case Assign:
          if (!in_argument) {
            /* Expecting at least a type before an assignment. */
            match(Word);
          }
          assignment();
          break;
        case String:     /* Needed for legacy create info. */
        case Or:         /* Needed for legacy create info. */
        case Equal:      /* Needed for some macros. */
        case LThan:      /* Needed for some macros. */
        case GThan:      /* Needed for some macros. */
        case LogicalOr:  /* Needed for some macros. */
        case LogicalAnd: /* Needed for some macros. */
        case Dot:        /* Needed for some macros. */
        case Star:       /* Needed for pointers in shared files. */
        case Word:
        case Number:
        case Minus: /* For C++ constructors.  */
        case Plus:  /* For C++ constructors.  */
        case Const:
        case Constexpr:
        case Ampersand:
        case Colon:
          if (!in_argument) {
            open_scope(curr, ScopeType::FunctionArg);
            in_argument = true;
          }
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  void function_call_or_local_parenthesis()
  {
    TokenType prev = curr.prev().type();
    if (prev == Word || prev == lexit::TemplateClose) {
      function_call();
    }
    else {
      local_parenthesis();
    }
  }

  void local_parenthesis()
  {
    open_scope(curr, ScopeType::Local);
    match('(');

    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case ParOpen:
          function_call_or_local_parenthesis();
          break;
        case ParClose:
          close_scope(curr, ScopeType::Local);
          match(')');
          return;
        case SquareOpen:
          subscript();
          break;
        case This:
        case Comma:
        case Number:
        case Word:
        case String:
        case Assign: /* Because LEqual and GEqual might not be parsed. */
        case EXPRESSION_TOKENS:
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  void function_call()
  {
    open_scope(curr, ScopeType::FunctionCall);
    match('(');

    bool in_argument = false;
    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case ParOpen:
          if (!in_argument) {
            open_scope(curr, ScopeType::FunctionParam);
            in_argument = true;
          }
          function_call_or_local_parenthesis();
          break;
        case ParClose:
          if (in_argument) {
            in_argument = false;
            close_scope(curr.prev(), ScopeType::FunctionParam);
          }
          close_scope(curr, ScopeType::FunctionCall);
          match(')');
          return;
        case Template:
          template_explicit_call();
          break;
        case TemplateOpen:
          template_argument_list();
          break;
        case BracketOpen:
          local_scope(ScopeType::Local);
          break;
        case Comma:
          if (in_argument) {
            in_argument = false;
            close_scope(curr.prev(), ScopeType::FunctionParam);
          }
          next();
          break;
        case SquareOpen:
          subscript();
          break;
        case Number:
        case Word:
        case String:
        case This:
        case Assign: /* Because LEqual and GEqual might not be parsed. */
        case EXPRESSION_TOKENS:
          if (!in_argument) {
            open_scope(curr, ScopeType::FunctionParam);
            in_argument = true;
          }
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  void attribute_or_subscript()
  {
    if (curr.next() == '[') {
      attribute();
    }
    else {
      subscript();
    }
  }

  void subscript()
  {
    open_scope(curr, ScopeType::Subscript);
    match('[');
    if (peek() == '[') {
      error("Unexpected attribute specifier");
      return;
    }

    while (true) {
      switch (peek()) {
        case Hash:
          preprocessor();
          break;
        case ParOpen:
          function_call_or_local_parenthesis();
          break;
        case SquareOpen:
          subscript();
          break;
        case SquareClose:
          close_scope(curr, ScopeType::Subscript);
          match(']');
          return;
        case Number:
        case Word:
        case This:
        case Assign: /* Because LEqual and GEqual might not be parsed. */
        case EXPRESSION_TOKENS:
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  void attribute()
  {
    open_scope(curr, ScopeType::Subscript);
    match('[');
    open_scope(curr, ScopeType::Attributes);
    match('[');

    bool in_attribute = false;
    while (true) {
      switch (peek()) {
        case SquareClose:
          if (in_attribute) {
            in_attribute = false;
            close_scope(curr.prev(), ScopeType::Attribute);
          }
          close_scope(curr, ScopeType::Attributes);
          match(']');
          close_scope(curr, ScopeType::Subscript);
          match(']');
          /* Attributes can be chained. */
          if (peek() == '[') {
            attribute();
          }
          return;
        case ParOpen:
          function_call();
          break;
        case Comma:
          if (in_attribute) {
            in_attribute = false;
            close_scope(curr.prev(), ScopeType::Attribute);
          }
          next();
          break;
        case Word:
          if (!in_attribute) {
            open_scope(curr, ScopeType::Attribute);
            in_attribute = true;
          }
          next();
          break;
        default:
          error("Unexpected token \"" + to_string(peek()) + "\"");
          return;
      }
    }
  }

  /* Example: `namespace A::B {}`. */
  void namespace_declaration()
  {
    match(Namespace);
    qualified_id();
    open_scope(curr, ScopeType::Namespace);
    match('{');
    external_declaration();
    close_scope(curr, ScopeType::Namespace);
    match('}');
  }

  /* Example: `A::B`. */
  void qualified_id()
  {
    match(Word);
    while (peek() == ':') {
      match(':');
      match(':');
      match(Word);
    }
  }

  /* Example: `#define A\n`. */
  void preprocessor()
  {
    const LexerBase &lex = parser;
    open_scope(curr, ScopeType::Preprocessor);

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
    curr = lex[tok_id];
    close_scope(curr, ScopeType::Preprocessor);
    next();
  }

 private:
  void open_scope(Token tok, ScopeType type)
  {
    int index = parser.scope_types.size();
    parser.scope_types.emplace_back(type);
    parser.scope_ranges.emplace_back(tok.index_, 1);

    ScopeLinks &links = parser.scope_links.emplace_back();
    links.parent_ = curr_node.index_;
    if (links.parent_ != -1) {
      ScopeLinks &parent_links = parser.scope_links[links.parent_];
      if (parent_links.child_first_ == -1) {
        parent_links.child_first_ = index;
      }
      links.prev_ = parent_links.child_last_;
      parent_links.child_last_ = index;
    }
    if (links.prev_ != -1) {
      parser.scope_links[links.prev_].next_ = index;
    }

    curr_node = Node(parser, index);
  }

  void close_scope(Token tok, ScopeType type)
  {
    if (curr_node.type() == type) {
      IndexRange &range = parser.scope_ranges[curr_node.index_];
      range.size = tok.index_ - range.start + 1;
      curr_node = curr_node.parent();
    }
  }

  TokenType peek() const
  {
    return curr.type();
  }

  void error(const std::string &str)
  {
    /* Only emit one error to avoid a cascade of error. */
    if (error_str.empty()) {
      error_str = str;
      error_tok = curr;
      /* Set token to EndOfFile/Invalid. */
      curr = Token(parser);
    }
  }

  void match(char expected)
  {
    if (curr != TokenType(expected)) {
      error("Syntax Error: Expected token \"" + to_string(TokenType(expected)) + "\" but got \"" +
            to_string(curr.type()) + "\"");
    }
    next();
  }

  void match(char expected, char expected2)
  {
    if (curr != TokenType(expected) && curr != TokenType(expected2)) {
      error("Syntax Error: Expected token \"" + to_string(TokenType(expected)) + "\" or \"" +
            to_string(TokenType(expected)) + "\" but got \"" + to_string(curr.type()) + "\"");
    }
    next();
  }

  /* Only go to next token if matching an optional token. */
  bool match_if(char expected)
  {
    if (curr == TokenType(expected)) {
      next();
      return true;
    }
    return false;
  }

  Token next()
  {
    return curr = curr.next();
  }
};

void ParserBase::build_scope_tree(report_callback &report_error)
{
  LexerBase &lex = *this;

  lex.identify_template_tokens();

  scope_types.clear();
  scope_ranges.clear();
  scope_links.clear();

  size_t predicted_scope_count = lex.size() / 2;
  scope_types.reserve(predicted_scope_count);
  scope_ranges.reserve(predicted_scope_count);
  scope_links.reserve(predicted_scope_count);

  ScopeParser p(*this, lex[0]);
  p.translation_unit();

  lex.reset_template_tokens();

  if (!p.error_str.empty()) {
    report_error(p.error_tok.line_number(),
                 p.error_tok.char_number(),
                 p.error_tok.line_str(),
                 p.error_str.c_str());
    /* Avoid out of bound access for the rest of the processing. Empty everything. */
    scope_types = {ScopeType::Global};
    scope_ranges = {IndexRange(0, 0)};
  }
  update_string_view();
}

}  // namespace blender::gpu::shader::parser

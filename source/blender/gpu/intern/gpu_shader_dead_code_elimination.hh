/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "shader_tool/intermediate.hh"

namespace blender::gpu {

struct DeadCodeEliminator
    : shader::parser::IntermediateForm<shader::parser::SimpleLexer, shader::parser::NullParser> {
  using Token = shader::parser::Token;
  using TokenType = shader::parser::TokenType;

 private:
  /* Function ID that is unique for each function and all its overloads. */
  using FnId = int;

  struct FunctionGraph {
    /* Counter to assign unique IDs to functions. */
    int counter = 0;
    /* Map declarations (name token) to a function id. */
    Vector<std::pair<Token, FnId>> declarations;
    /* Map identifier to id. */
    Map<StringRef, FnId> names;
    /* Function call (from, to). */
    Vector<std::pair<FnId, FnId>> edges;
  } graph;

  FnId current_fn_id;

  /* Disable function declaration processing.
   * However, still process function calls. */
  bool parsing_enabled;

  /* TODO(fclem): Meh find a better way. Exceptions? */
  static void report_fn(int /*error_line*/,
                        int /*error_char*/,
                        std::string /*error_line_string*/,
                        const char * /*error_str*/)
  {
    BLI_assert_unreachable();
  }
  shader::parser::report_callback report_fn_ptr = report_fn;

 public:
  DeadCodeEliminator(const std::string_view str)
      : shader::parser::IntermediateForm<shader::parser::SimpleLexer, shader::parser::NullParser>(
            str, report_fn_ptr)
  {
  }

  /* Fetch previous token skipping whitespace. */
  static Token prev(Token tok)
  {
    tok = tok.prev();
    while (tok == shader::parser::Space || tok == shader::parser::NewLine) {
      tok = tok.prev();
    }
    return tok;
  }

  /* Fetch next token skipping whitespace. */
  static Token next(Token tok)
  {
    tok = tok.next();
    while (tok == shader::parser::Space || tok == shader::parser::NewLine) {
      tok = tok.next();
    }
    return tok;
  }

  static Token find_matching_pair(Token start, TokenType scope_open, TokenType scope_close)
  {
    int stack = 1;
    Token tok = start;
    while (tok.is_valid()) {
      tok = next(tok);
      if (tok == scope_open) {
        stack++;
        continue;
      }
      if (tok == scope_close) {
        stack--;
        if (stack == 0) {
          return tok;
        }
      }
    }
    BLI_assert_unreachable();
    return Token::invalid();
  }

  void function_definition(Token name_tok, Token par_tok)
  {
    StringRef name = str(name_tok);
    FnId &id = graph.names.lookup_or_add(name, -1);

    if (id == -1) {
      id = graph.counter++;
    }

    graph.declarations.append_as(name_tok, id);

    Token end_of_args = find_matching_pair(par_tok, TokenType::ParOpen, TokenType::ParClose);

    if (next(end_of_args) == '{') {
      current_fn_id = id;
    }
  }

  void function_call(Token name_tok)
  {
    if (current_fn_id == -1) {
      return;
    }

    StringRef name = str(name_tok);

    int fn_id = graph.names.lookup_default(name, -1);
    /* TODO(fclem): On Metal, the function prototypes are removed, which means they can be defined
     * later on.  */
    if (fn_id == -1) {
      /* Functions is not defined. Can be builtin function. */
      return;
    }
    graph.edges.append_as(current_fn_id, fn_id);
  }

  /* There can be a few remaining directive. Avoid parsing them as functions. */
  void process_function(int &cursor)
  {
    Token parenthesis_tok = parser_[cursor];
    Token name_tok = prev(parenthesis_tok);
    /* WATCH(fclem): It could be that a line directive is put between the return type and the
     * function name (which would mess up the). This is currently not happening with the
     * current code-base but might in the future. Checking for it would be quite expensive. */
    if (name_tok != TokenType::Word) {
      return;
    }
    Token type_tok = prev(name_tok);
    StringRef type_str = str(type_tok);

    TokenType type_tok_type = type_tok.type();
    if (type_tok == TokenType::Word && type_str[0] >= '0' && type_str[0] <= '9') {
      /* Case where a function is called just after a line directive. The type token was not
       * recognized as a Number token from the tokenizer rules. */
      type_tok_type = TokenType::Number;
    }

    if (type_tok_type == TokenType::Word && type_str != "return" && type_str != "else") {
      if (parsing_enabled) {
        function_definition(name_tok, parenthesis_tok);
      }
    }
    else {
      function_call(name_tok);
    }
  }

  /* There can be a few remaining directive. Avoid parsing them as functions. */
  void process_directives(int &cursor)
  {
    Token hash_tok = parser_[cursor];
    Token dir_name = next(hash_tok);
    Token end_tok = end_of_directive(dir_name);
    cursor = end_tok.index;

    StringRef whole_dir_str = substr_range_inclusive_view(dir_name, end_tok);

    if (whole_dir_str == "pragma blender dead_code_elimination off") {
      parsing_enabled = false;
    }
    else if (whole_dir_str == "pragma blender dead_code_elimination on") {
      parsing_enabled = true;
    }
  }

  void parse_source()
  {
    current_fn_id = -1;
    parsing_enabled = true;

    int stack_depth = 0;

    for (int cursor = 0; cursor < lex_.token_types.size(); cursor++) {
      TokenType tok_type = TokenType(lex_.token_types[cursor]);
      if (tok_type == TokenType::ParOpen) {
        process_function(cursor);
      }
      else if (tok_type == TokenType::Hash) {
        process_directives(cursor);
      }
      else if (current_fn_id != -1) {
        if (tok_type == TokenType::BracketOpen) {
          stack_depth++;
        }
        else if (tok_type == TokenType::BracketClose) {
          stack_depth--;
          if (stack_depth == 0) {
            current_fn_id = -1;
          }
        }
      }
    }
  }

  Map<FnId, Vector<FnId>> build_adjacency()
  {
    Map<FnId, Vector<FnId>> adj;
    adj.reserve(graph.counter);
    for (const auto &[from, to] : graph.edges) {
      adj.lookup_or_add_default(from).append(to);
    }
    return adj;
  }

  Set<FnId> compute_used_functions(const Vector<FnId> &roots)
  {
    Set<FnId> used;
    used.reserve(graph.counter);

    auto adj = build_adjacency();

    std::vector<FnId> stack;
    stack.reserve(64);

    for (FnId root : roots) {
      if (used.add(root)) {
        stack.push_back(root);
      }

      while (!stack.empty()) {
        FnId f = stack.back();
        stack.pop_back();

        const auto *calls = adj.lookup_ptr(f);
        if (calls == nullptr) {
          continue;
        }

        for (FnId callee : *calls) {
          if (used.add(callee)) {
            stack.push_back(callee);
          }
        }
      }
    }

    return used;
  }

  void prune_unused_functions()
  {
    FnId main_id = graph.names.lookup_default("main", -1);
    if (main_id == -1) {
      /* Can be true inside tests. */
      return;
    }

    Vector<FnId> entry_points{graph.names.lookup("main")};
    /* TODO(fclem): Properly support forward declaration. */
    if (graph.names.contains("nodetree_displacement")) {
      entry_points.append(graph.names.lookup("nodetree_displacement"));
    }
    if (graph.names.contains("nodetree_surface")) {
      entry_points.append(graph.names.lookup("nodetree_surface"));
    }
    if (graph.names.contains("nodetree_volume")) {
      entry_points.append(graph.names.lookup("nodetree_volume"));
    }
    if (graph.names.contains("nodetree_thickness")) {
      entry_points.append(graph.names.lookup("nodetree_thickness"));
    }
    if (graph.names.contains("derivative_scale_get")) {
      entry_points.append(graph.names.lookup("derivative_scale_get"));
    }
    if (graph.names.contains("closure_to_rgba")) {
      entry_points.append(graph.names.lookup("closure_to_rgba"));
    }

    Set<FnId> used = compute_used_functions(entry_points);

    for (auto [name_tok, id] : graph.declarations) {
      if (used.contains(id)) {
        continue;
      }
      Token type = prev(name_tok);
      Token parenthesis = next(name_tok);
      Token end_of_args = find_matching_pair(parenthesis, TokenType::ParOpen, TokenType::ParClose);
      Token body_start = next(end_of_args);
      if (body_start == '{') {
        /* Full definition. */
        Token body_end = find_matching_pair(
            body_start, TokenType::BracketOpen, TokenType::BracketClose);
        erase(type, body_end);
      }
      else {
        /* Prototype. */
        /* Filter MSL & GLSL specific identifiers that could have confused the parser. */
        StringRef type_str = str(type);
        StringRef name_str = str(name_tok);
        if (type_str == "thread" || type_str == "device" || name_str == "layout") {
          continue;
        }
        erase(type, next(end_of_args));
      }
    }
  }

  void optimize()
  {
    parse_source();
    prune_unused_functions();
  }

  static StringRef str(const Token t)
  {
    /* NOTE: White-spaces where not merged (because of #TokenizePreprocessor),
     * so using #str_view_with_whitespace will be faster. */
    return t.str_view_with_whitespace();
  }

  static Token end_of_directive(const Token dir_tok)
  {
    Token tok = dir_tok;

    while (tok != TokenType::NewLine) {
      if (tok.next() == TokenType::Invalid) {
        /* Error or end of file. */
        return tok;
      }
      tok = skip_directive_newlines(tok.next());
    }
    return tok.prev();
  }

  static Token skip_directive_newlines(Token tok)
  {
    while (tok == '\\' && tok.next() == '\n') {
      tok = tok.next().next();
    }
    return tok;
  }
};

/** \} */

}  // namespace blender::gpu

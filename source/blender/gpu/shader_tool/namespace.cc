/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include <set>
#include <unordered_set>

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;

static string get_prefix(Scope ns_scope)
{
  string prefix;
  while (ns_scope.type() == ScopeType::Namespace || ns_scope.type() == ScopeType::Struct) {
    prefix = ns_scope.front().prev().full_symbol_name() + "::" + prefix;
    ns_scope = ns_scope.scope();
  }
  return prefix;
}

TemplateDefinition SourceProcessor::parse_template_definition(SourceProcessor::Parser &parser,
                                                              Token template_tok,
                                                              bool is_method,
                                                              Scope ns_scope,
                                                              const std::string &filepath)
{
  Token def_start = template_tok;
  Scope template_args = def_start.next().scope();
  /* Skip arguments. */
  Token tok_type = template_args.back().next();

  Token body_start = template_tok.find_next(BracketOpen);
  Token def_end = body_start.scope().back();

  TemplateDefinition symbol;
  symbol.filepath = filepath;
  symbol.definition_line = tok_type.line_number();
  symbol.is_method = is_method;
  symbol.is_static = tok_type == Static;
  symbol.is_struct = tok_type == Struct || tok_type == Class;
  symbol.name_space = get_prefix(ns_scope);

  if (symbol.is_struct) {
    Token name = body_start.prev();
    symbol.identifier = string(name.str());
  }
  else {
    Token fn_args = body_start.prev() == Const ? body_start.prev(2) : body_start.prev();
    Token fn_name = fn_args.scope().front().prev();
    symbol.identifier = string(fn_name.str());
  }

  /* Capture end semicolon for structs. */
  def_end = (symbol.is_struct) ? def_end.next() : def_end;
  symbol.definition = parser.substr(def_start, def_end);
  return symbol;
}

void SourceProcessor::parse_namespace_symbols(SourceProcessor::Parser &parser,
                                              Scope ns,
                                              metadata::Source &metadata,
                                              const std::string &filepath)
{
  ns.foreach_scope(ScopeType::Namespace, [&](const Scope &ns) {
    parse_namespace_symbols(parser, ns, metadata, filepath);
  });

  auto process_symbol = [&](Scope ns_scope,
                            Token name,
                            string_view identifier,
                            size_t line,
                            bool is_method,
                            bool is_static,
                            bool is_struct,
                            std::vector<std::pair<std::string, std::string>> members = {}) {
    if (name.scope() != ns_scope) {
      return;
    }
    Symbol symbol;
    symbol.name_space = get_prefix(ns_scope);
    symbol.identifier = identifier;
    symbol.definition_line = line;
    symbol.is_method = is_method;
    symbol.is_static = is_static;
    symbol.is_struct = is_struct;
    symbol.members = members;
    metadata.symbol_table.emplace_back(symbol);
  };

  auto process_templates = [&](Scope ns_scope, Token t, bool is_method) {
    if (t.scope() != ns_scope) {
      return;
    }

    if (t.next() == '<') {
      if (t.next(2) == '>') {
        /* Template specialization. */
        return;
      }
      TemplateDefinition symbol = SourceProcessor::parse_template_definition(
          parser, t, is_method, ns_scope, filepath);
      metadata.template_definitions.emplace_back(symbol);
      return;
    }
    /* Line number of the instantiation should be the one of the definition.
     * But it is very hard at this point to search for the definition.
     * Instead we consider the instantiation to be at the top of the file.
     * It is unlikely we will have name collision with an instantiated template. */
    size_t line = 0;
    if (t.next() == Struct || t.next() == Class) {
      /* Struct. */
      Token name = t.next().next();
      Scope template_args = name.next().scope();
      string resolved_name = string(name.str()) +
                             SourceProcessor::template_arguments_mangle(template_args);
      process_symbol(ns_scope, name, resolved_name, line, false, false, true, {});
    }
    else {
      /* Function. */
      Token end = t.find_next(SemiColon);
      Scope template_args = end.prev().scope().front().prev().scope();
      Token name = template_args.front().prev();
      string resolved_name = string(name.str()) +
                             SourceProcessor::template_arguments_mangle(template_args);
      process_symbol(ns_scope, name, resolved_name, line, is_method, false, false);
    }
  };

  ns.foreach_struct([&](Token, Scope, Token struct_name, Scope body) {
    /* Parse member. */
    std::vector<std::pair<std::string, std::string>> members;
    body.foreach_declaration([&](Scope, Token, Token type, Scope, Token name, Scope, Token) {
      /* For methods, the declaration line is the top of the struct. */
      members.emplace_back(type.str(), name.str());
    });
    process_symbol(ns,
                   struct_name,
                   struct_name.str(),
                   struct_name.line_number(),
                   false,
                   false,
                   true,
                   members);
    /* Methods. */
    body.foreach_function([&](bool is_static, Token, Token name, Scope, bool, Scope) {
      /* For methods, the declaration line is the top of the struct. */
      process_symbol(body, name, name.str(), struct_name.line_number(), true, is_static, false);
    });
    /* Parse template instantiations. */
    body.foreach_token(Template, [&](Token t) { process_templates(body, t, true); });
  });

  ns.foreach_function([&](bool, Token, Token name, Scope, bool, Scope) {
    process_symbol(ns, name, name.str(), name.line_number(), false, false, false);
  });
  /* Parse template instantiations. */
  ns.foreach_token(Template, [&](Token t) { process_templates(ns, t, false); });
}

void SourceProcessor::parse_local_symbols(Parser &parser)
{
  parse_namespace_symbols(parser, parser(), metadata_, filepath_);
}

static void lower_namespace(string ns_prefix,
                            const Scope &scope,
                            SourceProcessor::Parser &parser,
                            ErrorHandler &error_handler,
                            const set<Symbol> &symbols_set)
{
  string ns_name(scope.front().prev().str());
  ns_prefix += ns_name + "::";

  bool has_nested_scope = false;
  scope.foreach_scope(ScopeType::Namespace, [&](const Scope &scope) {
    lower_namespace(ns_prefix, scope, parser, error_handler, symbols_set);
    has_nested_scope = true;
  });

  if (has_nested_scope) {
    /* Process iteratively. */
    return;
  }

  scope.foreach_token(Word, [&](const Token &token) {
    /* Reject method calls. */
    if (token.prev() == '.') {
      return;
    }
    Token next = token.next();
    /* Only process the end token of a namespace qualified identifier. */
    if (next == ':') {
      return;
    }

    bool is_pipeline_arg = false;
    if (token.scope().type() == ScopeType::FunctionArg) {
      std::string_view type = token.scope().scope().front().prev(2).str();
      is_pipeline_arg = (type == "PipelineGraphic" || type == "PipelineCompute");
    }

    if (is_pipeline_arg) {
      /* Special case for pipelines. We need to match the function names but they are arguments. */
    }
    /* Only process function calls or types. */
    else if (next != '<' &&  /* Templated type or function. */
             next != '{' &&  /* Type definition. */
             next != '&' &&  /* Reference definition. */
             next != Word && /* Variable definition. */
             token.scope().type() != parser::ScopeType::TemplateArg && /* Template argument. */
             next != '(' /* Function call or reference definition. */
    )
    {
      return;
    }

    const bool is_fn = (token.next() == '(') ||
                       (token.next() == '<' && token.next().scope().back().next() == '(');
    /* Reject method definition. */
    if (is_fn && token.scope().type() == ScopeType::Struct) {
      return;
    }

    string struct_name;
    if (is_fn) {
      /* If this is function call inside a struct, this could reference a method.
       * In this case we need to add the struct name during the fully qualified name lookup. */
      const Scope struct_scope = token.scope().first_scope_of_type(ScopeType::Struct);
      if (struct_scope.is_valid()) {
        struct_name = struct_scope.front().prev().full_symbol_name();
      }
    }

    const int token_line = token.line_number();

    for (const auto &symbol : symbols_set) {
      if (token.str() != symbol.identifier) {
        continue;
      }
      /* Only expand symbols that are visible inside this namespace. */
      if (!symbol.name_space.starts_with(ns_prefix)) {
        continue;
      }
      /* Reject symbols declared after the identifier.
       * Note that static method have their definition line at the top of the struct. */
      if (token_line < symbol.definition_line) {
        continue;
      }
      /* Symbol as it could be specified from this namespace. */
      string symbol_visible = symbol.name_space.substr(ns_prefix.size()) + symbol.identifier;

      bool append_struct_ns = false;
      string specified_symbol = token.full_symbol_name();
      /* First try to match methods. */
      if (symbol.is_method && !struct_name.empty()) {
        if (!symbol.is_static) {
          continue;
        }

        bool is_prev_ns_specifier = token.prev() == ':' && token.prev(2) == ':';
        if (!is_prev_ns_specifier) {
          /* For unspecified symbol, we append the struct namespace to try to match the method
           * visible symbol. */
          specified_symbol = struct_name + "::" + specified_symbol;
          append_struct_ns = true;
        }

        if (specified_symbol != symbol_visible) {
          continue;
        }
        /* Matched a static method. */
      }
      else {
        /* Other symbols. */
        if (specified_symbol != symbol_visible) {
          continue;
        }
      }

      /* Only for non-definition. */
      if (token.prev() != Word) {
        /* WORKAROUND: Since we do not have overload argument type infos, we cannot resolve
         * them like C++ does. Instead, we error on ambiguity and let the user resolve it. */
        for (const auto &overload : symbols_set) {
          /* Searching for overload in other namespaces. */
          if (overload.name_space == symbol.name_space || overload.identifier != symbol.identifier)
          {
            continue;
          }
          /* Reject symbols declared after the identifier.
           * Note that static method have their definition line at the top of the struct. */
          if (token_line < overload.definition_line) {
            continue;
          }
          /* Only expand symbols that are visible inside this namespace. */
          if (!ns_prefix.starts_with(overload.name_space)) {
            continue;
          }
          if (specified_symbol != overload.identifier) {
            continue;
          }
          error_handler.report(
              token, "Call to function is ambiguous. Specify namespace to remove ambiguity.");
          break;
        }
      }

      /* Append current namespace. */
      parser.insert_before(token.namespace_start(), ns_name + "::");
      if (append_struct_ns) {
        /* Append struct namespace for static methods. */
        parser.insert_before(token.namespace_start(), struct_name + "::");
      }
      /* Only match a symbol once. */
      break;
    }
  });

  /* Pipeline declarations.
   * Manually handle them. They are the only use-case of variable defined in global scope. */
  scope.foreach_match("AA(A", [&](vector<Token> toks) {
    if (toks[0].scope().type() != ScopeType::Namespace || !toks[0].str().starts_with("Pipeline")) {
      return;
    }
    parser.insert_before(toks[1], ns_name + SourceProcessor::namespace_separator);
  });

  Token namespace_tok = scope.front().prev().namespace_start().prev();
  if (namespace_tok == Namespace) {
    parser.erase(namespace_tok, scope.front());
    parser.erase(scope.back());
  }
  else {
    error_handler.report(namespace_tok, "Expected namespace token.");
  }
}

/* Lower namespaces by adding namespace prefix to all the contained structs and functions. */
void SourceProcessor::lower_namespaces(Parser &parser)
{
  using namespace metadata;

  /* Expand compound namespaces. Simplify lowering.
   * Example: `namespace A::B {}` > `namespace A { namespace B {} }` */
  parser().foreach_token(Namespace, [&](Token t) {
    int nesting = 0;
    Token name = t.next();
    while (name.next() == ':') {
      parser.replace(name.next(), name.next().next(), " { namespace ");
      name = name.next().next().next();
      nesting++;
    }
    Scope scope = name.next().scope();
    for (int i = 0; i < nesting; i++) {
      parser.insert_before(scope.back(), "}");
    }
  });

  parser.apply_mutations();

  /* Using an ordered set ordered by namespace make homonym symbols are resolve
   * properly (closest from current namespace). */
  set<Symbol> symbols_set;
  {
    /* Deduplicate symbols. Done this way because we want to keep line definition ordering
     * inside the symbols_set. */
    unordered_set<string> unique_symbols;
    for (const auto &symbol : metadata_.symbol_table) {
      auto [_, inserted] = unique_symbols.insert(symbol.name_space + symbol.identifier);
      if (inserted) {
        symbols_set.emplace(symbol);
      }
    }
  }

  do {
    /* Parse each namespace declaration.
     * Do it iteratively from the deepest namespace to the shallowest. */
    parser().foreach_scope(ScopeType::Namespace, [&](const Scope &scope) {
      lower_namespace("", scope, parser, error_handler, symbols_set);
    });
  } while (parser.apply_mutations());
}

void SourceProcessor::lower_scope_resolution_operators(Parser &parser)
{
  parser().foreach_match<true>("::", [&](const vector<Token> &tokens) {
    if (tokens[0].scope().type() == ScopeType::Attribute) {
      return;
    }
    Token prev = tokens[0].prev();
    if (prev != Word && !(prev == '>' && prev.followed_by_whitespace() == false)) {
      /* Global namespace reference. */
      parser.erase(tokens.front(), tokens.back());
    }
    else {
      /* Specific namespace reference. */
      parser.replace(tokens.front(), tokens.back(), namespace_separator);
    }
  });
  parser.apply_mutations();
}

}  // namespace blender::gpu::shader

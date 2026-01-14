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

static void parse_namespace_symbols(Scope ns, metadata::Source &metadata)
{
  ns.foreach_scope(ScopeType::Namespace,
                   [&](const Scope &ns) { parse_namespace_symbols(ns, metadata); });

  auto process_symbol =
      [&](Scope ns_scope, Token name, string identifier, size_t line, bool is_method) {
        if (name.scope() != ns_scope) {
          return;
        }
        string prefix;
        while (ns_scope.type() == ScopeType::Namespace || ns_scope.type() == ScopeType::Struct) {
          prefix = ns_scope.front().prev().full_symbol_name() + "::" + prefix;
          ns_scope = ns_scope.scope();
        }
        Symbol symbol;
        symbol.name_space = prefix;
        symbol.identifier = identifier;
        symbol.definition_line = line;
        symbol.is_method = is_method;
        metadata.symbol_table.emplace_back(symbol);
      };

  auto process_templates = [&](Scope ns_scope, Token t, bool is_method) {
    if (t.next() == '<') {
      /* Template definition.*/
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
      string resolved_name = name.str() +
                             SourceProcessor::template_arguments_mangle(template_args);
      process_symbol(ns_scope, name, resolved_name, line, false);
    }
    else {
      /* Function. */
      Token end = t.find_next(SemiColon);
      Scope template_args = end.prev().scope().front().prev().scope();
      Token name = template_args.front().prev();
      string resolved_name = name.str() +
                             SourceProcessor::template_arguments_mangle(template_args);
      process_symbol(ns_scope, name, resolved_name, line, is_method);
    }
  };

  ns.foreach_struct([&](Token, Scope, Token struct_name, Scope body) {
    process_symbol(ns, struct_name, struct_name.str(), struct_name.line_number(), false);
    /* Methods. */
    body.foreach_function([&](bool, Token, Token name, Scope, bool, Scope) {
      /* For methods, the declaration line is the top of the struct. */
      process_symbol(body, name, name.str(), struct_name.line_number(), true);
    });
    /* Parse template instantiations. */
    ns.foreach_token(Template, [&](Token t) { process_templates(body, t, true); });
  });

  ns.foreach_function([&](bool, Token, Token name, Scope, bool, Scope) {
    process_symbol(ns, name, name.str(), name.line_number(), false);
  });
  /* Parse template instantiations. */
  ns.foreach_token(Template, [&](Token t) { process_templates(ns, t, false); });
}

void SourceProcessor::parse_local_symbols(Parser &parser)
{
  parser().foreach_scope(ScopeType::Namespace,
                         [&](const Scope &ns) { parse_namespace_symbols(ns, metadata_); });
}

static void lower_namespace(string ns_prefix,
                            const Scope &scope,
                            SourceProcessor::Parser &parser,
                            SourceProcessor::report_callback report_error,
                            const set<Symbol> &symbols_set)
{
  string ns_name = scope.front().prev().str();
  ns_prefix += ns_name + "::";

  bool has_nested_scope = false;
  scope.foreach_scope(ScopeType::Namespace, [&](const Scope &scope) {
    lower_namespace(ns_prefix, scope, parser, report_error, symbols_set);
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

    const bool is_fn = (token.next() == '(');
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
        struct_name = struct_scope.str();
      }
    }

    for (const auto &symbol : symbols_set) {
      if (token.str() != symbol.identifier) {
        continue;
      }
      /* Only expand symbols that are visible inside this namespace. */
      if (symbol.name_space.substr(0, ns_prefix.size()) != ns_prefix) {
        continue;
      }
      /* Reject symbols declared after the identifier. */
      if (token.line_number() < symbol.definition_line) {
        continue;
      }
      /* Symbol as it could be specified from this namespace. */
      string symbol_visible = symbol.name_space.substr(ns_prefix.size()) + symbol.identifier;

      /* First try to match methods. */
      if (symbol.is_method && !struct_name.empty()) {
        if (struct_name + token.full_symbol_name() == symbol_visible) {
          continue;
        }
        /* Do not append namespace for method call matches. */
        break;
      }

      /* Other symbols. */
      if (token.full_symbol_name() != symbol_visible) {
        continue;
      }

      /* Append current namespace. */
      parser.insert_before(token.namespace_start(), ns_name + "::");
      /* Only match a symbol once. */
      break;
    }
  });

  /* Pipeline declarations.
   * Manually handle them. They are the only use-case of variable defined in global scope. */
  scope.foreach_match("ww(w", [&](vector<Token> toks) {
    if (toks[0].scope().type() != ScopeType::Namespace || toks[0].str().find("Pipeline") != 0) {
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
    report_error(ERROR_TOK(namespace_tok), "Expected namespace token.");
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
      lower_namespace("", scope, parser, report_error_, symbols_set);
    });
  } while (parser.apply_mutations());
}

}  // namespace blender::gpu::shader

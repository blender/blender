/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include <unordered_set>

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;

/* `class` -> `struct` */
void SourceProcessor::lower_classes(Parser &parser)
{
  parser().foreach_token(Class, [&](const Token &token) {
    if (token.prev() != Enum) {
      parser.replace(token, "struct ");
    }
  });
}

/* Search for constructor definition in active code. These are not supported. */
void SourceProcessor::lint_constructors(Parser &parser)
{
  parser().foreach_struct([&](Token, Scope, Token struct_name, Scope struct_scope) {
    struct_scope.foreach_match("A(..)", [&](const Tokens &t) {
      if (t[0].scope() != struct_scope) {
        return;
      }
      if (t[0].str() == struct_name.str()) {
        report_error_(ERROR_TOK(t[0]), "Constructors are not supported.");
      }
    });
  });
}

/* Forward declaration of types are not supported and makes no sense in a shader program where
 * there is no pointers. */
void SourceProcessor::lint_forward_declared_structs(Parser &parser)
{
  parser().foreach_match("sA;", [&](const Tokens &t) {
    if (t[0].scope().type() == ScopeType::Global) {
      report_error_(ERROR_TOK(t[0]), "Forward declaration of types are not supported.");
    }
  });
}

/* Create default initializer (empty brace) for all classes. */
void SourceProcessor::lower_default_constructors(Parser &parser)
{
  unordered_set<string> builtin_types = {
      "bool32_t",      "float2",       "packed_float2", "float3",      "packed_float3", "float4",
      "packed_float4", "float2x2",     "float2x3",      "float2x4",    "float3x2",      "float3x3",
      "float3x4",      "float4x2",     "float4x3",      "float4x4",    "float2x2",      "float3x3",
      "float4x4",      "int2",         "int3",          "packed_int3", "int4",          "uint2",
      "uint3",         "packed_uint3", "uint4",         "bool2",       "bool3",         "bool4",
  };

  parser().foreach_struct([&](Token, Scope attributes, Token name, Scope body) {
    /* Don't do host shared structures. */
    if (attributes.is_valid()) {
      return;
    }

    int decl_count = 0;
    string decl;
    body.foreach_declaration([&](Scope, Token, Token type, Scope, Token name, Scope array, Token) {
      auto default_value = [&](const string &type) -> string {
        if (type == "float") {
          return "0.0f";
        }
        if (type == "uint" || type == "uchar") {
          return "0u";
        }
        if (type == "int" || type == "char") {
          return "0";
        }
        if (type == "bool") {
          return "false";
        }
        if (builtin_types.find(type) != builtin_types.end()) {
          return type + "(0)";
        }
        return type + "{}";
      };

      if (array.is_valid()) {
        int array_len = static_array_size(array, 0);
        if (array_len == 0) {
          decl += "for(int i=0;i < " + array.str_exclusive() + ";i++){";
          decl += "r." + name.str() + "[i]=" + default_value(type.str()) + ";";
          decl += "}";
        }
        else {
          for (int i = 0; i < array_len; i++) {
            decl += "r." + name.str() + "[" + to_string(i) + "]";
            decl += "=" + default_value(type.str()) + ";";
          }
        }
      }
      else {
        /* Assigning members one by one as the foreach decl iterator can be out of order. */
        decl += "r." + name.str() + "=" + default_value(type.str()) + ";";
      }
      decl_count++;
    });

    if (decl_count == 0) {
      /* Empty struct will have a padding int. */
      decl += "r._pad=0;";
    }

    decl = "static " + name.str() + " ctor_() {" + name.str() + " r;" + decl + "return r;}";

    parser.insert_after(body.front().str_index_last_no_whitespace(), decl);
  });
}

/* Make all members of a class to be referenced using `this->`. */
void SourceProcessor::lower_implicit_member(Parser &parser)
{
  parser().foreach_struct([&](Token, Scope, Token, Scope body) {
    vector<Token> members_tokens;
    vector<Token> methods_tokens;

    auto is_class_token = [&](const vector<Token> &members, const string &token) {
      for (const Token &member : members) {
        if (token == member.str()) {
          return true;
        }
      }
      return false;
    };

    auto check_shadowing = [&](const Tokens &toks) {
      if (is_class_token(members_tokens, toks[1].str())) {
        report_error_(ERROR_TOK(toks[1]), "Class member shadowing.");
      }
    };

    body.foreach_declaration([&](Scope, Token, Token, Scope, Token name, Scope, Token) {
      if (name.scope() == body) {
        members_tokens.emplace_back(name);
      }
    });

    body.foreach_function(
        [&](bool is_static, Token, Token fn_name, Scope fn_args, bool, Scope fn_body) {
          if (is_static) {
            return;
          }
          fn_args.foreach_match("AA", check_shadowing);
          fn_args.foreach_match("&A", check_shadowing);
          fn_body.foreach_match("AA", check_shadowing);
          fn_body.foreach_match("&A", check_shadowing);
          methods_tokens.emplace_back(fn_name);
        });

    body.foreach_function([&](bool is_static, Token, Token, Scope, bool, Scope fn_body) {
      if (is_static) {
        return;
      }
      fn_body.foreach_token(Word, [&](Token tok) {
        if (!(tok.prev().prev() == '-' && tok.prev() == '>') && tok.prev() != Dot &&
            /* Reject namespace qualified symbols. */
            (tok.prev() != Colon || tok.prev().prev() != Colon))
        {
          if (tok.next() == '(') {
            if (!is_class_token(methods_tokens, tok.str())) {
              return;
            }
          }
          else {
            if (!is_class_token(members_tokens, tok.str())) {
              return;
            }
          }
          parser.insert_before(tok, "this->");
        }
      });
    });
  });

  parser.apply_mutations();
}

/* Move all method definition outside of struct definition blocks. */
void SourceProcessor::lower_method_definitions(Parser &parser)
{
  /* NOTE: We need to avoid the case of `a * this->b` being replaced as 2 dereferences. */

  /* `(*this)` -> `(this_)` */
  parser().foreach_match("*T)", [&](const Tokens &t) { parser.replace(t[0], t[1], "this_"); });
  /* `return *this;` -> `return this_;` */
  parser().foreach_match("*T;", [&](const Tokens &t) { parser.replace(t[0], t[1], "this_"); });
  /* `this->` -> `this_.` */
  parser().foreach_match("T->", [&](const Tokens &t) { parser.replace(t[0], t[2], "this_."); });

  parser.apply_mutations();

  parser().foreach_match("sA:", [&](const Tokens &toks) {
    if (toks[2] == ':') {
      report_error_(ERROR_TOK(toks[2]), "class inheritance is not supported");
      return;
    }
  });

  parser().foreach_match("cAA(..)c?{..}", [&](const Tokens &toks) {
    if (toks[0].prev() == Const) {
      report_error_(ERROR_TOK(toks[0]),
                    "function return type is marked `const` but it makes no sense for values "
                    "and returning reference is not supported");
      return;
    }
  });

  /* Add `this` parameter and fold static keywords into function name. */
  parser().foreach_struct([&](Token struct_tok,
                              Scope,
                              const Token struct_name,
                              const Scope struct_scope) {
    const Scope attributes = struct_tok.prev().scope();
    const bool is_resource_table = (attributes.type() == ScopeType::Subscript) &&
                                   (attributes.str() == "[[resource_table]]");

    if (is_resource_table) {
      parser.replace(attributes, "");
    }

    struct_scope.foreach_function(
        [&](bool is_static, Token fn_type, Token fn_name, Scope fn_args, bool is_const, Scope) {
          const Token static_tok = is_static ? fn_type.prev() : Token::invalid();
          const Token const_tok = is_const ? fn_args.back().next() : Token::invalid();

          if (fn_name.str()[0] == '_') {
            report_error_(ERROR_TOK(fn_name),
                          "function name starting with an underscore are reserved");
          }

          if (is_static) {
            parser.replace(fn_name, struct_name.str() + namespace_separator + fn_name.str());
            /* WORKAROUND: Erase the static keyword as it conflicts with the wrapper class
             * member accesses MSL. */
            parser.erase(static_tok);
          }
          else {
            const bool has_no_args = fn_args.token_count() == 2;
            const char *suffix = (has_no_args ? "" : ", ");
            const string prefix = (is_resource_table ? "[[resource_table]] " : "");

            /* Add a prefix to all member functions. */
            parser.insert_before(fn_name, method_call_prefix);

            parser.erase(const_tok);
            if (is_const && !is_resource_table) {
              parser.insert_after(fn_args.front(),
                                  prefix + "const " + struct_name.str() + " this_" + suffix);
            }
            else {
              parser.insert_after(fn_args.front(),
                                  prefix + struct_name.str() + " &this_" + suffix);
            }

            if (fn_name.str().length() > 1 &&
                (fn_name.str().find_first_not_of("xyzw") == string::npos ||
                 fn_name.str().find_first_not_of("rgba") == string::npos))
            {
              report_error_(ERROR_TOK(fn_name),
                            "Method name matching swizzles accessor are forbidden.");
            }
          }
        });
  });

  parser.apply_mutations();

  /* Copy method functions outside of struct scope. */
  parser().foreach_struct([&](Token, Scope, const Token, const Scope struct_scope) {
    const Token struct_end = struct_scope.back().next();

    int method_len = 0;
    struct_scope.foreach_function([&](bool, Token, Token, Scope, bool, Scope) { method_len++; });
    if (method_len == 0) {
      /* Avoid unnecessary preprocessor directives. */
      return;
    }

    /* Add prototypes to allow arbitrary order of definition inside a class.
     * Can be skipped if there is only one method. */
    if (method_len > 1) {
      /* First output prototypes. Not needed on metal because of wrapper class. */
      parser.insert_after(struct_end, "\n#ifndef GPU_METAL\n");
      struct_scope.foreach_function(
          [&](bool is_static, Token fn_type, Token, Scope fn_args, bool, Scope) {
            const Token fn_start = is_static ? fn_type.prev() : fn_type;

            string proto_str = parser.substr_range_inclusive(fn_start, fn_args.back());
            proto_str = strip_whitespace(proto_str) + ";\n";
            Parser proto(proto_str, report_error_);

            parser.insert_after(struct_end, proto.result_get());
          });
      parser.insert_after(struct_end, "#endif\n");
    }

    struct_scope.foreach_function(
        [&](bool is_static, Token fn_type, Token, Scope, bool, Scope fn_body) {
          const Token fn_start = is_static ? fn_type.prev() : fn_type;

          string fn_str = parser.substr_range_inclusive(fn_start, fn_body.back());
          fn_str = string(fn_start.char_number(), ' ') + fn_str + "\n";

          parser.erase(fn_start, fn_body.back());
          parser.insert_line_number(struct_end, fn_start.line_number());
          parser.insert_after(struct_end, fn_str);
        });

    parser.insert_line_number(struct_end, struct_end.line_number(true));
  });

  parser.apply_mutations();
}

/* Transform `a.fn(b)` into `fn(a, b)`. */
void SourceProcessor::lower_method_calls(Parser &parser)
{
  do {
    parser().foreach_scope(ScopeType::Function, [&](Scope scope) {
      scope.foreach_match(".A(", [&](const vector<Token> &tokens) {
        const Token dot = tokens[0];
        const Token func = tokens[1];
        const Token par_open = tokens[2];
        const Token end_of_this = dot.prev();
        Token start_of_this = end_of_this;
        while (true) {
          if (start_of_this == ')') {
            /* Function call. Take argument scope and function name. No recursion. */
            start_of_this = start_of_this.scope().front().prev();
            break;
          }
          if (start_of_this == ']') {
            /* Array subscript. Take scope and continue. */
            start_of_this = start_of_this.scope().front().prev();
            continue;
          }
          if (start_of_this == Word) {
            /* Member. */
            if (start_of_this.prev() == '.') {
              start_of_this = start_of_this.prev().prev();
              /* Continue until we find root member. */
              continue;
            }
            /* End of chain. */
            break;
          }
          report_error_(start_of_this.line_number(),
                        start_of_this.char_number(),
                        start_of_this.line_str(),
                        "lower_method_call parsing error");
          break;
        }
        string this_str = parser.substr_range_inclusive(start_of_this, end_of_this);
        string func_str = method_call_prefix + func.str();
        const bool has_no_arg = par_open.next() == ')';
        /* `a.fn(b)` -> `_fn(a, b)` */
        parser.replace_try(
            start_of_this, par_open, func_str + "(" + this_str + (has_no_arg ? "" : ", "));
      });
    });
  } while (parser.apply_mutations());
}

}  // namespace blender::gpu::shader

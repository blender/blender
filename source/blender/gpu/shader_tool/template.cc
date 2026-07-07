/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;

string SourceProcessor::template_arguments_mangle(const Scope template_args)
{
  string args_concat;
  template_args.foreach_scope(ScopeType::TemplateArg,
                              [&](const Scope &scope) { args_concat += 'T' + scope.str(); });
  return args_concat;
}

static void parse_template_definition(const Scope arg,
                                      vector<string> &arg_list,
                                      const Scope fn_args,
                                      bool &all_template_args_in_function_signature,
                                      report_callback report_error)
{
  const Token type = arg.front();
  const Token name = type.str() == "enum" ? type.next().next() : type.next();
  const string name_str = name.str();
  const string type_str = type.str();

  arg_list.emplace_back(name_str);

  if (arg.contains_token('=')) {
    report_error(ERROR_TOK(arg[0]),
                 "Default arguments are not supported inside template declaration");
  }

  if (type_str == "typename") {
    bool found = false;
    /* Search argument list for type-names. If type-name matches, the template argument is
     * present inside the function signature. */
    fn_args.foreach_match("AA", [&](const vector<Token> &tokens) {
      if (tokens[0].str() == name_str) {
        found = true;
      }
    });
    all_template_args_in_function_signature &= found;
  }
  else if (type_str == "enum" || type_str == "bool") {
    /* Values cannot be resolved using type deduction. */
    all_template_args_in_function_signature = false;
  }
  else if (type_str == "int" || type_str == "uint" || type_str == "char" || type_str == "uchar" ||
           type_str == "short" || type_str == "ushort")
  {
    /* Values cannot be resolved using type deduction. */
    all_template_args_in_function_signature = false;
  }
  else {
    report_error(ERROR_TOK(type), "Invalid template argument type");
  }
}

static void lower_template_instantiation(SourceProcessor::Parser &parser,
                                         const vector<Token> &toks,
                                         const Scope &parent_scope,
                                         const Token &fn_start,
                                         const Token &fn_name,
                                         const vector<string> &arg_list,
                                         const string &fn_decl,
                                         const bool all_template_args_in_function_signature,
                                         report_callback report_error)
{
  if (toks[2].scope() != parent_scope || fn_name.str() != toks[2].str() ||
      toks[2].str_index_start() < fn_name.str_index_start())
  {
    return;
  }

  const Scope inst_args = toks[3].scope();
  const Token inst_start = toks[0];
  const Token inst_end = toks[0].find_next(SemiColon);

  /* Parse template values. */
  vector<pair<string, string>> arg_name_value_pairs;
  int i = 0;
  toks[3].scope().foreach_scope(ScopeType::TemplateArg, [&](const Scope &arg) {
    if (i < arg_list.size()) {
      arg_name_value_pairs.emplace_back(arg_list[i], arg.str());
    }
    i++;
  });
  if (i != arg_list.size()) {
    report_error(ERROR_TOK(toks[3]), "Invalid amount of argument in template instantiation.");
  }

  /* Specialize template content. */
  SourceProcessor::Parser instance_parser(fn_decl, report_error);
  instance_parser().foreach_token(Word, [&](const Token &word) {
    string token_str = word.str();
    for (const auto &arg_name_value : arg_name_value_pairs) {
      if (token_str == arg_name_value.first) {
        instance_parser.replace(word, arg_name_value.second, true);
      }
    }
  });

  if (!all_template_args_in_function_signature) {
    /* Append template args after function name.
     * `void func() {}` > `void func<a, 1>() {}`. */
    size_t pos = fn_decl.find(" " + fn_name.str());
    instance_parser.insert_after(pos + fn_name.str().size(),
                                 SourceProcessor::template_arguments_mangle(inst_args));
  }
  /* Paste template content in place of instantiation. */
  string instance = instance_parser.result_get();
  parser.erase(inst_start, inst_end);
  parser.insert_line_number(inst_end, fn_start.line_number());
  parser.insert_after(inst_end, instance);
  parser.insert_line_number(inst_end, inst_end.line_number(true));
}

void SourceProcessor::lower_template_dependent_names(Parser &parser)
{
  parser().foreach_match("tA<..>", [&](const Tokens &toks) {
    if (toks[0].prev() == '.' || (toks[0].prev().prev() == '-' && toks[0].prev() == '>')) {
      parser.erase(toks[0]);
    }
  });
  parser.apply_mutations();
}

void SourceProcessor::lower_templates(Parser &parser)
{
  /* Process templated function calls first to avoid matching them later. */

  parser().foreach_match("A<..>(..)", [&](const vector<Token> &tokens) {
    const Scope template_args = tokens[1].scope();
    template_args.foreach_match("A<..>", [&parser](const vector<Token> &tokens) {
      parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
    });
  });
  parser.apply_mutations();

  /* Then Specialization. */

  auto process_specialization = [&](const Token specialization_start, const Scope template_args) {
    parser.erase(specialization_start, specialization_start.next().next());
    parser.replace(template_args, template_arguments_mangle(template_args), true);
  };
  /* Replace full specialization by simple functions. */
  parser().foreach_match("t<>AA<", [&](const vector<Token> &tokens) {
    process_specialization(tokens[0], tokens[5].scope());
  });
  /* Replace full specialization by simple struct. */
  parser().foreach_match("t<>sA<..>", [&](const vector<Token> &tokens) {
    process_specialization(tokens[0], tokens[5].scope());
  });

  parser.apply_mutations();

  auto process_template_struct = [&](Scope template_scope) {
    /* Parse template declaration. */
    Token struct_start = template_scope.back().next();
    if (struct_start != Struct) {
      return;
    }
    Token struct_name = struct_start.next();
    Scope struct_body = struct_name.next().scope();

    Token struct_end = struct_body.back().next();
    const string struct_decl = parser.substr_range_inclusive(struct_start, struct_end);

    vector<string> arg_list;
    bool all_template_args_in_function_signature = false;
    template_scope.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
      parse_template_definition(
          arg, arg_list, Scope::invalid(), all_template_args_in_function_signature, report_error_);
    });

    /* Remove declaration. */
    Token template_keyword = template_scope.front().prev();
    parser.erase(template_keyword, struct_end);

    /* Replace instantiations. */
    Scope parent_scope = template_scope.scope();
    parent_scope.foreach_match("tsA<", [&](const vector<Token> &tokens) {
      lower_template_instantiation(parser,
                                   tokens,
                                   parent_scope,
                                   struct_start,
                                   struct_name,
                                   arg_list,
                                   struct_decl,
                                   all_template_args_in_function_signature,
                                   report_error_);
    });
  };

  parser().foreach_scope(ScopeType::Template, process_template_struct);
  parser().foreach_scope(ScopeType::Namespace, [&](Scope ns_scope) {
    ns_scope.foreach_scope(ScopeType::Template, process_template_struct);
  });
  parser.apply_mutations();

  auto process_template_function = [&](const Token fn_start,
                                       const Token fn_name,
                                       const Scope fn_args,
                                       const Scope template_scope,
                                       const Token fn_end) {
    bool error = false;
    template_scope.foreach_match("=", [&](const vector<Token> &tokens) {
      report_error_(tokens[0].line_number(),
                    tokens[0].char_number(),
                    tokens[0].line_str(),
                    "Default arguments are not supported inside template declaration");
      error = true;
    });
    if (error) {
      return;
    }

    vector<string> arg_list;
    bool all_template_args_in_function_signature = true;
    template_scope.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
      parse_template_definition(
          arg, arg_list, fn_args, all_template_args_in_function_signature, report_error_);
    });

    const string fn_decl = parser.substr_range_inclusive(fn_start, fn_end);

    /* Remove declaration. */
    Token template_keyword = template_scope.front().prev();
    parser.erase(template_keyword, fn_end);

    /* Replace instantiations. */
    Scope parent_scope = template_scope.scope();
    parent_scope.foreach_match("tAA<", [&](const vector<Token> &tokens) {
      lower_template_instantiation(parser,
                                   tokens,
                                   parent_scope,
                                   fn_start,
                                   fn_name,
                                   arg_list,
                                   fn_decl,
                                   all_template_args_in_function_signature,
                                   report_error_);
    });
  };

  parser().foreach_match("t<..>AA(..)c?{..}", [&](const vector<Token> &tokens) {
    process_template_function(
        tokens[5], tokens[6], tokens[7].scope(), tokens[1].scope(), tokens[16]);
  });

  parser.apply_mutations();

  /* Check if there is no remaining declaration and instantiation that were not processed. */
  parser().foreach_token(Template, [&](Token tok) {
    if (tok.next() == '<') {
      report_error_(ERROR_TOK(tok), "Template declaration unsupported syntax");
    }
    else {
      report_error_(ERROR_TOK(tok), "Template instantiation unsupported syntax");
    }
  });

  /* Process calls to templated types or functions. */
  parser().foreach_match("A<..>", [&](const vector<Token> &tokens) {
    parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
  });

  parser.apply_mutations();
}

}  // namespace blender::gpu::shader

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include <algorithm>
#include <unordered_map>

#include "intermediate.hh"
#include "processor.hh"

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace metadata;

string SourceProcessor::template_arguments_mangle(const Scope template_args)
{
  string args_concat;
  template_args.foreach_scope(ScopeType::TemplateArg, [&](const Scope &scope) {
    string str;
    if (scope[1] == '<') {
      str = string(scope[0].str()) + template_arguments_mangle(scope[1].scope());
    }
    else {
      str = scope.str();
    }
    /* In order to support negative integer literals. Replace minus sign by underscore. */
    replace(str.begin(), str.end(), '-', '_');
    args_concat += 'T' + str;
  });
  return args_concat;
}

string SourceProcessor::template_full_specified_name(metadata::TemplateDefinition &template_def)
{
  SourceProcessor::Parser name_parser(template_def.name_space + template_def.identifier,
                                      error_handler);
  lower_scope_resolution_operators(name_parser);
  return name_parser.result_get();
}

static void parse_template_definition_args(const Scope arg,
                                           vector<string> &arg_list,
                                           const Scope fn_args,
                                           bool &all_template_args_in_function_signature,
                                           ErrorHandler &error_handler)
{
  const Token type = arg.front();
  const Token name = type.str() == "enum" ? type.next().next() : type.next();
  const string_view name_str = name.str();
  const string_view type_str = type.str();

  arg_list.emplace_back(name_str);

  if (type_str == "typename") {
    bool found = false;
    /* Search argument list for type-names. If type-name matches, the template argument is
     * present inside the function signature. */
    fn_args.foreach_match("AA", [&](const vector<Token> &tokens) {
      if (tokens[0].str() == name_str) {
        found = true;
      }
    });
    fn_args.foreach_match("A&A", [&](const vector<Token> &tokens) {
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
    error_handler.report(type, "Invalid template argument type");
  }
}

void SourceProcessor::lower_template_instantiation(
    SourceProcessor::Parser &parser,
    /* If method, the end token of the template inside the struct. */
    const Token method_end,
    const Token &inst_start,
    const Scope &inst_args,
    const metadata::TemplateDefinition template_def,
    const Token &symbol_name,
    const vector<string> &arg_list,
    const string &fn_decl,
    const bool all_template_args_in_function_signature)
{
  /* Note that we do not use the full path as file identifier. So all names are unique. */
  string instance_filename = filepath_.substr(filepath_.find_last_of('/') + 1);
  string template_filename = template_def.filepath.substr(template_def.filepath.find_last_of('/') +
                                                          1);
  /* Avoid adding noise in the source file if instance is inside the same file as declaration. */
  if (instance_filename == template_filename) {
    instance_filename = "";
    template_filename = "";
  }

  /* Parse template values. */
  vector<pair<string, string>> arg_name_value_pairs;
  {
    int arg_count = 0;
    inst_args.foreach_scope(ScopeType::TemplateArg, [&](const Scope &arg) {
      if (arg_count < arg_list.size()) {
        arg_name_value_pairs.emplace_back(arg_list[arg_count], arg.str());
      }
      arg_count++;
    });
    if (arg_count != arg_list.size()) {
      report_error(inst_args.front(), "Invalid amount of argument in template instantiation.");
      return;
    }
  }

  /* Specialize template content. */
  string instance_content;
  {
    SourceProcessor::Parser instance_parser(fn_decl, error_handler);

    /* Inject namespace around definition for symbols namespaces resolution. */
    if (template_def.name_space.empty()) {
      instance_parser.insert_before(instance_parser.front(), "\n");
      instance_parser.insert_after(instance_parser.back(), "\n");
    }
    else {
      /* Remove suffix "::". */
      string ns_name(template_def.name_space.substr(0, template_def.name_space.size() - 2));

      if (template_def.is_method) {
        size_t split = ns_name.rfind("::");
        string struct_name;

        if (split == string::npos) {
          struct_name = ns_name;
          ns_name = "";
        }
        else {
          struct_name = ns_name.substr(split + 2);
          ns_name = ns_name.substr(0, split);
        }
        if (!ns_name.empty()) {
          instance_parser.insert_before(instance_parser.front(), "namespace " + ns_name + " {");
        }
        instance_parser.insert_before(instance_parser.front(), "struct " + struct_name + " {\n");
        instance_parser.insert_after(instance_parser.back(), "\n};");
        if (!ns_name.empty()) {
          instance_parser.insert_after(instance_parser.back(), "}");
        }
        instance_parser.insert_after(instance_parser.back(), "\n");
      }
      else {
        instance_parser.insert_before(instance_parser.front(), "namespace " + ns_name + " {\n");
        instance_parser.insert_after(instance_parser.back(), "\n}\n");
      }
    }
    /* Insert line directive. Important for symbol namespace resolution and error logging.
     * Not using insert_line_number because it uses insert_after. */
    string line_str = "\n#line " + std::to_string(template_def.definition_line);
    if (!template_filename.empty()) {
      line_str += " \"" + template_filename + '\"';
    }
    instance_parser.insert_before(instance_parser.front(), line_str + "\n");

    instance_parser().foreach_token(Word, [&](const Token &word) {
      string_view token_str = word.str();

      /* Replace each parameter appearance inside the instance. */
      for (const auto &arg_name_value : arg_name_value_pairs) {
        if (token_str == arg_name_value.first) {
          instance_parser.replace(word, arg_name_value.second, true);
        }
      }
      /* Append template args after unspecified struct typename references.
       * `A func(A<T> b) {}` > `A<T> func(A<T> b) {}`. */
      if (template_def.is_struct && word.next() != AngleOpen && token_str == symbol_name.str()) {
        instance_parser.insert_after(word.str_index_last_no_whitespace(),
                                     SourceProcessor::template_arguments_mangle(inst_args));
      }
    });

    /* Position of symbol name declaration. */
    size_t symbol_name_pos = instance_parser.str().find(" " + string(symbol_name.str()));

    /* Append template args after function name if needed. This is required by BSL specification.
     * `void func() {}` > `void func<a, 1>() {}`. */
    if (!template_def.is_struct && !all_template_args_in_function_signature) {
      instance_parser.insert_after(symbol_name_pos + symbol_name.str().size(),
                                   SourceProcessor::template_arguments_mangle(inst_args));
    }
    /* Append namespace to symbol name because the appended mangled arguments (above) make
     * namespace resolution impossible. Methods do not need it because they are instanciated inside
     * their struct. They will get the correct namespace prefix (if they are static) later on. */
    if (!template_def.is_method) {
      instance_parser.insert_after(symbol_name_pos, string(template_def.name_space));
    }

    instance_parser.apply_mutations();

    lower_pre_template(instance_parser);

    /* Paste template content in place of instantiation. */
    instance_content = instance_parser.result_get();
    /* Remove added first line from the injected namespace. */
    instance_content = instance_content.substr(instance_content.find_first_of('\n') + 1);
    if (template_def.is_method) {
      /* Remove added last line from the injected struct + namespace. */
      instance_content = instance_content.substr(
          0, instance_content.find_last_of('\n', instance_content.size() - 2) + 1);
    }
  }

  const Token inst_end = inst_start.find_next(SemiColon);
  /* Method are put back in their classes. */
  const Token insert_at = template_def.is_method ? method_end : inst_end;

  /* Insert instantiation content. Instance line directived was already added. */
  parser.insert_after(insert_at, instance_content);
  parser.insert_line_number(insert_at, insert_at.line_number(true), instance_filename);
}

void SourceProcessor::lower_template_dependent_names(Parser &parser)
{
  parser().foreach_match("tA<..>", [&](const Tokens &toks) {
    if (toks[0].prev() == '.' || (toks[0].prev().prev() == '-' && toks[0].prev() == '>')) {
      parser.erase(toks[0]);
    }
  });
}

void SourceProcessor::lower_pre_template(Parser &parser)
{
  /* Lower noop attributes after linting them. */
  lower_maybe_unused(parser);
  /* Lint and remove C++ accessor templates before lowering template. */
  lower_srt_accessor_templates(parser);
  lower_union_accessor_templates(parser);
  /* Lower namespaces. */
  lower_using(parser);
  lower_namespaces(parser);
  lower_scope_resolution_operators(parser);

  lower_template_calls(parser);
  lower_template_specialization(parser);
}

/* Mangle template parameter into the symbol name. */
void SourceProcessor::lower_template_calls(Parser &parser)
{
  /* Process templated function calls first to avoid matching them later. */
  parser().foreach_match("A<..>(..)", [&](const vector<Token> &tokens) {
    const Scope template_args = tokens[1].scope();
    template_args.foreach_match("A<..>", [&parser](const vector<Token> &tokens) {
      parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
    });
  });
  /* Likewise, process templated struct method definitions. */
  parser().foreach_match("A<..>A<", [&](const vector<Token> &tokens) {
    parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
  });

  parser.apply_mutations();
}

void SourceProcessor::lower_template_specialization(Parser &parser)
{
  auto process_specialization = [&](const Token specialization_start, const Scope template_args) {
    parser.erase(specialization_start, specialization_start.next().next());
    parser.replace(template_args, template_arguments_mangle(template_args), true);
  };

  parser().foreach_match("t<>AA<", [&](const vector<Token> &tokens) {
    process_specialization(tokens[0], tokens[5].scope());
  });

  parser().foreach_match("t<>sA<..>", [&](const vector<Token> &tokens) {
    process_specialization(tokens[0], tokens[5].scope());
  });

  parser.apply_mutations();
}

void SourceProcessor::process_template_struct(metadata::TemplateDefinition &template_def,
                                              SourceProcessor::Parser &parser)
{
  struct DefinitionParser {
    SourceProcessor::Parser def_parser;
    const Scope template_scope = def_parser[1].scope();
    const Token struct_start = template_scope.back().next();
    const Token struct_name = struct_start.next();
    const Scope struct_body = struct_name.next().scope();
    const Token struct_end = struct_body.back().next();
    const string struct_decl = def_parser.substr_range_inclusive(struct_start, struct_end);
    bool all_template_args_in_function_signature = false;
    vector<string> arg_list;

    /* Parse template declaration. */
    DefinitionParser(metadata::TemplateDefinition &template_def, ErrorHandler report_error)
        : def_parser(template_def.definition, report_error)
    {
      template_scope.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
        parse_template_definition_args(arg,
                                       arg_list,
                                       Scope(def_parser),
                                       all_template_args_in_function_signature,
                                       report_error);
      });
    }
  };

  /* Only parse if there is an instantiation. */
  unique_ptr<DefinitionParser> def_parser;
  /* Since we already lowered the namespaces in main parser, we need to search for the namespace
   * resolved symbol name. */
  const string full_specified_name = template_full_specified_name(template_def);
  /* Replace instantiations. */
  parser().foreach_match("tsA<", [&](const vector<Token> &tokens) {
    if (full_specified_name != tokens[2].str()) {
      return;
    }

    if (!def_parser) {
      def_parser = make_unique<DefinitionParser>(template_def, error_handler);
    }

    lower_template_instantiation(parser,
                                 Token::invalid(&parser),
                                 tokens[0],
                                 tokens[3].scope(),
                                 template_def,
                                 def_parser->struct_name,
                                 def_parser->arg_list,
                                 def_parser->struct_decl,
                                 def_parser->all_template_args_in_function_signature);
  });
}

void SourceProcessor::process_template_function(
    metadata::TemplateDefinition &template_def,
    SourceProcessor::Parser &parser,
    /* If method, the end token of the template inside the struct. */
    const Token method_end)
{
  struct DefinitionParser {
    SourceProcessor::Parser def_parser;
    const Scope template_scope = def_parser[1].scope();
    const Token fn_start = template_scope.back().next();
    /* Skip attributes */
    const Token after_attr = fn_start == SquareOpen ? fn_start.scope().back().next() : fn_start;
    const Scope fn_args = after_attr.find_next(ParOpen).scope();
    const Token fn_name = fn_args.front().prev();
    const Token fn_end = fn_args.back().find_next(BracketOpen).scope().back();
    const string fn_decl = def_parser.substr_range_inclusive(fn_start, fn_end);

    bool all_template_args_in_function_signature = true;
    vector<string> arg_list;

    /* Parse template declaration. */
    DefinitionParser(metadata::TemplateDefinition &template_def, ErrorHandler report_error)
        : def_parser(template_def.definition, report_error)
    {
      assert(fn_start.is_valid() && fn_name.is_valid() && fn_args.is_valid() &&
             template_scope.is_valid() && fn_end.is_valid());

      template_scope.foreach_scope(ScopeType::TemplateArg, [&](Scope arg) {
        parse_template_definition_args(
            arg, arg_list, fn_args, all_template_args_in_function_signature, report_error);
      });
    }
  };

  /* Only parse if there is an instantiation. */
  unique_ptr<DefinitionParser> def_parser;
  /* Since we already lowered the namespaces in main parser, we need to search for the namespace
   * resolved symbol name. */
  const string full_specified_name = template_full_specified_name(template_def);
  /* Replace instantiations. */
  parser().foreach_match("tAA<", [&](const vector<Token> &tokens) {
    if (full_specified_name != tokens[2].str()) {
      return;
    }

    if (!def_parser) {
      def_parser = make_unique<DefinitionParser>(template_def, error_handler);
    }

    lower_template_instantiation(parser,
                                 method_end,
                                 tokens[0],
                                 tokens[3].scope(),
                                 template_def,
                                 def_parser->fn_name,
                                 def_parser->arg_list,
                                 def_parser->fn_decl,
                                 def_parser->all_template_args_in_function_signature);
  });
}

void SourceProcessor::lower_templates(Parser &parser)
{
  /* Lint missing template arguments in instantiation and specialization.
   * This is required by the BSL spec in order to simplify implementation. */
  auto lint_explicit = [&](const Token symbol_name) {
    if (symbol_name.next().scope().type() != parser::ScopeType::Template) {
      report_error(
          symbol_name,
          "Template instantiation and specialization require explicit template arguments");
    }
  };
  parser().foreach_match("t<>AA", [&](const vector<Token> &toks) { lint_explicit(toks[4]); });
  parser().foreach_match("t<>A<..>A", [&](const vector<Token> &toks) { lint_explicit(toks[8]); });
  parser().foreach_match("tAA", [&](const vector<Token> &toks) { lint_explicit(toks[2]); });
  parser().foreach_match("tA<..>A", [&](const vector<Token> &toks) { lint_explicit(toks[6]); });

  /* Delete templated struct and function definitions (not methods! see later).
   * They were already parsed by `SourceProcessor::parse_namespace_symbols`. */
  bool error = false;
  parser().foreach_match("t<..>", [&](const vector<Token> &toks) {
    /* Only process global templates, not methods. */
    if (toks[0].scope() != parser()) {
      return;
    }
    /* Default arguments are not supported. */
    toks[1].scope().foreach_token(Assign, [&](Token tok) {
      report_error(tok, "Default arguments are not supported inside template declaration");
      error = true;
    });

    Token end = toks[0].find_next(BracketOpen).scope().back();
    if (toks[4].next() == Struct) {
      /* Capture end semicolon. */
      end = end.next();
    }
    /* This can fail as it might try to erase templated method inside templated struct. */
    parser.erase_try(toks[0], end);
  });

  parser.apply_mutations();

  /* Deduplicate symbols (can happen because the main file is parsed twice). */
  unordered_map<string, TemplateDefinition> unique_symbols;
  for (const auto &symbol : metadata_.template_definitions) {
    unique_symbols.try_emplace(symbol.name_space + symbol.identifier, symbol);
  }

  /* Process struct first, so methods can instantiate inside them. */
  for (auto [_, template_def] : unique_symbols) {
    if (template_def.is_struct) {
      process_template_struct(template_def, parser);
    }
  }
  parser.apply_mutations();

  /* Then process methods. We can only process methods if their struct exists in the same file.
   * This holds true for instanciated struct templates. */
  parser().foreach_struct([&](Token, Scope, Token, Scope body) {
    body.foreach_match("t<..>", [&](const vector<Token> &toks) {
      /* Since this can be an instanciated struct, we need to make sure to instanciate its own
       * methods. Hence the need to parse the definition. */
      TemplateDefinition template_def = parse_template_definition(
          parser, toks[0], true, toks[0].scope(), filepath_);
      /* Insertion point of the instantiation. */
      Token method_end = toks[0].find_next(BracketOpen).scope().back();

      process_template_function(template_def, parser, method_end);
      /* Delete definitions. */
      parser.erase(toks[0], method_end);
    });
  });

  /* For each template definition, process all instantiation. */
  for (auto [_, template_def] : unique_symbols) {
    if (!template_def.is_struct && !template_def.is_method) {
      process_template_function(template_def, parser, Token::invalid(&parser));
    }
  }
  parser.apply_mutations();

  /* Remove template instantiation afterward. */
  parser().foreach_token(Template, [&](const Token &tok) {
    if (tok.next() == '<') {
      report_error(tok, "Invalid template definition");
    }
    else {
      parser.erase(tok, tok.find_next(SemiColon));
    }
  });

  parser.apply_mutations();

  /* Process calls to templated types or functions. */
  parser().foreach_match("A<..>", [&](const vector<Token> &tokens) {
    parser.replace(tokens[1].scope(), template_arguments_mangle(tokens[1].scope()), true);
  });

  parser.apply_mutations();
}

}  // namespace blender::gpu::shader

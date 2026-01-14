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

/**
 * For safety reason, nested resource tables need to be declared with the srt_t template.
 * This avoid chained member access which isn't well defined with the preprocessing we are doing.
 *
 * This linting phase make sure that [[resource_table]] members uses it and that no incorrect
 * usage is made. We also remove this template because it has no real meaning.
 *
 * Need to run before lower_resource_table.
 */
void SourceProcessor::lower_srt_accessor_templates(Parser &parser)
{
  parser().foreach_struct([&](Token, Scope, Token, Scope body) {
    body.foreach_declaration([&](Scope attributes,
                                 Token,
                                 Token type,
                                 Scope template_scope,
                                 Token name,
                                 Scope array,
                                 Token) {
      if (attributes[1].str() != "resource_table") {
        if (type.str() == "srt_t") {
          report_error_(ERROR_TOK(name),
                        "The srt_t<T> template is only to be used with members declared with the "
                        "[[resource_table]] attribute.");
        }
        return;
      }

      if (type.str() != "srt_t") {
        report_error_(
            ERROR_TOK(type),
            "Members declared with the [[resource_table]] attribute must wrap their type "
            "with the srt_t<T> template.");
      }

      if (array.is_valid()) {
        report_error_(ERROR_TOK(name), "[[resource_table]] members cannot be arrays.");
      }

      /* Remove the template but not the wrapped type. */
      parser.erase(type);
      if (template_scope.is_valid()) {
        parser.erase(template_scope.front());
        parser.erase(template_scope.back());
      }
    });
  });
  parser.apply_mutations();
}

/* Add `srt_access` around all member access of SRT variables.
 * Need to run before local reference mutations. */
void SourceProcessor::lower_srt_member_access(Parser &parser)
{
  const string srt_attribute = "resource_table";

  auto memher_access_mutation = [&](Scope attribute, Token type, Token var, Scope body_scope) {
    if (attribute[2].str() != srt_attribute) {
      return;
    }

    const bool is_func_prototype_decl = body_scope.is_invalid();
    const bool is_local_reference = attribute.scope().type() != ScopeType::FunctionArgs &&
                                    attribute.scope().type() != ScopeType::FunctionArg;

    if (is_local_reference || is_func_prototype_decl) {
      parser.replace(attribute, "");
    }

    /* Change references to copies to allow placeholder "*_new_()" function result to be passed
     * as argument. Once these placeholder function are removed, we can pass the value as
     * reference. */
    if (!is_local_reference && var.prev() == '&') {
      parser.erase(var.prev());
    }

    string srt_type = type.str();
    string srt_var = var.str();

    body_scope.foreach_match("w.w", [&](const vector<Token> toks) {
      if (toks[0].str() != srt_var) {
        return;
      }
      parser.replace(
          toks[0], toks[2], "srt_access(" + srt_type + ", " + toks[2].str() + ")", true);
    });
  };

  parser().foreach_scope(ScopeType::FunctionArgs, [&](const Scope fn_args) {
    /* Parse both function and prototypes. */
    Scope fn_body = fn_args.next().type() == ScopeType::Function ? fn_args.next() :
                                                                   Scope::invalid();
    /* Function arguments. */
    fn_args.foreach_match("[[w]]c?w&w", [&](const vector<Token> toks) {
      memher_access_mutation(toks[0].scope(), toks[7], toks[9], fn_body);
    });
    fn_args.foreach_match("[[w]]c?ww", [&](const vector<Token> toks) {
      if (toks[2].str() == srt_attribute) {
        parser.erase(toks[0].scope());
        report_error_(ERROR_TOK(toks[8]), "Shader Resource Table arguments must be references.");
      }
    });
  });

  parser().foreach_scope(ScopeType::Function, [&](const Scope fn_body) {
    /* Local references. */
    fn_body.foreach_match("[[w]]c?w&w", [&](const vector<Token> toks) {
      memher_access_mutation(toks[0].scope(), toks[7], toks[9], toks[9].scope());
    });
    /* Local variables. */
    fn_body.foreach_match("[[w]]c?ww", [&](const vector<Token> toks) {
      memher_access_mutation(toks[0].scope(), toks[7], toks[8], toks[8].scope());
    });
  });

  parser.apply_mutations();
}

/* Add #ifdef directive around functions using SRT arguments.
 * Need to run after `lower_entry_points_signature`. */
void SourceProcessor::lower_srt_arguments(Parser &parser)
{
  /* SRT arguments. */
  parser().foreach_function([&](bool, Token fn_type, Token, Scope fn_args, bool, Scope fn_body) {
    string condition;
    fn_args.foreach_match("[[w]]c?w", [&](const vector<Token> &tokens) {
      if (tokens[2].str() != "resource_table") {
        return;
      }
      condition += " && defined(CREATE_INFO_" + tokens[7].str() + ")";
      parser.replace(tokens[0].scope(), "");
    });

    if (!condition.empty()) {
      /* Take attribute into account. */
      Token first_tok = fn_type.prev() == ']' ? fn_type.prev().scope().front() : fn_type;
      parser.insert_directive(first_tok.prev(), "#if " + condition.substr(4));
      parser.insert_directive(fn_body.back(), "#endif");
    }
  });

  parser.apply_mutations();
}

/* Add ifdefs guards around scopes using resource accessors. */
void SourceProcessor::lower_resource_access_functions(Parser &parser)
{
  /* Legacy access macros. */
  parser().foreach_function([&](bool, Token fn_type, Token, Scope, bool, Scope fn_body) {
    fn_body.foreach_match("w(w,", [&](const vector<Token> &tokens) {
      string func_name = tokens[0].str();
      if (func_name != "specialization_constant_get" && func_name != "shared_variable_get" &&
          func_name != "push_constant_get" && func_name != "interface_get" &&
          func_name != "attribute_get" && func_name != "buffer_get" &&
          func_name != "sampler_get" && func_name != "image_get")
      {
        return;
      }
      string info_name = tokens[2].str();
      Scope scope = tokens[0].scope();
      /* We can be in expression scope. Take parent scope until we find a local scope. */
      while (scope.type() != ScopeType::Function && scope.type() != ScopeType::Local) {
        scope = scope.scope();
      }

      string condition = "defined(CREATE_INFO_" + info_name + ")";

      if (scope.type() == ScopeType::Function) {
        guarded_scope_mutation(parser, scope, condition, fn_type);
      }
      else {
        guarded_scope_mutation(parser, scope, condition);
      }
    });
  });

  parser.apply_mutations();
}

/**
 * Needs to run before namespace mutation so that `using` have more precedence.
 * Otherwise the following would fail.
 * \code{.cc}
 * namespace B {
 * int test(int a) {}
 * }
 *
 * namespace A {
 * int test(int a) {}
 * int func(int a) {
 *   using B::test;
 *   return test(a); // Should reference B::test and not A::test
 * }
 * \endcode
 */
void SourceProcessor::lower_using(Parser &parser)
{
  parser().foreach_match("un", [&](const vector<Token> &tokens) {
    report_error_(ERROR_TOK(tokens[0]),
                  "Unsupported `using namespace`. "
                  "Add individual `using` directives for each needed symbol.");
  });

  auto process_using = [&](const Token &using_tok,
                           const Token &from,
                           const Token &to_start,
                           const Token &to_end,
                           const Token &end_tok) {
    string to = parser.substr_range_inclusive(to_start, to_end);
    string namespace_prefix = parser.substr_range_inclusive(to_start, to_end.prev().prev().prev());
    Scope scope = from.scope();

    /* Using the keyword in global or at namespace scope. */
    if (scope.type() == ScopeType::Global) {
      report_error_(ERROR_TOK(using_tok), "The `using` keyword is not allowed in global scope.");
      return;
    }
    if (scope.type() == ScopeType::Namespace) {
      /* Ensure we are bringing symbols from the same namespace.
       * Otherwise we can have different shadowing outcome between shader and C++. */
      string namespace_name = scope.front().prev().full_symbol_name();
      if (namespace_name != namespace_prefix) {
        report_error_(
            ERROR_TOK(using_tok),
            "The `using` keyword is only allowed in namespace scope to make visible symbols "
            "from the same namespace declared in another scope, potentially from another "
            "file.");
        return;
      }
    }

    /* Assignments do not allow to alias functions symbols. */
    const bool use_alias = from.str() != to_end.str();
    const bool replace_fn = !use_alias;
    /** IMPORTANT: If replace_fn is true, this can replace any symbol type if there are functions
     * and types with the same name. We could support being more explicit about the type of
     * symbol to replace using an optional attribute [[gpu::using_function]]. */

    /* Replace all occurrences of the non-namespace specified symbol. */
    scope.foreach_token(Word, [&](const Token &token) {
      /* Do not replace symbols before the using statement. */
      if (token.index <= to_end.index) {
        return;
      }
      /* Reject symbols that contain the target symbol name. */
      if (token.prev() == ':') {
        return;
      }
      if (!replace_fn && token.next() == '(') {
        return;
      }
      if (token.str() != from.str()) {
        return;
      }
      parser.replace(token, to, true);
    });

    parser.erase(using_tok, end_tok);
  };

  parser().foreach_match("uw::w", [&](const vector<Token> &tokens) {
    Token end = tokens.back().find_next(SemiColon);
    process_using(tokens[0], end.prev(), tokens[1], end.prev(), end);
  });

  parser().foreach_match("uw=w::w", [&](const vector<Token> &tokens) {
    Token end = tokens.back().find_next(SemiColon);
    process_using(tokens[0], tokens[1], tokens[3], end.prev(), end);
  });

  parser.apply_mutations();

  /* Verify all using were processed. */
  parser().foreach_token(Using, [&](const Token &token) {
    report_error_(ERROR_TOK(token), "Unsupported `using` keyword usage.");
  });
}

void SourceProcessor::lower_scope_resolution_operators(Parser &parser)
{
  parser().foreach_match("::", [&](const vector<Token> &tokens) {
    if (tokens[0].scope().type() == ScopeType::Attribute) {
      return;
    }
    if (tokens[0].prev() != Word) {
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

/* Parse SRT and interfaces, remove their attributes and create init function for SRT structs. */
void SourceProcessor::lower_resource_table(Parser &parser)
{
  enum class SrtType {
    undefined,
    none,
    resource_table,
    vertex_input,
    vertex_output,
    fragment_output,
  };

  auto parse_resource = [&](Scope attributes, Token type, Token name, Scope array) {
    metadata::ParsedResource resource{
        type.line_number(), type.str(), name.str(), array.str_with_whitespace()};
    attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
      string type = attribute[0].str();
      if (type == "sampler") {
        resource.res_type = type;
        resource.res_slot = attribute[2].str();
      }
      else if (type == "image") {
        resource.res_type = type;
        resource.res_slot = attribute[2].str();
        resource.res_qualifier = attribute[4].str();
        resource.res_format = attribute[6].str();
      }
      else if (type == "uniform") {
        resource.res_type = type;
        resource.res_slot = attribute[2].str();
      }
      else if (type == "storage") {
        resource.res_type = type;
        resource.res_slot = attribute[2].str();
        resource.res_qualifier = attribute[4].str();
      }
      else if (type == "push_constant") {
        resource.res_type = type;
      }
      else if (type == "compilation_constant") {
        resource.res_type = type;
      }
      else if (type == "specialization_constant") {
        resource.res_type = type;
        resource.res_value = attribute[2].str();
      }
      else if (type == "condition") {
        attribute[1].scope().foreach_token(Word, [&](const Token tok) {
          resource.res_condition += "int " + tok.str() + " = ";
          resource.res_condition += "ShaderCreateInfo::find_constant(constants, \"" + tok.str() +
                                    "\"); ";
        });
        resource.res_condition += "return " + attribute[1].scope().str() + ";";
      }
      else if (type == "frequency") {
        resource.res_frequency = attribute[2].str();
      }
      else if (type == "resource_table") {
        resource.res_type = type;
      }
      else if (type == "legacy_info") {
        resource.res_type = type;
      }
      else {
        report_error_(ERROR_TOK(attribute[0]), "Invalid attribute in resource table");
      }
    });
    return resource;
  };

  auto parse_vertex_input = [&](Scope attributes, Token type, Token name, Scope array) {
    if (array.is_valid()) {
      report_error_(ERROR_TOK(array[0]), "Array are not supported as vertex attributes");
    }

    metadata::ParsedVertInput vert_in{type.line_number(), type.str(), name.str()};

    if (vert_in.var_type == "float3x3" || vert_in.var_type == "float2x2" ||
        vert_in.var_type == "float4x4" || vert_in.var_type == "float3x4")
    {
      report_error_(ERROR_TOK(name), "Matrices are not supported as vertex attributes");
    }

    attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
      string type = attribute[0].str();
      if (type == "attribute") {
        vert_in.slot = attribute[2].str();
      }
      else {
        report_error_(ERROR_TOK(attribute[0]), "Invalid attribute in vertex input interface");
      }
    });
    return vert_in;
  };

  auto parse_vertex_output =
      [&](Token struct_name, Scope attributes, Token type, Token name, Scope array) {
        if (array.is_valid()) {
          report_error_(ERROR_TOK(array[0]), "Array are not supported in stage interface");
        }

        Token interpolation_mode = attributes[1];

        metadata::ParsedAttribute attr{type.line_number(),
                                       type.str(),
                                       struct_name.str() + "_" + name.str(),
                                       interpolation_mode.str()};

        if (attr.var_type == "float3x3" || attr.var_type == "float2x2" ||
            attr.var_type == "float4x4" || attr.var_type == "float3x4")
        {
          report_error_(ERROR_TOK(name), "Matrices are not supported in stage interface");
        }

        if (attr.interpolation_mode != "smooth" && attr.interpolation_mode != "flat" &&
            attr.interpolation_mode != "no_perspective")
        {
          report_error_(ERROR_TOK(attributes[0]), "Invalid attribute in shader stage interface");
        }
        return attr;
      };

  auto parse_fragment_output =
      [&](Token struct_name, Scope attributes, Token tok_type, Token name, Scope) {
        metadata::ParsedFragOuput frag_out{
            tok_type.line_number(), tok_type.str(), struct_name.str() + "_" + name.str()};

        attributes.foreach_scope(ScopeType::Attribute, [&](const Scope &attribute) {
          string type = attribute[0].str();
          if (type == "frag_color") {
            frag_out.slot = attribute[2].str();
          }
          else if (type == "raster_order_group") {
            frag_out.raster_order_group = attribute[2].str();
          }
          else if (type == "index") {
            frag_out.dual_source = attribute[2].str();
          }
          else {
            report_error_(ERROR_TOK(attributes[0]),
                          "Invalid attribute in fragment output interface");
          }
        });
        return frag_out;
      };

  auto is_resource_table_attribute = [](Token attr) {
    string type = attr.str();
    return (type == "sampler" || type == "image" || type == "uniform" || type == "storage" ||
            type == "push_constant" || type == "compilation_constant" ||
            type == "compilation_constant" || type == "legacy_info" || type == "resource_table");
  };
  auto is_vertex_input_attribute = [](Token attr) {
    string type = attr.str();
    return (type == "attribute");
  };
  auto is_vertex_output_attribute = [](Token attr) {
    string type = attr.str();
    return (type == "flat" || type == "smooth" || type == "no_perspective");
  };
  auto is_fragment_output_attribute = [](Token attr) {
    string type = attr.str();
    return (type == "frag_color" || type == "frag_depth" || type == "frag_stencil_ref");
  };

  parser().foreach_struct([&](Token struct_tok, Scope, Token struct_name, Scope body) {
    SrtType srt_type = SrtType::undefined;
    bool has_srt_members = false;

    metadata::ResourceTable srt;
    metadata::VertexInputs vertex_in;
    metadata::StageInterface vertex_out;
    metadata::FragmentOutputs fragment_out;
    srt.name = struct_name.str();
    vertex_in.name = struct_name.str();
    vertex_out.name = struct_name.str();
    fragment_out.name = struct_name.str();

    body.foreach_declaration([&](Scope attributes,
                                 Token const_tok,
                                 Token type,
                                 Scope /*template_scope TODO */,
                                 Token name,
                                 Scope array,
                                 Token decl_end) {
      SrtType decl_type = SrtType::undefined;
      if (attributes.is_invalid()) {
        decl_type = SrtType::none;
      }
      else if (is_resource_table_attribute(attributes[1])) {
        decl_type = SrtType::resource_table;
      }
      else if (is_vertex_input_attribute(attributes[1])) {
        decl_type = SrtType::vertex_input;
      }
      else if (is_vertex_output_attribute(attributes[1])) {
        decl_type = SrtType::vertex_output;
      }
      else if (is_fragment_output_attribute(attributes[1])) {
        decl_type = SrtType::fragment_output;
      }
      else {
        return;
      }

      if (srt_type == SrtType::undefined) {
        srt_type = decl_type;
      }
      else if (srt_type != decl_type) {
        switch (srt_type) {
          case SrtType::resource_table:
            report_error_(ERROR_TOK(struct_name), "Structure expected to contain resources...");
            break;
          case SrtType::vertex_input:
            report_error_(ERROR_TOK(struct_name),
                          "Structure expected to contain vertex inputs...");
            break;
          case SrtType::vertex_output:
            report_error_(ERROR_TOK(struct_name),
                          "Structure expected to contain vertex outputs...");
            break;
          case SrtType::fragment_output:
            report_error_(ERROR_TOK(struct_name),
                          "Structure expected to contain fragment inputs...");
            break;
          case SrtType::none:
            report_error_(ERROR_TOK(struct_name), "Structure expected to contain plain data...");
            break;
          case SrtType::undefined:
            break;
        }

        switch (decl_type) {
          case SrtType::resource_table:
            report_error_(ERROR_TOK(attributes[1]), "...but member declared as resource.");
            break;
          case SrtType::vertex_input:
            report_error_(ERROR_TOK(attributes[1]), "...but member declared as vertex input.");
            break;
          case SrtType::vertex_output:
            report_error_(ERROR_TOK(attributes[1]), "...but member declared as vertex output.");
            break;
          case SrtType::fragment_output:
            report_error_(ERROR_TOK(attributes[1]), "...but member declared as fragment output.");
            break;
          case SrtType::none:
            report_error_(ERROR_TOK(name), "...but member declared as plain data.");
            break;
          case SrtType::undefined:
            break;
        }
      }

      switch (decl_type) {
        case SrtType::resource_table:
          srt.emplace_back(parse_resource(attributes, type, name, array));
          if (attributes[1].str() == "resource_table") {
            has_srt_members = true;
            parser.erase(attributes.scope());
            parser.erase(const_tok);
          }
          else {
            parser.erase(attributes.front().line_start(), decl_end.line_end());
          }
          break;
        case SrtType::vertex_input:
          vertex_in.emplace_back(parse_vertex_input(attributes, type, name, array));
          parser.erase(attributes.scope());
          break;
        case SrtType::vertex_output:
          vertex_out.emplace_back(parse_vertex_output(struct_name, attributes, type, name, array));
          parser.erase(attributes.scope());
          break;
        case SrtType::fragment_output:
          fragment_out.emplace_back(
              parse_fragment_output(struct_name, attributes, type, name, array));
          parser.erase(attributes.scope());
          break;
        case SrtType::undefined:
        case SrtType::none:
          break;
      }
    });

    switch (srt_type) {
      case SrtType::resource_table:
        metadata_.resource_tables.emplace_back(srt);
        break;
      case SrtType::vertex_input:
        metadata_.vertex_inputs.emplace_back(vertex_in);
        break;
      case SrtType::vertex_output:
        metadata_.stage_interfaces.emplace_back(vertex_out);
        break;
      case SrtType::fragment_output:
        metadata_.fragment_outputs.emplace_back(fragment_out);
        break;
      case SrtType::undefined:
      case SrtType::none:
        break;
    }

    Token end_of_srt = body.back().prev();

    if (srt_type == SrtType::resource_table) {
      /* Add static constructor.
       * These are only to avoid warnings on certain backend compilers. */
      string ctor;
      ctor += "\nstatic " + srt.name + " new_()\n";
      ctor += "{\n";
      ctor += "  " + srt.name + " result;\n";
      if (has_srt_members == false) {
        ctor += "  result._pad = 0;\n";
      }
      for (const auto &member : srt) {
        if (member.res_type == "resource_table") {
          ctor += "  result." + member.var_name + " = " + member.var_type + "::new_();\n";
        }
      }
      ctor += "  return result;\n";
      /* Avoid messing up the line count and keep empty struct empty. */
      ctor += "#line " + to_string(end_of_srt.line_number()) + "\n";
      ctor += "}\n";
      parser.insert_after(end_of_srt, ctor);

      string access_macros;
      for (const auto &member : srt) {
        if (member.res_type == "resource_table") {
          access_macros += "#define access_" + srt.name + "_" + member.var_name + "() ";
          access_macros += member.var_type + "::new_()\n";
        }
        else {
          access_macros += "#define access_" + srt.name + "_" + member.var_name + "() ";
          access_macros += member.var_name + "\n";
        }
      }
      parser.insert_before(struct_tok, access_macros);
      parser.insert_before(struct_tok, get_create_info_placeholder(srt.name));

      parser.insert_before(struct_tok, "\n");
      parser.insert_line_number(struct_tok.str_index_start() - 1, struct_tok.line_number());

      /* Insert attribute so that method mutations know that this struct is an SRT. */
      parser.insert_before(struct_tok, "[[resource_table]] ");
    }
  });
  parser.apply_mutations();
}

}  // namespace blender::gpu::shader

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "intermediate.hh"
#include "metadata.hh"
#include "processor.hh"

#include <unordered_set>

namespace blender::gpu::shader {
using namespace std;
using namespace shader::parser;
using namespace shader::parser::ast;
using namespace metadata;

void SourceProcessor::lower_maybe_unused(Parser &parser)
{
  parser().foreach_token(SquareOpen, [&](Token par_open) {
    if (par_open.next() != '[') {
      return;
    }
    Scope attributes = par_open.next().scope();
    attributes.foreach_attribute([&](Token attr, Scope) {
      if (attr.str() == "maybe_unused") {
        if (attr.next() == ',') {
          parser.erase(attr, attr.next());
        }
        else if (attr.prev() == ',') {
          parser.erase(attr.prev(), attr);
        }
        else if (attr.next() == ']') {
          parser.erase(attributes.scope());
        }
      }
    });
  });

  parser.apply_mutations();
}

void SourceProcessor::lint_attributes(Parser &parser)
{
  parser().foreach_token(SquareOpen, [&](Token par_open) {
    if (par_open.next() != '[') {
      return;
    }
    Scope attributes = par_open.next().scope();
    bool invalid = false;
    attributes.foreach_attribute([&](Token attr, Scope attr_scope) {
      string attr_str = string(attr.str());
      if (attr_str == "base_instance" || attr_str == "clip_distance" ||
          attr_str == "force_inline" || attr_str == "compilation_constant" ||
          attr_str == "compute" || attr_str == "shared" || attr_str == "early_fragment_tests" ||
          attr_str == "flat" || attr_str == "frag_coord" || attr_str == "frag_stencil_ref" ||
          attr_str == "fragment" || attr_str == "front_facing" ||
          attr_str == "global_invocation_id" || attr_str == "in" || attr_str == "instance_id" ||
          attr_str == "instance_index" || attr_str == "layer" ||
          attr_str == "local_invocation_id" || attr_str == "local_invocation_index" ||
          attr_str == "no_perspective" || attr_str == "num_work_groups" || attr_str == "out" ||
          attr_str == "subpass_in" || attr_str == "point_coord" || attr_str == "point_size" ||
          attr_str == "position" || attr_str == "push_constant" || attr_str == "resource_table" ||
          attr_str == "smooth" || attr_str == "vertex_id" || attr_str == "legacy_info" ||
          attr_str == "legacy_iface" || attr_str == "vertex" || attr_str == "viewport_index" ||
          attr_str == "work_group_id" || attr_str == "maybe_unused" || attr_str == "fallthrough" ||
          attr_str == "nodiscard" || attr_str == "node" || attr_str == "clip_control" ||
          attr_str == "texture_atomic")
      {
        if (attr_scope.is_valid()) {
          report_error(attr, "This attribute requires no argument");
          invalid = true;
        }
      }
      else if (attr_str == "acceleration_structure" || attr_str == "attribute" ||
               attr_str == "index" || attr_str == "frag_color" || attr_str == "frag_depth" ||
               attr_str == "uniform" || attr_str == "condition" ||
               attr_str == "raster_order_group" || attr_str == "frequency" ||
               attr_str == "sampler" || attr_str == "specialization_constant")
      {
        if (attr_scope.is_invalid()) {
          report_error(attr, "This attribute requires 1 argument");
          invalid = true;
        }
      }
      else if (attr_str == "storage" || attr_str == "subpass_input") {
        if (attr_scope.is_invalid()) {
          report_error(attr, "This attribute requires 2 arguments");
          invalid = true;
        }
      }
      else if (attr_str == "image") {
        if (attr_scope.is_invalid()) {
          report_error(attr, "This attribute requires 3 arguments");
          invalid = true;
        }
      }
      else if (attr_str == "local_size") {
        if (attr_scope.is_invalid()) {
          report_error(attr, "This attribute requires at least 1 argument");
          invalid = true;
        }
      }
      else if (attr_str == "metal_max_total_threads_per_threadgroup") {
        if (attr_scope.is_invalid()) {
          report_error(attr, "This attribute requires at least 1 argument");
          invalid = true;
        }
      }
      else if (attr_str == "host_shared") {
        if (attributes.front().prev().prev() != Struct && attributes.front().prev().prev() != Enum)
        {
          report_error(
              attr, "host_shared attributes must be placed after a struct or an enum definition");
          invalid = true;
        }
        /* Placement already checked. */
        return;
      }
      else if (attr_str == "unroll" || attr_str == "unroll_n") {
        if (attributes.front().prev().prev().scope().front().prev() != For) {
          report_error(attr, "[[unroll]] attribute must be declared after a 'for' statement");
          invalid = true;
        }
        /* Placement already checked. */
        return;
      }
      else if (attr_str == "static_branch") {
        if (attributes.front().prev().prev().scope().front().prev() != If) {
          report_error(attr,
                       "[[static_branch]] attribute must be declared after a 'if' condition");
          invalid = true;
        }
        /* Placement already checked. */
        return;
      }
      else {
        report_error(attr, "Unrecognized attribute");
        invalid = true;
        /* Attribute already invalid, don't check placement. */
        return;
      }

      if (attr_str == "fallthrough") {
        /* Placement is too complicated to check. C++ compilation should already have checked. */
        return;
      }

      Token prev_tok = attributes.front().prev().prev();
      if (prev_tok == '(' || prev_tok == '{' || prev_tok == ';' || prev_tok == ',' ||
          prev_tok == '}' || prev_tok == ')' || prev_tok == '\n' || prev_tok == ' ' ||
          prev_tok == '>' || prev_tok.is_invalid() ||
          prev_tok.scope().type() == ScopeType::Preprocessor)
      {
        /* Placement is maybe correct. Could refine a bit more. */
      }
      else {
        report_error(attr, "attribute must be declared at a start of a declaration");
        invalid = true;
      }
    });
    if (invalid) {
      /* Erase invalid attributes to avoid spawning more errors. */
      parser.erase(attributes.scope());
    }
  });
  parser.apply_mutations();
}

void SourceProcessor::lint_attributes_ast(Parser &parser)
{
  struct ArgCount {
    int start, size;

    int last() const
    {
      return start + size - 1;
    }
  };

  unordered_map<string, ArgCount> attr_map = {
      /* No argument. */
      {"base_instance", ArgCount(0, 0)},
      {"clip_control", ArgCount(0, 0)},
      {"clip_distance", ArgCount(0, 0)},
      {"compilation_constant", ArgCount(0, 0)},
      {"compute", ArgCount(0, 0)},
      {"early_fragment_tests", ArgCount(0, 0)},
      {"fallthrough", ArgCount(0, 0)},
      {"flat", ArgCount(0, 0)},
      {"frag_coord", ArgCount(0, 0)},
      {"frag_stencil_ref", ArgCount(0, 0)},
      {"fragment", ArgCount(0, 0)},
      {"front_facing", ArgCount(0, 0)},
      {"global_invocation_id", ArgCount(0, 0)},
      {"host_shared", ArgCount(0, 0)},
      {"in", ArgCount(0, 0)},
      {"instance_id", ArgCount(0, 0)},
      {"instance_index", ArgCount(0, 0)},
      {"layer", ArgCount(0, 0)},
      {"legacy_info", ArgCount(0, 0)},
      {"legacy_iface", ArgCount(0, 0)},
      {"local_invocation_id", ArgCount(0, 0)},
      {"local_invocation_index", ArgCount(0, 0)},
      {"maybe_unused", ArgCount(0, 0)},
      {"no_perspective", ArgCount(0, 0)},
      {"node", ArgCount(0, 0)},
      {"nodiscard", ArgCount(0, 0)},
      {"num_work_groups", ArgCount(0, 0)},
      {"out", ArgCount(0, 0)},
      {"point_coord", ArgCount(0, 0)},
      {"point_size", ArgCount(0, 0)},
      {"position", ArgCount(0, 0)},
      {"push_constant", ArgCount(0, 0)},
      {"resource_table", ArgCount(0, 0)},
      {"shared", ArgCount(0, 0)},
      {"smooth", ArgCount(0, 0)},
      {"static_branch", ArgCount(0, 0)},
      {"subpass_in", ArgCount(0, 0)},
      {"texture_atomic", ArgCount(0, 0)},
      {"unroll", ArgCount(0, 0)},
      {"vertex", ArgCount(0, 0)},
      {"vertex_id", ArgCount(0, 0)},
      {"viewport_index", ArgCount(0, 0)},
      {"work_group_id", ArgCount(0, 0)},
      /* 1 argument. */
      {"attribute", ArgCount(1, 1)},
      {"condition", ArgCount(1, 1)},
      {"frag_color", ArgCount(1, 1)},
      {"frag_depth", ArgCount(1, 1)},
      {"frequency", ArgCount(1, 1)},
      {"unroll_n", ArgCount(1, 1)},
      {"index", ArgCount(1, 1)},
      {"metal_max_total_threads_per_threadgroup", ArgCount(1, 1)},
      {"raster_order_group", ArgCount(1, 1)},
      {"sampler", ArgCount(1, 1)},
      {"specialization_constant", ArgCount(1, 1)},
      {"uniform", ArgCount(1, 1)},
      /* 2 arguments. */
      {"storage", ArgCount(2, 1)},
      {"subpass_input", ArgCount(2, 1)},
      /* 3 arguments. */
      {"image", ArgCount(3, 1)},
      /* 1-3 arguments. */
      {"local_size", ArgCount(1, 3)},
  };

  for (Attr attr : parser.root().children_of_type<Attr>()) {
    FuncParamList param = attr.parameters();
    string attr_str(attr.identifier().str());
    if (!attr_map.contains(attr_str)) {
      report_error(attr, "Unrecognized attribute");
      return;
    }

    const auto &param_range = attr_map[string(attr.identifier().str())];
    if (param_range.size == 0 && param.is_valid()) {
      report_error(param, "This attribute requires no argument");
      return;
    }

    int arg_count = param.child_count();
    if (param_range.size == 1 && arg_count != param_range.start) {
      report_error(param,
                   "This attribute requires " + to_string(param_range.start) + " argument(s)");
      return;
    }
    if (param_range.size != 1 && arg_count < param_range.start && arg_count > param_range.last()) {
      report_error(param,
                   "This attribute requires between " + to_string(param_range.start) + " and " +
                       to_string(param_range.last()) + " argument(s)");
      return;
    }

    NodeType parent_type = attr.parent().type();

    if (attr_str == "host_shared" && parent_type != NodeType::ClassDecl) {
      report_error(attr, "host_shared attribute must be inside a struct or an enum definition");
      return;
    }

    NodeType parent_parent_type = attr.parent().parent().type();

    if ((attr_str == "unroll" || attr_str == "unroll_n") &&
        ((parent_type != NodeType::Condition) || (parent_parent_type != NodeType::ForLoop)))
    {
      report_error(attr, "unroll attribute must be declared after a for loop statement");
      return;
    }

    if (attr_str == "static_branch" &&
        (parent_type != NodeType::Condition || parent_parent_type != NodeType::IfStmt ||
         parent_parent_type != NodeType::ElseIfStmt))
    {
      report_error(attr, "static_branch attribute must be declared after a condition");
      return;
    }
  }
}

/* Merge attribute scopes. They are equivalent in the C++ standard.
 * This allow to simplify parsing later on.
 * `[[a]] [[b]]` > `[[a, b]]` */
void SourceProcessor::lower_attribute_sequences(Parser &parser)
{
  do {
    parser().foreach_match("[[..]][[..]]", [&](vector<Token> toks) {
      parser.insert_before(toks[4], ",");
      parser.erase(toks[4], toks[7]);
    });
  } while (parser.apply_mutations());
}

void SourceProcessor::lower_attribute_sequences_ast(Parser &parser)
{
  for (AttrList list : parser.root().descendants_of_type<AttrList>()) {
    if (list.parent().type() == NodeType::AttrList) {
      Token front = list.front().prev(2);
      parser.replace(front, ",");
      parser.erase(front.next(1), front.next(3));
    }
  }
}

}  // namespace blender::gpu::shader

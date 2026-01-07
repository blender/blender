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

void SourceProcessor::lint_attributes(Parser &parser)
{
  parser().foreach_token(SquareOpen, [&](Token par_open) {
    if (par_open.next() != '[') {
      return;
    }
    Scope attributes = par_open.next().scope();
    bool invalid = false;
    attributes.foreach_attribute([&](Token attr, Scope attr_scope) {
      string attr_str = attr.str();
      if (attr_str == "base_instance" || attr_str == "clip_distance" ||
          attr_str == "compilation_constant" || attr_str == "compute" ||
          attr_str == "early_fragment_tests" || attr_str == "flat" || attr_str == "frag_coord" ||
          attr_str == "frag_stencil_ref" || attr_str == "fragment" || attr_str == "front_facing" ||
          attr_str == "global_invocation_id" || attr_str == "in" || attr_str == "instance_id" ||
          attr_str == "layer" || attr_str == "local_invocation_id" ||
          attr_str == "local_invocation_index" || attr_str == "no_perspective" ||
          attr_str == "num_work_groups" || attr_str == "out" || attr_str == "point_coord" ||
          attr_str == "point_size" || attr_str == "position" || attr_str == "push_constant" ||
          attr_str == "resource_table" || attr_str == "smooth" ||
          attr_str == "specialization_constant" || attr_str == "vertex_id" ||
          attr_str == "legacy_info" || attr_str == "vertex" || attr_str == "viewport_index" ||
          attr_str == "work_group_id" || attr_str == "maybe_unused" || attr_str == "fallthrough" ||
          attr_str == "nodiscard" || attr_str == "node")
      {
        if (attr_scope.is_valid()) {
          report_error_(ERROR_TOK(attr), "This attribute requires no argument");
          invalid = true;
        }
      }
      else if (attr_str == "attribute" || attr_str == "index" || attr_str == "frag_color" ||
               attr_str == "frag_depth" || attr_str == "uniform" || attr_str == "condition" ||
               attr_str == "sampler")
      {
        if (attr_scope.is_invalid()) {
          report_error_(ERROR_TOK(attr), "This attribute requires 1 argument");
          invalid = true;
        }
      }
      else if (attr_str == "storage") {
        if (attr_scope.is_invalid()) {
          report_error_(ERROR_TOK(attr), "This attribute requires 2 arguments");
          invalid = true;
        }
      }
      else if (attr_str == "image") {
        if (attr_scope.is_invalid()) {
          report_error_(ERROR_TOK(attr), "This attribute requires 3 arguments");
          invalid = true;
        }
      }
      else if (attr_str == "local_size") {
        if (attr_scope.is_invalid()) {
          report_error_(ERROR_TOK(attr), "This attribute requires at least 1 argument");
          invalid = true;
        }
      }
      else if (attr_str == "host_shared") {
        if (attributes.front().prev().prev() != Struct && attributes.front().prev().prev() != Enum)
        {
          report_error_(
              ERROR_TOK(attr),
              "host_shared attributes must be placed after a struct or an enum definition");
          invalid = true;
        }
        /* Placement already checked. */
        return;
      }
      else if (attr_str == "unroll" || attr_str == "unroll_n") {
        if (attributes.front().prev().prev().scope().front().prev() != For) {
          report_error_(ERROR_TOK(attr),
                        "[[unroll]] attribute must be declared after a 'for' statement");
          invalid = true;
        }
        /* Placement already checked. */
        return;
      }
      else if (attr_str == "static_branch") {
        if (attributes.front().prev().prev().scope().front().prev() != If) {
          report_error_(ERROR_TOK(attr),
                        "[[static_branch]] attribute must be declared after a 'if' condition");
          invalid = true;
        }
        /* Placement already checked. */
        return;
      }
      else {
        report_error_(ERROR_TOK(attr), "Unrecognized attribute");
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
          prev_tok.is_invalid())
      {
        /* Placement is maybe correct. Could refine a bit more. */
      }
      else {
        report_error_(ERROR_TOK(attr), "attribute must be declared at a start of a declaration");
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

}  // namespace blender::gpu::shader

/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "resource.hh"

#include <unordered_map>

namespace bsl {

using namespace blender::gpu::shader::parser::ast;

string ParsedAttribute::parse_condition() const
{
  if (!condition.is_valid()) {
    return "";
  }
  string str;
  for (LocalVar node : condition.children_of_type<LocalVar>()) {
    str += "int " + string(node.str()) + " = ";
    str += "ShaderCreateInfo::find_constant(constants, \"" + string(node.str()) + "\"); ";
  }
  str += "return " + string(condition.str()) + ";";
  return str;
}

Result<ParsedAttribute> resource_type_from_attributes(AttrList list)
{
  struct AttributeDescriptor {
    ResourceTableType srt_type;
    ResourceType res_type;
    /**
     * Number of arguments to expect.
     * Positive values mean exactly 'args' attributes are expected.
     * Negative values means between 1 and 'abs(args)' attributes are expected.
     */
    /* TODO(fclem): Could be replaced by complete set of possible arguments once we support folding
     * preprocessor directives. */
    int args;
  };

  static unordered_map<string, AttributeDescriptor> attr_map = {
      /* clang-format off */
        /* Noop, to remove. */
        {"host_shared",                             {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        /* Vertex inputs. */
        {"attribute",                               {ResourceTableType::VERTEX_IN,      ResourceType::VERT_ATTR_IN,            1}},
        /* Vertex outputs. */
        {"flat",                                    {ResourceTableType::VERTEX_OUT,     ResourceType::VERT_ATTR_OUT,           0}},
        {"no_perspective",                          {ResourceTableType::VERTEX_OUT,     ResourceType::VERT_ATTR_OUT,           0}},
        {"smooth",                                  {ResourceTableType::VERTEX_OUT,     ResourceType::VERT_ATTR_OUT,           0}},
        /* Fragment outputs. */
        {"frag_color",                              {ResourceTableType::FRAGMENT_OUT,   ResourceType::FRAG_OUT,                1}},
        {"frag_stencil_ref",                        {ResourceTableType::FRAGMENT_OUT,   ResourceType::FRAG_OUT,                0}},
        /* Fragment inputs. */
        {"subpass_input",                           {ResourceTableType::FRAGMENT_IN,    ResourceType::FRAG_IN,                 2}},
        /* Entry point type. */
        {"vertex",                                  {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"fragment",                                {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"compute",                                 {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        /* Entry point attrib. */
        {"early_fragment_tests",                    {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"texture_atomic",                          {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"metal_max_total_threads_per_threadgroup", {ResourceTableType::NONE,           ResourceType::NONE,                    1}},
        {"local_size",                              {ResourceTableType::NONE,           ResourceType::NONE,                    -3}},
        /* Resource table. */
        {"compilation_constant",                    {ResourceTableType::RESOURCE_TABLE, ResourceType::COMPILATION_CONST,       0}},
        {"legacy_info",                             {ResourceTableType::RESOURCE_TABLE, ResourceType::LEGACY_INFO,             0}},
        {"legacy_iface",                            {ResourceTableType::RESOURCE_TABLE, ResourceType::LEGACY_IFACE,            0}},
        {"push_constant",                           {ResourceTableType::RESOURCE_TABLE, ResourceType::PUSH_CONST,              0}},
        {"resource_table",                          {ResourceTableType::RESOURCE_TABLE, ResourceType::RESOURCE_TABLE,          0}},
        {"shared",                                  {ResourceTableType::RESOURCE_TABLE, ResourceType::SHARED,                  0}},
        {"sampler",                                 {ResourceTableType::RESOURCE_TABLE, ResourceType::SAMPLER,                 1}},
        {"specialization_constant",                 {ResourceTableType::RESOURCE_TABLE, ResourceType::SPECIALIZATION_CONST,    1}},
        {"uniform",                                 {ResourceTableType::RESOURCE_TABLE, ResourceType::UNIFORM_BUF,             1}},
        {"storage",                                 {ResourceTableType::RESOURCE_TABLE, ResourceType::STORAGE_BUF,             2}},
        {"image",                                   {ResourceTableType::RESOURCE_TABLE, ResourceType::IMAGE,                   3}},
        /* Entry point argument. */
        {"base_instance",                           {ResourceTableType::ENTRY_POINT,    ResourceType::BASE_INSTANCE,           0}},
        {"clip_control",                            {ResourceTableType::ENTRY_POINT,    ResourceType::CLIP_CONTROL,            0}},
        {"clip_distance",                           {ResourceTableType::ENTRY_POINT,    ResourceType::CLIP_DISTANCES,          0}},
        {"front_facing",                            {ResourceTableType::ENTRY_POINT,    ResourceType::FRONT_FACING,            0}},
        {"global_invocation_id",                    {ResourceTableType::ENTRY_POINT,    ResourceType::GLOBAL_INVOCATION_ID,    0}},
        {"in",                                      {ResourceTableType::ENTRY_POINT,    ResourceType::IN,                      0}},
        {"instance_id",                             {ResourceTableType::ENTRY_POINT,    ResourceType::INSTANCE_ID,             0}},
        {"instance_index",                          {ResourceTableType::ENTRY_POINT,    ResourceType::INSTANCE_INDEX,          0}},
        {"layer",                                   {ResourceTableType::ENTRY_POINT,    ResourceType::LAYER,                   0}},
        {"local_invocation_id",                     {ResourceTableType::ENTRY_POINT,    ResourceType::LOCAL_INVOCATION_ID,     0}},
        {"local_invocation_index",                  {ResourceTableType::ENTRY_POINT,    ResourceType::LOCAL_INVOCATION_INDEX,  0}},
        {"out",                                     {ResourceTableType::ENTRY_POINT,    ResourceType::OUT,                     0}},
        {"point_coord",                             {ResourceTableType::ENTRY_POINT,    ResourceType::POINT_COORD,             0}},
        {"point_size",                              {ResourceTableType::ENTRY_POINT,    ResourceType::POINT_SIZE,              0}},
        {"position",                                {ResourceTableType::ENTRY_POINT,    ResourceType::POSITION,                0}},
        {"subpass_in",                              {ResourceTableType::ENTRY_POINT,    ResourceType::SUBPASS_IN,              0}},
        {"vertex_id",                               {ResourceTableType::ENTRY_POINT,    ResourceType::VERTEX_ID,               0}},
        {"viewport_index",                          {ResourceTableType::ENTRY_POINT,    ResourceType::VIEWPORT_INDEX,          0}},
        {"work_group_id",                           {ResourceTableType::ENTRY_POINT,    ResourceType::WORK_GROUP_ID,           0}},
        {"num_work_groups",                         {ResourceTableType::ENTRY_POINT,    ResourceType::NUM_WORK_GROUP,          0}},
        {"frag_depth",                              {ResourceTableType::ENTRY_POINT,    ResourceType::FRAG_DEPTH,              1}},
        {"frag_coord",                              {ResourceTableType::ENTRY_POINT,    ResourceType::FRAG_COORD,              0}},
        {"frag_stencil_ref",                        {ResourceTableType::ENTRY_POINT,    ResourceType::FRAG_STENCIL_REF,        0}},
        /* Misc. */
        {"fallthrough",                             {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"maybe_unused",                            {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"nodiscard",                               {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"node",                                    {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"static_branch",                           {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"unroll",                                  {ResourceTableType::NONE,           ResourceType::NONE,                    0}},
        {"condition",                               {ResourceTableType::NONE,           ResourceType::CONDITION,               1}},
        {"frequency",                               {ResourceTableType::NONE,           ResourceType::FREQUENCY,               1}},
        {"index",                                   {ResourceTableType::NONE,           ResourceType::DUAL_SOURCE_INDEX,       1}},
        {"raster_order_group",                      {ResourceTableType::NONE,           ResourceType::RASTER_ORDER_GROUP,      1}},
      /* clang-format on */
  };

  Result<ParsedAttribute> result;
  for (Attr attr : list.children_of_type<Attr>()) {
    string attr_str(attr.identifier().str());
    if (auto it = attr_map.find(attr_str); it != attr_map.end()) {
      auto [str_type, res_type, expected_param_count] = it->second;

      FuncParamList params = attr.parameters();
      int param_count = params.child_count();
      if (expected_param_count >= 0) {
        if (param_count != expected_param_count) {
          result.err = AstNodeException(params,
                                        Diag::ExpectedNParametersForAttribute,
                                        to_string(expected_param_count),
                                        attr_str);
          continue;
        }
      }
      else {
        /* Negative value denote a range of parameter count. */
        int max_param_count = abs(expected_param_count);
        if (param_count < 1 || param_count > max_param_count) {
          result.err = AstNodeException(params,
                                        Diag::ExpectedRangeParametersForAttribute,
                                        to_string(max_param_count),
                                        attr_str);
          continue;
        }
      }

      if (res_type == ResourceType::CONDITION) {
        result.value.condition = params.child_first();
      }
      else if (res_type == ResourceType::FREQUENCY) {
        result.value.frequency = params.child_first();
      }
      else if (res_type == ResourceType::DUAL_SOURCE_INDEX) {
        result.value.dual_source_index = params.child_first();
      }
      else if (res_type == ResourceType::RASTER_ORDER_GROUP) {
        result.value.raster_order_group = params.child_first();
      }
      else if (res_type != ResourceType::NONE) {
        if (result.value.res_type == ResourceType::NONE) {
          result.value.res_type = res_type;
          result.value.srt_type = str_type;
          result.value.attr = attr;

          Expr param1 = params.child_first();
          if (param1.is_valid()) {
            result.value.param1 = param1;
          }
          Expr param2 = param1.next();
          if (param2.is_valid()) {
            result.value.param2 = param2;
          }
          Expr param3 = param2.next();
          if (param3.is_valid()) {
            result.value.param3 = param3;
          }
        }
        else {
          result.err = AstNodeException(
              attr, Diag::ResourceTableDeclarationAlreadyOfType, to_str(result.value.res_type));
        }
      }
    }
    else {
      result.err = AstNodeException(attr, Diag::UnknownAttribute, attr_str);
    }
  }
  return result;
}

}  // namespace bsl

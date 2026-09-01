/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include "ast.hh"
#include "diagnostic.hh"

#include <string>

namespace bsl {

using namespace blender::gpu::shader::parser;

enum class ResourceType {
  NONE = 0,

  LEGACY_INFO,
  LEGACY_IFACE,
  COMPILATION_CONST,
  SPECIALIZATION_CONST,
  PUSH_CONST,
  SAMPLER,
  IMAGE,
  UNIFORM_BUF,
  STORAGE_BUF,
  ACCELERATION_STRUCTURE,
  RESOURCE_TABLE,
  SHARED,

  VERT_ATTR_IN,
  VERT_ATTR_OUT,
  FRAG_IN,
  FRAG_OUT,

  BASE_INSTANCE,
  CLIP_CONTROL,
  CLIP_DISTANCES,
  FRONT_FACING,
  GLOBAL_INVOCATION_ID,
  IN,
  INSTANCE_ID,
  INSTANCE_INDEX,
  LAYER,
  LOCAL_INVOCATION_ID,
  LOCAL_INVOCATION_INDEX,
  OUT,
  POINT_COORD,
  POINT_SIZE,
  POSITION,
  SUBPASS_IN,
  VERTEX_ID,
  VIEWPORT_INDEX,
  WORK_GROUP_ID,
  NUM_WORK_GROUP,
  FRAG_DEPTH,
  FRAG_COORD,
  FRAG_STENCIL_REF,

  CONDITION,
  FREQUENCY,
  DUAL_SOURCE_INDEX,
  RASTER_ORDER_GROUP,
};

static inline std::string to_str(ResourceType type)
{
#define SERIALIZE(a) \
  case ResourceType::a: \
    return #a

  switch (type) {
    SERIALIZE(NONE);
    SERIALIZE(LEGACY_INFO);
    SERIALIZE(LEGACY_IFACE);
    SERIALIZE(COMPILATION_CONST);
    SERIALIZE(SPECIALIZATION_CONST);
    SERIALIZE(PUSH_CONST);
    SERIALIZE(SAMPLER);
    SERIALIZE(IMAGE);
    SERIALIZE(UNIFORM_BUF);
    SERIALIZE(STORAGE_BUF);
    SERIALIZE(ACCELERATION_STRUCTURE);
    SERIALIZE(VERT_ATTR_IN);
    SERIALIZE(VERT_ATTR_OUT);
    SERIALIZE(FRAG_IN);
    SERIALIZE(FRAG_OUT);
    SERIALIZE(BASE_INSTANCE);
    SERIALIZE(CLIP_CONTROL);
    SERIALIZE(CLIP_DISTANCES);
    SERIALIZE(FRONT_FACING);
    SERIALIZE(GLOBAL_INVOCATION_ID);
    SERIALIZE(IN);
    SERIALIZE(INSTANCE_ID);
    SERIALIZE(INSTANCE_INDEX);
    SERIALIZE(LAYER);
    SERIALIZE(LOCAL_INVOCATION_ID);
    SERIALIZE(LOCAL_INVOCATION_INDEX);
    SERIALIZE(OUT);
    SERIALIZE(POINT_COORD);
    SERIALIZE(POINT_SIZE);
    SERIALIZE(POSITION);
    SERIALIZE(SUBPASS_IN);
    SERIALIZE(VERTEX_ID);
    SERIALIZE(VIEWPORT_INDEX);
    SERIALIZE(WORK_GROUP_ID);
    SERIALIZE(FRAG_DEPTH);
    SERIALIZE(FRAG_COORD);
    SERIALIZE(RESOURCE_TABLE);
    SERIALIZE(SHARED);
    SERIALIZE(NUM_WORK_GROUP);
    SERIALIZE(FRAG_STENCIL_REF);
    SERIALIZE(CONDITION);
    SERIALIZE(FREQUENCY);
    SERIALIZE(DUAL_SOURCE_INDEX);
    SERIALIZE(RASTER_ORDER_GROUP);
  }
  return "Unknown";
#undef SERIALIZE
}

enum class ResourceTableType {
  NONE = 0,

  VERTEX_IN,
  VERTEX_OUT,
  FRAGMENT_OUT,
  FRAGMENT_IN,
  RESOURCE_TABLE,

  ENTRY_POINT,
};

static inline std::string to_str(ResourceTableType type)
{
#define SERIALIZE(a) \
  case ResourceTableType::a: \
    return #a

  switch (type) {
    SERIALIZE(NONE);
    SERIALIZE(VERTEX_IN);
    SERIALIZE(VERTEX_OUT);
    SERIALIZE(FRAGMENT_IN);
    SERIALIZE(FRAGMENT_OUT);
    SERIALIZE(RESOURCE_TABLE);
    SERIALIZE(ENTRY_POINT);
  }
  return "Unknown";
#undef SERIALIZE
}

struct ParsedAttribute {
  ResourceType res_type = ResourceType::NONE;
  ResourceTableType srt_type = ResourceTableType::NONE;
  ast::Expr param1, param2, param3;
  ast::Attr attr;
  ast::Expr condition;
  ast::Expr frequency;
  ast::Expr dual_source_index;
  ast::Expr raster_order_group;

  std::string parse_condition() const;
};

Result<ParsedAttribute> resource_type_from_attributes(ast::AttrList list);

}  // namespace bsl

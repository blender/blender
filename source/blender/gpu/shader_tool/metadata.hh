/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#pragma once

#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

/* Metadata extracted from shader source file.
 * These are then converted to their GPU module equivalent. */
/* TODO(fclem): Make GPU enums standalone and directly use them instead of using separate enums
 * and types. */
namespace blender::gpu::shader::metadata {

/* Compile-time hashing function which converts string to a 64bit hash. */
constexpr static uint64_t hash(const char *name)
{
  uint64_t hash = 2166136261u;
  while (*name) {
    hash = hash * 16777619u;
    hash = hash ^ *name;
    ++name;
  }
  return hash;
}

static inline uint64_t hash(const std::string &name)
{
  return hash(name.c_str());
}

enum Builtin : uint64_t {
  ClipDistance = hash("gl_ClipDistance"),
  FragCoord = hash("gl_FragCoord"),
  FragStencilRef = hash("gl_FragStencilRefARB"),
  FrontFacing = hash("gl_FrontFacing"),
  GlobalInvocationID = hash("gl_GlobalInvocationID"),
  InstanceIndex = hash("gpu_InstanceIndex"),
  BaseInstance = hash("gpu_BaseInstance"),
  InstanceID = hash("gl_InstanceID"),
  LocalInvocationID = hash("gl_LocalInvocationID"),
  LocalInvocationIndex = hash("gl_LocalInvocationIndex"),
  NumWorkGroup = hash("gl_NumWorkGroup"),
  PointCoord = hash("gl_PointCoord"),
  PointSize = hash("gl_PointSize"),
  PrimitiveID = hash("gl_PrimitiveID"),
  VertexID = hash("gl_VertexID"),
  WorkGroupID = hash("gl_WorkGroupID"),
  WorkGroupSize = hash("gl_WorkGroupSize"),
  drw_debug = hash("drw_debug_"),
  printf = hash("printf"),
  assert = hash("assert"),
  runtime_generated = hash("runtime_generated"),
};

enum Qualifier : uint64_t {
  in = hash("in"),
  out = hash("out"),
  inout = hash("inout"),
};

enum Type : uint64_t {
  float1 = hash("float"),
  float2 = hash("float2"),
  float3 = hash("float3"),
  float4 = hash("float4"),
  float3x3 = hash("float3x3"),
  float4x4 = hash("float4x4"),
  sampler1DArray = hash("sampler1DArray"),
  sampler2DArray = hash("sampler2DArray"),
  sampler2D = hash("sampler2D"),
  sampler3D = hash("sampler3D"),
  Closure = hash("Closure"),
};

struct ArgumentFormat {
  Qualifier qualifier;
  Type type;
};

struct FunctionFormat {
  std::string name;
  std::vector<ArgumentFormat> arguments;
};

struct PrintfFormat {
  uint32_t hash;
  std::string format;
};

struct SharedVariable {
  std::string type;
  std::string name;
};

struct ParsedResource {
  /** Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;
  std::string var_array;

  std::string res_type;
  /** For images, storage, uniforms and samplers. */
  std::string res_frequency = "PASS";
  /** For images, storage, uniforms and samplers. */
  std::string res_slot;
  /** For images & storage. */
  std::string res_qualifier;
  /** For specialization & compilation constants. */
  std::string res_value;
  /** For images. */
  std::string res_format;
  /** Optional condition to enable this resource. */
  std::string res_condition;

  std::string serialize() const;
};

struct ResourceTable : std::vector<ParsedResource> {
  std::string name;
};

struct ParsedAttribute {
  /* Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;

  std::string interpolation_mode;

  std::string serialize() const;
};

struct StageInterface : std::vector<ParsedAttribute> {
  std::string name;

  std::string serialize() const;
};

struct ParsedFragOuput {
  /* Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;

  std::string slot;
  std::string dual_source;
  std::string raster_order_group;

  std::string serialize() const;
};

struct FragmentOutputs : std::vector<ParsedFragOuput> {
  std::string name;

  std::string serialize() const;
};

struct ParsedVertInput {
  /* Line this resource was defined. */
  size_t line;

  std::string var_type;
  std::string var_name;

  std::string slot;

  std::string serialize() const;
};

struct VertexInputs : std::vector<ParsedVertInput> {
  std::string name;

  std::string serialize() const;
};

struct Symbol {
  std::string identifier;
  std::string name_space;
  size_t definition_line;
  bool is_method;

  bool operator<(const Symbol &other) const
  {
    if (is_method != other.is_method) {
      /* Methods are supposed to have more precedence.
       * So make them smaller than anything else. */
      return is_method > other.is_method;
    }
    if (name_space != other.name_space) {
      return name_space > other.name_space;
    }
    if (definition_line != other.definition_line) {
      return definition_line < other.definition_line;
    }
    if (identifier != other.identifier) {
      return identifier < other.identifier;
    }
    return false;
  }
};

struct Source {
  std::vector<Builtin> builtins;
  /* Note: Could be a set, but for now the order matters. */
  std::vector<std::string> dependencies;
  std::vector<SharedVariable> shared_variables;
  std::vector<PrintfFormat> printf_formats;
  std::vector<FunctionFormat> functions;
  std::vector<std::string> create_infos;
  std::vector<std::string> create_infos_declarations;
  std::vector<std::string> create_infos_dependencies;
  std::vector<std::string> create_infos_defines;
  std::vector<ResourceTable> resource_tables;
  std::vector<StageInterface> stage_interfaces;
  std::vector<FragmentOutputs> fragment_outputs;
  std::vector<VertexInputs> vertex_inputs;
  std::vector<Symbol> symbol_table;

  /* Serialize Metadata for this source file. */
  std::string serialize(const std::string &function_name) const;
  /* Serialize Create Infos for this source file. */
  std::string serialize_infos() const;
};

}  // namespace blender::gpu::shader::metadata

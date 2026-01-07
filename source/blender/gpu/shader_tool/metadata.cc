/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include "metadata.hh"

namespace blender::gpu::shader::metadata {

std::string ParsedResource::serialize() const
{
  std::string res_condition_lambda;

  if (!res_condition.empty()) {
    res_condition_lambda = ", [](blender::Span<CompilationConstant> constants) { ";
    res_condition_lambda += res_condition;
    res_condition_lambda += "}";
  }

  std::stringstream ss;
  if (res_type == "legacy_info") {
    ss << "ADDITIONAL_INFO(" << var_name << ")";
  }
  else if (res_type == "resource_table") {
    if (!res_condition.empty()) {
      ss << ".additional_info_with_condition(\"" << var_type << "\"" << res_condition_lambda
         << ")";
    }
    else {
      ss << ".additional_info(\"" << var_type << "\")";
    }
  }
  else if (res_type == "sampler") {
    ss << ".sampler(" << res_slot;
    ss << ", ImageType::" << var_type;
    ss << ", \"" << var_name << "\"";
    ss << ", Frequency::" << res_frequency;
    ss << ", GPUSamplerState::internal_sampler()";
    ss << res_condition_lambda << ")";
  }
  else if (res_type == "image") {
    ss << ".image(" << res_slot;
    ss << ", blender::gpu::TextureFormat::" << res_format;
    ss << ", Qualifier::" << res_qualifier;
    ss << ", ImageReadWriteType::" << var_type;
    ss << ", \"" << var_name << "\"";
    ss << ", Frequency::" << res_frequency;
    ss << res_condition_lambda << ")";
  }
  else if (res_type == "uniform") {
    ss << ".uniform_buf(" << res_slot;
    ss << ", \"" << var_type << "\"";
    ss << ", \"" << var_name << var_array << "\"";
    ss << ", Frequency::" << res_frequency;
    ss << res_condition_lambda << ")";
  }
  else if (res_type == "storage") {
    ss << ".storage_buf(" << res_slot;
    ss << ", Qualifier::" << res_qualifier;
    ss << ", \"" << var_type << "\"";
    ss << ", \"" << var_name << var_array << "\"";
    ss << ", Frequency::" << res_frequency;
    ss << res_condition_lambda << ")";
  }
  else if (res_type == "push_constant") {
    if (!var_array.empty()) {
      ss << "PUSH_CONSTANT_ARRAY(" << var_type << ", " << var_name << ", "
         << var_array.substr(1, var_array.size() - 2) << ")";
    }
    else {
      ss << "PUSH_CONSTANT(" << var_type << ", " << var_name << ")";
    }
  }
  else if (res_type == "compilation_constant") {
    /* Needs to be defined on the shader declaration. */
    /* TODO(fclem): Add check that shader sets an existing compilation constant. */
    // ss << "COMPILATION_CONSTANT(" << var_type << ", " << var_name << ", " << res_value << ")";
  }
  else if (res_type == "specialization_constant") {
    ss << "SPECIALIZATION_CONSTANT(" << var_type << ", " << var_name << ", " << res_value << ")";
  }
  return ss.str();
}

std::string ParsedAttribute::serialize() const
{
  std::stringstream ss;
  if (interpolation_mode == "flat") {
    ss << "FLAT(" << var_type << ", " << var_name << ")";
  }
  else if (interpolation_mode == "smooth") {
    ss << "SMOOTH(" << var_type << ", " << var_name << ")";
  }
  else if (interpolation_mode == "no_perspective") {
    ss << "NO_PERSPECTIVE(" << var_type << ", " << var_name << ")";
  }
  return ss.str();
}

std::string StageInterface::serialize() const
{
  std::stringstream ss;
  ss << "GPU_SHADER_INTERFACE_INFO(" << name << "_t)\n";

  for (const auto &res : *this) {
    ss << res.serialize() << "\n";
  }

  ss << "GPU_SHADER_INTERFACE_END()\n";
  return ss.str();
}

std::string ParsedFragOuput::serialize() const
{
  std::stringstream ss;
  if (!dual_source.empty()) {
    ss << "FRAGMENT_OUT_DUAL(" << slot << ", " << var_type << ", " << var_name << ", "
       << dual_source << ")";
  }
  else if (!raster_order_group.empty()) {
    ss << "FRAGMENT_OUT_ROG(" << slot << ", " << var_type << ", " << var_name << ", "
       << raster_order_group << ")";
  }
  else {
    ss << "FRAGMENT_OUT(" << slot << ", " << var_type << ", " << var_name << ")";
  }
  return ss.str();
}

std::string FragmentOutputs::serialize() const
{
  std::stringstream ss;
  ss << "GPU_SHADER_CREATE_INFO(" << name << ")\n";

  for (const auto &res : *this) {
    ss << res.serialize() << "\n";
  }

  ss << "GPU_SHADER_CREATE_END()\n";
  return ss.str();
}

std::string ParsedVertInput::serialize() const
{
  std::stringstream ss;
  ss << "VERTEX_IN(" << slot << ", " << var_type << ", " << var_name << ")";
  return ss.str();
}

std::string VertexInputs::serialize() const
{
  std::stringstream ss;
  ss << "GPU_SHADER_CREATE_INFO(" << name << ")\n";

  for (const auto &res : *this) {
    ss << res.serialize() << "\n";
  }

  ss << "GPU_SHADER_CREATE_END()\n";
  return ss.str();
}

std::string Source::serialize(const std::string &function_name) const
{
  std::stringstream ss;
  ss << "static void " << function_name
     << "(GPUSource &source, GPUFunctionDictionary *g_functions, GPUPrintFormatMap *g_formats) "
        "{\n";
  for (auto function : functions) {
    ss << "  {\n";
    ss << "    Vector<metadata::ArgumentFormat> args = {\n";
    for (auto arg : function.arguments) {
      ss << "      "
         << "metadata::ArgumentFormat{"
         << "metadata::Qualifier(" << std::to_string(uint64_t(arg.qualifier)) << "LLU), "
         << "metadata::Type(" << std::to_string(uint64_t(arg.type)) << "LLU)"
         << "},\n";
    }
    ss << "    };\n";
    ss << "    source.add_function(\"" << function.name << "\", args, g_functions);\n";
    ss << "  }\n";
  }
  for (auto builtin : builtins) {
    ss << "  source.add_builtin(metadata::Builtin(" << std::to_string(builtin) << "LLU));\n";
  }
  for (auto dependency : dependencies) {
    ss << "  source.add_dependency(\"" << dependency << "\");\n";
  }
  for (auto var : shared_variables) {
    ss << "  source.add_shared_variable(Type::" << var.type << "_t, \"" << var.name << "\");\n";
  }
  for (auto format : printf_formats) {
    ss << "  source.add_printf_format(uint32_t(" << std::to_string(format.hash) << "), "
       << format.format << ", g_formats);\n";
  }
  /* Avoid warnings. */
  ss << "  UNUSED_VARS(source, g_functions, g_formats);\n";
  ss << "}\n";
  return ss.str();
}

std::string Source::serialize_infos() const
{
  std::stringstream ss;
  ss << "#pragma once\n";
  ss << "\n";
  for (auto dependency : create_infos_dependencies) {
    ss << "#include \"" << dependency << "\"\n";
  }
  ss << "\n";
  for (auto define : create_infos_defines) {
    ss << define;
  }
  ss << "\n";
  for (auto vert_inputs : vertex_inputs) {
    ss << vert_inputs.serialize() << "\n";
  }
  ss << "\n";
  for (auto frag_outputs : fragment_outputs) {
    ss << frag_outputs.serialize() << "\n";
  }
  ss << "\n";
  for (auto iface : stage_interfaces) {
    ss << iface.serialize() << "\n";
  }
  ss << "\n";
  for (auto res_table : resource_tables) {
    ss << "GPU_SHADER_CREATE_INFO(" << res_table.name << ")\n";
    for (const auto &res : res_table) {
      ss << res.serialize() << "\n";
    }
    ss << "GPU_SHADER_CREATE_END()\n";
  }
  ss << "\n";
  for (auto declaration : create_infos_declarations) {
    ss << declaration << "\n";
  }
  return ss.str();
}
}  // namespace blender::gpu::shader::metadata

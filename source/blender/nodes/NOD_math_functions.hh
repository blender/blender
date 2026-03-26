/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "BLI_string_ref.hh"
#include "BLI_ustring.hh"

#include "NOD_multi_function.hh"

namespace blender::nodes {

void node_math_build_multi_function(NodeMultiFunctionBuilder &builder);

struct FloatMathOperationInfo {
  StringRefNull title_case_name;
  StringRefNull shader_name;
  UString multi_function_name;

  FloatMathOperationInfo() = delete;
  FloatMathOperationInfo(StringRefNull title_case_name,
                         StringRefNull shader_name,
                         UString multi_function_name = {})
      : title_case_name(title_case_name),
        shader_name(shader_name),
        multi_function_name(multi_function_name)
  {
  }
};

const FloatMathOperationInfo *get_float_math_operation_info(int operation);
const FloatMathOperationInfo *get_float3_math_operation_info(int operation);
const FloatMathOperationInfo *get_float_compare_operation_info(int operation);

}  // namespace blender::nodes

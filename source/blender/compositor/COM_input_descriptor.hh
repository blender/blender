/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "COM_result.hh"

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Input Realization Mode
 *
 * Specifies how the input should be realized before execution. See the discussion in COM_domain.hh
 * for more information on what realization mean. */
enum class InputRealizationMode : uint8_t {
  /* The input should not be realized in any way. */
  None,
  /* The rotation and scale transforms of the input should be realized. */
  Transforms,
  /* The input should be realized on the operation domain, noting that the operation domain have
   * its transforms realized. */
  OperationDomain,
};

/* ------------------------------------------------------------------------------------------------
 * Implicit Input
 *
 * Specifies the implicit input that should be assigned to the input if it is unlinked. See the
 * ImplicitInputOperation operation for more information on the individual types. */
enum class ImplicitInput : uint8_t {
  /* The input does not have an implicit input and its value should be used. */
  None,
  /* The input should have the texture coordinates of the compositing space as an input. */
  TextureCoordinates,
};

/* ------------------------------------------------------------------------------------------------
 * Input Descriptor
 *
 * A class that describes an input of an operation. */
class InputDescriptor {
 public:
  /* The type of input. This may be different that the type of result that the operation will
   * receive for the input, in which case, an implicit conversion operation will be added as an
   * input processor to convert it to the required type. */
  ResultType type;
  /* Specify how the input should be realized. */
  InputRealizationMode realization_mode = InputRealizationMode::OperationDomain;
  /* Specifies the type of implicit input in case the input in unlinked. */
  ImplicitInput implicit_input = ImplicitInput::None;
  /* The priority of the input for determining the operation domain. The non-single value input
   * with the highest priority will be used to infer the operation domain, the highest priority
   * being zero. See the discussion in COM_domain.hh for more information. */
  int domain_priority = 0;
  /* If true, the input expects a single value, and if a non-single value is provided, a default
   * single value will be used instead, see the get_<type>_value_default methods in the Result
   * class. It follows that this also implies no realization, because we don't need to realize a
   * result that will be discarded anyways. If false, the input can work with both single and
   * non-single values. */
  bool expects_single_value = false;
  /* If true, the input will not be implicitly converted to the type of the input and will be
   * passed as is. */
  bool skip_type_conversion = false;
};

}  // namespace blender::compositor

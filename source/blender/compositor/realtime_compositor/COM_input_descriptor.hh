/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_result.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Input Realization Options
 *
 * A bit-field that specifies how the input should be realized before execution. See the discussion
 * in COM_domain.hh for more information on what realization mean. */
struct InputRealizationOptions {
  /* The input should be realized on the operation domain of the operation. */
  bool realize_on_operation_domain : 1;
  /* The input should be realized on a domain that is identical to the domain of the input but with
   * an identity rotation and an increased size that completely fits the image after rotation. This
   * is useful for operations that are not rotation invariant. */
  bool realize_rotation : 1;
  /* The input should be realized on a domain that is identical to the domain of the input but with
   * an identity scale and an increased/decreased size that completely fits the image after
   * scaling. This is useful for operations that are not scale invariant. */
  bool realize_scale : 1;
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
  /* The options that specify how the input should be realized. */
  InputRealizationOptions realization_options = {true};
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
};

}  // namespace blender::realtime_compositor

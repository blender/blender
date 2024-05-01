/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"

#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "MEM_guardedalloc.h"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_reduce_to_single_value_operation.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

ReduceToSingleValueOperation::ReduceToSingleValueOperation(Context &context, ResultType type)
    : SimpleOperation(context)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = type;
  declare_input_descriptor(input_descriptor);
  populate_result(context.create_result(type));
}

void ReduceToSingleValueOperation::execute()
{
  /* Make sure any prior writes to the texture are reflected before downloading it. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  const Result &input = get_input();
  float *pixel = static_cast<float *>(GPU_texture_read(input.texture(), GPU_DATA_FLOAT, 0));

  Result &result = get_result();
  result.allocate_single_value();
  switch (result.type()) {
    case ResultType::Color:
      result.set_color_value(pixel);
      break;
    case ResultType::Vector:
      result.set_vector_value(pixel);
      break;
    case ResultType::Float:
      result.set_float_value(*pixel);
      break;
    default:
      /* Other types are internal and needn't be handled by operations. */
      BLI_assert_unreachable();
      break;
  }

  MEM_freeN(pixel);
}

SimpleOperation *ReduceToSingleValueOperation::construct_if_needed(Context &context,
                                                                   const Result &input_result)
{
  /* Input result is already a single value, the operation is not needed. */
  if (input_result.is_single_value()) {
    return nullptr;
  }

  /* The input is a full sized texture and can't be reduced to a single value, the operation is not
   * needed. */
  if (input_result.domain().size != int2(1)) {
    return nullptr;
  }

  /* The input is a texture of a single pixel and can be reduced to a single value. */
  return new ReduceToSingleValueOperation(context, input_result.type());
}

}  // namespace blender::realtime_compositor

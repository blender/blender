/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief temporarily storage during execution of Tone-map
 * \ingroup operation
 */
typedef struct AvgLogLum {
  float al;
  float auto_key;
  float lav;
  float cav[4];
  float igm;
} AvgLogLum;

/**
 * \brief base class of tone-map, implementing the simple tone-map
 * \ingroup operation
 */
class TonemapOperation : public MultiThreadedOperation {
 protected:
  /**
   * \brief settings of the Tone-map
   */
  const NodeTonemap *data_;

  /**
   * \brief temporarily cache of the execution storage
   */
  AvgLogLum *cached_instance_;

 public:
  TonemapOperation();

  void set_data(const NodeTonemap *data)
  {
    data_ = data;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

/**
 * \brief class of tone-map, implementing the photo-receptor tone-map
 * most parts have already been done in #TonemapOperation.
 * \ingroup operation
 */
class PhotoreceptorTonemapOperation : public TonemapOperation {
 public:
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor

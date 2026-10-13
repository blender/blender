/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include <memory>

#include "GPU_batch.hh"
#include "MEM_guardedalloc.h"

namespace blender::draw {

/**
 * Wrapper around a #gpu::Batch, owning or borrowing it including drawing ranges.
 */
struct Batch {
 private:
  /** GPU batch wrapped by this draw-batch. */
  gpu::Batch *gpu_batch_ = nullptr;
  /** Whether the batch is owned by this draw-batch and discarded on destruction. */
  bool owns_batch_ = false;
  /**
   * When not #GPU_PRIM_NONE, this draw-batch is procedural: no #gpu::Batch is stored and the batch
   * is resolved from the active GPU context on every access (see #batch_get).
   */
  GPUPrimType batch_prim_ = GPU_PRIM_NONE;

#if 0
  /* Commented out as meshlet integration will be added later.*/
  /**
   * Refers to meshlets of the batch.
   */
  Span<Meshlet> meshlets_;
  /**
   * Contains the extracted meshlet for non-owning batches. Would contain the data to draw the
   * whole batch and \a meshlets_ would point to this entry.
   */
  Meshlet meshlet_;
#endif

 public:
  MEM_CXX_CLASS_ALLOC_FUNCS("Batch")

  /** Takes ownership of \a batch. The batch is discarded when destructed. */
  Batch(gpu::Batch *batch);
  /**
   * Wraps \a batch, taking ownership only when \a owns_batch is true.
   */
  Batch(gpu::Batch *batch, bool owns_batch);
  /**
   * Creates a procedural draw-batch.
   *
   * The underlying batch is resolved from the active GPU context on access (see #batch_get), so
   * the same draw-batch can be safely shared across threads and contexts.
   *
   * \a batch_prim must not be #GPU_PRIM_NONE, which is reserved as the sentinel for regular
   * (non-procedural) draw-batches.
   */
  explicit Batch(GPUPrimType batch_prim);

  ~Batch()
  {
    if (owns_batch_) {
      GPU_BATCH_DISCARD_SAFE(gpu_batch_);
    }
    gpu_batch_ = nullptr;
  }

  Batch(const Batch &) = delete;
  Batch &operator=(const Batch &) = delete;
  Batch(Batch &&) = delete;
  Batch &operator=(Batch &&) = delete;

  /**
   * Resolve the underlying #gpu::Batch.
   *
   * For procedural batches this fetches the #gpu::Batch from the active GPU context, so the result
   * follows context and thread switches. For regular draw-batches it returns the wrapped batch.
   */
  gpu::Batch *gpu_batch() const;

  static inline void clear(Batch *batch)
  {
    if (batch) {
      BLI_assert(batch->owns_batch_);
      GPU_BATCH_CLEAR_SAFE(batch->gpu_batch_);
    }
  }
};

using BatchPtr = std::unique_ptr<Batch>;

/**
 * Create a #BatchPtr that takes ownership of \a batch.
 */
inline BatchPtr make_batch(gpu::Batch *batch)
{
  return std::make_unique<Batch>(batch);
}

/**
 * Create a #BatchPtr that borrows \a batch.
 *
 * Use this for batches that are not owned by the draw system but that need a #Batch to outlive
 * the command recording, e.g. motion path batches.
 */
inline BatchPtr make_batch_borrowed(gpu::Batch *batch)
{
  return std::make_unique<Batch>(batch, /*owns_batch=*/false);
}

}  // namespace blender::draw

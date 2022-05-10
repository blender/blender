/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "mtl_context.hh"
#include "mtl_debug.hh"

using namespace blender;
using namespace blender::gpu;

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Memory Management
 * \{ */

bool MTLTemporaryBufferRange::requires_flush()
{
  /* We do not need to flush shared memory */
  return this->options & MTLResourceStorageModeManaged;
}

void MTLTemporaryBufferRange::flush()
{
  if (this->requires_flush()) {
    BLI_assert(this->metal_buffer);
    BLI_assert((this->buffer_offset + this->size) <= [this->metal_buffer length]);
    BLI_assert(this->buffer_offset >= 0);
    [this->metal_buffer
        didModifyRange:NSMakeRange(this->buffer_offset, this->size - this->buffer_offset)];
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MTLContext
 * \{ */

/* Placeholder functions */
MTLContext::MTLContext(void *ghost_window)
{
  /* Init debug. */
  debug::mtl_debug_init();

  /* TODO(Metal): Implement. */
}

MTLContext::~MTLContext()
{
  /* TODO(Metal): Implement. */
}

void MTLContext::check_error(const char *info)
{
  /* TODO(Metal): Implement. */
}

void MTLContext::activate(void)
{
  /* TODO(Metal): Implement. */
}
void MTLContext::deactivate(void)
{
  /* TODO(Metal): Implement. */
}

void MTLContext::flush(void)
{
  /* TODO(Metal): Implement. */
}
void MTLContext::finish(void)
{
  /* TODO(Metal): Implement. */
}

void MTLContext::memory_statistics_get(int *total_mem, int *free_mem)
{
  /* TODO(Metal): Implement. */
  *total_mem = 0;
  *free_mem = 0;
}

id<MTLCommandBuffer> MTLContext::get_active_command_buffer()
{
  /* TODO(Metal): Implement. */
  return nil;
}

/* Render Pass State and Management */
void MTLContext::begin_render_pass()
{
  /* TODO(Metal): Implement. */
}
void MTLContext::end_render_pass()
{
  /* TODO(Metal): Implement. */
}

/** \} */

}  // blender::gpu

/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_userdef_types.h"

#include "GPU_debug.hh"

#include "intern/GHOST_ContextMTL.hh"

#include "mtl_backend.hh"
#include "mtl_command_buffer.hh"
#include "mtl_common.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_framebuffer.hh"

#include <fstream>

using namespace blender;
using namespace blender::gpu;

namespace blender::gpu {

/* Counter for active command buffers. */
volatile std::atomic<int> MTLCommandBufferManager::num_active_cmd_bufs_in_system = 0;

/* -------------------------------------------------------------------- */
/** \name MTLCommandBuffer initialization and render coordination.
 * \{ */

void MTLCommandBufferManager::prepare(bool /*supports_render*/)
{
  render_pass_state_.reset_state();
  compute_state_.reset_state();
}

void MTLCommandBufferManager::register_encoder_counters()
{
  encoder_count_++;
  empty_ = false;
}

id<MTLCommandBuffer> MTLCommandBufferManager::ensure_begin()
{
  if (active_command_buffer_ == nil) {

    /* Verify number of active command buffers is below limit.
     * Exceeding this limit will mean we either have a command buffer leak/GPU hang
     * or we should increase the command buffer limit during MTLQueue creation.
     * Excessive command buffers can also be caused by frequent GPUContext switches, which cause
     * the GPU pipeline to flush. This is common during indirect light baking operations.
     *
     * NOTE: We currently stall until completion of GPU work upon ::submit if we have reached the
     * in-flight command buffer limit. */
    BLI_assert(MTLCommandBufferManager::num_active_cmd_bufs_in_system <
               GHOST_ContextMTL::max_command_buffer_count);

    if (G.debug & G_DEBUG_GPU) {
      /* Debug: Enable Advanced Errors for GPU work execution. */
      MTLCommandBufferDescriptor *desc = [[MTLCommandBufferDescriptor alloc] init];
      desc.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
      desc.retainedReferences = YES;
      BLI_assert(context_.queue != nil);
      active_command_buffer_ = [context_.queue commandBufferWithDescriptor:desc];

      std::string group_name = "CmdBuf: " + GPU_debug_get_groups_names({0, 1});
      [active_command_buffer_ setLabel:@(group_name.c_str())];
      [desc release];
    }
    else {
      active_command_buffer_ = [context_.queue commandBuffer];
    }

    [active_command_buffer_ retain];
    context_.main_command_buffer.inc_active_command_buffer_count();

    /* Ensure we begin new Scratch Buffer if we are on a new frame. */
    MTLScratchBufferManager &mem = context_.memory_manager;
    mem.ensure_increment_scratch_buffer();

    /* Reset Command buffer heuristics. */
    this->reset_counters();
  }
  BLI_assert(active_command_buffer_ != nil);
  return active_command_buffer_;
}

/* If wait is true, CPU will stall until GPU work has completed. */
bool MTLCommandBufferManager::submit(bool wait)
{
  /* If we have to wait, we must ensure to either finish the active command encoder or to wait
   * until all previous one have completed. */
  if (empty_ && !wait) {
    /* Skip submission if command buffer is empty. */
    return false;
  }

  if (active_command_buffer_ == nil) {
    if (wait) {
      /* Wait for any previously submitted work on this context to complete.
       * (The wait function will yield so may need reworking if this hits a
       * performance critical path which is sensitive to CPU<->GPU latency) */
      wait_until_active_command_buffers_complete();
    }
    return false;
  }

  /* Ensure current encoders are finished. */
  this->end_active_command_encoder();
  BLI_assert(active_command_encoder_type_ == MTL_NO_COMMAND_ENCODER);

  /* Flush active ScratchBuffer associated with parent MTLContext. */
  context_.memory_manager.flush_active_scratch_buffer();

  /*** Submit Command Buffer. ***/
  /* Command buffer lifetime tracking. */
  /* Increment current MTLSafeFreeList reference counter to flag MTLBuffers freed within
   * the current command buffer lifetime as used.
   * This ensures that in-use resources are not prematurely de-referenced and returned to the
   * available buffer pool while they are in-use by the GPU. */
  MTLSafeFreeList *cmd_free_buffer_list =
      MTLContext::get_global_memory_manager()->get_current_safe_list();
  BLI_assert(cmd_free_buffer_list);
  cmd_free_buffer_list->increment_reference();

  id<MTLCommandBuffer> cmd_buffer_ref = active_command_buffer_;
  [cmd_buffer_ref retain];

  [cmd_buffer_ref addCompletedHandler:^(id<MTLCommandBuffer> /*cb*/) {
    /* Upon command buffer completion, decrement MTLSafeFreeList reference count
     * to allow buffers no longer in use by this CommandBuffer to be freed. */
    cmd_free_buffer_list->decrement_reference();

    /* Release command buffer after completion callback handled. */
    [cmd_buffer_ref release];

    /* Decrement count. */
    context_.main_command_buffer.dec_active_command_buffer_count();
  }];

  /* Submit command buffer to GPU. */
  [active_command_buffer_ commit];

  /* If we have too many active command buffers in flight, wait until completed to avoid running
   * out. We can increase */
  if (MTLCommandBufferManager::num_active_cmd_bufs_in_system >=
      (GHOST_ContextMTL::max_command_buffer_count - 1))
  {
    wait = true;
    MTL_LOG_WARNING(
        "Maximum number of command buffers in flight. Host will wait until GPU work has "
        "completed. Consider increasing GHOST_ContextMTL::max_command_buffer_count or reducing "
        "work fragmentation to better utilize system hardware. Command buffers are flushed upon "
        "GPUContext switches, this is the most common cause of excessive command buffer "
        "generation.");
  }

  if (wait || (G.debug & G_DEBUG_GPU)) {
    /* Wait until current GPU work has finished executing. */
    [active_command_buffer_ waitUntilCompleted];

    /* Command buffer execution debugging can return an error message if
     * execution has failed or encountered GPU-side errors. */
    if (G.debug & G_DEBUG_GPU) {

      NSError *error = [active_command_buffer_ error];
      if (error != nil) {
        NSLog(@"%@", error);
        BLI_assert(false);
      }
    }
  }

  /* Release previous frames command buffer and reset active cmd buffer. */
  if (last_submitted_command_buffer_ != nil) {

    BLI_assert(MTLBackend::get()->is_inside_render_boundary());
    [last_submitted_command_buffer_ autorelease];
    last_submitted_command_buffer_ = nil;
  }
  last_submitted_command_buffer_ = active_command_buffer_;
  active_command_buffer_ = nil;

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Command Encoder Utility and management functions.
 * \{ */

/* Fetch/query current encoder. */
bool MTLCommandBufferManager::is_inside_render_pass()
{
  return (active_command_encoder_type_ == MTL_RENDER_COMMAND_ENCODER);
}

bool MTLCommandBufferManager::is_inside_blit()
{
  return (active_command_encoder_type_ == MTL_BLIT_COMMAND_ENCODER);
}

bool MTLCommandBufferManager::is_inside_compute()
{
  return (active_command_encoder_type_ == MTL_COMPUTE_COMMAND_ENCODER);
}

id<MTLRenderCommandEncoder> MTLCommandBufferManager::get_active_render_command_encoder()
{
  /* Calling code should check if inside render pass. Otherwise nil. */
  return active_render_command_encoder_;
}

id<MTLBlitCommandEncoder> MTLCommandBufferManager::get_active_blit_command_encoder()
{
  /* Calling code should check if inside render pass. Otherwise nil. */
  return active_blit_command_encoder_;
}

id<MTLComputeCommandEncoder> MTLCommandBufferManager::get_active_compute_command_encoder()
{
  /* Calling code should check if inside render pass. Otherwise nil. */
  return active_compute_command_encoder_;
}

MTLFrameBuffer *MTLCommandBufferManager::get_active_framebuffer()
{
  /* If outside of RenderPass, nullptr will be returned. */
  if (this->is_inside_render_pass()) {
    return active_frame_buffer_;
  }
  return nullptr;
}

/* Encoder and Pass management. */
/* End currently active MTLCommandEncoder. */
bool MTLCommandBufferManager::end_active_command_encoder(bool retain_framebuffers)
{
  /* End active encoder if one is active. */
  if (active_command_encoder_type_ == MTL_NO_COMMAND_ENCODER) {
    /* MTL_NO_COMMAND_ENCODER. */
    BLI_assert(active_render_command_encoder_ == nil);
    BLI_assert(active_blit_command_encoder_ == nil);
    BLI_assert(active_compute_command_encoder_ == nil);
    return false;
  }

  if (G.debug & G_DEBUG_GPU) {
    fold_remaining_debug_groups();
  }

  switch (active_command_encoder_type_) {
    case MTL_RENDER_COMMAND_ENCODER:
      BLI_assert(active_render_command_encoder_ != nil);
      [active_render_command_encoder_ endEncoding];
      [active_render_command_encoder_ release];
      active_render_command_encoder_ = nil;
      /* Reset associated frame-buffer flag. */
      if (!retain_framebuffers) {
        active_frame_buffer_ = nullptr;
        active_pass_descriptor_ = nullptr;
      }
      break;
    case MTL_BLIT_COMMAND_ENCODER:
      BLI_assert(active_blit_command_encoder_ != nil);
      [active_blit_command_encoder_ endEncoding];
      [active_blit_command_encoder_ release];
      active_blit_command_encoder_ = nil;
      break;
    case MTL_COMPUTE_COMMAND_ENCODER:
      BLI_assert(active_compute_command_encoder_ != nil);
      [active_compute_command_encoder_ endEncoding];
      [active_compute_command_encoder_ release];
      active_compute_command_encoder_ = nil;
      break;
    default: {
      BLI_assert(false && "Invalid command encoder type");
      return false;
    }
  }
  active_command_encoder_type_ = MTL_NO_COMMAND_ENCODER;
  return true;
}

id<MTLRenderCommandEncoder> MTLCommandBufferManager::ensure_begin_render_command_encoder(
    MTLFrameBuffer *ctx_framebuffer, bool force_begin, bool *r_new_pass)
{
  /* Ensure valid frame-buffer. */
  BLI_assert(ctx_framebuffer != nullptr);

  /* Ensure active command buffer. */
  id<MTLCommandBuffer> cmd_buf = this->ensure_begin();
  BLI_assert(cmd_buf);

  /* Begin new command encoder if the currently active one is
   * incompatible or requires updating. */
  if (active_command_encoder_type_ != MTL_RENDER_COMMAND_ENCODER ||
      active_frame_buffer_ != ctx_framebuffer || force_begin)
  {
    this->end_active_command_encoder();

    /* Determine if this is a re-bind of the same frame-buffer. */
    bool is_rebind = (active_frame_buffer_ == ctx_framebuffer);

    /* Generate RenderPassDescriptor from bound frame-buffer. */
    BLI_assert(ctx_framebuffer);
    active_frame_buffer_ = ctx_framebuffer;
    active_pass_descriptor_ = active_frame_buffer_->bake_render_pass_descriptor(
        is_rebind && (!active_frame_buffer_->get_pending_clear()));

    /* Determine if there is a visibility buffer assigned to the context. */
    gpu::MTLBuffer *visibility_buffer = context_.get_visibility_buffer();
    this->active_pass_descriptor_.visibilityResultBuffer =
        (visibility_buffer) ? visibility_buffer->get_metal_buffer() : nil;
    context_.clear_visibility_dirty();

    /* Ensure we have already cleaned up our previous render command encoder. */
    BLI_assert(active_render_command_encoder_ == nil);

    /* Create new RenderCommandEncoder based on descriptor (and begin encoding). */
    active_render_command_encoder_ = [cmd_buf
        renderCommandEncoderWithDescriptor:active_pass_descriptor_];
    [active_render_command_encoder_ retain];
    active_command_encoder_type_ = MTL_RENDER_COMMAND_ENCODER;

    /* Add debug label. */
    if (G.debug & G_DEBUG_GPU) {
      std::string debug_name = "FrameBuf: " + std::string(active_frame_buffer_->name_get());
      [active_render_command_encoder_ setLabel:@(debug_name.c_str())];
    }

    /* Unroll pending debug groups. */
    if (G.debug & G_DEBUG_GPU) {
      unfold_pending_debug_groups();
    }

    /* Update command buffer encoder heuristics. */
    this->register_encoder_counters();

    /* Apply initial state. */
    /* Update Viewport and Scissor State */
    active_frame_buffer_->apply_state();

    /* FLAG FRAMEBUFFER AS CLEARED -- A clear only lasts as long as one has been specified.
     * After this, resets to Load attachments to parallel GL behavior. */
    active_frame_buffer_->mark_cleared();

    /* Reset RenderPassState to ensure resource bindings are re-applied. */
    render_pass_state_.reset_state();

    /* Return true as new pass started. */
    *r_new_pass = true;
  }
  else {
    /* No new pass. */
    *r_new_pass = false;
  }

  BLI_assert(active_render_command_encoder_ != nil);
  return active_render_command_encoder_;
}

id<MTLBlitCommandEncoder> MTLCommandBufferManager::ensure_begin_blit_encoder()
{
  /* Ensure active command buffer. */
  id<MTLCommandBuffer> cmd_buf = this->ensure_begin();
  BLI_assert(cmd_buf);

  /* Ensure no existing command encoder of a different type is active. */
  if (active_command_encoder_type_ != MTL_BLIT_COMMAND_ENCODER) {
    this->end_active_command_encoder();
  }

  /* Begin new Blit Encoder. */
  if (active_blit_command_encoder_ == nil) {

    active_blit_command_encoder_ = [cmd_buf blitCommandEncoder];
    BLI_assert(active_blit_command_encoder_ != nil);
    [active_blit_command_encoder_ retain];
    active_command_encoder_type_ = MTL_BLIT_COMMAND_ENCODER;

    /* Add debug label. */
    if (G.debug & G_DEBUG_GPU) {
      std::string debug_name = GPU_debug_get_groups_names({1, 1});
      [active_blit_command_encoder_ setLabel:@(debug_name.c_str())];
    }
    /* Unroll pending debug groups. */
    if (G.debug & G_DEBUG_GPU) {
      unfold_pending_debug_groups();
    }

    /* Update command buffer encoder heuristics. */
    this->register_encoder_counters();
  }
  BLI_assert(active_blit_command_encoder_ != nil);
  return active_blit_command_encoder_;
}

id<MTLComputeCommandEncoder> MTLCommandBufferManager::ensure_begin_compute_encoder()
{
  /* Ensure active command buffer. */
  id<MTLCommandBuffer> cmd_buf = this->ensure_begin();
  BLI_assert(cmd_buf);

  /* Ensure no existing command encoder of a different type is active. */
  if (active_command_encoder_type_ != MTL_COMPUTE_COMMAND_ENCODER) {
    this->end_active_command_encoder();
  }

  /* Begin new Compute Encoder. */
  if (active_compute_command_encoder_ == nil) {
    active_compute_command_encoder_ = [cmd_buf computeCommandEncoder];
    BLI_assert(active_compute_command_encoder_ != nil);
    [active_compute_command_encoder_ retain];
    active_command_encoder_type_ = MTL_COMPUTE_COMMAND_ENCODER;

    /* Add debug label. */
    if (G.debug & G_DEBUG_GPU) {
      std::string debug_name = GPU_debug_get_groups_names({1, 1});
      [active_compute_command_encoder_ setLabel:@(debug_name.c_str())];
    }

    /* Unroll pending debug groups. */
    if (G.debug & G_DEBUG_GPU) {
      unfold_pending_debug_groups();
    }

    /* Update command buffer encoder heuristics. */
    this->register_encoder_counters();

    /* Reset RenderPassState to ensure resource bindings are re-applied. */
    compute_state_.reset_state();
  }
  BLI_assert(active_compute_command_encoder_ != nil);
  return active_compute_command_encoder_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Command buffer heuristics.
 * \{ */

/* Rendering Heuristics. */
void MTLCommandBufferManager::register_draw_counters(int vertex_submission)
{
  current_draw_call_count_++;
  vertex_submitted_count_ += vertex_submission;
  empty_ = false;
}

/* Reset workload counters. */
void MTLCommandBufferManager::reset_counters()
{
  empty_ = true;
  current_draw_call_count_ = 0;
  encoder_count_ = 0;
  vertex_submitted_count_ = 0;
}

/* Workload evaluation. */
bool MTLCommandBufferManager::do_break_submission()
{
  /* Skip if no active command buffer. */
  if (active_command_buffer_ == nil) {
    return false;
  }

  /* Use optimized heuristic to split heavy command buffer submissions to better saturate the
   * hardware and also reduce stalling from individual large submissions. */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY) ||
      GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY))
  {
    return ((current_draw_call_count_ > 30000) || (vertex_submitted_count_ > 100000000) ||
            (encoder_count_ > 25));
  }
  /* Apple Silicon is less efficient if splitting submissions. */
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Command buffer debugging.
 * \{ */

/* Debug. */
void MTLCommandBufferManager::push_debug_group(const char *name, int /*index*/)
{
  /* Only perform this operation if capturing. */
  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  if (![capture_manager isCapturing]) {
    return;
  }

  if (active_command_buffer_ == nil) {
    return;
  }

  id<MTLCommandBuffer> cmd = this->ensure_begin();
  if (cmd != nil) {
    switch (active_command_encoder_type_) {
      case MTL_RENDER_COMMAND_ENCODER:
        [active_render_command_encoder_ pushDebugGroup:@(name)];
        break;
      case MTL_BLIT_COMMAND_ENCODER:
        [active_blit_command_encoder_ pushDebugGroup:@(name)];
        break;
      case MTL_COMPUTE_COMMAND_ENCODER:
        [active_compute_command_encoder_ pushDebugGroup:@(name)];
        break;
      default:
        // [active_command_buffer_ pushDebugGroup:@(name)];
        break;
    }
  }
}

void MTLCommandBufferManager::pop_debug_group()
{
  /* Only perform this operation if capturing. */
  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  if (![capture_manager isCapturing]) {
    return;
  }

  if (active_command_buffer_ == nil) {
    return;
  }

  id<MTLCommandBuffer> cmd = this->ensure_begin();
  if (cmd != nil) {
    switch (active_command_encoder_type_) {
      case MTL_RENDER_COMMAND_ENCODER:
        [active_render_command_encoder_ popDebugGroup];
        break;
      case MTL_BLIT_COMMAND_ENCODER:
        [active_blit_command_encoder_ popDebugGroup];
        break;
      case MTL_COMPUTE_COMMAND_ENCODER:
        [active_compute_command_encoder_ popDebugGroup];
        break;
      default:
        // [active_command_buffer_ popDebugGroup];
        break;
    }
  }
}

void MTLCommandBufferManager::unfold_pending_debug_groups()
{
  /* Only perform this operation if capturing. */
  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  if (![capture_manager isCapturing]) {
    return;
  }

  if (active_command_buffer_ != nil) {
    MTLContext *ctx = MTLContext::get();
    const DebugStack &gpu_stack = ctx->debug_stack;
    DebugStack &mtl_stack = mtl_debug_stack_;
    /* Bottom level group is the label of the command buffer. */
    Span<StringRef> groups = gpu_stack.as_span().drop_front(1);

    /* Close closed groups. */
    for ([[maybe_unused]] int i : mtl_stack.index_range().drop_front(groups.size())) {
      pop_debug_group();
      mtl_stack.pop_last();
    }
    BLI_assert(mtl_stack.size() <= groups.size());
    /* Verify all open groups are the same on both stack. */
    int first_mismatch = 0;
    for (int i : mtl_stack.index_range()) {
      if (mtl_stack[i] != groups[i]) {
        break;
      }
      first_mismatch++;
    }
    /* Discard the ones that are not. */
    for ([[maybe_unused]] int i : mtl_stack.index_range().drop_front(first_mismatch)) {
      pop_debug_group();
      mtl_stack.pop_last();
    }
    BLI_assert(mtl_stack.size() <= groups.size());
    /* Add new groups not present in the metal stack. */
    for (const StringRef &name : groups.drop_front(mtl_stack.size())) {
      std::string s_name = name;
      push_debug_group(s_name.c_str(), 0);
      mtl_stack.append(name);
    }
    BLI_assert(mtl_stack.size() == groups.size());
  }
}

void MTLCommandBufferManager::fold_remaining_debug_groups()
{
  /* Only perform this operation if capturing. */
  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  if (![capture_manager isCapturing]) {
    return;
  }

  if (active_command_buffer_ != nil) {
    DebugStack &mtl_stack = mtl_debug_stack_;
    for ([[maybe_unused]] int i : mtl_stack.index_range()) {
      pop_debug_group();
      mtl_stack.pop_last();
    }
  }
}

/* Workload Synchronization. */
bool MTLCommandBufferManager::insert_memory_barrier(GPUBarrier barrier_bits,
                                                    GPUStageBarrierBits before_stages,
                                                    GPUStageBarrierBits after_stages)
{
  /* Apple Silicon does not support memory barriers for RenderCommandEncoder's.
   * We do not currently need these due to implicit API guarantees. However, render->render
   * resource dependencies are only evaluated at RenderCommandEncoder boundaries due to work
   * execution on TBDR architecture.
   *
   * NOTE: Render barriers are therefore inherently expensive. Where possible, opt for local
   * synchronization using raster order groups, or, prefer compute to avoid subsequent passes
   * re-loading pass attachments which are not needed. */
  const bool is_tile_based_arch = (GPU_platform_architecture() == GPU_ARCHITECTURE_TBDR);
  if (is_tile_based_arch) {
    if (active_command_encoder_type_ == MTL_RENDER_COMMAND_ENCODER) {
      /* Break render pass to ensure final pass results are visible to subsequent calls. */
      end_active_command_encoder();
      return true;
    }
    /* Skip all barriers for compute and blit passes as Metal will resolve these dependencies. */
    return false;
  }

  /* Resolve scope. */
  MTLBarrierScope scope = 0;
  if (barrier_bits & GPU_BARRIER_SHADER_IMAGE_ACCESS || barrier_bits & GPU_BARRIER_TEXTURE_FETCH) {
    bool is_compute = (active_command_encoder_type_ != MTL_RENDER_COMMAND_ENCODER);
    scope |= (is_compute ? 0 : MTLBarrierScopeRenderTargets) | MTLBarrierScopeTextures;
  }
  if (barrier_bits & GPU_BARRIER_SHADER_STORAGE ||
      barrier_bits & GPU_BARRIER_VERTEX_ATTRIB_ARRAY || barrier_bits & GPU_BARRIER_ELEMENT_ARRAY ||
      barrier_bits & GPU_BARRIER_UNIFORM || barrier_bits & GPU_BARRIER_BUFFER_UPDATE)
  {
    scope = scope | MTLBarrierScopeBuffers;
  }

  if (scope != 0) {
    /* Issue barrier based on encoder. */
    switch (active_command_encoder_type_) {
      case MTL_NO_COMMAND_ENCODER:
      case MTL_BLIT_COMMAND_ENCODER: {
        /* No barrier to be inserted. */
        return false;
      }

      /* Rendering. */
      case MTL_RENDER_COMMAND_ENCODER: {
        /* Currently flagging both stages -- can use bits above to filter on stage type --
         * though full barrier is safe for now. */
        MTLRenderStages before_stage_flags = 0;
        MTLRenderStages after_stage_flags = 0;
        if (before_stages & GPU_BARRIER_STAGE_VERTEX &&
            !(before_stages & GPU_BARRIER_STAGE_FRAGMENT))
        {
          before_stage_flags = before_stage_flags | MTLRenderStageVertex;
        }
        if (before_stages & GPU_BARRIER_STAGE_FRAGMENT) {
          before_stage_flags = before_stage_flags | MTLRenderStageFragment;
        }
        if (after_stages & GPU_BARRIER_STAGE_VERTEX) {
          after_stage_flags = after_stage_flags | MTLRenderStageVertex;
        }
        if (after_stages & GPU_BARRIER_STAGE_FRAGMENT) {
          after_stage_flags = MTLRenderStageFragment;
        }

        id<MTLRenderCommandEncoder> rec = this->get_active_render_command_encoder();
        BLI_assert(rec != nil);
        [rec memoryBarrierWithScope:scope
                        afterStages:after_stage_flags
                       beforeStages:before_stage_flags];
        return true;
      }

      /* Compute. */
      case MTL_COMPUTE_COMMAND_ENCODER: {
        id<MTLComputeCommandEncoder> rec = this->get_active_compute_command_encoder();
        BLI_assert(rec != nil);
        [rec memoryBarrierWithScope:scope];
        return true;
      }
    }
  }
  /* No barrier support. */
  return false;
}

void MTLCommandBufferManager::encode_signal_event(id<MTLEvent> event, uint64_t signal_value)
{
  /* Ensure active command buffer. */
  id<MTLCommandBuffer> cmd_buf = this->ensure_begin();
  BLI_assert(cmd_buf);
  this->end_active_command_encoder();
  [cmd_buf encodeSignalEvent:event value:signal_value];
  register_encoder_counters();
}

void MTLCommandBufferManager::encode_wait_for_event(id<MTLEvent> event, uint64_t signal_value)
{
  /* Ensure active command buffer. */
  id<MTLCommandBuffer> cmd_buf = this->ensure_begin();
  BLI_assert(cmd_buf);
  this->end_active_command_encoder();
  [cmd_buf encodeWaitForEvent:event value:signal_value];
  register_encoder_counters();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Pass State for active RenderCommandEncoder
 * \{ */

/* Reset binding state when a new RenderCommandEncoder is bound, to ensure
 * pipeline resources are re-applied to the new Encoder.
 * NOTE: In Metal, state is only persistent within an MTLCommandEncoder,
 * not globally. */
void MTLRenderPassState::reset_state()
{
  /* Reset Cached pipeline state. */
  this->bound_pso = nil;
  this->bound_ds_state = nil;

  /* Clear shader binding. */
  this->last_bound_shader_state = {nullptr, 0};

  /* Other states. */
  MTLFrameBuffer *fb = this->cmd.get_active_framebuffer();
  this->last_used_stencil_ref_value = 0;
  this->last_scissor_rect = {0,
                             0,
                             (uint)((fb != nullptr) ? fb->get_width() : 0),
                             (uint)((fb != nullptr) ? fb->get_height() : 0)};

  this->vertex_bindings = {};
  this->fragment_bindings = {};
}

void MTLComputeState::reset_state()
{
  /* Reset Cached pipeline state. */
  this->bound_pso = nil;

  this->compute_bindings = {};
}

void MTLComputeState::bind_pso(id<MTLComputePipelineState> pso)
{
  if (this->bound_pso != pso) {
    id<MTLComputeCommandEncoder> rec = this->cmd.get_active_compute_command_encoder();
    [rec setComputePipelineState:pso];
    this->bound_pso = pso;
  }
}

void MTLComputeState::bind_compute_texture(id<MTLTexture> tex, uint slot)
{
  this->compute_bindings.bind_texture(this->cmd.get_active_compute_command_encoder(), tex, slot);
}

void MTLComputeState::bind_compute_sampler(MTLSamplerBinding &sampler_binding,
                                           bool use_samplers_argument_buffer,
                                           uint slot)
{
  this->compute_bindings.bind_sampler(this->cmd.get_active_compute_command_encoder(),
                                      this->ctx.get_sampler_array(),
                                      sampler_binding.get_mtl_sampler(this->ctx),
                                      sampler_binding.state,
                                      use_samplers_argument_buffer,
                                      slot);
}

void MTLComputeState::bind_compute_buffer(id<MTLBuffer> buffer, uint64_t buffer_offset, uint index)
{
  this->compute_bindings.bind_buffer(
      this->cmd.get_active_compute_command_encoder(), buffer, buffer_offset, index);
}

void MTLComputeState::bind_compute_bytes(const void *bytes, uint64_t length, uint index)
{
  this->compute_bindings.bind_bytes(this->cmd.get_active_compute_command_encoder(),
                                    this->ctx.get_scratch_buffer_manager(),
                                    bytes,
                                    length,
                                    index);
}

void MTLRenderPassState::bind_vertex_texture(id<MTLTexture> tex, uint slot)
{
  this->vertex_bindings.bind_texture(this->cmd.get_active_render_command_encoder(), tex, slot);
}

void MTLRenderPassState::bind_vertex_sampler(MTLSamplerBinding &sampler_binding,
                                             bool use_samplers_argument_buffer,
                                             uint slot)
{
  this->vertex_bindings.bind_sampler(this->cmd.get_active_render_command_encoder(),
                                     this->ctx.get_sampler_array(),
                                     sampler_binding.get_mtl_sampler(this->ctx),
                                     sampler_binding.state,
                                     use_samplers_argument_buffer,
                                     slot);
}

void MTLRenderPassState::bind_vertex_buffer(id<MTLBuffer> buffer,
                                            uint64_t buffer_offset,
                                            uint index)
{
  this->vertex_bindings.bind_buffer(
      this->cmd.get_active_render_command_encoder(), buffer, buffer_offset, index);
}

void MTLRenderPassState::bind_vertex_bytes(const void *bytes, uint64_t length, uint index)
{
  this->vertex_bindings.bind_bytes(this->cmd.get_active_render_command_encoder(),
                                   this->ctx.get_scratch_buffer_manager(),
                                   bytes,
                                   length,
                                   index);
}

void MTLRenderPassState::bind_fragment_texture(id<MTLTexture> tex, uint slot)
{
  this->fragment_bindings.bind_texture(this->cmd.get_active_render_command_encoder(), tex, slot);
}

void MTLRenderPassState::bind_fragment_sampler(MTLSamplerBinding &sampler_binding,
                                               bool use_samplers_argument_buffer,
                                               uint slot)
{
  this->fragment_bindings.bind_sampler(this->cmd.get_active_render_command_encoder(),
                                       this->ctx.get_sampler_array(),
                                       sampler_binding.get_mtl_sampler(this->ctx),
                                       sampler_binding.state,
                                       use_samplers_argument_buffer,
                                       slot);
}

void MTLRenderPassState::bind_fragment_buffer(id<MTLBuffer> buffer,
                                              uint64_t buffer_offset,
                                              uint index)
{
  this->fragment_bindings.bind_buffer(
      this->cmd.get_active_render_command_encoder(), buffer, buffer_offset, index);
}

void MTLRenderPassState::bind_fragment_bytes(const void *bytes, uint64_t length, uint index)
{
  this->fragment_bindings.bind_bytes(this->cmd.get_active_render_command_encoder(),
                                     this->ctx.get_scratch_buffer_manager(),
                                     bytes,
                                     length,
                                     index);
}

/** \} */

id<MTLSamplerState> MTLSamplerBinding::get_mtl_sampler(MTLContext &ctx)
{
  return (this->state == DEFAULT_SAMPLER_STATE) ? ctx.get_default_sampler_state() :
                                                  ctx.get_sampler_from_state(this->state);
}

void MTLComputeCommandEncoder::set_buffer_offset(size_t offset, int index)
{
  [enc setBufferOffset:offset atIndex:index];
}
void MTLComputeCommandEncoder::set_buffer(id<MTLBuffer> buf, size_t offset, int index)
{
  [enc setBuffer:buf offset:offset atIndex:index];
}
void MTLComputeCommandEncoder::set_bytes(const void *bytes, size_t length, int index)
{
  [enc setBytes:bytes length:length atIndex:index];
}
void MTLComputeCommandEncoder::set_texture(id<MTLTexture> tex, int index)
{
  [enc setTexture:tex atIndex:index];
}
void MTLComputeCommandEncoder::set_sampler(id<MTLSamplerState> sampler_state, int index)
{
  [enc setSamplerState:sampler_state atIndex:index];
}

void MTLVertexCommandEncoder::set_buffer_offset(size_t offset, int index)
{
  [enc setVertexBufferOffset:offset atIndex:index];
}
void MTLVertexCommandEncoder::set_buffer(id<MTLBuffer> buf, size_t offset, int index)
{
  [enc setVertexBuffer:buf offset:offset atIndex:index];
}
void MTLVertexCommandEncoder::set_bytes(const void *bytes, size_t length, int index)
{
  [enc setVertexBytes:bytes length:length atIndex:index];
}
void MTLVertexCommandEncoder::set_texture(id<MTLTexture> tex, int index)
{
  [enc setVertexTexture:tex atIndex:index];
}
void MTLVertexCommandEncoder::set_sampler(id<MTLSamplerState> sampler_state, int index)
{
  [enc setVertexSamplerState:sampler_state atIndex:index];
}

void MTLFragmentCommandEncoder::set_buffer_offset(size_t offset, int index)
{
  [enc setFragmentBufferOffset:offset atIndex:index];
}
void MTLFragmentCommandEncoder::set_buffer(id<MTLBuffer> buf, size_t offset, int index)
{
  [enc setFragmentBuffer:buf offset:offset atIndex:index];
}
void MTLFragmentCommandEncoder::set_bytes(const void *bytes, size_t length, int index)
{
  [enc setFragmentBytes:bytes length:length atIndex:index];
}
void MTLFragmentCommandEncoder::set_texture(id<MTLTexture> tex, int index)
{
  [enc setFragmentTexture:tex atIndex:index];
}
void MTLFragmentCommandEncoder::set_sampler(id<MTLSamplerState> sampler_state, int index)
{
  [enc setFragmentSamplerState:sampler_state atIndex:index];
}

}  // namespace blender::gpu

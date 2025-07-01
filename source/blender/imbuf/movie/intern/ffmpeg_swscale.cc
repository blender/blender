/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#ifdef WITH_FFMPEG
#  include "ffmpeg_swscale.hh"

#  include <cstdint>
#  include <mutex>

#  include "BLI_mutex.hh"
#  include "BLI_threads.h"
#  include "BLI_vector.hh"

extern "C" {
#  include <libavutil/opt.h>
#  include <libavutil/pixfmt.h>
#  include <libswscale/swscale.h>

#  include "ffmpeg_compat.h"
}

/* libswscale context creation and destruction is expensive.
 * Maintain a cache of already created contexts. */

static constexpr int64_t swscale_cache_max_entries = 32;

struct SwscaleContext {
  int src_width = 0, src_height = 0;
  int dst_width = 0, dst_height = 0;
  AVPixelFormat src_format = AV_PIX_FMT_NONE, dst_format = AV_PIX_FMT_NONE;
  bool src_full_range = false, dst_full_range = false;
  int src_colorspace = -1, dst_colorspace = -1;
  int flags = 0;

  SwsContext *context = nullptr;
  int64_t last_use_timestamp = 0;
  bool is_used = false;
};

static blender::Mutex swscale_cache_lock;
static int64_t swscale_cache_timestamp = 0;
static blender::Vector<SwscaleContext> *swscale_cache = nullptr;

static SwsContext *sws_create_context(int src_width,
                                      int src_height,
                                      int av_src_format,
                                      int dst_width,
                                      int dst_height,
                                      int av_dst_format,
                                      int sws_flags)
{
#  if defined(FFMPEG_SWSCALE_THREADING)
  /* sws_getContext does not allow passing flags that ask for multi-threaded
   * scaling context, so do it the hard way. */
  SwsContext *c = sws_alloc_context();
  if (c == nullptr) {
    return nullptr;
  }
  av_opt_set_int(c, "srcw", src_width, 0);
  av_opt_set_int(c, "srch", src_height, 0);
  av_opt_set_int(c, "src_format", av_src_format, 0);
  av_opt_set_int(c, "dstw", dst_width, 0);
  av_opt_set_int(c, "dsth", dst_height, 0);
  av_opt_set_int(c, "dst_format", av_dst_format, 0);
  av_opt_set_int(c, "sws_flags", sws_flags, 0);
  av_opt_set_int(c, "threads", BLI_system_thread_count(), 0);

  if (sws_init_context(c, nullptr, nullptr) < 0) {
    sws_freeContext(c);
    return nullptr;
  }
#  else
  SwsContext *c = sws_getContext(src_width,
                                 src_height,
                                 AVPixelFormat(av_src_format),
                                 dst_width,
                                 dst_height,
                                 AVPixelFormat(av_dst_format),
                                 sws_flags,
                                 nullptr,
                                 nullptr,
                                 nullptr);
#  endif

  return c;
}

static void init_swscale_cache_if_needed()
{
  if (swscale_cache == nullptr) {
    swscale_cache = new blender::Vector<SwscaleContext>();
    swscale_cache_timestamp = 0;
  }
}

static bool remove_oldest_swscale_context()
{
  int64_t oldest_index = -1;
  int64_t oldest_time = 0;
  for (int64_t index = 0; index < swscale_cache->size(); index++) {
    SwscaleContext &ctx = (*swscale_cache)[index];
    if (ctx.is_used) {
      continue;
    }
    int64_t time = swscale_cache_timestamp - ctx.last_use_timestamp;
    if (time > oldest_time) {
      oldest_time = time;
      oldest_index = index;
    }
  }

  if (oldest_index >= 0) {
    SwscaleContext &ctx = (*swscale_cache)[oldest_index];
    sws_freeContext(ctx.context);
    swscale_cache->remove_and_reorder(oldest_index);
    return true;
  }
  return false;
}

static void maintain_swscale_cache_size()
{
  while (swscale_cache->size() > swscale_cache_max_entries) {
    if (!remove_oldest_swscale_context()) {
      /* Could not remove anything (all contexts are actively used),
       * stop trying. */
      break;
    }
  }
}

SwsContext *ffmpeg_sws_get_context(int src_width,
                                   int src_height,
                                   int av_src_format,
                                   bool src_full_range,
                                   int src_color_space,
                                   int dst_width,
                                   int dst_height,
                                   int av_dst_format,
                                   bool dst_full_range,
                                   int dst_color_space,
                                   int sws_flags)
{
  std::lock_guard lock(swscale_cache_lock);

  init_swscale_cache_if_needed();

  swscale_cache_timestamp++;

  /* Search for unused context that has suitable parameters. */
  SwsContext *ctx = nullptr;
  for (SwscaleContext &c : *swscale_cache) {
    if (!c.is_used && c.src_width == src_width && c.src_height == src_height &&
        c.src_format == av_src_format && c.src_full_range == src_full_range &&
        c.src_colorspace == src_color_space && c.dst_width == dst_width &&
        c.dst_height == dst_height && c.dst_format == av_dst_format &&
        c.dst_full_range == dst_full_range && c.dst_colorspace == dst_color_space &&
        c.flags == sws_flags)
    {
      ctx = c.context;
      /* Mark as used. */
      c.is_used = true;
      c.last_use_timestamp = swscale_cache_timestamp;
      break;
    }
  }
  if (ctx == nullptr) {
    /* No free matching context in cache: create a new one. */
    ctx = sws_create_context(
        src_width, src_height, av_src_format, dst_width, dst_height, av_dst_format, sws_flags);

    int src_range, dst_range, brightness, contrast, saturation;
    const int *table, *inv_table;
    if (sws_getColorspaceDetails(ctx,
                                 (int **)&inv_table,
                                 &src_range,
                                 (int **)&table,
                                 &dst_range,
                                 &brightness,
                                 &contrast,
                                 &saturation) >= 0)
    {
      if (src_full_range) {
        src_range = 1;
      }
      if (dst_full_range) {
        dst_range = 1;
      }
      if (src_color_space >= 0) {
        inv_table = sws_getCoefficients(src_color_space);
      }
      if (dst_color_space >= 0) {
        table = sws_getCoefficients(dst_color_space);
      }
      sws_setColorspaceDetails(
          ctx, (int *)inv_table, src_range, table, dst_range, brightness, contrast, saturation);
    }

    SwscaleContext c;
    c.src_width = src_width;
    c.src_height = src_height;
    c.dst_width = dst_width;
    c.dst_height = dst_height;
    c.src_format = AVPixelFormat(av_src_format);
    c.dst_format = AVPixelFormat(av_dst_format);
    c.src_full_range = src_full_range;
    c.dst_full_range = dst_full_range;
    c.src_colorspace = src_color_space;
    c.dst_colorspace = dst_color_space;
    c.flags = sws_flags;
    c.context = ctx;
    c.is_used = true;
    c.last_use_timestamp = swscale_cache_timestamp;
    swscale_cache->append(c);

    maintain_swscale_cache_size();
  }
  return ctx;
}

void ffmpeg_sws_release_context(SwsContext *ctx)
{
  std::lock_guard lock(swscale_cache_lock);
  init_swscale_cache_if_needed();

  bool found = false;
  for (SwscaleContext &c : *swscale_cache) {
    if (c.context == ctx) {
      BLI_assert_msg(c.is_used, "Releasing ffmpeg swscale context that is not in use");
      c.is_used = false;
      found = true;
      break;
    }
  }
  BLI_assert_msg(found, "Releasing ffmpeg swscale context that is not in cache");
  UNUSED_VARS_NDEBUG(found);
  maintain_swscale_cache_size();
}

void ffmpeg_sws_exit()
{
  std::lock_guard lock(swscale_cache_lock);
  if (swscale_cache != nullptr) {
    for (SwscaleContext &c : *swscale_cache) {
      sws_freeContext(c.context);
    }
    delete swscale_cache;
    swscale_cache = nullptr;
  }
}

void ffmpeg_sws_scale_frame(SwsContext *ctx, AVFrame *dst, const AVFrame *src)
{
#  if defined(FFMPEG_SWSCALE_THREADING)
  sws_scale_frame(ctx, dst, src);
#  else
  sws_scale(ctx, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
#  endif
}

#endif /* WITH_FFMPEG */

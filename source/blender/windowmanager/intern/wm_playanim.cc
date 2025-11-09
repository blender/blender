/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Animation player for image sequences & video's with sound support.
 * Launched in a separate process from Blender's #RENDER_OT_play_rendered_anim
 *
 * \note This file uses ghost directly and none of the WM definitions.
 * this could be made into its own module, alongside creator.
 */

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>

#ifndef WIN32
#  include <sys/times.h>
#  include <sys/wait.h>
#  include <unistd.h>
#else
#  include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_enum_flags.hh"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_system.h"
#include "BLI_time.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"
#include "MOV_util.hh"

#include "BKE_blender.hh"
#include "BKE_image.hh"

#include "BIF_glutil.hh"

#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_init_exit.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLF_api.hh"
#include "GHOST_C-api.h"

#include "wm_window_private.hh"

#include "WM_api.hh" /* Only for #WM_main_playanim. */

#ifdef WITH_AUDASPACE
#  include <AUD_Device.h>
#  include <AUD_Handle.h>
#  include <AUD_Sound.h>
#  include <AUD_Special.h>

static struct {
  AUD_Sound *source;
  AUD_Handle *playback_handle;
  AUD_Handle *scrub_handle;
  AUD_Device *audio_device;
} g_audaspace = {nullptr};
#endif

/* Simple limiter to avoid flooding memory. */
#define USE_FRAME_CACHE_LIMIT
#ifdef USE_FRAME_CACHE_LIMIT
#  define PLAY_FRAME_CACHE_MAX 30
#endif

static CLG_LogRef LOG = {"image"};

/** Used in user viable messages. */
static const char *message_prefix = "Animation Player";

struct PlayState;
static void playanim_window_zoom(PlayState &ps, const float zoom_offset);
static bool playanim_window_font_scale_from_dpi(PlayState &ps);

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

/**
 * \param filepath: The file path to read into memory.
 * \param r_mem: Optional, when nullptr, don't allocate memory (just set the size).
 * \param r_size: The file-size of `filepath`.
 */
static bool buffer_from_filepath(const char *filepath,
                                 void **r_mem,
                                 size_t *r_size,
                                 char **r_error_message)
{
  errno = 0;
  const int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
  if (UNLIKELY(file == -1)) {
    *r_error_message = BLI_sprintfN("failure '%s' to open file", strerror(errno));
    return false;
  }

  bool success = false;
  uchar *mem = nullptr;
  const size_t size = BLI_file_descriptor_size(file);
  int64_t size_read;
  if (UNLIKELY(size == size_t(-1))) {
    *r_error_message = BLI_sprintfN("failure '%s' to access size", strerror(errno));
  }
  else if (r_mem && UNLIKELY(!(mem = MEM_malloc_arrayN<uchar>(size, __func__)))) {
    *r_error_message = BLI_sprintfN("error allocating buffer %" PRIu64 " size", uint64_t(size));
  }
  else if (r_mem && UNLIKELY((size_read = BLI_read(file, mem, size)) != size)) {
    *r_error_message = BLI_sprintfN(
        "error '%s' reading file "
        "(expected %" PRIu64 ", was %" PRId64 ")",
        strerror(errno),
        uint64_t(size),
        size_read);
  }
  else {
    *r_size = size;
    if (r_mem) {
      *r_mem = mem;
      mem = nullptr; /* `r_mem` owns, don't free on exit. */
    }
    success = true;
  }

  MEM_SAFE_FREE(mem);
  close(file);
  return success;
}

/** \} */

/** Use a flag to store held modifiers & mouse buttons. */
enum eWS_Qual {
  WS_QUAL_LSHIFT = (1 << 0),
  WS_QUAL_RSHIFT = (1 << 1),
#define WS_QUAL_SHIFT (WS_QUAL_LSHIFT | WS_QUAL_RSHIFT)
  WS_QUAL_LALT = (1 << 2),
  WS_QUAL_RALT = (1 << 3),
#define WS_QUAL_ALT (WS_QUAL_LALT | WS_QUAL_RALT)
  WS_QUAL_LCTRL = (1 << 4),
  WS_QUAL_RCTRL = (1 << 5),
#define WS_QUAL_CTRL (WS_QUAL_LCTRL | WS_QUAL_RCTRL)
  WS_QUAL_LMOUSE = (1 << 16),
  WS_QUAL_MMOUSE = (1 << 17),
  WS_QUAL_RMOUSE = (1 << 18),
#define WS_QUAL_MOUSE (WS_QUAL_LMOUSE | WS_QUAL_MMOUSE | WS_QUAL_RMOUSE)
};
ENUM_OPERATORS(eWS_Qual)

struct GhostData {
  GHOST_SystemHandle system;
  GHOST_WindowHandle window;

  /** Not GHOST, but low level GPU context. */
  GPUContext *gpu_context;

  /** Held keys. */
  eWS_Qual qual;
};

struct PlayArgs {
  int argc;
  char **argv;
};

/**
 * The minimal context necessary for displaying an image.
 * Used while displaying images both on load and while playing.
 */
struct PlayDisplayContext {
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  /** Scale calculated from the DPI. */
  float ui_scale;
  /** Window & viewport size in pixels. */
  blender::int2 size;
};

/**
 * The current state of the player.
 *
 * \warning Don't store results of parsing command-line arguments
 * in this struct if they need to persist across playing back different
 * files as these will be cleared when playing other files (drag & drop).
 */
struct PlayState {
  /** Context for displaying images (color spaces & display-size). */
  PlayDisplayContext display_ctx;

  /** Current zoom level. */
  float zoom;

  /** Playback direction (-1, 1). */
  short direction;
  /** Set the next frame to implement frame stepping (using shortcuts). */
  short next_frame;

  /** Playback once then wait. */
  bool once;
  /** Play forwards/backwards. */
  bool pingpong;
  /** Disable frame skipping. */
  bool no_frame_skip;
  /** Display current frame over the window. */
  bool show_frame_indicator;
  /** Single-frame stepping has been enabled (frame loading and update pending). */
  bool single_step;
  /** Playback has stopped the image has been displayed. */
  bool wait;
  /** Playback stopped state once stop/start variables have been handled. */
  bool stopped;
  /**
   * When disabled the current animation will exit,
   * after this either the application exits or a new animation window is opened.
   *
   * This is used so drag & drop can load new files which setup a newly created animation window.
   */
  bool go;
  /** True when waiting for images to load. */
  bool loading;
  /** X/Y image flip (set via key bindings). */
  bool draw_flip[2];

  /** The number of frames to step each update (default to 1, command line argument). */
  int frame_step;

  /** Picture #PlayAnimPict, list (both image-sequence or videos) in-memory. */
  ListBase picsbase;

  /** Current frame (picture). */
  struct PlayAnimPict *picture;

  /** Image size in pixels, set once at the start. */
  blender::int2 ibuf_size;
  /** Mono-space font ID. */
  int font_id;
  int font_size;

  /** Restarts player for file drop (drag & drop). */
  int argc_next;
  char **argv_next;

  /** Force update when scrubbing with the cursor. */
  bool need_frame_update;
  /** The current frame calculated by scrubbing the mouse cursor. */
  int frame_cursor_x;

  GhostData ghost_data;
};

/* For debugging. */
#if 0
static void print_ps(const PlayState &ps)
{
  printf("ps:\n");
  printf("    direction=%d,\n", int(ps.direction));
  printf("    once=%d,\n", ps.once);
  printf("    pingpong=%d,\n", ps.pingpong);
  printf("    no_frame_skip=%d,\n", ps.no_frame_skip);
  printf("    single_step=%d,\n", ps.single_step);
  printf("    wait=%d,\n", ps.wait);
  printf("    stopped=%d,\n", ps.stopped);
  printf("    go=%d,\n\n", ps.go);
  fflush(stdout);
}
#endif

static blender::int2 playanim_window_size_get(GHOST_WindowHandle ghost_window)
{
  ;
  GHOST_RectangleHandle bounds = GHOST_GetClientBounds(ghost_window);
  const float native_pixel_size = GHOST_GetNativePixelSize(ghost_window);
  const blender::int2 window_size = {
      int(GHOST_GetWidthRectangle(bounds) * native_pixel_size),
      int(GHOST_GetHeightRectangle(bounds) * native_pixel_size),
  };
  GHOST_DisposeRectangle(bounds);
  return window_size;
}

static void playanim_gpu_matrix()
{
  /* Unified matrix, note it affects offset for drawing. */
  /* NOTE: cannot use GPU_matrix_ortho_2d_set here because shader ignores. */
  GPU_matrix_ortho_set(0.0f, 1.0f, 0.0f, 1.0f, -1.0, 1.0f);
}

/* Implementation. */
static void playanim_event_qual_update(GhostData &ghost_data)
{
  bool val;

  /* Shift. */
  GHOST_GetModifierKeyState(ghost_data.system, GHOST_kModifierKeyLeftShift, &val);
  SET_FLAG_FROM_TEST(ghost_data.qual, val, WS_QUAL_LSHIFT);

  GHOST_GetModifierKeyState(ghost_data.system, GHOST_kModifierKeyRightShift, &val);
  SET_FLAG_FROM_TEST(ghost_data.qual, val, WS_QUAL_RSHIFT);

  /* Control. */
  GHOST_GetModifierKeyState(ghost_data.system, GHOST_kModifierKeyLeftControl, &val);
  SET_FLAG_FROM_TEST(ghost_data.qual, val, WS_QUAL_LCTRL);

  GHOST_GetModifierKeyState(ghost_data.system, GHOST_kModifierKeyRightControl, &val);
  SET_FLAG_FROM_TEST(ghost_data.qual, val, WS_QUAL_RCTRL);

  /* Alt. */
  GHOST_GetModifierKeyState(ghost_data.system, GHOST_kModifierKeyLeftAlt, &val);
  SET_FLAG_FROM_TEST(ghost_data.qual, val, WS_QUAL_LALT);

  GHOST_GetModifierKeyState(ghost_data.system, GHOST_kModifierKeyRightAlt, &val);
  SET_FLAG_FROM_TEST(ghost_data.qual, val, WS_QUAL_RALT);
}

struct PlayAnimPict {
  PlayAnimPict *next, *prev;
  uchar *mem;
  size_t size;
  /** The allocated file-path to the image. */
  const char *filepath;
  /** The allocated error message to show if the file cannot be loaded. */
  char *error_message;
  ImBuf *ibuf;
  MovieReader *anim;
  int frame;
  int IB_flags;

#ifdef USE_FRAME_CACHE_LIMIT
  /** Back pointer to the #LinkData node for this struct in the #g_frame_cache.pics list. */
  LinkData *frame_cache_node;
  size_t size_in_memory;
#endif
};

/**
 * Various globals relating to playback.
 * \note Avoid adding members here where possible,
 * prefer #PlayState or one of its members where possible.
 */
static struct {
  bool from_disk;
  double swap_time;
  double total_time;
#ifdef WITH_AUDASPACE
  double fps_movie;
#endif
} g_playanim = {
    /*from_disk*/ false,
    /*swap_time*/ 0.04,
    /*total_time*/ 0.0,
#ifdef WITH_AUDASPACE
    /*fps_movie*/ 0.0,
#endif
};

#ifdef USE_FRAME_CACHE_LIMIT
static struct {
  /** A list of #LinkData nodes referencing #PlayAnimPict to track cached frames. */
  ListBase pics;
  /** Number if elements in `pics`. */
  int pics_len;
  /** Keep track of memory used by #g_frame_cache.pics when `g_frame_cache.memory_limit != 0`. */
  size_t pics_size_in_memory;
  /** Optionally limit the amount of memory used for cache (in bytes), ignored when zero. */
  size_t memory_limit;
} g_frame_cache = {
    /*pics*/ {nullptr, nullptr},
    /*pics_len*/ 0,
    /*pics_size_in_memory*/ 0,
    /*memory_limit*/ 0,
};

static void frame_cache_add(PlayAnimPict *pic)
{
  pic->frame_cache_node = BLI_genericNodeN(pic);
  BLI_addhead(&g_frame_cache.pics, pic->frame_cache_node);
  g_frame_cache.pics_len++;

  if (g_frame_cache.memory_limit != 0) {
    BLI_assert(pic->size_in_memory == 0);
    pic->size_in_memory = IMB_get_size_in_memory(pic->ibuf);
    g_frame_cache.pics_size_in_memory += pic->size_in_memory;
  }
}

static void frame_cache_remove(PlayAnimPict *pic)
{
  LinkData *node = pic->frame_cache_node;
  IMB_freeImBuf(pic->ibuf);
  if (g_frame_cache.memory_limit != 0) {
    BLI_assert(pic->size_in_memory != 0);
    g_frame_cache.pics_size_in_memory -= pic->size_in_memory;
    pic->size_in_memory = 0;
  }
  pic->ibuf = nullptr;
  pic->frame_cache_node = nullptr;
  BLI_freelinkN(&g_frame_cache.pics, node);
  g_frame_cache.pics_len--;
}

/* Don't free the current frame by moving it to the head of the list. */
static void frame_cache_touch(PlayAnimPict *pic)
{
  BLI_assert(pic->frame_cache_node->data == pic);
  BLI_remlink(&g_frame_cache.pics, pic->frame_cache_node);
  BLI_addhead(&g_frame_cache.pics, pic->frame_cache_node);
}

static bool frame_cache_limit_exceeded()
{
  return g_frame_cache.memory_limit ?
             (g_frame_cache.pics_size_in_memory > g_frame_cache.memory_limit) :
             (g_frame_cache.pics_len > PLAY_FRAME_CACHE_MAX);
}

static void frame_cache_limit_apply(ImBuf *ibuf_keep)
{
  /* Really basic memory conservation scheme. Keep frames in a FIFO queue. */
  LinkData *node = static_cast<LinkData *>(g_frame_cache.pics.last);
  while (node && frame_cache_limit_exceeded()) {
    PlayAnimPict *pic = static_cast<PlayAnimPict *>(node->data);
    BLI_assert(pic->frame_cache_node == node);

    node = node->prev;
    if (pic->ibuf && pic->ibuf != ibuf_keep) {
      frame_cache_remove(pic);
    }
  }
}

#endif /* USE_FRAME_CACHE_LIMIT */

static ImBuf *ibuf_from_picture(PlayAnimPict *pic)
{
  ImBuf *ibuf = nullptr;

  if (pic->ibuf) {
    ibuf = pic->ibuf;
  }
  else if (pic->anim) {
    ibuf = MOV_decode_frame(pic->anim, pic->frame, IMB_TC_NONE, IMB_PROXY_NONE);
  }
  else if (pic->mem) {
    /* Use correct color-space here. */
    ibuf = IMB_load_image_from_memory(
        pic->mem, pic->size, pic->IB_flags, pic->filepath, pic->filepath);
  }
  else {
    /* Use correct color-space here. */
    ibuf = IMB_load_image_from_filepath(pic->filepath, pic->IB_flags);
  }

  return ibuf;
}

static PlayAnimPict *playanim_step(PlayAnimPict *playanim, int step)
{
  if (step > 0) {
    while (step-- && playanim) {
      playanim = playanim->next;
    }
  }
  else if (step < 0) {
    while (step++ && playanim) {
      playanim = playanim->prev;
    }
  }
  return playanim;
}

static int pupdate_time()
{
  static double time_last;

  double time = BLI_time_now_seconds();

  g_playanim.total_time += (time - time_last);
  time_last = time;
  return (g_playanim.total_time < 0.0);
}

static void *ocio_transform_ibuf(const PlayDisplayContext &display_ctx,
                                 ImBuf *ibuf,
                                 bool *r_glsl_used,
                                 blender::gpu::TextureFormat *r_format,
                                 eGPUDataFormat *r_data,
                                 void **r_buffer_cache_handle)
{
  void *display_buffer;
  bool force_fallback = false;
  *r_glsl_used = false;
  force_fallback |= (ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL);
  force_fallback |= (ibuf->dither != 0.0f);

  /* Default. */
  *r_format = blender::gpu::TextureFormat::UNORM_8_8_8_8;
  *r_data = GPU_DATA_UBYTE;

  /* Fallback to CPU based color space conversion. */
  if (force_fallback) {
    *r_glsl_used = false;
    display_buffer = nullptr;
  }
  else if (ibuf->float_buffer.data) {
    display_buffer = ibuf->float_buffer.data;

    *r_data = GPU_DATA_FLOAT;
    if (ibuf->channels == 4) {
      *r_format = blender::gpu::TextureFormat::SFLOAT_16_16_16_16;
    }
    else if (ibuf->channels == 3) {
      /* Alpha is implicitly 1. */
      *r_format = blender::gpu::TextureFormat::SFLOAT_16_16_16;
    }

    if (ibuf->float_buffer.colorspace) {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space(&display_ctx.view_settings,
                                                                    &display_ctx.display_settings,
                                                                    ibuf->float_buffer.colorspace,
                                                                    ibuf->dither,
                                                                    false,
                                                                    false);
    }
    else {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw(
          &display_ctx.view_settings, &display_ctx.display_settings, ibuf->dither, false);
    }
  }
  else if (ibuf->byte_buffer.data) {
    display_buffer = ibuf->byte_buffer.data;
    *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space(&display_ctx.view_settings,
                                                                  &display_ctx.display_settings,
                                                                  ibuf->byte_buffer.colorspace,
                                                                  ibuf->dither,
                                                                  false,
                                                                  false);
  }
  else {
    display_buffer = nullptr;
  }

  /* There is data to be displayed, but GLSL is not initialized
   * properly, in this case we fallback to CPU-based display transform. */
  if ((ibuf->byte_buffer.data || ibuf->float_buffer.data) && !*r_glsl_used) {
    display_buffer = IMB_display_buffer_acquire(
        ibuf, &display_ctx.view_settings, &display_ctx.display_settings, r_buffer_cache_handle);
    *r_format = blender::gpu::TextureFormat::UNORM_8_8_8_8;
    *r_data = GPU_DATA_UBYTE;
  }

  return display_buffer;
}

static void draw_display_buffer(const PlayDisplayContext &display_ctx,
                                ImBuf *ibuf,
                                const rctf *canvas,
                                const bool draw_flip[2])
{
  /* Format needs to be created prior to any #immBindShader call.
   * Do it here because OCIO binds its own shader. */
  blender::gpu::TextureFormat format;
  eGPUDataFormat data;
  bool glsl_used = false;
  GPUVertFormat *imm_format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(imm_format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  uint texCoord = GPU_vertformat_attr_add(
      imm_format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);

  void *buffer_cache_handle = nullptr;
  void *display_buffer = ocio_transform_ibuf(
      display_ctx, ibuf, &glsl_used, &format, &data, &buffer_cache_handle);

  /* NOTE: This may fail, especially for large images that exceed the GPU's texture size limit.
   * Large images could be supported although this isn't so common for animation playback. */
  blender::gpu::Texture *texture = GPU_texture_create_2d(
      "display_buf", ibuf->x, ibuf->y, 1, format, GPU_TEXTURE_USAGE_SHADER_READ, nullptr);

  if (texture) {
    GPU_texture_update(texture, data, display_buffer);
    GPU_texture_filter_mode(texture, false);

    GPU_texture_bind(texture, 0);
  }

  if (!glsl_used) {
    immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);
    immUniformColor3f(1.0f, 1.0f, 1.0f);
  }

  immBegin(GPU_PRIM_TRI_FAN, 4);

  rctf preview;
  BLI_rctf_init(&preview, 0.0f, 1.0f, 0.0f, 1.0f);
  if (draw_flip[0]) {
    std::swap(preview.xmin, preview.xmax);
  }
  if (draw_flip[1]) {
    std::swap(preview.ymin, preview.ymax);
  }

  immAttr2f(texCoord, preview.xmin, preview.ymin);
  immVertex2f(pos, canvas->xmin, canvas->ymin);

  immAttr2f(texCoord, preview.xmin, preview.ymax);
  immVertex2f(pos, canvas->xmin, canvas->ymax);

  immAttr2f(texCoord, preview.xmax, preview.ymax);
  immVertex2f(pos, canvas->xmax, canvas->ymax);

  immAttr2f(texCoord, preview.xmax, preview.ymin);
  immVertex2f(pos, canvas->xmax, canvas->ymin);

  immEnd();

  if (texture) {
    GPU_texture_unbind(texture);
    GPU_texture_free(texture);
  }

  if (!glsl_used) {
    immUnbindProgram();
  }
  else {
    IMB_colormanagement_finish_glsl_draw();
  }

  if (buffer_cache_handle) {
    IMB_display_buffer_release(buffer_cache_handle);
  }
}

/**
 * \param font_id: ID of the font to display (-1 when no text should be displayed).
 * \param frame_step: Frame step (may be used in text display).
 * \param draw_zoom: Default to 1.0 (no zoom).
 * \param draw_flip: X/Y flipping (ignored when null).
 * \param frame_indicator_factor: Display a vertical frame-indicator (ignored when -1).
 */
static void playanim_toscreen_ex(GhostData &ghost_data,
                                 const PlayDisplayContext &display_ctx,
                                 const PlayAnimPict *picture,
                                 ImBuf *ibuf,
                                 /* Run-time drawing arguments (not used on-load). */
                                 const int font_id,
                                 const int frame_step,
                                 const float draw_zoom,
                                 const bool draw_flip[2],
                                 const float frame_indicator_factor)
{
  GHOST_ActivateWindowDrawingContext(ghost_data.window);
  GPU_render_begin();

  GHOST_SwapWindowBufferAcquire(ghost_data.window);
  GPUContext *restore_context = GPU_context_active_get();

  GPU_context_active_set(ghost_data.gpu_context);
  GPU_context_begin_frame(ghost_data.gpu_context);

  GPU_clear_color(0.1f, 0.1f, 0.1f, 0.0f);

  /* A null `ibuf` is an exceptional case and should almost never happen.
   * if it does, this function displays a warning along with the file-path that failed. */
  if (ibuf) {
    /* Size within window. */
    float span_x = (draw_zoom * ibuf->x) / float(display_ctx.size[0]);
    float span_y = (draw_zoom * ibuf->y) / float(display_ctx.size[1]);

    /* Offset within window. */
    float offs_x = 0.5f * (1.0f - span_x);
    float offs_y = 0.5f * (1.0f - span_y);

    CLAMP(offs_x, 0.0f, 1.0f);
    CLAMP(offs_y, 0.0f, 1.0f);

    /* Checkerboard for case alpha. */
    if (ibuf->planes == 32) {
      GPU_blend(GPU_BLEND_ALPHA);

      imm_draw_box_checker_2d_ex(offs_x,
                                 offs_y,
                                 offs_x + span_x,
                                 offs_y + span_y,
                                 blender::float4{0.15, 0.15, 0.15, 1.0},
                                 blender::float4{0.20, 0.20, 0.20, 1.0},
                                 8);
    }
    rctf canvas;
    BLI_rctf_init(&canvas, offs_x, offs_x + span_x, offs_y, offs_y + span_y);

    draw_display_buffer(display_ctx, ibuf, &canvas, draw_flip);

    GPU_blend(GPU_BLEND_NONE);
  }

  pupdate_time();

  if ((font_id != -1) && picture) {
    const int font_margin = int(10 * display_ctx.ui_scale);
    float fsizex_inv, fsizey_inv;
    char label[32 + FILE_MAX];
    if (ibuf) {
      SNPRINTF(label, "%s | %.2f frames/s", picture->filepath, frame_step / g_playanim.swap_time);
    }
    else {
      SNPRINTF(label,
               "%s | %s",
               picture->filepath,
               picture->error_message ? picture->error_message : "<unknown error>");
    }

    const blender::int2 window_size = playanim_window_size_get(ghost_data.window);
    fsizex_inv = 1.0f / window_size[0];
    fsizey_inv = 1.0f / window_size[1];

    BLF_color4f(font_id, 1.0, 1.0, 1.0, 1.0);

    /* FIXME(@ideasman42): Font positioning doesn't work because the aspect causes the position
     * to be rounded to zero, investigate making BLF support this,
     * for now use GPU matrix API to adjust the text position. */
#if 0
    BLF_enable(font_id, BLF_ASPECT);
    BLF_aspect(font_id, fsizex_inv, fsizey_inv, 1.0f);
    BLF_position(font_id, font_margin * fsizex_inv, font_margin * fsizey_inv, 0.0f);
    BLF_draw(font_id, label, sizeof(label));
#else
    GPU_matrix_push();
    GPU_matrix_scale_2f(fsizex_inv, fsizey_inv);
    GPU_matrix_translate_2f(font_margin, font_margin);
    BLF_position(font_id, 0, 0, 0.0f);
    BLF_draw(font_id, label, sizeof(label));
    GPU_matrix_pop();
#endif
  }

  if (frame_indicator_factor != -1.0f) {
    float fac = frame_indicator_factor;
    fac = 2.0f * fac - 1.0f;
    GPU_matrix_push_projection();
    GPU_matrix_identity_projection_set();
    GPU_matrix_push();
    GPU_matrix_identity_set();

    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor3ub(0, 255, 0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, fac, -1.0f);
    immVertex2f(pos, fac, 1.0f);
    immEnd();

    immUnbindProgram();

    GPU_matrix_pop();
    GPU_matrix_pop_projection();
  }

  GPU_render_step();
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    GPU_flush();
  }

  GPU_context_end_frame(ghost_data.gpu_context);
  GHOST_SwapWindowBufferRelease(ghost_data.window);
  GPU_context_active_set(restore_context);
  GPU_render_end();
}

static void playanim_toscreen_on_load(GhostData &ghost_data,
                                      const PlayDisplayContext &display_ctx,
                                      const PlayAnimPict *picture,
                                      ImBuf *ibuf)
{
  const int font_id = -1; /* Don't draw text. */
  const int frame_step = -1;
  const float zoom = 1.0f;
  const float frame_indicator_factor = -1.0f;
  const bool draw_flip[2] = {false, false};

  playanim_toscreen_ex(ghost_data,
                       display_ctx,
                       picture,
                       ibuf,
                       font_id,
                       frame_step,
                       zoom,
                       draw_flip,
                       frame_indicator_factor);
}

static void playanim_toscreen(PlayState &ps, const PlayAnimPict *picture, ImBuf *ibuf)
{
  float frame_indicator_factor = -1.0f;
  if (ps.show_frame_indicator) {
    const int frame_range = static_cast<const PlayAnimPict *>(ps.picsbase.last)->frame -
                            static_cast<const PlayAnimPict *>(ps.picsbase.first)->frame;
    if (frame_range > 0) {
      frame_indicator_factor = float(double(picture->frame) / double(frame_range));
    }
    else {
      BLI_assert_msg(BLI_listbase_is_single(&ps.picsbase),
                     "Multiple frames without a valid range!");
    }
  }

  int font_id = -1;
  if ((ps.ghost_data.qual & (WS_QUAL_SHIFT | WS_QUAL_LMOUSE)) ||
      /* Always inform the user of an error, this should be an exceptional case. */
      (ibuf == nullptr))
  {
    font_id = ps.font_id;
  }

  BLI_assert(ps.loading == false);
  playanim_toscreen_ex(ps.ghost_data,
                       ps.display_ctx,
                       picture,
                       ibuf,
                       font_id,
                       ps.frame_step,
                       ps.zoom,
                       ps.draw_flip,
                       frame_indicator_factor);
}

static void build_pict_list_from_anim(ListBase &picsbase,
                                      GhostData &ghost_data,
                                      const PlayDisplayContext &display_ctx,
                                      const char *filepath_first,
                                      const int frame_offset)
{
  /* OCIO_TODO: support different input color space. */
  MovieReader *anim = MOV_open_file(filepath_first, IB_byte_data, 0, false, nullptr);
  if (anim == nullptr) {
    CLOG_WARN(&LOG, "couldn't open anim '%s'", filepath_first);
    return;
  }

  ImBuf *ibuf = MOV_decode_frame(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
  if (ibuf) {
    playanim_toscreen_on_load(ghost_data, display_ctx, nullptr, ibuf);
    IMB_freeImBuf(ibuf);
  }

  for (int pic = 0; pic < MOV_get_duration_frames(anim, IMB_TC_NONE); pic++) {
    PlayAnimPict *picture = MEM_callocN<PlayAnimPict>("Pict");
    picture->anim = anim;
    picture->frame = pic + frame_offset;
    picture->IB_flags = IB_byte_data;
    picture->filepath = BLI_sprintfN("%s : %4.d", filepath_first, pic + 1);
    BLI_addtail(&picsbase, picture);
  }

  const PlayAnimPict *picture = static_cast<const PlayAnimPict *>(picsbase.last);
  if (!(picture && picture->anim == anim)) {
    MOV_close(anim);
    CLOG_WARN(&LOG, "no frames added for: '%s'", filepath_first);
  }
}

static void build_pict_list_from_image_sequence(ListBase &picsbase,
                                                GhostData &ghost_data,
                                                const PlayDisplayContext &display_ctx,
                                                const char *filepath_first,
                                                const int frame_offset,
                                                const int totframes,
                                                const int frame_step,
                                                const bool *loading_p)
{
  /* Load images into cache until the cache is full,
   * this resolves choppiness for images that are slow to load, see: #81751. */
  bool fill_cache = (
#ifdef USE_FRAME_CACHE_LIMIT
      true
#else
      false
#endif
  );

  int fp_framenr;
  struct {
    char head[FILE_MAX], tail[FILE_MAX];
    ushort digits;
  } fp_decoded;

  char filepath[FILE_MAX];
  STRNCPY(filepath, filepath_first);
  fp_framenr = BLI_path_sequence_decode(filepath,
                                        fp_decoded.head,
                                        sizeof(fp_decoded.head),
                                        fp_decoded.tail,
                                        sizeof(fp_decoded.tail),
                                        &fp_decoded.digits);

  pupdate_time();
  g_playanim.total_time = 1.0;

  for (int pic = 0; pic < totframes; pic++) {
    if (!IMB_test_image(filepath)) {
      break;
    }

    bool has_error = false;
    char *error_message = nullptr;
    void *mem = nullptr;
    size_t size = -1;
    if (!buffer_from_filepath(
            filepath, g_playanim.from_disk ? nullptr : &mem, &size, &error_message))
    {
      has_error = true;
      size = 0;
    }

    PlayAnimPict *picture = MEM_callocN<PlayAnimPict>("picture");
    picture->size = size;
    picture->IB_flags = IB_byte_data;
    picture->mem = static_cast<uchar *>(mem);
    picture->filepath = BLI_strdup(filepath);
    picture->error_message = error_message;
    picture->frame = pic + frame_offset;
    BLI_addtail(&picsbase, picture);

    pupdate_time();

    const bool display_imbuf = g_playanim.total_time > 1.0;

    if (has_error) {
      CLOG_WARN(&LOG,
                "Picture %s failed: %s",
                filepath,
                error_message ? error_message : "<unknown error>");
    }
    else if (display_imbuf || fill_cache) {
      /* OCIO_TODO: support different input color space. */
      ImBuf *ibuf = ibuf_from_picture(picture);

      if (ibuf) {
        if (display_imbuf) {
          playanim_toscreen_on_load(ghost_data, display_ctx, picture, ibuf);
        }
#ifdef USE_FRAME_CACHE_LIMIT
        if (fill_cache) {
          picture->ibuf = ibuf;
          frame_cache_add(picture);
          fill_cache = !frame_cache_limit_exceeded();
        }
        else
#endif
        {
          IMB_freeImBuf(ibuf);
        }
      }

      if (display_imbuf) {
        pupdate_time();
        g_playanim.total_time = 0.0;
      }
    }

    /* Create a new file-path each time. */
    fp_framenr += frame_step;
    BLI_path_sequence_encode(filepath,
                             sizeof(filepath),
                             fp_decoded.head,
                             fp_decoded.tail,
                             fp_decoded.digits,
                             fp_framenr);

    while (GHOST_ProcessEvents(ghost_data.system, false)) {
      GHOST_DispatchEvents(ghost_data.system);
      if (*loading_p == false) {
        break;
      }
    }
  }
}

static void build_pict_list(ListBase &picsbase,
                            GhostData &ghost_data,
                            const PlayDisplayContext &display_ctx,
                            const char *filepath_first,
                            const int totframes,
                            const int frame_step,
                            bool *loading_p)
{
  *loading_p = true;

  /* NOTE(@ideasman42): When loading many files (e.g. expanded from shell globing)
   * it's important the frame number increases each time. Otherwise playing `*.png`
   * in a directory will expand into many arguments, each calling this function adding
   * a frame that's set to zero. */
  const PlayAnimPict *picture_last = static_cast<PlayAnimPict *>(picsbase.last);
  const int frame_offset = picture_last ? (picture_last->frame + 1) : 0;

  bool do_image_load = false;
  if (MOV_is_movie_file(filepath_first)) {
    build_pict_list_from_anim(picsbase, ghost_data, display_ctx, filepath_first, frame_offset);

    if (picsbase.last == picture_last) {
      /* FFMPEG detected JPEG2000 as a video which would load with zero duration.
       * Resolve this by using images as a fallback when a video file has no frames to display. */
      do_image_load = true;
    }
  }
  else {
    do_image_load = true;
  }

  if (do_image_load) {
    build_pict_list_from_image_sequence(picsbase,
                                        ghost_data,
                                        display_ctx,
                                        filepath_first,
                                        frame_offset,
                                        totframes,
                                        frame_step,
                                        loading_p);
  }

  *loading_p = false;
}

static void update_sound_fps()
{
#ifdef WITH_AUDASPACE
  if (g_audaspace.playback_handle) {
    /* Swap-time stores the 1.0/fps ratio. */
    double speed = 1.0 / (g_playanim.swap_time * g_playanim.fps_movie);

    AUD_Handle_setPitch(g_audaspace.playback_handle, speed);
  }
#endif
}

static void playanim_change_frame_tag(PlayState &ps, int cx)
{
  ps.need_frame_update = true;
  ps.frame_cursor_x = cx;
}

static void playanim_change_frame(PlayState &ps)
{
  if (!ps.need_frame_update) {
    return;
  }
  if (BLI_listbase_is_empty(&ps.picsbase)) {
    return;
  }

  const blender::int2 window_size = playanim_window_size_get(ps.ghost_data.window);
  const int i_last = static_cast<PlayAnimPict *>(ps.picsbase.last)->frame;
  /* Without this the frame-indicator location isn't closest to the cursor. */
  const int correct_rounding = (window_size[0] / (i_last + 1)) / 2;
  const int i = clamp_i(
      (i_last * (ps.frame_cursor_x + correct_rounding)) / window_size[0], 0, i_last);

#ifdef WITH_AUDASPACE
  if (g_audaspace.scrub_handle) {
    AUD_Handle_stop(g_audaspace.scrub_handle);
    g_audaspace.scrub_handle = nullptr;
  }

  if (g_audaspace.playback_handle) {
    AUD_Status status = AUD_Handle_getStatus(g_audaspace.playback_handle);
    if (status != AUD_STATUS_PLAYING) {
      AUD_Handle_stop(g_audaspace.playback_handle);
      g_audaspace.playback_handle = AUD_Device_play(
          g_audaspace.audio_device, g_audaspace.source, 1);
      if (g_audaspace.playback_handle) {
        AUD_Handle_setPosition(g_audaspace.playback_handle, i / g_playanim.fps_movie);
        g_audaspace.scrub_handle = AUD_pauseAfter(g_audaspace.playback_handle,
                                                  1.0 / g_playanim.fps_movie);
      }
      update_sound_fps();
    }
    else {
      AUD_Handle_setPosition(g_audaspace.playback_handle, i / g_playanim.fps_movie);
      g_audaspace.scrub_handle = AUD_pauseAfter(g_audaspace.playback_handle,
                                                1.0 / g_playanim.fps_movie);
    }
  }
  else if (g_audaspace.source) {
    g_audaspace.playback_handle = AUD_Device_play(g_audaspace.audio_device, g_audaspace.source, 1);
    if (g_audaspace.playback_handle) {
      AUD_Handle_setPosition(g_audaspace.playback_handle, i / g_playanim.fps_movie);
      g_audaspace.scrub_handle = AUD_pauseAfter(g_audaspace.playback_handle,
                                                1.0 / g_playanim.fps_movie);
    }
    update_sound_fps();
  }
#endif

  ps.picture = static_cast<PlayAnimPict *>(BLI_findlink(&ps.picsbase, i));
  BLI_assert(ps.picture != nullptr);

  ps.single_step = true;
  ps.wait = false;
  ps.next_frame = 0;

  ps.need_frame_update = false;
}

static void playanim_audio_resume(PlayState &ps)
{
#ifdef WITH_AUDASPACE
  /* TODO: store in ps direct? */
  const int i = BLI_findindex(&ps.picsbase, ps.picture);
  if (g_audaspace.playback_handle) {
    AUD_Handle_stop(g_audaspace.playback_handle);
  }
  g_audaspace.playback_handle = AUD_Device_play(g_audaspace.audio_device, g_audaspace.source, 1);
  if (g_audaspace.playback_handle) {
    AUD_Handle_setPosition(g_audaspace.playback_handle, i / g_playanim.fps_movie);
  }
  update_sound_fps();
#else
  UNUSED_VARS(ps);
#endif
}

static void playanim_audio_stop(PlayState & /*ps*/)
{
#ifdef WITH_AUDASPACE
  if (g_audaspace.playback_handle) {
    AUD_Handle_stop(g_audaspace.playback_handle);
    g_audaspace.playback_handle = nullptr;
  }
#endif
}

static bool ghost_event_proc(GHOST_EventHandle ghost_event, GHOST_TUserDataPtr ps_void_ptr)
{
  PlayState &ps = *static_cast<PlayState *>(ps_void_ptr);
  const GHOST_TEventType type = GHOST_GetEventType(ghost_event);
  GHOST_TEventDataPtr data = GHOST_GetEventData(ghost_event);
  /* Convert ghost event into value keyboard or mouse. */
  const int val = ELEM(type, GHOST_kEventKeyDown, GHOST_kEventButtonDown);
  GHOST_SystemHandle ghost_system = ps.ghost_data.system;
  GHOST_WindowHandle ghost_window = ps.ghost_data.window;

  // print_ps(ps);

  playanim_event_qual_update(ps.ghost_data);

  /* First check if we're busy loading files. */
  if (ps.loading) {
    switch (type) {
      case GHOST_kEventKeyDown:
      case GHOST_kEventKeyUp: {
        const GHOST_TEventKeyData *key_data = static_cast<const GHOST_TEventKeyData *>(data);
        switch (key_data->key) {
          case GHOST_kKeyEsc:
            ps.loading = false;
            break;
          default:
            break;
        }
        break;
      }
      default:
        break;
    }
    return true;
  }

  if (ps.wait && ps.stopped == false) {
    ps.stopped = true;
  }

  if (ps.wait) {
    pupdate_time();
    g_playanim.total_time = 0.0;
  }

  switch (type) {
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      const GHOST_TEventKeyData *key_data = static_cast<const GHOST_TEventKeyData *>(data);
      switch (key_data->key) {
        case GHOST_kKeyA:
          if (val) {
            ps.no_frame_skip = !ps.no_frame_skip;
          }
          break;
        case GHOST_kKeyI:
          if (val) {
            ps.show_frame_indicator = !ps.show_frame_indicator;
          }
          break;
        case GHOST_kKeyP:
          if (val) {
            ps.pingpong = !ps.pingpong;
          }
          break;
        case GHOST_kKeyF: {
          if (val) {
            int axis = (ps.ghost_data.qual & WS_QUAL_SHIFT) ? 1 : 0;
            ps.draw_flip[axis] = !ps.draw_flip[axis];
          }
          break;
        }
        case GHOST_kKey1:
        case GHOST_kKeyNumpad1:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 60.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey2:
        case GHOST_kKeyNumpad2:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 50.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey3:
        case GHOST_kKeyNumpad3:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 30.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey4:
        case GHOST_kKeyNumpad4:
          if (ps.ghost_data.qual & WS_QUAL_SHIFT) {
            g_playanim.swap_time = ps.frame_step / 24.0;
            update_sound_fps();
          }
          else {
            g_playanim.swap_time = ps.frame_step / 25.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey5:
        case GHOST_kKeyNumpad5:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 20.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey6:
        case GHOST_kKeyNumpad6:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 15.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey7:
        case GHOST_kKeyNumpad7:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 12.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey8:
        case GHOST_kKeyNumpad8:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 10.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey9:
        case GHOST_kKeyNumpad9:
          if (val) {
            g_playanim.swap_time = ps.frame_step / 6.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKeyLeftArrow:
          if (val) {
            ps.single_step = true;
            ps.wait = false;
            playanim_audio_stop(ps);

            if (ps.ghost_data.qual & WS_QUAL_SHIFT) {
              ps.picture = static_cast<PlayAnimPict *>(ps.picsbase.first);
              ps.next_frame = 0;
            }
            else {
              ps.next_frame = -1;
            }
          }
          break;
        case GHOST_kKeyDownArrow:
          if (val) {
            ps.wait = false;
            playanim_audio_stop(ps);

            if (ps.ghost_data.qual & WS_QUAL_SHIFT) {
              ps.next_frame = ps.direction = -1;
            }
            else {
              ps.next_frame = -10;
              ps.single_step = true;
            }
          }
          break;
        case GHOST_kKeyRightArrow:
          if (val) {
            ps.single_step = true;
            ps.wait = false;
            playanim_audio_stop(ps);

            if (ps.ghost_data.qual & WS_QUAL_SHIFT) {
              ps.picture = static_cast<PlayAnimPict *>(ps.picsbase.last);
              ps.next_frame = 0;
            }
            else {
              ps.next_frame = 1;
            }
          }
          break;
        case GHOST_kKeyUpArrow:
          if (val) {
            ps.wait = false;
            if (ps.ghost_data.qual & WS_QUAL_SHIFT) {
              ps.next_frame = ps.direction = 1;
              if (ps.single_step == false) {
                playanim_audio_resume(ps);
              }
            }
            else {
              ps.next_frame = 10;
              ps.single_step = true;
              playanim_audio_stop(ps);
            }
          }
          break;

        case GHOST_kKeySlash:
        case GHOST_kKeyNumpadSlash:
          if (val) {
            if (ps.ghost_data.qual & WS_QUAL_SHIFT) {
              if (ps.picture && ps.picture->ibuf) {
                printf(" Name: %s | Speed: %.2f frames/s\n",
                       ps.picture->ibuf->filepath,
                       ps.frame_step / g_playanim.swap_time);
              }
            }
            else {
              g_playanim.swap_time = ps.frame_step / 5.0;
              update_sound_fps();
            }
          }
          break;
        case GHOST_kKey0:
        case GHOST_kKeyNumpad0:
          if (val) {
            if (ps.once) {
              ps.once = ps.wait = false;
            }
            else {
              ps.picture = nullptr;
              ps.once = true;
              ps.wait = false;
            }
          }
          break;

        case GHOST_kKeySpace:
          if (val) {
            if (ps.wait || ps.single_step) {
              ps.wait = ps.single_step = false;
              playanim_audio_resume(ps);
            }
            else {
              ps.single_step = true;
              ps.wait = true;
              playanim_audio_stop(ps);
            }
          }
          break;
        case GHOST_kKeyEnter:
        case GHOST_kKeyNumpadEnter:
          if (val) {
            ps.wait = ps.single_step = false;
            playanim_audio_resume(ps);
          }
          break;
        case GHOST_kKeyPeriod:
        case GHOST_kKeyNumpadPeriod:
          if (val) {
            if (ps.single_step) {
              ps.wait = false;
            }
            else {
              ps.single_step = true;
              ps.wait = !ps.wait;
              playanim_audio_stop(ps);
            }
          }
          break;
        case GHOST_kKeyEqual:
        case GHOST_kKeyPlus:
        case GHOST_kKeyNumpadPlus: {
          if (val == 0) {
            break;
          }
          if (ps.ghost_data.qual & WS_QUAL_CTRL) {
            playanim_window_zoom(ps, 0.1f);
          }
          else {
            if (g_playanim.swap_time > ps.frame_step / 60.0) {
              g_playanim.swap_time /= 1.1;
              update_sound_fps();
            }
          }
          break;
        }
        case GHOST_kKeyMinus:
        case GHOST_kKeyNumpadMinus: {
          if (val == 0) {
            break;
          }
          if (ps.ghost_data.qual & WS_QUAL_CTRL) {
            playanim_window_zoom(ps, -0.1f);
          }
          else {
            if (g_playanim.swap_time < ps.frame_step / 5.0) {
              g_playanim.swap_time *= 1.1;
              update_sound_fps();
            }
          }
          break;
        }
        case GHOST_kKeyEsc:
          ps.go = false;
          break;
        default:
          break;
      }
      break;
    }
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      const GHOST_TEventButtonData *bd = static_cast<const GHOST_TEventButtonData *>(data);
      int cx, cy;
      const blender::int2 window_size = playanim_window_size_get(ghost_window);

      const bool inside_window = (GHOST_GetCursorPosition(ghost_system, ghost_window, &cx, &cy) ==
                                  GHOST_kSuccess) &&
                                 (cx >= 0 && cx < window_size[0] && cy >= 0 &&
                                  cy <= window_size[1]);

      if (bd->button == GHOST_kButtonMaskLeft) {
        if (type == GHOST_kEventButtonDown) {
          if (inside_window) {
            ps.ghost_data.qual |= WS_QUAL_LMOUSE;
            playanim_change_frame_tag(ps, cx);
          }
        }
        else {
          ps.ghost_data.qual &= ~WS_QUAL_LMOUSE;
        }
      }
      else if (bd->button == GHOST_kButtonMaskMiddle) {
        if (type == GHOST_kEventButtonDown) {
          if (inside_window) {
            ps.ghost_data.qual |= WS_QUAL_MMOUSE;
          }
        }
        else {
          ps.ghost_data.qual &= ~WS_QUAL_MMOUSE;
        }
      }
      else if (bd->button == GHOST_kButtonMaskRight) {
        if (type == GHOST_kEventButtonDown) {
          if (inside_window) {
            ps.ghost_data.qual |= WS_QUAL_RMOUSE;
          }
        }
        else {
          ps.ghost_data.qual &= ~WS_QUAL_RMOUSE;
        }
      }
      break;
    }
    case GHOST_kEventCursorMove: {
      if (ps.ghost_data.qual & WS_QUAL_LMOUSE) {
        const GHOST_TEventCursorData *cd = static_cast<const GHOST_TEventCursorData *>(data);
        int cx, cy;

        /* Ignore 'in-between' events, since they can make scrubbing lag.
         *
         * Ideally we would keep into the event queue and see if this is the last motion event.
         * however the API currently doesn't support this. */
        {
          int x_test, y_test;
          if (GHOST_GetCursorPosition(ghost_system, ghost_window, &cx, &cy) == GHOST_kSuccess) {
            GHOST_ScreenToClient(ghost_window, cd->x, cd->y, &x_test, &y_test);
            if (cx != x_test || cy != y_test) {
              /* We're not the last event... skipping. */
              break;
            }
          }
        }

        playanim_change_frame_tag(ps, cx);
      }
      break;
    }
    case GHOST_kEventWindowActivate:
    case GHOST_kEventWindowDeactivate: {
      ps.ghost_data.qual &= ~WS_QUAL_MOUSE;
      break;
    }
    case GHOST_kEventWindowSize:
    case GHOST_kEventWindowMove: {
      float zoomx, zoomy;

      ps.display_ctx.size = playanim_window_size_get(ghost_window);
      GHOST_ActivateWindowDrawingContext(ghost_window);

      zoomx = float(ps.display_ctx.size[0]) / ps.ibuf_size[0];
      zoomy = float(ps.display_ctx.size[1]) / ps.ibuf_size[1];

      /* Zoom always show entire image. */
      ps.zoom = std::min(zoomx, zoomy);

      GPU_viewport(0, 0, ps.display_ctx.size[0], ps.display_ctx.size[1]);
      GPU_scissor(0, 0, ps.display_ctx.size[0], ps.display_ctx.size[1]);

      playanim_gpu_matrix();

      g_playanim.total_time = 0.0;

      playanim_toscreen(ps, ps.picture, ps.picture ? ps.picture->ibuf : nullptr);

      break;
    }
    case GHOST_kEventQuitRequest:
    case GHOST_kEventWindowClose: {
      ps.go = false;
      break;
    }
    case GHOST_kEventWindowDPIHintChanged: {
      /* Rely on frame-change to redraw. */
      playanim_window_font_scale_from_dpi(ps);
      break;
    }
    case GHOST_kEventDraggingDropDone: {
      const GHOST_TEventDragnDropData *ddd = static_cast<const GHOST_TEventDragnDropData *>(data);

      if (ddd->dataType == GHOST_kDragnDropTypeFilenames) {
        const GHOST_TStringArray *stra = static_cast<const GHOST_TStringArray *>(ddd->data);
        ps.argc_next = stra->count;
        ps.argv_next = MEM_malloc_arrayN<char *>(size_t(ps.argc_next), __func__);
        for (int i = 0; i < stra->count; i++) {
          ps.argv_next[i] = BLI_strdup(reinterpret_cast<const char *>(stra->strings[i]));
        }
        ps.go = false;
        printf("dropped %s, %d file(s)\n", ps.argv_next[0], ps.argc_next);
      }
      break;
    }
    default:
      /* Quiet warnings. */
      break;
  }

  return true;
}

static GHOST_WindowHandle playanim_window_open(
    GHOST_SystemHandle ghost_system, const char *title, int posx, int posy, int sizex, int sizey)
{
  GHOST_GPUSettings gpu_settings = {0};
  const GPUBackendType gpu_backend = GPU_backend_type_selection_get();
  gpu_settings.context_type = wm_ghost_drawing_context_type(gpu_backend);
  gpu_settings.preferred_device.index = U.gpu_preferred_index;
  gpu_settings.preferred_device.vendor_id = U.gpu_preferred_vendor_id;
  gpu_settings.preferred_device.device_id = U.gpu_preferred_device_id;
  if (GPU_backend_vsync_is_overridden()) {
    gpu_settings.flags |= GHOST_gpuVSyncIsOverridden;
    gpu_settings.vsync = GHOST_TVSyncModes(GPU_backend_vsync_get());
  }

  {
    bool screen_size_valid = false;
    uint32_t screen_size[2];
    if ((GHOST_GetMainDisplayDimensions(ghost_system, &screen_size[0], &screen_size[1]) ==
         GHOST_kSuccess) &&
        (screen_size[0] > 0) && (screen_size[1] > 0))
    {
      screen_size_valid = true;
    }
    else {
      /* Unlikely the screen size fails to access,
       * if this happens it's still important to clamp the window size by *something*. */
      screen_size[0] = 1024;
      screen_size[1] = 1024;
    }

    if (screen_size_valid) {
      if (GHOST_GetCapabilities() & GHOST_kCapabilityWindowPosition) {
        posy = (screen_size[1] - posy - sizey);
      }
    }
    else {
      posx = 0;
      posy = 0;
    }

    /* NOTE: ideally the GPU could be queried for the maximum supported window size,
     * this isn't so simple as the GPU back-end's capabilities are initialized *after* the window
     * has been created. Further, it's quite unlikely the users main monitor size is larger
     * than the maximum window size supported by the GPU. */

    /* Clamp the size so very large requests aren't rejected by the GPU.
     * Halve until a usable range is reached instead of scaling down to meet the screen size
     * since fractional scaling tends not to look so nice. */
    while (sizex >= int(screen_size[0]) || sizey >= int(screen_size[1])) {
      sizex /= 2;
      sizey /= 2;
    }
    /* Unlikely but ensure the size is *never* zero. */
    CLAMP_MIN(sizex, 1);
    CLAMP_MIN(sizey, 1);
  }

  return GHOST_CreateWindow(ghost_system,
                            nullptr,
                            title,
                            posx,
                            posy,
                            sizex,
                            sizey,
                            /* Could optionally start full-screen. */
                            GHOST_kWindowStateNormal,
                            false,
                            gpu_settings);
}

static void playanim_window_zoom(PlayState &ps, const float zoom_offset)
{
  blender::int2 size;
  // blender::int2 ofs; /* UNUSED. */

  if (ps.zoom + zoom_offset > 0.0f) {
    ps.zoom += zoom_offset;
  }

  // playanim_window_get_position(&ofs[0], &ofs[1]);
  // size = playanim_window_size_get(ps.ghost_data.window);
  // ofs[0] += size[0] / 2; /* UNUSED. */
  // ofs[1] += size[1] / 2; /* UNUSED. */
  size[0] = ps.zoom * ps.ibuf_size[0];
  size[1] = ps.zoom * ps.ibuf_size[1];
  // ofs[0] -= size[0] / 2; /* UNUSED. */
  // ofs[1] -= size[1] / 2; /* UNUSED. */
  // window_set_position(ps.ghost_data.window, size[0], size[1]);
  GHOST_SetClientSize(ps.ghost_data.window, size[0], size[1]);
}

static bool playanim_window_font_scale_from_dpi(PlayState &ps)
{
  const float scale = (GHOST_GetDPIHint(ps.ghost_data.window) *
                       GHOST_GetNativePixelSize(ps.ghost_data.window) / 96.0f);
  const float font_size_base = 11.0f; /* Font size un-scaled. */
  const int font_size = int((font_size_base * scale) + 0.5f);
  bool changed = false;
  if (ps.font_size != font_size) {
    BLF_size(ps.font_id, font_size);
    ps.font_size = font_size;
    changed = true;
  }
  if (ps.display_ctx.ui_scale != scale) {
    ps.display_ctx.ui_scale = scale;
  }
  return changed;
}

/**
 * \return True when `args_next` is filled with arguments used to re-run this function
 * (used for drag & drop).
 */
static std::optional<int> wm_main_playanim_intern(int argc, const char **argv, PlayArgs *args_next)
{
  ImBuf *ibuf = nullptr;
  blender::int2 window_pos = {0, 0};
  int frame_start = -1;
  int frame_end = -1;

  PlayState ps{};

  ps.go = true;
  ps.direction = true;
  ps.next_frame = 1;
  ps.once = false;
  ps.pingpong = false;
  ps.no_frame_skip = false;
  ps.single_step = false;
  ps.wait = false;
  ps.stopped = false;
  ps.loading = false;
  ps.picture = nullptr;
  ps.show_frame_indicator = false;
  ps.argc_next = 0;
  ps.argv_next = nullptr;
  ps.zoom = 1.0f;
  ps.draw_flip[0] = false;
  ps.draw_flip[1] = false;

  ps.frame_step = 1;

  ps.font_id = -1;

  IMB_init();
  MOV_init();

  STRNCPY_UTF8(ps.display_ctx.display_settings.display_device,
               IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE));
  IMB_colormanagement_init_untonemapped_view_settings(&ps.display_ctx.view_settings,
                                                      &ps.display_ctx.display_settings);
  ps.display_ctx.ui_scale = 1.0f;

  while ((argc > 0) && (argv[0][0] == '-')) {
    switch (argv[0][1]) {
      case 'm': {
        g_playanim.from_disk = true;
        break;
      }
      case 'p': {
        if (argc > 2) {
          window_pos[0] = atoi(argv[1]);
          window_pos[1] = atoi(argv[2]);
          argc -= 2;
          argv += 2;
        }
        else {
          printf("too few arguments for -p (need 2): skipping\n");
        }
        break;
      }
      case 'f': {
        if (argc > 2) {
          double fps = atof(argv[1]);
          double fps_base = atof(argv[2]);
          if (fps == 0.0) {
            fps = 1;
            printf("invalid fps, forcing 1\n");
          }
          g_playanim.swap_time = fps_base / fps;
          argc -= 2;
          argv += 2;
        }
        else {
          printf("too few arguments for -f (need 2): skipping\n");
        }
        break;
      }
      case 's': {
        frame_start = atoi(argv[1]);
        CLAMP(frame_start, 1, MAXFRAME);
        argc--;
        argv++;
        break;
      }
      case 'e': {
        frame_end = atoi(argv[1]);
        CLAMP(frame_end, 1, MAXFRAME);
        argc--;
        argv++;
        break;
      }
      case 'j': {
        ps.frame_step = atoi(argv[1]);
        CLAMP(ps.frame_step, 1, MAXFRAME);
        g_playanim.swap_time *= ps.frame_step;
        argc--;
        argv++;
        break;
      }
      case 'c': {
#ifdef USE_FRAME_CACHE_LIMIT
        const int memory_in_mb = max_ii(0, atoi(argv[1]));
        g_frame_cache.memory_limit = size_t(memory_in_mb) * (1024 * 1024);
#endif
        argc--;
        argv++;
        break;
      }
      default: {
        printf("unknown option '%c': skipping\n", argv[0][1]);
        break;
      }
    }
    argc--;
    argv++;
  }

  const char *filepath = nullptr;
  GHOST_EventConsumerHandle ghost_event_consumer = nullptr;

  {
    std::optional<int> exit_code = [&]() -> std::optional<int> {
      if (argc == 0) {
        fprintf(stderr, "%s: no filepath argument given\n", message_prefix);
        return EXIT_FAILURE;
      }

      filepath = argv[0];
      if (MOV_is_movie_file(filepath)) {
        /* OCIO_TODO: support different input color spaces. */
        /* Image buffer is used for display, which does support displaying any buffer from any
         * colorspace. Skip colorspace conversions in the movie module to improve performance. */
        MovieReader *anim = MOV_open_file(filepath, IB_byte_data, 0, true, nullptr);
        if (anim) {
          ibuf = MOV_decode_frame(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
          MOV_close(anim);
          anim = nullptr;
        }
      }
      else if (IMB_test_image(filepath)) {
        /* Pass. */
      }
      else {
        fprintf(stderr, "%s: '%s' not an image file\n", message_prefix, filepath);
        return EXIT_FAILURE;
      }

      if (ibuf == nullptr) {
        /* OCIO_TODO: support different input color space. */
        ibuf = IMB_load_image_from_filepath(filepath, IB_byte_data);
      }

      if (ibuf == nullptr) {
        fprintf(stderr, "%s: '%s' couldn't open\n", message_prefix, filepath);
        return EXIT_FAILURE;
      }

      /* Select GPU backend. */
      GPU_backend_type_selection_detect();

      /* Init GHOST and open window. */
      GHOST_SetBacktraceHandler((GHOST_TBacktraceFn)BLI_system_backtrace);
      GHOST_UseWindowFrame(WM_init_window_frame_get());

      ps.ghost_data.system = GHOST_CreateSystem();
      if (UNLIKELY(ps.ghost_data.system == nullptr)) {
        /* GHOST will have reported the back-ends that failed to load. */
        fprintf(stderr, "%s: unable to initialize GHOST, exiting!\n", message_prefix);
        return EXIT_FAILURE;
      }

      GPU_backend_ghost_system_set(ps.ghost_data.system);

      GHOST_UseNativePixels();

      ps.ghost_data.window = playanim_window_open(ps.ghost_data.system,
                                                  "Blender Animation Player",
                                                  window_pos[0],
                                                  window_pos[1],
                                                  ibuf->x,
                                                  ibuf->y);

      if (UNLIKELY(ps.ghost_data.window == nullptr)) {
        fprintf(stderr, "%s: unable to create window, exiting!\n", message_prefix);
        return EXIT_FAILURE;
      }

      ghost_event_consumer = GHOST_CreateEventConsumer(ghost_event_proc, &ps);
      GHOST_AddEventConsumer(ps.ghost_data.system, ghost_event_consumer);

      return std::nullopt;
    }();

    if (exit_code) {
      if (ps.ghost_data.system) {
        GHOST_DisposeSystem(ps.ghost_data.system);
      }
      if (ibuf) {
        IMB_freeImBuf(ibuf);
      }
      IMB_exit();
      MOV_exit();
      return exit_code;
    }
  }

  // GHOST_ActivateWindowDrawingContext(ps.ghost_data.window);

  /* Init Blender GPU context. */
  ps.ghost_data.gpu_context = GPU_context_create(ps.ghost_data.window, nullptr);
  GPU_init();

  /* Initialize the font. */
  BLF_init();
  ps.font_id = BLF_load_mono_default(false);

  ps.font_size = -1; /* Force update. */
  playanim_window_font_scale_from_dpi(ps);

  ps.ibuf_size[0] = ibuf->x;
  ps.ibuf_size[1] = ibuf->y;

  ps.display_ctx.size = ps.ibuf_size;

  GHOST_SwapWindowBufferAcquire(ps.ghost_data.window);
  GPU_render_begin();
  GPU_render_step();
  GPU_clear_color(0.1f, 0.1f, 0.1f, 0.0f);

  {
    const blender::int2 window_size = playanim_window_size_get(ps.ghost_data.window);
    GPU_viewport(0, 0, window_size[0], window_size[1]);
    GPU_scissor(0, 0, window_size[0], window_size[1]);
    playanim_gpu_matrix();
  }

  GHOST_SwapWindowBufferRelease(ps.ghost_data.window);
  GPU_render_end();

  /* One of the frames was invalid or not passed in. */
  if (frame_start == -1 || frame_end == -1) {
    frame_start = 1;
    if (argc == 1) {
      /* A single file was passed in, attempt to load all images from an image sequence.
       * (if it is an image sequence). */
      frame_end = MAXFRAME;
    }
    else {
      /* Multiple files passed in, show each file without expanding image sequences.
       * This occurs when dropping multiple files. */
      frame_end = 1;
    }
  }

  build_pict_list(ps.picsbase,
                  ps.ghost_data,
                  ps.display_ctx,
                  filepath,
                  (frame_end - frame_start) + 1,
                  ps.frame_step,
                  &ps.loading);

#ifdef WITH_AUDASPACE
  g_audaspace.source = AUD_Sound_file(filepath);
  if (!BLI_listbase_is_empty(&ps.picsbase)) {
    const MovieReader *anim_movie = static_cast<PlayAnimPict *>(ps.picsbase.first)->anim;
    if (anim_movie) {
      g_playanim.fps_movie = MOV_get_fps(anim_movie);
      /* Enforce same fps for movie as sound. */
      g_playanim.swap_time = ps.frame_step / g_playanim.fps_movie;
    }
  }
#endif

  for (int i = 1; i < argc; i++) {
    filepath = argv[i];
    build_pict_list(ps.picsbase,
                    ps.ghost_data,
                    ps.display_ctx,
                    filepath,
                    (frame_end - frame_start) + 1,
                    ps.frame_step,
                    &ps.loading);
  }

  IMB_freeImBuf(ibuf);
  ibuf = nullptr;

  pupdate_time();
  g_playanim.total_time = 0.0;

/* Newly added in 2.6x, without this images never get freed. */
#define USE_IMB_CACHE

  while (ps.go) {
    if (ps.pingpong) {
      ps.direction = -ps.direction;
    }

    if (ps.direction == 1) {
      ps.picture = static_cast<PlayAnimPict *>(ps.picsbase.first);
    }
    else {
      ps.picture = static_cast<PlayAnimPict *>(ps.picsbase.last);
    }

    if (ps.picture == nullptr) {
      printf("couldn't find pictures\n");
      ps.go = false;
    }
    if (ps.pingpong) {
      if (ps.direction == 1) {
        ps.picture = ps.picture->next;
      }
      else {
        ps.picture = ps.picture->prev;
      }
    }
    g_playanim.total_time = std::min(g_playanim.total_time, 0.0);

#ifdef WITH_AUDASPACE
    if (g_audaspace.playback_handle) {
      AUD_Handle_stop(g_audaspace.playback_handle);
    }
    g_audaspace.playback_handle = AUD_Device_play(g_audaspace.audio_device, g_audaspace.source, 1);
    update_sound_fps();
#endif

    while (ps.picture) {
      bool has_event;
#ifndef USE_IMB_CACHE
      if (ibuf != nullptr && ibuf->ftype == IMB_FTYPE_NONE) {
        IMB_freeImBuf(ibuf);
      }
#endif

      ibuf = ibuf_from_picture(ps.picture);

      {
#ifdef USE_IMB_CACHE
        ps.picture->ibuf = ibuf;
#endif
        if (ibuf) {
#ifdef USE_FRAME_CACHE_LIMIT
          if (ps.picture->frame_cache_node == nullptr) {
            frame_cache_add(ps.picture);
          }
          else {
            frame_cache_touch(ps.picture);
          }
          frame_cache_limit_apply(ibuf);
#endif /* USE_FRAME_CACHE_LIMIT */

          STRNCPY(ibuf->filepath, ps.picture->filepath);
          ibuf->fileframe = ps.picture->frame;
        }

        while (pupdate_time()) {
          BLI_time_sleep_ms(1);
        }
        g_playanim.total_time -= g_playanim.swap_time;
        playanim_toscreen(ps, ps.picture, ibuf);
      }

      if (ps.once) {
        if (ps.picture->next == nullptr) {
          ps.wait = true;
        }
        else if (ps.picture->prev == nullptr) {
          ps.wait = true;
        }
      }

      ps.next_frame = ps.direction;

      GPU_render_begin();
      GPUContext *restore_context = GPU_context_active_get();
      GPU_context_active_set(ps.ghost_data.gpu_context);
      while ((has_event = GHOST_ProcessEvents(ps.ghost_data.system, false))) {
        GHOST_DispatchEvents(ps.ghost_data.system);
      }
      GPU_render_end();
      GPU_context_active_set(restore_context);

      if (ps.go == false) {
        break;
      }
      playanim_change_frame(ps);
      if (!has_event) {
        BLI_time_sleep_ms(1);
      }
      if (ps.wait) {
        continue;
      }

      ps.wait = ps.single_step;

      if (ps.wait == false && ps.stopped) {
        ps.stopped = false;
      }

      pupdate_time();

      if (ps.picture && ps.next_frame) {
        /* Advance to the next frame, always at least set one step.
         * Implement frame-skipping when enabled and playback is not fast enough. */
        while (ps.picture) {
          ps.picture = playanim_step(ps.picture, ps.next_frame);

          if (ps.once && ps.picture != nullptr) {
            if (ps.picture->next == nullptr) {
              ps.wait = true;
            }
            else if (ps.picture->prev == nullptr) {
              ps.wait = true;
            }
          }

          if (ps.wait || g_playanim.total_time < g_playanim.swap_time || ps.no_frame_skip) {
            break;
          }
          g_playanim.total_time -= g_playanim.swap_time;
        }
        if (ps.picture == nullptr && ps.single_step) {
          ps.picture = playanim_step(ps.picture, ps.next_frame);
        }
      }
      if (ps.go == false) {
        break;
      }
    }
  }
  while ((ps.picture = static_cast<PlayAnimPict *>(BLI_pophead(&ps.picsbase)))) {
    if (ps.picture->anim) {
      if ((ps.picture->next == nullptr) || (ps.picture->next->anim != ps.picture->anim)) {
        MOV_close(ps.picture->anim);
      }
    }

    if (ps.picture->ibuf) {
      IMB_freeImBuf(ps.picture->ibuf);
    }
    if (ps.picture->mem) {
      MEM_freeN(ps.picture->mem);
    }
    if (ps.picture->error_message) {
      MEM_freeN(ps.picture->error_message);
    }
    MEM_freeN(ps.picture->filepath);
    MEM_freeN(ps.picture);
  }

/* Cleanup. */
#ifndef USE_IMB_CACHE
  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }
#endif

#ifdef USE_FRAME_CACHE_LIMIT
  BLI_freelistN(&g_frame_cache.pics);
  g_frame_cache.pics_len = 0;
  g_frame_cache.pics_size_in_memory = 0;
#endif

#ifdef WITH_AUDASPACE
  if (g_audaspace.playback_handle) {
    AUD_Handle_stop(g_audaspace.playback_handle);
    g_audaspace.playback_handle = nullptr;
  }
  if (g_audaspace.scrub_handle) {
    AUD_Handle_stop(g_audaspace.scrub_handle);
    g_audaspace.scrub_handle = nullptr;
  }
  AUD_Sound_free(g_audaspace.source);
  g_audaspace.source = nullptr;
#endif

  /* Free subsystems the animation player is responsible for starting.
   * The rest is handled by #BKE_blender_atexit, see early-exit logic in `creator.cc`. */

  BLF_exit();

  /* NOTE: Must happen before GPU Context destruction as GPU resources are released via
   * Color Management module. */
  IMB_exit();
  MOV_exit();

  if (ps.ghost_data.gpu_context) {
    GPU_context_active_set(ps.ghost_data.gpu_context);
    GPU_exit();
    GPU_context_discard(ps.ghost_data.gpu_context);
    ps.ghost_data.gpu_context = nullptr;
  }
  GHOST_RemoveEventConsumer(ps.ghost_data.system, ghost_event_consumer);
  GHOST_DisposeEventConsumer(ghost_event_consumer);

  GHOST_DisposeWindow(ps.ghost_data.system, ps.ghost_data.window);

  GHOST_DisposeSystem(ps.ghost_data.system);

  if (ps.argv_next) {
    args_next->argc = ps.argc_next;
    args_next->argv = ps.argv_next;
    /* Returning none, run this function again with the *next* arguments. */
    return std::nullopt;
  }

  return EXIT_SUCCESS;
}

int WM_main_playanim(int argc, const char **argv)
{
#ifdef WITH_AUDASPACE
  {
    AUD_DeviceSpecs specs;

    specs.rate = AUD_RATE_48000;
    specs.format = AUD_FORMAT_FLOAT32;
    specs.channels = AUD_CHANNELS_STEREO;

    AUD_initOnce();

    if (!(g_audaspace.audio_device = AUD_init(nullptr, specs, 1024, "Blender"))) {
      g_audaspace.audio_device = AUD_init("None", specs, 0, "Blender");
    }
  }
#endif

  std::optional<int> exit_code = std::nullopt;
  PlayArgs args_next = {0};
  do {
    PlayArgs args_free = args_next;
    args_next = {0};

    if ((exit_code = wm_main_playanim_intern(argc, argv, &args_next))) {
      argc = 0;
      argv = nullptr;
    }
    else {
      argc = args_next.argc;
      argv = const_cast<const char **>(args_next.argv);
    }

    if (args_free.argv) {
      for (int i = 0; i < args_free.argc; i++) {
        MEM_freeN(args_free.argv[i]);
      }
      MEM_freeN(args_free.argv);
    }
  } while (argv != nullptr);
  /* Set in the loop. */
  BLI_assert(exit_code.has_value());

#ifdef WITH_AUDASPACE
  AUD_exit(g_audaspace.audio_device);
  AUD_exitOnce();
#endif

  /* Cleanup sub-systems started before this function was called. */
  BKE_blender_atexit();

  return exit_code.value();
}

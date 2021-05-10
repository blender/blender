/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * Animation player for image sequences & video's with sound support.
 * Launched in a separate process from Blender's #RENDER_OT_play_rendered_anim
 *
 * \note This file uses ghost directly and none of the WM definitions.
 * this could be made into its own module, alongside creator.
 */

#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef WIN32
#  include <sys/times.h>
#  include <sys/wait.h>
#  include <unistd.h>
#else
#  include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_image.h"

#include "BIF_glutil.h"

#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_init_exit.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLF_api.h"
#include "DNA_scene_types.h"
#include "ED_datafiles.h" /* for fonts */
#include "GHOST_C-api.h"

#include "DEG_depsgraph.h"

#include "WM_api.h" /* only for WM_main_playanim */

#ifdef WITH_AUDASPACE
#  include <AUD_Device.h>
#  include <AUD_Handle.h>
#  include <AUD_Sound.h>
#  include <AUD_Special.h>

static AUD_Sound *source = NULL;
static AUD_Handle *playback_handle = NULL;
static AUD_Handle *scrub_handle = NULL;
static AUD_Device *audio_device = NULL;
#endif

/* simple limiter to avoid flooding memory */
#define USE_FRAME_CACHE_LIMIT
#ifdef USE_FRAME_CACHE_LIMIT
#  define PLAY_FRAME_CACHE_MAX 30
#endif

struct PlayState;
static void playanim_window_zoom(struct PlayState *ps, const float zoom_offset);

/**
 * The current state of the player.
 *
 * \warning Don't store results of parsing command-line arguments
 * in this struct if they need to persist across playing back different
 * files as these will be cleared when playing other files (drag & drop).
 */
typedef struct PlayState {

  /** Window and viewport size. */
  int win_x, win_y;

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
  bool noskip;
  /** Display current frame over the window. */
  bool indicator;
  /** Single-frame stepping has been enabled (frame loading and update pending). */
  bool sstep;
  /** Playback has stopped the image has been displayed. */
  bool wait2;
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
  int fstep;

  /** Current frame (picture). */
  struct PlayAnimPict *picture;

  /** Image size in pixels, set once at the start. */
  int ibufx, ibufy;
  /** Mono-space font ID. */
  int fontid;

  /** Restarts player for file drop (drag & drop). */
  char dropped_file[FILE_MAX];

  /** Force update when scrubbing with the cursor. */
  bool need_frame_update;
  /** The current frame calculated by scrubbing the mouse cursor. */
  int frame_cursor_x;

  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
} PlayState;

/* for debugging */
#if 0
static void print_ps(PlayState *ps)
{
  printf("ps:\n");
  printf("    direction=%d,\n", (int)ps->direction);
  printf("    once=%d,\n", ps->once);
  printf("    pingpong=%d,\n", ps->pingpong);
  printf("    noskip=%d,\n", ps->noskip);
  printf("    sstep=%d,\n", ps->sstep);
  printf("    wait2=%d,\n", ps->wait2);
  printf("    stopped=%d,\n", ps->stopped);
  printf("    go=%d,\n\n", ps->go);
  fflush(stdout);
}
#endif

/* global for window and events */
typedef enum eWS_Qual {
  WS_QUAL_LSHIFT = (1 << 0),
  WS_QUAL_RSHIFT = (1 << 1),
  WS_QUAL_SHIFT = (WS_QUAL_LSHIFT | WS_QUAL_RSHIFT),
  WS_QUAL_LALT = (1 << 2),
  WS_QUAL_RALT = (1 << 3),
  WS_QUAL_ALT = (WS_QUAL_LALT | WS_QUAL_RALT),
  WS_QUAL_LCTRL = (1 << 4),
  WS_QUAL_RCTRL = (1 << 5),
  WS_QUAL_CTRL = (WS_QUAL_LCTRL | WS_QUAL_RCTRL),
  WS_QUAL_LMOUSE = (1 << 16),
  WS_QUAL_MMOUSE = (1 << 17),
  WS_QUAL_RMOUSE = (1 << 18),
  WS_QUAL_MOUSE = (WS_QUAL_LMOUSE | WS_QUAL_MMOUSE | WS_QUAL_RMOUSE),
} eWS_Qual;

static struct WindowStateGlobal {
  GHOST_SystemHandle ghost_system;
  void *ghost_window;
  GPUContext *gpu_context;

  /* events */
  eWS_Qual qual;
} g_WS = {NULL};

static void playanim_window_get_size(int *r_width, int *r_height)
{
  GHOST_RectangleHandle bounds = GHOST_GetClientBounds(g_WS.ghost_window);
  *r_width = GHOST_GetWidthRectangle(bounds);
  *r_height = GHOST_GetHeightRectangle(bounds);
  GHOST_DisposeRectangle(bounds);
}

static void playanim_gl_matrix(void)
{
  /* unified matrix, note it affects offset for drawing */
  /* note! cannot use GPU_matrix_ortho_2d_set here because shader ignores. */
  GPU_matrix_ortho_set(0.0f, 1.0f, 0.0f, 1.0f, -1.0, 1.0f);
}

/* implementation */
static void playanim_event_qual_update(void)
{
  int val;

  /* Shift */
  GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyLeftShift, &val);
  SET_FLAG_FROM_TEST(g_WS.qual, val, WS_QUAL_LSHIFT);

  GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyRightShift, &val);
  SET_FLAG_FROM_TEST(g_WS.qual, val, WS_QUAL_RSHIFT);

  /* Control */
  GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyLeftControl, &val);
  SET_FLAG_FROM_TEST(g_WS.qual, val, WS_QUAL_LCTRL);

  GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyRightControl, &val);
  SET_FLAG_FROM_TEST(g_WS.qual, val, WS_QUAL_RCTRL);

  /* Alt */
  GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyLeftAlt, &val);
  SET_FLAG_FROM_TEST(g_WS.qual, val, WS_QUAL_LALT);

  GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyRightAlt, &val);
  SET_FLAG_FROM_TEST(g_WS.qual, val, WS_QUAL_RALT);
}

typedef struct PlayAnimPict {
  struct PlayAnimPict *next, *prev;
  uchar *mem;
  int size;
  const char *name;
  struct ImBuf *ibuf;
  struct anim *anim;
  int frame;
  int IB_flags;

#ifdef USE_FRAME_CACHE_LIMIT
  /** Back pointer to the #LinkData node for this struct in the #g_frame_cache.pics list. */
  LinkData *frame_cache_node;
  size_t size_in_memory;
#endif
} PlayAnimPict;

static struct ListBase picsbase = {NULL, NULL};
/* frames in memory - store them here to for easy deallocation later */
static bool fromdisk = false;
static double ptottime = 0.0, swaptime = 0.04;
#ifdef WITH_AUDASPACE
static double fps_movie;
#endif

#ifdef USE_FRAME_CACHE_LIMIT
static struct {
  /** A list of #LinkData nodes referencing #PlayAnimPict to track cached frames. */
  struct ListBase pics;
  /** Number if elements in `pics`. */
  int pics_len;
  /** Keep track of memory used by #g_frame_cache.pics when `g_frame_cache.memory_limit != 0`. */
  size_t pics_size_in_memory;
  /** Optionally limit the amount of memory used for cache (in bytes), ignored when zero. */
  size_t memory_limit;
} g_frame_cache = {
    .pics = {NULL, NULL},
    .pics_len = 0,
    .pics_size_in_memory = 0,
    .memory_limit = 0,
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
  pic->ibuf = NULL;
  pic->frame_cache_node = NULL;
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

static bool frame_cache_limit_exceeded(void)
{
  return g_frame_cache.memory_limit ?
             (g_frame_cache.pics_size_in_memory > g_frame_cache.memory_limit) :
             (g_frame_cache.pics_len > PLAY_FRAME_CACHE_MAX);
}

static void frame_cache_limit_apply(ImBuf *ibuf_keep)
{
  /* Really basic memory conservation scheme. Keep frames in a FIFO queue. */
  LinkData *node = g_frame_cache.pics.last;
  while (node && frame_cache_limit_exceeded()) {
    PlayAnimPict *pic = node->data;
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
  ImBuf *ibuf = NULL;

  if (pic->ibuf) {
    ibuf = pic->ibuf;
  }
  else if (pic->anim) {
    ibuf = IMB_anim_absolute(pic->anim, pic->frame, IMB_TC_NONE, IMB_PROXY_NONE);
  }
  else if (pic->mem) {
    /* use correct colorspace here */
    ibuf = IMB_ibImageFromMemory(pic->mem, pic->size, pic->IB_flags, NULL, pic->name);
  }
  else {
    /* use correct colorspace here */
    ibuf = IMB_loadiffname(pic->name, pic->IB_flags, NULL);
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

static int pupdate_time(void)
{
  static double ltime;

  double time = PIL_check_seconds_timer();

  ptottime += (time - ltime);
  ltime = time;
  return (ptottime < 0);
}

static void *ocio_transform_ibuf(PlayState *ps,
                                 ImBuf *ibuf,
                                 bool *r_glsl_used,
                                 eGPUTextureFormat *r_format,
                                 eGPUDataFormat *r_data,
                                 void **r_buffer_cache_handle)
{
  void *display_buffer;
  bool force_fallback = false;
  *r_glsl_used = false;
  force_fallback |= (ED_draw_imbuf_method(ibuf) != IMAGE_DRAW_METHOD_GLSL);
  force_fallback |= (ibuf->dither != 0.0f);

  /* Default */
  *r_format = GPU_RGBA8;
  *r_data = GPU_DATA_UBYTE;

  /* Fallback to CPU based color space conversion. */
  if (force_fallback) {
    *r_glsl_used = false;
    display_buffer = NULL;
  }
  else if (ibuf->rect_float) {
    display_buffer = ibuf->rect_float;

    *r_data = GPU_DATA_FLOAT;
    if (ibuf->channels == 4) {
      *r_format = GPU_RGBA16F;
    }
    else if (ibuf->channels == 3) {
      /* Alpha is implicitly 1. */
      *r_format = GPU_RGB16F;
    }

    if (ibuf->float_colorspace) {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space(&ps->view_settings,
                                                                    &ps->display_settings,
                                                                    ibuf->float_colorspace,
                                                                    ibuf->dither,
                                                                    false,
                                                                    false);
    }
    else {
      *r_glsl_used = IMB_colormanagement_setup_glsl_draw(
          &ps->view_settings, &ps->display_settings, ibuf->dither, false);
    }
  }
  else if (ibuf->rect) {
    display_buffer = ibuf->rect;
    *r_glsl_used = IMB_colormanagement_setup_glsl_draw_from_space(&ps->view_settings,
                                                                  &ps->display_settings,
                                                                  ibuf->float_colorspace,
                                                                  ibuf->dither,
                                                                  false,
                                                                  false);
  }
  else {
    display_buffer = NULL;
  }

  /* There is data to be displayed, but GLSL is not initialized
   * properly, in this case we fallback to CPU-based display transform. */
  if ((ibuf->rect || ibuf->rect_float) && !*r_glsl_used) {
    display_buffer = IMB_display_buffer_acquire(
        ibuf, &ps->view_settings, &ps->display_settings, r_buffer_cache_handle);
    *r_format = GPU_RGBA8;
    *r_data = GPU_DATA_UBYTE;
  }

  return display_buffer;
}

static void draw_display_buffer(PlayState *ps, ImBuf *ibuf)
{
  void *display_buffer;

  /* Format needs to be created prior to any #immBindShader call.
   * Do it here because OCIO binds its own shader. */
  eGPUTextureFormat format;
  eGPUDataFormat data;
  bool glsl_used = false;
  GPUVertFormat *imm_format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(imm_format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint texCoord = GPU_vertformat_attr_add(
      imm_format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  void *buffer_cache_handle = NULL;
  display_buffer = ocio_transform_ibuf(ps, ibuf, &glsl_used, &format, &data, &buffer_cache_handle);

  GPUTexture *texture = GPU_texture_create_2d("display_buf", ibuf->x, ibuf->y, 1, format, NULL);
  GPU_texture_update(texture, data, display_buffer);
  GPU_texture_filter_mode(texture, false);

  GPU_texture_bind(texture, 0);

  if (!glsl_used) {
    immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_COLOR);
    immUniformColor3f(1.0f, 1.0f, 1.0f);
    immUniform1i("image", 0);
  }

  immBegin(GPU_PRIM_TRI_FAN, 4);

  rctf preview;
  rctf canvas;

  BLI_rctf_init(&canvas, 0.0f, 1.0f, 0.0f, 1.0f);
  BLI_rctf_init(&preview, 0.0f, 1.0f, 0.0f, 1.0f);

  if (ps->draw_flip[0]) {
    SWAP(float, canvas.xmin, canvas.xmax);
  }
  if (ps->draw_flip[1]) {
    SWAP(float, canvas.ymin, canvas.ymax);
  }

  immAttr2f(texCoord, canvas.xmin, canvas.ymin);
  immVertex2f(pos, preview.xmin, preview.ymin);

  immAttr2f(texCoord, canvas.xmin, canvas.ymax);
  immVertex2f(pos, preview.xmin, preview.ymax);

  immAttr2f(texCoord, canvas.xmax, canvas.ymax);
  immVertex2f(pos, preview.xmax, preview.ymax);

  immAttr2f(texCoord, canvas.xmax, canvas.ymin);
  immVertex2f(pos, preview.xmax, preview.ymin);

  immEnd();

  GPU_texture_unbind(texture);
  GPU_texture_free(texture);

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

static void playanim_toscreen(
    PlayState *ps, PlayAnimPict *picture, struct ImBuf *ibuf, int fontid, int fstep)
{
  if (ibuf == NULL) {
    printf("%s: no ibuf for picture '%s'\n", __func__, picture ? picture->name : "<NIL>");
    return;
  }

  GHOST_ActivateWindowDrawingContext(g_WS.ghost_window);

  /* size within window */
  float span_x = (ps->zoom * ibuf->x) / (float)ps->win_x;
  float span_y = (ps->zoom * ibuf->y) / (float)ps->win_y;

  /* offset within window */
  float offs_x = 0.5f * (1.0f - span_x);
  float offs_y = 0.5f * (1.0f - span_y);

  CLAMP(offs_x, 0.0f, 1.0f);
  CLAMP(offs_y, 0.0f, 1.0f);

  GPU_clear_color(0.1f, 0.1f, 0.1f, 0.0f);

  /* checkerboard for case alpha */
  if (ibuf->planes == 32) {
    GPU_blend(GPU_BLEND_ALPHA);

    imm_draw_box_checker_2d_ex(offs_x,
                               offs_y,
                               offs_x + span_x,
                               offs_y + span_y,
                               (const float[4]){0.15, 0.15, 0.15, 1.0},
                               (const float[4]){0.20, 0.20, 0.20, 1.0},
                               8);
  }

  draw_display_buffer(ps, ibuf);

  GPU_blend(GPU_BLEND_NONE);

  pupdate_time();

  if (picture && (g_WS.qual & (WS_QUAL_SHIFT | WS_QUAL_LMOUSE)) && (fontid != -1)) {
    int sizex, sizey;
    float fsizex_inv, fsizey_inv;
    char str[32 + FILE_MAX];
    BLI_snprintf(str, sizeof(str), "%s | %.2f frames/s", picture->name, fstep / swaptime);

    playanim_window_get_size(&sizex, &sizey);
    fsizex_inv = 1.0f / sizex;
    fsizey_inv = 1.0f / sizey;

    BLF_color4f(fontid, 1.0, 1.0, 1.0, 1.0);
    BLF_enable(fontid, BLF_ASPECT);
    BLF_aspect(fontid, fsizex_inv, fsizey_inv, 1.0f);
    BLF_position(fontid, 10.0f * fsizex_inv, 10.0f * fsizey_inv, 0.0f);
    BLF_draw(fontid, str, sizeof(str));
  }

  if (ps->indicator) {
    float fac = ps->picture->frame / (double)(((PlayAnimPict *)picsbase.last)->frame -
                                              ((PlayAnimPict *)picsbase.first)->frame);

    fac = 2.0f * fac - 1.0f;
    GPU_matrix_push_projection();
    GPU_matrix_identity_projection_set();
    GPU_matrix_push();
    GPU_matrix_identity_set();

    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor3ub(0, 255, 0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, fac, -1.0f);
    immVertex2f(pos, fac, 1.0f);
    immEnd();

    immUnbindProgram();

    GPU_matrix_pop();
    GPU_matrix_pop_projection();
  }

  GHOST_SwapWindowBuffers(g_WS.ghost_window);
}

static void build_pict_list_ex(
    PlayState *ps, const char *first, int totframes, int fstep, int fontid)
{
  if (IMB_isanim(first)) {
    /* OCIO_TODO: support different input color space */
    struct anim *anim = IMB_open_anim(first, IB_rect, 0, NULL);
    if (anim) {
      int pic;
      struct ImBuf *ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
      if (ibuf) {
        playanim_toscreen(ps, NULL, ibuf, fontid, fstep);
        IMB_freeImBuf(ibuf);
      }

      for (pic = 0; pic < IMB_anim_get_duration(anim, IMB_TC_NONE); pic++) {
        PlayAnimPict *picture = (PlayAnimPict *)MEM_callocN(sizeof(PlayAnimPict), "Pict");
        picture->anim = anim;
        picture->frame = pic;
        picture->IB_flags = IB_rect;
        picture->name = BLI_sprintfN("%s : %4.d", first, pic + 1);
        BLI_addtail(&picsbase, picture);
      }
    }
    else {
      printf("couldn't open anim %s\n", first);
    }
  }
  else {
    /* Load images into cache until the cache is full,
     * this resolves choppiness for images that are slow to load, see: T81751. */
#ifdef USE_FRAME_CACHE_LIMIT
    bool fill_cache = true;
#else
    bool fill_cache = false;
#endif

    int count = 0;

    int fp_framenr;
    struct {
      char head[FILE_MAX], tail[FILE_MAX];
      unsigned short digits;
    } fp_decoded;

    char filepath[FILE_MAX];
    BLI_strncpy(filepath, first, sizeof(filepath));
    fp_framenr = BLI_path_sequence_decode(
        filepath, fp_decoded.head, fp_decoded.tail, &fp_decoded.digits);

    pupdate_time();
    ptottime = 1.0;

    /* O_DIRECT
     *
     * If set, all reads and writes on the resulting file descriptor will
     * be performed directly to or from the user program buffer, provided
     * appropriate size and alignment restrictions are met.  Refer to the
     * F_SETFL and F_DIOINFO commands in the fcntl(2) manual entry for
     * information about how to determine the alignment constraints.
     * O_DIRECT is a Silicon Graphics extension and is only supported on
     * local EFS and XFS file systems.
     */

    while (IMB_ispic(filepath) && totframes) {
      bool has_event;
      size_t size;
      int file;

      file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
      if (file < 0) {
        /* print errno? */
        return;
      }

      PlayAnimPict *picture = (PlayAnimPict *)MEM_callocN(sizeof(PlayAnimPict), "picture");
      if (picture == NULL) {
        printf("Not enough memory for pict struct '%s'\n", filepath);
        close(file);
        return;
      }
      size = BLI_file_descriptor_size(file);

      if (size < 1) {
        close(file);
        MEM_freeN(picture);
        return;
      }

      picture->size = size;
      picture->IB_flags = IB_rect;

      uchar *mem;
      if (fromdisk == false) {
        mem = MEM_mallocN(size, "build pic list");
        if (mem == NULL) {
          printf("Couldn't get memory\n");
          close(file);
          MEM_freeN(picture);
          return;
        }

        if (read(file, mem, size) != size) {
          printf("Error while reading %s\n", filepath);
          close(file);
          MEM_freeN(picture);
          MEM_freeN(mem);
          return;
        }
      }
      else {
        mem = NULL;
      }

      picture->mem = mem;
      picture->name = BLI_strdup(filepath);
      picture->frame = count;
      close(file);
      BLI_addtail(&picsbase, picture);
      count++;

      pupdate_time();

      const bool display_imbuf = ptottime > 1.0;

      if (display_imbuf || fill_cache) {
        /* OCIO_TODO: support different input color space */
        ImBuf *ibuf = ibuf_from_picture(picture);

        if (ibuf) {
          if (display_imbuf) {
            playanim_toscreen(ps, picture, ibuf, fontid, fstep);
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
          ptottime = 0.0;
        }
      }

      /* create a new filepath each time */
      fp_framenr += fstep;
      BLI_path_sequence_encode(
          filepath, fp_decoded.head, fp_decoded.tail, fp_decoded.digits, fp_framenr);

      while ((has_event = GHOST_ProcessEvents(g_WS.ghost_system, false))) {
        GHOST_DispatchEvents(g_WS.ghost_system);
        if (ps->loading == false) {
          return;
        }
      }

      totframes--;
    }
  }
}

static void build_pict_list(PlayState *ps, const char *first, int totframes, int fstep, int fontid)
{
  ps->loading = true;
  build_pict_list_ex(ps, first, totframes, fstep, fontid);
  ps->loading = false;
}

static void update_sound_fps(void)
{
#ifdef WITH_AUDASPACE
  if (playback_handle) {
    /* swaptime stores the 1.0/fps ratio */
    double speed = 1.0 / (swaptime * fps_movie);

    AUD_Handle_setPitch(playback_handle, speed);
  }
#endif
}

static void tag_change_frame(PlayState *ps, int cx)
{
  ps->need_frame_update = true;
  ps->frame_cursor_x = cx;
}

static void change_frame(PlayState *ps)
{
  if (!ps->need_frame_update) {
    return;
  }

  int sizex, sizey;
  int i, i_last;

  if (BLI_listbase_is_empty(&picsbase)) {
    return;
  }

  playanim_window_get_size(&sizex, &sizey);
  i_last = ((struct PlayAnimPict *)picsbase.last)->frame;
  i = (i_last * ps->frame_cursor_x) / sizex;
  CLAMP(i, 0, i_last);

#ifdef WITH_AUDASPACE
  if (scrub_handle) {
    AUD_Handle_stop(scrub_handle);
    scrub_handle = NULL;
  }

  if (playback_handle) {
    AUD_Status status = AUD_Handle_getStatus(playback_handle);
    if (status != AUD_STATUS_PLAYING) {
      AUD_Handle_stop(playback_handle);
      playback_handle = AUD_Device_play(audio_device, source, 1);
      if (playback_handle) {
        AUD_Handle_setPosition(playback_handle, i / fps_movie);
        scrub_handle = AUD_pauseAfter(playback_handle, 1 / fps_movie);
      }
      update_sound_fps();
    }
    else {
      AUD_Handle_setPosition(playback_handle, i / fps_movie);
      scrub_handle = AUD_pauseAfter(playback_handle, 1 / fps_movie);
    }
  }
  else if (source) {
    playback_handle = AUD_Device_play(audio_device, source, 1);
    if (playback_handle) {
      AUD_Handle_setPosition(playback_handle, i / fps_movie);
      scrub_handle = AUD_pauseAfter(playback_handle, 1 / fps_movie);
    }
    update_sound_fps();
  }
#endif

  ps->picture = BLI_findlink(&picsbase, i);
  BLI_assert(ps->picture != NULL);

  ps->sstep = true;
  ps->wait2 = false;
  ps->next_frame = 0;

  ps->need_frame_update = false;
}

static int ghost_event_proc(GHOST_EventHandle evt, GHOST_TUserDataPtr ps_void)
{
  PlayState *ps = (PlayState *)ps_void;
  const GHOST_TEventType type = GHOST_GetEventType(evt);
  /* Convert ghost event into value keyboard or mouse. */
  const int val = ELEM(type, GHOST_kEventKeyDown, GHOST_kEventButtonDown);

  // print_ps(ps);

  playanim_event_qual_update();

  /* first check if we're busy loading files */
  if (ps->loading) {
    switch (type) {
      case GHOST_kEventKeyDown:
      case GHOST_kEventKeyUp: {
        GHOST_TEventKeyData *key_data;

        key_data = (GHOST_TEventKeyData *)GHOST_GetEventData(evt);
        switch (key_data->key) {
          case GHOST_kKeyEsc:
            ps->loading = false;
            break;
          default:
            break;
        }
        break;
      }
      default:
        break;
    }
    return 1;
  }

  if (ps->wait2 && ps->stopped == false) {
    ps->stopped = true;
  }

  if (ps->wait2) {
    pupdate_time();
    ptottime = 0;
  }

  switch (type) {
    case GHOST_kEventKeyDown:
    case GHOST_kEventKeyUp: {
      GHOST_TEventKeyData *key_data;

      key_data = (GHOST_TEventKeyData *)GHOST_GetEventData(evt);
      switch (key_data->key) {
        case GHOST_kKeyA:
          if (val) {
            ps->noskip = !ps->noskip;
          }
          break;
        case GHOST_kKeyI:
          if (val) {
            ps->indicator = !ps->indicator;
          }
          break;
        case GHOST_kKeyP:
          if (val) {
            ps->pingpong = !ps->pingpong;
          }
          break;
        case GHOST_kKeyF: {
          if (val) {
            int axis = (g_WS.qual & WS_QUAL_SHIFT) ? 1 : 0;
            ps->draw_flip[axis] = !ps->draw_flip[axis];
          }
          break;
        }
        case GHOST_kKey1:
        case GHOST_kKeyNumpad1:
          if (val) {
            swaptime = ps->fstep / 60.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey2:
        case GHOST_kKeyNumpad2:
          if (val) {
            swaptime = ps->fstep / 50.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey3:
        case GHOST_kKeyNumpad3:
          if (val) {
            swaptime = ps->fstep / 30.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey4:
        case GHOST_kKeyNumpad4:
          if (g_WS.qual & WS_QUAL_SHIFT) {
            swaptime = ps->fstep / 24.0;
            update_sound_fps();
          }
          else {
            swaptime = ps->fstep / 25.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey5:
        case GHOST_kKeyNumpad5:
          if (val) {
            swaptime = ps->fstep / 20.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey6:
        case GHOST_kKeyNumpad6:
          if (val) {
            swaptime = ps->fstep / 15.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey7:
        case GHOST_kKeyNumpad7:
          if (val) {
            swaptime = ps->fstep / 12.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey8:
        case GHOST_kKeyNumpad8:
          if (val) {
            swaptime = ps->fstep / 10.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKey9:
        case GHOST_kKeyNumpad9:
          if (val) {
            swaptime = ps->fstep / 6.0;
            update_sound_fps();
          }
          break;
        case GHOST_kKeyLeftArrow:
          if (val) {
            ps->sstep = true;
            ps->wait2 = false;
            if (g_WS.qual & WS_QUAL_SHIFT) {
              ps->picture = picsbase.first;
              ps->next_frame = 0;
            }
            else {
              ps->next_frame = -1;
            }
          }
          break;
        case GHOST_kKeyDownArrow:
          if (val) {
            ps->wait2 = false;
            if (g_WS.qual & WS_QUAL_SHIFT) {
              ps->next_frame = ps->direction = -1;
            }
            else {
              ps->next_frame = -10;
              ps->sstep = true;
            }
          }
          break;
        case GHOST_kKeyRightArrow:
          if (val) {
            ps->sstep = true;
            ps->wait2 = false;
            if (g_WS.qual & WS_QUAL_SHIFT) {
              ps->picture = picsbase.last;
              ps->next_frame = 0;
            }
            else {
              ps->next_frame = 1;
            }
          }
          break;
        case GHOST_kKeyUpArrow:
          if (val) {
            ps->wait2 = false;
            if (g_WS.qual & WS_QUAL_SHIFT) {
              ps->next_frame = ps->direction = 1;
            }
            else {
              ps->next_frame = 10;
              ps->sstep = true;
            }
          }
          break;

        case GHOST_kKeySlash:
        case GHOST_kKeyNumpadSlash:
          if (val) {
            if (g_WS.qual & WS_QUAL_SHIFT) {
              if (ps->picture && ps->picture->ibuf) {
                printf(" Name: %s | Speed: %.2f frames/s\n",
                       ps->picture->ibuf->name,
                       ps->fstep / swaptime);
              }
            }
            else {
              swaptime = ps->fstep / 5.0;
              update_sound_fps();
            }
          }
          break;
        case GHOST_kKey0:
        case GHOST_kKeyNumpad0:
          if (val) {
            if (ps->once) {
              ps->once = ps->wait2 = false;
            }
            else {
              ps->picture = NULL;
              ps->once = true;
              ps->wait2 = false;
            }
          }
          break;

        case GHOST_kKeySpace:
          if (val) {
            if (ps->wait2 || ps->sstep) {
              ps->wait2 = ps->sstep = false;
#ifdef WITH_AUDASPACE
              {
                PlayAnimPict *picture = picsbase.first;
                /* TODO - store in ps direct? */
                int i = 0;

                while (picture && picture != ps->picture) {
                  i++;
                  picture = picture->next;
                }
                if (playback_handle) {
                  AUD_Handle_stop(playback_handle);
                }
                playback_handle = AUD_Device_play(audio_device, source, 1);
                if (playback_handle) {
                  AUD_Handle_setPosition(playback_handle, i / fps_movie);
                }
                update_sound_fps();
              }
#endif
            }
            else {
              ps->sstep = true;
              ps->wait2 = true;
#ifdef WITH_AUDASPACE
              if (playback_handle) {
                AUD_Handle_stop(playback_handle);
                playback_handle = NULL;
              }
#endif
            }
          }
          break;
        case GHOST_kKeyEnter:
        case GHOST_kKeyNumpadEnter:
          if (val) {
            ps->wait2 = ps->sstep = false;
#ifdef WITH_AUDASPACE
            {
              PlayAnimPict *picture = picsbase.first;
              /* TODO - store in ps direct? */
              int i = 0;
              while (picture && picture != ps->picture) {
                i++;
                picture = picture->next;
              }
              if (playback_handle) {
                AUD_Handle_stop(playback_handle);
              }
              playback_handle = AUD_Device_play(audio_device, source, 1);
              if (playback_handle) {
                AUD_Handle_setPosition(playback_handle, i / fps_movie);
              }
              update_sound_fps();
            }
#endif
          }
          break;
        case GHOST_kKeyPeriod:
        case GHOST_kKeyNumpadPeriod:
          if (val) {
            if (ps->sstep) {
              ps->wait2 = false;
            }
            else {
              ps->sstep = true;
              ps->wait2 = !ps->wait2;
#ifdef WITH_AUDASPACE
              if (playback_handle) {
                AUD_Handle_stop(playback_handle);
                playback_handle = NULL;
              }
#endif
            }
          }
          break;
        case GHOST_kKeyEqual:
        case GHOST_kKeyPlus:
        case GHOST_kKeyNumpadPlus: {
          if (val == 0) {
            break;
          }
          if (g_WS.qual & WS_QUAL_CTRL) {
            playanim_window_zoom(ps, 0.1f);
          }
          else {
            if (swaptime > ps->fstep / 60.0) {
              swaptime /= 1.1;
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
          if (g_WS.qual & WS_QUAL_CTRL) {
            playanim_window_zoom(ps, -0.1f);
          }
          else {
            if (swaptime < ps->fstep / 5.0) {
              swaptime *= 1.1;
              update_sound_fps();
            }
          }
          break;
        }
        case GHOST_kKeyEsc:
          ps->go = false;
          break;
        default:
          break;
      }
      break;
    }
    case GHOST_kEventButtonDown:
    case GHOST_kEventButtonUp: {
      GHOST_TEventButtonData *bd = GHOST_GetEventData(evt);
      int cx, cy, sizex, sizey, inside_window;

      GHOST_GetCursorPosition(g_WS.ghost_system, &cx, &cy);
      GHOST_ScreenToClient(g_WS.ghost_window, cx, cy, &cx, &cy);
      playanim_window_get_size(&sizex, &sizey);

      inside_window = (cx >= 0 && cx < sizex && cy >= 0 && cy <= sizey);

      if (bd->button == GHOST_kButtonMaskLeft) {
        if (type == GHOST_kEventButtonDown) {
          if (inside_window) {
            g_WS.qual |= WS_QUAL_LMOUSE;
            tag_change_frame(ps, cx);
          }
        }
        else {
          g_WS.qual &= ~WS_QUAL_LMOUSE;
        }
      }
      else if (bd->button == GHOST_kButtonMaskMiddle) {
        if (type == GHOST_kEventButtonDown) {
          if (inside_window) {
            g_WS.qual |= WS_QUAL_MMOUSE;
          }
        }
        else {
          g_WS.qual &= ~WS_QUAL_MMOUSE;
        }
      }
      else if (bd->button == GHOST_kButtonMaskRight) {
        if (type == GHOST_kEventButtonDown) {
          if (inside_window) {
            g_WS.qual |= WS_QUAL_RMOUSE;
          }
        }
        else {
          g_WS.qual &= ~WS_QUAL_RMOUSE;
        }
      }
      break;
    }
    case GHOST_kEventCursorMove: {
      if (g_WS.qual & WS_QUAL_LMOUSE) {
        GHOST_TEventCursorData *cd = GHOST_GetEventData(evt);
        int cx, cy;

        /* Ignore 'in-between' events, since they can make scrubbing lag.
         *
         * Ideally we would keep into the event queue and see if this is the last motion event.
         * however the API currently doesn't support this. */
        {
          int x_test, y_test;
          GHOST_GetCursorPosition(g_WS.ghost_system, &x_test, &y_test);
          if (x_test != cd->x || y_test != cd->y) {
            /* we're not the last event... skipping */
            break;
          }
        }

        GHOST_ScreenToClient(g_WS.ghost_window, cd->x, cd->y, &cx, &cy);

        tag_change_frame(ps, cx);
      }
      break;
    }
    case GHOST_kEventWindowActivate:
    case GHOST_kEventWindowDeactivate: {
      g_WS.qual &= ~WS_QUAL_MOUSE;
      break;
    }
    case GHOST_kEventWindowSize:
    case GHOST_kEventWindowMove: {
      float zoomx, zoomy;

      playanim_window_get_size(&ps->win_x, &ps->win_y);
      GHOST_ActivateWindowDrawingContext(g_WS.ghost_window);

      zoomx = (float)ps->win_x / ps->ibufx;
      zoomy = (float)ps->win_y / ps->ibufy;

      /* zoom always show entire image */
      ps->zoom = MIN2(zoomx, zoomy);

      GPU_viewport(0, 0, ps->win_x, ps->win_y);
      GPU_scissor(0, 0, ps->win_x, ps->win_y);

      playanim_gl_matrix();

      ptottime = 0.0;
      playanim_toscreen(
          ps, ps->picture, ps->picture ? ps->picture->ibuf : NULL, ps->fontid, ps->fstep);

      break;
    }
    case GHOST_kEventQuitRequest:
    case GHOST_kEventWindowClose: {
      ps->go = false;
      break;
    }
    case GHOST_kEventDraggingDropDone: {
      GHOST_TEventDragnDropData *ddd = GHOST_GetEventData(evt);

      if (ddd->dataType == GHOST_kDragnDropTypeFilenames) {
        GHOST_TStringArray *stra = ddd->data;
        int a;

        for (a = 0; a < stra->count; a++) {
          BLI_strncpy(ps->dropped_file, (char *)stra->strings[a], sizeof(ps->dropped_file));
          ps->go = false;
          printf("drop file %s\n", stra->strings[a]);
          break; /* only one drop element supported now */
        }
      }
      break;
    }
    default:
      /* quiet warnings */
      break;
  }

  return 1;
}

static void playanim_window_open(const char *title, int posx, int posy, int sizex, int sizey)
{
  GHOST_GLSettings glsettings = {0};
  GHOST_TUns32 scr_w, scr_h;

  GHOST_GetMainDisplayDimensions(g_WS.ghost_system, &scr_w, &scr_h);

  posy = (scr_h - posy - sizey);

  g_WS.ghost_window = GHOST_CreateWindow(g_WS.ghost_system,
                                         NULL,
                                         title,
                                         posx,
                                         posy,
                                         sizex,
                                         sizey,
                                         /* could optionally start fullscreen */
                                         GHOST_kWindowStateNormal,
                                         false,
                                         GHOST_kDrawingContextTypeOpenGL,
                                         glsettings);
}

static void playanim_window_zoom(PlayState *ps, const float zoom_offset)
{
  int sizex, sizey;
  /* int ofsx, ofsy; */ /* UNUSED */

  if (ps->zoom + zoom_offset > 0.0f) {
    ps->zoom += zoom_offset;
  }

  // playanim_window_get_position(&ofsx, &ofsy);
  playanim_window_get_size(&sizex, &sizey);
  /* ofsx += sizex / 2; */ /* UNUSED */
  /* ofsy += sizey / 2; */ /* UNUSED */
  sizex = ps->zoom * ps->ibufx;
  sizey = ps->zoom * ps->ibufy;
  /* ofsx -= sizex / 2; */ /* UNUSED */
  /* ofsy -= sizey / 2; */ /* UNUSED */
  // window_set_position(g_WS.ghost_window, sizex, sizey);
  GHOST_SetClientSize(g_WS.ghost_window, sizex, sizey);
}

/**
 * \return The a path used to restart the animation player or NULL to exit.
 */
static char *wm_main_playanim_intern(int argc, const char **argv)
{
  struct ImBuf *ibuf = NULL;
  static char filepath[FILE_MAX]; /* abused to return dropped file path */
  GHOST_TUns32 maxwinx, maxwiny;
  int i;
  /* This was done to disambiguate the name for use under c++. */
  int start_x = 0, start_y = 0;
  int sfra = -1;
  int efra = -1;
  int totblock;

  PlayState ps = {0};

  ps.go = true;
  ps.direction = true;
  ps.next_frame = 1;
  ps.once = false;
  ps.pingpong = false;
  ps.noskip = false;
  ps.sstep = false;
  ps.wait2 = false;
  ps.stopped = false;
  ps.loading = false;
  ps.picture = NULL;
  ps.indicator = false;
  ps.dropped_file[0] = 0;
  ps.zoom = 1.0f;
  ps.draw_flip[0] = false;
  ps.draw_flip[1] = false;

  ps.fstep = 1;

  ps.fontid = -1;

  STRNCPY(ps.display_settings.display_device,
          IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE));
  IMB_colormanagement_init_default_view_settings(&ps.view_settings, &ps.display_settings);

  /* Skip the first argument which is assumed to be '-a' (used to launch this player). */
  while (argc > 1) {
    if (argv[1][0] == '-') {
      switch (argv[1][1]) {
        case 'm':
          fromdisk = true;
          break;
        case 'p':
          if (argc > 3) {
            start_x = atoi(argv[2]);
            start_y = atoi(argv[3]);
            argc -= 2;
            argv += 2;
          }
          else {
            printf("too few arguments for -p (need 2): skipping\n");
          }
          break;
        case 'f':
          if (argc > 3) {
            double fps = atof(argv[2]);
            double fps_base = atof(argv[3]);
            if (fps == 0.0) {
              fps = 1;
              printf(
                  "invalid fps,"
                  "forcing 1\n");
            }
            swaptime = fps_base / fps;
            argc -= 2;
            argv += 2;
          }
          else {
            printf("too few arguments for -f (need 2): skipping\n");
          }
          break;
        case 's':
          sfra = atoi(argv[2]);
          CLAMP(sfra, 1, MAXFRAME);
          argc--;
          argv++;
          break;
        case 'e':
          efra = atoi(argv[2]);
          CLAMP(efra, 1, MAXFRAME);
          argc--;
          argv++;
          break;
        case 'j':
          ps.fstep = atoi(argv[2]);
          CLAMP(ps.fstep, 1, MAXFRAME);
          swaptime *= ps.fstep;
          argc--;
          argv++;
          break;
        case 'c': {
#ifdef USE_FRAME_CACHE_LIMIT
          const int memory_in_mb = max_ii(0, atoi(argv[2]));
          g_frame_cache.memory_limit = (size_t)memory_in_mb * (1024 * 1024);
#endif
          argc--;
          argv++;
          break;
        }
        default:
          printf("unknown option '%c': skipping\n", argv[1][1]);
          break;
      }
      argc--;
      argv++;
    }
    else {
      break;
    }
  }

  if (argc > 1) {
    BLI_strncpy(filepath, argv[1], sizeof(filepath));
  }
  else {
    printf("%s: no filepath argument given\n", __func__);
    exit(1);
  }

  if (IMB_isanim(filepath)) {
    /* OCIO_TODO: support different input color spaces */
    struct anim *anim;
    anim = IMB_open_anim(filepath, IB_rect, 0, NULL);
    if (anim) {
      ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
      IMB_close_anim(anim);
      anim = NULL;
    }
  }
  else if (!IMB_ispic(filepath)) {
    printf("%s: '%s' not an image file\n", __func__, filepath);
    exit(1);
  }

  if (ibuf == NULL) {
    /* OCIO_TODO: support different input color space */
    ibuf = IMB_loadiffname(filepath, IB_rect, NULL);
  }

  if (ibuf == NULL) {
    printf("%s: '%s' couldn't open\n", __func__, filepath);
    exit(1);
  }

  {

    GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(ghost_event_proc, &ps);

    g_WS.ghost_system = GHOST_CreateSystem();
    GHOST_AddEventConsumer(g_WS.ghost_system, consumer);

    playanim_window_open("Blender Animation Player", start_x, start_y, ibuf->x, ibuf->y);
  }

  GHOST_GetMainDisplayDimensions(g_WS.ghost_system, &maxwinx, &maxwiny);

  // GHOST_ActivateWindowDrawingContext(g_WS.ghost_window);

  /* initialize OpenGL immediate mode */
  g_WS.gpu_context = GPU_context_create(g_WS.ghost_window);
  GPU_init();

  /* initialize the font */
  BLF_init();
  ps.fontid = BLF_load_mono_default(false);
  BLF_size(ps.fontid, 11, 72);

  ps.ibufx = ibuf->x;
  ps.ibufy = ibuf->y;

  ps.win_x = ps.ibufx;
  ps.win_y = ps.ibufy;

  if (maxwinx % ibuf->x) {
    maxwinx = ibuf->x * (1 + (maxwinx / ibuf->x));
  }
  if (maxwiny % ibuf->y) {
    maxwiny = ibuf->y * (1 + (maxwiny / ibuf->y));
  }

  GPU_clear_color(0.1f, 0.1f, 0.1f, 0.0f);

  int win_x, win_y;
  playanim_window_get_size(&win_x, &win_y);
  GPU_viewport(0, 0, win_x, win_y);
  GPU_scissor(0, 0, win_x, win_y);
  playanim_gl_matrix();

  GHOST_SwapWindowBuffers(g_WS.ghost_window);

  if (sfra == -1 || efra == -1) {
    /* one of the frames was invalid, just use all images */
    sfra = 1;
    efra = MAXFRAME;
  }

  build_pict_list(&ps, filepath, (efra - sfra) + 1, ps.fstep, ps.fontid);

#ifdef WITH_AUDASPACE
  source = AUD_Sound_file(filepath);
  {
    struct anim *anim_movie = ((struct PlayAnimPict *)picsbase.first)->anim;
    if (anim_movie) {
      short frs_sec = 25;
      float frs_sec_base = 1.0;

      IMB_anim_get_fps(anim_movie, &frs_sec, &frs_sec_base, true);

      fps_movie = (double)frs_sec / (double)frs_sec_base;
      /* enforce same fps for movie as sound */
      swaptime = ps.fstep / fps_movie;
    }
  }
#endif

  for (i = 2; i < argc; i++) {
    BLI_strncpy(filepath, argv[i], sizeof(filepath));
    build_pict_list(&ps, filepath, (efra - sfra) + 1, ps.fstep, ps.fontid);
  }

  IMB_freeImBuf(ibuf);
  ibuf = NULL;

  pupdate_time();
  ptottime = 0;

  /* newly added in 2.6x, without this images never get freed */
#define USE_IMB_CACHE

  while (ps.go) {
    if (ps.pingpong) {
      ps.direction = -ps.direction;
    }

    if (ps.direction == 1) {
      ps.picture = picsbase.first;
    }
    else {
      ps.picture = picsbase.last;
    }

    if (ps.picture == NULL) {
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
    if (ptottime > 0.0) {
      ptottime = 0.0;
    }

#ifdef WITH_AUDASPACE
    if (playback_handle) {
      AUD_Handle_stop(playback_handle);
    }
    playback_handle = AUD_Device_play(audio_device, source, 1);
    update_sound_fps();
#endif

    while (ps.picture) {
      bool has_event;
#ifndef USE_IMB_CACHE
      if (ibuf != NULL && ibuf->ftype == IMB_FTYPE_NONE) {
        IMB_freeImBuf(ibuf);
      }
#endif

      ibuf = ibuf_from_picture(ps.picture);

      if (ibuf) {
#ifdef USE_IMB_CACHE
        ps.picture->ibuf = ibuf;
#endif

#ifdef USE_FRAME_CACHE_LIMIT
        if (ps.picture->frame_cache_node == NULL) {
          frame_cache_add(ps.picture);
        }
        else {
          frame_cache_touch(ps.picture);
        }
        frame_cache_limit_apply(ibuf);

#endif /* USE_FRAME_CACHE_LIMIT */

        BLI_strncpy(ibuf->name, ps.picture->name, sizeof(ibuf->name));

        /* why only windows? (from 2.4x) - campbell */
#ifdef _WIN32
        GHOST_SetTitle(g_WS.ghost_window, ps.picture->name);
#endif

        while (pupdate_time()) {
          PIL_sleep_ms(1);
        }
        ptottime -= swaptime;
        playanim_toscreen(&ps, ps.picture, ibuf, ps.fontid, ps.fstep);
      } /* else delete */
      else {
        printf("error: can't play this image type\n");
        exit(0);
      }

      if (ps.once) {
        if (ps.picture->next == NULL) {
          ps.wait2 = true;
        }
        else if (ps.picture->prev == NULL) {
          ps.wait2 = true;
        }
      }

      ps.next_frame = ps.direction;

      while ((has_event = GHOST_ProcessEvents(g_WS.ghost_system, false))) {
        GHOST_DispatchEvents(g_WS.ghost_system);
      }
      if (ps.go == false) {
        break;
      }
      change_frame(&ps);
      if (!has_event) {
        PIL_sleep_ms(1);
      }
      if (ps.wait2) {
        continue;
      }

      ps.wait2 = ps.sstep;

      if (ps.wait2 == false && ps.stopped) {
        ps.stopped = false;
      }

      pupdate_time();

      if (ps.picture && ps.next_frame) {
        /* Advance to the next frame, always at least set one step.
         * Implement frame-skipping when enabled and playback is not fast enough. */
        while (ps.picture) {
          ps.picture = playanim_step(ps.picture, ps.next_frame);

          if (ps.once && ps.picture != NULL) {
            if (ps.picture->next == NULL) {
              ps.wait2 = true;
            }
            else if (ps.picture->prev == NULL) {
              ps.wait2 = true;
            }
          }

          if (ps.wait2 || ptottime < swaptime || ps.noskip) {
            break;
          }
          ptottime -= swaptime;
        }
        if (ps.picture == NULL && ps.sstep) {
          ps.picture = playanim_step(ps.picture, ps.next_frame);
        }
      }
      if (ps.go == false) {
        break;
      }
    }
  }
  while ((ps.picture = BLI_pophead(&picsbase))) {
    if (ps.picture->anim) {
      if ((ps.picture->next == NULL) || (ps.picture->next->anim != ps.picture->anim)) {
        IMB_close_anim(ps.picture->anim);
      }
    }

    if (ps.picture->ibuf) {
      IMB_freeImBuf(ps.picture->ibuf);
    }
    if (ps.picture->mem) {
      MEM_freeN(ps.picture->mem);
    }

    MEM_freeN((void *)ps.picture->name);
    MEM_freeN(ps.picture);
  }

  /* cleanup */
#ifndef USE_IMB_CACHE
  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }
#endif

  BLI_freelistN(&picsbase);

#ifdef USE_FRAME_CACHE_LIMIT
  BLI_freelistN(&g_frame_cache.pics);
  g_frame_cache.pics_len = 0;
  g_frame_cache.pics_size_in_memory = 0;
#endif

#ifdef WITH_AUDASPACE
  if (playback_handle) {
    AUD_Handle_stop(playback_handle);
    playback_handle = NULL;
  }
  if (scrub_handle) {
    AUD_Handle_stop(scrub_handle);
    scrub_handle = NULL;
  }
  AUD_Sound_free(source);
  source = NULL;
#endif
  /* we still miss freeing a lot!,
   * but many areas could skip initialization too for anim play */

  GPU_shader_free_builtin_shaders();

  if (g_WS.gpu_context) {
    GPU_context_active_set(g_WS.gpu_context);
    GPU_context_discard(g_WS.gpu_context);
    g_WS.gpu_context = NULL;
  }

  BLF_exit();

  GPU_exit();

  GHOST_DisposeWindow(g_WS.ghost_system, g_WS.ghost_window);

  /* early exit, IMB and BKE should be exited only in end */
  if (ps.dropped_file[0]) {
    BLI_strncpy(filepath, ps.dropped_file, sizeof(filepath));
    return filepath;
  }

  IMB_exit();
  BKE_images_exit();
  DEG_free_node_types();

  totblock = MEM_get_memory_blocks_in_use();
  if (totblock != 0) {
    /* prints many bAKey, bArgument's which are tricky to fix */
#if 0
    printf("Error Totblock: %d\n", totblock);
    MEM_printmemlist();
#endif
  }

  return NULL;
}

void WM_main_playanim(int argc, const char **argv)
{
  const char *argv_next[2];
  bool looping = true;

#ifdef WITH_AUDASPACE
  {
    AUD_DeviceSpecs specs;

    specs.rate = AUD_RATE_48000;
    specs.format = AUD_FORMAT_S16;
    specs.channels = AUD_CHANNELS_STEREO;

    AUD_initOnce();

    if (!(audio_device = AUD_init(NULL, specs, 1024, "Blender"))) {
      audio_device = AUD_init("None", specs, 0, "Blender");
    }
  }
#endif

  while (looping) {
    const char *filepath = wm_main_playanim_intern(argc, argv);

    if (filepath) { /* use simple args */
      argv_next[0] = argv[0];
      argv_next[1] = filepath;
      argc = 2;

      /* continue with new args */
      argv = argv_next;
    }
    else {
      looping = false;
    }
  }

#ifdef WITH_AUDASPACE
  AUD_exit(audio_device);
  AUD_exitOnce();
#endif
}

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
 * \ingroup DNA
 */

#ifndef __DNA_IMAGE_TYPES_H__
#define __DNA_IMAGE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_color_types.h" /* for color management */

struct GPUTexture;
struct MovieCache;
struct PackedFile;
struct RenderResult;
struct Scene;
struct anim;

/* ImageUser is in Texture, in Nodes, Background Image, Image Window, .... */
/* should be used in conjunction with an ID * to Image. */
typedef struct ImageUser {
  /** To retrieve render result. */
  struct Scene *scene;

  /** Movies, sequences: current to display. */
  int framenr;
  /** Total amount of frames to use. */
  int frames;
  /** Offset within movie, start frame in global time. */
  int offset, sfra;
  /** Cyclic flag. */
  char _pad0, cycl;
  char ok;

  /** Multiview current eye - for internal use of drawing routines. */
  char multiview_eye;
  short pass;
  char _pad1[2];

  /** Listbase indices, for menu browsing or retrieve buffer. */
  short multi_index, view, layer;
  short flag;
} ImageUser;

typedef struct ImageAnim {
  struct ImageAnim *next, *prev;
  struct anim *anim;
} ImageAnim;

typedef struct ImageView {
  struct ImageView *next, *prev;
  /** MAX_NAME. */
  char name[64];
  /** 1024 = FILE_MAX. */
  char filepath[1024];
} ImageView;

typedef struct ImagePackedFile {
  struct ImagePackedFile *next, *prev;
  struct PackedFile *packedfile;
  /** 1024 = FILE_MAX. */
  char filepath[1024];
} ImagePackedFile;

typedef struct RenderSlot {
  struct RenderSlot *next, *prev;
  /** 64 = MAX_NAME. */
  char name[64];
  struct RenderResult *render;
} RenderSlot;

/* iuser->flag */
#define IMA_ANIM_ALWAYS (1 << 0)
/* #define IMA_UNUSED_1         (1 << 1) */
/* #define IMA_UNUSED_2         (1 << 2) */
#define IMA_NEED_FRAME_RECALC (1 << 3)
#define IMA_SHOW_STEREO (1 << 4)

enum {
  TEXTARGET_TEXTURE_2D = 0,
  TEXTARGET_TEXTURE_CUBE_MAP = 1,
  TEXTARGET_COUNT = 2,
};

typedef struct Image {
  ID id;

  /** File path, 1024 = FILE_MAX. */
  char name[1024];

  /** Not written in file. */
  struct MovieCache *cache;
  /** Not written in file 2 = TEXTARGET_COUNT. */
  struct GPUTexture *gputexture[2];

  /* sources from: */
  ListBase anims;
  struct RenderResult *rr;

  ListBase renderslots;
  short render_slot, last_render_slot;

  int flag;
  short source, type;
  int lastframe;

  /* GPU texture flag. */
  short gpuflag;
  char _pad2[6];

  /** Deprecated. */
  struct PackedFile *packedfile DNA_DEPRECATED;
  struct ListBase packedfiles;
  struct PreviewImage *preview;

  int lastused;
  short ok;
  char _pad4[6];

  /* for generated images */
  int gen_x, gen_y;
  char gen_type, gen_flag;
  short gen_depth;
  float gen_color[4];

  /* display aspect - for UV editing images resized for faster openGL display */
  float aspx, aspy;

  /* color management */
  ColorManagedColorspaceSettings colorspace_settings;
  char alpha_mode;

  char _pad[5];

  /* Multiview */
  /** For viewer node stereoscopy. */
  char eye;
  char views_format;
  /** ImageView. */
  ListBase views;
  struct Stereo3dFormat *stereo3d_format;
} Image;

/* **************** IMAGE ********************* */

/* Image.flag */
enum {
  IMA_FLAG_UNUSED_0 = (1 << 0), /* cleared */
  IMA_FLAG_UNUSED_1 = (1 << 1), /* cleared */
#ifdef DNA_DEPRECATED
  IMA_DO_PREMUL = (1 << 2),
#endif
  IMA_FLAG_UNUSED_4 = (1 << 4), /* cleared */
  IMA_NOCOLLECT = (1 << 5),
  IMA_FLAG_UNUSED_6 = (1 << 6), /* cleared */
  IMA_OLD_PREMUL = (1 << 7),
  IMA_FLAG_UNUSED_8 = (1 << 8), /* cleared */
  IMA_USED_FOR_RENDER = (1 << 9),
  /** For image user, but these flags are mixed. */
  IMA_USER_FRAME_IN_RANGE = (1 << 10),
  IMA_VIEW_AS_RENDER = (1 << 11),
  IMA_IGNORE_ALPHA = (1 << 12),
  IMA_DEINTERLACE = (1 << 13),
  IMA_USE_VIEWS = (1 << 14),
  IMA_FLAG_UNUSED_15 = (1 << 15), /* cleared */
  IMA_FLAG_UNUSED_16 = (1 << 16), /* cleared */
};

/* Image.gpuflag */
enum {
  /** GPU texture needs to be refreshed. */
  IMA_GPU_REFRESH = (1 << 0),
  /** All mipmap levels in OpenGL texture set? */
  IMA_GPU_MIPMAP_COMPLETE = (1 << 1),
};

/* ima->type and ima->source moved to BKE_image.h, for API */

/* render */
#define IMA_MAX_RENDER_TEXT (1 << 9)

/* gen_flag */
#define IMA_GEN_FLOAT 1

/* alpha_mode */
enum {
  IMA_ALPHA_STRAIGHT = 0,
  IMA_ALPHA_PREMUL = 1,
};

#endif

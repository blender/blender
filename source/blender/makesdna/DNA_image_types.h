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

#pragma once

#include "DNA_ID.h"
#include "DNA_color_types.h" /* for color management */
#include "DNA_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

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
  char cycl;

  /** Multiview current eye - for internal use of drawing routines. */
  char multiview_eye;
  short pass;

  int tile;

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

typedef struct ImageTile_RuntimeTextureSlot {
  int tilearray_layer;
  int _pad;
  int tilearray_offset[2];
  int tilearray_size[2];
} ImageTile_RuntimeTextureSlot;

typedef struct ImageTile_Runtime {
  /* Data per `eImageTextureResolution`.
   * Should match `IMA_TEXTURE_RESOLUTION_LEN` */
  ImageTile_RuntimeTextureSlot slots[2];
} ImageTile_Runtime;

typedef struct ImageTile {
  struct ImageTile *next, *prev;

  struct ImageTile_Runtime runtime;

  char _pad[4];
  int tile_number;
  char label[64];
} ImageTile;

/* iuser->flag */
#define IMA_ANIM_ALWAYS (1 << 0)
/* #define IMA_UNUSED_1         (1 << 1) */
/* #define IMA_UNUSED_2         (1 << 2) */
#define IMA_NEED_FRAME_RECALC (1 << 3)
#define IMA_SHOW_STEREO (1 << 4)
/* Do not limit the resolution by the limit texture size option in the user preferences.
 * Images in the image editor or used as a backdrop are always shown using the maximum
 * possible resolution. */
#define IMA_SHOW_MAX_RESOLUTION (1 << 5)

/* Used to get the correct gpu texture from an Image datablock. */
typedef enum eGPUTextureTarget {
  TEXTARGET_2D = 0,
  TEXTARGET_2D_ARRAY,
  TEXTARGET_TILE_MAPPING,
  TEXTARGET_COUNT,
} eGPUTextureTarget;

/* Resolution variations that can be cached for an image. */
typedef enum eImageTextureResolution {
  IMA_TEXTURE_RESOLUTION_FULL = 0,
  IMA_TEXTURE_RESOLUTION_LIMITED,

  /* Not an option, but holds the number of options defined for this struct. */
  IMA_TEXTURE_RESOLUTION_LEN
} eImageTextureResolution;

typedef struct Image {
  ID id;

  /** File path, 1024 = FILE_MAX. */
  char filepath[1024];

  /** Not written in file. */
  struct MovieCache *cache;
  /** Not written in file 3 = TEXTARGET_COUNT, 2 = stereo eyes, 2 = IMA_TEXTURE_RESOLUTION_LEN. */
  struct GPUTexture *gputexture[3][2][2];

  /* sources from: */
  ListBase anims;
  struct RenderResult *rr;

  ListBase renderslots;
  short render_slot, last_render_slot;

  int flag;
  short source, type;
  int lastframe;

  /* GPU texture flag. */
  /* Contains `ImagePartialRefresh`. */
  ListBase gpu_refresh_areas;
  int gpuframenr;
  short gpuflag;
  short gpu_pass;
  short gpu_layer;
  short gpu_view;
  char _pad2[4];

  /** Deprecated. */
  struct PackedFile *packedfile DNA_DEPRECATED;
  struct ListBase packedfiles;
  struct PreviewImage *preview;

  int lastused;

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

  char _pad;

  /* Multiview */
  /** For viewer node stereoscopy. */
  char eye;
  char views_format;

  /* ImageTile list for UDIMs. */
  int active_tile_index;
  ListBase tiles;

  /** ImageView. */
  ListBase views;
  struct Stereo3dFormat *stereo3d_format;
} Image;

/* **************** IMAGE ********************* */

/* Image.flag */
enum {
  IMA_HIGH_BITDEPTH = (1 << 0),
  IMA_FLAG_UNUSED_1 = (1 << 1), /* cleared */
#ifdef DNA_DEPRECATED_ALLOW
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
  IMA_FLAG_UNUSED_12 = (1 << 12), /* cleared */
  IMA_DEINTERLACE = (1 << 13),
  IMA_USE_VIEWS = (1 << 14),
  IMA_FLAG_UNUSED_15 = (1 << 15), /* cleared */
  IMA_FLAG_UNUSED_16 = (1 << 16), /* cleared */
};

/* Image.gpuflag */
enum {
  /** GPU texture needs to be refreshed. */
  IMA_GPU_REFRESH = (1 << 0),
  /** GPU texture needs to be partially refreshed. */
  IMA_GPU_PARTIAL_REFRESH = (1 << 1),
  /** All mipmap levels in OpenGL texture set? */
  IMA_GPU_MIPMAP_COMPLETE = (1 << 2),
  /* Reuse the max resolution textures as they fit in the limited scale. */
  IMA_GPU_REUSE_MAX_RESOLUTION = (1 << 3),
  /* Has any limited scale textures been allocated.
   * Adds additional checks to reuse max resolution images when they fit inside limited scale. */
  IMA_GPU_HAS_LIMITED_SCALE_TEXTURES = (1 << 4),
};

/* Image.source, where the image comes from */
typedef enum eImageSource {
  /* IMA_SRC_CHECK = 0, */ /* UNUSED */
  IMA_SRC_FILE = 1,
  IMA_SRC_SEQUENCE = 2,
  IMA_SRC_MOVIE = 3,
  IMA_SRC_GENERATED = 4,
  IMA_SRC_VIEWER = 5,
  IMA_SRC_TILED = 6,
} eImageSource;

/* Image.type, how to handle or generate the image */
typedef enum eImageType {
  IMA_TYPE_IMAGE = 0,
  IMA_TYPE_MULTILAYER = 1,
  /* generated */
  IMA_TYPE_UV_TEST = 2,
  /* viewers */
  IMA_TYPE_R_RESULT = 4,
  IMA_TYPE_COMPOSITE = 5,
} eImageType;

/* Image.gen_type */
enum {
  IMA_GENTYPE_BLANK = 0,
  IMA_GENTYPE_GRID = 1,
  IMA_GENTYPE_GRID_COLOR = 2,
};

/* render */
#define IMA_MAX_RENDER_TEXT (1 << 9)

/* Image.gen_flag */
enum {
  IMA_GEN_FLOAT = 1,
};

/* Image.alpha_mode */
enum {
  IMA_ALPHA_STRAIGHT = 0,
  IMA_ALPHA_PREMUL = 1,
  IMA_ALPHA_CHANNEL_PACKED = 2,
  IMA_ALPHA_IGNORE = 3,
};

#ifdef __cplusplus
}
#endif

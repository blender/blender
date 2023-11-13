/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_color_types.h" /* for color management */
#include "DNA_defs.h"

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

  /* Which view and tile this ImagePackedFile represents. Normal images will use 0 and 1001
   * respectively when creating their ImagePackedFile. Must be provided for each packed image. */
  int view;
  int tile_number;
  /** 1024 = FILE_MAX. */
  char filepath[1024];
} ImagePackedFile;

typedef struct RenderSlot {
  struct RenderSlot *next, *prev;
  /** 64 = MAX_NAME. */
  char name[64];
  struct RenderResult *render;
} RenderSlot;

typedef struct ImageTile_Runtime {
  int tilearray_layer;
  int _pad;
  int tilearray_offset[2];
  int tilearray_size[2];
} ImageTile_Runtime;

typedef struct ImageTile {
  struct ImageTile *next, *prev;

  struct ImageTile_Runtime runtime;

  int tile_number;

  /* for generated images */
  int gen_x, gen_y;
  char gen_type, gen_flag;
  short gen_depth;
  float gen_color[4];

  char label[64];
} ImageTile;

/** #ImageUser::flag */
enum {
  IMA_ANIM_ALWAYS = 1 << 0,
  // IMA_UNUSED_1 = 1 << 1,
  // IMA_UNUSED_2 = 1 << 2,
  IMA_NEED_FRAME_RECALC = 1 << 3,
  IMA_SHOW_STEREO = 1 << 4,
  // IMA_UNUSED_5 = 1 << 5,
};

/* Used to get the correct gpu texture from an Image datablock. */
typedef enum eGPUTextureTarget {
  TEXTARGET_2D = 0,
  TEXTARGET_2D_ARRAY,
  TEXTARGET_TILE_MAPPING,
  TEXTARGET_COUNT,
} eGPUTextureTarget;

/* Defined in BKE_image.h. */
struct PartialUpdateRegister;
struct PartialUpdateUser;

typedef struct Image_Runtime {
  /* Mutex used to guarantee thread-safe access to the cached ImBuf of the corresponding image ID.
   */
  void *cache_mutex;

  /** \brief Register containing partial updates. */
  struct PartialUpdateRegister *partial_update_register;
  /** \brief Partial update user for GPUTextures stored inside the Image. */
  struct PartialUpdateUser *partial_update_user;

} Image_Runtime;

typedef struct Image {
  ID id;

  /** File path, 1024 = FILE_MAX. */
  char filepath[1024];

  /** Not written in file. */
  struct MovieCache *cache;
  /** Not written in file 3 = TEXTARGET_COUNT, 2 = stereo eyes. */
  struct GPUTexture *gputexture[3][2];

  /* sources from: */
  ListBase anims;
  struct RenderResult *rr;

  ListBase renderslots;
  short render_slot, last_render_slot;

  int flag;
  short source, type;
  int lastframe;

  /* GPU texture flag. */
  int gpuframenr;
  short gpuflag;
  short gpu_pass;
  short gpu_layer;
  short gpu_view;

  /* Number of iterations to perform when extracting mask for uv seam fixing. */
  short seam_margin;

  char _pad2[2];

  /** Deprecated. */
  struct PackedFile *packedfile DNA_DEPRECATED;
  struct ListBase packedfiles;
  struct PreviewImage *preview;

  int lastused;

  /* for generated images */
  int gen_x DNA_DEPRECATED, gen_y DNA_DEPRECATED;
  char gen_type DNA_DEPRECATED, gen_flag DNA_DEPRECATED;
  short gen_depth DNA_DEPRECATED;
  float gen_color[4] DNA_DEPRECATED;

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

  /** Offset caused by translation. Used in compositor backdrop for viewer nodes in image space. */
  int offset_x, offset_y;

  /* ImageTile list for UDIMs. */
  int active_tile_index;
  ListBase tiles;

  /** ImageView. */
  ListBase views;
  struct Stereo3dFormat *stereo3d_format;

  Image_Runtime runtime;
} Image;

/* **************** IMAGE ********************* */

/** #Image.flag */
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

/** #Image.gpuflag */
enum {
  /** All mipmap levels in OpenGL texture set? */
  IMA_GPU_MIPMAP_COMPLETE = (1 << 0),
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

/** #Image.gen_type */
enum {
  IMA_GENTYPE_BLANK = 0,
  IMA_GENTYPE_GRID = 1,
  IMA_GENTYPE_GRID_COLOR = 2,
};

/** Size of allocated string #RenderResult::text. */
#define IMA_MAX_RENDER_TEXT_SIZE 512

/** #Image.gen_flag */
enum {
  IMA_GEN_FLOAT = (1 << 0),
  IMA_GEN_TILE = (1 << 1),
};

/** #Image.alpha_mode */
enum {
  IMA_ALPHA_STRAIGHT = 0,
  IMA_ALPHA_PREMUL = 1,
  IMA_ALPHA_CHANNEL_PACKED = 2,
  IMA_ALPHA_IGNORE = 3,
};

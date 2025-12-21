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

#ifdef __cplusplus
namespace blender::bke {
struct ImageRuntime;
}  // namespace blender::bke
using ImageRuntimeHandle = blender::bke::ImageRuntime;
#else
struct ImageRuntimeHandle;
#endif

struct MovieReader;
struct MovieCache;
struct PackedFile;
struct RenderResult;
struct Scene;

/** #ImageUser::flag */
enum {
  IMA_ANIM_ALWAYS = 1 << 0,
  IMA_SHOW_SEQUENCER_SCENE = 1 << 1,
  // IMA_UNUSED_2 = 1 << 2,
  IMA_NEED_FRAME_RECALC = 1 << 3,
  IMA_SHOW_STEREO = 1 << 4,
  // IMA_UNUSED_5 = 1 << 5,
};

/* Used to get the correct gpu texture from an Image datablock. */
enum eGPUTextureTarget {
  TEXTARGET_2D = 0,
  TEXTARGET_2D_ARRAY = 1,
  TEXTARGET_TILE_MAPPING = 2,
  TEXTARGET_COUNT = 3,
};

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
enum eImageSource {
  /* IMA_SRC_CHECK = 0, */ /* UNUSED */
  IMA_SRC_FILE = 1,
  IMA_SRC_SEQUENCE = 2,
  IMA_SRC_MOVIE = 3,
  IMA_SRC_GENERATED = 4,
  IMA_SRC_VIEWER = 5,
  IMA_SRC_TILED = 6,
};

/* Image.type, how to handle or generate the image */
enum eImageType {
  IMA_TYPE_IMAGE = 0,
  IMA_TYPE_MULTILAYER = 1,
  /* generated */
  IMA_TYPE_UV_TEST = 2,
  /* viewers */
  IMA_TYPE_R_RESULT = 4,
  IMA_TYPE_COMPOSITE = 5,
};

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

/**
 * ImageUser is in Texture, in Nodes, Background Image, Image Window, ...
 * should be used in conjunction with an ID * to Image.
 */
struct ImageUser {
  /** To retrieve render result. */
  struct Scene *scene = nullptr;

  /** Movies, sequences: current to display. */
  int framenr = 0;
  /** Total amount of frames to use. */
  int frames = 0;
  /** Offset within movie, start frame in global time. */
  int offset = 0, sfra = 0;
  /** Cyclic flag. */
  char cycl = 0;

  /** Multiview current eye - for internal use of drawing routines. */
  char multiview_eye = 0;
  short pass = 0;

  int tile = 0;

  /** Listbase indices, for menu browsing or retrieve buffer. */
  short multi_index = 0, view = 0, layer = 0;
  short flag = 0;
};

struct ImageAnim {
  struct ImageAnim *next = nullptr, *prev = nullptr;
  struct MovieReader *anim = nullptr;
};

struct ImageView {
  struct ImageView *next = nullptr, *prev = nullptr;
  char name[/*MAX_NAME*/ 64] = "";
  char filepath[/*FILE_MAX*/ 1024] = "";
};

struct ImagePackedFile {
  struct ImagePackedFile *next = nullptr, *prev = nullptr;
  struct PackedFile *packedfile = nullptr;

  /* Which view and tile this ImagePackedFile represents. Normal images will use 0 and 1001
   * respectively when creating their ImagePackedFile. Must be provided for each packed image. */
  int view = 0;
  int tile_number = 0;
  char filepath[/*FILE_MAX*/ 1024] = "";
};

struct RenderSlot {
  struct RenderSlot *next = nullptr, *prev = nullptr;
  char name[/*MAX_NAME*/ 64] = "";
  struct RenderResult *render = nullptr;
};

struct ImageTile_Runtime {
  int tilearray_layer = 0;
  int _pad = {};
  int tilearray_offset[2] = {};
  int tilearray_size[2] = {};
};

struct ImageTile {
  struct ImageTile *next = nullptr, *prev = nullptr;

  struct ImageTile_Runtime runtime;

  int tile_number = 0;

  /* for generated images */
  int gen_x = 0, gen_y = 0;
  char gen_type = 0, gen_flag = 0;
  short gen_depth = 0;
  float gen_color[4] = {};

  char label[64] = "";
};

struct Image {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_IM;
#endif

  ID id;
  struct AnimData *adt = nullptr;

  /** File path. */
  char filepath[/*FILE_MAX*/ 1024] = "";

  /* sources from: */
  ListBaseT<ImageAnim> anims = {nullptr, nullptr};
  struct RenderResult *rr = nullptr;

  ListBaseT<RenderSlot> renderslots = {nullptr, nullptr};
  short render_slot = 0, last_render_slot = 0;

  int flag = 0;
  short source = 0, type = 0;
  int lastframe = 0;

  /* Number of iterations to perform when extracting mask for uv seam fixing. */
  short seam_margin = 8;

  char _pad2[6] = {};

  /** Deprecated. */
  DNA_DEPRECATED struct PackedFile *packedfile = nullptr;
  ListBaseT<ImagePackedFile> packedfiles = {nullptr, nullptr};
  struct PreviewImage *preview = nullptr;

  char _pad3[4] = {};

  /* for generated images */
  DNA_DEPRECATED int gen_x = 1024;
  DNA_DEPRECATED int gen_y = 1024;
  DNA_DEPRECATED char gen_type = IMA_GENTYPE_GRID;
  DNA_DEPRECATED char gen_flag = 0;
  DNA_DEPRECATED short gen_depth = 0;
  DNA_DEPRECATED float gen_color[4] = {};

  /* display aspect - for UV editing images resized for faster openGL display */
  float aspx = 1.0, aspy = 1.0;

  /* color management */
  ColorManagedColorspaceSettings colorspace_settings;
  char alpha_mode = 0;

  char _pad = {};

  /* Multiview */
  /** For viewer node stereoscopy. */
  char eye = 0;
  char views_format = 0;

  /* ImageTile list for UDIMs. */
  int active_tile_index = 0;
  ListBaseT<ImageTile> tiles = {nullptr, nullptr};

  ListBaseT<ImageView> views = {nullptr, nullptr};
  struct Stereo3dFormat *stereo3d_format = nullptr;

  ImageRuntimeHandle *runtime = nullptr;
};

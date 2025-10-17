/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <optional>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include <regex>
#include <string>

#include "BLI_array.hh"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_moviecache.hh"
#include "IMB_openexr.hh"

#include "MOV_read.hh"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_defaults.h"
#include "DNA_image_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BLI_math_vector.h"
#include "BLI_mempool.h"
#include "BLI_string_utf8.h"
#include "BLI_system.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_time.h"
#include "BLI_timecode.h" /* For stamp time-code format. */
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_bpath.hh"
#include "BKE_colortools.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_packedFile.hh"
#include "BKE_preview_image.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_workspace.hh"

#include "BLF_api.hh"

#include "RE_pipeline.h"

#include "SEQ_utils.hh" /* SEQ_get_topmost_sequence() */

#include "GPU_material.hh"
#include "GPU_texture.hh"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_engine.hh"

#include "BLO_read_write.hh"

/* for image user iteration */
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

using blender::Array;

static CLG_LogRef LOG = {"image"};

static void image_init(Image *ima, short source, short type);
static void image_free_packedfiles(Image *ima);
static void copy_image_packedfiles(ListBase *lb_dst, const ListBase *lb_src);

/* -------------------------------------------------------------------- */
/** \name Image #IDTypeInfo API
 * \{ */

static void image_runtime_free_data(Image *image)
{
  if (image->runtime->partial_update_user != nullptr) {
    BKE_image_partial_update_free(image->runtime->partial_update_user);
    image->runtime->partial_update_user = nullptr;
  }
  BKE_image_partial_update_register_free(image);
}

static void image_gpu_runtime_reset(Image *ima)
{
  ima->lastused = 0;
  ima->gpuflag = 0;
  ima->gpuframenr = IMAGE_GPU_FRAME_NONE;
  ima->gpu_pass = IMAGE_GPU_PASS_NONE;
  ima->gpu_layer = IMAGE_GPU_LAYER_NONE;
  ima->gpu_view = IMAGE_GPU_VIEW_NONE;
}

static void image_init_data(ID *id)
{
  Image *image = (Image *)id;

  if (image != nullptr) {
    image_init(image, IMA_SRC_GENERATED, IMA_TYPE_UV_TEST);
  }
}

static void image_copy_data(Main * /*bmain*/,
                            std::optional<Library *> /*owner_library*/,
                            ID *id_dst,
                            const ID *id_src,
                            const int flag)
{
  Image *image_dst = (Image *)id_dst;
  const Image *image_src = (const Image *)id_src;

  BKE_color_managed_colorspace_settings_copy(&image_dst->colorspace_settings,
                                             &image_src->colorspace_settings);

  copy_image_packedfiles(&image_dst->packedfiles, &image_src->packedfiles);

  image_dst->stereo3d_format = static_cast<Stereo3dFormat *>(
      MEM_dupallocN(image_src->stereo3d_format));
  BLI_duplicatelist(&image_dst->views, &image_src->views);

  /* Cleanup stuff that cannot be copied. */
  image_dst->cache = nullptr;
  image_dst->rr = nullptr;

  BLI_duplicatelist(&image_dst->renderslots, &image_src->renderslots);
  LISTBASE_FOREACH (RenderSlot *, slot, &image_dst->renderslots) {
    slot->render = nullptr;
  }

  BLI_listbase_clear(&image_dst->anims);

  BLI_duplicatelist(&image_dst->tiles, &image_src->tiles);

  for (int eye = 0; eye < 2; eye++) {
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      image_dst->gputexture[i][eye] = nullptr;
    }
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&image_dst->id, &image_src->id);
  }
  else {
    image_dst->preview = nullptr;
  }

  image_dst->runtime = MEM_new<blender::bke::ImageRuntime>(__func__);
}

static void image_free_data(ID *id)
{
  Image *image = (Image *)id;

  /* Also frees animations (#Image.anims list). */
  BKE_image_free_buffers(image);

  image_free_packedfiles(image);

  LISTBASE_FOREACH (RenderSlot *, slot, &image->renderslots) {
    if (slot->render) {
      RE_FreeRenderResult(slot->render);
      slot->render = nullptr;
    }
  }
  BLI_freelistN(&image->renderslots);

  BKE_image_free_views(image);
  MEM_SAFE_FREE(image->stereo3d_format);

  BKE_icon_id_delete(&image->id);
  BKE_previewimg_free(&image->preview);

  BLI_freelistN(&image->tiles);

  image_runtime_free_data(image);
  MEM_delete(image->runtime);
}

static void image_foreach_cache(ID *id,
                                IDTypeForeachCacheFunctionCallback function_callback,
                                void *user_data)
{
  Image *image = (Image *)id;
  IDCacheKey key;
  key.id_session_uid = id->session_uid;
  key.identifier = offsetof(Image, cache);
  function_callback(id, &key, (void **)&image->cache, 0, user_data);

  key.identifier = offsetof(Image, anims.first);
  function_callback(id, &key, &image->anims.first, 0, user_data);
  key.identifier = offsetof(Image, anims.last);
  function_callback(id, &key, &image->anims.last, 0, user_data);

  auto gputexture_offset = [image](int target, int eye) {
    constexpr size_t base_offset = offsetof(Image, gputexture);
    blender::gpu::Texture **first = &image->gputexture[0][0];
    const size_t array_offset = sizeof(*first) * (&image->gputexture[target][eye] - first);
    return base_offset + array_offset;
  };

  for (int eye = 0; eye < 2; eye++) {
    for (int a = 0; a < TEXTARGET_COUNT; a++) {
      blender::gpu::Texture *texture = image->gputexture[a][eye];
      if (texture == nullptr) {
        continue;
      }
      key.identifier = gputexture_offset(a, eye);
      function_callback(id, &key, (void **)&image->gputexture[a][eye], 0, user_data);
    }
  }

  key.identifier = offsetof(Image, rr);
  function_callback(id, &key, (void **)&image->rr, 0, user_data);

  LISTBASE_FOREACH (RenderSlot *, slot, &image->renderslots) {
    key.identifier = size_t(BLI_ghashutil_strhash_p(slot->name));
    function_callback(id, &key, (void **)&slot->render, 0, user_data);
  }
}

static void image_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  Image *ima = (Image *)id;
  const eBPathForeachFlag flag = bpath_data->flag;

  if (BKE_image_has_packedfile(ima) && (flag & BKE_BPATH_FOREACH_PATH_SKIP_PACKED) != 0) {
    return;
  }
  /* Skip empty file paths, these are typically from generated images and
   * don't make sense to add directories to until the image has been saved
   * once to give it a meaningful value. */
  /* TODO re-assess whether this behavior is desired in the new generic code context. */
  if (!ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE, IMA_SRC_TILED) ||
      ima->filepath[0] == '\0')
  {
    return;
  }

  /* If this is a tiled image, and we're asked to resolve the tokens in the virtual
   * filepath, use the first tile to generate a concrete path for use during processing. */
  bool result = false;
  if (ima->source == IMA_SRC_TILED && (flag & BKE_BPATH_FOREACH_PATH_RESOLVE_TOKEN) != 0) {
    char temp_path[FILE_MAX], orig_file[FILE_MAXFILE];
    STRNCPY(temp_path, ima->filepath);
    BLI_path_split_file_part(temp_path, orig_file, sizeof(orig_file));

    eUDIM_TILE_FORMAT tile_format;
    char *udim_pattern = BKE_image_get_tile_strformat(temp_path, &tile_format);
    BKE_image_set_filepath_from_tile_number(
        temp_path, udim_pattern, tile_format, ((ImageTile *)ima->tiles.first)->tile_number);
    MEM_SAFE_FREE(udim_pattern);

    result = BKE_bpath_foreach_path_fixed_process(bpath_data, temp_path, sizeof(temp_path));
    if (result) {
      /* Put the filepath back together using the new directory and the original file name. */
      char new_dir[FILE_MAXDIR];
      BLI_path_split_dir_part(temp_path, new_dir, sizeof(new_dir));
      BLI_path_join(ima->filepath, sizeof(ima->filepath), new_dir, orig_file);
    }
  }
  else {
    result = BKE_bpath_foreach_path_fixed_process(
        bpath_data, ima->filepath, sizeof(ima->filepath));
  }

  if (result) {
    if (flag & BKE_BPATH_FOREACH_PATH_RELOAD_EDITED) {
      if (!BKE_image_has_packedfile(ima) &&
          /* Image may have been painted onto (and not saved, #44543). */
          !BKE_image_is_dirty(ima))
      {
        BKE_image_signal(bpath_data->bmain, ima, nullptr, IMA_SIGNAL_RELOAD);
      }
    }
  }
}

static void image_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Image *ima = (Image *)id;
  const bool is_undo = BLO_write_is_undo(writer);

  /* Clear all data that isn't read to reduce false detection of changed image during memfile undo.
   */
  ima->cache = nullptr;
  image_gpu_runtime_reset(ima);
  BLI_listbase_clear(&ima->anims);
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 2; j++) {
      ima->gputexture[i][j] = nullptr;
    }
  }

  ImagePackedFile *imapf;

  BLI_assert(ima->packedfile == nullptr);
  if (!is_undo) {
    /* Do not store packed files in case this is a library override ID. */
    if (ID_IS_OVERRIDE_LIBRARY(ima)) {
      BLI_listbase_clear(&ima->packedfiles);
    }
    else {
      /* Some trickery to keep forward compatibility of packed images. */
      if (ima->packedfiles.first != nullptr) {
        imapf = static_cast<ImagePackedFile *>(ima->packedfiles.first);
        ima->packedfile = imapf->packedfile;
      }
    }
  }

  /* write LibData */
  BLO_write_id_struct(writer, Image, id_address, &ima->id);
  BKE_id_blend_write(writer, &ima->id);

  LISTBASE_FOREACH (ImagePackedFile *, imapf, &ima->packedfiles) {
    BLO_write_struct(writer, ImagePackedFile, imapf);
    BKE_packedfile_blend_write(writer, imapf->packedfile);
  }

  BKE_previewimg_blend_write(writer, ima->preview);

  LISTBASE_FOREACH (ImageView *, iv, &ima->views) {
    BLO_write_struct(writer, ImageView, iv);
  }
  BLO_write_struct(writer, Stereo3dFormat, ima->stereo3d_format);

  BLO_write_struct_list(writer, ImageTile, &ima->tiles);

  ima->packedfile = nullptr;

  BLO_write_struct_list(writer, RenderSlot, &ima->renderslots);
}

static void image_blend_read_data(BlendDataReader *reader, ID *id)
{
  Image *ima = (Image *)id;
  BLO_read_struct_list(reader, ImageTile, &ima->tiles);

  BLO_read_struct_list(reader, RenderSlot, &(ima->renderslots));
  if (!BLO_read_data_is_undo(reader)) {
    /* We reset this last render slot index only when actually reading a file, not for undo. */
    ima->last_render_slot = ima->render_slot;
  }

  BLO_read_struct_list(reader, ImageView, &(ima->views));
  BLO_read_struct_list(reader, ImagePackedFile, &(ima->packedfiles));

  if (ima->packedfiles.first) {
    LISTBASE_FOREACH_MUTABLE (ImagePackedFile *, imapf, &ima->packedfiles) {
      BKE_packedfile_blend_read(reader, &imapf->packedfile, imapf->filepath);
      if (!imapf->packedfile) {
        BLI_remlink(&ima->packedfiles, imapf);
        MEM_freeN(imapf);
      }
    }
    ima->packedfile = nullptr;
  }
  else {
    BKE_packedfile_blend_read(reader, &ima->packedfile, ima->filepath);
  }

  BLI_listbase_clear(&ima->anims);
  BLO_read_struct(reader, PreviewImage, &ima->preview);
  BKE_previewimg_blend_read(reader, ima->preview);
  BLO_read_struct(reader, Stereo3dFormat, &ima->stereo3d_format);

  image_gpu_runtime_reset(ima);
  ima->runtime = MEM_new<blender::bke::ImageRuntime>(__func__);
}

static void image_blend_read_after_liblink(BlendLibReader * /*reader*/, ID *id)
{
  Image *ima = reinterpret_cast<Image *>(id);

  /* Images have some kind of 'main' cache, when null we should also clear all others. */
  /* Needs to be done *after* cache pointers are restored (call to
   * `foreach_cache`/`blo_cache_storage_entry_restore_in_new`), easier for now to do it in
   * lib_link... */
  if (ima->cache == nullptr) {
    BKE_image_free_buffers(ima);
  }
}

constexpr IDTypeInfo get_type_info()
{
  IDTypeInfo info{};
  info.id_code = ID_IM;
  info.id_filter = FILTER_ID_IM;
  info.dependencies_id_types = 0;
  info.main_listbase_index = INDEX_ID_IM;
  info.struct_size = sizeof(Image);
  info.name = "Image";
  info.name_plural = "images";
  info.translation_context = BLT_I18NCONTEXT_ID_IMAGE;
  info.flags = IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE;
  info.asset_type_info = nullptr;

  info.init_data = image_init_data;
  info.copy_data = image_copy_data;
  info.free_data = image_free_data;
  info.make_local = nullptr;
  info.foreach_id = nullptr;
  info.foreach_cache = image_foreach_cache;
  info.foreach_path = image_foreach_path;
  info.owner_pointer_get = nullptr;

  info.blend_write = image_blend_write;
  info.blend_read_data = image_blend_read_data;
  info.blend_read_after_liblink = image_blend_read_after_liblink;

  info.blend_read_undo_preserve = nullptr;

  info.lib_override_apply_post = nullptr;
  return info;
}
IDTypeInfo IDType_ID_IM = get_type_info();

/* prototypes */
static int image_num_viewfiles(Image *ima);
static ImBuf *image_load_image_file(
    Image *ima, ImageUser *iuser, int entry, int cfra, bool is_sequence);
static ImBuf *image_acquire_ibuf(Image *ima, ImageUser *iuser, void **r_lock);
static void image_update_views_format(Image *ima, ImageUser *iuser);
static void image_add_view(Image *ima, const char *viewname, const char *filepath);

/* max int, to indicate we don't store sequences in ibuf */
#define IMA_NO_INDEX 0x7FEFEFEF

/* quick lookup: supports 1 million entries, thousand passes */
#define IMA_MAKE_INDEX(entry, index) (((entry) << 10) + (index))
#define IMA_INDEX_ENTRY(index) ((index) >> 10)
#if 0
#  define IMA_INDEX_PASS(index) (index & ~1023)
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Cache
 * \{ */

struct ImageCacheKey {
  int index;
};

static uint imagecache_hashhash(const void *key_v)
{
  const ImageCacheKey *key = static_cast<const ImageCacheKey *>(key_v);
  return key->index;
}

static bool imagecache_hashcmp(const void *a_v, const void *b_v)
{
  const ImageCacheKey *a = static_cast<const ImageCacheKey *>(a_v);
  const ImageCacheKey *b = static_cast<const ImageCacheKey *>(b_v);

  return (a->index != b->index);
}

static void imagecache_keydata(void *userkey, int *framenr, int *proxy, int *render_flags)
{
  ImageCacheKey *key = static_cast<ImageCacheKey *>(userkey);

  *framenr = IMA_INDEX_ENTRY(key->index);
  *proxy = IMB_PROXY_NONE;
  *render_flags = 0;
}

static void imagecache_put(Image *image, int index, ImBuf *ibuf)
{
  ImageCacheKey key;

  if (image->cache == nullptr) {
    // char cache_name[64];
    // SNPRINTF_UTF8(cache_name, "Image Datablock %s", image->id.name);

    image->cache = IMB_moviecache_create(
        "Image Datablock Cache", sizeof(ImageCacheKey), imagecache_hashhash, imagecache_hashcmp);
    IMB_moviecache_set_getdata_callback(image->cache, imagecache_keydata);
  }

  key.index = index;

  IMB_moviecache_put(image->cache, &key, ibuf);
}

static void imagecache_remove(Image *image, int index)
{
  if (image->cache == nullptr) {
    return;
  }

  ImageCacheKey key;
  key.index = index;
  IMB_moviecache_remove(image->cache, &key);
}

static ImBuf *imagecache_get(Image *image, int index, bool *r_is_cached_empty)
{
  if (image->cache) {
    ImageCacheKey key;
    key.index = index;
    return IMB_moviecache_get(image->cache, &key, r_is_cached_empty);
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Allocate & Free, Data Managing
 * \{ */

static void image_free_cached_frames(Image *image)
{
  if (image->cache) {
    IMB_moviecache_free(image->cache);
    image->cache = nullptr;
  }
}

static void image_free_packedfiles(Image *ima)
{
  while (ima->packedfiles.last) {
    ImagePackedFile *imapf = static_cast<ImagePackedFile *>(ima->packedfiles.last);
    if (imapf->packedfile) {
      BKE_packedfile_free(imapf->packedfile);
    }
    BLI_remlink(&ima->packedfiles, imapf);
    MEM_freeN(imapf);
  }
}

void BKE_image_free_packedfiles(Image *ima)
{
  image_free_packedfiles(ima);
}

void BKE_image_free_views(Image *image)
{
  BLI_freelistN(&image->views);
}

static void image_free_anims(Image *ima)
{
  while (ima->anims.last) {
    ImageAnim *ia = static_cast<ImageAnim *>(ima->anims.last);
    if (ia->anim) {
      MOV_close(ia->anim);
      ia->anim = nullptr;
    }
    BLI_remlink(&ima->anims, ia);
    MEM_freeN(ia);
  }
}

void BKE_image_free_buffers_ex(Image *ima, bool do_lock)
{
  if (do_lock) {
    ima->runtime->cache_mutex.lock();
  }
  image_free_cached_frames(ima);

  image_free_anims(ima);

  if (ima->rr) {
    RE_FreeRenderResult(ima->rr);
    ima->rr = nullptr;
  }

  BKE_image_free_gputextures(ima);

  if (do_lock) {
    ima->runtime->cache_mutex.unlock();
  }
}

void BKE_image_free_buffers(Image *ima)
{
  BKE_image_free_buffers_ex(ima, false);
}

void BKE_image_free_data(Image *ima)
{
  image_free_data(&ima->id);
}

static ImageTile *imagetile_alloc(int tile_number)
{
  ImageTile *tile = MEM_callocN<ImageTile>("Image Tile");
  tile->tile_number = tile_number;
  tile->gen_x = 1024;
  tile->gen_y = 1024;
  tile->gen_type = IMA_GENTYPE_GRID;
  return tile;
}

/* only image block itself */
static void image_init(Image *ima, short source, short type)
{
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(ima, id));

  MEMCPY_STRUCT_AFTER(ima, DNA_struct_default_get(Image), id);

  ima->source = source;
  ima->type = type;

  if (source == IMA_SRC_VIEWER) {
    ima->flag |= IMA_VIEW_AS_RENDER;
  }

  ImageTile *tile = imagetile_alloc(1001);
  BLI_addtail(&ima->tiles, tile);

  if (type == IMA_TYPE_R_RESULT) {
    for (int i = 0; i < 8; i++) {
      BKE_image_add_renderslot(ima, nullptr);
    }
  }

  ima->runtime = MEM_new<blender::bke::ImageRuntime>(__func__);

  BKE_color_managed_colorspace_settings_init(&ima->colorspace_settings);
  ima->stereo3d_format = MEM_callocN<Stereo3dFormat>("Image Stereo Format");
}

static Image *image_alloc(Main *bmain,
                          std::optional<Library *> owner_library,
                          const char *name,
                          short source,
                          short type)
{
  Image *ima;

  ima = static_cast<Image *>(BKE_libblock_alloc_in_lib(bmain, owner_library, ID_IM, name, 0));
  if (ima) {
    image_init(ima, source, type);
  }

  return ima;
}

/**
 * Get the ibuf from an image cache by its index and entry.
 * Local use here only.
 *
 * \returns referenced image buffer if it exists, callee is to call #IMB_freeImBuf
 * to de-reference the image buffer after it's done handling it.
 */
static ImBuf *image_get_cached_ibuf_for_index_entry(Image *ima,
                                                    int index,
                                                    int entry,
                                                    bool *r_is_cached_empty)
{
  if (index != IMA_NO_INDEX) {
    index = IMA_MAKE_INDEX(entry, index);
  }

  return imagecache_get(ima, index, r_is_cached_empty);
}

static void image_assign_ibuf(Image *ima, ImBuf *ibuf, int index, int entry)
{
  if (index != IMA_NO_INDEX) {
    index = IMA_MAKE_INDEX(entry, index);
  }

  imagecache_put(ima, index, ibuf);
}

static void image_remove_ibuf(Image *ima, int index, int entry)
{
  if (index != IMA_NO_INDEX) {
    index = IMA_MAKE_INDEX(entry, index);
  }
  imagecache_remove(ima, index);
}

static void copy_image_packedfiles(ListBase *lb_dst, const ListBase *lb_src)
{
  const ImagePackedFile *imapf_src;

  BLI_listbase_clear(lb_dst);
  for (imapf_src = static_cast<const ImagePackedFile *>(lb_src->first); imapf_src;
       imapf_src = imapf_src->next)
  {
    ImagePackedFile *imapf_dst = MEM_mallocN<ImagePackedFile>("Image Packed Files (copy)");

    imapf_dst->view = imapf_src->view;
    imapf_dst->tile_number = imapf_src->tile_number;
    STRNCPY(imapf_dst->filepath, imapf_src->filepath);

    if (imapf_src->packedfile) {
      imapf_dst->packedfile = BKE_packedfile_duplicate(imapf_src->packedfile);
    }

    BLI_addtail(lb_dst, imapf_dst);
  }
}

void BKE_image_merge(Main *bmain, Image *dest, Image *source)
{
  /* sanity check */
  if (dest && source && dest != source) {
    {
      std::scoped_lock lock_src(source->runtime->cache_mutex);
      std::scoped_lock lock_dst(dest->runtime->cache_mutex);

      if (source->cache != nullptr) {
        MovieCacheIter *iter;
        iter = IMB_moviecacheIter_new(source->cache);
        while (!IMB_moviecacheIter_done(iter)) {
          ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
          ImageCacheKey *key = static_cast<ImageCacheKey *>(IMB_moviecacheIter_getUserKey(iter));
          imagecache_put(dest, key->index, ibuf);
          IMB_moviecacheIter_step(iter);
        }
        IMB_moviecacheIter_free(iter);
      }
    }
    BKE_id_free(bmain, source);
  }
}

bool BKE_image_scale(Image *image, int width, int height, ImageUser *iuser)
{
  /* NOTE: We could be clever and scale all imbuf's
   * but since some are mipmaps its not so simple. */

  ImBuf *ibuf;
  void *lock;

  ibuf = BKE_image_acquire_ibuf(image, iuser, &lock);

  if (ibuf) {
    IMB_scale(ibuf, width, height, IMBScaleFilter::Box, false);
    BKE_image_mark_dirty(image, ibuf);
  }

  BKE_image_release_ibuf(image, ibuf, lock);

  return (ibuf != nullptr);
}

bool BKE_image_has_opengl_texture(Image *ima)
{
  for (int eye = 0; eye < 2; eye++) {
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      if (ima->gputexture[i][eye] != nullptr) {
        return true;
      }
    }
  }
  return false;
}

static int image_get_tile_number_from_iuser(const Image *ima, const ImageUser *iuser)
{
  BLI_assert(ima != nullptr && ima->tiles.first);
  ImageTile *tile = static_cast<ImageTile *>(ima->tiles.first);
  return (iuser && iuser->tile) ? iuser->tile : tile->tile_number;
}

ImageTile *BKE_image_get_tile(Image *ima, int tile_number)
{
  if (ima == nullptr) {
    return nullptr;
  }

  /* Tiles 0 and 1001 are a special case and refer to the first tile, typically
   * coming from non-UDIM-aware code. */
  if (ELEM(tile_number, 0, 1001)) {
    return static_cast<ImageTile *>(ima->tiles.first);
  }

  /* Must have a tiled image and a valid tile number at this point. */
  if (ima->source != IMA_SRC_TILED || tile_number < 1001 || tile_number > IMA_UDIM_MAX) {
    return nullptr;
  }

  LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
    if (tile->tile_number == tile_number) {
      return tile;
    }
  }

  return nullptr;
}

ImageTile *BKE_image_get_tile_from_iuser(Image *ima, const ImageUser *iuser)
{
  return BKE_image_get_tile(ima, image_get_tile_number_from_iuser(ima, iuser));
}

int BKE_image_get_tile_from_pos(Image *ima, const float uv[2], float r_uv[2], float r_ofs[2])
{
  float local_ofs[2];
  if (r_ofs == nullptr) {
    r_ofs = local_ofs;
  }

  copy_v2_v2(r_uv, uv);
  zero_v2(r_ofs);

  if ((ima->source != IMA_SRC_TILED) || uv[0] < 0.0f || uv[1] < 0.0f || uv[0] >= 10.0f) {
    return 0;
  }

  int ix = int(uv[0]);
  int iy = int(uv[1]);
  int tile_number = 1001 + 10 * iy + ix;

  if (BKE_image_get_tile(ima, tile_number) == nullptr) {
    return 0;
  }
  r_ofs[0] = ix;
  r_ofs[1] = iy;
  sub_v2_v2(r_uv, r_ofs);

  return tile_number;
}

void BKE_image_get_tile_uv(const Image *ima, const int tile_number, float r_uv[2])
{
  if (ima->source != IMA_SRC_TILED) {
    zero_v2(r_uv);
  }
  else {
    const int tile_index = tile_number - 1001;
    r_uv[0] = float(tile_index % 10);
    r_uv[1] = float(tile_index / 10);
  }
}

/** Linear distance between #x and the unit interval. */
static float distance_to_unit_interval(float x)
{
  /* The unit interval is between 0 and 1.
   * Within the interval, return 0.
   * Outside the interval, return the distance to the nearest boundary.
   * Intuitively, the function looks like:
   *  \ |   | /
   * __\|___|/__
   *    0   1
   */

  if (x <= 0.0f) {
    return -x; /* Distance to left border. */
  }
  if (x <= 1.0f) {
    return 0.0f; /* Inside unit interval. */
  }
  return x - 1.0f; /* Distance to right border. */
}

/** Distance squared between #co and the unit square with lower-left starting at #udim. */
static float distance_squared_to_udim(const float co[2], const float udim[2])
{
  float delta[2];
  sub_v2_v2v2(delta, co, udim);
  delta[0] = distance_to_unit_interval(delta[0]);
  delta[1] = distance_to_unit_interval(delta[1]);
  return len_squared_v2(delta);
}

static bool nearest_udim_tile_tie_break(const float best_dist_sq,
                                        const float best_uv[2],
                                        const float dist_sq,
                                        const float uv[2])
{
  if (best_dist_sq == dist_sq) {   /* Exact same distance? Tie-break. */
    if (best_uv[0] == uv[0]) {     /* Exact same U? Tie-break. */
      return (uv[1] > best_uv[1]); /* Higher than previous candidate? */
    }
    return (uv[0] > best_uv[0]); /* Further right than previous candidate? */
  }
  return (dist_sq < best_dist_sq); /* Closer than previous candidate? */
}

int BKE_image_find_nearest_tile_with_offset(const Image *image,
                                            const float co[2],
                                            float r_uv_offset[2])
{
  /* NOTE: If the coordinates are integers, take special care to break ties. */

  zero_v2(r_uv_offset);
  int tile_number_best = -1;

  if (!image || image->source != IMA_SRC_TILED) {
    return tile_number_best;
  }

  /* Distance squared to the closest UDIM tile. */
  float dist_best_sq = FLT_MAX;

  LISTBASE_FOREACH (const ImageTile *, tile, &image->tiles) {
    float uv_offset[2];
    BKE_image_get_tile_uv(image, tile->tile_number, uv_offset);

    /* Distance squared between #co and closest point on UDIM tile. */
    const float dist_sq = distance_squared_to_udim(co, uv_offset);

    if (dist_sq == 0) { /* Either inside in the UDIM, or on its boundary. */
      if (floorf(co[0]) == uv_offset[0] && floorf(co[1]) == uv_offset[1]) {
        /* Within the half-open interval of the UDIM. */
        copy_v2_v2(r_uv_offset, uv_offset);
        return tile_number_best;
      }
    }

    if (nearest_udim_tile_tie_break(dist_best_sq, r_uv_offset, dist_sq, uv_offset)) {
      /* Tile is better than previous best, update. */
      dist_best_sq = dist_sq;
      copy_v2_v2(r_uv_offset, uv_offset);
      tile_number_best = tile->tile_number;
    }
  }
  return tile_number_best;
}

int BKE_image_find_nearest_tile(const Image *image, const float co[2])
{
  float uv_offset_dummy[2];
  return BKE_image_find_nearest_tile_with_offset(image, co, uv_offset_dummy);
}

static void image_init_color_management(Image *ima)
{
  ImBuf *ibuf;
  char filepath[FILE_MAX];

  BKE_image_user_file_path(nullptr, ima, filepath);

  /* Will set input color space to image format default's. */
  ibuf = IMB_load_image_from_filepath(
      filepath, IB_test | IB_alphamode_detect, ima->colorspace_settings.name);

  if (ibuf) {
    if (ibuf->flags & IB_alphamode_premul) {
      ima->alpha_mode = IMA_ALPHA_PREMUL;
    }
    else if (ibuf->flags & IB_alphamode_channel_packed) {
      ima->alpha_mode = IMA_ALPHA_CHANNEL_PACKED;
    }
    else if (ibuf->flags & IB_alphamode_ignore) {
      ima->alpha_mode = IMA_ALPHA_IGNORE;
    }
    else {
      ima->alpha_mode = IMA_ALPHA_STRAIGHT;
    }

    IMB_freeImBuf(ibuf);
  }
}

char BKE_image_alpha_mode_from_extension_ex(const char *filepath)
{
  if (BLI_path_extension_check_n(filepath, ".exr", ".cin", ".dpx", ".hdr", nullptr)) {
    return IMA_ALPHA_PREMUL;
  }

  return IMA_ALPHA_STRAIGHT;
}

void BKE_image_alpha_mode_from_extension(Image *image)
{
  image->alpha_mode = BKE_image_alpha_mode_from_extension_ex(image->filepath);
}

static void image_abs_path(Main *bmain,
                           Library *owner_library,
                           const char *filepath,
                           char *r_filepath_abs)
{
  BLI_strncpy(r_filepath_abs, filepath, FILE_MAX);
  if (owner_library) {
    BLI_path_abs(r_filepath_abs, owner_library->runtime->filepath_abs);
  }
  else {
    BLI_path_abs(r_filepath_abs, BKE_main_blendfile_path(bmain));
  }
}

Image *BKE_image_load(Main *bmain, const char *filepath)
{
  return BKE_image_load_in_lib(bmain, std::nullopt, filepath);
}

Image *BKE_image_load_in_lib(Main *bmain,
                             std::optional<Library *> owner_library,
                             const char *filepath)
{
  Image *ima;
  int file;
  char filepath_abs[FILE_MAX];

  /* If no owner library is explicitly given, use the current library from the given #Main. */
  Library *owner_lib = owner_library.value_or(bmain->curlib);

  image_abs_path(bmain, owner_lib, filepath, filepath_abs);

  /* exists? */
  file = BLI_open(filepath_abs, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    if (!BKE_image_tile_filepath_exists(filepath_abs)) {
      return nullptr;
    }
  }
  else {
    close(file);
  }

  ima = image_alloc(
      bmain, owner_library, BLI_path_basename(filepath), IMA_SRC_FILE, IMA_TYPE_IMAGE);
  STRNCPY(ima->filepath, filepath);

  if (BLI_path_extension_check_array(filepath, imb_ext_movie)) {
    ima->source = IMA_SRC_MOVIE;
  }

  image_init_color_management(ima);

  return ima;
}

Image *BKE_image_load_exists(Main *bmain, const char *filepath, bool *r_exists)
{
  return BKE_image_load_exists_in_lib(bmain, std::nullopt, filepath, r_exists);
}

Image *BKE_image_load_exists_in_lib(Main *bmain,
                                    std::optional<Library *> owner_library,
                                    const char *filepath,
                                    bool *r_exists)
{
  Image *ima;
  char filepath_abs[FILE_MAX], filepath_test[FILE_MAX];

  /* If not owner library is explicitly given, use the current library from the given Main. */
  Library *owner_lib = owner_library.value_or(bmain->curlib);

  image_abs_path(bmain, owner_lib, filepath, filepath_abs);

  /* First search an identical filepath. */
  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    if (!ELEM(ima->source, IMA_SRC_VIEWER, IMA_SRC_GENERATED)) {
      STRNCPY(filepath_test, ima->filepath);
      BLI_path_abs(filepath_test, ID_BLEND_PATH(bmain, &ima->id));

      if (BLI_path_cmp(filepath_test, filepath_abs) != 0) {
        continue;
      }
      if (BKE_image_has_anim(ima) && (ima->id.us != 0)) {
        /* TODO explain why animated images with already one or more users are skipped? */
        continue;
      }
      if (ima->id.lib != owner_lib) {
        continue;
      }
      /* FIXME Should not happen here. Only code actually adding an ID usage should increment this
       * counter. */
      id_us_plus(&ima->id);
      if (r_exists) {
        *r_exists = true;
      }
      return ima;
    }
  }

  if (r_exists) {
    *r_exists = false;
  }
  return BKE_image_load_in_lib(bmain, owner_library, filepath);
}

struct ImageFillData {
  short gen_type;
  uint width;
  uint height;
  uchar *rect;
  float *rect_float;
  float fill_color[4];
};

static void image_buf_fill_isolated(void *usersata_v)
{
  ImageFillData *usersata = static_cast<ImageFillData *>(usersata_v);

  const short gen_type = usersata->gen_type;
  const uint width = usersata->width;
  const uint height = usersata->height;

  uchar *rect = usersata->rect;
  float *rect_float = usersata->rect_float;

  switch (gen_type) {
    case IMA_GENTYPE_GRID:
      BKE_image_buf_fill_checker(rect, rect_float, width, height);
      break;
    case IMA_GENTYPE_GRID_COLOR:
      BKE_image_buf_fill_checker_color(rect, rect_float, width, height);
      break;
    default:
      BKE_image_buf_fill_color(rect, rect_float, width, height, usersata->fill_color);
      break;
  }
}

static ImBuf *add_ibuf_for_tile(Image *ima, ImageTile *tile)
{
  ImBuf *ibuf;
  uchar *rect = nullptr;
  float *rect_float = nullptr;
  float fill_color[4];

  const bool floatbuf = (tile->gen_flag & IMA_GEN_FLOAT) != 0;
  if (floatbuf) {
    ibuf = IMB_allocImBuf(tile->gen_x, tile->gen_y, tile->gen_depth, IB_float_data);

    if (ima->colorspace_settings.name[0] == '\0') {
      const char *colorspace = IMB_colormanagement_role_colorspace_name_get(
          COLOR_ROLE_DEFAULT_FLOAT);

      STRNCPY_UTF8(ima->colorspace_settings.name, colorspace);
    }

    if (ibuf != nullptr) {
      rect_float = ibuf->float_buffer.data;
      IMB_colormanagement_check_is_data(ibuf, ima->colorspace_settings.name);
    }

    if (IMB_colormanagement_space_name_is_data(ima->colorspace_settings.name)) {
      copy_v4_v4(fill_color, tile->gen_color);
    }
    else {
      /* The input color here should ideally be linear already, but for now
       * we just convert and postpone breaking the API for later. */
      srgb_to_linearrgb_v4(fill_color, tile->gen_color);
    }
  }
  else {
    ibuf = IMB_allocImBuf(tile->gen_x, tile->gen_y, tile->gen_depth, IB_byte_data);

    if (ima->colorspace_settings.name[0] == '\0') {
      const char *colorspace = IMB_colormanagement_role_colorspace_name_get(
          COLOR_ROLE_DEFAULT_BYTE);

      STRNCPY_UTF8(ima->colorspace_settings.name, colorspace);
    }

    if (ibuf != nullptr) {
      rect = ibuf->byte_buffer.data;
      IMB_colormanagement_assign_byte_colorspace(ibuf, ima->colorspace_settings.name);
    }

    copy_v4_v4(fill_color, tile->gen_color);
  }

  if (!ibuf) {
    return nullptr;
  }

  STRNCPY(ibuf->filepath, ima->filepath);
  BLI_path_abs(ibuf->filepath, ID_BLEND_PATH_FROM_GLOBAL(&ima->id));

  /* Mark the tile itself as having been generated. */
  tile->gen_flag |= IMA_GEN_TILE;

  ImageFillData data;

  data.gen_type = tile->gen_type;
  data.width = tile->gen_x;
  data.height = tile->gen_y;
  data.rect = rect;
  data.rect_float = rect_float;
  copy_v4_v4(data.fill_color, fill_color);

  BLI_task_isolate(image_buf_fill_isolated, &data);

  return ibuf;
}

Image *BKE_image_add_generated(Main *bmain,
                               uint width,
                               uint height,
                               const char *name,
                               int depth,
                               int floatbuf,
                               short gen_type,
                               const float color[4],
                               const bool stereo3d,
                               const bool is_data,
                               const bool tiled)
{
  /* Saving the image changes it's #Image.source to #IMA_SRC_FILE (leave as generated here). */
  Image *ima;
  if (tiled) {
    ima = image_alloc(bmain, std::nullopt, name, IMA_SRC_TILED, IMA_TYPE_IMAGE);
  }
  else {
    ima = image_alloc(bmain, std::nullopt, name, IMA_SRC_GENERATED, IMA_TYPE_UV_TEST);
  }
  if (ima == nullptr) {
    return nullptr;
  }

  int view_id;
  const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

  /* NOTE: leave `ima->filepath` unset,
   * setting it to a dummy value may write to an invalid file-path. */

  /* The generation info is always stored in the tiles. The first tile
   * will be used for non-tiled images. */
  ImageTile *tile = static_cast<ImageTile *>(ima->tiles.first);
  tile->gen_x = width;
  tile->gen_y = height;
  tile->gen_type = gen_type;
  tile->gen_flag |= (floatbuf ? IMA_GEN_FLOAT : 0);
  tile->gen_depth = depth;
  copy_v4_v4(tile->gen_color, color);

  if (is_data) {
    STRNCPY_UTF8(ima->colorspace_settings.name,
                 IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DATA));
  }

  for (view_id = 0; view_id < 2; view_id++) {
    ImBuf *ibuf;
    ibuf = add_ibuf_for_tile(ima, tile);
    int index = tiled ? 0 : IMA_NO_INDEX;
    int entry = tiled ? 1001 : 0;
    image_assign_ibuf(ima, ibuf, stereo3d ? view_id : index, entry);

    /* #image_assign_ibuf puts buffer to the cache, which increments user counter. */
    IMB_freeImBuf(ibuf);
    if (!stereo3d) {
      break;
    }

    image_add_view(ima, names[view_id], "");
  }

  return ima;
}

static void image_colorspace_from_imbuf(Image *image, const ImBuf *ibuf)
{
  const char *colorspace_name = nullptr;

  if (ibuf->float_buffer.data) {
    if (ibuf->float_buffer.colorspace) {
      colorspace_name = IMB_colormanagement_colorspace_get_name(ibuf->float_buffer.colorspace);
    }
    else {
      colorspace_name = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_FLOAT);
    }
  }

  if (ibuf->byte_buffer.data && !colorspace_name) {
    if (ibuf->byte_buffer.colorspace) {
      colorspace_name = IMB_colormanagement_colorspace_get_name(ibuf->byte_buffer.colorspace);
    }
    else {
      colorspace_name = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);
    }
  }

  if (colorspace_name) {
    STRNCPY_UTF8(image->colorspace_settings.name, colorspace_name);
  }
}

Image *BKE_image_add_from_imbuf(Main *bmain, ImBuf *ibuf, const char *name)
{
  if (name == nullptr) {
    name = BLI_path_basename(ibuf->filepath);
  }

  /* When the image buffer has valid path create a new image with "file" source and copy the path
   * from the image buffer.
   * Otherwise create "generated" image, avoiding invalid configuration with an empty file path. */
  const eImageSource source = ibuf->filepath[0] != '\0' ? IMA_SRC_FILE : IMA_SRC_GENERATED;

  Image *ima = image_alloc(bmain, std::nullopt, name, source, IMA_TYPE_IMAGE);

  if (!ima) {
    return nullptr;
  }

  BKE_image_replace_imbuf(ima, ibuf);

  return ima;
}

void BKE_image_replace_imbuf(Image *image, ImBuf *ibuf)
{
  BLI_assert(image->type == IMA_TYPE_IMAGE &&
             ELEM(image->source, IMA_SRC_FILE, IMA_SRC_GENERATED));

  BKE_image_free_buffers(image);

  image_assign_ibuf(image, ibuf, IMA_NO_INDEX, 0);
  image_colorspace_from_imbuf(image, ibuf);

  /* Keep generated image type flags consistent with the image buffer. */
  if (image->source == IMA_SRC_GENERATED) {
    if (ibuf->float_buffer.data) {
      image->gen_flag |= IMA_GEN_FLOAT;
    }
    else {
      image->gen_flag &= ~IMA_GEN_FLOAT;
    }

    image->gen_x = ibuf->x;
    image->gen_y = ibuf->y;
  }

  /* Consider image dirty since its content can not be re-created unless the image is explicitly
   * saved. */
  BKE_image_mark_dirty(image, ibuf);
}

/** Pack image buffer to memory as PNG or EXR. */
static bool image_memorypack_imbuf(
    Image *ima, ImBuf *ibuf, int view, int tile_number, const char *filepath)
{
  ibuf->ftype = (ibuf->float_buffer.data) ? IMB_FTYPE_OPENEXR : IMB_FTYPE_PNG;

  IMB_save_image(ibuf, filepath, IB_byte_data | IB_mem);

  if (ibuf->encoded_buffer.data == nullptr) {
    CLOG_STR_ERROR(&LOG, "memory save for pack error");
    IMB_freeImBuf(ibuf);
    image_free_packedfiles(ima);
    return false;
  }

  ImagePackedFile *imapf;
  const int encoded_size = ibuf->encoded_size;
  PackedFile *pf = BKE_packedfile_new_from_memory(IMB_steal_encoded_buffer(ibuf), encoded_size);

  imapf = MEM_mallocN<ImagePackedFile>("Image PackedFile");
  STRNCPY(imapf->filepath, filepath);
  imapf->packedfile = pf;
  imapf->view = view;
  imapf->tile_number = tile_number;
  BLI_addtail(&ima->packedfiles, imapf);

  ibuf->userflags &= ~IB_BITMAPDIRTY;

  return true;
}

bool BKE_image_memorypack(Image *ima)
{
  bool ok = true;

  image_free_packedfiles(ima);

  const int tot_viewfiles = image_num_viewfiles(ima);
  const bool is_tiled = (ima->source == IMA_SRC_TILED);
  const bool is_multiview = BKE_image_is_multiview(ima);

  ImageUser iuser{};
  BKE_imageuser_default(&iuser);
  char tiled_filepath[FILE_MAX];

  for (int view = 0; view < tot_viewfiles; view++) {
    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      int index = (is_multiview || is_tiled) ? view : IMA_NO_INDEX;
      int entry = is_tiled ? tile->tile_number : 0;
      ImBuf *ibuf = image_get_cached_ibuf_for_index_entry(ima, index, entry, nullptr);
      if (!ibuf) {
        ok = false;
        break;
      }

      const char *filepath = ibuf->filepath;
      if (is_tiled) {
        iuser.tile = tile->tile_number;
        BKE_image_user_file_path(&iuser, ima, tiled_filepath);
        filepath = tiled_filepath;
      }
      else if (is_multiview) {
        ImageView *iv = static_cast<ImageView *>(BLI_findlink(&ima->views, view));
        /* if the image was a R_IMF_VIEWS_STEREO_3D we force _L, _R suffices */
        if (ima->views_format == R_IMF_VIEWS_STEREO_3D) {
          const char *suffix[2] = {STEREO_LEFT_SUFFIX, STEREO_RIGHT_SUFFIX};
          BLI_path_suffix(iv->filepath, FILE_MAX, suffix[view], "");
        }
        filepath = iv->filepath;
      }

      ok = ok && image_memorypack_imbuf(ima, ibuf, view, tile->tile_number, filepath);
      IMB_freeImBuf(ibuf);
    }
  }

  if (is_multiview) {
    ima->views_format = R_IMF_VIEWS_INDIVIDUAL;
  }

  /* Images which were "generated" before packing should now be
   * treated as if they were saved as real files. */
  if (ok) {
    if (ima->source == IMA_SRC_GENERATED) {
      ima->source = IMA_SRC_FILE;
      ima->type = IMA_TYPE_IMAGE;
    }

    /* Clear the per-tile generated flag if all tiles were ok.
     * Mirrors similar processing inside #BKE_image_save. */
    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      tile->gen_flag &= ~IMA_GEN_TILE;
    }
  }

  return ok;
}

void BKE_image_packfiles(ReportList *reports, Image *ima, const char *basepath)
{
  const int tot_viewfiles = image_num_viewfiles(ima);

  ImageUser iuser{};
  BKE_imageuser_default(&iuser);
  for (int view = 0; view < tot_viewfiles; view++) {
    iuser.view = view;
    LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
      iuser.tile = tile->tile_number;
      char filepath[FILE_MAX];
      BKE_image_user_file_path(&iuser, ima, filepath);

      ImagePackedFile *imapf = MEM_mallocN<ImagePackedFile>("Image packed file");
      BLI_addtail(&ima->packedfiles, imapf);

      imapf->packedfile = BKE_packedfile_new(reports, filepath, basepath);
      imapf->view = view;
      imapf->tile_number = tile->tile_number;
      if (imapf->packedfile) {
        STRNCPY(imapf->filepath, filepath);
      }
      else {
        BLI_freelinkN(&ima->packedfiles, imapf);
      }
    }
  }
}

void BKE_image_packfiles_from_mem(ReportList *reports,
                                  Image *ima,
                                  char *data,
                                  const size_t data_len)
{
  const int tot_viewfiles = image_num_viewfiles(ima);

  if (tot_viewfiles != 1) {
    BKE_report(reports, RPT_ERROR, "Cannot pack multiview images from raw data currently...");
  }
  else if (ima->source == IMA_SRC_TILED) {
    BKE_report(reports, RPT_ERROR, "Cannot pack tiled images from raw data currently...");
  }
  else {
    ImagePackedFile *imapf = MEM_mallocN<ImagePackedFile>(__func__);
    BLI_addtail(&ima->packedfiles, imapf);
    imapf->packedfile = BKE_packedfile_new_from_memory(data, data_len);
    imapf->view = 0;
    imapf->tile_number = 1001;
    STRNCPY(imapf->filepath, ima->filepath);

    /* The image should not be marked as "generated" since image data was provided. */
    ImageTile *base_tile = BKE_image_get_tile(ima, 0);
    base_tile->gen_flag &= ~IMA_GEN_TILE;
  }
}

void BKE_image_tag_time(Image *ima)
{
  ima->lastused = BLI_time_now_seconds_i();
}

static uintptr_t image_mem_size(Image *image)
{
  uintptr_t size = 0;

  /* viewers have memory depending on other rules, has no valid rect pointer */
  if (image->source == IMA_SRC_VIEWER) {
    return 0;
  }

  std::scoped_lock lock(image->runtime->cache_mutex);

  if (image->cache != nullptr) {
    MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

    while (!IMB_moviecacheIter_done(iter)) {
      ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
      IMB_moviecacheIter_step(iter);
      if (ibuf == nullptr) {
        continue;
      }

      size += IMB_get_size_in_memory(ibuf);
    }
    IMB_moviecacheIter_free(iter);
  }

  return size;
}

void BKE_image_print_memlist(Main *bmain)
{
  Image *ima;
  uintptr_t size, totsize = 0;

  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    totsize += image_mem_size(ima);
  }

  printf("\ntotal image memory len: %.3f MB\n", double(totsize) / double(1024 * 1024));

  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    size = image_mem_size(ima);

    if (size) {
      printf("%s len: %.3f MB\n", ima->id.name + 2, double(size) / double(1024 * 1024));
    }
  }
}

static bool imagecache_check_dirty(ImBuf *ibuf, void * /*userkey*/, void * /*userdata*/)
{
  if (ibuf == nullptr) {
    return false;
  }
  return (ibuf->userflags & IB_BITMAPDIRTY) == 0;
}

void BKE_image_free_all_textures(Main *bmain)
{
#undef CHECK_FREED_SIZE

  Tex *tex;
  Image *ima;
#ifdef CHECK_FREED_SIZE
  uintptr_t tot_freed_size = 0;
#endif

  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    ima->id.tag &= ~ID_TAG_DOIT;
  }

  for (tex = static_cast<Tex *>(bmain->textures.first); tex;
       tex = static_cast<Tex *>(tex->id.next))
  {
    if (tex->ima) {
      tex->ima->id.tag |= ID_TAG_DOIT;
    }
  }

  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    if (ima->cache && (ima->id.tag & ID_TAG_DOIT)) {
#ifdef CHECK_FREED_SIZE
      uintptr_t old_size = image_mem_size(ima);
#endif

      IMB_moviecache_cleanup(ima->cache, imagecache_check_dirty, nullptr);

#ifdef CHECK_FREED_SIZE
      tot_freed_size += old_size - image_mem_size(ima);
#endif
    }
  }
#ifdef CHECK_FREED_SIZE
  printf("%s: freed total %lu MB\n", __func__, tot_freed_size / (1024 * 1024));
#endif
}

static bool imagecache_check_free_anim(ImBuf *ibuf, void * /*userkey*/, void *userdata)
{
  if (ibuf == nullptr) {
    return true;
  }
  int except_frame = *(int *)userdata;
  return (ibuf->userflags & IB_BITMAPDIRTY) == 0 && (ibuf->index != IMA_NO_INDEX) &&
         (except_frame != IMA_INDEX_ENTRY(ibuf->index));
}

void BKE_image_free_anim_ibufs(Image *ima, int except_frame)
{
  std::scoped_lock lock(ima->runtime->cache_mutex);
  if (ima->cache != nullptr) {
    IMB_moviecache_cleanup(ima->cache, imagecache_check_free_anim, &except_frame);
  }
}

void BKE_image_all_free_anim_ibufs(Main *bmain, int cfra)
{
  Image *ima;

  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    if (BKE_image_is_animated(ima)) {
      BKE_image_free_anim_ibufs(ima, cfra);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read and Write
 * \{ */

#define STAMP_NAME_SIZE ((MAX_ID_NAME - 2) + 16)
/* could allow access externally - 512 is for long names,
 * STAMP_NAME_SIZE is for id names, allowing them some room for description */
struct StampDataCustomField {
  StampDataCustomField *next, *prev;
  /* TODO(sergey): Think of better size here, maybe dynamically allocated even. */
  char key[512];
  char *value;
  /* TODO(sergey): Support non-string values. */
};

struct StampData {
  char file[512];
  char note[512];
  char date[512];
  char marker[512];
  char time[512];
  char frame[512];
  char frame_range[512];
  char camera[STAMP_NAME_SIZE];
  char cameralens[STAMP_NAME_SIZE];
  char scene[STAMP_NAME_SIZE];
  char strip[STAMP_NAME_SIZE];
  char rendertime[STAMP_NAME_SIZE];
  char memory[STAMP_NAME_SIZE];
  char hostname[512];

  /* Custom fields are used to put extra meta information header from render
   * engine to the result image.
   *
   * NOTE: This fields are not stamped onto the image. At least for now.
   */
  ListBase custom_fields;
};
#undef STAMP_NAME_SIZE

/**
 * \param do_prefix: Include a label like "File ", "Date ", etc. in the stamp data strings.
 * \param use_dynamic: Also include data that can change on a per-frame basis.
 */
static void stampdata(
    const Scene *scene, Object *camera, StampData *stamp_data, int do_prefix, bool use_dynamic)
{
  char text[256];
  tm *tl;
  time_t t;

  if (scene->r.stamp & R_STAMP_FILENAME) {
    const char *blendfile_path = BKE_main_blendfile_path_from_global();
    SNPRINTF_UTF8(stamp_data->file,
                  do_prefix ? "File %s" : "%s",
                  (blendfile_path[0] != '\0') ? blendfile_path : "<untitled>");
  }
  else {
    stamp_data->file[0] = '\0';
  }

  if (scene->r.stamp & R_STAMP_NOTE) {
    /* Never do prefix for Note */
    STRNCPY_UTF8(stamp_data->note, scene->r.stamp_udata);
  }
  else {
    stamp_data->note[0] = '\0';
  }

  if (scene->r.stamp & R_STAMP_DATE) {
    t = time(nullptr);
    tl = localtime(&t);
    SNPRINTF_UTF8(text,
                  "%04d/%02d/%02d %02d:%02d:%02d",
                  tl->tm_year + 1900,
                  tl->tm_mon + 1,
                  tl->tm_mday,
                  tl->tm_hour,
                  tl->tm_min,
                  tl->tm_sec);
    SNPRINTF_UTF8(stamp_data->date, do_prefix ? "Date %s" : "%s", text);
  }
  else {
    stamp_data->date[0] = '\0';
  }

  if (use_dynamic && scene->r.stamp & R_STAMP_MARKER) {
    const char *name = BKE_scene_find_last_marker_name(scene, scene->r.cfra);

    if (name) {
      STRNCPY_UTF8(text, name);
    }
    else {
      STRNCPY_UTF8(text, "<none>");
    }

    SNPRINTF_UTF8(stamp_data->marker, do_prefix ? "Marker %s" : "%s", text);
  }
  else {
    stamp_data->marker[0] = '\0';
  }

  if (use_dynamic && scene->r.stamp & R_STAMP_TIME) {
    const short timecode_style = USER_TIMECODE_SMPTE_FULL;
    BLI_timecode_string_from_time(text,
                                  sizeof(text),
                                  0,
                                  FRA2TIME(scene->r.cfra),
                                  scene->frames_per_second(),
                                  timecode_style);
    SNPRINTF_UTF8(stamp_data->time, do_prefix ? "Timecode %s" : "%s", text);
  }
  else {
    stamp_data->time[0] = '\0';
  }

  if (use_dynamic && scene->r.stamp & R_STAMP_FRAME) {
    char fmtstr[32];
    int digits = 1;

    if (scene->r.efra > 9) {
      digits = integer_digits_i(scene->r.efra);
    }

    SNPRINTF_UTF8(fmtstr, do_prefix ? "Frame %%0%di" : "%%0%di", digits);
    SNPRINTF_UTF8(stamp_data->frame, fmtstr, scene->r.cfra);
  }
  else {
    stamp_data->frame[0] = '\0';
  }

  if (scene->r.stamp & R_STAMP_FRAME_RANGE) {
    SNPRINTF_UTF8(stamp_data->frame_range,
                  do_prefix ? "Frame Range %d:%d" : "%d:%d",
                  scene->r.sfra,
                  scene->r.efra);
  }
  else {
    stamp_data->frame_range[0] = '\0';
  }

  if (use_dynamic && scene->r.stamp & R_STAMP_CAMERA) {
    SNPRINTF_UTF8(stamp_data->camera,
                  do_prefix ? "Camera %s" : "%s",
                  camera ? camera->id.name + 2 : "<none>");
  }
  else {
    stamp_data->camera[0] = '\0';
  }

  if (use_dynamic && scene->r.stamp & R_STAMP_CAMERALENS) {
    if (camera && camera->type == OB_CAMERA) {
      SNPRINTF_UTF8(text, "%.2f", ((Camera *)camera->data)->lens);
    }
    else {
      STRNCPY_UTF8(text, "<none>");
    }

    SNPRINTF_UTF8(stamp_data->cameralens, do_prefix ? "Lens %s" : "%s", text);
  }
  else {
    stamp_data->cameralens[0] = '\0';
  }

  if (scene->r.stamp & R_STAMP_SCENE) {
    SNPRINTF_UTF8(stamp_data->scene, do_prefix ? "Scene %s" : "%s", scene->id.name + 2);
  }
  else {
    stamp_data->scene[0] = '\0';
  }

  if (use_dynamic && scene->r.stamp & R_STAMP_SEQSTRIP) {
    const Strip *strip = blender::seq::strip_topmost_get(scene, scene->r.cfra);

    if (strip) {
      STRNCPY_UTF8(text, strip->name + 2);
    }
    else {
      STRNCPY_UTF8(text, "<none>");
    }

    SNPRINTF_UTF8(stamp_data->strip, do_prefix ? "Strip %s" : "%s", text);
  }
  else {
    stamp_data->strip[0] = '\0';
  }

  {
    Render *re = RE_GetSceneRender(scene);
    RenderStats *stats = re ? RE_GetStats(re) : nullptr;

    if (use_dynamic && stats && (scene->r.stamp & R_STAMP_RENDERTIME)) {
      BLI_timecode_string_from_time_simple(text, sizeof(text), stats->lastframetime);

      SNPRINTF_UTF8(stamp_data->rendertime, do_prefix ? "RenderTime %s" : "%s", text);
    }
    else {
      stamp_data->rendertime[0] = '\0';
    }

    if (use_dynamic && stats && (scene->r.stamp & R_STAMP_MEMORY)) {
      SNPRINTF_UTF8(stamp_data->memory, do_prefix ? "Peak Memory ddM" : "%dM", stats->mem_peak);
    }
    else {
      stamp_data->memory[0] = '\0';
    }
  }
  if (scene->r.stamp & R_STAMP_FRAME_RANGE) {
    SNPRINTF_UTF8(stamp_data->frame_range,
                  do_prefix ? "Frame Range %d:%d" : "%d:%d",
                  scene->r.sfra,
                  scene->r.efra);
  }
  else {
    stamp_data->frame_range[0] = '\0';
  }

  if (scene->r.stamp & R_STAMP_HOSTNAME) {
    char hostname[500]; /* sizeof(stamp_data->hostname) minus some bytes for a label. */
    BLI_hostname_get(hostname, sizeof(hostname));
    SNPRINTF_UTF8(stamp_data->hostname, do_prefix ? "Hostname %s" : "%s", hostname);
  }
  else {
    stamp_data->hostname[0] = '\0';
  }
}

static void stampdata_from_template(StampData *stamp_data,
                                    const Scene *scene,
                                    const StampData *stamp_data_template,
                                    bool do_prefix)
{
  if (scene->r.stamp & R_STAMP_FILENAME) {
    SNPRINTF_UTF8(stamp_data->file, do_prefix ? "File %s" : "%s", stamp_data_template->file);
  }
  else {
    stamp_data->file[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_NOTE) {
    STRNCPY_UTF8(stamp_data->note, stamp_data_template->note);
  }
  else {
    stamp_data->note[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_DATE) {
    SNPRINTF_UTF8(stamp_data->date, do_prefix ? "Date %s" : "%s", stamp_data_template->date);
  }
  else {
    stamp_data->date[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_MARKER) {
    SNPRINTF_UTF8(stamp_data->marker, do_prefix ? "Marker %s" : "%s", stamp_data_template->marker);
  }
  else {
    stamp_data->marker[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_TIME) {
    SNPRINTF_UTF8(stamp_data->time, do_prefix ? "Timecode %s" : "%s", stamp_data_template->time);
  }
  else {
    stamp_data->time[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_FRAME) {
    SNPRINTF_UTF8(stamp_data->frame, do_prefix ? "Frame %s" : "%s", stamp_data_template->frame);
  }
  else {
    stamp_data->frame[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_FRAME_RANGE) {
    SNPRINTF_UTF8(stamp_data->frame_range,
                  do_prefix ? "Frame Range %s" : "%s",
                  stamp_data_template->frame_range);
  }
  else {
    stamp_data->frame_range[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_CAMERA) {
    SNPRINTF_UTF8(stamp_data->camera, do_prefix ? "Camera %s" : "%s", stamp_data_template->camera);
  }
  else {
    stamp_data->camera[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_CAMERALENS) {
    SNPRINTF_UTF8(
        stamp_data->cameralens, do_prefix ? "Lens %s" : "%s", stamp_data_template->cameralens);
  }
  else {
    stamp_data->cameralens[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_SCENE) {
    SNPRINTF_UTF8(stamp_data->scene, do_prefix ? "Scene %s" : "%s", stamp_data_template->scene);
  }
  else {
    stamp_data->scene[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_SEQSTRIP) {
    SNPRINTF_UTF8(stamp_data->strip, do_prefix ? "Strip %s" : "%s", stamp_data_template->strip);
  }
  else {
    stamp_data->strip[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_RENDERTIME) {
    SNPRINTF_UTF8(stamp_data->rendertime,
                  do_prefix ? "RenderTime %s" : "%s",
                  stamp_data_template->rendertime);
  }
  else {
    stamp_data->rendertime[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_MEMORY) {
    SNPRINTF_UTF8(
        stamp_data->memory, do_prefix ? "Peak Memory %s" : "%s", stamp_data_template->memory);
  }
  else {
    stamp_data->memory[0] = '\0';
  }
  if (scene->r.stamp & R_STAMP_HOSTNAME) {
    SNPRINTF_UTF8(
        stamp_data->hostname, do_prefix ? "Hostname %s" : "%s", stamp_data_template->hostname);
  }
  else {
    stamp_data->hostname[0] = '\0';
  }
}

void BKE_image_stamp_buf(Scene *scene,
                         Object *camera,
                         const StampData *stamp_data_template,
                         ImBuf *ibuf)
{
  StampData stamp_data;
  int w, h, pad;
  int x, y, y_ofs;
  int h_fixed;
  const int mono = blf_mono_font_render; /* XXX */

  /* vars for calculating wordwrap */
  struct {
    ResultBLF info;
    rcti rect;
  } wrap;

  /* this could be an argument if we want to operate on non linear float imbuf's
   * for now though this is only used for renders which use scene settings */

#define TEXT_SIZE_CHECK(str, w, h) \
  ((str[0]) && ((void)(h = h_fixed), (w = int(BLF_width(mono, str, sizeof(str))))))

  /* must enable BLF_WORD_WRAP before using */
#define TEXT_SIZE_CHECK_WORD_WRAP(str, w, h) \
  ((str[0]) && (BLF_boundbox(mono, str, sizeof(str), &wrap.rect, &wrap.info), \
                (void)(h = h_fixed * wrap.info.lines), \
                (w = BLI_rcti_size_x(&wrap.rect))))

#define BUFF_MARGIN_X 2
#define BUFF_MARGIN_Y 1

  if (!ibuf->byte_buffer.data && !ibuf->float_buffer.data) {
    return;
  }

  bool do_prefix = (scene->r.stamp & R_STAMP_HIDE_LABELS) == 0;
  if (stamp_data_template == nullptr) {
    stampdata(scene, camera, &stamp_data, do_prefix, true);
  }
  else {
    stampdata_from_template(&stamp_data, scene, stamp_data_template, do_prefix);
  }

  /* TODO: do_versions. */
  if (scene->r.stamp_font_id < 8) {
    scene->r.stamp_font_id = 12;
  }

  /* set before return */
  BLF_size(mono, scene->r.stamp_font_id);
  BLF_wordwrap(mono, ibuf->x - (BUFF_MARGIN_X * 2));

  BLF_buffer(mono,
             ibuf->float_buffer.data,
             ibuf->byte_buffer.data,
             ibuf->x,
             ibuf->y,
             ibuf->byte_buffer.colorspace);
  /* No conversion to scene linear needed, #BLF_buffer_col accepts sRGB. */
  BLF_buffer_col(mono, scene->r.fg_stamp);
  pad = BLF_width_max(mono);

  /* use 'h_fixed' rather than 'h', aligns better */
  h_fixed = BLF_height_max(mono);
  y_ofs = -BLF_descender(mono);

  x = 0;
  y = ibuf->y;

  /* Background needs to be converted to scene linear. */
  float bg_stamp[4];
  IMB_colormanagement_srgb_to_scene_linear_v3(bg_stamp, scene->r.bg_stamp);
  bg_stamp[3] = scene->r.bg_stamp[3];

  if (TEXT_SIZE_CHECK(stamp_data.file, w, h)) {
    /* Top left corner */
    y -= h;

    /* also a little of space to the background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);

    /* and draw the text. */
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.file, sizeof(stamp_data.file));

    /* the extra pixel for background. */
    y -= BUFF_MARGIN_Y * 2;
  }

  /* Top left corner, below File */
  if (TEXT_SIZE_CHECK(stamp_data.date, w, h)) {
    y -= h;

    /* and space for background. */
    IMB_rectfill_area(
        ibuf, bg_stamp, 0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.date, sizeof(stamp_data.date));

    /* the extra pixel for background. */
    y -= BUFF_MARGIN_Y * 2;
  }

  /* Top left corner, below File, Date */
  if (TEXT_SIZE_CHECK(stamp_data.rendertime, w, h)) {
    y -= h;

    /* and space for background. */
    IMB_rectfill_area(
        ibuf, bg_stamp, 0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.rendertime, sizeof(stamp_data.rendertime));

    /* the extra pixel for background. */
    y -= BUFF_MARGIN_Y * 2;
  }

  /* Top left corner, below File, Date, Render-time */
  if (TEXT_SIZE_CHECK(stamp_data.memory, w, h)) {
    y -= h;

    /* and space for background. */
    IMB_rectfill_area(
        ibuf, bg_stamp, 0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.memory, sizeof(stamp_data.memory));

    /* the extra pixel for background. */
    y -= BUFF_MARGIN_Y * 2;
  }

  /* Top left corner, below: File, Date, Render-time, Memory. */
  if (TEXT_SIZE_CHECK(stamp_data.hostname, w, h)) {
    y -= h;

    /* and space for background. */
    IMB_rectfill_area(
        ibuf, bg_stamp, 0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.hostname, sizeof(stamp_data.hostname));

    /* the extra pixel for background. */
    y -= BUFF_MARGIN_Y * 2;
  }

  /* Top left corner, below: File, Date, Memory, Render-time, Host-name. */
  BLF_enable(mono, BLF_WORD_WRAP);
  if (TEXT_SIZE_CHECK_WORD_WRAP(stamp_data.note, w, h)) {
    y -= h;

    /* and space for background. */
    IMB_rectfill_area(
        ibuf, bg_stamp, 0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

    BLF_position(mono, x, y + y_ofs + (h - h_fixed), 0.0);
    BLF_draw_buffer(mono, stamp_data.note, sizeof(stamp_data.note));
  }
  BLF_disable(mono, BLF_WORD_WRAP);

  x = 0;
  y = 0;

  /* Bottom left corner, leaving space for timing */
  if (TEXT_SIZE_CHECK(stamp_data.marker, w, h)) {

    /* extra space for background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);

    /* and pad the text. */
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.marker, sizeof(stamp_data.marker));

    /* space width. */
    x += w + pad;
  }

  /* Left bottom corner */
  if (TEXT_SIZE_CHECK(stamp_data.time, w, h)) {

    /* extra space for background */
    IMB_rectfill_area(
        ibuf, bg_stamp, x - BUFF_MARGIN_X, y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

    /* and pad the text. */
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.time, sizeof(stamp_data.time));

    /* space width. */
    x += w + pad;
  }

  if (TEXT_SIZE_CHECK(stamp_data.frame, w, h)) {

    /* extra space for background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      x + w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);

    /* and pad the text. */
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.frame, sizeof(stamp_data.frame));

    /* space width. */
    x += w + pad;
  }

  if (TEXT_SIZE_CHECK(stamp_data.frame_range, w, h)) {

    /* extra space for background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      x + w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);

    /* and pad the text. */
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.frame_range, sizeof(stamp_data.frame_range));

    /* space width. */
    x += w + pad;
  }

  if (TEXT_SIZE_CHECK(stamp_data.camera, w, h)) {

    /* extra space for background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      x + w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.camera, sizeof(stamp_data.camera));

    /* space width. */
    x += w + pad;
  }

  if (TEXT_SIZE_CHECK(stamp_data.cameralens, w, h)) {

    /* extra space for background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      x + w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.cameralens, sizeof(stamp_data.cameralens));
  }

  if (TEXT_SIZE_CHECK(stamp_data.scene, w, h)) {

    /* Bottom right corner, with an extra space because the BLF API is too strict! */
    x = ibuf->x - w - 2;

    /* extra space for background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      x + w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);

    /* and pad the text. */
    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.scene, sizeof(stamp_data.scene));
  }

  if (TEXT_SIZE_CHECK(stamp_data.strip, w, h)) {

    /* Top right corner, with an extra space because the BLF API is too strict! */
    x = ibuf->x - w - pad;
    y = ibuf->y - h;

    /* extra space for background. */
    IMB_rectfill_area(ibuf,
                      bg_stamp,
                      x - BUFF_MARGIN_X,
                      y - BUFF_MARGIN_Y,
                      x + w + BUFF_MARGIN_X,
                      y + h + BUFF_MARGIN_Y);

    BLF_position(mono, x, y + y_ofs, 0.0);
    BLF_draw_buffer(mono, stamp_data.strip, sizeof(stamp_data.strip));
  }

  /* cleanup the buffer. */
  BLF_buffer(mono, nullptr, nullptr, 0, 0, nullptr);
  BLF_wordwrap(mono, 0);

#undef TEXT_SIZE_CHECK
#undef TEXT_SIZE_CHECK_WORD_WRAP
#undef BUFF_MARGIN_X
#undef BUFF_MARGIN_Y
}

void BKE_render_result_stamp_info(Scene *scene,
                                  Object *camera,
                                  RenderResult *rr,
                                  bool allocate_only)
{
  StampData *stamp_data;

  if (!(scene && (scene->r.stamp & R_STAMP_ALL)) && !allocate_only) {
    return;
  }

  if (!rr->stamp_data) {
    stamp_data = MEM_callocN<StampData>("RenderResult.stamp_data");
  }
  else {
    stamp_data = rr->stamp_data;
  }

  if (!allocate_only) {
    stampdata(scene, camera, stamp_data, 0, true);
  }

  if (!rr->stamp_data) {
    rr->stamp_data = stamp_data;
  }
}

StampData *BKE_stamp_info_from_scene_static(const Scene *scene)
{
  StampData *stamp_data;

  if (!(scene && (scene->r.stamp & R_STAMP_ALL))) {
    return nullptr;
  }

  /* Memory is allocated here (instead of by the caller) so that the caller
   * doesn't have to know the size of the StampData struct. */
  stamp_data = MEM_callocN<StampData>(__func__);
  stampdata(scene, nullptr, stamp_data, 0, false);

  return stamp_data;
}

static const char *stamp_metadata_fields[] = {
    "File",
    "Note",
    "Date",
    "Marker",
    "Time",
    "Frame",
    "FrameRange",
    "Camera",
    "Lens",
    "Scene",
    "Strip",
    "RenderTime",
    "Memory",
    "Hostname",
    nullptr,
};

bool BKE_stamp_is_known_field(const char *field_name)
{
  int i = 0;
  while (stamp_metadata_fields[i] != nullptr) {
    if (STREQ(field_name, stamp_metadata_fields[i])) {
      return true;
    }
    i++;
  }
  return false;
}

void BKE_stamp_info_callback(void *data,
                             StampData *stamp_data,
                             StampCallback callback,
                             bool noskip)
{
  if ((callback == nullptr) || (stamp_data == nullptr)) {
    return;
  }

#define CALL(member, value_str) \
  if (noskip || stamp_data->member[0]) { \
    callback(data, value_str, stamp_data->member, sizeof(stamp_data->member)); \
  } \
  ((void)0)

  /* TODO(sergey): Use stamp_metadata_fields somehow, or make it more generic
   * meta information to avoid duplication. */
  CALL(file, "File");
  CALL(note, "Note");
  CALL(date, "Date");
  CALL(marker, "Marker");
  CALL(time, "Time");
  CALL(frame, "Frame");
  CALL(frame_range, "FrameRange");
  CALL(camera, "Camera");
  CALL(cameralens, "Lens");
  CALL(scene, "Scene");
  CALL(strip, "Strip");
  CALL(rendertime, "RenderTime");
  CALL(memory, "Memory");
  CALL(hostname, "Hostname");

  LISTBASE_FOREACH (StampDataCustomField *, custom_field, &stamp_data->custom_fields) {
    if (noskip || custom_field->value[0]) {
      callback(data, custom_field->key, custom_field->value, strlen(custom_field->value) + 1);
    }
  }

#undef CALL
}

void BKE_image_multilayer_stamp_info_callback(void *data,
                                              const Image &image,
                                              StampCallback callback,
                                              bool noskip)
{
  std::scoped_lock lock(image.runtime->cache_mutex);

  if (!image.rr || !image.rr->stamp_data) {
    return;
  }

  BKE_stamp_info_callback(data, image.rr->stamp_data, callback, noskip);
}

void BKE_render_result_stamp_data(RenderResult *rr, const char *key, const char *value)
{
  StampData *stamp_data;
  if (rr->stamp_data == nullptr) {
    rr->stamp_data = MEM_callocN<StampData>("RenderResult.stamp_data");
  }
  stamp_data = rr->stamp_data;
  StampDataCustomField *field = MEM_mallocN<StampDataCustomField>("StampData Custom Field");
  STRNCPY_UTF8(field->key, key);
  field->value = BLI_strdup(value);
  BLI_addtail(&stamp_data->custom_fields, field);
}

StampData *BKE_stamp_data_copy(const StampData *stamp_data)
{
  if (stamp_data == nullptr) {
    return nullptr;
  }

  StampData *stamp_datan = static_cast<StampData *>(MEM_dupallocN(stamp_data));
  BLI_duplicatelist(&stamp_datan->custom_fields, &stamp_data->custom_fields);

  LISTBASE_FOREACH (StampDataCustomField *, custom_fieldn, &stamp_datan->custom_fields) {
    custom_fieldn->value = static_cast<char *>(MEM_dupallocN(custom_fieldn->value));
  }

  return stamp_datan;
}

void BKE_stamp_data_free(StampData *stamp_data)
{
  if (stamp_data == nullptr) {
    return;
  }
  LISTBASE_FOREACH (StampDataCustomField *, custom_field, &stamp_data->custom_fields) {
    MEM_freeN(custom_field->value);
  }
  BLI_freelistN(&stamp_data->custom_fields);
  MEM_freeN(stamp_data);
}

/* wrap for callback only */
static void metadata_set_field(void *data,
                               const char *propname,
                               char *propvalue,
                               int /*propvalue_maxncpy*/)
{
  /* We know it is an `ImBuf *` because that's what we pass to #BKE_stamp_info_callback. */
  ImBuf *imbuf = static_cast<ImBuf *>(data);
  IMB_metadata_set_field(imbuf->metadata, propname, propvalue);
}

static void metadata_get_field(void *data,
                               const char *propname,
                               char *propvalue,
                               int propvalue_maxncpy)
{
  /* We know it is an `ImBuf *` because that's what we pass to #BKE_stamp_info_callback. */
  ImBuf *imbuf = static_cast<ImBuf *>(data);
  IMB_metadata_get_field(imbuf->metadata, propname, propvalue, propvalue_maxncpy);
}

void BKE_imbuf_stamp_info(const RenderResult *rr, ImBuf *ibuf)
{
  StampData *stamp_data = const_cast<StampData *>(rr->stamp_data);
  IMB_metadata_ensure(&ibuf->metadata);
  BKE_stamp_info_callback(ibuf, stamp_data, metadata_set_field, false);
}

static void metadata_copy_custom_fields(const char *field, const char *value, void *rr_v)
{
  if (BKE_stamp_is_known_field(field)) {
    return;
  }
  RenderResult *rr = (RenderResult *)rr_v;
  BKE_render_result_stamp_data(rr, field, value);
}

void BKE_stamp_info_from_imbuf(RenderResult *rr, ImBuf *ibuf)
{
  if (rr->stamp_data == nullptr) {
    rr->stamp_data = MEM_callocN<StampData>("RenderResult.stamp_data");
  }
  StampData *stamp_data = rr->stamp_data;
  IMB_metadata_ensure(&ibuf->metadata);
  BKE_stamp_info_callback(ibuf, stamp_data, metadata_get_field, true);
  /* Copy render engine specific settings. */
  IMB_metadata_foreach(ibuf, metadata_copy_custom_fields, rr);
}

bool BKE_imbuf_alpha_test(ImBuf *ibuf)
{
  if (ibuf->float_buffer.data) {
    const float *buf = ibuf->float_buffer.data;
    for (size_t tot = IMB_get_pixel_count(ibuf); tot--; buf += 4) {
      if (buf[3] < 1.0f) {
        return true;
      }
    }
  }
  else if (ibuf->byte_buffer.data) {
    uchar *buf = ibuf->byte_buffer.data;
    for (size_t tot = IMB_get_pixel_count(ibuf); tot--; buf += 4) {
      if (buf[3] != 255) {
        return true;
      }
    }
  }

  return false;
}

bool BKE_imbuf_write(ImBuf *ibuf, const char *filepath, const ImageFormatData *imf)
{
  if (!BLI_file_ensure_parent_dir_exists(filepath)) {
    CLOG_ERROR(&LOG, "Couldn't create directory for file %s: %s", filepath, std::strerror(errno));
    return false;
  }

  BKE_image_format_to_imbuf(ibuf, imf);

  const bool ok = IMB_save_image(ibuf, filepath, IB_byte_data);
  if (!ok && errno != 0) {
    perror(filepath);
  }

  return ok;
}

bool BKE_imbuf_write_as(ImBuf *ibuf,
                        const char *filepath,
                        const ImageFormatData *imf,
                        const bool save_copy)
{
  ImBuf ibuf_back = *ibuf;
  bool ok;

  /* All data is RGBA anyway, this just controls how to save for some formats. */
  ibuf->planes = imf->planes;

  ok = BKE_imbuf_write(ibuf, filepath, imf);

  if (save_copy) {
    /* note that we are not restoring _all_ settings */
    ibuf->planes = ibuf_back.planes;
    ibuf->ftype = ibuf_back.ftype;
    ibuf->foptions = ibuf_back.foptions;
  }

  return ok;
}

bool BKE_imbuf_write_stamp(const Scene *scene,
                           const RenderResult *rr,
                           ImBuf *ibuf,
                           const char *filepath,
                           const ImageFormatData *imf)
{
  if (scene && scene->r.stamp & R_STAMP_ALL) {
    BKE_imbuf_stamp_info(rr, ibuf);
  }

  return BKE_imbuf_write(ibuf, filepath, imf);
}

MovieReader *openanim_noload(const char *filepath,
                             const int flags,
                             const int streamindex,
                             const bool keep_original_colorspace,
                             char colorspace[IMA_MAX_SPACE])
{
  MovieReader *anim;

  anim = MOV_open_file(filepath, flags, streamindex, keep_original_colorspace, colorspace);
  return anim;
}

MovieReader *openanim(const char *filepath,
                      const int ibuf_flags,
                      const int streamindex,
                      const bool keep_original_colorspace,
                      char colorspace[IMA_MAX_SPACE])
{
  MovieReader *anim;
  ImBuf *ibuf;

  anim = MOV_open_file(filepath, ibuf_flags, streamindex, keep_original_colorspace, colorspace);
  if (anim == nullptr) {
    return nullptr;
  }

  ibuf = MOV_decode_frame(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
  if (ibuf == nullptr) {
    const char *reason;
    if (!BLI_exists(filepath)) {
      reason = "file doesn't exist";
    }
    else {
      reason = "not an anim";
    }
    CLOG_INFO(&LOG, "unable to load anim, %s: %s", reason, filepath);

    MOV_close(anim);
    return nullptr;
  }
  IMB_freeImBuf(ibuf);

  return anim;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Image API
 * \{ */

/* Notes about Image storage
 * - packedfile
 *   -> written in .blend
 * - filename
 *   -> written in .blend
 * - movie
 *   -> comes from packedfile or filename
 * - renderresult
 *   -> comes from packedfile or filename
 * - listbase
 *   -> ibufs from EXR-handle.
 * - flip-book array
 *   -> ibufs come from movie, temporary renderresult or sequence
 * - ibuf
 *   -> comes from packedfile or filename or generated
 */

Image *BKE_image_ensure_viewer(Main *bmain, int type, const char *name)
{
  Image *ima;

  for (ima = static_cast<Image *>(bmain->images.first); ima;
       ima = static_cast<Image *>(ima->id.next))
  {
    if (ima->source == IMA_SRC_VIEWER) {
      if (ima->type == type) {
        break;
      }
    }
  }

  if (ima == nullptr) {
    ima = image_alloc(bmain, std::nullopt, name, IMA_SRC_VIEWER, type);
  }

  /* Happens on reload, image-window cannot be image user when hidden. */
  if (ima->id.us == 0) {
    id_us_ensure_real(&ima->id);
  }

  return ima;
}

static void image_viewer_create_views(const RenderData *rd, Image *ima)
{
  if ((rd->scemode & R_MULTIVIEW) == 0) {
    image_add_view(ima, "", "");
  }
  else {
    LISTBASE_FOREACH (SceneRenderView *, srv, &rd->views) {
      if (BKE_scene_multiview_is_render_view_active(rd, srv) == false) {
        continue;
      }
      image_add_view(ima, srv->name, "");
    }
  }
}

/* Returns true if the views of the given image match the actives views in the given render data.
 * Returns false otherwise to indicate that the image views should be recreated to make them match
 * the render data. */
static bool image_views_match_render_views(const Image *image, const RenderData *render_data)
{
  /* The number of views in the image do not match the number of active views in the render
   * data. */
  const int number_of_active_views = BKE_scene_multiview_num_views_get(render_data);
  if (BLI_listbase_count(&image->views) != number_of_active_views) {
    return false;
  }

  /* If the render data is not multi-view, then a single unnamed view should exist in the image,
   * otherwise, the views don't match. We don't have to check that a single view exist, since this
   * is handled by the check above for the number of views. */
  if (!(render_data->scemode & R_MULTIVIEW)) {
    const ImageView *image_view = static_cast<ImageView *>(image->views.first);
    /* If the view is unnamed, the views match, otherwise, they don't. */
    return image_view->name[0] == '\0';
  }

  /* The render data is multi-view, so we need to check that for every view in the image, a
   * corresponding active render view with the same name exists, noting that they may not
   * necessarily be in the same order. */
  LISTBASE_FOREACH (ImageView *, image_view, &image->views) {
    SceneRenderView *scene_render_view = static_cast<SceneRenderView *>(
        BLI_findstring(&render_data->views, image_view->name, offsetof(SceneRenderView, name)));
    /* A render view doesn't exist for that image view with the same name, so the views don't
     * match. */
    if (!scene_render_view) {
      return false;
    }

    /* A render view exists for that image view with the same name, but it is disabled, so the
     * views don't match. */
    if (!BKE_scene_multiview_is_render_view_active(render_data, scene_render_view)) {
      return false;
    }
  }

  return true;
}

void BKE_image_ensure_viewer_views(const RenderData *rd, Image *ima, ImageUser *iuser)
{
  BLI_thread_lock(LOCK_DRAW_IMAGE);

  if (!BKE_scene_multiview_is_stereo3d(rd)) {
    iuser->flag &= ~IMA_SHOW_STEREO;
  }

  if (!image_views_match_render_views(ima, rd)) {
    std::scoped_lock lock(ima->runtime->cache_mutex);

    image_free_cached_frames(ima);
    BKE_image_free_views(ima);

    /* add new views */
    image_viewer_create_views(rd, ima);
  }

  BLI_thread_unlock(LOCK_DRAW_IMAGE);
}

static void image_walk_ntree_all_users(
    bNodeTree *ntree,
    ID *id,
    void *customdata,
    void callback(Image *ima, ID *iuser_id, ImageUser *iuser, void *customdata))
{
  switch (ntree->type) {
    case NTREE_SHADER:
      for (bNode *node : ntree->all_nodes()) {
        if (node->id) {
          if (node->type_legacy == SH_NODE_TEX_IMAGE) {
            NodeTexImage *tex = static_cast<NodeTexImage *>(node->storage);
            Image *ima = (Image *)node->id;
            callback(ima, id, &tex->iuser, customdata);
          }
          if (node->type_legacy == SH_NODE_TEX_ENVIRONMENT) {
            NodeTexImage *tex = static_cast<NodeTexImage *>(node->storage);
            Image *ima = (Image *)node->id;
            callback(ima, id, &tex->iuser, customdata);
          }
        }
      }
      break;
    case NTREE_TEXTURE:
      for (bNode *node : ntree->all_nodes()) {
        if (node->id && node->type_legacy == TEX_NODE_IMAGE) {
          Image *ima = (Image *)node->id;
          ImageUser *iuser = static_cast<ImageUser *>(node->storage);
          callback(ima, id, iuser, customdata);
        }
      }
      break;
    case NTREE_COMPOSIT:
      for (bNode *node : ntree->all_nodes()) {
        if (node->id && node->type_legacy == CMP_NODE_IMAGE) {
          Image *ima = (Image *)node->id;
          ImageUser *iuser = static_cast<ImageUser *>(node->storage);
          callback(ima, id, iuser, customdata);
        }
        if (node->type_legacy == CMP_NODE_CRYPTOMATTE) {
          CMPNodeCryptomatteSource source = static_cast<CMPNodeCryptomatteSource>(node->custom1);
          if (source == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
            Image *image = (Image *)node->id;
            ImageUser *image_user = &static_cast<NodeCryptomatte *>(node->storage)->iuser;
            callback(image, id, image_user, customdata);
          }
        }
      }
      break;
  }
}

static void image_walk_gpu_materials(
    ID *id,
    ListBase *gpu_materials,
    void *customdata,
    void callback(Image *ima, ID *iuser_id, ImageUser *iuser, void *customdata))
{
  LISTBASE_FOREACH (LinkData *, link, gpu_materials) {
    GPUMaterial *gpu_material = (GPUMaterial *)link->data;
    ListBase textures = GPU_material_textures(gpu_material);
    LISTBASE_FOREACH (GPUMaterialTexture *, gpu_material_texture, &textures) {
      if (gpu_material_texture->iuser_available) {
        callback(gpu_material_texture->ima, id, &gpu_material_texture->iuser, customdata);
      }
    }
  }
}

static void image_walk_id_all_users(
    ID *id,
    bool skip_nested_nodes,
    void *customdata,
    void callback(Image *ima, ID *iuser_id, ImageUser *iuser, void *customdata))
{
  switch (GS(id->name)) {
    case ID_OB: {
      Object *ob = (Object *)id;
      if (ob->empty_drawtype == OB_EMPTY_IMAGE && ob->data) {
        callback(static_cast<Image *>(ob->data), &ob->id, ob->iuser, customdata);
      }
      break;
    }
    case ID_MA: {
      Material *ma = (Material *)id;
      if (ma->nodetree && !skip_nested_nodes) {
        image_walk_ntree_all_users(ma->nodetree, &ma->id, customdata, callback);
      }
      image_walk_gpu_materials(id, &ma->gpumaterial, customdata, callback);
      break;
    }
    case ID_LA: {
      Light *light = (Light *)id;
      if (light->nodetree && light->use_nodes && !skip_nested_nodes) {
        image_walk_ntree_all_users(light->nodetree, &light->id, customdata, callback);
      }
      break;
    }
    case ID_WO: {
      World *world = (World *)id;
      if (world->nodetree && !skip_nested_nodes) {
        image_walk_ntree_all_users(world->nodetree, &world->id, customdata, callback);
      }
      image_walk_gpu_materials(id, &world->gpumaterial, customdata, callback);
      break;
    }
    case ID_TE: {
      Tex *tex = (Tex *)id;
      if (tex->type == TEX_IMAGE && tex->ima) {
        callback(tex->ima, &tex->id, &tex->iuser, customdata);
      }
      if (tex->nodetree && tex->use_nodes && !skip_nested_nodes) {
        image_walk_ntree_all_users(tex->nodetree, &tex->id, customdata, callback);
      }
      break;
    }
    case ID_NT: {
      bNodeTree *ntree = (bNodeTree *)id;
      image_walk_ntree_all_users(ntree, &ntree->id, customdata, callback);
      break;
    }
    case ID_CA: {
      Camera *cam = (Camera *)id;
      LISTBASE_FOREACH (CameraBGImage *, bgpic, &cam->bg_images) {
        callback(bgpic->ima, nullptr, &bgpic->iuser, customdata);
      }
      break;
    }
    case ID_WM: {
      wmWindowManager *wm = (wmWindowManager *)id;
      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);

        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          if (area->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
            callback(sima->image, nullptr, &sima->iuser, customdata);
          }
        }
      }
      break;
    }
    default:
      break;
  }
}

void BKE_image_walk_all_users(
    const Main *mainp,
    void *customdata,
    void callback(Image *ima, ID *iuser_id, ImageUser *iuser, void *customdata))
{
  for (Scene *scene = static_cast<Scene *>(mainp->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    image_walk_id_all_users(&scene->id, false, customdata, callback);
  }

  for (Object *ob = static_cast<Object *>(mainp->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    image_walk_id_all_users(&ob->id, false, customdata, callback);
  }

  for (bNodeTree *ntree = static_cast<bNodeTree *>(mainp->nodetrees.first); ntree;
       ntree = static_cast<bNodeTree *>(ntree->id.next))
  {
    image_walk_id_all_users(&ntree->id, false, customdata, callback);
  }

  for (Material *ma = static_cast<Material *>(mainp->materials.first); ma;
       ma = static_cast<Material *>(ma->id.next))
  {
    image_walk_id_all_users(&ma->id, false, customdata, callback);
  }

  for (Light *light = static_cast<Light *>(mainp->materials.first); light;
       light = static_cast<Light *>(light->id.next))
  {
    image_walk_id_all_users(&light->id, false, customdata, callback);
  }

  for (World *world = static_cast<World *>(mainp->materials.first); world;
       world = static_cast<World *>(world->id.next))
  {
    image_walk_id_all_users(&world->id, false, customdata, callback);
  }

  for (Tex *tex = static_cast<Tex *>(mainp->textures.first); tex;
       tex = static_cast<Tex *>(tex->id.next))
  {
    image_walk_id_all_users(&tex->id, false, customdata, callback);
  }

  for (Camera *cam = static_cast<Camera *>(mainp->cameras.first); cam;
       cam = static_cast<Camera *>(cam->id.next))
  {
    image_walk_id_all_users(&cam->id, false, customdata, callback);
  }
  /* Only ever 1 `wm`. */
  for (wmWindowManager *wm = static_cast<wmWindowManager *>(mainp->wm.first); wm;
       wm = static_cast<wmWindowManager *>(wm->id.next))
  {
    image_walk_id_all_users(&wm->id, false, customdata, callback);
  }
}

static void image_tag_frame_recalc(Image *ima, ID *iuser_id, ImageUser *iuser, void *customdata)
{
  Image *changed_image = static_cast<Image *>(customdata);

  if (ima == changed_image && BKE_image_is_animated(ima)) {
    iuser->flag |= IMA_NEED_FRAME_RECALC;

    if (iuser_id) {
      /* Must copy image user changes to evaluated data-block. */
      DEG_id_tag_update(iuser_id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
}

static void image_tag_reload(Image *ima, ID *iuser_id, ImageUser *iuser, void *customdata)
{
  Image *changed_image = static_cast<Image *>(customdata);

  if (ima == changed_image) {
    if (iuser->scene) {
      image_update_views_format(ima, iuser);
    }
    if (iuser_id) {
      /* Must copy image user changes to evaluated data-block. */
      DEG_id_tag_update(iuser_id, ID_RECALC_SYNC_TO_EVAL);
    }
    BKE_image_partial_update_mark_full_update(ima);
  }
}

void BKE_imageuser_default(ImageUser *iuser)
{
  *iuser = ImageUser{};
  iuser->frames = 100;
  iuser->sfra = 1;
}

void BKE_image_init_imageuser(Image *ima, ImageUser *iuser)
{
  RenderResult *rr = ima->rr;

  iuser->multi_index = 0;
  iuser->layer = iuser->pass = iuser->view = 0;

  if (rr) {
    BKE_image_multilayer_index(rr, iuser);
  }
}

static void image_free_tile(Image *ima, ImageTile *tile)
{
  for (int i = 0; i < TEXTARGET_COUNT; i++) {
    /* Only two textures depends on all tiles, so if this is a secondary tile we can keep the other
     * two. */
    if (tile != ima->tiles.first && !ELEM(i, TEXTARGET_2D_ARRAY, TEXTARGET_TILE_MAPPING)) {
      continue;
    }

    for (int eye = 0; eye < 2; eye++) {
      if (ima->gputexture[i][eye] != nullptr) {
        GPU_texture_free(ima->gputexture[i][eye]);
        ima->gputexture[i][eye] = nullptr;
      }
    }
  }
  BKE_image_partial_update_mark_full_update(ima);

  if (BKE_image_is_multiview(ima)) {
    const int totviews = BLI_listbase_count(&ima->views);
    for (int i = 0; i < totviews; i++) {
      image_remove_ibuf(ima, i, tile->tile_number);
    }
  }
  else {
    image_remove_ibuf(ima, 0, tile->tile_number);
  }
}

static bool image_remove_tile(Image *ima, ImageTile *tile)
{
  if (BLI_listbase_is_single(&ima->tiles)) {
    /* Can't remove the last remaining tile. */
    return false;
  }

  image_free_tile(ima, tile);
  BLI_remlink(&ima->tiles, tile);
  MEM_freeN(tile);

  return true;
}

static void image_remove_all_tiles(Image *ima)
{
  /* Remove all but the final tile. */
  while (image_remove_tile(ima, static_cast<ImageTile *>(ima->tiles.last))) {
    ;
  }
}

void BKE_image_signal(Main *bmain, Image *ima, ImageUser *iuser, int signal)
{
  if (ima == nullptr) {
    return;
  }

  ima->runtime->cache_mutex.lock();

  switch (signal) {
    case IMA_SIGNAL_FREE:
      BKE_image_free_buffers(ima);

      if (iuser) {
        if (iuser->scene) {
          image_update_views_format(ima, iuser);
        }
      }
      break;
    case IMA_SIGNAL_SRC_CHANGE:
      if (ima->type == IMA_TYPE_UV_TEST) {
        if (ima->source != IMA_SRC_GENERATED) {
          ima->type = IMA_TYPE_IMAGE;
        }
      }

      if (ima->source == IMA_SRC_GENERATED) {
        ImageTile *base_tile = BKE_image_get_tile(ima, 0);
        if (base_tile->gen_x == 0 || base_tile->gen_y == 0) {
          ImBuf *ibuf = image_get_cached_ibuf_for_index_entry(ima, IMA_NO_INDEX, 0, nullptr);
          if (ibuf) {
            base_tile->gen_x = ibuf->x;
            base_tile->gen_y = ibuf->y;
            IMB_freeImBuf(ibuf);
          }
        }

        /* Changing source type to generated will likely change file format
         * used by generated image buffer. Saving different file format to
         * the old name might confuse other applications.
         *
         * Here we ensure original image path wouldn't be used when saving
         * generated image.
         */
        ima->filepath[0] = '\0';
      }

      if (ima->source != IMA_SRC_TILED) {
        /* Free all but the first tile. */
        image_remove_all_tiles(ima);

        /* If this used to be a UDIM image, get the concrete filepath associated
         * with the remaining tile and use that as the new filepath. */
        ImageTile *base_tile = BKE_image_get_tile(ima, 0);
        if (BKE_image_is_filename_tokenized(ima->filepath)) {
          const bool was_relative = BLI_path_is_rel(ima->filepath);

          eUDIM_TILE_FORMAT tile_format;
          char *udim_pattern = BKE_image_get_tile_strformat(ima->filepath, &tile_format);
          BKE_image_set_filepath_from_tile_number(
              ima->filepath, udim_pattern, tile_format, base_tile->tile_number);
          MEM_freeN(udim_pattern);

          if (was_relative) {
            const char *relbase = ID_BLEND_PATH(bmain, &ima->id);
            BLI_path_rel(ima->filepath, relbase);
          }
        }

        /* If the remaining tile was not number 1001, we need to reassign it so that
         * ibuf lookups from the cache still succeed. */
        base_tile->tile_number = 1001;
      }
      else {
        /* When changing to UDIM, attempt to tokenize the filepath. */
        BKE_image_ensure_tile_token(ima->filepath, sizeof(ima->filepath));
      }

      /* image buffers for non-sequence multilayer will share buffers with RenderResult,
       * however sequence multilayer will own buffers. Such logic makes switching from
       * single multilayer file to sequence completely unstable
       * since changes in nodes seems this workaround isn't needed anymore, all sockets
       * are nicely detecting anyway, but freeing buffers always here makes multilayer
       * sequences behave stable
       */
      BKE_image_free_buffers(ima);

      if (iuser) {
        image_tag_frame_recalc(ima, nullptr, iuser, ima);
      }
      BKE_image_walk_all_users(bmain, ima, image_tag_frame_recalc);
      BKE_image_partial_update_mark_full_update(ima);

      break;

    case IMA_SIGNAL_RELOAD:
      /* try to repack file */
      if (BKE_image_has_packedfile(ima)) {
        const int tot_viewfiles = image_num_viewfiles(ima);
        const int tot_files = tot_viewfiles * BLI_listbase_count(&ima->tiles);

        if (!BLI_listbase_count_is_equal_to(&ima->packedfiles, tot_files)) {
          /* in case there are new available files to be loaded */
          image_free_packedfiles(ima);
          BKE_image_packfiles(nullptr, ima, ID_BLEND_PATH(bmain, &ima->id));
        }
        else {
          LISTBASE_FOREACH (ImagePackedFile *, imapf, &ima->packedfiles) {
            PackedFile *pf;
            pf = BKE_packedfile_new(nullptr, imapf->filepath, ID_BLEND_PATH(bmain, &ima->id));
            if (pf) {
              BKE_packedfile_free(imapf->packedfile);
              imapf->packedfile = pf;
            }
            else {
              printf("ERROR: Image \"%s\" not available. Keeping packed image\n", imapf->filepath);
            }
          }
        }

        if (BKE_image_has_packedfile(ima)) {
          BKE_image_free_buffers(ima);
        }
      }
      else {
        BKE_image_free_buffers(ima);
      }

      if (ima->source == IMA_SRC_TILED) {
        ListBase new_tiles = {nullptr, nullptr};
        int new_start, new_range;

        char filepath[FILE_MAX];
        STRNCPY(filepath, ima->filepath);
        BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&ima->id));
        bool result = BKE_image_get_tile_info(filepath, &new_tiles, &new_start, &new_range);
        if (result) {
          /* Because the prior and new list of tiles are both sparse sequences, we need to be sure
           * to account for how the two sets might or might not overlap. To be complete, we start
           * the refresh process by clearing all existing tiles, stopping when there's only 1 tile
           * left. */
          image_remove_all_tiles(ima);

          ImageTile *base_tile = BKE_image_get_tile(ima, 0);
          int remaining_tile_number = base_tile->tile_number;
          bool needs_final_cleanup = true;

          /* Add in all the new tiles. As the image is proven to be on disk at this point, remove
           * the generation flag from the remaining tile in case this was previously a generated
           * image. */
          base_tile->gen_flag &= ~IMA_GEN_TILE;
          LISTBASE_FOREACH (LinkData *, new_tile, &new_tiles) {
            int new_tile_number = POINTER_AS_INT(new_tile->data);
            BKE_image_add_tile(ima, new_tile_number, nullptr);
            if (new_tile_number == remaining_tile_number) {
              needs_final_cleanup = false;
            }
          }

          /* Final cleanup if the prior remaining tile was never encountered in the new list. */
          if (needs_final_cleanup) {
            BKE_image_remove_tile(ima, BKE_image_get_tile(ima, remaining_tile_number));
          }
        }
        BLI_freelistN(&new_tiles);
      }
      else if (ima->filepath[0] != '\0') {
        /* If the filepath is set at this point remove the generation flag. */
        ImageTile *base_tile = BKE_image_get_tile(ima, 0);
        base_tile->gen_flag &= ~IMA_GEN_TILE;
      }

      if (iuser) {
        image_tag_reload(ima, nullptr, iuser, ima);
      }
      BKE_image_walk_all_users(bmain, ima, image_tag_reload);
      break;
    case IMA_SIGNAL_USER_NEW_IMAGE:
      if (iuser) {
        if (ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_SEQUENCE, IMA_SRC_TILED)) {
          if (ima->type == IMA_TYPE_MULTILAYER) {
            BKE_image_init_imageuser(ima, iuser);
          }
        }
      }
      break;
    case IMA_SIGNAL_COLORMANAGE:
      BKE_image_free_buffers(ima);
      break;
  }

  /* NOTE: It is important that the image is unlocked before calling the node tree updates since
   * its update functions might need to acquire image buffers from this image. */
  ima->runtime->cache_mutex.unlock();

  BKE_ntree_update_tag_id_changed(bmain, &ima->id);
  BKE_ntree_update(*bmain);
}

/**
 * \return render-pass for a given pass index and active view.
 * fall back to available if there are missing passes for active view.
 */
static RenderPass *image_render_pass_get(RenderLayer *rl,
                                         const int pass,
                                         const int view,
                                         int *r_passindex)
{
  RenderPass *rpass_ret = nullptr;
  RenderPass *rpass;

  int rp_index = 0;
  const char *rp_name = "";

  for (rpass = static_cast<RenderPass *>(rl->passes.first); rpass; rpass = rpass->next, rp_index++)
  {
    if (rp_index == pass) {
      rpass_ret = rpass;
      if (view == 0) {
        /* no multiview or left eye */
        break;
      }

      rp_name = rpass->name;
    }
    /* multiview */
    else if (rp_name[0] && STREQ(rpass->name, rp_name) && (rpass->view_id == view)) {
      rpass_ret = rpass;
      break;
    }
  }

  /* fall back to the first pass in the layer */
  if (rpass_ret == nullptr) {
    rp_index = 0;
    rpass_ret = static_cast<RenderPass *>(rl->passes.first);
  }

  if (r_passindex) {
    *r_passindex = (rpass == rpass_ret ? rp_index : pass);
  }

  return rpass_ret;
}

int BKE_image_get_tile_label(const Image *ima,
                             const ImageTile *tile,
                             char *label,
                             const int label_maxncpy)
{
  label[0] = '\0';
  if (ima == nullptr || tile == nullptr) {
    return 0;
  }

  if (tile->label[0]) {
    return BLI_strncpy_rlen(label, tile->label, label_maxncpy);
  }
  return BLI_snprintf_rlen(label, label_maxncpy, "%d", tile->tile_number);
}

bool BKE_image_get_tile_info(char *filepath, ListBase *tiles, int *r_tile_start, int *r_tile_range)
{
  char filename[FILE_MAXFILE], dirname[FILE_MAXDIR];
  BLI_path_split_dir_file(filepath, dirname, sizeof(dirname), filename, sizeof(filename));

  if (!BKE_image_is_filename_tokenized(filename)) {
    BKE_image_ensure_tile_token_filename_only(filename, sizeof(filename));
  }

  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern = BKE_image_get_tile_strformat(filename, &tile_format);

  bool all_valid_udim = true;
  int min_udim = IMA_UDIM_MAX + 1;
  int max_udim = 0;
  int id;

  direntry *dirs;
  const uint dirs_num = BLI_filelist_dir_contents(dirname, &dirs);
  for (int i = 0; i < dirs_num; i++) {
    if (!(dirs[i].type & S_IFREG)) {
      continue;
    }

    if (!BKE_image_get_tile_number_from_filepath(dirs[i].relname, udim_pattern, tile_format, &id))
    {
      continue;
    }

    if (id < 1001 || id > IMA_UDIM_MAX) {
      all_valid_udim = false;
      break;
    }

    BLI_addtail(tiles, BLI_genericNodeN(POINTER_FROM_INT(id)));
    min_udim = min_ii(min_udim, id);
    max_udim = max_ii(max_udim, id);
  }
  BLI_filelist_free(dirs, dirs_num);
  MEM_SAFE_FREE(udim_pattern);

  if (all_valid_udim && min_udim <= IMA_UDIM_MAX) {
    BLI_path_join(filepath, FILE_MAX, dirname, filename);

    *r_tile_start = min_udim;
    *r_tile_range = max_udim - min_udim + 1;
    return true;
  }
  return false;
}

ImageTile *BKE_image_add_tile(Image *ima, int tile_number, const char *label)
{
  if (ima->source != IMA_SRC_TILED) {
    return nullptr;
  }

  if (tile_number < 1001 || tile_number > IMA_UDIM_MAX) {
    return nullptr;
  }

  /* Search the first tile that has a higher number.
   * We then insert before that to keep the list sorted. */
  ImageTile *next_tile;
  for (next_tile = static_cast<ImageTile *>(ima->tiles.first); next_tile;
       next_tile = next_tile->next)
  {
    if (next_tile->tile_number == tile_number) {
      /* Tile already exists. */
      return nullptr;
    }
    if (next_tile->tile_number > tile_number) {
      break;
    }
  }

  ImageTile *tile = imagetile_alloc(tile_number);

  if (next_tile) {
    BLI_insertlinkbefore(&ima->tiles, next_tile, tile);
  }
  else {
    BLI_addtail(&ima->tiles, tile);
  }

  if (label) {
    STRNCPY_UTF8(tile->label, label);
  }

  for (int eye = 0; eye < 2; eye++) {
    /* Reallocate GPU tile array. */
    if (ima->gputexture[TEXTARGET_2D_ARRAY][eye] != nullptr) {
      GPU_texture_free(ima->gputexture[TEXTARGET_2D_ARRAY][eye]);
      ima->gputexture[TEXTARGET_2D_ARRAY][eye] = nullptr;
    }
    if (ima->gputexture[TEXTARGET_TILE_MAPPING][eye] != nullptr) {
      GPU_texture_free(ima->gputexture[TEXTARGET_TILE_MAPPING][eye]);
      ima->gputexture[TEXTARGET_TILE_MAPPING][eye] = nullptr;
    }
  }
  BKE_image_partial_update_mark_full_update(ima);

  return tile;
}

bool BKE_image_remove_tile(Image *ima, ImageTile *tile)
{
  if (ima == nullptr || tile == nullptr || ima->source != IMA_SRC_TILED) {
    return false;
  }

  return image_remove_tile(ima, tile);
}

void BKE_image_reassign_tile(Image *ima, ImageTile *tile, int new_tile_number)
{
  if (ima == nullptr || tile == nullptr || ima->source != IMA_SRC_TILED) {
    return;
  }

  if (new_tile_number < 1001 || new_tile_number > IMA_UDIM_MAX) {
    return;
  }

  const int old_tile_number = tile->tile_number;
  tile->tile_number = new_tile_number;

  if (BKE_image_is_multiview(ima)) {
    const int totviews = BLI_listbase_count(&ima->views);
    for (int i = 0; i < totviews; i++) {
      ImBuf *ibuf = image_get_cached_ibuf_for_index_entry(ima, i, old_tile_number, nullptr);
      image_remove_ibuf(ima, i, old_tile_number);
      image_assign_ibuf(ima, ibuf, i, new_tile_number);
      IMB_freeImBuf(ibuf);
    }
  }
  else {
    ImBuf *ibuf = image_get_cached_ibuf_for_index_entry(ima, 0, old_tile_number, nullptr);
    image_remove_ibuf(ima, 0, old_tile_number);
    image_assign_ibuf(ima, ibuf, 0, new_tile_number);
    IMB_freeImBuf(ibuf);
  }

  for (int eye = 0; eye < 2; eye++) {
    /* Reallocate GPU tile array. */
    if (ima->gputexture[TEXTARGET_2D_ARRAY][eye] != nullptr) {
      GPU_texture_free(ima->gputexture[TEXTARGET_2D_ARRAY][eye]);
      ima->gputexture[TEXTARGET_2D_ARRAY][eye] = nullptr;
    }
    if (ima->gputexture[TEXTARGET_TILE_MAPPING][eye] != nullptr) {
      GPU_texture_free(ima->gputexture[TEXTARGET_TILE_MAPPING][eye]);
      ima->gputexture[TEXTARGET_TILE_MAPPING][eye] = nullptr;
    }
  }
  BKE_image_partial_update_mark_full_update(ima);
}

static int tile_sort_cb(const void *a, const void *b)
{
  const ImageTile *tile_a = static_cast<const ImageTile *>(a);
  const ImageTile *tile_b = static_cast<const ImageTile *>(b);
  return (tile_a->tile_number > tile_b->tile_number) ? 1 : 0;
}

void BKE_image_sort_tiles(Image *ima)
{
  if (ima == nullptr || ima->source != IMA_SRC_TILED) {
    return;
  }

  BLI_listbase_sort(&ima->tiles, tile_sort_cb);
}

bool BKE_image_fill_tile(Image *ima, ImageTile *tile)
{
  if (ima == nullptr || tile == nullptr || ima->source != IMA_SRC_TILED) {
    return false;
  }

  image_free_tile(ima, tile);

  ImBuf *tile_ibuf = add_ibuf_for_tile(ima, tile);

  if (tile_ibuf != nullptr) {
    image_assign_ibuf(ima, tile_ibuf, 0, tile->tile_number);
    BKE_image_release_ibuf(ima, tile_ibuf, nullptr);
    return true;
  }
  return false;
}

bool BKE_image_is_filename_tokenized(char *filepath)
{
  const char *filename = BLI_path_basename(filepath);
  return strstr(filename, "<UDIM>") != nullptr || strstr(filename, "<UVTILE>") != nullptr;
}

void BKE_image_ensure_tile_token_filename_only(char *filename, size_t filename_maxncpy)
{
  BLI_assert_msg(BLI_path_slash_find(filename) == nullptr,
                 "Only the file-name component should be used!");

  if (BKE_image_is_filename_tokenized(filename)) {
    return;
  }

  std::string path(filename);
  std::smatch match;

  /* General 4-digit "udim" pattern. As this format is susceptible to ambiguity
   * with other digit sequences, we can leverage the supported range of roughly
   * 1000 through 2000 to provide better detection. */
  std::regex pattern(R"((.*[._-])([12]\d{3})([._-].*))");
  if (std::regex_search(path, match, pattern)) {
    BLI_strncpy(filename, match.format("$1<UDIM>$3").c_str(), filename_maxncpy);
    return;
  }

  /* General `u##_v###` `uvtile` pattern. */
  pattern = std::regex(R"((.*)(u\d{1,2}_v\d{1,3})(\D.*))");
  if (std::regex_search(path, match, pattern)) {
    BLI_strncpy(filename, match.format("$1<UVTILE>$3").c_str(), filename_maxncpy);
    return;
  }
}

void BKE_image_ensure_tile_token(char *filepath, size_t filepath_maxncpy)
{
  char *filename = (char *)BLI_path_basename(filepath);
  BKE_image_ensure_tile_token_filename_only(filename, filepath_maxncpy - (filename - filepath));
}

bool BKE_image_tile_filepath_exists(const char *filepath)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  char dirname[FILE_MAXDIR];
  BLI_path_split_dir_part(filepath, dirname, sizeof(dirname));

  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern = BKE_image_get_tile_strformat(filepath, &tile_format);

  bool found = false;
  direntry *dirs;
  const uint dirs_num = BLI_filelist_dir_contents(dirname, &dirs);
  for (int i = 0; i < dirs_num; i++) {
    if (!(dirs[i].type & S_IFREG)) {
      continue;
    }

    int id;
    if (!BKE_image_get_tile_number_from_filepath(dirs[i].path, udim_pattern, tile_format, &id)) {
      continue;
    }

    if (id < 1001 || id > IMA_UDIM_MAX) {
      continue;
    }

    found = true;
    break;
  }
  BLI_filelist_free(dirs, dirs_num);
  MEM_SAFE_FREE(udim_pattern);

  return found;
}

char *BKE_image_get_tile_strformat(const char *filepath, eUDIM_TILE_FORMAT *r_tile_format)
{
  if (filepath == nullptr || r_tile_format == nullptr) {
    return nullptr;
  }

  if (strstr(filepath, "<UDIM>") != nullptr) {
    *r_tile_format = UDIM_TILE_FORMAT_UDIM;
    return BLI_string_replaceN(filepath, "<UDIM>", "%d");
  }
  if (strstr(filepath, "<UVTILE>") != nullptr) {
    *r_tile_format = UDIM_TILE_FORMAT_UVTILE;
    return BLI_string_replaceN(filepath, "<UVTILE>", "u%d_v%d");
  }

  *r_tile_format = UDIM_TILE_FORMAT_NONE;
  return nullptr;
}

bool BKE_image_get_tile_number_from_filepath(const char *filepath,
                                             const char *pattern,
                                             eUDIM_TILE_FORMAT tile_format,
                                             int *r_tile_number)
{
  if (filepath == nullptr || pattern == nullptr || r_tile_number == nullptr) {
    return false;
  }

  int u, v;
  bool result = false;

  if (tile_format == UDIM_TILE_FORMAT_UDIM) {
    if (sscanf(filepath, pattern, &u) == 1) {
      *r_tile_number = u;
      result = true;
    }
  }
  else if (tile_format == UDIM_TILE_FORMAT_UVTILE) {
    if (sscanf(filepath, pattern, &u, &v) == 2) {
      *r_tile_number = 1001 + (u - 1) + ((v - 1) * 10);
      result = true;
    }
  }

  return result;
}

void BKE_image_set_filepath_from_tile_number(char *filepath,
                                             const char *pattern,
                                             eUDIM_TILE_FORMAT tile_format,
                                             int tile_number)
{
  if (filepath == nullptr || pattern == nullptr) {
    return;
  }

  if (tile_format == UDIM_TILE_FORMAT_UDIM) {
    BLI_snprintf(filepath, FILE_MAX, pattern, tile_number);
  }
  else if (tile_format == UDIM_TILE_FORMAT_UVTILE) {
    int u = ((tile_number - 1001) % 10);
    int v = ((tile_number - 1001) / 10);
    BLI_snprintf(filepath, FILE_MAX, pattern, u + 1, v + 1);
  }
}

RenderPass *BKE_image_multilayer_index(RenderResult *rr, ImageUser *iuser)
{
  /* If layer or pass changes, we need an index for the imbufs list. */
  /* NOTE: it is called for rendered results, but it doesn't use the index! */

  RenderLayer *rl;
  RenderPass *rpass = nullptr;

  if (rr == nullptr) {
    return nullptr;
  }

  if (iuser) {
    short index = 0, rv_index, rl_index = 0;
    bool is_stereo = (iuser->flag & IMA_SHOW_STEREO) && RE_RenderResult_is_stereo(rr);

    rv_index = is_stereo ? iuser->multiview_eye : iuser->view;
    if (RE_HasCombinedLayer(rr)) {
      rl_index += 1;
    }

    for (rl = static_cast<RenderLayer *>(rr->layers.first); rl; rl = rl->next, rl_index++) {
      if (iuser->layer == rl_index) {
        int rp_index;
        rpass = image_render_pass_get(rl, iuser->pass, rv_index, &rp_index);
        iuser->multi_index = index + rp_index;
        break;
      }

      index += BLI_listbase_count(&rl->passes);
    }
  }

  return rpass;
}

void BKE_image_multiview_index(const Image *ima, ImageUser *iuser)
{
  if (iuser) {
    bool is_stereo = BKE_image_is_stereo(ima) && (iuser->flag & IMA_SHOW_STEREO);
    if (is_stereo) {
      iuser->multi_index = iuser->multiview_eye;
    }
    else {
      if ((iuser->view < 0) ||
          (iuser->view >= BLI_listbase_count_at_most(&ima->views, iuser->view + 1)))
      {
        iuser->multi_index = iuser->view = 0;
      }
      else {
        iuser->multi_index = iuser->view;
      }
    }
  }
}

bool BKE_image_is_multilayer(const Image *ima)
{
  /* If layer or pass changes, we need an index for the imbufs list. */
  /* NOTE:  it is called for rendered results, but it doesn't use the index! */

  if (ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_SEQUENCE, IMA_SRC_TILED)) {
    if (ima->type == IMA_TYPE_MULTILAYER) {
      return true;
    }
  }
  else if (ima->source == IMA_SRC_VIEWER) {
    if (ima->type == IMA_TYPE_R_RESULT) {
      return true;
    }
  }
  return false;
}

bool BKE_image_is_multiview(const Image *ima)
{
  ImageView *view = static_cast<ImageView *>(ima->views.first);
  return (view && (view->next || view->name[0]));
}

bool BKE_image_is_stereo(const Image *ima)
{
  return BKE_image_is_multiview(ima) &&
         (BLI_findstring(&ima->views, STEREO_LEFT_NAME, offsetof(ImageView, name)) &&
          BLI_findstring(&ima->views, STEREO_RIGHT_NAME, offsetof(ImageView, name)));
}

static void image_init_multilayer_multiview(Image *ima, RenderResult *rr)
{
  /* update image views from render views, but only if they actually changed,
   * to avoid invalid memory access during render. ideally these should always
   * be acquired with a mutex along with the render result, but there are still
   * some places with just an image pointer that need to access views */
  if (rr && BLI_listbase_count(&ima->views) == BLI_listbase_count(&rr->views)) {
    ImageView *iv = static_cast<ImageView *>(ima->views.first);
    RenderView *rv = static_cast<RenderView *>(rr->views.first);
    bool modified = false;
    for (; rv; rv = rv->next, iv = iv->next) {
      modified |= !STREQ(rv->name, iv->name);
    }
    if (!modified) {
      return;
    }
  }

  BKE_image_free_views(ima);

  if (rr) {
    LISTBASE_FOREACH (RenderView *, rv, &rr->views) {
      ImageView *iv = MEM_callocN<ImageView>("Viewer Image View");
      STRNCPY_UTF8(iv->name, rv->name);
      BLI_addtail(&ima->views, iv);
    }
  }
}

RenderResult *BKE_image_acquire_renderresult(Scene *scene, Image *ima)
{
  std::scoped_lock lock(ima->runtime->cache_mutex);

  RenderResult *rr = nullptr;

  if (ima->rr) {
    rr = ima->rr;
  }
  else if (scene && ima->type == IMA_TYPE_R_RESULT) {
    if (ima->render_slot == ima->last_render_slot) {
      rr = RE_AcquireResultRead(RE_GetSceneRender(scene));
    }
    else {
      rr = BKE_image_get_renderslot(ima, ima->render_slot)->render;
      BKE_image_partial_update_mark_full_update(ima);
    }

    /* set proper views */
    image_init_multilayer_multiview(ima, rr);
  }

  if (rr) {
    RE_ReferenceRenderResult(rr);
  }

  return rr;
}

void BKE_image_release_renderresult(Scene *scene, Image *ima, RenderResult *render_result)
{
  if (render_result) {
    /* Decrement user counter, and free if nobody else holds a reference to the result. */
    std::scoped_lock lock(ima->runtime->cache_mutex);
    RE_FreeRenderResult(render_result);
  }

  if (ima->rr) {
    /* pass */
  }
  else if (scene && ima->type == IMA_TYPE_R_RESULT) {
    if (ima->render_slot == ima->last_render_slot) {
      RE_ReleaseResult(RE_GetSceneRender(scene));
    }
  }
}

bool BKE_image_is_openexr(Image *ima)
{
#ifdef WITH_IMAGE_OPENEXR
  if (ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_SEQUENCE, IMA_SRC_TILED)) {
    return BLI_path_extension_check(ima->filepath, ".exr");
  }
#else
  UNUSED_VARS(ima);
#endif
  return false;
}

void BKE_image_backup_render(Scene *scene, Image *ima, bool free_current_slot)
{
  /* called right before rendering, ima->renderslots contains render
   * result pointers for everything but the current render */
  Render *re = RE_GetSceneRender(scene);

  /* Ensure we always have a valid render slot. */
  if (!ima->renderslots.first) {
    BKE_image_add_renderslot(ima, nullptr);
    ima->render_slot = 0;
    ima->last_render_slot = 0;
  }
  else if (ima->render_slot >= BLI_listbase_count(&ima->renderslots)) {
    ima->render_slot = 0;
    ima->last_render_slot = 0;
  }

  RenderSlot *last_slot = BKE_image_get_renderslot(ima, ima->last_render_slot);
  RenderSlot *cur_slot = BKE_image_get_renderslot(ima, ima->render_slot);

  if (last_slot && ima->render_slot != ima->last_render_slot) {
    last_slot->render = nullptr;
    RE_SwapResult(re, &last_slot->render);

    if (cur_slot->render) {
      if (free_current_slot) {
        BKE_image_clear_renderslot(ima, nullptr, ima->render_slot);
      }
      else {
        RE_SwapResult(re, &cur_slot->render);
      }
    }
  }

  ima->last_render_slot = ima->render_slot;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Multiview Load OpenEXR
 * \{ */

static void image_add_view(Image *ima, const char *viewname, const char *filepath)
{
  ImageView *iv;

  iv = MEM_mallocN<ImageView>("Viewer Image View");
  STRNCPY_UTF8(iv->name, viewname);
  STRNCPY(iv->filepath, filepath);

  /* For stereo drawing we need to ensure:
   * STEREO_LEFT_NAME  == STEREO_LEFT_ID and
   * STEREO_RIGHT_NAME == STEREO_RIGHT_ID */

  if (STREQ(viewname, STEREO_LEFT_NAME)) {
    BLI_addhead(&ima->views, iv);
  }
  else if (STREQ(viewname, STEREO_RIGHT_NAME)) {
    ImageView *left_iv = static_cast<ImageView *>(
        BLI_findstring(&ima->views, STEREO_LEFT_NAME, offsetof(ImageView, name)));

    if (left_iv == nullptr) {
      BLI_addhead(&ima->views, iv);
    }
    else {
      BLI_insertlinkafter(&ima->views, left_iv, iv);
    }
  }
  else {
    BLI_addtail(&ima->views, iv);
  }
}

/* After imbuf load, OpenEXR type can return with a EXR-handle open
 * in that case we have to build a render-result. */
#ifdef WITH_IMAGE_OPENEXR
static void image_create_multilayer(Image *ima, ImBuf *ibuf, int framenr)
{
  const char *colorspace = ima->colorspace_settings.name;
  bool predivide = (ima->alpha_mode == IMA_ALPHA_PREMUL);

  /* only load rr once for multiview */
  if (!ima->rr) {
    ima->rr = RE_MultilayerConvert(ibuf->exrhandle, colorspace, predivide, ibuf->x, ibuf->y);
  }

  IMB_exr_close(ibuf->exrhandle);

  ibuf->exrhandle = nullptr;
  if (ima->rr != nullptr) {
    ima->rr->framenr = framenr;
    BKE_stamp_info_from_imbuf(ima->rr, ibuf);
  }

  /* set proper views */
  image_init_multilayer_multiview(ima, ima->rr);
}
#endif /* WITH_IMAGE_OPENEXR */

/** Common stuff to do with images after loading. */
static void image_init_after_load(Image *ima, ImageUser *iuser, ImBuf * /*ibuf*/)
{
  /* Preview is null when it has never been used as an icon before.
   * Never handle previews/icons outside of main thread. */
  if (G.background == 0 && ima->preview == nullptr && BLI_thread_is_main()) {
    BKE_icon_changed(BKE_icon_id_ensure(&ima->id));
  }

  /* timer */
  BKE_image_tag_time(ima);

  ImageTile *tile = BKE_image_get_tile_from_iuser(ima, iuser);
  /* Images should never get loaded if the corresponding tile does not exist,
   * but we should at least not crash if it happens due to a bug elsewhere. */
  BLI_assert(tile != nullptr);
  UNUSED_VARS_NDEBUG(tile);
}

static int imbuf_alpha_flags_for_image(Image *ima)
{
  switch (ima->alpha_mode) {
    case IMA_ALPHA_STRAIGHT:
      return 0;
    case IMA_ALPHA_PREMUL:
      return IB_alphamode_premul;
    case IMA_ALPHA_CHANNEL_PACKED:
      return IB_alphamode_channel_packed;
    case IMA_ALPHA_IGNORE:
      return IB_alphamode_ignore;
  }

  return 0;
}

/**
 * \return the number of files will vary according to the stereo format.
 */
static int image_num_viewfiles(Image *ima)
{
  const bool is_multiview = BKE_image_is_multiview(ima);

  if (!is_multiview) {
    return 1;
  }
  if (ima->views_format == R_IMF_VIEWS_STEREO_3D) {
    return 1;
  }
  /* R_IMF_VIEWS_INDIVIDUAL */

  return BLI_listbase_count(&ima->views);
}

static ImBuf *image_load_sequence_multilayer(Image *ima, ImageUser *iuser, int entry, int frame)
{
  ImBuf *ibuf = nullptr;

  /* either we load from RenderResult, or we have to load a new one */

  /* check for new RenderResult */
  if (ima->rr == nullptr || frame != ima->rr->framenr) {
    if (ima->rr) {
      /* Cached image buffers shares pointers with render result,
       * need to ensure there's no image buffers are hanging around
       * with dead links after freeing the render result.
       */
      image_free_cached_frames(ima);
      RE_FreeRenderResult(ima->rr);
      ima->rr = nullptr;
    }

    ibuf = image_load_image_file(ima, iuser, entry, frame, true);

    if (ibuf) { /* actually an error */
      ima->type = IMA_TYPE_IMAGE;
      printf("error, multi is normal image\n");
    }
  }
  if (ima->rr) {
    RenderPass *rpass = BKE_image_multilayer_index(ima->rr, iuser);

    if (rpass && rpass->ibuf) {
      ibuf = rpass->ibuf;
      IMB_refImBuf(ibuf);

      BKE_imbuf_stamp_info(ima->rr, ibuf);
      copy_v2_v2_db(ibuf->ppm, ima->rr->ppm);

      image_init_after_load(ima, iuser, ibuf);
      image_assign_ibuf(ima, ibuf, iuser ? iuser->multi_index : 0, entry);
    }
    // else printf("pass not found\n");
  }

  return ibuf;
}

static ImBuf *load_movie_single(Image *ima, ImageUser *iuser, int frame, const int view_id)
{
  ImBuf *ibuf = nullptr;
  ImageAnim *ia;

  ia = static_cast<ImageAnim *>(BLI_findlink(&ima->anims, view_id));

  if (ia->anim == nullptr) {
    char filepath[FILE_MAX];
    int flags = IB_byte_data;
    ImageUser iuser_t{};

    if (ima->flag & IMA_DEINTERLACE) {
      flags |= IB_animdeinterlace;
    }

    if (iuser) {
      iuser_t = *iuser;
    }

    iuser_t.view = view_id;

    BKE_image_user_file_path(&iuser_t, ima, filepath);

    /* FIXME: make several stream accessible in image editor, too. */
    ia->anim = openanim(filepath, flags, 0, false, ima->colorspace_settings.name);

    /* let's initialize this user */
    if (ia->anim && iuser && iuser->frames == 0) {
      iuser->frames = MOV_get_duration_frames(ia->anim, IMB_TC_RECORD_RUN);
    }
  }

  if (ia->anim) {
    int dur = MOV_get_duration_frames(ia->anim, IMB_TC_RECORD_RUN);
    int fra = frame - 1;

    fra = std::max(fra, 0);
    fra = std::min(fra, dur - 1);
    ibuf = IMB_makeSingleUser(MOV_decode_frame(ia->anim, fra, IMB_TC_RECORD_RUN, IMB_PROXY_NONE));

    if (ibuf) {
      image_init_after_load(ima, iuser, ibuf);
    }
  }

  return ibuf;
}

static ImBuf *image_load_movie_file(Image *ima, ImageUser *iuser, int frame)
{
  ImBuf *ibuf = nullptr;
  const bool is_multiview = BKE_image_is_multiview(ima);
  const int tot_viewfiles = image_num_viewfiles(ima);

  if (!BLI_listbase_count_is_equal_to(&ima->anims, tot_viewfiles)) {
    image_free_anims(ima);

    for (int i = 0; i < tot_viewfiles; i++) {
      /* allocate the ImageAnim */
      ImageAnim *ia = MEM_callocN<ImageAnim>("Image Anim");
      BLI_addtail(&ima->anims, ia);
    }
  }

  if (!is_multiview) {
    ibuf = load_movie_single(ima, iuser, frame, 0);
    image_assign_ibuf(ima, ibuf, 0, frame);
  }
  else {
    const int totviews = BLI_listbase_count(&ima->views);
    Array<ImBuf *> ibuf_arr(totviews);

    for (int i = 0; i < tot_viewfiles; i++) {
      ibuf_arr[i] = load_movie_single(ima, iuser, frame, i);
    }

    if (BKE_image_is_stereo(ima) && ima->views_format == R_IMF_VIEWS_STEREO_3D) {
      IMB_ImBufFromStereo3d(ima->stereo3d_format, ibuf_arr[0], ibuf_arr.data(), &ibuf_arr[1]);
    }

    for (int i = 0; i < totviews; i++) {
      image_assign_ibuf(ima, ibuf_arr[i], i, frame);
    }

    /* return the original requested ImBuf */
    ibuf = ibuf_arr[(iuser ? iuser->multi_index : 0)];

    /* "remove" the others (decrease their refcount) */
    for (int i = 0; i < totviews; i++) {
      if (ibuf_arr[i] != ibuf) {
        IMB_freeImBuf(ibuf_arr[i]);
      }
    }
  }

  return ibuf;
}

static ImBuf *load_image_single(Image *ima,
                                ImageUser *iuser,
                                int cfra,
                                const int view_id,
                                const bool has_packed,
                                const bool is_sequence,
                                bool *r_cache_ibuf)
{
  char filepath[FILE_MAX];
  ImBuf *ibuf = nullptr;
  int flag = IB_byte_data | IB_multilayer | IB_metadata | imbuf_alpha_flags_for_image(ima);

  *r_cache_ibuf = true;
  const int tile_number = image_get_tile_number_from_iuser(ima, iuser);

  /* is there a PackedFile with this image ? */
  if (has_packed && !is_sequence) {
    LISTBASE_FOREACH (ImagePackedFile *, imapf, &ima->packedfiles) {
      if (imapf->view == view_id && imapf->tile_number == tile_number) {
        if (imapf->packedfile) {
          ibuf = IMB_load_image_from_memory((uchar *)imapf->packedfile->data,
                                            imapf->packedfile->size,
                                            flag,
                                            "<packed data>",
                                            nullptr,
                                            ima->colorspace_settings.name);
        }
        break;
      }
    }
  }
  else {
    if (is_sequence) {
      ima->lastframe = cfra;
    }

    /* get the correct filepath */
    const bool is_tiled = (ima->source == IMA_SRC_TILED);
    if (!(is_sequence || is_tiled)) {
      BKE_image_user_frame_calc(ima, iuser, cfra);
    }

    ImageUser iuser_t{};
    if (iuser) {
      iuser_t = *iuser;
    }
    else {
      iuser_t.framenr = ima->lastframe;
    }

    iuser_t.view = view_id;

    BKE_image_user_file_path(&iuser_t, ima, filepath);

    /* read ibuf */
    ibuf = IMB_load_image_from_filepath(filepath, flag, ima->colorspace_settings.name);
  }

  if (ibuf) {
#ifdef WITH_IMAGE_OPENEXR
    if (ibuf->ftype == IMB_FTYPE_OPENEXR && ibuf->exrhandle) {
      /* Handle multilayer and multiview cases, don't assign ibuf here.
       * will be set layer in BKE_image_acquire_ibuf from ima->rr. */
      if (IMB_exr_has_multilayer(ibuf->exrhandle)) {
        image_create_multilayer(ima, ibuf, cfra);
        ima->type = IMA_TYPE_MULTILAYER;
        IMB_freeImBuf(ibuf);
        ibuf = nullptr;
        /* Null ibuf in the cache means the image failed to load. However for multilayer we load
         * pixels into RenderResult instead and intentionally leave ibuf null. */
        *r_cache_ibuf = false;
      }
    }
    else
#endif
    {
      image_init_after_load(ima, iuser, ibuf);

      /* Make packed file for auto-pack. */
      if (!is_sequence && (has_packed == false) && (G.fileflags & G_FILE_AUTOPACK)) {
        ImagePackedFile *imapf = MEM_mallocN<ImagePackedFile>("Image Pack-file");
        BLI_addtail(&ima->packedfiles, imapf);

        STRNCPY(imapf->filepath, filepath);
        imapf->view = view_id;
        imapf->tile_number = tile_number;
        imapf->packedfile = BKE_packedfile_new(
            nullptr, filepath, ID_BLEND_PATH_FROM_GLOBAL(&ima->id));
      }
    }
  }

  return ibuf;
}

/* warning, 'iuser' can be null
 * NOTE: Image->views was already populated (in image_update_views_format)
 */
static ImBuf *image_load_image_file(
    Image *ima, ImageUser *iuser, int entry, int cfra, bool is_sequence)
{
  ImBuf *ibuf = nullptr;
  const bool is_multiview = BKE_image_is_multiview(ima);
  const bool is_tiled = (ima->source == IMA_SRC_TILED);
  const int tot_viewfiles = image_num_viewfiles(ima);
  bool has_packed = BKE_image_has_packedfile(ima);

  if (!(is_sequence || is_tiled)) {
    /* ensure clean ima */
    BKE_image_free_buffers(ima);
  }

  /* this should never happen, but just playing safe */
  if (!is_sequence && has_packed) {
    const int totfiles = tot_viewfiles * BLI_listbase_count(&ima->tiles);
    if (!BLI_listbase_count_is_equal_to(&ima->packedfiles, totfiles)) {
      image_free_packedfiles(ima);
      has_packed = false;
    }
  }

  if (!is_multiview) {
    bool put_in_cache;
    ibuf = load_image_single(ima, iuser, cfra, 0, has_packed, is_sequence, &put_in_cache);
    if (put_in_cache) {
      const int index = (is_sequence || is_tiled) ? 0 : IMA_NO_INDEX;
      image_assign_ibuf(ima, ibuf, index, entry);
    }
  }
  else {
    const int totviews = BLI_listbase_count(&ima->views);
    BLI_assert(totviews > 0);

    Array<ImBuf *> ibuf_arr(totviews);
    Array<bool> cache_ibuf_arr(totviews);

    for (int i = 0; i < tot_viewfiles; i++) {
      ibuf_arr[i] = load_image_single(
          ima, iuser, cfra, i, has_packed, is_sequence, &cache_ibuf_arr[i]);
    }

    /* multi-views/multi-layers OpenEXR files directly populate ima, and return null ibuf... */
    if (BKE_image_is_stereo(ima) && ima->views_format == R_IMF_VIEWS_STEREO_3D && ibuf_arr[0] &&
        tot_viewfiles == 1 && totviews >= 2)
    {
      IMB_ImBufFromStereo3d(ima->stereo3d_format, ibuf_arr[0], ibuf_arr.data(), &ibuf_arr[1]);
    }

    /* return the original requested ImBuf */
    const int ibuf_index = (iuser && iuser->multi_index < totviews) ? iuser->multi_index : 0;
    ibuf = ibuf_arr[ibuf_index];

    for (int i = 0; i < totviews; i++) {
      if (cache_ibuf_arr[i]) {
        image_assign_ibuf(ima, ibuf_arr[i], i, entry);
      }
    }

    /* "remove" the others (decrease their refcount) */
    for (int i = 0; i < totviews; i++) {
      if (ibuf_arr[i] != ibuf) {
        IMB_freeImBuf(ibuf_arr[i]);
      }
    }
  }

  return ibuf;
}

static ImBuf *image_get_ibuf_multilayer(Image *ima, ImageUser *iuser)
{
  ImBuf *ibuf = nullptr;

  if (ima->rr == nullptr) {
    ibuf = image_load_image_file(ima, iuser, 0, 0, false);
    if (ibuf) { /* actually an error */
      ima->type = IMA_TYPE_IMAGE;
      return ibuf;
    }
  }
  if (ima->rr) {
    RenderPass *rpass = BKE_image_multilayer_index(ima->rr, iuser);

    if (rpass && rpass->ibuf) {
      ibuf = rpass->ibuf;
      IMB_refImBuf(ibuf);

      image_init_after_load(ima, iuser, ibuf);

      BKE_imbuf_stamp_info(ima->rr, ibuf);
      copy_v2_v2_db(ibuf->ppm, ima->rr->ppm);

      image_assign_ibuf(ima, ibuf, iuser ? iuser->multi_index : IMA_NO_INDEX, 0);
    }
  }

  return ibuf;
}

/* showing RGBA result itself (from compo/sequence) or
 * like exr, using layers etc */
/* always returns a single ibuf, also during render progress */
static ImBuf *image_get_render_result(Image *ima, ImageUser *iuser, void **r_lock)
{
  RenderView *rv;
  ImBuf *pass_ibuf = nullptr;
  float dither;
  const int from_render = (ima->render_slot == ima->last_render_slot);

  if (!(iuser && iuser->scene)) {
    return nullptr;
  }

  /* if we the caller is not going to release the lock, don't give the image */
  if (!r_lock) {
    return nullptr;
  }

  Render *re = RE_GetSceneRender(iuser->scene);

  const int layer = iuser->layer;
  const int pass = iuser->pass;
  int actview = iuser->view;

  if (BKE_image_is_stereo(ima) && (iuser->flag & IMA_SHOW_STEREO)) {
    actview = iuser->multiview_eye;
  }

  RenderResult rres{};
  RenderSlot *slot;
  if (from_render) {
    RE_AcquireResultImage(re, &rres, actview);
  }
  else if ((slot = BKE_image_get_renderslot(ima, ima->render_slot))->render) {
    rres = *(slot->render);
    rres.have_combined = ((RenderView *)rres.views.first)->ibuf != nullptr;
  }

  if (!(rres.rectx > 0 && rres.recty > 0)) {
    if (from_render) {
      RE_ReleaseResultImage(re);
    }
    return nullptr;
  }

  /* release is done in BKE_image_release_ibuf using r_lock */
  if (from_render) {
    BLI_thread_lock(LOCK_VIEWER);
    *r_lock = re;
    rv = nullptr;
  }
  else {
    rv = static_cast<RenderView *>(BLI_findlink(&rres.views, actview));
    if (rv == nullptr) {
      rv = static_cast<RenderView *>(rres.views.first);
    }
  }

  /* this gives active layer, composite or sequence result */
  if (rv == nullptr) {
    pass_ibuf = rres.ibuf;
  }
  else {
    pass_ibuf = rv->ibuf;
  }

  dither = iuser->scene->r.dither_intensity;

  /* combined layer gets added as first layer */
  if (rres.have_combined && layer == 0) {
    /* pass */
  }
  else if (pass_ibuf && pass_ibuf->byte_buffer.data && layer == 0) {
    /* pass */
  }
  else if (rres.layers.first) {
    RenderLayer *rl = static_cast<RenderLayer *>(
        BLI_findlink(&rres.layers, layer - (rres.have_combined ? 1 : 0)));
    if (rl) {
      RenderPass *rpass = image_render_pass_get(rl, pass, actview, nullptr);
      if (rpass) {
        pass_ibuf = rpass->ibuf;
        if (pass != 0) {
          dither = 0.0f; /* don't dither passes */
        }
      }
    }
  }

  /* Put an empty image buffer to the cache. This allows to achieve the following:
   *
   * 1. It makes it so the generic logic in the #BKE_image_has_loaded_ibuf properly detects that
   * an Image used to display render result has loaded image buffer.
   *
   * Surely there are all the design questions about scene-dependent Render Result image
   * data-block, and the behavior of the flag dependent on whether the Render Result image was ever
   * shown on screen. The purpose of this code is to preserve the Python API behavior to the level
   * prior to the #RenderResult refactor to use #ImBuf which happened for Blender 4.0.
   *
   * 2. Provides an image buffer which can be used to communicate the render resolution (with
   * possible border render applied to it) prior to the actual pixels storage is allocated. */
  if (ima->cache == nullptr) {
    ImBuf *empty_ibuf = IMB_allocImBuf(0, 0, 0, 0);
    image_assign_ibuf(ima, empty_ibuf, IMA_NO_INDEX, 0);

    /* The cache references the image buffer, and the freeing only happens if the buffer has 0
     * references at the time when the #IMB_freeImBuf() is called. This particular image buffer is
     * to be freed together with the cache, without any extra reference counting done by any image
     * pixel accessor. */
    IMB_freeImBuf(empty_ibuf);
  }

  if (pass_ibuf) {
    /* TODO(@sergey): Perhaps its better to assign dither when #ImBuf is allocated for the render
     * result. It will avoid modification here, and allow comparing render results with different
     * dither applied to them. */
    pass_ibuf->dither = dither;

    IMB_refImBuf(pass_ibuf);
  }
  else {
    pass_ibuf = image_get_cached_ibuf_for_index_entry(ima, IMA_NO_INDEX, 0, nullptr);

    /* Assign the current render resolution to the image buffer.
     * The actual storage is still empty. The intended use is to merely communicate the actual
     * render resolution prior to render border is "un-cropped". */
    pass_ibuf->x = rres.rectx;
    pass_ibuf->y = rres.recty;
  }

  return pass_ibuf;
}

static int image_get_multiview_index(Image *ima, ImageUser *iuser)
{
  const bool is_multilayer = BKE_image_is_multilayer(ima);
  const bool is_backdrop = (ima->source == IMA_SRC_VIEWER) && (ima->type == IMA_TYPE_COMPOSITE) &&
                           (iuser == nullptr);
  int index = BKE_image_has_multiple_ibufs(ima) ? 0 : IMA_NO_INDEX;

  if (is_multilayer) {
    return iuser ? iuser->multi_index : index;
  }
  if (is_backdrop) {
    if (BKE_image_is_stereo(ima)) {
      /* Backdrop hack / workaround (since there is no `iuser`). */
      return ima->eye;
    }
  }
  else if (BKE_image_is_multiview(ima)) {
    return iuser ? iuser->multi_index : index;
  }

  return index;
}

static void image_get_entry_and_index(Image *ima, ImageUser *iuser, int *r_entry, int *r_index)
{
  int frame = 0, index = image_get_multiview_index(ima, iuser);

  /* see if we already have an appropriate ibuf, with image source and type */
  if (ima->source == IMA_SRC_MOVIE) {
    frame = iuser ? iuser->framenr : ima->lastframe;
  }
  else if (ima->source == IMA_SRC_SEQUENCE) {
    if (ima->type == IMA_TYPE_IMAGE) {
      frame = iuser ? iuser->framenr : ima->lastframe;
    }
    else if (ima->type == IMA_TYPE_MULTILAYER) {
      frame = iuser ? iuser->framenr : ima->lastframe;
    }
  }
  else if (ima->source == IMA_SRC_TILED) {
    frame = image_get_tile_number_from_iuser(ima, iuser);
  }

  *r_entry = frame;
  *r_index = index;
}

/* Get the ibuf from an image cache for a given image user.
 *
 * Returns referenced image buffer if it exists, callee is to
 * call IMB_freeImBuf to de-reference the image buffer after
 * it's done handling it.
 */
static ImBuf *image_get_cached_ibuf(
    Image *ima, ImageUser *iuser, int *r_entry, int *r_index, bool *r_is_cached_empty)
{
  ImBuf *ibuf = nullptr;
  int entry = 0, index = image_get_multiview_index(ima, iuser);

  /* see if we already have an appropriate ibuf, with image source and type */
  if (ima->source == IMA_SRC_MOVIE) {
    entry = iuser ? iuser->framenr : ima->lastframe;
    ibuf = image_get_cached_ibuf_for_index_entry(ima, index, entry, r_is_cached_empty);
    ima->lastframe = entry;
  }
  else if (ima->source == IMA_SRC_SEQUENCE) {
    if (ima->type == IMA_TYPE_IMAGE) {
      entry = iuser ? iuser->framenr : ima->lastframe;
      ibuf = image_get_cached_ibuf_for_index_entry(ima, index, entry, r_is_cached_empty);
      ima->lastframe = entry;
    }
    else if (ima->type == IMA_TYPE_MULTILAYER) {
      entry = iuser ? iuser->framenr : ima->lastframe;
      ibuf = image_get_cached_ibuf_for_index_entry(ima, index, entry, r_is_cached_empty);
    }
  }
  else if (ima->source == IMA_SRC_FILE) {
    if (ima->type == IMA_TYPE_IMAGE) {
      ibuf = image_get_cached_ibuf_for_index_entry(ima, index, 0, r_is_cached_empty);
    }
    else if (ima->type == IMA_TYPE_MULTILAYER) {
      ibuf = image_get_cached_ibuf_for_index_entry(ima, index, 0, r_is_cached_empty);
    }
  }
  else if (ima->source == IMA_SRC_GENERATED) {
    ibuf = image_get_cached_ibuf_for_index_entry(ima, index, 0, r_is_cached_empty);
  }
  else if (ima->source == IMA_SRC_VIEWER) {
    /* always verify entirely, not that this shouldn't happen
     * as part of texture sampling in rendering anyway, so not
     * a big bottleneck */
  }
  else if (ima->source == IMA_SRC_TILED) {
    if (ELEM(ima->type, IMA_TYPE_IMAGE, IMA_TYPE_MULTILAYER)) {
      entry = image_get_tile_number_from_iuser(ima, iuser);
      ibuf = image_get_cached_ibuf_for_index_entry(ima, index, entry, r_is_cached_empty);
    }
  }

  if (r_entry) {
    *r_entry = entry;
  }

  if (r_index) {
    *r_index = index;
  }

  return ibuf;
}

BLI_INLINE bool image_quick_test(Image *ima, const ImageUser *iuser)
{
  if (ima == nullptr) {
    return false;
  }

  ImageTile *tile = BKE_image_get_tile_from_iuser(ima, iuser);
  if (tile == nullptr) {
    return false;
  }

  return true;
}

/**
 * Checks optional #ImageUser and verifies/creates #ImBuf.
 *
 * \warning Not thread-safe, so callee should worry about thread locks.
 */
static ImBuf *image_acquire_ibuf(Image *ima, ImageUser *iuser, void **r_lock)
{
  ImBuf *ibuf = nullptr;
  int entry = 0, index = 0;

  if (r_lock) {
    *r_lock = nullptr;
  }

  /* quick reject tests */
  if (!image_quick_test(ima, iuser)) {
    return nullptr;
  }

  bool is_cached_empty = false;
  ibuf = image_get_cached_ibuf(ima, iuser, &entry, &index, &is_cached_empty);
  if (is_cached_empty) {
    return nullptr;
  }

  if (ibuf == nullptr) {
    /* We are sure we have to load the ibuf, using source and type. */
    if (ima->source == IMA_SRC_MOVIE) {
      /* Source is from single file, use flip-book to store ibuf. */
      ibuf = image_load_movie_file(ima, iuser, entry);
    }
    else if (ima->source == IMA_SRC_SEQUENCE) {
      if (ima->type == IMA_TYPE_IMAGE) {
        /* Regular files, ibufs in flip-book, allows saving. */
        ibuf = image_load_image_file(ima, iuser, entry, entry, true);
      }
      /* no else; on load the ima type can change */
      if (ima->type == IMA_TYPE_MULTILAYER) {
        /* Only 1 layer/pass stored in imbufs, no EXR-handle anim storage, no saving. */
        ibuf = image_load_sequence_multilayer(ima, iuser, entry, entry);
      }
    }
    else if (ima->source == IMA_SRC_TILED) {
      /* Nothing was cached. Check to see if the tile should be generated. */
      ImageTile *tile = BKE_image_get_tile(ima, entry);
      if ((tile->gen_flag & IMA_GEN_TILE) != 0) {
        ibuf = add_ibuf_for_tile(ima, tile);
        image_assign_ibuf(ima, ibuf, 0, entry);
      }
      else {
        if (ima->type == IMA_TYPE_IMAGE) {
          /* Regular files, ibufs in flip-book, allows saving */
          ibuf = image_load_image_file(ima, iuser, entry, 0, false);
        }
        /* no else; on load the ima type can change */
        if (ima->type == IMA_TYPE_MULTILAYER) {
          /* Only 1 layer/pass stored in imbufs, no EXR-handle anim storage, no saving. */
          ibuf = image_load_sequence_multilayer(ima, iuser, entry, 0);
        }
      }
    }
    else if (ima->source == IMA_SRC_FILE) {

      if (ima->type == IMA_TYPE_IMAGE) {
        ibuf = image_load_image_file(
            ima, iuser, 0, entry, false); /* cfra only for '#', this global is OK */
      }
      /* no else; on load the ima type can change */
      if (ima->type == IMA_TYPE_MULTILAYER) {
        /* keeps render result, stores ibufs in listbase, allows saving */
        ibuf = image_get_ibuf_multilayer(ima, iuser);
      }
    }
    else if (ima->source == IMA_SRC_GENERATED) {
      /* Generated is: `ibuf` is allocated dynamically. */
      /* UV test-grid or black or solid etc. */
      ImageTile *base_tile = BKE_image_get_tile(ima, 0);
      if (base_tile->gen_x == 0) {
        base_tile->gen_x = 1024;
      }
      if (base_tile->gen_y == 0) {
        base_tile->gen_y = 1024;
      }
      if (base_tile->gen_depth == 0) {
        base_tile->gen_depth = 24;
      }
      ibuf = add_ibuf_for_tile(ima, base_tile);
      image_assign_ibuf(ima, ibuf, index, 0);
    }
    else if (ima->source == IMA_SRC_VIEWER) {
      if (ima->type == IMA_TYPE_R_RESULT) {
        /* always verify entirely, and potentially
         * returns pointer to release later */
        ibuf = image_get_render_result(ima, iuser, r_lock);
      }
      else if (ima->type == IMA_TYPE_COMPOSITE) {
        /* requires lock/unlock, otherwise don't return image */
        if (r_lock) {
          /* unlock in BKE_image_release_ibuf */
          BLI_thread_lock(LOCK_VIEWER);
          *r_lock = ima;

          /* XXX anim play for viewer nodes not yet supported */
          entry = 0;  // XXX iuser ? iuser->framenr : 0;
          ibuf = image_get_cached_ibuf_for_index_entry(ima, index, entry, nullptr);

          if (!ibuf) {
            /* Composite Viewer, all handled in compositor */
            /* fake ibuf, will be filled in compositor */
            ibuf = IMB_allocImBuf(256, 256, 32, IB_byte_data | IB_float_data);
            image_assign_ibuf(ima, ibuf, index, entry);
          }
        }
      }
    }

    /* We only want movies and sequences to be memory limited. */
    if (ibuf != nullptr && !ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
      ibuf->userflags |= IB_PERSISTENT;
    }
  }

  BKE_image_tag_time(ima);

  return ibuf;
}

ImBuf *BKE_image_acquire_ibuf(Image *ima, ImageUser *iuser, void **r_lock)
{
  /* NOTE: same as #image_acquire_ibuf, but can be used to retrieve images being rendered in
   * a thread safe way, always call both acquire and release. */

  if (ima == nullptr) {
    return nullptr;
  }

  std::scoped_lock lock(ima->runtime->cache_mutex);
  return image_acquire_ibuf(ima, iuser, r_lock);
}

static int get_multilayer_view_index(const Image &image,
                                     const ImageUser &image_user,
                                     const char *view_name)
{
  if (BLI_listbase_count_at_most(&image.rr->views, 2) <= 1) {
    return 0;
  }

  const int view_image = image_user.view;
  const bool is_allview = (view_image == 0); /* if view selected == All (0) */

  if (is_allview) {
    /* Heuristic to match image name with scene names check if the view name exists in the image.
     */
    const int view = BLI_findstringindex(&image.rr->views, view_name, offsetof(RenderView, name));
    if (view == -1) {
      return 0;
    }
    return view;
  }

  return view_image - 1;
}

ImBuf *BKE_image_acquire_multilayer_view_ibuf(const RenderData &render_data,
                                              Image &image,
                                              const ImageUser &image_user,
                                              const char *pass_name,
                                              const char *view_name)
{
  std::scoped_lock lock(image.runtime->cache_mutex);

  /* Local changes to the original ImageUser. */
  ImageUser local_user = image_user;

  /* Force load the image once, possibly with a different user from what it will need to be in the
   * end. This ensures proper image type, and initializes multi-layer state when needed. */
  ImBuf *tmp_ibuf = image_acquire_ibuf(&image, &local_user, nullptr);
  IMB_freeImBuf(tmp_ibuf);

  if (BKE_image_is_multilayer(&image)) {
    BLI_assert(pass_name);

    if (!image.rr) {
      return nullptr;
    }

    const RenderLayer *render_layer = static_cast<const RenderLayer *>(
        BLI_findlink(&image.rr->layers, local_user.layer));

    local_user.view = get_multilayer_view_index(image, local_user, view_name);
    local_user.pass = BLI_findstringindex(
        &render_layer->passes, pass_name, offsetof(RenderPass, name));

    if (!BKE_image_multilayer_index(image.rr, &local_user)) {
      return nullptr;
    }
  }
  else {
    local_user.multi_index = BKE_scene_multiview_view_id_get(&render_data, view_name);
  }

  return image_acquire_ibuf(&image, &local_user, nullptr);
}

void BKE_image_release_ibuf(Image *ima, ImBuf *ibuf, void *lock)
{
  if (lock != nullptr) {
    /* for getting image during threaded render / compositing, need to release */
    if (lock == ima) {
      BLI_thread_unlock(LOCK_VIEWER); /* viewer image */
    }
    else {
      RE_ReleaseResultImage(static_cast<Render *>(lock)); /* render result */
      BLI_thread_unlock(LOCK_VIEWER);                     /* view image imbuf */
    }
  }

  if (ibuf) {
    std::scoped_lock lock(ima->runtime->cache_mutex);
    IMB_freeImBuf(ibuf);
  }
}

bool BKE_image_has_ibuf(Image *ima, ImageUser *iuser)
{
  ImBuf *ibuf;

  /* quick reject tests */
  if (!image_quick_test(ima, iuser)) {
    return false;
  }

  std::scoped_lock lock(ima->runtime->cache_mutex);

  ibuf = image_get_cached_ibuf(ima, iuser, nullptr, nullptr, nullptr);

  if (!ibuf) {
    ibuf = image_acquire_ibuf(ima, iuser, nullptr);
  }

  IMB_freeImBuf(ibuf);

  return ibuf != nullptr;
}

ImBuf *BKE_image_preview(Image *ima, const short max_size, short *r_width, short *r_height)
{
  void *lock;
  ImBuf *image_ibuf = BKE_image_acquire_ibuf(ima, nullptr, &lock);
  if (image_ibuf == nullptr) {
    return nullptr;
  }

  ImBuf *preview = IMB_dupImBuf(image_ibuf);
  float scale = float(max_size) / float(std::max(image_ibuf->x, image_ibuf->y));
  if (r_width) {
    *r_width = image_ibuf->x;
  }
  if (r_height) {
    *r_height = image_ibuf->y;
  }
  BKE_image_release_ibuf(ima, image_ibuf, lock);

  /* Resize. */
  IMB_scale(preview, scale * image_ibuf->x, scale * image_ibuf->y, IMBScaleFilter::Box, false);
  IMB_byte_from_float(preview);

  return preview;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pool for Image Buffers
 * \{ */

struct ImagePoolItem {
  ImagePoolItem *next, *prev;
  Image *image;
  ImBuf *ibuf;
  int index;
  int entry;
};

struct ImagePool {
  ListBase image_buffers = {};
  BLI_mempool *memory_pool = nullptr;
  blender::Mutex mutex;
};

ImagePool *BKE_image_pool_new()
{
  ImagePool *pool = MEM_new<ImagePool>(__func__);
  pool->memory_pool = BLI_mempool_create(sizeof(ImagePoolItem), 0, 128, BLI_MEMPOOL_NOP);
  return pool;
}

void BKE_image_pool_free(ImagePool *pool)
{
  /* Use single lock to dereference all the image buffers. */
  {
    std::scoped_lock lock(pool->mutex);
    for (ImagePoolItem *item = static_cast<ImagePoolItem *>(pool->image_buffers.first);
         item != nullptr;
         item = item->next)
    {
      if (item->ibuf != nullptr) {
        std::scoped_lock lock(item->image->runtime->cache_mutex);
        IMB_freeImBuf(item->ibuf);
      }
    }

    BLI_mempool_destroy(pool->memory_pool);
  }
  MEM_delete(pool);
}

BLI_INLINE ImBuf *image_pool_find_item(
    ImagePool *pool, Image *image, int entry, int index, bool *r_found)
{
  LISTBASE_FOREACH (ImagePoolItem *, item, &pool->image_buffers) {
    if (item->image == image && item->entry == entry && item->index == index) {
      *r_found = true;
      return item->ibuf;
    }
  }

  *r_found = false;
  return nullptr;
}

ImBuf *BKE_image_pool_acquire_ibuf(Image *ima, ImageUser *iuser, ImagePool *pool)
{
  ImBuf *ibuf;
  int index, entry;
  bool found;

  if (!image_quick_test(ima, iuser)) {
    return nullptr;
  }

  if (pool == nullptr) {
    /* Pool could be null, in this case use general acquire function. */
    return BKE_image_acquire_ibuf(ima, iuser, nullptr);
  }

  image_get_entry_and_index(ima, iuser, &entry, &index);

  /* Use double-checked locking, to avoid locking when the requested image buffer is already in the
   * pool. */

  ibuf = image_pool_find_item(pool, ima, entry, index, &found);
  if (found) {
    return ibuf;
  }

  /* Lock the pool, to allow thread-safe modification of the content of the pool. */
  std::scoped_lock lock(pool->mutex);

  ibuf = image_pool_find_item(pool, ima, entry, index, &found);

  /* Will also create item even in cases image buffer failed to load,
   * prevents trying to load the same buggy file multiple times. */
  if (!found) {
    ImagePoolItem *item;

    /* Thread-safe acquisition of an image buffer from the image.
     * The acquisition does not use image pools, so there is no risk of recursive or out-of-order
     * mutex locking. */
    ibuf = BKE_image_acquire_ibuf(ima, iuser, nullptr);

    item = static_cast<ImagePoolItem *>(BLI_mempool_alloc(pool->memory_pool));
    item->image = ima;
    item->entry = entry;
    item->index = index;
    item->ibuf = ibuf;

    BLI_addtail(&pool->image_buffers, item);
  }

  return ibuf;
}

void BKE_image_pool_release_ibuf(Image *ima, ImBuf *ibuf, ImagePool *pool)
{
  /* if pool wasn't actually used, use general release stuff,
   * for pools image buffers will be dereferenced on pool free
   */
  if (pool == nullptr) {
    BKE_image_release_ibuf(ima, ibuf, nullptr);
  }
}

int BKE_image_user_frame_get(const ImageUser *iuser, int cfra, bool *r_is_in_range)
{
  const int len = iuser->frames;

  if (r_is_in_range) {
    *r_is_in_range = false;
  }

  if (len == 0) {
    return 0;
  }

  int framenr;
  cfra = cfra - iuser->sfra + 1;

  /* cyclic */
  if (iuser->cycl) {
    cfra = ((cfra) % len);
    if (cfra < 0) {
      cfra += len;
    }
    if (cfra == 0) {
      cfra = len;
    }

    if (r_is_in_range) {
      *r_is_in_range = true;
    }
  }

  if (cfra < 0) {
    cfra = 0;
  }
  else if (cfra > len) {
    cfra = len;
  }
  else {
    if (r_is_in_range) {
      *r_is_in_range = true;
    }
  }

  /* transform to images space */
  framenr = cfra;
  framenr = std::min(framenr, iuser->frames);

  if (iuser->cycl) {
    framenr = ((framenr) % len);
    while (framenr < 0) {
      framenr += len;
    }
    if (framenr == 0) {
      framenr = len;
    }
  }

  /* important to apply after else we can't loop on frames 100 - 110 for eg. */
  framenr += iuser->offset;

  return framenr;
}

void BKE_image_user_frame_calc(Image *ima, ImageUser *iuser, int cfra)
{
  if (iuser) {
    if (ima && BKE_image_is_animated(ima)) {
      /* Compute current frame for animated image. */
      bool is_in_range;
      const int framenr = BKE_image_user_frame_get(iuser, cfra, &is_in_range);

      if (is_in_range) {
        iuser->flag |= IMA_USER_FRAME_IN_RANGE;
      }
      else {
        iuser->flag &= ~IMA_USER_FRAME_IN_RANGE;
      }

      iuser->framenr = framenr;
    }
    else {
      /* Set fixed frame number for still image. */
      iuser->framenr = 0;
      iuser->flag |= IMA_USER_FRAME_IN_RANGE;
    }

    if (ima && ima->gpuframenr != iuser->framenr) {
      /* NOTE: a single texture and refresh doesn't really work when
       * multiple image users may use different frames, this is to
       * be improved with perhaps a GPU texture cache. */
      if (ima->gpuframenr != IMAGE_GPU_FRAME_NONE) {
        BKE_image_partial_update_mark_full_update(ima);
      }
      ima->gpuframenr = iuser->framenr;
    }

    iuser->flag &= ~IMA_NEED_FRAME_RECALC;
  }
}

/* goes over all ImageUsers, and sets frame numbers if auto-refresh is set */
static void image_editors_update_frame(Image *ima,
                                       ID * /*iuser_id*/,
                                       ImageUser *iuser,
                                       void *customdata)
{
  if (ima && BKE_image_is_animated(ima)) {
    if ((iuser->flag & IMA_ANIM_ALWAYS) || (iuser->flag & IMA_NEED_FRAME_RECALC)) {
      int cfra = *(int *)customdata;

      BKE_image_user_frame_calc(ima, iuser, cfra);
    }
  }
}

void BKE_image_editors_update_frame(const Main *bmain, int cfra)
{
  /* This only updates images used by the user interface. For others the
   * dependency graph will call BKE_image_user_id_eval_animation. */
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  image_walk_id_all_users(&wm->id, false, &cfra, image_editors_update_frame);
}

static void image_user_id_has_animation(Image *ima,
                                        ID * /*iuser_id*/,
                                        ImageUser * /*iuser*/,
                                        void *customdata)
{
  if (ima && BKE_image_is_animated(ima)) {
    *(bool *)customdata = true;
  }
}

bool BKE_image_user_id_has_animation(ID *id)
{
  /* For the dependency graph, this does not consider nested node
   * trees as these are handled as their own data-block. */
  bool has_animation = false;
  bool skip_nested_nodes = true;
  image_walk_id_all_users(id, skip_nested_nodes, &has_animation, image_user_id_has_animation);
  return has_animation;
}

static void image_user_id_eval_animation(Image *ima,
                                         ID * /*iduser_id*/,
                                         ImageUser *iuser,
                                         void *customdata)
{
  if (ima && BKE_image_is_animated(ima)) {
    Depsgraph *depsgraph = (Depsgraph *)customdata;

    if ((iuser->flag & IMA_ANIM_ALWAYS) || (iuser->flag & IMA_NEED_FRAME_RECALC) ||
        (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER))
    {
      float cfra = DEG_get_ctime(depsgraph);

      BKE_image_user_frame_calc(ima, iuser, cfra);
    }
  }
}

void BKE_image_user_id_eval_animation(Depsgraph *depsgraph, ID *id)
{
  /* This is called from the dependency graph to update the image
   * users in data-blocks. It computes the current frame number
   * and tags the image to be refreshed.
   * This does not consider nested node trees as these are handled
   * as their own data-block. */
  bool skip_nested_nodes = true;
  image_walk_id_all_users(id, skip_nested_nodes, depsgraph, image_user_id_eval_animation);
}

void BKE_image_user_file_path(const ImageUser *iuser, const Image *ima, char *filepath)
{
  BKE_image_user_file_path_ex(G_MAIN, iuser, ima, filepath, true, true);
}

void BKE_image_user_file_path_ex(const Main *bmain,
                                 const ImageUser *iuser,
                                 const Image *ima,
                                 char *filepath,
                                 const bool resolve_udim,
                                 const bool resolve_multiview)
{
  if (resolve_multiview && BKE_image_is_multiview(ima)) {
    ImageView *iv = static_cast<ImageView *>(BLI_findlink(&ima->views, iuser->view));
    if (iv->filepath[0]) {
      BLI_strncpy(filepath, iv->filepath, FILE_MAX);
    }
    else {
      BLI_strncpy(filepath, ima->filepath, FILE_MAX);
    }
  }
  else {
    BLI_strncpy(filepath, ima->filepath, FILE_MAX);
  }

  if (ELEM(ima->source, IMA_SRC_SEQUENCE, IMA_SRC_TILED)) {
    char head[FILE_MAX], tail[FILE_MAX];
    ushort numlen;

    int index;
    if (ima->source == IMA_SRC_SEQUENCE) {
      index = iuser ? iuser->framenr : ima->lastframe;
      BLI_path_sequence_decode(filepath, head, sizeof(head), tail, sizeof(tail), &numlen);
      BLI_path_sequence_encode(filepath, FILE_MAX, head, tail, numlen, index);
    }
    else if (resolve_udim) {
      index = image_get_tile_number_from_iuser(ima, iuser);

      eUDIM_TILE_FORMAT tile_format;
      char *udim_pattern = BKE_image_get_tile_strformat(filepath, &tile_format);
      BKE_image_set_filepath_from_tile_number(filepath, udim_pattern, tile_format, index);
      MEM_SAFE_FREE(udim_pattern);
    }
  }

  BLI_path_abs(filepath, ID_BLEND_PATH(bmain, &ima->id));
}

bool BKE_image_has_alpha(Image *image)
{
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, nullptr, &lock);
  const int planes = (ibuf ? ibuf->planes : 0);
  BKE_image_release_ibuf(image, ibuf, lock);

  if (ELEM(planes, 32, 16)) {
    return true;
  }

  return false;
}

void BKE_image_get_size(Image *image, ImageUser *iuser, int *r_width, int *r_height)
{
  ImBuf *ibuf = nullptr;
  void *lock;

  if (image != nullptr) {
    ibuf = BKE_image_acquire_ibuf(image, iuser, &lock);
  }

  if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
    *r_width = ibuf->x;
    *r_height = ibuf->y;
  }
  else if (image != nullptr && image->type == IMA_TYPE_R_RESULT && iuser != nullptr &&
           iuser->scene != nullptr)
  {
    BKE_render_resolution(&iuser->scene->r, true, r_width, r_height);
  }
  else {
    *r_width = IMG_SIZE_FALLBACK;
    *r_height = IMG_SIZE_FALLBACK;
  }

  if (image != nullptr) {
    BKE_image_release_ibuf(image, ibuf, lock);
  }
}

void BKE_image_get_size_fl(Image *image, ImageUser *iuser, float r_size[2])
{
  int width, height;
  BKE_image_get_size(image, iuser, &width, &height);

  r_size[0] = float(width);
  r_size[1] = float(height);
}

void BKE_image_get_aspect(Image *image, float *r_aspx, float *r_aspy)
{
  *r_aspx = 1.0;

  /* x is always 1 */
  if (image) {
    *r_aspy = image->aspy / image->aspx;
  }
  else {
    *r_aspy = 1.0f;
  }
}

uchar *BKE_image_get_pixels_for_frame(Image *image, int frame, int tile)
{
  ImageUser iuser;
  BKE_imageuser_default(&iuser);
  void *lock;
  ImBuf *ibuf;
  uchar *pixels = nullptr;

  iuser.framenr = frame;
  iuser.tile = tile;

  ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

  if (ibuf) {
    pixels = ibuf->byte_buffer.data;

    if (pixels) {
      pixels = static_cast<uchar *>(MEM_dupallocN(pixels));
    }

    BKE_image_release_ibuf(image, ibuf, lock);
  }

  if (!pixels) {
    return nullptr;
  }

  return pixels;
}

float *BKE_image_get_float_pixels_for_frame(Image *image, int frame, int tile)
{
  ImageUser iuser;
  BKE_imageuser_default(&iuser);
  void *lock;
  ImBuf *ibuf;
  float *pixels = nullptr;

  iuser.framenr = frame;
  iuser.tile = tile;

  ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

  if (ibuf) {
    pixels = ibuf->float_buffer.data;

    if (pixels) {
      pixels = static_cast<float *>(MEM_dupallocN(pixels));
    }

    BKE_image_release_ibuf(image, ibuf, lock);
  }

  if (!pixels) {
    return nullptr;
  }

  return pixels;
}

int BKE_image_sequence_guess_offset(Image *image)
{
  return BLI_path_sequence_decode(image->filepath, nullptr, 0, nullptr, 0, nullptr);
}

bool BKE_image_has_anim(Image *ima)
{
  return (BLI_listbase_is_empty(&ima->anims) == false);
}

bool BKE_image_has_packedfile(const Image *ima)
{
  return (BLI_listbase_is_empty(&ima->packedfiles) == false);
}

bool BKE_image_has_filepath(const Image *ima)
{
  /* This could be improved to detect cases like //../../, currently path
   * remapping empty file paths empty. */
  return ima->filepath[0] != '\0';
}

bool BKE_image_is_animated(Image *image)
{
  return ELEM(image->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE);
}

bool BKE_image_has_multiple_ibufs(Image *image)
{
  return ELEM(image->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE, IMA_SRC_TILED);
}

bool BKE_image_is_dirty_writable(Image *image, bool *r_is_writable)
{
  bool is_dirty = false;
  bool is_writable = false;

  std::scoped_lock lock(image->runtime->cache_mutex);
  if (image->cache != nullptr) {
    MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

    while (!IMB_moviecacheIter_done(iter)) {
      ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
      if (ibuf != nullptr && ibuf->userflags & IB_BITMAPDIRTY) {
        is_writable = BKE_image_buffer_format_writable(ibuf);
        is_dirty = true;
        break;
      }
      IMB_moviecacheIter_step(iter);
    }
    IMB_moviecacheIter_free(iter);
  }

  if (r_is_writable) {
    *r_is_writable = is_writable;
  }

  return is_dirty;
}

bool BKE_image_is_dirty(Image *image)
{
  return BKE_image_is_dirty_writable(image, nullptr);
}

void BKE_image_mark_dirty(Image * /*image*/, ImBuf *ibuf)
{
  ibuf->userflags |= IB_BITMAPDIRTY;
}

bool BKE_image_buffer_format_writable(ImBuf *ibuf)
{
  ImageFormatData im_format;
  ImbFormatOptions options_dummy;
  BKE_image_format_from_imbuf(&im_format, ibuf);
  return (BKE_imtype_to_ftype(im_format.imtype, &options_dummy) == ibuf->ftype);
}

void BKE_image_file_format_set(Image *image, int ftype, const ImbFormatOptions *options)
{
  std::scoped_lock lock(image->runtime->cache_mutex);
  if (image->cache != nullptr) {
    MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

    while (!IMB_moviecacheIter_done(iter)) {
      ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
      if (ibuf != nullptr) {
        ibuf->ftype = static_cast<eImbFileType>(ftype);
        ibuf->foptions = *options;
      }
      IMB_moviecacheIter_step(iter);
    }
    IMB_moviecacheIter_free(iter);
  }
}

bool BKE_image_has_loaded_ibuf(Image *image)
{
  bool has_loaded_ibuf = false;

  std::scoped_lock lock(image->runtime->cache_mutex);
  if (image->cache != nullptr) {
    MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

    while (!IMB_moviecacheIter_done(iter)) {
      ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
      if (ibuf != nullptr) {
        has_loaded_ibuf = true;
        break;
      }
      IMB_moviecacheIter_step(iter);
    }
    IMB_moviecacheIter_free(iter);
  }

  return has_loaded_ibuf;
}

ImBuf *BKE_image_get_ibuf_with_name(Image *image, const char *filepath)
{
  BLI_assert(!BLI_path_is_rel(filepath));
  ImBuf *ibuf = nullptr;

  std::scoped_lock lock(image->runtime->cache_mutex);
  if (image->cache != nullptr) {
    MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

    while (!IMB_moviecacheIter_done(iter)) {
      ImBuf *current_ibuf = IMB_moviecacheIter_getImBuf(iter);
      if (current_ibuf != nullptr && STREQ(current_ibuf->filepath, filepath)) {
        ibuf = current_ibuf;
        IMB_refImBuf(ibuf);
        break;
      }
      IMB_moviecacheIter_step(iter);
    }
    IMB_moviecacheIter_free(iter);
  }

  return ibuf;
}

ImBuf *BKE_image_get_first_ibuf(Image *image)
{
  ImBuf *ibuf = nullptr;

  std::scoped_lock lock(image->runtime->cache_mutex);
  if (image->cache != nullptr) {
    MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

    while (!IMB_moviecacheIter_done(iter)) {
      ibuf = IMB_moviecacheIter_getImBuf(iter);
      if (ibuf != nullptr) {
        IMB_refImBuf(ibuf);
      }
      break;
    }
    IMB_moviecacheIter_free(iter);
  }

  return ibuf;
}

static void image_update_views_format(Image *ima, ImageUser *iuser)
{
  ImageView *iv;
  Scene *scene = iuser->scene;
  const bool is_multiview = ((scene->r.scemode & R_MULTIVIEW) != 0) &&
                            ((ima->flag & IMA_USE_VIEWS) != 0);

  /* reset the image views */
  BKE_image_free_views(ima);

  if (!is_multiview) {
    /* nothing to do */
  }
  else if (ima->views_format == R_IMF_VIEWS_STEREO_3D) {
    const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

    for (int i = 0; i < 2; i++) {
      image_add_view(ima, names[i], ima->filepath);
    }
    return;
  }
  else {
    /* R_IMF_VIEWS_INDIVIDUAL */
    char prefix[FILE_MAX] = {'\0'};
    char *filepath = ima->filepath;
    const char *ext = nullptr;

    BKE_scene_multiview_view_prefix_get(scene, filepath, prefix, &ext);

    if (prefix[0] == '\0') {
      BKE_image_free_views(ima);
      return;
    }

    /* create all the image views */
    LISTBASE_FOREACH (SceneRenderView *, srv, &scene->r.views) {
      if (BKE_scene_multiview_is_render_view_active(&scene->r, srv)) {
        char filepath_view[FILE_MAX];
        SNPRINTF(filepath_view, "%s%s%s", prefix, srv->suffix, ext);
        image_add_view(ima, srv->name, filepath_view);
      }
    }

    /* check if the files are all available */
    iv = static_cast<ImageView *>(ima->views.last);
    while (iv) {
      int file;
      char filepath[FILE_MAX];

      STRNCPY(filepath, iv->filepath);
      BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&ima->id));

      /* exists? */
      file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
      if (file == -1) {
        ImageView *iv_del = iv;
        iv = iv->prev;
        BLI_remlink(&ima->views, iv_del);
        MEM_freeN(iv_del);
      }
      else {
        iv = iv->prev;
        close(file);
      }
    }

    /* all good */
    if (!BKE_image_is_multiview(ima)) {
      BKE_image_free_views(ima);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Slots
 * \{ */

RenderSlot *BKE_image_add_renderslot(Image *ima, const char *name)
{
  RenderSlot *slot = MEM_callocN<RenderSlot>("Image new Render Slot");
  if (name && name[0]) {
    STRNCPY_UTF8(slot->name, name);
  }
  else {
    int n = BLI_listbase_count(&ima->renderslots) + 1;
    SNPRINTF_UTF8(slot->name, DATA_("Slot %d"), n);
  }
  BLI_addtail(&ima->renderslots, slot);
  return slot;
}

bool BKE_image_remove_renderslot(Image *ima, ImageUser *iuser, int slot)
{
  if (slot == ima->last_render_slot) {
    /* Don't remove render slot while rendering to it. */
    if (G.is_rendering) {
      return false;
    }
  }

  int num_slots = BLI_listbase_count(&ima->renderslots);
  if (slot >= num_slots || num_slots == 1) {
    return false;
  }

  RenderSlot *remove_slot = static_cast<RenderSlot *>(BLI_findlink(&ima->renderslots, slot));
  RenderSlot *current_slot = static_cast<RenderSlot *>(
      BLI_findlink(&ima->renderslots, ima->render_slot));
  RenderSlot *current_last_slot = static_cast<RenderSlot *>(
      BLI_findlink(&ima->renderslots, ima->last_render_slot));

  RenderSlot *next_slot;
  if (current_slot == remove_slot) {
    next_slot = static_cast<RenderSlot *>(
        BLI_findlink(&ima->renderslots, (slot == num_slots - 1) ? slot - 1 : slot + 1));
  }
  else {
    next_slot = current_slot;
  }

  /* If the slot to be removed is the slot with the last render,
   * make another slot the last render slot. */
  if (remove_slot == current_last_slot) {
    /* Choose the currently selected slot unless that one is being removed,
     * in that case take the next one. */
    RenderSlot *next_last_slot;
    if (current_slot == remove_slot) {
      next_last_slot = next_slot;
    }
    else {
      next_last_slot = current_slot;
    }

    if (iuser == nullptr || iuser->scene == nullptr) {
      return false;
    }
    Render *re = RE_GetSceneRender(iuser->scene);
    if (re != nullptr) {
      RE_SwapResult(re, &current_last_slot->render);
      RE_SwapResult(re, &next_last_slot->render);
    }
    current_last_slot = next_last_slot;
  }

  current_slot = next_slot;

  BLI_remlink(&ima->renderslots, remove_slot);

  ima->render_slot = BLI_findindex(&ima->renderslots, current_slot);
  ima->last_render_slot = BLI_findindex(&ima->renderslots, current_last_slot);

  if (remove_slot->render) {
    RE_FreeRenderResult(remove_slot->render);
  }
  MEM_freeN(remove_slot);

  return true;
}

bool BKE_image_clear_renderslot(Image *ima, ImageUser *iuser, int slot)
{
  if (slot == ima->last_render_slot) {
    if (!iuser) {
      return false;
    }
    if (G.is_rendering) {
      return false;
    }
    Render *re = RE_GetSceneRender(iuser->scene);
    if (!re) {
      return false;
    }
    RE_ClearResult(re);
    return true;
  }

  RenderSlot *render_slot = static_cast<RenderSlot *>(BLI_findlink(&ima->renderslots, slot));
  if (!render_slot) {
    return false;
  }
  if (render_slot->render) {
    RE_FreeRenderResult(render_slot->render);
    render_slot->render = nullptr;
  }
  return true;
}

RenderSlot *BKE_image_get_renderslot(Image *ima, int index)
{
  /* Can be null for images without render slots. */
  return static_cast<RenderSlot *>(BLI_findlink(&ima->renderslots, index));
}

/** \} */

/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <optional>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include <ctime>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"

#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_bpath.hh"
#include "BKE_colortools.hh"
#include "BKE_idtype.hh"
#include "BKE_image.hh" /* openanim */
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_movieclip.h"
#include "BKE_node_tree_update.hh"
#include "BKE_tracking.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_moviecache.hh"
#include "IMB_openexr.hh"

#include "MOV_read.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_engine.hh"

#include "GPU_texture.hh"

#include "BLO_read_write.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"gpu.texture"};

static void free_buffers(MovieClip *clip);

/** Reset runtime mask fields when data-block is being initialized. */
static void movie_clip_runtime_reset(MovieClip *clip)
{
  /* TODO: we could store those in undo cache storage as well, and preserve them instead of
   * re-creating them... */
  BLI_listbase_clear(&clip->runtime.gputextures);

  clip->runtime.last_update = 0;
}

static void movie_clip_init_data(ID *id)
{
  MovieClip *movie_clip = (MovieClip *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(movie_clip, id));

  MEMCPY_STRUCT_AFTER(movie_clip, DNA_struct_default_get(MovieClip), id);

  BKE_tracking_settings_init(&movie_clip->tracking);
  BKE_color_managed_colorspace_settings_init(&movie_clip->colorspace_settings);
}

static void movie_clip_copy_data(Main * /*bmain*/,
                                 std::optional<Library *> /*owner_library*/,
                                 ID *id_dst,
                                 const ID *id_src,
                                 const int flag)
{
  MovieClip *movie_clip_dst = (MovieClip *)id_dst;
  const MovieClip *movie_clip_src = (const MovieClip *)id_src;

  /* We never handle user-count here for owned data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  movie_clip_dst->anim = nullptr;
  movie_clip_dst->cache = nullptr;

  BKE_tracking_copy(&movie_clip_dst->tracking, &movie_clip_src->tracking, flag_subdata);
  movie_clip_dst->tracking_context = nullptr;

  BKE_color_managed_colorspace_settings_copy(&movie_clip_dst->colorspace_settings,
                                             &movie_clip_src->colorspace_settings);
}

static void movie_clip_free_data(ID *id)
{
  MovieClip *movie_clip = (MovieClip *)id;

  /* Also frees animation-data. */
  free_buffers(movie_clip);

  BKE_tracking_free(&movie_clip->tracking);
}

static void movie_clip_foreach_id(ID *id, LibraryForeachIDData *data)
{
  MovieClip *movie_clip = (MovieClip *)id;
  MovieTracking *tracking = &movie_clip->tracking;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, movie_clip->gpd, IDWALK_CB_USER);

  LISTBASE_FOREACH (MovieTrackingObject *, object, &tracking->objects) {
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &object->tracks) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, track->gpd, IDWALK_CB_USER);
    }
    LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, &object->plane_tracks) {
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, plane_track->image, IDWALK_CB_USER);
    }
  }
}

static void movie_clip_foreach_cache(ID *id,
                                     IDTypeForeachCacheFunctionCallback function_callback,
                                     void *user_data)
{
  MovieClip *movie_clip = (MovieClip *)id;
  IDCacheKey key{};
  key.id_session_uid = id->session_uid;
  key.identifier = offsetof(MovieClip, cache);
  function_callback(id, &key, (void **)&movie_clip->cache, 0, user_data);

  key.identifier = offsetof(MovieClip, tracking.camera.intrinsics);
  function_callback(id, &key, (&movie_clip->tracking.camera.intrinsics), 0, user_data);
}

static void movie_clip_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  MovieClip *movie_clip = (MovieClip *)id;
  BKE_bpath_foreach_path_fixed_process(
      bpath_data, movie_clip->filepath, sizeof(movie_clip->filepath));
}

static void write_movieTracks(BlendWriter *writer, ListBase *tracks)
{
  MovieTrackingTrack *track;

  track = static_cast<MovieTrackingTrack *>(tracks->first);
  while (track) {
    BLO_write_struct(writer, MovieTrackingTrack, track);

    if (track->markers) {
      BLO_write_struct_array(writer, MovieTrackingMarker, track->markersnr, track->markers);
    }

    track = track->next;
  }
}

static void write_moviePlaneTracks(BlendWriter *writer, ListBase *plane_tracks_base)
{
  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, plane_tracks_base) {
    BLO_write_struct(writer, MovieTrackingPlaneTrack, plane_track);

    BLO_write_pointer_array(writer, plane_track->point_tracksnr, plane_track->point_tracks);
    BLO_write_struct_array(
        writer, MovieTrackingPlaneMarker, plane_track->markersnr, plane_track->markers);
  }
}

static void write_movieReconstruction(BlendWriter *writer,
                                      MovieTrackingReconstruction *reconstruction)
{
  if (reconstruction->camnr) {
    BLO_write_struct_array(
        writer, MovieReconstructedCamera, reconstruction->camnr, reconstruction->cameras);
  }
}

static void movieclip_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  MovieClip *clip = (MovieClip *)id;

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  clip->anim = nullptr;
  clip->tracking_context = nullptr;
  clip->tracking.stats = nullptr;

  MovieTracking *tracking = &clip->tracking;

  BLO_write_id_struct(writer, MovieClip, id_address, &clip->id);
  BKE_id_blend_write(writer, &clip->id);

  LISTBASE_FOREACH (MovieTrackingObject *, object, &tracking->objects) {
    BLO_write_struct(writer, MovieTrackingObject, object);
    write_movieTracks(writer, &object->tracks);
    write_moviePlaneTracks(writer, &object->plane_tracks);
    write_movieReconstruction(writer, &object->reconstruction);
  }
}

static void direct_link_movieReconstruction(BlendDataReader *reader,
                                            MovieTrackingReconstruction *reconstruction)
{
  BLO_read_struct_array(
      reader, MovieReconstructedCamera, reconstruction->camnr, &reconstruction->cameras);
}

static void direct_link_movieTracks(BlendDataReader *reader, ListBase *tracksbase)
{
  BLO_read_struct_list(reader, MovieTrackingTrack, tracksbase);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, tracksbase) {
    BLO_read_struct_array(reader, MovieTrackingMarker, track->markersnr, &track->markers);
  }
}

static void direct_link_moviePlaneTracks(BlendDataReader *reader, ListBase *plane_tracks_base)
{
  BLO_read_struct_list(reader, MovieTrackingPlaneTrack, plane_tracks_base);

  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, plane_tracks_base) {
    BLO_read_pointer_array(
        reader, plane_track->point_tracksnr, (void **)&plane_track->point_tracks);
    for (int i = 0; i < plane_track->point_tracksnr; i++) {
      BLO_read_struct(reader, MovieTrackingTrack, &plane_track->point_tracks[i]);
    }

    BLO_read_struct_array(
        reader, MovieTrackingPlaneMarker, plane_track->markersnr, &plane_track->markers);
  }
}

static void movieclip_blend_read_data(BlendDataReader *reader, ID *id)
{
  MovieClip *clip = (MovieClip *)id;
  MovieTracking *tracking = &clip->tracking;

  direct_link_movieTracks(reader, &tracking->tracks_legacy);
  direct_link_moviePlaneTracks(reader, &tracking->plane_tracks_legacy);
  direct_link_movieReconstruction(reader, &tracking->reconstruction_legacy);

  BLO_read_struct(reader, MovieTrackingTrack, &clip->tracking.act_track_legacy);
  BLO_read_struct(reader, MovieTrackingPlaneTrack, &clip->tracking.act_plane_track_legacy);

  clip->anim = nullptr;
  clip->tracking_context = nullptr;
  clip->tracking.stats = nullptr;

  /* Needed for proper versioning, will be nullptr for all newer files anyway. */
  BLO_read_struct(reader, MovieTrackingTrack, &clip->tracking.stabilization.rot_track_legacy);

  clip->tracking.dopesheet.ok = 0;
  BLI_listbase_clear(&clip->tracking.dopesheet.channels);
  BLI_listbase_clear(&clip->tracking.dopesheet.coverage_segments);

  BLO_read_struct_list(reader, MovieTrackingObject, &tracking->objects);

  LISTBASE_FOREACH (MovieTrackingObject *, object, &tracking->objects) {
    direct_link_movieTracks(reader, &object->tracks);
    direct_link_moviePlaneTracks(reader, &object->plane_tracks);
    direct_link_movieReconstruction(reader, &object->reconstruction);

    BLO_read_struct(reader, MovieTrackingTrack, &object->active_track);
    BLO_read_struct(reader, MovieTrackingPlaneTrack, &object->active_plane_track);
  }

  movie_clip_runtime_reset(clip);
}

IDTypeInfo IDType_ID_MC = {
    /*id_code*/ MovieClip::id_type,
    /*id_filter*/ FILTER_ID_MC,
    /*dependencies_id_types*/ FILTER_ID_GD_LEGACY | FILTER_ID_IM,
    /*main_listbase_index*/ INDEX_ID_MC,
    /*struct_size*/ sizeof(MovieClip),
    /*name*/ "MovieClip",
    /*name_plural*/ N_("movieclips"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_MOVIECLIP,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ movie_clip_init_data,
    /*copy_data*/ movie_clip_copy_data,
    /*free_data*/ movie_clip_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ movie_clip_foreach_id,
    /*foreach_cache*/ movie_clip_foreach_cache,
    /*foreach_path*/ movie_clip_foreach_path,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ movieclip_blend_write,
    /*blend_read_data*/ movieclip_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/*********************** movieclip buffer loaders *************************/

static int sequence_guess_offset(const char *full_name, int head_len, ushort numlen)
{
  char num[FILE_MAX] = {0};

  BLI_strncpy(num, full_name + head_len, numlen + 1);

  return atoi(num);
}

static int rendersize_to_proxy(const MovieClipUser *user, int flag)
{
  if ((flag & MCLIP_USE_PROXY) == 0) {
    return IMB_PROXY_NONE;
  }

  switch (user->render_size) {
    case MCLIP_PROXY_RENDER_SIZE_25:
      return IMB_PROXY_25;

    case MCLIP_PROXY_RENDER_SIZE_50:
      return IMB_PROXY_50;

    case MCLIP_PROXY_RENDER_SIZE_75:
      return IMB_PROXY_75;

    case MCLIP_PROXY_RENDER_SIZE_100:
      return IMB_PROXY_100;

    case MCLIP_PROXY_RENDER_SIZE_FULL:
      return IMB_PROXY_NONE;
  }

  return IMB_PROXY_NONE;
}

static int rendersize_to_number(int render_size)
{
  switch (render_size) {
    case MCLIP_PROXY_RENDER_SIZE_25:
      return 25;

    case MCLIP_PROXY_RENDER_SIZE_50:
      return 50;

    case MCLIP_PROXY_RENDER_SIZE_75:
      return 75;

    case MCLIP_PROXY_RENDER_SIZE_100:
      return 100;

    case MCLIP_PROXY_RENDER_SIZE_FULL:
      return 100;
  }

  return 100;
}

static int get_timecode(MovieClip *clip, int flag)
{
  if ((flag & MCLIP_USE_PROXY) == 0) {
    return IMB_TC_NONE;
  }

  return clip->proxy.tc;
}

static void get_sequence_filepath(const MovieClip *clip,
                                  const int framenr,
                                  char filepath[FILE_MAX])
{
  ushort numlen;
  char head[FILE_MAX], tail[FILE_MAX];
  int offset;

  BLI_strncpy(filepath, clip->filepath, sizeof(clip->filepath));
  BLI_path_sequence_decode(filepath, head, sizeof(head), tail, sizeof(tail), &numlen);

  /* Movie-clips always points to first image from sequence, auto-guess offset for now.
   * Could be something smarter in the future. */
  offset = sequence_guess_offset(clip->filepath, strlen(head), numlen);

  if (numlen) {
    BLI_path_sequence_encode(filepath,
                             FILE_MAX,
                             head,
                             tail,
                             numlen,
                             offset + framenr - clip->start_frame + clip->frame_offset);
  }
  else {
    BLI_strncpy(filepath, clip->filepath, sizeof(clip->filepath));
  }

  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&clip->id));
}

/* supposed to work with sequences only */
static void get_proxy_filepath(const MovieClip *clip,
                               int proxy_render_size,
                               bool undistorted,
                               int framenr,
                               char filepath[FILE_MAX])
{
  int size = rendersize_to_number(proxy_render_size);
  char dir[FILE_MAX], clipdir[FILE_MAX], clipfile[FILE_MAX];
  int proxynr = framenr - clip->start_frame + 1 + clip->frame_offset;

  BLI_path_split_dir_file(clip->filepath, clipdir, FILE_MAX, clipfile, FILE_MAX);

  if (clip->flag & MCLIP_USE_PROXY_CUSTOM_DIR) {
    STRNCPY(dir, clip->proxy.dir);
  }
  else {
    SNPRINTF(dir, "%s" SEP_STR "BL_proxy", clipdir);
  }

  if (undistorted) {
    BLI_snprintf(filepath,
                 FILE_MAX,
                 "%s" SEP_STR "%s" SEP_STR "proxy_%d_undistorted" SEP_STR "%08d",
                 dir,
                 clipfile,
                 size,
                 proxynr);
  }
  else {
    BLI_snprintf(filepath,
                 FILE_MAX,
                 "%s" SEP_STR "%s" SEP_STR "proxy_%d" SEP_STR "%08d",
                 dir,
                 clipfile,
                 size,
                 proxynr);
  }

  BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());
  BLI_path_frame(filepath, FILE_MAX, 1, 0);
  BLI_strncat(filepath, ".jpg", FILE_MAX);
}

#ifdef WITH_IMAGE_OPENEXR

namespace {

struct MultilayerConvertContext {
  float *combined_pass;
  int num_combined_channels;
};

}  // namespace

static void *movieclip_convert_multilayer_add_view(void * /*ctx_v*/, const char * /*view_name*/)
{
  return nullptr;
}

static void *movieclip_convert_multilayer_add_layer(void *ctx_v, const char * /*layer_name*/)
{
  /* Return dummy non-nullptr value, we don't use layer handle but need to return
   * something, so render API invokes the add_pass() callbacks. */
  return ctx_v;
}

static void movieclip_convert_multilayer_add_pass(void * /*layer*/,
                                                  void *ctx_v,
                                                  const char *pass_name,
                                                  float *rect,
                                                  int num_channels,
                                                  const char *chan_id,
                                                  const char * /*view_name*/)
{
  /* NOTE: This function must free pass pixels data if it is not used, this
   * is how IMB_exr_multilayer_convert() is working. */
  MultilayerConvertContext *ctx = static_cast<MultilayerConvertContext *>(ctx_v);
  /* If we've found a first combined pass, skip all the rest ones. */
  if (ctx->combined_pass != nullptr) {
    MEM_freeN(rect);
    return;
  }
  if (STREQ(pass_name, RE_PASSNAME_COMBINED) || STR_ELEM(chan_id, "RGBA", "RGB")) {
    ctx->combined_pass = rect;
    ctx->num_combined_channels = num_channels;
  }
  else {
    MEM_freeN(rect);
  }
}

#endif /* WITH_IMAGE_OPENEXR */

void BKE_movieclip_convert_multilayer_ibuf(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }
#ifdef WITH_IMAGE_OPENEXR
  if (ibuf->ftype != IMB_FTYPE_OPENEXR || ibuf->exrhandle == nullptr) {
    return;
  }
  MultilayerConvertContext ctx;
  ctx.combined_pass = nullptr;
  ctx.num_combined_channels = 0;
  IMB_exr_multilayer_convert(ibuf->exrhandle,
                             &ctx,
                             movieclip_convert_multilayer_add_view,
                             movieclip_convert_multilayer_add_layer,
                             movieclip_convert_multilayer_add_pass);
  if (ctx.combined_pass != nullptr) {
    BLI_assert(ibuf->float_buffer.data == nullptr);
    IMB_assign_float_buffer(ibuf, ctx.combined_pass, IB_TAKE_OWNERSHIP);
    ibuf->channels = ctx.num_combined_channels;
  }
  IMB_exr_close(ibuf->exrhandle);
  ibuf->exrhandle = nullptr;
#endif
}

static ImBuf *movieclip_load_sequence_file(MovieClip *clip,
                                           const MovieClipUser *user,
                                           int framenr,
                                           int flag)
{
  ImBuf *ibuf;
  char filepath[FILE_MAX];
  int loadflag;
  bool use_proxy = false;
  char *colorspace;

  use_proxy = (flag & MCLIP_USE_PROXY) && user->render_size != MCLIP_PROXY_RENDER_SIZE_FULL;
  if (use_proxy) {
    int undistort = user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT;
    get_proxy_filepath(clip, user->render_size, undistort, framenr, filepath);

    /* Well, this is a bit weird, but proxies for movie sources
     * are built in the same exact color space as the input,
     *
     * But image sequences are built in the display space.
     */
    if (clip->source == MCLIP_SRC_MOVIE) {
      colorspace = clip->colorspace_settings.name;
    }
    else {
      colorspace = nullptr;
    }
  }
  else {
    get_sequence_filepath(clip, framenr, filepath);
    colorspace = clip->colorspace_settings.name;
  }

  loadflag = IB_byte_data | IB_multilayer | IB_alphamode_detect | IB_metadata;

  /* read ibuf */
  ibuf = IMB_load_image_from_filepath(filepath, loadflag, colorspace);
  BKE_movieclip_convert_multilayer_ibuf(ibuf);

  return ibuf;
}

static void movieclip_open_anim_file(MovieClip *clip)
{
  char filepath_abs[FILE_MAX];

  if (!clip->anim) {
    STRNCPY(filepath_abs, clip->filepath);
    BLI_path_abs(filepath_abs, ID_BLEND_PATH_FROM_GLOBAL(&clip->id));

    /* FIXME: make several stream accessible in image editor, too */
    clip->anim = openanim(filepath_abs, IB_byte_data, 0, false, clip->colorspace_settings.name);

    if (clip->anim) {
      if (clip->flag & MCLIP_USE_PROXY_CUSTOM_DIR) {
        char dir[FILE_MAX];
        STRNCPY(dir, clip->proxy.dir);
        BLI_path_abs(dir, BKE_main_blendfile_path_from_global());
        MOV_set_custom_proxy_dir(clip->anim, dir);
      }
    }
  }
}

static ImBuf *movieclip_load_movie_file(MovieClip *clip,
                                        const MovieClipUser *user,
                                        int framenr,
                                        int flag)
{
  ImBuf *ibuf = nullptr;
  int tc = get_timecode(clip, flag);
  int proxy = rendersize_to_proxy(user, flag);

  movieclip_open_anim_file(clip);

  if (clip->anim) {
    int fra = framenr - clip->start_frame + clip->frame_offset;

    ibuf = MOV_decode_frame(clip->anim, fra, IMB_Timecode_Type(tc), IMB_Proxy_Size(proxy));
  }

  return ibuf;
}

static void movieclip_calc_length(MovieClip *clip)
{
  if (clip->source == MCLIP_SRC_MOVIE) {
    movieclip_open_anim_file(clip);

    if (clip->anim) {
      clip->len = MOV_get_duration_frames(clip->anim, IMB_Timecode_Type(clip->proxy.tc));
    }
  }
  else if (clip->source == MCLIP_SRC_SEQUENCE) {
    ushort numlen;
    char filepath[FILE_MAX], head[FILE_MAX], tail[FILE_MAX];

    BLI_path_sequence_decode(clip->filepath, head, sizeof(head), tail, sizeof(tail), &numlen);

    if (numlen == 0) {
      /* there's no number group in file name, assume it's single framed sequence */
      clip->len = 1;
    }
    else {
      clip->len = 0;
      for (;;) {
        get_sequence_filepath(clip, clip->len + clip->start_frame, filepath);

        if (BLI_exists(filepath)) {
          clip->len++;
        }
        else {
          break;
        }
      }
    }
  }
}

/*********************** image buffer cache *************************/

struct MovieClipCache {
  /* regular movie cache */
  MovieCache *moviecache;

  /* cached postprocessed shot */
  struct {
    ImBuf *ibuf;
    int framenr;
    int flag;

    /* cache for undistorted shot */
    float focal_length;
    float principal_point[2];
    float polynomial_k[3];
    float division_k[2];
    float nuke_k[2];
    float nuke_p[2];
    float brown_k[4];
    float brown_p[2];
    short distortion_model;
    bool undistortion_used;

    int proxy;
    short render_flag;
  } postprocessed;

  /* cache for stable shot */
  struct {
    ImBuf *reference_ibuf;

    ImBuf *ibuf;
    int framenr;
    int postprocess_flag;

    float loc[2], scale, angle, aspect;
    int proxy, filter;
    short render_flag;
  } stabilized;

  int sequence_offset;

  bool is_still_sequence;
};

struct MovieClipImBufCacheKey {
  int framenr;
  int proxy;
  short render_flag;
};

struct MovieClipCachePriorityData {
  int framenr;
};

static int user_frame_to_cache_frame(MovieClip *clip, int framenr)
{
  int index;

  index = framenr - clip->start_frame + clip->frame_offset;

  if (clip->source == MCLIP_SRC_SEQUENCE) {
    if (clip->cache->sequence_offset == -1) {
      ushort numlen;
      char head[FILE_MAX], tail[FILE_MAX];

      BLI_path_sequence_decode(clip->filepath, head, sizeof(head), tail, sizeof(tail), &numlen);

      /* see comment in get_sequence_filepath */
      clip->cache->sequence_offset = sequence_guess_offset(clip->filepath, strlen(head), numlen);
    }

    index += clip->cache->sequence_offset;
  }

  if (index < 0) {
    return framenr - index;
  }

  return framenr;
}

static void moviecache_keydata(void *userkey, int *framenr, int *proxy, int *render_flags)
{
  const MovieClipImBufCacheKey *key = static_cast<const MovieClipImBufCacheKey *>(userkey);

  *framenr = key->framenr;
  *proxy = key->proxy;
  *render_flags = key->render_flag;
}

static uint moviecache_hashhash(const void *keyv)
{
  const MovieClipImBufCacheKey *key = static_cast<const MovieClipImBufCacheKey *>(keyv);
  int rval = key->framenr;

  return rval;
}

static bool moviecache_hashcmp(const void *av, const void *bv)
{
  const MovieClipImBufCacheKey *a = static_cast<const MovieClipImBufCacheKey *>(av);
  const MovieClipImBufCacheKey *b = static_cast<const MovieClipImBufCacheKey *>(bv);

  return ((a->framenr != b->framenr) || (a->proxy != b->proxy) ||
          (a->render_flag != b->render_flag));
}

static void *moviecache_getprioritydata(void *key_v)
{
  MovieClipImBufCacheKey *key = (MovieClipImBufCacheKey *)key_v;
  MovieClipCachePriorityData *priority_data;

  priority_data = MEM_callocN<MovieClipCachePriorityData>("movie cache clip priority data");
  priority_data->framenr = key->framenr;

  return priority_data;
}

static int moviecache_getitempriority(void *last_userkey_v, void *priority_data_v)
{
  MovieClipImBufCacheKey *last_userkey = (MovieClipImBufCacheKey *)last_userkey_v;
  MovieClipCachePriorityData *priority_data = (MovieClipCachePriorityData *)priority_data_v;

  return -abs(last_userkey->framenr - priority_data->framenr);
}

static void moviecache_prioritydeleter(void *priority_data_v)
{
  MovieClipCachePriorityData *priority_data = (MovieClipCachePriorityData *)priority_data_v;

  MEM_freeN(priority_data);
}

static ImBuf *get_imbuf_cache(MovieClip *clip, const MovieClipUser *user, int flag)
{
  if (clip->cache) {
    MovieClipImBufCacheKey key;

    if (!clip->cache->is_still_sequence) {
      key.framenr = user_frame_to_cache_frame(clip, user->framenr);
    }
    else {
      key.framenr = 1;
    }

    if (flag & MCLIP_USE_PROXY) {
      key.proxy = rendersize_to_proxy(user, flag);
      key.render_flag = user->render_flag;
    }
    else {
      key.proxy = IMB_PROXY_NONE;
      key.render_flag = 0;
    }

    return IMB_moviecache_get(clip->cache->moviecache, &key, nullptr);
  }

  return nullptr;
}

static bool has_imbuf_cache(MovieClip *clip, const MovieClipUser *user, int flag)
{
  if (clip->cache) {
    MovieClipImBufCacheKey key;

    key.framenr = user_frame_to_cache_frame(clip, user->framenr);

    if (flag & MCLIP_USE_PROXY) {
      key.proxy = rendersize_to_proxy(user, flag);
      key.render_flag = user->render_flag;
    }
    else {
      key.proxy = IMB_PROXY_NONE;
      key.render_flag = 0;
    }

    return IMB_moviecache_has_frame(clip->cache->moviecache, &key);
  }

  return false;
}

static bool put_imbuf_cache(
    MovieClip *clip, const MovieClipUser *user, ImBuf *ibuf, int flag, bool destructive)
{
  MovieClipImBufCacheKey key;

  if (clip->cache == nullptr) {
    MovieCache *moviecache;

    // char cache_name[64];
    // SNPRINTF(cache_name, "movie %s", clip->id.name);

    clip->cache = MEM_callocN<MovieClipCache>("movieClipCache");

    moviecache = IMB_moviecache_create(
        "movieclip", sizeof(MovieClipImBufCacheKey), moviecache_hashhash, moviecache_hashcmp);

    IMB_moviecache_set_getdata_callback(moviecache, moviecache_keydata);
    IMB_moviecache_set_priority_callback(moviecache,
                                         moviecache_getprioritydata,
                                         moviecache_getitempriority,
                                         moviecache_prioritydeleter);

    clip->cache->moviecache = moviecache;
    clip->cache->sequence_offset = -1;
    if (clip->source == MCLIP_SRC_SEQUENCE) {
      ushort numlen;
      BLI_path_sequence_decode(clip->filepath, nullptr, 0, nullptr, 0, &numlen);
      clip->cache->is_still_sequence = (numlen == 0);
    }
  }

  if (!clip->cache->is_still_sequence) {
    key.framenr = user_frame_to_cache_frame(clip, user->framenr);
  }
  else {
    key.framenr = 1;
  }

  if (flag & MCLIP_USE_PROXY) {
    key.proxy = rendersize_to_proxy(user, flag);
    key.render_flag = user->render_flag;
  }
  else {
    key.proxy = IMB_PROXY_NONE;
    key.render_flag = 0;
  }

  if (destructive) {
    IMB_moviecache_put(clip->cache->moviecache, &key, ibuf);
    return true;
  }

  return IMB_moviecache_put_if_possible(clip->cache->moviecache, &key, ibuf);
}

static bool moviecache_check_free_proxy(ImBuf * /*ibuf*/, void *userkey, void * /*userdata*/)
{
  MovieClipImBufCacheKey *key = (MovieClipImBufCacheKey *)userkey;

  return !(key->proxy == IMB_PROXY_NONE && key->render_flag == 0);
}

/*********************** common functions *************************/

/* only image block itself */
static MovieClip *movieclip_alloc(Main *bmain, const char *name)
{
  MovieClip *clip;

  clip = BKE_id_new<MovieClip>(bmain, name);

  return clip;
}

static void movieclip_load_get_size(MovieClip *clip)
{
  int width, height;
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);

  user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, 1);
  BKE_movieclip_get_size(clip, &user, &width, &height);

  if (!width || !height) {
    clip->lastsize[0] = clip->lastsize[1] = IMG_SIZE_FALLBACK;
  }
}

static void detect_clip_source(Main *bmain, MovieClip *clip)
{
  ImBuf *ibuf;
  char filepath[FILE_MAX];

  STRNCPY(filepath, clip->filepath);
  BLI_path_abs(filepath, ID_BLEND_PATH(bmain, &clip->id));

  ibuf = IMB_load_image_from_filepath(filepath, IB_byte_data | IB_multilayer | IB_test);
  if (ibuf) {
    clip->source = MCLIP_SRC_SEQUENCE;
    IMB_freeImBuf(ibuf);
  }
  else {
    clip->source = MCLIP_SRC_MOVIE;
  }
}

MovieClip *BKE_movieclip_file_add(Main *bmain, const char *filepath)
{
  MovieClip *clip;
  int file;
  char filepath_abs[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));

  /* exists? */
  file = BLI_open(filepath_abs, O_BINARY | O_RDONLY, 0);
  if (file == -1) {
    return nullptr;
  }
  close(file);

  /* ** add new movieclip ** */

  /* create a short library name */
  clip = movieclip_alloc(bmain, BLI_path_basename(filepath));
  STRNCPY(clip->filepath, filepath);

  detect_clip_source(bmain, clip);

  movieclip_load_get_size(clip);
  if (clip->lastsize[0]) {
    int width = clip->lastsize[0];

    clip->tracking.camera.focal = 24.0f * width / clip->tracking.camera.sensor_width;
  }

  movieclip_calc_length(clip);

  return clip;
}

MovieClip *BKE_movieclip_file_add_exists_ex(Main *bmain, const char *filepath, bool *r_exists)
{
  MovieClip *clip;
  char filepath_abs[FILE_MAX], filepath_test[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));

  /* first search an identical filepath */
  for (clip = static_cast<MovieClip *>(bmain->movieclips.first); clip;
       clip = static_cast<MovieClip *>(clip->id.next))
  {
    STRNCPY(filepath_test, clip->filepath);
    BLI_path_abs(filepath_test, ID_BLEND_PATH(bmain, &clip->id));

    if (BLI_path_cmp(filepath_test, filepath_abs) == 0) {
      id_us_plus(&clip->id); /* officially should not, it doesn't link here! */
      if (r_exists) {
        *r_exists = true;
      }
      return clip;
    }
  }

  if (r_exists) {
    *r_exists = false;
  }
  return BKE_movieclip_file_add(bmain, filepath);
}

MovieClip *BKE_movieclip_file_add_exists(Main *bmain, const char *filepath)
{
  return BKE_movieclip_file_add_exists_ex(bmain, filepath, nullptr);
}

static void real_ibuf_size(
    const MovieClip *clip, const MovieClipUser *user, const ImBuf *ibuf, int *width, int *height)
{
  *width = ibuf->x;
  *height = ibuf->y;

  if (clip->flag & MCLIP_USE_PROXY) {
    switch (user->render_size) {
      case MCLIP_PROXY_RENDER_SIZE_25:
        (*width) *= 4;
        (*height) *= 4;
        break;

      case MCLIP_PROXY_RENDER_SIZE_50:
        (*width) *= 2.0f;
        (*height) *= 2.0f;
        break;

      case MCLIP_PROXY_RENDER_SIZE_75:
        *width = float(*width) * 4.0f / 3.0f;
        *height = float(*height) * 4.0f / 3.0f;
        break;
    }
  }
}

static ImBuf *get_undistorted_ibuf(MovieClip *clip, MovieDistortion *distortion, ImBuf *ibuf)
{
  ImBuf *undistibuf;

  if (distortion) {
    undistibuf = BKE_tracking_distortion_exec(
        distortion, &clip->tracking, ibuf, ibuf->x, ibuf->y, 0.0f, true);
  }
  else {
    undistibuf = BKE_tracking_undistort_frame(&clip->tracking, ibuf, ibuf->x, ibuf->y, 0.0f);
  }

  IMB_scale(undistibuf, ibuf->x, ibuf->y, IMBScaleFilter::Box, false);

  return undistibuf;
}

static bool need_undistortion_postprocess(const MovieClipUser *user, int clip_flag)
{
  bool result = false;
  const bool uses_full_frame = ((clip_flag & MCLIP_USE_PROXY) == 0) ||
                               (user->render_size == MCLIP_PROXY_RENDER_SIZE_FULL);
  /* Only full undistorted render can be used as on-fly undistorting image. */
  result |= uses_full_frame && (user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT) != 0;
  return result;
}

static bool need_postprocessed_frame(const MovieClipUser *user,
                                     int clip_flag,
                                     int postprocess_flag)
{
  bool result = (postprocess_flag != 0);
  result |= need_undistortion_postprocess(user, clip_flag);
  return result;
}

static bool check_undistortion_cache_flags(const MovieClip *clip)
{
  const MovieClipCache *cache = clip->cache;
  const MovieTrackingCamera *camera = &clip->tracking.camera;

  if (camera->focal != cache->postprocessed.focal_length) {
    return false;
  }

  /* check for distortion model changes */
  if (!equals_v2v2(camera->principal_point, cache->postprocessed.principal_point)) {
    return false;
  }

  if (camera->distortion_model != cache->postprocessed.distortion_model) {
    return false;
  }

  if (!equals_v3v3(&camera->k1, cache->postprocessed.polynomial_k)) {
    return false;
  }

  if (!equals_v2v2(&camera->division_k1, cache->postprocessed.division_k)) {
    return false;
  }

  if (!equals_v2v2(&camera->nuke_k1, cache->postprocessed.nuke_k)) {
    return false;
  }
  if (!equals_v2v2(&camera->nuke_p1, cache->postprocessed.nuke_p)) {
    return false;
  }

  if (!equals_v4v4(&camera->brown_k1, cache->postprocessed.brown_k)) {
    return false;
  }
  if (!equals_v2v2(&camera->brown_p1, cache->postprocessed.brown_p)) {
    return false;
  }

  return true;
}

static ImBuf *get_postprocessed_cached_frame(const MovieClip *clip,
                                             const MovieClipUser *user,
                                             int flag,
                                             int postprocess_flag)
{
  const MovieClipCache *cache = clip->cache;
  int framenr = user->framenr;
  short proxy = IMB_PROXY_NONE;
  int render_flag = 0;

  if (flag & MCLIP_USE_PROXY) {
    proxy = rendersize_to_proxy(user, flag);
    render_flag = user->render_flag;
  }

  /* no cache or no cached postprocessed image */
  if (!clip->cache || !clip->cache->postprocessed.ibuf) {
    return nullptr;
  }

  /* Postprocessing happened for other frame. */
  if (cache->postprocessed.framenr != framenr) {
    return nullptr;
  }

  /* cached ibuf used different proxy settings */
  if (cache->postprocessed.render_flag != render_flag || cache->postprocessed.proxy != proxy) {
    return nullptr;
  }

  if (cache->postprocessed.flag != postprocess_flag) {
    return nullptr;
  }

  if (need_undistortion_postprocess(user, flag)) {
    if (!check_undistortion_cache_flags(clip)) {
      return nullptr;
    }
  }
  else if (cache->postprocessed.undistortion_used) {
    return nullptr;
  }

  IMB_refImBuf(cache->postprocessed.ibuf);

  return cache->postprocessed.ibuf;
}

static ImBuf *postprocess_frame(
    MovieClip *clip, const MovieClipUser *user, ImBuf *ibuf, int flag, int postprocess_flag)
{
  ImBuf *postproc_ibuf = nullptr;

  if (need_undistortion_postprocess(user, flag)) {
    postproc_ibuf = get_undistorted_ibuf(clip, nullptr, ibuf);
  }
  else {
    postproc_ibuf = IMB_dupImBuf(ibuf);
  }

  if (postprocess_flag) {
    bool disable_red = (postprocess_flag & MOVIECLIP_DISABLE_RED) != 0;
    bool disable_green = (postprocess_flag & MOVIECLIP_DISABLE_GREEN) != 0;
    bool disable_blue = (postprocess_flag & MOVIECLIP_DISABLE_BLUE) != 0;
    bool grayscale = (postprocess_flag & MOVIECLIP_PREVIEW_GRAYSCALE) != 0;

    if (disable_red || disable_green || disable_blue || grayscale) {
      BKE_tracking_disable_channels(postproc_ibuf, disable_red, disable_green, disable_blue, true);
    }
  }

  return postproc_ibuf;
}

static void put_postprocessed_frame_to_cache(
    MovieClip *clip, const MovieClipUser *user, ImBuf *ibuf, int flag, int postprocess_flag)
{
  MovieClipCache *cache = clip->cache;
  MovieTrackingCamera *camera = &clip->tracking.camera;

  cache->postprocessed.framenr = user->framenr;
  cache->postprocessed.flag = postprocess_flag;

  if (flag & MCLIP_USE_PROXY) {
    cache->postprocessed.proxy = rendersize_to_proxy(user, flag);
    cache->postprocessed.render_flag = user->render_flag;
  }
  else {
    cache->postprocessed.proxy = IMB_PROXY_NONE;
    cache->postprocessed.render_flag = 0;
  }

  if (need_undistortion_postprocess(user, flag)) {
    cache->postprocessed.distortion_model = camera->distortion_model;
    cache->postprocessed.focal_length = camera->focal;
    copy_v2_v2(cache->postprocessed.principal_point, camera->principal_point);
    copy_v3_v3(cache->postprocessed.polynomial_k, &camera->k1);
    copy_v2_v2(cache->postprocessed.division_k, &camera->division_k1);
    copy_v2_v2(cache->postprocessed.nuke_k, &camera->nuke_k1);
    copy_v2_v2(cache->postprocessed.nuke_p, &camera->nuke_p1);
    copy_v4_v4(cache->postprocessed.brown_k, &camera->brown_k1);
    copy_v2_v2(cache->postprocessed.brown_p, &camera->brown_p1);
    cache->postprocessed.undistortion_used = true;
  }
  else {
    cache->postprocessed.undistortion_used = false;
  }

  IMB_refImBuf(ibuf);

  if (cache->postprocessed.ibuf) {
    IMB_freeImBuf(cache->postprocessed.ibuf);
  }

  cache->postprocessed.ibuf = ibuf;
}

static ImBuf *movieclip_get_postprocessed_ibuf(
    MovieClip *clip, const MovieClipUser *user, int flag, int postprocess_flag, int cache_flag)
{
  ImBuf *ibuf = nullptr;
  int framenr = user->framenr;
  bool need_postprocess = false;

  /* cache isn't threadsafe itself and also loading of movies
   * can't happen from concurrent threads that's why we use lock here */
  BLI_thread_lock(LOCK_MOVIECLIP);

  /* try to obtain cached postprocessed frame first */
  if (need_postprocessed_frame(user, flag, postprocess_flag)) {
    ibuf = get_postprocessed_cached_frame(clip, user, flag, postprocess_flag);

    if (!ibuf) {
      need_postprocess = true;
    }
  }

  if (!ibuf) {
    ibuf = get_imbuf_cache(clip, user, flag);
  }

  if (!ibuf) {
    bool use_sequence = false;

    /* undistorted proxies for movies should be read as image sequence */
    use_sequence = (user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT) &&
                   (user->render_size != MCLIP_PROXY_RENDER_SIZE_FULL);

    if (clip->source == MCLIP_SRC_SEQUENCE || use_sequence) {
      ibuf = movieclip_load_sequence_file(clip, user, framenr, flag);
    }
    else {
      ibuf = movieclip_load_movie_file(clip, user, framenr, flag);
    }

    if (ibuf && (cache_flag & MOVIECLIP_CACHE_SKIP) == 0) {
      put_imbuf_cache(clip, user, ibuf, flag, true);
    }
  }

  if (ibuf) {
    real_ibuf_size(clip, user, ibuf, &clip->lastsize[0], &clip->lastsize[1]);

    /* Post-process frame and put to cache if needed. */
    if (need_postprocess) {
      ImBuf *tmpibuf = ibuf;
      ibuf = postprocess_frame(clip, user, tmpibuf, flag, postprocess_flag);
      IMB_freeImBuf(tmpibuf);
      if (ibuf && (cache_flag & MOVIECLIP_CACHE_SKIP) == 0) {
        put_postprocessed_frame_to_cache(clip, user, ibuf, flag, postprocess_flag);
      }
    }
  }

  BLI_thread_unlock(LOCK_MOVIECLIP);

  /* Fallback render in case proxies are not enabled or built */
  if (!ibuf && user->render_flag & MCLIP_PROXY_RENDER_USE_FALLBACK_RENDER &&
      user->render_size != MCLIP_PROXY_RENDER_SIZE_FULL)
  {
    MovieClipUser user_fallback = *user;
    user_fallback.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;

    ibuf = movieclip_get_postprocessed_ibuf(
        clip, &user_fallback, flag, postprocess_flag, cache_flag);
  }

  return ibuf;
}

ImBuf *BKE_movieclip_get_ibuf(MovieClip *clip, const MovieClipUser *user)
{
  return BKE_movieclip_get_ibuf_flag(clip, user, clip->flag, 0);
}

ImBuf *BKE_movieclip_get_ibuf_flag(MovieClip *clip,
                                   const MovieClipUser *user,
                                   const int flag,
                                   const int cache_flag)
{
  return movieclip_get_postprocessed_ibuf(clip, user, flag, 0, cache_flag);
}

ImBuf *BKE_movieclip_get_postprocessed_ibuf(MovieClip *clip,
                                            const MovieClipUser *user,
                                            const int postprocess_flag)
{
  return movieclip_get_postprocessed_ibuf(clip, user, clip->flag, postprocess_flag, 0);
}

static ImBuf *get_stable_cached_frame(MovieClip *clip,
                                      const MovieClipUser *user,
                                      ImBuf *reference_ibuf,
                                      const int framenr,
                                      const int postprocess_flag)
{
  MovieClipCache *cache = clip->cache;
  MovieTracking *tracking = &clip->tracking;
  ImBuf *stableibuf;
  float tloc[2], tscale, tangle;
  short proxy = IMB_PROXY_NONE;
  int render_flag = 0;
  int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, framenr);

  if (clip->flag & MCLIP_USE_PROXY) {
    proxy = rendersize_to_proxy(user, clip->flag);
    render_flag = user->render_flag;
  }

  /* there's no cached frame or it was calculated for another frame */
  if (!cache->stabilized.ibuf || cache->stabilized.framenr != framenr) {
    return nullptr;
  }

  if (cache->stabilized.reference_ibuf != reference_ibuf) {
    return nullptr;
  }

  /* cached ibuf used different proxy settings */
  if (cache->stabilized.render_flag != render_flag || cache->stabilized.proxy != proxy) {
    return nullptr;
  }

  if (cache->stabilized.postprocess_flag != postprocess_flag) {
    return nullptr;
  }

  /* stabilization also depends on pixel aspect ratio */
  if (cache->stabilized.aspect != tracking->camera.pixel_aspect) {
    return nullptr;
  }

  if (cache->stabilized.filter != tracking->stabilization.filter) {
    return nullptr;
  }

  stableibuf = cache->stabilized.ibuf;

  BKE_tracking_stabilization_data_get(
      clip, clip_framenr, stableibuf->x, stableibuf->y, tloc, &tscale, &tangle);

  /* check for stabilization parameters */
  if (tscale != cache->stabilized.scale || tangle != cache->stabilized.angle ||
      !equals_v2v2(tloc, cache->stabilized.loc))
  {
    return nullptr;
  }

  IMB_refImBuf(stableibuf);

  return stableibuf;
}

static ImBuf *put_stabilized_frame_to_cache(MovieClip *clip,
                                            const MovieClipUser *user,
                                            ImBuf *ibuf,
                                            const int framenr,
                                            const int postprocess_flag)
{
  MovieClipCache *cache = clip->cache;
  MovieTracking *tracking = &clip->tracking;
  ImBuf *stableibuf;
  float tloc[2], tscale, tangle;
  int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, framenr);

  stableibuf = BKE_tracking_stabilize_frame(clip, clip_framenr, ibuf, tloc, &tscale, &tangle);

  copy_v2_v2(cache->stabilized.loc, tloc);

  cache->stabilized.reference_ibuf = ibuf;
  cache->stabilized.scale = tscale;
  cache->stabilized.angle = tangle;
  cache->stabilized.framenr = framenr;
  cache->stabilized.aspect = tracking->camera.pixel_aspect;
  cache->stabilized.filter = tracking->stabilization.filter;

  if (clip->flag & MCLIP_USE_PROXY) {
    cache->stabilized.proxy = rendersize_to_proxy(user, clip->flag);
    cache->stabilized.render_flag = user->render_flag;
  }
  else {
    cache->stabilized.proxy = IMB_PROXY_NONE;
    cache->stabilized.render_flag = 0;
  }

  cache->stabilized.postprocess_flag = postprocess_flag;

  if (cache->stabilized.ibuf) {
    IMB_freeImBuf(cache->stabilized.ibuf);
  }

  cache->stabilized.ibuf = stableibuf;

  IMB_refImBuf(stableibuf);

  return stableibuf;
}

ImBuf *BKE_movieclip_get_stable_ibuf(MovieClip *clip,
                                     const MovieClipUser *user,
                                     const int postprocess_flag,
                                     float r_loc[2],
                                     float *r_scale,
                                     float *r_angle)
{
  ImBuf *ibuf, *stableibuf = nullptr;
  int framenr = user->framenr;

  ibuf = BKE_movieclip_get_postprocessed_ibuf(clip, user, postprocess_flag);

  if (!ibuf) {
    return nullptr;
  }

  if (clip->tracking.stabilization.flag & TRACKING_2D_STABILIZATION) {
    MovieClipCache *cache = clip->cache;

    stableibuf = get_stable_cached_frame(clip, user, ibuf, framenr, postprocess_flag);

    if (!stableibuf) {
      stableibuf = put_stabilized_frame_to_cache(clip, user, ibuf, framenr, postprocess_flag);
    }

    if (r_loc) {
      copy_v2_v2(r_loc, cache->stabilized.loc);
    }

    if (r_scale) {
      *r_scale = cache->stabilized.scale;
    }

    if (r_angle) {
      *r_angle = cache->stabilized.angle;
    }
  }
  else {
    if (r_loc) {
      zero_v2(r_loc);
    }

    if (r_scale) {
      *r_scale = 1.0f;
    }

    if (r_angle) {
      *r_angle = 0.0f;
    }

    stableibuf = ibuf;
  }

  if (stableibuf != ibuf) {
    IMB_freeImBuf(ibuf);
    ibuf = stableibuf;
  }

  return ibuf;
}

bool BKE_movieclip_has_frame(MovieClip *clip, const MovieClipUser *user)
{
  ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, user);

  if (ibuf) {
    IMB_freeImBuf(ibuf);
    return true;
  }

  return false;
}

void BKE_movieclip_get_size(MovieClip *clip,
                            const MovieClipUser *user,
                            int *r_width,
                            int *r_height)
{
  /* TODO(sergey): Support reading sequences of different resolution. */
  if (clip->lastsize[0] != 0 && clip->lastsize[1] != 0) {
    *r_width = clip->lastsize[0];
    *r_height = clip->lastsize[1];
  }
  else {
    ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, user);

    if (ibuf && ibuf->x && ibuf->y) {
      real_ibuf_size(clip, user, ibuf, r_width, r_height);
    }
    else {
      *r_width = clip->lastsize[0];
      *r_height = clip->lastsize[1];
    }

    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }
  }
}
void BKE_movieclip_get_size_fl(MovieClip *clip, const MovieClipUser *user, float r_size[2])
{
  int width, height;
  BKE_movieclip_get_size(clip, user, &width, &height);

  r_size[0] = float(width);
  r_size[1] = float(height);
}

int BKE_movieclip_get_duration(MovieClip *clip)
{
  if (!clip->len) {
    movieclip_calc_length(clip);
  }

  return clip->len;
}

float BKE_movieclip_get_fps(MovieClip *clip)
{
  if (clip->source != MCLIP_SRC_MOVIE) {
    return 0.0f;
  }
  movieclip_open_anim_file(clip);
  if (clip->anim == nullptr) {
    return 0.0f;
  }
  return MOV_get_fps(clip->anim);
}

void BKE_movieclip_get_aspect(MovieClip *clip, float *aspx, float *aspy)
{
  *aspx = 1.0;

  /* x is always 1 */
  *aspy = clip->aspy / clip->aspx / clip->tracking.camera.pixel_aspect;
}

void BKE_movieclip_get_cache_segments(MovieClip *clip,
                                      const MovieClipUser *user,
                                      int *r_totseg,
                                      int **r_points)
{
  *r_totseg = 0;
  *r_points = nullptr;

  if (clip->cache) {
    int proxy = rendersize_to_proxy(user, clip->flag);

    BLI_thread_lock(LOCK_MOVIECLIP);
    IMB_moviecache_get_cache_segments(
        clip->cache->moviecache, proxy, user->render_flag, r_totseg, r_points);
    BLI_thread_unlock(LOCK_MOVIECLIP);
  }
}

void BKE_movieclip_user_set_frame(MovieClipUser *user, int framenr)
{
  /* TODO: clamp framenr here? */

  user->framenr = framenr;
}

static void free_buffers(MovieClip *clip)
{
  if (clip->cache) {
    IMB_moviecache_free(clip->cache->moviecache);

    if (clip->cache->postprocessed.ibuf) {
      IMB_freeImBuf(clip->cache->postprocessed.ibuf);
    }

    if (clip->cache->stabilized.ibuf) {
      IMB_freeImBuf(clip->cache->stabilized.ibuf);
    }

    MEM_freeN(clip->cache);
    clip->cache = nullptr;
  }

  if (clip->anim) {
    MOV_close(clip->anim);
    clip->anim = nullptr;
  }

  MovieClip_RuntimeGPUTexture *tex;
  for (tex = static_cast<MovieClip_RuntimeGPUTexture *>(clip->runtime.gputextures.first); tex;
       tex = static_cast<MovieClip_RuntimeGPUTexture *>(tex->next))
  {
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      if (tex->gputexture[i] != nullptr) {
        GPU_texture_free(tex->gputexture[i]);
        tex->gputexture[i] = nullptr;
      }
    }
  }
  BLI_freelistN(&clip->runtime.gputextures);
}

void BKE_movieclip_clear_cache(MovieClip *clip)
{
  free_buffers(clip);
}

void BKE_movieclip_clear_proxy_cache(MovieClip *clip)
{
  if (clip->cache && clip->cache->moviecache) {
    IMB_moviecache_cleanup(clip->cache->moviecache, moviecache_check_free_proxy, nullptr);
  }
}

void BKE_movieclip_reload(Main *bmain, MovieClip *clip)
{
  /* clear cache */
  free_buffers(clip);

  /* update clip source */
  detect_clip_source(bmain, clip);

  /* Tag for re-calculation of the actual size. */
  clip->lastsize[0] = clip->lastsize[1] = 0;

  movieclip_load_get_size(clip);
  movieclip_calc_length(clip);

  BKE_ntree_update_tag_id_changed(bmain, &clip->id);
}

void BKE_movieclip_update_scopes(MovieClip *clip,
                                 const MovieClipUser *user,
                                 MovieClipScopes *scopes)
{
  if (scopes->ok) {
    return;
  }

  if (scopes->track_preview) {
    IMB_freeImBuf(scopes->track_preview);
    scopes->track_preview = nullptr;
  }

  if (scopes->track_search) {
    IMB_freeImBuf(scopes->track_search);
    scopes->track_search = nullptr;
  }

  scopes->marker = nullptr;
  scopes->track = nullptr;
  scopes->track_locked = true;

  scopes->scene_framenr = user->framenr;
  scopes->ok = true;

  if (clip == nullptr) {
    return;
  }

  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  MovieTrackingTrack *track = tracking_object->active_track;
  if (track == nullptr) {
    return;
  }

  const int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
  MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

  scopes->marker = marker;
  scopes->track = track;

  if (marker->flag & MARKER_DISABLED) {
    scopes->track_disabled = true;
  }
  else {
    ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, user);

    scopes->track_disabled = false;

    if (ibuf && (ibuf->byte_buffer.data || ibuf->float_buffer.data)) {
      MovieTrackingMarker undist_marker = *marker;

      if (user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
        int width, height;
        float aspy = 1.0f / clip->tracking.camera.pixel_aspect;

        BKE_movieclip_get_size(clip, user, &width, &height);

        undist_marker.pos[0] *= width;
        undist_marker.pos[1] *= height * aspy;

        BKE_tracking_undistort_v2(
            &clip->tracking, width, height, undist_marker.pos, undist_marker.pos);

        undist_marker.pos[0] /= width;
        undist_marker.pos[1] /= height * aspy;
      }

      scopes->track_search = BKE_tracking_get_search_imbuf(
          ibuf, track, &undist_marker, true, true);

      scopes->undist_marker = undist_marker;

      scopes->frame_width = ibuf->x;
      scopes->frame_height = ibuf->y;

      scopes->use_track_mask = (track->flag & TRACK_PREVIEW_ALPHA) != 0;
    }

    IMB_freeImBuf(ibuf);
  }

  if ((track->flag & TRACK_LOCKED) == 0) {
    float pat_min[2], pat_max[2];

    scopes->track_locked = false;

    /* XXX: would work fine with non-transformed patterns, but would likely fail
     *      with transformed patterns, but that would be easier to debug when
     *      we'll have real pattern sampling (at least to test) */
    BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

    scopes->slide_scale[0] = pat_max[0] - pat_min[0];
    scopes->slide_scale[1] = pat_max[1] - pat_min[1];
  }
}

static void movieclip_build_proxy_ibuf(const MovieClip *clip,
                                       const ImBuf *ibuf,
                                       int cfra,
                                       int proxy_render_size,
                                       bool undistorted,
                                       bool threaded)
{
  char filepath[FILE_MAX];
  int quality, rectx, recty;
  int size = rendersize_to_number(proxy_render_size);

  get_proxy_filepath(clip, proxy_render_size, undistorted, cfra, filepath);

  rectx = ibuf->x * size / 100.0f;
  recty = ibuf->y * size / 100.0f;

  ImBuf *scaleibuf = IMB_scale_into_new(ibuf, rectx, recty, IMBScaleFilter::Bilinear, threaded);

  quality = clip->proxy.quality;
  scaleibuf->ftype = IMB_FTYPE_JPG;
  scaleibuf->foptions.quality = quality;
  /* unsupported feature only confuses other s/w */
  if (scaleibuf->planes == 32) {
    scaleibuf->planes = 24;
  }

  /* TODO: currently the most weak part of multi-threaded proxies,
   *       could be solved in a way that thread only prepares memory
   *       buffer and write to disk happens separately
   */
  BLI_thread_lock(LOCK_MOVIECLIP);

  BLI_file_ensure_parent_dir_exists(filepath);
  if (IMB_save_image(scaleibuf, filepath, IB_byte_data) == 0) {
    perror(filepath);
  }

  BLI_thread_unlock(LOCK_MOVIECLIP);

  IMB_freeImBuf(scaleibuf);
}

void BKE_movieclip_build_proxy_frame(MovieClip *clip,
                                     int clip_flag,
                                     MovieDistortion *distortion,
                                     int cfra,
                                     const int *build_sizes,
                                     int build_count,
                                     bool undistorted)
{
  ImBuf *ibuf;
  MovieClipUser user;

  if (!build_count) {
    return;
  }

  user.framenr = cfra;
  user.render_flag = 0;
  user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;

  ibuf = BKE_movieclip_get_ibuf_flag(clip, &user, clip_flag, MOVIECLIP_CACHE_SKIP);

  if (ibuf) {
    ImBuf *tmpibuf = ibuf;
    int i;

    if (undistorted) {
      tmpibuf = get_undistorted_ibuf(clip, distortion, ibuf);
    }

    for (i = 0; i < build_count; i++) {
      movieclip_build_proxy_ibuf(clip, tmpibuf, cfra, build_sizes[i], undistorted, true);
    }

    IMB_freeImBuf(ibuf);

    if (tmpibuf != ibuf) {
      IMB_freeImBuf(tmpibuf);
    }
  }
}

void BKE_movieclip_build_proxy_frame_for_ibuf(MovieClip *clip,
                                              ImBuf *ibuf,
                                              MovieDistortion *distortion,
                                              int cfra,
                                              const int *build_sizes,
                                              int build_count,
                                              bool undistorted)
{
  if (!build_count) {
    return;
  }

  if (ibuf) {
    ImBuf *tmpibuf = ibuf;
    int i;

    if (undistorted) {
      tmpibuf = get_undistorted_ibuf(clip, distortion, ibuf);
    }

    for (i = 0; i < build_count; i++) {
      movieclip_build_proxy_ibuf(clip, tmpibuf, cfra, build_sizes[i], undistorted, false);
    }

    if (tmpibuf != ibuf) {
      IMB_freeImBuf(tmpibuf);
    }
  }
}

bool BKE_movieclip_proxy_enabled(MovieClip *clip)
{
  return clip->flag & MCLIP_USE_PROXY;
}

float BKE_movieclip_remap_scene_to_clip_frame(const MovieClip *clip, const float framenr)
{
  return framenr - float(clip->start_frame) + 1.0f;
}

float BKE_movieclip_remap_clip_to_scene_frame(const MovieClip *clip, const float framenr)
{
  return framenr + float(clip->start_frame) - 1.0f;
}

void BKE_movieclip_filepath_for_frame(MovieClip *clip, const MovieClipUser *user, char *filepath)
{
  if (clip->source == MCLIP_SRC_SEQUENCE) {
    int use_proxy;

    use_proxy = (clip->flag & MCLIP_USE_PROXY) &&
                user->render_size != MCLIP_PROXY_RENDER_SIZE_FULL;

    if (use_proxy) {
      int undistort = user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT;
      get_proxy_filepath(clip, user->render_size, undistort, user->framenr, filepath);
    }
    else {
      get_sequence_filepath(clip, user->framenr, filepath);
    }
  }
  else {
    BLI_strncpy(filepath, clip->filepath, FILE_MAX);
    BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&clip->id));
  }
}

ImBuf *BKE_movieclip_anim_ibuf_for_frame_no_lock(MovieClip *clip, const MovieClipUser *user)
{
  ImBuf *ibuf = nullptr;

  if (clip->source == MCLIP_SRC_MOVIE) {
    ibuf = movieclip_load_movie_file(clip, user, user->framenr, clip->flag);
  }

  return ibuf;
}

bool BKE_movieclip_has_cached_frame(MovieClip *clip, const MovieClipUser *user)
{
  bool has_frame = false;

  BLI_thread_lock(LOCK_MOVIECLIP);
  has_frame = has_imbuf_cache(clip, user, clip->flag);
  BLI_thread_unlock(LOCK_MOVIECLIP);

  return has_frame;
}

bool BKE_movieclip_put_frame_if_possible(MovieClip *clip, const MovieClipUser *user, ImBuf *ibuf)
{
  bool result;

  BLI_thread_lock(LOCK_MOVIECLIP);
  result = put_imbuf_cache(clip, user, ibuf, clip->flag, false);
  BLI_thread_unlock(LOCK_MOVIECLIP);

  return result;
}

static void movieclip_eval_update_reload(Depsgraph *depsgraph, Main *bmain, MovieClip *clip)
{
  BKE_movieclip_reload(bmain, clip);
  if (DEG_is_active(depsgraph)) {
    MovieClip *clip_orig = DEG_get_original(clip);
    BKE_movieclip_reload(bmain, clip_orig);
  }
}

static void movieclip_eval_update_generic(Depsgraph *depsgraph, MovieClip *clip)
{
  BKE_tracking_dopesheet_tag_update(&clip->tracking);
  if (DEG_is_active(depsgraph)) {
    MovieClip *clip_orig = DEG_get_original(clip);
    BKE_tracking_dopesheet_tag_update(&clip_orig->tracking);
  }
}

void BKE_movieclip_eval_update(Depsgraph *depsgraph, Main *bmain, MovieClip *clip)
{
  DEG_debug_print_eval(depsgraph, __func__, clip->id.name, clip);
  if (clip->id.recalc & ID_RECALC_SOURCE) {
    movieclip_eval_update_reload(depsgraph, bmain, clip);
  }
  else {
    movieclip_eval_update_generic(depsgraph, clip);
  }
  clip->runtime.last_update = DEG_get_update_count(depsgraph);
}

/* -------------------------------------------------------------------- */
/** \name GPU textures
 * \{ */

static blender::gpu::Texture **movieclip_get_gputexture_ptr(MovieClip *clip,
                                                            MovieClipUser *cuser,
                                                            eGPUTextureTarget textarget)
{
  /* Check if we have an existing entry for that clip user. */
  MovieClip_RuntimeGPUTexture *tex;
  for (tex = static_cast<MovieClip_RuntimeGPUTexture *>(clip->runtime.gputextures.first); tex;
       tex = static_cast<MovieClip_RuntimeGPUTexture *>(tex->next))
  {
    if (memcmp(&tex->user, cuser, sizeof(MovieClipUser)) == 0) {
      break;
    }
  }

  /* If not, allocate a new one. */
  if (tex == nullptr) {
    tex = MEM_mallocN<MovieClip_RuntimeGPUTexture>(__func__);

    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      tex->gputexture[i] = nullptr;
    }

    memcpy(&tex->user, cuser, sizeof(MovieClipUser));
    BLI_addtail(&clip->runtime.gputextures, tex);
  }

  return &tex->gputexture[textarget];
}

blender::gpu::Texture *BKE_movieclip_get_gpu_texture(MovieClip *clip, MovieClipUser *cuser)
{
  if (clip == nullptr) {
    return nullptr;
  }

  blender::gpu::Texture **tex = movieclip_get_gputexture_ptr(clip, cuser, TEXTARGET_2D);
  if (*tex) {
    return *tex;
  }

  /* check if we have a valid image buffer */
  ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, cuser);
  if (ibuf == nullptr) {
    CLOG_ERROR(&LOG, "Failed to create GPU texture from Blender movie clip");
    *tex = GPU_texture_create_error(2, false);
    return *tex;
  }

  /* This only means RGBA16F instead of RGBA32F. */
  const bool high_bitdepth = false;
  const bool store_premultiplied = ibuf->float_buffer.data ? false : true;
  *tex = IMB_create_gpu_texture(clip->id.name + 2, ibuf, high_bitdepth, store_premultiplied);

  /* Do not generate mips for movieclips... too slow. */
  GPU_texture_mipmap_mode(*tex, false, true);

  IMB_freeImBuf(ibuf);

  return *tex;
}

void BKE_movieclip_free_gputexture(MovieClip *clip)
{
  /* Number of gpu textures to keep around as cache.
   * We don't want to keep too many GPU textures for
   * movie clips around, as they can be large. */
  const int MOVIECLIP_NUM_GPUTEXTURES = 1;

  while (BLI_listbase_count(&clip->runtime.gputextures) > MOVIECLIP_NUM_GPUTEXTURES) {
    MovieClip_RuntimeGPUTexture *tex = (MovieClip_RuntimeGPUTexture *)BLI_pophead(
        &clip->runtime.gputextures);
    for (int i = 0; i < TEXTARGET_COUNT; i++) {
      /* Free GLSL image binding. */
      if (tex->gputexture[i]) {
        GPU_texture_free(tex->gputexture[i]);
        tex->gputexture[i] = nullptr;
      }
    }
    MEM_freeN(tex);
  }
}

/** \} */

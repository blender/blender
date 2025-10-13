/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "IMB_colormanagement.hh"
#include "IMB_colormanagement_intern.hh"

#include <cmath>
#include <cstring>

#include "DNA_ID.h"
#include "DNA_color_types.h"
#include "DNA_image_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "IMB_filetype.hh"
#include "IMB_filter.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_moviecache.hh"

#include "MEM_guardedalloc.h"

#include "BLI_color.hh"
#include "BLI_colorspace.hh"
#include "BLI_fileops.hh"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_color.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_path_utils.hh"
#include "BLI_rect.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_vector_set.hh"

#include "BKE_appdir.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_image_format.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"

#include "GPU_capabilities.hh"

#include "RNA_define.hh"

#include "SEQ_iterator.hh"

#include "DEG_depsgraph.hh"

#include "CLG_log.h"

#include "OCIO_api.hh"

static CLG_LogRef LOG = {"color_management"};

using blender::float3;
using blender::float3x3;
using blender::StringRef;
using blender::StringRefNull;

namespace ocio = blender::ocio;
namespace math = blender::math;

/* -------------------------------------------------------------------- */
/** \name Global declarations
 * \{ */

static std::unique_ptr<ocio::Config> g_config = nullptr;
static bool g_config_is_custom = false;
static blender::VectorSet<blender::StringRefNull> g_all_view_names;

#define DISPLAY_BUFFER_CHANNELS 4

/* ** list of all supported color spaces, displays and views */
static char global_role_data[MAX_COLORSPACE_NAME];
static char global_role_scene_linear[MAX_COLORSPACE_NAME];
static char global_role_color_picking[MAX_COLORSPACE_NAME];
static char global_role_texture_painting[MAX_COLORSPACE_NAME];
static char global_role_default_byte[MAX_COLORSPACE_NAME];
static char global_role_default_float[MAX_COLORSPACE_NAME];
static char global_role_default_sequencer[MAX_COLORSPACE_NAME];
static char global_role_aces_interchange[MAX_COLORSPACE_NAME];

/* Defaults from the config that never change with working space. */
static char global_role_scene_linear_default[MAX_COLORSPACE_NAME];
static char global_role_default_float_default[MAX_COLORSPACE_NAME];

float3x3 global_scene_linear_to_xyz_default = float3x3::zero();

/* lock used by pre-cached processors getters, so processor wouldn't
 * be created several times
 * LOCK_COLORMANAGE can not be used since this mutex could be needed to
 * be locked before pre-cached processor are creating
 */
static pthread_mutex_t processor_lock = BLI_MUTEX_INITIALIZER;

struct ColormanageProcessor {
  std::shared_ptr<const ocio::CPUProcessor> cpu_processor;
  CurveMapping *curve_mapping;
  bool is_data_result;
};

static struct GlobalGPUState {
  GlobalGPUState() = default;

  ~GlobalGPUState()
  {
    if (curve_mapping) {
      BKE_curvemapping_free(curve_mapping);
    }
  }

  /* GPU shader currently bound. */
  bool gpu_shader_bound = false;

  /* Curve mapping. */
  CurveMapping *curve_mapping = nullptr, *orig_curve_mapping = nullptr;
  bool use_curve_mapping = false;
  int curve_mapping_timestamp = 0;
} global_gpu_state;

static struct GlobalColorPickingState {
  GlobalColorPickingState() = default;

  /* Cached processor for color picking conversion. */
  std::shared_ptr<const ocio::CPUProcessor> cpu_processor_to;
  std::shared_ptr<const ocio::CPUProcessor> cpu_processor_from;
  bool failed = false;
} global_color_picking_state;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Managed Cache
 * \{ */

/**
 * Cache Implementation Notes
 * ==========================
 *
 * All color management cache stuff is stored in two properties of
 * image buffers:
 *
 *   1. display_buffer_flags
 *
 *      This is a bit field which used to mark calculated transformations
 *      for particular image buffer. Index inside of this array means index
 *      of a color managed display. Element with given index matches view
 *      transformations applied for a given display. So if bit B of array
 *      element B is set to 1, this means display buffer with display index
 *      of A and view transform of B was ever calculated for this imbuf.
 *
 *      In contrast with indices in global lists of displays and views this
 *      indices are 0-based, not 1-based. This is needed to save some bytes
 *      of memory.
 *
 *   2. colormanage_cache
 *
 *      This is a pointer to a structure which holds all data which is
 *      needed for color management cache to work.
 *
 *      It contains two parts:
 *        - data
 *        - moviecache
 *
 *      Data field is used to store additional information about cached
 *      buffers which affects on whether cached buffer could be used.
 *      This data can't go to cache key because changes in this data
 *      shouldn't lead extra buffers adding to cache, it shall
 *      invalidate cached images.
 *
 *      Currently such a data contains only exposure and gamma, but
 *      would likely extended further.
 *
 *      data field is not null only for elements of cache, not used for
 *      original image buffers.
 *
 *      Color management cache is using generic MovieCache implementation
 *      to make it easier to deal with memory limitation.
 *
 *      Currently color management is using the same memory limitation
 *      pool as sequencer and clip editor are using which means color
 *      managed buffers would be removed from the cache as soon as new
 *      frames are loading for the movie clip and there's no space in
 *      cache.
 *
 *      Every image buffer has got own movie cache instance, which
 *      means keys for color managed buffers could be really simple
 *      and look up in this cache would be fast and independent from
 *      overall amount of color managed images.
 */

/* NOTE: ColormanageCacheViewSettings and ColormanageCacheDisplaySettings are
 *       quite the same as ColorManagedViewSettings and ColorManageDisplaySettings
 *       but they holds indexes of all transformations and color spaces, not
 *       their names.
 *
 *       This helps avoid extra colorspace / display / view lookup without
 *       requiring to pass all variables which affects on display buffer
 *       to color management cache system and keeps calls small and nice.
 */
struct ColormanageCacheViewSettings {
  int flag;
  int look;
  int view;
  float exposure;
  float gamma;
  float dither;
  float temperature;
  float tint;
  CurveMapping *curve_mapping;
};

struct ColormanageCacheDisplaySettings {
  int display;
};

struct ColormanageCacheKey {
  int view;    /* view transformation used for display buffer */
  int display; /* display device name */
};

struct ColormanageCacheData {
  int flag;                    /* view flags of cached buffer */
  int look;                    /* Additional artistic transform. */
  float exposure;              /* exposure value cached buffer is calculated with */
  float gamma;                 /* gamma value cached buffer is calculated with */
  float dither;                /* dither value cached buffer is calculated with */
  float temperature;           /* temperature value cached buffer is calculated with */
  float tint;                  /* tint value cached buffer is calculated with */
  CurveMapping *curve_mapping; /* curve mapping used for cached buffer */
  int curve_mapping_timestamp; /* time stamp of curve mapping used for cached buffer */
};

struct ColormanageCache {
  MovieCache *moviecache;

  ColormanageCacheData *data;
};

static MovieCache *colormanage_moviecache_get(const ImBuf *ibuf)
{
  if (!ibuf->colormanage_cache) {
    return nullptr;
  }

  return ibuf->colormanage_cache->moviecache;
}

static ColormanageCacheData *colormanage_cachedata_get(const ImBuf *ibuf)
{
  if (!ibuf->colormanage_cache) {
    return nullptr;
  }

  return ibuf->colormanage_cache->data;
}

static uint colormanage_hashhash(const void *key_v)
{
  const ColormanageCacheKey *key = static_cast<const ColormanageCacheKey *>(key_v);

  uint rval = (key->display << 16) | (key->view % 0xffff);

  return rval;
}

static bool colormanage_hashcmp(const void *av, const void *bv)
{
  const ColormanageCacheKey *a = static_cast<const ColormanageCacheKey *>(av);
  const ColormanageCacheKey *b = static_cast<const ColormanageCacheKey *>(bv);

  return ((a->view != b->view) || (a->display != b->display));
}

static MovieCache *colormanage_moviecache_ensure(ImBuf *ibuf)
{
  if (!ibuf->colormanage_cache) {
    ibuf->colormanage_cache = MEM_callocN<ColormanageCache>("imbuf colormanage cache");
  }

  if (!ibuf->colormanage_cache->moviecache) {
    MovieCache *moviecache;

    moviecache = IMB_moviecache_create("colormanage cache",
                                       sizeof(ColormanageCacheKey),
                                       colormanage_hashhash,
                                       colormanage_hashcmp);

    ibuf->colormanage_cache->moviecache = moviecache;
  }

  return ibuf->colormanage_cache->moviecache;
}

static void colormanage_cachedata_set(ImBuf *ibuf, ColormanageCacheData *data)
{
  if (!ibuf->colormanage_cache) {
    ibuf->colormanage_cache = MEM_callocN<ColormanageCache>("imbuf colormanage cache");
  }

  ibuf->colormanage_cache->data = data;
}

static void colormanage_view_settings_to_cache(ImBuf *ibuf,
                                               ColormanageCacheViewSettings *cache_view_settings,
                                               const ColorManagedViewSettings *view_settings)
{
  int look = IMB_colormanagement_look_get_named_index(view_settings->look);
  int view = IMB_colormanagement_view_get_id_by_name(view_settings->view_transform);

  cache_view_settings->look = look;
  cache_view_settings->view = view;
  cache_view_settings->exposure = view_settings->exposure;
  cache_view_settings->gamma = view_settings->gamma;
  cache_view_settings->dither = ibuf->dither;
  cache_view_settings->temperature = view_settings->temperature;
  cache_view_settings->tint = view_settings->tint;
  cache_view_settings->flag = view_settings->flag;
  cache_view_settings->curve_mapping = view_settings->curve_mapping;
}

static void colormanage_display_settings_to_cache(
    ColormanageCacheDisplaySettings *cache_display_settings,
    const ColorManagedDisplaySettings *display_settings)
{
  int display = IMB_colormanagement_display_get_named_index(display_settings->display_device);

  cache_display_settings->display = display;
}

static void colormanage_settings_to_key(ColormanageCacheKey *key,
                                        const ColormanageCacheViewSettings *view_settings,
                                        const ColormanageCacheDisplaySettings *display_settings)
{
  key->view = view_settings->view;
  key->display = display_settings->display;
}

static ImBuf *colormanage_cache_get_ibuf(ImBuf *ibuf,
                                         ColormanageCacheKey *key,
                                         void **cache_handle)
{
  ImBuf *cache_ibuf;
  MovieCache *moviecache = colormanage_moviecache_get(ibuf);

  if (!moviecache) {
    /* If there's no moviecache it means no color management was applied
     * on given image buffer before. */
    return nullptr;
  }

  *cache_handle = nullptr;

  cache_ibuf = IMB_moviecache_get(moviecache, key, nullptr);

  *cache_handle = cache_ibuf;

  return cache_ibuf;
}

static uchar *colormanage_cache_get(ImBuf *ibuf,
                                    const ColormanageCacheViewSettings *view_settings,
                                    const ColormanageCacheDisplaySettings *display_settings,
                                    void **cache_handle)
{
  ColormanageCacheKey key;
  ImBuf *cache_ibuf;
  int view_flag = 1 << view_settings->view;
  CurveMapping *curve_mapping = view_settings->curve_mapping;
  int curve_mapping_timestamp = curve_mapping ? curve_mapping->changed_timestamp : 0;

  colormanage_settings_to_key(&key, view_settings, display_settings);

  /* check whether image was marked as dirty for requested transform */
  if ((ibuf->display_buffer_flags[display_settings->display] & view_flag) == 0) {
    return nullptr;
  }

  cache_ibuf = colormanage_cache_get_ibuf(ibuf, &key, cache_handle);

  if (cache_ibuf) {

    BLI_assert(cache_ibuf->x == ibuf->x && cache_ibuf->y == ibuf->y);

    /* only buffers with different color space conversions are being stored
     * in cache separately. buffer which were used only different exposure/gamma
     * are re-suing the same cached buffer
     *
     * check here which exposure/gamma/curve was used for cached buffer and if they're
     * different from requested buffer should be re-generated
     */
    const ColormanageCacheData *cache_data = colormanage_cachedata_get(cache_ibuf);

    if (cache_data->look != view_settings->look ||
        cache_data->exposure != view_settings->exposure ||
        cache_data->gamma != view_settings->gamma || cache_data->dither != view_settings->dither ||
        cache_data->temperature != view_settings->temperature ||
        cache_data->tint != view_settings->tint || cache_data->flag != view_settings->flag ||
        cache_data->curve_mapping != curve_mapping ||
        cache_data->curve_mapping_timestamp != curve_mapping_timestamp)
    {
      *cache_handle = nullptr;

      IMB_freeImBuf(cache_ibuf);

      return nullptr;
    }

    return (uchar *)cache_ibuf->byte_buffer.data;
  }

  return nullptr;
}

static void colormanage_cache_put(ImBuf *ibuf,
                                  const ColormanageCacheViewSettings *view_settings,
                                  const ColormanageCacheDisplaySettings *display_settings,
                                  uchar *display_buffer,
                                  void **cache_handle)
{
  ColormanageCacheKey key;
  ImBuf *cache_ibuf;
  ColormanageCacheData *cache_data;
  int view_flag = 1 << view_settings->view;
  MovieCache *moviecache = colormanage_moviecache_ensure(ibuf);
  CurveMapping *curve_mapping = view_settings->curve_mapping;
  int curve_mapping_timestamp = curve_mapping ? curve_mapping->changed_timestamp : 0;

  colormanage_settings_to_key(&key, view_settings, display_settings);

  /* mark display buffer as valid */
  ibuf->display_buffer_flags[display_settings->display] |= view_flag;

  /* buffer itself */
  cache_ibuf = IMB_allocImBuf(ibuf->x, ibuf->y, ibuf->planes, 0);
  IMB_assign_byte_buffer(cache_ibuf, display_buffer, IB_TAKE_OWNERSHIP);

  /* Store data which is needed to check whether cached buffer
   * could be used for color managed display settings. */
  cache_data = MEM_callocN<ColormanageCacheData>("color manage cache imbuf data");
  cache_data->look = view_settings->look;
  cache_data->exposure = view_settings->exposure;
  cache_data->gamma = view_settings->gamma;
  cache_data->dither = view_settings->dither;
  cache_data->temperature = view_settings->temperature;
  cache_data->tint = view_settings->tint;
  cache_data->flag = view_settings->flag;
  cache_data->curve_mapping = curve_mapping;
  cache_data->curve_mapping_timestamp = curve_mapping_timestamp;

  colormanage_cachedata_set(cache_ibuf, cache_data);

  *cache_handle = cache_ibuf;

  IMB_moviecache_put(moviecache, &key, cache_ibuf);
}

static void colormanage_cache_handle_release(void *cache_handle)
{
  ImBuf *cache_ibuf = static_cast<ImBuf *>(cache_handle);

  IMB_freeImBuf(cache_ibuf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialization / De-initialization
 * \{ */

static bool colormanage_role_color_space_name_get(ocio::Config &config,
                                                  char *colorspace_name,
                                                  const char *role,
                                                  const char *backup_role,
                                                  const bool optional = false)
{
  const ColorSpace *ociocs = config.get_color_space(role);

  if (ociocs == nullptr && backup_role) {
    ociocs = config.get_color_space(backup_role);
  }

  if (ociocs == nullptr) {
    /* Overall fallback role. */
    ociocs = config.get_color_space("default");
  }

  if (ociocs == nullptr) {
    if (!optional) {
      CLOG_ERROR(&LOG, "Could not find role \"%s\"", role);
    }
    colorspace_name[0] = '\0';
    return false;
  }

  /* assume function was called with buffer properly allocated to MAX_COLORSPACE_NAME chars */
  BLI_strncpy_utf8(colorspace_name, ociocs->name().c_str(), MAX_COLORSPACE_NAME);
  return true;
}

static void colormanage_update_matrices()
{
  /* Load luminance coefficients. */
  blender::colorspace::luma_coefficients = g_config->get_default_luma_coefs();

  /* Load standard color spaces. */
  blender::colorspace::xyz_to_scene_linear = g_config->get_xyz_to_scene_linear_matrix();
  blender::colorspace::scene_linear_to_xyz = math::invert(
      blender::colorspace::xyz_to_scene_linear);

  blender::colorspace::scene_linear_to_rec709 = ocio::XYZ_TO_REC709 *
                                                blender::colorspace::scene_linear_to_xyz;
  blender::colorspace::rec709_to_scene_linear = math::invert(
      blender::colorspace::scene_linear_to_rec709);

  blender::colorspace::scene_linear_to_rec2020 = ocio::XYZ_TO_REC2020 *
                                                 blender::colorspace::scene_linear_to_xyz;
  blender::colorspace::rec2020_to_scene_linear = math::invert(
      blender::colorspace::scene_linear_to_rec2020);

  blender::colorspace::aces_to_scene_linear = blender::colorspace::xyz_to_scene_linear *
                                              ocio::ACES_TO_XYZ;
  blender::colorspace::scene_linear_to_aces = math::invert(
      blender::colorspace::aces_to_scene_linear);

  blender::colorspace::acescg_to_scene_linear = blender::colorspace::xyz_to_scene_linear *
                                                ocio::ACESCG_TO_XYZ;
  blender::colorspace::scene_linear_to_acescg = math::invert(
      blender::colorspace::acescg_to_scene_linear);

  blender::colorspace::scene_linear_is_rec709 = math::is_equal(
      blender::colorspace::scene_linear_to_rec709, float3x3::identity(), 0.0001f);
}

static bool colormanage_load_config(ocio::Config &config)
{
  bool ok = true;

  /* get roles */
  ok &= colormanage_role_color_space_name_get(config, global_role_data, OCIO_ROLE_DATA, nullptr);
  ok &= colormanage_role_color_space_name_get(
      config, global_role_scene_linear, OCIO_ROLE_SCENE_LINEAR, nullptr);
  ok &= colormanage_role_color_space_name_get(
      config, global_role_color_picking, OCIO_ROLE_COLOR_PICKING, nullptr);
  ok &= colormanage_role_color_space_name_get(
      config, global_role_texture_painting, OCIO_ROLE_TEXTURE_PAINT, nullptr);
  ok &= colormanage_role_color_space_name_get(
      config, global_role_default_sequencer, OCIO_ROLE_DEFAULT_SEQUENCER, OCIO_ROLE_SCENE_LINEAR);
  ok &= colormanage_role_color_space_name_get(
      config, global_role_default_byte, OCIO_ROLE_DEFAULT_BYTE, OCIO_ROLE_TEXTURE_PAINT);
  ok &= colormanage_role_color_space_name_get(
      config, global_role_default_float, OCIO_ROLE_DEFAULT_FLOAT, OCIO_ROLE_SCENE_LINEAR);

  colormanage_role_color_space_name_get(
      config, global_role_aces_interchange, OCIO_ROLE_ACES_INTERCHANGE, nullptr, true);

  if (g_config->get_num_displays() == 0) {
    CLOG_ERROR(&LOG, "Could not find any displays");
    ok = false;
  }
  /* NOTE: The look "None" is expected to be hard-coded to exist in the OpenColorIO integration. */
  if (g_config->get_num_looks() == 0) {
    CLOG_ERROR(&LOG, "Could not find any looks");
    ok = false;
  }

  for (const int display_index : blender::IndexRange(g_config->get_num_displays())) {
    const ocio::Display *display = g_config->get_display_by_index(display_index);
    const int num_views = display->get_num_views();
    if (num_views <= 0) {
      CLOG_ERROR(&LOG, "Could not find any views for display %s", display->name().c_str());
      ok = false;
      break;
    }

    for (const int view_index : blender::IndexRange(num_views)) {
      const ocio::View *view = display->get_view_by_index(view_index);
      g_all_view_names.add(view->name());
    }
  }

  colormanage_update_matrices();

  /* Defaults that don't change with file working space. */
  STRNCPY(global_role_scene_linear_default, global_role_scene_linear);
  STRNCPY(global_role_default_float_default, global_role_default_float);
  global_scene_linear_to_xyz_default = blender::colorspace::scene_linear_to_xyz;

  return ok;
}

static void colormanage_free_config()
{
  g_config = nullptr;
  g_all_view_names.clear();
}

void colormanagement_init()
{
  /* Handle Blender specific override. */
  const char *blender_ocio_env = BLI_getenv("BLENDER_OCIO");
  if (blender_ocio_env) {
    BLI_setenv("OCIO", blender_ocio_env);
  }

  /* First try config from environment variable. */
  const char *ocio_env = BLI_getenv("OCIO");

  if (ocio_env && ocio_env[0] != '\0') {
    g_config = ocio::Config::create_from_environment();
    if (g_config != nullptr) {
      CLOG_INFO_NOCHECK(&LOG, "Using %s as a configuration file", ocio_env);
      const bool ok = colormanage_load_config(*g_config);

      if (ok) {
        g_config_is_custom = true;
      }
      else {
        CLOG_ERROR(&LOG, "Failed to load config from environment");
        colormanage_free_config();
      }
    }
  }

  /* Then try bundled configuration file. */
  if (g_config == nullptr) {
    const std::optional<std::string> configdir = BKE_appdir_folder_id(BLENDER_DATAFILES,
                                                                      "colormanagement");
    if (configdir.has_value()) {
      char configfile[FILE_MAX];
      BLI_path_join(configfile, sizeof(configfile), configdir->c_str(), BCM_CONFIG_FILE);

      g_config = ocio::Config::create_from_file(configfile);

      if (g_config != nullptr) {
        const bool ok = colormanage_load_config(*g_config);
        if (!ok) {
          CLOG_ERROR(&LOG, "Failed to load bundled config");
          colormanage_free_config();
        }
      }
    }
  }

  /* Then use fallback. */
  if (g_config == nullptr) {
    CLOG_STR_INFO_NOCHECK(&LOG, "Using fallback mode for management");
    g_config = ocio::Config::create_fallback();
    colormanage_load_config(*g_config);
  }

  BLI_init_srgb_conversion();
}

void colormanagement_exit()
{
  global_gpu_state = GlobalGPUState();
  global_color_picking_state = GlobalColorPickingState();

  colormanage_free_config();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal functions
 * \{ */

static StringRef view_filter_for_look(StringRefNull view_name)
{
  if (view_name.is_empty()) {
    return StringRef();
  }

  /* First try to find any looks with the full name prefix. */
  for (const int look_index : blender::IndexRange(g_config->get_num_looks())) {
    const ocio::Look *look = g_config->get_look_by_index(look_index);
    if (look->view() == view_name) {
      return view_name;
    }
  }

  /* Then try with the short name prefix. */
  const int64_t separator_offset = view_name.find(" - ");
  if (separator_offset == -1) {
    return StringRef();
  }
  StringRef view_short_name = view_name.substr(0, separator_offset);

  for (const int look_index : blender::IndexRange(g_config->get_num_looks())) {
    const ocio::Look *look = g_config->get_look_by_index(look_index);
    if (look->view() == view_short_name) {
      return view_short_name;
    }
  }

  return StringRef();
}

static bool colormanage_compatible_look(const ocio::Look *look, StringRef view_filter)
{
  if (look->is_noop) {
    return true;
  }

  /* Skip looks only relevant to specific view transforms.
   * If the view transform has view-specific look ignore non-specific looks. */
  return view_filter.is_empty() ? look->view().is_empty() : look->view() == view_filter;
}

static bool colormanage_compatible_look(const ocio::Look *look, const char *view_name)
{
  const StringRef view_filter = view_filter_for_look(view_name);
  return colormanage_compatible_look(look, view_filter);
}

static bool colormanage_use_look(const char *look_name, const char *view_name)
{
  const ocio::Look *look = g_config->get_look_by_name(look_name);
  return (look->is_noop == false && colormanage_compatible_look(look, view_name));
}

void colormanage_cache_free(ImBuf *ibuf)
{
  MEM_SAFE_FREE(ibuf->display_buffer_flags);

  if (ibuf->colormanage_cache) {
    ColormanageCacheData *cache_data = colormanage_cachedata_get(ibuf);
    MovieCache *moviecache = colormanage_moviecache_get(ibuf);

    if (cache_data) {
      MEM_freeN(cache_data);
    }

    if (moviecache) {
      IMB_moviecache_free(moviecache);
    }

    MEM_freeN(ibuf->colormanage_cache);

    ibuf->colormanage_cache = nullptr;
  }
}

void IMB_colormanagement_display_settings_from_ctx(
    const bContext *C,
    ColorManagedViewSettings **r_view_settings,
    ColorManagedDisplaySettings **r_display_settings)
{
  Scene *scene = CTX_data_scene(C);
  SpaceImage *sima = CTX_wm_space_image(C);

  *r_view_settings = &scene->view_settings;
  *r_display_settings = &scene->display_settings;

  if (sima && sima->image) {
    if ((sima->image->flag & IMA_VIEW_AS_RENDER) == 0) {
      *r_view_settings = nullptr;
    }
  }
}

static bool get_display_emulation(const ColorManagedDisplaySettings &display_settings)
{
  switch (display_settings.emulation) {
    case COLORMANAGE_DISPLAY_EMULATION_OFF:
      return false;
    case COLORMANAGE_DISPLAY_EMULATION_AUTO:
      return true;
  }

  return true;
}

static std::shared_ptr<const ocio::CPUProcessor> get_display_buffer_processor(
    const ColorManagedDisplaySettings &display_settings,
    const char *look,
    const char *view_transform,
    const float exposure,
    const float gamma,
    const float temperature,
    const float tint,
    const bool use_white_balance,
    const char *from_colorspace,
    const ColorManagedDisplaySpace target,
    const bool inverse = false)
{
  ocio::DisplayParameters display_parameters;
  display_parameters.from_colorspace = from_colorspace;
  display_parameters.view = view_transform;
  display_parameters.display = display_settings.display_device;
  display_parameters.look = colormanage_use_look(look, view_transform) ? look : "";
  display_parameters.scale = (exposure == 0.0f) ? 1.0f : powf(2.0f, exposure);
  display_parameters.exponent = (gamma == 1.0f) ? 1.0f : 1.0f / max_ff(FLT_EPSILON, gamma);
  display_parameters.temperature = temperature;
  display_parameters.tint = tint;
  display_parameters.use_white_balance = use_white_balance;
  display_parameters.inverse = inverse;
  display_parameters.use_hdr_buffer = GPU_hdr_support();
  display_parameters.use_hdr_display = IMB_colormanagement_display_is_hdr(&display_settings,
                                                                          view_transform);
  display_parameters.is_image_output = (target == DISPLAY_SPACE_IMAGE_OUTPUT);
  display_parameters.use_display_emulation = (target == DISPLAY_SPACE_DRAW) ?
                                                 get_display_emulation(display_settings) :
                                                 false;

  return g_config->get_display_cpu_processor(display_parameters);
}

void IMB_colormanagement_init_untonemapped_view_settings(
    ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings * /*display_settings*/)
{
  /* Empty view transform name means skip tone mapping. */
  view_settings->view_transform[0] = '\0';
  /* TODO(sergey): Find a way to safely/reliable un-hardcode this. */
  STRNCPY_UTF8(view_settings->look, "None");
  /* Initialize rest of the settings. */
  view_settings->flag = 0;
  view_settings->gamma = 1.0f;
  view_settings->exposure = 0.0f;
  view_settings->temperature = 6500.0f;
  view_settings->tint = 10.0f;
  view_settings->curve_mapping = nullptr;
}

static void curve_mapping_apply_pixel(const CurveMapping *curve_mapping,
                                      float *pixel,
                                      int channels)
{
  if (channels == 1) {
    pixel[0] = BKE_curvemap_evaluateF(curve_mapping, curve_mapping->cm, pixel[0]);
  }
  else if (channels == 2) {
    pixel[0] = BKE_curvemap_evaluateF(curve_mapping, curve_mapping->cm, pixel[0]);
    pixel[1] = BKE_curvemap_evaluateF(curve_mapping, curve_mapping->cm, pixel[1]);
  }
  else {
    BKE_curvemapping_evaluate_premulRGBF(curve_mapping, pixel, pixel);
  }
}

void colormanage_imbuf_set_default_spaces(ImBuf *ibuf)
{
  /* Regression tests are allocating ImBuf. Guard against access of uninitialized color
   * management configuration. */
  /* TODO(sergey): Always allocate the fallback color management configuration for such cases? */
  if (!g_config) {
    return;
  }
  ibuf->byte_buffer.colorspace = g_config->get_color_space(global_role_default_byte);
}

void colormanage_imbuf_make_linear(ImBuf *ibuf,
                                   const char *from_colorspace,
                                   const ColorManagedFileOutput output)
{
  const ColorSpace *colorspace = g_config->get_color_space(from_colorspace);

  if (colorspace && colorspace->is_data()) {
    ibuf->colormanage_flag |= IMB_COLORMANAGE_IS_DATA;
    return;
  }

  if (ibuf->float_buffer.data) {
    const char *to_colorspace = global_role_scene_linear;
    const bool predivide = IMB_alpha_affects_rgb(ibuf);

    if (ibuf->byte_buffer.data) {
      IMB_free_byte_pixels(ibuf);
    }

    if (output != ColorManagedFileOutput::Video) {
      const ColorSpace *image_colorspace = g_config->get_color_space_for_hdr_image(
          from_colorspace);
      if (image_colorspace) {
        from_colorspace = image_colorspace->name().c_str();
      }
    }

    IMB_colormanagement_transform_float(ibuf->float_buffer.data,
                                        ibuf->x,
                                        ibuf->y,
                                        ibuf->channels,
                                        from_colorspace,
                                        to_colorspace,
                                        predivide);
    ibuf->float_buffer.colorspace = nullptr;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Functions
 * \{ */

static bool colormanage_check_display_settings(ColorManagedDisplaySettings *display_settings,
                                               const char *what,
                                               const ocio::Display *default_display)
{
  StringRefNull display_name = display_settings->display_device;

  if (display_name.is_empty()) {
    STRNCPY_UTF8(display_settings->display_device, default_display->name().c_str());
    return true;
  }

  const ocio::Display *display = g_config->get_display_by_name(display_name);
  if (display) {
    return true;
  }

  StringRefNull new_display_name = default_display->name();

  /* Try to find a similar name, so that we can match e.g. "sRGB - Display" and "sRGB".
   * There are aliases for color spaces, but not displays. */
  for (const int display_index : blender::IndexRange(g_config->get_num_displays())) {
    display = g_config->get_display_by_index(display_index);
    if (display->name().startswith(display_name) || display_name.startswith(display->name())) {
      new_display_name = display->name();
      break;
    }
  }

  CLOG_WARN(&LOG,
            "Display \"%s\" used by %s not found, setting to \"%s\".",
            display_settings->display_device,
            what,
            new_display_name.c_str());

  STRNCPY_UTF8(display_settings->display_device, new_display_name.c_str());
  return false;
}

static StringRefNull colormanage_find_matching_view_name(const ocio::Display *display,
                                                         StringRefNull view_name)
{
  /* Match untonemapped view conventions between Blender and ACES 2.0. */
  if (view_name == "Standard" && display->get_view_by_name("Un-tone-mapped")) {
    return "Un-tone-mapped";
  }
  if (view_name == "Un-tone-mapped" && display->get_view_by_name("Standard")) {
    return "Standard";
  }

  /* Try to find a similar name, so that we can match e.g. "ACES 2.0" and "ACES 2.0 - HDR
   * 1000 when switching between SDR and HDR displays. */
  for (const int view_index : blender::IndexRange(display->get_num_views())) {
    const ocio::View *view = display->get_view_by_index(view_index);
    if (view->name().startswith(view_name) || view_name.startswith(view->name())) {
      return view->name();
    }
  }

  const int64_t separator_offset = view_name.find(" - ");
  if (separator_offset != -1) {
    const StringRef view_short_name = view_name.substr(0, separator_offset);
    for (const int view_index : blender::IndexRange(display->get_num_views())) {
      const ocio::View *view = display->get_view_by_index(view_index);
      if (view->name().startswith(view_short_name)) {
        return view->name();
      }
    }
  }

  if (const ocio::View *view = display->get_default_view()) {
    return view->name();
  }

  return "";
}

static bool colormanage_check_view_settings(ColorManagedDisplaySettings *display_settings,
                                            ColorManagedViewSettings *view_settings,
                                            const char *what)
{
  const ocio::Display *display = g_config->get_display_by_name(display_settings->display_device);
  if (!display) {
    return false;
  }

  bool ok = true;

  const char *default_look_name = IMB_colormanagement_look_get_default_name();
  StringRefNull view_name = view_settings->view_transform;

  if (view_name.is_empty()) {
    const ocio::View *default_view = display->get_default_view();
    if (default_view) {
      STRNCPY_UTF8(view_settings->view_transform, default_view->name().c_str());
    }
  }
  else {
    const ocio::View *view = display->get_view_by_name(view_name);
    if (!view) {
      StringRefNull new_view_name = colormanage_find_matching_view_name(display, view_name);
      if (!new_view_name.is_empty()) {
        CLOG_WARN(&LOG,
                  "%s view \"%s\" not found, setting to \"%s\".",
                  what,
                  view_settings->view_transform,
                  new_view_name.c_str());
        STRNCPY_UTF8(view_settings->view_transform, new_view_name.c_str());
        ok = false;
      }
    }
  }

  if (view_settings->look[0] == '\0') {
    STRNCPY_UTF8(view_settings->look, default_look_name);
  }
  else {
    const ocio::Look *look = g_config->get_look_by_name(view_settings->look);
    if (look == nullptr) {
      CLOG_WARN(&LOG,
                "%s look \"%s\" not found, setting default \"%s\".",
                what,
                view_settings->look,
                default_look_name);

      STRNCPY_UTF8(view_settings->look, default_look_name);
      ok = false;
    }
    else if (!colormanage_compatible_look(look, view_settings->view_transform)) {
      CLOG_INFO(&LOG,
                "%s look \"%s\" is not compatible with view \"%s\", setting "
                "default "
                "\"%s\".",
                what,
                view_settings->look,
                view_settings->view_transform,
                default_look_name);

      STRNCPY_UTF8(view_settings->look, default_look_name);
      ok = false;
    }
  }

  /* OCIO_TODO: move to do_versions() */
  if (view_settings->exposure == 0.0f && view_settings->gamma == 0.0f) {
    view_settings->exposure = 0.0f;
    view_settings->gamma = 1.0f;
  }

  return ok;
}

static bool colormanage_check_colorspace_name(char *name, const char *what)
{
  bool ok = true;
  if (name[0] == '\0') {
    /* pass */
  }
  else {
    const ColorSpace *colorspace = g_config->get_color_space(name);

    if (!colorspace) {
      CLOG_WARN(&LOG, "%s colorspace \"%s\" not found, will use default instead.", what, name);
      name[0] = '\0';
      ok = false;
    }
  }

  (void)what;
  return ok;
}

static bool colormanage_check_colorspace_settings(ColorManagedColorspaceSettings *settings,
                                                  const char *what)
{
  return colormanage_check_colorspace_name(settings->name, what);
}

void IMB_colormanagement_check_file_config(Main *bmain)
{
  const ocio::Display *default_display = g_config->get_default_display();
  if (!default_display) {
    /* happens when OCIO configuration is incorrect */
    return;
  }

  /* Check display, view and colorspace names in datablocks. */
  bool is_missing_opencolorio_config = false;

  /* Check scenes. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    ColorManagedColorspaceSettings *sequencer_colorspace_settings;
    bool ok = true;

    /* check scene color management settings */
    ok &= colormanage_check_display_settings(&scene->display_settings, "scene", default_display);
    ok &= colormanage_check_view_settings(
        &scene->display_settings, &scene->view_settings, "scene");

    sequencer_colorspace_settings = &scene->sequencer_colorspace_settings;

    ok &= colormanage_check_colorspace_settings(sequencer_colorspace_settings, "sequencer");

    if (sequencer_colorspace_settings->name[0] == '\0') {
      STRNCPY_UTF8(sequencer_colorspace_settings->name, global_role_default_sequencer);
    }

    /* Check sequencer strip input colorspace. */
    if (scene->ed != nullptr) {
      blender::seq::foreach_strip(&scene->ed->seqbase, [&](Strip *strip) {
        if (strip->data) {
          ok &= colormanage_check_colorspace_settings(&strip->data->colorspace_settings,
                                                      "sequencer strip");
        }
        return true;
      });
    }

    is_missing_opencolorio_config |= (!ok && !ID_IS_LINKED(&scene->id));
  }

  /* Check image and movie input colorspace. */
  LISTBASE_FOREACH (Image *, image, &bmain->images) {
    const bool ok = colormanage_check_colorspace_settings(&image->colorspace_settings, "image");
    is_missing_opencolorio_config |= (!ok && !ID_IS_LINKED(&image->id));
  }

  LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
    const bool ok = colormanage_check_colorspace_settings(&clip->colorspace_settings, "clip");
    is_missing_opencolorio_config |= (!ok && !ID_IS_LINKED(&clip->id));
  }

  /* Check compositing nodes. */
  LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
    if (ntree->type == NTREE_COMPOSIT) {
      bool ok = true;
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy == CMP_NODE_CONVERT_TO_DISPLAY) {
          NodeConvertToDisplay *nctd = static_cast<NodeConvertToDisplay *>(node->storage);
          ok &= colormanage_check_display_settings(
              &nctd->display_settings, "node", default_display);
          ok &= colormanage_check_view_settings(
              &nctd->display_settings, &nctd->view_settings, "node");
        }
        else if (node->type_legacy == CMP_NODE_CONVERT_COLOR_SPACE) {
          NodeConvertColorSpace *ncs = static_cast<NodeConvertColorSpace *>(node->storage);
          ok &= colormanage_check_colorspace_name(ncs->from_color_space, "node");
          ok &= colormanage_check_colorspace_name(ncs->to_color_space, "node");
        }
      }
      is_missing_opencolorio_config |= (!ok && !ID_IS_LINKED(&ntree->id));
    }
  }

  /* Inform users about mismatch, but not for new files. Linked datablocks are also ignored,
   * as we are not overwriting them on blend file save which is the main purpose of this
   * warning. */
  if (bmain->filepath[0] != '\0' && is_missing_opencolorio_config) {
    bmain->colorspace.is_missing_opencolorio_config = true;
  }
}

void IMB_colormanagement_validate_settings(const ColorManagedDisplaySettings *display_settings,
                                           ColorManagedViewSettings *view_settings)
{
  const ocio::Display *display = g_config->get_display_by_name(display_settings->display_device);

  bool found = false;
  for (const int view_index : blender::IndexRange(display->get_num_views())) {
    const ocio::View *view = display->get_view_by_index(view_index);
    if (view->name() == view_settings->view_transform) {
      found = true;
      break;
    }
  }

  if (!found) {
    StringRefNull new_view_name = colormanage_find_matching_view_name(
        display, view_settings->view_transform);
    if (!new_view_name.is_empty()) {
      STRNCPY_UTF8(view_settings->view_transform, new_view_name.c_str());
    }
  }
}

const char *IMB_colormanagement_role_colorspace_name_get(int role)
{
  switch (role) {
    case COLOR_ROLE_DATA:
      return global_role_data;
    case COLOR_ROLE_SCENE_LINEAR:
      return global_role_scene_linear;
    case COLOR_ROLE_COLOR_PICKING:
      return global_role_color_picking;
    case COLOR_ROLE_TEXTURE_PAINTING:
      return global_role_texture_painting;
    case COLOR_ROLE_DEFAULT_SEQUENCER:
      return global_role_default_sequencer;
    case COLOR_ROLE_DEFAULT_FLOAT:
      return global_role_default_float;
    case COLOR_ROLE_DEFAULT_BYTE:
      return global_role_default_byte;
    case COLOR_ROLE_ACES_INTERCHANGE:
      return global_role_aces_interchange;
    default:
      CLOG_WARN(&LOG, "Unknown role was passed to %s", __func__);
      BLI_assert(0);
      break;
  }

  return nullptr;
}

void IMB_colormanagement_check_is_data(ImBuf *ibuf, const char *name)
{
  const ColorSpace *colorspace = g_config->get_color_space(name);

  if (colorspace && colorspace->is_data()) {
    ibuf->colormanage_flag |= IMB_COLORMANAGE_IS_DATA;
  }
  else {
    ibuf->colormanage_flag &= ~IMB_COLORMANAGE_IS_DATA;
  }
}

void IMB_colormanagegent_copy_settings(ImBuf *ibuf_src, ImBuf *ibuf_dst)
{
  IMB_colormanagement_assign_byte_colorspace(ibuf_dst,
                                             IMB_colormanagement_get_rect_colorspace(ibuf_src));
  IMB_colormanagement_assign_float_colorspace(ibuf_dst,
                                              IMB_colormanagement_get_float_colorspace(ibuf_src));
  if (ibuf_src->flags & IB_alphamode_premul) {
    ibuf_dst->flags |= IB_alphamode_premul;
  }
  else if (ibuf_src->flags & IB_alphamode_channel_packed) {
    ibuf_dst->flags |= IB_alphamode_channel_packed;
  }
  else if (ibuf_src->flags & IB_alphamode_ignore) {
    ibuf_dst->flags |= IB_alphamode_ignore;
  }
}

void IMB_colormanagement_assign_float_colorspace(ImBuf *ibuf, const char *name)
{
  const ColorSpace *colorspace = g_config->get_color_space(name);

  ibuf->float_buffer.colorspace = colorspace;

  if (colorspace && colorspace->is_data()) {
    ibuf->colormanage_flag |= IMB_COLORMANAGE_IS_DATA;
  }
  else {
    ibuf->colormanage_flag &= ~IMB_COLORMANAGE_IS_DATA;
  }
}

void IMB_colormanagement_assign_byte_colorspace(ImBuf *ibuf, const char *name)
{
  const ColorSpace *colorspace = g_config->get_color_space(name);

  ibuf->byte_buffer.colorspace = colorspace;

  if (colorspace && colorspace->is_data()) {
    ibuf->colormanage_flag |= IMB_COLORMANAGE_IS_DATA;
  }
  else {
    ibuf->colormanage_flag &= ~IMB_COLORMANAGE_IS_DATA;
  }
}

const char *IMB_colormanagement_get_float_colorspace(const ImBuf *ibuf)
{
  if (ibuf->float_buffer.colorspace) {
    return ibuf->float_buffer.colorspace->name().c_str();
  }

  return IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);
}

const char *IMB_colormanagement_get_rect_colorspace(const ImBuf *ibuf)
{
  if (ibuf->byte_buffer.colorspace) {
    return ibuf->byte_buffer.colorspace->name().c_str();
  }

  return IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);
}

const char *IMB_colormanagement_space_from_filepath_rules(const char *filepath)
{
  return g_config->get_color_space_from_filepath(filepath);
}

const ColorSpace *IMB_colormanagement_space_get_named(const char *name)
{
  return g_config->get_color_space(name);
}

bool IMB_colormanagement_space_is_data(const ColorSpace *colorspace)
{
  return (colorspace && colorspace->is_data());
}

bool IMB_colormanagement_space_is_scene_linear(const ColorSpace *colorspace)
{
  return (colorspace && colorspace->is_scene_linear());
}

bool IMB_colormanagement_space_is_srgb(const ColorSpace *colorspace)
{
  return (colorspace && colorspace->is_srgb());
}

bool IMB_colormanagement_space_name_is_data(const char *name)
{
  const ColorSpace *colorspace = g_config->get_color_space(name);
  return (colorspace && colorspace->is_data());
}

bool IMB_colormanagement_space_name_is_scene_linear(const char *name)
{
  const ColorSpace *colorspace = g_config->get_color_space(name);
  return (colorspace && IMB_colormanagement_space_is_scene_linear(colorspace));
}

bool IMB_colormanagement_space_name_is_srgb(const char *name)
{
  const ColorSpace *colorspace = g_config->get_color_space(name);
  return (colorspace && IMB_colormanagement_space_is_srgb(colorspace));
}

const char *IMB_colormanagement_srgb_colorspace_name_get()
{
  /* Make a best effort to find by common names. First two are from the ColorInterop forum. */
  const char *names[] = {"sRGB Encoded Rec.709 (sRGB)",
                         "srgb_rec709_scene",
                         "Utility - sRGB - Texture",
                         "sRGB - Texture",
                         "sRGB",
                         nullptr};
  for (int i = 0; names[i]; i++) {
    const ColorSpace *colorspace = g_config->get_color_space(names[i]);
    if (colorspace) {
      return colorspace->name().c_str();
    }
  }

  /* Fallback if nothing can be found. */
  return global_role_default_byte;
}

blender::Vector<char> IMB_colormanagement_space_to_icc_profile(const ColorSpace *colorspace)
{
  /* ICC profiles shipped with Blender are named after the OpenColorIO interop ID. */
  blender::Vector<char> icc_profile;

  const StringRefNull interop_id = colorspace->interop_id();
  if (interop_id.is_empty()) {
    return icc_profile;
  }

  const std::optional<std::string> dir = BKE_appdir_folder_id(BLENDER_DATAFILES,
                                                              "colormanagement");
  if (!dir.has_value()) {
    return icc_profile;
  }

  char icc_filename[FILE_MAX];
  STRNCPY(icc_filename, (interop_id + ".icc").c_str());
  BLI_path_make_safe_filename(icc_filename);

  char icc_filepath[FILE_MAX];
  BLI_path_join(icc_filepath, sizeof(icc_filepath), dir->c_str(), "icc", icc_filename);

  blender::fstream f(icc_filepath, std::ios::binary | std::ios::in | std::ios::ate);
  if (!f.is_open()) {
    /* If we can't find a scene referred filename, try display referred. */
    StringRef icc_filepath_ref = icc_filepath;
    if (icc_filepath_ref.endswith("_scene.icc")) {
      std::string icc_filepath_display = icc_filepath_ref.drop_suffix(strlen("_scene.icc")) +
                                         "_display.icc";
      f.open(icc_filepath_display, std::ios::binary | std::ios::in | std::ios::ate);
    }

    if (!f.is_open()) {
      return icc_profile;
    }
  }

  std::streamsize size = f.tellg();
  if (size <= 0) {
    return icc_profile;
  }
  icc_profile.resize(size);

  f.seekg(0, std::ios::beg);
  if (!f.read(icc_profile.data(), icc_profile.size())) {
    icc_profile.clear();
  }

  return icc_profile;
}

/* Primaries */
static const int CICP_PRI_REC709 = 1;
static const int CICP_PRI_REC2020 = 9;
static const int CICP_PRI_P3D65 = 12;
/* Transfer functions */
static const int CICP_TRC_BT709 = 1;
static const int CICP_TRC_G22 = 4;
static const int CICP_TRC_SRGB = 13;
static const int CICP_TRC_PQ = 16;
static const int CICP_TRC_G26 = 17;
static const int CICP_TRC_HLG = 18;
/* Matrix */
static const int CICP_MATRIX_RGB = 0;
static const int CICP_MATRIX_BT709 = 1;
static const int CICP_MATRIX_REC2020_NCL = 9;
/* Range */
static const int CICP_RANGE_FULL = 1;

bool IMB_colormanagement_space_to_cicp(const ColorSpace *colorspace,
                                       const ColorManagedFileOutput output,
                                       const bool rgb_matrix,
                                       int cicp[4])
{
  const StringRefNull interop_id = colorspace->interop_id();
  if (interop_id.is_empty()) {
    return false;
  }

  /* References:
   * ASWF Color Interop Forum defined display spaces.
   * https://en.wikipedia.org/wiki/Coding-independent_code_points
   * https://www.w3.org/TR/png-3/#cICP-chunk
   */
  if (interop_id == "pq_rec2020_display") {
    cicp[0] = CICP_PRI_REC2020;
    cicp[1] = CICP_TRC_PQ;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_REC2020_NCL;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (interop_id == "hlg_rec2020_display") {
    cicp[0] = CICP_PRI_REC2020;
    cicp[1] = CICP_TRC_HLG;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_REC2020_NCL;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (interop_id == "pq_p3d65_display") {
    /* Rec.2020 matrix may seem odd, but follows Color Interop Forum recommendation. */
    cicp[0] = CICP_PRI_P3D65;
    cicp[1] = CICP_TRC_PQ;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_REC2020_NCL;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (interop_id == "g26_p3d65_display") {
    /* BT.709 matrix may seem odd, but follows Color Interop Forum recommendation. */
    cicp[0] = CICP_PRI_P3D65;
    cicp[1] = CICP_TRC_G26;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_BT709;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (interop_id == "g22_rec709_display") {
    cicp[0] = CICP_PRI_REC709;
    cicp[1] = CICP_TRC_G22;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_BT709;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (interop_id == "g24_rec2020_display") {
    /* There is no gamma 2.4 trc, but BT.709 is close. */
    cicp[0] = CICP_PRI_REC2020;
    cicp[1] = CICP_TRC_BT709;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_REC2020_NCL;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (interop_id == "g24_rec709_display") {
    /* There is no gamma 2.4 trc, but BT.709 is close. */
    cicp[0] = CICP_PRI_REC709;
    cicp[1] = CICP_TRC_BT709;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_BT709;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (ELEM(interop_id, "srgb_p3d65_display", "srgbe_p3d65_display")) {
    /* For video we use BT.709 to match default sRGB writing, even though it is wrong.
     * But we have been writing sRGB like this forever, and there is the so called
     * "Quicktime gamma shift bug" that complicates things. */
    cicp[0] = CICP_PRI_P3D65;
    cicp[1] = (output == ColorManagedFileOutput::Video) ? CICP_TRC_BT709 : CICP_TRC_SRGB;
    cicp[2] = (rgb_matrix) ? CICP_MATRIX_RGB : CICP_MATRIX_BT709;
    cicp[3] = CICP_RANGE_FULL;
    return true;
  }
  if (interop_id == "srgb_rec709_display") {
    /* Don't write anything for backwards compatibility. Is fine for PNG
     * and video but may reconsider when JXL or AVIF get added. */
    return false;
  }

  return false;
}

const ColorSpace *IMB_colormanagement_space_from_cicp(const int cicp[4],
                                                      const ColorManagedFileOutput output)
{
  StringRefNull interop_id;

  /* We don't care about matrix or range, we assume decoding handles that and we get
   * full range RGB values out. */
  if (cicp[0] == CICP_PRI_REC2020 && cicp[1] == CICP_TRC_PQ) {
    interop_id = "pq_rec2020_display";
  }
  else if (cicp[0] == CICP_PRI_REC2020 && cicp[1] == CICP_TRC_HLG) {
    interop_id = "hlg_rec2020_display";
  }
  else if (cicp[0] == CICP_PRI_P3D65 && cicp[1] == CICP_TRC_PQ) {
    interop_id = "pq_p3d65_display";
  }
  else if (cicp[0] == CICP_PRI_P3D65 && cicp[1] == CICP_TRC_G26) {
    interop_id = "g26_p3d65_display";
  }
  else if (cicp[0] == CICP_PRI_REC709 && cicp[1] == CICP_TRC_G22) {
    interop_id = "g22_rec709_display";
  }
  else if (cicp[0] == CICP_PRI_REC2020 && cicp[1] == CICP_TRC_BT709) {
    interop_id = "g24_rec2020_display";
  }
  else if (cicp[0] == CICP_PRI_REC709 && cicp[1] == CICP_TRC_BT709) {
    if (output == ColorManagedFileOutput::Video) {
      /* Arguably this should be g24_rec709_display, but we write sRGB like this.
       * So there is an exception for now. */
      interop_id = "srgb_rec709_display";
    }
    else {
      interop_id = "g24_rec709_display";
    }
  }
  else if (cicp[0] == CICP_PRI_P3D65 && ELEM(cicp[1], CICP_TRC_SRGB, CICP_TRC_BT709)) {
    interop_id = "srgb_p3d65_display";
  }
  else if (cicp[0] == CICP_PRI_REC709 && cicp[1] == CICP_TRC_SRGB) {
    interop_id = "srgb_rec709_display";
  }

  return interop_id.is_empty() ? nullptr : g_config->get_color_space_by_interop_id(interop_id);
}

StringRefNull IMB_colormanagement_space_get_interop_id(const ColorSpace *colorspace)
{
  return colorspace->interop_id();
}

const ColorSpace *IMB_colormanagement_space_from_interop_id(StringRefNull interop_id)
{
  return g_config->get_color_space_by_interop_id(interop_id);
}

blender::float3x3 IMB_colormanagement_get_xyz_to_scene_linear()
{
  return blender::float3x3(blender::colorspace::xyz_to_scene_linear);
}

blender::float3x3 IMB_colormanagement_get_scene_linear_to_xyz()
{
  return blender::float3x3(blender::colorspace::scene_linear_to_xyz);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Functions for converting between color temperature/tint and RGB white points
 * \{ */

void IMB_colormanagement_get_whitepoint(const float temperature,
                                        const float tint,
                                        float whitepoint[3])
{
  blender::float3 xyz = blender::math::whitepoint_from_temp_tint(temperature, tint);
  IMB_colormanagement_xyz_to_scene_linear(whitepoint, xyz);
}

bool IMB_colormanagement_set_whitepoint(const float whitepoint[3], float &temperature, float &tint)
{
  blender::float3 xyz;
  IMB_colormanagement_scene_linear_to_xyz(xyz, whitepoint);
  return blender::math::whitepoint_to_temp_tint(xyz, temperature, tint);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display Buffer Transform Routines
 * \{ */

struct DisplayBufferThread {
  ColormanageProcessor *cm_processor;

  const float *buffer;
  uchar *byte_buffer;

  float *display_buffer;
  uchar *display_buffer_byte;

  int width;
  int start_line;
  int tot_line;

  int channels;
  float dither;
  bool is_data;
  bool predivide;

  const char *byte_colorspace;
  const char *float_colorspace;
};

struct DisplayBufferInitData {
  ImBuf *ibuf;
  ColormanageProcessor *cm_processor;
  const float *buffer;
  uchar *byte_buffer;

  float *display_buffer;
  uchar *display_buffer_byte;

  int width;

  const char *byte_colorspace;
  const char *float_colorspace;
};

static void display_buffer_init_handle(DisplayBufferThread *handle,
                                       int start_line,
                                       int tot_line,
                                       DisplayBufferInitData *init_data)
{
  ImBuf *ibuf = init_data->ibuf;

  int channels = ibuf->channels;
  float dither = ibuf->dither;
  bool is_data = (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) != 0;

  size_t offset = size_t(channels) * start_line * ibuf->x;
  size_t display_buffer_byte_offset = size_t(DISPLAY_BUFFER_CHANNELS) * start_line * ibuf->x;

  memset(handle, 0, sizeof(DisplayBufferThread));

  handle->cm_processor = init_data->cm_processor;

  if (init_data->buffer) {
    handle->buffer = init_data->buffer + offset;
  }

  if (init_data->byte_buffer) {
    handle->byte_buffer = init_data->byte_buffer + offset;
  }

  if (init_data->display_buffer) {
    handle->display_buffer = init_data->display_buffer + offset;
  }

  if (init_data->display_buffer_byte) {
    handle->display_buffer_byte = init_data->display_buffer_byte + display_buffer_byte_offset;
  }

  handle->width = ibuf->x;

  handle->start_line = start_line;
  handle->tot_line = tot_line;

  handle->channels = channels;
  handle->dither = dither;
  handle->is_data = is_data;
  handle->predivide = IMB_alpha_affects_rgb(ibuf);

  handle->byte_colorspace = init_data->byte_colorspace;
  handle->float_colorspace = init_data->float_colorspace;
}

static void display_buffer_apply_get_linear_buffer(DisplayBufferThread *handle,
                                                   int height,
                                                   float *linear_buffer,
                                                   bool *is_straight_alpha)
{
  int channels = handle->channels;
  int width = handle->width;

  size_t buffer_size = size_t(channels) * width * height;

  bool is_data = handle->is_data;
  bool is_data_display = handle->cm_processor->is_data_result;
  bool predivide = handle->predivide;

  if (!handle->buffer) {
    uchar *byte_buffer = handle->byte_buffer;

    const char *from_colorspace = handle->byte_colorspace;
    const char *to_colorspace = global_role_scene_linear;

    float *fp;
    uchar *cp;
    const size_t i_last = size_t(width) * height;
    size_t i;

    /* first convert byte buffer to float, keep in image space */
    for (i = 0, fp = linear_buffer, cp = byte_buffer; i != i_last;
         i++, fp += channels, cp += channels)
    {
      if (channels == 3) {
        rgb_uchar_to_float(fp, cp);
      }
      else if (channels == 4) {
        rgba_uchar_to_float(fp, cp);
      }
      else {
        BLI_assert_msg(0, "Buffers of 3 or 4 channels are only supported here");
      }
    }

    if (!is_data && !is_data_display) {
      /* convert float buffer to scene linear space */
      IMB_colormanagement_transform_float(
          linear_buffer, width, height, channels, from_colorspace, to_colorspace, false);
    }

    *is_straight_alpha = true;
  }
  else if (handle->float_colorspace) {
    /* currently float is non-linear only in sequencer, which is working
     * in its own color space even to handle float buffers.
     * This color space is the same for byte and float images.
     * Need to convert float buffer to linear space before applying display transform
     */

    const char *from_colorspace = handle->float_colorspace;
    const char *to_colorspace = global_role_scene_linear;

    memcpy(linear_buffer, handle->buffer, buffer_size * sizeof(float));

    if (!is_data && !is_data_display) {
      IMB_colormanagement_transform_float(
          linear_buffer, width, height, channels, from_colorspace, to_colorspace, predivide);
    }

    *is_straight_alpha = false;
  }
  else {
    /* some processors would want to modify float original buffer
     * before converting it into display byte buffer, so we need to
     * make sure original's ImBuf buffers wouldn't be modified by
     * using duplicated buffer here
     */

    memcpy(linear_buffer, handle->buffer, buffer_size * sizeof(float));

    *is_straight_alpha = false;
  }
}

static void do_display_buffer_apply_no_processor(DisplayBufferThread *handle)
{
  const int width = handle->width;
  const int height = handle->tot_line;
  if (handle->display_buffer_byte && handle->display_buffer_byte != handle->byte_buffer) {
    if (handle->byte_buffer) {
      IMB_buffer_byte_from_byte(handle->display_buffer_byte,
                                handle->byte_buffer,
                                IB_PROFILE_SRGB,
                                IB_PROFILE_SRGB,
                                false,
                                width,
                                height,
                                width,
                                width);
    }
    else if (handle->buffer) {
      IMB_buffer_byte_from_float(handle->display_buffer_byte,
                                 handle->buffer,
                                 handle->channels,
                                 handle->dither,
                                 IB_PROFILE_SRGB,
                                 IB_PROFILE_SRGB,
                                 handle->predivide,
                                 width,
                                 height,
                                 width,
                                 width,
                                 handle->start_line);
    }
  }

  if (handle->display_buffer) {
    if (handle->byte_buffer) {
      IMB_buffer_float_from_byte(handle->display_buffer,
                                 handle->byte_buffer,
                                 IB_PROFILE_SRGB,
                                 IB_PROFILE_SRGB,
                                 false,
                                 width,
                                 height,
                                 width,
                                 width);
    }
    else if (handle->buffer && handle->display_buffer != handle->buffer) {
      IMB_buffer_float_from_float(handle->display_buffer,
                                  handle->buffer,
                                  handle->channels,
                                  IB_PROFILE_SRGB,
                                  IB_PROFILE_SRGB,
                                  handle->predivide,
                                  width,
                                  height,
                                  width,
                                  width);
    }
  }
}

static void do_display_buffer_apply_thread(DisplayBufferThread *handle)
{
  ColormanageProcessor *cm_processor = handle->cm_processor;
  if (cm_processor == nullptr) {
    do_display_buffer_apply_no_processor(handle);
    return;
  }

  float *display_buffer = handle->display_buffer;
  uchar *display_buffer_byte = handle->display_buffer_byte;
  int channels = handle->channels;
  int width = handle->width;
  int height = handle->tot_line;
  float *linear_buffer = MEM_malloc_arrayN<float>(
      size_t(channels) * size_t(width) * size_t(height), "color conversion linear buffer");

  bool is_straight_alpha;
  display_buffer_apply_get_linear_buffer(handle, height, linear_buffer, &is_straight_alpha);

  bool predivide = handle->predivide && (is_straight_alpha == false);

  /* Apply processor (note: data buffers never get color space conversions). */
  if (!handle->is_data) {
    IMB_colormanagement_processor_apply(
        cm_processor, linear_buffer, width, height, channels, predivide);
  }

  /* copy result to output buffers */
  if (display_buffer_byte) {
    /* do conversion */
    IMB_buffer_byte_from_float(display_buffer_byte,
                               linear_buffer,
                               channels,
                               handle->dither,
                               IB_PROFILE_SRGB,
                               IB_PROFILE_SRGB,
                               predivide,
                               width,
                               height,
                               width,
                               width,
                               handle->start_line);
  }

  if (display_buffer) {
    memcpy(display_buffer, linear_buffer, size_t(width) * height * channels * sizeof(float));

    if (is_straight_alpha && channels == 4) {
      const size_t i_last = size_t(width) * height;
      size_t i;
      float *fp;

      for (i = 0, fp = display_buffer; i != i_last; i++, fp += channels) {
        straight_to_premul_v4(fp);
      }
    }
  }

  MEM_freeN(linear_buffer);
}

static void display_buffer_apply_threaded(ImBuf *ibuf,
                                          const float *buffer,
                                          uchar *byte_buffer,
                                          float *display_buffer,
                                          uchar *display_buffer_byte,
                                          ColormanageProcessor *cm_processor)
{
  using namespace blender;
  DisplayBufferInitData init_data;

  init_data.ibuf = ibuf;
  init_data.cm_processor = cm_processor;
  init_data.buffer = buffer;
  init_data.byte_buffer = byte_buffer;
  init_data.display_buffer = display_buffer;
  init_data.display_buffer_byte = display_buffer_byte;

  if (ibuf->byte_buffer.colorspace != nullptr) {
    init_data.byte_colorspace = ibuf->byte_buffer.colorspace->name().c_str();
  }
  else {
    /* happens for viewer images, which are not so simple to determine where to
     * set image buffer's color spaces
     */
    init_data.byte_colorspace = global_role_default_byte;
  }

  if (ibuf->float_buffer.colorspace != nullptr) {
    /* sequencer stores float buffers in non-linear space */
    init_data.float_colorspace = ibuf->float_buffer.colorspace->name().c_str();
  }
  else {
    init_data.float_colorspace = nullptr;
  }

  threading::parallel_for(IndexRange(ibuf->y), 64, [&](const IndexRange y_range) {
    DisplayBufferThread handle;
    display_buffer_init_handle(&handle, y_range.first(), y_range.size(), &init_data);
    do_display_buffer_apply_thread(&handle);
  });
}

/* Checks if given colorspace can be used for display as-is:
 * - View settings do not have extra curves, exposure, gamma or look applied, and
 * - Display colorspace is the same as given colorspace. */
static bool is_colorspace_same_as_display(const ColorSpace *colorspace,
                                          const ColorManagedViewSettings *view_settings,
                                          const ColorManagedDisplaySettings *display_settings)
{
  if ((view_settings->flag & COLORMANAGE_VIEW_USE_CURVES) != 0 ||
      (view_settings->flag & COLORMANAGE_VIEW_USE_WHITE_BALANCE) != 0 ||
      view_settings->exposure != 0.0f || view_settings->gamma != 1.0f)
  {
    return false;
  }

  const ocio::Look *look = g_config->get_look_by_name(view_settings->look);
  if (look != nullptr && !look->process_space().is_empty()) {
    return false;
  }

  const ColorSpace *display_colorspace = IMB_colormangement_display_get_color_space(
      view_settings, display_settings);
  if (display_colorspace != colorspace) {
    return false;
  }

  const ocio::Display *display = g_config->get_display_by_name(display_settings->display_device);
  const ocio::View *untonemapped_view = (display) ? display->get_untonemapped_view() : nullptr;
  return untonemapped_view && untonemapped_view->name() == view_settings->view_transform;
}

bool IMB_colormanagement_display_processor_needed(
    const ImBuf *ibuf,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings)
{
  if (ibuf->float_buffer.data == nullptr && ibuf->byte_buffer.colorspace) {
    return !is_colorspace_same_as_display(
        ibuf->byte_buffer.colorspace, view_settings, display_settings);
  }
  if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.colorspace) {
    return !is_colorspace_same_as_display(
        ibuf->float_buffer.colorspace, view_settings, display_settings);
  }
  return true;
}

ColormanageProcessor *IMB_colormanagement_display_processor_for_imbuf(
    const ImBuf *ibuf,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space)
{
  if (!IMB_colormanagement_display_processor_needed(ibuf, view_settings, display_settings)) {
    return nullptr;
  }
  return IMB_colormanagement_display_processor_new(view_settings, display_settings, display_space);
}

static void colormanage_display_buffer_process_ex(
    ImBuf *ibuf,
    float *display_buffer,
    uchar *display_buffer_byte,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space)
{
  ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_for_imbuf(
      ibuf, view_settings, display_settings, display_space);
  display_buffer_apply_threaded(ibuf,
                                ibuf->float_buffer.data,
                                ibuf->byte_buffer.data,
                                display_buffer,
                                display_buffer_byte,
                                cm_processor);

  if (cm_processor) {
    IMB_colormanagement_processor_free(cm_processor);
  }
}

static void colormanage_display_buffer_process(ImBuf *ibuf,
                                               uchar *display_buffer,
                                               const ColorManagedViewSettings *view_settings,
                                               const ColorManagedDisplaySettings *display_settings,
                                               const ColorManagedDisplaySpace display_space)
{
  colormanage_display_buffer_process_ex(
      ibuf, nullptr, display_buffer, view_settings, display_settings, display_space);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Threaded Processor Transform Routines
 * \{ */

struct ProcessorTransformThread {
  ColormanageProcessor *cm_processor;
  uchar *byte_buffer;
  float *float_buffer;
  int width;
  int start_line;
  int tot_line;
  int channels;
  bool predivide;
  bool float_from_byte;
};

struct ProcessorTransformInitData {
  ColormanageProcessor *cm_processor;
  uchar *byte_buffer;
  float *float_buffer;
  int width;
  int height;
  int channels;
  bool predivide;
  bool float_from_byte;
};

static void processor_transform_init_handle(ProcessorTransformThread *handle,
                                            int start_line,
                                            int tot_line,
                                            ProcessorTransformInitData *init_data)
{
  const int channels = init_data->channels;
  const int width = init_data->width;
  const bool predivide = init_data->predivide;
  const bool float_from_byte = init_data->float_from_byte;

  const size_t offset = size_t(channels) * start_line * width;

  memset(handle, 0, sizeof(ProcessorTransformThread));

  handle->cm_processor = init_data->cm_processor;

  if (init_data->byte_buffer != nullptr) {
    /* TODO(serge): Offset might be different for byte and float buffers. */
    handle->byte_buffer = init_data->byte_buffer + offset;
  }
  if (init_data->float_buffer != nullptr) {
    handle->float_buffer = init_data->float_buffer + offset;
  }

  handle->width = width;

  handle->start_line = start_line;
  handle->tot_line = tot_line;

  handle->channels = channels;
  handle->predivide = predivide;
  handle->float_from_byte = float_from_byte;
}

static void do_processor_transform_thread(ProcessorTransformThread *handle)
{
  uchar *byte_buffer = handle->byte_buffer;
  float *float_buffer = handle->float_buffer;
  const int channels = handle->channels;
  const int width = handle->width;
  const int height = handle->tot_line;
  const bool predivide = handle->predivide;
  const bool float_from_byte = handle->float_from_byte;

  if (float_from_byte) {
    IMB_buffer_float_from_byte(float_buffer,
                               byte_buffer,
                               IB_PROFILE_SRGB,
                               IB_PROFILE_SRGB,
                               false,
                               width,
                               height,
                               width,
                               width);
    IMB_colormanagement_processor_apply(
        handle->cm_processor, float_buffer, width, height, channels, predivide);
    IMB_premultiply_rect_float(float_buffer, 4, width, height);
  }
  else {
    if (byte_buffer != nullptr) {
      IMB_colormanagement_processor_apply_byte(
          handle->cm_processor, byte_buffer, width, height, channels);
    }
    if (float_buffer != nullptr) {
      IMB_colormanagement_processor_apply(
          handle->cm_processor, float_buffer, width, height, channels, predivide);
    }
  }
}

static void processor_transform_apply_threaded(uchar *byte_buffer,
                                               float *float_buffer,
                                               const int width,
                                               const int height,
                                               const int channels,
                                               ColormanageProcessor *cm_processor,
                                               const bool predivide,
                                               const bool float_from_byte)
{
  using namespace blender;
  ProcessorTransformInitData init_data;

  init_data.cm_processor = cm_processor;
  init_data.byte_buffer = byte_buffer;
  init_data.float_buffer = float_buffer;
  init_data.width = width;
  init_data.height = height;
  init_data.channels = channels;
  init_data.predivide = predivide;
  init_data.float_from_byte = float_from_byte;

  threading::parallel_for(IndexRange(height), 64, [&](const IndexRange y_range) {
    ProcessorTransformThread handle;
    processor_transform_init_handle(&handle, y_range.first(), y_range.size(), &init_data);
    do_processor_transform_thread(&handle);
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Space Transformation Functions
 * \{ */

/* Convert the whole buffer from specified by name color space to another -
 * internal implementation. */
static void colormanagement_transform_ex(uchar *byte_buffer,
                                         float *float_buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         const char *from_colorspace,
                                         const char *to_colorspace,
                                         bool predivide)
{
  if (from_colorspace[0] == '\0') {
    return;
  }

  if (STREQ(from_colorspace, to_colorspace)) {
    /* if source and destination color spaces are identical, do nothing. */
    return;
  }

  ColormanageProcessor *cm_processor = IMB_colormanagement_colorspace_processor_new(
      from_colorspace, to_colorspace);
  if (IMB_colormanagement_processor_is_noop(cm_processor)) {
    IMB_colormanagement_processor_free(cm_processor);
    return;
  }

  processor_transform_apply_threaded(
      byte_buffer, float_buffer, width, height, channels, cm_processor, predivide, false);
  IMB_colormanagement_processor_free(cm_processor);
}

void IMB_colormanagement_transform_float(float *buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         const char *from_colorspace,
                                         const char *to_colorspace,
                                         bool predivide)
{
  colormanagement_transform_ex(
      nullptr, buffer, width, height, channels, from_colorspace, to_colorspace, predivide);
}

void IMB_colormanagement_transform_byte(uchar *buffer,
                                        int width,
                                        int height,
                                        int channels,
                                        const char *from_colorspace,
                                        const char *to_colorspace)
{
  colormanagement_transform_ex(
      buffer, nullptr, width, height, channels, from_colorspace, to_colorspace, false);
}

void IMB_colormanagement_transform_byte_to_float(float *float_buffer,
                                                 uchar *byte_buffer,
                                                 int width,
                                                 int height,
                                                 int channels,
                                                 const char *from_colorspace,
                                                 const char *to_colorspace)
{
  using namespace blender;
  ColormanageProcessor *cm_processor;
  if (from_colorspace == nullptr || from_colorspace[0] == '\0') {
    return;
  }
  if (STREQ(from_colorspace, to_colorspace) && channels == 4) {
    /* Color spaces are the same, just do a simple byte->float conversion. */
    int64_t pixel_count = int64_t(width) * height;
    threading::parallel_for(IndexRange(pixel_count), 256 * 1024, [&](IndexRange pix_range) {
      float *dst_ptr = float_buffer + pix_range.first() * channels;
      uchar *src_ptr = byte_buffer + pix_range.first() * channels;
      for ([[maybe_unused]] const int i : pix_range) {
        /* Equivalent of rgba_uchar_to_float + premultiply. */
        float cr = float(src_ptr[0]) * (1.0f / 255.0f);
        float cg = float(src_ptr[1]) * (1.0f / 255.0f);
        float cb = float(src_ptr[2]) * (1.0f / 255.0f);
        float ca = float(src_ptr[3]) * (1.0f / 255.0f);
        dst_ptr[0] = cr * ca;
        dst_ptr[1] = cg * ca;
        dst_ptr[2] = cb * ca;
        dst_ptr[3] = ca;
        src_ptr += 4;
        dst_ptr += 4;
      }
    });
    return;
  }
  cm_processor = IMB_colormanagement_colorspace_processor_new(from_colorspace, to_colorspace);
  processor_transform_apply_threaded(
      byte_buffer, float_buffer, width, height, channels, cm_processor, false, true);
  IMB_colormanagement_processor_free(cm_processor);
}

void IMB_colormanagement_transform_v4(float pixel[4],
                                      const char *from_colorspace,
                                      const char *to_colorspace)
{
  ColormanageProcessor *cm_processor;

  if (from_colorspace[0] == '\0') {
    return;
  }

  if (STREQ(from_colorspace, to_colorspace)) {
    /* if source and destination color spaces are identical, skip
     * threading overhead and simply do nothing
     */
    return;
  }

  cm_processor = IMB_colormanagement_colorspace_processor_new(from_colorspace, to_colorspace);

  IMB_colormanagement_processor_apply_v4(cm_processor, pixel);

  IMB_colormanagement_processor_free(cm_processor);
}

void IMB_colormanagement_colorspace_to_scene_linear_v3(float pixel[3],
                                                       const ColorSpace *colorspace)
{
  if (colorspace == nullptr) { /* should never happen */
    printf("%s: perform conversion from unknown color space\n", __func__);
    return;
  }
  const ocio::CPUProcessor *processor = colorspace->get_to_scene_linear_cpu_processor();
  if (processor == nullptr) {
    return;
  }
  processor->apply_rgb(pixel);
}

void IMB_colormanagement_scene_linear_to_colorspace_v3(float pixel[3],
                                                       const ColorSpace *colorspace)
{
  if (colorspace == nullptr) { /* should never happen */
    printf("%s: perform conversion from unknown color space\n", __func__);
    return;
  }
  const ocio::CPUProcessor *processor = colorspace->get_from_scene_linear_cpu_processor();
  if (processor == nullptr) {
    return;
  }
  processor->apply_rgb(pixel);
}

void IMB_colormanagement_colorspace_to_scene_linear_v4(float pixel[4],
                                                       const bool predivide,
                                                       const ColorSpace *colorspace)
{
  if (colorspace == nullptr) { /* should never happen */
    printf("%s: perform conversion from unknown color space\n", __func__);
    return;
  }
  const ocio::CPUProcessor *processor = colorspace->get_to_scene_linear_cpu_processor();
  if (processor == nullptr) {
    return;
  }
  if (predivide) {
    processor->apply_rgba_predivide(pixel);
  }
  else {
    processor->apply_rgba(pixel);
  }
}

void IMB_colormanagement_colorspace_to_scene_linear(float *buffer,
                                                    const int width,
                                                    const int height,
                                                    const int channels,
                                                    const ColorSpace *colorspace,
                                                    const bool predivide)
{
  if (colorspace == nullptr) { /* should never happen */
    printf("%s: perform conversion from unknown color space\n", __func__);
    return;
  }
  const ocio::CPUProcessor *processor = colorspace->get_to_scene_linear_cpu_processor();
  if (processor == nullptr) {
    return;
  }
  ocio::PackedImage img(buffer,
                        width,
                        height,
                        channels,
                        ocio::BitDepth::BIT_DEPTH_F32,
                        sizeof(float),
                        size_t(channels) * sizeof(float),
                        size_t(channels) * sizeof(float) * width);

  if (predivide) {
    processor->apply_predivide(img);
  }
  else {
    processor->apply(img);
  }
}

void IMB_colormanagement_scene_linear_to_colorspace(float *buffer,
                                                    const int width,
                                                    const int height,
                                                    const int channels,
                                                    const ColorSpace *colorspace)
{
  if (colorspace == nullptr) { /* should never happen */
    printf("%s: perform conversion from unknown color space\n", __func__);
    return;
  }
  const ocio::CPUProcessor *processor = colorspace->get_from_scene_linear_cpu_processor();
  if (processor == nullptr) {
    return;
  }
  const ocio::PackedImage img(buffer,
                              width,
                              height,
                              channels,
                              ocio::BitDepth::BIT_DEPTH_F32,
                              sizeof(float),
                              size_t(channels) * sizeof(float),
                              size_t(channels) * sizeof(float) * width);
  processor->apply(img);
}

void IMB_colormanagement_imbuf_to_byte_texture(uchar *out_buffer,
                                               const int offset_x,
                                               const int offset_y,
                                               const int width,
                                               const int height,
                                               const ImBuf *ibuf,
                                               const bool store_premultiplied)
{
  /* Byte buffer storage, only for sRGB, scene linear and data texture since other
   * color space conversions can't be done on the GPU. */
  BLI_assert(ibuf->byte_buffer.data);
  BLI_assert(ibuf->float_buffer.data == nullptr);
  BLI_assert(IMB_colormanagement_space_is_srgb(ibuf->byte_buffer.colorspace) ||
             IMB_colormanagement_space_is_scene_linear(ibuf->byte_buffer.colorspace) ||
             IMB_colormanagement_space_is_data(ibuf->byte_buffer.colorspace));

  const uchar *in_buffer = ibuf->byte_buffer.data;
  const bool use_premultiply = IMB_alpha_affects_rgb(ibuf) && store_premultiplied;

  for (int y = 0; y < height; y++) {
    const size_t in_offset = (offset_y + y) * ibuf->x + offset_x;
    const size_t out_offset = y * width;
    const uchar *in = in_buffer + in_offset * 4;
    uchar *out = out_buffer + out_offset * 4;

    if (use_premultiply) {
      /* Premultiply only. */
      for (int x = 0; x < width; x++, in += 4, out += 4) {
        out[0] = (in[0] * in[3]) >> 8;
        out[1] = (in[1] * in[3]) >> 8;
        out[2] = (in[2] * in[3]) >> 8;
        out[3] = in[3];
      }
    }
    else {
      /* Copy only. */
      for (int x = 0; x < width; x++, in += 4, out += 4) {
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        out[3] = in[3];
      }
    }
  }
}

void IMB_colormanagement_imbuf_to_float_texture(float *out_buffer,
                                                const int offset_x,
                                                const int offset_y,
                                                const int width,
                                                const int height,
                                                const ImBuf *ibuf,
                                                const bool store_premultiplied)
{
  using namespace blender;

  /* Float texture are stored in scene linear color space, with premultiplied
   * alpha depending on the image alpha mode. */
  if (ibuf->float_buffer.data) {
    /* Float source buffer. */
    const float *in_buffer = ibuf->float_buffer.data;
    const int in_channels = ibuf->channels;
    const bool use_unpremultiply = IMB_alpha_affects_rgb(ibuf) && !store_premultiplied;

    for (int y = 0; y < height; y++) {
      const size_t in_offset = (offset_y + y) * ibuf->x + offset_x;
      const size_t out_offset = y * width;
      const float *in = in_buffer + in_offset * in_channels;
      float *out = out_buffer + out_offset * 4;

      if (in_channels == 1) {
        /* Copy single channel. */
        for (int x = 0; x < width; x++, in += 1, out += 4) {
          out[0] = in[0];
          out[1] = in[0];
          out[2] = in[0];
          out[3] = in[0];
        }
      }
      else if (in_channels == 3) {
        /* Copy RGB. */
        for (int x = 0; x < width; x++, in += 3, out += 4) {
          out[0] = in[0];
          out[1] = in[1];
          out[2] = in[2];
          out[3] = 1.0f;
        }
      }
      else if (in_channels == 4) {
        /* Copy or convert RGBA. */
        if (use_unpremultiply) {
          for (int x = 0; x < width; x++, in += 4, out += 4) {
            premul_to_straight_v4_v4(out, in);
          }
        }
        else {
          memcpy(out, in, sizeof(float[4]) * width);
        }
      }
    }
  }
  else {
    /* Byte source buffer. */
    const uchar *in_buffer = ibuf->byte_buffer.data;
    const bool use_premultiply = IMB_alpha_affects_rgb(ibuf) && store_premultiplied;

    const ocio::CPUProcessor *processor =
        (ibuf->byte_buffer.colorspace) ?
            ibuf->byte_buffer.colorspace->get_to_scene_linear_cpu_processor() :
            nullptr;

    threading::parallel_for(IndexRange(height), 128, [&](const IndexRange y_range) {
      for (const int y : y_range) {
        const size_t in_offset = (offset_y + y) * ibuf->x + offset_x;
        const size_t out_offset = y * width;
        const uchar *in = in_buffer + in_offset * 4;
        float *out = out_buffer + out_offset * 4;
        for (int x = 0; x < width; x++, in += 4, out += 4) {
          /* Convert to scene linear and premultiply. */
          float pixel[4];
          rgba_uchar_to_float(pixel, in);
          if (processor) {
            processor->apply_rgb(pixel);
          }
          else {
            srgb_to_linearrgb_v3_v3(pixel, pixel);
          }
          if (use_premultiply) {
            mul_v3_fl(pixel, pixel[3]);
          }
          copy_v4_v4(out, pixel);
        }
      }
    });
  }
}

void IMB_colormanagement_scene_linear_to_color_picking_v3(float color_picking[3],
                                                          const float scene_linear[3])
{
  if (!global_color_picking_state.cpu_processor_to && !global_color_picking_state.failed) {
    /* Create processor if none exists. */
    BLI_mutex_lock(&processor_lock);

    if (!global_color_picking_state.cpu_processor_to && !global_color_picking_state.failed) {
      std::shared_ptr<const ocio::CPUProcessor> cpu_processor = g_config->get_cpu_processor(
          global_role_scene_linear, global_role_color_picking);

      if (cpu_processor) {
        global_color_picking_state.cpu_processor_to = cpu_processor;
      }
      else {
        global_color_picking_state.failed = true;
      }
    }

    BLI_mutex_unlock(&processor_lock);
  }

  copy_v3_v3(color_picking, scene_linear);

  if (global_color_picking_state.cpu_processor_to) {
    global_color_picking_state.cpu_processor_to->apply_rgb(color_picking);
  }
}

void IMB_colormanagement_color_picking_to_scene_linear_v3(float scene_linear[3],
                                                          const float color_picking[3])
{
  if (!global_color_picking_state.cpu_processor_from && !global_color_picking_state.failed) {
    /* Create processor if none exists. */
    BLI_mutex_lock(&processor_lock);

    if (!global_color_picking_state.cpu_processor_from && !global_color_picking_state.failed) {
      std::shared_ptr<const ocio::CPUProcessor> cpu_processor = g_config->get_cpu_processor(
          global_role_color_picking, global_role_scene_linear);

      if (cpu_processor) {
        global_color_picking_state.cpu_processor_from = cpu_processor;
      }
      else {
        global_color_picking_state.failed = true;
      }
    }

    BLI_mutex_unlock(&processor_lock);
  }

  copy_v3_v3(scene_linear, color_picking);

  if (global_color_picking_state.cpu_processor_from) {
    global_color_picking_state.cpu_processor_from->apply_rgb(scene_linear);
  }
}

void IMB_colormanagement_scene_linear_to_display_v3(float pixel[3],
                                                    const ColorManagedDisplay *display,
                                                    const ColorManagedDisplaySpace display_space)
{
  const ocio::CPUProcessor *processor = display->get_from_scene_linear_cpu_processor(
      display_space == DISPLAY_SPACE_DRAW);
  if (processor != nullptr) {
    processor->apply_rgb(pixel);
  }
}

void IMB_colormanagement_display_to_scene_linear_v3(float pixel[3],
                                                    const ColorManagedDisplay *display,
                                                    const ColorManagedDisplaySpace display_space)
{
  const ocio::CPUProcessor *processor = display->get_to_scene_linear_cpu_processor(
      display_space == DISPLAY_SPACE_DRAW);
  if (processor != nullptr) {
    processor->apply_rgb(pixel);
  }
}

void IMB_colormanagement_pixel_to_display_space_v4(
    float result[4],
    const float pixel[4],
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space)
{
  ColormanageProcessor *cm_processor;

  copy_v4_v4(result, pixel);

  cm_processor = IMB_colormanagement_display_processor_new(
      view_settings, display_settings, display_space);
  IMB_colormanagement_processor_apply_v4(cm_processor, result);
  IMB_colormanagement_processor_free(cm_processor);
}

static void colormanagement_imbuf_make_display_space(
    ImBuf *ibuf,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space,
    bool make_byte)
{
  if (!ibuf->byte_buffer.data && make_byte) {
    IMB_alloc_byte_pixels(ibuf);
  }

  colormanage_display_buffer_process_ex(ibuf,
                                        ibuf->float_buffer.data,
                                        ibuf->byte_buffer.data,
                                        view_settings,
                                        display_settings,
                                        display_space);
}

void IMB_colormanagement_imbuf_make_display_space(
    ImBuf *ibuf,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space)
{
  colormanagement_imbuf_make_display_space(
      ibuf, view_settings, display_settings, display_space, false);
}

static ImBuf *imbuf_ensure_editable(ImBuf *ibuf, ImBuf *colormanaged_ibuf, bool allocate_result)
{
  if (colormanaged_ibuf != ibuf) {
    /* Is already an editable copy. */
    return colormanaged_ibuf;
  }

  if (allocate_result) {
    /* Copy full image buffer. */
    colormanaged_ibuf = IMB_dupImBuf(ibuf);
    IMB_metadata_copy(colormanaged_ibuf, ibuf);
    return colormanaged_ibuf;
  }

  /* Render pipeline is constructing image buffer itself,
   * but it's re-using byte and float buffers from render result make copy of this buffers
   * here sine this buffers would be transformed to other color space here. */
  IMB_make_writable_byte_buffer(ibuf);
  IMB_make_writable_float_buffer(ibuf);

  return ibuf;
}

ImBuf *IMB_colormanagement_imbuf_for_write(ImBuf *ibuf,
                                           bool save_as_render,
                                           bool allocate_result,
                                           const ImageFormatData *image_format)
{
  ImBuf *colormanaged_ibuf = ibuf;

  /* Update byte buffer if exists but invalid. */
  if (ibuf->float_buffer.data && ibuf->byte_buffer.data &&
      (ibuf->userflags & (IB_DISPLAY_BUFFER_INVALID | IB_RECT_INVALID)) != 0)
  {
    IMB_byte_from_float(ibuf);
    ibuf->userflags &= ~(IB_RECT_INVALID | IB_DISPLAY_BUFFER_INVALID);
  }

  /* Detect if we are writing to a file format that needs a linear float buffer. */
  const bool linear_float_output = BKE_imtype_requires_linear_float(image_format->imtype);

  /* Detect if we are writing output a byte buffer, which we would need to create
   * with color management conversions applied. This may be for either applying the
   * display transform for renders, or a user specified color space for the file. */
  const bool byte_output = BKE_image_format_is_byte(image_format);

  /* If we're saving from RGBA to RGB buffer then it's not so much useful to just ignore alpha --
   * it leads to bad artifacts especially when saving byte images.
   *
   * What we do here is we're overlaying our image on top of background color (which is currently
   * black). This is quite much the same as what Gimp does and it seems to be what artists
   * expects from saving.
   *
   * Do a conversion here, so image format writers could happily assume all the alpha tricks were
   * made already. helps keep things locally here, not spreading it to all possible image writers
   * we've got.
   */
  if (image_format->planes != R_IMF_PLANES_RGBA) {
    float color[3] = {0, 0, 0};

    colormanaged_ibuf = imbuf_ensure_editable(ibuf, colormanaged_ibuf, allocate_result);

    if (colormanaged_ibuf->float_buffer.data && colormanaged_ibuf->channels == 4) {
      IMB_alpha_under_color_float(
          colormanaged_ibuf->float_buffer.data, colormanaged_ibuf->x, colormanaged_ibuf->y, color);
    }

    if (colormanaged_ibuf->byte_buffer.data) {
      IMB_alpha_under_color_byte(
          colormanaged_ibuf->byte_buffer.data, colormanaged_ibuf->x, colormanaged_ibuf->y, color);
    }
  }

  if (save_as_render && !linear_float_output) {
    /* Render output: perform conversion to display space using view transform. */
    colormanaged_ibuf = imbuf_ensure_editable(ibuf, colormanaged_ibuf, allocate_result);

    colormanagement_imbuf_make_display_space(colormanaged_ibuf,
                                             &image_format->view_settings,
                                             &image_format->display_settings,
                                             image_format->media_type == MEDIA_TYPE_VIDEO ?
                                                 DISPLAY_SPACE_VIDEO_OUTPUT :
                                                 DISPLAY_SPACE_IMAGE_OUTPUT,
                                             byte_output);

    if (colormanaged_ibuf->float_buffer.data) {
      /* Float buffer isn't linear anymore.
       * - Image format write callback checks for this flag and assumes no space
       *   conversion should happen if ibuf->float_buffer.colorspace != nullptr. */
      colormanaged_ibuf->float_buffer.colorspace = IMB_colormangement_display_get_color_space(
          &image_format->view_settings, &image_format->display_settings);
      if (byte_output) {
        colormanaged_ibuf->byte_buffer.colorspace = colormanaged_ibuf->float_buffer.colorspace;
      }
    }
  }
  else {
    /* Linear render or regular file output: conversion between two color spaces. */

    /* Detect which color space we need to convert between. */
    const char *from_colorspace = (ibuf->float_buffer.data &&
                                   !(byte_output && ibuf->byte_buffer.data)) ?
                                      /* From float buffer. */
                                      (ibuf->float_buffer.colorspace) ?
                                      ibuf->float_buffer.colorspace->name().c_str() :
                                      global_role_scene_linear :
                                      /* From byte buffer. */
                                      (ibuf->byte_buffer.colorspace) ?
                                      ibuf->byte_buffer.colorspace->name().c_str() :
                                      global_role_default_byte;

    const char *to_colorspace = image_format->linear_colorspace_settings.name;

    /* to_colorspace may need to modified to compensate for 100 vs 203 nits conventions. */
    if (image_format->media_type != MEDIA_TYPE_VIDEO) {
      const ColorSpace *image_colorspace = g_config->get_color_space_for_hdr_image(to_colorspace);
      if (image_colorspace) {
        to_colorspace = image_colorspace->name().c_str();
      }
    }

    /* TODO: can we check with OCIO if color spaces are the same but have different names? */
    if (to_colorspace[0] == '\0' || STREQ(from_colorspace, to_colorspace)) {
      /* No conversion needed, but may still need to allocate byte buffer for output. */
      if (byte_output && !ibuf->byte_buffer.data) {
        ibuf->byte_buffer.colorspace = ibuf->float_buffer.colorspace;
        IMB_byte_from_float(ibuf);
      }
    }
    else {
      /* Color space conversion needed. */
      colormanaged_ibuf = imbuf_ensure_editable(ibuf, colormanaged_ibuf, allocate_result);

      if (byte_output) {
        colormanaged_ibuf->byte_buffer.colorspace = colormanage_colorspace_get_named(
            to_colorspace);

        if (colormanaged_ibuf->byte_buffer.data) {
          /* Byte to byte. */
          IMB_colormanagement_transform_byte(colormanaged_ibuf->byte_buffer.data,
                                             colormanaged_ibuf->x,
                                             colormanaged_ibuf->y,
                                             colormanaged_ibuf->channels,
                                             from_colorspace,
                                             to_colorspace);
        }
        else {
          /* Float to byte. */
          IMB_byte_from_float(colormanaged_ibuf);
        }
      }
      else {
        if (!colormanaged_ibuf->float_buffer.data) {
          /* Byte to float. */
          IMB_float_from_byte(colormanaged_ibuf);
          IMB_free_byte_pixels(colormanaged_ibuf);

          /* This conversion always goes to scene linear. */
          from_colorspace = global_role_scene_linear;
        }

        if (colormanaged_ibuf->float_buffer.data) {
          /* Float to float. */
          IMB_colormanagement_transform_float(colormanaged_ibuf->float_buffer.data,
                                              colormanaged_ibuf->x,
                                              colormanaged_ibuf->y,
                                              colormanaged_ibuf->channels,
                                              from_colorspace,
                                              to_colorspace,
                                              false);

          colormanaged_ibuf->float_buffer.colorspace = colormanage_colorspace_get_named(
              to_colorspace);
        }
      }
    }
  }

  return colormanaged_ibuf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Display Buffers Interfaces
 * \{ */

uchar *IMB_display_buffer_acquire(ImBuf *ibuf,
                                  const ColorManagedViewSettings *view_settings,
                                  const ColorManagedDisplaySettings *display_settings,
                                  void **cache_handle)
{
  uchar *display_buffer;
  ColormanageCacheViewSettings cache_view_settings;
  ColormanageCacheDisplaySettings cache_display_settings;
  ColorManagedViewSettings untonemapped_view_settings;
  const ColorManagedViewSettings *applied_view_settings;

  *cache_handle = nullptr;

  if (!ibuf->x || !ibuf->y) {
    return nullptr;
  }

  if (view_settings) {
    applied_view_settings = view_settings;
  }
  else {
    /* If no view settings were specified, use default ones, which will
     * attempt not to do any extra color correction. */
    IMB_colormanagement_init_untonemapped_view_settings(&untonemapped_view_settings,
                                                        display_settings);
    applied_view_settings = &untonemapped_view_settings;
  }

  /* No float buffer and byte buffer is already in display space, let's just use it. */
  if (ibuf->float_buffer.data == nullptr && ibuf->byte_buffer.colorspace && ibuf->channels == 4) {
    if (is_colorspace_same_as_display(
            ibuf->byte_buffer.colorspace, applied_view_settings, display_settings))
    {
      return ibuf->byte_buffer.data;
    }
  }

  colormanage_view_settings_to_cache(ibuf, &cache_view_settings, applied_view_settings);
  colormanage_display_settings_to_cache(&cache_display_settings, display_settings);

  if (ibuf->invalid_rect.xmin != ibuf->invalid_rect.xmax) {
    if ((ibuf->userflags & IB_DISPLAY_BUFFER_INVALID) == 0) {
      IMB_partial_display_buffer_update_threaded(ibuf,
                                                 ibuf->float_buffer.data,
                                                 ibuf->byte_buffer.data,
                                                 ibuf->x,
                                                 0,
                                                 0,
                                                 applied_view_settings,
                                                 display_settings,
                                                 ibuf->invalid_rect.xmin,
                                                 ibuf->invalid_rect.ymin,
                                                 ibuf->invalid_rect.xmax,
                                                 ibuf->invalid_rect.ymax);
    }

    BLI_rcti_init(&ibuf->invalid_rect, 0, 0, 0, 0);
  }

  BLI_thread_lock(LOCK_COLORMANAGE);

  /* ensure color management bit fields exists */
  if (!ibuf->display_buffer_flags) {
    ibuf->display_buffer_flags = MEM_calloc_arrayN<uint>(g_config->get_num_displays(),
                                                         "imbuf display_buffer_flags");
  }
  else if (ibuf->userflags & IB_DISPLAY_BUFFER_INVALID) {
    /* all display buffers were marked as invalid from other areas,
     * now propagate this flag to internal color management routines
     */
    memset(ibuf->display_buffer_flags, 0, g_config->get_num_displays() * sizeof(uint));

    ibuf->userflags &= ~IB_DISPLAY_BUFFER_INVALID;
  }

  display_buffer = colormanage_cache_get(
      ibuf, &cache_view_settings, &cache_display_settings, cache_handle);

  if (display_buffer) {
    BLI_thread_unlock(LOCK_COLORMANAGE);
    return display_buffer;
  }

  display_buffer = MEM_malloc_arrayN<uchar>(
      DISPLAY_BUFFER_CHANNELS * size_t(ibuf->x) * size_t(ibuf->y), "imbuf display buffer");

  colormanage_display_buffer_process(
      ibuf, display_buffer, applied_view_settings, display_settings, DISPLAY_SPACE_DRAW);

  colormanage_cache_put(
      ibuf, &cache_view_settings, &cache_display_settings, display_buffer, cache_handle);

  BLI_thread_unlock(LOCK_COLORMANAGE);

  return display_buffer;
}

uchar *IMB_display_buffer_acquire_ctx(const bContext *C, ImBuf *ibuf, void **cache_handle)
{
  ColorManagedViewSettings *view_settings;
  ColorManagedDisplaySettings *display_settings;

  IMB_colormanagement_display_settings_from_ctx(C, &view_settings, &display_settings);

  return IMB_display_buffer_acquire(ibuf, view_settings, display_settings, cache_handle);
}

void IMB_display_buffer_transform_apply(uchar *display_buffer,
                                        float *linear_buffer,
                                        int width,
                                        int height,
                                        int channels,
                                        const ColorManagedViewSettings *view_settings,
                                        const ColorManagedDisplaySettings *display_settings,
                                        bool predivide)
{
  float *buffer;
  ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_new(view_settings,
                                                                                 display_settings);

  buffer = MEM_malloc_arrayN<float>(size_t(channels) * size_t(width) * size_t(height),
                                    "display transform temp buffer");
  memcpy(buffer, linear_buffer, size_t(channels) * width * height * sizeof(float));

  IMB_colormanagement_processor_apply(cm_processor, buffer, width, height, channels, predivide);

  IMB_colormanagement_processor_free(cm_processor);

  IMB_buffer_byte_from_float(display_buffer,
                             buffer,
                             channels,
                             0.0f,
                             IB_PROFILE_SRGB,
                             IB_PROFILE_SRGB,
                             false,
                             width,
                             height,
                             width,
                             width);

  MEM_freeN(buffer);
}

void IMB_display_buffer_release(void *cache_handle)
{
  if (cache_handle) {
    BLI_thread_lock(LOCK_COLORMANAGE);

    colormanage_cache_handle_release(cache_handle);

    BLI_thread_unlock(LOCK_COLORMANAGE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display Functions
 * \{ */

int IMB_colormanagement_display_get_named_index(const char *name)
{
  const ocio::Display *display = g_config->get_display_by_name(name);
  if (display) {
    return display->index;
  }
  return -1;
}

const char *IMB_colormanagement_display_get_indexed_name(const int index)
{
  const ocio::Display *display = g_config->get_display_by_index(index);
  if (!display) {
    return "";
  }
  return display->name().c_str();
}

const char *IMB_colormanagement_display_get_default_name()
{
  const ocio::Display *display = g_config->get_default_display();
  return display->name().c_str();
}

const ColorManagedDisplay *IMB_colormanagement_display_get_named(const char *name)
{
  return g_config->get_display_by_name(name);
}

const char *IMB_colormanagement_display_get_none_name()
{
  if (g_config->get_display_by_name("None") != nullptr) {
    return "None";
  }
  return IMB_colormanagement_display_get_default_name();
}

const char *IMB_colormanagement_display_get_default_view_transform_name(
    const ColorManagedDisplay *display)
{
  const ocio::View *default_view = display->get_default_view();
  if (!default_view) {
    return "";
  }
  return default_view->name().c_str();
}

const ColorSpace *IMB_colormangement_display_get_color_space(
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings)
{
  /* Get the colorspace that the image is in after applying this view and display
   * transform. If we are going to a display referred colorspace we can use that. */
  const ocio::Display *display = g_config->get_display_by_name(display_settings->display_device);
  const ocio::View *view = (display) ? display->get_view_by_name(view_settings->view_transform) :
                                       nullptr;
  const ColorSpace *colorspace = (view) ? view->display_colorspace() : nullptr;
  if (colorspace && colorspace->is_display_referred()) {
    return colorspace;
  }
  /* If not available, try to guess what the untonemapped view is and use its colorspace.
   * This is especially needed for v1 configs. */
  const ocio::View *untonemapped_view = (display) ? display->get_untonemapped_view() : nullptr;
  const ocio::ColorSpace *untonemapped_colorspace = (untonemapped_view) ?
                                                        g_config->get_display_view_color_space(
                                                            display_settings->display_device,
                                                            untonemapped_view->name()) :
                                                        nullptr;
  return (untonemapped_colorspace) ? untonemapped_colorspace : colorspace;
}

bool IMB_colormanagement_display_is_hdr(const ColorManagedDisplaySettings *display_settings,
                                        const char *view_name)
{
  const ocio::Display *display = g_config->get_display_by_name(display_settings->display_device);
  if (display == nullptr) {
    return false;
  }
  const ocio::View *view = display->get_view_by_name(view_name);
  return (view) ? view->is_hdr() : false;
}

bool IMB_colormanagement_display_is_wide_gamut(const ColorManagedDisplaySettings *display_settings,
                                               const char *view_name)
{
  const ocio::Display *display = g_config->get_display_by_name(display_settings->display_device);
  if (display == nullptr) {
    return false;
  }
  const ocio::View *view = display->get_view_by_name(view_name);
  return (view) ? view->gamut() != ocio::Gamut::Rec709 : false;
}

bool IMB_colormanagement_display_support_emulation(
    const ColorManagedDisplaySettings *display_settings, const char *view_name)
{
  const ocio::Display *display = g_config->get_display_by_name(display_settings->display_device);
  if (display == nullptr) {
    return false;
  }
  const ocio::View *view = display->get_view_by_name(view_name);
  return (view) ? view->support_emulation() : false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Functions
 * \{ */

int IMB_colormanagement_view_get_id_by_name(const char *name)
{
  return g_all_view_names.index_of_try(name);
}

const char *IMB_colormanagement_view_get_name_by_id(const int index)
{
  /* The code is expected to be used for the purposes of the EnumPropertyItem and maintaining the
   * DNA values from RNA's get() and set(). It is unexpected that an invalid index will be passed
   * here, as it will indicate a coding error somewhere else. */
  if (index < 0 || index >= g_all_view_names.size()) {
    BLI_assert(0);
    return "";
  }

  return g_all_view_names[index].c_str();
}

const char *IMB_colormanagement_view_get_default_name(const char *display_name)
{
  const ocio::Display *display = g_config->get_display_by_name(display_name);
  if (!display) {
    return "";
  }

  const ocio::View *view = display->get_default_view();
  if (!view) {
    return "";
  }

  return view->name().c_str();
}

const char *IMB_colormanagement_view_get_raw_or_default_name(const char *display_name)
{
  const ocio::Display *display = g_config->get_display_by_name(display_name);
  if (!display) {
    return "";
  }

  const ocio::View *view = nullptr;

  if (!view) {
    view = display->get_view_by_name("Raw");
  }

  if (!view) {
    view = display->get_default_view();
  }

  if (!view) {
    return "";
  }

  return view->name().c_str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Space Functions
 * \{ */

const ColorSpace *colormanage_colorspace_get_named(const char *name)
{
  return g_config->get_color_space(name);
}

const ColorSpace *colormanage_colorspace_get_roled(const int role)
{
  const char *role_colorspace = IMB_colormanagement_role_colorspace_name_get(role);

  return colormanage_colorspace_get_named(role_colorspace);
}

int IMB_colormanagement_colorspace_get_named_index(const char *name)
{
  /* Roles. */
  if (STREQ(name, OCIO_ROLE_SCENE_LINEAR)) {
    return g_config->get_num_color_spaces();
  }

  /* Regular color spaces. */
  const ColorSpace *colorspace = colormanage_colorspace_get_named(name);
  if (colorspace) {
    return colorspace->index;
  }
  return -1;
}

const char *IMB_colormanagement_colorspace_get_indexed_name(const int index)
{
  /* Roles. */
  if (index == g_config->get_num_color_spaces()) {
    return OCIO_ROLE_SCENE_LINEAR;
  }

  /* Regular color spaces. */
  const ColorSpace *colorspace = g_config->get_color_space_by_index(index);
  if (colorspace) {
    return colorspace->name().c_str();
  }
  return "";
}

const char *IMB_colormanagement_colorspace_get_name(const ColorSpace *colorspace)
{
  return colorspace->name().c_str();
}

void IMB_colormanagement_colorspace_from_ibuf_ftype(
    ColorManagedColorspaceSettings *colorspace_settings, ImBuf *ibuf)
{
  /* Don't modify non-color data space, it does not change with file type. */
  const ColorSpace *colorspace = g_config->get_color_space(colorspace_settings->name);

  if (colorspace && colorspace->is_data()) {
    return;
  }

  /* Get color space from file type. */
  const ImFileType *type = IMB_file_type_from_ibuf(ibuf);
  if (type != nullptr) {
    if (type->save != nullptr) {
      const char *role_colorspace = IMB_colormanagement_role_colorspace_name_get(
          type->default_save_role);
      STRNCPY_UTF8(colorspace_settings->name, role_colorspace);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Looks Functions
 * \{ */

int IMB_colormanagement_look_get_named_index(const char *name)
{
  const ocio::Look *look = g_config->get_look_by_name(name);
  if (look) {
    return look->index;
  }
  return -1;
}

const char *IMB_colormanagement_look_get_indexed_name(const int index)
{
  const ocio::Look *look = g_config->get_look_by_index(index);
  if (!look) {
    return "";
  }
  return look->name().c_str();
}

const char *IMB_colormanagement_look_get_default_name()
{
  const ocio::Look *look = g_config->get_look_by_index(0);
  if (!look) {
    return "";
  }
  return look->name().c_str();
}

const char *IMB_colormanagement_look_validate_for_view(const char *view_name,
                                                       const char *look_name)
{
  const ocio::Look *look = g_config->get_look_by_name(look_name);
  if (!look) {
    return look_name;
  }

  /* Keep same look if compatible. */
  if (colormanage_compatible_look(look, view_name)) {
    return look_name;
  }

  /* Try to find another compatible look with the same UI name, in case of looks specialized for
   * view transform, */
  for (const int other_look_index : blender::IndexRange(g_config->get_num_looks())) {
    const ocio::Look *other_look = g_config->get_look_by_index(other_look_index);
    if (look->ui_name() == other_look->ui_name() &&
        colormanage_compatible_look(other_look, view_name))
    {
      return other_look->name().c_str();
    }
  }

  return IMB_colormanagement_look_get_default_name();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Working Space Functions
 * \{ */

/* Should have enough bits of precision, and this can be reasonably high assuming
 * that if colorspaces are really this close, no point converting anyway. */
static const float imb_working_space_compare_threshold = 0.001f;

const char *IMB_colormanagement_working_space_get_default()
{
  return global_role_scene_linear_default;
}

int IMB_colormanagement_working_space_get_named_index(const char *name)
{
  return IMB_colormanagement_colorspace_get_named_index(name);
}

const char *IMB_colormanagement_working_space_get_indexed_name(int index)
{
  return IMB_colormanagement_colorspace_get_indexed_name(index);
}

void IMB_colormanagement_working_space_items_add(EnumPropertyItem **items, int *totitem)
{
  const ColorSpace *scene_linear = g_config->get_color_space(global_role_scene_linear_default);

  /* Keep this in sync with known color spaces in
   * imb_colormanagement_working_space_set_from_matrix. */
  blender::Vector<const ColorSpace *> working_spaces = {
      IMB_colormanagement_space_from_interop_id("lin_rec709_scene"),
      IMB_colormanagement_space_from_interop_id("lin_rec2020_scene"),
      IMB_colormanagement_space_from_interop_id("lin_ap1_scene")};

  if (!working_spaces.contains(scene_linear)) {
    working_spaces.prepend(scene_linear);
  }

  for (const ColorSpace *colorspace : working_spaces) {
    if (colorspace == nullptr) {
      continue;
    }

    EnumPropertyItem item;

    item.value = colorspace->index;
    item.name = colorspace->name().c_str();
    item.identifier = colorspace->name().c_str();
    item.icon = 0;
    item.description = colorspace->description().c_str();

    RNA_enum_item_add(items, totitem, &item);
  }
}

const char *IMB_colormanagement_working_space_get()
{
  return global_role_scene_linear;
}

bool IMB_colormanagement_working_space_set_from_name(const char *name)
{
  if (STREQ(global_role_scene_linear, name)) {
    return false;
  }

  CLOG_DEBUG(&LOG, "Setting blend file working color space to '%s'", name);

  /* Change default float along with working space for convenience, if it was the same. */
  if (STREQ(global_role_default_float_default, global_role_scene_linear_default)) {
    STRNCPY(global_role_default_float, name);
  }
  else {
    STRNCPY(global_role_default_float, global_role_default_float_default);
  }

  STRNCPY(global_role_scene_linear, name);
  g_config->set_scene_linear_role(name);

  global_color_picking_state.cpu_processor_from.reset();
  global_color_picking_state.cpu_processor_to.reset();
  colormanage_update_matrices();

  return true;
}

static bool imb_colormanagement_working_space_set_from_matrix(
    Main *bmain, const char *name, const blender::float3x3 &scene_linear_to_xyz)
{
  StringRefNull interop_id;

  /* Check if we match the working space defined by the config. */
  if (blender::math::is_equal(scene_linear_to_xyz,
                              global_scene_linear_to_xyz_default,
                              imb_working_space_compare_threshold))
  {
    /* Update scene linear name in case it is different for this config. */
    STRNCPY(bmain->colorspace.scene_linear_name, global_role_scene_linear_default);
    return IMB_colormanagement_working_space_set_from_name(global_role_scene_linear_default);
  }

  /* Check if we match a known working space made available in
   * IMB_colormanagement_working_space_items_add, that hopefully exists in the config. */
  if (blender::math::is_equal(
          scene_linear_to_xyz, ocio::ACESCG_TO_XYZ, imb_working_space_compare_threshold))
  {
    interop_id = "lin_ap1_scene";
  }
  else if (blender::math::is_equal(scene_linear_to_xyz,
                                   blender::math::invert(ocio::XYZ_TO_REC709),
                                   imb_working_space_compare_threshold))
  {
    interop_id = "lin_rec709_scene";
  }
  else if (blender::math::is_equal(scene_linear_to_xyz,
                                   blender::math::invert(ocio::XYZ_TO_REC2020),
                                   imb_working_space_compare_threshold))
  {
    interop_id = "lin_rec2020_scene";
  }

  if (!interop_id.is_empty()) {
    const ColorSpace *colorspace = g_config->get_color_space_by_interop_id(interop_id);
    if (colorspace) {
      /* Update scene linear name in case it is different for this config. */
      STRNCPY(bmain->colorspace.scene_linear_name, colorspace->name().c_str());
      return IMB_colormanagement_working_space_set_from_name(colorspace->name().c_str());
    }
  }

  /* We couldn't find a matching colorspace, set to the default and inform users.
   * We could try to preserve the original scene linear space, but that would require
   * editing the config at runtime to add it. Not trying to do that for now. */
  STRNCPY(bmain->colorspace.scene_linear_name, global_role_scene_linear_default);
  bmain->colorspace.scene_linear_to_xyz = global_scene_linear_to_xyz_default;

  if (bmain->filepath[0] != '\0') {
    CLOG_ERROR(
        &LOG, "Unknown scene linear working space '%s'. Missing OpenColorIO configuration?", name);
    bmain->colorspace.is_missing_opencolorio_config = true;
  }

  return IMB_colormanagement_working_space_set_from_name(global_role_scene_linear_default);
}

void IMB_colormanagement_working_space_check(Main *bmain,
                                             const bool for_undo,
                                             const bool have_editable_assets)
{
  /* For old files without info, assume current OpenColorIO config. */
  if (blender::math::is_zero(bmain->colorspace.scene_linear_to_xyz)) {
    STRNCPY(bmain->colorspace.scene_linear_name, global_role_scene_linear_default);
    bmain->colorspace.scene_linear_to_xyz = global_scene_linear_to_xyz_default;
    CLOG_DEBUG(&LOG,
               "Blend file has unknown scene linear working color space, setting to default");
  }

  const blender::float3x3 current_scene_linear_to_xyz = blender::colorspace::scene_linear_to_xyz;

  /* Change the working space to the one from the blend file. */
  const bool working_space_changed = imb_colormanagement_working_space_set_from_matrix(
      bmain, bmain->colorspace.scene_linear_name, bmain->colorspace.scene_linear_to_xyz);
  if (!working_space_changed) {
    return;
  }

  /* For undo, we need to convert the linked datablocks as they were left unchanged by undo.
   * For file load, we need to convert editable assets that came from the previous main. */
  if (!(for_undo || have_editable_assets)) {
    return;
  }

  IMB_colormanagement_working_space_convert(
      bmain,
      current_scene_linear_to_xyz,
      blender::math::invert(bmain->colorspace.scene_linear_to_xyz),
      for_undo,
      for_undo,
      !for_undo && have_editable_assets);
}

static blender::float3 imb_working_space_convert(const blender::float3x3 &m,
                                                 const bool is_smaller_gamut,
                                                 const blender::float3 in_rgb)
{
  blender::float3 rgb = m * in_rgb;

  for (int i = 0; i < 3; i++) {
    /* Round to nicer fractions. */
    rgb[i] = 1e-5f * roundf(rgb[i] * 1e5f);
    /* Snap to 0 and 1. */
    if (fabsf(rgb[i]) < 5e-5) {
      rgb[i] = 0.0f;
    }
    else if (fabsf(1.0f - rgb[i]) < 5e-5) {
      rgb[i] = 1.0f;
    }
    /* Clamp when goig to smaller gamut. We can't really distinguish
     * between HDR and out of gamut colors. */
    if (is_smaller_gamut) {
      rgb[i] = blender::math::clamp(rgb[i], 0.0f, 1.0f);
    }
  }

  return rgb;
}

static blender::ColorGeometry4f imb_working_space_convert(const blender::float3x3 &m,
                                                          const bool is_smaller_gamut,
                                                          const blender::ColorGeometry4f color)
{
  using namespace blender;
  const float3 in_rgb = float3(color::unpremultiply_alpha(color));
  const float3 rgb = imb_working_space_convert(m, is_smaller_gamut, in_rgb);
  return color::premultiply_alpha(ColorGeometry4f(rgb[0], rgb[1], rgb[2], color[3]));
}

void IMB_colormanagement_working_space_convert(
    Main *bmain,
    const blender::float3x3 &current_scene_linear_to_xyz,
    const blender::float3x3 &new_xyz_to_scene_linear,
    const bool depsgraph_tag,
    const bool linked_only,
    const bool editable_assets_only)
{
  using namespace blender;
  /* If unknown, assume it's the OpenColorIO config scene linear space. */
  float3x3 bmain_scene_linear_to_xyz = math::is_zero(current_scene_linear_to_xyz) ?
                                           global_scene_linear_to_xyz_default :
                                           current_scene_linear_to_xyz;

  float3x3 M = new_xyz_to_scene_linear * bmain_scene_linear_to_xyz;

  /* Already in the same space? */
  if (math::is_equal(M, float3x3::identity(), imb_working_space_compare_threshold)) {
    return;
  }

  if (math::determinant(M) == 0.0f) {
    CLOG_ERROR(&LOG, "Working space conversion matrix is not invertible");
    return;
  }

  /* Determine if we are going to a smaller gamut and need to clamp. We prefer not to,
   * to preserve HDR colors, although they should not be common in properties. */
  bool is_smaller_gamut = false;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (M[i][j] < 0.0f) {
        is_smaller_gamut = true;
      }
    }
  }

  /* Single color. */
  const auto single = [&M, is_smaller_gamut](float rgb[3]) {
    copy_v3_v3(rgb, imb_working_space_convert(M, is_smaller_gamut, float3(rgb)));
  };

  /* Array with implicit sharing.
   *
   * We store references to all color arrays, so we can efficiently preserve implicit
   * sharing and write in place when possible. */
  struct ColorArrayInfo {
    Vector<ColorGeometry4f **> data_ptrs;
    Vector<ImplicitSharingPtr<> *> sharing_info_ptrs;
    /* Though it is unlikely, the same data array could be used among multiple geometries with
     * different domain sizes, so keep track of the maximum size among all users. */
    size_t max_size;
  };
  Map<const ImplicitSharingInfo *, ColorArrayInfo> color_array_map;

  const auto implicit_sharing_array =
      [&](ImplicitSharingPtr<> &sharing_info, ColorGeometry4f *&data, size_t size) {
        /* No data? */
        if (!sharing_info) {
          BLI_assert(size == 0);
          return;
        }
        color_array_map.add_or_modify(
            sharing_info.get(),
            [&](ColorArrayInfo *value) {
              new (value) ColorArrayInfo();
              value->data_ptrs.append(&data);
              value->sharing_info_ptrs.append(&sharing_info);
              value->max_size = size;
            },
            [&](ColorArrayInfo *value) {
              BLI_assert(data == *value->data_ptrs.last());
              value->data_ptrs.append(&data);
              value->sharing_info_ptrs.append(&sharing_info);
              value->max_size = std::max(value->max_size, size);
            });
      };

  IDTypeForeachColorFunctionCallback fn = {single, implicit_sharing_array};

  /* Iterate over IDs and embedded IDs. No need to do it for master collections
   * though, they don't have colors. */
  /* TODO: Multithreading over IDs? */
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (linked_only) {
      if (!id_iter->lib) {
        continue;
      }
    }
    if (editable_assets_only) {
      if (!(id_iter->lib && (id_iter->lib->runtime->tag & LIBRARY_ASSET_EDITABLE))) {
        continue;
      }
    }

    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id_iter);
    if (id_type->foreach_working_space_color) {
      id_type->foreach_working_space_color(id_iter, fn);
      if (depsgraph_tag) {
        DEG_id_tag_update(id_iter, ID_RECALC_ALL);
      }
    }

    if (bNodeTree *node_tree = bke::node_tree_from_id(id_iter)) {
      const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(&node_tree->id);
      if (id_type->foreach_working_space_color) {
        id_type->foreach_working_space_color(&node_tree->id, fn);
      }
    }
  }
  FOREACH_MAIN_ID_END;

  /* Handle implicit sharing arrays. */
  Vector<Map<const ImplicitSharingInfo *, ColorArrayInfo>::Item> color_array_items(
      color_array_map.items().begin(), color_array_map.items().end());

  threading::parallel_for(color_array_items.index_range(), 64, [&](const IndexRange range) {
    for (const int item_index : range) {
      const auto &item = color_array_items[item_index];

      if (item.value.data_ptrs.size() == item.key->strong_users()) {
        /* All of the users of the array data are from the main we're converting, so we can change
         * the data array in place without allocating a new version. */
        item.key->tag_ensured_mutable();
        MutableSpan<ColorGeometry4f> data(*item.value.data_ptrs.first(), item.value.max_size);
        threading::parallel_for(data.index_range(), 1024, [&](const IndexRange range) {
          for (const int64_t i : range) {
            data[i] = imb_working_space_convert(M, is_smaller_gamut, data[i]);
          }
        });
      }
      else {
        /* Somehow the data is used by something outside of the Main we're currently converting, it
         * has to be duplicated before being converted to avoid changing the original. */
        const Span<ColorGeometry4f> src_data(*item.value.data_ptrs.first(), item.value.max_size);

        auto *dst_data = MEM_malloc_arrayN<ColorGeometry4f>(
            src_data.size(), "IMB_colormanagement_working_space_convert");
        const ImplicitSharingPtr<> sharing_ptr(implicit_sharing::info_for_mem_free(dst_data));

        threading::parallel_for(src_data.index_range(), 1024, [&](const IndexRange range) {
          for (const int64_t i : range) {
            dst_data[i] = imb_working_space_convert(M, is_smaller_gamut, src_data[i]);
          }
        });

        /* Replace the data pointer and the sharing info pointer with the new data in all of the
         * users from the main data-base. The sharing pointer assignment adds a user. */
        for (ColorGeometry4f **pointer : item.value.data_ptrs) {
          *pointer = dst_data;
        }
        for (ImplicitSharingPtr<> *pointer : item.value.sharing_info_ptrs) {
          *pointer = sharing_ptr;
        }
      }
    }
  });
}

void IMB_colormanagement_working_space_convert(Main *bmain, const Main *reference_bmain)
{
  /* If unknown, assume it's the OpenColorIO config scene linear space. */
  float3x3 reference_scene_linear_to_xyz = blender::math::is_zero(
                                               reference_bmain->colorspace.scene_linear_to_xyz) ?
                                               global_scene_linear_to_xyz_default :
                                               reference_bmain->colorspace.scene_linear_to_xyz;

  IMB_colormanagement_working_space_convert(bmain,
                                            bmain->colorspace.scene_linear_to_xyz,
                                            blender::math::invert(reference_scene_linear_to_xyz),
                                            false);

  STRNCPY(bmain->colorspace.scene_linear_name, reference_bmain->colorspace.scene_linear_name);
  bmain->colorspace.scene_linear_to_xyz = reference_bmain->colorspace.scene_linear_to_xyz;
}

void IMB_colormanagement_working_space_init_default(Main *bmain)
{
  STRNCPY(bmain->colorspace.scene_linear_name, global_role_scene_linear_default);
  bmain->colorspace.scene_linear_to_xyz = global_scene_linear_to_xyz_default;
}

void IMB_colormanagement_working_space_init_startup(Main *bmain)
{
  /* If using the default config, keep the one saved in the startup blend.
   * If using the non-default OCIO config, assume we want the working space from that config. */
  if (blender::math::is_zero(bmain->colorspace.scene_linear_to_xyz) || g_config_is_custom) {
    IMB_colormanagement_working_space_init_default(bmain);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Helper Functions
 * \{ */

void IMB_colormanagement_display_items_add(EnumPropertyItem **items, int *totitem)
{
  /* Group by SDR/HDR, to help communicate what obscure Rec.XXXX names do. */
  for (const bool hdr : {false, true}) {
    bool first = true;

    for (const int display_index : blender::IndexRange(g_config->get_num_displays())) {
      const ocio::Display *display = g_config->get_display_by_index(display_index);

      if (display->is_hdr() != hdr) {
        continue;
      }

      if (first) {
        EnumPropertyItem item;
        item.value = -1;
        item.name = (hdr) ? "HDR" : "SDR";
        item.identifier = "";
        item.icon = 0;
        item.description = "";
        RNA_enum_item_add(items, totitem, &item);

        first = false;
      }

      EnumPropertyItem item;

      item.value = display->index;
      item.name = display->ui_name().c_str();
      item.identifier = display->name().c_str();
      item.icon = 0;
      item.description = display->description().c_str();

      RNA_enum_item_add(items, totitem, &item);
    }
  }
}

void IMB_colormanagement_view_items_add(EnumPropertyItem **items,
                                        int *totitem,
                                        const char *display_name)
{
  const ocio::Display *display = g_config->get_display_by_name(display_name);
  if (!display) {
    return;
  }

  for (const int view_index : blender::IndexRange(display->get_num_views())) {
    const ocio::View *view = display->get_view_by_index(view_index);

    EnumPropertyItem item;

    item.value = IMB_colormanagement_view_get_id_by_name(view->name().c_str());
    item.name = view->name().c_str();
    item.identifier = view->name().c_str();
    item.icon = 0;
    item.description = view->description().c_str();

    RNA_enum_item_add(items, totitem, &item);
  }
}

void IMB_colormanagement_look_items_add(EnumPropertyItem **items,
                                        int *totitem,
                                        const char *view_name)
{
  const StringRef view_filter = view_filter_for_look(view_name);

  for (const int look_index : blender::IndexRange(g_config->get_num_looks())) {
    const ocio::Look *look = g_config->get_look_by_index(look_index);
    if (!colormanage_compatible_look(look, view_filter)) {
      continue;
    }

    EnumPropertyItem item;

    item.value = look->index;
    item.name = look->ui_name().c_str();
    item.identifier = look->name().c_str();
    item.icon = 0;
    item.description = look->description().c_str();

    RNA_enum_item_add(items, totitem, &item);
  }
}

void IMB_colormanagement_colorspace_items_add(EnumPropertyItem **items, int *totitem)
{
  /* Regular color spaces. */
  for (const int colorspace_index : blender::IndexRange(g_config->get_num_color_spaces())) {
    const ColorSpace *colorspace = g_config->get_sorted_color_space_by_index(colorspace_index);
    if (!colorspace->is_invertible()) {
      continue;
    }

    EnumPropertyItem item;

    item.value = colorspace->index;
    item.name = colorspace->name().c_str();
    item.identifier = colorspace->name().c_str();
    item.icon = 0;
    item.description = colorspace->description().c_str();

    RNA_enum_item_add(items, totitem, &item);
  }

  /* Scene linear role. This is useful for example to create compositing convert colorspace
   * nodes that work the same regardless of working space. */
  EnumPropertyItem item;

  item.value = g_config->get_num_color_spaces();
  item.name = "Working Space";
  item.identifier = OCIO_ROLE_SCENE_LINEAR;
  item.icon = 0;
  item.description = "Working color space of the current file";

  RNA_enum_item_add(items, totitem, &item);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Partial Display Buffer Update
 * \{ */

/*
 * Partial display update is supposed to be used by such areas as
 * compositor and renderer, This areas are calculating tiles of the
 * images and because of performance reasons only this tiles should
 * be color managed.
 * This gives nice visual feedback without slowing things down.
 *
 * Updating happens for active display transformation only, all
 * the rest buffers would be marked as dirty
 */

static void partial_buffer_update_rect(ImBuf *ibuf,
                                       uchar *display_buffer,
                                       const float *linear_buffer,
                                       const uchar *byte_buffer,
                                       int display_stride,
                                       int linear_stride,
                                       int linear_offset_x,
                                       int linear_offset_y,
                                       ColormanageProcessor *cm_processor,
                                       const int xmin,
                                       const int ymin,
                                       const int xmax,
                                       const int ymax)
{
  int x, y;
  int channels = ibuf->channels;
  float dither = ibuf->dither;
  const ColorSpace *rect_colorspace = ibuf->byte_buffer.colorspace;
  float *display_buffer_float = nullptr;
  const int width = xmax - xmin;
  const int height = ymax - ymin;
  bool is_data = (ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA) != 0;

  if (dither != 0.0f) {
    /* cm_processor is nullptr in cases byte_buffer's space matches display
     * buffer's space
     * in this case we could skip extra transform and only apply dither
     * use 4 channels for easier byte->float->byte conversion here so
     * (this is only needed to apply dither, in other cases we'll convert
     * byte buffer to display directly)
     */
    if (!cm_processor) {
      channels = 4;
    }

    display_buffer_float = MEM_malloc_arrayN<float>(
        size_t(channels) * size_t(width) * size_t(height), "display buffer for dither");
  }

  if (cm_processor) {
    for (y = ymin; y < ymax; y++) {
      for (x = xmin; x < xmax; x++) {
        size_t display_index = (size_t(y) * display_stride + x) * 4;
        size_t linear_index = (size_t(y - linear_offset_y) * linear_stride +
                               (x - linear_offset_x)) *
                              channels;
        float pixel[4];

        if (linear_buffer) {
          if (channels == 4) {
            copy_v4_v4(pixel, (float *)linear_buffer + linear_index);
          }
          else if (channels == 3) {
            copy_v3_v3(pixel, (float *)linear_buffer + linear_index);
            pixel[3] = 1.0f;
          }
          else if (channels == 1) {
            pixel[0] = linear_buffer[linear_index];
          }
          else {
            BLI_assert_msg(0, "Unsupported number of channels in partial buffer update");
          }
        }
        else if (byte_buffer) {
          rgba_uchar_to_float(pixel, byte_buffer + linear_index);
          IMB_colormanagement_colorspace_to_scene_linear_v3(pixel, rect_colorspace);
          straight_to_premul_v4(pixel);
        }

        if (!is_data) {
          IMB_colormanagement_processor_apply_pixel(cm_processor, pixel, channels);
        }

        if (display_buffer_float) {
          size_t index = (size_t(y - ymin) * width + (x - xmin)) * channels;

          if (channels == 4) {
            copy_v4_v4(display_buffer_float + index, pixel);
          }
          else if (channels == 3) {
            copy_v3_v3(display_buffer_float + index, pixel);
          }
          else /* if (channels == 1) */ {
            display_buffer_float[index] = pixel[0];
          }
        }
        else {
          if (channels == 4) {
            float pixel_straight[4];
            premul_to_straight_v4_v4(pixel_straight, pixel);
            rgba_float_to_uchar(display_buffer + display_index, pixel_straight);
          }
          else if (channels == 3) {
            rgb_float_to_uchar(display_buffer + display_index, pixel);
            display_buffer[display_index + 3] = 255;
          }
          else /* if (channels == 1) */ {
            display_buffer[display_index] = display_buffer[display_index + 1] =
                display_buffer[display_index + 2] = display_buffer[display_index + 3] =
                    unit_float_to_uchar_clamp(pixel[0]);
          }
        }
      }
    }
  }
  else {
    if (display_buffer_float) {
      /* huh, for dither we need float buffer first, no cheaper way. currently */
      IMB_buffer_float_from_byte(display_buffer_float,
                                 byte_buffer,
                                 IB_PROFILE_SRGB,
                                 IB_PROFILE_SRGB,
                                 true,
                                 width,
                                 height,
                                 width,
                                 display_stride);
    }
    else {
      int i;

      for (i = ymin; i < ymax; i++) {
        size_t byte_offset = (size_t(linear_stride) * i + xmin) * 4;
        size_t display_offset = (size_t(display_stride) * i + xmin) * 4;

        memcpy(
            display_buffer + display_offset, byte_buffer + byte_offset, sizeof(char[4]) * width);
      }
    }
  }

  if (display_buffer_float) {
    size_t display_index = (size_t(ymin) * display_stride + xmin) * channels;

    IMB_buffer_byte_from_float(display_buffer + display_index,
                               display_buffer_float,
                               channels,
                               dither,
                               IB_PROFILE_SRGB,
                               IB_PROFILE_SRGB,
                               true,
                               width,
                               height,
                               display_stride,
                               width,
                               ymin);

    MEM_freeN(display_buffer_float);
  }
}

static void imb_partial_display_buffer_update_ex(
    ImBuf *ibuf,
    const float *linear_buffer,
    const uchar *byte_buffer,
    int stride,
    int offset_x,
    int offset_y,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    int xmin,
    int ymin,
    int xmax,
    int ymax,
    bool do_threads)
{
  using namespace blender;
  ColormanageCacheViewSettings cache_view_settings;
  ColormanageCacheDisplaySettings cache_display_settings;
  void *cache_handle = nullptr;
  uchar *display_buffer = nullptr;
  int buffer_width = ibuf->x;

  if (ibuf->display_buffer_flags) {
    int view_flag, display_index;

    colormanage_view_settings_to_cache(ibuf, &cache_view_settings, view_settings);
    colormanage_display_settings_to_cache(&cache_display_settings, display_settings);

    view_flag = 1 << cache_view_settings.view;
    display_index = cache_display_settings.display;

    BLI_thread_lock(LOCK_COLORMANAGE);

    if ((ibuf->userflags & IB_DISPLAY_BUFFER_INVALID) == 0) {
      display_buffer = colormanage_cache_get(
          ibuf, &cache_view_settings, &cache_display_settings, &cache_handle);
    }

    /* In some rare cases buffer's dimension could be changing directly from
     * different thread
     * this i.e. happens when image editor acquires render result
     */
    buffer_width = ibuf->x;

    /* Mark all other buffers as invalid. */
    memset(ibuf->display_buffer_flags, 0, g_config->get_num_displays() * sizeof(uint));
    ibuf->display_buffer_flags[display_index] |= view_flag;

    BLI_thread_unlock(LOCK_COLORMANAGE);
  }

  if (display_buffer) {
    ColormanageProcessor *cm_processor = nullptr;
    bool skip_transform = false;

    /* If we only have a byte or a float buffer, and color space already
     * matches display, there's no need to do color transforms.
     * However if both float and byte buffers exist, it is likely that
     * some operation was performed on float buffer first, and the byte
     * buffer is out of date. */
    if (linear_buffer == nullptr && byte_buffer != nullptr) {
      skip_transform = is_colorspace_same_as_display(
          ibuf->byte_buffer.colorspace, view_settings, display_settings);
    }
    if (byte_buffer == nullptr && linear_buffer != nullptr) {
      skip_transform = is_colorspace_same_as_display(
          ibuf->float_buffer.colorspace, view_settings, display_settings);
    }

    if (!skip_transform) {
      cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);
    }

    threading::parallel_for(IndexRange(ymin, ymax - ymin),
                            do_threads ? 64 : ymax - ymin,
                            [&](const IndexRange y_range) {
                              partial_buffer_update_rect(ibuf,
                                                         display_buffer,
                                                         linear_buffer,
                                                         byte_buffer,
                                                         buffer_width,
                                                         stride,
                                                         offset_x,
                                                         offset_y,
                                                         cm_processor,
                                                         xmin,
                                                         y_range.first(),
                                                         xmax,
                                                         y_range.one_after_last());
                            });

    if (cm_processor) {
      IMB_colormanagement_processor_free(cm_processor);
    }

    IMB_display_buffer_release(cache_handle);
  }
}

void IMB_partial_display_buffer_update(ImBuf *ibuf,
                                       const float *linear_buffer,
                                       const uchar *byte_buffer,
                                       int stride,
                                       int offset_x,
                                       int offset_y,
                                       const ColorManagedViewSettings *view_settings,
                                       const ColorManagedDisplaySettings *display_settings,
                                       int xmin,
                                       int ymin,
                                       int xmax,
                                       int ymax)
{
  imb_partial_display_buffer_update_ex(ibuf,
                                       linear_buffer,
                                       byte_buffer,
                                       stride,
                                       offset_x,
                                       offset_y,
                                       view_settings,
                                       display_settings,
                                       xmin,
                                       ymin,
                                       xmax,
                                       ymax,
                                       false);
}

void IMB_partial_display_buffer_update_threaded(
    ImBuf *ibuf,
    const float *linear_buffer,
    const uchar *byte_buffer,
    int stride,
    int offset_x,
    int offset_y,
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    int xmin,
    int ymin,
    int xmax,
    int ymax)
{
  int width = xmax - xmin;
  int height = ymax - ymin;
  bool do_threads = (size_t(width) * height >= 64 * 64);
  imb_partial_display_buffer_update_ex(ibuf,
                                       linear_buffer,
                                       byte_buffer,
                                       stride,
                                       offset_x,
                                       offset_y,
                                       view_settings,
                                       display_settings,
                                       xmin,
                                       ymin,
                                       xmax,
                                       ymax,
                                       do_threads);
}

void IMB_partial_display_buffer_update_delayed(ImBuf *ibuf, int xmin, int ymin, int xmax, int ymax)
{
  if (ibuf->invalid_rect.xmin == ibuf->invalid_rect.xmax) {
    BLI_rcti_init(&ibuf->invalid_rect, xmin, xmax, ymin, ymax);
  }
  else {
    rcti rect;
    BLI_rcti_init(&rect, xmin, xmax, ymin, ymax);
    BLI_rcti_union(&ibuf->invalid_rect, &rect);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pixel Processor Functions
 * \{ */

ColormanageProcessor *IMB_colormanagement_display_processor_new(
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorManagedDisplaySpace display_space,
    const bool inverse)
{
  ColormanageProcessor *cm_processor;
  ColorManagedViewSettings untonemapped_view_settings;
  const ColorManagedViewSettings *applied_view_settings;
  const ColorSpace *display_colorspace;

  cm_processor = MEM_new<ColormanageProcessor>("colormanagement processor");

  if (view_settings) {
    applied_view_settings = view_settings;
  }
  else {
    IMB_colormanagement_init_untonemapped_view_settings(&untonemapped_view_settings,
                                                        display_settings);
    applied_view_settings = &untonemapped_view_settings;
  }

  display_colorspace = IMB_colormangement_display_get_color_space(applied_view_settings,
                                                                  display_settings);
  if (display_colorspace) {
    cm_processor->is_data_result = display_colorspace->is_data();
  }

  const bool use_white_balance = applied_view_settings->flag & COLORMANAGE_VIEW_USE_WHITE_BALANCE;
  cm_processor->cpu_processor = get_display_buffer_processor(*display_settings,
                                                             applied_view_settings->look,
                                                             applied_view_settings->view_transform,
                                                             applied_view_settings->exposure,
                                                             applied_view_settings->gamma,
                                                             applied_view_settings->temperature,
                                                             applied_view_settings->tint,
                                                             use_white_balance,
                                                             global_role_scene_linear,
                                                             display_space,
                                                             inverse);

  if (applied_view_settings->flag & COLORMANAGE_VIEW_USE_CURVES) {
    cm_processor->curve_mapping = BKE_curvemapping_copy(applied_view_settings->curve_mapping);
    BKE_curvemapping_premultiply(cm_processor->curve_mapping, false);
  }

  return cm_processor;
}

ColormanageProcessor *IMB_colormanagement_colorspace_processor_new(const char *from_colorspace,
                                                                   const char *to_colorspace)
{
  ColormanageProcessor *cm_processor;

  cm_processor = MEM_new<ColormanageProcessor>("colormanagement processor");
  cm_processor->is_data_result = IMB_colormanagement_space_name_is_data(to_colorspace);

  cm_processor->cpu_processor = g_config->get_cpu_processor(from_colorspace, to_colorspace);

  return cm_processor;
}

bool IMB_colormanagement_processor_is_noop(ColormanageProcessor *cm_processor)
{
  if (cm_processor->curve_mapping) {
    /* Consider processor which has curve mapping as a non no-op.
     * This is mainly for the simplicity of the check, since the current cases where this
     * function is used the curve mapping is never assigned. */
    return false;
  }

  if (!cm_processor->cpu_processor) {
    /* The CPU processor might have failed to be created, for example when the requested color
     * space does not exist in the configuration, or if there is a missing lookup table, or the
     * configuration is invalid due to other reasons.
     *
     * The actual processing checks for the cpu_processor not being null pointer, and it if is
     * then processing does not apply it. However, processing could still apply curve mapping.
     *
     * Hence a null-pointer here, which happens after the curve mapping check, but before
     * accessing cpu_processor. */
    return true;
  }

  return cm_processor->cpu_processor->is_noop();
}

void IMB_colormanagement_processor_apply_v4(ColormanageProcessor *cm_processor, float pixel[4])
{
  if (cm_processor->curve_mapping) {
    BKE_curvemapping_evaluate_premulRGBF(cm_processor->curve_mapping, pixel, pixel);
  }

  if (cm_processor->cpu_processor) {
    cm_processor->cpu_processor->apply_rgba(pixel);
  }
}

void IMB_colormanagement_processor_apply_v4_predivide(ColormanageProcessor *cm_processor,
                                                      float pixel[4])
{
  if (cm_processor->curve_mapping) {
    BKE_curvemapping_evaluate_premulRGBF(cm_processor->curve_mapping, pixel, pixel);
  }

  if (cm_processor->cpu_processor) {
    cm_processor->cpu_processor->apply_rgba_predivide(pixel);
    ;
  }
}

void IMB_colormanagement_processor_apply_v3(ColormanageProcessor *cm_processor, float pixel[3])
{
  if (cm_processor->curve_mapping) {
    BKE_curvemapping_evaluate_premulRGBF(cm_processor->curve_mapping, pixel, pixel);
  }

  if (cm_processor->cpu_processor) {
    cm_processor->cpu_processor->apply_rgb(pixel);
  }
}

void IMB_colormanagement_processor_apply_pixel(ColormanageProcessor *cm_processor,
                                               float *pixel,
                                               int channels)
{
  if (channels == 4) {
    IMB_colormanagement_processor_apply_v4_predivide(cm_processor, pixel);
  }
  else if (channels == 3) {
    IMB_colormanagement_processor_apply_v3(cm_processor, pixel);
  }
  else if (channels == 1) {
    if (cm_processor->curve_mapping) {
      curve_mapping_apply_pixel(cm_processor->curve_mapping, pixel, 1);
    }
  }
  else {
    BLI_assert_msg(
        false, "Incorrect number of channels passed to IMB_colormanagement_processor_apply_pixel");
  }
}

void IMB_colormanagement_processor_apply(ColormanageProcessor *cm_processor,
                                         float *buffer,
                                         int width,
                                         int height,
                                         int channels,
                                         bool predivide)
{
  /* apply curve mapping */
  if (cm_processor->curve_mapping) {
    int x, y;

    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        float *pixel = buffer + channels * (size_t(y) * width + x);

        curve_mapping_apply_pixel(cm_processor->curve_mapping, pixel, channels);
      }
    }
  }

  if (cm_processor->cpu_processor && channels >= 3) {
    /* apply OCIO processor */
    const ocio::PackedImage img(buffer,
                                width,
                                height,
                                channels,
                                ocio::BitDepth::BIT_DEPTH_F32,
                                sizeof(float),
                                size_t(channels) * sizeof(float),
                                size_t(channels) * sizeof(float) * width);

    if (predivide) {
      cm_processor->cpu_processor->apply_predivide(img);
    }
    else {
      cm_processor->cpu_processor->apply(img);
    }
  }
}

void IMB_colormanagement_processor_apply_byte(
    ColormanageProcessor *cm_processor, uchar *buffer, int width, int height, int channels)
{
  /* TODO(sergey): Would be nice to support arbitrary channels configurations,
   * but for now it's not so important.
   */
  BLI_assert(channels == 4);
  float pixel[4];
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      size_t offset = channels * (size_t(y) * width + x);
      rgba_uchar_to_float(pixel, buffer + offset);
      IMB_colormanagement_processor_apply_v4(cm_processor, pixel);
      rgba_float_to_uchar(buffer + offset, pixel);
    }
  }
}

void IMB_colormanagement_processor_free(ColormanageProcessor *cm_processor)
{
  if (cm_processor->curve_mapping) {
    BKE_curvemapping_free(cm_processor->curve_mapping);
  }

  MEM_delete(cm_processor);
}

/* **** OpenGL drawing routines using GLSL for color space transform ***** */

static CurveMapping *update_glsl_curve_mapping(const ColorManagedViewSettings *view_settings)
{
  /* Using curve mapping? */
  const bool use_curve_mapping = (view_settings->flag & COLORMANAGE_VIEW_USE_CURVES) != 0;
  if (!use_curve_mapping) {
    return nullptr;
  }

  /* Already up to date? */
  if (view_settings->curve_mapping->changed_timestamp ==
          global_gpu_state.curve_mapping_timestamp &&
      view_settings->curve_mapping == global_gpu_state.orig_curve_mapping)
  {
    return view_settings->curve_mapping;
  }

  /* We're using curve mapping's address as a cache ID, so we need to make sure re-allocation
   * gives new address here. We do this by allocating new curve mapping before freeing old one.
   */
  CurveMapping *new_curve_mapping = BKE_curvemapping_copy(view_settings->curve_mapping);

  if (global_gpu_state.curve_mapping) {
    BKE_curvemapping_free(global_gpu_state.curve_mapping);
    global_gpu_state.curve_mapping = nullptr;
  }

  /* Fill in OCIO's curve mapping settings. */
  global_gpu_state.curve_mapping = new_curve_mapping;
  global_gpu_state.curve_mapping_timestamp = view_settings->curve_mapping->changed_timestamp;
  global_gpu_state.orig_curve_mapping = view_settings->curve_mapping;
  global_gpu_state.use_curve_mapping = true;

  return global_gpu_state.curve_mapping;
}

bool IMB_colormanagement_setup_glsl_draw_from_space(
    const ColorManagedViewSettings *view_settings,
    const ColorManagedDisplaySettings *display_settings,
    const ColorSpace *from_colorspace,
    float dither,
    bool predivide,
    bool do_overlay_merge)
{
  ColorManagedViewSettings untonemapped_view_settings;
  const ColorManagedViewSettings *applied_view_settings;

  if (view_settings) {
    applied_view_settings = view_settings;
  }
  else {
    /* If no view settings were specified, use default ones, which will attempt not to do any
     * extra color correction. */
    IMB_colormanagement_init_untonemapped_view_settings(&untonemapped_view_settings,
                                                        display_settings);
    applied_view_settings = &untonemapped_view_settings;
  }

  /* Ensure curve mapping is up to date. */
  CurveMapping *applied_curve_mapping = update_glsl_curve_mapping(applied_view_settings);

  /* GPU shader parameters. */
  const bool use_look = colormanage_use_look(applied_view_settings->look,
                                             applied_view_settings->view_transform);
  const float exposure = applied_view_settings->exposure;
  const float gamma = applied_view_settings->gamma;

  /* TODO)sergey): Use designated initializer. */
  ocio::GPUDisplayParameters display_parameters;
  display_parameters.from_colorspace = from_colorspace ? from_colorspace->name().c_str() :
                                                         global_role_scene_linear;
  display_parameters.view = applied_view_settings->view_transform;
  display_parameters.display = display_settings->display_device;
  display_parameters.look = (use_look) ? applied_view_settings->look : "";
  display_parameters.curve_mapping = applied_curve_mapping;
  display_parameters.scale = (exposure == 0.0f) ? 1.0f : powf(2.0f, exposure);
  display_parameters.exponent = (gamma == 1.0f) ? 1.0f : 1.0f / max_ff(FLT_EPSILON, gamma);
  display_parameters.dither = dither;
  display_parameters.temperature = applied_view_settings->temperature;
  display_parameters.tint = applied_view_settings->tint;
  display_parameters.use_white_balance = (applied_view_settings->flag &
                                          COLORMANAGE_VIEW_USE_WHITE_BALANCE) != 0;
  display_parameters.use_predivide = predivide;
  display_parameters.do_overlay_merge = do_overlay_merge;
  display_parameters.use_hdr_buffer = GPU_hdr_support();
  display_parameters.use_hdr_display = IMB_colormanagement_display_is_hdr(
      display_settings, display_parameters.view.c_str());
  display_parameters.use_display_emulation = get_display_emulation(*display_settings);

  /* Bind shader. Internally GPU shaders are created and cached on demand. */
  global_gpu_state.gpu_shader_bound = g_config->get_gpu_shader_binder().display_bind(
      display_parameters);

  return global_gpu_state.gpu_shader_bound;
}

bool IMB_colormanagement_setup_glsl_draw(const ColorManagedViewSettings *view_settings,
                                         const ColorManagedDisplaySettings *display_settings,
                                         float dither,
                                         bool predivide)
{
  return IMB_colormanagement_setup_glsl_draw_from_space(
      view_settings, display_settings, nullptr, dither, predivide, false);
}

bool IMB_colormanagement_setup_glsl_draw_from_space_ctx(const bContext *C,
                                                        const ColorSpace *from_colorspace,
                                                        float dither,
                                                        bool predivide)
{
  ColorManagedViewSettings *view_settings;
  ColorManagedDisplaySettings *display_settings;

  IMB_colormanagement_display_settings_from_ctx(C, &view_settings, &display_settings);

  return IMB_colormanagement_setup_glsl_draw_from_space(
      view_settings, display_settings, from_colorspace, dither, predivide, false);
}

bool IMB_colormanagement_setup_glsl_draw_ctx(const bContext *C, float dither, bool predivide)
{
  return IMB_colormanagement_setup_glsl_draw_from_space_ctx(C, nullptr, dither, predivide);
}

bool IMB_colormanagement_setup_glsl_draw_to_scene_linear(const char *from_colorspace_name,
                                                         const bool predivide)
{
  global_gpu_state.gpu_shader_bound = g_config->get_gpu_shader_binder().to_scene_linear_bind(
      from_colorspace_name, predivide);
  return global_gpu_state.gpu_shader_bound;
}

void IMB_colormanagement_finish_glsl_draw()
{
  if (global_gpu_state.gpu_shader_bound) {
    g_config->get_gpu_shader_binder().unbind();
    global_gpu_state.gpu_shader_bound = false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering Tables
 * \{ */

/* Calculate color in range 800..12000 using an approximation
 * a/x+bx+c for R and G and ((at + b)t + c)t + d) for B
 *
 * The result of this can be negative to support gamut wider than
 * than rec.709, just needs to be clamped. */

static const float blackbody_table_r[7][3] = {{1.61919106e+03f, -2.05010916e-03f, 5.02995757e+00f},
                                              {2.48845471e+03f, -1.11330907e-03f, 3.22621544e+00f},
                                              {3.34143193e+03f, -4.86551192e-04f, 1.76486769e+00f},
                                              {4.09461742e+03f, -1.27446582e-04f, 7.25731635e-01f},
                                              {4.67028036e+03f, 2.91258199e-05f, 1.26703442e-01f},
                                              {4.59509185e+03f, 2.87495649e-05f, 1.50345020e-01f},
                                              {3.78717450e+03f, 9.35907826e-06f, 3.99075871e-01f}};

static const float blackbody_table_g[7][3] = {
    {-4.88999748e+02f, 6.04330754e-04f, -7.55807526e-02f},
    {-7.55994277e+02f, 3.16730098e-04f, 4.78306139e-01f},
    {-1.02363977e+03f, 1.20223470e-04f, 9.36662319e-01f},
    {-1.26571316e+03f, 4.87340896e-06f, 1.27054498e+00f},
    {-1.42529332e+03f, -4.01150431e-05f, 1.43972784e+00f},
    {-1.17554822e+03f, -2.16378048e-05f, 1.30408023e+00f},
    {-5.00799571e+02f, -4.59832026e-06f, 1.09098763e+00f}};

static const float blackbody_table_b[7][4] = {
    {5.96945309e-11f, -4.85742887e-08f, -9.70622247e-05f, -4.07936148e-03f},
    {2.40430366e-11f, 5.55021075e-08f, -1.98503712e-04f, 2.89312858e-02f},
    {-1.40949732e-11f, 1.89878968e-07f, -3.56632824e-04f, 9.10767778e-02f},
    {-3.61460868e-11f, 2.84822009e-07f, -4.93211319e-04f, 1.56723440e-01f},
    {-1.97075738e-11f, 1.75359352e-07f, -2.50542825e-04f, -2.22783266e-02f},
    {-1.61997957e-13f, -1.64216008e-08f, 3.86216271e-04f, -7.38077418e-01f},
    {6.72650283e-13f, -2.73078809e-08f, 4.24098264e-04f, -7.52335691e-01f}};

static void blackbody_temperature_to_rec709(float rec709[3], float t)
{
  if (t >= 12000.0f) {
    rec709[0] = 0.8262954810464208f;
    rec709[1] = 0.9945080501520986f;
    rec709[2] = 1.566307710274283f;
  }
  else if (t < 800.0f) {
    rec709[0] = 5.413294490189271f;
    rec709[1] = -0.20319390035873933f;
    rec709[2] = -0.0822535242887164f;
  }
  else {
    int i = (t >= 6365.0f) ? 6 :
            (t >= 3315.0f) ? 5 :
            (t >= 1902.0f) ? 4 :
            (t >= 1449.0f) ? 3 :
            (t >= 1167.0f) ? 2 :
            (t >= 965.0f)  ? 1 :
                             0;

    const float *r = blackbody_table_r[i];
    const float *g = blackbody_table_g[i];
    const float *b = blackbody_table_b[i];

    const float t_inv = 1.0f / t;
    rec709[0] = r[0] * t_inv + r[1] * t + r[2];
    rec709[1] = g[0] * t_inv + g[1] * t + g[2];
    rec709[2] = ((b[0] * t + b[1]) * t + b[2]) * t + b[3];
  }
}

void IMB_colormanagement_blackbody_temperature_to_rgb(float r_dest[4], float value)
{
  float rec709[3];
  blackbody_temperature_to_rec709(rec709, value);

  float rgb[3];
  IMB_colormanagement_rec709_to_scene_linear(rgb, rec709);
  clamp_v3(rgb, 0.0f, FLT_MAX);

  copy_v3_v3(r_dest, rgb);
  r_dest[3] = 1.0f;
}

void IMB_colormanagement_blackbody_temperature_to_rgb_table(float *r_table,
                                                            const int width,
                                                            const float min,
                                                            const float max)
{
  for (int i = 0; i < width; i++) {
    float temperature = min + (max - min) / float(width) * float(i);
    IMB_colormanagement_blackbody_temperature_to_rgb(&r_table[i * 4], temperature);
  }
}

/**
 * CIE color matching functions `xBar`, `yBar`, and `zBar` for
 * wavelengths from 380 through 780 nanometers, every 5 nanometers.
 *
 * For a wavelength lambda in this range:
 * \code{.txt}
 * cie_color_match[(lambda - 380) / 5][0] = xBar
 * cie_color_match[(lambda - 380) / 5][1] = yBar
 * cie_color_match[(lambda - 380) / 5][2] = zBar
 * \endcode
 */

static float cie_color_match[81][3] = {
    {0.0014f, 0.0000f, 0.0065f}, {0.0022f, 0.0001f, 0.0105f}, {0.0042f, 0.0001f, 0.0201f},
    {0.0076f, 0.0002f, 0.0362f}, {0.0143f, 0.0004f, 0.0679f}, {0.0232f, 0.0006f, 0.1102f},
    {0.0435f, 0.0012f, 0.2074f}, {0.0776f, 0.0022f, 0.3713f}, {0.1344f, 0.0040f, 0.6456f},
    {0.2148f, 0.0073f, 1.0391f}, {0.2839f, 0.0116f, 1.3856f}, {0.3285f, 0.0168f, 1.6230f},
    {0.3483f, 0.0230f, 1.7471f}, {0.3481f, 0.0298f, 1.7826f}, {0.3362f, 0.0380f, 1.7721f},
    {0.3187f, 0.0480f, 1.7441f}, {0.2908f, 0.0600f, 1.6692f}, {0.2511f, 0.0739f, 1.5281f},
    {0.1954f, 0.0910f, 1.2876f}, {0.1421f, 0.1126f, 1.0419f}, {0.0956f, 0.1390f, 0.8130f},
    {0.0580f, 0.1693f, 0.6162f}, {0.0320f, 0.2080f, 0.4652f}, {0.0147f, 0.2586f, 0.3533f},
    {0.0049f, 0.3230f, 0.2720f}, {0.0024f, 0.4073f, 0.2123f}, {0.0093f, 0.5030f, 0.1582f},
    {0.0291f, 0.6082f, 0.1117f}, {0.0633f, 0.7100f, 0.0782f}, {0.1096f, 0.7932f, 0.0573f},
    {0.1655f, 0.8620f, 0.0422f}, {0.2257f, 0.9149f, 0.0298f}, {0.2904f, 0.9540f, 0.0203f},
    {0.3597f, 0.9803f, 0.0134f}, {0.4334f, 0.9950f, 0.0087f}, {0.5121f, 1.0000f, 0.0057f},
    {0.5945f, 0.9950f, 0.0039f}, {0.6784f, 0.9786f, 0.0027f}, {0.7621f, 0.9520f, 0.0021f},
    {0.8425f, 0.9154f, 0.0018f}, {0.9163f, 0.8700f, 0.0017f}, {0.9786f, 0.8163f, 0.0014f},
    {1.0263f, 0.7570f, 0.0011f}, {1.0567f, 0.6949f, 0.0010f}, {1.0622f, 0.6310f, 0.0008f},
    {1.0456f, 0.5668f, 0.0006f}, {1.0026f, 0.5030f, 0.0003f}, {0.9384f, 0.4412f, 0.0002f},
    {0.8544f, 0.3810f, 0.0002f}, {0.7514f, 0.3210f, 0.0001f}, {0.6424f, 0.2650f, 0.0000f},
    {0.5419f, 0.2170f, 0.0000f}, {0.4479f, 0.1750f, 0.0000f}, {0.3608f, 0.1382f, 0.0000f},
    {0.2835f, 0.1070f, 0.0000f}, {0.2187f, 0.0816f, 0.0000f}, {0.1649f, 0.0610f, 0.0000f},
    {0.1212f, 0.0446f, 0.0000f}, {0.0874f, 0.0320f, 0.0000f}, {0.0636f, 0.0232f, 0.0000f},
    {0.0468f, 0.0170f, 0.0000f}, {0.0329f, 0.0119f, 0.0000f}, {0.0227f, 0.0082f, 0.0000f},
    {0.0158f, 0.0057f, 0.0000f}, {0.0114f, 0.0041f, 0.0000f}, {0.0081f, 0.0029f, 0.0000f},
    {0.0058f, 0.0021f, 0.0000f}, {0.0041f, 0.0015f, 0.0000f}, {0.0029f, 0.0010f, 0.0000f},
    {0.0020f, 0.0007f, 0.0000f}, {0.0014f, 0.0005f, 0.0000f}, {0.0010f, 0.0004f, 0.0000f},
    {0.0007f, 0.0002f, 0.0000f}, {0.0005f, 0.0002f, 0.0000f}, {0.0003f, 0.0001f, 0.0000f},
    {0.0002f, 0.0001f, 0.0000f}, {0.0002f, 0.0001f, 0.0000f}, {0.0001f, 0.0000f, 0.0000f},
    {0.0001f, 0.0000f, 0.0000f}, {0.0001f, 0.0000f, 0.0000f}, {0.0000f, 0.0000f, 0.0000f}};

static void wavelength_to_xyz(float xyz[3], float lambda_nm)
{
  float ii = (lambda_nm - 380.0f) * (1.0f / 5.0f); /* Scaled 0..80. */
  int i = int(ii);

  if (i < 0 || i >= 80) {
    xyz[0] = 0.0f;
    xyz[1] = 0.0f;
    xyz[2] = 0.0f;
  }
  else {
    ii -= float(i);
    const float *c = cie_color_match[i];
    xyz[0] = c[0] + ii * (c[3] - c[0]);
    xyz[1] = c[1] + ii * (c[4] - c[1]);
    xyz[2] = c[2] + ii * (c[5] - c[2]);
  }
}

void IMB_colormanagement_wavelength_to_rgb(float r_dest[4], float value)
{
  float xyz[3];
  wavelength_to_xyz(xyz, value);

  float rgb[3];
  IMB_colormanagement_xyz_to_scene_linear(rgb, xyz);
  clamp_v3(rgb, 0.0f, FLT_MAX);

  copy_v3_v3(r_dest, rgb);
  r_dest[3] = 1.0f;
}

void IMB_colormanagement_wavelength_to_rgb_table(float *r_table, const int width)
{
  for (int i = 0; i < width; i++) {
    float wavelength = 380 + 400 / float(width) * float(i);
    IMB_colormanagement_wavelength_to_rgb(&r_table[i * 4], wavelength);
  }
}

/** \} */

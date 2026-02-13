/* SPDX-FileCopyrightText: 2025 OpenImageIO project
 * SPDX-FileCopyrightText: 2026 Blender Authors
 * SPDX-License-Identifier: Apache-2.0
 *
 * This is a modified version of maketexture.cpp from OpenImageIO, to add a few
 * features missing in the native implementation. */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>

#include <fmt/format.h>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/string_view.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>

#include "util/color.h"
#include "util/colorspace.h"
#include "util/image.h"
#include "util/image_maketx.h"
#include "util/image_metadata.h"
#include "util/log.h"
#include "util/md5.h"
#include "util/path.h"
#include "util/tbb.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

static std::string datestring(time_t t)
{
  struct tm mytm;
  OIIO::Sysutil::get_local_time(&t, &mytm);
  return fmt::format("{:4d}:{:02d}:{:02d} {:02d}:{:02d}:{:02d}",
                     mytm.tm_year + 1900,
                     mytm.tm_mon + 1,
                     mytm.tm_mday,
                     mytm.tm_hour,
                     mytm.tm_min,
                     mytm.tm_sec);
}

template<class SRCTYPE>
static void interppixel_NDC(const OIIO::ImageBuf &buf,
                            float x,
                            float y,
                            OIIO::span<float> pixel,
                            bool envlatlmode,
                            OIIO::ImageBuf::ConstIterator<SRCTYPE> &it,
                            OIIO::ImageBuf::WrapMode wrapmode)
{
  const ImageSpec &spec(buf.spec());
  int fx = spec.x;
  int fy = spec.y;
  int fw = spec.width;
  int fh = spec.height;
  x = static_cast<float>(fx) + x * static_cast<float>(fw);
  y = static_cast<float>(fy) + y * static_cast<float>(fh);

  int n = spec.nchannels;
  float *p0 = OIIO_ALLOCA(float, 4 * n), *p1 = p0 + n, *p2 = p1 + n, *p3 = p2 + n;

  x -= 0.5f;
  y -= 0.5f;
  int xtexel, ytexel;
  float xfrac, yfrac;
  xfrac = floorfrac(x, &xtexel);
  yfrac = floorfrac(y, &ytexel);

  /* Get the four texels */
  it.rerange(xtexel, xtexel + 2, ytexel, ytexel + 2, 0, 1, wrapmode);
  for (int c = 0; c < n; ++c) {
    p0[c] = it[c];
  }
  ++it;
  for (int c = 0; c < n; ++c) {
    p1[c] = it[c];
  }
  ++it;
  for (int c = 0; c < n; ++c) {
    p2[c] = it[c];
  }
  ++it;
  for (int c = 0; c < n; ++c) {
    p3[c] = it[c];
  }

  if (envlatlmode) {
    /* For latlong environment maps, in order to conserve energy, we must weight the pixels by
     * sin(t*PI) because pixels closer to the pole are actually less area on the sphere. Doing
     * this wrong will tend to over-represent the high latitudes in low-res MIP levels. We fold
     * the area weighting into our linear interpolation by adjusting yfrac. */
    int ymin = spec.y;
    int ymax = ymin + spec.height - 1;
    int ynext = OIIO::clamp(ytexel + 1, ymin, ymax);
    ytexel = OIIO::clamp(ytexel, ymin, ymax);
    float w0 = (1.0f - yfrac) * sinf((float)M_PI * (ytexel + 0.5f) / (float)fh);
    float w1 = yfrac * sinf((float)M_PI * (ynext + 0.5f) / (float)fh);
    yfrac = w1 / (w0 + w1);
  }

  /* Bilinearly interpolate. */
  OIIO::bilerp(p0, p1, p2, p3, xfrac, yfrac, n, pixel.data());
}

/* Resize src into dst, relying on the linear interpolation of
 * interppixel_NDC_full or interppixel_NDC_clamped, for the pixel range. */
template<class SRCTYPE>
static bool resize_block_(OIIO::ImageBuf &dst,
                          const OIIO::ImageBuf &src,
                          OIIO::ROI roi,
                          bool envlatlmode)
{
  int x0 = roi.xbegin, x1 = roi.xend, y0 = roi.ybegin, y1 = roi.yend;

  const ImageSpec &dstspec(dst.spec());
  OIIO::span<float> pel = OIIO::OIIO_ALLOCA_SPAN(float, dstspec.nchannels);
  float xoffset = (float)dstspec.x;
  float yoffset = (float)dstspec.y;
  float xscale = 1.0f / (float)dstspec.width;
  float yscale = 1.0f / (float)dstspec.height;
  int nchannels = dst.nchannels();
  assert(dst.spec().format == TypeFloat);
  OIIO::ImageBuf::WrapMode wrapmode = OIIO::ImageBuf::WrapClamp;
  OIIO::ImageBuf::ConstIterator<SRCTYPE> srcit(src);
  OIIO::ImageBuf::Iterator<float> d(dst, roi);
  for (int y = y0; y < y1; ++y) {
    float t = (y + 0.5f) * yscale + yoffset;
    for (int x = x0; x < x1; ++x, ++d) {
      float s = (x + 0.5f) * xscale + xoffset;
      interppixel_NDC<SRCTYPE>(src, s, t, pel, envlatlmode, srcit, wrapmode);
      for (int c = 0; c < nchannels; ++c) {
        d[c] = pel[c];
      }
    }
  }
  return true;
}

/* Helper function to compute the first bilerp pass into a scanline buffer. */
template<class SRCTYPE>
static void halve_scanline(const SRCTYPE *s, const int nchannels, size_t sw, float *dst)
{
  for (size_t i = 0; i < sw; i += 2, s += nchannels) {
    for (int j = 0; j < nchannels; ++j, ++dst, ++s) {
      *dst = 0.5f * (float)(*s + *(s + nchannels));
    }
  }
}

/* Bilinear resize performed as a 2-pass filter.
 * Optimized to assume that the images are contiguous. */
template<class SRCTYPE>
static bool resize_block_2pass(OIIO::ImageBuf &dst, const OIIO::ImageBuf &src, OIIO::ROI roi)
{
  /* Two-pass filtering introduces a half-pixel shift for odd resolutions.
   * Revert to correct bilerp sampling. */
  if (src.spec().width % 2 || src.spec().height % 2) {
    return resize_block_<SRCTYPE>(dst, src, roi, false);
  }

  assert(roi.ybegin + roi.height() <= dst.spec().height);

  /* Allocate two scanline buffers to hold the result of the first pass. */
  const int nchannels = dst.nchannels();
  const size_t row_elem = roi.width() * nchannels; /* # floats in scanline */
  std::unique_ptr<float[]> S0(new float[row_elem]);
  std::unique_ptr<float[]> S1(new float[row_elem]);

  /* We know that the buffers created for mipmapping are all contiguous,
   * so we can skip the iterators for a bilerp resize entirely along with
   * any NDC -> pixel math, and just directly traverse pixels. */
  const SRCTYPE *s = (const SRCTYPE *)src.localpixels();
  SRCTYPE *d = (SRCTYPE *)dst.localpixels();
  assert(s && d);                                      /* Assume contig bufs */
  d += roi.ybegin * dst.spec().width * nchannels;      /* Top of dst OIIO::ROI */
  const size_t ystride = src.spec().width * nchannels; /* Scanline offset */
  s += 2 * roi.ybegin * ystride;                       /* Top of src OIIO::ROI */

  /* Run through destination rows, doing the two-pass bilerp filter. */
  const size_t dw = roi.width(), dh = roi.height(); /* Loop invariants */
  const size_t sw = dw * 2;                         /* Handle odd res */
  for (size_t y = 0; y < dh; ++y) {                 /* For each dst OIIO::ROI row */
    halve_scanline<SRCTYPE>(s, nchannels, sw, &S0[0]);
    s += ystride;
    halve_scanline<SRCTYPE>(s, nchannels, sw, &S1[0]);
    s += ystride;
    const float *s0 = &S0[0], *s1 = &S1[0];
    for (size_t x = 0; x < dw; ++x) { /* For each dst OIIO::ROI col */
      for (int i = 0; i < nchannels; ++i, ++s0, ++s1, ++d) {
        *d = (SRCTYPE)(0.5f * (*s0 + *s1)); /* Average vertically */
      }
    }
  }

  return true;
}

template<typename SRCTYPE>
static bool resize_block(OIIO::ImageBuf &dst,
                         const OIIO::ImageBuf &src,
                         OIIO::ROI roi,
                         bool envlatlmode)
{
  const ImageSpec &srcspec(src.spec());
  const ImageSpec &dstspec(dst.spec());
  assert(dstspec.nchannels == srcspec.nchannels);
  assert(dst.localpixels());
  bool ok;
  if (src.localpixels() &&                    /* Not a cached image */
      !envlatlmode &&                         /* not latlong wrap mode */
      roi.xbegin == 0 &&                      /* Region x at origin */
      dstspec.width == roi.width() &&         /* Full width OIIO::ROI */
      (2 * dstspec.width) == srcspec.width && /* Src is 2x resize */
      dstspec.format == srcspec.format &&     /* Same formats */
      dstspec.x == 0 && dstspec.y == 0 &&     /* Not a crop or overscan */
      srcspec.x == 0 && srcspec.y == 0)
  {
    /* If all these conditions are met, we have a special case that
     * can be more highly optimized. */
    ok = resize_block_2pass<SRCTYPE>(dst, src, roi);
  }
  else {
    assert(dst.spec().format == TypeFloat);
    ok = resize_block_<SRCTYPE>(dst, src, roi, envlatlmode);
  }
  return ok;
}

static void fix_latl_edges(OIIO::ImageBuf &buf)
{
  int n = buf.nchannels();
  OIIO::span<float> left = OIIO::OIIO_ALLOCA_SPAN(float, n);
  OIIO::span<float> right = OIIO::OIIO_ALLOCA_SPAN(float, n);

  /* Make the whole first and last row be solid, since they are exactly on the pole. */
  float wscale = 1.0f / (buf.spec().width);
  for (int j = 0; j <= 1; ++j) {
    int y = (j == 0) ? buf.ybegin() : buf.yend() - 1;
    /* Use left for the sum, right for each new pixel. */
    for (int c = 0; c < n; ++c) {
      left[c] = 0.0f;
    }
    for (int x = buf.xbegin(); x < buf.xend(); ++x) {
      buf.getpixel(x, y, right);
      for (int c = 0; c < n; ++c) {
        left[c] += right[c];
      }
    }
    for (int c = 0; c < n; ++c) {
      left[c] *= wscale;
    }
    for (int x = buf.xbegin(); x < buf.xend(); ++x) {
      buf.setpixel(x, y, left);
    }
  }

  /* Make the left and right match, since they are both right on the prime meridian. */
  for (int y = buf.ybegin(); y < buf.yend(); ++y) {
    buf.getpixel(buf.xbegin(), y, left);
    buf.getpixel(buf.xend() - 1, y, right);
    for (int c = 0; c < n; ++c) {
      left[c] = 0.5f * left[c] + 0.5f * right[c];
    }
    buf.setpixel(buf.xbegin(), y, left);
    buf.setpixel(buf.xend() - 1, y, left);
  }
}

static bool texture_cache_file_outdated(const string &filepath, const string &tx_filepath)
{
  if (!path_is_file(tx_filepath)) {
    return true;
  }

  std::time_t in_time = OIIO::Filesystem::last_write_time(filepath);
  std::time_t out_time = OIIO::Filesystem::last_write_time(tx_filepath);

  if (in_time == out_time) {
    LOG_INFO << "Using texture cache file: " << tx_filepath;
    return false;
  }

  LOG_INFO << "Texture cache file is outdated: " << tx_filepath;
  return true;
}

static std::string alpha_type_string(const ImageAlphaType alpha_type)
{
  switch (alpha_type) {
    case IMAGE_ALPHA_ASSOCIATED:
      return "associated";
      break;
    case IMAGE_ALPHA_UNASSOCIATED:
      return "unassociated";
      break;
    case IMAGE_ALPHA_CHANNEL_PACKED:
      return "channel_packed";
      break;
    case IMAGE_ALPHA_IGNORE:
      return "ignore";
      break;
    case IMAGE_ALPHA_AUTO:
      return "auto";
    case IMAGE_ALPHA_NUM_TYPES:
      break;
  }

  assert(!"Unknown tx alpha type");

  return "auto";
}

static std::string format_tyoe_string(const ImageFormatType format_type)
{
  switch (format_type) {
    case IMAGE_FORMAT_PLAIN:
      return "plain";
    case IMAGE_FORMAT_EQUIANGULAR:
      return "equiangular";
  }

  assert(!"Unknown tx format type");

  return "unknown";
}

static std::string unique_filename_tx(const string &filepath,
                                      const string &texture_cache_path,
                                      ustring colorspace,
                                      const ImageAlphaType alpha_type,
                                      const ImageFormatType format_type,
                                      const bool absolute_texture_cache)
{
  /* NOTE: Be careful not to change existing hashes if at all possible, as this
   * will cause all texture files to be regenerated with significantly increased
   * disk usage. */
  MD5Hash md5;

  /* Scene linear colorspace that we may be converting to. */
  md5.append("xyz_to_scene_linear:" + ColorSpaceManager::get_xyz_to_scene_linear_rgb_string());

  /* Colorspace. */
  md5.append("colorspace:" + (ColorSpaceManager::colorspace_is_data(colorspace) ?
                                  u_colorspace_data.string() :
                                  colorspace.string()));

  md5.append("alpha:" + alpha_type_string(alpha_type));

  md5.append("format:" + format_tyoe_string(format_type));

  /* For absolute texture cache path, include the full file path. This requires
   * a matching directory structure though. */
  /* TODO: Can we use a relative path? The problem is that we can not automatically
   * distinguish between cases like /tmp/texture_cache and /projects/X/texture_cache
   * where the former is not helped by a relative path, while the latter can be. */
  if (absolute_texture_cache) {
    md5.append("filepath:" + path_normalize(path_make_relative(filepath, texture_cache_path)));
  }

  const std::string version = string_printf(".%d-", TX_FILE_FORMAT_VERSION);

  return path_filename(filepath) + version + md5.get_hex() + ".tx";
}

bool resolve_tx(const string &filepath,
                const string &texture_cache_path,
                ustring colorspace,
                const ImageAlphaType alpha_type,
                const ImageFormatType format_type,
                string &out_filepath)
{

  /* Nothing to do if file doesn't even exist. */
  if (!path_is_file(filepath)) {
    return false;
  }

  const string filedir = path_dirname(filepath);

  /* Check the specified directory if one is given. */
  if (!texture_cache_path.empty()) {
    const bool absolute_cache_path = !path_is_relative(texture_cache_path);
    const string tx_filename = unique_filename_tx(
        filepath, texture_cache_path, colorspace, alpha_type, format_type, absolute_cache_path);
    const string tx_filepath = path_join(
        absolute_cache_path ? string(texture_cache_path) : path_join(filedir, texture_cache_path),
        tx_filename);

    out_filepath = tx_filepath;

    if (!texture_cache_file_outdated(filepath, tx_filepath)) {
      return true;
    }
  }

  /* Check in the default location. */
  const char *default_texture_cache_dir = "blender_tx";
  if (texture_cache_path != default_texture_cache_dir) {
    const string tx_filename = unique_filename_tx(
        filepath, texture_cache_path, colorspace, alpha_type, format_type, false);
    const string tx_default_filepath = path_join(path_join(filedir, default_texture_cache_dir),
                                                 tx_filename);
    if (!texture_cache_file_outdated(filepath, tx_default_filepath)) {
      out_filepath = tx_default_filepath;
      return true;
    }

    if (texture_cache_path.empty()) {
      out_filepath = tx_default_filepath;
    }
  }

  /* If it's already a tx file, we can use it directly as well. But it's
   * preferable to use a Cycles native tx file for performance. */
  if (string_endswith(filepath, ".tx")) {
    out_filepath = filepath;
    return true;
  }

  return false;
}

static void write_stats_tx(OIIO::ImageBuf &buf, const bool use_openexr)
{
  OIIO::ImageBufAlgo::PixelStats pixel_stats = OIIO::ImageBufAlgo::computePixelStats(buf);

  /* Constant color optimization. Creating small image and add metadata. */
  std::vector<float> constant_color(buf.nchannels());
  bool is_constant_color = (pixel_stats.min == pixel_stats.max);
  if (is_constant_color) {
    constant_color = pixel_stats.min;

    ImageSpec spec = buf.spec();
    spec.width = std::min(spec.tile_width, spec.width);
    spec.height = std::min(spec.tile_height, spec.height);
    spec.depth = std::min(spec.tile_depth, spec.depth);
    buf.reset(spec);
    OIIO::ImageBufAlgo::fill(buf, constant_color);
    LOG_INFO << "  Constant color image detected. Creating " << spec.width << "x" << spec.height
             << " texture instead.";
  }

  /* TODO: Opaque detection optimization doesn't work currently, because we
   * expected tile pixels to be conformed and that means 1 or 4 channels. */
  const bool is_opaque = buf.spec().alpha_channel == buf.nchannels() - 1 &&
                         pixel_stats.min[buf.spec().alpha_channel] == 1.0f &&
                         pixel_stats.max[buf.spec().alpha_channel] == 1.0f;
#if 0
  if (is_opaque) {
    LOG_INFO << "  Alpha==1 image detected. Dropping the alpha channel.";
    OIIO::ImageBuf new_buf(buf.spec());
    OIIO::ImageBufAlgo::channels(new_buf,
                           buf,
                           buf.nchannels() - 1,
                           OIIO::cspan<int>(),
                           OIIO::cspan<float>(),
                           OIIO::cspan<std::string>(),
                           true);
    std::swap(buf, new_buf);
  }
#endif

  /* Monochrome detection. */
  if (is_opaque && (buf.nchannels() == 3 || buf.nchannels() == 4) &&
      pixel_stats.avg[0] == pixel_stats.avg[1] && pixel_stats.avg[0] == pixel_stats.avg[2] &&
      OIIO::ImageBufAlgo::isMonochrome(buf))
  {
    LOG_INFO << "  Monochrome image detected. Converting to single channel texture.";
    OIIO::ImageBuf new_buf(buf.spec());
    OIIO::ImageBufAlgo::channels(new_buf,
                                 buf,
                                 1,
                                 OIIO::cspan<int>(),
                                 OIIO::cspan<float>(),
                                 OIIO::cspan<std::string>(),
                                 true);
    new_buf.specmod().default_channel_names();
    std::swap(buf, new_buf);
  }

  /* Image description for tiff, to store arbitrary metadata because it's not natively
   * supported by this file format. */
  string desc;

  /* Add hash attribute. */
  const int sha1_blocksize = 256;
  string extra_hash_data;
  std::string hash_digest = OIIO::ImageBufAlgo::computePixelHashSHA1(
      buf, extra_hash_data, OIIO::ROI::All(), sha1_blocksize);
  if (hash_digest.length()) {
    if (use_openexr) {
      buf.specmod().attribute("oiio:SHA-1", hash_digest);
    }
    else {
      if (desc.length()) {
        desc += " ";
      }
      desc += "oiio:SHA-1=";
      desc += hash_digest;
    }
    LOG_DEBUG << "  SHA-1: " << hash_digest;
  }

  /* Add constant color attribute. */
  if (is_constant_color) {
    std::string colstr = OIIO::Strutil::join(constant_color, ",", buf.spec().nchannels);
    if (use_openexr) {
      buf.specmod().attribute("oiio:ConstantColor", colstr);
    }
    else {
      desc += fmt::format("{}oiio:ConstantColor={}", desc.length() ? " " : "", colstr);
    }
    LOG_DEBUG << "  ConstantColor: " << colstr;
  }

  /* Add average color attribtue. */
  std::string avgstr = OIIO::Strutil::join(pixel_stats.avg, ",", buf.spec().nchannels);
  if (use_openexr) {
    buf.specmod().attribute("oiio:AverageColor", avgstr);
  }
  else {
    desc += fmt::format("{}oiio:AverageColor={}", desc.length() ? " " : "", avgstr);
  }
  LOG_DEBUG << "  AverageColor: " << avgstr;

  if (!use_openexr) {
    buf.specmod().attribute("ImageDescription", desc);
  }
}

static void clamp_half_tx(OIIO::ImageBuf &buf, const TypeDesc out_format)
{
  /* Clamp to half bounds to avoid creating inf when converting from float to half on write. */
  if (out_format != TypeDesc::HALF) {
    return;
  }

  assert(buf.spec().format == TypeFloat);

  const int64_t num_values = buf.spec().width * buf.spec().height * buf.spec().nchannels;
  float *pixels = static_cast<float *>(buf.localpixels());
  for (int64_t i = 0; i < num_values; i++) {
    pixels[i] = clamp(pixels[i], -HALF_MAX, HALF_MAX);
  }
}

static void convert_srgb_tx(OIIO::ImageBuf &buf, const bool from_srgb)
{
  assert(buf.spec().format == TypeFloat);
  assert(buf.spec().nchannels == 1 || buf.spec().nchannels == 4);

  const int64_t num_pixels = buf.spec().width * buf.spec().height;

  if (buf.spec().nchannels == 1) {
    float *pixels = static_cast<float *>(buf.localpixels());
    if (from_srgb) {
      for (int64_t i = 0; i < num_pixels; i++) {
        pixels[i] = color_srgb_to_linear(pixels[i]);
      }
    }
    else {
      for (int64_t i = 0; i < num_pixels; i++) {
        pixels[i] = color_linear_to_srgb(pixels[i]);
      }
    }
  }
  else {
    float4 *pixels = static_cast<float4 *>(buf.localpixels());
    for (int64_t i = 0; i < num_pixels; i++) {
      float4 value = pixels[i];
      const bool has_alpha = !(value.w <= 0.0f || value.w == 1.0f);

      if (has_alpha) {
        const float inv_alpha = 1.0f / value.w;
        value.x *= inv_alpha;
        value.y *= inv_alpha;
        value.z *= inv_alpha;
      }

      if (from_srgb) {
        value = color_srgb_to_linear_v4(value);
      }
      else {
        value = color_linear_to_srgb_v4(value);
      }

      if (has_alpha) {
        value.x *= value.w;
        value.y *= value.w;
        value.z *= value.w;
      }

      pixels[i] = value;
    }
  }
}

static bool write_buf_tx(std::unique_ptr<ImageOutput> &out,
                         const string &out_filepath,
                         const TypeDesc out_format,
                         const ImageFormatType format_type,
                         const bool compress_as_srgb,
                         const OIIO::ImageOutput::OpenMode mode,
                         OIIO::ImageBuf &buf)
{
  ImageSpec out_spec = buf.spec();
  out_spec.set_format(out_format);
  if (format_type == IMAGE_FORMAT_EQUIANGULAR) {
    fix_latl_edges(buf);
  }
  OIIO::ImageBuf srgb_buf;
  if (compress_as_srgb) {
    srgb_buf.copy(buf);
    convert_srgb_tx(srgb_buf, false);
  }
  clamp_half_tx(buf, out_format);
  OIIO::ImageBuf &write_buf = (compress_as_srgb) ? srgb_buf : buf;
  if (!out->open(out_filepath, out_spec, mode)) {
    LOG_ERROR << "Could not open \"" << out_filepath << "\" : " << out->geterror();
    return false;
  }
  if (!write_buf.write(out.get())) {
    LOG_ERROR << "Write failed: " << write_buf.geterror();
    out->close();
    return false;
  }

  return true;
}

static bool write_mipmap_tx(std::unique_ptr<ImageOutput> &out,
                            const string &out_filepath,
                            const TypeDesc out_format,
                            const ImageFormatType format_type,
                            const bool compress_as_srgb,
                            OIIO::ImageBuf &large_buf)
{
  const bool envlatlmode = format_type == IMAGE_FORMAT_EQUIANGULAR;
  const ImageOutput::OpenMode append_mode = out->supports("mipmap") ? ImageOutput::AppendMIPLevel :
                                                                      ImageOutput::AppendSubimage;

  /* Write top level. */
  write_buf_tx(
      out, out_filepath, out_format, format_type, false, OIIO::ImageOutput::Create, large_buf);

  /* When using sRGB, convert to linear before resizing, and convert to sRGB again in write_buf_tx.
   * for each mip level. */
  if (compress_as_srgb) {
    convert_srgb_tx(large_buf, true);
  }

  /* Write mip levels. */
  OIIO::ImageBuf small_buf;
  while (large_buf.spec().width > 1 || large_buf.spec().height > 1) {
    ImageSpec small_spec = large_buf.spec();
    small_spec.extra_attribs.free();

    /* Halve resolution and realliocate small buffer */
    if (small_spec.width > 1) {
      small_spec.width /= 2;
    }
    if (small_spec.height > 1) {
      small_spec.height /= 2;
    }
    small_buf.reset(small_spec);

    /* TODO: Support other filters than box? If we do this, be sure to include them in the
     * hash like OIIO maketx does. */
    /* TODO: Support highlight compensation like OIIO? */

    const int rows_per_task = divide_up(16384, small_spec.width);
    parallel_for(blocked_range<int64_t>(0, small_spec.height, rows_per_task),
                 [&](const blocked_range<int64_t> &r) {
                   OIIO::ROI roi(0, small_spec.width, r.begin(), r.end());
                   /* Always float for now. */
                   assert(small_buf.spec().format == TypeFloat);
                   resize_block<float>(small_buf, large_buf, roi, envlatlmode);
                 });

    write_buf_tx(
        out, out_filepath, out_format, format_type, compress_as_srgb, append_mode, small_buf);
    std::swap(large_buf, small_buf);
  }

  if (!out->close()) {
    LOG_ERROR << "Error writing \"" << out_filepath << "\" : " << out->geterror();
    return false;
  }

  return true;
}

static bool make_tx(const string &filepath,
                    const string &out_filepath,
                    ImageMetaData &metadata,
                    const ImageAlphaType alpha_type,
                    const ImageFormatType format_type,
                    const ImageSpec &in_spec)
{
  const bool use_openexr = metadata.is_float();
  const TypeDesc out_format = metadata.typedesc();

  ImageSpec spec;

  /* Dimensions. */
  spec.width = metadata.width;
  spec.height = metadata.height;
  spec.depth = 0;
  spec.nchannels = metadata.is_rgba() ? 4 : 1;
  spec.set_format(out_format);
  spec.tile_width = 64;
  spec.tile_height = 64;
  spec.tile_depth = 1;

  /* Inherit extra attributes from original image. */
  spec.extra_attribs = in_spec.extra_attribs;

  /* Software info, version is used to detect if file format is known by old versions. */
  spec.attribute("Software", string_printf("Blender maketx v%d", TX_FILE_FORMAT_VERSION));

  /* Compression. */
  std::string in_compression = in_spec.get_string_attribute("compression", "none");
  if (in_compression == "none") {
    spec.attribute("compression", "zip");
  }
  spec.attribute("planarconfig", "contig");

  /* Always convert to scene linear or data colorspace with associated alpha. */
  const bool is_data = ColorSpaceManager::colorspace_is_data(metadata.colorspace);
  const bool compress_as_srgb = metadata.is_compressible_as_srgb;
  const ustring colorspace = is_data          ? u_colorspace_data :
                             compress_as_srgb ? u_colorspace_scene_linear_srgb :
                                                u_colorspace_scene_linear;

  spec.attribute("oiio:ColorSpace", colorspace);
  const char *interop_id = ColorSpaceManager::colorspace_interop_id(colorspace);
  if (interop_id) {
    spec.attribute("colorInteropID", interop_id);
  }
  spec.attribute("oiio:UnassociatedAlpha", 0);

  /* Source image metadata. */
  if (use_openexr) {
    spec.attribute("blender:SourceFile", filepath);
    spec.attribute("blender:SourceColorSpace", metadata.colorspace);
    spec.attribute("blender:SourceAlpha", alpha_type_string(alpha_type));
    spec.attribute("blender:XYZToSceneLinear",
                   ColorSpaceManager::get_xyz_to_scene_linear_rgb_string());
  }
  else {
    /* TIFF does not have arbitrary metadata, so we put it in an existing one. */
    spec.attribute("DocumentName",
                   string_printf("%s (colorspace: %s, xyz_to_scene_linear:%s, alpha: %s)",
                                 filepath.c_str(),
                                 metadata.colorspace.c_str(),
                                 ColorSpaceManager::get_xyz_to_scene_linear_rgb_string().c_str(),
                                 alpha_type_string(alpha_type).c_str()));
  }

  /* OIIO texture format metadata. */
  switch (format_type) {
    case IMAGE_FORMAT_PLAIN:
      spec.attribute("textureformat", "Plain Texture");
      spec.attribute("wrapmodes", "black,black");
      break;
    case IMAGE_FORMAT_EQUIANGULAR:
      spec.attribute("textureformat", "LatLong Environment");
      spec.attribute("wrapmodes", "periodic,clamp");
      if (use_openexr) {
        spec.attribute("oiio:updirection", "y");
        spec.attribute("oiio:sampleborder", 1);
      }
      break;
  }

  /* OpenEXR mipmap metadata. */
  if (use_openexr) {
    spec.attribute("openexr:roundingmode", 0 /* ROUND_DOWN */);
    spec.erase_attribute("openexr:levelmode");
  }

  OIIO::ImageBuf buf(spec, OIIO::InitializePixels::No);
  std::time_t in_time = OIIO::Filesystem::last_write_time(filepath);

  if (!metadata.oiio_load_pixels(filepath, buf.localpixels(), false)) {
    LOG_WARNING << "Failed to load pixels for " << filepath;
    return false;
  }

  metadata.conform_pixels(buf.localpixels());

  /* Convert to float only after conforming, so it matches the regular image
   * loading code path exactly. */
  if (buf.spec().format != TypeFloat) {
    spec.set_format(TypeFloat);
    OIIO::ImageBuf float_buf(spec, OIIO::InitializePixels::No);
    OIIO::ImageBufAlgo::copy(float_buf, buf, TypeFloat);
    buf = std::move(float_buf);
  }

  /* Write date and time from input file. */
  buf.specmod().attribute("DateTime", datestring(in_time));

  /* Write pixel statistics. */
  write_stats_tx(buf, use_openexr);

  /* Write to temporary file, next to the final filepath for atomic rename. */
  /* TODO: Can we protect against leaving files on crashes? Two possibilities:
   * - Write to tmp directory, then move next to target file and rename. This reduces
   *   the probability but does not eliminate it.
   * - Add an atexit handles that removes any temporary files we are writing. This
   *   does not help with a power outage or maybe some types of process killing. */
  const char *tmp_extension = (use_openexr) ? ".%%%%%%%%.temp.exr" : ".%%%%%%%%.temp.tif";
  string tmp_filepath = OIIO::Filesystem::unique_path(out_filepath + tmp_extension);

  std::unique_ptr<ImageOutput> out = ImageOutput::create(tmp_filepath);
  bool ok = write_mipmap_tx(
      out, tmp_filepath, out_format, format_type, metadata.is_compressible_as_srgb, buf);
  out.reset();

  /* Stamp with same time as input image file to detect updates. */
  if (ok) {
    OIIO::Filesystem::last_write_time(tmp_filepath, in_time);

    /* Atomic move in case multiple process try to do this, and to avoid writing
     * incomplete files in case of failure. */
    std::string rename_err;
    if (!OIIO::Filesystem::rename(tmp_filepath, out_filepath, rename_err)) {
      LOG_ERROR << "Could not rename file: " << rename_err;
      ok = false;
    }
  }

  path_remove(tmp_filepath);
  assert(path_is_file(out_filepath));
  LOG_INFO << "Wrote tx file: " << out_filepath;

  return ok;
}

bool make_tx(const string &filepath,
             const string &out_filepath,
             ustring colorspace,
             const ImageAlphaType alpha_type,
             const ImageFormatType format_type)
{
  LOG_INFO << "Generating tx file for: " << filepath;

  if (!path_create_directories(out_filepath)) {
    LOG_WARNING << "Failed to create directory for texture cache: " << path_dirname(out_filepath);
    return false;
  }

  ImageMetaData metadata;
  ImageSpec spec;
  metadata.colorspace = colorspace;
  if (!metadata.oiio_load_metadata(filepath, &spec)) {
    LOG_WARNING << "Failed to load metadata for " << filepath;
    return false;
  }
  metadata.finalize(alpha_type);

  if (!make_tx(filepath, out_filepath, metadata, alpha_type, format_type, spec)) {
    LOG_WARNING << "Failed to write tx file";
    return false;
  }

  return true;
}

CCL_NAMESPACE_END

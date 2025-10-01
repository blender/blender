/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "openimageio_support.hh"
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include <algorithm>

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BKE_idprop.hh"

#include "DNA_ID.h"

#include "IMB_allocimbuf.hh"
#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_metadata.hh"

#include "CLG_log.h"

static CLG_LogRef LOG_READ = {"image.read"};
static CLG_LogRef LOG_WRITE = {"image.write"};

OIIO_NAMESPACE_USING

using std::string;
using std::unique_ptr;

namespace blender::imbuf {

/* An OIIO IOProxy used during file packing to write into an in-memory #ImBuf buffer. */
class ImBufMemWriter : public Filesystem::IOProxy {
 public:
  ImBufMemWriter(ImBuf *ibuf) : IOProxy("", Write), ibuf_(ibuf) {}

  const char *proxytype() const override
  {
    return "ImBufMemWriter";
  }

  size_t write(const void *buf, size_t size) override
  {
    size = pwrite(buf, size, m_pos);
    m_pos += size;
    return size;
  }

  size_t pwrite(const void *buf, size_t size, int64_t offset) override
  {
    /* If buffer is too small increase it. */
    size_t end = offset + size;
    while (end > ibuf_->encoded_buffer_size) {
      if (!imb_enlargeencodedbufferImBuf(ibuf_)) {
        /* Out of memory. */
        return 0;
      }
    }

    memcpy(ibuf_->encoded_buffer.data + offset, buf, size);

    ibuf_->encoded_size = std::max<size_t>(end, ibuf_->encoded_size);

    return size;
  }

  size_t size() const override
  {
    return ibuf_->encoded_size;
  }

 private:
  ImBuf *ibuf_;
};

/* Utility to in-place expand an n-component pixel buffer into a 4-component buffer. */
template<typename T>
static void fill_all_channels(T *pixels, int width, int height, int components, T alpha)
{
  const int64_t pixel_count = int64_t(width) * height;
  if (components == 3) {
    for (int64_t i = 0; i < pixel_count; i++) {
      pixels[i * 4 + 3] = alpha;
    }
  }
  else if (components == 1) {
    for (int64_t i = 0; i < pixel_count; i++) {
      pixels[i * 4 + 3] = alpha;
      pixels[i * 4 + 2] = pixels[i * 4 + 0];
      pixels[i * 4 + 1] = pixels[i * 4 + 0];
    }
  }
  else if (components == 2) {
    for (int64_t i = 0; i < pixel_count; i++) {
      pixels[i * 4 + 3] = pixels[i * 4 + 1];
      pixels[i * 4 + 2] = pixels[i * 4 + 0];
      pixels[i * 4 + 1] = pixels[i * 4 + 0];
    }
  }
}

template<typename T>
static ImBuf *load_pixels(
    ImageInput *in, int width, int height, int channels, int flags, bool use_all_planes)
{
  /* Allocate the ImBuf for the image. */
  constexpr bool is_float = sizeof(T) > 1;
  const uint format_flag = (is_float ? IB_float_data : IB_byte_data) | IB_uninitialized_pixels;
  const uint ibuf_flags = (flags & IB_test) ? 0 : format_flag;
  const int planes = use_all_planes ? 32 : 8 * channels;
  ImBuf *ibuf = IMB_allocImBuf(width, height, planes, ibuf_flags);
  if (!ibuf) {
    return nullptr;
  }

  /* No need to load actual pixel data during the test phase. */
  if (flags & IB_test) {
    return ibuf;
  }

  /* Calculate an appropriate stride to read n-channels directly into
   * the ImBuf 4-channel layout. */
  const stride_t ibuf_xstride = sizeof(T) * 4;
  const stride_t ibuf_ystride = ibuf_xstride * width;
  const TypeDesc format = is_float ? TypeDesc::FLOAT : TypeDesc::UINT8;
  uchar *rect = is_float ? reinterpret_cast<uchar *>(ibuf->float_buffer.data) :
                           reinterpret_cast<uchar *>(ibuf->byte_buffer.data);
  void *ibuf_data = rect + ((stride_t(height) - 1) * ibuf_ystride);

  bool ok = in->read_image(
      0, 0, 0, channels, format, ibuf_data, ibuf_xstride, -ibuf_ystride, AutoStride);
  if (!ok) {
    CLOG_ERROR(&LOG_READ, "OpenImageIO read failed: %s", in->geterror().c_str());

    IMB_freeImBuf(ibuf);
    return nullptr;
  }

  /* ImBuf always needs 4 channels */
  const T alpha_fill = is_float ? 1.0f : 0xFF;
  fill_all_channels<T>(reinterpret_cast<T *>(rect), width, height, channels, alpha_fill);

  return ibuf;
}

static void set_file_colorspace(ImFileColorSpace &r_colorspace,
                                const ReadContext &ctx,
                                const ImageSpec &spec,
                                bool is_float)
{
  /* Guess float data types means HDR colors. File formats can override this later. */
  r_colorspace.is_hdr_float = is_float;

  /* Override if necessary. */
  if (ctx.use_metadata_colorspace) {
    string ics = spec.get_string_attribute("oiio:ColorSpace");
    STRNCPY_UTF8(r_colorspace.metadata_colorspace, ics.c_str());
  }

  /* Get colorspace from CICP. */
  int cicp[4] = {};
  if (spec.getattribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp, true)) {
    const ColorSpace *colorspace = IMB_colormanagement_space_from_cicp(
        cicp, ColorManagedFileOutput::Image);
    if (colorspace) {
      STRNCPY_UTF8(r_colorspace.metadata_colorspace,
                   IMB_colormanagement_colorspace_get_name(colorspace));
    }
  }
}

/**
 * Get an #ImBuf filled in with pixel data and associated metadata using the provided ImageInput.
 */
static ImBuf *get_oiio_ibuf(ImageInput *in, const ReadContext &ctx, ImFileColorSpace &r_colorspace)
{
  const ImageSpec &spec = in->spec();
  const int width = spec.width;
  const int height = spec.height;
  const bool has_alpha = spec.alpha_channel != -1;
  const bool is_float = spec.format.basesize() > 1;

  /* Only a maximum of 4 channels are supported by ImBuf. */
  const int channels = spec.nchannels <= 4 ? spec.nchannels : 4;
  if (channels < 1) {
    return nullptr;
  }

  const bool use_all_planes = has_alpha || ctx.use_all_planes;

  ImBuf *ibuf = nullptr;
  if (is_float) {
    ibuf = load_pixels<float>(in, width, height, channels, ctx.flags, use_all_planes);
  }
  else {
    ibuf = load_pixels<uchar>(in, width, height, channels, ctx.flags, use_all_planes);
  }

  /* Fill in common ibuf properties. */
  if (ibuf) {
    ibuf->ftype = ctx.file_type;
    ibuf->foptions.flag |= (spec.format == TypeDesc::HALF) ? OPENEXR_HALF : 0;

    set_file_colorspace(r_colorspace, ctx, spec, is_float);

    double x_res = spec.get_float_attribute("XResolution", 0.0f);
    double y_res = spec.get_float_attribute("YResolution", 0.0f);
    /* Some formats store the resolution as integers. */
    if (!(x_res > 0.0f && y_res > 0.0f)) {
      x_res = spec.get_int_attribute("XResolution", 0);
      y_res = spec.get_int_attribute("YResolution", 0);
    }

    if (x_res > 0.0f && y_res > 0.0f) {
      double scale = 1.0;
      auto unit = spec.get_string_attribute("ResolutionUnit", "");
      if (ELEM(unit, "in", "inch")) {
        scale = 100.0 / 2.54;
      }
      else if (unit == "cm") {
        scale = 100.0;
      }
      ibuf->ppm[0] = scale * x_res;
      ibuf->ppm[1] = scale * y_res;
    }

    /* Transfer metadata to the ibuf if necessary. */
    if (ctx.flags & IB_metadata) {
      IMB_metadata_ensure(&ibuf->metadata);
      ibuf->flags |= spec.extra_attribs.empty() ? 0 : IB_metadata;

      for (const auto &attrib : spec.extra_attribs) {
        if (attrib.name().find("ICCProfile") != string::npos) {
          continue;
        }
        IMB_metadata_set_field(ibuf->metadata, attrib.name().c_str(), attrib.get_string().c_str());
      }
    }
  }

  return ibuf;
}

/**
 * Returns an #ImageInput for the precise `format` requested using the provided #IOMemReader.
 * If successful, the #ImageInput will be opened and ready for operations. Null will be returned if
 * the format was not found or if the open call fails.
 */
static unique_ptr<ImageInput> get_oiio_reader(const char *format,
                                              const ImageSpec &config,
                                              Filesystem::IOMemReader &mem_reader,
                                              ImageSpec &r_newspec)
{
  /* Attempt to create a reader based on the passed in format. */
  unique_ptr<ImageInput> in = ImageInput::create(format);
  if (!(in && in->valid_file(&mem_reader))) {
    return nullptr;
  }

  /* Open the reader using the ioproxy. */
  in->set_ioproxy(&mem_reader);
  bool ok = in->open("", r_newspec, config);
  if (!ok) {
    return nullptr;
  }

  return in;
}

bool imb_oiio_check(const uchar *mem, size_t mem_size, const char *file_format)
{
  ImageSpec config, spec;

  /* This memory proxy must remain alive for the full duration of the read. */
  Filesystem::IOMemReader mem_reader(cspan<uchar>(mem, mem_size));
  unique_ptr<ImageInput> in = ImageInput::create(file_format);
  return in && in->valid_file(&mem_reader);
}

ImBuf *imb_oiio_read(const ReadContext &ctx,
                     const ImageSpec &config,
                     ImFileColorSpace &r_colorspace,
                     ImageSpec &r_newspec)
{
  /* This memory proxy must remain alive for the full duration of the read. */
  Filesystem::IOMemReader mem_reader(cspan<uchar>(ctx.mem_start, ctx.mem_size));
  unique_ptr<ImageInput> in = get_oiio_reader(ctx.file_format, config, mem_reader, r_newspec);
  if (!in) {
    return nullptr;
  }

  return get_oiio_ibuf(in.get(), ctx, r_colorspace);
}

bool imb_oiio_write(const WriteContext &ctx, const char *filepath, const ImageSpec &file_spec)
{
  unique_ptr<ImageOutput> out = ImageOutput::create(ctx.file_format);
  if (!out) {
    return false;
  }

  ImageBuf orig_buf(ctx.mem_spec, ctx.mem_start, ctx.mem_xstride, -ctx.mem_ystride, AutoStride);
  ImageBuf final_buf{};

#if OIIO_VERSION_MAJOR >= 3
  const size_t original_channels_count = orig_buf.nchannels();
#else
  const int original_channels_count = orig_buf.nchannels();
#endif

  if (original_channels_count > 1 && file_spec.nchannels == 1) {
    /* Convert to gray-scale image by computing the luminance. Make sure the weight of alpha
     * channel is zero since it should not contribute to the luminance. */
    float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    IMB_colormanagement_get_luminance_coefficients(weights);
    ImageBufAlgo::channel_sum(final_buf, orig_buf, {weights, original_channels_count});
  }
  else if (original_channels_count == 1 && file_spec.nchannels > 1) {
    /* Broadcast the gray-scale channel to as many channels as needed, filling the alpha channel
     * with ones if needed. 0 channel order mean we will be copying from the first channel, while
     * -1 means we will be filling based on the corresponding value from the defined channel
     * values. */
    const int channel_order[] = {0, 0, 0, -1};
    const float channel_values[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const std::string channel_names[] = {"R", "G", "B", "A"};
    ImageBufAlgo::channels(final_buf,
                           orig_buf,
                           file_spec.nchannels,
                           cspan<int>(channel_order, file_spec.nchannels),
                           cspan<float>(channel_values, file_spec.nchannels),
                           cspan<std::string>(channel_names, file_spec.nchannels));
  }
  else if (original_channels_count != file_spec.nchannels) {
    /* Either trim or fill new channels based on the needed channels count. */
    int channel_order[4];
    for (int i = 0; i < 4; i++) {
      /* If a channel exists in the original buffer, we copy it, if not, we fill it by supplying
       * -1, which is a special value that means filling based on the value in the defined channels
       * values. So alpha is filled with 1, and other channels are filled with zero. */
      const bool channel_exists = i + 1 <= original_channels_count;
      channel_order[i] = channel_exists ? i : -1;
    }
    const float channel_values[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const std::string channel_names[] = {"R", "G", "B", "A"};
    ImageBufAlgo::channels(final_buf,
                           orig_buf,
                           file_spec.nchannels,
                           cspan<int>(channel_order, file_spec.nchannels),
                           cspan<float>(channel_values, file_spec.nchannels),
                           cspan<std::string>(channel_names, file_spec.nchannels));
  }
  else {
    final_buf = std::move(orig_buf);
  }

  bool write_ok = false;
  bool close_ok = false;
  if (ctx.flags & IB_mem) {
    /* This memory proxy must remain alive until the ImageOutput is finally closed. */
    ImBufMemWriter writer(ctx.ibuf);

    imb_addencodedbufferImBuf(ctx.ibuf);
    out->set_ioproxy(&writer);
    if (out->open("", file_spec)) {
      write_ok = final_buf.write(out.get());
      close_ok = out->close();
    }
  }
  else {
    if (out->open(filepath, file_spec)) {
      write_ok = final_buf.write(out.get());
      close_ok = out->close();
    }
  }

  const bool all_ok = write_ok && close_ok;
  if (!all_ok) {
    CLOG_ERROR(&LOG_WRITE, "OpenImageIO write failed: %s", out->geterror().c_str());
    errno = 0; /* Prevent higher level layers from calling `perror` unnecessarily. */
  }

  return all_ok;
}

WriteContext imb_create_write_context(const char *file_format,
                                      ImBuf *ibuf,
                                      int flags,
                                      bool prefer_float)
{
  WriteContext ctx{};
  ctx.file_format = file_format;
  ctx.ibuf = ibuf;
  ctx.flags = flags;

  const int width = ibuf->x;
  const int height = ibuf->y;
  const bool use_float = prefer_float && (ibuf->float_buffer.data != nullptr);
  if (use_float) {
    const int mem_channels = ibuf->channels ? ibuf->channels : 4;
    ctx.mem_xstride = sizeof(float) * mem_channels;
    ctx.mem_ystride = width * ctx.mem_xstride;
    ctx.mem_start = reinterpret_cast<uchar *>(ibuf->float_buffer.data);
    ctx.mem_spec = ImageSpec(width, height, mem_channels, TypeDesc::FLOAT);
  }
  else {
    const int mem_channels = 4;
    ctx.mem_xstride = sizeof(uchar) * mem_channels;
    ctx.mem_ystride = width * ctx.mem_xstride;
    ctx.mem_start = ibuf->byte_buffer.data;
    ctx.mem_spec = ImageSpec(width, height, mem_channels, TypeDesc::UINT8);
  }

  /* We always write using a negative y-stride so ensure we start at the end. */
  ctx.mem_start = ctx.mem_start + ((stride_t(height) - 1) * ctx.mem_ystride);

  return ctx;
}

ImageSpec imb_create_write_spec(const WriteContext &ctx, int file_channels, TypeDesc data_format)
{
  const int width = ctx.ibuf->x;
  const int height = ctx.ibuf->y;
  ImageSpec file_spec(width, height, file_channels, data_format);

  /* Populate the spec with all common attributes.
   *
   * Care must be taken with the metadata:
   * - It should be processed first, before the "Resolution" metadata below, to
   *   ensure the proper values end up in the #ImageSpec
   * - It needs to filter format-specific metadata that may no longer apply to
   *   the current format being written (e.g. metadata for tiff being written to a `PNG`)
   */

  if (ctx.ibuf->metadata) {
    LISTBASE_FOREACH (IDProperty *, prop, &ctx.ibuf->metadata->data.group) {
      if (prop->type == IDP_STRING) {
        /* If this property has a prefixed name (oiio:, tiff:, etc.) and it belongs to
         * oiio or a different format, then skip. */
        if (char *colon = strchr(prop->name, ':')) {
          std::string prefix(prop->name, colon);
          Strutil::to_lower(prefix);
          if (prefix == "oiio" ||
              (!STREQ(prefix.c_str(), ctx.file_format) && OIIO::is_imageio_format_name(prefix)))
          {
            /* Skip this attribute. */
            continue;
          }
        }

        file_spec.attribute(prop->name, IDP_string_get(prop));
      }
    }
  }

  if (ctx.ibuf->ppm[0] > 0.0 && ctx.ibuf->ppm[1] > 0.0) {
    if (STREQ(ctx.file_format, "bmp")) {
      /* BMP only supports meters as integers. */
      file_spec.attribute("ResolutionUnit", "m");
      file_spec.attribute("XResolution", int(round(ctx.ibuf->ppm[0])));
      file_spec.attribute("YResolution", int(round(ctx.ibuf->ppm[1])));
    }
    else {
      /* More OIIO formats support inch than meter. */
      file_spec.attribute("ResolutionUnit", "in");
      file_spec.attribute("XResolution", float(ctx.ibuf->ppm[0] * 0.0254));
      file_spec.attribute("YResolution", float(ctx.ibuf->ppm[1] * 0.0254));
    }
  }

  /* Write ICC profile and/or CICP if there is one associated with the colorspace. */
  const ColorSpace *colorspace = (ctx.mem_spec.format == TypeDesc::FLOAT) ?
                                     ctx.ibuf->float_buffer.colorspace :
                                     ctx.ibuf->byte_buffer.colorspace;
  if (colorspace) {
    Vector<char> icc_profile = IMB_colormanagement_space_to_icc_profile(colorspace);
    if (!icc_profile.is_empty()) {
      file_spec.attribute("ICCProfile",
                          OIIO::TypeDesc(OIIO::TypeDesc::UINT8, icc_profile.size()),
                          icc_profile.data());
    }

    /* PNG only supports RGB matrix. For AVIF and HEIF we want to use a YUV matrix
     * as these are based on video codecs designed to use them. */
    const bool rgb_matrix = STREQ(ctx.file_format, "png");
    int cicp[4];
    if (IMB_colormanagement_space_to_cicp(
            colorspace, ColorManagedFileOutput::Image, rgb_matrix, cicp))
    {
      file_spec.attribute("CICP", TypeDesc(TypeDesc::INT, 4), cicp);
    }
  }

  return file_spec;
}

}  // namespace blender::imbuf

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright by Gernot Ziegler <gz@lysator.liu.se>. All rights reserved. */

/** \file
 * \ingroup openexr
 */

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

/* The OpenEXR version can reliably be found in this header file from OpenEXR,
 * for both 2.x and 3.x:
 */
#include <OpenEXR/OpenEXRConfig.h>
#define COMBINED_OPENEXR_VERSION \
  ((10000 * OPENEXR_VERSION_MAJOR) + (100 * OPENEXR_VERSION_MINOR) + OPENEXR_VERSION_PATCH)

#if COMBINED_OPENEXR_VERSION >= 20599
/* >=2.5.99 -> OpenEXR >=3.0 */
#  include <Imath/half.h>
#  include <OpenEXR/ImfFrameBuffer.h>
#  define exr_file_offset_t uint64_t
#else
/* OpenEXR 2.x, use the old locations. */
#  include <OpenEXR/half.h>
#  define exr_file_offset_t Int64
#endif

#include <OpenEXR/Iex.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include <OpenEXR/ImfIO.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfPixelType.h>
#include <OpenEXR/ImfPreviewImage.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfStandardAttributes.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfVersion.h>

/* multiview/multipart */
#include <OpenEXR/ImfInputPart.h>
#include <OpenEXR/ImfMultiPartInputFile.h>
#include <OpenEXR/ImfMultiPartOutputFile.h>
#include <OpenEXR/ImfMultiView.h>
#include <OpenEXR/ImfOutputPart.h>
#include <OpenEXR/ImfPartHelper.h>
#include <OpenEXR/ImfPartType.h>
#include <OpenEXR/ImfTiledOutputPart.h>

#include "DNA_scene_types.h" /* For OpenEXR compression constants */

#include <openexr_api.h>

#if defined(WIN32)
#  include "utfconv.h"
#  include <io.h>
#else
#  include <unistd.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_fileops.h"
#include "BLI_math_color.h"
#include "BLI_mmap.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"

#include "BKE_idprop.h"
#include "BKE_image.h"

#include "IMB_allocimbuf.h"
#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"
#include "IMB_openexr.h"

using namespace Imf;
using namespace Imath;

extern "C" {
/* prototype */
static struct ExrPass *imb_exr_get_pass(ListBase *lb, char *passname);
static bool exr_has_multiview(MultiPartInputFile &file);
static bool exr_has_multipart_file(MultiPartInputFile &file);
static bool exr_has_alpha(MultiPartInputFile &file);
static bool exr_has_zbuffer(MultiPartInputFile &file);
static void exr_printf(const char *__restrict fmt, ...);
static void imb_exr_type_by_channels(ChannelList &channels,
                                     StringVector &views,
                                     bool *r_singlelayer,
                                     bool *r_multilayer,
                                     bool *r_multiview);
}

/* Memory Input Stream */

class IMemStream : public Imf::IStream {
 public:
  IMemStream(uchar *exrbuf, size_t exrsize) : IStream("<memory>"), _exrpos(0), _exrsize(exrsize)
  {
    _exrbuf = exrbuf;
  }

  bool read(char c[], int n) override
  {
    if (n + _exrpos <= _exrsize) {
      memcpy(c, (void *)(&_exrbuf[_exrpos]), n);
      _exrpos += n;
      return true;
    }

    return false;
  }

  exr_file_offset_t tellg() override
  {
    return _exrpos;
  }

  void seekg(exr_file_offset_t pos) override
  {
    _exrpos = pos;
  }

  void clear() override {}

 private:
  exr_file_offset_t _exrpos;
  exr_file_offset_t _exrsize;
  uchar *_exrbuf;
};

/* Memory-Mapped Input Stream */

class IMMapStream : public Imf::IStream {
 public:
  IMMapStream(const char *filepath) : IStream(filepath)
  {
    int file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
    if (file < 0) {
      throw IEX_NAMESPACE::InputExc("file not found");
    }
    _exrpos = 0;
    _exrsize = BLI_file_descriptor_size(file);
    imb_mmap_lock();
    _mmap_file = BLI_mmap_open(file);
    imb_mmap_unlock();
    if (_mmap_file == nullptr) {
      throw IEX_NAMESPACE::InputExc("BLI_mmap_open failed");
    }
    close(file);
    _exrbuf = (uchar *)BLI_mmap_get_pointer(_mmap_file);
  }

  ~IMMapStream() override
  {
    imb_mmap_lock();
    BLI_mmap_free(_mmap_file);
    imb_mmap_unlock();
  }

  /* This is implementing regular `read`, not `readMemoryMapped`, because DWAA and DWAB
   * decompressors load on unaligned offsets. Therefore we can't avoid the memory copy. */

  bool read(char c[], int n) override
  {
    if (_exrpos + n > _exrsize) {
      throw Iex::InputExc("Unexpected end of file.");
    }
    memcpy(c, _exrbuf + _exrpos, n);
    _exrpos += n;

    return _exrpos < _exrsize;
  }

  exr_file_offset_t tellg() override
  {
    return _exrpos;
  }

  void seekg(exr_file_offset_t pos) override
  {
    _exrpos = pos;
  }

 private:
  BLI_mmap_file *_mmap_file;
  exr_file_offset_t _exrpos;
  exr_file_offset_t _exrsize;
  uchar *_exrbuf;
};

/* File Input Stream */

class IFileStream : public Imf::IStream {
 public:
  IFileStream(const char *filepath) : IStream(filepath)
  {
    /* utf-8 file path support on windows */
#if defined(WIN32)
    wchar_t *wfilepath = alloc_utf16_from_8(filepath, 0);
    ifs.open(wfilepath, std::ios_base::binary);
    free(wfilepath);
#else
    ifs.open(filepath, std::ios_base::binary);
#endif

    if (!ifs) {
      Iex::throwErrnoExc();
    }
  }

  bool read(char c[], int n) override
  {
    if (!ifs) {
      throw Iex::InputExc("Unexpected end of file.");
    }

    errno = 0;
    ifs.read(c, n);
    return check_error();
  }

  exr_file_offset_t tellg() override
  {
    return std::streamoff(ifs.tellg());
  }

  void seekg(exr_file_offset_t pos) override
  {
    ifs.seekg(pos);
    check_error();
  }

  void clear() override
  {
    ifs.clear();
  }

 private:
  bool check_error()
  {
    if (!ifs) {
      if (errno) {
        Iex::throwErrnoExc();
      }

      return false;
    }

    return true;
  }

  std::ifstream ifs;
};

/* Memory Output Stream */

class OMemStream : public OStream {
 public:
  OMemStream(ImBuf *ibuf_) : OStream("<memory>"), ibuf(ibuf_), offset(0) {}

  void write(const char c[], int n) override
  {
    ensure_size(offset + n);
    memcpy(ibuf->encodedbuffer + offset, c, n);
    offset += n;
    ibuf->encodedsize += n;
  }

  exr_file_offset_t tellp() override
  {
    return offset;
  }

  void seekp(exr_file_offset_t pos) override
  {
    offset = pos;
    ensure_size(offset);
  }

 private:
  void ensure_size(exr_file_offset_t size)
  {
    /* if buffer is too small increase it. */
    while (size > ibuf->encodedbuffersize) {
      if (!imb_enlargeencodedbufferImBuf(ibuf)) {
        throw Iex::ErrnoExc("Out of memory.");
      }
    }
  }

  ImBuf *ibuf;
  exr_file_offset_t offset;
};

/* File Output Stream */

class OFileStream : public OStream {
 public:
  OFileStream(const char *filepath) : OStream(filepath)
  {
    /* utf-8 file path support on windows */
#if defined(WIN32)
    wchar_t *wfilepath = alloc_utf16_from_8(filepath, 0);
    ofs.open(wfilepath, std::ios_base::binary);
    free(wfilepath);
#else
    ofs.open(filepath, std::ios_base::binary);
#endif

    if (!ofs) {
      Iex::throwErrnoExc();
    }
  }

  void write(const char c[], int n) override
  {
    errno = 0;
    ofs.write(c, n);
    check_error();
  }

  exr_file_offset_t tellp() override
  {
    return std::streamoff(ofs.tellp());
  }

  void seekp(exr_file_offset_t pos) override
  {
    ofs.seekp(pos);
    check_error();
  }

 private:
  void check_error()
  {
    if (!ofs) {
      if (errno) {
        Iex::throwErrnoExc();
      }

      throw Iex::ErrnoExc("File output failed.");
    }
  }

  std::ofstream ofs;
};

struct _RGBAZ {
  half r;
  half g;
  half b;
  half a;
  half z;
};

using RGBAZ = _RGBAZ;

static half float_to_half_safe(const float value)
{
  return half(clamp_f(value, -HALF_MAX, HALF_MAX));
}

extern "C" {

bool imb_is_a_openexr(const uchar *mem, const size_t size)
{
  /* No define is exposed for this size. */
  if (size < 4) {
    return false;
  }
  return Imf::isImfMagic((const char *)mem);
}

static void openexr_header_compression(Header *header, int compression)
{
  switch (compression) {
    case R_IMF_EXR_CODEC_NONE:
      header->compression() = NO_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_PXR24:
      header->compression() = PXR24_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_ZIP:
      header->compression() = ZIP_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_PIZ:
      header->compression() = PIZ_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_RLE:
      header->compression() = RLE_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_ZIPS:
      header->compression() = ZIPS_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_B44:
      header->compression() = B44_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_B44A:
      header->compression() = B44A_COMPRESSION;
      break;
#if OPENEXR_VERSION_MAJOR > 2 || (OPENEXR_VERSION_MAJOR >= 2 && OPENEXR_VERSION_MINOR >= 2)
    case R_IMF_EXR_CODEC_DWAA:
      header->compression() = DWAA_COMPRESSION;
      break;
    case R_IMF_EXR_CODEC_DWAB:
      header->compression() = DWAB_COMPRESSION;
      break;
#endif
    default:
      header->compression() = ZIP_COMPRESSION;
      break;
  }
}

static void openexr_header_metadata(Header *header, struct ImBuf *ibuf)
{
  if (ibuf->metadata) {
    IDProperty *prop;

    for (prop = (IDProperty *)ibuf->metadata->data.group.first; prop; prop = prop->next) {
      if (prop->type == IDP_STRING && !STREQ(prop->name, "compression")) {
        header->insert(prop->name, StringAttribute(IDP_String(prop)));
      }
    }
  }

  if (ibuf->ppm[0] > 0.0) {
    /* Convert meters to inches. */
    addXDensity(*header, ibuf->ppm[0] * 0.0254);
  }
}

static void openexr_header_metadata_callback(void *data,
                                             const char *propname,
                                             char *prop,
                                             int /*len*/)
{
  Header *header = (Header *)data;
  header->insert(propname, StringAttribute(prop));
}

static bool imb_save_openexr_half(ImBuf *ibuf, const char *filepath, const int flags)
{
  const int channels = ibuf->channels;
  const bool is_alpha = (channels >= 4) && (ibuf->planes == 32);
  const bool is_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != nullptr; /* summarize */
  const int width = ibuf->x;
  const int height = ibuf->y;
  OStream *file_stream = nullptr;

  try {
    Header header(width, height);

    openexr_header_compression(&header, ibuf->foptions.flag & OPENEXR_COMPRESS);
    openexr_header_metadata(&header, ibuf);

    /* create channels */
    header.channels().insert("R", Channel(HALF));
    header.channels().insert("G", Channel(HALF));
    header.channels().insert("B", Channel(HALF));
    if (is_alpha) {
      header.channels().insert("A", Channel(HALF));
    }
    if (is_zbuf) {
      /* z we do as float always */
      header.channels().insert("Z", Channel(Imf::FLOAT));
    }

    FrameBuffer frameBuffer;

    /* manually create ofstream, so we can handle utf-8 filepaths on windows */
    if (flags & IB_mem) {
      file_stream = new OMemStream(ibuf);
    }
    else {
      file_stream = new OFileStream(filepath);
    }
    OutputFile file(*file_stream, header);

    /* we store first everything in half array */
    std::vector<RGBAZ> pixels(height * width);
    RGBAZ *to = pixels.data();
    int xstride = sizeof(RGBAZ);
    int ystride = xstride * width;

    /* indicate used buffers */
    frameBuffer.insert("R", Slice(HALF, (char *)&to->r, xstride, ystride));
    frameBuffer.insert("G", Slice(HALF, (char *)&to->g, xstride, ystride));
    frameBuffer.insert("B", Slice(HALF, (char *)&to->b, xstride, ystride));
    if (is_alpha) {
      frameBuffer.insert("A", Slice(HALF, (char *)&to->a, xstride, ystride));
    }
    if (is_zbuf) {
      frameBuffer.insert("Z",
                         Slice(Imf::FLOAT,
                               (char *)(ibuf->zbuf_float + (height - 1) * width),
                               sizeof(float),
                               sizeof(float) * -width));
    }
    if (ibuf->rect_float) {
      float *from;

      for (int i = ibuf->y - 1; i >= 0; i--) {
        from = ibuf->rect_float + channels * i * width;

        for (int j = ibuf->x; j > 0; j--) {
          to->r = float_to_half_safe(from[0]);
          to->g = float_to_half_safe((channels >= 2) ? from[1] : from[0]);
          to->b = float_to_half_safe((channels >= 3) ? from[2] : from[0]);
          to->a = float_to_half_safe((channels >= 4) ? from[3] : 1.0f);
          to++;
          from += channels;
        }
      }
    }
    else {
      uchar *from;

      for (int i = ibuf->y - 1; i >= 0; i--) {
        from = (uchar *)ibuf->rect + 4 * i * width;

        for (int j = ibuf->x; j > 0; j--) {
          to->r = srgb_to_linearrgb(float(from[0]) / 255.0f);
          to->g = srgb_to_linearrgb(float(from[1]) / 255.0f);
          to->b = srgb_to_linearrgb(float(from[2]) / 255.0f);
          to->a = channels >= 4 ? float(from[3]) / 255.0f : 1.0f;
          to++;
          from += 4;
        }
      }
    }

    exr_printf("OpenEXR-save: Writing OpenEXR file of height %d.\n", height);

    file.setFrameBuffer(frameBuffer);
    file.writePixels(height);
  }
  catch (const std::exception &exc) {
    delete file_stream;
    printf("OpenEXR-save: ERROR: %s\n", exc.what());

    return false;
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    delete file_stream;
    printf("OpenEXR-save: UNKNOWN ERROR\n");

    return false;
  }

  delete file_stream;
  return true;
}

static bool imb_save_openexr_float(ImBuf *ibuf, const char *filepath, const int flags)
{
  const int channels = ibuf->channels;
  const bool is_alpha = (channels >= 4) && (ibuf->planes == 32);
  const bool is_zbuf = (flags & IB_zbuffloat) && ibuf->zbuf_float != nullptr; /* summarize */
  const int width = ibuf->x;
  const int height = ibuf->y;
  OStream *file_stream = nullptr;

  try {
    Header header(width, height);

    openexr_header_compression(&header, ibuf->foptions.flag & OPENEXR_COMPRESS);
    openexr_header_metadata(&header, ibuf);

    /* create channels */
    header.channels().insert("R", Channel(Imf::FLOAT));
    header.channels().insert("G", Channel(Imf::FLOAT));
    header.channels().insert("B", Channel(Imf::FLOAT));
    if (is_alpha) {
      header.channels().insert("A", Channel(Imf::FLOAT));
    }
    if (is_zbuf) {
      header.channels().insert("Z", Channel(Imf::FLOAT));
    }

    FrameBuffer frameBuffer;

    /* manually create ofstream, so we can handle utf-8 filepaths on windows */
    if (flags & IB_mem) {
      file_stream = new OMemStream(ibuf);
    }
    else {
      file_stream = new OFileStream(filepath);
    }
    OutputFile file(*file_stream, header);

    int xstride = sizeof(float) * channels;
    int ystride = -xstride * width;

    /* Last scan-line, stride negative. */
    float *rect[4] = {nullptr, nullptr, nullptr, nullptr};
    rect[0] = ibuf->rect_float + channels * (height - 1) * width;
    rect[1] = (channels >= 2) ? rect[0] + 1 : rect[0];
    rect[2] = (channels >= 3) ? rect[0] + 2 : rect[0];
    rect[3] = (channels >= 4) ?
                  rect[0] + 3 :
                  rect[0]; /* red as alpha, is this needed since alpha isn't written? */

    frameBuffer.insert("R", Slice(Imf::FLOAT, (char *)rect[0], xstride, ystride));
    frameBuffer.insert("G", Slice(Imf::FLOAT, (char *)rect[1], xstride, ystride));
    frameBuffer.insert("B", Slice(Imf::FLOAT, (char *)rect[2], xstride, ystride));
    if (is_alpha) {
      frameBuffer.insert("A", Slice(Imf::FLOAT, (char *)rect[3], xstride, ystride));
    }
    if (is_zbuf) {
      frameBuffer.insert("Z",
                         Slice(Imf::FLOAT,
                               (char *)(ibuf->zbuf_float + (height - 1) * width),
                               sizeof(float),
                               sizeof(float) * -width));
    }

    file.setFrameBuffer(frameBuffer);
    file.writePixels(height);
  }
  catch (const std::exception &exc) {
    printf("OpenEXR-save: ERROR: %s\n", exc.what());
    delete file_stream;
    return false;
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    printf("OpenEXR-save: UNKNOWN ERROR\n");
    delete file_stream;
    return false;
  }

  delete file_stream;
  return true;
}

bool imb_save_openexr(struct ImBuf *ibuf, const char *filepath, int flags)
{
  if (flags & IB_mem) {
    imb_addencodedbufferImBuf(ibuf);
    ibuf->encodedsize = 0;
  }

  if (ibuf->foptions.flag & OPENEXR_HALF) {
    return imb_save_openexr_half(ibuf, filepath, flags);
  }

  /* when no float rect, we save as half (16 bits is sufficient) */
  if (ibuf->rect_float == nullptr) {
    return imb_save_openexr_half(ibuf, filepath, flags);
  }

  return imb_save_openexr_float(ibuf, filepath, flags);
}

/* ******* Nicer API, MultiLayer and with Tile file support ************************************ */

/* naming rules:
 * - parse name from right to left
 * - last character is channel ID, 1 char like 'A' 'R' 'G' 'B' 'X' 'Y' 'Z' 'W' 'U' 'V'
 * - separated with a dot; the Pass name (like "Depth", "Color", "Diffuse" or "Combined")
 * - separated with a dot: the Layer name (like "Light1" or "Walls" or "Characters")
 */

static ListBase exrhandles = {nullptr, nullptr};

struct ExrHandle {
  struct ExrHandle *next, *prev;
  char name[FILE_MAX];

  IStream *ifile_stream;
  MultiPartInputFile *ifile;

  OFileStream *ofile_stream;
  MultiPartOutputFile *mpofile;
  OutputFile *ofile;

  int tilex, tiley;
  int width, height;
  int mipmap;

  /** It needs to be a pointer due to Windows release builds of EXR2.0
   * segfault when opening EXR bug. */
  StringVector *multiView;

  int parts;

  ListBase channels; /* flattened out, ExrChannel */
  ListBase layers;   /* hierarchical, pointing in end to ExrChannel */

  /** Used during file save, allows faster temporary buffers allocation. */
  int num_half_channels;
};

/* flattened out channel */
struct ExrChannel {
  struct ExrChannel *next, *prev;

  char name[EXR_TOT_MAXNAME + 1]; /* full name with everything */
  struct MultiViewChannelName *m; /* struct to store all multipart channel info */
  int xstride, ystride;           /* step to next pixel, to next scan-line. */
  float *rect;                    /* first pointer to write in */
  char chan_id;                   /* quick lookup of channel char */
  int view_id;                    /* quick lookup of channel view */
  bool use_half_float;            /* when saving use half float for file storage */
};

/* hierarchical; layers -> passes -> channels[] */
struct ExrPass {
  struct ExrPass *next, *prev;
  char name[EXR_PASS_MAXNAME];
  int totchan;
  float *rect;
  struct ExrChannel *chan[EXR_PASS_MAXCHAN];
  char chan_id[EXR_PASS_MAXCHAN];

  char internal_name[EXR_PASS_MAXNAME]; /* name with no view */
  char view[EXR_VIEW_MAXNAME];
  int view_id;
};

struct ExrLayer {
  struct ExrLayer *next, *prev;
  char name[EXR_LAY_MAXNAME + 1];
  ListBase passes;
};

static bool imb_exr_multilayer_parse_channels_from_file(ExrHandle *data);

/* ********************** */

void *IMB_exr_get_handle(void)
{
  ExrHandle *data = MEM_cnew<ExrHandle>("exr handle");
  data->multiView = new StringVector();

  BLI_addtail(&exrhandles, data);
  return data;
}

void *IMB_exr_get_handle_name(const char *name)
{
  ExrHandle *data = (ExrHandle *)BLI_rfindstring(&exrhandles, name, offsetof(ExrHandle, name));

  if (data == nullptr) {
    data = (ExrHandle *)IMB_exr_get_handle();
    BLI_strncpy(data->name, name, strlen(name) + 1);
  }
  return data;
}

/* multiview functions */
} /* extern "C" */

extern "C" {

void IMB_exr_add_view(void *handle, const char *name)
{
  ExrHandle *data = (ExrHandle *)handle;
  data->multiView->push_back(name);
}

static int imb_exr_get_multiView_id(StringVector &views, const std::string &name)
{
  int count = 0;
  for (StringVector::const_iterator i = views.begin(); count < views.size(); ++i) {
    if (name == *i) {
      return count;
    }

    count++;
  }

  /* no views or wrong name */
  return -1;
}

static void imb_exr_get_views(MultiPartInputFile &file, StringVector &views)
{
  if (exr_has_multipart_file(file) == false) {
    if (exr_has_multiview(file)) {
      StringVector sv = multiView(file.header(0));
      for (const std::string &view_name : sv) {
        views.push_back(view_name);
      }
    }
  }

  else {
    for (int p = 0; p < file.parts(); p++) {
      std::string view;
      if (file.header(p).hasView()) {
        view = file.header(p).view();
      }

      if (imb_exr_get_multiView_id(views, view) == -1) {
        views.push_back(view);
      }
    }
  }
}

/* Multi-layer Blender files have the view name in all the passes (even the default view one). */
static void imb_exr_insert_view_name(char *name_full, const char *passname, const char *viewname)
{
  BLI_assert(!ELEM(name_full, passname, viewname));

  if (viewname == nullptr || viewname[0] == '\0') {
    BLI_strncpy(name_full, passname, sizeof(ExrChannel::name));
    return;
  }

  const char delims[] = {'.', '\0'};
  const char *sep;
  const char *token;
  size_t len;

  len = BLI_str_rpartition(passname, delims, &sep, &token);

  if (sep) {
    BLI_snprintf(name_full, EXR_PASS_MAXNAME, "%.*s.%s.%s", int(len), passname, viewname, token);
  }
  else {
    BLI_snprintf(name_full, EXR_PASS_MAXNAME, "%s.%s", passname, viewname);
  }
}

void IMB_exr_add_channel(void *handle,
                         const char *layname,
                         const char *passname,
                         const char *viewname,
                         int xstride,
                         int ystride,
                         float *rect,
                         bool use_half_float)
{
  ExrHandle *data = (ExrHandle *)handle;
  ExrChannel *echan;

  echan = MEM_cnew<ExrChannel>("exr channel");
  echan->m = new MultiViewChannelName();

  if (layname && layname[0] != '\0') {
    echan->m->name = layname;
    echan->m->name.append(".");
    echan->m->name.append(passname);
  }
  else {
    echan->m->name.assign(passname);
  }

  echan->m->internal_name = echan->m->name;

  echan->m->view.assign(viewname ? viewname : "");

  /* quick look up */
  echan->view_id = std::max(0, imb_exr_get_multiView_id(*data->multiView, echan->m->view));

  /* name has to be unique, thus it's a combination of layer, pass, view, and channel */
  if (layname && layname[0] != '\0') {
    imb_exr_insert_view_name(echan->name, echan->m->name.c_str(), echan->m->view.c_str());
  }
  else if (!data->multiView->empty()) {
    std::string raw_name = insertViewName(echan->m->name, *data->multiView, echan->view_id);
    STRNCPY(echan->name, raw_name.c_str());
  }
  else {
    STRNCPY(echan->name, echan->m->name.c_str());
  }

  echan->xstride = xstride;
  echan->ystride = ystride;
  echan->rect = rect;
  echan->use_half_float = use_half_float;

  if (echan->use_half_float) {
    data->num_half_channels++;
  }

  exr_printf("added channel %s\n", echan->name);
  BLI_addtail(&data->channels, echan);
}

bool IMB_exr_begin_write(void *handle,
                         const char *filepath,
                         int width,
                         int height,
                         int compress,
                         const StampData *stamp)
{
  ExrHandle *data = (ExrHandle *)handle;
  Header header(width, height);
  ExrChannel *echan;

  data->width = width;
  data->height = height;

  bool is_singlelayer, is_multilayer, is_multiview;

  for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
    header.channels().insert(echan->name, Channel(echan->use_half_float ? Imf::HALF : Imf::FLOAT));
  }

  openexr_header_compression(&header, compress);
  BKE_stamp_info_callback(
      &header, const_cast<StampData *>(stamp), openexr_header_metadata_callback, false);
  /* header.lineOrder() = DECREASING_Y; this crashes in windows for file read! */

  imb_exr_type_by_channels(
      header.channels(), *data->multiView, &is_singlelayer, &is_multilayer, &is_multiview);

  if (is_multilayer) {
    header.insert("BlenderMultiChannel", StringAttribute("Blender V2.55.1 and newer"));
  }

  if (is_multiview) {
    addMultiView(header, *data->multiView);
  }

  /* avoid crash/abort when we don't have permission to write here */
  /* manually create ofstream, so we can handle utf-8 filepaths on windows */
  try {
    data->ofile_stream = new OFileStream(filepath);
    data->ofile = new OutputFile(*(data->ofile_stream), header);
  }
  catch (const std::exception &exc) {
    std::cerr << "IMB_exr_begin_write: ERROR: " << exc.what() << std::endl;

    delete data->ofile;
    delete data->ofile_stream;

    data->ofile = nullptr;
    data->ofile_stream = nullptr;
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    std::cerr << "IMB_exr_begin_write: UNKNOWN ERROR" << std::endl;

    delete data->ofile;
    delete data->ofile_stream;

    data->ofile = nullptr;
    data->ofile_stream = nullptr;
  }

  return (data->ofile != nullptr);
}

void IMB_exrtile_begin_write(
    void *handle, const char *filepath, int mipmap, int width, int height, int tilex, int tiley)
{
  ExrHandle *data = (ExrHandle *)handle;
  Header header(width, height);
  std::vector<Header> headers;
  ExrChannel *echan;

  data->tilex = tilex;
  data->tiley = tiley;
  data->width = width;
  data->height = height;
  data->mipmap = mipmap;

  header.setTileDescription(TileDescription(tilex, tiley, (mipmap) ? MIPMAP_LEVELS : ONE_LEVEL));
  header.compression() = RLE_COMPRESSION;
  header.setType(TILEDIMAGE);

  header.insert("BlenderMultiChannel", StringAttribute("Blender V2.43"));

  int numparts = data->multiView->size();

  /* copy header from all parts of input to our header array
   * those temporary files have one part per view */
  for (int i = 0; i < numparts; i++) {
    headers.push_back(header);
    headers[headers.size() - 1].setView((*(data->multiView))[i]);
    headers[headers.size() - 1].setName((*(data->multiView))[i]);
  }

  exr_printf("\nIMB_exrtile_begin_write\n");
  exr_printf("%s %-6s %-22s \"%s\"\n", "p", "view", "name", "internal_name");
  exr_printf("---------------------------------------------------------------\n");

  /* Assign channels. */
  for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
    /* Tiles are expected to be saved with full float currently. */
    BLI_assert(echan->use_half_float == 0);

    echan->m->internal_name = echan->m->name;
    echan->m->part_number = echan->view_id;

    headers[echan->view_id].channels().insert(echan->m->internal_name, Channel(Imf::FLOAT));
    exr_printf("%d %-6s %-22s \"%s\"\n",
               echan->m->part_number,
               echan->m->view.c_str(),
               echan->m->name.c_str(),
               echan->m->internal_name.c_str());
  }

  /* avoid crash/abort when we don't have permission to write here */
  /* manually create ofstream, so we can handle utf-8 filepaths on windows */
  try {
    data->ofile_stream = new OFileStream(filepath);
    data->mpofile = new MultiPartOutputFile(*(data->ofile_stream), headers.data(), headers.size());
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    delete data->mpofile;
    delete data->ofile_stream;

    data->mpofile = nullptr;
    data->ofile_stream = nullptr;
  }
}

bool IMB_exr_begin_read(
    void *handle, const char *filepath, int *width, int *height, const bool parse_channels)
{
  ExrHandle *data = (ExrHandle *)handle;
  ExrChannel *echan;

  /* 32 is arbitrary, but zero length files crashes exr. */
  if (!(BLI_exists(filepath) && BLI_file_size(filepath) > 32)) {
    return false;
  }

  /* avoid crash/abort when we don't have permission to write here */
  try {
    data->ifile_stream = new IFileStream(filepath);
    data->ifile = new MultiPartInputFile(*(data->ifile_stream));
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    delete data->ifile;
    delete data->ifile_stream;

    data->ifile = nullptr;
    data->ifile_stream = nullptr;
  }

  if (!data->ifile) {
    return false;
  }

  Box2i dw = data->ifile->header(0).dataWindow();
  data->width = *width = dw.max.x - dw.min.x + 1;
  data->height = *height = dw.max.y - dw.min.y + 1;

  if (parse_channels) {
    /* Parse channels into view/layer/pass. */
    if (!imb_exr_multilayer_parse_channels_from_file(data)) {
      return false;
    }
  }
  else {
    /* Read view and channels without parsing. */
    imb_exr_get_views(*data->ifile, *data->multiView);

    std::vector<MultiViewChannelName> channels;
    GetChannelsInMultiPartFile(*data->ifile, channels);

    for (const MultiViewChannelName &channel : channels) {
      IMB_exr_add_channel(
          data, nullptr, channel.name.c_str(), channel.view.c_str(), 0, 0, nullptr, false);

      echan = (ExrChannel *)data->channels.last;
      echan->m->name = channel.name;
      echan->m->view = channel.view;
      echan->m->part_number = channel.part_number;
      echan->m->internal_name = channel.internal_name;
    }
  }

  return true;
}

void IMB_exr_set_channel(
    void *handle, const char *layname, const char *passname, int xstride, int ystride, float *rect)
{
  ExrHandle *data = (ExrHandle *)handle;
  ExrChannel *echan;
  char name[EXR_TOT_MAXNAME + 1];

  if (layname && layname[0] != '\0') {
    char lay[EXR_LAY_MAXNAME + 1], pass[EXR_PASS_MAXNAME + 1];
    BLI_strncpy(lay, layname, EXR_LAY_MAXNAME);
    BLI_strncpy(pass, passname, EXR_PASS_MAXNAME);

    SNPRINTF(name, "%s.%s", lay, pass);
  }
  else {
    BLI_strncpy(name, passname, EXR_TOT_MAXNAME - 1);
  }

  echan = (ExrChannel *)BLI_findstring(&data->channels, name, offsetof(ExrChannel, name));

  if (echan) {
    echan->xstride = xstride;
    echan->ystride = ystride;
    echan->rect = rect;
  }
  else {
    printf("IMB_exr_set_channel error %s\n", name);
  }
}

float *IMB_exr_channel_rect(void *handle,
                            const char *layname,
                            const char *passname,
                            const char *viewname)
{
  ExrHandle *data = (ExrHandle *)handle;
  ExrChannel *echan;
  char name[EXR_TOT_MAXNAME + 1];

  if (layname) {
    char lay[EXR_LAY_MAXNAME + 1], pass[EXR_PASS_MAXNAME + 1];
    BLI_strncpy(lay, layname, EXR_LAY_MAXNAME);
    BLI_strncpy(pass, passname, EXR_PASS_MAXNAME);

    SNPRINTF(name, "%s.%s", lay, pass);
  }
  else {
    BLI_strncpy(name, passname, EXR_TOT_MAXNAME - 1);
  }

  /* name has to be unique, thus it's a combination of layer, pass, view, and channel */
  if (layname && layname[0] != '\0') {
    char temp_buf[EXR_PASS_MAXNAME];
    imb_exr_insert_view_name(temp_buf, name, viewname);
    STRNCPY(name, temp_buf);
  }
  else if (!data->multiView->empty()) {
    const int view_id = std::max(0, imb_exr_get_multiView_id(*data->multiView, viewname));
    std::string raw_name = insertViewName(name, *data->multiView, view_id);
    STRNCPY(name, raw_name.c_str());
  }

  echan = (ExrChannel *)BLI_findstring(&data->channels, name, offsetof(ExrChannel, name));

  if (echan) {
    return echan->rect;
  }

  return nullptr;
}

void IMB_exr_clear_channels(void *handle)
{
  ExrHandle *data = (ExrHandle *)handle;
  ExrChannel *chan;

  for (chan = (ExrChannel *)data->channels.first; chan; chan = chan->next) {
    delete chan->m;
  }

  BLI_freelistN(&data->channels);
}

void IMB_exr_write_channels(void *handle)
{
  ExrHandle *data = (ExrHandle *)handle;
  FrameBuffer frameBuffer;
  ExrChannel *echan;

  if (data->channels.first) {
    const size_t num_pixels = size_t(data->width) * data->height;
    half *rect_half = nullptr, *current_rect_half = nullptr;

    /* We allocate temporary storage for half pixels for all the channels at once. */
    if (data->num_half_channels != 0) {
      rect_half = (half *)MEM_mallocN(sizeof(half) * data->num_half_channels * num_pixels,
                                      __func__);
      current_rect_half = rect_half;
    }

    for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
      /* Writing starts from last scan-line, stride negative. */
      if (echan->use_half_float) {
        float *rect = echan->rect;
        half *cur = current_rect_half;
        for (size_t i = 0; i < num_pixels; i++, cur++) {
          *cur = float_to_half_safe(rect[i * echan->xstride]);
        }
        half *rect_to_write = current_rect_half + (data->height - 1L) * data->width;
        frameBuffer.insert(
            echan->name,
            Slice(Imf::HALF, (char *)rect_to_write, sizeof(half), -data->width * sizeof(half)));
        current_rect_half += num_pixels;
      }
      else {
        float *rect = echan->rect + echan->xstride * (data->height - 1L) * data->width;
        frameBuffer.insert(echan->name,
                           Slice(Imf::FLOAT,
                                 (char *)rect,
                                 echan->xstride * sizeof(float),
                                 -echan->ystride * sizeof(float)));
      }
    }

    data->ofile->setFrameBuffer(frameBuffer);
    try {
      data->ofile->writePixels(data->height);
    }
    catch (const std::exception &exc) {
      std::cerr << "OpenEXR-writePixels: ERROR: " << exc.what() << std::endl;
    }
    catch (...) { /* Catch-all for edge cases or compiler bugs. */
      std::cerr << "OpenEXR-writePixels: UNKNOWN ERROR" << std::endl;
    }
    /* Free temporary buffers. */
    if (rect_half != nullptr) {
      MEM_freeN(rect_half);
    }
  }
  else {
    printf("Error: attempt to save MultiLayer without layers.\n");
  }
}

void IMB_exrtile_write_channels(
    void *handle, int partx, int party, int level, const char *viewname, bool empty)
{
  /* Can write empty channels for incomplete renders. */
  ExrHandle *data = (ExrHandle *)handle;
  FrameBuffer frameBuffer;
  std::string view(viewname);
  const int view_id = imb_exr_get_multiView_id(*data->multiView, view);

  exr_printf("\nIMB_exrtile_write_channels(view: %s)\n", viewname);
  exr_printf("%s %-6s %-22s \"%s\"\n", "p", "view", "name", "internal_name");
  exr_printf("---------------------------------------------------------------------\n");

  if (!empty) {
    ExrChannel *echan;

    for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {

      /* eventually we can make the parts' channels to include
       * only the current view TODO */
      if (!STREQ(viewname, echan->m->view.c_str())) {
        continue;
      }

      exr_printf("%d %-6s %-22s \"%s\"\n",
                 echan->m->part_number,
                 echan->m->view.c_str(),
                 echan->m->name.c_str(),
                 echan->m->internal_name.c_str());

      float *rect = echan->rect - echan->xstride * partx - echan->ystride * party;
      frameBuffer.insert(echan->m->internal_name,
                         Slice(Imf::FLOAT,
                               (char *)rect,
                               echan->xstride * sizeof(float),
                               echan->ystride * sizeof(float)));
    }
  }

  TiledOutputPart out(*data->mpofile, view_id);
  out.setFrameBuffer(frameBuffer);

  try {
    // printf("write tile %d %d\n", partx/data->tilex, party/data->tiley);
    out.writeTile(partx / data->tilex, party / data->tiley, level);
  }
  catch (const std::exception &exc) {
    std::cerr << "OpenEXR-writeTile: ERROR: " << exc.what() << std::endl;
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    std::cerr << "OpenEXR-writeTile: UNKNOWN ERROR" << std::endl;
  }
}

void IMB_exr_read_channels(void *handle)
{
  ExrHandle *data = (ExrHandle *)handle;
  int numparts = data->ifile->parts();

  /* Check if EXR was saved with previous versions of blender which flipped images. */
  const StringAttribute *ta = data->ifile->header(0).findTypedAttribute<StringAttribute>(
      "BlenderMultiChannel");

  /* 'previous multilayer attribute, flipped. */
  short flip = (ta && STRPREFIX(ta->value().c_str(), "Blender V2.43"));

  exr_printf(
      "\nIMB_exr_read_channels\n%s %-6s %-22s "
      "\"%s\"\n---------------------------------------------------------------------\n",
      "p",
      "view",
      "name",
      "internal_name");

  for (int i = 0; i < numparts; i++) {
    /* Read part header. */
    InputPart in(*data->ifile, i);
    Header header = in.header();
    Box2i dw = header.dataWindow();

    /* Insert all matching channel into frame-buffer. */
    FrameBuffer frameBuffer;
    ExrChannel *echan;

    for (echan = (ExrChannel *)data->channels.first; echan; echan = echan->next) {
      if (echan->m->part_number != i) {
        continue;
      }

      exr_printf("%d %-6s %-22s \"%s\"\n",
                 echan->m->part_number,
                 echan->m->view.c_str(),
                 echan->m->name.c_str(),
                 echan->m->internal_name.c_str());

      if (echan->rect) {
        float *rect = echan->rect;
        size_t xstride = echan->xstride * sizeof(float);
        size_t ystride = echan->ystride * sizeof(float);

        if (!flip) {
          /* Inverse correct first pixel for data-window coordinates. */
          rect -= echan->xstride * (dw.min.x - dw.min.y * data->width);
          /* Move to last scan-line to flip to Blender convention. */
          rect += echan->xstride * (data->height - 1) * data->width;
          ystride = -ystride;
        }
        else {
          /* Inverse correct first pixel for data-window coordinates. */
          rect -= echan->xstride * (dw.min.x + dw.min.y * data->width);
        }

        frameBuffer.insert(echan->m->internal_name,
                           Slice(Imf::FLOAT, (char *)rect, xstride, ystride));
      }
      else {
        printf("warning, channel with no rect set %s\n", echan->m->internal_name.c_str());
      }
    }

    /* Read pixels. */
    try {
      in.setFrameBuffer(frameBuffer);
      exr_printf("readPixels:readPixels[%d]: min.y: %d, max.y: %d\n", i, dw.min.y, dw.max.y);
      in.readPixels(dw.min.y, dw.max.y);
    }
    catch (const std::exception &exc) {
      std::cerr << "OpenEXR-readPixels: ERROR: " << exc.what() << std::endl;
      break;
    }
    catch (...) { /* Catch-all for edge cases or compiler bugs. */
      std::cerr << "OpenEXR-readPixels: UNKNOWN ERROR: " << std::endl;
      break;
    }
  }
}

void IMB_exr_multilayer_convert(void *handle,
                                void *base,
                                void *(*addview)(void *base, const char *str),
                                void *(*addlayer)(void *base, const char *str),
                                void (*addpass)(void *base,
                                                void *lay,
                                                const char *str,
                                                float *rect,
                                                int totchan,
                                                const char *chan_id,
                                                const char *view))
{
  ExrHandle *data = (ExrHandle *)handle;
  ExrLayer *lay;
  ExrPass *pass;

  /* RenderResult needs at least one RenderView */
  if (data->multiView->empty()) {
    addview(base, "");
  }
  else {
    /* add views to RenderResult */
    for (const std::string &view_name : *data->multiView) {
      addview(base, view_name.c_str());
    }
  }

  if (BLI_listbase_is_empty(&data->layers)) {
    printf("cannot convert multilayer, no layers in handle\n");
    return;
  }

  for (lay = (ExrLayer *)data->layers.first; lay; lay = lay->next) {
    void *laybase = addlayer(base, lay->name);
    if (laybase) {
      for (pass = (ExrPass *)lay->passes.first; pass; pass = pass->next) {
        addpass(base,
                laybase,
                pass->internal_name,
                pass->rect,
                pass->totchan,
                pass->chan_id,
                pass->view);
        pass->rect = nullptr;
      }
    }
  }
}

void IMB_exr_close(void *handle)
{
  ExrHandle *data = (ExrHandle *)handle;
  ExrLayer *lay;
  ExrPass *pass;
  ExrChannel *chan;

  delete data->ifile;
  delete data->ifile_stream;
  delete data->ofile;
  delete data->mpofile;
  delete data->ofile_stream;
  delete data->multiView;

  data->ifile = nullptr;
  data->ifile_stream = nullptr;
  data->ofile = nullptr;
  data->mpofile = nullptr;
  data->ofile_stream = nullptr;

  for (chan = (ExrChannel *)data->channels.first; chan; chan = chan->next) {
    delete chan->m;
  }
  BLI_freelistN(&data->channels);

  for (lay = (ExrLayer *)data->layers.first; lay; lay = lay->next) {
    for (pass = (ExrPass *)lay->passes.first; pass; pass = pass->next) {
      if (pass->rect) {
        MEM_freeN(pass->rect);
      }
    }
    BLI_freelistN(&lay->passes);
  }
  BLI_freelistN(&data->layers);

  BLI_remlink(&exrhandles, data);
  MEM_freeN(data);
}

/* ********* */

/** Get a sub-string from the end of the name, separated by '.'. */
static int imb_exr_split_token(const char *str, const char *end, const char **token)
{
  const char delims[] = {'.', '\0'};
  const char *sep;

  BLI_str_partition_ex(str, end, delims, &sep, token, true);

  if (!sep) {
    *token = str;
  }

  return int(end - *token);
}

static int imb_exr_split_channel_name(ExrChannel *echan, char *layname, char *passname)
{
  const char *name = echan->m->name.c_str();
  const char *end = name + strlen(name);
  const char *token;

  /* Some multi-layers have the combined buffer with names A B G R saved. */
  if (name[1] == 0) {
    echan->chan_id = BLI_toupper_ascii(name[0]);
    layname[0] = '\0';

    if (ELEM(name[0], 'R', 'G', 'B', 'A')) {
      strcpy(passname, "Combined");
    }
    else if (name[0] == 'Z') {
      strcpy(passname, "Depth");
    }
    else {
      strcpy(passname, name);
    }

    return 1;
  }

  /* last token is channel identifier */
  size_t len = imb_exr_split_token(name, end, &token);
  if (len == 0) {
    printf("multilayer read: bad channel name: %s\n", name);
    return 0;
  }

  char channelname[EXR_TOT_MAXNAME];
  BLI_strncpy(channelname, token, std::min(len + 1, sizeof(channelname)));

  if (len == 1) {
    echan->chan_id = BLI_toupper_ascii(channelname[0]);
  }
  else if (len > 1) {
    bool ok = false;

    if (len == 2) {
      /* Some multi-layers are using two-letter channels name,
       * like, MX or NZ, which is basically has structure of
       *   <pass_prefix><component>
       *
       * This is a bit silly, but see file from #35658.
       *
       * Here we do some magic to distinguish such cases.
       */
      const char chan_id = BLI_toupper_ascii(channelname[1]);
      if (ELEM(chan_id, 'X', 'Y', 'Z', 'R', 'G', 'B', 'U', 'V', 'A')) {
        echan->chan_id = chan_id;
        ok = true;
      }
    }
    else if (BLI_strcaseeq(channelname, "red")) {
      echan->chan_id = 'R';
      ok = true;
    }
    else if (BLI_strcaseeq(channelname, "green")) {
      echan->chan_id = 'G';
      ok = true;
    }
    else if (BLI_strcaseeq(channelname, "blue")) {
      echan->chan_id = 'B';
      ok = true;
    }
    else if (BLI_strcaseeq(channelname, "alpha")) {
      echan->chan_id = 'A';
      ok = true;
    }
    else if (BLI_strcaseeq(channelname, "depth")) {
      echan->chan_id = 'Z';
      ok = true;
    }

    if (ok == false) {
      printf("multilayer read: unknown channel token: %s\n", channelname);
      return 0;
    }
  }
  end -= len + 1; /* +1 to skip '.' separator */

  /* second token is pass name */
  len = imb_exr_split_token(name, end, &token);
  if (len == 0) {
    printf("multilayer read: bad channel name: %s\n", name);
    return 0;
  }
  BLI_strncpy(passname, token, len + 1);
  end -= len + 1; /* +1 to skip '.' separator */

  /* all preceding tokens combined as layer name */
  if (end > name) {
    BLI_strncpy(layname, name, int(end - name) + 1);
  }
  else {
    layname[0] = '\0';
  }

  return 1;
}

static ExrLayer *imb_exr_get_layer(ListBase *lb, char *layname)
{
  ExrLayer *lay = (ExrLayer *)BLI_findstring(lb, layname, offsetof(ExrLayer, name));

  if (lay == nullptr) {
    lay = MEM_cnew<ExrLayer>("exr layer");
    BLI_addtail(lb, lay);
    BLI_strncpy(lay->name, layname, EXR_LAY_MAXNAME);
  }

  return lay;
}

static ExrPass *imb_exr_get_pass(ListBase *lb, char *passname)
{
  ExrPass *pass = (ExrPass *)BLI_findstring(lb, passname, offsetof(ExrPass, name));

  if (pass == nullptr) {
    pass = MEM_cnew<ExrPass>("exr pass");

    if (STREQ(passname, "Combined")) {
      BLI_addhead(lb, pass);
    }
    else {
      BLI_addtail(lb, pass);
    }
  }

  STRNCPY(pass->name, passname);

  return pass;
}

static bool imb_exr_multilayer_parse_channels_from_file(ExrHandle *data)
{
  std::vector<MultiViewChannelName> channels;
  GetChannelsInMultiPartFile(*data->ifile, channels);

  imb_exr_get_views(*data->ifile, *data->multiView);

  for (const MultiViewChannelName &channel : channels) {
    IMB_exr_add_channel(
        data, nullptr, channel.name.c_str(), channel.view.c_str(), 0, 0, nullptr, false);

    ExrChannel *echan = (ExrChannel *)data->channels.last;
    echan->m->name = channel.name;
    echan->m->view = channel.view;
    echan->m->part_number = channel.part_number;
    echan->m->internal_name = channel.internal_name;
  }

  /* now try to sort out how to assign memory to the channels */
  /* first build hierarchical layer list */
  ExrChannel *echan = (ExrChannel *)data->channels.first;
  for (; echan; echan = echan->next) {
    char layname[EXR_TOT_MAXNAME], passname[EXR_TOT_MAXNAME];
    if (imb_exr_split_channel_name(echan, layname, passname)) {

      const char *view = echan->m->view.c_str();
      char internal_name[EXR_PASS_MAXNAME];

      STRNCPY(internal_name, passname);

      if (view[0] != '\0') {
        char tmp_pass[EXR_PASS_MAXNAME];
        SNPRINTF(tmp_pass, "%s.%s", passname, view);
        STRNCPY(passname, tmp_pass);
      }

      ExrLayer *lay = imb_exr_get_layer(&data->layers, layname);
      ExrPass *pass = imb_exr_get_pass(&lay->passes, passname);

      pass->chan[pass->totchan] = echan;
      pass->totchan++;
      pass->view_id = echan->view_id;
      STRNCPY(pass->view, view);
      STRNCPY(pass->internal_name, internal_name);

      if (pass->totchan >= EXR_PASS_MAXCHAN) {
        break;
      }
    }
  }
  if (echan) {
    printf("error, too many channels in one pass: %s\n", echan->m->name.c_str());
    return false;
  }

  /* with some heuristics, try to merge the channels in buffers */
  for (ExrLayer *lay = (ExrLayer *)data->layers.first; lay; lay = lay->next) {
    for (ExrPass *pass = (ExrPass *)lay->passes.first; pass; pass = pass->next) {
      if (pass->totchan) {
        pass->rect = (float *)MEM_callocN(
            data->width * data->height * pass->totchan * sizeof(float), "pass rect");
        if (pass->totchan == 1) {
          ExrChannel *echan = pass->chan[0];
          echan->rect = pass->rect;
          echan->xstride = 1;
          echan->ystride = data->width;
          pass->chan_id[0] = echan->chan_id;
        }
        else {
          char lookup[256];

          memset(lookup, 0, sizeof(lookup));

          /* we can have RGB(A), XYZ(W), UVA */
          if (ELEM(pass->totchan, 3, 4)) {
            if (pass->chan[0]->chan_id == 'B' || pass->chan[1]->chan_id == 'B' ||
                pass->chan[2]->chan_id == 'B')
            {
              lookup[uint('R')] = 0;
              lookup[uint('G')] = 1;
              lookup[uint('B')] = 2;
              lookup[uint('A')] = 3;
            }
            else if (pass->chan[0]->chan_id == 'Y' || pass->chan[1]->chan_id == 'Y' ||
                     pass->chan[2]->chan_id == 'Y')
            {
              lookup[uint('X')] = 0;
              lookup[uint('Y')] = 1;
              lookup[uint('Z')] = 2;
              lookup[uint('W')] = 3;
            }
            else {
              lookup[uint('U')] = 0;
              lookup[uint('V')] = 1;
              lookup[uint('A')] = 2;
            }
            for (int a = 0; a < pass->totchan; a++) {
              echan = pass->chan[a];
              echan->rect = pass->rect + lookup[uint(echan->chan_id)];
              echan->xstride = pass->totchan;
              echan->ystride = data->width * pass->totchan;
              pass->chan_id[uint(lookup[uint(echan->chan_id)])] = echan->chan_id;
            }
          }
          else { /* unknown */
            for (int a = 0; a < pass->totchan; a++) {
              ExrChannel *echan = pass->chan[a];
              echan->rect = pass->rect + a;
              echan->xstride = pass->totchan;
              echan->ystride = data->width * pass->totchan;
              pass->chan_id[a] = echan->chan_id;
            }
          }
        }
      }
    }
  }

  return true;
}

/* creates channels, makes a hierarchy and assigns memory to channels */
static ExrHandle *imb_exr_begin_read_mem(IStream &file_stream,
                                         MultiPartInputFile &file,
                                         int width,
                                         int height)
{
  ExrHandle *data = (ExrHandle *)IMB_exr_get_handle();

  data->ifile_stream = &file_stream;
  data->ifile = &file;

  data->width = width;
  data->height = height;

  if (!imb_exr_multilayer_parse_channels_from_file(data)) {
    IMB_exr_close(data);
    return nullptr;
  }

  return data;
}

/* ********************************************************* */

/* debug only */
static void exr_printf(const char *fmt, ...)
{
#if 0
  char output[1024];
  va_list args;
  va_start(args, fmt);
  std::vsprintf(output, fmt, args);
  va_end(args);
  printf("%s", output);
#else
  (void)fmt;
#endif
}

static void exr_print_filecontents(MultiPartInputFile &file)
{
  int numparts = file.parts();
  if (numparts == 1 && hasMultiView(file.header(0))) {
    const StringVector views = multiView(file.header(0));
    printf("OpenEXR-load: MultiView file\n");
    printf("OpenEXR-load: Default view: %s\n", defaultViewName(views).c_str());
    for (const std::string &view : views) {
      printf("OpenEXR-load: Found view %s\n", view.c_str());
    }
  }
  else if (numparts > 1) {
    printf("OpenEXR-load: MultiPart file\n");
    for (int i = 0; i < numparts; i++) {
      if (file.header(i).hasView()) {
        printf("OpenEXR-load: Part %d: view = \"%s\"\n", i, file.header(i).view().c_str());
      }
    }
  }

  for (int j = 0; j < numparts; j++) {
    const ChannelList &channels = file.header(j).channels();
    for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
      const Channel &channel = i.channel();
      printf("OpenEXR-load: Found channel %s of type %d\n", i.name(), channel.type);
    }
  }
}

/* For non-multi-layer, map R G B A channel names to something that's in this file. */
static const char *exr_rgba_channelname(MultiPartInputFile &file, const char *chan)
{
  const ChannelList &channels = file.header(0).channels();

  for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
    // const Channel &channel = i.channel(); /* Not used yet. */
    const char *str = i.name();
    int len = strlen(str);
    if (len) {
      if (BLI_strcasecmp(chan, str + len - 1) == 0) {
        return str;
      }
    }
  }
  return chan;
}

static int exr_has_rgb(MultiPartInputFile &file, const char *rgb_channels[3])
{
  /* Common names for RGB-like channels in order. */
  static const char *channel_names[] = {
      "R", "Red", "G", "Green", "B", "Blue", "AR", "RA", "AG", "GA", "AB", "BA", nullptr};

  const Header &header = file.header(0);
  int num_channels = 0;

  for (int i = 0; channel_names[i]; i++) {
    if (header.channels().findChannel(channel_names[i])) {
      rgb_channels[num_channels++] = channel_names[i];
      if (num_channels == 3) {
        break;
      }
    }
  }

  return num_channels;
}

static bool exr_has_luma(MultiPartInputFile &file)
{
  /* Y channel is the luma and should always present fir luma space images,
   * optionally it could be also channels for chromas called BY and RY.
   */
  const Header &header = file.header(0);
  return header.channels().findChannel("Y") != nullptr;
}

static bool exr_has_chroma(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return header.channels().findChannel("BY") != nullptr &&
         header.channels().findChannel("RY") != nullptr;
}

static bool exr_has_zbuffer(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return !(header.channels().findChannel("Z") == nullptr);
}

static bool exr_has_alpha(MultiPartInputFile &file)
{
  const Header &header = file.header(0);
  return !(header.channels().findChannel("A") == nullptr);
}

static bool exr_is_half_float(MultiPartInputFile &file)
{
  const ChannelList &channels = file.header(0).channels();
  for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
    const Channel &channel = i.channel();
    if (channel.type != HALF) {
      return false;
    }
  }
  return true;
}

static bool imb_exr_is_multilayer_file(MultiPartInputFile &file)
{
  const ChannelList &channels = file.header(0).channels();
  std::set<std::string> layerNames;

  /* This will not include empty layer names, so files with just R/G/B/A
   * channels without a layer name will be single layer. */
  channels.layers(layerNames);

  return !layerNames.empty();
}

static void imb_exr_type_by_channels(ChannelList &channels,
                                     StringVector &views,
                                     bool *r_singlelayer,
                                     bool *r_multilayer,
                                     bool *r_multiview)
{
  std::set<std::string> layerNames;

  *r_singlelayer = true;
  *r_multilayer = *r_multiview = false;

  /* will not include empty layer names */
  channels.layers(layerNames);

  if (!views.empty() && !views[0].empty()) {
    *r_multiview = true;
  }
  else {
    *r_singlelayer = false;
    *r_multilayer = (layerNames.size() > 1);
    *r_multiview = false;
    return;
  }

  if (!layerNames.empty()) {
    /* If `layerNames` is not empty, it means at least one layer is non-empty,
     * but it also could be layers without names in the file and such case
     * shall be considered a multi-layer EXR.
     *
     * That's what we do here: test whether there are empty layer names together
     * with non-empty ones in the file.
     */
    for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); i++) {
      for (const std::string &layer_name : layerNames) {
        /* see if any layername differs from a viewname */
        if (imb_exr_get_multiView_id(views, layer_name) == -1) {
          std::string layerName = layer_name;
          size_t pos = layerName.rfind('.');

          if (pos == std::string::npos) {
            *r_multilayer = true;
            *r_singlelayer = false;
            return;
          }
        }
      }
    }
  }
  else {
    *r_singlelayer = true;
    *r_multilayer = false;
  }

  BLI_assert(r_singlelayer != r_multilayer);
}

static bool exr_has_multiview(MultiPartInputFile &file)
{
  for (int p = 0; p < file.parts(); p++) {
    if (hasMultiView(file.header(p))) {
      return true;
    }
  }

  return false;
}

static bool exr_has_multipart_file(MultiPartInputFile &file)
{
  return file.parts() > 1;
}

/* it returns true if the file is multilayer or multiview */
static bool imb_exr_is_multi(MultiPartInputFile &file)
{
  /* Multipart files are treated as multilayer in blender -
   * even if they are single layer openexr with multiview. */
  if (exr_has_multipart_file(file)) {
    return true;
  }

  if (exr_has_multiview(file)) {
    return true;
  }

  if (imb_exr_is_multilayer_file(file)) {
    return true;
  }

  return false;
}

bool IMB_exr_has_multilayer(void *handle)
{
  ExrHandle *data = (ExrHandle *)handle;
  return imb_exr_is_multi(*data->ifile);
}

struct ImBuf *imb_load_openexr(const uchar *mem,
                               size_t size,
                               int flags,
                               char colorspace[IM_MAX_SPACE])
{
  struct ImBuf *ibuf = nullptr;
  IMemStream *membuf = nullptr;
  MultiPartInputFile *file = nullptr;

  if (imb_is_a_openexr(mem, size) == 0) {
    return nullptr;
  }

  colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_FLOAT);

  try {
    bool is_multi;

    membuf = new IMemStream((uchar *)mem, size);
    file = new MultiPartInputFile(*membuf);

    Box2i dw = file->header(0).dataWindow();
    const size_t width = dw.max.x - dw.min.x + 1;
    const size_t height = dw.max.y - dw.min.y + 1;

    // printf("OpenEXR-load: image data window %d %d %d %d\n",
    //     dw.min.x, dw.min.y, dw.max.x, dw.max.y);

    if (false) { /* debug */
      exr_print_filecontents(*file);
    }

    is_multi = imb_exr_is_multi(*file);

    /* do not make an ibuf when */
    if (is_multi && !(flags & IB_test) && !(flags & IB_multilayer)) {
      printf("Error: can't process EXR multilayer file\n");
    }
    else {
      const bool is_alpha = exr_has_alpha(*file);

      ibuf = IMB_allocImBuf(width, height, is_alpha ? 32 : 24, 0);
      ibuf->flags |= exr_is_half_float(*file) ? IB_halffloat : 0;

      if (hasXDensity(file->header(0))) {
        /* Convert inches to meters. */
        ibuf->ppm[0] = double(xDensity(file->header(0))) / 0.0254;
        ibuf->ppm[1] = ibuf->ppm[0] * double(file->header(0).pixelAspectRatio());
      }

      ibuf->ftype = IMB_FTYPE_OPENEXR;

      if (!(flags & IB_test)) {

        if (flags & IB_metadata) {
          const Header &header = file->header(0);
          Header::ConstIterator iter;

          IMB_metadata_ensure(&ibuf->metadata);
          for (iter = header.begin(); iter != header.end(); iter++) {
            const StringAttribute *attr = file->header(0).findTypedAttribute<StringAttribute>(
                iter.name());

            /* not all attributes are string attributes so we might get some NULLs here */
            if (attr) {
              IMB_metadata_set_field(ibuf->metadata, iter.name(), attr->value().c_str());
              ibuf->flags |= IB_metadata;
            }
          }
        }

        /* Only enters with IB_multilayer flag set. */
        if (is_multi && ((flags & IB_thumbnail) == 0)) {
          /* constructs channels for reading, allocates memory in channels */
          ExrHandle *handle = imb_exr_begin_read_mem(*membuf, *file, width, height);
          if (handle) {
            IMB_exr_read_channels(handle);
            ibuf->userdata = handle; /* potential danger, the caller has to check for this! */
          }
        }
        else {
          const char *rgb_channels[3];
          const int num_rgb_channels = exr_has_rgb(*file, rgb_channels);
          const bool has_luma = exr_has_luma(*file);
          FrameBuffer frameBuffer;
          float *first;
          size_t xstride = sizeof(float[4]);
          size_t ystride = -xstride * width;

          imb_addrectfloatImBuf(ibuf, 4);

          /* Inverse correct first pixel for data-window
           * coordinates (- dw.min.y because of y flip). */
          first = ibuf->rect_float - 4 * (dw.min.x - dw.min.y * width);
          /* But, since we read y-flipped (negative y stride) we move to last scan-line. */
          first += 4 * (height - 1) * width;

          if (num_rgb_channels > 0) {
            for (int i = 0; i < num_rgb_channels; i++) {
              frameBuffer.insert(exr_rgba_channelname(*file, rgb_channels[i]),
                                 Slice(Imf::FLOAT, (char *)(first + i), xstride, ystride));
            }
          }
          else if (has_luma) {
            frameBuffer.insert(exr_rgba_channelname(*file, "Y"),
                               Slice(Imf::FLOAT, (char *)first, xstride, ystride));
            frameBuffer.insert(
                exr_rgba_channelname(*file, "BY"),
                Slice(Imf::FLOAT, (char *)(first + 1), xstride, ystride, 1, 1, 0.5f));
            frameBuffer.insert(
                exr_rgba_channelname(*file, "RY"),
                Slice(Imf::FLOAT, (char *)(first + 2), xstride, ystride, 1, 1, 0.5f));
          }

          /* 1.0 is fill value, this still needs to be assigned even when (is_alpha == 0) */
          frameBuffer.insert(exr_rgba_channelname(*file, "A"),
                             Slice(Imf::FLOAT, (char *)(first + 3), xstride, ystride, 1, 1, 1.0f));

          if (exr_has_zbuffer(*file)) {
            float *firstz;

            addzbuffloatImBuf(ibuf);
            firstz = ibuf->zbuf_float - (dw.min.x - dw.min.y * width);
            firstz += (height - 1) * width;
            frameBuffer.insert(
                "Z", Slice(Imf::FLOAT, (char *)firstz, sizeof(float), -width * sizeof(float)));
          }

          InputPart in(*file, 0);
          in.setFrameBuffer(frameBuffer);
          in.readPixels(dw.min.y, dw.max.y);

          /* XXX, ImBuf has no nice way to deal with this.
           * ideally IM_rect would be used when the caller wants a rect BUT
           * at the moment all functions use IM_rect.
           * Disabling this is ok because all functions should check
           * if a rect exists and create one on demand.
           *
           * Disabling this because the sequencer frees immediate. */
#if 0
          if (flag & IM_rect) {
            IMB_rect_from_float(ibuf);
          }
#endif

          if (num_rgb_channels == 0 && has_luma && exr_has_chroma(*file)) {
            for (size_t a = 0; a < size_t(ibuf->x) * ibuf->y; a++) {
              float *color = ibuf->rect_float + a * 4;
              ycc_to_rgb(color[0] * 255.0f,
                         color[1] * 255.0f,
                         color[2] * 255.0f,
                         &color[0],
                         &color[1],
                         &color[2],
                         BLI_YCC_ITU_BT709);
            }
          }
          else if (num_rgb_channels <= 1) {
            /* Convert 1 to 3 channels. */
            for (size_t a = 0; a < size_t(ibuf->x) * ibuf->y; a++) {
              float *color = ibuf->rect_float + a * 4;
              if (num_rgb_channels <= 1) {
                color[1] = color[0];
              }
              if (num_rgb_channels <= 2) {
                color[2] = color[0];
              }
            }
          }

          /* file is no longer needed */
          delete membuf;
          delete file;
        }
      }
      else {
        delete membuf;
        delete file;
      }

      if (flags & IB_alphamode_detect) {
        ibuf->flags |= IB_alphamode_premul;
      }
    }
    return ibuf;
  }
  catch (const std::exception &exc) {
    std::cerr << exc.what() << std::endl;
    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }
    delete file;
    delete membuf;

    return nullptr;
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    std::cerr << "OpenEXR-Load: UNKNOWN ERROR" << std::endl;
    if (ibuf) {
      IMB_freeImBuf(ibuf);
    }
    delete file;
    delete membuf;

    return nullptr;
  }
}

struct ImBuf *imb_load_filepath_thumbnail_openexr(const char *filepath,
                                                  const int /*flags*/,
                                                  const size_t max_thumb_size,
                                                  char colorspace[],
                                                  size_t *r_width,
                                                  size_t *r_height)
{
  IStream *stream = nullptr;
  Imf::RgbaInputFile *file = nullptr;

  /* OpenExr uses exceptions for error-handling. */
  try {

    /* The memory-mapped stream is faster, but don't use for huge files as it requires contiguous
     * address space and we are processing multiple files at once (typically one per processor
     * core). The 100 MB limit here is arbitrary, but seems reasonable and conservative. */
    if (BLI_file_size(filepath) < 100 * 1024 * 1024) {
      stream = new IMMapStream(filepath);
    }
    else {
      stream = new IFileStream(filepath);
    }

    /* imb_initopenexr() creates a global pool of worker threads. But we thumbnail multiple images
     * at once, and by default each file will attempt to use the entire pool for itself, stalling
     * the others. So each thumbnail should use a single thread of the pool. */
    file = new RgbaInputFile(*stream, 1);

    if (!file->isComplete()) {
      return nullptr;
    }

    Imath::Box2i dw = file->dataWindow();
    int source_w = dw.max.x - dw.min.x + 1;
    int source_h = dw.max.y - dw.min.y + 1;
    *r_width = source_w;
    *r_height = source_h;

    /* If there is an embedded thumbnail, return that instead of making a new one. */
    if (file->header().hasPreviewImage()) {
      const Imf::PreviewImage &preview = file->header().previewImage();
      ImBuf *ibuf = IMB_allocFromBuffer(
          (uint *)preview.pixels(), nullptr, preview.width(), preview.height(), 4);
      delete file;
      delete stream;
      IMB_flipy(ibuf);
      return ibuf;
    }

    /* Create a new thumbnail. */

    if (colorspace && colorspace[0]) {
      colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_FLOAT);
    }

    float scale_factor = MIN2(float(max_thumb_size) / float(source_w),
                              float(max_thumb_size) / float(source_h));
    int dest_w = MAX2(int(source_w * scale_factor), 1);
    int dest_h = MAX2(int(source_h * scale_factor), 1);

    struct ImBuf *ibuf = IMB_allocImBuf(dest_w, dest_h, 32, IB_rectfloat);

    /* A single row of source pixels. */
    Imf::Array<Imf::Rgba> pixels(source_w);

    /* Loop through destination thumbnail rows. */
    for (int h = 0; h < dest_h; h++) {

      /* Load the single source row that corresponds with destination row. */
      int source_y = int(float(h) / scale_factor) + dw.min.y;
      file->setFrameBuffer(&pixels[0] - dw.min.x - source_y * source_w, 1, source_w);
      file->readPixels(source_y);

      for (int w = 0; w < dest_w; w++) {
        /* For each destination pixel find single corresponding source pixel. */
        int source_x = int(MIN2((w / scale_factor), dw.max.x - 1));
        float *dest_px = &ibuf->rect_float[(h * dest_w + w) * 4];
        dest_px[0] = pixels[source_x].r;
        dest_px[1] = pixels[source_x].g;
        dest_px[2] = pixels[source_x].b;
        dest_px[3] = pixels[source_x].a;
      }
    }

    if (file->lineOrder() == INCREASING_Y) {
      IMB_flipy(ibuf);
    }

    delete file;
    delete stream;

    return ibuf;
  }

  catch (const std::exception &exc) {
    std::cerr << exc.what() << std::endl;
    delete file;
    delete stream;
    return nullptr;
  }
  catch (...) { /* Catch-all for edge cases or compiler bugs. */
    std::cerr << "OpenEXR-Thumbnail: UNKNOWN ERROR" << std::endl;
    delete file;
    delete stream;
    return nullptr;
  }

  return nullptr;
}

void imb_initopenexr(void)
{
  /* In a multithreaded program, staticInitialize() must be called once during startup, before the
   * program accesses any other functions or classes in the IlmImf library. */
  Imf::staticInitialize();
  Imf::setGlobalThreadCount(BLI_system_thread_count());
}

void imb_exitopenexr(void)
{
  /* Tells OpenEXR to free thread pool, also ensures there is no running tasks. */
  Imf::setGlobalThreadCount(0);
}

} /* export "C" */

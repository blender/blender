/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

/* This little block needed for linking to Blender... */
#include <algorithm>
#include <csetjmp>
#include <cstdio>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_idprop.hh"

#include "DNA_ID.h" /* ID property definitions. */

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"

#include "CLG_log.h"

#include <cstring>
#include <jerror.h>
#include <jpeglib.h>

static CLG_LogRef LOG = {"image.jpeg"};

/* the types are from the jpeg lib */
static void jpeg_error(j_common_ptr cinfo) ATTR_NORETURN;
static void init_source(j_decompress_ptr cinfo);
static boolean fill_input_buffer(j_decompress_ptr cinfo);
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes);
static void term_source(j_decompress_ptr cinfo);
static void memory_source(j_decompress_ptr cinfo, const uchar *buffer, size_t size);
static boolean handle_app1(j_decompress_ptr cinfo);
static ImBuf *ibJpegImageFromCinfo(
    jpeg_decompress_struct *cinfo, int flags, int max_size, size_t *r_width, size_t *r_height);

static const uchar jpeg_default_quality = 75;
static uchar ibuf_quality;

bool imb_is_a_jpeg(const uchar *mem, const size_t size)
{
  const char magic[2] = {0xFF, 0xD8};
  if (size < sizeof(magic)) {
    return false;
  }
  return memcmp(mem, magic, sizeof(magic)) == 0;
}

/*----------------------------------------------------------
 * JPG ERROR HANDLING
 *---------------------------------------------------------- */

struct my_error_mgr {
  jpeg_error_mgr pub; /* "public" fields */

  jmp_buf setjmp_buffer; /* for return to caller */
};

using my_error_ptr = my_error_mgr *;

static void jpeg_error(j_common_ptr cinfo)
{
  my_error_ptr err = (my_error_ptr)cinfo->err;

  /* Always display the message */
  (*cinfo->err->output_message)(cinfo);

  /* Let the memory manager delete any temp files before we die */
  jpeg_destroy(cinfo);

  /* return control to the setjmp point */
  longjmp(err->setjmp_buffer, 1);
}

/*----------------------------------------------------------
 * INPUT HANDLER FROM MEMORY
 *---------------------------------------------------------- */

struct my_source_mgr {
  jpeg_source_mgr pub; /* public fields */

  const uchar *buffer;
  int size;
  JOCTET terminal[2];
};

using my_src_ptr = my_source_mgr *;

static void init_source(j_decompress_ptr cinfo)
{
  (void)cinfo; /* unused */
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr)cinfo->src;

  /* Since we have given all we have got already
   * we simply fake an end of file
   */

  src->pub.next_input_byte = src->terminal;
  src->pub.bytes_in_buffer = 2;
  src->terminal[0] = (JOCTET)0xFF;
  src->terminal[1] = (JOCTET)JPEG_EOI;

  return true;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
  my_src_ptr src = (my_src_ptr)cinfo->src;

  if (num_bytes > 0) {
    /* prevent skipping over file end */
    size_t skip_size = size_t(num_bytes) <= src->pub.bytes_in_buffer ? num_bytes :
                                                                       src->pub.bytes_in_buffer;

    src->pub.next_input_byte = src->pub.next_input_byte + skip_size;
    src->pub.bytes_in_buffer = src->pub.bytes_in_buffer - skip_size;
  }
}

static void term_source(j_decompress_ptr cinfo)
{
  (void)cinfo; /* unused */
}

static void memory_source(j_decompress_ptr cinfo, const uchar *buffer, size_t size)
{
  my_src_ptr src;

  if (cinfo->src == nullptr) { /* first time for this JPEG object? */
    cinfo->src = (jpeg_source_mgr *)(*cinfo->mem->alloc_small)(
        (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));
  }

  src = (my_src_ptr)cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;
  src->pub.term_source = term_source;

  src->pub.bytes_in_buffer = size;
  src->pub.next_input_byte = buffer;

  src->buffer = buffer;
  src->size = size;
}

#define MAKESTMT(stuff) \
  do { \
    stuff \
  } while (0)

#define INPUT_VARS(cinfo) \
  jpeg_source_mgr *datasrc = (cinfo)->src; \
  const JOCTET *next_input_byte = datasrc->next_input_byte; \
  size_t bytes_in_buffer = datasrc->bytes_in_buffer

/* Unload the local copies --- do this only at a restart boundary */
#define INPUT_SYNC(cinfo) \
  (datasrc->next_input_byte = next_input_byte, datasrc->bytes_in_buffer = bytes_in_buffer)

/* Reload the local copies --- seldom used except in MAKE_BYTE_AVAIL */
#define INPUT_RELOAD(cinfo) \
  (next_input_byte = datasrc->next_input_byte, bytes_in_buffer = datasrc->bytes_in_buffer)

/* Internal macro for INPUT_BYTE and INPUT_2BYTES: make a byte available.
 * Note we do *not* do INPUT_SYNC before calling fill_input_buffer,
 * but we must reload the local copies after a successful fill.
 */
#define MAKE_BYTE_AVAIL(cinfo, action) \
  if (bytes_in_buffer == 0) { \
    if (!(*datasrc->fill_input_buffer)(cinfo)) { \
      action; \
    } \
    INPUT_RELOAD(cinfo); \
  } \
  (void)0

/* Read a byte into variable V.
 * If must suspend, take the specified action (typically "return false").
 */
#define INPUT_BYTE(cinfo, V, action) \
  MAKESTMT(MAKE_BYTE_AVAIL(cinfo, action); bytes_in_buffer--; V = GETJOCTET(*next_input_byte++);)

/* As above, but read two bytes interpreted as an unsigned 16-bit integer.
 * V should be declared `uint` or perhaps INT32.
 */
#define INPUT_2BYTES(cinfo, V, action) \
  MAKESTMT(MAKE_BYTE_AVAIL(cinfo, action); bytes_in_buffer--; \
           V = uint(GETJOCTET(*next_input_byte++)) << 8; \
           MAKE_BYTE_AVAIL(cinfo, action); \
           bytes_in_buffer--; \
           V += GETJOCTET(*next_input_byte++);)

struct NeoGeo_Word {
  uchar pad1;
  uchar pad2;
  uchar pad3;
  uchar quality;
};
BLI_STATIC_ASSERT(sizeof(NeoGeo_Word) == 4, "Must be 4 bytes");

static boolean handle_app1(j_decompress_ptr cinfo)
{
  INT32 length; /* initialized by the macro */
  INT32 i;
  char neogeo[128];

  INPUT_VARS(cinfo);

  INPUT_2BYTES(cinfo, length, return false);
  length -= 2;

  if (length < 16) {
    for (i = 0; i < length; i++) {
      INPUT_BYTE(cinfo, neogeo[i], return false);
    }
    length = 0;
    if (STRPREFIX(neogeo, "NeoGeo")) {
      NeoGeo_Word *neogeo_word = (NeoGeo_Word *)(neogeo + 6);
      ibuf_quality = neogeo_word->quality;
    }
  }
  INPUT_SYNC(cinfo); /* do before skip_input_data */
  if (length > 0) {
    (*cinfo->src->skip_input_data)(cinfo, length);
  }
  return true;
}

static ImBuf *ibJpegImageFromCinfo(
    jpeg_decompress_struct *cinfo, int flags, int max_size, size_t *r_width, size_t *r_height)
{
  JSAMPARRAY row_pointer;
  JSAMPLE *buffer = nullptr;
  int row_stride;
  int x, y, depth, r, g, b, k;
  ImBuf *ibuf = nullptr;
  uchar *rect;
  jpeg_saved_marker_ptr marker;
  char *str, *key, *value;

  /* install own app1 handler */
  ibuf_quality = jpeg_default_quality;
  jpeg_set_marker_processor(cinfo, 0xe1, handle_app1);
  cinfo->dct_method = JDCT_FLOAT;
  jpeg_save_markers(cinfo, JPEG_COM, 0xffff);

  if (jpeg_read_header(cinfo, false) == JPEG_HEADER_OK) {
    depth = cinfo->num_components;

    if (cinfo->jpeg_color_space == JCS_YCCK) {
      cinfo->out_color_space = JCS_CMYK;
    }

    if (r_width) {
      *r_width = cinfo->image_width;
    }
    if (r_height) {
      *r_height = cinfo->image_height;
    }

    if (max_size > 0) {
      /* `libjpeg` can more quickly decompress while scaling down to 1/2, 1/4, 1/8,
       * while `libjpeg-turbo` can also do 3/8, 5/8, etc. But max is 1/8. */
      float scale = float(max_size) / std::max(cinfo->image_width, cinfo->image_height);
      cinfo->scale_denom = 8;
      cinfo->scale_num = max_uu(1, min_uu(8, ceill(scale * float(cinfo->scale_denom))));
      cinfo->dct_method = JDCT_FASTEST;
      cinfo->dither_mode = JDITHER_ORDERED;
    }

    jpeg_start_decompress(cinfo);

    x = cinfo->output_width;
    y = cinfo->output_height;

    if (flags & IB_test) {
      jpeg_abort_decompress(cinfo);
      ibuf = IMB_allocImBuf(x, y, 8 * depth, 0);
    }
    else if ((ibuf = IMB_allocImBuf(x, y, 8 * depth, IB_byte_data | IB_uninitialized_pixels)) ==
             nullptr)
    {
      jpeg_abort_decompress(cinfo);
    }
    else {
      row_stride = cinfo->output_width * depth;

      row_pointer = (*cinfo->mem->alloc_sarray)((j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);

      for (y = ibuf->y - 1; y >= 0; y--) {
        jpeg_read_scanlines(cinfo, row_pointer, 1);
        rect = ibuf->byte_buffer.data + 4 * y * size_t(ibuf->x);
        buffer = row_pointer[0];

        switch (depth) {
          case 1:
            for (x = ibuf->x; x > 0; x--) {
              rect[3] = 255;
              rect[0] = rect[1] = rect[2] = *buffer++;
              rect += 4;
            }
            break;
          case 3:
            for (x = ibuf->x; x > 0; x--) {
              rect[3] = 255;
              rect[0] = *buffer++;
              rect[1] = *buffer++;
              rect[2] = *buffer++;
              rect += 4;
            }
            break;
          case 4:
            for (x = ibuf->x; x > 0; x--) {
              r = *buffer++;
              g = *buffer++;
              b = *buffer++;
              k = *buffer++;

              r = (r * k) / 255;
              g = (g * k) / 255;
              b = (b * k) / 255;

              rect[3] = 255;
              rect[2] = b;
              rect[1] = g;
              rect[0] = r;
              rect += 4;
            }
            break;
        }
      }

      marker = cinfo->marker_list;
      while (marker) {
        if (marker->marker != JPEG_COM) {
          goto next_stamp_marker;
        }

        /*
         * JPEG marker strings are not meant to be null-terminated,
         * create a null-terminated copy before going further.
         *
         * Files saved from Blender pre v4.0 were null terminated,
         * use `BLI_strnlen` to prevent assertion on passing in too short a string. */
        str = BLI_strdupn((const char *)marker->data,
                          BLI_strnlen((const char *)marker->data, marker->data_length));

        /*
         * Because JPEG format don't support the
         * pair "key/value" like PNG, we store the
         * stamp-info in a single "encode" string:
         * "Blender:key:value"
         *
         * That is why we need split it to the
         * common key/value here.
         */
        if (!STRPREFIX(str, "Blender")) {
          /*
           * Maybe the file have text that
           * we don't know "what it's", in that
           * case we keep the text (with a
           * key "None").
           * This is only for don't "lose"
           * the information when we write
           * it back to disk.
           */
          IMB_metadata_ensure(&ibuf->metadata);
          IMB_metadata_set_field(ibuf->metadata, "None", str);
          ibuf->flags |= IB_metadata;
          MEM_freeN(str);
          goto next_stamp_marker;
        }

        key = strchr(str, ':');
        /*
         * A little paranoid, but the file maybe
         * is broken... and a "extra" check is better
         * then segfault ;)
         */
        if (!key) {
          MEM_freeN(str);
          goto next_stamp_marker;
        }

        key++;
        value = strchr(key, ':');
        if (!value) {
          MEM_freeN(str);
          goto next_stamp_marker;
        }

        *value = '\0'; /* need finish the key string */
        value++;
        IMB_metadata_ensure(&ibuf->metadata);
        IMB_metadata_set_field(ibuf->metadata, key, value);
        ibuf->flags |= IB_metadata;
        MEM_freeN(str);
      next_stamp_marker:
        marker = marker->next;
      }

      jpeg_finish_decompress(cinfo);
    }

    if (ibuf) {
      /* Density_unit may be 0 for unknown, 1 for dots/inch, or 2 for dots/cm. */
      if (cinfo->density_unit == 1) {
        /* Convert inches to meters. */
        ibuf->ppm[0] = double(cinfo->X_density) / 0.0254;
        ibuf->ppm[1] = double(cinfo->Y_density) / 0.0254;
      }
      else if (cinfo->density_unit == 2) {
        ibuf->ppm[0] = double(cinfo->X_density) * 100.0;
        ibuf->ppm[1] = double(cinfo->Y_density) * 100.0;
      }

      ibuf->ftype = IMB_FTYPE_JPG;
      ibuf->foptions.quality = std::min<char>(ibuf_quality, 100);
    }
    jpeg_destroy((j_common_ptr)cinfo);
  }

  return ibuf;
}

ImBuf *imb_load_jpeg(const uchar *buffer,
                     size_t size,
                     int flags,
                     ImFileColorSpace & /*r_colorspace*/)
{
  jpeg_decompress_struct _cinfo, *cinfo = &_cinfo;
  my_error_mgr jerr;
  ImBuf *ibuf;

  if (!imb_is_a_jpeg(buffer, size)) {
    return nullptr;
  }

  cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error;

  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(cinfo);
    return nullptr;
  }

  jpeg_create_decompress(cinfo);
  memory_source(cinfo, buffer, size);

  ibuf = ibJpegImageFromCinfo(cinfo, flags, -1, nullptr, nullptr);

  return ibuf;
}

/* Defines for JPEG Header markers and segment size. */
#define JPEG_MARKER_MSB (0xFF)
#define JPEG_MARKER_SOI (0xD8)
#define JPEG_MARKER_APP1 (0xE1)
#define JPEG_APP1_MAX (1 << 16)

ImBuf *imb_thumbnail_jpeg(const char *filepath,
                          const int flags,
                          const size_t max_thumb_size,
                          ImFileColorSpace &r_colorspace,
                          size_t *r_width,
                          size_t *r_height)
{
  jpeg_decompress_struct _cinfo, *cinfo = &_cinfo;
  my_error_mgr jerr;
  FILE *infile = nullptr;

  cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error;

  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(cinfo);
    return nullptr;
  }

  if ((infile = BLI_fopen(filepath, "rb")) == nullptr) {
    CLOG_ERROR(&LOG, "Cannot open \"%s\"", filepath);
    return nullptr;
  }

  /* If file contains an embedded thumbnail, let's return that instead. */

  if ((fgetc(infile) == JPEG_MARKER_MSB) && (fgetc(infile) == JPEG_MARKER_SOI) &&
      (fgetc(infile) == JPEG_MARKER_MSB) && (fgetc(infile) == JPEG_MARKER_APP1))
  {
    /* This is a JPEG in EXIF format (SOI + APP1), not JFIF (SOI + APP0). */
    uint i = JPEG_APP1_MAX;
    /* All EXIF data is within this 64K header segment. Skip ahead until next SOI for thumbnail. */
    while (!((fgetc(infile) == JPEG_MARKER_MSB) && (fgetc(infile) == JPEG_MARKER_SOI)) &&
           !feof(infile) && i--)
    {
    }
    if (i > 0 && !feof(infile)) {
      /* We found a JPEG thumbnail inside this image. */
      ImBuf *ibuf = nullptr;
      uchar *buffer = MEM_calloc_arrayN<uchar>(JPEG_APP1_MAX, "thumbbuffer");
      /* Just put SOI directly in buffer rather than seeking back 2 bytes. */
      buffer[0] = JPEG_MARKER_MSB;
      buffer[1] = JPEG_MARKER_SOI;
      if (fread(buffer + 2, JPEG_APP1_MAX - 2, 1, infile) == 1) {
        ibuf = imb_load_jpeg(buffer, JPEG_APP1_MAX, flags, r_colorspace);
      }
      MEM_SAFE_FREE(buffer);
      if (ibuf) {
        fclose(infile);
        return ibuf;
      }
    }
  }

  /* No embedded thumbnail found, so let's create a new one. */

  fseek(infile, 0, SEEK_SET);
  jpeg_create_decompress(cinfo);

  jpeg_stdio_src(cinfo, infile);
  ImBuf *ibuf = ibJpegImageFromCinfo(cinfo, flags, max_thumb_size, r_width, r_height);
  fclose(infile);

  return ibuf;
}

#undef JPEG_MARKER_MSB
#undef JPEG_MARKER_SOI
#undef JPEG_MARKER_APP1
#undef JPEG_APP1_MAX

static void write_jpeg(jpeg_compress_struct *cinfo, ImBuf *ibuf)
{
  JSAMPLE *buffer = nullptr;
  JSAMPROW row_pointer[1];
  uchar *rect;
  int x, y;
  char neogeo[128];
  NeoGeo_Word *neogeo_word;

  jpeg_start_compress(cinfo, true);

  STRNCPY_UTF8(neogeo, "NeoGeo");
  neogeo_word = (NeoGeo_Word *)(neogeo + 6);
  memset(neogeo_word, 0, sizeof(*neogeo_word));
  neogeo_word->quality = ibuf->foptions.quality;
  jpeg_write_marker(cinfo, 0xe1, (JOCTET *)neogeo, 10);
  if (ibuf->metadata) {

    /* Static storage array for the short metadata. */
    char static_text[1024];
    const size_t static_text_size = ARRAY_SIZE(static_text);
    LISTBASE_FOREACH (IDProperty *, prop, &ibuf->metadata->data.group) {
      if (prop->type == IDP_STRING) {
        size_t text_len;
        if (STREQ(prop->name, "None")) {
          jpeg_write_marker(cinfo, JPEG_COM, (JOCTET *)IDP_string_get(prop), prop->len);
        }

        char *text = static_text;
        size_t text_size = static_text_size;
        /* 7 is for Blender, 2 colon separators, length of property
         * name and property value, followed by the nullptr-terminator
         * which isn't needed by JPEG but #BLI_snprintf_rlen requires it. */
        const size_t text_length_required = 7 + 2 + strlen(prop->name) +
                                            strlen(IDP_string_get(prop)) + 1;
        if (text_length_required > static_text_size) {
          text = MEM_malloc_arrayN<char>(text_length_required, "jpeg metadata field");
          text_size = text_length_required;
        }

        /*
         * The JPEG format don't support a pair "key/value"
         * like PNG, so we "encode" the stamp in a
         * single string:
         * "Blender:key:value"
         *
         * The first "Blender" is a simple identify to help
         * in the read process.
         */
        text_len = BLI_snprintf_utf8_rlen(
            text, text_size, "Blender:%s:%s", prop->name, IDP_string_get(prop));
        /* Don't write the null byte (not expected by the JPEG format). */
        jpeg_write_marker(cinfo, JPEG_COM, (JOCTET *)text, uint(text_len));

        /* TODO(sergey): Ideally we will try to re-use allocation as
         * much as possible. In practice, such long fields don't happen
         * often. */
        if (text != static_text) {
          MEM_freeN(text);
        }
      }
    }
  }

  /* Write ICC profile if there is one associated with the colorspace. */
  const ColorSpace *colorspace = ibuf->byte_buffer.colorspace;
  if (colorspace) {
    blender::Vector<char> icc_profile = IMB_colormanagement_space_to_icc_profile(colorspace);
    if (!icc_profile.is_empty()) {
      icc_profile.prepend({'I', 'C', 'C', '_', 'P', 'R', 'O', 'F', 'I', 'L', 'E', 0, 0, 1});
      jpeg_write_marker(cinfo,
                        JPEG_APP0 + 2,
                        reinterpret_cast<const JOCTET *>(icc_profile.data()),
                        icc_profile.size());
    }
  }

  row_pointer[0] = MEM_malloc_arrayN<std::remove_pointer_t<JSAMPROW>>(
      size_t(cinfo->input_components) * size_t(cinfo->image_width), "jpeg row_pointer");

  for (y = ibuf->y - 1; y >= 0; y--) {
    rect = ibuf->byte_buffer.data + 4 * y * size_t(ibuf->x);
    buffer = row_pointer[0];

    switch (cinfo->in_color_space) {
      case JCS_RGB:
        for (x = 0; x < ibuf->x; x++) {
          *buffer++ = rect[0];
          *buffer++ = rect[1];
          *buffer++ = rect[2];
          rect += 4;
        }
        break;
      case JCS_GRAYSCALE:
        for (x = 0; x < ibuf->x; x++) {
          *buffer++ = rect[0];
          rect += 4;
        }
        break;
      case JCS_UNKNOWN:
        memcpy(buffer, rect, 4 * ibuf->x);
        break;
      /* default was missing... intentional ? */
      default:
        /* do nothing */
        break;
    }

    jpeg_write_scanlines(cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(cinfo);
  MEM_freeN(row_pointer[0]);
}

static int init_jpeg(FILE *outfile, jpeg_compress_struct *cinfo, ImBuf *ibuf)
{
  int quality;

  quality = ibuf->foptions.quality;
  if (quality <= 0) {
    quality = jpeg_default_quality;
  }
  quality = std::min(quality, 100);

  jpeg_create_compress(cinfo);
  jpeg_stdio_dest(cinfo, outfile);

  cinfo->image_width = ibuf->x;
  cinfo->image_height = ibuf->y;

  cinfo->in_color_space = JCS_RGB;
  if (ibuf->planes == 8) {
    cinfo->in_color_space = JCS_GRAYSCALE;
  }
#if 0
  /* just write RGBA as RGB,
   * unsupported feature only confuses other s/w */

  if (ibuf->planes == 32) {
    cinfo->in_color_space = JCS_UNKNOWN;
  }
#endif
  switch (cinfo->in_color_space) {
    case JCS_RGB:
      cinfo->input_components = 3;
      break;
    case JCS_GRAYSCALE:
      cinfo->input_components = 1;
      break;
    case JCS_UNKNOWN:
      cinfo->input_components = 4;
      break;
    /* default was missing... intentional ? */
    default:
      /* do nothing */
      break;
  }
  jpeg_set_defaults(cinfo);

  /* own settings */

  cinfo->dct_method = JDCT_FLOAT;
  jpeg_set_quality(cinfo, quality, true);

  return 0;
}

static bool save_stdjpeg(const char *filepath, ImBuf *ibuf)
{
  FILE *outfile;
  jpeg_compress_struct _cinfo, *cinfo = &_cinfo;
  my_error_mgr jerr;

  if ((outfile = BLI_fopen(filepath, "wb")) == nullptr) {
    return false;
  }

  cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error;

  /* Establish the setjmp return context for jpeg_error to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_compress(cinfo);
    fclose(outfile);
    remove(filepath);
    return false;
  }

  init_jpeg(outfile, cinfo, ibuf);

  write_jpeg(cinfo, ibuf);

  fclose(outfile);
  jpeg_destroy_compress(cinfo);

  return true;
}

bool imb_savejpeg(ImBuf *ibuf, const char *filepath, int flags)
{

  ibuf->flags = flags;
  return save_stdjpeg(filepath, ibuf);
}

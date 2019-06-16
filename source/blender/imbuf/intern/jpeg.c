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
 * \ingroup imbuf
 */

/* This little block needed for linking to Blender... */
#include <stdio.h>
#include <setjmp.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_fileops.h"

#include "BKE_idprop.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_metadata.h"
#include "IMB_filetype.h"
#include "jpeglib.h"
#include "jerror.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

/* the types are from the jpeg lib */
static void jpeg_error(j_common_ptr cinfo) ATTR_NORETURN;
static void init_source(j_decompress_ptr cinfo);
static boolean fill_input_buffer(j_decompress_ptr cinfo);
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes);
static void term_source(j_decompress_ptr cinfo);
static void memory_source(j_decompress_ptr cinfo, const unsigned char *buffer, size_t size);
static boolean handle_app1(j_decompress_ptr cinfo);
static ImBuf *ibJpegImageFromCinfo(struct jpeg_decompress_struct *cinfo, int flags);

static const uchar jpeg_default_quality = 75;
static uchar ibuf_quality;

int imb_is_a_jpeg(const unsigned char *mem)
{
  if ((mem[0] == 0xFF) && (mem[1] == 0xD8)) {
    return 1;
  }
  return 0;
}

/*----------------------------------------------------------
 * JPG ERROR HANDLING
 *---------------------------------------------------------- */

typedef struct my_error_mgr {
  struct jpeg_error_mgr pub; /* "public" fields */

  jmp_buf setjmp_buffer; /* for return to caller */
} my_error_mgr;

typedef my_error_mgr *my_error_ptr;

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

#if 0
typedef struct {
  unsigned char *buffer;
  int filled;
} buffer_struct;
#endif

typedef struct {
  struct jpeg_source_mgr pub; /* public fields */

  const unsigned char *buffer;
  int size;
  JOCTET terminal[2];
} my_source_mgr;

typedef my_source_mgr *my_src_ptr;

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
    size_t skip_size = (size_t)num_bytes <= src->pub.bytes_in_buffer ? num_bytes :
                                                                       src->pub.bytes_in_buffer;

    src->pub.next_input_byte = src->pub.next_input_byte + skip_size;
    src->pub.bytes_in_buffer = src->pub.bytes_in_buffer - skip_size;
  }
}

static void term_source(j_decompress_ptr cinfo)
{
  (void)cinfo; /* unused */
}

static void memory_source(j_decompress_ptr cinfo, const unsigned char *buffer, size_t size)
{
  my_src_ptr src;

  if (cinfo->src == NULL) { /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)(
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
  struct jpeg_source_mgr *datasrc = (cinfo)->src; \
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
 * V should be declared unsigned int or perhaps INT32.
 */
#define INPUT_2BYTES(cinfo, V, action) \
  MAKESTMT(MAKE_BYTE_AVAIL(cinfo, action); bytes_in_buffer--; \
           V = ((unsigned int)GETJOCTET(*next_input_byte++)) << 8; \
           MAKE_BYTE_AVAIL(cinfo, action); \
           bytes_in_buffer--; \
           V += GETJOCTET(*next_input_byte++);)

struct NeoGeo_Word {
  uchar pad1;
  uchar pad2;
  uchar pad3;
  uchar quality;
};
BLI_STATIC_ASSERT(sizeof(struct NeoGeo_Word) == 4, "Must be 4 bytes");

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
    if (STREQLEN(neogeo, "NeoGeo", 6)) {
      struct NeoGeo_Word *neogeo_word = (struct NeoGeo_Word *)(neogeo + 6);
      ibuf_quality = neogeo_word->quality;
    }
  }
  INPUT_SYNC(cinfo); /* do before skip_input_data */
  if (length > 0) {
    (*cinfo->src->skip_input_data)(cinfo, length);
  }
  return true;
}

static ImBuf *ibJpegImageFromCinfo(struct jpeg_decompress_struct *cinfo, int flags)
{
  JSAMPARRAY row_pointer;
  JSAMPLE *buffer = NULL;
  int row_stride;
  int x, y, depth, r, g, b, k;
  struct ImBuf *ibuf = NULL;
  uchar *rect;
  jpeg_saved_marker_ptr marker;
  char *str, *key, *value;

  /* install own app1 handler */
  ibuf_quality = jpeg_default_quality;
  jpeg_set_marker_processor(cinfo, 0xe1, handle_app1);
  cinfo->dct_method = JDCT_FLOAT;
  jpeg_save_markers(cinfo, JPEG_COM, 0xffff);

  if (jpeg_read_header(cinfo, false) == JPEG_HEADER_OK) {
    x = cinfo->image_width;
    y = cinfo->image_height;
    depth = cinfo->num_components;

    if (cinfo->jpeg_color_space == JCS_YCCK) {
      cinfo->out_color_space = JCS_CMYK;
    }

    jpeg_start_decompress(cinfo);

    if (flags & IB_test) {
      jpeg_abort_decompress(cinfo);
      ibuf = IMB_allocImBuf(x, y, 8 * depth, 0);
    }
    else if ((ibuf = IMB_allocImBuf(x, y, 8 * depth, IB_rect)) == NULL) {
      jpeg_abort_decompress(cinfo);
    }
    else {
      row_stride = cinfo->output_width * depth;

      row_pointer = (*cinfo->mem->alloc_sarray)((j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);

      for (y = ibuf->y - 1; y >= 0; y--) {
        jpeg_read_scanlines(cinfo, row_pointer, 1);
        rect = (uchar *)(ibuf->rect + y * ibuf->x);
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
         * JPEG marker strings are not null-terminated,
         * create a null-terminated copy before going further
         */
        str = BLI_strdupn((char *)marker->data, marker->data_length);

        /*
         * Because JPEG format don't support the
         * pair "key/value" like PNG, we store the
         * stamp-info in a single "encode" string:
         * "Blender:key:value"
         *
         * That is why we need split it to the
         * common key/value here.
         */
        if (!STREQLEN(str, "Blender", 7)) {
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

    jpeg_destroy((j_common_ptr)cinfo);
    if (ibuf) {
      ibuf->ftype = IMB_FTYPE_JPG;
      ibuf->foptions.quality = MIN2(ibuf_quality, 100);
    }
  }

  return (ibuf);
}

ImBuf *imb_load_jpeg(const unsigned char *buffer,
                     size_t size,
                     int flags,
                     char colorspace[IM_MAX_SPACE])
{
  struct jpeg_decompress_struct _cinfo, *cinfo = &_cinfo;
  struct my_error_mgr jerr;
  ImBuf *ibuf;

  if (!imb_is_a_jpeg(buffer)) {
    return NULL;
  }

  colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

  cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_error;

  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(cinfo);
    return NULL;
  }

  jpeg_create_decompress(cinfo);
  memory_source(cinfo, buffer, size);

  ibuf = ibJpegImageFromCinfo(cinfo, flags);

  return (ibuf);
}

static void write_jpeg(struct jpeg_compress_struct *cinfo, struct ImBuf *ibuf)
{
  JSAMPLE *buffer = NULL;
  JSAMPROW row_pointer[1];
  uchar *rect;
  int x, y;
  char neogeo[128];
  struct NeoGeo_Word *neogeo_word;

  jpeg_start_compress(cinfo, true);

  strcpy(neogeo, "NeoGeo");
  neogeo_word = (struct NeoGeo_Word *)(neogeo + 6);
  memset(neogeo_word, 0, sizeof(*neogeo_word));
  neogeo_word->quality = ibuf->foptions.quality;
  jpeg_write_marker(cinfo, 0xe1, (JOCTET *)neogeo, 10);
  if (ibuf->metadata) {
    IDProperty *prop;
    /* Static storage array for the short metadata. */
    char static_text[1024];
    const int static_text_size = ARRAY_SIZE(static_text);
    for (prop = ibuf->metadata->data.group.first; prop; prop = prop->next) {
      if (prop->type == IDP_STRING) {
        int text_len;
        if (!strcmp(prop->name, "None")) {
          jpeg_write_marker(cinfo, JPEG_COM, (JOCTET *)IDP_String(prop), prop->len + 1);
        }

        char *text = static_text;
        int text_size = static_text_size;
        /* 7 is for Blender, 2 colon separators, length of property
         * name and property value, followed by the NULL-terminator. */
        const int text_length_required = 7 + 2 + strlen(prop->name) + strlen(IDP_String(prop)) + 1;
        if (text_length_required <= static_text_size) {
          text = MEM_mallocN(text_length_required, "jpeg metadata field");
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
        text_len = BLI_snprintf(text, text_size, "Blender:%s:%s", prop->name, IDP_String(prop));
        jpeg_write_marker(cinfo, JPEG_COM, (JOCTET *)text, text_len + 1);

        /* TODO(sergey): Ideally we will try to re-use allocation as
         * much as possible. In practice, such long fields don't happen
         * often. */
        if (text != static_text) {
          MEM_freeN(text);
        }
      }
    }
  }

  row_pointer[0] = MEM_mallocN(sizeof(JSAMPLE) * cinfo->input_components * cinfo->image_width,
                               "jpeg row_pointer");

  for (y = ibuf->y - 1; y >= 0; y--) {
    rect = (uchar *)(ibuf->rect + y * ibuf->x);
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

static int init_jpeg(FILE *outfile, struct jpeg_compress_struct *cinfo, struct ImBuf *ibuf)
{
  int quality;

  quality = ibuf->foptions.quality;
  if (quality <= 0) {
    quality = jpeg_default_quality;
  }
  if (quality > 100) {
    quality = 100;
  }

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

  return (0);
}

static int save_stdjpeg(const char *name, struct ImBuf *ibuf)
{
  FILE *outfile;
  struct jpeg_compress_struct _cinfo, *cinfo = &_cinfo;
  struct my_error_mgr jerr;

  if ((outfile = BLI_fopen(name, "wb")) == NULL) {
    return 0;
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
    remove(name);
    return 0;
  }

  init_jpeg(outfile, cinfo, ibuf);

  write_jpeg(cinfo, ibuf);

  fclose(outfile);
  jpeg_destroy_compress(cinfo);

  return 1;
}

int imb_savejpeg(struct ImBuf *ibuf, const char *name, int flags)
{

  ibuf->flags = flags;
  return save_stdjpeg(name, ibuf);
}

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
 * The Original Code is Copyright
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 */

/* ----------------------------------------------------------------------
 * Radiance High Dynamic Range image file IO
 * For description and code for reading/writing of radiance hdr files
 * by Greg Ward, refer to:
 * http://radsite.lbl.gov/radiance/refer/Notes/picture_format.html
 * ----------------------------------------------------------------------
 */

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_utildefines.h"

#include "imbuf.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

/* needed constants */
#define MINELEN 8
#define MAXELEN 0x7fff
#define MINRUN 4 /* minimum run length */
#define RED 0
#define GRN 1
#define BLU 2
#define EXP 3
#define COLXS 128
typedef unsigned char RGBE[4];
typedef float fCOLOR[3];

/* copy source -> dest */
#define COPY_RGBE(c1, c2) \
  (c2[RED] = c1[RED], c2[GRN] = c1[GRN], c2[BLU] = c1[BLU], c2[EXP] = c1[EXP])

/* read routines */
static const unsigned char *oldreadcolrs(RGBE *scan,
                                         const unsigned char *mem,
                                         int xmax,
                                         const unsigned char *mem_eof)
{
  size_t i, rshift = 0, len = xmax;
  while (len > 0) {
    if (UNLIKELY(mem_eof - mem < 4)) {
      return NULL;
    }
    scan[0][RED] = *mem++;
    scan[0][GRN] = *mem++;
    scan[0][BLU] = *mem++;
    scan[0][EXP] = *mem++;
    if (scan[0][RED] == 1 && scan[0][GRN] == 1 && scan[0][BLU] == 1) {
      for (i = scan[0][EXP] << rshift; i > 0; i--) {
        COPY_RGBE(scan[-1], scan[0]);
        scan++;
        len--;
      }
      rshift += 8;
    }
    else {
      scan++;
      len--;
      rshift = 0;
    }
  }
  return mem;
}

static const unsigned char *freadcolrs(RGBE *scan,
                                       const unsigned char *mem,
                                       int xmax,
                                       const unsigned char *mem_eof)
{
  if (UNLIKELY(mem_eof - mem < 4)) {
    return NULL;
  }

  if (UNLIKELY((xmax < MINELEN) | (xmax > MAXELEN))) {
    return oldreadcolrs(scan, mem, xmax, mem_eof);
  }

  int val = *mem++;
  if (val != 2) {
    return oldreadcolrs(scan, mem - 1, xmax, mem_eof);
  }

  scan[0][GRN] = *mem++;
  scan[0][BLU] = *mem++;

  val = *mem++;

  if (scan[0][GRN] != 2 || scan[0][BLU] & 128) {
    scan[0][RED] = 2;
    scan[0][EXP] = val;
    return oldreadcolrs(scan + 1, mem, xmax - 1, mem_eof);
  }

  if (UNLIKELY(((scan[0][BLU] << 8) | val) != xmax)) {
    return NULL;
  }

  for (size_t i = 0; i < 4; i++) {
    if (UNLIKELY(mem_eof - mem < 2)) {
      return NULL;
    }
    for (size_t j = 0; j < xmax;) {
      int code = *mem++;
      if (code > 128) {
        code &= 127;
        if (UNLIKELY(code + j > xmax)) {
          return NULL;
        }
        val = *mem++;
        while (code--) {
          scan[j++][i] = (unsigned char)val;
        }
      }
      else {
        if (UNLIKELY(mem_eof - mem < code)) {
          return NULL;
        }
        if (UNLIKELY(code + j > xmax)) {
          return NULL;
        }
        while (code--) {
          scan[j++][i] = *mem++;
        }
      }
    }
  }

  return mem;
}

/* helper functions */

/* rgbe -> float color */
static void RGBE2FLOAT(RGBE rgbe, fCOLOR fcol)
{
  if (rgbe[EXP] == 0) {
    fcol[RED] = fcol[GRN] = fcol[BLU] = 0;
  }
  else {
    float f = ldexp(1.0, rgbe[EXP] - (COLXS + 8));
    fcol[RED] = f * (rgbe[RED] + 0.5f);
    fcol[GRN] = f * (rgbe[GRN] + 0.5f);
    fcol[BLU] = f * (rgbe[BLU] + 0.5f);
  }
}

/* float color -> rgbe */
static void FLOAT2RGBE(fCOLOR fcol, RGBE rgbe)
{
  int e;
  float d = (fcol[RED] > fcol[GRN]) ? fcol[RED] : fcol[GRN];
  if (fcol[BLU] > d) {
    d = fcol[BLU];
  }
  if (d <= 1e-32f) {
    rgbe[RED] = rgbe[GRN] = rgbe[BLU] = rgbe[EXP] = 0;
  }
  else {
    d = (float)frexp(d, &e) * 256.0f / d;
    rgbe[RED] = (unsigned char)(fcol[RED] * d);
    rgbe[GRN] = (unsigned char)(fcol[GRN] * d);
    rgbe[BLU] = (unsigned char)(fcol[BLU] * d);
    rgbe[EXP] = (unsigned char)(e + COLXS);
  }
}

/* ImBuf read */

int imb_is_a_hdr(const unsigned char *buf)
{
  /* For recognition, Blender only loads first 32 bytes, so use #?RADIANCE id instead */
  /* update: actually, the 'RADIANCE' part is just an optional program name,
   * the magic word is really only the '#?' part */
  // if (strstr((char *)buf, "#?RADIANCE")) return 1;
  if (strstr((char *)buf, "#?")) {
    return 1;
  }
  // if (strstr((char *)buf, "32-bit_rle_rgbe")) return 1;
  return 0;
}

struct ImBuf *imb_loadhdr(const unsigned char *mem,
                          size_t size,
                          int flags,
                          char colorspace[IM_MAX_SPACE])
{
  struct ImBuf *ibuf;
  RGBE *sline;
  fCOLOR fcol;
  float *rect_float;
  int found = 0;
  int width = 0, height = 0;
  const unsigned char *ptr, *mem_eof = mem + size;
  char oriY[80], oriX[80];

  if (imb_is_a_hdr((void *)mem)) {
    colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_FLOAT);

    /* find empty line, next line is resolution info */
    size_t x;
    for (x = 1; x < size; x++) {
      if ((mem[x - 1] == '\n') && (mem[x] == '\n')) {
        found = 1;
        break;
      }
    }
    if (found && (x < (size + 2))) {
      if (sscanf((char *)&mem[x + 1],
                 "%79s %d %79s %d",
                 (char *)&oriY,
                 &height,
                 (char *)&oriX,
                 &width) != 4) {
        return NULL;
      }

      /* find end of this line, data right behind it */
      ptr = (unsigned char *)strchr((char *)&mem[x + 1], '\n');
      ptr++;

      if (flags & IB_test) {
        ibuf = IMB_allocImBuf(width, height, 32, 0);
      }
      else {
        ibuf = IMB_allocImBuf(width, height, 32, (flags & IB_rect) | IB_rectfloat);
      }

      if (UNLIKELY(ibuf == NULL)) {
        return NULL;
      }
      ibuf->ftype = IMB_FTYPE_RADHDR;

      if (flags & IB_alphamode_detect) {
        ibuf->flags |= IB_alphamode_premul;
      }

      if (flags & IB_test) {
        return ibuf;
      }

      /* read in and decode the actual data */
      sline = (RGBE *)MEM_mallocN(sizeof(*sline) * width, __func__);
      rect_float = ibuf->rect_float;

      for (size_t y = 0; y < height; y++) {
        ptr = freadcolrs(sline, ptr, width, mem_eof);
        if (ptr == NULL) {
          printf(
              "WARNING! HDR decode error, image may be just truncated, or completely wrong...\n");
          break;
        }
        for (x = 0; x < width; x++) {
          /* convert to ldr */
          RGBE2FLOAT(sline[x], fcol);
          *rect_float++ = fcol[RED];
          *rect_float++ = fcol[GRN];
          *rect_float++ = fcol[BLU];
          *rect_float++ = 1.0f;
        }
      }
      MEM_freeN(sline);
      if (oriY[0] == '-') {
        IMB_flipy(ibuf);
      }

      if (flags & IB_rect) {
        IMB_rect_from_float(ibuf);
      }

      return ibuf;
    }
    // else printf("Data not found!\n");
  }
  // else printf("Not a valid radiance HDR file!\n");

  return NULL;
}

/* ImBuf write */
static int fwritecolrs(FILE *file, int width, int channels, unsigned char *ibufscan, float *fpscan)
{
  int beg, c2, cnt = 0;
  fCOLOR fcol;
  RGBE rgbe, *rgbe_scan;

  if (UNLIKELY((ibufscan == NULL) && (fpscan == NULL))) {
    return 0;
  }

  rgbe_scan = (RGBE *)MEM_mallocN(sizeof(RGBE) * width, "radhdr_write_tmpscan");

  /* convert scanline */
  for (size_t i = 0, j = 0; i < width; i++) {
    if (fpscan) {
      fcol[RED] = fpscan[j];
      fcol[GRN] = (channels >= 2) ? fpscan[j + 1] : fpscan[j];
      fcol[BLU] = (channels >= 3) ? fpscan[j + 2] : fpscan[j];
    }
    else {
      fcol[RED] = (float)ibufscan[j] / 255.f;
      fcol[GRN] = (float)((channels >= 2) ? ibufscan[j + 1] : ibufscan[j]) / 255.f;
      fcol[BLU] = (float)((channels >= 3) ? ibufscan[j + 2] : ibufscan[j]) / 255.f;
    }
    FLOAT2RGBE(fcol, rgbe);
    COPY_RGBE(rgbe, rgbe_scan[i]);
    j += channels;
  }

  if ((width < MINELEN) | (width > MAXELEN)) { /* OOBs, write out flat */
    int x = fwrite((char *)rgbe_scan, sizeof(RGBE), width, file) - width;
    MEM_freeN(rgbe_scan);
    return x;
  }
  /* put magic header */
  putc(2, file);
  putc(2, file);
  putc((unsigned char)(width >> 8), file);
  putc((unsigned char)(width & 255), file);
  /* put components separately */
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = 0; j < width; j += cnt) { /* find next run */
      for (beg = j; beg < width; beg += cnt) {
        for (cnt = 1; (cnt < 127) && ((beg + cnt) < width) &&
                      (rgbe_scan[beg + cnt][i] == rgbe_scan[beg][i]);
             cnt++) {
          /* pass */
        }
        if (cnt >= MINRUN) {
          break; /* long enough */
        }
      }
      if (((beg - j) > 1) && ((beg - j) < MINRUN)) {
        c2 = j + 1;
        while (rgbe_scan[c2++][i] == rgbe_scan[j][i]) {
          if (c2 == beg) { /* short run */
            putc((unsigned char)(128 + beg - j), file);
            putc((unsigned char)(rgbe_scan[j][i]), file);
            j = beg;
            break;
          }
        }
      }
      while (j < beg) { /* write out non-run */
        if ((c2 = beg - j) > 128) {
          c2 = 128;
        }
        putc((unsigned char)(c2), file);
        while (c2--) {
          putc(rgbe_scan[j++][i], file);
        }
      }
      if (cnt >= MINRUN) { /* write out run */
        putc((unsigned char)(128 + cnt), file);
        putc(rgbe_scan[beg][i], file);
      }
      else {
        cnt = 0;
      }
    }
  }
  MEM_freeN(rgbe_scan);
  return (ferror(file) ? -1 : 0);
}

static void writeHeader(FILE *file, int width, int height)
{
  fprintf(file, "#?RADIANCE");
  fputc(10, file);
  fprintf(file, "# %s", "Created with Blender");
  fputc(10, file);
  fprintf(file, "EXPOSURE=%25.13f", 1.0);
  fputc(10, file);
  fprintf(file, "FORMAT=32-bit_rle_rgbe");
  fputc(10, file);
  fputc(10, file);
  fprintf(file, "-Y %d +X %d", height, width);
  fputc(10, file);
}

int imb_savehdr(struct ImBuf *ibuf, const char *name, int flags)
{
  FILE *file = BLI_fopen(name, "wb");
  float *fp = NULL;
  size_t width = ibuf->x, height = ibuf->y;
  unsigned char *cp = NULL;

  (void)flags; /* unused */

  if (file == NULL) {
    return 0;
  }

  writeHeader(file, width, height);

  if (ibuf->rect) {
    cp = (unsigned char *)ibuf->rect + ibuf->channels * (height - 1) * width;
  }
  if (ibuf->rect_float) {
    fp = ibuf->rect_float + ibuf->channels * (height - 1) * width;
  }

  for (size_t y = 0; y < height; y++) {
    if (fwritecolrs(file, width, ibuf->channels, cp, fp) < 0) {
      fclose(file);
      printf("HDR write error\n");
      return 0;
    }
    if (cp) {
      cp -= ibuf->channels * width;
    }
    if (fp) {
      fp -= ibuf->channels * width;
    }
  }

  fclose(file);
  return 1;
}

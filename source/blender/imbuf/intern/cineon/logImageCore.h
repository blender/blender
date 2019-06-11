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
 * Copyright 1999,2000,2001 David Hodson <hodsond@acm.org>
 */

/** \file
 * \ingroup imbcineon
 *
 * Cineon image file format library definitions.
 * Cineon and DPX common structures.
 *
 * This header file contains private details.
 * User code should generally use cineonlib.h and dpxlib.h only.
 * Hmm. I thought the two formats would have more in common!
 */

#ifndef __LOGIMAGECORE_H__
#define __LOGIMAGECORE_H__

#include <stdio.h>

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Image structure
 */

/* There are some differences between DPX and Cineon
 * so we need to know from what type of file the data came from. */
enum format {
  format_DPX,
  format_Cineon,
};

typedef struct LogImageElement {
  int depth;
  int bitsPerSample;
  int dataOffset;
  int packing;
  int transfer;
  int descriptor;
  unsigned int refLowData;
  unsigned int refHighData;
  float refLowQuantity;
  float refHighQuantity;
  float maxValue; /* = 2^bitsPerSample - 1 (used internally, doesn't come from the file header) */
} LogImageElement;

typedef struct LogImageFile {
  /* specified in header */
  int width;
  int height;
  int numElements;
  int depth;
  LogImageElement element[8];

  /* used for log <-> lin conversion */
  float referenceBlack;
  float referenceWhite;
  float gamma;

  /* io stuff */
  FILE *file;
  unsigned char *memBuffer;
  uintptr_t memBufferSize;
  unsigned char *memCursor;

  /* is the file LSB or MSB ? */
  int isMSB;

  /* DPX or Cineon ? */
  int srcFormat;
} LogImageFile;

/* The SMPTE defines this code:
 *  0 - User-defined
 *  1 - Printing density
 *  2 - Linear
 *  3 - Logarithmic
 *  4 - Unspecified video
 *  5 - SMPTE 240M
 *  6 - CCIR 709-1
 *  7 - CCIR 601-2 system B or G
 *  8 - CCIR 601-2 system M
 *  9 - NTSC composite video
 *  10 - PAL composite video
 *  11 - Z linear
 *  12 - homogeneous
 *
 * Note that transfer_characteristics is U8, don't need
 * check the byte order.
 */

enum transfer {
  transfer_UserDefined,
  transfer_PrintingDensity,
  transfer_Linear,
  transfer_Logarithmic,
  transfer_Unspecified,
  transfer_Smpte240M,
  transfer_Ccir7091,
  transfer_Ccir6012BG,
  transfer_Ccir6012M,
  transfer_NTSC,
  transfer_PAL,
  transfer_ZLinear,
  transfer_Homogeneous,
};

/* The SMPTE defines this code:
 * 0 - User-defined
 * 1 - Red
 * 2 - Green
 * 3 - Blue
 * 4 - Alpha
 * 6 - Luminance
 * 7 - Chrominance
 * 8 - Depth
 * 9 - Composite video
 * 50 - RGB
 * 51 - RGBA
 * 52 - ABGR
 * 100 - CbYCrY
 * 101 - CbYACrYA
 * 102 - CbYCr
 * 103 - CbYCrA
 * 150 - User-defined 2-component element
 * 151 - User-defined 3-component element
 * 152 - User-defined 4-component element
 * 153 - User-defined 5-component element
 * 154 - User-defined 6-component element
 * 155 - User-defined 7-component element
 * 156 - User-defined 8-component element
 */

enum descriptor {
  descriptor_UserDefined,
  descriptor_Red,
  descriptor_Green,
  descriptor_Blue,
  descriptor_Alpha,
  descriptor_Luminance = 6, /* don't ask me why there's no 5 */
  descriptor_Chrominance,
  descriptor_Depth,
  descriptor_Composite,
  descriptor_RGB = 50,
  descriptor_RGBA,
  descriptor_ABGR,
  descriptor_CbYCrY = 100,
  descriptor_CbYACrYA,
  descriptor_CbYCr,
  descriptor_CbYCrA,
  descriptor_UserDefined2Elt = 150,
  descriptor_UserDefined3Elt,
  descriptor_UserDefined4Elt,
  descriptor_UserDefined5Elt,
  descriptor_UserDefined6Elt,
  descriptor_UserDefined7Elt,
  descriptor_UserDefined8Elt,
  /* following descriptors are for internal use only */
  descriptor_YA,
};

/* int functions return 0 for OK */

void logImageSetVerbose(int verbosity);
int logImageIsDpx(const void *buffer);
int logImageIsCineon(const void *buffer);
LogImageFile *logImageOpenFromMemory(const unsigned char *buffer, unsigned int size);
LogImageFile *logImageOpenFromFile(const char *filename, int cineon);
void logImageGetSize(LogImageFile *logImage, int *width, int *height, int *depth);
LogImageFile *logImageCreate(const char *filename,
                             int cineon,
                             int width,
                             int height,
                             int bitsPerSample,
                             int isLogarithmic,
                             int hasAlpha,
                             int referenceWhite,
                             int referenceBlack,
                             float gamma,
                             const char *creator);
void logImageClose(LogImageFile *logImage);

/* Data handling */
size_t getRowLength(size_t width, LogImageElement logElement);
int logImageSetDataRGBA(LogImageFile *logImage, float *data, int dataIsLinearRGB);
int logImageGetDataRGBA(LogImageFile *logImage, float *data, int dataIsLinearRGB);

/*
 * Inline routines
 */

/* Endianness swapping */

BLI_INLINE unsigned short swap_ushort(unsigned short x, int swap)
{
  if (swap != 0) {
    return (x >> 8) | (x << 8);
  }
  else {
    return x;
  }
}

BLI_INLINE unsigned int swap_uint(unsigned int x, int swap)
{
  if (swap != 0) {
    return (x >> 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x << 24);
  }
  else {
    return x;
  }
}

BLI_INLINE float swap_float(float x, int swap)
{
  if (swap != 0) {
    union {
      float f;
      unsigned char b[4];
    } dat1, dat2;

    dat1.f = x;
    dat2.b[0] = dat1.b[3];
    dat2.b[1] = dat1.b[2];
    dat2.b[2] = dat1.b[1];
    dat2.b[3] = dat1.b[0];
    return dat2.f;
  }
  else {
    return x;
  }
}

/* Other */

BLI_INLINE unsigned int clamp_uint(unsigned int x, unsigned int low, unsigned int high)
{
  if (x > high) {
    return high;
  }
  else if (x < low) {
    return low;
  }
  else {
    return x;
  }
}

BLI_INLINE float clamp_float(float x, float low, float high)
{
  if (x > high) {
    return high;
  }
  else if (x < low) {
    return low;
  }
  else {
    return x;
  }
}

BLI_INLINE unsigned int float_uint(float value, unsigned int max)
{
  if (value < 0.0f) {
    return 0;
  }
  else if (value > (1.0f - 0.5f / (float)max)) {
    return max;
  }
  else {
    return (unsigned int)(((float)max * value) + 0.5f);
  }
}

#ifdef __cplusplus
}
#endif

#endif /* __LOGIMAGECORE_H__ */

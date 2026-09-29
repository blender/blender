/* SPDX-FileCopyrightText: 1999-2001 David Hodson <hodsond@acm.org>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbcineon
 *
 * Cineon image file format library definitions.
 *
 * This header file contains private details.
 * User code should generally use `cineonlib.h` only.
 */

#pragma once

#include <cstdio>

#include "BLI_compiler_compat.hh"
#include "BLI_sys_types.hh"

namespace blender {

#ifdef _WIN32
#  define PATHSEP_CHAR '\\'
#else
#  define PATHSEP_CHAR '/'
#endif

/*
 * Image structure
 */

struct LogImageElement {
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
};

struct LogImageFile {
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

  /* IO stuff. */
  FILE *file;
  unsigned char *memBuffer;
  uintptr_t memBufferSize;
  unsigned char *memCursor;

  /* is the file LSB or MSB ? */
  int isMSB;
};

/* Transfer characteristics, as defined by SMPTE. */
enum transfer {
  transfer_PrintingDensity = 1,
  transfer_Linear = 2,
};

/* Element descriptors, as defined by SMPTE. */
enum descriptor {
  descriptor_Red = 1,
  descriptor_Green = 2,
  descriptor_Blue = 3,
  descriptor_Luminance = 6,
  descriptor_RGB = 50,
};

/* int functions return 0 for OK */

void logImageSetVerbose(int verbosity);
int logImageIsCineon(const void *buffer, unsigned int size);
LogImageFile *logImageOpenFromMemory(const unsigned char *buffer, unsigned int size);
void logImageGetSize(const LogImageFile *logImage, int *width, int *height, int *depth);
/* Create a 10 bit RGB Cineon file. */
LogImageFile *logImageCreate(const char *filepath, int width, int height, const char *creator);
void logImageClose(LogImageFile *logImage);

/* Data handling */
size_t getRowLength(size_t width, const LogImageElement *logElement);
int logImageSetDataRGBA(LogImageFile *logImage, const float *data, int dataIsLinearRGB);
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
  return x;
}

BLI_INLINE unsigned int swap_uint(unsigned int x, int swap)
{
  if (swap != 0) {
    return (x >> 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x << 24);
  }
  return x;
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
  return x;
}

/* Other */

BLI_INLINE float clamp_float(float x, float low, float high)
{
  if (x > high) {
    return high;
  }
  if (x < low) {
    return low;
  }
  return x;
}

BLI_INLINE unsigned int float_uint(float value, unsigned int max)
{
  if (value < 0.0f) {
    return 0;
  }
  if (value > (1.0f - 0.5f / float(max))) {
    return max;
  }
  return static_cast<unsigned int>((float(max) * value) + 0.5f);
}

}  // namespace blender

/* SPDX-FileCopyrightText: 1999-2001 David Hodson <hodsond@acm.org>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbcineon
 *
 * Cineon image file format library routines.
 */

#include "logImageCore.h"
#include "cineonlib.h"
#include "logmemfile.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "BLI_assert.hh"
#include "BLI_memory_utils.hh"
#include "BLI_utildefines.hh"

#include "IMB_imbuf.hh"

#include "MEM_guardedalloc.h"

namespace blender {

/*
 * Declaration of static functions
 */

static int logImageSetData10(LogImageFile *logImage,
                             const LogImageElement &logElement,
                             const float *data);
static int logImageElementGetData(LogImageFile *logImage,
                                  const LogImageElement &logElement,
                                  float *data);
static int logImageElementGetData1(LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   float *data);
static int logImageElementGetData8(LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   float *data);
static int logImageElementGetData10(LogImageFile *logImage,
                                    const LogImageElement &logElement,
                                    float *data);
static int logImageElementGetData10Packed(LogImageFile *logImage,
                                          const LogImageElement &logElement,
                                          float *data);
static int logImageElementGetData12(LogImageFile *logImage,
                                    const LogImageElement &logElement,
                                    float *data);
static int logImageElementGetData12Packed(LogImageFile *logImage,
                                          const LogImageElement &logElement,
                                          float *data);
static int logImageElementGetData16(LogImageFile *logImage,
                                    const LogImageElement &logElement,
                                    float *data);
static int convertLogElementToRGBA(const float *src,
                                   float *dst,
                                   const LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   int dstIsLinearRGB);
static int convertRGBAToLogElement(const float *src,
                                   float *dst,
                                   const LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   int srcIsLinearRGB);

/*
 * For debug purpose
 */

static int verbose = 0;

void logImageSetVerbose(int verbosity)
{
  verbose = verbosity;
  cineonSetVerbose(verbosity);
}

/*
 * IO stuff
 */

int logImageIsCineon(const void *buffer, const uint size)
{
  uint magicNum;
  if (size < sizeof(magicNum)) {
    return 0;
  }
  magicNum = *static_cast<uint *>(const_cast<void *>(buffer));
  return (magicNum == CINEON_FILE_MAGIC || magicNum == swap_uint(CINEON_FILE_MAGIC, 1));
}

LogImageFile *logImageOpenFromMemory(const uchar *buffer, uint size)
{
  if (logImageIsCineon(buffer, size)) {
    return cineonOpen(buffer, size);
  }

  return nullptr;
}

LogImageFile *logImageCreate(const char *filepath, int width, int height, const char *creator)
{
  return cineonCreate(filepath, width, height, creator);
}

void logImageClose(LogImageFile *logImage)
{
  if (logImage != nullptr) {
    if (logImage->file) {
      fclose(logImage->file);
      logImage->file = nullptr;
    }
    MEM_delete(logImage);
  }
}

void logImageGetSize(const LogImageFile *logImage, int *width, int *height, int *depth)
{
  *width = logImage->width;
  *height = logImage->height;
  *depth = logImage->depth;
}

/*
 * Helper
 */

static size_t getRowLength(size_t width, const LogImageElement &logElement)
{
  /* return the row length in bytes according to width and packing method */
  switch (logElement.bitsPerSample) {
    case 1:
      return ((width * logElement.depth - 1) / 32 + 1) * 4;

    case 8:
      return ((width * logElement.depth - 1) / 4 + 1) * 4;

    case 10:
      if (logElement.packing == 0) {
        return ((width * logElement.depth * 10 - 1) / 32 + 1) * 4;
      }
      else if (ELEM(logElement.packing, 1, 2)) {
        return ((width * logElement.depth - 1) / 3 + 1) * 4;
      }
      break;
    case 12:
      if (logElement.packing == 0) {
        return ((width * logElement.depth * 12 - 1) / 32 + 1) * 4;
      }
      else if (ELEM(logElement.packing, 1, 2)) {
        return width * logElement.depth * 2;
      }
      break;
    case 16:
      return width * logElement.depth * 2;
  }
  return 0;
}

size_t getRowLength(size_t width, const LogImageElement *logElement)
{
  /* For the C-API. */

  return getRowLength(width, *logElement);
}

/*
 * Data writing
 */

int logImageSetDataRGBA(LogImageFile *logImage, const float *data, int dataIsLinearRGB)
{
  float *elementData;
  int returnValue;

  elementData = static_cast<float *>(imb_alloc_pixels(
      logImage->width, logImage->height, logImage->depth, sizeof(float), true, __func__));
  if (elementData == nullptr) {
    return 1;
  }

  if (convertRGBAToLogElement(
          data, elementData, logImage, logImage->element[0], dataIsLinearRGB) != 0)
  {
    MEM_delete(elementData);
    return 1;
  }

  /* Writing only supports 10 bits. */
  returnValue = logImageSetData10(logImage, logImage->element[0], elementData);

  MEM_delete(elementData);
  return returnValue;
}

static int logImageSetData10(LogImageFile *logImage,
                             const LogImageElement &logElement,
                             const float *data)
{
  size_t rowLength = getRowLength(logImage->width, logElement);
  uint pixel, index;
  uint *row;

  row = static_cast<uint *>(MEM_new_uninitialized(rowLength, __func__));
  if (row == nullptr) {
    if (verbose) {
      printf("Cineon: Cannot allocate row.\n");
    }
    return 1;
  }

  for (size_t y = 0; y < logImage->height; y++) {
    int offset = 22;
    index = 0;
    pixel = 0;

    for (size_t x = 0; x < logImage->width * logImage->depth; x++) {
      pixel |= uint(float_uint(data[y * logImage->width * logImage->depth + x], 1023)) << offset;
      offset -= 10;
      if (offset < 0) {
        row[index] = swap_uint(pixel, logImage->isMSB);
        index++;
        pixel = 0;
        offset = 22;
      }
    }
    if (pixel != 0) {
      row[index] = swap_uint(pixel, logImage->isMSB);
    }

    if (logimage_fwrite(row, rowLength, 1, logImage) == 0) {
      if (verbose) {
        printf("Cineon: Error while writing file.\n");
      }
      MEM_delete(row);
      return 1;
    }
  }
  MEM_delete(row);
  return 0;
}

/*
 * Data reading
 */

int logImageGetDataRGBA(LogImageFile *logImage, float *data, int dataIsLinearRGB)
{
  /* Fills data with 32 bits float RGBA values. The image is either a single RGB or
   * luminance element, or separate R, G and B elements. */
  BLI_assert(ELEM(logImage->numElements, 1, 3));

  float *elementData[3] = {};
  BLI_SCOPED_DEFER([&]() {
    for (float *element_data : elementData) {
      MEM_delete(element_data);
    }
  });

  for (int i = 0; i < logImage->numElements; i++) {
    elementData[i] = static_cast<float *>(imb_alloc_pixels(logImage->width,
                                                           logImage->height,
                                                           logImage->element[i].depth,
                                                           sizeof(float),
                                                           true,
                                                           __func__));
    if (elementData[i] == nullptr) {
      if (verbose) {
        printf("Cineon: Cannot allocate memory for elementData[%d]\n.", i);
      }
      return 1;
    }

    if (logImageElementGetData(logImage, logImage->element[i], elementData[i]) != 0) {
      if (verbose) {
        printf("Cineon: Cannot read elementData[%d]\n.", i);
      }
      return 1;
    }
  }

  if (logImage->numElements == 1) {
    return convertLogElementToRGBA(
        elementData[0], data, logImage, logImage->element[0], dataIsLinearRGB);
  }

  /* Interleave separate R, G and B elements into a single RGB element. */
  LogImageElement mergedElement = logImage->element[0];
  mergedElement.descriptor = descriptor_RGB;
  mergedElement.depth = 3;

  float *mergedData = static_cast<float *>(
      imb_alloc_pixels(logImage->width, logImage->height, 3, sizeof(float), true, __func__));
  if (mergedData == nullptr) {
    if (verbose) {
      printf("Cineon: Cannot allocate mergedData.\n");
    }
    return 1;
  }

  const size_t numPixels = size_t(logImage->width) * size_t(logImage->height);
  for (size_t i = 0; i < numPixels; i++) {
    for (int c = 0; c < 3; c++) {
      mergedData[i * 3 + c] = elementData[c][i];
    }
  }

  const int returnValue = convertLogElementToRGBA(
      mergedData, data, logImage, mergedElement, dataIsLinearRGB);
  MEM_delete(mergedData);
  return returnValue;
}

static int logImageElementGetData(LogImageFile *logImage,
                                  const LogImageElement &logElement,
                                  float *data)
{
  switch (logElement.bitsPerSample) {
    case 1:
      return logImageElementGetData1(logImage, logElement, data);

    case 8:
      return logImageElementGetData8(logImage, logElement, data);

    case 10:
      if (logElement.packing == 0) {
        return logImageElementGetData10Packed(logImage, logElement, data);
      }
      else if (ELEM(logElement.packing, 1, 2)) {
        return logImageElementGetData10(logImage, logElement, data);
      }
      break;

    case 12:
      if (logElement.packing == 0) {
        return logImageElementGetData12Packed(logImage, logElement, data);
      }
      else if (ELEM(logElement.packing, 1, 2)) {
        return logImageElementGetData12(logImage, logElement, data);
      }
      break;

    case 16:
      return logImageElementGetData16(logImage, logElement, data);
  }
  /* format not supported */
  return 1;
}

static int logImageElementGetData1(LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   float *data)
{
  uint pixel;

  /* seek at the right place */
  if (logimage_fseek(logImage, logElement.dataOffset) != 0) {
    if (verbose) {
      printf("Cineon: Couldn't seek at %d\n", logElement.dataOffset);
    }
    return 1;
  }

  /* read 1 bit data padded to 32 bits */
  for (size_t y = 0; y < logImage->height; y++) {
    for (size_t x = 0; x < logImage->width * logElement.depth; x += 32) {
      if (logimage_read_uint(&pixel, logImage) != 0) {
        if (verbose) {
          printf("Cineon: EOF reached\n");
        }
        return 1;
      }
      pixel = swap_uint(pixel, logImage->isMSB);
      for (int offset = 0; offset < 32 && x + offset < logImage->width; offset++) {
        data[y * logImage->width * logElement.depth + x + offset] = float((pixel >> offset) &
                                                                          0x01);
      }
    }
  }
  return 0;
}

static int logImageElementGetData8(LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   float *data)
{
  size_t rowLength = getRowLength(logImage->width, logElement);
  uchar pixel;

  /* extract required pixels */
  for (size_t y = 0; y < logImage->height; y++) {
    /* 8 bits are 32-bits padded so we need to seek at each row */
    if (logimage_fseek(logImage, logElement.dataOffset + y * rowLength) != 0) {
      if (verbose) {
        printf("Cineon: Couldn't seek at %d\n", int(logElement.dataOffset + y * rowLength));
      }
      return 1;
    }

    for (size_t x = 0; x < logImage->width * logElement.depth; x++) {
      if (logimage_read_uchar(&pixel, logImage) != 0) {
        if (verbose) {
          printf("Cineon: EOF reached\n");
        }
        return 1;
      }
      data[y * logImage->width * logElement.depth + x] = float(pixel) / 255.0f;
    }
  }
  return 0;
}

static int logImageElementGetData10(LogImageFile *logImage,
                                    const LogImageElement &logElement,
                                    float *data)
{
  uint pixel;

  /* seek to data */
  if (logimage_fseek(logImage, logElement.dataOffset) != 0) {
    if (verbose) {
      printf("Cineon: Couldn't seek at %d\n", logElement.dataOffset);
    }
    return 1;
  }

  for (size_t y = 0; y < logImage->height; y++) {
    int offset = -1;
    for (size_t x = 0; x < logImage->width * logElement.depth; x++) {
      /* we need to read the next long */
      if (offset < 0) {
        if (logElement.packing == 1) {
          offset = 22;
        }
        else if (logElement.packing == 2) {
          offset = 20;
        }

        if (logimage_read_uint(&pixel, logImage) != 0) {
          if (verbose) {
            printf("Cineon: EOF reached\n");
          }
          return 1;
        }
        pixel = swap_uint(pixel, logImage->isMSB);
      }
      data[y * logImage->width * logElement.depth + x] = float((pixel >> offset) & 0x3ff) /
                                                         1023.0f;
      offset -= 10;
    }
  }

  return 0;
}

static int logImageElementGetData10Packed(LogImageFile *logImage,
                                          const LogImageElement &logElement,
                                          float *data)
{
  size_t rowLength = getRowLength(logImage->width, logElement);
  uint pixel, oldPixel;

  /* converting bytes to pixels */
  for (size_t y = 0; y < logImage->height; y++) {
    /* seek to data */
    if (logimage_fseek(logImage, y * rowLength + logElement.dataOffset) != 0) {
      if (verbose) {
        printf("Cineon: Couldn't seek at %u\n", uint(y * rowLength + logElement.dataOffset));
      }
      return 1;
    }

    oldPixel = 0;
    int offset = 0;
    int offset2 = 0;

    for (size_t x = 0; x < logImage->width * logElement.depth; x++) {
      if (offset2 != 0) {
        offset = 10 - offset2;
        offset2 = 0;
        oldPixel = 0;
      }
      else if (offset == 32) {
        offset = 0;
      }
      else if (offset + 10 > 32) {
        /* next pixel is on two different longs */
        oldPixel = (pixel >> offset);
        offset2 = 32 - offset;
        offset = 0;
      }

      if (offset == 0) {
        /* we need to read the next long */
        if (logimage_read_uint(&pixel, logImage) != 0) {
          if (verbose) {
            printf("Cineon: EOF reached\n");
          }
          return 1;
        }
        pixel = swap_uint(pixel, logImage->isMSB);
      }
      data[y * logImage->width * logElement.depth + x] =
          float((((pixel << offset2) >> offset) & 0x3ff) | oldPixel) / 1023.0f;
      offset += 10;
    }
  }
  return 0;
}

static int logImageElementGetData12(LogImageFile *logImage,
                                    const LogImageElement &logElement,
                                    float *data)
{
  uint sampleIndex;
  uint numSamples = logImage->width * logImage->height * logElement.depth;
  ushort pixel;

  /* seek to data */
  if (logimage_fseek(logImage, logElement.dataOffset) != 0) {
    if (verbose) {
      printf("Cineon: Couldn't seek at %d\n", logElement.dataOffset);
    }
    return 1;
  }

  /* convert bytes to pixels */
  sampleIndex = 0;

  for (sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
    if (logimage_read_ushort(&pixel, logImage) != 0) {
      if (verbose) {
        printf("Cineon: EOF reached\n");
      }
      return 1;
    }
    pixel = swap_ushort(pixel, logImage->isMSB);

    if (logElement.packing == 1) { /* padded to the right */
      data[sampleIndex] = float(pixel >> 4) / 4095.0f;
    }
    else if (logElement.packing == 2) { /* padded to the left */
      data[sampleIndex] = float(pixel) / 4095.0f;
    }
  }
  return 0;
}

static int logImageElementGetData12Packed(LogImageFile *logImage,
                                          const LogImageElement &logElement,
                                          float *data)
{
  size_t rowLength = getRowLength(logImage->width, logElement);
  uint pixel, oldPixel;

  /* converting bytes to pixels */
  for (size_t y = 0; y < logImage->height; y++) {
    /* seek to data */
    if (logimage_fseek(logImage, y * rowLength + logElement.dataOffset) != 0) {
      if (verbose) {
        printf("Cineon: Couldn't seek at %u\n", uint(y * rowLength + logElement.dataOffset));
      }
      return 1;
    }

    oldPixel = 0;
    int offset = 0;
    int offset2 = 0;

    for (size_t x = 0; x < logImage->width * logElement.depth; x++) {
      if (offset2 != 0) {
        offset = 12 - offset2;
        offset2 = 0;
        oldPixel = 0;
      }
      else if (offset == 32) {
        offset = 0;
      }
      else if (offset + 12 > 32) {
        /* next pixel is on two different longs */
        oldPixel = (pixel >> offset);
        offset2 = 32 - offset;
        offset = 0;
      }

      if (offset == 0) {
        /* we need to read the next long */
        if (logimage_read_uint(&pixel, logImage) != 0) {
          if (verbose) {
            printf("Cineon: EOF reached\n");
          }
          return 1;
        }
        pixel = swap_uint(pixel, logImage->isMSB);
      }
      data[y * logImage->width * logElement.depth + x] =
          float((((pixel << offset2) >> offset) & 0xfff) | oldPixel) / 4095.0f;
      offset += 12;
    }
  }
  return 0;
}

static int logImageElementGetData16(LogImageFile *logImage,
                                    const LogImageElement &logElement,
                                    float *data)
{
  uint numSamples = logImage->width * logImage->height * logElement.depth;
  uint sampleIndex;
  ushort pixel;

  /* seek to data */
  if (logimage_fseek(logImage, logElement.dataOffset) != 0) {
    if (verbose) {
      printf("Cineon: Couldn't seek at %d\n", logElement.dataOffset);
    }
    return 1;
  }

  for (sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
    if (logimage_read_ushort(&pixel, logImage) != 0) {
      if (verbose) {
        printf("Cineon: EOF reached\n");
      }
      return 1;
    }
    pixel = swap_ushort(pixel, logImage->isMSB);
    data[sampleIndex] = float(pixel) / 65535.0f;
  }

  return 0;
}

/*
 * Color conversion
 */

static float *getLinToLogLut(const LogImageFile *logImage, const LogImageElement &logElement)
{
  float *lut;
  float gain, negativeFilmGamma, offset, step;
  uint lutsize = uint(logElement.maxValue + 1);
  uint i;

  lut = MEM_new_array_uninitialized<float>(lutsize, "getLinToLogLut");

  negativeFilmGamma = 0.6;
  step = logElement.refHighQuantity / logElement.maxValue;
  gain = logElement.maxValue /
         (1.0f - powf(10,
                      (logImage->referenceBlack - logImage->referenceWhite) * step /
                          negativeFilmGamma * logImage->gamma / 1.7f));
  offset = gain - logElement.maxValue;

  for (i = 0; i < lutsize; i++) {
    lut[i] = (logImage->referenceWhite +
              log10f(powf((i + offset) / gain, 1.7f / logImage->gamma)) /
                  (step / negativeFilmGamma)) /
             logElement.maxValue;
  }

  return lut;
}

static float *getLogToLinLut(const LogImageFile *logImage, const LogImageElement &logElement)
{
  float *lut;
  float breakPoint, gain, kneeGain, kneeOffset, negativeFilmGamma, offset, step, softClip;
  /* float filmGamma; unused */
  uint lutsize = uint(logElement.maxValue + 1);
  uint i;

  lut = MEM_new_array_uninitialized<float>(lutsize, "getLogToLinLut");

  /* Building the Log -> Lin LUT */
  step = logElement.refHighQuantity / logElement.maxValue;
  negativeFilmGamma = 0.6;

  /* these are default values */
  /* filmGamma = 2.2f;  unused */
  softClip = 0;

  breakPoint = logImage->referenceWhite - softClip;
  gain = logElement.maxValue /
         (1.0f - powf(10,
                      (logImage->referenceBlack - logImage->referenceWhite) * step /
                          negativeFilmGamma * logImage->gamma / 1.7f));
  offset = gain - logElement.maxValue;
  kneeOffset = powf(10,
                    (breakPoint - logImage->referenceWhite) * step / negativeFilmGamma *
                        logImage->gamma / 1.7f) *
                   gain -
               offset;
  kneeGain = (logElement.maxValue - kneeOffset) / powf(5 * softClip, softClip / 100);

  for (i = 0; i < lutsize; i++) {
    if (i < logImage->referenceBlack) {
      lut[i] = 0.0f;
    }
    else if (i > breakPoint) {
      lut[i] = (powf(i - breakPoint, softClip / 100) * kneeGain + kneeOffset) /
               logElement.maxValue;
    }
    else {
      lut[i] = (powf(10,
                     (float(i) - logImage->referenceWhite) * step / negativeFilmGamma *
                         logImage->gamma / 1.7f) *
                    gain -
                offset) /
               logElement.maxValue;
    }
  }

  return lut;
}

static float *getLinToSrgbLut(const LogImageElement &logElement)
{
  float col, *lut;
  uint lutsize = uint(logElement.maxValue + 1);
  uint i;

  lut = MEM_new_array_uninitialized<float>(lutsize, "getLogToLinLut");

  for (i = 0; i < lutsize; i++) {
    col = float(i) / logElement.maxValue;
    if (col < 0.0031308f) {
      lut[i] = (col < 0.0f) ? 0.0f : col * 12.92f;
    }
    else {
      lut[i] = 1.055f * powf(col, 1.0f / 2.4f) - 0.055f;
    }
  }

  return lut;
}

static float *getSrgbToLinLut(const LogImageElement &logElement)
{
  float col, *lut;
  uint lutsize = uint(logElement.maxValue + 1);
  uint i;

  lut = MEM_new_array_uninitialized<float>(lutsize, "getLogToLinLut");

  for (i = 0; i < lutsize; i++) {
    col = float(i) / logElement.maxValue;
    if (col < 0.04045f) {
      lut[i] = (col < 0.0f) ? 0.0f : col * (1.0f / 12.92f);
    }
    else {
      lut[i] = powf((col + 0.055f) * (1.0f / 1.055f), 2.4f);
    }
  }

  return lut;
}

/* RGB elements always use the printing density transfer. */
static void convertRGBA_RGB(const float *src,
                            float *dst,
                            const LogImageFile *logImage,
                            const LogImageElement &logElement)
{
  uint i;
  const float *src_ptr = src;
  float *dst_ptr = dst;
  float *lut = getLinToLogLut(logImage, logElement);

  for (i = 0; i < logImage->width * logImage->height; i++) {
    *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
    *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
    *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
    src_ptr++;
  }

  MEM_delete(lut);
}

static void convertRGB_RGBA(const float *src,
                            float *dst,
                            const LogImageFile *logImage,
                            const LogImageElement &logElement)
{
  uint i;
  const float *src_ptr = src;
  float *dst_ptr = dst;
  float *lut = getLogToLinLut(logImage, logElement);

  for (i = 0; i < logImage->width * logImage->height; i++) {
    *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
    *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
    *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
    *(dst_ptr++) = 1.0f;
  }

  MEM_delete(lut);
}

/* Luminance elements always use the linear transfer. */
static void convertLuminance_RGBA(const float *src,
                                  float *dst,
                                  const LogImageFile *logImage,
                                  const LogImageElement &logElement)
{
  uint i;
  float value;
  const float *src_ptr = src;
  float *dst_ptr = dst;

  const float refHighData = float(logElement.refHighData) / logElement.maxValue;
  const float refLowData = float(logElement.refLowData) / logElement.maxValue;
  const float scale = 1.0f / (refHighData - refLowData);

  for (i = 0; i < logImage->width * logImage->height; i++) {
    value = clamp_float((*(src_ptr++) - refLowData) * scale, 0.0f, 1.0f);
    *(dst_ptr++) = value;
    *(dst_ptr++) = value;
    *(dst_ptr++) = value;
    *(dst_ptr++) = 1.0f;
  }
}

static int convertLogElementToRGBA(const float *src,
                                   float *dst,
                                   const LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   int dstIsLinearRGB)
{
  uint i;
  float *src_ptr;
  float *dst_ptr;

  /* Convert data in src to linear RGBA in dst */
  switch (logElement.descriptor) {
    case descriptor_RGB:
      convertRGB_RGBA(src, dst, logImage, logElement);
      break;

    case descriptor_Luminance:
      convertLuminance_RGBA(src, dst, logImage, logElement);
      break;

    default:
      return 1;
  }

  if (dstIsLinearRGB) {
    /* convert data from sRGB to Linear RGB via lut */
    float *lut = getSrgbToLinLut(logElement);
    src_ptr = dst; /* no error here */
    dst_ptr = dst;
    for (i = 0; i < logImage->width * logImage->height; i++) {
      *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
      *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
      *(dst_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
      dst_ptr++;
      src_ptr++;
    }
    MEM_delete(lut);
  }
  return 0;
}

static int convertRGBAToLogElement(const float *src,
                                   float *dst,
                                   const LogImageFile *logImage,
                                   const LogImageElement &logElement,
                                   int srcIsLinearRGB)
{
  uint i;
  const float *srgbSrc;
  float *srgbSrc_alloc;
  float *srgbSrc_ptr;
  const float *src_ptr = src;
  float *lut;

  if (srcIsLinearRGB != 0) {
    /* we need to convert src to sRGB */
    srgbSrc_alloc = static_cast<float *>(
        imb_alloc_pixels(logImage->width, logImage->height, 4, sizeof(float), false, __func__));
    if (srgbSrc_alloc == nullptr) {
      return 1;
    }

    memcpy(srgbSrc_alloc,
           src,
           4 * size_t(logImage->width) * size_t(logImage->height) * sizeof(float));
    srgbSrc_ptr = srgbSrc_alloc;

    /* convert data from Linear RGB to sRGB via lut */
    lut = getLinToSrgbLut(logElement);
    for (i = 0; i < logImage->width * logImage->height; i++) {
      *(srgbSrc_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
      *(srgbSrc_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
      *(srgbSrc_ptr++) = lut[float_uint(*(src_ptr++), logElement.maxValue)];
      srgbSrc_ptr++;
      src_ptr++;
    }
    MEM_delete(lut);
    srgbSrc = srgbSrc_alloc;
  }
  else {
    srgbSrc = src;
  }

  /* Convert linear RGBA data in src to the RGB element in dst. */
  BLI_assert(logElement.descriptor == descriptor_RGB);
  convertRGBA_RGB(srgbSrc, dst, logImage, logElement);

  if (srcIsLinearRGB != 0) {
    MEM_delete(srgbSrc_alloc);
  }

  return 0;
}

}  // namespace blender

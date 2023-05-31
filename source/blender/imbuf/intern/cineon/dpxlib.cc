/* SPDX-FileCopyrightText: 1999-2002 David Hodson <hodsond@acm.org>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbcineon
 *
 * DPX image file format library routines.
 */

#include "dpxlib.h"
#include "logmemfile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "BLI_fileops.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

/*
 * For debug purpose
 */

static int verbose = 0;

void dpxSetVerbose(int verbosity)
{
  verbose = verbosity;
}

/*
 * Headers
 */

static void fillDpxMainHeader(LogImageFile *dpx,
                              DpxMainHeader *header,
                              const char *filename,
                              const char *creator)
{
  time_t fileClock;
  struct tm *fileTime;

  memset(header, 0, sizeof(DpxMainHeader));

  /* --- File header --- */
  header->fileHeader.magic_num = swap_uint(DPX_FILE_MAGIC, dpx->isMSB);
  header->fileHeader.offset = swap_uint(dpx->element[0].dataOffset, dpx->isMSB);
  strcpy(header->fileHeader.version, "V2.0");
  header->fileHeader.file_size = swap_uint(
      dpx->element[0].dataOffset + dpx->height * getRowLength(dpx->width, dpx->element[0]),
      dpx->isMSB);
  header->fileHeader.ditto_key = 0;
  header->fileHeader.gen_hdr_size = swap_uint(
      sizeof(DpxFileHeader) + sizeof(DpxImageHeader) + sizeof(DpxOrientationHeader), dpx->isMSB);
  header->fileHeader.ind_hdr_size = swap_uint(sizeof(DpxFilmHeader) + sizeof(DpxTelevisionHeader),
                                              dpx->isMSB);
  header->fileHeader.user_data_size = DPX_UNDEFINED_U32;
  STRNCPY(header->fileHeader.file_name, filename);
  fileClock = time(nullptr);
  fileTime = localtime(&fileClock);
  strftime(header->fileHeader.creation_date, 24, "%Y:%m:%d:%H:%M:%S%Z", fileTime);
  header->fileHeader.creation_date[23] = 0;
  STRNCPY(header->fileHeader.creator, creator);
  header->fileHeader.project[0] = 0;
  header->fileHeader.copyright[0] = 0;
  header->fileHeader.key = 0xFFFFFFFF;

  /* --- Image header --- */
  header->imageHeader.orientation = 0;
  header->imageHeader.elements_per_image = swap_ushort(1, dpx->isMSB);
  header->imageHeader.pixels_per_line = swap_uint(dpx->width, dpx->isMSB);
  header->imageHeader.lines_per_element = swap_uint(dpx->height, dpx->isMSB);

  /* Fills element */
  header->imageHeader.element[0].data_sign = 0;
  header->imageHeader.element[0].ref_low_data = swap_uint(dpx->element[0].refLowData, dpx->isMSB);
  header->imageHeader.element[0].ref_low_quantity = swap_float(dpx->element[0].refLowQuantity,
                                                               dpx->isMSB);
  header->imageHeader.element[0].ref_high_data = swap_uint(dpx->element[0].refHighData,
                                                           dpx->isMSB);
  header->imageHeader.element[0].ref_high_quantity = swap_float(dpx->element[0].refHighQuantity,
                                                                dpx->isMSB);
  header->imageHeader.element[0].descriptor = dpx->element[0].descriptor;
  header->imageHeader.element[0].transfer = dpx->element[0].transfer;
  header->imageHeader.element[0].colorimetric = 0;
  header->imageHeader.element[0].bits_per_sample = dpx->element[0].bitsPerSample;
  header->imageHeader.element[0].packing = swap_ushort(dpx->element[0].packing, dpx->isMSB);
  header->imageHeader.element[0].encoding = 0;
  header->imageHeader.element[0].data_offset = swap_uint(dpx->element[0].dataOffset, dpx->isMSB);
  header->imageHeader.element[0].line_padding = 0;
  header->imageHeader.element[0].element_padding = 0;
  header->imageHeader.element[0].description[0] = 0;

  /* --- Orientation header --- */
  /* we leave it blank */

  /* --- Television header --- */
  header->televisionHeader.time_code = DPX_UNDEFINED_U32;
  header->televisionHeader.user_bits = DPX_UNDEFINED_U32;
  header->televisionHeader.interlace = DPX_UNDEFINED_U8;
  header->televisionHeader.field_number = DPX_UNDEFINED_U8;
  header->televisionHeader.video_signal = DPX_UNDEFINED_U8;
  header->televisionHeader.padding = DPX_UNDEFINED_U8;
  header->televisionHeader.horizontal_sample_rate = swap_float(DPX_UNDEFINED_R32, dpx->isMSB);
  header->televisionHeader.vertical_sample_rate = swap_float(DPX_UNDEFINED_R32, dpx->isMSB);
  header->televisionHeader.frame_rate = swap_float(DPX_UNDEFINED_R32, dpx->isMSB);
  header->televisionHeader.time_offset = swap_float(DPX_UNDEFINED_R32, dpx->isMSB);
  header->televisionHeader.gamma = swap_float(dpx->gamma, dpx->isMSB);
  header->televisionHeader.black_level = swap_float(dpx->referenceBlack, dpx->isMSB);
  header->televisionHeader.black_gain = swap_float(DPX_UNDEFINED_R32, dpx->isMSB);
  header->televisionHeader.breakpoint = swap_float(DPX_UNDEFINED_R32, dpx->isMSB);
  header->televisionHeader.white_level = swap_float(dpx->referenceWhite, dpx->isMSB);
  header->televisionHeader.integration_times = swap_float(DPX_UNDEFINED_R32, dpx->isMSB);
}

LogImageFile *dpxOpen(const uchar *byteStuff, int fromMemory, size_t bufferSize)
{
  DpxMainHeader header;
  LogImageFile *dpx = (LogImageFile *)MEM_mallocN(sizeof(LogImageFile), __func__);
  const char *filepath = (const char *)byteStuff;
  int i;

  if (dpx == nullptr) {
    if (verbose) {
      printf("DPX: Failed to malloc dpx file structure.\n");
    }
    return nullptr;
  }

  /* zero the header */
  memset(&header, 0, sizeof(DpxMainHeader));

  /* for close routine */
  dpx->file = nullptr;

  if (fromMemory == 0) {
    /* byteStuff is then the filepath */
    dpx->file = BLI_fopen(filepath, "rb");
    if (dpx->file == nullptr) {
      if (verbose) {
        printf("DPX: Failed to open file \"%s\".\n", filepath);
      }
      logImageClose(dpx);
      return nullptr;
    }
    /* not used in this case */
    dpx->memBuffer = nullptr;
    dpx->memCursor = nullptr;
    dpx->memBufferSize = 0;
  }
  else {
    dpx->memBuffer = (uchar *)byteStuff;
    dpx->memCursor = (uchar *)byteStuff;
    dpx->memBufferSize = bufferSize;
  }

  if (logimage_fread(&header, sizeof(header), 1, dpx) == 0) {
    if (verbose) {
      printf("DPX: Not enough data for header in \"%s\".\n", byteStuff);
    }
    logImageClose(dpx);
    return nullptr;
  }

  /* endianness determination */
  if (header.fileHeader.magic_num == swap_uint(DPX_FILE_MAGIC, 1)) {
    dpx->isMSB = 1;
    if (verbose) {
      printf("DPX: File is MSB.\n");
    }
  }
  else if (header.fileHeader.magic_num == DPX_FILE_MAGIC) {
    dpx->isMSB = 0;
    if (verbose) {
      printf("DPX: File is LSB.\n");
    }
  }
  else {
    if (verbose) {
      printf("DPX: Bad magic number %u in \"%s\".\n", header.fileHeader.magic_num, byteStuff);
    }
    logImageClose(dpx);
    return nullptr;
  }

  dpx->srcFormat = format_DPX;
  dpx->numElements = swap_ushort(header.imageHeader.elements_per_image, dpx->isMSB);
  size_t max_elements = ARRAY_SIZE(header.imageHeader.element);
  if (dpx->numElements == 0 || dpx->numElements >= max_elements) {
    if (verbose) {
      printf("DPX: Wrong number of elements: %d\n", dpx->numElements);
    }
    logImageClose(dpx);
    return nullptr;
  }

  dpx->width = swap_uint(header.imageHeader.pixels_per_line, dpx->isMSB);
  dpx->height = swap_uint(header.imageHeader.lines_per_element, dpx->isMSB);

  if (dpx->width == 0 || dpx->height == 0) {
    if (verbose) {
      printf("DPX: Wrong image dimension: %dx%d\n", dpx->width, dpx->height);
    }
    logImageClose(dpx);
    return nullptr;
  }

  dpx->depth = 0;

  for (i = 0; i < dpx->numElements; i++) {
    dpx->element[i].descriptor = header.imageHeader.element[i].descriptor;

    switch (dpx->element[i].descriptor) {
      case descriptor_Red:
      case descriptor_Green:
      case descriptor_Blue:
      case descriptor_Alpha:
      case descriptor_Luminance:
      case descriptor_Chrominance:
        dpx->depth++;
        dpx->element[i].depth = 1;
        break;

      case descriptor_CbYCrY:
        dpx->depth += 2;
        dpx->element[i].depth = 2;
        break;

      case descriptor_RGB:
      case descriptor_CbYCr:
      case descriptor_CbYACrYA:
        dpx->depth += 3;
        dpx->element[i].depth = 3;
        break;

      case descriptor_RGBA:
      case descriptor_ABGR:
      case descriptor_CbYCrA:
        dpx->depth += 4;
        dpx->element[i].depth = 4;
        break;

      case descriptor_Depth:
      case descriptor_Composite:
        /* unsupported */
        break;
    }

    if (dpx->depth == 0 || dpx->depth > 4) {
      if (verbose) {
        printf("DPX: Unsupported image depth: %d\n", dpx->depth);
      }
      logImageClose(dpx);
      return nullptr;
    }

    dpx->element[i].bitsPerSample = header.imageHeader.element[i].bits_per_sample;
    if (!ELEM(dpx->element[i].bitsPerSample, 1, 8, 10, 12, 16)) {
      if (verbose) {
        printf("DPX: Unsupported bitsPerSample for elements %d: %d\n",
               i,
               dpx->element[i].bitsPerSample);
      }
      logImageClose(dpx);
      return nullptr;
    }

    dpx->element[i].maxValue = powf(2, dpx->element[i].bitsPerSample) - 1.0f;

    dpx->element[i].packing = swap_ushort(header.imageHeader.element[i].packing, dpx->isMSB);
    if (dpx->element[i].packing > 2) {
      if (verbose) {
        printf("DPX: Unsupported packing for element %d: %d\n", i, dpx->element[i].packing);
      }
      logImageClose(dpx);
      return nullptr;
    }

    /* Sometimes, the offset is not set correctly in the header */
    dpx->element[i].dataOffset = swap_uint(header.imageHeader.element[i].data_offset, dpx->isMSB);
    if (dpx->element[i].dataOffset == 0 && dpx->numElements == 1) {
      dpx->element[i].dataOffset = swap_uint(header.fileHeader.offset, dpx->isMSB);
    }

    if (dpx->element[i].dataOffset == 0) {
      if (verbose) {
        printf("DPX: Image header is corrupted.\n");
      }
      logImageClose(dpx);
      return nullptr;
    }

    dpx->element[i].transfer = header.imageHeader.element[i].transfer;

    /* if undefined, assign default */
    dpx->element[i].refLowData = swap_uint(header.imageHeader.element[i].ref_low_data, dpx->isMSB);
    dpx->element[i].refLowQuantity = swap_float(header.imageHeader.element[i].ref_low_quantity,
                                                dpx->isMSB);
    dpx->element[i].refHighData = swap_uint(header.imageHeader.element[i].ref_high_data,
                                            dpx->isMSB);
    dpx->element[i].refHighQuantity = swap_float(header.imageHeader.element[i].ref_high_quantity,
                                                 dpx->isMSB);

    switch (dpx->element[i].descriptor) {
      case descriptor_Red:
      case descriptor_Green:
      case descriptor_Blue:
      case descriptor_Alpha:
      case descriptor_RGB:
      case descriptor_RGBA:
      case descriptor_ABGR:
        if (dpx->element[i].refLowData == DPX_UNDEFINED_U32) {
          dpx->element[i].refLowData = 0;
        }

        if (dpx->element[i].refHighData == DPX_UNDEFINED_U32) {
          dpx->element[i].refHighData = uint(dpx->element[i].maxValue);
        }

        if (IS_DPX_UNDEFINED_R32(dpx->element[i].refLowQuantity)) {
          dpx->element[i].refLowQuantity = 0.0f;
        }

        if (IS_DPX_UNDEFINED_R32(dpx->element[i].refHighQuantity)) {
          if (ELEM(dpx->element[i].transfer, transfer_PrintingDensity, transfer_Logarithmic)) {
            dpx->element[i].refHighQuantity = 2.048f;
          }
          else {
            dpx->element[i].refHighQuantity = dpx->element[i].maxValue;
          }
        }

        break;

      case descriptor_Luminance:
      case descriptor_Chrominance:
      case descriptor_CbYCrY:
      case descriptor_CbYCr:
      case descriptor_CbYACrYA:
      case descriptor_CbYCrA:
        if (dpx->element[i].refLowData == DPX_UNDEFINED_U32) {
          dpx->element[i].refLowData = 16.0f / 255.0f * dpx->element[i].maxValue;
        }

        if (dpx->element[i].refHighData == DPX_UNDEFINED_U32) {
          dpx->element[i].refHighData = 235.0f / 255.0f * dpx->element[i].maxValue;
        }

        if (IS_DPX_UNDEFINED_R32(dpx->element[i].refLowQuantity)) {
          dpx->element[i].refLowQuantity = 0.0f;
        }

        if (IS_DPX_UNDEFINED_R32(dpx->element[i].refHighQuantity)) {
          dpx->element[i].refHighQuantity = 0.7f;
        }

        break;

      default:
        break;
    }
  }

  dpx->referenceBlack = swap_float(header.televisionHeader.black_level, dpx->isMSB);
  dpx->referenceWhite = swap_float(header.televisionHeader.white_level, dpx->isMSB);
  dpx->gamma = swap_float(header.televisionHeader.gamma, dpx->isMSB);

  if (IS_DPX_UNDEFINED_R32(dpx->referenceBlack) ||
      (dpx->referenceWhite <= dpx->referenceBlack || IS_DPX_UNDEFINED_R32(dpx->referenceWhite)) ||
      (dpx->gamma <= 0 || IS_DPX_UNDEFINED_R32(dpx->gamma)))
  {
    dpx->referenceBlack = 95.0f / 1023.0f * dpx->element[0].maxValue;
    dpx->referenceWhite = 685.0f / 1023.0f * dpx->element[0].maxValue;
    dpx->gamma = 1.7f;
  }

  if (verbose) {
    printf("size %d x %d x %d elements\n", dpx->width, dpx->height, dpx->numElements);
    for (i = 0; i < dpx->numElements; i++) {
      printf(" Element %d:\n", i);
      printf("  Bits per sample: %d\n", dpx->element[i].bitsPerSample);
      printf("  Depth: %d\n", dpx->element[i].depth);
      printf("  Transfer characteristics: %d\n", dpx->element[i].transfer);
      printf("  Packing: %d\n", dpx->element[i].packing);
      printf("  Descriptor: %d\n", dpx->element[i].descriptor);
      printf("  Data offset: %d\n", dpx->element[i].dataOffset);
      printf("  Reference low data: %u\n", dpx->element[i].refLowData);
      printf("  Reference low quantity: %f\n", dpx->element[i].refLowQuantity);
      printf("  Reference high data: %u\n", dpx->element[i].refHighData);
      printf("  Reference high quantity: %f\n", dpx->element[i].refHighQuantity);
      printf("\n");
    }

    printf("Gamma: %f\n", dpx->gamma);
    printf("Reference black: %f\n", dpx->referenceBlack);
    printf("Reference white: %f\n", dpx->referenceWhite);
    printf("Orientation: %d\n", header.imageHeader.orientation);
    printf("----------------------------\n");
  }
  return dpx;
}

LogImageFile *dpxCreate(const char *filepath,
                        int width,
                        int height,
                        int bitsPerSample,
                        int hasAlpha,
                        int isLogarithmic,
                        int referenceWhite,
                        int referenceBlack,
                        float gamma,
                        const char *creator)
{
  DpxMainHeader header;
  const char *shortFilename = nullptr;
  uchar pad[6044];

  LogImageFile *dpx = (LogImageFile *)MEM_mallocN(sizeof(LogImageFile), __func__);
  if (dpx == nullptr) {
    if (verbose) {
      printf("DPX: Failed to malloc dpx file structure.\n");
    }
    return nullptr;
  }

  dpx->width = width;
  dpx->height = height;
  dpx->element[0].bitsPerSample = bitsPerSample;
  dpx->element[0].dataOffset = 8092;
  dpx->element[0].maxValue = powf(2, dpx->element[0].bitsPerSample) - 1.0f;
  dpx->isMSB = 1;
  dpx->numElements = 1;

  switch (bitsPerSample) {
    case 8:
    case 16:
      dpx->element[0].packing = 0;
      break;

    case 10:
    case 12:
      /* Packed Type A padding is the most common 10/12 bits format */
      dpx->element[0].packing = 1;
      break;

    default:
      if (verbose) {
        printf("DPX: bitsPerSample not supported: %d\n", bitsPerSample);
      }
      logImageClose(dpx);
      return nullptr;
  }

  if (hasAlpha == 0) {
    dpx->depth = 3;
    dpx->element[0].depth = 3;
    dpx->element[0].descriptor = descriptor_RGB;
  }
  else {
    dpx->depth = 4;
    dpx->element[0].depth = 4;
    dpx->element[0].descriptor = descriptor_RGBA;
  }

  if (isLogarithmic == 0) {
    dpx->element[0].transfer = transfer_Linear;
    dpx->element[0].refHighQuantity = dpx->element[0].maxValue;
  }
  else {
    dpx->element[0].transfer = transfer_PrintingDensity;
    dpx->element[0].refHighQuantity = 2.048f;
  }

  dpx->element[0].refLowQuantity = 0;
  dpx->element[0].refLowData = 0;
  dpx->element[0].refHighData = dpx->element[0].maxValue;

  if (referenceWhite > 0) {
    dpx->referenceWhite = referenceWhite;
  }
  else {
    dpx->referenceWhite = 685.0f / 1023.0f * dpx->element[0].maxValue;
  }

  if (referenceBlack > 0) {
    dpx->referenceBlack = referenceBlack;
  }
  else {
    dpx->referenceBlack = 95.0f / 1023.0f * dpx->element[0].maxValue;
  }

  if (gamma > 0.0f) {
    dpx->gamma = gamma;
  }
  else {
    dpx->gamma = 1.7f;
  }

  shortFilename = strrchr(filepath, PATHSEP_CHAR);
  if (shortFilename == nullptr) {
    shortFilename = filepath;
  }
  else {
    shortFilename++;
  }

  dpx->file = BLI_fopen(filepath, "wb");

  if (dpx->file == nullptr) {
    if (verbose) {
      printf("DPX: Couldn't open file %s\n", filepath);
    }
    logImageClose(dpx);
    return nullptr;
  }

  fillDpxMainHeader(dpx, &header, shortFilename, creator);

  if (fwrite(&header, sizeof(header), 1, dpx->file) == 0) {
    if (verbose) {
      printf("DPX: Couldn't write image header\n");
    }
    logImageClose(dpx);
    return nullptr;
  }

  /* Header should be rounded to next 8k block
   * 6044 = 8092 - sizeof(DpxMainHeader) */
  memset(&pad, 0, 6044);
  if (fwrite(&pad, 6044, 1, dpx->file) == 0) {
    if (verbose) {
      printf("DPX: Couldn't write image header\n");
    }
    logImageClose(dpx);
    return nullptr;
  }

  return dpx;
}

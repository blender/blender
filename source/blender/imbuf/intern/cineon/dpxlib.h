/*
 * DPX image file format library definitions.
 *
 * Copyright 1999 - 2002 David Hodson <hodsond@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Julien Enche.
 *
 */

/** \file blender/imbuf/intern/cineon/dpxlib.h
 *  \ingroup imbcineon
 */


#ifndef __DPX_LIB_H__
#define __DPX_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "logImageCore.h"

#define DPX_FILE_MAGIC      0x53445058
#define DPX_UNDEFINED_U8    0xFF
#define DPX_UNDEFINED_U16   0xFFFF
#define DPX_UNDEFINED_U32   0xFFFFFFFF
#define DPX_UNDEFINED_R32   0xFFFFFFFF
#define DPX_UNDEFINED_CHAR  0

typedef struct {
	unsigned int    magic_num;
	unsigned int    offset;
	char            version[8];
	unsigned int    file_size;
	unsigned int    ditto_key;
	unsigned int    gen_hdr_size;
	unsigned int    ind_hdr_size;
	unsigned int    user_data_size;
	char            file_name[100];
	char            creation_date[24];
	char            creator[100];
	char            project[200];
	char            copyright[200];
	unsigned int    key;
	char            reserved[104];
} DpxFileHeader;

typedef struct {
	unsigned int    data_sign;
	unsigned int    ref_low_data;
	float           ref_low_quantity;
	unsigned int    ref_high_data;
	float           ref_high_quantity;
	unsigned char   descriptor;
	unsigned char   transfer;
	unsigned char   colorimetric;
	unsigned char   bits_per_sample;
	unsigned short  packing;
	unsigned short  encoding;
	unsigned int    data_offset;
	unsigned int    line_padding;
	unsigned int    element_padding;
	char            description[32];
} DpxElementHeader;

typedef struct {
	unsigned short      orientation;
	unsigned short      elements_per_image;
	unsigned int        pixels_per_line;
	unsigned int        lines_per_element;
	DpxElementHeader    element[8];
	char                reserved[52];
} DpxImageHeader;

typedef struct {
	unsigned int    x_offset;
	unsigned int    y_offset;
	float           x_center;
	float           y_center;
	unsigned int    x_original_size;
	unsigned int    y_original_size;
	char            file_name[100];
	char            creation_time[24];
	char            input_device[32];
	char            input_serial_number[32];
	unsigned short  border_validity[4];
	unsigned int    pixel_aspect_ratio[2];
	char            reserved[28];
} DpxOrientationHeader;

typedef struct {
	char            film_manufacturer_id[2];
	char            film_type[2];
	char            edge_code_perforation_offset[2];
	char            edge_code_prefix[6];
	char            edge_code_count[4];
	char            film_format[32];
	unsigned int    frame_position;
	unsigned int    sequence_length;
	unsigned int    held_count;
	float           frame_rate;
	float           shutter_angle;
	char            frame_identification[32];
	char            slate_info[100];
	char            reserved[56];
} DpxFilmHeader;

typedef struct {
	unsigned int    time_code;
	unsigned int    user_bits;
	unsigned char   interlace;
	unsigned char   field_number;
	unsigned char   video_signal;
	unsigned char   padding;
	float           horizontal_sample_rate;
	float           vertical_sample_rate;
	float           frame_rate;
	float           time_offset;
	float           gamma;
	float           black_level;
	float           black_gain;
	float           breakpoint;
	float           white_level;
	float           integration_times;
	unsigned char   reserved[76];
} DpxTelevisionHeader;


typedef struct {
	DpxFileHeader           fileHeader;
	DpxImageHeader          imageHeader;
	DpxOrientationHeader    orientationHeader;
	DpxFilmHeader           filmHeader;
	DpxTelevisionHeader     televisionHeader;
} DpxMainHeader;

void dpxSetVerbose(int verbosity);
LogImageFile *dpxOpen(const unsigned char *byteStuff, int fromMemory, size_t bufferSize);
LogImageFile *dpxCreate(const char *filename, int width, int height, int bitsPerSample, int hasAlpha, int isLogarithmic, int referenceWhite, int referenceBlack, float gamma, const char *creator);

#ifdef __cplusplus
}
#endif

#endif

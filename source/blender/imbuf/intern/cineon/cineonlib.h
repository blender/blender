/*
 * Cineon image file format library definitions.
 * Also handles DPX files (almost)
 *
 * Copyright 1999,2000,2001 David Hodson <hodsond@acm.org>
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

/** \file blender/imbuf/intern/cineon/cineonlib.h
 *  \ingroup imbcineon
 */


#ifndef __CINEON_LIB_H__
#define __CINEON_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "logImageCore.h"

#define CINEON_FILE_MAGIC       0x802A5FD7
#define CINEON_UNDEFINED_U8     0xFF
#define CINEON_UNDEFINED_U16    0xFFFF
#define CINEON_UNDEFINED_U32    0xFFFFFFFF
#define CINEON_UNDEFINED_R32    0x7F800000
#define CINEON_UNDEFINED_CHAR   0

typedef struct {
	unsigned int    magic_num;
	unsigned int    offset;
	unsigned int    gen_hdr_size;
	unsigned int    ind_hdr_size;
	unsigned int    user_data_size;
	unsigned int    file_size;
	char            version[8];
	char            file_name[100];
	char            creation_date[12];
	char            creation_time[12];
	char            reserved[36];
} CineonFileHeader;

typedef struct {
	unsigned char   descriptor1;
	unsigned char   descriptor2;
	unsigned char   bits_per_sample;
	unsigned char   filler;
	unsigned int    pixels_per_line;
	unsigned int    lines_per_image;
	unsigned int    ref_low_data;
	float           ref_low_quantity;
	unsigned int    ref_high_data;
	float           ref_high_quantity;
} CineonElementHeader;

typedef struct {
	unsigned char       orientation;
	unsigned char       elements_per_image;
	unsigned short      filler;
	CineonElementHeader element[8];
	float               white_point_x;
	float               white_point_y;
	float               red_primary_x;
	float               red_primary_y;
	float               green_primary_x;
	float               green_primary_y;
	float               blue_primary_x;
	float               blue_primary_y;
	char                label[200];
	char                reserved[28];
	unsigned char       interleave;
	unsigned char       packing;
	unsigned char       data_sign;
	unsigned char       sense;
	unsigned int        line_padding;
	unsigned int        element_padding;
	char                reserved2[20];
} CineonImageHeader;

typedef struct {
	int     x_offset;
	int     y_offset;
	char    file_name[100];
	char    creation_date[12];
	char    creation_time[12];
	char    input_device[64];
	char    model_number[32];
	char    input_serial_number[32];
	float   x_input_samples_per_mm;
	float   y_input_samples_per_mm;
	float   input_device_gamma;
	char    reserved[40];
} CineonOriginationHeader;

typedef struct {
	unsigned char   film_code;
	unsigned char   film_type;
	unsigned char   edge_code_perforation_offset;
	unsigned char   filler;
	unsigned int    prefix;
	unsigned int    count;
	char            format[32];
	unsigned int    frame_position;
	float           frame_rate;
	char            attribute[32];
	char            slate[200];
	char            reserved[740];
} CineonFilmHeader;

typedef struct {
	CineonFileHeader        fileHeader;
	CineonImageHeader       imageHeader;
	CineonOriginationHeader originationHeader;
	CineonFilmHeader        filmHeader;
} CineonMainHeader;

void cineonSetVerbose(int);
LogImageFile *cineonOpen(const unsigned char *byteStuff, int fromMemory, size_t bufferSize);
LogImageFile *cineonCreate(const char *filename, int width, int height, int bitsPerSample, const char *creator);

#ifdef __cplusplus
}
#endif

#endif  /* __CINEON_LIB_H__ */

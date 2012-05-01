/*
 *	 Cineon image file format library definitions.
 *	 Cineon file format structures.
 *
 *	 This header file contains private details.
 *	 User code should generally use cineonlib.h only.
 *
 *	 Copyright 1999,2000,2001 David Hodson <hodsond@acm.org>
 *
 *	 This program is free software; you can redistribute it and/or modify it
 *	 under the terms of the GNU General Public License as published by the Free
 *	 Software Foundation; either version 2 of the License, or (at your option)
 *	 any later version.
 *
 *	 This program is distributed in the hope that it will be useful, but
 *	 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *	 or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU General Public License
 *	 for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the Free Software
 *	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __CINEONFILE_H__
#define __CINEONFILE_H__

/** \file blender/imbuf/intern/cineon/cineonfile.h
 *  \ingroup imbcineon
 */

#include "logImageCore.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
		U32		magic_num;				/* magic number */
		U32		image_offset;			/* offset to image data in bytes */
		U32		gen_hdr_size;			/* generic header length in bytes */
		U32		ind_hdr_size;			/* industry header length in bytes */
		U32		user_data_size;		/* user-defined data length in bytes */
		U32		file_size;				/* file size in bytes */
		ASCII vers[8];					/* which header format version is being used (v4.5) */
		ASCII file_name[100];		/* image file name */
		ASCII create_date[12];	/* file creation date */
		ASCII create_time[12];	/* file creation time */
		ASCII Reserved[36];			/* reserved field TBD (need to pad) */
} CineonFileInformation;

typedef struct {
		U8		 designator1;
		U8		 designator2;
		U8		 bits_per_pixel;
		U8		 filler;
		U32		 pixels_per_line;
		U32		 lines_per_image;
		U32		 ref_low_data;		 /* reference low data code value */
		R32		 ref_low_quantity; /* reference low quantity represented */
		U32		 ref_high_data;		 /* reference high data code value */
		R32		 ref_high_quantity;/* reference high quantity represented */
} CineonChannelInformation;

typedef struct {
		U8		 orientation;					 /* image orientation */
		U8		 channels_per_image;
		U16		 filler;
		CineonChannelInformation channel[8];
		R32		 white_point_x;
		R32		 white_point_y;
		R32		 red_primary_x;
		R32		 red_primary_y;
		R32		 green_primary_x;
		R32		 green_primary_y;
		R32		 blue_primary_x;
		R32		 blue_primary_y;
		ASCII	 label[200];
		ASCII	 reserved[28];
} CineonImageInformation;

typedef struct {
		U8		interleave;
		U8		packing;
		U8		signage;
		U8		sense;
		U32		line_padding;
		U32		channel_padding;
		ASCII reserved[20];
} CineonFormatInformation;

typedef struct {
		S32		x_offset;
		S32		y_offset;
		ASCII file_name[100];
		ASCII create_date[12];	/* file creation date */
		ASCII create_time[12];	/* file creation time */
		ASCII input_device[64];
		ASCII model_number[32];
		ASCII serial_number[32];
		R32		x_input_samples_per_mm;
		R32		y_input_samples_per_mm;
		R32		input_device_gamma;
		ASCII reserved[40];
} CineonOriginationInformation;

typedef struct {
	CineonFileInformation fileInfo;
	CineonImageInformation imageInfo;
	CineonFormatInformation formatInfo;
	CineonOriginationInformation originInfo;
} CineonGenericHeader;

typedef struct {
	U8 filmCode;
	U8 filmType;
	U8 perfOffset;
	U8 filler;
	U32 keycodePrefix;
	U32 keycodeCount;
	ASCII format[32];
	U32 framePosition; /* in sequence */
	R32 frameRate; /* frames per second */
	ASCII attribute[32];
	ASCII slate[200];
	ASCII reserved[740];
} CineonMPISpecificInformation;

#ifdef __cplusplus
}
#endif

#endif /* __CINEONFILE_H__ */

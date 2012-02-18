/** \file blender/imbuf/intern/cineon/dpxfile.h
 *  \ingroup imbcineon
 */
/*
 *	 Cineon image file format library definitions.
 *	 Dpx file format structures.
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

#ifndef __DPXFILE_H__
#define __DPXFILE_H__

#include "logImageCore.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
		U32		magic_num;				/* magic number */
		U32		offset;						/* offset to image data in bytes */
		ASCII vers[8];					/* which header format version is being used (v1.0) */
		U32		file_size;				/* file size in bytes */
		U32		ditto_key;				/* I bet some people use this */
		U32		gen_hdr_size;			/* generic header length in bytes */
		U32		ind_hdr_size;			/* industry header length in bytes */
		U32		user_data_size;		/* user-defined data length in bytes */
		ASCII file_name[100];		/* image file name */
		ASCII create_date[24];	/* file creation date, yyyy:mm:dd:hh:mm:ss:LTZ */
		ASCII creator[100];
		ASCII project[200];
		ASCII copyright[200];
		U32		key;							/* encryption key, FFFFFFF = unencrypted */
		ASCII Reserved[104];		/* reserved field TBD (need to pad) */
} DpxFileInformation;

typedef struct {
		U32		 signage;
		U32		 ref_low_data;		 /* reference low data code value */
		R32		 ref_low_quantity; /* reference low quantity represented */
		U32		 ref_high_data;		 /* reference high data code value */
		R32		 ref_high_quantity;/* reference high quantity represented */
		U8		 designator1;
		U8		 transfer_characteristics;
		U8		 colourimetry;
		U8		 bits_per_pixel;
		U16		 packing;
		U16		 encoding;
		U32		 data_offset;
		U32		 line_padding;
		U32		 channel_padding;
		ASCII	 description[32];
} DpxChannelInformation;

typedef struct {
		U16		 orientation;
		U16		 channels_per_image;
		U32		 pixels_per_line;
		U32		 lines_per_image;
		DpxChannelInformation channel[8];
		ASCII	 reserved[52];
} DpxImageInformation;

typedef struct {
		U32		x_offset;
		U32		y_offset;
		R32		x_centre;
		R32		y_centre;
		U32		x_original_size;
		U32		y_original_size;
		ASCII file_name[100];
		ASCII creation_time[24];
		ASCII input_device[32];
		ASCII input_serial_number[32];
		U16		border_validity[4];
		U32		pixel_aspect_ratio[2]; /* h:v */
		ASCII reserved[28];
} DpxOriginationInformation;

typedef struct {
		ASCII film_manufacturer_id[2];
		ASCII film_type[2];
		ASCII edge_code_perforation_offset[2];
		ASCII edge_code_prefix[6];
		ASCII edge_code_count[4];
		ASCII film_format[32];
		U32		frame_position;
		U32		sequence_length;
		U32		held_count;
		R32		frame_rate;
		R32		shutter_angle;
		ASCII frame_identification[32];
		ASCII slate_info[100];
		ASCII reserved[56];
} DpxMPIInformation;

typedef struct {
	DpxFileInformation fileInfo;
	DpxImageInformation imageInfo;
	DpxOriginationInformation originInfo;
	DpxMPIInformation filmHeader;
} DpxMainHeader;

#ifdef __cplusplus
}
#endif

#endif /* __DPXFILE_H__ */

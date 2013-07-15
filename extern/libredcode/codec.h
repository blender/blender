/* ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2008 Peter Schlaile
 *
 * This file is part of libredcode.
 *
 * Libredcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Libredcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Libredcode; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****/

#ifndef __CODEC_H__
#define __CODEC_H__

struct redcode_frame;

struct redcode_frame_raw {
	void * data;
	int width;
	int height;
};

/* do the JPEG2000 decompression into YCbCrY planes */
struct redcode_frame_raw * redcode_decode_video_raw(
	struct redcode_frame * frame, int scale);

/* finally decode RAW frame into out-buffer (which has to be allocated
   in advance)

   Keep in mind: frame_raw-width + height is half sized. 
   (one pixel contains 2x2 bayer-sensor data)

   output-buffer should have room for

   scale = 1 : width * height * 4 * 4 * sizeof(float)
   scale = 2 : width * height * 4 * sizeof(float)
   scale = 4 : width * height * sizeof(float)

*/

int redcode_decode_video_float(struct redcode_frame_raw * frame, 
			       float * out, int scale);


#endif

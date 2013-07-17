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

#ifndef __FORMAT_H__
#define __FORMAT_H__

struct redcode_handle;
struct redcode_frame {
	unsigned int length;
	unsigned int offset;
	unsigned char * data;
};

struct redcode_handle * redcode_open(const char * fname);
void redcode_close(struct redcode_handle * handle);

long redcode_get_length(struct redcode_handle * handle);

struct redcode_frame * redcode_read_video_frame(
	struct redcode_handle * handle, long frame);
struct redcode_frame * redcode_read_audio_frame(
	struct redcode_handle * handle, long frame);

void redcode_free_frame(struct redcode_frame * frame);


#endif

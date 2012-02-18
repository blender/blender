/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Peter Schlaile
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#ifndef __IMB_INDEXER_H__
#define __IMB_INDEXER_H__

#ifdef WIN32
#  include <io.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include "BKE_utildefines.h"
#include "IMB_anim.h"

/*
  separate animation index files to solve the following problems:

  a) different timecodes within one file (like DTS/PTS, Timecode-Track, 
     "implicit" timecodes within DV-files and HDV-files etc.)
  b) seeking difficulties within ffmpeg for files with timestamp holes
  c) broken files that miss several frames / have varying framerates
  d) use proxies accordingly

  ... we need index files, that provide us with 
  
  the binary(!) position, where we have to seek into the file *and*
  the continuous frame number (ignoring the holes) starting from the 
  beginning of the file, so that we know, which proxy frame to serve.

  This index has to be only built once for a file and is written into
  the BL_proxy directory structure for later reuse in different blender files.

*/

typedef struct anim_index_entry {
	int frameno;
	unsigned long long seek_pos;
	unsigned long long seek_pos_dts;
	unsigned long long pts;
} anim_index_entry;

struct anim_index {
	char name[1024];

	int num_entries;
	struct anim_index_entry * entries;
};

struct anim_index_builder;

typedef struct anim_index_builder {
	FILE * fp;
	char name[FILE_MAX];
	char temp_name[FILE_MAX];

	void * private_data;

	void (*delete_priv_data)(struct anim_index_builder * idx);
	void (*proc_frame)(struct anim_index_builder * idx,
	                   unsigned char * buffer,
	                   int data_size,
	                   struct anim_index_entry * entry);
} anim_index_builder;

anim_index_builder * IMB_index_builder_create(const char * name);
void IMB_index_builder_add_entry(
        anim_index_builder * fp,
        int frameno, unsigned long long seek_pos,
        unsigned long long seek_pos_dts,
        unsigned long long pts);

void IMB_index_builder_proc_frame(
        anim_index_builder * fp,
        unsigned char * buffer,
        int data_size,
        int frameno, unsigned long long seek_pos,
        unsigned long long seek_pos_dts,
        unsigned long long pts);

void IMB_index_builder_finish(anim_index_builder * fp, int rollback);

struct anim_index * IMB_indexer_open(const char * name);
unsigned long long IMB_indexer_get_seek_pos(
	struct anim_index * idx, int frameno_index);
unsigned long long IMB_indexer_get_seek_pos_dts(
	struct anim_index * idx, int frameno_index);

int IMB_indexer_get_frame_index(struct anim_index * idx, int frameno);
unsigned long long IMB_indexer_get_pts(struct anim_index * idx, 
				       int frame_index);
int IMB_indexer_get_duration(struct anim_index * idx);

int IMB_indexer_can_scan(struct anim_index * idx, 
                         int old_frame_index, int new_frame_index);

void IMB_indexer_close(struct anim_index * idx);

void IMB_free_indices(struct anim * anim);

int IMB_anim_index_get_frame_index(
	struct anim * anim, IMB_Timecode_Type tc, int position);

struct anim * IMB_anim_open_proxy(
	struct anim * anim, IMB_Proxy_Size preview_size);
struct anim_index * IMB_anim_open_index(
	struct anim * anim, IMB_Timecode_Type tc);

int IMB_proxy_size_to_array_index(IMB_Proxy_Size pr_size);
int IMB_timecode_to_array_index(IMB_Timecode_Type tc);

#endif

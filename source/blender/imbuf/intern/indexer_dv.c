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
 * Peter Schlaile <peter [at] schlaile [dot] de> 2011
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/indexer_dv.c
 *  \ingroup imbuf
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "IMB_indexer.h"
#include <time.h>

typedef struct indexer_dv_bitstream {
	unsigned char *buffer;
	int bit_pos;
} indexer_dv_bitstream;

static indexer_dv_bitstream bitstream_new(unsigned char *buffer_)
{
	indexer_dv_bitstream rv;

	rv.buffer = buffer_;
	rv.bit_pos = 0;

	return rv;
}

static unsigned long bitstream_get_bits(indexer_dv_bitstream *This, int num)
{
	int byte_pos = This->bit_pos >> 3;
	unsigned long i = 
	        This->buffer[byte_pos] | (This->buffer[byte_pos + 1] << 8) |
	        (This->buffer[byte_pos + 2] << 16) |
	        (This->buffer[byte_pos + 3] << 24);
	int rval = (i >> (This->bit_pos & 0x7)) & ((1 << num) - 1);
	This->bit_pos += num;
	return rval;
}

static int parse_num(indexer_dv_bitstream *b, int numbits)
{
	return bitstream_get_bits(b, numbits);
}

static int parse_bcd(indexer_dv_bitstream *b, int n)
{
	char s[256];
	char *p = s + (n + 3) / 4;

	*p-- = 0;

	while (n > 4) {
		char a;
		int v = bitstream_get_bits(b, 4);

		n -= 4;
		a = '0' + v;

		if (a > '9') {
			bitstream_get_bits(b, n);
			return -1;
		}

		*p-- = a;
	}
	if (n) {
		char a;
		int v = bitstream_get_bits(b, n);
		a = '0' + v;
		if (a > '9') {
			return -1;
		}
		*p-- = a;
	}

	return atol(s);
}

typedef struct indexer_dv_context {
	int rec_curr_frame;
	int rec_curr_second;
	int rec_curr_minute;
	int rec_curr_hour;

	int rec_curr_day;
	int rec_curr_month;
	int rec_curr_year;

	char got_record_date;
	char got_record_time;

	time_t ref_time_read;
	time_t ref_time_read_new;
	int curr_frame;

	time_t gap_start;
	int gap_frame;

	int frameno_offset;

	anim_index_entry backbuffer[31];
	int fsize;

	anim_index_builder *idx;
} indexer_dv_context;

static void parse_packet(indexer_dv_context *This, unsigned char *p)
{
	indexer_dv_bitstream b;
	int type = p[0];

	b = bitstream_new(p + 1);

	switch (type) {
		case 0x62: /* Record date */
			parse_num(&b, 8);
			This->rec_curr_day = parse_bcd(&b, 6);
			parse_num(&b, 2);
			This->rec_curr_month = parse_bcd(&b, 5);
			parse_num(&b, 3);
			This->rec_curr_year = parse_bcd(&b, 8);
			if (This->rec_curr_year < 25) {
				This->rec_curr_year += 2000;
			}
			else {
				This->rec_curr_year += 1900;
			}
			This->got_record_date = 1;
			break;
		case 0x63: /* Record time */
			This->rec_curr_frame = parse_bcd(&b, 6);
			parse_num(&b, 2);
			This->rec_curr_second = parse_bcd(&b, 7);
			parse_num(&b, 1);
			This->rec_curr_minute = parse_bcd(&b, 7);
			parse_num(&b, 1);
			This->rec_curr_hour = parse_bcd(&b, 6);
			This->got_record_time = 1;
			break;
	}
}

static void parse_header_block(indexer_dv_context *This, unsigned char *target)
{
	int i;
	for (i = 3; i < 80; i += 5) {
		if (target[i] != 0xff) {
			parse_packet(This, target + i);
		}
	}
}

static void parse_subcode_blocks(
        indexer_dv_context *This, unsigned char *target)
{
	int i, j;

	for (j = 0; j < 2; j++) {
		for (i = 3; i < 80; i += 5) {
			if (target[i] != 0xff) {
				parse_packet(This, target + i);
			}
		}
	}
}

static void parse_vaux_blocks(
        indexer_dv_context *This, unsigned char *target)
{
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 3; i < 80; i += 5) {
			if (target[i] != 0xff) {
				parse_packet(This, target + i);
			}
		}
		target += 80;
	}
}

static void parse_audio_headers(
        indexer_dv_context *This, unsigned char *target)
{
	int i;

	for (i = 0; i < 9; i++) {
		if (target[3] != 0xff) {
			parse_packet(This, target + 3);
		}
		target += 16 * 80;
	}
}

static void parse_frame(indexer_dv_context *This,
                        unsigned char *framebuffer, int isPAL)
{
	int numDIFseq = isPAL ? 12 : 10;
	unsigned char *target = framebuffer;
	int ds;

	for (ds = 0; ds < numDIFseq; ds++) {
		parse_header_block(This, target);
		target +=   1 * 80;
		parse_subcode_blocks(This, target);
		target +=   2 * 80;
		parse_vaux_blocks(This, target);
		target +=   3 * 80;
		parse_audio_headers(This, target);
		target += 144 * 80;
	}
}

static void inc_frame(int *frame, time_t *t, int isPAL)
{
	if ((isPAL && *frame >= 25) || (!isPAL && *frame >= 30)) {
		fprintf(stderr, "Ouchie: inc_frame: invalid_frameno: %d\n",
		        *frame);
	}
	(*frame)++;
	if (isPAL && *frame >= 25) {
		(*t)++;
		*frame = 0;
	}
	else if (!isPAL && *frame >= 30) {
		(*t)++;
		*frame = 0;
	}
}

static void write_index(indexer_dv_context *This, anim_index_entry *entry)
{
	IMB_index_builder_add_entry(
	        This->idx, entry->frameno + This->frameno_offset,
	        entry->seek_pos, entry->seek_pos_dts, entry->pts);
}

static void fill_gap(indexer_dv_context *This, int isPAL)
{
	int i;

	for (i = 0; i < This->fsize; i++) {
		if (This->gap_start == This->ref_time_read &&
		    This->gap_frame == This->curr_frame)
		{
			fprintf(stderr,
			        "indexer_dv::fill_gap: "
			        "can't seek backwards !\n");
			break;
		}
		inc_frame(&This->gap_frame, &This->gap_start, isPAL);
	}

	while (This->gap_start != This->ref_time_read ||
	       This->gap_frame != This->curr_frame)
	{
		inc_frame(&This->gap_frame, &This->gap_start, isPAL);
		This->frameno_offset++;
	}

	for (i = 0; i < This->fsize; i++) {
		write_index(This, This->backbuffer + i);
	}
	This->fsize = 0;
}

static void proc_frame(indexer_dv_context *This,
                       unsigned char *UNUSED(framebuffer), int isPAL)
{
	struct tm recDate;
	time_t t;

	if (!This->got_record_date || !This->got_record_time) {
		return;
	}

	recDate.tm_sec = This->rec_curr_second;
	recDate.tm_min = This->rec_curr_minute;
	recDate.tm_hour = This->rec_curr_hour;
	recDate.tm_mday = This->rec_curr_day;
	recDate.tm_mon = This->rec_curr_month - 1;
	recDate.tm_year = This->rec_curr_year - 1900;
	recDate.tm_wday = -1;
	recDate.tm_yday = -1;
	recDate.tm_isdst = -1;
	
	t = mktime(&recDate);
	if (t == -1) {
		return;
	}

	This->ref_time_read_new = t;

	if (This->ref_time_read < 0) {
		This->ref_time_read = This->ref_time_read_new;
		This->curr_frame = 0;
	}
	else {
		if (This->ref_time_read_new - This->ref_time_read == 1) {
			This->curr_frame = 0;
			This->ref_time_read = This->ref_time_read_new;
			if (This->gap_frame >= 0) {
				fill_gap(This, isPAL);
				This->gap_frame = -1;
			}
		}
		else if (This->ref_time_read_new == This->ref_time_read) {
			/* do nothing */
		}
		else {
			This->gap_start = This->ref_time_read;
			This->gap_frame = This->curr_frame;
			This->ref_time_read = This->ref_time_read_new;
			This->curr_frame = -1;
		}
	}
}

static void indexer_dv_proc_frame(anim_index_builder *idx,
                                  unsigned char *buffer,
                                  int UNUSED(data_size),
                                  struct anim_index_entry *entry)
{
	int isPAL;
	
	indexer_dv_context *This = (indexer_dv_context *) idx->private_data;

	isPAL = (buffer[3] & 0x80);

	This->got_record_date = false;
	This->got_record_time = false;

	parse_frame(This, buffer, isPAL);
	proc_frame(This, buffer, isPAL);

	if (This->curr_frame >= 0) {
		write_index(This, entry);
		inc_frame(&This->curr_frame, &This->ref_time_read, isPAL);
	}
	else {
		This->backbuffer[This->fsize++] = *entry;
		if (This->fsize >= 31) {
			int i;

			fprintf(stderr, "indexer_dv::indexer_dv_proc_frame: "
			        "backbuffer overrun, emergency flush");

			for (i = 0; i < This->fsize; i++) {
				write_index(This, This->backbuffer + i);
			}
			This->fsize = 0;
		}
	}
}

static void indexer_dv_delete(anim_index_builder *idx)
{
	int i = 0;
	indexer_dv_context *This = (indexer_dv_context *) idx->private_data;

	for (i = 0; i < This->fsize; i++) {
		write_index(This, This->backbuffer + i);
	}

	MEM_freeN(This);
}

static void UNUSED_FUNCTION(IMB_indexer_dv_new)(anim_index_builder *idx)
{
	indexer_dv_context *rv = MEM_callocN(
	        sizeof(indexer_dv_context), "index_dv builder context");

	rv->ref_time_read = -1;
	rv->curr_frame = -1;
	rv->gap_frame = -1;
	rv->idx = idx;
	
	idx->private_data = rv;
	idx->proc_frame = indexer_dv_proc_frame;
	idx->delete_priv_data = indexer_dv_delete;
}

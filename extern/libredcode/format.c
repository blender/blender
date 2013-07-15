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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "format.h"

struct red_reob {
	unsigned int len;
	unsigned int head;

	unsigned int rdvo;
	unsigned int rdvs;
	unsigned int rdao;
	unsigned int rdas;

	unsigned int unknown1;
	unsigned int unknown2;
	unsigned int totlen;
	
	unsigned int avgv;
	unsigned int avgs;

	unsigned int unknown3;
	unsigned int unknown4;
	unsigned int unknown5;
};

struct redcode_handle {
	FILE * fp;
	struct red_reob * reob;
        unsigned int * rdvo;
	unsigned int * rdvs;
	unsigned int * rdao;
	unsigned int * rdas;
	long cfra;
	long length;
};

unsigned int read_be32(unsigned int val)
{
	unsigned char * v = (unsigned char*) & val;
 
	return  (v[0] << 24) | (v[1] << 16) | (v[2] << 8) | v[3];
}

static unsigned char* read_packet(FILE * fp, char * expect)
{
	unsigned int len;
	char head[5];
	unsigned char * rv;

	fread(&len, 4, 1, fp);
	fread(&head, 4, 1, fp);

	head[4] = 0;

	len = read_be32(len);

	if (strcmp(expect, head) != 0) {
		fprintf(stderr, "Read: %s, expect: %s\n", head, expect);
		return NULL;
	}

	rv = (unsigned char*) malloc(len + 8);

	memcpy(rv, &len, 4);
	memcpy(rv + 4, &head, 4);
	
	fread(rv + 8, len, 1, fp);

	return rv;
}

static unsigned int * read_index_packet(FILE * fp, char * expect)
{
	unsigned int * rv = (unsigned int*) read_packet(fp, expect);
	int i;

	if (!rv) {
		return NULL;
	}

	for (i = 2; i < rv[0]/4; i++) {
		rv[i] = read_be32(rv[i]);
	}
	return rv;
}

static struct red_reob * read_reob(FILE * fp)
{
	fseek(fp, -0x38, SEEK_END);

	return (struct red_reob *) read_index_packet(fp, "REOB");
}

static unsigned int * read_index(FILE * fp, unsigned int i, char * expect)
{
	fseek(fp, i, SEEK_SET);
	
	return (unsigned int*) read_index_packet(fp, expect);
}

static unsigned char * read_data(FILE * fp, unsigned int i, char * expect)
{
	fseek(fp, i, SEEK_SET);
	
	return read_packet(fp, expect);
}

struct redcode_handle * redcode_open(const char * fname)
{
	struct redcode_handle * rv = NULL;
	struct red_reob * reob = NULL;
	int i;

	FILE * fp = fopen(fname, "rb");

	if (!fp) {
		return NULL;
	}

	reob = read_reob(fp);
	if (!reob) {
		fclose(fp);
		return NULL;
	}

	rv = (struct redcode_handle*) calloc(1, sizeof(struct redcode_handle));

	rv->fp = fp;
	rv->reob = reob;
	rv->rdvo = read_index(fp, reob->rdvo, "RDVO");
	rv->rdvs = read_index(fp, reob->rdvs, "RDVS");
	rv->rdao = read_index(fp, reob->rdao, "RDAO");
	rv->rdas = read_index(fp, reob->rdas, "RDAS");

	if (!rv->rdvo || !rv->rdvs || !rv->rdao || !rv->rdas) {
		redcode_close(rv);
		return NULL;
	}

	for (i = 0; i < (rv->rdvo[0] - 8)/4; i++) {
		if (rv->rdvo[i + 2]) {
			rv->length = i;
		}
	}

	return rv;
}

void redcode_close(struct redcode_handle * handle)
{
	if (handle->reob) {
		free(handle->reob);
	}
	if (handle->rdvo) {
		free(handle->rdvo);
	}
	if (handle->rdvs) {
		free(handle->rdvs);
	}
	if (handle->rdao) {
		free(handle->rdao);
	}
	if (handle->rdas) {
		free(handle->rdas);
	}
	fclose(handle->fp);
	free(handle);
}

long redcode_get_length(struct redcode_handle * handle)
{
	return handle->length;
}

struct redcode_frame * redcode_read_video_frame(
	struct redcode_handle * handle, long frame)
{
	struct redcode_frame * rv;
	unsigned char * data;

	if (frame > handle->rdvo[0]/4 || handle->rdvo[frame + 2] == 0) {
		return NULL;
	}
	data = read_data(handle->fp, handle->rdvo[frame + 2], "REDV");
	if (!data) {
		return NULL;
	}

	rv = (struct redcode_frame*) calloc(1, sizeof(struct redcode_frame));

	rv->offset = 12+8;
	rv->length = *(unsigned int*)data - rv->offset;
	rv->data = data;

	return rv;
}

struct redcode_frame * redcode_read_audio_frame(
	struct redcode_handle * handle, long frame)
{
	struct redcode_frame * rv;
	unsigned char * data;

	if (frame > handle->rdao[0]/4 || handle->rdao[frame + 2] == 0) {
		return NULL;
	}
	data = read_data(handle->fp, handle->rdao[frame+2], "REDA");
	if (!data) {
		return NULL;
	}

	rv = (struct redcode_frame*) calloc(1, sizeof(struct redcode_frame));

	rv->offset = 24+8;
	rv->length = *(unsigned int*)data - rv->offset;
	rv->data = data;

	return rv;
}

void redcode_free_frame(struct redcode_frame * frame)
{
	free(frame->data);
	free(frame);
}

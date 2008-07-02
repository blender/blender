#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "format.h"

struct red_reob {
	unsigned long len;
	char head[4];

	unsigned long rdvo;
	unsigned long rdvs;
	unsigned long rdao;
	unsigned long rdas;

	unsigned long unknown1;
	unsigned long unknown2;
	unsigned long totlen;
	
	unsigned long avgv;
	unsigned long avgs;

	unsigned long unknown3;
	unsigned long unknown4;
	unsigned long unknown5;
};

struct redcode_handle {
	FILE * fp;
	struct red_reob * reob;
        unsigned long * rdvo;
	unsigned long * rdvs;
	unsigned long * rdao;
	unsigned long * rdas;
	long cfra;
};


static unsigned char* read_packet(FILE * fp, char * expect)
{
	unsigned long len;
	char head[5];
	unsigned char * rv;

	fread(&len, 4, 1, fp);
	fread(&head, 4, 1, fp);

	head[4] = 0;

	len = ntohl(len);

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

static unsigned long * read_index_packet(FILE * fp, char * expect)
{
	unsigned long * rv = (unsigned long*) read_packet(fp, expect);
	int i;

	if (!rv) {
		return NULL;
	}

	for (i = 2; i < rv[0]/4; i++) {
		rv[i] = ntohl(rv[i]);
	}
	return rv;
}

static struct red_reob * read_reob(FILE * fp)
{
	fseek(fp, -0x38, SEEK_END);

	return (struct red_reob *) read_index_packet(fp, "REOB");
}

static unsigned long * read_index(FILE * fp, unsigned long i, char * expect)
{
	fseek(fp, i, SEEK_SET);
	
	return (unsigned long*) read_index_packet(fp, expect);
}

static unsigned char * read_data(FILE * fp, unsigned long i, char * expect)
{
	fseek(fp, i, SEEK_SET);
	
	return read_packet(fp, expect);
}

struct redcode_handle * redcode_open(const char * fname)
{
	struct redcode_handle * rv = NULL;
	struct red_reob * reob = NULL;

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
	return handle->rdvo[0]/4;
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
	rv->length = *(unsigned long*)data - rv->offset;
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
	rv->length = *(unsigned long*)data - rv->offset;
	rv->data = data;

	return rv;
}

void redcode_free_frame(struct redcode_frame * frame)
{
	free(frame->data);
	free(frame);
}

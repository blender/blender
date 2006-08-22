/*
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verse_header.h"
#include "v_pack.h"
#include "v_cmd_buf.h"

static const size_t	vcmdbuf_chunk_size[] = { 10000, 10000, 10000, 10000, 8000, 5000, 500 };	/* If you think memory is cheap, set this to a high value. */

/* Sizes of individual command buffers, indexable by VCMDBufSize values. Switch-killer. */
static const size_t	vcmdbuf_size[] = {
	sizeof (VCMDBuffer10), sizeof (VCMDBuffer20), sizeof (VCMDBuffer30), sizeof (VCMDBuffer80),
	sizeof (VCMDBuffer160), sizeof (VCMDBuffer320), sizeof (VCMDBuffer1500)
};

#define VCMDBUF_INIT_CHUNK_FACTOR 0.5

static struct {
	VCMDBufHead	*buffers[VCMDBS_COUNT];
	unsigned int available[VCMDBS_COUNT];
} VCMDBufData;

static boolean v_cmd_buf_initialized = FALSE;

void cmd_buf_init(void)
{
	unsigned int i, j;
	VCMDBufHead *buf, *b;

	for(i = 0; i < VCMDBS_COUNT; i++)
	{
		VCMDBufData.buffers[i] = NULL;
		VCMDBufData.available[i] = (unsigned int) (vcmdbuf_chunk_size[i] * VCMDBUF_INIT_CHUNK_FACTOR);
		for(j = 0, buf = NULL; j < VCMDBufData.available[i]; j++, buf = b)
		{
			b = v_cmd_buf_allocate(i);
			b->next = buf;
		}
		VCMDBufData.buffers[i] = buf;
	}
	v_cmd_buf_initialized = TRUE;
}

VCMDBufHead * v_cmd_buf_allocate(VCMDBufSize buf_size)
{
	VCMDBufHead	*output = NULL;

	if(VCMDBufData.buffers[buf_size] != NULL)
	{
		output = VCMDBufData.buffers[buf_size];
		VCMDBufData.buffers[buf_size] = output->next;
		VCMDBufData.available[buf_size]--;
	}
	else
	{
		if(buf_size < sizeof vcmdbuf_size / sizeof *vcmdbuf_size)
			output = malloc(vcmdbuf_size[buf_size]);
		else
		{
			fprintf(stderr, "v_cmd_buf.c: Can't handle buffer size %d\n", buf_size);
			return NULL;
		}
		output->buf_size = buf_size;
	}
	output->next = NULL;	
	output->packet = 0;
	output->size = 0;
	output->address_size = -1;
	return output;
}

void v_cmd_buf_free(VCMDBufHead *head)
{
	if(VCMDBufData.available[head->buf_size] < vcmdbuf_chunk_size[head->buf_size])
	{
		head->next = VCMDBufData.buffers[head->buf_size];
		VCMDBufData.buffers[head->buf_size] = head;
		VCMDBufData.available[head->buf_size]++;
	}
	else
		free(head);
}

void v_cmd_buf_set_size(VCMDBufHead *head, unsigned int size)
{
	head->size = size;
}

void v_cmd_buf_set_address_size(VCMDBufHead *head, unsigned int size)
{
	unsigned int i;

	head->address_size = size;
	head->address_sum = 0;
	for(i = 1; i < size + 1; i++)
		head->address_sum += i * i * (uint32)(((VCMDBuffer1500 *)head)->buf[i - 1]);
}

void v_cmd_buf_set_unique_address_size(VCMDBufHead *head, unsigned int size)
{
	static unsigned int i = 0;

	head->address_size = size;
	head->address_sum = i++;
}

boolean	v_cmd_buf_compare(VCMDBufHead *a, VCMDBufHead *b)
{
	if(a->address_sum != b->address_sum)
		return FALSE;
	if(a->address_size != b->address_size)
		return FALSE;
	return memcmp(((VCMDBuffer1500 *)a)->buf, ((VCMDBuffer1500 *)b)->buf, a->address_size) == 0;
}

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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/avi/intern/avi_endian.c
 *  \ingroup avi
 *
 * This is external code. Streams bytes to output depending on the
 * endianness of the system.
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "AVI_avi.h"
#include "avi_endian.h"
#include "avi_intern.h"

#ifdef __BIG_ENDIAN__
#include "MEM_guardedalloc.h"
#endif

#ifdef __BIG_ENDIAN__

/* copied from BLI_endian_switch_inline.h */
static void invert(int *val)
{
	int tval = *val;
	*val = ((tval >> 24))             |
	       ((tval << 8) & 0x00ff0000) |
	       ((tval >> 8) & 0x0000ff00) |
	       ((tval << 24));
}

static void sinvert(short int *val)
{
	short tval = *val;
	*val = (tval >> 8) |
	       (tval << 8);
}

static void Ichunk(AviChunk *chunk)
{
	invert(&chunk->fcc);
	invert(&chunk->size);
}
#endif

#ifdef __BIG_ENDIAN__
static void Ilist(AviList *list)
{
	invert(&list->fcc);
	invert(&list->size);
	invert(&list->ids);
}

static void Imainh(AviMainHeader *mainh)
{
	invert(&mainh->fcc);
	invert(&mainh->size);
	invert(&mainh->MicroSecPerFrame);
	invert(&mainh->MaxBytesPerSec);
	invert(&mainh->PaddingGranularity);
	invert(&mainh->Flags);
	invert(&mainh->TotalFrames);
	invert(&mainh->InitialFrames);
	invert(&mainh->Streams);
	invert(&mainh->SuggestedBufferSize);
	invert(&mainh->Width);
	invert(&mainh->Height);
	invert(&mainh->Reserved[0]);
	invert(&mainh->Reserved[1]);
	invert(&mainh->Reserved[2]);
	invert(&mainh->Reserved[3]);
}

static void Istreamh(AviStreamHeader *streamh)
{
	invert(&streamh->fcc);
	invert(&streamh->size);
	invert(&streamh->Type);
	invert(&streamh->Handler);
	invert(&streamh->Flags);
	sinvert(&streamh->Priority);
	sinvert(&streamh->Language);
	invert(&streamh->InitialFrames);
	invert(&streamh->Scale);
	invert(&streamh->Rate);
	invert(&streamh->Start);
	invert(&streamh->Length);
	invert(&streamh->SuggestedBufferSize);
	invert(&streamh->Quality);
	invert(&streamh->SampleSize);
	sinvert(&streamh->left);
	sinvert(&streamh->right);
	sinvert(&streamh->top);
	sinvert(&streamh->bottom);
}

static void Ibitmaph(AviBitmapInfoHeader *bitmaph)
{
	invert(&bitmaph->fcc);
	invert(&bitmaph->size);
	invert(&bitmaph->Size);
	invert(&bitmaph->Width);
	invert(&bitmaph->Height);
	sinvert(&bitmaph->Planes);
	sinvert(&bitmaph->BitCount);
	invert(&bitmaph->Compression);
	invert(&bitmaph->SizeImage);
	invert(&bitmaph->XPelsPerMeter);
	invert(&bitmaph->YPelsPerMeter);
	invert(&bitmaph->ClrUsed);
	invert(&bitmaph->ClrImportant);
}

static void Imjpegu(AviMJPEGUnknown *mjpgu)
{
	invert(&mjpgu->a);
	invert(&mjpgu->b);
	invert(&mjpgu->c);
	invert(&mjpgu->d);
	invert(&mjpgu->e);
	invert(&mjpgu->f);
	invert(&mjpgu->g);
}

static void Iindexe(AviIndexEntry *indexe)
{
	invert(&indexe->ChunkId);
	invert(&indexe->Flags);
	invert(&indexe->Offset);
	invert(&indexe->Size);
}
#endif /* __BIG_ENDIAN__ */

void awrite(AviMovie *movie, void *datain, int block, int size, FILE *fp, int type)
{
#ifdef __BIG_ENDIAN__
	void *data;

	data = MEM_mallocN(size, "avi endian");

	memcpy(data, datain, size);

	switch (type) {
		case AVI_RAW:
			fwrite(data, block, size, fp);
			break;
		case AVI_CHUNK:
			Ichunk((AviChunk *) data);
			fwrite(data, block, size, fp);
			break;
		case AVI_LIST:
			Ilist((AviList *) data);
			fwrite(data, block, size, fp);
			break;
		case AVI_MAINH:
			Imainh((AviMainHeader *) data);
			fwrite(data, block, size, fp);
			break;
		case AVI_STREAMH:
			Istreamh((AviStreamHeader *) data);
			fwrite(data, block, size, fp);
			break;
		case AVI_BITMAPH:
			Ibitmaph((AviBitmapInfoHeader *) data);
			if (size == sizeof(AviBitmapInfoHeader) + sizeof(AviMJPEGUnknown)) {
				Imjpegu((AviMJPEGUnknown *)((char *)data + sizeof(AviBitmapInfoHeader)));
			}
			fwrite(data, block, size, fp);
			break;
		case AVI_MJPEGU:
			Imjpegu((AviMJPEGUnknown *) data);
			fwrite(data, block, size, fp);
			break;
		case AVI_INDEXE:
			Iindexe((AviIndexEntry *) data);
			fwrite(data, block, size, fp);
			break;
		default:
			break;
	}

	MEM_freeN(data);
#else /* __BIG_ENDIAN__ */
	(void)movie; /* unused */
	(void)type; /* unused */
	fwrite(datain, block, size, fp);
#endif /* __BIG_ENDIAN__ */
}

/*
 *
 * This is external code.
 *
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

/** \file blender/avi/intern/avi.c
 *  \ingroup avi
 */


#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"
#include "MEM_sys_types.h"

#include "BLI_winstuff.h"

#include "AVI_avi.h"
#include "avi_intern.h"

#include "endian.h"

static int AVI_DEBUG=0;
static char DEBUG_FCC[4];

#define DEBUG_PRINT(x) if (AVI_DEBUG) printf("AVI DEBUG: " x);

/* local functions */
char *fcc_to_char (unsigned int fcc);
char *tcc_to_char (unsigned int tcc);



/* implemetation */

unsigned int GET_FCC (FILE *fp)
{
	unsigned char tmp[4];

	tmp[0] = getc(fp);
	tmp[1] = getc(fp);
	tmp[2] = getc(fp);
	tmp[3] = getc(fp);

	return FCC (tmp);
}

unsigned int GET_TCC (FILE *fp)
{
	char tmp[5];

	tmp[0] = getc(fp);
	tmp[1] = getc(fp);
	tmp[2] = 0;
	tmp[3] = 0;

	return FCC (tmp);
}

char *fcc_to_char (unsigned int fcc)
{
	DEBUG_FCC[0]= (fcc)&127;
	DEBUG_FCC[1]= (fcc>>8)&127;
	DEBUG_FCC[2]= (fcc>>16)&127;
	DEBUG_FCC[3]= (fcc>>24)&127;

	return DEBUG_FCC;	
}

char *tcc_to_char (unsigned int tcc)
{
	DEBUG_FCC[0]= (tcc)&127;
	DEBUG_FCC[1]= (tcc>>8)&127;
	DEBUG_FCC[2]= 0;
	DEBUG_FCC[3]= 0;

	return DEBUG_FCC;	
}

int AVI_get_stream (AviMovie *movie, int avist_type, int stream_num)
{
	int cur_stream;

	if (movie == NULL)
		return -AVI_ERROR_OPTION;

	for (cur_stream=0; cur_stream < movie->header->Streams; cur_stream++) {
		if (movie->streams[cur_stream].sh.Type == avist_type) {
			if (stream_num == 0)
				return cur_stream;
			else
				stream_num--;
		}
	}

	return -AVI_ERROR_FOUND;
}

static int fcc_get_stream (int fcc)
{
	char fccs[4];

	fccs[0] = fcc;
	fccs[1] = fcc>>8;
	fccs[2] = fcc>>16;
	fccs[3] = fcc>>24;

	return 10*(fccs[0]-'0') + (fccs[1]-'0');
}

static int fcc_is_data (int fcc)
{
	char fccs[4];

	fccs[0] = fcc;
	fccs[1] = fcc>>8;
	fccs[2] = fcc>>16;
	fccs[3] = fcc>>24;

	if (!isdigit (fccs[0]) || !isdigit (fccs[1]) || (fccs[2] != 'd' && fccs[2] != 'w'))
		return 0;
	if (fccs[3] != 'b' && fccs[3] != 'c')
		return 0;

	return 1;
}

AviError AVI_print_error (AviError in_error)
{
	int error;

	if ((int) in_error < 0)
		error = -in_error;
	else
		error = in_error;

	switch (error) {
	case AVI_ERROR_NONE:
		break;
	case AVI_ERROR_COMPRESSION:
		printf ("AVI ERROR: compressed in an unsupported format\n");
		break;
	case AVI_ERROR_OPEN:
		printf ("AVI ERROR: could not open file\n");
		break;
	case AVI_ERROR_READING:
		printf ("AVI ERROR: could not read from file\n");
		break;
	case AVI_ERROR_WRITING:
		printf ("AVI ERROR: could not write to file\n");
		break;
	case AVI_ERROR_FORMAT:
		printf ("AVI ERROR: file is in an illegal or unrecognized format\n");
		break;
	case AVI_ERROR_ALLOC:
		printf ("AVI ERROR: error encountered while allocating memory\n");
		break;
	case AVI_ERROR_OPTION:
		printf ("AVI ERROR: program made illegal request\n");
		break;
	case AVI_ERROR_FOUND:
		printf ("AVI ERROR: movie did not contain expected item\n");
		break;
	default: 
		break;
	}

	return in_error;
}
#if 0
void AVI_set_debug (int mode)
{
	AVI_DEBUG= mode;
}

int AVI_is_avi (char *name)
{
	FILE *fp;
	int ret;
	
	fp = fopen (name, "rb");
	if (fp == NULL)
		return 0;

	if (GET_FCC (fp) != FCC("RIFF") ||
		!GET_FCC (fp) ||
		GET_FCC (fp) != FCC("AVI ")) {
		ret = 0;
	}
	else {
		ret = 1;
	}

	fclose(fp);
	return ret;
}
#endif

int AVI_is_avi (const char *name)
{
	int temp, fcca, j;
	AviMovie movie= {NULL};
	AviMainHeader header;
	AviBitmapInfoHeader bheader;
	int movie_tracks = 0;
	
	DEBUG_PRINT("opening movie\n");

	movie.type = AVI_MOVIE_READ;
	movie.fp = fopen (name, "rb");
	movie.offset_table = NULL;

	if (movie.fp == NULL)
		return 0;

	if (GET_FCC (movie.fp) != FCC("RIFF") ||
		!(movie.size = GET_FCC (movie.fp))) {
		fclose(movie.fp);
		return 0;
	}

	movie.header = &header;

	if (GET_FCC (movie.fp) != FCC("AVI ") ||
		GET_FCC (movie.fp) != FCC("LIST") ||
		!GET_FCC (movie.fp) ||
		GET_FCC (movie.fp) != FCC("hdrl") ||
		(movie.header->fcc = GET_FCC (movie.fp)) != FCC("avih") ||
		!(movie.header->size = GET_FCC (movie.fp))) {
		DEBUG_PRINT("bad initial header info\n");
		fclose(movie.fp);
		return 0;
	}
	
	movie.header->MicroSecPerFrame = GET_FCC(movie.fp);
	movie.header->MaxBytesPerSec = GET_FCC(movie.fp);
	movie.header->PaddingGranularity = GET_FCC(movie.fp);
	movie.header->Flags = GET_FCC(movie.fp);
	movie.header->TotalFrames = GET_FCC(movie.fp);
	movie.header->InitialFrames = GET_FCC(movie.fp);
	movie.header->Streams = GET_FCC(movie.fp);
	movie.header->SuggestedBufferSize = GET_FCC(movie.fp);
	movie.header->Width = GET_FCC(movie.fp);
	movie.header->Height = GET_FCC(movie.fp);
	movie.header->Reserved[0] = GET_FCC(movie.fp);
	movie.header->Reserved[1] = GET_FCC(movie.fp);
	movie.header->Reserved[2] = GET_FCC(movie.fp);
	movie.header->Reserved[3] = GET_FCC(movie.fp);

	fseek (movie.fp, movie.header->size-14*4, SEEK_CUR);

	if (movie.header->Streams < 1) {
		DEBUG_PRINT("streams less than 1\n");
		fclose(movie.fp);
		return 0;
	}
	
	movie.streams = (AviStreamRec *) MEM_callocN (sizeof(AviStreamRec) * movie.header->Streams, "moviestreams");

	for (temp=0; temp < movie.header->Streams; temp++) {

		if (GET_FCC(movie.fp) != FCC("LIST") ||
			!GET_FCC (movie.fp) ||
			GET_FCC (movie.fp) != FCC ("strl") ||
			(movie.streams[temp].sh.fcc = GET_FCC (movie.fp)) != FCC ("strh") ||
			!(movie.streams[temp].sh.size = GET_FCC (movie.fp))) {
			DEBUG_PRINT("bad stream header information\n");
			
			MEM_freeN(movie.streams);
			fclose(movie.fp);
			return 0;				
		}

		movie.streams[temp].sh.Type = GET_FCC (movie.fp);
		movie.streams[temp].sh.Handler = GET_FCC (movie.fp);

		fcca = movie.streams[temp].sh.Handler;
		
		if (movie.streams[temp].sh.Type == FCC("vids")) {
			if (fcca == FCC ("DIB ") ||
				fcca == FCC ("RGB ") ||
				fcca == FCC ("rgb ") ||
				fcca == FCC ("RAW ") ||
				fcca == 0) {
				movie.streams[temp].format = AVI_FORMAT_AVI_RGB;
			}
			else if (fcca == FCC ("mjpg")||fcca == FCC ("MJPG")) {
				movie.streams[temp].format = AVI_FORMAT_MJPEG;
			}
			else {
				MEM_freeN(movie.streams);
				fclose(movie.fp);
				return 0;
			}
			movie_tracks++;
		}
		
		movie.streams[temp].sh.Flags = GET_FCC (movie.fp);
		movie.streams[temp].sh.Priority = GET_TCC (movie.fp);
		movie.streams[temp].sh.Language = GET_TCC (movie.fp);
		movie.streams[temp].sh.InitialFrames = GET_FCC (movie.fp);
		movie.streams[temp].sh.Scale = GET_FCC (movie.fp);
		movie.streams[temp].sh.Rate = GET_FCC (movie.fp);
		movie.streams[temp].sh.Start = GET_FCC (movie.fp);
		movie.streams[temp].sh.Length = GET_FCC (movie.fp);
		movie.streams[temp].sh.SuggestedBufferSize = GET_FCC (movie.fp);
		movie.streams[temp].sh.Quality = GET_FCC (movie.fp);
		movie.streams[temp].sh.SampleSize = GET_FCC (movie.fp);
		movie.streams[temp].sh.left = GET_TCC (movie.fp);
		movie.streams[temp].sh.top = GET_TCC (movie.fp);
		movie.streams[temp].sh.right = GET_TCC (movie.fp);
		movie.streams[temp].sh.bottom = GET_TCC (movie.fp);

		fseek (movie.fp, movie.streams[temp].sh.size-14*4, SEEK_CUR);

		if (GET_FCC (movie.fp) != FCC("strf")) {
			DEBUG_PRINT("no stream format information\n");
			MEM_freeN(movie.streams);
			fclose(movie.fp);
			return 0;
		}

		movie.streams[temp].sf_size= GET_FCC(movie.fp);
		if (movie.streams[temp].sh.Type == FCC("vids")) {
			j = movie.streams[temp].sf_size - (sizeof(AviBitmapInfoHeader) - 8);
			if (j >= 0) {
				AviBitmapInfoHeader *bi;
				
				movie.streams[temp].sf= &bheader;
				bi= (AviBitmapInfoHeader *) movie.streams[temp].sf;
				
				bi->fcc= FCC("strf");
				bi->size= movie.streams[temp].sf_size;
				bi->Size= GET_FCC(movie.fp);
				bi->Width= GET_FCC(movie.fp);
				bi->Height= GET_FCC(movie.fp);
				bi->Planes= GET_TCC(movie.fp);
				bi->BitCount= GET_TCC(movie.fp);
				bi->Compression= GET_FCC(movie.fp);
				bi->SizeImage= GET_FCC(movie.fp);
				bi->XPelsPerMeter= GET_FCC(movie.fp);
				bi->YPelsPerMeter= GET_FCC(movie.fp);
				bi->ClrUsed= GET_FCC(movie.fp);
				bi->ClrImportant= GET_FCC(movie.fp);
				
				fcca = bi->Compression;

				if ( movie.streams[temp].format ==
					 AVI_FORMAT_AVI_RGB) {
					if (fcca == FCC ("DIB ") ||
						fcca == FCC ("RGB ") ||
						fcca == FCC ("rgb ") ||
						fcca == FCC ("RAW ") ||
						fcca == 0 ) {
					}
					else if ( fcca == FCC ("mjpg") ||
						fcca == FCC ("MJPG")) {
							movie.streams[temp].format = AVI_FORMAT_MJPEG;
					}
					else {
						MEM_freeN(movie.streams);
						fclose(movie.fp);
						return 0;
					}
				}

			} 
			if (j > 0) fseek (movie.fp, j, SEEK_CUR);
		}
		else fseek (movie.fp, movie.streams[temp].sf_size, SEEK_CUR);

		/* Walk to the next LIST */		
		while (GET_FCC (movie.fp) != FCC("LIST")) {
			temp= GET_FCC (movie.fp);
			if (temp<0 || ftell(movie.fp) > movie.size) {
				DEBUG_PRINT("incorrect size in header or error in AVI\n");
				
				MEM_freeN(movie.streams);
				fclose(movie.fp);
				return 0;				
			}
			fseek(movie.fp, temp, SEEK_CUR);			
		}

		fseek(movie.fp, -4L, SEEK_CUR);
	}
	
	MEM_freeN(movie.streams);
	fclose(movie.fp);

	/* at least one video track is needed */
	return (movie_tracks != 0); 

}

AviError AVI_open_movie (const char *name, AviMovie *movie)
{
	int temp, fcca, size, j;
	
	DEBUG_PRINT("opening movie\n");

	memset(movie, 0, sizeof(AviMovie));

	movie->type = AVI_MOVIE_READ;
	movie->fp = fopen (name, "rb");
	movie->offset_table = NULL;

	if (movie->fp == NULL)
		return AVI_ERROR_OPEN;

	if (GET_FCC (movie->fp) != FCC("RIFF") ||
		!(movie->size = GET_FCC (movie->fp)))
		return AVI_ERROR_FORMAT;

	movie->header = (AviMainHeader *) MEM_mallocN (sizeof (AviMainHeader), "movieheader");

	if (GET_FCC (movie->fp) != FCC("AVI ") ||
		GET_FCC (movie->fp) != FCC("LIST") ||
		!GET_FCC (movie->fp) ||
		GET_FCC (movie->fp) != FCC("hdrl") ||
		(movie->header->fcc = GET_FCC (movie->fp)) != FCC("avih") ||
		!(movie->header->size = GET_FCC (movie->fp))) {
		DEBUG_PRINT("bad initial header info\n");
		return AVI_ERROR_FORMAT;
	}
	
	movie->header->MicroSecPerFrame = GET_FCC(movie->fp);
	movie->header->MaxBytesPerSec = GET_FCC(movie->fp);
	movie->header->PaddingGranularity = GET_FCC(movie->fp);
	movie->header->Flags = GET_FCC(movie->fp);
	movie->header->TotalFrames = GET_FCC(movie->fp);
	movie->header->InitialFrames = GET_FCC(movie->fp);
	movie->header->Streams = GET_FCC(movie->fp);
	movie->header->SuggestedBufferSize = GET_FCC(movie->fp);
	movie->header->Width = GET_FCC(movie->fp);
	movie->header->Height = GET_FCC(movie->fp);
	movie->header->Reserved[0] = GET_FCC(movie->fp);
	movie->header->Reserved[1] = GET_FCC(movie->fp);
	movie->header->Reserved[2] = GET_FCC(movie->fp);
	movie->header->Reserved[3] = GET_FCC(movie->fp);

	fseek (movie->fp, movie->header->size-14*4, SEEK_CUR);

	if (movie->header->Streams < 1) {
		DEBUG_PRINT("streams less than 1\n");
		return AVI_ERROR_FORMAT;
	}
	
	movie->streams = (AviStreamRec *) MEM_callocN (sizeof(AviStreamRec) * movie->header->Streams, "moviestreams");

	for (temp=0; temp < movie->header->Streams; temp++) {

		if (GET_FCC(movie->fp) != FCC("LIST") ||
			!GET_FCC (movie->fp) ||
			GET_FCC (movie->fp) != FCC ("strl") ||
			(movie->streams[temp].sh.fcc = GET_FCC (movie->fp)) != FCC ("strh") ||
			!(movie->streams[temp].sh.size = GET_FCC (movie->fp))) {
			DEBUG_PRINT("bad stream header information\n");
			return AVI_ERROR_FORMAT;				
		}

		movie->streams[temp].sh.Type = GET_FCC (movie->fp);
		movie->streams[temp].sh.Handler = GET_FCC (movie->fp);

		fcca = movie->streams[temp].sh.Handler;
		
		if (movie->streams[temp].sh.Type == FCC("vids")) {
			if (fcca == FCC ("DIB ") ||
				fcca == FCC ("RGB ") ||
				fcca == FCC ("rgb ") ||
				fcca == FCC ("RAW ") ||
				fcca == 0) {
				movie->streams[temp].format = AVI_FORMAT_AVI_RGB;
			}
			else if (fcca == FCC ("mjpg")||fcca == FCC ("MJPG")) {
				movie->streams[temp].format = AVI_FORMAT_MJPEG;
			}
			else {
				return AVI_ERROR_COMPRESSION;
			}
		}
		
		movie->streams[temp].sh.Flags = GET_FCC (movie->fp);
		movie->streams[temp].sh.Priority = GET_TCC (movie->fp);
		movie->streams[temp].sh.Language = GET_TCC (movie->fp);
		movie->streams[temp].sh.InitialFrames = GET_FCC (movie->fp);
		movie->streams[temp].sh.Scale = GET_FCC (movie->fp);
		movie->streams[temp].sh.Rate = GET_FCC (movie->fp);
		movie->streams[temp].sh.Start = GET_FCC (movie->fp);
		movie->streams[temp].sh.Length = GET_FCC (movie->fp);
		movie->streams[temp].sh.SuggestedBufferSize = GET_FCC (movie->fp);
		movie->streams[temp].sh.Quality = GET_FCC (movie->fp);
		movie->streams[temp].sh.SampleSize = GET_FCC (movie->fp);
		movie->streams[temp].sh.left = GET_TCC (movie->fp);
		movie->streams[temp].sh.top = GET_TCC (movie->fp);
		movie->streams[temp].sh.right = GET_TCC (movie->fp);
		movie->streams[temp].sh.bottom = GET_TCC (movie->fp);

		fseek (movie->fp, movie->streams[temp].sh.size-14*4, SEEK_CUR);

		if (GET_FCC (movie->fp) != FCC("strf")) {
			DEBUG_PRINT("no stream format information\n");
			return AVI_ERROR_FORMAT;
		}

		movie->streams[temp].sf_size= GET_FCC(movie->fp);
		if (movie->streams[temp].sh.Type == FCC("vids")) {
			j = movie->streams[temp].sf_size - (sizeof(AviBitmapInfoHeader) - 8);
			if (j >= 0) {
				AviBitmapInfoHeader *bi;
				
				movie->streams[temp].sf= MEM_mallocN(sizeof(AviBitmapInfoHeader), "streamformat");
				
				bi= (AviBitmapInfoHeader *) movie->streams[temp].sf;
				
				bi->fcc= FCC("strf");
				bi->size= movie->streams[temp].sf_size;
				bi->Size= GET_FCC(movie->fp);
				bi->Width= GET_FCC(movie->fp);
				bi->Height= GET_FCC(movie->fp);
				bi->Planes= GET_TCC(movie->fp);
				bi->BitCount= GET_TCC(movie->fp);
				bi->Compression= GET_FCC(movie->fp);
				bi->SizeImage= GET_FCC(movie->fp);
				bi->XPelsPerMeter= GET_FCC(movie->fp);
				bi->YPelsPerMeter= GET_FCC(movie->fp);
				bi->ClrUsed= GET_FCC(movie->fp);
				bi->ClrImportant= GET_FCC(movie->fp);
				
				fcca = bi->Compression;

								if ( movie->streams[temp].format ==
					 AVI_FORMAT_AVI_RGB) {
					if (fcca == FCC ("DIB ") ||
						fcca == FCC ("RGB ") ||
						fcca == FCC ("rgb ") ||
						fcca == FCC ("RAW ") ||
						fcca == 0 ) {
					}
					else if ( fcca == FCC ("mjpg") ||
						fcca == FCC ("MJPG")) {
							movie->streams[temp].format = AVI_FORMAT_MJPEG;
					}
					else {
						return AVI_ERROR_COMPRESSION;
					}
				}

			} 
			if (j > 0) fseek (movie->fp, j, SEEK_CUR);
		}
		else fseek (movie->fp, movie->streams[temp].sf_size, SEEK_CUR);
		
		/* Walk to the next LIST */		
		while (GET_FCC (movie->fp) != FCC("LIST")) {
			temp= GET_FCC (movie->fp);
			if (temp<0 || ftell(movie->fp) > movie->size) {
				DEBUG_PRINT("incorrect size in header or error in AVI\n");
				return AVI_ERROR_FORMAT;				
			}
			fseek(movie->fp, temp, SEEK_CUR);			
		}
		
		fseek(movie->fp, -4L, SEEK_CUR);		
	}

	while (1) {
		temp = GET_FCC (movie->fp);
		size = GET_FCC (movie->fp);

		if (size == 0)
			break;

		if (temp == FCC("LIST")) {
			if (GET_FCC(movie->fp) == FCC ("movi"))
				break;
			else
				fseek (movie->fp, size-4, SEEK_CUR);
		}
		else {
			fseek (movie->fp, size, SEEK_CUR);
		}
		if (ftell(movie->fp) > movie->size) {
			DEBUG_PRINT("incorrect size in header or error in AVI\n");
			return AVI_ERROR_FORMAT;
		}
	}

	movie->movi_offset = ftell (movie->fp);
	movie->read_offset = movie->movi_offset;
	
	/* Read in the index if the file has one, otherwise create one */
	if (movie->header->Flags & AVIF_HASINDEX) {
		fseek(movie->fp, size-4, SEEK_CUR);

		if (GET_FCC(movie->fp) != FCC("idx1")) {
			DEBUG_PRINT("bad index informatio\n");
			return AVI_ERROR_FORMAT;
		}

		movie->index_entries = GET_FCC (movie->fp)/sizeof(AviIndexEntry);
		if (movie->index_entries == 0) {
			DEBUG_PRINT("no index entries\n");
			return AVI_ERROR_FORMAT;
		}

		movie->entries = (AviIndexEntry *) MEM_mallocN (movie->index_entries * sizeof(AviIndexEntry),"movieentries");

		for (temp=0; temp < movie->index_entries; temp++) {
			movie->entries[temp].ChunkId = GET_FCC (movie->fp);
			movie->entries[temp].Flags = GET_FCC (movie->fp);
			movie->entries[temp].Offset = GET_FCC (movie->fp);
			movie->entries[temp].Size = GET_FCC (movie->fp);
			
			if (AVI_DEBUG) {
				printf("Index entry %04d: ChunkId:%s Flags:%d Offset:%d Size:%d\n",
				       temp, fcc_to_char(movie->entries[temp].ChunkId), movie->entries[temp].Flags,
				       movie->entries[temp].Offset, movie->entries[temp].Size);
			}
		}

/* Some AVI's have offset entries in absolute coordinates
 * instead of an offset from the movie beginning... this is...
 * wacky, but we need to handle it. The wacky offset always
 * starts at movi_offset it seems... so we'll check that.
 * Note the the offset needs an extra 4 bytes for some 
 * undetermined reason */
 
		if (movie->entries[0].Offset == movie->movi_offset)
			movie->read_offset= 4;
	}

	DEBUG_PRINT("movie succesfully opened\n");
	return AVI_ERROR_NONE;
}

void *AVI_read_frame (AviMovie *movie, AviFormat format, int frame, int stream)
{
	int cur_frame=-1, temp, i=0, rewind=1;
	void *buffer;

	/* Retrieve the record number of the desired frame in the index 
	 * If a chunk has Size 0 we need to rewind to previous frame */
	while (rewind && frame > -1) {
		i=0;
		cur_frame=-1;
		rewind = 0;

		while (cur_frame < frame && i < movie->index_entries) {
			if (fcc_is_data (movie->entries[i].ChunkId) &&
				fcc_get_stream (movie->entries[i].ChunkId) == stream) {
				if ((cur_frame == frame -1) && (movie->entries[i].Size == 0)) {
					rewind = 1;
					frame = frame -1;
				}
				else {
					cur_frame++;
				}
			}
			i++;
		}
	}

	if (cur_frame != frame) return NULL;


	fseek (movie->fp, movie->read_offset + movie->entries[i-1].Offset, SEEK_SET);

	temp = GET_FCC(movie->fp);
	buffer = MEM_mallocN (temp,"readbuffer");

	if (fread (buffer, 1, temp, movie->fp) != temp) {
		MEM_freeN(buffer);

		return NULL;
	}
	
	buffer = avi_format_convert (movie, stream, buffer, movie->streams[stream].format, format, &temp);

	return buffer;
}

AviError AVI_close (AviMovie *movie)
{
	int i;

	fclose (movie->fp);

	for (i=0; i < movie->header->Streams; i++) {
		if (movie->streams[i].sf != NULL)
			MEM_freeN (movie->streams[i].sf);
	}

	if (movie->header != NULL)
		MEM_freeN (movie->header);
	if (movie->streams!= NULL)
		MEM_freeN (movie->streams);
	if (movie->entries != NULL)
		MEM_freeN (movie->entries);
	if (movie->offset_table != NULL)
		MEM_freeN (movie->offset_table);

	return AVI_ERROR_NONE;
}

AviError AVI_open_compress (char *name, AviMovie *movie, int streams, ...)
{
	va_list ap;
	AviList list;
	AviChunk chunk;
	int i;
	int64_t header_pos1, header_pos2;
	int64_t stream_pos1, stream_pos2;

	movie->type = AVI_MOVIE_WRITE;
	movie->fp = fopen (name, "wb");

	movie->index_entries = 0;

	if (movie->fp == NULL)
		return AVI_ERROR_OPEN;

	movie->offset_table = (int64_t *) MEM_mallocN ((1+streams*2) * sizeof (int64_t),"offsettable");
	
	for (i=0; i < 1 + streams*2; i++)
		movie->offset_table[i] = -1L;

	movie->entries = NULL;

	movie->header = (AviMainHeader *) MEM_mallocN (sizeof(AviMainHeader),"movieheader");

	movie->header->fcc = FCC("avih");
	movie->header->size = 56;
	movie->header->MicroSecPerFrame = 66667;
	movie->header->MaxBytesPerSec = 0;
	movie->header->PaddingGranularity = 0;
	movie->header->Flags = AVIF_HASINDEX | AVIF_MUSTUSEINDEX;
	movie->header->TotalFrames = 0;
	movie->header->InitialFrames = 0;
	movie->header->Streams = streams;
	movie->header->SuggestedBufferSize = 0;
	movie->header->Width = 0;
	movie->header->Height = 0;
	movie->header->Reserved[0] = 0;
	movie->header->Reserved[1] = 0;
	movie->header->Reserved[2] = 0;
	movie->header->Reserved[3] = 0;

	movie->streams = (AviStreamRec *) MEM_mallocN (sizeof(AviStreamRec) * movie->header->Streams,"moviestreams");

	va_start (ap, streams);

	for (i=0; i < movie->header->Streams; i++) {
		movie->streams[i].format = va_arg(ap, AviFormat);

		movie->streams[i].sh.fcc = FCC ("strh");
		movie->streams[i].sh.size = 56;
		movie->streams[i].sh.Type = avi_get_format_type (movie->streams[i].format);
		if (movie->streams[i].sh.Type == 0)
			return AVI_ERROR_FORMAT;

		movie->streams[i].sh.Handler = avi_get_format_fcc (movie->streams[i].format);
		if (movie->streams[i].sh.Handler == 0)
			return AVI_ERROR_FORMAT;

		movie->streams[i].sh.Flags = 0;
		movie->streams[i].sh.Priority = 0;
		movie->streams[i].sh.Language = 0;
		movie->streams[i].sh.InitialFrames = 0;
		movie->streams[i].sh.Scale = 66667;
		movie->streams[i].sh.Rate = 1000000;
		movie->streams[i].sh.Start = 0;
		movie->streams[i].sh.Length = 0;
		movie->streams[i].sh.SuggestedBufferSize = 0;
		movie->streams[i].sh.Quality = 10000;
		movie->streams[i].sh.SampleSize = 0;
		movie->streams[i].sh.left = 0;
		movie->streams[i].sh.top = 0;
		movie->streams[i].sh.right = 0;
		movie->streams[i].sh.bottom = 0;

		if (movie->streams[i].sh.Type == FCC("vids")) {	
#if 0
			if (movie->streams[i].format == AVI_FORMAT_MJPEG) {
				movie->streams[i].sf = MEM_mallocN (sizeof(AviBitmapInfoHeader) 
										+ sizeof(AviMJPEGUnknown),"moviestreamformatL");
				movie->streams[i].sf_size = sizeof(AviBitmapInfoHeader) + sizeof(AviMJPEGUnknown);
			}
			else {
#endif
			movie->streams[i].sf = MEM_mallocN (sizeof(AviBitmapInfoHeader),  "moviestreamformatS");
			movie->streams[i].sf_size = sizeof(AviBitmapInfoHeader);

			((AviBitmapInfoHeader *) movie->streams[i].sf)->fcc = FCC ("strf");
			((AviBitmapInfoHeader *) movie->streams[i].sf)->size = movie->streams[i].sf_size - 8;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->Size = movie->streams[i].sf_size - 8;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->Width = 0;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->Height = 0;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->Planes = 1;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->BitCount = 24;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->Compression = avi_get_format_compression (movie->streams[i].format);
			((AviBitmapInfoHeader *) movie->streams[i].sf)->SizeImage = 0;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->XPelsPerMeter = 0;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->YPelsPerMeter = 0;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->ClrUsed = 0;
			((AviBitmapInfoHeader *) movie->streams[i].sf)->ClrImportant = 0;

/*
			if (movie->streams[i].format == AVI_FORMAT_MJPEG) {
				AviMJPEGUnknown *tmp;
				
				tmp = (AviMJPEGUnknown *) ((char*) movie->streams[i].sf +sizeof(AviBitmapInfoHeader));
				
				tmp->a = 44;
				tmp->b = 24;
				tmp->c = 0;
				tmp->d = 2;
				tmp->e = 8;
				tmp->f = 2;
				tmp->g = 1;
			}
		}
		else if (movie->streams[i].sh.Type == FCC("auds")) {
			// pass
		}
*/
		}
	}

	list.fcc = FCC("RIFF");
	list.size = 0;
	list.ids = FCC("AVI ");

	awrite (movie, &list, 1, sizeof(AviList), movie->fp, AVI_LIST);

	list.fcc = FCC("LIST");
	list.size = 0;
	list.ids = FCC("hdrl");

	awrite (movie, &list, 1, sizeof(AviList), movie->fp, AVI_LIST);

	header_pos1 = ftell(movie->fp);

	movie->offset_table[0] = ftell(movie->fp);

	awrite (movie, movie->header, 1, sizeof(AviMainHeader), movie->fp, AVI_MAINH);

	for (i=0; i < movie->header->Streams; i++) {
		list.fcc = FCC("LIST");
		list.size = 0;
		list.ids = FCC("strl");

		awrite (movie, &list, 1, sizeof(AviList), movie->fp, AVI_LIST);

		stream_pos1 = ftell(movie->fp);

		movie->offset_table[1+i*2] = ftell(movie->fp);
		awrite (movie, &movie->streams[i].sh, 1, sizeof(AviStreamHeader), movie->fp, AVI_STREAMH);

		movie->offset_table[1+i*2+1] = ftell(movie->fp);
		awrite (movie, movie->streams[i].sf, 1, movie->streams[i].sf_size, movie->fp, AVI_BITMAPH);

		stream_pos2 = ftell(movie->fp);

		fseek (movie->fp, stream_pos1-8, SEEK_SET);

		PUT_FCCN((stream_pos2-stream_pos1+4L), movie->fp);

		fseek (movie->fp, stream_pos2, SEEK_SET);
	}

	if (ftell(movie->fp) < 2024 - 8) {
		chunk.fcc = FCC("JUNK");
		chunk.size = 2024-8-ftell(movie->fp);

		awrite (movie, &chunk, 1, sizeof(AviChunk), movie->fp, AVI_CHUNK);

		for (i=0; i < chunk.size; i++)
			putc(0, movie->fp);
	}

	header_pos2 = ftell(movie->fp);

	list.fcc = FCC("LIST");
	list.size = 0;
	list.ids = FCC("movi");

	awrite (movie, &list, 1, sizeof(AviList), movie->fp, AVI_LIST);

	movie->movi_offset = ftell(movie->fp)-8L;

	fseek (movie->fp, AVI_HDRL_SOFF, SEEK_SET);

	PUT_FCCN((header_pos2-header_pos1+4L), movie->fp);

	return AVI_ERROR_NONE;
}

AviError AVI_write_frame (AviMovie *movie, int frame_num, ...)
{
	AviList list;
	AviChunk chunk;
	AviIndexEntry *temp;
	va_list ap;
	int stream;
	int64_t rec_off;
	AviFormat format;
	void *buffer;
	int size;

	if (frame_num < 0)
		return AVI_ERROR_OPTION;

	/* Allocate the new memory for the index entry */

	if (frame_num+1 > movie->index_entries) {
		temp = (AviIndexEntry *) MEM_mallocN ((frame_num+1) * 
			(movie->header->Streams+1) * sizeof(AviIndexEntry),"newidxentry");
		if (movie->entries != NULL) {
			memcpy (temp, movie->entries, movie->index_entries * (movie->header->Streams+1)
				* sizeof(AviIndexEntry));
			MEM_freeN (movie->entries);
		}

		movie->entries = temp;
		movie->index_entries = frame_num+1;
	}

	/* Slap a new record entry onto the end of the file */

	fseek (movie->fp, 0L, SEEK_END);

	list.fcc = FCC("LIST");
	list.size = 0;
	list.ids = FCC("rec ");

	awrite (movie, &list, 1, sizeof(AviList), movie->fp, AVI_LIST);

	rec_off = ftell (movie->fp)-8L;

	/* Write a frame for every stream */

	va_start (ap, frame_num);

	for (stream=0; stream < movie->header->Streams; stream++) {
		unsigned int tbuf=0;
		
		format = va_arg (ap, AviFormat);
		buffer = va_arg (ap, void*);
		size = va_arg (ap, int);

		/* Convert the buffer into the output format */
		buffer = avi_format_convert (movie, stream, buffer, format, movie->streams[stream].format, &size);

		/* Write the header info for this data chunk */

		fseek (movie->fp, 0L, SEEK_END);

		chunk.fcc = avi_get_data_id (format, stream);
		chunk.size = size;
		
		if (size%4) chunk.size += 4 - size%4;
		
		awrite (movie, &chunk, 1, sizeof(AviChunk), movie->fp, AVI_CHUNK);

		/* Write the index entry for this data chunk */

		movie->entries[frame_num * (movie->header->Streams+1) + stream + 1].ChunkId = chunk.fcc;
		movie->entries[frame_num * (movie->header->Streams+1) + stream + 1].Flags = AVIIF_KEYFRAME;
		movie->entries[frame_num * (movie->header->Streams+1) + stream + 1].Offset = ftell(movie->fp)-12L-movie->movi_offset;
		movie->entries[frame_num * (movie->header->Streams+1) + stream + 1].Size = chunk.size;

		/* Write the chunk */
		awrite (movie, buffer, 1, size, movie->fp, AVI_RAW);
		MEM_freeN (buffer);

		if (size%4) awrite (movie, &tbuf, 1, 4-size%4, movie->fp, AVI_RAW);

		/* Update the stream headers length field */
		movie->streams[stream].sh.Length++;
		fseek (movie->fp, movie->offset_table[1+stream*2], SEEK_SET);
		awrite (movie, &movie->streams[stream].sh, 1, sizeof(AviStreamHeader), movie->fp, AVI_STREAMH);
	}
	va_end (ap);

	/* Record the entry for the new record */

	fseek (movie->fp, 0L, SEEK_END);

	movie->entries[frame_num * (movie->header->Streams+1)].ChunkId = FCC("rec ");
	movie->entries[frame_num * (movie->header->Streams+1)].Flags = AVIIF_LIST;
	movie->entries[frame_num * (movie->header->Streams+1)].Offset = rec_off-8L-movie->movi_offset;
	movie->entries[frame_num * (movie->header->Streams+1)].Size = ftell(movie->fp)-(rec_off+4L);

	/* Update the record size */
	fseek (movie->fp, rec_off, SEEK_SET);
	PUT_FCCN (movie->entries[frame_num * (movie->header->Streams+1)].Size, movie->fp);

	/* Update the main header information in the file */
	movie->header->TotalFrames++;
	fseek (movie->fp, movie->offset_table[0], SEEK_SET);
	awrite (movie, movie->header, 1, sizeof(AviMainHeader), movie->fp, AVI_MAINH);

	return AVI_ERROR_NONE;
}

AviError AVI_close_compress (AviMovie *movie)
{
	int temp, movi_size, i;

	fseek (movie->fp, 0L, SEEK_END);
	movi_size = ftell (movie->fp);

	PUT_FCC ("idx1", movie->fp);
	PUT_FCCN ((movie->index_entries*(movie->header->Streams+1)*16), movie->fp);

	for (temp=0; temp < movie->index_entries*(movie->header->Streams+1); temp++)
		awrite (movie, &movie->entries[temp], 1, sizeof(AviIndexEntry), movie->fp, AVI_INDEXE);

	temp = ftell (movie->fp);

	fseek (movie->fp, AVI_RIFF_SOFF, SEEK_SET);

	PUT_FCCN((temp-8L), movie->fp);

	fseek (movie->fp, movie->movi_offset, SEEK_SET);

	PUT_FCCN((movi_size-(movie->movi_offset+4L)),movie->fp);

	fclose (movie->fp);

	for (i=0; i < movie->header->Streams; i++) {
		if (movie->streams[i].sf != NULL)
			MEM_freeN (movie->streams[i].sf);
	}
	if (movie->header != NULL)
		MEM_freeN (movie->header);
	if (movie->entries != NULL)
		MEM_freeN (movie->entries);
	if (movie->streams != NULL)
		MEM_freeN (movie->streams);
	if (movie->offset_table != NULL)
		MEM_freeN (movie->offset_table);
	return AVI_ERROR_NONE;
}

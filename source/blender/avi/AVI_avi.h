/**
 * @mainpage AVI - AVI module external interface
 *
 * @section about About the AVI module
 *
 * This is external code. It provides avi file import/export and
 * conversions. It has been adapted to make use of Blender memory
 * management functions, and because of this it needs module
 * blenlib. You need to provide this lib when linking with libavi.a .
 *
 * @section issues Known issues with AVI
 *
 * - avi uses mallocN, freeN from blenlib. 
 * - Not all functions that are used externally are properly
 *   prototyped.
 *
 * This header has not been split, since it interleaves type defines
 * and functions. You would need the types to be able to include the
 * function headers anyway. And, after all, it is someone else's
 * code. So we keep it like this.
 *
 * $Id$ 
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

#ifndef __AVI_H__
#define __AVI_H__

#include <stdio.h> /* for FILE */

typedef struct _AviChunk {
  int fcc;
  int size;
} AviChunk;

typedef struct _AviList {
  int fcc;
  int size;
  int ids;
} AviList;

typedef struct _AviMainHeader {
  int fcc;
  int size;  
  int MicroSecPerFrame;       /* MicroSecPerFrame - timing between frames */
  int MaxBytesPerSec;        /* MaxBytesPerSec - approx bps system must handle */
  int PaddingGranularity;
  int Flags;
#define AVIF_HASINDEX           0x00000010        /* had idx1 chunk */
#define AVIF_MUSTUSEINDEX       0x00000020        /* must use idx1 chunk to determine order */
#define AVIF_ISINTERLEAVED      0x00000100        /* AVI file is interleaved */
#define AVIF_TRUSTCKTYPE        0x00000800
#define AVIF_WASCAPTUREFILE     0x00010000        /* specially allocated used for capturing real time video */
#define AVIF_COPYRIGHTED        0x00020000        /* contains copyrighted data */

  int TotalFrames;
  int InitialFrames;    /* InitialFrames - initial frame before interleaving */
  int Streams;
  int SuggestedBufferSize;
  int Width;
  int Height;
  int Reserved[4];
} AviMainHeader;

typedef struct _AviStreamHeader {
  int fcc;
  int size;  
  int Type;
#define AVIST_VIDEO FCC("vids")
#define AVIST_AUDIO FCC("auds")
#define AVIST_MIDI  FCC("mids")
#define AVIST_TEXT  FCC("txts")
  
  int Handler;
  int Flags;
#define AVISF_DISABLED 0x00000001
#define AVISF_VIDEO_PALCHANGES 0x00010000

  short Priority;
  short Language;
  int InitialFrames;
  int Scale;
  int Rate;
  int Start;
  int Length;
  int SuggestedBufferSize;
  int Quality;
  int SampleSize;
  short left;
  short top;
  short right;
  short bottom;
} AviStreamHeader;

typedef struct _AviBitmapInfoHeader {
  int fcc;
  int size;  
  int Size;
  int Width;
  int Height;
  short Planes;
  short BitCount;
  int Compression;
  int SizeImage;
  int XPelsPerMeter;
  int YPelsPerMeter;
  int ClrUsed;
  int ClrImportant;
} AviBitmapInfoHeader;

typedef struct _AviMJPEGUnknown {
  int a;
  int b;
  int c;
  int d;
  int e;
  int f;
  int g;
} AviMJPEGUnknown;

typedef struct _AviIndexEntry {
  int ChunkId;
  int Flags;
#define AVIIF_LIST       0x00000001
#define AVIIF_KEYFRAME   0x00000010 
#define AVIIF_NO_TIME    0x00000100
#define AVIIF_COMPRESSOR 0x0FFF0000
  int Offset;
  int Size;
} AviIndexEntry;

typedef struct _AviIndex {
  int fcc;
  int size;
  AviIndexEntry *entrys;
} AviIndex;

typedef enum {
  AVI_FORMAT_RGB24,  /* The most basic of forms, 3 bytes per pixel, 1 per r, g, b */
  AVI_FORMAT_RGB32,  /* The second most basic of forms, 4 bytes per pixel, 1 per r, g, b, alpha */
  AVI_FORMAT_AVI_RGB, /* Same as above, but is in the wierd AVI order (bottom to top, left to right) */
  AVI_FORMAT_MJPEG /* Motion-JPEG */
} AviFormat;

typedef struct _AviStreamRec {
  AviStreamHeader sh;
  void *sf;
  int sf_size;
  AviFormat format;
} AviStreamRec;

typedef struct _AviMovie {
    FILE *fp; 
	
	int type;
#define AVI_MOVIE_READ  0
#define AVI_MOVIE_WRITE 1
	
	unsigned long size;

	AviMainHeader *header;
	AviStreamRec *streams;
	AviIndexEntry *entries;
	int index_entries;
	
	int movi_offset;
	int read_offset;
	long *offset_table;
	
	/* Local data goes here */
	int interlace;
	int odd_fields;
} AviMovie;

typedef enum {
  AVI_ERROR_NONE=0,
  AVI_ERROR_COMPRESSION,
  AVI_ERROR_OPEN,
  AVI_ERROR_READING,
  AVI_ERROR_WRITING,
  AVI_ERROR_FORMAT,
  AVI_ERROR_ALLOC,
  AVI_ERROR_FOUND,
  AVI_ERROR_OPTION
} AviError;

/* belongs to the option-setting function. */
typedef enum {
  AVI_OPTION_WIDTH=0,
  AVI_OPTION_HEIGHT, 
  AVI_OPTION_QUALITY,
  AVI_OPTION_FRAMERATE
} AviOption;

/* The offsets that will always stay the same in AVI files we
 * write... used to seek around to the places where we need to write
 * the sizes */

#define AVI_RIFF_SOFF 4L
#define AVI_HDRL_SOFF 16L

/**
 * This is a sort of MAKE_ID thing. Used in imbuf :( It is used
 * through options in the AVI header (AviStreamHeader). */
#define FCC(ch4) (ch4[0] | ch4[1]<<8 | ch4[2]<<16 | ch4[3] << 24)

/**
 * Test whether this is an avi-format.
 */
int AVI_is_avi (char *name);


/**
 * Open a compressed file, decompress it into memory.
 */
AviError AVI_open_compress (char *name, AviMovie *movie, int streams, ...);

/**
 * Finalize a compressed output stream.
 */
AviError AVI_close_compress (AviMovie *movie);

/**
 * Choose a compression option for <movie>. Possible options are
 * AVI_OPTION_TYPE_MAIN, AVI_OPTION_TYPE_STRH, AVI_OPTION_TYPE_STRF
 */
AviError AVI_set_compress_option (AviMovie *movie,
								  int option_type,
								  int stream,
								  AviOption option,
								  void *opt_data);
/* Hmmm... there should be some explanantion about what these mean */
/**
 * Compression option, for use in avi_set_compress_option
 */
#define AVI_OPTION_TYPE_MAIN 0
/**
 * Compression option, for use in avi_set_compress_option
 */
#define AVI_OPTION_TYPE_STRH 1
/**
 * Compression option, for use in avi_set_compress_option
 */
#define AVI_OPTION_TYPE_STRF 2

/**
 * Direct the streams <avist_type> to <movie>. Redirect <stream_num>
 * streams.
 */
int AVI_get_stream (AviMovie *movie, int avist_type, int stream_num);

/**
 * Open a movie stream from file.
 */
AviError AVI_open_movie (char *name, AviMovie *movie);

/**
 * Read a frame from a movie stream.
 */
void *AVI_read_frame (AviMovie *movie,
					  AviFormat format,
					  int frame,
					  int stream);
/**
 * Close an open movie stream.
 */
AviError AVI_close (AviMovie *movie);

/**
 * Write frames to a movie stream.
 */
AviError AVI_write_frame (AviMovie *movie, int frame_num, ...);

/**
 * Unused but still external
 */
AviError AVI_print_error (AviError error);
void AVI_set_debug (int mode);

#endif /* __AVI_H__ */


/**
 * anim.c
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef _WIN32
#define INC_OLE2
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <memory.h>
#include <commdlg.h>

#ifndef FREE_WINDOWS
#include <vfw.h>
#endif

#undef AVIIF_KEYFRAME // redefined in AVI_avi.h
#undef AVIIF_LIST // redefined in AVI_avi.h

#define FIXCC(fcc)  if (fcc == 0)	fcc = mmioFOURCC('N', 'o', 'n', 'e'); \
		if (fcc == BI_RLE8) fcc = mmioFOURCC('R', 'l', 'e', '8');
#endif

#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef _WIN32
#include <dirent.h>
#else
#include <io.h>
#endif

#include "BLI_blenlib.h" /* BLI_remlink BLI_filesize BLI_addtail
                            BLI_countlist BLI_stringdec */
#include "DNA_userdef_types.h"
#include "BKE_global.h"

#include "imbuf.h"
#include "imbuf_patch.h"

#include "AVI_avi.h"

#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__)
#include "quicktime_import.h"
#endif /* _WIN32 || __APPLE__ */
#endif /* WITH_QUICKTIME */

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_bitplanes.h"
#include "IMB_anim.h"
#include "IMB_anim5.h"

#ifdef WITH_FFMPEG
#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>
#include <ffmpeg/rational.h>
#include <ffmpeg/swscale.h>

#if LIBAVFORMAT_VERSION_INT < (49 << 16)
#define FFMPEG_OLD_FRAME_RATE 1
#else
#define FFMPEG_CODEC_IS_POINTER 1
#endif

#endif

#ifdef WITH_REDCODE
#include <redcode/format.h>
#include <redcode/codec.h>
#endif

/****/

#ifdef __sgi

#include <dmedia/moviefile.h>

static void movie_printerror(char * str) {
	const char * errstr = mvGetErrorStr(mvGetErrno());

	if (str) {
		if (errstr) printf("%s: %s\n", str, errstr);
		else printf("%s: returned error\n", str);
	} else printf("%s\n", errstr);
}

static int startmovie(struct anim * anim) {
	if (anim == 0) return(-1);

	if ( mvOpenFile (anim->name, O_BINARY|O_RDONLY, &anim->movie ) != DM_SUCCESS ) {
		printf("Can't open movie: %s\n", anim->name);
		return(-1);
	}
	if ( mvFindTrackByMedium (anim->movie, DM_IMAGE, &anim->track) != DM_SUCCESS ) {
		printf("No image track in movie: %s\n", anim->name);
		mvClose(anim->movie);
		return(-1);
	}

	anim->duration = mvGetTrackLength (anim->track);
	anim->params = mvGetParams( anim->track );

	anim->x = dmParamsGetInt( anim->params, DM_IMAGE_WIDTH);
	anim->y = dmParamsGetInt( anim->params, DM_IMAGE_HEIGHT);
	anim->interlacing = dmParamsGetEnum (anim->params, DM_IMAGE_INTERLACING);
	anim->orientation = dmParamsGetEnum (anim->params, DM_IMAGE_ORIENTATION);
	anim->framesize = dmImageFrameSize(anim->params);

	anim->curposition = 0;
	anim->preseek = 0;

	/*printf("x:%d y:%d size:%d interl:%d dur:%d\n", anim->x, anim->y, anim->framesize, anim->interlacing, anim->duration);*/
	return (0);
}

static ImBuf * movie_fetchibuf(struct anim * anim, int position) {
	ImBuf * ibuf;
/*  	extern rectcpy(); */
	int size;
	unsigned int *rect1, *rect2;

	if (anim == 0) return (0);

	ibuf = IMB_allocImBuf(anim->x, anim->y, 24, IB_rect, 0);

	if ( mvReadFrames(anim->track, position, 1, ibuf->x * ibuf->y * 
		sizeof(int), ibuf->rect ) != DM_SUCCESS ) {
		movie_printerror("mvReadFrames");
		IMB_freeImBuf(ibuf);
		return(0);
	}

/*
	if (anim->interlacing == DM_IMAGE_INTERLACED_EVEN) {
		rect1 = ibuf->rect + (ibuf->x * ibuf->y) - 1;
		rect2 = rect1 - ibuf->x;
    
		for (size = ibuf->x * (ibuf->y - 1); size > 0; size--){
			*rect1-- = *rect2--;
		}
	}
*/

	if (anim->interlacing == DM_IMAGE_INTERLACED_EVEN)
	{
		rect1 = ibuf->rect;
		rect2 = rect1 + ibuf->x;

		for (size = ibuf->x * (ibuf->y - 1); size > 0; size--){
			*rect1++ = *rect2++;
		}
	}
	/*if (anim->orientation == DM_TOP_TO_BOTTOM) IMB_flipy(ibuf);*/


	return(ibuf);
}

static void free_anim_movie(struct anim * anim) {
	if (anim == NULL) return;

	if (anim->movie) {
		mvClose(anim->movie);
		anim->movie = NULL;
	}
	anim->duration = 0;
}

int ismovie(char *name) {
	return (mvIsMovieFile(name) == DM_TRUE);
}

#else

int ismovie(char *name) {
	return 0;
}

	/* never called, just keep the linker happy */
static int startmovie(struct anim * anim) { return 1; }
static ImBuf * movie_fetchibuf(struct anim * anim, int position) { return NULL; }
static void free_anim_movie(struct anim * anim) { ; }

#endif

static int an_stringdec(char *string, char* kop, char *staart,unsigned short *numlen) {
	unsigned short len,nume,nums=0;
	short i,found=FALSE;

	len=strlen(string);
	nume = len;

	for(i=len-1;i>=0;i--){
		if (string[i]=='/') break;
		if (isdigit(string[i])) {
			if (found){
				nums=i;
			} else{
				nume=i;
				nums=i;
				found=TRUE;
			}
		} else{
			if (found) break;
		}
	}
	if (found){
		strcpy(staart,&string[nume+1]);
		strcpy(kop,string);
		kop[nums]=0;
		*numlen=nume-nums+1;
		return ((int)atoi(&(string[nums])));
	}
	staart[0]=0;
	strcpy(kop,string);
	*numlen=0;
	return (1);
}


static void an_stringenc(char *string, char *kop, char *staart, 
unsigned short numlen, int pic) {
	char numstr[10];
	unsigned short len,i;

	len=sprintf(numstr,"%d",pic);

	strcpy(string,kop);
	for(i=len;i<numlen;i++){
		strcat(string,"0");
	}
	strcat(string,numstr);
	strcat(string,staart);
}


static void free_anim_avi (struct anim *anim) {
#if defined(_WIN32) && !defined(FREE_WINDOWS)
	int i;
#endif

	if (anim == NULL) return;
	if (anim->avi == NULL) return;

	AVI_close (anim->avi);
	MEM_freeN (anim->avi);
	anim->avi = NULL;

#if defined(_WIN32) && !defined(FREE_WINDOWS)

	if (anim->pgf) {
		AVIStreamGetFrameClose(anim->pgf);
		anim->pgf = NULL;
	}

	for (i = 0; i < anim->avistreams; i++){
		AVIStreamRelease(anim->pavi[i]);
	}
	anim->avistreams = 0;

	if (anim->pfileopen) {
		AVIFileRelease(anim->pfile);
		anim->pfileopen = 0;
		AVIFileExit();
	}
#endif

	anim->duration = 0;
}

void IMB_free_anim_ibuf(struct anim * anim) {
	if (anim == NULL) return;

	if (anim->ibuf1) IMB_freeImBuf(anim->ibuf1);
	if (anim->ibuf2) IMB_freeImBuf(anim->ibuf2);

	anim->ibuf1 = anim->ibuf2 = NULL;
}

#ifdef WITH_FFMPEG
static void free_anim_ffmpeg(struct anim * anim);
#endif
#ifdef WITH_REDCODE
static void free_anim_redcode(struct anim * anim);
#endif

void IMB_free_anim(struct anim * anim) {
	if (anim == NULL) {
		printf("free anim, anim == NULL\n");
		return;
	}

	IMB_free_anim_ibuf(anim);
	free_anim_anim5(anim);
	free_anim_movie(anim);
	free_anim_avi(anim);

#ifdef WITH_QUICKTIME
	free_anim_quicktime(anim);
#endif
#ifdef WITH_FFMPEG
	free_anim_ffmpeg(anim);
#endif
#ifdef WITH_REDCODE
	free_anim_redcode(anim);
#endif

	free(anim);
}

void IMB_close_anim(struct anim * anim) {
	if (anim == 0) return;

	IMB_free_anim(anim);
}


struct anim * IMB_open_anim( const char * name, int ib_flags) {
	struct anim * anim;

	anim = (struct anim*)MEM_callocN(sizeof(struct anim), "anim struct");
	if (anim != NULL) {
		strcpy(anim->name, name);  /* fixme: possible buffer overflow here? */
		anim->ib_flags = ib_flags;
	}
	return(anim);
}


static int startavi (struct anim *anim) {

	AviError avierror;
#if defined(_WIN32) && !defined(FREE_WINDOWS)
	HRESULT	hr;
	int i, firstvideo = -1;
	BYTE abFormat[1024];
	LONG l;
	LPBITMAPINFOHEADER lpbi;
	AVISTREAMINFO avis;
#endif

	anim->avi = MEM_callocN (sizeof(AviMovie),"animavi");

	if (anim->avi == NULL) {
		printf("Can't open avi: %s\n", anim->name);
		return -1;
	}

	avierror = AVI_open_movie (anim->name, anim->avi);

#if defined(_WIN32) && !defined(FREE_WINDOWS)
	if (avierror == AVI_ERROR_COMPRESSION) {
		AVIFileInit();
		hr = AVIFileOpen(&anim->pfile, anim->name, OF_READ, 0L);
		if (hr == 0) {
			anim->pfileopen = 1;
			for (i = 0; i < MAXNUMSTREAMS; i++) {
				if (AVIFileGetStream(anim->pfile, &anim->pavi[i], 0L, i) != AVIERR_OK) {
					break;
				}
				
				AVIStreamInfo(anim->pavi[i], &avis, sizeof(avis));
				if ((avis.fccType == streamtypeVIDEO) && (firstvideo == -1)) {
					anim->pgf = AVIStreamGetFrameOpen(anim->pavi[i], NULL);
					if (anim->pgf) {
						firstvideo = i;

						// get stream length
						anim->avi->header->TotalFrames = AVIStreamLength(anim->pavi[i]);
						
						// get information about images inside the stream
						l = sizeof(abFormat);
						AVIStreamReadFormat(anim->pavi[i], 0, &abFormat, &l);
						lpbi = (LPBITMAPINFOHEADER)abFormat;
						anim->avi->header->Height = lpbi->biHeight;
						anim->avi->header->Width = lpbi->biWidth;
					} else {
						FIXCC(avis.fccHandler);
						FIXCC(avis.fccType);
						printf("Can't find AVI decoder for type : %4.4hs/%4.4hs\n",
							(LPSTR)&avis.fccType,
							(LPSTR)&avis.fccHandler);
					}
				}
			}

			// register number of opened avistreams
			anim->avistreams = i;

			//
			// Couldn't get any video streams out of this file
			//
			if ((anim->avistreams == 0) || (firstvideo == -1)) {
				avierror = AVI_ERROR_FORMAT;
			} else {
				avierror = AVI_ERROR_NONE;
				anim->firstvideo = firstvideo;
			}
		} else {
			AVIFileExit();
		}
	}
#endif

	if (avierror != AVI_ERROR_NONE) {
		AVI_print_error(avierror);
		printf ("Error loading avi: %s\n", anim->name);		
		free_anim_avi(anim);
		return -1;
	}
	
	anim->duration = anim->avi->header->TotalFrames;
	anim->params = 0;

	anim->x = anim->avi->header->Width;
	anim->y = anim->avi->header->Height;
	anim->interlacing = 0;
	anim->orientation = 0;
	anim->framesize = anim->x * anim->y * 4;

	anim->curposition = 0;
	anim->preseek = 0;

	/*  printf("x:%d y:%d size:%d interl:%d dur:%d\n", anim->x, anim->y, anim->framesize, anim->interlacing, anim->duration);*/

	return 0;
}

static ImBuf * avi_fetchibuf (struct anim *anim, int position) {
	ImBuf *ibuf = NULL;
	int *tmp;
	int y;
	
	if (anim == NULL) return (NULL);

#if defined(_WIN32) && !defined(FREE_WINDOWS)
	if (anim->avistreams) {
		LPBITMAPINFOHEADER lpbi;

		if (anim->pgf) {
			lpbi = AVIStreamGetFrame(anim->pgf, position + AVIStreamStart(anim->pavi[anim->firstvideo]));
			if (lpbi) {
				ibuf = IMB_ibImageFromMemory((int *) lpbi, 100, IB_rect);
//Oh brother...
			}
		}
	} else {
#else
	if (1) {
#endif
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, IB_rect, 0);

		tmp = AVI_read_frame (anim->avi, AVI_FORMAT_RGB32, position,
			AVI_get_stream(anim->avi, AVIST_VIDEO, 0));
		
		if (tmp == NULL) {
			printf ("Error reading frame from AVI");
			IMB_freeImBuf (ibuf);
			return NULL;
		}

		for (y=0; y < anim->y; y++) {
			memcpy (&(ibuf->rect)[((anim->y-y)-1)*anim->x],  &tmp[y*anim->x],  
					anim->x * 4);
		}
		
		MEM_freeN (tmp);
	}

	return ibuf;
}

#ifdef WITH_FFMPEG

extern void do_init_ffmpeg();

#ifdef FFMPEG_CODEC_IS_POINTER
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return stream->codec;
}
#else
static AVCodecContext* get_codec_from_stream(AVStream* stream)
{
	return &stream->codec;
}
#endif

static int startffmpeg(struct anim * anim) {
	int            i, videoStream;

	AVCodec *pCodec;
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;

	if (anim == 0) return(-1);

	do_init_ffmpeg();

	if(av_open_input_file(&pFormatCtx, anim->name, NULL, 0, NULL)!=0) {
		return -1;
	}

	if(av_find_stream_info(pFormatCtx)<0) {
		av_close_input_file(pFormatCtx);
		return -1;
	}

	dump_format(pFormatCtx, 0, anim->name, 0);


        /* Find the first video stream */
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++)
		if(get_codec_from_stream(pFormatCtx->streams[i])->codec_type
		   == CODEC_TYPE_VIDEO)	{
			videoStream=i;
			break;
		}

	if(videoStream==-1) {
		av_close_input_file(pFormatCtx);
		return -1;
	}

	pCodecCtx = get_codec_from_stream(pFormatCtx->streams[videoStream]);

        /* Find the decoder for the video stream */
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		av_close_input_file(pFormatCtx);
		return -1;
	}

	pCodecCtx->workaround_bugs = 1;

	if(avcodec_open(pCodecCtx, pCodec)<0) {
		av_close_input_file(pFormatCtx);
		return -1;
	}

#ifdef FFMPEG_OLD_FRAME_RATE
	if(pCodecCtx->frame_rate>1000 && pCodecCtx->frame_rate_base==1)
		pCodecCtx->frame_rate_base=1000;


	anim->duration = pFormatCtx->duration * pCodecCtx->frame_rate 
		/ pCodecCtx->frame_rate_base / AV_TIME_BASE;
#else
	anim->duration = pFormatCtx->duration 
		* av_q2d(pFormatCtx->streams[videoStream]->r_frame_rate) 
		/ AV_TIME_BASE;

#endif
	anim->params = 0;

	anim->x = pCodecCtx->width;
	anim->y = pCodecCtx->height;
	anim->interlacing = 0;
	anim->orientation = 0;
	anim->framesize = anim->x * anim->y * 4;

	anim->curposition = -1;

	anim->pFormatCtx = pFormatCtx;
	anim->pCodecCtx = pCodecCtx;
	anim->pCodec = pCodec;
	anim->videoStream = videoStream;

	anim->pFrame = avcodec_alloc_frame();
	anim->pFrameDeinterlaced = avcodec_alloc_frame();
	anim->pFrameRGB = avcodec_alloc_frame();

	if (avpicture_get_size(PIX_FMT_BGR32, anim->x, anim->y)
	    != anim->x * anim->y * 4) {
		fprintf (stderr,
			 "ffmpeg has changed alloc scheme ... ARGHHH!\n");
		avcodec_close(anim->pCodecCtx);
		av_close_input_file(anim->pFormatCtx);
		av_free(anim->pFrameRGB);
		av_free(anim->pFrameDeinterlaced);
		av_free(anim->pFrame);
		anim->pCodecCtx = NULL;
		return -1;
	}

	if (anim->ib_flags & IB_animdeinterlace) {
		avpicture_fill((AVPicture*) anim->pFrameDeinterlaced, 
			       MEM_callocN(avpicture_get_size(
						   anim->pCodecCtx->pix_fmt,
						   anim->x, anim->y), 
					   "ffmpeg deinterlace"), 
			       anim->pCodecCtx->pix_fmt, anim->x, anim->y);
	}

	if (pCodecCtx->has_b_frames) {
		anim->preseek = 25; /* FIXME: detect gopsize ... */
	} else {
		anim->preseek = 0;
	}
	
	anim->img_convert_ctx = sws_getContext(
		anim->pCodecCtx->width,
		anim->pCodecCtx->height,
		anim->pCodecCtx->pix_fmt,
		anim->pCodecCtx->width,
		anim->pCodecCtx->height,
		PIX_FMT_BGR32,
		SWS_FAST_BILINEAR | SWS_PRINT_INFO,
		NULL, NULL, NULL);
		
	if (!anim->img_convert_ctx) {
		fprintf (stderr,
			 "Can't transform color space??? Bailing out...\n");
		avcodec_close(anim->pCodecCtx);
		av_close_input_file(anim->pFormatCtx);
		av_free(anim->pFrameRGB);
		av_free(anim->pFrameDeinterlaced);
		av_free(anim->pFrame);
		anim->pCodecCtx = NULL;
		return -1;
	}
		
	return (0);
}

static ImBuf * ffmpeg_fetchibuf(struct anim * anim, int position) {
	ImBuf * ibuf;
	int frameFinished;
	AVPacket packet;
	int64_t pts_to_search = 0;
	int pos_found = 1;
	int filter_y = 0;

	if (anim == 0) return (0);

	ibuf = IMB_allocImBuf(anim->x, anim->y, 24, IB_rect, 0);

	avpicture_fill((AVPicture*) anim->pFrameRGB, 
		       (unsigned char*) ibuf->rect, 
		       PIX_FMT_BGR32, anim->x, anim->y);

	if (position != anim->curposition + 1) { 
		if (position > anim->curposition + 1 
		    && anim->preseek 
		    && position - (anim->curposition + 1) < anim->preseek) {
			while(av_read_frame(anim->pFormatCtx, &packet)>=0) {
				if (packet.stream_index == anim->videoStream) {
					avcodec_decode_video(
						anim->pCodecCtx, 
						anim->pFrame, &frameFinished, 
						packet.data, packet.size);

					if (frameFinished) {
						anim->curposition++;
					}
				}
				av_free_packet(&packet);
				if (position == anim->curposition+1) {
					break;
				}
			}
		}
	}

	if (position != anim->curposition + 1) { 
#ifdef FFMPEG_OLD_FRAME_RATE
		double frame_rate = 
			(double) anim->pCodecCtx->frame_rate
			/ (double) anim->pCodecCtx->frame_rate_base;
#else
		double frame_rate = 
			av_q2d(anim->pFormatCtx->streams[anim->videoStream]
			       ->r_frame_rate);
#endif
		double time_base = 
			av_q2d(anim->pFormatCtx->streams[anim->videoStream]
			       ->time_base);
		long long pos = (long long) (position - anim->preseek) 
			* AV_TIME_BASE / frame_rate;
		long long st_time = anim->pFormatCtx
			->streams[anim->videoStream]->start_time;

		if (pos < 0) {
			pos = 0;
		}

		if (st_time != AV_NOPTS_VALUE) {
			pos += st_time * AV_TIME_BASE * time_base;
		}

		av_seek_frame(anim->pFormatCtx, -1, 
			      pos, AVSEEK_FLAG_BACKWARD);

		pts_to_search = (long long) 
			(((double) position) / time_base / frame_rate);
		if (st_time != AV_NOPTS_VALUE) {
			pts_to_search += st_time;
		}

		pos_found = 0;
		avcodec_flush_buffers(anim->pCodecCtx);
	}

	while(av_read_frame(anim->pFormatCtx, &packet)>=0) {
		if(packet.stream_index == anim->videoStream) {
			avcodec_decode_video(anim->pCodecCtx, 
					     anim->pFrame, &frameFinished, 
					     packet.data, packet.size);

			if (frameFinished && !pos_found) {
				if (packet.dts >= pts_to_search) {
					pos_found = 1;
				}
			} 

			if(frameFinished && pos_found == 1) {
				AVFrame * input = anim->pFrame;

				/* This means the data wasnt read properly, 
				   this check stops crashing */
				if (input->data[0]==0 && input->data[1]==0 
				    && input->data[2]==0 && input->data[3]==0){
					av_free_packet(&packet);
					break;
				}

				if (anim->ib_flags & IB_animdeinterlace) {
					if (avpicture_deinterlace(
						    (AVPicture*) 
						    anim->pFrameDeinterlaced,
						    (const AVPicture*)
						    anim->pFrame,
						    anim->pCodecCtx->pix_fmt,
						    anim->pCodecCtx->width,
						    anim->pCodecCtx->height)
					    < 0) {
						filter_y = 1;
					} else {
						input = anim->pFrameDeinterlaced;
					}
				}

				if (G.order == B_ENDIAN) {
					int * dstStride 
						= anim->pFrameRGB->linesize;
					uint8_t** dst = anim->pFrameRGB->data;
					int dstStride2[4]
						= { dstStride[0], 0, 0, 0 };
					uint8_t* dst2[4]= {
						dst[0],	0, 0, 0 };
					int x,y,h,w;
					unsigned char* bottom;
					unsigned char* top;

					sws_scale(anim->img_convert_ctx,
						  input->data,
						  input->linesize,
						  0,
						  anim->pCodecCtx->height,
						  dst2,
						  dstStride2);
				
					/* workaround: sws_scale 
					   sets alpha = 0 and compensate
					   for altivec-bugs and flipy... */
				
					bottom = (unsigned char*) ibuf->rect;
					top = bottom 
						+ ibuf->x * (ibuf->y-1) * 4;

					h = (ibuf->y + 1) / 2;
					w = ibuf->x;

					for (y = 0; y < h; y++) {
						unsigned char tmp[4];
						unsigned long * tmp_l =
							(unsigned long*) tmp;
						tmp[3] = 0xff;

						for (x = 0; x < w; x++) {
							tmp[0] = bottom[3];
							tmp[1] = bottom[2];
							tmp[2] = bottom[1];

							bottom[0] = top[3];
							bottom[1] = top[2];
							bottom[2] = top[1];
							bottom[3] = 0xff;
								
							*(unsigned long*) top
								= *tmp_l;

							bottom +=4;
							top += 4;
						}
						top -= 8 * w;
					}

					av_free_packet(&packet);
					break;
				} else {
					int * dstStride 
						= anim->pFrameRGB->linesize;
					uint8_t** dst = anim->pFrameRGB->data;
					int dstStride2[4]
						= { -dstStride[0], 0, 0, 0 };
					uint8_t* dst2[4]= {
						dst[0] 
						+ (anim->y - 1)*dstStride[0],
						0, 0, 0 };
					int i;
					unsigned char* r;
					
						
					sws_scale(anim->img_convert_ctx,
						  input->data,
						  input->linesize,
						  0,
						  anim->pCodecCtx->height,
						  dst2,
						  dstStride2);
					
					/* workaround: sws_scale 
					   sets alpha = 0... */
					
					r = (unsigned char*) ibuf->rect;
					
					for (i = 0; i < ibuf->x * ibuf->y;i++){
						r[3] = 0xff;
						r+=4;
					}
					
					av_free_packet(&packet);
					break;
				}
			}
		}

		av_free_packet(&packet);
	}

	if (filter_y && ibuf) {
		IMB_filtery(ibuf);
	}

	return(ibuf);
}

static void free_anim_ffmpeg(struct anim * anim) {
	if (anim == NULL) return;

	if (anim->pCodecCtx) {
		avcodec_close(anim->pCodecCtx);
		av_close_input_file(anim->pFormatCtx);
		av_free(anim->pFrameRGB);
		av_free(anim->pFrame);

		if (anim->ib_flags & IB_animdeinterlace) {
			MEM_freeN(anim->pFrameDeinterlaced->data[0]);
		}
		av_free(anim->pFrameDeinterlaced);
		sws_freeContext(anim->img_convert_ctx);
	}
	anim->duration = 0;
}

#endif

#ifdef WITH_REDCODE

static int startredcode(struct anim * anim) {
	anim->redcodeCtx = redcode_open(anim->name);
	if (!anim->redcodeCtx) {
		return -1;
	}
	anim->duration = redcode_get_length(anim->redcodeCtx);
	
	return 0;
}

static ImBuf * redcode_fetchibuf(struct anim * anim, int position) {
	struct ImBuf * ibuf;
	struct redcode_frame * frame;
	struct redcode_frame_raw * raw_frame;

	if (!anim->redcodeCtx) {
		return NULL;
	}

	frame = redcode_read_video_frame(anim->redcodeCtx, position);
	
	if (!frame) {
		return NULL;
	}

	raw_frame = redcode_decode_video_raw(frame, 1);

	redcode_free_frame(frame);

	if (!raw_frame) {
		return NULL;
	}
	
        ibuf = IMB_allocImBuf(raw_frame->width * 2, 
			      raw_frame->height * 2, 32, IB_rectfloat, 0);

	redcode_decode_video_float(raw_frame, ibuf->rect_float, 1);

	return ibuf;
}

static void free_anim_redcode(struct anim * anim) {
	if (anim->redcodeCtx) {
		redcode_close(anim->redcodeCtx);
		anim->redcodeCtx = 0;
	}
	anim->duration = 0;
}

#endif

/* probeer volgende plaatje te lezen */
/* Geen plaatje, probeer dan volgende animatie te openen */
/* gelukt, haal dan eerste plaatje van animatie */

static struct ImBuf * anim_getnew(struct anim * anim) {
	struct ImBuf *ibuf = 0;

	if (anim == NULL) return(0);

	free_anim_anim5(anim);
	free_anim_movie(anim);
	free_anim_avi(anim);
#ifdef WITH_QUICKTIME
	free_anim_quicktime(anim);
#endif
#ifdef WITH_FFMPEG
	free_anim_ffmpeg(anim);
#endif
#ifdef WITH_REDCODE
	free_anim_redcode(anim);
#endif


	if (anim->curtype != 0) return (0);
	anim->curtype = imb_get_anim_type(anim->name);	

	switch (anim->curtype) {
	case ANIM_ANIM5:
		if (startanim5(anim)) return (0);
		ibuf = anim5_fetchibuf(anim);
		break;
	case ANIM_SEQUENCE:
		ibuf = IMB_loadiffname(anim->name, anim->ib_flags);
		if (ibuf) {
			strcpy(anim->first, anim->name);
			anim->duration = 1;
		}
		break;
	case ANIM_MOVIE:
		if (startmovie(anim)) return (0);
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, 0, 0); /* fake */
		break;
	case ANIM_AVI:
		if (startavi(anim)) {
			printf("couldnt start avi\n"); 
			return (0);
		}
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, 0, 0);
		break;
#ifdef WITH_QUICKTIME
	case ANIM_QTIME:
		if (startquicktime(anim)) return (0);
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, 0, 0);
		break;
#endif
#ifdef WITH_FFMPEG
	case ANIM_FFMPEG:
		if (startffmpeg(anim)) return (0);
		ibuf = IMB_allocImBuf (anim->x, anim->y, 24, 0, 0);
		break;
#endif
#ifdef WITH_REDCODE
	case ANIM_REDCODE:
		if (startredcode(anim)) return (0);
		ibuf = IMB_allocImBuf (8, 8, 32, 0, 0);
		break;
#endif
	}
	return(ibuf);
}

struct ImBuf * IMB_anim_previewframe(struct anim * anim) {
	struct ImBuf * ibuf = 0;
	int position = 0;
	
	ibuf = IMB_anim_absolute(anim, 0);
	if (ibuf) {
		IMB_freeImBuf(ibuf);
		position = anim->duration / 2;
		ibuf = IMB_anim_absolute(anim, position);
	}
	return ibuf;
}

struct ImBuf * IMB_anim_absolute(struct anim * anim, int position) {
	struct ImBuf * ibuf = 0;
	char head[256], tail[256];
	unsigned short digits;
	int pic;
	int filter_y = (anim->ib_flags & IB_animdeinterlace);

	if (anim == NULL) return(0);

	if (anim->curtype == 0)	{
		ibuf = anim_getnew(anim);
		if (ibuf == NULL) {
			return (0);
		}
		IMB_freeImBuf(ibuf); /* ???? */
	}

	if (position < 0) return(0);
	if (position >= anim->duration) return(0);

	switch(anim->curtype) {
	case ANIM_ANIM5:
		if (anim->curposition > position) rewindanim5(anim);
		while (anim->curposition < position) {
			if (nextanim5(anim)) return (0);
		}
		ibuf = anim5_fetchibuf(anim);
		break;
	case ANIM_SEQUENCE:
		pic = an_stringdec(anim->first, head, tail, &digits);
		pic += position;
		an_stringenc(anim->name, head, tail, digits, pic);
		ibuf = IMB_loadiffname(anim->name, LI_rect);
		if (ibuf) {
			anim->curposition = position;
			/* patch... by freeing the cmap you prevent a double apply cmap... */
			/* probably the IB_CMAP option isn't working proper
			 * after the abgr->rgba reconstruction
			 */
			IMB_freecmapImBuf(ibuf);
		}
		break;
	case ANIM_MOVIE:
		ibuf = movie_fetchibuf(anim, position);
		if (ibuf) {
			anim->curposition = position;
			IMB_convert_rgba_to_abgr(ibuf);
		}
		break;
	case ANIM_AVI:
		ibuf = avi_fetchibuf(anim, position);
		if (ibuf) anim->curposition = position;
		break;
#ifdef WITH_QUICKTIME
	case ANIM_QTIME:
		ibuf = qtime_fetchibuf(anim, position);
		if (ibuf) anim->curposition = position;
		break;
#endif
#ifdef WITH_FFMPEG
	case ANIM_FFMPEG:
		ibuf = ffmpeg_fetchibuf(anim, position);
		if (ibuf) anim->curposition = position;
		filter_y = 0; /* done internally */
		break;
#endif
#ifdef WITH_REDCODE
	case ANIM_REDCODE:
		ibuf = redcode_fetchibuf(anim, position);
		if (ibuf) anim->curposition = position;
		break;
#endif
	}

	if (ibuf) {
		if (anim->ib_flags & IB_ttob) IMB_flipy(ibuf);
		if (filter_y) IMB_filtery(ibuf);
		sprintf(ibuf->name, "%s.%04d", anim->name, anim->curposition + 1);
		
	}
	return(ibuf);
}

struct ImBuf * IMB_anim_nextpic(struct anim * anim) {
	struct ImBuf * ibuf = 0;

	if (anim == 0) return(0);

	ibuf = IMB_anim_absolute(anim, anim->curposition + 1);

	return(ibuf);
}

/***/

int IMB_anim_get_duration(struct anim *anim) {
	return anim->duration;
}

void IMB_anim_set_preseek(struct anim * anim, int preseek)
{
	anim->preseek = preseek;
}

int IMB_anim_get_preseek(struct anim * anim)
{
	return anim->preseek;
}

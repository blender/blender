/**
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

//#ifdef __sgi
#if 0

#include <unistd.h>
#include <movie.h>
#include <cdaudio.h>
#include <dmedia/cl.h>
#include <dmedia/cl_cosmo.h>
#include <sys/file.h>			/* flock */
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_writemovie.h"
#include "BIF_toolbox.h"

#include "render.h"

#define error(str) {perror(str) ; error("%s", str); G.afbreek= 1;}
#define QUIT(str) {error(str); return;}

#define DIR_UP 1
#define DIR_DOWN 2
#define DIR_BOTH (DIR_UP | DIR_DOWN)

#define MAXQUAL R.r.quality
#define MINQUAL 30


/* globals */

static CL_Handle	compr, soft_compr;
static MVid		movie, image; 
static DMparams	*movie_params, *image_params;
static int			compr_params[64];
static int			myindex, qualindex, qualnow, mv_outx, mv_outy, numfields= 2;
static char		*comp_buf;
static int			sfra, efra, first = TRUE, maxbufsize;
static int			ntsc = FALSE;

#define FIRST_IMAGE "FIRST_IMAGE"
#define BLENDER_FIRST_IMAGE "BLENDER_1ST_IMG"


static void report_flock(void)
{
	static int flock_reported = FALSE;
	
	if (flock_reported) return;
	flock_reported = TRUE;
	
	error("WriteMovie: couldn't flock() moviefile. Ignoring.");
}


static void make_movie_name(char *string)
{
	int len;
	char txt[64];

	if (string==0) return;

	strcpy(string, G.scene->r.pic);
	BLI_convertstringcode(string, G.sce);
	len= strlen(string);

	BLI_make_existing_file(string);

	if (BLI_strcasecmp(string + len - 3, ".mv")) {
		sprintf(txt, "%04d_%04d.mv", sfra, efra);
		strcat(string, txt);
	}
}

static int my_Compress(uint * rect, int *bufsize)
{
	int err = 0;
	
	compr_params[qualindex] = qualnow;
	clSetParams(compr, compr_params, myindex);

	while (clCompress(compr, numfields, rect, bufsize, comp_buf) != numfields) {
		if (compr == soft_compr) {
			error("clCompress (software)");
			return 1;
		}
		
		/* hardware opnieuw initialiseren */
		clCloseCompressor(compr);
		clOpenCompressor(CL_JPEG_COSMO, &compr);

		qualnow--;
		compr_params[qualindex] = qualnow;
		clSetParams(compr, compr_params, myindex);
		printf("retrying at quality %d\n", qualnow);
		
		err= TRUE;
	}
	
	return (err);
}

static void set_sfra_efra(void)
{
	sfra = (G.scene->r.sfra);
	efra = (G.scene->r.efra);
}

static void open_compressor(void)
{
	int cosmo = FAILURE;
	
	/* initialiseren van de compressor */
	
	if (clOpenCompressor(CL_JPEG_SOFTWARE, &soft_compr) != SUCCESS) QUIT("clOpenCompressor");
	
	if (G.scene->r.mode & R_COSMO) {
		cosmo = clOpenCompressor(CL_JPEG_COSMO, &compr);
		if (cosmo != SUCCESS && first) error("warning: using software compression");
		first = FALSE;
	}
	
	if (cosmo != SUCCESS) compr = soft_compr;
	
	myindex = 0;

	compr_params[myindex++]= CL_IMAGE_WIDTH;
	compr_params[myindex++]= mv_outx;

	compr_params[myindex++]= CL_IMAGE_HEIGHT;
	compr_params[myindex++]= mv_outy / numfields;
	
	compr_params[myindex++]= CL_JPEG_QUALITY_FACTOR;
	qualindex = myindex;
	compr_params[myindex++]= R.r.quality;

	compr_params[myindex++]= CL_ORIGINAL_FORMAT;
	compr_params[myindex++]= CL_RGBX;

	compr_params[myindex++]= CL_ORIENTATION;
	compr_params[myindex++]= CL_TOP_DOWN;

	compr_params[myindex++]= CL_INTERNAL_FORMAT;
	compr_params[myindex++]= CL_YUV422;

	/* this parameter must be set for non-queueing mode */
	compr_params[myindex++]= CL_ENABLE_IMAGEINFO;
	compr_params[myindex++]= 1;

	/* enable stream headers */
	compr_params[myindex++]= CL_STREAM_HEADERS;
	compr_params[myindex++]= TRUE;

	clSetParams(compr, compr_params, myindex);
	if (compr != soft_compr) clSetParams(soft_compr, compr_params, myindex);
	
	maxbufsize = 2 * clGetParam(compr, CL_COMPRESSED_BUFFER_SIZE);
	comp_buf = MEM_mallocN(maxbufsize, "cosmo_buffer");
}

static void close_compressor(void)
{
	MEM_freeN(comp_buf);
	comp_buf = 0;

	clCloseCompressor(compr);
	if (soft_compr != compr) clCloseCompressor(soft_compr);
}

void end_movie(void)
{
}

static void new_movie(int fd)
{
	char	string[120];

	if (dmParamsCreate(&movie_params) != DM_SUCCESS) QUIT("dmParamsCreate");
	if (dmParamsCreate(&image_params) != DM_SUCCESS) QUIT("dmParamsCreate");
		
		if (mvSetMovieDefaults(movie_params, MV_FORMAT_SGI_3) != DM_SUCCESS) QUIT("mvSetMovieDefaults");
		if (dmSetImageDefaults(image_params, mv_outx, mv_outy, DM_PACKING_RGBX) != DM_SUCCESS) QUIT("dmSetImageDefaults");
			
		mvAddUserParam(BLENDER_FIRST_IMAGE);
		sprintf(string, "%04d", sfra);
		dmParamsSetString(image_params, BLENDER_FIRST_IMAGE, string);
	
		if (ntsc) dmParamsSetFloat(image_params, DM_IMAGE_RATE, 29.97);
		else dmParamsSetFloat(image_params, DM_IMAGE_RATE, 25.0);
		
		if (numfields == 2) {
			if (ntsc) dmParamsSetEnum(image_params, DM_IMAGE_INTERLACING, DM_IMAGE_INTERLACED_ODD);
			else dmParamsSetEnum(image_params, DM_IMAGE_INTERLACING, DM_IMAGE_INTERLACED_EVEN);
		} else dmParamsSetEnum(image_params, DM_IMAGE_INTERLACING, DM_IMAGE_NONINTERLACED);
	
		dmParamsSetEnum(image_params, DM_IMAGE_ORIENTATION, DM_TOP_TO_BOTTOM);
		dmParamsSetString(image_params, DM_IMAGE_COMPRESSION, DM_IMAGE_JPEG);
	
		if (mvCreateFD(fd, movie_params, NULL, &movie) != DM_SUCCESS) QUIT("mvCreateFile");
		if (mvAddTrack(movie, DM_IMAGE, image_params, NULL, &image)) QUIT("mvAddTrack");;
		if (mvSetLoopMode(movie, MV_LOOP_CONTINUOUSLY) != DM_SUCCESS) QUIT("mvSetMovieDefaults");
						
		if (mvWrite(movie) != DM_SUCCESS) QUIT("mvWrite");
		if (mvClose(movie) != DM_SUCCESS) QUIT("mvClose");
	
	dmParamsDestroy(image_params);
	dmParamsDestroy(movie_params);
}


void start_movie(void)
{
	char	name[FILE_MAXDIR+FILE_MAXFILE];
	char	bak[sizeof(name) + 4];
	int		fd;
	
	first = TRUE;
	
	set_sfra_efra();
	
	/* naam bedenken voor de movie */
	make_movie_name(name);
	
	ntsc = FALSE;
	
	switch (R.recty) {
		case 480: case 360: case 240: case 120:
			ntsc = TRUE;
	}
	
	if (ntsc) {
		switch (R.rectx) {
		case 360: case 320: case 720: case 640:
			mv_outx = R.rectx;
			break;
		default:
			if (R.rectx <= 320) mv_outx = 320;
			else if (R.rectx <= 640) mv_outx = 640;
			else mv_outx = 720;
		}
	} else {
		switch (R.rectx) {
		case 360: case 384: case 720: case 768:
			mv_outx = R.rectx;
			break;
		default:
			if (R.rectx < 384) mv_outx = 384;
			else mv_outx = 768;
		}
	}
	
	if (ntsc) {
		if (R.recty <= 240) {
			mv_outy = 240;
			numfields = 1;
		} else {
			mv_outy = 480;
			numfields = 2;
		}
	} else {
		if (R.recty <= 288) {
			mv_outy = 288;
			numfields = 1;
		} else {
			mv_outy = 576;
			numfields = 2;
		}
	}
		
	qualnow = R.r.quality;

	fd = open(name, O_BINARY|O_RDWR);
	if (fd != -1) {
		if (flock(fd, LOCK_EX) == -1) report_flock();
		
			if (mvOpenFD(fd, &movie) == DM_SUCCESS) {
				if (mvFindTrackByMedium(movie, DM_IMAGE, &image) == DM_SUCCESS) {
					if (mvGetImageWidth(image) == mv_outx) {
						if (mvGetImageHeight(image) == mv_outy) {
							mvClose(movie);
							close(fd);
							return;
						}
					}
				}
				strcpy(bak, name);
				strcat(bak, ".bak");
				BLI_rename(name, bak);
				mvClose(movie);
			}
		
		close(fd);
	}
	fd = open(name, O_BINARY|O_RDWR | O_CREAT | O_EXCL, 0664);
	if (fd != -1) {
		if (flock(fd, LOCK_EX) == -1) report_flock();
			new_movie(fd);
			printf("Created movie: %s\n", name);
		close(fd);
	}
}

void append_movie(int cfra)
{
	ImBuf		*ibuf, *tbuf;
	int			err, ofsx, ofsy, bufsize, rate, lastqual, qualstep, direction, first_image, num_images;
	char		name[FILE_MAXDIR+FILE_MAXFILE];
	const char	*string;
	int			fd;
	float col[4] = {0.0,0.0,0.0,0.0};
	
	set_sfra_efra();
	make_movie_name(name);
	open_compressor();
	
	rate = 1024 * R.r.maximsize;
	
	/* veranderd: kopie van rectot maken */
	ibuf= IMB_allocImBuf(R.rectx, R.recty, 32, IB_rect, 0);
	memcpy(ibuf->rect, R.rectot, 4*R.rectx*R.recty);
	
	if (ibuf->x != mv_outx || ibuf->y != mv_outy) {
		tbuf = IMB_allocImBuf(mv_outx, mv_outy, 32, IB_rect, 0);
		IMB_rectfill(tbuf,col);
		
		ofsx = (tbuf->x - ibuf->x) / 2;
		ofsy = (tbuf->y - ibuf->y) / 2;
		if (numfields == 2) ofsy &= ~1;
		
		IMB_rectcpy(tbuf, ibuf, ofsx, ofsy, 0, 0, ibuf->x, ibuf->y);
		IMB_freeImBuf(ibuf);
		strcpy(tbuf->name, ibuf->name);
		ibuf = tbuf;
	}
	IMB_convert_rgba_to_abgr(ibuf);
	
	if (numfields == 2) {
		if (ntsc) {
			IMB_rectcpy(ibuf, ibuf, 0, 0, 0, 1, ibuf->x, ibuf->y);
			IMB_flipy(ibuf);
			IMB_de_interlace(ibuf);
			if (ntsc) IMB_rectcpy(ibuf, ibuf, 0, 0, 0, 1, ibuf->x, ibuf->y);
		} else {
			IMB_flipy(ibuf);
			IMB_rectcpy(ibuf, ibuf, 0, 0, 0, 1, ibuf->x, ibuf->y);
			IMB_de_interlace(ibuf);
		}
	}
	else {
		/* kleine movies anders op de kop */
		IMB_flipy(ibuf);
	}
	
	if (rate == 0) {
		qualnow = R.r.quality;
		my_Compress(ibuf->rect, &bufsize);
	} else {
		qualstep = 4;
		direction = 0;
		
		do {
			if (qualnow > MAXQUAL) qualnow = MAXQUAL;
			if (qualnow < MINQUAL) qualnow = MINQUAL;

			compr_params[qualindex] = qualnow;
			clSetParams(compr, compr_params, myindex);

			lastqual = qualnow;
			err = my_Compress(ibuf->rect, &bufsize);
			
			printf(" tried quality: %d, size %d\n", qualnow, bufsize);
			
			if (bufsize < 0.9 * rate) {
				if (err) {
					/* forget about this frame, retry next frame at old quality settting */
					qualnow = lastqual;
					break;
				}
				if (qualnow == MAXQUAL) break;
				direction |= DIR_UP;
				if (direction == DIR_BOTH) qualstep /= 2;
				qualnow += qualstep;
			} else if (bufsize > 1.1 * rate) {
				if (qualnow == MINQUAL) break;
				direction |= DIR_DOWN;
				if (direction == DIR_BOTH) qualstep /= 2;
				qualnow -= qualstep;
			} else break;
									
			if (qualstep == 0) {
				/* this was the last iteration. Make sure that the buffer isn't to big */
				if (bufsize < 1.1 * rate) break;
				else qualnow--;
			}
		} while (1);
		
		printf("used quality: %d\n", qualnow);
		
		if (bufsize < rate) qualnow++;
		else qualnow--;
		
	}
	
	fd = open(name, O_BINARY|O_RDWR);

	if (fd != -1) {
		if (flock(fd, LOCK_EX) == -1) report_flock();
			if (mvOpenFD(fd, &movie) == DM_SUCCESS){
				if (mvFindTrackByMedium(movie, DM_IMAGE, &image) == DM_SUCCESS) {
					image_params = mvGetParams(image);
					
					first_image = 1;
					
					string = dmParamsGetString(image_params, FIRST_IMAGE);
					if (string) {
						first_image = atoi(string);
					}
					string = dmParamsGetString(image_params, BLENDER_FIRST_IMAGE);
					if (string) {
						first_image = atoi(string);
					}
					
					num_images = mvGetTrackLength(image);
					
					if (cfra >= first_image && cfra <= (first_image + num_images - 1)) {
						if (mvDeleteFrames(image, cfra - first_image, 1) != DM_SUCCESS) {
							mvDestroyMovie(movie);
							error("mvDeleteFrames");
							G.afbreek = 1;
						}
					}
					
					if (G.afbreek != 1) {
						if (mvInsertCompressedImage(image, cfra - first_image, bufsize, comp_buf) == DM_SUCCESS) {
							printf("added frame %3d (frame %3d in movie): length %6d: ", cfra, cfra - first_image + 1, bufsize);
							mvClose(movie);
						} else {
							mvDestroyMovie(movie);
							error("mvInsertCompressedImage");
							G.afbreek = 1;
						}
					}
				} else {
					mvDestroyMovie(movie);
					error("mvFindTrackByMedium");
					G.afbreek = 1;
				}
			}else {
				error("mvOpenFD");
				G.afbreek = 1;
			}
		close(fd);
	} else {
		error("open movie");
		G.afbreek = 1;
	}
	
	IMB_freeImBuf(ibuf);
	
	close_compressor();	
}

#endif	/* __sgi */

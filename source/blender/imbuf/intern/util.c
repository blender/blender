/**
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * util.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"

#include "DNA_userdef_types.h"
#include "BKE_global.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_targa.h"
#include "IMB_png.h"
#include "IMB_bmp.h"

#include "IMB_anim.h"

#ifdef WITH_QUICKTIME
#include "quicktime_import.h"
#endif
#ifdef WITH_FREEIMAGE
#include "IMB_freeimage.h"
#endif
#ifdef WITH_IMAGEMAGICK
#include "IMB_imagemagick.h"
#endif


#define UTIL_DEBUG 0

/* from misc_util: flip the bytes from x  */
#define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1])

/* this one is only def-ed once, strangely... */
#define GSS(x) (((uchar *)(x))[1] << 8 | ((uchar *)(x))[0])

int IMB_ispic_name(char *name)
{
	struct stat st;
	int fp, buf[10];
	int ofs = 0;

	if(UTIL_DEBUG) printf("IMB_ispic_name: loading %s\n", name);
	
	if (ib_stat(name,&st) == -1) return(0);
	if (((st.st_mode) & S_IFMT) == S_IFREG){
		if ((fp = open(name,O_BINARY|O_RDONLY)) >= 0){
			if (read(fp,buf,32)==32){
				close(fp);
				if (buf[ofs] == CAT) ofs += 3;
				if (buf[ofs] == FORM){
					if (buf[ofs + 2] == ILBM) return(AMI);
					if (buf[ofs + 2] == ANIM){
						if (buf[ofs + 3] == FORM){
							return(ANIM);
						}else{
							return(Anim);
						}
					}
				} else {
					if (GS(buf) == IMAGIC) return(IMAGIC);
					if (GSS(buf) == IMAGIC) return(IMAGIC);
					if ((BIG_LONG(buf[0]) & 0xfffffff0) == 0xffd8ffe0) return(JPG);

					/* at windows there are ".ffl" files with the same magic numnber... 
					   besides that,  tim images are not really important anymore! */
					/* if ((BIG_LONG(buf[0]) == 0x10000000) && ((BIG_LONG(buf[1]) & 0xf0ffffff) == 0)) return(TIM); */

				}
				if (imb_is_a_png(buf)) return(PNG);
				if (imb_is_a_targa(buf)) return(TGA);
/*
				if (imb_is_a_bmp(buf)) return(BMP);
*/

#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__)
				if(G.have_quicktime) {
					if (imb_is_a_quicktime(name)) return(QUICKTIME);
				}
#endif
#endif

#ifdef WITH_FREEIMAGE
				if (imb_is_a_freeimage(name)) return(FREEIMAGE);
#endif

#ifdef WITH_IMAGEMAGICK
				if (imb_is_imagick(name)) return(IMAGEMAGICK);
#endif

				return(FALSE);
			}
			close(fp);
		}
	}
	return(FALSE);
}



int IMB_ispic(char *filename)
{
	if(U.uiflag & USER_FILTERFILEEXTS) {
		if (G.have_quicktime){
			if(		BLI_testextensie(filename, ".jpg")
				||	BLI_testextensie(filename, ".jpeg")
				||	BLI_testextensie(filename, ".tga")
				||	BLI_testextensie(filename, ".rgb")
				||	BLI_testextensie(filename, ".bmp")
				||	BLI_testextensie(filename, ".png")
				||	BLI_testextensie(filename, ".iff")
				||	BLI_testextensie(filename, ".lbm")
				||	BLI_testextensie(filename, ".gif")
				||	BLI_testextensie(filename, ".psd")
				||	BLI_testextensie(filename, ".tif")
				||	BLI_testextensie(filename, ".tiff")
				||	BLI_testextensie(filename, ".pct")
				||	BLI_testextensie(filename, ".pict")
				||	BLI_testextensie(filename, ".pntg") //macpaint
				||	BLI_testextensie(filename, ".qtif")
#if defined(WITH_FREEIMAGE) || defined (WITH_IMAGEMAGICK) //nasty for now
				||	BLI_testextensie(filename, ".jng")
				||	BLI_testextensie(filename, ".mng")
				||	BLI_testextensie(filename, ".pbm")
				||	BLI_testextensie(filename, ".pgm")
				||	BLI_testextensie(filename, ".ppm")
				||	BLI_testextensie(filename, ".wbmp")
				||	BLI_testextensie(filename, ".cut")
				||	BLI_testextensie(filename, ".ico")
				||	BLI_testextensie(filename, ".koa")
				||	BLI_testextensie(filename, ".koala")
				||	BLI_testextensie(filename, ".pcd")
				||	BLI_testextensie(filename, ".pcx")
				||	BLI_testextensie(filename, ".ras")
#endif
				||	BLI_testextensie(filename, ".sgi")) {
				return IMB_ispic_name(filename);
			} else {
				return(FALSE);			
			}
		} else { // no quicktime
			if(		BLI_testextensie(filename, ".jpg")
				||	BLI_testextensie(filename, ".jpeg")
				||	BLI_testextensie(filename, ".tga")
				||	BLI_testextensie(filename, ".rgb")
				||	BLI_testextensie(filename, ".bmp")
				||	BLI_testextensie(filename, ".png")
				||	BLI_testextensie(filename, ".iff")
				||	BLI_testextensie(filename, ".lbm")
#if defined(WITH_FREEIMAGE) || defined (WITH_IMAGEMAGICK) //nasty for now
				||	BLI_testextensie(filename, ".jng")
				||	BLI_testextensie(filename, ".mng")
				||	BLI_testextensie(filename, ".pbm")
				||	BLI_testextensie(filename, ".pgm")
				||	BLI_testextensie(filename, ".ppm")
				||	BLI_testextensie(filename, ".wbmp")
				||	BLI_testextensie(filename, ".cut")
				||	BLI_testextensie(filename, ".ico")
				||	BLI_testextensie(filename, ".koa")
				||	BLI_testextensie(filename, ".koala")
				||	BLI_testextensie(filename, ".pcd")
				||	BLI_testextensie(filename, ".pcx")
				||	BLI_testextensie(filename, ".ras")
				||	BLI_testextensie(filename, ".gif")
				||	BLI_testextensie(filename, ".psd")
				||	BLI_testextensie(filename, ".tif")
				||	BLI_testextensie(filename, ".tiff")
#endif
			||	BLI_testextensie(filename, ".sgi")) {
				return IMB_ispic_name(filename);
			}
			else  {
				return(FALSE);
			}
		}
	} else { // no FILTERFILEEXTS
		return IMB_ispic_name(filename);
	}
}



static int isavi (char *name) {
    return AVI_is_avi (name);
}

#ifdef WITH_QUICKTIME
static int isqtime (char *name) {
	return anim_is_quicktime (name);
}
#endif

int imb_get_anim_type(char * name) {
	int type;
	struct stat st;

	if(UTIL_DEBUG) printf("in getanimtype: %s\n", name);

    if (ib_stat(name,&st) == -1) return(0);
    if (((st.st_mode) & S_IFMT) != S_IFREG) return(0);
	
	if (isavi(name)) return (ANIM_AVI);

	if (ismovie(name)) return (ANIM_MOVIE);
#ifdef WITH_QUICKTIME
	if (isqtime(name)) return (ANIM_QTIME);
#endif
	type = IMB_ispic(name);
	if (type == ANIM) return (ANIM_ANIM5);
	if (type) return(ANIM_SEQUENCE);
	return(0);
}
 
int IMB_isanim(char *filename) {
	int type;
	
	if(U.uiflag & USER_FILTERFILEEXTS) {
		if (G.have_quicktime){
			if(		BLI_testextensie(filename, ".avi")
				||	BLI_testextensie(filename, ".flc")
				||	BLI_testextensie(filename, ".mov")
				||	BLI_testextensie(filename, ".movie")
				||	BLI_testextensie(filename, ".mv")) {
				type = imb_get_anim_type(filename);
			} else {
				return(FALSE);			
			}
		} else { // no quicktime
			if(		BLI_testextensie(filename, ".avi")
				||	BLI_testextensie(filename, ".mv")) {
				type = imb_get_anim_type(filename);
			}
			else  {
				return(FALSE);
			}
		}
	} else { // no FILTERFILEEXTS
		type = imb_get_anim_type(filename);
	}
	
	return (type && type!=ANIM_SEQUENCE);
}

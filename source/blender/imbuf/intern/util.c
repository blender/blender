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

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_targa.h"
#include "IMB_png.h"

/* from misc_util: flip the bytes from x  */
#define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1])

/* this one is only def-ed once, strangely... */
#define GSS(x) (((uchar *)(x))[1] << 8 | ((uchar *)(x))[0])

int IMB_ispic(char *name)
{
	struct stat st;
	int fp, buf[10];
	int ofs = 0;
	
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
				return(FALSE);
			}
			close(fp);
		}
	}
	return(FALSE);
}

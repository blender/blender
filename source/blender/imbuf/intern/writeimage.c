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
 * writeimage.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"

#include "IMB_targa.h"
#include "IMB_jpeg.h"
#include "IMB_iris.h"
#include "IMB_ham.h"
#include "IMB_hamx.h"
#include "IMB_amiga.h"
#include "IMB_png.h"

#include "IMB_iff.h"
#include "IMB_bitplanes.h"
#include "IMB_divers.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


short IMB_saveiff(struct ImBuf *ibuf,char *naam,int flags)
{
	short ok=TRUE,delpl=FALSE;
	int file = -1;

	if (ibuf==0) return (FALSE);
	ibuf->flags = flags;

	/* Put formats that take a filename here */
	if (IS_jpg(ibuf)) {
		if(imb_savejpeg(ibuf, naam, flags)) return (0);
		else return (TRUE);
	}

	file = open(naam, O_BINARY | O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (file < 0) return (FALSE);

	if (flags & IB_rect){
		if (ibuf->cmap){
			imb_checkncols(ibuf);
		}
	}

	/* Put formats that take a filehandle here */
	if (IS_png(ibuf)) {
		ok = imb_savepng(ibuf,file,flags);
		if (ok) {
			close (file);
			return (ok);
		}
	}

	if (IS_tga(ibuf)) {
		ok = imb_savetarga(ibuf,file,flags);
		if (ok) {
			close (file);
			return (ok);
		}
	}
	
	if (IS_iris(ibuf)) {
		ok = imb_saveiris(ibuf,file,flags);
		if (ok) {
			close (file);
			return (ok);
		}
	}
	
	if (ok) ok = imb_start_iff(ibuf,file);

	if (IS_amiga(ibuf)){
		IMB_flipy(ibuf);
		if (flags & IB_rect){
			if ((flags & IB_cmap) == 0) {
				if (IS_ham(ibuf)){
					if (ok) ok = imb_converttoham(ibuf);
				}else if (ibuf->cmap){
					if (ok) ok = IMB_converttocmap(ibuf);
				}
			}
			if (ok){
				if (ibuf->planes==0){
					delpl=TRUE;
					ok=imb_addplanesImBuf(ibuf);
				}
				imb_longtobp(ibuf);
			}
		}

		if (flags & IB_vert){
			if (ok) ok = imb_encodebodyv(ibuf,file);
		}
		else{
			if (ok) ok = imb_encodebodyh(ibuf,file);
		}
		if (ok) ok = imb_update_iff(file,BODY);
	}else if (IS_anim(ibuf)) {
		if (ok) ok = imb_enc_anim(ibuf, file);
		if (ok) ok = imb_update_iff(file, BODY);
	}
	close(file);

	if (ok==FALSE) {
		fprintf(stderr,"Couldn't save picture.\n");
	}	
	if (delpl) imb_freeplanesImBuf(ibuf);

	return (ok);
}


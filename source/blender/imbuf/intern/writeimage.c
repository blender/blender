/**
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
 * writeimage.c
 *
 * $Id$
 */

#include <stdio.h>

#include "BKE_global.h"
#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"

#include "IMB_dpxcineon.h"
#include "IMB_targa.h"
#include "IMB_jpeg.h"
#include "IMB_iris.h"
#include "IMB_ham.h"
#include "IMB_hamx.h"
#include "IMB_amiga.h"
#include "IMB_png.h"
#include "IMB_bmp.h"
#include "IMB_tiff.h"
#include "IMB_radiance_hdr.h"
#ifdef WITH_OPENJPEG
#include "IMB_jp2.h"
#endif
#ifdef WITH_OPENEXR
#include "openexr/openexr_api.h"
#endif
#ifdef WITH_DDS
#include "dds/dds_api.h"
#endif

#include "IMB_iff.h"
#include "IMB_bitplanes.h"
#include "IMB_divers.h"

#ifdef WIN32
#include <io.h>
#include "BLI_winstuff.h"
#endif
/* added facility to copy with saving non-float rects */

short IMB_saveiff(struct ImBuf *ibuf, char *name, int flags)
{
	short ok=TRUE,delpl=FALSE;
	int file = -1;

	if (ibuf==0) return (FALSE);
	ibuf->flags = flags;

	/* Put formats that take a filename here */
	if (IS_jpg(ibuf)) {
		if(ibuf->rect==NULL && ibuf->rect_float)
			IMB_rect_from_float(ibuf);
		return imb_savejpeg(ibuf, name, flags);
	}
	if (IS_radhdr(ibuf)) {
		return imb_savehdr(ibuf, name, flags);
	}
	if (IS_png(ibuf)) {
		if(ibuf->rect==NULL && ibuf->rect_float)
			IMB_rect_from_float(ibuf);
		return imb_savepng(ibuf, name, flags);
	}
	if (IS_bmp(ibuf)) {
		if(ibuf->rect==NULL && ibuf->rect_float)
			IMB_rect_from_float(ibuf);
		return imb_savebmp(ibuf, name, flags);
	}
	if (IS_tga(ibuf)) {
		if(ibuf->rect==NULL && ibuf->rect_float)
			IMB_rect_from_float(ibuf);
		return imb_savetarga(ibuf, name, flags);
	}
	if (IS_iris(ibuf)) {
		if(ibuf->rect==NULL && ibuf->rect_float)
			IMB_rect_from_float(ibuf);
		return imb_saveiris(ibuf, name, flags);
	}
	if (G.have_libtiff && IS_tiff(ibuf)) {
		if(ibuf->rect==NULL && ibuf->rect_float)
			IMB_rect_from_float(ibuf);
		return imb_savetiff(ibuf, name, flags);
	}
#ifdef WITH_OPENEXR
	if (IS_openexr(ibuf)) {
		return imb_save_openexr(ibuf, name, flags);
	}
#endif
/* not supported yet
#ifdef WITH_DDS
	if (IS_dds(ibuf)) {
		return imb_save_dds(ibuf, name, flags);
	}
#endif
*/
	if (IS_cineon(ibuf)) {
		return imb_savecineon(ibuf, name, flags);
		
	}
	if (IS_dpx(ibuf)) {
		return imb_save_dpx(ibuf, name, flags);
	}
#ifdef WITH_OPENJPEG
	if (IS_jp2(ibuf)) {
		return imb_savejp2(ibuf, name, flags);
	}
#endif	
	file = open(name, O_BINARY | O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (file < 0) return (FALSE);

	if (flags & IB_rect){
		if (ibuf->cmap){
			imb_checkncols(ibuf);
		}
	}

	/* Put formats that take a filehandle here */
	ok = imb_start_iff(ibuf,file);
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


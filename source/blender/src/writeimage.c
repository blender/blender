/**
 * $Id$
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
 */

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"  // ImBuf{}
#include "DNA_scene_types.h"
#include "DNA_texture_types.h" // EnvMap{}
#include "DNA_image_types.h" // Image{}
#include "render.h"
#include "BKE_utildefines.h" // ELEM
#include "BIF_writeimage.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int BIF_write_ibuf(ImBuf *ibuf, char *name)
{
	int ok;

	/* to be used for e.g. envmap, not rendered images */
	
	if(R.r.imtype== R_IRIS) ibuf->ftype= IMAGIC;
	else if ((R.r.imtype==R_PNG)) {
		ibuf->ftype= PNG;
	}
	else if ((R.r.imtype==R_TARGA) || (R.r.imtype==R_PNG)) {
		// fall back to Targa if PNG writing is not supported
		ibuf->ftype= TGA;
	}
	else if(R.r.imtype==R_RAWTGA) {
		ibuf->ftype= RAWTGA;
	}
	else if(R.r.imtype==R_HAMX) {
		ibuf->ftype= AN_hamx;
	}
	else if ELEM(R.r.imtype, R_JPEG90, R_MOVIE) {
		if(R.r.quality < 10) R.r.quality= 90;

		ibuf->ftype= JPG|R.r.quality;
	}
	else ibuf->ftype= TGA;
	
	RE_make_existing_file(name);
	
	ok = IMB_saveiff(ibuf, name, IB_rect);
	if (ok == 0) {
		perror(name);
	}

	return(ok);
}


/* ------------------------------------------------------------------------- */

void BIF_save_envmap(EnvMap *env, char *str)
{
	ImBuf *ibuf;
/*  	extern rectcpy(); */
	int dx;
	
	/* all interactive stuff is handled in buttons.c */
	
	dx= env->cuberes;
	ibuf= IMB_allocImBuf(3*dx, 2*dx, 24, IB_rect, 0);
	
	IMB_rectop(ibuf, env->cube[0]->ibuf, 
			0, 0, 0, 0, dx, dx, IMB_rectcpy, 0);
	IMB_rectop(ibuf, env->cube[1]->ibuf, 
			dx, 0, 0, 0, dx, dx, IMB_rectcpy, 0);
	IMB_rectop(ibuf, env->cube[2]->ibuf, 
			2*dx, 0, 0, 0, dx, dx, IMB_rectcpy, 0);
	IMB_rectop(ibuf, env->cube[3]->ibuf, 
			0, dx, 0, 0, dx, dx, IMB_rectcpy, 0);
	IMB_rectop(ibuf, env->cube[4]->ibuf, 
			dx, dx, 0, 0, dx, dx, IMB_rectcpy, 0);
	IMB_rectop(ibuf, env->cube[5]->ibuf, 
			2*dx, dx, 0, 0, dx, dx, IMB_rectcpy, 0);
	
	BIF_write_ibuf(ibuf, str);
	IMB_freeImBuf(ibuf);
}

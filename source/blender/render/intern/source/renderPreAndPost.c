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
 * Prepare the scene data for rendering.
 * 
 * $Id$
 */

#include "MEM_guardedalloc.h"

#include "render.h"
#include "render_intern.h"
#include "renderPreAndPost.h"
#include "RE_callbacks.h"

#include "shadbuf.h"
#include "envmap.h"
#include "renderHelp.h"
#include "shadowBuffer.h"
#include "radio.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * Rotate all objects, make shadowbuffers and environment maps.
 */
void prepareScene()
{
	int a;

	
	if(R.rectot) MEM_freeN(R.rectot);
	R.rectot= 0;
	if(R.rectz) MEM_freeN(R.rectz);
	R.rectz= 0;

	RE_local_get_renderdata();

	/* SHADOW BUFFER */
	for(a=0; a<R.totlamp; a++) {
		if(RE_local_test_break()) break;
		/* Again, switch between old and new shadowing system. The
         * buffer objects were initially created in
         * blenderWorldManipulation.c */
		if (R.r.mode & R_UNIFIED) {
			if (R.la[a]->shadowBufOb) {
				RE_buildShadowBuffer(R.la[a]->shadowBufOb,
									 R.la[a]);
			}
		} else {
			if(R.la[a]->shb) makeshadowbuf(R.la[a]);
		}
	}

	/* RADIO */
	if(R.r.mode & R_RADIO) do_radio_render();

	/* ENVIRONMENT MAPS */
	make_envmaps();
}

void finalizeScene(void)
{
	/* Among other things, it releases the shadow buffers. */
	RE_local_free_renderdata();
}


void doClipping( void (*projectfunc)(float *, float *) )
{
	setzbufvlaggen(projectfunc);	
}

/* eof */

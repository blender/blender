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

#include <assert.h>
//#include <iostream.h>

#include "render.h"
#include "render_intern.h"
#include "shadbuf.h"
#include "shadowBuffer.h"         /* the C header            */
#include "RE_ShadowBuffer.h"      /* the base buffer         */
#include "RE_DummyShadowBuffer.h" /* A dummy shadow buffer   */
#include "RE_basicShadowBuffer.h" /* the 'old' shadow buffer */

struct ShadBuf;
struct LampRen;
struct Lamp;
/*
 * Creates a shadow buffer of a certain type
 */
RE_ShadowBufferHandle RE_createShadowBuffer(struct LampRen *lar,
											float mat[][4],
											int mode)
{
	/* make a dummy: this always returns a fixed value */
	RE_ShadowBuffer* buf = NULL;
	switch (mode) {
	case 0: 
		buf = new RE_DummyShadowBuffer();
		break;
	case 1:
		/* loop to the old c-based buffer */
		/* memory release is done implicitly! */
		initshadowbuf(lar, mat);
		break;
	case 2:	  
		buf = new RE_BasicShadowBuffer(lar, mat);
		break;
	case 3:
//		cout << "Deep shadow buffer requested\n";
		break;
	default:
//		cerr << "Bad shadow buffer type specified\n";
		; /* nada */
	}
	return (RE_ShadowBufferHandle) buf;
}

void RE_deleteShadowBuffer(RE_ShadowBufferHandle buf)
{
//  	cout << "requesting buffer delete\n";
	assert(buf);
	delete (RE_ShadowBuffer*) buf;
}

void RE_buildShadowBuffer(RE_ShadowBufferHandle buf,
						  struct LampRen *lar)
{
	assert(buf);
	((RE_ShadowBuffer*) buf)->importScene(lar);
}


void RE_testshadowbuf(RE_ShadowBufferHandle buf,
					  struct ShadBuf* shbp,
					  float inp,
					  float* shadres)
{
	assert(buf);
	((RE_ShadowBuffer*) buf)->readShadowValue(shbp, inp, shadres);
}

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

#ifndef RE_SHADOWBUFFER_H
#define RE_SHADOWBUFFER_H

#include <iostream.h>

struct ShadBuf;
struct LampRen;

class RE_ShadowBuffer {

 public:

	virtual ~RE_ShadowBuffer(void){};
	/**
	 * Place this scene in the buffer
	 */
	virtual void importScene(struct LampRen* lar) = 0;

	/**
	 * Test the shadow factor at a location in the buffer
	 * @param shadowResult a vector of 3 floats with rgb shadow values
	 */
	virtual void readShadowValue(struct ShadBuf *shb,
								 float inp,
								 float* shadowResult) = 0;
	
};

#endif /*  RE_SHADOWBUFFER_H */


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

#ifndef SHADOWBUFFER_H
#define SHADOWBUFFER_H

#ifdef __cplusplus
extern "C" { 
#endif

#ifndef RE_SHADOWBUFFERHANDLE
#define RE_SHADOWBUFFERHANDLE
#define RE_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name
RE_DECLARE_HANDLE(RE_ShadowBufferHandle);
#endif
	
	struct ShadBuf;
	struct LampRen;
	
/**
 * Calculates shadowbuffers for a vector of shadow-giving lamps
 * @param lar The vector of lamps
 * @returns a handle to the buffer
 */
	extern void RE_buildShadowBuffer(RE_ShadowBufferHandle dsbh,
									 struct LampRen *lar);
	
/**
 * Determines the shadow factor for a face and lamp. There is some
 * communication with global variables here? Should be made explicit...
 * @param shadres The RGB shadow factors: 1.0 for no shadow, 0.0 for complete
 *                shadow. There must be a float[3] to write the result to.
 * @param shb The shadowbuffer to find the shadow factor in.
 * @param inp The inproduct between viewvector and ?
 *
 */
	void RE_testshadowbuf(RE_ShadowBufferHandle dsbh,
						  struct ShadBuf* shbp,
						  float inp,
						  float* shadres);	

/**
 * Determines a shadow factor for halo-shadows.
 */
	
#ifdef __cplusplus
}
#endif

#endif /* SHADOWBUFFER_H */


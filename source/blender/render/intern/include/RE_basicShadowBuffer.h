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

#ifndef RE_BASICSHADOWBUFFER_H
#define RE_BASICSHADOWBUFFER_H

#include "RE_ShadowBuffer.h"

struct LampRen;
struct Lamp;

class RE_BasicShadowBuffer : public RE_ShadowBuffer {
	
 private:
	
	void lrectreadRectz(int x1, int y1, int x2, int y2, char *r1);
	int sizeoflampbuf(struct ShadBuf *shb);
	int firstreadshadbuf(struct ShadBuf *shb, int xs, int ys, int nr);
	float readshadowbuf(struct ShadBuf *shb, int xs, int ys, int zs);
	float readshadowbuf_halo(struct ShadBuf *shb, int xs, int ys, int zs);
	float *give_jitter_tab(int samp);
	
	int bias;
	
 public:
	/**
	 * Make a shadow buffer from these settings
	 */
	RE_BasicShadowBuffer(struct LampRen *lar, float mat[][4]);

	/**
	 * Delete and clear this buffer
	 */
	virtual ~RE_BasicShadowBuffer(void);
	
	/**
	 * Calculates shadowbuffers for a vector of shadow-giving lamps
	 * @param lar The vector of lamps
	 */
	void importScene(LampRen *lar);

	/**
	 * Determines the shadow factor for a face and lamp. There is some
	 * communication with global variables here.
	 * @param shadres The RGB shadow factors: 1.0 for no shadow, 0.0 for complete
	 *                shadow. There must be a float[3] to write the result to.
	 * @param shb The shadowbuffer to find the shadow factor in.
	 * @param inp The inproduct between viewvector and ?
	 *
	 */
	virtual void readShadowValue(struct ShadBuf *shb,
								 float inp,
								 float* shadowResult);

	/**
	 * Determines the shadow factor for lamp <lar>, between <p1>
	 * and <p2>. (Which CS?)
	 */
	float shadow_halo(LampRen *lar, float *p1, float *p2);

};

#endif /* RE_BASICSHADOWBUFFER_H */



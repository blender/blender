/**
 * $Id$
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

#include "IMG_Rect.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

TVisibility IMG_Rect::getVisibility(IMG_Rect& r) const
{
	bool lt = isInside(r.m_l, r.m_t);
	bool rt = isInside(r.m_r, r.m_t);
	bool lb = isInside(r.m_l, r.m_b);
	bool rb = isInside(r.m_r, r.m_b);
	TVisibility v;
	if (lt && rt && lb && rb) {
		// All points inside, rectangle is inside this
		v = kFullyVisible;		
	}
	else if (!(lt || rt || lb || rb)) {
		// None of the points inside
		// Check to see whether the rectangle is larger than this one
		if ((r.m_l < m_l) && (r.m_t < m_t) && (r.m_r > m_r) && (r.m_b > m_b)) {
			v = kPartiallyVisible;
		}
		else {
			v = kNotVisible;
		}
	}
	else {
		// Some of the points inside, rectangle is partially inside
		v = kPartiallyVisible;
	}
	return v;
}

TVisibility IMG_Rect::getVisibility(IMG_Line& l) const
{
	bool s = isInside(l.m_xs, l.m_ys);
	bool e = isInside(l.m_xe, l.m_ye);
	TVisibility v;
	if (s && e) {
		v = kFullyVisible;
	}
	else if (s || e) {
		v = kPartiallyVisible;
	}
	else {
		v = kNotVisible;
	}
	return v;
}

	
void IMG_Rect::setCenter(TInt32 cx, TInt32 cy)
{
	TInt32 offset = cx - (m_l + (m_r - m_l)/2);
	m_l += offset;
	m_r += offset;
	offset = cy - (m_t + (m_b - m_t)/2);
	m_t += offset;
	m_b += offset;
}

void IMG_Rect::setCenter(TInt32 cx, TInt32 cy, TInt32 w, TInt32 h)
{
	long w_2, h_2;
	
	w_2 = w >> 1;
	h_2 = h >> 1;
	m_l = cx - w_2;
	m_t = cy - h_2;
	m_r = m_l + w;
	m_b = m_t + h;
}

bool IMG_Rect::clip(IMG_Rect& r) const
{
	bool clipped = false;
	if (r.m_l < m_l) {
		r.m_l = m_l;
		clipped = true;
	}
	if (r.m_t < m_t) {
		r.m_t = m_t;
		clipped = true;
	}
	if (r.m_r > m_r) {
		r.m_r = m_r;
		clipped = true;
	}
	if (r.m_b > m_b) {
		r.m_b = m_b;
		clipped = true;
	}
	return clipped;
}

bool IMG_Rect::clip(IMG_Line& l) const
{
	bool clipped = false;
	return clipped;
}

/**
 * $Id$
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
 */
#ifndef __RAS_CAMERADATA_H
#define __RAS_CAMERADATA_H

struct RAS_CameraData
{
	float m_lens;
	float m_clipstart;
	float m_clipend;
	bool m_perspective;
	bool m_viewport;
	int m_viewportleft;
	int m_viewportbottom;
	int m_viewportright;
	int m_viewporttop;
	float m_focallength;

	RAS_CameraData(float lens = 35., float clipstart = 0.1, float clipend = 100., bool perspective = true,
	float focallength = 0.0f, bool viewport = false, int viewportleft = 0, int viewportbottom = 0, 
	int viewportright = 0, int viewporttop = 0) :
		m_lens(lens),
		m_clipstart(clipstart),
		m_clipend(clipend),
		m_perspective(perspective),
		m_viewport(viewport),
		m_viewportleft(viewportleft),
		m_viewportbottom(viewportbottom),
		m_viewportright(viewportright),
		m_viewporttop(viewporttop),
		m_focallength(focallength)
	{
	}
};

#endif //__RAS_CAMERADATA_H


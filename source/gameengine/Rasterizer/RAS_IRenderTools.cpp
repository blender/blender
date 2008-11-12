/**
 * $Id$
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

#include "RAS_IRenderTools.h"

void RAS_IRenderTools::SetClientObject(RAS_IRasterizer* rasty, void *obj)
{
	if (m_clientobject != obj)
		m_clientobject = obj;
}

void RAS_IRenderTools::SetAuxilaryClientInfo(void* inf)
{
	m_auxilaryClientInfo = inf;
}

void RAS_IRenderTools::AddLight(struct RAS_LightObject* lightobject)
{
	m_lights.push_back(lightobject);
}

void RAS_IRenderTools::RemoveLight(struct RAS_LightObject* lightobject)
{
	std::vector<struct	RAS_LightObject*>::iterator lit = 
		std::find(m_lights.begin(),m_lights.end(),lightobject);

	if (!(lit==m_lights.end()))
		m_lights.erase(lit);
}


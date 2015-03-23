/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can [0]istribute it and/or
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file gameengine/Converter/BlenderWorldInfo.cpp
 *  \ingroup bgeconv
 */


#include <stdio.h>  // printf()

#include "BlenderWorldInfo.h"
#include "KX_PythonInit.h"
#include "GPU_material.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_world_types.h"

#include "BLI_math.h"

#include "BKE_global.h"
#include "BKE_scene.h"
/* end of blender include block */


BlenderWorldInfo::BlenderWorldInfo(Scene *blenderscene, World *blenderworld)
{
	if (blenderworld) {
		m_do_color_management = BKE_scene_check_color_management_enabled(blenderscene);
		m_hasworld = true;
		m_hasmist = ((blenderworld->mode) & WO_MIST ? true : false);
		m_misttype = blenderworld->mistype;
		m_miststart = blenderworld->miststa;
		m_mistdistance = blenderworld->mistdist;
		m_mistintensity = blenderworld->misi;
		setMistColor(blenderworld->horr, blenderworld->horg, blenderworld->horb);
		setBackColor(blenderworld->horr, blenderworld->horg, blenderworld->horb);
		setAmbientColor(blenderworld->ambr, blenderworld->ambg, blenderworld->ambb);
	}
	else {
		m_hasworld = false;
	}
}

BlenderWorldInfo::~BlenderWorldInfo()
{
}

bool BlenderWorldInfo::hasWorld()
{
	return m_hasworld;
}

bool BlenderWorldInfo::hasMist()
{
	return m_hasmist;
}

float BlenderWorldInfo::getBackColorRed()
{
	return m_backgroundcolor[0];
}

float BlenderWorldInfo::getBackColorGreen()
{
	return m_backgroundcolor[1];
}

float BlenderWorldInfo::getBackColorBlue()
{
	return m_backgroundcolor[2];
}

float BlenderWorldInfo::getAmbientColorRed()
{
	return m_ambientcolor[0];
}

float BlenderWorldInfo::getAmbientColorGreen()
{
	return m_ambientcolor[1];
}

float BlenderWorldInfo::getAmbientColorBlue()
{
	return m_ambientcolor[2];
}

short BlenderWorldInfo::getMistType()
{
	return m_misttype;
}

float BlenderWorldInfo::getMistStart()
{
	return m_miststart;
}

float BlenderWorldInfo::getMistDistance()
{
	return m_mistdistance;
}

float BlenderWorldInfo::getMistIntensity()
{
	return m_mistintensity;
}

float BlenderWorldInfo::getMistColorRed()
{
	return m_mistcolor[0];
}

float BlenderWorldInfo::getMistColorGreen()
{
	return m_mistcolor[1];
}

float BlenderWorldInfo::getMistColorBlue()
{
	return m_mistcolor[2];
}

void BlenderWorldInfo::setBackColor(float r, float g, float b)
{
	m_backgroundcolor[0] = r;
	m_backgroundcolor[1] = g;
	m_backgroundcolor[2] = b;

	if (m_do_color_management) {
		linearrgb_to_srgb_v3_v3(m_con_backgroundcolor, m_backgroundcolor);
	}
	else {
		copy_v3_v3(m_con_backgroundcolor, m_backgroundcolor);
	}
}

void BlenderWorldInfo::setMistType(short type)
{
	m_misttype = type;
}

void BlenderWorldInfo::setUseMist(bool enable)
{
	m_hasmist = enable;
}

void BlenderWorldInfo::setMistStart(float d)
{
	m_miststart = d;
}

void BlenderWorldInfo::setMistDistance(float d)
{
	m_mistdistance = d;
}

void BlenderWorldInfo::setMistIntensity(float intensity)
{
	m_mistintensity = intensity;
}
void BlenderWorldInfo::setMistColor(float r, float g, float b)
{
	m_mistcolor[0] = r;
	m_mistcolor[1] = g;
	m_mistcolor[2] = b;

	if (m_do_color_management) {
		linearrgb_to_srgb_v3_v3(m_con_mistcolor, m_mistcolor);
	}
	else {
		copy_v3_v3(m_con_mistcolor, m_mistcolor);
	}
}

void BlenderWorldInfo::setAmbientColor(float r, float g, float b)
{
	m_ambientcolor[0] = r;
	m_ambientcolor[1] = g;
	m_ambientcolor[2] = b;

	if (m_do_color_management) {
		linearrgb_to_srgb_v3_v3(m_con_ambientcolor, m_ambientcolor);
	}
	else {
		copy_v3_v3(m_con_ambientcolor, m_ambientcolor);
	}
}

void BlenderWorldInfo::UpdateBackGround()
{
	if (m_hasworld) {
		RAS_IRasterizer *m_rasterizer = KX_GetActiveEngine()->GetRasterizer();

		if (m_rasterizer->GetDrawingMode() >= RAS_IRasterizer::KX_SOLID) {
			m_rasterizer->SetBackColor(m_con_backgroundcolor);
			GPU_horizon_update_color(m_backgroundcolor);
		}
	}
}

void BlenderWorldInfo::UpdateWorldSettings()
{
	if (m_hasworld) {
		RAS_IRasterizer *m_rasterizer = KX_GetActiveEngine()->GetRasterizer();

		if (m_rasterizer->GetDrawingMode() >= RAS_IRasterizer::KX_SOLID) {
			m_rasterizer->SetAmbientColor(m_con_ambientcolor);
			GPU_ambient_update_color(m_ambientcolor);

			if (m_hasmist) {
				m_rasterizer->SetFog(m_misttype, m_miststart, m_mistdistance, m_mistintensity, m_con_mistcolor);
				GPU_mist_update_values(m_misttype, m_miststart, m_mistdistance, m_mistintensity, m_mistcolor);
				m_rasterizer->EnableFog(true);
				GPU_mist_update_enable(true);
			}
			else {
				m_rasterizer->EnableFog(false);
				GPU_mist_update_enable(false);
			}
		}
	}
}

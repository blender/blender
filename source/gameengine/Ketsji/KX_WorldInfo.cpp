/*
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

/** \file gameengine/Ketsji/KX_WorldInfo.cpp
 *  \ingroup ketsji
 */


#include "KX_WorldInfo.h"
#include "KX_PythonInit.h"
#include "GPU_material.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BLI_math.h"

#include "BKE_global.h"
#include "BKE_scene.h"
/* end of blender include block */


KX_WorldInfo::KX_WorldInfo(Scene *blenderscene, World *blenderworld)
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

KX_WorldInfo::~KX_WorldInfo()
{
}

bool KX_WorldInfo::hasWorld()
{
	return m_hasworld;
}

bool KX_WorldInfo::hasMist()
{
	return m_hasmist;
}

float KX_WorldInfo::getBackColorRed()
{
	return m_backgroundcolor[0];
}

float KX_WorldInfo::getBackColorGreen()
{
	return m_backgroundcolor[1];
}

float KX_WorldInfo::getBackColorBlue()
{
	return m_backgroundcolor[2];
}

float KX_WorldInfo::getAmbientColorRed()
{
	return m_ambientcolor[0];
}

float KX_WorldInfo::getAmbientColorGreen()
{
	return m_ambientcolor[1];
}

float KX_WorldInfo::getAmbientColorBlue()
{
	return m_ambientcolor[2];
}

short KX_WorldInfo::getMistType()
{
	return m_misttype;
}

float KX_WorldInfo::getMistStart()
{
	return m_miststart;
}

float KX_WorldInfo::getMistDistance()
{
	return m_mistdistance;
}

float KX_WorldInfo::getMistIntensity()
{
	return m_mistintensity;
}

float KX_WorldInfo::getMistColorRed()
{
	return m_mistcolor[0];
}

float KX_WorldInfo::getMistColorGreen()
{
	return m_mistcolor[1];
}

float KX_WorldInfo::getMistColorBlue()
{
	return m_mistcolor[2];
}

void KX_WorldInfo::setBackColor(float r, float g, float b)
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

void KX_WorldInfo::setMistType(short type)
{
	m_misttype = type;
}

void KX_WorldInfo::setUseMist(bool enable)
{
	m_hasmist = enable;
}

void KX_WorldInfo::setMistStart(float d)
{
	m_miststart = d;
}

void KX_WorldInfo::setMistDistance(float d)
{
	m_mistdistance = d;
}

void KX_WorldInfo::setMistIntensity(float intensity)
{
	m_mistintensity = intensity;
}
void KX_WorldInfo::setMistColor(float r, float g, float b)
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

void KX_WorldInfo::setAmbientColor(float r, float g, float b)
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

void KX_WorldInfo::UpdateBackGround()
{
	if (m_hasworld) {
		RAS_IRasterizer *m_rasterizer = KX_GetActiveEngine()->GetRasterizer();

		if (m_rasterizer->GetDrawingMode() >= RAS_IRasterizer::KX_SOLID) {
			m_rasterizer->SetBackColor(m_con_backgroundcolor);
			GPU_horizon_update_color(m_backgroundcolor);
		}
	}
}

void KX_WorldInfo::UpdateWorldSettings()
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

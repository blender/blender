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

/** \file BlenderWorldInfo.h
 *  \ingroup bgeconv
 */

#ifndef __BLENDERWORLDINFO_H__
#define __BLENDERWORLDINFO_H__
#include "KX_WorldInfo.h"
#include "KX_KetsjiEngine.h"
#include "RAS_IRasterizer.h"

struct Scene;
struct World;
const class KX_KetsjiEngine;
const class RAS_IRasterizer;

class BlenderWorldInfo : public KX_WorldInfo
{
	bool m_hasworld;
	bool m_hasmist;
	short m_misttype;
	float m_miststart;
	float m_mistdistance;
	float m_mistintensity;
	float m_mistcolor[3];
	float m_backgroundcolor[3];
	float m_ambientcolor[3];
	float m_con_mistcolor[3];
	float m_con_backgroundcolor[3];
	float m_con_ambientcolor[3];

public:
	BlenderWorldInfo(Scene *blenderscene, World *blenderworld);
	~BlenderWorldInfo();

	bool m_do_color_management;
	bool hasWorld();
	bool hasMist();
	short getMistType();
	float getMistStart();
	float getMistDistance();
	float getMistIntensity();
	float getMistColorRed();
	float getMistColorGreen();
	float getMistColorBlue();
	float getBackColorRed();
	float getBackColorGreen();
	float getBackColorBlue();
	float getAmbientColorRed();
	float getAmbientColorGreen();
	float getAmbientColorBlue();
	void setBackColor(float r, float g, float b);
	void setUseMist(bool enable);
	void setMistType(short type);
	void setMistStart(float d);
	void setMistDistance(float d);
	void setMistIntensity(float intensity);
	void setMistColor(float r, float g, float b);
	void setAmbientColor(float r, float g, float b);
	void UpdateBackGround();
	void UpdateWorldSettings();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BlenderWorldInfo")
#endif
};

#endif  /* __BLENDERWORLDINFO_H__ */

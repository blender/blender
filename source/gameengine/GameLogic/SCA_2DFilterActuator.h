/**
 * SCA_2DFilterActuator.h
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __SCA_2DFILETRACTUATOR_H__
#define __SCA_2DFILETRACTUATOR_H__

#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"
#include "SCA_IActuator.h"

class SCA_2DFilterActuator : public SCA_IActuator
{
    Py_Header;

private:
	vector<STR_String> m_propNames;
	void * m_gameObj;
	RAS_2DFilterManager::RAS_2DFILTER_MODE m_type;
	short m_disableMotionBlur;
	float m_float_arg;
	int   m_int_arg;
	STR_String	m_shaderText;
	RAS_IRasterizer* m_rasterizer;
	RAS_IRenderTools* m_rendertools;

public:

    SCA_2DFilterActuator(
        class SCA_IObject* gameobj,
        RAS_2DFilterManager::RAS_2DFILTER_MODE type,
		short flag,
		float float_arg,
		int int_arg,
		RAS_IRasterizer* rasterizer,
		RAS_IRenderTools* rendertools);

	void	SetShaderText(const char *text);
    virtual ~SCA_2DFilterActuator();
    virtual bool Update();

    virtual CValue* GetReplica();
};
#endif

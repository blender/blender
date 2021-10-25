/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_CurveBaseOperation.h"

#ifdef __cplusplus
extern "C" {
#endif
#  include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

CurveBaseOperation::CurveBaseOperation() : NodeOperation()
{
	this->m_curveMapping = NULL;
}

CurveBaseOperation::~CurveBaseOperation()
{
	if (this->m_curveMapping) {
		curvemapping_free(this->m_curveMapping);
		this->m_curveMapping = NULL;
	}
}

void CurveBaseOperation::initExecution()
{
	curvemapping_initialize(this->m_curveMapping);
}
void CurveBaseOperation::deinitExecution()
{
	if (this->m_curveMapping) {
		curvemapping_free(this->m_curveMapping);
		this->m_curveMapping = NULL;
	}
}

void CurveBaseOperation::setCurveMapping(CurveMapping *mapping)
{
	/* duplicate the curve to avoid glitches while drawing, see bug [#32374] */
	if (this->m_curveMapping) {
		curvemapping_free(this->m_curveMapping);
	}
	this->m_curveMapping = curvemapping_copy(mapping);
}

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

#ifndef _COM_CurveBaseOperation_h
#define _COM_CurveBaseOperation_h
#include "COM_NodeOperation.h"
#include "DNA_color_types.h"

class CurveBaseOperation : public NodeOperation {
protected:
	/**
	 * Cached reference to the inputProgram
	 */
	CurveMapping *m_curveMapping;
public:
	CurveBaseOperation();
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	void deinitExecution();
	
	void setCurveMapping(CurveMapping *mapping);
};
#endif

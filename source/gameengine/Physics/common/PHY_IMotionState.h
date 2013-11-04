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

/** \file PHY_IMotionState.h
 *  \ingroup phys
 */

#ifndef __PHY_IMOTIONSTATE_H__
#define __PHY_IMOTIONSTATE_H__

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

/**
 * PHY_IMotionState is the Interface to explicitly synchronize the world transformation.
 * Default implementations for mayor graphics libraries like OpenGL and DirectX can be provided.
 */
class	PHY_IMotionState

{
	public:
		virtual ~PHY_IMotionState(){};

		virtual void	GetWorldPosition(float& posX,float& posY,float& posZ)=0;
		virtual void	GetWorldScaling(float& scaleX,float& scaleY,float& scaleZ)=0;
		virtual void	GetWorldOrientation(float& quatIma0,float& quatIma1,float& quatIma2,float& quatReal)=0;
		// ori = array 12 floats, [0..3] = first column + 0, [4..7] = second column, [8..11] = third column
		virtual void	GetWorldOrientation(float* ori)=0;
		virtual	void	SetWorldOrientation(const float* ori)=0;
		
		virtual void	SetWorldPosition(float posX,float posY,float posZ)=0;
		virtual	void	SetWorldOrientation(float quatIma0,float quatIma1,float quatIma2,float quatReal)=0;


		virtual	void	CalculateWorldTransformations()=0;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:PHY_IMotionState")
#endif
};

#endif  /* __PHY_IMOTIONSTATE_H__ */

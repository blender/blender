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
#ifndef PHY__MOTIONSTATE_H
#define PHY__MOTIONSTATE_H

/**
	PHY_IMotionState is the Interface to explicitly synchronize the world transformation.
	Default implementations for mayor graphics libraries like OpenGL and DirectX can be provided.
*/
class	PHY_IMotionState

{
	public:
		
		virtual ~PHY_IMotionState();

		virtual void	getWorldPosition(float& posX,float& posY,float& posZ)=0;
		virtual void	getWorldScaling(float& scaleX,float& scaleY,float& scaleZ)=0;
		virtual void	getWorldOrientation(float& quatIma0,float& quatIma1,float& quatIma2,float& quatReal)=0;
		
		virtual void	setWorldPosition(float posX,float posY,float posZ)=0;
		virtual	void	setWorldOrientation(float quatIma0,float quatIma1,float quatIma2,float quatReal)=0;

		virtual	void	calculateWorldTransformations()=0;
};

#endif //PHY__MOTIONSTATE_H


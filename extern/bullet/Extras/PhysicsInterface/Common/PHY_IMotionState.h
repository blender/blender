/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
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


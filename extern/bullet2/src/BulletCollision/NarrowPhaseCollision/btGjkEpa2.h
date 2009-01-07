/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2008 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software in a
product, an acknowledgment in the product documentation would be appreciated
but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

/*
GJK-EPA collision solver by Nathanael Presson, 2008
*/
#ifndef _68DA1F85_90B7_4bb0_A705_83B4040A75C6_
#define _68DA1F85_90B7_4bb0_A705_83B4040A75C6_
#include "BulletCollision/CollisionShapes/btConvexShape.h"

///btGjkEpaSolver contributed under zlib by Nathanael Presson
struct	btGjkEpaSolver2
{
struct	sResults
	{
	enum eStatus
		{
		Separated,		/* Shapes doesnt penetrate												*/ 
		Penetrating,	/* Shapes are penetrating												*/ 
		GJK_Failed,		/* GJK phase fail, no big issue, shapes are probably just 'touching'	*/ 
		EPA_Failed		/* EPA phase fail, bigger problem, need to save parameters, and debug	*/ 
		}		status;
	btVector3	witnesses[2];
	btVector3	normal;
	btScalar	distance;
	};

static int		StackSizeRequirement();

static bool		Distance(	const btConvexShape* shape0,const btTransform& wtrs0,
							const btConvexShape* shape1,const btTransform& wtrs1,
							const btVector3& guess,
							sResults& results);

static bool		Penetration(const btConvexShape* shape0,const btTransform& wtrs0,
							const btConvexShape* shape1,const btTransform& wtrs1,
							const btVector3& guess,
							sResults& results,
							bool usemargins=true);

static btScalar	SignedDistance(	const btVector3& position,
								btScalar margin,
								const btConvexShape* shape,
								const btTransform& wtrs,
								sResults& results);
							
static bool		SignedDistance(	const btConvexShape* shape0,const btTransform& wtrs0,
								const btConvexShape* shape1,const btTransform& wtrs1,
								const btVector3& guess,
								sResults& results);
};

#endif

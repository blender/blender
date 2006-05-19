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

#ifndef MANIFOLD_CONTACT_POINT_H
#define MANIFOLD_CONTACT_POINT_H

#include "SimdVector3.h"
#include "SimdTransformUtil.h"





/// ManifoldContactPoint collects and maintains persistent contactpoints.
/// used to improve stability and performance of rigidbody dynamics response.
class ManifoldPoint
	{
		public:
			ManifoldPoint()
				:m_userPersistentData(0)
			{
			}

			ManifoldPoint( const SimdVector3 &pointA, const SimdVector3 &pointB, 
					const SimdVector3 &normal, 
					SimdScalar distance ) :
					m_localPointA( pointA ), 
					m_localPointB( pointB ), 
					m_normalWorldOnB( normal ), 
					m_distance1( distance ),
					m_userPersistentData(0),					
					m_lifeTime(0)
			{
				
					
			}

			

			SimdVector3 m_localPointA;			
			SimdVector3 m_localPointB;			
			SimdVector3	m_positionWorldOnB;
			///m_positionWorldOnA is redundant information, see GetPositionWorldOnA(), but for clarity
			SimdVector3	m_positionWorldOnA;
			SimdVector3 m_normalWorldOnB;
		
			float	m_distance1;

				
			void*	m_userPersistentData;

			int		m_lifeTime;//lifetime of the contactpoint in frames
			
			float GetDistance() const
			{
				return m_distance1;
			}
			int	GetLifeTime() const
			{
				return m_lifeTime;
			}

			SimdVector3 GetPositionWorldOnA() {
				return m_positionWorldOnA;
//				return m_positionWorldOnB + m_normalWorldOnB * m_distance1;
			}

			const SimdVector3& GetPositionWorldOnB()
			{
				return m_positionWorldOnB;
			}

			void	SetDistance(float dist)
			{
				m_distance1 = dist;
			}
			
			

	};

#endif //MANIFOLD_CONTACT_POINT_H

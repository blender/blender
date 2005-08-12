#ifndef MANIFOLD_CONTACT_POINT_H
#define MANIFOLD_CONTACT_POINT_H

#include "SimdVector3.h"

/// ManifoldContactPoint collects and maintains persistent contactpoints.
/// used to improve stability and performance of rigidbody dynamics response.
class ManifoldPoint
	{
		public:
			ManifoldPoint()
			{
			}

			ManifoldPoint( const SimdVector3 &pointA, const SimdVector3 &pointB, 
					const SimdVector3 &normal, 
					SimdScalar distance ) :
					m_localPointA( pointA ), 
					m_localPointB( pointB ), 
					m_normalWorldOnB( normal ), 
					m_distance1( distance )
					,m_appliedImpulse(0.f),
					m_lifeTime(0)
			{}

			SimdVector3 m_localPointA;			
			SimdVector3 m_localPointB;			
			SimdVector3	m_positionWorldOnB;
			///m_positionWorldOnA is redundant information, see GetPositionWorldOnA(), but for clarity
			SimdVector3	m_positionWorldOnA;
			SimdVector3 m_normalWorldOnB;
			float	m_distance1;
			/// total applied impulse during most recent frame
			float	m_appliedImpulse;
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

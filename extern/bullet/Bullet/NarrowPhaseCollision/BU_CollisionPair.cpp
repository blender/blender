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



#include "BU_CollisionPair.h"
#include "NarrowPhaseCollision/BU_VertexPoly.h"
#include "NarrowPhaseCollision/BU_EdgeEdge.h"
#include "BU_Collidable.h"


#include "BU_MotionStateInterface.h"
#include "CollisionShapes/PolyhedralConvexShape.h"
#include <SimdMinMax.h>
#include "SimdTransformUtil.h"



BU_CollisionPair::BU_CollisionPair(const PolyhedralConvexShape* convexA,const PolyhedralConvexShape* convexB,SimdScalar tolerance)
: m_convexA(convexA),m_convexB(convexB),m_screwing(SimdVector3(0,0,0),SimdVector3(0,0,0)),
m_tolerance(tolerance)
{

}

// if there exists a time-of-impact between any feature_pair (edgeA,edgeB),
// (vertexA,faceB) or (vertexB,faceA) in [0..1], report true and smallest time


/*
bool BU_CollisionPair::GetTimeOfImpact(const SimdVector3& linearMotionA,const SimdQuaternion& angularMotionA,const SimdVector3& linearMotionB,const SimdQuaternion& angularMotionB, SimdScalar& toi,SimdTransform& impactTransA,SimdTransform& impactTransB)

*/

bool BU_CollisionPair::calcTimeOfImpact(
					const SimdTransform& fromA,
					const SimdTransform& toA,
					const SimdTransform& fromB,
					const SimdTransform& toB,
					CastResult& result)
{



	
	SimdVector3 linvelA,angvelA;
	SimdVector3 linvelB,angvelB;

	SimdTransformUtil::CalculateVelocity(fromA,toA,1.f,linvelA,angvelA);
	SimdTransformUtil::CalculateVelocity(fromB,toB,1.f,linvelB,angvelB);


	SimdVector3 linearMotionA = toA.getOrigin() - fromA.getOrigin();
	SimdQuaternion angularMotionA(0,0,0,1.f);
	SimdVector3 linearMotionB = toB.getOrigin() - fromB.getOrigin();
	SimdQuaternion angularMotionB(0,0,0,1);
	


	result.m_fraction = 1.f;

	SimdTransform impactTransA;
	SimdTransform impactTransB;

	int index=0;

	SimdScalar toiUnscaled=result.m_fraction;
	const SimdScalar toiUnscaledLimit = result.m_fraction;

	SimdTransform a2w;
	a2w = fromA;
	SimdTransform b2w = fromB;

/* debugging code
	{
		const int numvertsB = m_convexB->GetNumVertices();
		for (int v=0;v<numvertsB;v++)
		{
			SimdPoint3 pt;
			m_convexB->GetVertex(v,pt);
			pt = b2w * pt;
			char buf[1000];

			if (pt.y() < 0.)
			{
				sprintf(buf,"PRE ERROR (%d) %.20E %.20E %.20E!!!!!!!!!\n",v,pt.x(),pt.y(),pt.z());
				if (debugFile)
					fwrite(buf,1,strlen(buf),debugFile);
			} else
			{
				sprintf(buf,"PRE %d = %.20E,%.20E,%.20E\n",v,pt.x(),pt.y(),pt.z());
				if (debugFile)
					fwrite(buf,1,strlen(buf),debugFile);

			}
		}
	}
*/


	SimdTransform b2wp = b2w;
	
	b2wp.setOrigin(b2w.getOrigin() + linearMotionB);
	b2wp.setRotation( b2w.getRotation() + angularMotionB);

	impactTransB = b2wp;
	
	SimdTransform a2wp;
	a2wp.setOrigin(a2w.getOrigin()+ linearMotionA);
	a2wp.setRotation(a2w.getRotation()+angularMotionA);

	impactTransA = a2wp;

	SimdTransform a2winv;
	a2winv = a2w.inverse();

	SimdTransform b2wpinv;
	b2wpinv = b2wp.inverse();

	SimdTransform b2winv;
	b2winv = b2w.inverse();

	SimdTransform a2wpinv;
	a2wpinv = a2wp.inverse();

		//Redon's version with concatenated transforms

	SimdTransform relative;

	relative = b2w * b2wpinv * a2wp * a2winv;

	//relative = a2winv * a2wp  * b2wpinv * b2w;

	SimdQuaternion qrel;
	relative.getBasis().getRotation(qrel);

	SimdVector3 linvel = relative.getOrigin();

	if (linvel.length() < SCREWEPSILON)
	{
		linvel.setValue(0.,0.,0.);
	}
	SimdVector3 angvel;
	angvel[0] = 2.f * SimdAsin (qrel[0]);
	angvel[1] = 2.f * SimdAsin (qrel[1]);
	angvel[2] = 2.f * SimdAsin (qrel[2]);
	
	if (angvel.length() < SCREWEPSILON)
	{
		angvel.setValue(0.f,0.f,0.f);
	}

	//Redon's version with concatenated transforms
	m_screwing = BU_Screwing(linvel,angvel);
	
	SimdTransform w2s;
	m_screwing.LocalMatrix(w2s);

	SimdTransform s2w;
	s2w = w2s.inverse();

	//impactTransA = a2w;
	//impactTransB = b2w;

	bool hit = false;
	
	if (SimdFuzzyZero(m_screwing.GetS()) && SimdFuzzyZero(m_screwing.GetW()))
	{
		//W = 0 , S = 0 , no collision
		//toi = 0;
	/*	
		{
			const int numvertsB = m_convexB->GetNumVertices();
			for (int v=0;v<numvertsB;v++)
			{
				SimdPoint3 pt;
				m_convexB->GetVertex(v,pt);
				pt = impactTransB * pt;
				char buf[1000];
				
				if (pt.y() < 0.)
				{
					sprintf(buf,"EARLY POST ERROR (%d) %.20E,%.20E,%.20E!!!!!!!!!\n",v,pt.x(),pt.y(),pt.z());
					if (debugFile)
						fwrite(buf,1,strlen(buf),debugFile);
				}
				else
				{
					sprintf(buf,"EARLY POST %d = %.20E,%.20E,%.20E\n",v,pt.x(),pt.y(),pt.z());
					if (debugFile)
						fwrite(buf,1,strlen(buf),debugFile);
				}
			}
		}
	*/	
		
		return false;//don't continue moving within epsilon
	}

#define EDGEEDGE
#ifdef EDGEEDGE

	BU_EdgeEdge edgeEdge;

	//for all edged in A check agains all edges in B
	for (int ea = 0;ea < m_convexA->GetNumEdges();ea++)
	{
		SimdPoint3 pA0,pA1;

		m_convexA->GetEdge(ea,pA0,pA1);

		pA0= a2w * pA0;//in world space
		pA0 = w2s * pA0;//in screwing space

		pA1= a2w * pA1;//in world space
		pA1 = w2s * pA1;//in screwing space

		int numedgesB = m_convexB->GetNumEdges();
		for (int eb = 0; eb < numedgesB;eb++)
		{
			{
				SimdPoint3 pB0,pB1;
				m_convexB->GetEdge(eb,pB0,pB1);

				pB0= b2w * pB0;//in world space
				pB0 = w2s * pB0;//in screwing space

				pB1= b2w * pB1;//in world space
				pB1 = w2s * pB1;//in screwing space


				SimdScalar lambda,mu;
				
				toiUnscaled = 1.;

				SimdVector3 edgeDirA(pA1-pA0);
				SimdVector3 edgeDirB(pB1-pB0);

				if (edgeEdge.GetTimeOfImpact(m_screwing,pA0,edgeDirA,pB0,edgeDirB,toiUnscaled,lambda,mu))
				{
					//printf("edgeedge potential hit\n");
					if (toiUnscaled>=0)
					{
						if (toiUnscaled < toiUnscaledLimit)							
						{
		
							//inside check is already done by checking the mu and gamma !

							SimdPoint3 vtx  = pA0+lambda * (pA1-pA0);
							SimdPoint3 hitpt = m_screwing.InBetweenPosition(vtx,toiUnscaled);
							
							SimdPoint3 hitptWorld =   s2w * hitpt;
							{

								if (toiUnscaled < result.m_fraction)
									result.m_fraction = toiUnscaled;

								hit = true;

								SimdVector3 hitNormal = edgeDirB.cross(edgeDirA);
								
								hitNormal = m_screwing.InBetweenVector(hitNormal,toiUnscaled);
							

								hitNormal.normalize();
								
								//an approximated normal can be calculated by taking the cross product of both edges
								//take care of the sign !
								
								SimdVector3 hitNormalWorld = s2w.getBasis() * hitNormal ;
						
								SimdScalar dist = m_screwing.GetU().dot(hitNormalWorld);
								if (dist > 0)
									hitNormalWorld *= -1;
								
								//todo: this is the wrong point, because b2winv is still at begin of motion
								// not at time-of-impact location!
								//bhitpt = b2winv * hitptWorld;

//								m_manifold.SetContactPoint(BUM_FeatureEdgeEdge,index,ea,eb,hitptWorld,hitNormalWorld);
							}
					
						}
					}
				}
			}

			index++;
		}
	};
#endif //EDGEEDGE

#define VERTEXFACE
#ifdef VERTEXFACE

	// for all vertices in A, for each face in B,do vertex-face
	{
		const int numvertsA = m_convexA->GetNumVertices();
		for (int v=0;v<numvertsA;v++)
		//int v=3;

		{
			SimdPoint3 vtx;
			m_convexA->GetVertex(v,vtx);

			vtx = a2w * vtx;//in world space
			vtx = w2s * vtx;//in screwing space

			const int numplanesB = m_convexB->GetNumPlanes();

			for (int p = 0 ; p < numplanesB; p++)
			//int p=2;
			{

				{
				
					SimdVector3 planeNorm;
					SimdPoint3 planeSupport;

					m_convexB->GetPlane(planeNorm,planeSupport,p);


					planeSupport = b2w * planeSupport;//transform to world space
					SimdVector3 planeNormWorld =  b2w.getBasis() * planeNorm;
				
					planeSupport =  w2s * planeSupport  ; //transform to screwing space
					planeNorm =  w2s.getBasis() * planeNormWorld;

					planeNorm.normalize();

					SimdScalar d = planeSupport.dot(planeNorm);
					
					SimdVector4 planeEq(planeNorm[0],planeNorm[1],planeNorm[2],d);
				
					BU_VertexPoly vtxApolyB;

					toiUnscaled = 1.;

					if ((p==2) && (v==6))
					{
//						printf("%f toiUnscaled\n",toiUnscaled);

					}
					if (vtxApolyB.GetTimeOfImpact(m_screwing,vtx,planeEq,toiUnscaled,false))
					{
					


						
						if (toiUnscaled >= 0. )
						{
							//not only collect the first point, get every contactpoint, later we have to check the
							//manifold properly!

							if (toiUnscaled <= toiUnscaledLimit)
							{
	//							printf("toiUnscaled %f\n",toiUnscaled );

								SimdPoint3 hitpt = m_screwing.InBetweenPosition(vtx,toiUnscaled);
								SimdVector3 hitNormal = m_screwing.InBetweenVector(planeNorm ,toiUnscaled);

								SimdVector3 hitNormalWorld = s2w.getBasis() * hitNormal ;
								SimdPoint3 hitptWorld = s2w * hitpt;


								hitpt = b2winv * hitptWorld;
								//vertex has to be 'within' the facet's boundary
								if (m_convexB->IsInside(hitpt,m_tolerance))
								{
//									m_manifold.SetContactPoint(BUM_FeatureVertexFace, index,v,p,hitptWorld,hitNormalWorld);
									
									if (toiUnscaled < result.m_fraction)
										result.m_fraction= toiUnscaled;
									hit = true;

								}
							}
						}
					}
					
				}

				index++;
			}
		}
	}

	//
	// for all vertices in B, for each face in A,do vertex-face
	//copy and pasted from all verts A -> all planes B so potential typos!
	//todo: make this into one method with a kind of 'swapped' logic
	//
	{
		const int numvertsB = m_convexB->GetNumVertices();
		for (int v=0;v<numvertsB;v++)
		//int v=0;

		{
			SimdPoint3 vtx;
			m_convexB->GetVertex(v,vtx);

			vtx = b2w * vtx;//in world space
/*
			
			char buf[1000];

			if (vtx.y() < 0.)
			{
				sprintf(buf,"ERROR !!!!!!!!!\n",v,vtx.x(),vtx.y(),vtx.z());
				if (debugFile)
					fwrite(buf,1,strlen(buf),debugFile);
			}
			sprintf(buf,"vertexWorld(%d) = (%.20E,%.20E,%.20E)\n",v,vtx.x(),vtx.y(),vtx.z());
			if (debugFile)
				fwrite(buf,1,strlen(buf),debugFile);

*/			
			vtx = w2s * vtx;//in screwing space

			const int numplanesA = m_convexA->GetNumPlanes();

			for (int p = 0 ; p < numplanesA; p++)
			//int p=2;
			{

				{
					SimdVector3 planeNorm;
					SimdPoint3 planeSupport;

					m_convexA->GetPlane(planeNorm,planeSupport,p);


					planeSupport = a2w * planeSupport;//transform to world space
					SimdVector3 planeNormWorld =  a2w.getBasis() * planeNorm;
				
					planeSupport =  w2s * planeSupport  ; //transform to screwing space
					planeNorm =  w2s.getBasis() * planeNormWorld;

					planeNorm.normalize();

					SimdScalar d = planeSupport.dot(planeNorm);
					
					SimdVector4 planeEq(planeNorm[0],planeNorm[1],planeNorm[2],d);
				
					BU_VertexPoly vtxBpolyA;

					toiUnscaled = 1.;

					if (vtxBpolyA.GetTimeOfImpact(m_screwing,vtx,planeEq,toiUnscaled,true))
					{
						if (toiUnscaled>=0.)
						{
							if (toiUnscaled < toiUnscaledLimit)
							{
								SimdPoint3 hitpt = m_screwing.InBetweenPosition( vtx , -toiUnscaled);
								SimdVector3 hitNormal = m_screwing.InBetweenVector(-planeNorm ,-toiUnscaled);
								//SimdScalar len =  hitNormal.length()-1;

								//assert( SimdFuzzyZero(len) );

								
								SimdVector3 hitNormalWorld = s2w.getBasis() * hitNormal ;
								SimdPoint3 hitptWorld = s2w * hitpt;
								hitpt = a2winv * hitptWorld;
							
							
								//vertex has to be 'within' the facet's boundary
								if (m_convexA->IsInside(hitpt,m_tolerance))
								{
									
//									m_manifold.SetContactPoint(BUM_FeatureFaceVertex,index,p,v,hitptWorld,hitNormalWorld);
									if (toiUnscaled <result.m_fraction)
										result.m_fraction = toiUnscaled;
									hit = true;
								}
							}
						
						}
					
					}
					}

			}
		
			index++;
		}
	}
	

#endif// VERTEXFACE

	//the manifold now consists of all points/normals generated by feature-pairs that have a time-of-impact within this frame
	//in addition there are contact points from previous frames
	//we have to cleanup the manifold, using an additional epsilon/tolerance
	//as long as the distance from the contactpoint (in worldspace) to both objects is within this epsilon we keep the point
	//else throw it away
	

	if (hit)
	{

		//try to avoid numerical drift on close contact
		
		if (result.m_fraction < 0.00001)
		{
//			printf("toiUnscaledMin< 0.00001\n");
			impactTransA = a2w;
			impactTransB = b2w;

		} else
		{

			//SimdScalar vel = linearMotionB.length();
			
			//todo: check this margin
			result.m_fraction *= 0.99f;

			//move B to new position
			impactTransB.setOrigin(b2w.getOrigin()+ result.m_fraction*linearMotionB);
			SimdQuaternion ornB = b2w.getRotation()+angularMotionB*result.m_fraction;
			ornB.normalize();
			impactTransB.setRotation(ornB);

			//now transform A
			SimdTransform a2s,a2b;
			a2s.mult( w2s , a2w);
			a2s= m_screwing.InBetweenTransform(a2s,result.m_fraction);
			a2s.multInverseLeft(w2s,a2s);
			a2b.multInverseLeft(b2w, a2s);

			//transform by motion B
			impactTransA.mult(impactTransB, a2b);
			//normalize rotation
			SimdQuaternion orn;
			impactTransA.getBasis().getRotation(orn);
			orn.normalize();
			impactTransA.setBasis(SimdMatrix3x3(orn));
		}
	}

/*
	{
		const int numvertsB = m_convexB->GetNumVertices();
		for (int v=0;v<numvertsB;v++)
		{
			SimdPoint3 pt;
			m_convexB->GetVertex(v,pt);
			pt = impactTransB * pt;
			char buf[1000];

			if (pt.y() < 0.)
			{
				sprintf(buf,"POST ERROR (%d) %.20E,%.20E,%.20E!!!!!!!!!\n",v,pt.x(),pt.y(),pt.z());
				if (debugFile)
					fwrite(buf,1,strlen(buf),debugFile);
			}
			else
			{
				sprintf(buf,"POST %d = %.20E,%.20E,%.20E\n",v,pt.x(),pt.y(),pt.z());
				if (debugFile)
					fwrite(buf,1,strlen(buf),debugFile);
			}
		}
	}
*/
	return hit;
}



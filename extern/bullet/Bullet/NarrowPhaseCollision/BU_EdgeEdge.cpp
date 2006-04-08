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


#include "BU_EdgeEdge.h"
#include "BU_Screwing.h"
#include <SimdPoint3.h>
#include <SimdPoint3.h>

//#include "BU_IntervalArithmeticPolynomialSolver.h"
#include "BU_AlgebraicPolynomialSolver.h"

#define USE_ALGEBRAIC
#ifdef USE_ALGEBRAIC	
#define BU_Polynomial BU_AlgebraicPolynomialSolver
#else	
#define BU_Polynomial BU_IntervalArithmeticPolynomialSolver
#endif

BU_EdgeEdge::BU_EdgeEdge()
{
}


bool BU_EdgeEdge::GetTimeOfImpact(
								  const BU_Screwing& screwAB,
								  const SimdPoint3& a,//edge in object A
								  const SimdVector3& u,
								  const SimdPoint3& c,//edge in object B
								  const SimdVector3& v,
								  SimdScalar &minTime,
								  SimdScalar &lambda1,
								  SimdScalar& mu1
								  
								  )
{
	bool hit=false;
	
	SimdScalar lambda;
	SimdScalar mu;
	
	const SimdScalar w=screwAB.GetW();
	const SimdScalar s=screwAB.GetS();
	
	if (SimdFuzzyZero(s) &&
		SimdFuzzyZero(w))
	{
		//no motion, no collision
		return false;
	}
	
	if (SimdFuzzyZero(w) )
	{
		//pure translation W=0, S <> 0
		//no trig, f(t)=t
		SimdScalar det = u.y()*v.x()-u.x()*v.y();
		if (!SimdFuzzyZero(det))
		{		
			lambda = (a.x()*v.y() - c.x() * v.y() - v.x() * a.y() + v.x() * c.y()) / det;
			mu = (u.y() * a.x() - u.y() * c.x() - u.x() * a.y() + u.x() * c.y()) / det;

			if (mu >=0 && mu <= 1 && lambda >= 0 && lambda <= 1)
			{
				// single potential collision is
				SimdScalar t = (c.z()-a.z()+mu*v.z()-lambda*u.z())/s;
				//if this is on the edge, and time t within [0..1] report hit
				if (t>=0 && t <= minTime)
				{
					hit = true;
					lambda1 = lambda;
					mu1 = mu;
					minTime=t;
				}
			}
			
		} else
		{
			//parallel case, not yet
		}
	} else
	{
		if (SimdFuzzyZero(s) )
		{
			if (SimdFuzzyZero(u.z()) )
			{
				if (SimdFuzzyZero(v.z()) )
				{
					//u.z()=0,v.z()=0
					if (SimdFuzzyZero(a.z()-c.z()))
					{
						//printf("NOT YET planar problem, 4 vertex=edge cases\n");
						
					} else
					{
						//printf("parallel but distinct planes, no collision\n");
						return false;
					}
					
				} else
				{
					SimdScalar mu = (a.z() - c.z())/v.z();
					if (0<=mu && mu <= 1)
					{
					//	printf("NOT YET//u.z()=0,v.z()<>0\n");
					} else
					{
						return false;
					}
					
				}
			} else
			{
				//u.z()<>0
				
				if (SimdFuzzyZero(v.z()) )
				{
					//printf("u.z()<>0,v.z()=0\n");
					lambda =  (c.z() - a.z())/u.z();
					if (0<=lambda && lambda <= 1)
					{
						//printf("u.z()<>0,v.z()=0\n");
						SimdPoint3 rotPt(a.x()+lambda * u.x(), a.y()+lambda * u.y(),0.f);
						SimdScalar r2 = rotPt.length2();//px*px + py*py;
						
						//either y=a*x+b, or x = a*x+b...
						//depends on whether value v.x() is zero or not
						SimdScalar aa;
						SimdScalar bb;
						
						if (SimdFuzzyZero(v.x()))
						{
							aa = v.x()/v.y();
							bb= c.x()+  (-c.y() /v.y()) *v.x();
						} else
						{
							//line is c+mu*v;
							//x = c.x()+mu*v.x();
							//mu = ((x-c.x())/v.x());
							//y = c.y()+((x-c.x())/v.x())*v.y();
							//y = c.y()+  (-c.x() /v.x()) *v.y() + (x /v.x())   *v.y();
							//y = a*x+b,where a = v.y()/v.x(), b= c.y()+  (-c.x() /v.x()) *v.y();
							aa = v.y()/v.x();
							bb= c.y()+  (-c.x() /v.x()) *v.y();
						}
						
						SimdScalar disc = aa*aa*r2 + r2 - bb*bb;
						if (disc <0)
						{
							//edge doesn't intersect the circle (motion of the vertex)
							return false;
						}
						SimdScalar rad = SimdSqrt(r2);
						
						if (SimdFuzzyZero(disc))
						{
							SimdPoint3 intersectPt;
							
							SimdScalar mu;
							//intersectionPoint edge with circle;
							if (SimdFuzzyZero(v.x()))
							{
								intersectPt.setY( (-2*aa*bb)/(2*(aa*aa+1)));
								intersectPt.setX( aa*intersectPt.y()+bb );
								mu = ((intersectPt.y()-c.y())/v.y());
							} else
							{
								intersectPt.setX((-2*aa*bb)/(2*(aa*aa+1)));
								intersectPt.setY(aa*intersectPt.x()+bb);
								mu = ((intersectPt.getX()-c.getX())/v.getX());
								
							}
							
							if (0 <= mu && mu <= 1)
							{
								hit = Calc2DRotationPointPoint(rotPt,rad,screwAB.GetW(),intersectPt,minTime);
							}
							//only one solution
						} else
						{
							//two points...
							//intersectionPoint edge with circle;
							SimdPoint3 intersectPt;
							//intersectionPoint edge with circle;
							if (SimdFuzzyZero(v.x()))
							{
								SimdScalar mu;
								
								intersectPt.setY((-2.f*aa*bb+2.f*SimdSqrt(disc))/(2.f*(aa*aa+1.f)));
								intersectPt.setX(aa*intersectPt.y()+bb);
								mu = ((intersectPt.getY()-c.getY())/v.getY());
								if (0.f <= mu && mu <= 1.f)
								{
									hit = Calc2DRotationPointPoint(rotPt,rad,screwAB.GetW(),intersectPt,minTime);
								}
								intersectPt.setY((-2.f*aa*bb-2.f*SimdSqrt(disc))/(2.f*(aa*aa+1.f)));
								intersectPt.setX(aa*intersectPt.y()+bb);
								mu = ((intersectPt.getY()-c.getY())/v.getY());
								if (0 <= mu && mu <= 1)
								{
									hit = hit || Calc2DRotationPointPoint(rotPt,rad,screwAB.GetW(),intersectPt,minTime);
								}
								
							} else
							{
								SimdScalar mu;
								
								intersectPt.setX((-2.f*aa*bb+2.f*SimdSqrt(disc))/(2*(aa*aa+1.f)));
								intersectPt.setY(aa*intersectPt.x()+bb);
								mu = ((intersectPt.getX()-c.getX())/v.getX());
								if (0 <= mu && mu <= 1)
								{
									hit = Calc2DRotationPointPoint(rotPt,rad,screwAB.GetW(),intersectPt,minTime);
								}
								intersectPt.setX((-2.f*aa*bb-2.f*SimdSqrt(disc))/(2.f*(aa*aa+1.f)));
								intersectPt.setY(aa*intersectPt.x()+bb);
								mu = ((intersectPt.getX()-c.getX())/v.getX());
								if (0.f <= mu && mu <= 1.f)
								{
									hit = hit || Calc2DRotationPointPoint(rotPt,rad,screwAB.GetW(),intersectPt,minTime);
								}
							}
						}
						
						
						
						//int k=0;
						
					} else
					{
						return false;
					}
					
					
				} else
				{
					//u.z()<>0,v.z()<>0
					//printf("general case with s=0\n");
					hit = GetTimeOfImpactGeneralCase(screwAB,a,u,c,v,minTime,lambda,mu);
					if (hit)
					{
						lambda1 = lambda;
						mu1 = mu;
						
					}
				}
			}
			
		} else
		{
			//printf("general case, W<>0,S<>0\n");
			hit = GetTimeOfImpactGeneralCase(screwAB,a,u,c,v,minTime,lambda,mu);
			if (hit)
			{
				lambda1 = lambda;
				mu1 = mu;
			}
			
		}
		
		
		//W <> 0,pure rotation
	}
	
	return hit;
}


bool BU_EdgeEdge::GetTimeOfImpactGeneralCase(
											 const BU_Screwing& screwAB,
											 const SimdPoint3& a,//edge in object A
											 const SimdVector3& u,
											 const SimdPoint3& c,//edge in object B
											 const SimdVector3& v,
											 SimdScalar &minTime,
											 SimdScalar &lambda,
											 SimdScalar& mu
											 
											 )
{
	bool hit = false;
	
	SimdScalar coefs[4]={0.f,0.f,0.f,0.f};
	BU_Polynomial polynomialSolver;
	int numroots = 0;
	
	//SimdScalar eps=1e-15f;
	//SimdScalar eps2=1e-20f;
	SimdScalar s=screwAB.GetS();
	SimdScalar w = screwAB.GetW();
	
	SimdScalar ax = a.x();
	SimdScalar ay = a.y();
	SimdScalar az = a.z();
	SimdScalar cx = c.x();
	SimdScalar cy = c.y();
	SimdScalar cz = c.z();
	SimdScalar vx = v.x();
	SimdScalar vy = v.y();
	SimdScalar vz = v.z();
	SimdScalar ux = u.x();
	SimdScalar uy = u.y();
	SimdScalar uz = u.z();
	
	
	if (!SimdFuzzyZero(v.z()))
	{
		
		//Maple Autogenerated C code
		SimdScalar t1,t2,t3,t4,t7,t8,t10;
		SimdScalar t13,t14,t15,t16,t17,t18,t19,t20;
		SimdScalar t21,t22,t23,t24,t25,t26,t27,t28,t29,t30;
		SimdScalar t31,t32,t33,t34,t35,t36,t39,t40;
		SimdScalar t41,t43,t48;
		SimdScalar t63;
		
		SimdScalar aa,bb,cc,dd;//the coefficients
		
		t1 = v.y()*s;      t2 = t1*u.x();
		t3 = v.x()*s;
		t4 = t3*u.y();
		t7 = SimdTan(w/2.0f);
		t8 = 1.0f/t7;
		t10 = 1.0f/v.z();
		aa = (t2-t4)*t8*t10;
		t13 = a.x()*t7;
		t14 = u.z()*v.y();
		t15 = t13*t14;
		t16 = u.x()*v.z();
		t17 = a.y()*t7;
		t18 = t16*t17;
		t19 = u.y()*v.z();
		t20 = t13*t19;
		t21 = v.y()*u.x();
		t22 = c.z()*t7;
		t23 = t21*t22;
		t24 = v.x()*a.z();
		t25 = t7*u.y();
		t26 = t24*t25;
		t27 = c.y()*t7;
		t28 = t16*t27;
		t29 = a.z()*t7;
		t30 = t21*t29;
		t31 = u.z()*v.x();
		t32 = t31*t27;
		t33 = t31*t17;
		t34 = c.x()*t7;
		t35 = t34*t19;
		t36 = t34*t14;
		t39 = v.x()*c.z();
		t40 = t39*t25;
		t41 = 2.0f*t1*u.y()-t15+t18-t20-t23-t26+t28+t30+t32+t33-t35-t36+2.0f*t3*u.x()+t40;
		bb = t41*t8*t10;
		t43 = t7*u.x();
		t48 = u.y()*v.y();
		cc = (-2.0f*t39*t43+2.0f*t24*t43+t4-2.0f*t48*t22+2.0f*t34*t16-2.0f*t31*t13-t2
			-2.0f*t17*t14+2.0f*t19*t27+2.0f*t48*t29)*t8*t10;
		t63 = -t36+t26+t32-t40+t23+t35-t20+t18-t28-t33+t15-t30;
		dd = t63*t8*t10;
		
		coefs[0]=aa;
		coefs[1]=bb;
		coefs[2]=cc;
		coefs[3]=dd;
		
	} else
	{
		
		SimdScalar t1,t2,t3,t4,t7,t8,t10;
		SimdScalar t13,t14,t15,t16,t17,t18,t19,t20;
		SimdScalar t21,t22,t23,t24,t25,t26,t27,t28,t29,t30;
		SimdScalar t31,t32,t33,t34,t35,t36,t37,t38,t57;
		SimdScalar p1,p2,p3,p4;

	  t1 = uy*s;
      t2 = t1*vx;
      t3 = ux*s;
      t4 = t3*vy;
      t7 = SimdTan(w/2.0f);
      t8 = 1/t7;
      t10 = 1/uz;
      t13 = ux*az;
      t14 = t7*vy;
      t15 = t13*t14;
      t16 = ax*t7;
      t17 = uy*vz;
      t18 = t16*t17;
      t19 = cx*t7;
      t20 = t19*t17;
      t21 = vy*uz;
      t22 = t19*t21;
      t23 = ay*t7;
      t24 = vx*uz;
      t25 = t23*t24;
      t26 = uy*cz;
      t27 = t7*vx;
      t28 = t26*t27;
      t29 = t16*t21;
      t30 = cy*t7;
      t31 = ux*vz;
      t32 = t30*t31;
      t33 = ux*cz;
      t34 = t33*t14;
      t35 = t23*t31;
      t36 = t30*t24;
      t37 = uy*az;
      t38 = t37*t27;

	  p4 = (-t2+t4)*t8*t10;
      p3 = 2.0f*t1*vy+t15-t18-t20-t22+t25+t28-t29+t32-t34+t35+t36-t38+2.0f*t3*vx;
      p2 = -2.0f*t33*t27-2.0f*t26*t14-2.0f*t23*t21+2.0f*t37*t14+2.0f*t30*t17+2.0f*t13
*t27+t2-t4+2.0f*t19*t31-2.0f*t16*t24;
      t57 = -t22+t29+t36-t25-t32+t34+t35-t28-t15+t20-t18+t38;
      p1 = t57*t8*t10;

	coefs[0] = p4;
	coefs[1] = p3;
	coefs[2] = p2;
	coefs[1] = p1;
		
	}
	
	numroots = polynomialSolver.Solve3Cubic(coefs[0],coefs[1],coefs[2],coefs[3]);
	
	for (int i=0;i<numroots;i++)
	{
		//SimdScalar tau = roots[i];//polynomialSolver.GetRoot(i);
		SimdScalar tau = polynomialSolver.GetRoot(i);
		
		//check whether mu and lambda are in range [0..1]
		
		if (!SimdFuzzyZero(v.z()))
		{
			SimdScalar A1=(ux-ux*tau*tau-2.f*tau*uy)-((1.f+tau*tau)*vx*uz/vz);
			SimdScalar B1=((1.f+tau*tau)*(cx*SimdTan(1.f/2.f*w)*vz+
				vx*az*SimdTan(1.f/2.f*w)-vx*cz*SimdTan(1.f/2.f*w)+
				vx*s*tau)/SimdTan(1.f/2.f*w)/vz)-(ax-ax*tau*tau-2.f*tau*ay);
			lambda = B1/A1;
			
			mu = (a.z()-c.z()+lambda*u.z()+(s*tau)/(SimdTan(w/2.f)))/v.z();
			
			
			//double check in original equation
			
			SimdScalar lhs = (a.x()+lambda*u.x())
				*((1.f-tau*tau)/(1.f+tau*tau))-
				(a.y()+lambda*u.y())*((2.f*tau)/(1.f+tau*tau));
			
			lhs = lambda*((ux-ux*tau*tau-2.f*tau*uy)-((1.f+tau*tau)*vx*uz/vz));
			
			SimdScalar rhs = c.x()+mu*v.x();
			
			rhs = ((1.f+tau*tau)*(cx*SimdTan(1.f/2.f*w)*vz+vx*az*SimdTan(1.f/2.f*w)-
				vx*cz*SimdTan(1.f/2.f*w)+vx*s*tau)/(SimdTan(1.f/2.f*w)*vz))-
				
				(ax-ax*tau*tau-2.f*tau*ay);
			
			/*SimdScalar res = coefs[0]*tau*tau*tau+
				coefs[1]*tau*tau+
				coefs[2]*tau+
				coefs[3];*/
			
			//lhs should be rhs !
			
			if (0.<= mu && mu <=1 && 0.<=lambda && lambda <= 1)
			{
				
			} else
			{
				//skip this solution, not really touching
				continue;				
			}
			
		}
		
		SimdScalar t = 2.f*SimdAtan(tau)/screwAB.GetW();
		//tau = tan (wt/2) so 2*atan (tau)/w
		if (t>=0.f && t<minTime)
		{
#ifdef STATS_EDGE_EDGE
			printf(" ax = %12.12f\n ay = %12.12f\n az = %12.12f\n",a.x(),a.y(),a.z());
			printf(" ux = %12.12f\n uy = %12.12f\n uz = %12.12f\n",u.x(),u.y(),u.z());
			printf(" cx = %12.12f\n cy = %12.12f\n cz = %12.12f\n",c.x(),c.y(),c.z());
			printf(" vx = %12.12f\n vy = %12.12f\n vz = %12.12f\n",v.x(),v.y(),v.z());
			printf(" s  = %12.12f\n w  = %12.12f\n",       s,     w);
			
			printf(" tau = %12.12f \n lambda = %12.12f \n mu = %f\n",tau,lambda,mu); 
			printf(" ---------------------------------------------\n"); 
			
#endif
			
			//	v,u,a,c,s,w
			
			//	BU_IntervalArithmeticPolynomialSolver iaSolver;
			//	int numroots2 = iaSolver.Solve3Cubic(coefs[0],coefs[1],coefs[2],coefs[3]);
			
			minTime = t;
			hit = true;
		}
	}
	
	return hit;
}


//C -S
//S C

bool BU_EdgeEdge::Calc2DRotationPointPoint(const SimdPoint3& rotPt, SimdScalar rotRadius, SimdScalar rotW,const SimdPoint3& intersectPt,SimdScalar& minTime)
{
	bool hit = false;
	
	// now calculate the planeEquation for the vertex motion,
	// and check if the intersectionpoint is at the positive side
	SimdPoint3 rotPt1(SimdCos(rotW)*rotPt.x()-SimdSin(rotW)*rotPt.y(),
	SimdSin(rotW)*rotPt.x()+SimdCos(rotW)*rotPt.y(),
	0.f);

	SimdVector3 rotVec = rotPt1-rotPt;
	
	SimdVector3 planeNormal( -rotVec.y() , rotVec.x() ,0.f);
	
	//SimdPoint3 pt(a.x(),a.y());//for sake of readability,could write dot directly
	SimdScalar planeD = planeNormal.dot(rotPt1);
	
	SimdScalar dist = (planeNormal.dot(intersectPt)-planeD);
	hit = (dist >= -0.001);
	
	//if (hit)
	{
		//		minTime = 0;
		//calculate the time of impact, using the fact of
		//toi = alpha / screwAB.getW();
		// cos (alpha) = adjacent/hypothenuse;
		//adjacent = dotproduct(ipedge,point);
		//hypothenuse = sqrt(r2);
		SimdScalar adjacent = intersectPt.dot(rotPt)/rotRadius;
		SimdScalar hypo = rotRadius;
		SimdScalar alpha = SimdAcos(adjacent/hypo);
		SimdScalar t = alpha / rotW;
		if (t >= 0 && t < minTime)
		{
			hit = true;
			minTime = t;
		} else
		{
			hit = false;
		}
		
	}
	return hit;
}

bool BU_EdgeEdge::GetTimeOfImpactVertexEdge(
											const BU_Screwing& screwAB,
											const SimdPoint3& a,//edge in object A
											const SimdVector3& u,
											const SimdPoint3& c,//edge in object B
											const SimdVector3& v,
											SimdScalar &minTime,
											SimdScalar &lamda,
											SimdScalar& mu
											
											)
{
	return false;
}

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

#include "COM_LensGhostOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#define MAX_STEP 256
class Ray {
public:
	float position[3];
	float direction[3];
	float uv[2];
	double wavelength;
	float intensity;
	bool valid;
	void copyFrom(Ray* other) {
		copy_v3_v3(position, other->position);
		copy_v3_v3(direction, other->direction);
		copy_v2_v2(uv, other->uv);
		wavelength = other->wavelength;
		intensity = other->intensity;
		this->valid = other->valid;
	}
};

class Intersection {
public:
	float position[3];
	float normal[3];
	double theta;
	bool hit;
	bool inverted;
};

class LensInterface {
public:
	float position[3];
	float radius;
	float nominalRadius;
	double refraction1;
	double refraction2;
	double refraction3;
	float thicknessCoathing;
	virtual bool isFlat() = 0;
	virtual void intersect(Intersection* result, Ray* ray) = 0;
};

class FlatInterface: public LensInterface {
public:
	bool isFlat() {return true;}
	FlatInterface(float positionX, float positionY, float positionZ, float radius) {
		this->position[0] = positionX;
		this->position[1] = positionY;
		this->position[2] = positionZ;
		this->radius = radius;
		this->nominalRadius = radius;
		this->refraction1 = 1.0f;
		this->refraction2 = 1.0f;
		this->refraction3 = 1.0f;
		this->thicknessCoathing = 0.0f;

	}
	void intersect(Intersection* result, Ray* ray) {
		const float dz = this->position[2]-ray->position[2];
		result->position[0] = ray->position[0] + ray->direction[0]*(dz)/ray->direction[2];
		result->position[1] = ray->position[1] + ray->direction[1]*(dz)/ray->direction[2];
		result->position[2] = ray->position[2] + ray->direction[2]*(dz)/ray->direction[2];
		result->normal[0] = 0.0f;
		result->normal[1] = 0.0f;
		result->normal[2] = ray->direction[2]>0?-1.0f:1.0f;
		result->theta = 0.0f;
//		result->hit = this->nominalRadius>maxf(fabs(result->position[0]), fabs(result->position[1]));
		result->hit = true;
		result->inverted = false;
	}
};

class SphereInterface: public LensInterface {
public:
	SphereInterface(float positionX, float positionY, float positionZ, float radius, float nominalRadius, float n0, float n2, float coatingPhase) {
		this->position[0] = positionX;
		this->position[1] = positionY;
		this->position[2] = positionZ;
		this->radius = radius;
		this->nominalRadius = nominalRadius;
		this->refraction1 = n0;
		this->refraction3 = n2;
		this->refraction2 = maxf(sqrtf(n0*n2), 1.38);

		this->thicknessCoathing = coatingPhase/4/this->refraction2;
	}
	bool isFlat() {return false;}
	void intersect(Intersection* result, Ray* ray) {
		float delta[3] ={ray->position[0] - this->position[0],
			ray->position[1] - this->position[1],
			ray->position[2] - this->position[2]};
		float b = dot_v3v3(delta, ray->direction);
		float c = dot_v3v3(delta, delta) - this->radius*this->radius;
		float b2c = b*b-c;
		if (b2c < 0) {
			result->hit = false;
		} else {
			float sgn = (this->radius*ray->direction[2])>0?1.0f:-1.0f;
			float t = sqrtf(b2c)*sgn-b;
			result->position[0] = ray->direction[0]*t+ray->position[0];
			result->position[1] = ray->direction[1]*t+ray->position[1];
			result->position[2] = ray->direction[2]*t+ray->position[2];

			float p[3] = {
				result->position[0] - this->position[0],
				result->position[1] - this->position[1],
				result->position[2] - this->position[2]
			};
			normalize_v3(p);

			if (dot_v3v3(p, ray->direction)> 0) {
				result->normal[0] = -p[0];
				result->normal[1] = -p[1];
				result->normal[2] = -p[2];
			} else {
				result->normal[0] = p[0];
				result->normal[1] = p[1];
				result->normal[2] = p[2];
			}

			float inverse[3] ={
				-ray->direction[0],
				-ray->direction[1],
				-ray->direction[2]};

			result->theta = acosf(dot_v3v3(inverse, result->normal));
			result->hit = this->nominalRadius>sqrt(result->position[0]*result->position[0]+result->position[1]*result->position[1]);
//			result->hit = this->nominalRadius>maxf(fabs(result->position[0]), fabs(result->position[1]));
//			result->hit = true;
			result->inverted = t < 0;
		}
	}
};
class RayResult {
public:
	float x;
	float y;
	float intensity[3];
	float u;
	float v;
	float screenX;
	float screenY;
	bool valid;
	bool hasIntensity;
};
class Bounce{
public:
	LensInterface *interface1;
	LensInterface *interface2;
	RayResult *raster;
	int length; // number of interfaces to travel
	int rasterLength;
	Bounce(LensInterface *interface1, LensInterface *interface2, int length, int rasterStep) {
		this->interface1 = interface1;
		this->interface2 = interface2;
		this->length = length;
		this->rasterLength = rasterStep;
		this->raster = new RayResult[rasterLength*rasterLength];
		for (int i = 0 ; i < rasterLength*rasterLength ; i++) {
			RayResult * res = &this->raster[i];
			res->intensity[0] = 0.0f;
			res->intensity[1] = 0.0f;
			res->intensity[2] = 0.0f;
			res->x = 0.0f;
			res->y = 0.0f;
			res->u = 0.0f;
			res->v = 0.0f;
			res->valid = false;
		}
	}
	~Bounce() {
		delete raster;

	}

	RayResult* getRayResult(int x, int y) {
		return &(raster[x+y*rasterLength]);
	}
};
class LensSystem {
public:
	vector<LensInterface*> interfaces;
	vector<Bounce*> bounces;
	int bokehIndex;
	int lensIndex;

	~LensSystem() {
		for (int index = 0 ; index <bounces.size();index++) {delete bounces[index];}
		for (int index = 0 ; index <interfaces.size();index++) {delete interfaces[index];}
	}

	void updateBounces(int step) {
		for (int i = 0; i < interfaces.size()-1 ; i ++) {
			if (!interfaces[i]->isFlat()) {
				for (int j = i+1; j < interfaces.size()-1 ; j ++) {
					if (!interfaces[j]->isFlat()) {
						int length = interfaces.size()+2*(j-i);
						Bounce* bounce = new Bounce(interfaces[j], interfaces[i], length, step);
						bounces.push_back(bounce);
					}
				}
			}
		}

	}

	void addInterface(LensInterface* pinterface) {
		this->interfaces.push_back(pinterface);
		this->lensIndex = this->interfaces.size()-1;
	}

	static int refraction(float *refract, float *n, float *view, double index)
	{

                return 1;

//                float dot, fac;

//                VECCOPY(refract, view);

//                dot= view[0]*n[0] + view[1]*n[1] + view[2]*n[2];

//                if(dot>0.0f) {
//                        index = 1.0f/index;
//                        fac= 1.0f - (1.0f - dot*dot)*index*index;
//                        if(fac<= 0.0f) return 0;
//                        fac= -dot*index + sqrt(fac);
//                }
//                else {
//                        fac= 1.0f - (1.0f - dot*dot)*index*index;
//                        if(fac<= 0.0f) return 0;
//                        fac= -dot*index - sqrt(fac);
//                }

//                refract[0]= index*view[0] + fac*n[0];
//                refract[1]= index*view[1] + fac*n[1];
//                refract[2]= index*view[2] + fac*n[2];

//                normalize_v3(refract);
//                return 1;
		//---
//		const double cosI = dot_v3v3(n, view);
//		const double sinT2 = index * index * (1.0 - cosI * cosI);
//		if (sinT2 >= 1.0f)
//		{
//			return 0;
//		}
//		refract[0] = index*view[0] - (index + sqrt(1.0-sinT2))*n[0];
//		refract[1] = index*view[1] - (index + sqrt(1.0-sinT2))*n[1];
//		refract[2] = index*view[2] - (index + sqrt(1.0-sinT2))*n[2];
//		normalize_v3(refract);
//		return 1;
		//---

//                double ni = -dot_v3v3(view, n);
//                double test = 1.0f - index*index*(1.0f-ni*ni);
//                if (test < 0) {
//                        return 0;
//                } else {
//                        double mul = index*ni + sqrt(test);
//                        refract[0] = index * view[0] - mul*n[0];
//                        refract[1] = index * view[1] - mul*n[1];
//                        refract[2] = index * view[2] - mul*n[2];
//                        normalize_v3(refract);
//                        return 1;
//                }
	}

	/* orn = original face normal */
	static void reflection(float *ref, float *n, float *view)
	{
		float f1;

		f1= -2.0f*dot_v3v3(n, view);

		ref[0]= (view[0]+f1*n[0]);
		ref[1]= (view[1]+f1*n[1]);
		ref[2]= (view[2]+f1*n[2]);
		normalize_v3(ref);
	}

	static float fresnelAR(float theta0, float lambda, float d1, float n0, float n1, float n2) {
		// refractionangles in coating and the 2nd medium
		float theta1 = asin(sin(theta0)*n0/n1);
		float theta2 = asin(sin(theta0)*n0/n2);

		float rs01 = -sin(theta0-theta1)/sin(theta0+theta1);
		float rp01 = tan( theta0-theta1)/tan(theta0+theta1);
		float ts01 = 2 * sin ( theta1 ) * cos ( theta0 ) / sin ( theta0+theta1 ) ;
		float tp01 = ts01*cos(theta0-theta1);
		// amplitude for inner reflection
		float rs12 = -sin ( theta1-theta2 ) / sin ( theta1+theta2 ) ;
		float rp12 = +tan ( theta1-theta2 ) / tan ( theta1+theta2 ) ;
		// after passing through first surface twice :
		// 2 transmissions and 1 reflection
		float ris = ts01 * ts01 * rs12 ;
		float rip = tp01 * tp01 * rp12 ;
		// phase difference between outer and inner reflections
		float dy = d1 * n1 ;
		float dx = tan ( theta1 ) * dy ;
		float delay = sqrt ( dx * dx+dy * dy ) ;
		float relPhase = 4 * M_PI / lambda * ( delay-dx * sin ( theta0 ) ) ;
		// Add up sines of different phase and amplitude
		float out_s2 = rs01 * rs01 + ris * ris + 2 * rs01 * ris * cos ( relPhase ) ;
		float out_p2 = rp01 * rp01 + rip * rip + 2 * rp01 * rip * cos ( relPhase ) ;
		return ( out_s2+out_p2 ) / 2 ;
	}

	void detectHit(Ray* result, Ray* inputRay, Bounce *bounce) {
		int phase = 0;
		int delta = 1;
		int t = 1;
		int k;
		result->copyFrom(inputRay);
		result->valid = false;
		LensInterface* next = bounce->interface1;
		LensInterface* f = NULL;
		Intersection intersection;
		for (k = 0 ; k < bounce->length-1;k++, t+=delta) {
			f = this->interfaces[t];
			bool breflect = next == f;
			if (breflect) {
				delta = -delta;
				if (phase == 0) {
					next = bounce->interface2;
				} else {
					next = NULL;
				}
				phase ++;
			}

			f->intersect(&intersection, result);
			if (!intersection.hit) {
				break;
			}
			if (f->isFlat()) {
				if (t == this->bokehIndex) {
					result->uv[0] = intersection.position[0]/f->nominalRadius;
					result->uv[1] = intersection.position[1]/f->nominalRadius;
				}
			}

			float p[3] = {
				intersection.position[0]-result->position[0],
				intersection.position[1]-result->position[1],
				intersection.position[2]-result->position[2]
			};

			float nfac = sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);

			if (intersection.inverted) {
				nfac *= -1;
			}

			result->direction[0] = p[0]/nfac;
			result->direction[1] = p[1]/nfac;
			result->direction[2] = p[2]/nfac;
			result->position[0] = intersection.position[0];
			result->position[1] = intersection.position[1];
			result->position[2] = intersection.position[2];

			if (!f->isFlat()) {
				// do refraction and reflection
				double n0 = result->direction[2]<0?f->refraction1:f->refraction3;
				double n1 = f->refraction2;
				double n2 = result->direction[2]<0?f->refraction3:f->refraction1;
				if (!breflect) {
					float view[3] ={
						result->direction[0],
						result->direction[1],
						result->direction[2]
					};
					int ref = this->refraction(result->direction, intersection.normal, view, n0/n1);
					if (ref == 0) {
						break;
					}
				} else {
					this->reflection(result->direction, intersection.normal, result->direction);
					float  fresnelMultiplyer = fresnelAR(intersection.theta, result->wavelength, f->thicknessCoathing, n0, n1, n2);
					if (isnan(fresnelMultiplyer)) {
						fresnelMultiplyer = 0.0f;
					}
					result->intensity *= fresnelMultiplyer;
				}
			}

		}
		if (k < bounce->length-1) {
			result->intensity = 0;
		} else {
			result->valid = true;
		}
	}
};

typedef struct LensFace {
	RayResult* v1;
	RayResult* v2;
	RayResult* v3;
} LensFace;

LensGhostProjectionOperation::LensGhostProjectionOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->lampObject = NULL;
	this->cameraObject = NULL;
	this->system = NULL;
	this->quality = COM_QUALITY_HIGH;
	this->setComplex(false);
}

LensGhostOperation::LensGhostOperation(): LensGhostProjectionOperation() {
	this->setComplex(true);

}

void LensGhostProjectionOperation::initExecution() {
	if (this->cameraObject != NULL && this->lampObject != NULL) {
		if (lampObject == NULL || cameraObject == NULL) {
			visualLampPosition[0] = 0;
			visualLampPosition[1] = 0;
			visualLampPosition[2] = 0;
		} else {
			/* too simple, better to return the distance on the view axis only
			 * return len_v3v3(ob->obmat[3], cam->dof_ob->obmat[3]); */
			float matt[4][4], imat[4][4], obmat[4][4];

			copy_m4_m4(obmat, cameraObject->obmat);
			normalize_m4(obmat);
			invert_m4_m4(imat, obmat);
			mult_m4_m4m4(matt, imat, lampObject->obmat);

			visualLampPosition[0] = (float)(matt[3][0]);
			visualLampPosition[1] = (float)(matt[3][1]);
			visualLampPosition[2] = (float)fabs(matt[3][2]);
		}
	}
	this->lamp = (Lamp*)lampObject->data;

	this->step = this->quality==COM_QUALITY_LOW?64:this->quality==COM_QUALITY_MEDIUM?128:256;
	this->bokehReader = this->getInputSocketReader(1);

#define MM *0.001f
#define CM *0.01f
#define NM *0.000000001
#define RED 650 NM
#define GREEN 510 NM
#define BLUE 475 NM
#define AIR 1.000293f
#define GLASS 1.5200f
#define TEST 0.000002f
	// determine interfaces
	LensSystem *system = new LensSystem();
	system->addInterface(new FlatInterface(0.0f,0.0f, 6.5 CM, 30 MM)); //ENTRANCE
        system->addInterface(new SphereInterface(0.0f,0.0f, -3 CM, 8 CM, 3 CM, AIR, GLASS , 0.f));
        system->addInterface(new SphereInterface(0.0f,0.0f, -4 CM, 8 CM, 3 CM, GLASS, AIR, GREEN));
        system->addInterface(new FlatInterface(0.0f,0.0f, 3.0 CM, 15 MM)); // BOKEH
        system->addInterface(new SphereInterface(0.0f,0.0f, 6 CM, 3 CM, 2 CM, AIR, GLASS, 0.0f));
        system->addInterface(new SphereInterface(0.0f,0.0f, 5.5 CM, 3 CM, 2 CM, GLASS, AIR, 0.f));
        system->addInterface(new FlatInterface(0.0f,0.0f,0 CM, 30 MM)); // SENSOR
	system->bokehIndex =3;

	// determine interfaces
//	LensSystem *system = new LensSystem();
//	system->addInterface(new FlatInterface(0.0f,0.0f, 6.5 CM, 30 MM)); //ENTRANCE
//	system->addInterface(new SphereInterface(0.0f,0.0f, 14 CM, 8 CM, 6 CM, AIR, GLASS , 0.0f));
//	system->addInterface(new SphereInterface(0.0f,0.0f, 12 CM, 8 CM, 6 CM, GLASS, AIR, GREEN));
//	system->addInterface(new FlatInterface(0.0f,0.0f, 3.0 CM, 30 MM)); // BOKEH
//	system->addInterface(new SphereInterface(0.0f,0.0f, 1 CM, 3 CM, 2 CM, AIR, GLASS, GREEN));
//	system->addInterface(new SphereInterface(0.0f,0.0f, -2 CM, 3 CM, 2 CM, GLASS, AIR, RED));
//	system->addInterface(new FlatInterface(0.0f,0.0f,0 CM, 20 MM)); // SENSOR
//	system->bokehIndex = 3;
#undef CM
#undef MM
	// determine bounces
	system->updateBounces(step);
	this->system = system;
}

void LensGhostOperation::initExecution() {
	LensGhostProjectionOperation::initExecution();
	LensSystem *system = (LensSystem*)this->system;
	LensInterface *interface1 = system->interfaces[0];

	// for every herz
	float HERZ[3]={650 NM,510 NM,475 NM}; /// @todo use 7 for high quality?
	for (int iw = 0 ; iw < 3 ; iw ++) {
		float wavelength = HERZ[iw];
	// for every bounce
		for (int ib = 0 ; ib < system->bounces.size() ; ib++) {
			Bounce* bounce = system->bounces[ib];
	// based on quality setting the number of iteration will be different (128^2, 64^2, 32^2)
			for (int xi = 0 ; xi < step ; xi ++) {
				float x = -interface1->radius+xi*(interface1->radius*2/step);
				for (int yi = 0 ; yi < step ; yi ++) {
					float y = -interface1->radius+yi*(interface1->radius*2/step);
					Ray r;
					Ray result;
					r.wavelength = wavelength;
					r.intensity = this->lamp->energy;
					r.uv[0] = 0.0f;
					r.uv[1] = 0.0f;
					r.position[0] = visualLampPosition[0];
					r.position[1] = visualLampPosition[1];
					r.position[2] = visualLampPosition[2];
					r.direction[0] = interface1->position[0]+x - r.position[0];
					r.direction[1] = interface1->position[1]+y - r.position[1];
					r.direction[2] = interface1->position[2] - r.position[2];
					normalize_v3(r.direction);
					system->detectHit(&result, &r, bounce);
					RayResult *res = bounce->getRayResult(xi, yi);
					if (iw == 0) {
						res->x = result.position[0];
						res->y = result.position[1];
						res->u = result.uv[0];
						res->v = result.uv[1];
					}
					res->intensity[iw] = result.intensity;
					if (result.valid) {
						res->valid = true;
					}
				}
			}
		}
	}
#undef NM
	const int width = this->getWidth();
	const int height = this->getHeight();
	const float width2 = width/2.0f;
	const float height2 = height/2.0f;
	float *data = new float[width*height*4];
	for (int i = 0 ; i < width*height ; i ++) {
		data[i*4+0] = 0.0f;
		data[i*4+1] = 0.0f;
		data[i*4+2] = 0.0f;
		data[i*4+3] = 1.0f;
	}
	/// @todo every bounce creates own image. these images are added together at the end
//	LensSystem *system = (LensSystem*)this->system;
	LensInterface * lens = system->interfaces[system->lensIndex];
	for (int i = 0 ; i < system->bounces.size() ; i ++) {
		Bounce* bounce = system->bounces[i];
		for (int r = 0 ; r < bounce->rasterLength*bounce->rasterLength ; r ++) {
			RayResult *result = &bounce->raster[r];
//			if (result->valid) {
				float ru= result->x/lens->nominalRadius*width2+width2;
				float rv  = result->y/lens->nominalRadius*height2+height2;
				result->screenX = ru;
				result->screenY = rv;
				result->hasIntensity = result->intensity[0]>0.0f &&result->intensity[1]>0.0f&& result->intensity[2]>0.0f;
//			}
		}
	}
}

void* LensGhostOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers) {
	vector<LensFace*>* result = new vector<LensFace*>();
	LensSystem *system = (LensSystem*)this->system;
	const float minx = rect->xmin;
	const float miny = rect->ymin;
	const float maxx = rect->xmax;
	const float maxy = rect->ymax;
	for (int i = 0 ; i < system->bounces.size() ; i ++) {
		Bounce* bounce = system->bounces[i];
		int faceX, faceY;
		for (faceX = 0 ; faceX < bounce->rasterLength-1 ; faceX++) {
			for (faceY = 0 ; faceY < bounce->rasterLength-1 ; faceY++) {
				RayResult* vertex1 = bounce->getRayResult(faceX, faceY);
				RayResult* vertex2 = bounce->getRayResult(faceX+1, faceY);
				RayResult* vertex3 = bounce->getRayResult(faceX+1, faceY+1);
				RayResult* vertex4 = bounce->getRayResult(faceX, faceY+1);
				// early hit test
				if (!((vertex1->screenX < minx && vertex2->screenX < minx && vertex3->screenX < minx && vertex4->screenX < minx) ||
					(vertex1->screenX > maxx && vertex2->screenX > maxx && vertex3->screenX > maxx && vertex4->screenX > maxx) ||
					(vertex1->screenY < miny && vertex2->screenY < miny && vertex3->screenY < miny && vertex4->screenY < miny) ||
					(vertex1->screenY > maxy && vertex2->screenY > maxy && vertex3->screenY > maxy && vertex4->screenY > maxy))) {
					int number = vertex1->hasIntensity +vertex2->hasIntensity +vertex3->hasIntensity +vertex4->hasIntensity;
					if (number == 4) {
						LensFace* face = new LensFace();
						face->v1 = vertex1;
						face->v2 = vertex2;
						face->v3 = vertex3;
						result->push_back(face);
						face = new LensFace();
						face->v1 = vertex3;
						face->v2 = vertex4;
						face->v3 = vertex1;
						result->push_back(face);
					} else if (number == 3) {
						LensFace *face = new LensFace();
						if (!vertex1->hasIntensity) {
							face->v1 = vertex2;
							face->v2 = vertex3;
							face->v3 = vertex4;
						} else if (!vertex2->hasIntensity) {
							face->v1 = vertex1;
							face->v2 = vertex3;
							face->v3 = vertex4;
						} else if (!vertex3->hasIntensity) {
							face->v1 = vertex1;
							face->v2 = vertex2;
							face->v3 = vertex4;
						} else {
							face->v1 = vertex1;
							face->v2 = vertex2;
							face->v3 = vertex3;
						}
						result->push_back(face);
					}
				}
			}
		}
	}

	return result;
}

void LensGhostOperation::deinitializeTileData(rcti *rect, MemoryBuffer **memoryBuffers, void *data) {
	if (data) {
		vector<LensFace*>* faces = (vector<LensFace*>*)data;
		while (faces->size() != 0) {
			LensFace *face = faces->back();
			faces->pop_back();
			delete face;
		}
		delete faces;
	}
}


void LensGhostProjectionOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float bokeh[4];
	LensSystem *system = (LensSystem*)this->system;
	LensInterface *interface1 = system->interfaces[0];
	color[0] = 0.0f;
	color[1] = 0.0f;
	color[2] = 0.0f;
	color[3] = 0.0f;
	const float width = this->getWidth();
	const float height = this->getHeight();
	const float size = min(height, width);
	const float width2 = width/2;
	const float height2 = height/2;
	const float size2 = size/2;

#define NM *0.000000001
	float HERZ[3]={650 NM,510 NM,475 NM}; /// @todo use 7 for high quality?
	float rx = ((x-width2)/size2) * interface1->radius;
	float ry = ((y-height2)/size2) * interface1->radius;

	for (int iw = 0 ; iw < 3 ; iw ++) {
		float intensity = 0.0f;
		float wavelength = HERZ[iw];
		float colorcomponent = 0.0f;
		if (iw ==0 ) colorcomponent = lamp->r;
		if (iw ==1 ) colorcomponent = lamp->g;
		if (iw ==2 ) colorcomponent = lamp->b;


	// for every bounce
		for (int ib = 0 ; ib < system->bounces.size() ; ib++) {
			Bounce* bounce = system->bounces[ib];
	// based on quality setting the number of iteration will be different (128^2, 64^2, 32^2)

			Ray r;
			Ray result;
			r.wavelength = wavelength;
			r.intensity = this->lamp->energy;
			r.uv[0] = 0.0f;
			r.uv[1] = 0.0f;
			r.position[0] = visualLampPosition[0];
			r.position[1] = visualLampPosition[1];
			r.position[2] = visualLampPosition[2];
			r.direction[0] = interface1->position[0]+rx - r.position[0];
			r.direction[1] = interface1->position[1]+ry - r.position[1];
			r.direction[2] = interface1->position[2] - r.position[2];
			normalize_v3(r.direction);
			system->detectHit(&result, &r, bounce);
			if (result.valid) {
				float u = ((result.uv[0]+1.0f)/2)*bokehReader->getWidth();
				float v = ((result.uv[1]+1.0f)/2)*bokehReader->getHeight();

				bokehReader->read(bokeh, u, v, sampler, inputBuffers);

				intensity += result.intensity *bokeh[iw];
			}
		}
		intensity = maxf(0.0f, intensity);
		color[iw] = intensity*colorcomponent;
	}
	color[3] = 1.0f;
#undef NM

}



void LensGhostOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	vector<LensFace*>* faces = (vector<LensFace*>*)data;
#if 0 /* UNUSED */
	const float bokehWidth = bokehReader->getWidth();
	const float bokehHeight = bokehReader->getHeight();
	float bokeh[4];
#endif
	color[0] = 0.0f;
	color[1] = 0.0f;
	color[2] = 0.0f;
	color[3] = 1.0f;

	unsigned int index;
	for (index = 0 ; index < faces->size() ; index ++) {
		LensFace * face = faces->operator [](index);
		RayResult* vertex1 = face->v1;
		RayResult* vertex2 = face->v2;
		RayResult* vertex3 = face->v3;
		if (!((vertex1->screenX < x && vertex2->screenX < x && vertex3->screenX < x) ||
			(vertex1->screenX > x && vertex2->screenX > x && vertex3->screenX > x) ||
			(vertex1->screenY < y && vertex2->screenY < y && vertex3->screenY < y) ||
			(vertex1->screenY > y && vertex2->screenY > y && vertex3->screenY > y))) {

			const float v1[2] = {vertex1->screenX, vertex1->screenY};
			const float v2[2] = {vertex2->screenX, vertex2->screenY};
			const float v3[2] = {vertex3->screenX, vertex3->screenY};
			const float co[2] = {x, y};
			float weights[3];

			barycentric_weights_v2(v1, v2, v3, co, weights);
			if (weights[0]>=0.0f && weights[0]<=1.0f &&
					weights[1]>=0.0f && weights[1]<=1.0f &&
					weights[2]>=0.0f && weights[2]<=1.0f) {
//                            const float u = (vertex1->u*weights[0]+vertex2->u*weights[1]+vertex3->u*weights[2]);
//                            const float v = (vertex1->v*weights[0]+vertex2->v*weights[1]+vertex3->v*weights[2]);
//                            const float tu = ((u+1.0f)/2.0f)*bokehWidth;
//                            const float tv = ((v+1.0f)/2.0f)*bokehHeight;
//                            bokehReader->read(bokeh, tu, tv, inputBuffers);

//							color[0] = max(color[0], bokeh[0]*(vertex1->intensity[0]*weights[0]+vertex2->intensity[0]*weights[1]+vertex3->intensity[0]*weights[2]));
//                            color[1] = max(color[1], bokeh[1]*(vertex1->intensity[1]*weights[0]+vertex2->intensity[1]*weights[1]+vertex3->intensity[1]*weights[2]));
//                            color[2] = max(color[2], bokeh[2]*(vertex1->intensity[2]*weights[0]+vertex2->intensity[2]*weights[1]+vertex3->intensity[2]*weights[2]));
							color[0] = max(color[0], (vertex1->intensity[0]*weights[0]+vertex2->intensity[0]*weights[1]+vertex3->intensity[0]*weights[2]));
                            color[1] = max(color[1], (vertex1->intensity[1]*weights[0]+vertex2->intensity[1]*weights[1]+vertex3->intensity[1]*weights[2]));
                            color[2] = max(color[2], (vertex1->intensity[2]*weights[0]+vertex2->intensity[2]*weights[1]+vertex3->intensity[2]*weights[2]));
			}
		}
	}
}


void LensGhostProjectionOperation::deinitExecution() {
	if (this->system) delete (LensSystem*)this->system;
	this->system = NULL;
	this->bokehReader = NULL;
}

bool LensGhostProjectionOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti bokehInput;

	NodeOperation *operation = this->getInputOperation(1);
	bokehInput.xmax = operation->getWidth();
	bokehInput.xmin = 0;
	bokehInput.ymax = operation->getHeight();
	bokehInput.ymin = 0;
	if (operation->determineDependingAreaOfInterest(&bokehInput, readOperation, output) ) {
		return true;
	}

	return NodeOperation::determineDependingAreaOfInterest(input, readOperation, output);
}

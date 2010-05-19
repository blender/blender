//
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef DETOURCOMMON_H
#define DETOURCOMMON_H

//////////////////////////////////////////////////////////////////////////////////////////

template<class T> inline void swap(T& a, T& b) { T t = a; a = b; b = t; }
template<class T> inline T min(T a, T b) { return a < b ? a : b; }
template<class T> inline T max(T a, T b) { return a > b ? a : b; }
template<class T> inline T abs(T a) { return a < 0 ? -a : a; }
template<class T> inline T sqr(T a) { return a*a; }
template<class T> inline T clamp(T v, T mn, T mx) { return v < mn ? mn : (v > mx ? mx : v); }

inline void vcross(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[1]*v2[2] - v1[2]*v2[1];
	dest[1] = v1[2]*v2[0] - v1[0]*v2[2];
	dest[2] = v1[0]*v2[1] - v1[1]*v2[0]; 
}

inline float vdot(const float* v1, const float* v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

inline void vmad(float* dest, const float* v1, const float* v2, const float s)
{
	dest[0] = v1[0]+v2[0]*s;
	dest[1] = v1[1]+v2[1]*s;
	dest[2] = v1[2]+v2[2]*s;
}

inline void vadd(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[0]+v2[0];
	dest[1] = v1[1]+v2[1];
	dest[2] = v1[2]+v2[2];
}

inline void vsub(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[0]-v2[0];
	dest[1] = v1[1]-v2[1];
	dest[2] = v1[2]-v2[2];
}

inline void vmin(float* mn, const float* v)
{
	mn[0] = min(mn[0], v[0]);
	mn[1] = min(mn[1], v[1]);
	mn[2] = min(mn[2], v[2]);
}

inline void vmax(float* mx, const float* v)
{
	mx[0] = max(mx[0], v[0]);
	mx[1] = max(mx[1], v[1]);
	mx[2] = max(mx[2], v[2]);
}

inline void vcopy(float* dest, const float* a)
{
	dest[0] = a[0];
	dest[1] = a[1];
	dest[2] = a[2];
}

inline float vdist(const float* v1, const float* v2)
{
	float dx = v2[0] - v1[0];
	float dy = v2[1] - v1[1];
	float dz = v2[2] - v1[2];
	return sqrtf(dx*dx + dy*dy + dz*dz);
}

inline float vdistSqr(const float* v1, const float* v2)
{
	float dx = v2[0] - v1[0];
	float dy = v2[1] - v1[1];
	float dz = v2[2] - v1[2];
	return dx*dx + dy*dy + dz*dz;
}

inline void vnormalize(float* v)
{
	float d = 1.0f / sqrtf(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
	v[0] *= d;
	v[1] *= d;
	v[2] *= d;
}

inline bool vequal(const float* p0, const float* p1)
{
	static const float thr = sqr(1.0f/16384.0f);
	const float d = vdistSqr(p0, p1);
	return d < thr;
}

inline int nextPow2(int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

inline float vdot2D(const float* u, const float* v)
{
	return u[0]*v[0] + u[2]*v[2];
}

inline float vperp2D(const float* u, const float* v)
{
	return u[2]*v[0] - u[0]*v[2];
}

inline float triArea2D(const float* a, const float* b, const float* c)
{
	return ((b[0]*a[2] - a[0]*b[2]) + (c[0]*b[2] - b[0]*c[2]) + (a[0]*c[2] - c[0]*a[2])) * 0.5f;
}

inline bool checkOverlapBox(const unsigned short amin[3], const unsigned short amax[3],
							const unsigned short bmin[3], const unsigned short bmax[3])
{
	bool overlap = true;
	overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
	overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
	overlap = (amin[2] > bmax[2] || amax[2] < bmin[2]) ? false : overlap;
	return overlap;
}

void closestPtPointTriangle(float* closest, const float* p,
							const float* a, const float* b, const float* c);

bool closestHeightPointTriangle(const float* p, const float* a, const float* b, const float* c, float& h);

bool intersectSegmentPoly2D(const float* p0, const float* p1,
							const float* verts, int nverts,
							float& tmin, float& tmax,
							int& segMin, int& segMax);

float distancePtSegSqr2D(const float* pt, const float* p, const float* q, float& t);

void calcPolyCenter(float* tc, const unsigned short* idx, int nidx, const float* verts);

#endif // DETOURCOMMON_H
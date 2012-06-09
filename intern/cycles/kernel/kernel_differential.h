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
 */

CCL_NAMESPACE_BEGIN

/* See "Tracing Ray Differentials", Homan Igehy, 1999. */

__device void differential_transfer(differential3 *dP_, const differential3 dP, float3 D, const differential3 dD, float3 Ng, float t)
{
	/* ray differential transfer through homogenous medium, to
	 * compute dPdx/dy at a shading point from the incoming ray */

	float3 tmp = D/dot(D, Ng);
	float3 tmpx = dP.dx + t*dD.dx;
	float3 tmpy = dP.dy + t*dD.dy;

	dP_->dx = tmpx - dot(tmpx, Ng)*tmp;
	dP_->dy = tmpy - dot(tmpy, Ng)*tmp;
}

__device void differential_incoming(differential3 *dI, const differential3 dD)
{
	/* compute dIdx/dy at a shading point, we just need to negate the
	 * differential of the ray direction */

	dI->dx = -dD.dx;
	dI->dy = -dD.dy;
}

__device void differential_dudv(differential *du, differential *dv, float3 dPdu, float3 dPdv, differential3 dP, float3 Ng)
{
	/* now we have dPdx/dy from the ray differential transfer, and dPdu/dv
	 * from the primitive, we can compute dudx/dy and dvdx/dy. these are
	 * mainly used for differentials of arbitrary mesh attributes. */

	/* find most stable axis to project to 2D */
	float xn = fabsf(Ng.x);
	float yn = fabsf(Ng.y);
	float zn = fabsf(Ng.z);

	if(zn < xn || zn < yn) {
		if(yn < xn || yn < zn) {
			dPdu.x = dPdu.y;
			dPdv.x = dPdv.y;
			dP.dx.x = dP.dx.y;
			dP.dy.x = dP.dy.y;
		}

		dPdu.y = dPdu.z;
		dPdv.y = dPdv.z;
		dP.dx.y = dP.dx.z;
		dP.dy.y = dP.dy.z;
	}

	/* using Cramer's rule, we solve for dudx and dvdx in a 2x2 linear system,
	 * and the same for dudy and dvdy. the denominator is the same for both
	 * solutions, so we compute it only once.
	 *
	 * dP.dx = dPdu * dudx + dPdv * dvdx;
	 * dP.dy = dPdu * dudy + dPdv * dvdy; */

	float det = (dPdu.x*dPdv.y - dPdv.x*dPdu.y);

	if(det != 0.0f)
		det = 1.0f/det;

	du->dx = (dP.dx.x*dPdv.y - dP.dx.y*dPdv.x)*det;
	dv->dx = (dP.dx.y*dPdu.x - dP.dx.x*dPdu.y)*det;

	du->dy = (dP.dy.x*dPdv.y - dP.dy.y*dPdv.x)*det;
	dv->dy = (dP.dy.y*dPdu.x - dP.dy.x*dPdu.y)*det;
}

CCL_NAMESPACE_END


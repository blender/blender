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
#include "SM_FhObject.h"
#include "MT_MinMax.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SM_FhObject::SM_FhObject(DT_ShapeHandle rayshape, MT_Vector3 ray, SM_Object *parent_object) :
		SM_Object(rayshape, NULL, NULL, NULL),
		m_ray(ray),
		m_ray_direction(ray.normalized()),
		m_parent_object(parent_object) 
{
}

SM_FhObject::~SM_FhObject()
{
	DT_DeleteShape(getShapeHandle());
}

DT_Bool SM_FhObject::ray_hit(void *client_data,
						  void *client_object1,
						  void *client_object2,
						  const DT_CollData *coll_data) 
{

	SM_FhObject *fh_object  = dynamic_cast<SM_FhObject *>((SM_Object *)client_object2);
	if (!fh_object)
	{
		std::swap(client_object1, client_object2);
		fh_object  = dynamic_cast<SM_FhObject *>((SM_Object *)client_object2);
	}
	
	SM_Object   *hit_object = (SM_Object *)client_object1;
	const SM_MaterialProps *matProps = hit_object->getMaterialProps();
	
	if ((matProps == 0) || (matProps->m_fh_distance < MT_EPSILON)) {
		return DT_CONTINUE;
	}

	SM_Object           *cl_object  = fh_object->getParentObject();
	
	assert(fh_object);

	if (hit_object == cl_object) {
		// Shot myself in the foot...
		return DT_CONTINUE;
	}

	const SM_ShapeProps *shapeProps = cl_object->getShapeProps();

	// Exit if the client object is not dynamic.
	if (shapeProps == 0) {
		return DT_CONTINUE;
	}

	MT_Point3 lspot; 
	MT_Vector3 normal; 
	
	DT_Vector3 from, to, dnormal;
	DT_Scalar dlspot;
	fh_object->getPosition().getValue(from);
	fh_object->getSpot().getValue(to);
	
	
	if (DT_ObjectRayCast(hit_object->getObjectHandle(), 
						 from, 
						 to,
						 1.,
						 &dlspot, 
						 dnormal)) {
		
		lspot = fh_object->getPosition() + (fh_object->getSpot() - fh_object->getPosition()) * dlspot;
		const MT_Vector3& ray_dir = fh_object->getRayDirection();
		MT_Scalar dist = MT_distance(fh_object->getPosition(), lspot) - 
			cl_object->getMargin() - shapeProps->m_radius;

		normal = MT_Vector3(dnormal).safe_normalized();
		
		if (dist < matProps->m_fh_distance) {
			
			if (shapeProps->m_do_fh) {
				lspot -= hit_object->getPosition();
				MT_Vector3 rel_vel = cl_object->getLinearVelocity() - hit_object->getVelocity(lspot);
				MT_Scalar rel_vel_ray = ray_dir.dot(rel_vel);
				MT_Scalar spring_extent = 1.0 - dist / matProps->m_fh_distance; 
				
				MT_Scalar i_spring = spring_extent * matProps->m_fh_spring;
				MT_Scalar i_damp =   rel_vel_ray * matProps->m_fh_damping;
				
				cl_object->addLinearVelocity(-(i_spring + i_damp) * ray_dir); 
				if (matProps->m_fh_normal) {
					cl_object->addLinearVelocity(
						(i_spring + i_damp) *
						(normal - normal.dot(ray_dir) * ray_dir));
				}
				
				MT_Vector3 lateral = rel_vel - rel_vel_ray * ray_dir;
				const SM_ShapeProps *shapeProps = cl_object->getShapeProps();
				
				if (shapeProps->m_do_anisotropic) {
					MT_Matrix3x3 lcs(cl_object->getOrientation());
					MT_Vector3 loc_lateral = lateral * lcs;
					const MT_Vector3& friction_scaling = 
						shapeProps->m_friction_scaling; 
					
					loc_lateral.scale(friction_scaling[0], 
									  friction_scaling[1], 
									  friction_scaling[2]);
					lateral = lcs * loc_lateral;
				}
				

				MT_Scalar rel_vel_lateral = lateral.length();
				
				if (rel_vel_lateral > MT_EPSILON) {
					MT_Scalar friction_factor = matProps->m_friction;
					MT_Scalar max_friction = friction_factor * MT_max(MT_Scalar(0.0), i_spring);
					
					MT_Scalar rel_mom_lateral = rel_vel_lateral / 
						cl_object->getInvMass();
					
					MT_Vector3 friction =
						(rel_mom_lateral > max_friction) ?
						-lateral * (max_friction / rel_vel_lateral) :
						-lateral;
					
					cl_object->applyCenterImpulse(friction);
				}
			}
			
			if (shapeProps->m_do_rot_fh) {
				const double *ogl_mat = cl_object->getMatrix();
				MT_Vector3 up(&ogl_mat[8]);
				MT_Vector3 t_spring = up.cross(normal) * matProps->m_fh_spring;
				MT_Vector3 ang_vel = cl_object->getAngularVelocity();
				
				// only rotations that tilt relative to the normal are damped
				ang_vel -= ang_vel.dot(normal) * normal;
				
				MT_Vector3 t_damp = ang_vel * matProps->m_fh_damping;  
				
				cl_object->addAngularVelocity(t_spring - t_damp);
			}
		}
	}	
	
	return DT_CONTINUE;
}




#ifndef SM_FHOBJECT_H
#define SM_FHOBJECT_H

#include "SM_Object.h"

class SM_FhObject : public SM_Object {
public:
	SM_FhObject() {}
	SM_FhObject(const MT_Vector3& ray, SM_Object *client_object) :
		SM_Object(DT_Ray(ray[0], ray[1], ray[2]), 0, 0, 0),
		m_ray(ray),
		m_ray_direction(ray.normalized()),
		m_client_object(client_object) {}

	const MT_Vector3&  getRay()          const { return m_ray; }
	MT_Point3          getSpot()         const { return m_pos + m_ray; }
	const MT_Vector3&  getRayDirection() const { return m_ray_direction; }
	SM_Object         *getClientObject() const { return m_client_object; }

	static void ray_hit(void *client_data,  
		void *object1,
		void *object2,
		const DT_CollData *coll_data);

private:
	MT_Vector3      m_ray;
	MT_Vector3      m_ray_direction;
	SM_Object      *m_client_object;
};

#endif


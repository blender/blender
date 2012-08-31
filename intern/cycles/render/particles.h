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

#ifndef __PARTICLES_H__
#define __PARTICLES_H__

#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

/* Particle System */

struct Particle {
	int index;
	float age;
	float lifetime;
};

class ParticleSystem {
public:
	ParticleSystem();
	~ParticleSystem();

	void tag_update(Scene *scene);

	vector<Particle> particles;
};

/* ParticleSystem Manager */

class ParticleSystemManager {
public:
	bool need_update;

	ParticleSystemManager();
	~ParticleSystemManager();

	void device_update_particles(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __PARTICLES_H__ */


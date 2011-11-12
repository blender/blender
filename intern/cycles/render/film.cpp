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

#include "camera.h"
#include "device.h"
#include "film.h"
#include "scene.h"

CCL_NAMESPACE_BEGIN

Film::Film()
{
	exposure = 0.8f;
	need_update = true;
}

Film::~Film()
{
}

void Film::device_update(Device *device, DeviceScene *dscene)
{
	if(!need_update)
		return;

	KernelFilm *kfilm = &dscene->data.film;

	/* update __data */
	kfilm->exposure = exposure;

	need_update = false;
}

void Film::device_free(Device *device, DeviceScene *dscene)
{
}

bool Film::modified(const Film& film)
{
	return !(exposure == film.exposure);
}

void Film::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END


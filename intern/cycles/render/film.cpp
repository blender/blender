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
#include "integrator.h"
#include "scene.h"

#include "util_algorithm.h"
#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Pass */

static bool compare_pass_order(const Pass& a, const Pass& b)
{
	if(a.components == b.components)
		return (a.type < b.type);
	return (a.components > b.components);
}

void Pass::add(PassType type, vector<Pass>& passes)
{
	foreach(Pass& existing_pass, passes)
		if(existing_pass.type == type)
			return;

	Pass pass;

	pass.type = type;
	pass.filter = true;
	pass.exposure = false;
	pass.divide_type = PASS_NONE;

	switch(type) {
		case PASS_NONE:
			pass.components = 0;
			break;
		case PASS_COMBINED:
			pass.components = 4;
			pass.exposure = true;
			break;
		case PASS_DEPTH:
			pass.components = 1;
			pass.filter = false;
			break;
		case PASS_NORMAL:
			pass.components = 4;
			break;
		case PASS_UV:
			pass.components = 4;
			break;
		case PASS_MOTION:
			pass.components = 4;
			pass.divide_type = PASS_MOTION_WEIGHT;
			break;
		case PASS_MOTION_WEIGHT:
			pass.components = 1;
			break;
		case PASS_OBJECT_ID:
			pass.components = 1;
			pass.filter = false;
			break;
		case PASS_MATERIAL_ID:
			pass.components = 1;
			pass.filter = false;
			break;
		case PASS_DIFFUSE_COLOR:
			pass.components = 4;
			break;
		case PASS_GLOSSY_COLOR:
			pass.components = 4;
			break;
		case PASS_TRANSMISSION_COLOR:
			pass.components = 4;
			break;
		case PASS_DIFFUSE_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_DIFFUSE_COLOR;
			break;
		case PASS_GLOSSY_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_GLOSSY_COLOR;
			break;
		case PASS_TRANSMISSION_INDIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_TRANSMISSION_COLOR;
			break;
		case PASS_DIFFUSE_DIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_DIFFUSE_COLOR;
			break;
		case PASS_GLOSSY_DIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_GLOSSY_COLOR;
			break;
		case PASS_TRANSMISSION_DIRECT:
			pass.components = 4;
			pass.exposure = true;
			pass.divide_type = PASS_TRANSMISSION_COLOR;
			break;

		case PASS_EMISSION:
			pass.components = 4;
			pass.exposure = true;
			break;
		case PASS_BACKGROUND:
			pass.components = 4;
			pass.exposure = true;
			break;
		case PASS_AO:
			pass.components = 4;
			break;
		case PASS_SHADOW:
			pass.components = 4;
			pass.exposure = false;
			break;
	}

	passes.push_back(pass);

	/* order from by components, to ensure alignment so passes with size 4
	   come first and then passes with size 1 */
	sort(passes.begin(), passes.end(), compare_pass_order);

	if(pass.divide_type != PASS_NONE)
		Pass::add(pass.divide_type, passes);
}

bool Pass::equals(const vector<Pass>& A, const vector<Pass>& B)
{
	if(A.size() != B.size())
		return false;
	
	for(int i = 0; i < A.size(); i++)
		if(A[i].type != B[i].type)
			return false;
	
	return true;
}

bool Pass::contains(const vector<Pass>& passes, PassType type)
{
	foreach(const Pass& pass, passes)
		if(pass.type == type)
			return true;
	
	return false;
}

/* Film */

Film::Film()
{
	exposure = 0.8f;
	Pass::add(PASS_COMBINED, passes);
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
	kfilm->pass_flag = 0;
	kfilm->pass_stride = 0;
	kfilm->use_light_pass = 0;

	foreach(Pass& pass, passes) {
		kfilm->pass_flag |= pass.type;

		switch(pass.type) {
			case PASS_COMBINED:
				kfilm->pass_combined = kfilm->pass_stride;
				break;
			case PASS_DEPTH:
				kfilm->pass_depth = kfilm->pass_stride;
				break;
			case PASS_NORMAL:
				kfilm->pass_normal = kfilm->pass_stride;
				break;
			case PASS_UV:
				kfilm->pass_uv = kfilm->pass_stride;
				break;
			case PASS_MOTION:
				kfilm->pass_motion = kfilm->pass_stride;
				break;
			case PASS_MOTION_WEIGHT:
				kfilm->pass_motion_weight = kfilm->pass_stride;
				break;
			case PASS_OBJECT_ID:
				kfilm->pass_object_id = kfilm->pass_stride;
				break;
			case PASS_MATERIAL_ID:
				kfilm->pass_material_id = kfilm->pass_stride;
				break;
			case PASS_DIFFUSE_COLOR:
				kfilm->pass_diffuse_color = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_GLOSSY_COLOR:
				kfilm->pass_glossy_color = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_TRANSMISSION_COLOR:
				kfilm->pass_transmission_color = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_DIFFUSE_INDIRECT:
				kfilm->pass_diffuse_indirect = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_GLOSSY_INDIRECT:
				kfilm->pass_glossy_indirect = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_TRANSMISSION_INDIRECT:
				kfilm->pass_transmission_indirect = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_DIFFUSE_DIRECT:
				kfilm->pass_diffuse_direct = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_GLOSSY_DIRECT:
				kfilm->pass_glossy_direct = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_TRANSMISSION_DIRECT:
				kfilm->pass_transmission_direct = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;

			case PASS_EMISSION:
				kfilm->pass_emission = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
				break;
			case PASS_BACKGROUND:
				kfilm->pass_background = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
			case PASS_AO:
				kfilm->pass_ao = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
			case PASS_SHADOW:
				kfilm->pass_shadow = kfilm->pass_stride;
				kfilm->use_light_pass = 1;
			case PASS_NONE:
				break;
		}

		kfilm->pass_stride += pass.components;
	}

	kfilm->pass_stride = align_up(kfilm->pass_stride, 4);

	need_update = false;
}

void Film::device_free(Device *device, DeviceScene *dscene)
{
}

bool Film::modified(const Film& film)
{
	return !(exposure == film.exposure
		&& Pass::equals(passes, film.passes));
}

void Film::tag_update(Scene *scene)
{
	scene->integrator->tag_update(scene);
	need_update = true;
}

CCL_NAMESPACE_END


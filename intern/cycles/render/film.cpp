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
#include "film_response.h"
#include "scene.h"

CCL_NAMESPACE_BEGIN

static FilmResponseType find_response_type(const string& name)
{
	if(name == "" || name == "None")
		return FILM_RESPONSE_NONE;

	for(size_t i = 0; i < FILM_RESPONSE_NUM; i++) {
		FilmResponse *curve = &FILM_RESPONSE[i];
		if(curve->name == name)
			return (FilmResponseType)i;
	}

	return FILM_RESPONSE_NONE;
}

Film::Film()
{
	exposure = 0.8f;
	response = "Advantix 400";
	last_response = "";
	need_update = true;
}

Film::~Film()
{
}

#if 0
static void generate_python_enum()
{
	for(size_t i = 0; i < FILM_RESPONSE_NUM; i++) {
		FilmResponse *curve = &FILM_RESPONSE[i];
		/*if(i == 0 || strcmp(curve->brand, FILM_RESPONSE[i-1].brand))
			printf("(\"\", \"%s\", \"\"),\n", curve->brand);*/
		printf("(\"%s\", \"%s %s\", \"\"),\n", curve->name, curve->brand, curve->name);
	}
}
#endif

void Film::device_update(Device *device, DeviceScene *dscene)
{
	if(!need_update)
		return;

	KernelFilm *kfilm = &dscene->data.film;

	/* update __data */
	kfilm->exposure = exposure;

	FilmResponseType response_type = find_response_type(response);

	/* update __response_curves */
	if(response != last_response)  {
		device_free(device, dscene);

		if(response_type != FILM_RESPONSE_NONE)  {
			FilmResponse *curve = &FILM_RESPONSE[response_type];
			size_t response_curve_size = FILM_RESPONSE_SIZE;

			dscene->response_curve_R.copy(curve->B_R, response_curve_size);

			if(curve->rgb) {
				dscene->response_curve_G.copy(curve->B_G, response_curve_size);
				dscene->response_curve_B.copy(curve->B_B, response_curve_size);
			}
			else {
				dscene->response_curve_G.copy(curve->B_R, response_curve_size);
				dscene->response_curve_B.copy(curve->B_R, response_curve_size);
			}

			device->tex_alloc("__response_curve_R", dscene->response_curve_R, true);
			device->tex_alloc("__response_curve_G", dscene->response_curve_G, true);
			device->tex_alloc("__response_curve_B", dscene->response_curve_B, true);
		}

		last_response = response;
	}

	kfilm->use_response_curve = (response_type != FILM_RESPONSE_NONE);

	need_update = false;
}

void Film::device_free(Device *device, DeviceScene *dscene)
{
	device->tex_free(dscene->response_curve_R);
	device->tex_free(dscene->response_curve_G);
	device->tex_free(dscene->response_curve_B);

	dscene->response_curve_R.clear();
	dscene->response_curve_G.clear();
	dscene->response_curve_B.clear();

	last_response = "";
}

bool Film::modified(const Film& film)
{
	return !(response == film.response &&
		exposure == film.exposure &&
		pass == film.pass);
}

void Film::tag_update(Scene *scene)
{
	need_update = true;
}

CCL_NAMESPACE_END


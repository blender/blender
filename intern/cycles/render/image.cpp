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

#include "device.h"
#include "image.h"
#include "scene.h"

#include "util_foreach.h"
#include "util_image.h"
#include "util_path.h"
#include "util_progress.h"

#ifdef WITH_OSL
#include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

ImageManager::ImageManager()
{
	need_update = true;
	osl_texture_system = NULL;
}

ImageManager::~ImageManager()
{
	for(size_t slot = 0; slot < images.size(); slot++) {
		assert(!images[slot]);
	}
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
	osl_texture_system = texture_system;
}

int ImageManager::add_image(const string& filename)
{
	Image *img;
	size_t slot;

	/* find existing image */
	for(slot = 0; slot < images.size(); slot++) {
		if(images[slot] && images[slot]->filename == filename) {
			images[slot]->users++;
			return slot;
		}
	}

	/* find free slot */
	for(slot = 0; slot < images.size(); slot++)
		if(!images[slot])
			break;
	
	if(slot == images.size()) {
		/* max images limit reached */
		if(images.size() == TEX_IMAGE_MAX)
			return -1;

		images.resize(images.size() + 1);
	}
	
	/* add new image */
	img = new Image();
	img->filename = filename;
	img->need_load = true;
	img->users = 1;

	images[slot] = img;
	need_update = true;

	return slot;
}

void ImageManager::remove_image(const string& filename)
{
	size_t slot;

	for(slot = 0; slot < images.size(); slot++)
		if(images[slot] && images[slot]->filename == filename)
			break;
	
	if(slot == images.size())
		return;

	assert(images[slot]);

	/* decrement user count */
	images[slot]->users--;
	assert(images[slot]->users >= 0);
	
	/* don't remove immediately, rather do it all together later on. one of
	   the reasons for this is that on shader changes we add and remove nodes
	   that use them, but we do not want to reload the image all the time. */
	if(images[slot]->users == 0)
		need_update = true;
}

bool ImageManager::file_load_image(Image *img, device_vector<uchar4>& tex_img)
{
	if(img->filename == "")
		return false;

	/* load image from file through OIIO */
	ImageInput *in = ImageInput::create(img->filename);

	if(!in)
		return false;

	ImageSpec spec;

	if(!in->open(img->filename, spec)) {
		delete in;
		return false;
	}

	/* we only handle certain number of components */
	int width = spec.width;
	int height = spec.height;
	int components = spec.nchannels;

	if(!(components == 1 || components == 3 || components == 4)) {
		in->close();
		delete in;
		return false;
	}

	/* read RGBA pixels */
	uchar *pixels = (uchar*)tex_img.resize(width, height);
	int scanlinesize = width*components*sizeof(uchar);

	in->read_image(TypeDesc::UINT8,
		(uchar*)pixels + (height-1)*scanlinesize,
		AutoStride,
		-scanlinesize,
		AutoStride);

	in->close();
	delete in;

	if(components == 3) {
		for(int i = width*height-1; i >= 0; i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = pixels[i*3+2];
			pixels[i*4+1] = pixels[i*3+1];
			pixels[i*4+0] = pixels[i*3+0];
		}
	}
	else if(components == 1) {
		for(int i = width*height-1; i >= 0; i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = pixels[i];
			pixels[i*4+1] = pixels[i];
			pixels[i*4+0] = pixels[i];
		}
	}

	return true;
}

void ImageManager::device_load_image(Device *device, DeviceScene *dscene, int slot)
{
	if(osl_texture_system)
		return;

	Image *img = images[slot];
	device_vector<uchar4>& tex_img = dscene->tex_image[slot];

	if(tex_img.device_pointer)
		device->tex_free(tex_img);

	if(!file_load_image(img, tex_img)) {
		/* on failure to load, we set a 1x1 pixels black image */
		uchar *pixels = (uchar*)tex_img.resize(1, 1);

		pixels[0] = 0;
		pixels[1] = 0;
		pixels[2] = 0;
		pixels[3] = 0;
	}

	string name;

	if(slot >= 10) name = string_printf("__tex_image_0%d", slot);
	else name = string_printf("__tex_image_00%d", slot);

	device->tex_alloc(name.c_str(), tex_img, true, true);
}

void ImageManager::device_free_image(Device *device, DeviceScene *dscene, int slot)
{
	if(images[slot]) {
		if(osl_texture_system) {
#ifdef WITH_OSL
			ustring filename(images[slot]->filename);
			((OSL::TextureSystem*)osl_texture_system)->invalidate(filename);
#endif
		}
		else {
			device->tex_free(dscene->tex_image[slot]);
			dscene->tex_image[slot].clear();
		}

		delete images[slot];
		images[slot] = NULL;
	}
}

void ImageManager::device_update(Device *device, DeviceScene *dscene, Progress& progress)
{
	if(!need_update)
		return;

	for(size_t slot = 0; slot < images.size(); slot++) {
		if(images[slot]) {
			if(images[slot]->users == 0) {
				device_free_image(device, dscene, slot);
			}
			else if(images[slot]->need_load) {
				string name = path_filename(images[slot]->filename);
				progress.set_status("Updating Images", "Loading " + name);
				device_load_image(device, dscene, slot);
				images[slot]->need_load = false;
			}

			if(progress.get_cancel()) return;
		}
	}

	need_update = false;
}

void ImageManager::device_free(Device *device, DeviceScene *dscene)
{
	for(size_t slot = 0; slot < images.size(); slot++)
		device_free_image(device, dscene, slot);

	images.clear();
}

CCL_NAMESPACE_END


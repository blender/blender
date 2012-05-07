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
	for(size_t slot = 0; slot < images.size(); slot++)
		assert(!images[slot]);
	for(size_t slot = 0; slot < float_images.size(); slot++)
		assert(!float_images[slot]);
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
	osl_texture_system = texture_system;
}

static bool is_float_image(const string& filename)
{
	ImageInput *in = ImageInput::create(filename);
	bool is_float = false;

	if(in) {
		ImageSpec spec;

		if(in->open(filename, spec)) {
			/* check the main format, and channel formats;
			   if any are non-integer, we'll need a float texture slot */
			if(spec.format == TypeDesc::HALF ||
			   spec.format == TypeDesc::FLOAT ||
			   spec.format == TypeDesc::DOUBLE) {
				is_float = true;
			}

			for(size_t channel = 0; channel < spec.channelformats.size(); channel++) {
				if(spec.channelformats[channel] == TypeDesc::HALF ||
				   spec.channelformats[channel] == TypeDesc::FLOAT ||
				   spec.channelformats[channel] == TypeDesc::DOUBLE) {
					is_float = true;
				}
			}

			in->close();
		}

		delete in;
	}

	return is_float;
}

int ImageManager::add_image(const string& filename, bool& is_float)
{
	Image *img;
	size_t slot;

	/* load image info and find out if we need a float texture */
	is_float = is_float_image(filename);

	if(is_float) {
		/* find existing image */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(float_images[slot] && float_images[slot]->filename == filename) {
				float_images[slot]->users++;
				return slot+TEX_IMAGE_FLOAT_START;
			}
		}

		/* find free slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(!float_images[slot])
				break;
		}

		if(slot == float_images.size()) {
			/* max images limit reached */
			if(float_images.size() == TEX_NUM_FLOAT_IMAGES)
				return -1;

			float_images.resize(float_images.size() + 1);
		}

		/* add new image */
		img = new Image();
		img->filename = filename;
		img->need_load = true;
		img->users = 1;

		float_images[slot] = img;
		/* report slot out of total set of textures */
		slot += TEX_IMAGE_FLOAT_START;
	}
	else {
		for(slot = 0; slot < images.size(); slot++) {
			if(images[slot] && images[slot]->filename == filename) {
				images[slot]->users++;
				return slot;
			}
		}

		/* find free slot */
		for(slot = 0; slot < images.size(); slot++) {
			if(!images[slot])
				break;
		}

		if(slot == images.size()) {
			/* max images limit reached */
			if(images.size() == TEX_NUM_IMAGES)
				return -1;

			images.resize(images.size() + 1);
		}

		/* add new image */
		img = new Image();
		img->filename = filename;
		img->need_load = true;
		img->users = 1;

		images[slot] = img;
	}
	need_update = true;

	return slot;
}

void ImageManager::remove_image(const string& filename)
{
	size_t slot;

	for(slot = 0; slot < images.size(); slot++) {
		if(images[slot] && images[slot]->filename == filename) {
			/* decrement user count */
			images[slot]->users--;
			assert(images[slot]->users >= 0);

			/* don't remove immediately, rather do it all together later on. one of
			   the reasons for this is that on shader changes we add and remove nodes
			   that use them, but we do not want to reload the image all the time. */
			if(images[slot]->users == 0)
				need_update = true;

			break;
		}
	}

	if(slot == images.size()) {
		/* see if it's in a float texture slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(float_images[slot] && float_images[slot]->filename == filename) {
				/* decrement user count */
				float_images[slot]->users--;
				assert(float_images[slot]->users >= 0);

				/* don't remove immediately, rather do it all together later on. one of
				   the reasons for this is that on shader changes we add and remove nodes
				   that use them, but we do not want to reload the image all the time. */
				if(float_images[slot]->users == 0)
					need_update = true;

				break;
			}
		}
	}
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

bool ImageManager::file_load_float_image(Image *img, device_vector<float4>& tex_img)
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
	float *pixels = (float*)tex_img.resize(width, height);
	int scanlinesize = width*components*sizeof(float);

	in->read_image(TypeDesc::FLOAT,
		(uchar*)pixels + (height-1)*scanlinesize,
		AutoStride,
		-scanlinesize,
		AutoStride);

	in->close();
	delete in;

	if(components == 3) {
		for(int i = width*height-1; i >= 0; i--) {
			pixels[i*4+3] = 1.0f;
			pixels[i*4+2] = pixels[i*3+2];
			pixels[i*4+1] = pixels[i*3+1];
			pixels[i*4+0] = pixels[i*3+0];
		}
	}
	else if(components == 1) {
		for(int i = width*height-1; i >= 0; i--) {
			pixels[i*4+3] = 1.0f;
			pixels[i*4+2] = pixels[i];
			pixels[i*4+1] = pixels[i];
			pixels[i*4+0] = pixels[i];
		}
	}

	return true;
}

void ImageManager::device_load_image(Device *device, DeviceScene *dscene, int slot, Progress *progress)
{
	if(progress->get_cancel())
		return;
	if(osl_texture_system)
		return;

	Image *img;
	bool is_float;

	if(slot < TEX_IMAGE_FLOAT_START) {
		img = images[slot];
		is_float = false;
	}
	else {
		img = float_images[slot - TEX_IMAGE_FLOAT_START];
		is_float = true;
	}

	if(is_float) {
		string filename = path_filename(float_images[slot - TEX_IMAGE_FLOAT_START]->filename);
		progress->set_status("Updating Images", "Loading " + filename);

		device_vector<float4>& tex_img = dscene->tex_float_image[slot - TEX_IMAGE_FLOAT_START];

		if(tex_img.device_pointer)
			device->tex_free(tex_img);

		if(!file_load_float_image(img, tex_img)) {
			/* on failure to load, we set a 1x1 pixels black image */
			float *pixels = (float*)tex_img.resize(1, 1);

			pixels[0] = 0.0f;
			pixels[1] = 0.0f;
			pixels[2] = 0.0f;
			pixels[3] = 0.0f;
		}

		string name;

		if(slot >= 10) name = string_printf("__tex_image_float_0%d", slot);
		else name = string_printf("__tex_image_float_00%d", slot);

		device->tex_alloc(name.c_str(), tex_img, true, true);
	}
	else {
		string filename = path_filename(images[slot]->filename);
		progress->set_status("Updating Images", "Loading " + filename);

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

	img->need_load = false;
}

void ImageManager::device_free_image(Device *device, DeviceScene *dscene, int slot)
{
	Image *img;
	bool is_float;

	if(slot < TEX_IMAGE_FLOAT_START) {
		img = images[slot];
		is_float = false;
	}
	else {
		img = float_images[slot - TEX_IMAGE_FLOAT_START];
		is_float = true;
	}

	if(img) {
		if(osl_texture_system) {
#ifdef WITH_OSL
			ustring filename(images[slot]->filename);
			((OSL::TextureSystem*)osl_texture_system)->invalidate(filename);
#endif
		}
		else if(is_float) {
			device->tex_free(dscene->tex_float_image[slot - TEX_IMAGE_FLOAT_START]);
			dscene->tex_float_image[slot - TEX_IMAGE_FLOAT_START].clear();

			delete float_images[slot - TEX_IMAGE_FLOAT_START];
			float_images[slot - TEX_IMAGE_FLOAT_START] = NULL;
		}
		else {
			device->tex_free(dscene->tex_image[slot]);
			dscene->tex_image[slot].clear();

			delete images[slot];
			images[slot] = NULL;
		}
	}
}

void ImageManager::device_update(Device *device, DeviceScene *dscene, Progress& progress)
{
	if(!need_update)
		return;
	
	TaskPool pool;

	for(size_t slot = 0; slot < images.size(); slot++) {
		if(!images[slot])
			continue;

		if(images[slot]->users == 0) {
			device_free_image(device, dscene, slot);
		}
		else if(images[slot]->need_load) {
			if(!osl_texture_system) 
				pool.push(function_bind(&ImageManager::device_load_image, this, device, dscene, slot, &progress));
		}
	}

	for(size_t slot = 0; slot < float_images.size(); slot++) {
		if(!float_images[slot])
			continue;

		if(float_images[slot]->users == 0) {
			device_free_image(device, dscene, slot + TEX_IMAGE_FLOAT_START);
		}
		else if(float_images[slot]->need_load) {
			if(!osl_texture_system) 
				pool.push(function_bind(&ImageManager::device_load_image, this, device, dscene, slot + TEX_IMAGE_FLOAT_START, &progress));
		}
	}

	pool.wait_work();

	need_update = false;
}

void ImageManager::device_free(Device *device, DeviceScene *dscene)
{
	for(size_t slot = 0; slot < images.size(); slot++)
		device_free_image(device, dscene, slot);
	for(size_t slot = 0; slot < float_images.size(); slot++)
		device_free_image(device, dscene, slot + TEX_IMAGE_FLOAT_START);

	images.clear();
	float_images.clear();
}

CCL_NAMESPACE_END


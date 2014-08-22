/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
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
	pack_images = false;
	osl_texture_system = NULL;
	animation_frame = 0;

	tex_num_images = TEX_NUM_IMAGES;
	tex_num_float_images = TEX_NUM_FLOAT_IMAGES;
	tex_image_byte_start = TEX_IMAGE_BYTE_START;
}

ImageManager::~ImageManager()
{
	for(size_t slot = 0; slot < images.size(); slot++)
		assert(!images[slot]);
	for(size_t slot = 0; slot < float_images.size(); slot++)
		assert(!float_images[slot]);
}

void ImageManager::set_pack_images(bool pack_images_)
{
	pack_images = pack_images_;
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
	osl_texture_system = texture_system;
}

void ImageManager::set_extended_image_limits(const DeviceInfo& info)
{
	if(info.type == DEVICE_CPU) {
		tex_num_images = TEX_EXTENDED_NUM_IMAGES_CPU;
		tex_num_float_images = TEX_EXTENDED_NUM_FLOAT_IMAGES;
		tex_image_byte_start = TEX_EXTENDED_IMAGE_BYTE_START;
	}
	else if((info.type == DEVICE_CUDA || info.type == DEVICE_MULTI) && info.extended_images) {
		tex_num_images = TEX_EXTENDED_NUM_IMAGES_GPU;
	}
}

bool ImageManager::set_animation_frame_update(int frame)
{
	if(frame != animation_frame) {
		animation_frame = frame;

		for(size_t slot = 0; slot < images.size(); slot++)
			if(images[slot] && images[slot]->animated)
				return true;

		for(size_t slot = 0; slot < float_images.size(); slot++)
			if(float_images[slot] && float_images[slot]->animated)
				return true;
	}
	
	return false;
}

bool ImageManager::is_float_image(const string& filename, void *builtin_data, bool& is_linear)
{
	bool is_float = false;
	is_linear = false;

	if(builtin_data) {
		if(builtin_image_info_cb) {
			int width, height, depth, channels;
			builtin_image_info_cb(filename, builtin_data, is_float, width, height, depth, channels);
		}

		if(is_float)
			is_linear = true;

		return is_float;
	}

	ImageInput *in = ImageInput::create(filename);

	if(in) {
		ImageSpec spec;

		if(in->open(filename, spec)) {
			/* check the main format, and channel formats;
			 * if any take up more than one byte, we'll need a float texture slot */
			if(spec.format.basesize() > 1) {
				is_float = true;
				is_linear = true;
			}

			for(size_t channel = 0; channel < spec.channelformats.size(); channel++) {
				if(spec.channelformats[channel].basesize() > 1) {
					is_float = true;
					is_linear = true;
				}
			}

			/* basic color space detection, not great but better than nothing
			 * before we do OpenColorIO integration */
			if(is_float) {
				string colorspace = spec.get_string_attribute("oiio:ColorSpace");

				is_linear = !(colorspace == "sRGB" ||
				              colorspace == "GammaCorrected" ||
				              (colorspace == "" &&
				                  (strcmp(in->format_name(), "png") == 0 ||
				                   strcmp(in->format_name(), "tiff") == 0 ||
				                   strcmp(in->format_name(), "jpeg2000") == 0)));
			}
			else {
				is_linear = false;
			}

			in->close();
		}

		delete in;
	}

	return is_float;
}

static bool image_equals(ImageManager::Image *image, const string& filename, void *builtin_data, InterpolationType interpolation)
{
	return image->filename == filename &&
	       image->builtin_data == builtin_data &&
	       image->interpolation == interpolation;
}

int ImageManager::add_image(const string& filename, void *builtin_data, bool animated, float frame,
	bool& is_float, bool& is_linear, InterpolationType interpolation, bool use_alpha)
{
	Image *img;
	size_t slot;

	/* load image info and find out if we need a float texture */
	is_float = (pack_images)? false: is_float_image(filename, builtin_data, is_linear);

	if(is_float) {
		/* find existing image */
		for(slot = 0; slot < float_images.size(); slot++) {
			img = float_images[slot];
			if(img && image_equals(img, filename, builtin_data, interpolation)) {
				if(img->frame != frame) {
					img->frame = frame;
					img->need_load = true;
				}
				img->users++;
				return slot;
			}
		}

		/* find free slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(!float_images[slot])
				break;
		}

		if(slot == float_images.size()) {
			/* max images limit reached */
			if(float_images.size() == tex_num_float_images) {
				printf("ImageManager::add_image: float image limit reached %d, skipping '%s'\n",
				       tex_num_float_images, filename.c_str());
				return -1;
			}

			float_images.resize(float_images.size() + 1);
		}

		/* add new image */
		img = new Image();
		img->filename = filename;
		img->builtin_data = builtin_data;
		img->need_load = true;
		img->animated = animated;
		img->frame = frame;
		img->interpolation = interpolation;
		img->users = 1;
		img->use_alpha = use_alpha;

		float_images[slot] = img;
	}
	else {
		for(slot = 0; slot < images.size(); slot++) {
			img = images[slot];
			if(img && image_equals(img, filename, builtin_data, interpolation)) {
				if(img->frame != frame) {
					img->frame = frame;
					img->need_load = true;
				}
				img->users++;
				return slot+tex_image_byte_start;
			}
		}

		/* find free slot */
		for(slot = 0; slot < images.size(); slot++) {
			if(!images[slot])
				break;
		}

		if(slot == images.size()) {
			/* max images limit reached */
			if(images.size() == tex_num_images) {
				printf("ImageManager::add_image: byte image limit reached %d, skipping '%s'\n",
				       tex_num_images, filename.c_str());
				return -1;
			}

			images.resize(images.size() + 1);
		}

		/* add new image */
		img = new Image();
		img->filename = filename;
		img->builtin_data = builtin_data;
		img->need_load = true;
		img->animated = animated;
		img->frame = frame;
		img->interpolation = interpolation;
		img->users = 1;
		img->use_alpha = use_alpha;

		images[slot] = img;

		slot += tex_image_byte_start;
	}

	need_update = true;

	return slot;
}

void ImageManager::remove_image(int slot)
{
	if(slot >= tex_image_byte_start) {
		slot -= tex_image_byte_start;

		assert(images[slot] != NULL);

		/* decrement user count */
		images[slot]->users--;
		assert(images[slot]->users >= 0);

		/* don't remove immediately, rather do it all together later on. one of
		 * the reasons for this is that on shader changes we add and remove nodes
		 * that use them, but we do not want to reload the image all the time. */
		if(images[slot]->users == 0)
			need_update = true;
	}
	else {
		/* decrement user count */
		float_images[slot]->users--;
		assert(float_images[slot]->users >= 0);

		/* don't remove immediately, rather do it all together later on. one of
		 * the reasons for this is that on shader changes we add and remove nodes
		 * that use them, but we do not want to reload the image all the time. */
		if(float_images[slot]->users == 0)
			need_update = true;
	}
}

void ImageManager::remove_image(const string& filename, void *builtin_data, InterpolationType interpolation)
{
	size_t slot;

	for(slot = 0; slot < images.size(); slot++) {
		if(images[slot] && image_equals(images[slot], filename, builtin_data, interpolation)) {
			remove_image(slot+tex_image_byte_start);
			break;
		}
	}

	if(slot == images.size()) {
		/* see if it's in a float texture slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(float_images[slot] && image_equals(float_images[slot], filename, builtin_data, interpolation)) {
				remove_image(slot);
				break;
			}
		}
	}
}

/* TODO(sergey): Deduplicate with the iteration above, but make it pretty,
 * without bunch of arguments passing around making code readability even
 * more cluttered.
 */
void ImageManager::tag_reload_image(const string& filename, void *builtin_data, InterpolationType interpolation)
{
	size_t slot;

	for(slot = 0; slot < images.size(); slot++) {
		if(images[slot] && image_equals(images[slot], filename, builtin_data, interpolation)) {
			images[slot]->need_load = true;
			break;
		}
	}

	if(slot == images.size()) {
		/* see if it's in a float texture slot */
		for(slot = 0; slot < float_images.size(); slot++) {
			if(float_images[slot] && image_equals(float_images[slot], filename, builtin_data, interpolation)) {
				float_images[slot]->need_load = true;
				break;
			}
		}
	}
}

bool ImageManager::file_load_image(Image *img, device_vector<uchar4>& tex_img)
{
	if(img->filename == "")
		return false;

	ImageInput *in = NULL;
	int width, height, depth, components;

	if(!img->builtin_data) {
		/* load image from file through OIIO */
		in = ImageInput::create(img->filename);

		if(!in)
			return false;

		ImageSpec spec = ImageSpec();
		ImageSpec config = ImageSpec();

		if(img->use_alpha == false)
			config.attribute("oiio:UnassociatedAlpha", 1);

		if(!in->open(img->filename, spec, config)) {
			delete in;
			return false;
		}

		width = spec.width;
		height = spec.height;
		depth = spec.depth;
		components = spec.nchannels;
	}
	else {
		/* load image using builtin images callbacks */
		if(!builtin_image_info_cb || !builtin_image_pixels_cb)
			return false;

		bool is_float;
		builtin_image_info_cb(img->filename, img->builtin_data, is_float, width, height, depth, components);
	}

	/* we only handle certain number of components */
	if(!(components >= 1 && components <= 4)) {
		if(in) {
			in->close();
			delete in;
		}

		return false;
	}

	/* read RGBA pixels */
	uchar *pixels = (uchar*)tex_img.resize(width, height, depth);
	bool cmyk = false;

	if(in) {
		if(depth <= 1) {
			int scanlinesize = width*components*sizeof(uchar);

			in->read_image(TypeDesc::UINT8,
				(uchar*)pixels + (height-1)*scanlinesize,
				AutoStride,
				-scanlinesize,
				AutoStride);
		}
		else {
			in->read_image(TypeDesc::UINT8, (uchar*)pixels);
		}

		cmyk = strcmp(in->format_name(), "jpeg") == 0 && components == 4;

		in->close();
		delete in;
	}
	else {
		builtin_image_pixels_cb(img->filename, img->builtin_data, pixels);
	}

	if(cmyk) {
		/* CMYK */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
			pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
			pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
			pixels[i*4+3] = 255;
		}
	}
	else if(components == 2) {
		/* grayscale + alpha */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = pixels[i*2+1];
			pixels[i*4+2] = pixels[i*2+0];
			pixels[i*4+1] = pixels[i*2+0];
			pixels[i*4+0] = pixels[i*2+0];
		}
	}
	else if(components == 3) {
		/* RGB */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = pixels[i*3+2];
			pixels[i*4+1] = pixels[i*3+1];
			pixels[i*4+0] = pixels[i*3+0];
		}
	}
	else if(components == 1) {
		/* grayscale */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = pixels[i];
			pixels[i*4+1] = pixels[i];
			pixels[i*4+0] = pixels[i];
		}
	}

	if(img->use_alpha == false) {
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = 255;
		}
	}

	return true;
}

bool ImageManager::file_load_float_image(Image *img, device_vector<float4>& tex_img)
{
	if(img->filename == "")
		return false;

	ImageInput *in = NULL;
	int width, height, depth, components;

	if(!img->builtin_data) {
		/* load image from file through OIIO */
		in = ImageInput::create(img->filename);

		if(!in)
			return false;

		ImageSpec spec = ImageSpec();
		ImageSpec config = ImageSpec();

		if(img->use_alpha == false)
			config.attribute("oiio:UnassociatedAlpha",1);

		if(!in->open(img->filename, spec, config)) {
			delete in;
			return false;
		}

		/* we only handle certain number of components */
		width = spec.width;
		height = spec.height;
		depth = spec.depth;
		components = spec.nchannels;
	}
	else {
		/* load image using builtin images callbacks */
		if(!builtin_image_info_cb || !builtin_image_float_pixels_cb)
			return false;

		bool is_float;
		builtin_image_info_cb(img->filename, img->builtin_data, is_float, width, height, depth, components);
	}

	if(components < 1 || width == 0 || height == 0) {
		if(in) {
			in->close();
			delete in;
		}
		return false;
	}

	/* read RGBA pixels */
	float *pixels = (float*)tex_img.resize(width, height, depth);
	bool cmyk = false;

	if(in) {
		float *readpixels = pixels;
		vector<float> tmppixels;

		if(components > 4) {
			tmppixels.resize(width*height*components);
			readpixels = &tmppixels[0];
		}

		if(depth <= 1) {
			int scanlinesize = width*components*sizeof(float);

			in->read_image(TypeDesc::FLOAT,
				(uchar*)readpixels + (height-1)*scanlinesize,
				AutoStride,
				-scanlinesize,
				AutoStride);
		}
		else {
			in->read_image(TypeDesc::FLOAT, (uchar*)readpixels);
		}

		if(components > 4) {
			for(int i = width*height-1; i >= 0; i--) {
				pixels[i*4+3] = tmppixels[i*components+3];
				pixels[i*4+2] = tmppixels[i*components+2];
				pixels[i*4+1] = tmppixels[i*components+1];
				pixels[i*4+0] = tmppixels[i*components+0];
			}

			tmppixels.clear();
		}

		cmyk = strcmp(in->format_name(), "jpeg") == 0 && components == 4;

		in->close();
		delete in;
	}
	else {
		builtin_image_float_pixels_cb(img->filename, img->builtin_data, pixels);
	}

	if(cmyk) {
		/* CMYK */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = 255;
			pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
			pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
			pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
		}
	}
	else if(components == 2) {
		/* grayscale + alpha */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = pixels[i*2+1];
			pixels[i*4+2] = pixels[i*2+0];
			pixels[i*4+1] = pixels[i*2+0];
			pixels[i*4+0] = pixels[i*2+0];
		}
	}
	else if(components == 3) {
		/* RGB */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = 1.0f;
			pixels[i*4+2] = pixels[i*3+2];
			pixels[i*4+1] = pixels[i*3+1];
			pixels[i*4+0] = pixels[i*3+0];
		}
	}
	else if(components == 1) {
		/* grayscale */
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = 1.0f;
			pixels[i*4+2] = pixels[i];
			pixels[i*4+1] = pixels[i];
			pixels[i*4+0] = pixels[i];
		}
	}

	if(img->use_alpha == false) {
		for(int i = width*height*depth-1; i >= 0; i--) {
			pixels[i*4+3] = 1.0f;
		}
	}

	return true;
}

void ImageManager::device_load_image(Device *device, DeviceScene *dscene, int slot, Progress *progress)
{
	if(progress->get_cancel())
		return;
	
	Image *img;
	bool is_float;

	if(slot >= tex_image_byte_start) {
		img = images[slot - tex_image_byte_start];
		is_float = false;
	}
	else {
		img = float_images[slot];
		is_float = true;
	}

	if(osl_texture_system && !img->builtin_data)
		return;

	if(is_float) {
		string filename = path_filename(float_images[slot]->filename);
		progress->set_status("Updating Images", "Loading " + filename);

		device_vector<float4>& tex_img = dscene->tex_float_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_float_image(img, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			float *pixels = (float*)tex_img.resize(1, 1);

			pixels[0] = TEX_IMAGE_MISSING_R;
			pixels[1] = TEX_IMAGE_MISSING_G;
			pixels[2] = TEX_IMAGE_MISSING_B;
			pixels[3] = TEX_IMAGE_MISSING_A;
		}

		string name;

		if(slot >= 100) name = string_printf("__tex_image_float_%d", slot);
		else if(slot >= 10) name = string_printf("__tex_image_float_0%d", slot);
		else name = string_printf("__tex_image_float_00%d", slot);

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(), tex_img, img->interpolation, true);
		}
	}
	else {
		string filename = path_filename(images[slot - tex_image_byte_start]->filename);
		progress->set_status("Updating Images", "Loading " + filename);

		device_vector<uchar4>& tex_img = dscene->tex_image[slot - tex_image_byte_start];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image(img, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			uchar *pixels = (uchar*)tex_img.resize(1, 1);

			pixels[0] = (TEX_IMAGE_MISSING_R * 255);
			pixels[1] = (TEX_IMAGE_MISSING_G * 255);
			pixels[2] = (TEX_IMAGE_MISSING_B * 255);
			pixels[3] = (TEX_IMAGE_MISSING_A * 255);
		}

		string name;

		if(slot >= 100) name = string_printf("__tex_image_%d", slot);
		else if(slot >= 10) name = string_printf("__tex_image_0%d", slot);
		else name = string_printf("__tex_image_00%d", slot);

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(), tex_img, img->interpolation, true);
		}
	}

	img->need_load = false;
}

void ImageManager::device_free_image(Device *device, DeviceScene *dscene, int slot)
{
	Image *img;
	bool is_float;

	if(slot >= tex_image_byte_start) {
		img = images[slot - tex_image_byte_start];
		is_float = false;
	}
	else {
		img = float_images[slot];
		is_float = true;
	}

	if(img) {
		if(osl_texture_system && !img->builtin_data) {
#ifdef WITH_OSL
			ustring filename(images[slot]->filename);
			((OSL::TextureSystem*)osl_texture_system)->invalidate(filename);
#endif
		}
		else if(is_float) {
			device_vector<float4>& tex_img = dscene->tex_float_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();

			delete float_images[slot];
			float_images[slot] = NULL;
		}
		else {
			device_vector<uchar4>& tex_img = dscene->tex_image[slot - tex_image_byte_start];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();

			delete images[slot - tex_image_byte_start];
			images[slot - tex_image_byte_start] = NULL;
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
			device_free_image(device, dscene, slot + tex_image_byte_start);
		}
		else if(images[slot]->need_load) {
			if(!osl_texture_system || images[slot]->builtin_data) 
				pool.push(function_bind(&ImageManager::device_load_image, this, device, dscene, slot + tex_image_byte_start, &progress));
		}
	}

	for(size_t slot = 0; slot < float_images.size(); slot++) {
		if(!float_images[slot])
			continue;

		if(float_images[slot]->users == 0) {
			device_free_image(device, dscene, slot);
		}
		else if(float_images[slot]->need_load) {
			if(!osl_texture_system || float_images[slot]->builtin_data) 
				pool.push(function_bind(&ImageManager::device_load_image, this, device, dscene, slot, &progress));
		}
	}

	pool.wait_work();

	if(pack_images)
		device_pack_images(device, dscene, progress);

	need_update = false;
}

void ImageManager::device_pack_images(Device *device, DeviceScene *dscene, Progress& progess)
{
	/* for OpenCL, we pack all image textures inside a single big texture, and
	 * will do our own interpolation in the kernel */
	size_t size = 0;

	for(size_t slot = 0; slot < images.size(); slot++) {
		if(!images[slot])
			continue;

		device_vector<uchar4>& tex_img = dscene->tex_image[slot];
		size += tex_img.size();
	}

	uint4 *info = dscene->tex_image_packed_info.resize(images.size());
	uchar4 *pixels = dscene->tex_image_packed.resize(size);

	size_t offset = 0;

	for(size_t slot = 0; slot < images.size(); slot++) {
		if(!images[slot])
			continue;

		device_vector<uchar4>& tex_img = dscene->tex_image[slot];

		/* todo: support 3D textures, only CPU for now */

		/* The image options are packed
		   bit 0 -> periodic
		   bit 1 + 2 -> interpolation type */
		uint8_t interpolation = (images[slot]->interpolation << 1) + 1;
		info[slot] = make_uint4(tex_img.data_width, tex_img.data_height, offset, interpolation);

		memcpy(pixels+offset, (void*)tex_img.data_pointer, tex_img.memory_size());
		offset += tex_img.size();
	}

	if(dscene->tex_image_packed.size()) {
		if(dscene->tex_image_packed.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_packed);
		}
		device->tex_alloc("__tex_image_packed", dscene->tex_image_packed);
	}
	if(dscene->tex_image_packed_info.size()) {
		if(dscene->tex_image_packed_info.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_packed_info);
		}
		device->tex_alloc("__tex_image_packed_info", dscene->tex_image_packed_info);
	}
}

void ImageManager::device_free_builtin(Device *device, DeviceScene *dscene)
{
	for(size_t slot = 0; slot < images.size(); slot++)
		if(images[slot] && images[slot]->builtin_data)
			device_free_image(device, dscene, slot + tex_image_byte_start);

	for(size_t slot = 0; slot < float_images.size(); slot++)
		if(float_images[slot] && float_images[slot]->builtin_data)
			device_free_image(device, dscene, slot);
}

void ImageManager::device_free(Device *device, DeviceScene *dscene)
{
	for(size_t slot = 0; slot < images.size(); slot++)
		device_free_image(device, dscene, slot + tex_image_byte_start);
	for(size_t slot = 0; slot < float_images.size(); slot++)
		device_free_image(device, dscene, slot);

	device->tex_free(dscene->tex_image_packed);
	device->tex_free(dscene->tex_image_packed_info);

	dscene->tex_image_packed.clear();
	dscene->tex_image_packed_info.clear();

	images.clear();
	float_images.clear();
}

CCL_NAMESPACE_END


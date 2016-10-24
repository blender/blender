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
 * limitations under the License.
 */

#include "device.h"
#include "image.h"
#include "scene.h"

#include "util_foreach.h"
#include "util_path.h"
#include "util_progress.h"
#include "util_texture.h"

#ifdef WITH_OSL
#include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

ImageManager::ImageManager(const DeviceInfo& info)
{
	need_update = true;
	pack_images = false;
	osl_texture_system = NULL;
	animation_frame = 0;

	/* In case of multiple devices used we need to know type of an actual
	 * compute device.
	 *
	 * NOTE: We assume that all the devices are same type, otherwise we'll
	 * be screwed on so many levels..
	 */
	DeviceType device_type = info.type;
	if(device_type == DEVICE_MULTI) {
		device_type = info.multi_devices[0].type;
	}

	/* Set image limits */
#define SET_TEX_IMAGES_LIMITS(ARCH) \
	{ \
		tex_num_images[IMAGE_DATA_TYPE_FLOAT4] = TEX_NUM_FLOAT4_ ## ARCH; \
		tex_num_images[IMAGE_DATA_TYPE_BYTE4] = TEX_NUM_BYTE4_ ## ARCH; \
		tex_num_images[IMAGE_DATA_TYPE_HALF4] = TEX_NUM_HALF4_ ## ARCH; \
		tex_num_images[IMAGE_DATA_TYPE_FLOAT] = TEX_NUM_FLOAT_ ## ARCH; \
		tex_num_images[IMAGE_DATA_TYPE_BYTE] = TEX_NUM_BYTE_ ## ARCH; \
		tex_num_images[IMAGE_DATA_TYPE_HALF] = TEX_NUM_HALF_ ## ARCH; \
		tex_start_images[IMAGE_DATA_TYPE_FLOAT4] = TEX_START_FLOAT4_ ## ARCH; \
		tex_start_images[IMAGE_DATA_TYPE_BYTE4] = TEX_START_BYTE4_ ## ARCH; \
		tex_start_images[IMAGE_DATA_TYPE_HALF4] = TEX_START_HALF4_ ## ARCH; \
		tex_start_images[IMAGE_DATA_TYPE_FLOAT] = TEX_START_FLOAT_ ## ARCH; \
		tex_start_images[IMAGE_DATA_TYPE_BYTE] = TEX_START_BYTE_ ## ARCH; \
		tex_start_images[IMAGE_DATA_TYPE_HALF] = TEX_START_HALF_ ## ARCH; \
	}

	if(device_type == DEVICE_CPU) {
		SET_TEX_IMAGES_LIMITS(CPU);
	}
	else if(device_type == DEVICE_CUDA) {
		if(info.has_bindless_textures) {
			SET_TEX_IMAGES_LIMITS(CUDA_KEPLER);
		}
		else {
			SET_TEX_IMAGES_LIMITS(CUDA);
		}
	}
	else if(device_type == DEVICE_OPENCL) {
		SET_TEX_IMAGES_LIMITS(OPENCL);
	}
	else {
		/* Should not happen. */
		tex_num_images[IMAGE_DATA_TYPE_FLOAT4] = 0;
		tex_num_images[IMAGE_DATA_TYPE_BYTE4] = 0;
		tex_num_images[IMAGE_DATA_TYPE_HALF4] = 0;
		tex_num_images[IMAGE_DATA_TYPE_FLOAT] = 0;
		tex_num_images[IMAGE_DATA_TYPE_BYTE] = 0;
		tex_num_images[IMAGE_DATA_TYPE_HALF] = 0;
		tex_start_images[IMAGE_DATA_TYPE_FLOAT4] = 0;
		tex_start_images[IMAGE_DATA_TYPE_BYTE4] = 0;
		tex_start_images[IMAGE_DATA_TYPE_HALF4] = 0;
		tex_start_images[IMAGE_DATA_TYPE_FLOAT] = 0;
		tex_start_images[IMAGE_DATA_TYPE_BYTE] = 0;
		tex_start_images[IMAGE_DATA_TYPE_HALF] = 0;
		assert(0);
	}

#undef SET_TEX_IMAGES_LIMITS
}

ImageManager::~ImageManager()
{
	for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++)
			assert(!images[type][slot]);
	}
}

void ImageManager::set_pack_images(bool pack_images_)
{
	pack_images = pack_images_;
}

void ImageManager::set_osl_texture_system(void *texture_system)
{
	osl_texture_system = texture_system;
}

bool ImageManager::set_animation_frame_update(int frame)
{
	if(frame != animation_frame) {
		animation_frame = frame;

		for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
			for(size_t slot = 0; slot < images[type].size(); slot++) {
				if(images[type][slot] && images[type][slot]->animated)
					return true;
			}
		}
	}

	return false;
}

ImageManager::ImageDataType ImageManager::get_image_metadata(const string& filename,
                                                             void *builtin_data,
                                                             bool& is_linear)
{
	bool is_float = false, is_half = false;
	is_linear = false;
	int channels = 4;

	if(builtin_data) {
		if(builtin_image_info_cb) {
			int width, height, depth;
			builtin_image_info_cb(filename, builtin_data, is_float, width, height, depth, channels);
		}

		if(is_float) {
			is_linear = true;
			return (channels > 1) ? IMAGE_DATA_TYPE_FLOAT4 : IMAGE_DATA_TYPE_FLOAT;
		}
		else {
			return (channels > 1) ? IMAGE_DATA_TYPE_BYTE4 : IMAGE_DATA_TYPE_BYTE;
		}
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

			/* check if it's half float */
			if(spec.format == TypeDesc::HALF)
				is_half = true;

			channels = spec.nchannels;

			/* basic color space detection, not great but better than nothing
			 * before we do OpenColorIO integration */
			if(is_float) {
				string colorspace = spec.get_string_attribute("oiio:ColorSpace");

				is_linear = !(colorspace == "sRGB" ||
				              colorspace == "GammaCorrected" ||
				              (colorspace == "" &&
				                  (strcmp(in->format_name(), "png") == 0 ||
				                   strcmp(in->format_name(), "tiff") == 0 ||
				                   strcmp(in->format_name(), "dpx") == 0 ||
				                   strcmp(in->format_name(), "jpeg2000") == 0)));
			}
			else {
				is_linear = false;
			}

			in->close();
		}

		delete in;
	}

	if(is_half) {
		return (channels > 1) ? IMAGE_DATA_TYPE_HALF4 : IMAGE_DATA_TYPE_HALF;
	}
	else if(is_float) {
		return (channels > 1) ? IMAGE_DATA_TYPE_FLOAT4 : IMAGE_DATA_TYPE_FLOAT;
	}
	else {
		return (channels > 1) ? IMAGE_DATA_TYPE_BYTE4 : IMAGE_DATA_TYPE_BYTE;
	}
}

/* We use a consecutive slot counting scheme on the devices, in order
 * float4, byte4, half4, float, byte, half.
 * These functions convert the slot ids from ImageManager "images" ones
 * to device ones and vice versa. */
int ImageManager::type_index_to_flattened_slot(int slot, ImageDataType type)
{
	return slot + tex_start_images[type];
}

int ImageManager::flattened_slot_to_type_index(int flat_slot, ImageDataType *type)
{
	for(int i = IMAGE_DATA_NUM_TYPES - 1; i >= 0; i--) {
		if(flat_slot >= tex_start_images[i]) {
			*type = (ImageDataType)i;
			return flat_slot - tex_start_images[i];
		}
	}

	/* Should not happen. */
	return flat_slot;
}

string ImageManager::name_from_type(int type)
{
	if(type == IMAGE_DATA_TYPE_FLOAT4)
		return "float4";
	else if(type == IMAGE_DATA_TYPE_FLOAT)
		return "float";
	else if(type == IMAGE_DATA_TYPE_BYTE)
		return "byte";
	else if(type == IMAGE_DATA_TYPE_HALF4)
		return "half4";
	else if(type == IMAGE_DATA_TYPE_HALF)
		return "half";
	else
		return "byte4";
}

static bool image_equals(ImageManager::Image *image,
                         const string& filename,
                         void *builtin_data,
                         InterpolationType interpolation,
                         ExtensionType extension)
{
	return image->filename == filename &&
	       image->builtin_data == builtin_data &&
	       image->interpolation == interpolation &&
	       image->extension == extension;
}

int ImageManager::add_image(const string& filename,
                            void *builtin_data,
                            bool animated,
                            float frame,
                            bool& is_float,
                            bool& is_linear,
                            InterpolationType interpolation,
                            ExtensionType extension,
                            bool use_alpha)
{
	Image *img;
	size_t slot;

	ImageDataType type = get_image_metadata(filename, builtin_data, is_linear);

	/* Do we have a float? */
	if(type == IMAGE_DATA_TYPE_FLOAT || type == IMAGE_DATA_TYPE_FLOAT4)
		is_float = true;

	/* No single channel and half textures on CUDA (Fermi) and no half on OpenCL, use available slots */
	if((type == IMAGE_DATA_TYPE_FLOAT ||
	    type == IMAGE_DATA_TYPE_HALF4 ||
	    type == IMAGE_DATA_TYPE_HALF) &&
	    tex_num_images[type] == 0) {
		type = IMAGE_DATA_TYPE_FLOAT4;
	}
	if(type == IMAGE_DATA_TYPE_BYTE && tex_num_images[type] == 0) {
		type = IMAGE_DATA_TYPE_BYTE4;
	}

	/* Fnd existing image. */
	for(slot = 0; slot < images[type].size(); slot++) {
		img = images[type][slot];
		if(img && image_equals(img,
		                       filename,
		                       builtin_data,
		                       interpolation,
		                       extension))
		{
			if(img->frame != frame) {
				img->frame = frame;
				img->need_load = true;
			}
			if(img->use_alpha != use_alpha) {
				img->use_alpha = use_alpha;
				img->need_load = true;
			}
			img->users++;
			return type_index_to_flattened_slot(slot, type);
		}
	}

	/* Find free slot. */
	for(slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			break;
	}

	if(slot == images[type].size()) {
		/* Max images limit reached. */
		if(images[type].size() == tex_num_images[type]) {
			printf("ImageManager::add_image: Reached %s image limit (%d), skipping '%s'\n",
			       name_from_type(type).c_str(), tex_num_images[type], filename.c_str());
			return -1;
		}

		images[type].resize(images[type].size() + 1);
	}

	/* Add new image. */
	img = new Image();
	img->filename = filename;
	img->builtin_data = builtin_data;
	img->need_load = true;
	img->animated = animated;
	img->frame = frame;
	img->interpolation = interpolation;
	img->extension = extension;
	img->users = 1;
	img->use_alpha = use_alpha;

	images[type][slot] = img;

	need_update = true;

	return type_index_to_flattened_slot(slot, type);
}

void ImageManager::remove_image(int flat_slot)
{
	ImageDataType type;
	int slot = flattened_slot_to_type_index(flat_slot, &type);

	Image *image = images[type][slot];
	assert(image && image->users >= 1);

	/* decrement user count */
	image->users--;

	/* don't remove immediately, rather do it all together later on. one of
	 * the reasons for this is that on shader changes we add and remove nodes
	 * that use them, but we do not want to reload the image all the time. */
	if(image->users == 0)
		need_update = true;
}

void ImageManager::remove_image(const string& filename,
                                void *builtin_data,
                                InterpolationType interpolation,
                                ExtensionType extension)
{
	size_t slot;

	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && image_equals(images[type][slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension))
			{
				remove_image(type_index_to_flattened_slot(slot, (ImageDataType)type));
				return;
			}
		}
	}
}

/* TODO(sergey): Deduplicate with the iteration above, but make it pretty,
 * without bunch of arguments passing around making code readability even
 * more cluttered.
 */
void ImageManager::tag_reload_image(const string& filename,
                                    void *builtin_data,
                                    InterpolationType interpolation,
                                    ExtensionType extension)
{
	for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && image_equals(images[type][slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension))
			{
				images[type][slot]->need_load = true;
				break;
			}
		}
	}
}

bool ImageManager::file_load_image_generic(Image *img, ImageInput **in, int &width, int &height, int &depth, int &components)
{
	if(img->filename == "")
		return false;

	if(!img->builtin_data) {
		/* load image from file through OIIO */
		*in = ImageInput::create(img->filename);

		if(!*in)
			return false;

		ImageSpec spec = ImageSpec();
		ImageSpec config = ImageSpec();

		if(img->use_alpha == false)
			config.attribute("oiio:UnassociatedAlpha", 1);

		if(!(*in)->open(img->filename, spec, config)) {
			delete *in;
			*in = NULL;
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
		if(*in) {
			(*in)->close();
			delete *in;
			*in = NULL;
		}

		return false;
	}

	return true;
}

template<typename T>
bool ImageManager::file_load_byte_image(Image *img, ImageDataType type, device_vector<T>& tex_img)
{
	ImageInput *in = NULL;
	int width, height, depth, components;

	if(!file_load_image_generic(img, &in, width, height, depth, components))
		return false;

	/* read RGBA pixels */
	uchar *pixels = (uchar*)tex_img.resize(width, height, depth);
	if(pixels == NULL) {
		return false;
	}
	bool cmyk = false;

	if(in) {
		if(depth <= 1) {
			int scanlinesize = width*components*sizeof(uchar);

			in->read_image(TypeDesc::UINT8,
			               (uchar*)pixels + (((size_t)height)-1)*scanlinesize,
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

	/* Check if we actually have a byte4 slot, in case components == 1, but device
	 * doesn't support single channel textures. */
	if(type == IMAGE_DATA_TYPE_BYTE4) {
		size_t num_pixels = ((size_t)width) * height * depth;
		if(cmyk) {
			/* CMYK */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
				pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
				pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
				pixels[i*4+3] = 255;
			}
		}
		else if(components == 2) {
			/* grayscale + alpha */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = pixels[i*2+1];
				pixels[i*4+2] = pixels[i*2+0];
				pixels[i*4+1] = pixels[i*2+0];
				pixels[i*4+0] = pixels[i*2+0];
			}
		}
		else if(components == 3) {
			/* RGB */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 255;
				pixels[i*4+2] = pixels[i*3+2];
				pixels[i*4+1] = pixels[i*3+1];
				pixels[i*4+0] = pixels[i*3+0];
			}
		}
		else if(components == 1) {
			/* grayscale */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 255;
				pixels[i*4+2] = pixels[i];
				pixels[i*4+1] = pixels[i];
				pixels[i*4+0] = pixels[i];
			}
		}

		if(img->use_alpha == false) {
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 255;
			}
		}
	}

	return true;
}

template<typename T>
bool ImageManager::file_load_float_image(Image *img, ImageDataType type, device_vector<T>& tex_img)
{
	ImageInput *in = NULL;
	int width, height, depth, components;

	if(!file_load_image_generic(img, &in, width, height, depth, components))
		return false;

	/* read RGBA pixels */
	float *pixels = (float*)tex_img.resize(width, height, depth);
	if(pixels == NULL) {
		return false;
	}
	bool cmyk = false;

	if(in) {
		float *readpixels = pixels;
		vector<float> tmppixels;

		if(components > 4) {
			tmppixels.resize(((size_t)width)*height*components);
			readpixels = &tmppixels[0];
		}

		if(depth <= 1) {
			size_t scanlinesize = ((size_t)width)*components*sizeof(float);
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
			size_t dimensions = ((size_t)width)*height;
			for(size_t i = dimensions-1, pixel = 0; pixel < dimensions; pixel++, i--) {
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

	/* Check if we actually have a float4 slot, in case components == 1, but device
	 * doesn't support single channel textures. */
	if(type == IMAGE_DATA_TYPE_FLOAT4) {
		size_t num_pixels = ((size_t)width) * height * depth;
		if(cmyk) {
			/* CMYK */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 255;
				pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
				pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
				pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
			}
		}
		else if(components == 2) {
			/* grayscale + alpha */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = pixels[i*2+1];
				pixels[i*4+2] = pixels[i*2+0];
				pixels[i*4+1] = pixels[i*2+0];
				pixels[i*4+0] = pixels[i*2+0];
			}
		}
		else if(components == 3) {
			/* RGB */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 1.0f;
				pixels[i*4+2] = pixels[i*3+2];
				pixels[i*4+1] = pixels[i*3+1];
				pixels[i*4+0] = pixels[i*3+0];
			}
		}
		else if(components == 1) {
			/* grayscale */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 1.0f;
				pixels[i*4+2] = pixels[i];
				pixels[i*4+1] = pixels[i];
				pixels[i*4+0] = pixels[i];
			}
		}

		if(img->use_alpha == false) {
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 1.0f;
			}
		}
	}

	return true;
}

template<typename T>
bool ImageManager::file_load_half_image(Image *img, ImageDataType type, device_vector<T>& tex_img)
{
	ImageInput *in = NULL;
	int width, height, depth, components;

	if(!file_load_image_generic(img, &in, width, height, depth, components))
		return false;

	/* read RGBA pixels */
	half *pixels = (half*)tex_img.resize(width, height, depth);
	if(pixels == NULL) {
		return false;
	}

	if(in) {
		half *readpixels = pixels;
		vector<half> tmppixels;

		if(components > 4) {
			tmppixels.resize(((size_t)width)*height*components);
			readpixels = &tmppixels[0];
		}

		if(depth <= 1) {
			size_t scanlinesize = ((size_t)width)*components*sizeof(half);
			in->read_image(TypeDesc::HALF,
			               (uchar*)readpixels + (height-1)*scanlinesize,
			               AutoStride,
			               -scanlinesize,
			               AutoStride);
		}
		else {
			in->read_image(TypeDesc::HALF, (uchar*)readpixels);
		}

		if(components > 4) {
			size_t dimensions = ((size_t)width)*height;
			for(size_t i = dimensions-1, pixel = 0; pixel < dimensions; pixel++, i--) {
				pixels[i*4+3] = tmppixels[i*components+3];
				pixels[i*4+2] = tmppixels[i*components+2];
				pixels[i*4+1] = tmppixels[i*components+1];
				pixels[i*4+0] = tmppixels[i*components+0];
			}

			tmppixels.clear();
		}

		in->close();
		delete in;
	}
#if 0
	/* TODO(dingto): Support half for ImBuf. */
	else {
		builtin_image_float_pixels_cb(img->filename, img->builtin_data, pixels);
	}
#endif

	/* Check if we actually have a half4 slot, in case components == 1, but device
	 * doesn't support single channel textures. */
	if(type == IMAGE_DATA_TYPE_HALF4) {
		size_t num_pixels = ((size_t)width) * height * depth;
		if(components == 2) {
			/* grayscale + alpha */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = pixels[i*2+1];
				pixels[i*4+2] = pixels[i*2+0];
				pixels[i*4+1] = pixels[i*2+0];
				pixels[i*4+0] = pixels[i*2+0];
			}
		}
		else if(components == 3) {
			/* RGB */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 1.0f;
				pixels[i*4+2] = pixels[i*3+2];
				pixels[i*4+1] = pixels[i*3+1];
				pixels[i*4+0] = pixels[i*3+0];
			}
		}
		else if(components == 1) {
			/* grayscale */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 1.0f;
				pixels[i*4+2] = pixels[i];
				pixels[i*4+1] = pixels[i];
				pixels[i*4+0] = pixels[i];
			}
		}

		if(img->use_alpha == false) {
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = 1.0f;
			}
		}
	}

	return true;
}

void ImageManager::device_load_image(Device *device, DeviceScene *dscene, ImageDataType type, int slot, Progress *progress)
{
	if(progress->get_cancel())
		return;
	
	Image *img = images[type][slot];

	if(osl_texture_system && !img->builtin_data)
		return;

	string filename = path_filename(images[type][slot]->filename);
	progress->set_status("Updating Images", "Loading " + filename);

	/* Slot assignment */
	int flat_slot = type_index_to_flattened_slot(slot, type);

	string name;
	if(flat_slot >= 100)
		name = string_printf("__tex_image_%s_%d", name_from_type(type).c_str(), flat_slot);
	else if(flat_slot >= 10)
		name = string_printf("__tex_image_%s_0%d", name_from_type(type).c_str(), flat_slot);
	else
		name = string_printf("__tex_image_%s_00%d", name_from_type(type).c_str(), flat_slot);

	if(type == IMAGE_DATA_TYPE_FLOAT4) {
		device_vector<float4>& tex_img = dscene->tex_float4_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_float_image(img, type, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			float *pixels = (float*)tex_img.resize(1, 1);

			pixels[0] = TEX_IMAGE_MISSING_R;
			pixels[1] = TEX_IMAGE_MISSING_G;
			pixels[2] = TEX_IMAGE_MISSING_B;
			pixels[3] = TEX_IMAGE_MISSING_A;
		}

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}
	else if(type == IMAGE_DATA_TYPE_FLOAT) {
		device_vector<float>& tex_img = dscene->tex_float_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_float_image(img, type, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			float *pixels = (float*)tex_img.resize(1, 1);

			pixels[0] = TEX_IMAGE_MISSING_R;
		}

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}
	else if(type == IMAGE_DATA_TYPE_BYTE4) {
		device_vector<uchar4>& tex_img = dscene->tex_byte4_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_byte_image(img, type, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			uchar *pixels = (uchar*)tex_img.resize(1, 1);

			pixels[0] = (TEX_IMAGE_MISSING_R * 255);
			pixels[1] = (TEX_IMAGE_MISSING_G * 255);
			pixels[2] = (TEX_IMAGE_MISSING_B * 255);
			pixels[3] = (TEX_IMAGE_MISSING_A * 255);
		}

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}
	else if(type == IMAGE_DATA_TYPE_BYTE){
		device_vector<uchar>& tex_img = dscene->tex_byte_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_byte_image(img, type, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			uchar *pixels = (uchar*)tex_img.resize(1, 1);

			pixels[0] = (TEX_IMAGE_MISSING_R * 255);
		}

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}
	else if(type == IMAGE_DATA_TYPE_HALF4){
		device_vector<half4>& tex_img = dscene->tex_half4_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_half_image(img, type, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			half *pixels = (half*)tex_img.resize(1, 1);

			pixels[0] = TEX_IMAGE_MISSING_R;
			pixels[1] = TEX_IMAGE_MISSING_G;
			pixels[2] = TEX_IMAGE_MISSING_B;
			pixels[3] = TEX_IMAGE_MISSING_A;
		}

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}
	else if(type == IMAGE_DATA_TYPE_HALF){
		device_vector<half>& tex_img = dscene->tex_half_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_half_image(img, type, tex_img)) {
			/* on failure to load, we set a 1x1 pixels pink image */
			half *pixels = (half*)tex_img.resize(1, 1);

			pixels[0] = TEX_IMAGE_MISSING_R;
		}

		if(!pack_images) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_alloc(name.c_str(),
			                  tex_img,
			                  img->interpolation,
			                  img->extension);
		}
	}

	img->need_load = false;
}

void ImageManager::device_free_image(Device *device, DeviceScene *dscene, ImageDataType type, int slot)
{
	Image *img = images[type][slot];

	if(img) {
		if(osl_texture_system && !img->builtin_data) {
#ifdef WITH_OSL
			ustring filename(images[type][slot]->filename);
			((OSL::TextureSystem*)osl_texture_system)->invalidate(filename);
#endif
		}
		else if(type == IMAGE_DATA_TYPE_FLOAT4) {
			device_vector<float4>& tex_img = dscene->tex_float4_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();
		}
		else if(type == IMAGE_DATA_TYPE_FLOAT) {
			device_vector<float>& tex_img = dscene->tex_float_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();
		}
		else if(type == IMAGE_DATA_TYPE_BYTE4) {
			device_vector<uchar4>& tex_img = dscene->tex_byte4_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();
		}
		else if(type == IMAGE_DATA_TYPE_BYTE){
			device_vector<uchar>& tex_img = dscene->tex_byte_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();
		}
		else if(type == IMAGE_DATA_TYPE_HALF4){
			device_vector<half4>& tex_img = dscene->tex_half4_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();
		}
		else if(type == IMAGE_DATA_TYPE_HALF){
			device_vector<half>& tex_img = dscene->tex_half_image[slot];

			if(tex_img.device_pointer) {
				thread_scoped_lock device_lock(device_mutex);
				device->tex_free(tex_img);
			}

			tex_img.clear();
		}

		delete images[type][slot];
		images[type][slot] = NULL;
	}
}

void ImageManager::device_update(Device *device, DeviceScene *dscene, Progress& progress)
{
	if(!need_update)
		return;

	TaskPool pool;

	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			if(!images[type][slot])
				continue;

			if(images[type][slot]->users == 0) {
				device_free_image(device, dscene, (ImageDataType)type, slot);
			}
			else if(images[type][slot]->need_load) {
				if(!osl_texture_system || images[type][slot]->builtin_data)
					pool.push(function_bind(&ImageManager::device_load_image, this, device, dscene, (ImageDataType)type, slot, &progress));
			}
		}
	}

	pool.wait_work();

	if(pack_images)
		device_pack_images(device, dscene, progress);

	need_update = false;
}

void ImageManager::device_update_slot(Device *device,
                                      DeviceScene *dscene,
                                      int flat_slot,
                                      Progress *progress)
{
	ImageDataType type;
	int slot = flattened_slot_to_type_index(flat_slot, &type);

	Image *image = images[type][slot];
	assert(image != NULL);

	if(image->users == 0) {
		device_free_image(device, dscene, type, slot);
	}
	else if(image->need_load) {
		if(!osl_texture_system || image->builtin_data)
			device_load_image(device,
			                  dscene,
			                  type,
			                  slot,
			                  progress);
	}
}

uint8_t ImageManager::pack_image_options(ImageDataType type, size_t slot)
{
	uint8_t options = 0;

	/* Image Options are packed into one uint:
	 * bit 0 -> Interpolation
	 * bit 1 + 2  + 3-> Extension */
	if(images[type][slot]->interpolation == INTERPOLATION_CLOSEST)
		options |= (1 << 0);

	if(images[type][slot]->extension == EXTENSION_REPEAT)
		options |= (1 << 1);
	else if(images[type][slot]->extension == EXTENSION_EXTEND)
		options |= (1 << 2);
	else /* EXTENSION_CLIP */
		options |= (1 << 3);

	return options;
}

void ImageManager::device_pack_images(Device *device,
                                      DeviceScene *dscene,
                                      Progress& /*progess*/)
{
	/* For OpenCL, we pack all image textures into a single large texture, and
	 * do our own interpolation in the kernel. */
	size_t size = 0, offset = 0;
	ImageDataType type;

	int info_size = tex_num_images[IMAGE_DATA_TYPE_FLOAT4] + tex_num_images[IMAGE_DATA_TYPE_BYTE4]
	                + tex_num_images[IMAGE_DATA_TYPE_FLOAT] + tex_num_images[IMAGE_DATA_TYPE_BYTE];
	uint4 *info = dscene->tex_image_packed_info.resize(info_size);

	/* Byte4 Textures*/
	type = IMAGE_DATA_TYPE_BYTE4;

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<uchar4>& tex_img = dscene->tex_byte4_image[slot];
		size += tex_img.size();
	}

	uchar4 *pixels_byte4 = dscene->tex_image_byte4_packed.resize(size);

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<uchar4>& tex_img = dscene->tex_byte4_image[slot];

		uint8_t options = pack_image_options(type, slot);

		info[type_index_to_flattened_slot(slot, type)] = make_uint4(tex_img.data_width, tex_img.data_height, offset, options);

		memcpy(pixels_byte4+offset, (void*)tex_img.data_pointer, tex_img.memory_size());
		offset += tex_img.size();
	}

	/* Float4 Textures*/
	type = IMAGE_DATA_TYPE_FLOAT4;
	size = 0, offset = 0;

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<float4>& tex_img = dscene->tex_float4_image[slot];
		size += tex_img.size();
	}

	float4 *pixels_float4 = dscene->tex_image_float4_packed.resize(size);

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<float4>& tex_img = dscene->tex_float4_image[slot];

		/* todo: support 3D textures, only CPU for now */

		uint8_t options = pack_image_options(type, slot);
		info[type_index_to_flattened_slot(slot, type)] = make_uint4(tex_img.data_width, tex_img.data_height, offset, options);

		memcpy(pixels_float4+offset, (void*)tex_img.data_pointer, tex_img.memory_size());
		offset += tex_img.size();
	}

	/* Byte Textures*/
	type = IMAGE_DATA_TYPE_BYTE;
	size = 0, offset = 0;

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<uchar>& tex_img = dscene->tex_byte_image[slot];
		size += tex_img.size();
	}

	uchar *pixels_byte = dscene->tex_image_byte_packed.resize(size);

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<uchar>& tex_img = dscene->tex_byte_image[slot];

		uint8_t options = pack_image_options(type, slot);

		info[type_index_to_flattened_slot(slot, type)] = make_uint4(tex_img.data_width, tex_img.data_height, offset, options);

		memcpy(pixels_byte+offset, (void*)tex_img.data_pointer, tex_img.memory_size());
		offset += tex_img.size();
	}

	/* Float Textures*/
	type = IMAGE_DATA_TYPE_FLOAT;
	size = 0, offset = 0;

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<float>& tex_img = dscene->tex_float_image[slot];
		size += tex_img.size();
	}

	float *pixels_float = dscene->tex_image_float_packed.resize(size);

	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(!images[type][slot])
			continue;

		device_vector<float>& tex_img = dscene->tex_float_image[slot];

		/* todo: support 3D textures, only CPU for now */

		uint8_t options = pack_image_options(type, slot);
		info[type_index_to_flattened_slot(slot, type)] = make_uint4(tex_img.data_width, tex_img.data_height, offset, options);

		memcpy(pixels_float+offset, (void*)tex_img.data_pointer, tex_img.memory_size());
		offset += tex_img.size();
	}

	if(dscene->tex_image_byte4_packed.size()) {
		if(dscene->tex_image_byte4_packed.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_byte4_packed);
		}
		device->tex_alloc("__tex_image_byte4_packed", dscene->tex_image_byte4_packed);
	}
	if(dscene->tex_image_float4_packed.size()) {
		if(dscene->tex_image_float4_packed.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_float4_packed);
		}
		device->tex_alloc("__tex_image_float4_packed", dscene->tex_image_float4_packed);
	}
	if(dscene->tex_image_byte_packed.size()) {
		if(dscene->tex_image_byte_packed.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_byte_packed);
		}
		device->tex_alloc("__tex_image_byte_packed", dscene->tex_image_byte_packed);
	}
	if(dscene->tex_image_float_packed.size()) {
		if(dscene->tex_image_float_packed.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(dscene->tex_image_float_packed);
		}
		device->tex_alloc("__tex_image_float_packed", dscene->tex_image_float_packed);
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
	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && images[type][slot]->builtin_data)
				device_free_image(device, dscene, (ImageDataType)type, slot);
		}
	}
}

void ImageManager::device_free(Device *device, DeviceScene *dscene)
{
	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			device_free_image(device, dscene, (ImageDataType)type, slot);
		}
		images[type].clear();
	}

	device->tex_free(dscene->tex_image_byte4_packed);
	device->tex_free(dscene->tex_image_float4_packed);
	device->tex_free(dscene->tex_image_byte_packed);
	device->tex_free(dscene->tex_image_float_packed);
	device->tex_free(dscene->tex_image_packed_info);

	dscene->tex_image_byte4_packed.clear();
	dscene->tex_image_float4_packed.clear();
	dscene->tex_image_byte_packed.clear();
	dscene->tex_image_float_packed.clear();
	dscene->tex_image_packed_info.clear();
}

CCL_NAMESPACE_END


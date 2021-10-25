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

#include "device/device.h"
#include "render/image.h"
#include "render/scene.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_path.h"
#include "util/util_progress.h"
#include "util/util_texture.h"

#ifdef WITH_OSL
#include <OSL/oslexec.h>
#endif

CCL_NAMESPACE_BEGIN

/* Some helpers to silence warning in templated function. */
static bool isfinite(uchar /*value*/)
{
	return false;
}
static bool isfinite(half /*value*/)
{
	return false;
}

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
	max_num_images = TEX_NUM_MAX;
	has_half_images = true;
	cuda_fermi_limits = false;

	if(device_type == DEVICE_CUDA) {
		if(!info.has_bindless_textures) {
			/* CUDA Fermi hardware (SM 2.x) has a hard limit on the number of textures */
			cuda_fermi_limits = true;
			has_half_images = false;
		}
	}
	else if(device_type == DEVICE_OPENCL) {
		has_half_images = false;
	}

	for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		tex_num_images[type] = 0;
	}
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

ImageDataType ImageManager::get_image_metadata(const string& filename,
                                               void *builtin_data,
                                               bool& is_linear,
                                               bool& builtin_free_cache)
{
	bool is_float = false, is_half = false;
	is_linear = false;
	builtin_free_cache = false;
	int channels = 4;

	if(builtin_data) {
		if(builtin_image_info_cb) {
			int width, height, depth;
			builtin_image_info_cb(filename, builtin_data, is_float, width, height, depth, channels, builtin_free_cache);
		}

		if(is_float) {
			is_linear = true;
			return (channels > 1) ? IMAGE_DATA_TYPE_FLOAT4 : IMAGE_DATA_TYPE_FLOAT;
		}
		else {
			return (channels > 1) ? IMAGE_DATA_TYPE_BYTE4 : IMAGE_DATA_TYPE_BYTE;
		}
	}

	/* Perform preliminary checks, with meaningful logging. */
	if(!path_exists(filename)) {
		VLOG(1) << "File '" << filename << "' does not exist.";
		return IMAGE_DATA_TYPE_BYTE4;
	}
	if(path_is_directory(filename)) {
		VLOG(1) << "File '" << filename << "' is a directory, can't use as image.";
		return IMAGE_DATA_TYPE_BYTE4;
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

int ImageManager::max_flattened_slot(ImageDataType type)
{
	if(tex_num_images[type] == 0) {
		/* No textures for the type, no slots needs allocation. */
		return 0;
	}
	return type_index_to_flattened_slot(tex_num_images[type], type);
}

/* The lower three bits of a device texture slot number indicate its type.
 * These functions convert the slot ids from ImageManager "images" ones
 * to device ones and vice verse.
 */
int ImageManager::type_index_to_flattened_slot(int slot, ImageDataType type)
{
	return (slot << IMAGE_DATA_TYPE_SHIFT) | (type);
}

int ImageManager::flattened_slot_to_type_index(int flat_slot, ImageDataType *type)
{
	*type = (ImageDataType)(flat_slot & IMAGE_DATA_TYPE_MASK);
	return flat_slot >> IMAGE_DATA_TYPE_SHIFT;
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
                         ExtensionType extension,
                         bool use_alpha)
{
	return image->filename == filename &&
	       image->builtin_data == builtin_data &&
	       image->interpolation == interpolation &&
	       image->extension == extension &&
	       image->use_alpha == use_alpha;
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
	bool builtin_free_cache;

	ImageDataType type = get_image_metadata(filename, builtin_data, is_linear, builtin_free_cache);

	thread_scoped_lock device_lock(device_mutex);

	/* Check whether it's a float texture. */
	is_float = (type == IMAGE_DATA_TYPE_FLOAT || type == IMAGE_DATA_TYPE_FLOAT4);

	/* No single channel and half textures on CUDA (Fermi) and no half on OpenCL, use available slots */
	if(!has_half_images) {
		if(type == IMAGE_DATA_TYPE_HALF4) {
			type = IMAGE_DATA_TYPE_FLOAT4;
		}
		else if(type == IMAGE_DATA_TYPE_HALF) {
			type = IMAGE_DATA_TYPE_FLOAT;
		}
	}

	if(cuda_fermi_limits) {
		if(type == IMAGE_DATA_TYPE_FLOAT) {
			type = IMAGE_DATA_TYPE_FLOAT4;
		}
		else if(type == IMAGE_DATA_TYPE_BYTE) {
			type = IMAGE_DATA_TYPE_BYTE4;
		}
	}

	/* Fnd existing image. */
	for(slot = 0; slot < images[type].size(); slot++) {
		img = images[type][slot];
		if(img && image_equals(img,
		                       filename,
		                       builtin_data,
		                       interpolation,
		                       extension,
		                       use_alpha))
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

	/* Count if we're over the limit */
	if(cuda_fermi_limits) {
		if(tex_num_images[IMAGE_DATA_TYPE_BYTE4] == TEX_NUM_BYTE4_CUDA
			|| tex_num_images[IMAGE_DATA_TYPE_FLOAT4] == TEX_NUM_FLOAT4_CUDA)
		{
			printf("ImageManager::add_image: Reached %s image limit (%d), skipping '%s'\n",
				name_from_type(type).c_str(), tex_num_images[type], filename.c_str());
			return -1;
		}
	}
	else {
		/* Very unlikely, since max_num_images is insanely big. But better safe than sorry. */
		int tex_count = 0;
		for (int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
			tex_count += tex_num_images[type];
		}
		if(tex_count > max_num_images) {
			printf("ImageManager::add_image: Reached image limit (%d), skipping '%s'\n",
				max_num_images, filename.c_str());
			return -1;
		}
	}

	if(slot == images[type].size()) {
		images[type].resize(images[type].size() + 1);
	}

	/* Add new image. */
	img = new Image();
	img->filename = filename;
	img->builtin_data = builtin_data;
	img->builtin_free_cache = builtin_free_cache;
	img->need_load = true;
	img->animated = animated;
	img->frame = frame;
	img->interpolation = interpolation;
	img->extension = extension;
	img->users = 1;
	img->use_alpha = use_alpha;

	images[type][slot] = img;

	++tex_num_images[type];

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
                                ExtensionType extension,
                                bool use_alpha)
{
	size_t slot;

	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && image_equals(images[type][slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension,
			                                      use_alpha))
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
                                    ExtensionType extension,
                                    bool use_alpha)
{
	for(size_t type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		for(size_t slot = 0; slot < images[type].size(); slot++) {
			if(images[type][slot] && image_equals(images[type][slot],
			                                      filename,
			                                      builtin_data,
			                                      interpolation,
			                                      extension,
			                                      use_alpha))
			{
				images[type][slot]->need_load = true;
				break;
			}
		}
	}
}

bool ImageManager::file_load_image_generic(Image *img,
                                           ImageInput **in,
                                           int &width,
                                           int &height,
                                           int &depth,
                                           int &components)
{
	if(img->filename == "")
		return false;

	if(!img->builtin_data) {
		/* NOTE: Error logging is done in meta data acquisition. */
		if(!path_exists(img->filename) || path_is_directory(img->filename)) {
			return false;
		}

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

		bool is_float, free_cache;
		builtin_image_info_cb(img->filename, img->builtin_data, is_float, width, height, depth, components, free_cache);
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

template<TypeDesc::BASETYPE FileFormat,
         typename StorageType,
         typename DeviceType>
bool ImageManager::file_load_image(Image *img,
                                   ImageDataType type,
                                   int texture_limit,
                                   device_vector<DeviceType>& tex_img)
{
	const StorageType alpha_one = (FileFormat == TypeDesc::UINT8)? 255 : 1;
	ImageInput *in = NULL;
	int width, height, depth, components;
	if(!file_load_image_generic(img, &in, width, height, depth, components)) {
		return false;
	}
	/* Read RGBA pixels. */
	vector<StorageType> pixels_storage;
	StorageType *pixels;
	const size_t max_size = max(max(width, height), depth);
	if(max_size == 0) {
		/* Don't bother with invalid images. */
		return false;
	}
	if(texture_limit > 0 && max_size > texture_limit) {
		pixels_storage.resize(((size_t)width)*height*depth*4);
		pixels = &pixels_storage[0];
	}
	else {
		pixels = (StorageType*)tex_img.resize(width, height, depth);
	}
	if(pixels == NULL) {
		/* Could be that we've run out of memory. */
		return false;
	}
	bool cmyk = false;
	const size_t num_pixels = ((size_t)width) * height * depth;
	if(in) {
		StorageType *readpixels = pixels;
		vector<StorageType> tmppixels;
		if(components > 4) {
			tmppixels.resize(((size_t)width)*height*components);
			readpixels = &tmppixels[0];
		}
		if(depth <= 1) {
			size_t scanlinesize = ((size_t)width)*components*sizeof(StorageType);
			in->read_image(FileFormat,
			               (uchar*)readpixels + (height-1)*scanlinesize,
			               AutoStride,
			               -scanlinesize,
			               AutoStride);
		}
		else {
			in->read_image(FileFormat, (uchar*)readpixels);
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
		if(FileFormat == TypeDesc::FLOAT) {
			builtin_image_float_pixels_cb(img->filename,
			                              img->builtin_data,
			                              (float*)&pixels[0],
			                              num_pixels * components,
			                              img->builtin_free_cache);
		}
		else if(FileFormat == TypeDesc::UINT8) {
			builtin_image_pixels_cb(img->filename,
			                        img->builtin_data,
			                        (uchar*)&pixels[0],
			                        num_pixels * components,
			                        img->builtin_free_cache);
		}
		else {
			/* TODO(dingto): Support half for ImBuf. */
		}
	}
	/* Check if we actually have a float4 slot, in case components == 1,
	 * but device doesn't support single channel textures.
	 */
	bool is_rgba = (type == IMAGE_DATA_TYPE_FLOAT4 ||
	                type == IMAGE_DATA_TYPE_HALF4 ||
	                type == IMAGE_DATA_TYPE_BYTE4);
	if(is_rgba) {
		if(cmyk) {
			/* CMYK */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+2] = (pixels[i*4+2]*pixels[i*4+3])/255;
				pixels[i*4+1] = (pixels[i*4+1]*pixels[i*4+3])/255;
				pixels[i*4+0] = (pixels[i*4+0]*pixels[i*4+3])/255;
				pixels[i*4+3] = alpha_one;
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
				pixels[i*4+3] = alpha_one;
				pixels[i*4+2] = pixels[i*3+2];
				pixels[i*4+1] = pixels[i*3+1];
				pixels[i*4+0] = pixels[i*3+0];
			}
		}
		else if(components == 1) {
			/* grayscale */
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = alpha_one;
				pixels[i*4+2] = pixels[i];
				pixels[i*4+1] = pixels[i];
				pixels[i*4+0] = pixels[i];
			}
		}
		if(img->use_alpha == false) {
			for(size_t i = num_pixels-1, pixel = 0; pixel < num_pixels; pixel++, i--) {
				pixels[i*4+3] = alpha_one;
			}
		}
	}
	/* Make sure we don't have buggy values. */
	if(FileFormat == TypeDesc::FLOAT) {
		/* For RGBA buffers we put all channels to 0 if either of them is not
		 * finite. This way we avoid possible artifacts caused by fully changed
		 * hue.
		 */
		if(is_rgba) {
			for(size_t i = 0; i < num_pixels; i += 4) {
				StorageType *pixel = &pixels[i*4];
				if(!isfinite(pixel[0]) ||
				   !isfinite(pixel[1]) ||
				   !isfinite(pixel[2]) ||
				   !isfinite(pixel[3]))
				{
					pixel[0] = 0;
					pixel[1] = 0;
					pixel[2] = 0;
					pixel[3] = 0;
				}
			}
		}
		else {
			for(size_t i = 0; i < num_pixels; ++i) {
				StorageType *pixel = &pixels[i];
				if(!isfinite(pixel[0])) {
					pixel[0] = 0;
				}
			}
		}
	}
	/* Scale image down if needed. */
	if(pixels_storage.size() > 0) {
		float scale_factor = 1.0f;
		while(max_size * scale_factor > texture_limit) {
			scale_factor *= 0.5f;
		}
		VLOG(1) << "Scaling image " << img->filename
		        << " by a factor of " << scale_factor << ".";
		vector<StorageType> scaled_pixels;
		size_t scaled_width, scaled_height, scaled_depth;
		util_image_resize_pixels(pixels_storage,
		                         width, height, depth,
		                         is_rgba ? 4 : 1,
		                         scale_factor,
		                         &scaled_pixels,
		                         &scaled_width, &scaled_height, &scaled_depth);
		StorageType *texture_pixels = (StorageType*)tex_img.resize(scaled_width,
		                                                           scaled_height,
		                                                           scaled_depth);
		memcpy(texture_pixels,
		       &scaled_pixels[0],
		       scaled_pixels.size() * sizeof(StorageType));
	}
	return true;
}

void ImageManager::device_load_image(Device *device,
                                     DeviceScene *dscene,
                                     Scene *scene,
                                     ImageDataType type,
                                     int slot,
                                     Progress *progress)
{
	if(progress->get_cancel())
		return;

	Image *img = images[type][slot];

	if(osl_texture_system && !img->builtin_data)
		return;

	string filename = path_filename(images[type][slot]->filename);
	progress->set_status("Updating Images", "Loading " + filename);

	const int texture_limit = scene->params.texture_limit;

	/* Slot assignment */
	int flat_slot = type_index_to_flattened_slot(slot, type);

	string name = string_printf("__tex_image_%s_%03d", name_from_type(type).c_str(), flat_slot);

	if(type == IMAGE_DATA_TYPE_FLOAT4) {
		if(dscene->tex_float4_image[slot] == NULL)
			dscene->tex_float4_image[slot] = new device_vector<float4>();
		device_vector<float4>& tex_img = *dscene->tex_float4_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image<TypeDesc::FLOAT, float>(img,
		                                            type,
		                                            texture_limit,
		                                            tex_img))
		{
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
		if(dscene->tex_float_image[slot] == NULL)
			dscene->tex_float_image[slot] = new device_vector<float>();
		device_vector<float>& tex_img = *dscene->tex_float_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image<TypeDesc::FLOAT, float>(img,
		                                            type,
		                                            texture_limit,
		                                            tex_img))
		{
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
		if(dscene->tex_byte4_image[slot] == NULL)
			dscene->tex_byte4_image[slot] = new device_vector<uchar4>();
		device_vector<uchar4>& tex_img = *dscene->tex_byte4_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image<TypeDesc::UINT8, uchar>(img,
		                                            type,
		                                            texture_limit,
		                                            tex_img))
		{
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
		if(dscene->tex_byte_image[slot] == NULL)
			dscene->tex_byte_image[slot] = new device_vector<uchar>();
		device_vector<uchar>& tex_img = *dscene->tex_byte_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image<TypeDesc::UINT8, uchar>(img,
		                                            type,
		                                            texture_limit,
		                                            tex_img)) {
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
		if(dscene->tex_half4_image[slot] == NULL)
			dscene->tex_half4_image[slot] = new device_vector<half4>();
		device_vector<half4>& tex_img = *dscene->tex_half4_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image<TypeDesc::HALF, half>(img,
		                                          type,
		                                          texture_limit,
		                                          tex_img)) {
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
		if(dscene->tex_half_image[slot] == NULL)
			dscene->tex_half_image[slot] = new device_vector<half>();
		device_vector<half>& tex_img = *dscene->tex_half_image[slot];

		if(tex_img.device_pointer) {
			thread_scoped_lock device_lock(device_mutex);
			device->tex_free(tex_img);
		}

		if(!file_load_image<TypeDesc::HALF, half>(img,
		                                          type,
		                                          texture_limit,
		                                          tex_img)) {
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
		else {
			device_memory *tex_img = NULL;
			switch(type) {
				case IMAGE_DATA_TYPE_FLOAT4:
					if(slot >= dscene->tex_float4_image.size()) {
						break;
					}
					tex_img = dscene->tex_float4_image[slot];
					dscene->tex_float4_image[slot] = NULL;
					break;
				case IMAGE_DATA_TYPE_BYTE4:
					if(slot >= dscene->tex_byte4_image.size()) {
						break;
					}
					tex_img = dscene->tex_byte4_image[slot];
					dscene->tex_byte4_image[slot]= NULL;
					break;
				case IMAGE_DATA_TYPE_HALF4:
					if(slot >= dscene->tex_half4_image.size()) {
						break;
					}
					tex_img = dscene->tex_half4_image[slot];
					dscene->tex_half4_image[slot]= NULL;
					break;
				case IMAGE_DATA_TYPE_FLOAT:
					if(slot >= dscene->tex_float_image.size()) {
						break;
					}
					tex_img = dscene->tex_float_image[slot];
					dscene->tex_float_image[slot] = NULL;
					break;
				case IMAGE_DATA_TYPE_BYTE:
					if(slot >= dscene->tex_byte_image.size()) {
						break;
					}
					tex_img = dscene->tex_byte_image[slot];
					dscene->tex_byte_image[slot]= NULL;
					break;
				case IMAGE_DATA_TYPE_HALF:
					if(slot >= dscene->tex_half_image.size()) {
						break;
					}
					tex_img = dscene->tex_half_image[slot];
					dscene->tex_half_image[slot]= NULL;
					break;
				default:
					assert(0);
					tex_img = NULL;
			}
			if(tex_img) {
				if(tex_img->device_pointer) {
					thread_scoped_lock device_lock(device_mutex);
					device->tex_free(*tex_img);
				}

				delete tex_img;
			}
		}

		delete images[type][slot];
		images[type][slot] = NULL;
		--tex_num_images[type];
	}
}

void ImageManager::device_prepare_update(DeviceScene *dscene)
{
	for(int type = 0; type < IMAGE_DATA_NUM_TYPES; type++) {
		switch(type) {
			case IMAGE_DATA_TYPE_FLOAT4:
				if(dscene->tex_float4_image.size() <= tex_num_images[IMAGE_DATA_TYPE_FLOAT4])
					dscene->tex_float4_image.resize(tex_num_images[IMAGE_DATA_TYPE_FLOAT4]);
				break;
			case IMAGE_DATA_TYPE_BYTE4:
				if(dscene->tex_byte4_image.size() <= tex_num_images[IMAGE_DATA_TYPE_BYTE4])
					dscene->tex_byte4_image.resize(tex_num_images[IMAGE_DATA_TYPE_BYTE4]);
				break;
			case IMAGE_DATA_TYPE_HALF4:
				if(dscene->tex_half4_image.size() <= tex_num_images[IMAGE_DATA_TYPE_HALF4])
					dscene->tex_half4_image.resize(tex_num_images[IMAGE_DATA_TYPE_HALF4]);
				break;
			case IMAGE_DATA_TYPE_BYTE:
				if(dscene->tex_byte_image.size() <= tex_num_images[IMAGE_DATA_TYPE_BYTE])
					dscene->tex_byte_image.resize(tex_num_images[IMAGE_DATA_TYPE_BYTE]);
				break;
			case IMAGE_DATA_TYPE_FLOAT:
				if(dscene->tex_float_image.size() <= tex_num_images[IMAGE_DATA_TYPE_FLOAT])
					dscene->tex_float_image.resize(tex_num_images[IMAGE_DATA_TYPE_FLOAT]);
				break;
			case IMAGE_DATA_TYPE_HALF:
				if(dscene->tex_half_image.size() <= tex_num_images[IMAGE_DATA_TYPE_HALF])
					dscene->tex_half_image.resize(tex_num_images[IMAGE_DATA_TYPE_HALF]);
				break;
		}
	}
}

void ImageManager::device_update(Device *device,
                                 DeviceScene *dscene,
                                 Scene *scene,
                                 Progress& progress)
{
	if(!need_update) {
		return;
	}

	/* Make sure arrays are proper size. */
	device_prepare_update(dscene);

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
					pool.push(function_bind(&ImageManager::device_load_image,
					                        this,
					                        device,
					                        dscene,
					                        scene,
					                        (ImageDataType)type,
					                        slot,
					                        &progress));
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
                                      Scene *scene,
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
			                  scene,
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
	 * bit 1 + 2 + 3 -> Extension
	 */
	if(images[type][slot]->interpolation == INTERPOLATION_CLOSEST) {
		options |= (1 << 0);
	}
	if(images[type][slot]->extension == EXTENSION_REPEAT) {
		options |= (1 << 1);
	}
	else if(images[type][slot]->extension == EXTENSION_EXTEND) {
		options |= (1 << 2);
	}
	else /* EXTENSION_CLIP */ {
		options |= (1 << 3);
	}
	return options;
}

template<typename T>
void ImageManager::device_pack_images_type(
        ImageDataType type,
        const vector<device_vector<T>*>& cpu_textures,
        device_vector<T> *device_image,
        uint4 *info)
{
	size_t size = 0, offset = 0;
	/* First step is to calculate size of the texture we need. */
	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(images[type][slot] == NULL) {
			continue;
		}
		device_vector<T>& tex_img = *cpu_textures[slot];
		size += tex_img.size();
	}
	/* Now we know how much memory we need, so we can allocate and fill. */
	T *pixels = device_image->resize(size);
	for(size_t slot = 0; slot < images[type].size(); slot++) {
		if(images[type][slot] == NULL) {
			continue;
		}
		device_vector<T>& tex_img = *cpu_textures[slot];
		uint8_t options = pack_image_options(type, slot);
		const int index = type_index_to_flattened_slot(slot, type) * 2;
		info[index] = make_uint4(tex_img.data_width,
		                         tex_img.data_height,
		                         offset,
		                         options);
		info[index+1] = make_uint4(tex_img.data_depth, 0, 0, 0);
		memcpy(pixels + offset,
		       (void*)tex_img.data_pointer,
		       tex_img.memory_size());
		offset += tex_img.size();
	}
}

void ImageManager::device_pack_images(Device *device,
                                      DeviceScene *dscene,
                                      Progress& /*progess*/)
{
	/* For OpenCL, we pack all image textures into a single large texture, and
	 * do our own interpolation in the kernel.
	 */

	/* TODO(sergey): This will over-allocate a bit, but this is constant memory
	 * so should be fine for a short term.
	 */
	const size_t info_size = max4(max_flattened_slot(IMAGE_DATA_TYPE_FLOAT4),
	                              max_flattened_slot(IMAGE_DATA_TYPE_BYTE4),
	                              max_flattened_slot(IMAGE_DATA_TYPE_FLOAT),
	                              max_flattened_slot(IMAGE_DATA_TYPE_BYTE));
	uint4 *info = dscene->tex_image_packed_info.resize(info_size*2);

	/* Pack byte4 textures. */
	device_pack_images_type(IMAGE_DATA_TYPE_BYTE4,
	                        dscene->tex_byte4_image,
	                        &dscene->tex_image_byte4_packed,
	                        info);
	/* Pack float4 textures. */
	device_pack_images_type(IMAGE_DATA_TYPE_FLOAT4,
	                        dscene->tex_float4_image,
	                        &dscene->tex_image_float4_packed,
	                        info);
	/* Pack byte textures. */
	device_pack_images_type(IMAGE_DATA_TYPE_BYTE,
	                        dscene->tex_byte_image,
	                        &dscene->tex_image_byte_packed,
	                        info);
	/* Pack float textures. */
	device_pack_images_type(IMAGE_DATA_TYPE_FLOAT,
	                        dscene->tex_float_image,
	                        &dscene->tex_image_float_packed,
	                        info);

	/* Push textures to the device. */
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

	dscene->tex_float4_image.clear();
	dscene->tex_byte4_image.clear();
	dscene->tex_half4_image.clear();
	dscene->tex_float_image.clear();
	dscene->tex_byte_image.clear();
	dscene->tex_half_image.clear();

	device->tex_free(dscene->tex_image_float4_packed);
	device->tex_free(dscene->tex_image_byte4_packed);
	device->tex_free(dscene->tex_image_float_packed);
	device->tex_free(dscene->tex_image_byte_packed);
	device->tex_free(dscene->tex_image_packed_info);

	dscene->tex_image_float4_packed.clear();
	dscene->tex_image_byte4_packed.clear();
	dscene->tex_image_float_packed.clear();
	dscene->tex_image_byte_packed.clear();
	dscene->tex_image_packed_info.clear();
}

CCL_NAMESPACE_END


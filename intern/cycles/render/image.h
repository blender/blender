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

#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "device_memory.h"

#include "util_string.h"
#include "util_thread.h"
#include "util_vector.h"

#include "kernel_types.h"  /* for TEX_NUM_FLOAT_IMAGES */

CCL_NAMESPACE_BEGIN

#define TEX_NUM_IMAGES			95
#define TEX_IMAGE_BYTE_START	TEX_NUM_FLOAT_IMAGES

#define TEX_EXTENDED_NUM_FLOAT_IMAGES	1024
#define TEX_EXTENDED_NUM_IMAGES			1024
#define TEX_EXTENDED_IMAGE_BYTE_START	TEX_EXTENDED_NUM_FLOAT_IMAGES

/* color to use when textures are not found */
#define TEX_IMAGE_MISSING_R 1
#define TEX_IMAGE_MISSING_G 0
#define TEX_IMAGE_MISSING_B 1
#define TEX_IMAGE_MISSING_A 1

class Device;
class DeviceScene;
class Progress;

class ImageManager {
public:
	ImageManager();
	~ImageManager();

	int add_image(const string& filename, void *builtin_data, bool animated, bool& is_float, bool& is_linear);
	void remove_image(const string& filename, void *builtin_data);
	bool is_float_image(const string& filename, void *builtin_data, bool& is_linear);

	void device_update(Device *device, DeviceScene *dscene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void set_osl_texture_system(void *texture_system);
	void set_pack_images(bool pack_images_);
	void set_extended_image_limits(void);
	bool set_animation_frame_update(int frame);

	bool need_update;

	boost::function<void(const string &filename, void *data, bool &is_float, int &width, int &height, int &channels)> builtin_image_info_cb;
	boost::function<bool(const string &filename, void *data, unsigned char *pixels)> builtin_image_pixels_cb;
	boost::function<bool(const string &filename, void *data, float *pixels)> builtin_image_float_pixels_cb;
private:
	int tex_num_images;
	int tex_num_float_images;
	int tex_image_byte_start;
	thread_mutex device_mutex;
	int animation_frame;

	struct Image {
		string filename;
		void *builtin_data;

		bool need_load;
		bool animated;
		int users;
	};

	vector<Image*> images;
	vector<Image*> float_images;
	void *osl_texture_system;
	bool pack_images;

	bool file_load_image(Image *img, device_vector<uchar4>& tex_img);
	bool file_load_float_image(Image *img, device_vector<float4>& tex_img);

	void device_load_image(Device *device, DeviceScene *dscene, int slot, Progress *progess);
	void device_free_image(Device *device, DeviceScene *dscene, int slot);

	void device_pack_images(Device *device, DeviceScene *dscene, Progress& progess);
};

CCL_NAMESPACE_END

#endif /* __IMAGE_H__ */


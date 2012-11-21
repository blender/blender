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

#define TEX_EXTENDED_NUM_FLOAT_IMAGES	5
#define TEX_EXTENDED_NUM_IMAGES			512
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

	int add_image(const string& filename, bool animated, bool& is_float);
	void remove_image(const string& filename);
	bool is_float_image(const string& filename);

	void device_update(Device *device, DeviceScene *dscene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void set_osl_texture_system(void *texture_system);
	void set_pack_images(bool pack_images_);
	void set_extended_image_limits(void);
	bool set_animation_frame_update(int frame);

	bool need_update;

private:
	int tex_num_images;
	int tex_num_float_images;
	int tex_image_byte_start;
	thread_mutex device_mutex;
	int animation_frame;

	struct Image {
		string filename;

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


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
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

#define TEX_NUM_FLOAT_IMAGES	5
#define TEX_NUM_IMAGES			95
#define TEX_IMAGE_MAX			(TEX_NUM_IMAGES + TEX_NUM_FLOAT_IMAGES)
#define TEX_IMAGE_FLOAT_START	TEX_NUM_IMAGES

class Device;
class DeviceScene;
class Progress;

class ImageManager {
public:
	ImageManager();
	~ImageManager();

	int add_image(const string& filename, bool& is_float);
	void remove_image(const string& filename);

	void device_update(Device *device, DeviceScene *dscene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene);

	void set_osl_texture_system(void *texture_system);

	bool need_update;

private:
	struct Image {
		string filename;

		bool need_load;
		int users;
	};

	vector<Image*> images;
	vector<Image*> float_images;
	void *osl_texture_system;

	bool file_load_image(Image *img, device_vector<uchar4>& tex_img);
	bool file_load_float_image(Image *img, device_vector<float4>& tex_img);

	void device_load_image(Device *device, DeviceScene *dscene, int slot);
	void device_free_image(Device *device, DeviceScene *dscene, int slot);
};

CCL_NAMESPACE_END

#endif /* __IMAGE_H__ */


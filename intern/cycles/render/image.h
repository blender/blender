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

#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "device/device.h"
#include "device/device_memory.h"

#include "util/util_image.h"
#include "util/util_string.h"
#include "util/util_thread.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

class ImageManager {
public:
	explicit ImageManager(const DeviceInfo& info);
	~ImageManager();

	int add_image(const string& filename,
	              void *builtin_data,
	              bool animated,
	              float frame,
	              bool& is_float,
	              bool& is_linear,
	              InterpolationType interpolation,
	              ExtensionType extension,
	              bool use_alpha);
	void remove_image(int flat_slot);
	void remove_image(const string& filename,
	                  void *builtin_data,
	                  InterpolationType interpolation,
	                  ExtensionType extension,
	                  bool use_alpha);
	void tag_reload_image(const string& filename,
	                      void *builtin_data,
	                      InterpolationType interpolation,
	                      ExtensionType extension,
	                      bool use_alpha);
	ImageDataType get_image_metadata(const string& filename,
	                                 void *builtin_data,
	                                 bool& is_linear,
	                                 bool& builtin_free_cache);

	void device_prepare_update(DeviceScene *dscene);
	void device_update(Device *device,
	                   DeviceScene *dscene,
	                   Scene *scene,
	                   Progress& progress);
	void device_update_slot(Device *device,
	                        DeviceScene *dscene,
	                        Scene *scene,
	                        int flat_slot,
	                        Progress *progress);
	void device_free(Device *device, DeviceScene *dscene);
	void device_free_builtin(Device *device, DeviceScene *dscene);

	void set_osl_texture_system(void *texture_system);
	void set_pack_images(bool pack_images_);
	bool set_animation_frame_update(int frame);

	bool need_update;

	/* NOTE: Here pixels_size is a size of storage, which equals to
	 *       width * height * depth.
	 *       Use this to avoid some nasty memory corruptions.
	 */
	function<void(const string &filename,
	              void *data,
	              bool &is_float,
	              int &width,
	              int &height,
	              int &depth,
	              int &channels,
	              bool &free_cache)> builtin_image_info_cb;
	function<bool(const string &filename,
	              void *data,
	              unsigned char *pixels,
	              const size_t pixels_size,
	              const bool free_cache)> builtin_image_pixels_cb;
	function<bool(const string &filename,
	              void *data,
	              float *pixels,
	              const size_t pixels_size,
	              const bool free_cache)> builtin_image_float_pixels_cb;

	struct Image {
		string filename;
		void *builtin_data;
		bool builtin_free_cache;

		bool use_alpha;
		bool need_load;
		bool animated;
		float frame;
		InterpolationType interpolation;
		ExtensionType extension;

		int users;
	};

private:
	int tex_num_images[IMAGE_DATA_NUM_TYPES];
	int max_num_images;
	bool has_half_images;
	bool cuda_fermi_limits;

	thread_mutex device_mutex;
	int animation_frame;

	vector<Image*> images[IMAGE_DATA_NUM_TYPES];
	void *osl_texture_system;
	bool pack_images;

	bool file_load_image_generic(Image *img,
	                             ImageInput **in,
	                             int &width,
	                             int &height,
	                             int &depth,
	                             int &components);

	template<TypeDesc::BASETYPE FileFormat,
	         typename StorageType,
	         typename DeviceType>
	bool file_load_image(Image *img,
	                     ImageDataType type,
	                     int texture_limit,
	                     device_vector<DeviceType>& tex_img);

	int max_flattened_slot(ImageDataType type);
	int type_index_to_flattened_slot(int slot, ImageDataType type);
	int flattened_slot_to_type_index(int flat_slot, ImageDataType *type);
	string name_from_type(int type);

	uint8_t pack_image_options(ImageDataType type, size_t slot);

	void device_load_image(Device *device,
	                       DeviceScene *dscene,
	                       Scene *scene,
	                       ImageDataType type,
	                       int slot,
	                       Progress *progess);
	void device_free_image(Device *device,
	                       DeviceScene *dscene,
	                       ImageDataType type,
	                       int slot);

	template<typename T>
	void device_pack_images_type(
	        ImageDataType type,
	        const vector<device_vector<T>*>& cpu_textures,
	        device_vector<T> *device_image,
	        uint4 *info);

	void device_pack_images(Device *device,
	                        DeviceScene *dscene,
	                        Progress& progess);
};

CCL_NAMESPACE_END

#endif /* __IMAGE_H__ */


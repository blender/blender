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
class Progress;
class RenderStats;
class Scene;

class ImageMetaData {
public:
	/* Must be set by image file or builtin callback. */
	bool is_float, is_half;
	int channels;
	size_t width, height, depth;
	bool builtin_free_cache;

	/* Automatically set. */
	ImageDataType type;
	bool is_linear;
};

class ImageManager {
public:
	explicit ImageManager(const DeviceInfo& info);
	~ImageManager();

	int add_image(const string& filename,
	              void *builtin_data,
	              bool animated,
	              float frame,
	              InterpolationType interpolation,
	              ExtensionType extension,
	              bool use_alpha,
	              ImageMetaData& metadata);
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
	bool get_image_metadata(const string& filename,
	                        void *builtin_data,
	                        ImageMetaData& metadata);
	bool get_image_metadata(int flat_slot,
	                        ImageMetaData& metadata);

	void device_update(Device *device,
	                   Scene *scene,
	                   Progress& progress);
	void device_update_slot(Device *device,
	                        Scene *scene,
	                        int flat_slot,
	                        Progress *progress);
	void device_free(Device *device);

	void device_load_builtin(Device *device,
	                         Scene *scene,
	                         Progress& progress);
	void device_free_builtin(Device *device);

	void set_osl_texture_system(void *texture_system);
	bool set_animation_frame_update(int frame);

	device_memory *image_memory(int flat_slot);

	void collect_statistics(RenderStats *stats);

	bool need_update;

	/* NOTE: Here pixels_size is a size of storage, which equals to
	 *       width * height * depth.
	 *       Use this to avoid some nasty memory corruptions.
	 */
	function<void(const string &filename,
	              void *data,
	              ImageMetaData& metadata)> builtin_image_info_cb;
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
		ImageMetaData metadata;

		bool use_alpha;
		bool need_load;
		bool animated;
		float frame;
		InterpolationType interpolation;
		ExtensionType extension;

		string mem_name;
		device_memory *mem;

		int users;
	};

private:
	int tex_num_images[IMAGE_DATA_NUM_TYPES];
	int max_num_images;
	bool has_half_images;

	thread_mutex device_mutex;
	int animation_frame;

	vector<Image*> images[IMAGE_DATA_NUM_TYPES];
	void *osl_texture_system;

	bool file_load_image_generic(Image *img,
	                             ImageInput **in);

	template<TypeDesc::BASETYPE FileFormat,
	         typename StorageType,
	         typename DeviceType>
	bool file_load_image(Image *img,
	                     ImageDataType type,
	                     int texture_limit,
	                     device_vector<DeviceType>& tex_img);

	void device_load_image(Device *device,
	                       Scene *scene,
	                       ImageDataType type,
	                       int slot,
	                       Progress *progress);
	void device_free_image(Device *device,
	                       ImageDataType type,
	                       int slot);
};

CCL_NAMESPACE_END

#endif /* __IMAGE_H__ */

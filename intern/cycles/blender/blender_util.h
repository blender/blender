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

#ifndef __BLENDER_UTIL_H__
#define __BLENDER_UTIL_H__

#include "util_map.h"
#include "util_path.h"
#include "util_set.h"
#include "util_transform.h"
#include "util_types.h"
#include "util_vector.h"

/* Hacks to hook into Blender API
 * todo: clean this up ... */

extern "C" {
void BLI_timestr(double _time, char *str, size_t maxlen);
void BKE_image_user_frame_calc(void *iuser, int cfra, int fieldnr);
void BKE_image_user_file_path(void *iuser, void *ima, char *path);
unsigned char *BKE_image_get_pixels_for_frame(void *image, int frame);
float *BKE_image_get_float_pixels_for_frame(void *image, int frame);
}

CCL_NAMESPACE_BEGIN

void python_thread_state_save(void **python_thread_state);
void python_thread_state_restore(void **python_thread_state);

static inline BL::Mesh object_to_mesh(BL::BlendData data, BL::Object object, BL::Scene scene, bool apply_modifiers, bool render, bool calc_undeformed)
{
	BL::Mesh me = data.meshes.new_from_object(scene, object, apply_modifiers, (render)? 2: 1, false, calc_undeformed);
	if ((bool)me) {
		if (me.use_auto_smooth()) {
			me.calc_normals_split(me.auto_smooth_angle());
		}
		me.calc_tessface();
	}
	return me;
}

static inline void colorramp_to_array(BL::ColorRamp ramp, float4 *data, int size)
{
	for(int i = 0; i < size; i++) {
		float color[4];

		ramp.evaluate((float)i/(float)(size-1), color);
		data[i] = make_float4(color[0], color[1], color[2], color[3]);
	}
}

static inline void curvemapping_color_to_array(BL::CurveMapping cumap, float4 *data, int size, bool rgb_curve)
{
	cumap.update();

	BL::CurveMap mapR = cumap.curves[0];
	BL::CurveMap mapG = cumap.curves[1];
	BL::CurveMap mapB = cumap.curves[2];

	if(rgb_curve) {
		BL::CurveMap mapI = cumap.curves[3];

		for(int i = 0; i < size; i++) {
			float t = (float)i/(float)(size-1);

			data[i][0] = mapR.evaluate(mapI.evaluate(t));
			data[i][1] = mapG.evaluate(mapI.evaluate(t));
			data[i][2] = mapB.evaluate(mapI.evaluate(t));
		}
	}
	else {
		for(int i = 0; i < size; i++) {
			float t = (float)i/(float)(size-1);

			data[i][0] = mapR.evaluate(t);
			data[i][1] = mapG.evaluate(t);
			data[i][2] = mapB.evaluate(t);
		}
	}
}

static inline bool BKE_object_is_modified(BL::Object self, BL::Scene scene, bool preview)
{
	return self.is_modified(scene, (preview)? (1<<0): (1<<1))? true: false;
}

static inline bool BKE_object_is_deform_modified(BL::Object self, BL::Scene scene, bool preview)
{
	return self.is_deform_modified(scene, (preview)? (1<<0): (1<<1))? true: false;
}

static inline int render_resolution_x(BL::RenderSettings b_render)
{
	return b_render.resolution_x()*b_render.resolution_percentage()/100;
}

static inline int render_resolution_y(BL::RenderSettings b_render)
{
	return b_render.resolution_y()*b_render.resolution_percentage()/100;
}

static inline string image_user_file_path(BL::ImageUser iuser, BL::Image ima, int cfra)
{
	char filepath[1024];
	BKE_image_user_frame_calc(iuser.ptr.data, cfra, 0);
	BKE_image_user_file_path(iuser.ptr.data, ima.ptr.data, filepath);
	return string(filepath);
}

static inline int image_user_frame_number(BL::ImageUser iuser, int cfra)
{
	BKE_image_user_frame_calc(iuser.ptr.data, cfra, 0);
	return iuser.frame_current();
}

static inline unsigned char *image_get_pixels_for_frame(BL::Image image, int frame)
{
	return BKE_image_get_pixels_for_frame(image.ptr.data, frame);
}

static inline float *image_get_float_pixels_for_frame(BL::Image image, int frame)
{
	return BKE_image_get_float_pixels_for_frame(image.ptr.data, frame);
}

/* Utilities */

static inline Transform get_transform(BL::Array<float, 16> array)
{
	Transform tfm;

	/* we assume both types to be just 16 floats, and transpose because blender
	 * use column major matrix order while we use row major */
	memcpy(&tfm, &array, sizeof(float)*16);
	tfm = transform_transpose(tfm);

	return tfm;
}

static inline float2 get_float2(BL::Array<float, 2> array)
{
	return make_float2(array[0], array[1]);
}

static inline float3 get_float3(BL::Array<float, 2> array)
{
	return make_float3(array[0], array[1], 0.0f);
}

static inline float3 get_float3(BL::Array<float, 3> array)
{
	return make_float3(array[0], array[1], array[2]);
}

static inline float3 get_float3(BL::Array<float, 4> array)
{
	return make_float3(array[0], array[1], array[2]);
}

static inline float4 get_float4(BL::Array<float, 4> array)
{
	return make_float4(array[0], array[1], array[2], array[3]);
}

static inline int3 get_int3(BL::Array<int, 3> array)
{
	return make_int3(array[0], array[1], array[2]);
}

static inline int4 get_int4(BL::Array<int, 4> array)
{
	return make_int4(array[0], array[1], array[2], array[3]);
}

static inline uint get_layer(BL::Array<int, 20> array)
{
	uint layer = 0;

	for(uint i = 0; i < 20; i++)
		if(array[i])
			layer |= (1 << i);
	
	return layer;
}

static inline uint get_layer(BL::Array<int, 20> array, BL::Array<int, 8> local_array, bool use_local, bool is_light = false)
{
	uint layer = 0;

	for(uint i = 0; i < 20; i++)
		if(array[i])
			layer |= (1 << i);

	if(is_light) {
		/* consider lamps on all local view layers */
		for(uint i = 0; i < 8; i++)
			layer |= (1 << (20+i));
	}
	else {
		for(uint i = 0; i < 8; i++)
			if(local_array[i])
				layer |= (1 << (20+i));
	}

	/* we don't have spare bits for localview (normally 20-28) because
	 * PATH_RAY_LAYER_SHIFT uses 20-32. So - check if we have localview and if
	 * so, shift local view bits down to 1-8, since this is done for the view
	 * port only - it should be OK and not conflict with render layers. */
	if(use_local)
		layer >>= 20;

	return layer;
}

static inline float3 get_float3(PointerRNA& ptr, const char *name)
{
	float3 f;
	RNA_float_get_array(&ptr, name, &f.x);
	return f;
}

static inline void set_float3(PointerRNA& ptr, const char *name, float3 value)
{
	RNA_float_set_array(&ptr, name, &value.x);
}

static inline float4 get_float4(PointerRNA& ptr, const char *name)
{
	float4 f;
	RNA_float_get_array(&ptr, name, &f.x);
	return f;
}

static inline void set_float4(PointerRNA& ptr, const char *name, float4 value)
{
	RNA_float_set_array(&ptr, name, &value.x);
}

static inline bool get_boolean(PointerRNA& ptr, const char *name)
{
	return RNA_boolean_get(&ptr, name)? true: false;
}

static inline void set_boolean(PointerRNA& ptr, const char *name, bool value)
{
	RNA_boolean_set(&ptr, name, (int)value);
}

static inline float get_float(PointerRNA& ptr, const char *name)
{
	return RNA_float_get(&ptr, name);
}

static inline void set_float(PointerRNA& ptr, const char *name, float value)
{
	RNA_float_set(&ptr, name, value);
}

static inline int get_int(PointerRNA& ptr, const char *name)
{
	return RNA_int_get(&ptr, name);
}

static inline void set_int(PointerRNA& ptr, const char *name, int value)
{
	RNA_int_set(&ptr, name, value);
}

static inline int get_enum(PointerRNA& ptr, const char *name)
{
	return RNA_enum_get(&ptr, name);
}

static inline string get_enum_identifier(PointerRNA& ptr, const char *name)
{
	PropertyRNA *prop = RNA_struct_find_property(&ptr, name);
	const char *identifier = "";
	int value = RNA_property_enum_get(&ptr, prop);

	RNA_property_enum_identifier(NULL, &ptr, prop, value, &identifier);

	return string(identifier);
}

static inline void set_enum(PointerRNA& ptr, const char *name, int value)
{
	RNA_enum_set(&ptr, name, value);
}

static inline void set_enum(PointerRNA& ptr, const char *name, const string &identifier)
{
	RNA_enum_set_identifier(&ptr, name, identifier.c_str());
}

static inline string get_string(PointerRNA& ptr, const char *name)
{
	char cstrbuf[1024];
	char *cstr = RNA_string_get_alloc(&ptr, name, cstrbuf, sizeof(cstrbuf));
	string str(cstr);
	if (cstr != cstrbuf)
		MEM_freeN(cstr);
	
	return str;
}

static inline void set_string(PointerRNA& ptr, const char *name, const string &value)
{
	RNA_string_set(&ptr, name, value.c_str());
}

/* Relative Paths */

static inline string blender_absolute_path(BL::BlendData b_data, BL::ID b_id, const string& path)
{
	if(path.size() >= 2 && path[0] == '/' && path[1] == '/') {
		string dirname;
		
		if(b_id.library())
			dirname = blender_absolute_path(b_data, b_id.library(), b_id.library().filepath());
		else
			dirname = b_data.filepath();

		return path_join(path_dirname(dirname), path.substr(2));
	}

	return path;
}

/* Texture Space */

static inline void mesh_texture_space(BL::Mesh b_mesh, float3& loc, float3& size)
{
	loc = get_float3(b_mesh.texspace_location());
	size = get_float3(b_mesh.texspace_size());

	if(size.x != 0.0f) size.x = 0.5f/size.x;
	if(size.y != 0.0f) size.y = 0.5f/size.y;
	if(size.z != 0.0f) size.z = 0.5f/size.z;

	loc = loc*size - make_float3(0.5f, 0.5f, 0.5f);
}

/* object used for motion blur */
static inline bool object_use_motion(BL::Object b_ob)
{
	PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
	bool use_motion = get_boolean(cobject, "use_motion_blur");
	
	return use_motion;
}

/* object motion steps */
static inline uint object_motion_steps(BL::Object b_ob)
{
	PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
	uint steps = get_int(cobject, "motion_steps");

	/* use uneven number of steps so we get one keyframe at the current frame,
	 * and ue 2^(steps - 1) so objects with more/fewer steps still have samples
	 * at the same times, to avoid sampling at many different times */
	return (2 << (steps - 1)) + 1;
}

/* object uses deformation motion blur */
static inline bool object_use_deform_motion(BL::Object b_ob)
{
	PointerRNA cobject = RNA_pointer_get(&b_ob.ptr, "cycles");
	bool use_deform_motion = get_boolean(cobject, "use_deform_motion");
	
	return use_deform_motion;
}

static inline BL::SmokeDomainSettings object_smoke_domain_find(BL::Object b_ob)
{
	BL::Object::modifiers_iterator b_mod;

	for(b_ob.modifiers.begin(b_mod); b_mod != b_ob.modifiers.end(); ++b_mod) {
		if (b_mod->is_a(&RNA_SmokeModifier)) {
			BL::SmokeModifier b_smd(*b_mod);

			if(b_smd.smoke_type() == BL::SmokeModifier::smoke_type_DOMAIN)
				return b_smd.domain_settings();
		}
	}
	
	return BL::SmokeDomainSettings(PointerRNA_NULL);
}

/* ID Map
 *
 * Utility class to keep in sync with blender data.
 * Used for objects, meshes, lights and shaders. */

template<typename K, typename T>
class id_map {
public:
	id_map(vector<T*> *scene_data_)
	{
		scene_data = scene_data_;
	}

	T *find(BL::ID id)
	{
		return find(id.ptr.id.data);
	}

	T *find(const K& key)
	{
		if(b_map.find(key) != b_map.end()) {
			T *data = b_map[key];
			return data;
		}

		return NULL;
	}

	void set_recalc(BL::ID id)
	{
		b_recalc.insert(id.ptr.data);
	}

	bool has_recalc()
	{
		return !(b_recalc.empty());
	}

	void pre_sync()
	{
		used_set.clear();
	}

	bool sync(T **r_data, BL::ID id)
	{
		return sync(r_data, id, id, id.ptr.id.data);
	}

	bool sync(T **r_data, BL::ID id, BL::ID parent, const K& key)
	{
		T *data = find(key);
		bool recalc;

		if(!data) {
			/* add data if it didn't exist yet */
			data = new T();
			scene_data->push_back(data);
			b_map[key] = data;
			recalc = true;
		}
		else {
			recalc = (b_recalc.find(id.ptr.data) != b_recalc.end());
			if(parent.ptr.data)
				recalc = recalc || (b_recalc.find(parent.ptr.data) != b_recalc.end());
		}

		used(data);

		*r_data = data;
		return recalc;
	}

	bool is_used(const K& key)
	{
		T *data = find(key);
		return (data) ? used_set.find(data) != used_set.end() : false;
	}

	void used(T *data)
	{
		/* tag data as still in use */
		used_set.insert(data);
	}

	void set_default(T *data)
	{
		b_map[NULL] = data;
	}

	bool post_sync(bool do_delete = true)
	{
		/* remove unused data */
		vector<T*> new_scene_data;
		typename vector<T*>::iterator it;
		bool deleted = false;

		for(it = scene_data->begin(); it != scene_data->end(); it++) {
			T *data = *it;

			if(do_delete && used_set.find(data) == used_set.end()) {
				delete data;
				deleted = true;
			}
			else
				new_scene_data.push_back(data);
		}

		*scene_data = new_scene_data;

		/* update mapping */
		map<K, T*> new_map;
		typedef pair<const K, T*> TMapPair;
		typename map<K, T*>::iterator jt;

		for(jt = b_map.begin(); jt != b_map.end(); jt++) {
			TMapPair& pair = *jt;

			if(used_set.find(pair.second) != used_set.end())
				new_map[pair.first] = pair.second;
		}

		used_set.clear();
		b_recalc.clear();
		b_map = new_map;

		return deleted;
	}

protected:
	vector<T*> *scene_data;
	map<K, T*> b_map;
	set<T*> used_set;
	set<void*> b_recalc;
};

/* Object Key */

enum { OBJECT_PERSISTENT_ID_SIZE = 8 };

struct ObjectKey {
	void *parent;
	int id[OBJECT_PERSISTENT_ID_SIZE];
	void *ob;

	ObjectKey(void *parent_, int id_[OBJECT_PERSISTENT_ID_SIZE], void *ob_)
	: parent(parent_), ob(ob_)
	{
		if(id_)
			memcpy(id, id_, sizeof(id));
		else
			memset(id, 0, sizeof(id));
	}

	bool operator<(const ObjectKey& k) const
	{
		if(ob < k.ob) {
			return true;
		}
		else if(ob == k.ob) {
			if(parent < k.parent)
				return true;
			else if(parent == k.parent)
				return memcmp(id, k.id, sizeof(id)) < 0;
		}

		return false;
	}
};

/* Particle System Key */

struct ParticleSystemKey {
	void *ob;
	int id[OBJECT_PERSISTENT_ID_SIZE];

	ParticleSystemKey(void *ob_, int id_[OBJECT_PERSISTENT_ID_SIZE])
	: ob(ob_)
	{
		if(id_)
			memcpy(id, id_, sizeof(id));
		else
			memset(id, 0, sizeof(id));
	}

	bool operator<(const ParticleSystemKey& k) const
	{
		/* first id is particle index, we don't compare that */
		if(ob < k.ob)
			return true;
		else if(ob == k.ob)
			return memcmp(id+1, k.id+1, sizeof(int)*(OBJECT_PERSISTENT_ID_SIZE-1)) < 0;

		return false;
	}
};

CCL_NAMESPACE_END

#endif /* __BLENDER_UTIL_H__ */


/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <type_traits>

#include "scene/image.h"

#include "kernel/types.h"

#include "util/implicit_sharing.h"
#include "util/list.h"
#include "util/param.h"
#include "util/set.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Attribute;
class AttributeRequest;
class AttributeRequestSet;
class AttributeSet;
class ImageHandle;
class Geometry;
class Hair;
class Mesh;
struct Transform;

/* AttrKernelDataType.
 *
 * The data type of the device arrays storing the attribute's data. Those data types are different
 * than the ones for attributes as some attribute types are stored in the same array, e.g. Point,
 * Vector, and Transform are all stored as float3 in the kernel.
 *
 * The values of this enumeration are also used as flags to detect changes in AttributeSet. */

enum AttrKernelDataType {
  FLOAT = 0,
  FLOAT2 = 1,
  FLOAT3 = 2,
  FLOAT4 = 3,
  UCHAR4 = 4,
  NORMAL = 5,
  NUM = 6
};

/* Attribute
 *
 * Arbitrary data layers on meshes.
 * Supported types: Float, Color, Vector, Normal, Point */

class Attribute {
 public:
  /* Storage buffer for one motion step, or a single buffer if no motion. */
  struct Buffer {
    /* Optionally used to share ownership of #data, see implicit_sharing.h.
     * If this is null, #data is wholly owned by this Attribute. */
    const void *data = nullptr;
    ImplicitSharingInfo sharing_info = nullptr;
  };

  ustring name;
  AttributeStandard std;

  TypeDesc type;
  /* Center motion step. */
  Buffer center;
  /* Other motion steps. Empty when the attribute has no motion. */
  vector<Buffer> motion;
  int size = 0;
  AttributeElement element;
  uint flags; /* enum AttributeFlag */

  bool modified;

  Attribute(ustring name,
            const TypeDesc type,
            AttributeElement element,
            Geometry *geom,
            AttributePrimitive prim);
  Attribute(ustring name,
            const TypeDesc type,
            AttributeElement element,
            const void *data,
            int size,
            ImplicitSharingInfo sharing_info);
  Attribute(Attribute &&other);
  Attribute &operator=(Attribute &&other) = delete;
  Attribute(const Attribute &other) = delete;
  Attribute &operator=(const Attribute &other) = delete;
  ~Attribute();

  void set(ustring name, const TypeDesc type, AttributeElement element);
  void resize(Geometry *geom, AttributePrimitive prim);
  void resize(const size_t num_elements);

  /* Allocate or free motion steps, matching the geometry's motion_steps. */
  void add_motion(const Geometry *geom);
  void remove_motion();
  bool has_motion() const
  {
    return !motion.empty();
  }
  /* Number of motion steps stored, including the center step. */
  int num_motion_steps() const
  {
    return has_motion() ? int(motion.size()) + 1 : 1;
  }

  /* Move motion steps from another attribute into this one. */
  void take_motion_from(Attribute &other);

  /* Replace a single motion step's data with a buffer shared with another owner,
   * using implicit sharing. Motion steps are 1-indexed (step 0 is the center). */
  void set_motion_step_shared(int step,
                              const void *data,
                              int new_size,
                              ImplicitSharingInfo sharing_info);

  size_t data_sizeof() const;
  static size_t element_size(Geometry *geom, AttributeElement element, AttributePrimitive prim);
  size_t buffer_size(Geometry *geom, AttributePrimitive prim) const;

  /* Typed data access. */
  template<typename T = void> const T *data(const int step = 0) const
  {
    if constexpr (!std::is_same_v<T, void>) {
      assert(data_sizeof() == sizeof(T));
    }
    if (step == 0 || !has_motion()) {
      return static_cast<const T *>(center.data);
    }
    assert(step >= 1 && step <= int(motion.size()));
    return static_cast<const T *>(motion[step - 1].data);
  }
  template<typename T = void> T *data_for_write(const int step = 0)
  {
    if constexpr (!std::is_same_v<T, void>) {
      assert(data_sizeof() == sizeof(T));
    }
    return reinterpret_cast<T *>(data_for_write_buffer(step));
  }

  /* Motion steps come in two orderings:
   * - data() indexes the attribute step, with the center step at index 0.
   * - data_at_time_step() indexes the time-ordered step, with the center step
   *   in the middle.
   *
   * The number of time steps must be passed in, as it may be lower than the
   * stored motion steps, for example when BVH building without motion blur
   * but there are motion steps for a motion render pass. */
  int time_step_to_attr_step(const int step, const int num_time_steps) const
  {
    const int center = (num_time_steps - 1) / 2;
    return (step == center) ? 0 : (step < center) ? step + 1 : step;
  }
  template<typename T = void>
  const T *data_at_time_step(const int step, const int num_time_steps) const
  {
    return data<T>(time_step_to_attr_step(step, num_time_steps));
  }

  /* Attributes for voxels are 3D NanoVDB images. */
  const ImageHandle &data_voxel() const
  {
    return *data<ImageHandle>();
  }
  ImageHandle &data_voxel_for_write()
  {
    return *data_for_write<ImageHandle>();
  }

 private:
  char *data_for_write_buffer(const int step);

 public:
  void zero_data(void *dst);

  void set_data_from(Attribute &&other);

  void free_data();

  static bool same_storage(const TypeDesc a, const TypeDesc b);
  static const char *standard_name(AttributeStandard std);
  static AttributeStandard name_standard(const char *name);

  static AttrKernelDataType kernel_type(const Attribute &attr);

  void get_uv_tiles(Geometry *geom, AttributePrimitive prim, unordered_set<int> &tiles) const;
};

/* Attribute Set
 *
 * Set of attributes on a mesh. */

class AttributeSet {
  uint32_t modified_flag;

 public:
  Geometry *geometry;
  AttributePrimitive prim;
  list<Attribute> attributes;

  AttributeSet(Geometry *geometry, AttributePrimitive prim);
  AttributeSet(AttributeSet &&) = default;
  ~AttributeSet();

  Attribute *add(ustring name, const TypeDesc type, AttributeElement element);
  Attribute *add_shared(ustring name,
                        const TypeDesc type,
                        AttributeElement element,
                        const void *data,
                        int size,
                        ImplicitSharingInfo sharing_info);
  Attribute *add_from(Attribute &&other);
  Attribute *find(ustring name) const;
  void remove(ustring name);

  Attribute *add(AttributeStandard std, ustring name = ustring());
  Attribute *add_shared(AttributeStandard std,
                        ustring name,
                        const void *data,
                        int size,
                        ImplicitSharingInfo sharing_info);
  Attribute *find(AttributeStandard std) const;
  void remove(AttributeStandard std);

  Attribute &copy(const Attribute &attr);

  Attribute *find(AttributeRequest &req);
  Attribute *find_matching(const Attribute &other);

  void remove(Attribute *attribute);

  void remove(list<Attribute>::iterator it);

  void resize();
  void clear(bool preserve_voxel_data = false);

  /* Update the attributes in this AttributeSet with the ones from the new set,
   * and remove any attribute not found on the new set from this. */
  void update(AttributeSet &&new_attributes);

  /* Return whether the attributes of the given kernel_type are modified, where "modified" means
   * that some attributes of the given type were added or removed from this AttributeSet. This does
   * not mean that the data of the remaining attributes in this AttributeSet were also modified. To
   * check this, use Attribute.modified. */
  bool modified(AttrKernelDataType kernel_type) const;

  void clear_modified();

 private:
  /* Set the relevant modified flag for the attribute. Only attributes that are stored in device
   * arrays will be considered for tagging this AttributeSet as modified. */
  void tag_modified(const Attribute &attr);
};

/* AttributeRequest
 *
 * Request from a shader to use a certain attribute, so we can figure out
 * which ones we need to export from the host app end store for the kernel.
 * The attribute is found either by name or by standard attribute type. */

class AttributeRequest {
 public:
  ustring name;
  AttributeStandard std;

  /* temporary variables used by GeometryManager */
  TypeDesc type;
  AttributeDescriptor desc;

  explicit AttributeRequest(ustring name_);
  explicit AttributeRequest(AttributeStandard std);
};

/* AttributeRequestSet
 *
 * Set of attributes requested by a shader. */

class AttributeRequestSet {
 public:
  vector<AttributeRequest> requests;

  AttributeRequestSet();
  ~AttributeRequestSet();

  void add(ustring name);
  void add(AttributeStandard std);
  void add(const AttributeRequestSet &reqs);
  void add_standard(ustring name);

  bool find(ustring name) const;
  bool find(AttributeStandard std) const;

  size_t size() const;
  void clear();

  bool modified(const AttributeRequestSet &other) const;
};

CCL_NAMESPACE_END

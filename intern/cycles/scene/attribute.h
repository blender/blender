/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __ATTRIBUTE_H__
#define __ATTRIBUTE_H__

#include "scene/image.h"

#include "kernel/types.h"

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

enum AttrKernelDataType { FLOAT = 0, FLOAT2 = 1, FLOAT3 = 2, FLOAT4 = 3, UCHAR4 = 4, NUM = 5 };

/* Attribute
 *
 * Arbitrary data layers on meshes.
 * Supported types: Float, Color, Vector, Normal, Point */

class Attribute {
 public:
  ustring name;
  AttributeStandard std;

  TypeDesc type;
  vector<char> buffer;
  AttributeElement element;
  uint flags; /* enum AttributeFlag */

  bool modified;

  Attribute(ustring name,
            TypeDesc type,
            AttributeElement element,
            Geometry *geom,
            AttributePrimitive prim);
  Attribute(Attribute &&other) = default;
  Attribute(const Attribute &other) = delete;
  Attribute &operator=(const Attribute &other) = delete;
  ~Attribute();

  void set(ustring name, TypeDesc type, AttributeElement element);
  void resize(Geometry *geom, AttributePrimitive prim, bool reserve_only);
  void resize(size_t num_elements);

  size_t data_sizeof() const;
  size_t element_size(Geometry *geom, AttributePrimitive prim) const;
  size_t buffer_size(Geometry *geom, AttributePrimitive prim) const;

  char *data()
  {
    return (buffer.size()) ? &buffer[0] : NULL;
  }
  float2 *data_float2()
  {
    assert(data_sizeof() == sizeof(float2));
    return (float2 *)data();
  }
  float3 *data_float3()
  {
    assert(data_sizeof() == sizeof(float3));
    return (float3 *)data();
  }
  float4 *data_float4()
  {
    assert(data_sizeof() == sizeof(float4));
    return (float4 *)data();
  }
  float *data_float()
  {
    assert(data_sizeof() == sizeof(float));
    return (float *)data();
  }
  uchar4 *data_uchar4()
  {
    assert(data_sizeof() == sizeof(uchar4));
    return (uchar4 *)data();
  }
  Transform *data_transform()
  {
    assert(data_sizeof() == sizeof(Transform));
    return (Transform *)data();
  }

  /* Attributes for voxels are images */
  ImageHandle &data_voxel()
  {
    assert(data_sizeof() == sizeof(ImageHandle));
    return *(ImageHandle *)data();
  }

  const char *data() const
  {
    return (buffer.size()) ? &buffer[0] : NULL;
  }
  const float2 *data_float2() const
  {
    assert(data_sizeof() == sizeof(float2));
    return (const float2 *)data();
  }
  const float3 *data_float3() const
  {
    assert(data_sizeof() == sizeof(float3));
    return (const float3 *)data();
  }
  const float4 *data_float4() const
  {
    assert(data_sizeof() == sizeof(float4));
    return (const float4 *)data();
  }
  const float *data_float() const
  {
    assert(data_sizeof() == sizeof(float));
    return (const float *)data();
  }
  const Transform *data_transform() const
  {
    assert(data_sizeof() == sizeof(Transform));
    return (const Transform *)data();
  }
  const ImageHandle &data_voxel() const
  {
    assert(data_sizeof() == sizeof(ImageHandle));
    return *(const ImageHandle *)data();
  }

  void zero_data(void *dst);
  void add_with_weight(void *dst, void *src, float weight);

  void add(const float &f);
  void add(const float2 &f);
  void add(const float3 &f);
  void add(const uchar4 &f);
  void add(const Transform &tfm);
  void add(const char *data);

  void set_data_from(Attribute &&other);

  static bool same_storage(TypeDesc a, TypeDesc b);
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

  Attribute *add(ustring name, TypeDesc type, AttributeElement element);
  Attribute *find(ustring name) const;
  void remove(ustring name);

  Attribute *add(AttributeStandard std, ustring name = ustring());
  Attribute *find(AttributeStandard std) const;
  void remove(AttributeStandard std);

  Attribute *find(AttributeRequest &req);
  Attribute *find_matching(const Attribute &other);

  void remove(Attribute *attribute);

  void remove(list<Attribute>::iterator it);

  void resize(bool reserve_only = false);
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
  TypeDesc type, subd_type;
  AttributeDescriptor desc, subd_desc;

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
  void add(AttributeRequestSet &reqs);
  void add_standard(ustring name);

  bool find(ustring name);
  bool find(AttributeStandard std);

  size_t size();
  void clear();

  bool modified(const AttributeRequestSet &other);
};

CCL_NAMESPACE_END

#endif /* __ATTRIBUTE_H__ */

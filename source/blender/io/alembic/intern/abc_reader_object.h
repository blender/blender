/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>

#include "DNA_ID.h"

struct CacheFile;
struct Main;
struct Mesh;
struct Object;

using Alembic::AbcCoreAbstract::chrono_t;

namespace blender::io::alembic {

struct ImportSettings {
  bool do_convert_mat;
  float conversion_mat[4][4];

  int from_up;
  int from_forward;
  float scale;
  bool is_sequence;
  bool set_frame_range;

  /* Length and frame offset of file sequences. */
  int sequence_len;
  int sequence_offset;

  /* From MeshSeqCacheModifierData.read_flag */
  int read_flag;

  /* From CacheFile and MeshSeqCacheModifierData */
  std::string velocity_name;
  float velocity_scale;

  bool validate_meshes;
  bool always_add_cache_reader;

  CacheFile *cache_file;

  ImportSettings()
      : do_convert_mat(false),
        from_up(0),
        from_forward(0),
        scale(1.0f),
        is_sequence(false),
        set_frame_range(false),
        sequence_len(1),
        sequence_offset(0),
        read_flag(0),
        velocity_name(""),
        velocity_scale(1.0f),
        validate_meshes(false),
        always_add_cache_reader(false),
        cache_file(NULL)
  {
  }
};

template<typename Schema> static bool has_animations(Schema &schema, ImportSettings *settings)
{
  return settings->is_sequence || !schema.isConstant();
}

class AbcObjectReader {
 protected:
  std::string m_name;
  std::string m_object_name;
  std::string m_data_name;
  Object *m_object;
  Alembic::Abc::IObject m_iobject;

  /* XXX - TODO(kevindietrich) : this references stack memory... */
  ImportSettings *m_settings;
  /* This is initialised from the ImportSettings above on construction. It will need to be removed
   * once we fix the stack memory reference situation.  */
  bool m_is_reading_a_file_sequence = false;

  chrono_t m_min_time;
  chrono_t m_max_time;

  /* Use reference counting since the same reader may be used by multiple
   * modifiers and/or constraints. */
  int m_refcount;

  bool m_inherits_xform;

 public:
  AbcObjectReader *parent_reader;

 public:
  explicit AbcObjectReader(const Alembic::Abc::IObject &object, ImportSettings &settings);

  virtual ~AbcObjectReader() = default;

  const Alembic::Abc::IObject &iobject() const;

  typedef std::vector<AbcObjectReader *> ptr_vector;

  /**
   * Returns the transform of this object. This can be the Alembic object
   * itself (in case of an Empty) or it can be the parent Alembic object.
   */
  virtual Alembic::AbcGeom::IXform xform();

  Object *object() const;
  void object(Object *ob);

  const std::string &name() const
  {
    return m_name;
  }
  const std::string &object_name() const
  {
    return m_object_name;
  }
  const std::string &data_name() const
  {
    return m_data_name;
  }
  bool inherits_xform() const
  {
    return m_inherits_xform;
  }

  virtual bool valid() const = 0;
  virtual bool accepts_object_type(const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
                                   const Object *const ob,
                                   const char **err_str) const = 0;

  virtual void readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel) = 0;

  virtual struct Mesh *read_mesh(struct Mesh *mesh,
                                 const Alembic::Abc::ISampleSelector &sample_sel,
                                 int read_flag,
                                 const char *velocity_name,
                                 float velocity_scale,
                                 const char **err_str);
  virtual bool topology_changed(const Mesh *existing_mesh,
                                const Alembic::Abc::ISampleSelector &sample_sel);

  /** Reads the object matrix and sets up an object transform if animated. */
  void setupObjectTransform(chrono_t time);

  void addCacheModifier();

  chrono_t minTime() const;
  chrono_t maxTime() const;

  int refcount() const;
  void incref();
  void decref();

  void read_matrix(float r_mat[4][4], chrono_t time, float scale, bool &is_constant);

 protected:
  /** Determine whether we can inherit our parent's XForm. */
  void determine_inherits_xform();
};

Imath::M44d get_matrix(const Alembic::AbcGeom::IXformSchema &schema, chrono_t time);

}  // namespace blender::io::alembic

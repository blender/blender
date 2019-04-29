/*
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

/** \file
 * \ingroup balembic
 */

#ifndef __ABC_EXPORTER_H__
#define __ABC_EXPORTER_H__

#include <Alembic/Abc/All.h>
#include <map>
#include <set>
#include <vector>

#include "abc_util.h"

class AbcObjectWriter;
class AbcTransformWriter;
class ArchiveWriter;

struct Base;
struct Depsgraph;
struct Main;
struct Object;
struct Scene;
struct ViewLayer;

struct ExportSettings {
  ExportSettings();

  Scene *scene;
  /** Scene layer to export; all its objects will be exported, unless selected_only=true. */
  ViewLayer *view_layer;
  Depsgraph *depsgraph;
  SimpleLogger logger;

  bool selected_only;
  bool visible_layers_only;
  bool renderable_only;

  double frame_start, frame_end;
  double frame_samples_xform;
  double frame_samples_shape;
  double shutter_open;
  double shutter_close;
  float global_scale;

  bool flatten_hierarchy;

  bool export_normals;
  bool export_uvs;
  bool export_vcols;
  bool export_face_sets;
  bool export_vweigths;
  bool export_hair;
  bool export_particles;

  bool apply_subdiv;
  bool curves_as_mesh;
  bool use_subdiv_schema;
  bool export_child_hairs;
  bool export_ogawa;
  bool pack_uv;
  bool triangulate;

  int quad_method;
  int ngon_method;

  bool do_convert_axis;
  float convert_matrix[3][3];
};

class AbcExporter {
  Main *m_bmain;
  ExportSettings &m_settings;

  const char *m_filename;

  unsigned int m_trans_sampling_index, m_shape_sampling_index;

  ArchiveWriter *m_writer;

  /* mapping from name to transform writer */
  typedef std::map<std::string, AbcTransformWriter *> m_xforms_type;
  m_xforms_type m_xforms;

  std::vector<AbcObjectWriter *> m_shapes;

 public:
  AbcExporter(Main *bmain, const char *filename, ExportSettings &settings);
  ~AbcExporter();

  void operator()(float &progress, bool &was_canceled);

 protected:
  void getShutterSamples(unsigned int nr_of_samples,
                         bool time_relative,
                         std::vector<double> &samples);
  void getFrameSet(unsigned int nr_of_samples, std::set<double> &frames);

 private:
  Alembic::Abc::TimeSamplingPtr createTimeSampling(double step);

  void createTransformWritersHierarchy();
  AbcTransformWriter *createTransformWriter(Object *ob, Object *parent, Object *dupliObParent);
  void exploreTransform(Base *base, Object *object, Object *parent, Object *dupliObParent);
  void exploreObject(Base *base, Object *object, Object *dupliObParent);
  void createShapeWriters();
  void createShapeWriter(Object *ob, Object *dupliObParent);
  void createParticleSystemsWriters(Object *ob, AbcTransformWriter *xform);

  AbcTransformWriter *getXForm(const std::string &name);

  void setCurrentFrame(Main *bmain, double t);
};

#endif /* __ABC_EXPORTER_H__ */

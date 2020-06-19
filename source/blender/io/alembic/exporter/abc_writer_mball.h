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

#ifndef __ABC_WRITER_MBALL_H__
#define __ABC_WRITER_MBALL_H__

#include "abc_writer_mesh.h"
#include "abc_writer_object.h"

struct Main;
struct Object;

namespace blender {
namespace io {
namespace alembic {

/* AbcMBallWriter converts the metaballs to meshes at every frame,
 * and defers to AbcGenericMeshWriter to perform the writing
 * to the Alembic file. Only the basis balls are exported, as this
 * results in the entire shape as one mesh. */
class AbcMBallWriter : public AbcGenericMeshWriter {
  Main *m_bmain;

 public:
  explicit AbcMBallWriter(Main *bmain,
                          Object *ob,
                          AbcTransformWriter *parent,
                          uint32_t time_sampling,
                          ExportSettings &settings);

  ~AbcMBallWriter();

  static bool isBasisBall(Scene *scene, Object *ob);

 protected:
  Mesh *getEvaluatedMesh(Scene *scene_eval, Object *ob_eval, bool &r_needsfree) override;
  void freeEvaluatedMesh(struct Mesh *mesh) override;

 private:
  bool isAnimated() const override;
};

}  // namespace alembic
}  // namespace io
}  // namespace blender

#endif /* __ABC_WRITER_MBALL_H__ */

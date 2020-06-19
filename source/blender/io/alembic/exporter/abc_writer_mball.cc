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

#include "abc_writer_mball.h"
#include "abc_writer_mesh.h"

#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"

#include "BKE_displist.h"
#include "BKE_lib_id.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "BLI_utildefines.h"

namespace blender {
namespace io {
namespace alembic {

AbcMBallWriter::AbcMBallWriter(Main *bmain,
                               Object *ob,
                               AbcTransformWriter *parent,
                               uint32_t time_sampling,
                               ExportSettings &settings)
    : AbcGenericMeshWriter(ob, parent, time_sampling, settings), m_bmain(bmain)
{
  m_is_animated = isAnimated();
}

AbcMBallWriter::~AbcMBallWriter()
{
}

bool AbcMBallWriter::isAnimated() const
{
  return true;
}

Mesh *AbcMBallWriter::getEvaluatedMesh(Scene * /*scene_eval*/, Object *ob_eval, bool &r_needsfree)
{
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  if (mesh_eval != NULL) {
    /* Mesh_eval only exists when generative modifiers are in use. */
    r_needsfree = false;
    return mesh_eval;
  }
  r_needsfree = true;

  /* The approach below is copied from BKE_mesh_new_from_object() */
  Mesh *tmpmesh = BKE_mesh_add(m_bmain, ((ID *)m_object->data)->name + 2);
  BLI_assert(tmpmesh != NULL);

  /* BKE_mesh_add gives us a user count we don't need */
  id_us_min(&tmpmesh->id);

  ListBase disp = {NULL, NULL};
  /* TODO(sergey): This is gonna to work for until Depsgraph
   *               only contains for_render flag. As soon as CoW is
   *               implemented, this is to be rethought.
   */
  BKE_displist_make_mball_forRender(m_settings.depsgraph, m_settings.scene, m_object, &disp);
  BKE_mesh_from_metaball(&disp, tmpmesh);
  BKE_displist_free(&disp);

  BKE_mesh_texspace_copy_from_object(tmpmesh, m_object);

  return tmpmesh;
}

void AbcMBallWriter::freeEvaluatedMesh(struct Mesh *mesh)
{
  BKE_id_free(m_bmain, mesh);
}

bool AbcMBallWriter::isBasisBall(Scene *scene, Object *ob)
{
  Object *basis_ob = BKE_mball_basis_find(scene, ob);
  return ob == basis_ob;
}

}  // namespace alembic
}  // namespace io
}  // namespace blender

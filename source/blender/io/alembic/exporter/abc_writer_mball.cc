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
#include "abc_hierarchy_iterator.h"

#include "BLI_assert.h"

#include "BKE_displist.h"
#include "BKE_lib_id.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"

namespace blender::io::alembic {

ABCMetaballWriter::ABCMetaballWriter(const ABCWriterConstructorArgs &args)
    : ABCGenericMeshWriter(args)
{
}

bool ABCMetaballWriter::is_supported(const HierarchyContext *context) const
{
  Scene *scene = DEG_get_input_scene(args_.depsgraph);
  bool supported = is_basis_ball(scene, context->object) &&
                   ABCGenericMeshWriter::is_supported(context);
  return supported;
}

bool ABCMetaballWriter::check_is_animated(const HierarchyContext & /*context*/) const
{
  /* We assume that metaballs are always animated, as the current object may
   * not be animated but another ball in the same group may be. */
  return true;
}

bool ABCMetaballWriter::export_as_subdivision_surface(Object * /*ob_eval*/) const
{
  /* Metaballs should be exported to subdivision surfaces, if the export options allow. */
  return true;
}

Mesh *ABCMetaballWriter::get_export_mesh(Object *object_eval, bool &r_needsfree)
{
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(object_eval);
  if (mesh_eval != nullptr) {
    /* Mesh_eval only exists when generative modifiers are in use. */
    r_needsfree = false;
    return mesh_eval;
  }
  r_needsfree = true;
  return BKE_mesh_new_from_object(args_.depsgraph, object_eval, false, false);
}

void ABCMetaballWriter::free_export_mesh(Mesh *mesh)
{
  BKE_id_free(nullptr, mesh);
}

bool ABCMetaballWriter::is_basis_ball(Scene *scene, Object *ob) const
{
  Object *basis_ob = BKE_mball_basis_find(scene, ob);
  return ob == basis_ob;
}

}  // namespace blender::io::alembic

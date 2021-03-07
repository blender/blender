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
 * \ingroup bke
 */

#include <iostream>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "BLI_compiler_compat.h"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_customdata.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_pointcache.h"
#include "BKE_simulation.h"

#include "NOD_geometry.h"
#include "NOD_node_tree_multi_function.hh"

#include "BLI_map.hh"
#include "BLT_translation.h"

#include "FN_multi_function_network_evaluation.hh"
#include "FN_multi_function_network_optimization.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

static void simulation_init_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(simulation, id));

  MEMCPY_STRUCT_AFTER(simulation, DNA_struct_default_get(Simulation), id);

  bNodeTree *ntree = ntreeAddTree(nullptr, "Geometry Nodetree", ntreeType_Geometry->idname);
  simulation->nodetree = ntree;
}

static void simulation_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Simulation *simulation_dst = (Simulation *)id_dst;
  const Simulation *simulation_src = (const Simulation *)id_src;

  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (simulation_src->nodetree) {
    BKE_id_copy_ex(bmain,
                   (ID *)simulation_src->nodetree,
                   (ID **)&simulation_dst->nodetree,
                   flag_private_id_data);
  }
}

static void simulation_free_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;

  BKE_animdata_free(&simulation->id, false);

  if (simulation->nodetree) {
    ntreeFreeEmbeddedTree(simulation->nodetree);
    MEM_freeN(simulation->nodetree);
    simulation->nodetree = nullptr;
  }
}

static void simulation_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Simulation *simulation = (Simulation *)id;
  if (simulation->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_library_foreach_ID_embedded(data, (ID **)&simulation->nodetree);
  }
}

static void simulation_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Simulation *simulation = (Simulation *)id;
  if (simulation->id.us > 0 || BLO_write_is_undo(writer)) {
    BLO_write_id_struct(writer, Simulation, id_address, &simulation->id);
    BKE_id_blend_write(writer, &simulation->id);

    if (simulation->adt) {
      BKE_animdata_blend_write(writer, simulation->adt);
    }

    /* nodetree is integral part of simulation, no libdata */
    if (simulation->nodetree) {
      BLO_write_struct(writer, bNodeTree, simulation->nodetree);
      ntreeBlendWrite(writer, simulation->nodetree);
    }
  }
}

static void simulation_blend_read_data(BlendDataReader *reader, ID *id)
{
  Simulation *simulation = (Simulation *)id;
  BLO_read_data_address(reader, &simulation->adt);
  BKE_animdata_blend_read_data(reader, simulation->adt);
}

static void simulation_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Simulation *simulation = (Simulation *)id;
  UNUSED_VARS(simulation, reader);
}

static void simulation_blend_read_expand(BlendExpander *expander, ID *id)
{
  Simulation *simulation = (Simulation *)id;
  UNUSED_VARS(simulation, expander);
}

IDTypeInfo IDType_ID_SIM = {
    /* id_code */ ID_SIM,
    /* id_filter */ FILTER_ID_SIM,
    /* main_listbase_index */ INDEX_ID_SIM,
    /* struct_size */ sizeof(Simulation),
    /* name */ "Simulation",
    /* name_plural */ "simulations",
    /* translation_context */ BLT_I18NCONTEXT_ID_SIMULATION,
    /* flags */ 0,

    /* init_data */ simulation_init_data,
    /* copy_data */ simulation_copy_data,
    /* free_data */ simulation_free_data,
    /* make_local */ nullptr,
    /* foreach_id */ simulation_foreach_id,
    /* foreach_cache */ nullptr,
    /* owner_get */ nullptr,

    /* blend_write */ simulation_blend_write,
    /* blend_read_data */ simulation_blend_read_data,
    /* blend_read_lib */ simulation_blend_read_lib,
    /* blend_read_expand */ simulation_blend_read_expand,

    /* blend_read_undo_preserve */ nullptr,

    /* lib_override_apply_post */ nullptr,
};

void *BKE_simulation_add(Main *bmain, const char *name)
{
  Simulation *simulation = (Simulation *)BKE_id_new(bmain, ID_SIM, name);
  return simulation;
}

void BKE_simulation_data_update(Depsgraph *UNUSED(depsgraph),
                                Scene *UNUSED(scene),
                                Simulation *UNUSED(simulation))
{
}

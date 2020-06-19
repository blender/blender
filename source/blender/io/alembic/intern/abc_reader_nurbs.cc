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

#include "abc_reader_nurbs.h"
#include "abc_axis_conversion.h"
#include "abc_reader_transform.h"
#include "abc_util.h"

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_curve.h"
#include "BKE_object.h"

using Alembic::AbcGeom::FloatArraySamplePtr;
using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::MetaData;
using Alembic::AbcGeom::P3fArraySamplePtr;

using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::INuPatch;
using Alembic::AbcGeom::INuPatchSchema;
using Alembic::AbcGeom::IObject;

namespace blender {
namespace io {
namespace alembic {

AbcNurbsReader::AbcNurbsReader(const IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  getNurbsPatches(m_iobject);
  get_min_max_time(m_iobject, m_schemas[0].first, m_min_time, m_max_time);
}

bool AbcNurbsReader::valid() const
{
  if (m_schemas.empty()) {
    return false;
  }

  std::vector<std::pair<INuPatchSchema, IObject>>::const_iterator it;
  for (it = m_schemas.begin(); it != m_schemas.end(); ++it) {
    const INuPatchSchema &schema = it->first;

    if (!schema.valid()) {
      return false;
    }
  }

  return true;
}

static bool set_knots(const FloatArraySamplePtr &knots, float *&nu_knots)
{
  if (!knots || knots->size() < 2) {
    return false;
  }

  /* Skip first and last knots, as they are used for padding. */
  const size_t num_knots = knots->size() - 2;
  nu_knots = static_cast<float *>(MEM_callocN(num_knots * sizeof(float), "abc_setsplineknotsu"));

  for (size_t i = 0; i < num_knots; i++) {
    nu_knots[i] = (*knots)[i + 1];
  }

  return true;
}

void AbcNurbsReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Curve *cu = static_cast<Curve *>(BKE_curve_add(bmain, "abc_curve", OB_SURF));
  cu->actvert = CU_ACT_NONE;

  std::vector<std::pair<INuPatchSchema, IObject>>::iterator it;

  for (it = m_schemas.begin(); it != m_schemas.end(); ++it) {
    Nurb *nu = static_cast<Nurb *>(MEM_callocN(sizeof(Nurb), "abc_getnurb"));
    nu->flag = CU_SMOOTH;
    nu->type = CU_NURBS;
    nu->resolu = cu->resolu;
    nu->resolv = cu->resolv;

    const INuPatchSchema &schema = it->first;
    INuPatchSchema::Sample smp;
    try {
      smp = schema.getValue(sample_sel);
    }
    catch (Alembic::Util::Exception &ex) {
      printf("Alembic: error reading nurbs sample for '%s/%s' at time %f: %s\n",
             m_iobject.getFullName().c_str(),
             schema.getName().c_str(),
             sample_sel.getRequestedTime(),
             ex.what());
      return;
    }

    nu->orderu = smp.getUOrder() - 1;
    nu->orderv = smp.getVOrder() - 1;
    nu->pntsu = smp.getNumU();
    nu->pntsv = smp.getNumV();

    /* Read positions and weights. */

    const P3fArraySamplePtr positions = smp.getPositions();
    const FloatArraySamplePtr weights = smp.getPositionWeights();

    const size_t num_points = positions->size();

    nu->bp = static_cast<BPoint *>(MEM_callocN(num_points * sizeof(BPoint), "abc_setsplinetype"));

    BPoint *bp = nu->bp;
    float posw_in = 1.0f;

    for (int i = 0; i < num_points; i++, bp++) {
      const Imath::V3f &pos_in = (*positions)[i];

      if (weights) {
        posw_in = (*weights)[i];
      }

      copy_zup_from_yup(bp->vec, pos_in.getValue());
      bp->vec[3] = posw_in;
      bp->f1 = SELECT;
      bp->radius = 1.0f;
      bp->weight = 1.0f;
    }

    /* Read knots. */

    if (!set_knots(smp.getUKnot(), nu->knotsu)) {
      BKE_nurb_knot_calc_u(nu);
    }

    if (!set_knots(smp.getVKnot(), nu->knotsv)) {
      BKE_nurb_knot_calc_v(nu);
    }

    /* Read flags. */

    ICompoundProperty user_props = schema.getUserProperties();

    if (has_property(user_props, "enpoint_u")) {
      nu->flagu |= CU_NURB_ENDPOINT;
    }

    if (has_property(user_props, "enpoint_v")) {
      nu->flagv |= CU_NURB_ENDPOINT;
    }

    if (has_property(user_props, "cyclic_u")) {
      nu->flagu |= CU_NURB_CYCLIC;
    }

    if (has_property(user_props, "cyclic_v")) {
      nu->flagv |= CU_NURB_CYCLIC;
    }

    BLI_addtail(BKE_curve_nurbs_get(cu), nu);
  }

  BLI_strncpy(cu->id.name + 2, m_data_name.c_str(), m_data_name.size() + 1);

  m_object = BKE_object_add_only_object(bmain, OB_SURF, m_object_name.c_str());
  m_object->data = cu;
}

void AbcNurbsReader::getNurbsPatches(const IObject &obj)
{
  if (!obj.valid()) {
    return;
  }

  const int num_children = obj.getNumChildren();

  if (num_children == 0) {
    INuPatch abc_nurb(obj, kWrapExisting);
    INuPatchSchema schem = abc_nurb.getSchema();
    m_schemas.push_back(std::pair<INuPatchSchema, IObject>(schem, obj));
    return;
  }

  for (int i = 0; i < num_children; i++) {
    bool ok = true;
    IObject child(obj, obj.getChildHeader(i).getName());

    if (!m_name.empty() && child.valid() && !begins_with(child.getFullName(), m_name)) {
      ok = false;
    }

    if (!child.valid()) {
      continue;
    }

    const MetaData &md = child.getMetaData();

    if (INuPatch::matches(md) && ok) {
      INuPatch abc_nurb(child, kWrapExisting);
      INuPatchSchema schem = abc_nurb.getSchema();
      m_schemas.push_back(std::pair<INuPatchSchema, IObject>(schem, child));
    }

    getNurbsPatches(child);
  }
}

}  // namespace alembic
}  // namespace io
}  // namespace blender

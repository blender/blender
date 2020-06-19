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

#include "abc_writer_nurbs.h"
#include "abc_writer_transform.h"
#include "intern/abc_axis_conversion.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"

#include "BKE_curve.h"

using Alembic::AbcGeom::FloatArraySample;
using Alembic::AbcGeom::OBoolProperty;
using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::ONuPatch;
using Alembic::AbcGeom::ONuPatchSchema;

AbcNurbsWriter::AbcNurbsWriter(Object *ob,
                               AbcTransformWriter *parent,
                               uint32_t time_sampling,
                               ExportSettings &settings)
    : AbcObjectWriter(ob, time_sampling, settings, parent)
{
  m_is_animated = isAnimated();

  /* if the object is static, use the default static time sampling */
  if (!m_is_animated) {
    m_time_sampling = 0;
  }

  Curve *curve = static_cast<Curve *>(m_object->data);
  size_t numNurbs = BLI_listbase_count(&curve->nurb);

  for (size_t i = 0; i < numNurbs; i++) {
    std::stringstream str;
    str << m_name << '_' << i;

    while (parent->alembicXform().getChildHeader(str.str())) {
      str << "_";
    }

    ONuPatch nurbs(parent->alembicXform(), str.str().c_str(), m_time_sampling);
    m_nurbs_schema.push_back(nurbs.getSchema());
  }
}

bool AbcNurbsWriter::isAnimated() const
{
  /* check if object has shape keys */
  Curve *cu = static_cast<Curve *>(m_object->data);
  return (cu->key != NULL);
}

static void get_knots(std::vector<float> &knots, const int num_knots, float *nu_knots)
{
  if (num_knots <= 1) {
    return;
  }

  /* Add an extra knot at the beginning and end of the array since most apps
   * require/expect them. */
  knots.reserve(num_knots + 2);

  knots.push_back(0.0f);

  for (int i = 0; i < num_knots; i++) {
    knots.push_back(nu_knots[i]);
  }

  knots[0] = 2.0f * knots[1] - knots[2];
  knots.push_back(2.0f * knots[num_knots] - knots[num_knots - 1]);
}

void AbcNurbsWriter::do_write()
{
  /* we have already stored a sample for this object. */
  if (!m_first_frame && !m_is_animated) {
    return;
  }

  if (!ELEM(m_object->type, OB_SURF, OB_CURVE)) {
    return;
  }

  Curve *curve = static_cast<Curve *>(m_object->data);
  ListBase *nulb;

  if (m_object->runtime.curve_cache->deformed_nurbs.first != NULL) {
    nulb = &m_object->runtime.curve_cache->deformed_nurbs;
  }
  else {
    nulb = BKE_curve_nurbs_get(curve);
  }

  size_t count = 0;
  for (Nurb *nu = static_cast<Nurb *>(nulb->first); nu; nu = nu->next, count++) {
    std::vector<float> knotsU;
    get_knots(knotsU, KNOTSU(nu), nu->knotsu);

    std::vector<float> knotsV;
    get_knots(knotsV, KNOTSV(nu), nu->knotsv);

    const int size = nu->pntsu * nu->pntsv;
    std::vector<Imath::V3f> positions(size);
    std::vector<float> weights(size);

    const BPoint *bp = nu->bp;

    for (int i = 0; i < size; i++, bp++) {
      copy_yup_from_zup(positions[i].getValue(), bp->vec);
      weights[i] = bp->vec[3];
    }

    ONuPatchSchema::Sample sample;
    sample.setUOrder(nu->orderu + 1);
    sample.setVOrder(nu->orderv + 1);
    sample.setPositions(positions);
    sample.setPositionWeights(weights);
    sample.setUKnot(FloatArraySample(knotsU));
    sample.setVKnot(FloatArraySample(knotsV));
    sample.setNu(nu->pntsu);
    sample.setNv(nu->pntsv);

    /* TODO(kevin): to accommodate other software we should duplicate control
     * points to indicate that a NURBS is cyclic. */
    OCompoundProperty user_props = m_nurbs_schema[count].getUserProperties();

    if ((nu->flagu & CU_NURB_ENDPOINT) != 0) {
      OBoolProperty prop(user_props, "endpoint_u");
      prop.set(true);
    }

    if ((nu->flagv & CU_NURB_ENDPOINT) != 0) {
      OBoolProperty prop(user_props, "endpoint_v");
      prop.set(true);
    }

    if ((nu->flagu & CU_NURB_CYCLIC) != 0) {
      OBoolProperty prop(user_props, "cyclic_u");
      prop.set(true);
    }

    if ((nu->flagv & CU_NURB_CYCLIC) != 0) {
      OBoolProperty prop(user_props, "cyclic_v");
      prop.set(true);
    }

    m_nurbs_schema[count].set(sample);
  }
}

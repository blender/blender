/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_nurbs.h"
#include "intern/abc_axis_conversion.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"

#include "BKE_curve.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

namespace blender::io::alembic {

using Alembic::Abc::OObject;
using Alembic::AbcGeom::FloatArraySample;
using Alembic::AbcGeom::OBoolProperty;
using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::ONuPatch;
using Alembic::AbcGeom::ONuPatchSchema;

ABCNurbsWriter::ABCNurbsWriter(const ABCWriterConstructorArgs &args) : ABCAbstractWriter(args) {}

void ABCNurbsWriter::create_alembic_objects(const HierarchyContext *context)
{
  Curve *curve = static_cast<Curve *>(context->object->data);
  size_t num_nurbs = BLI_listbase_count(&curve->nurb);
  OObject abc_parent = args_.abc_parent;
  const char *abc_parent_path = abc_parent.getFullName().c_str();

  for (size_t i = 0; i < num_nurbs; i++) {
    std::stringstream patch_name_stream;
    patch_name_stream << args_.abc_name << '_' << i;

    while (abc_parent.getChildHeader(patch_name_stream.str())) {
      patch_name_stream << "_";
    }

    std::string patch_name = patch_name_stream.str();
    CLOG_INFO(&LOG, 2, "exporting %s/%s", abc_parent_path, patch_name.c_str());

    ONuPatch nurbs(abc_parent, patch_name, timesample_index_);
    abc_nurbs_.push_back(nurbs);
    abc_nurbs_schemas_.push_back(nurbs.getSchema());
  }
}

OObject ABCNurbsWriter::get_alembic_object() const
{
  if (abc_nurbs_.empty()) {
    return OObject();
  }
  /* For parenting purposes within the Alembic file, all NURBS patches are equal, so just use the
   * first one. */
  return abc_nurbs_[0];
}

Alembic::Abc::OCompoundProperty ABCNurbsWriter::abc_prop_for_custom_props()
{
  if (abc_nurbs_.empty()) {
    return Alembic::Abc::OCompoundProperty();
  }

  /* A single NURBS object in Blender is expanded to multiple curves in Alembic.
   * Just store the custom properties on the first one for simplicity. */
  return abc_schema_prop_for_custom_props(abc_nurbs_schemas_[0]);
}

bool ABCNurbsWriter::check_is_animated(const HierarchyContext &context) const
{
  /* Check if object has shape keys. */
  Curve *cu = static_cast<Curve *>(context.object->data);
  return (cu->key != nullptr);
}

bool ABCNurbsWriter::is_supported(const HierarchyContext *context) const
{
  return ELEM(context->object->type, OB_SURF, OB_CURVES_LEGACY);
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

void ABCNurbsWriter::do_write(HierarchyContext &context)
{
  Curve *curve = static_cast<Curve *>(context.object->data);
  ListBase *nulb;

  if (context.object->runtime.curve_cache->deformed_nurbs.first != nullptr) {
    nulb = &context.object->runtime.curve_cache->deformed_nurbs;
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
    OCompoundProperty user_props = abc_nurbs_schemas_[count].getUserProperties();

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

    abc_nurbs_schemas_[count].set(sample);
  }
}

}  // namespace blender::io::alembic

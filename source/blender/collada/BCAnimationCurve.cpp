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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

#include "BCAnimationCurve.h"

BCAnimationCurve::BCAnimationCurve()
{
  this->curve_key.set_object_type(BC_ANIMATION_TYPE_OBJECT);
  this->fcurve = NULL;
  this->curve_is_local_copy = false;
}

BCAnimationCurve::BCAnimationCurve(const BCAnimationCurve &other)
{
  this->min = other.min;
  this->max = other.max;
  this->fcurve = other.fcurve;
  this->curve_key = other.curve_key;
  this->curve_is_local_copy = false;
  this->id_ptr = other.id_ptr;

  /* The fcurve of the new instance is a copy and can be modified */

  get_edit_fcurve();
}

BCAnimationCurve::BCAnimationCurve(BCCurveKey key, Object *ob, FCurve *fcu)
{
  this->min = 0;
  this->max = 0;
  this->curve_key = key;
  this->fcurve = fcu;
  this->curve_is_local_copy = false;
  init_pointer_rna(ob);
}

BCAnimationCurve::BCAnimationCurve(const BCCurveKey &key, Object *ob)
{
  this->curve_key = key;
  this->fcurve = NULL;
  this->curve_is_local_copy = false;
  init_pointer_rna(ob);
}

void BCAnimationCurve::init_pointer_rna(Object *ob)
{
  switch (this->curve_key.get_animation_type()) {
    case BC_ANIMATION_TYPE_BONE: {
      bArmature *arm = (bArmature *)ob->data;
      RNA_id_pointer_create(&arm->id, &id_ptr);
    } break;
    case BC_ANIMATION_TYPE_OBJECT: {
      RNA_id_pointer_create(&ob->id, &id_ptr);
    } break;
    case BC_ANIMATION_TYPE_MATERIAL: {
      Material *ma = give_current_material(ob, curve_key.get_subindex() + 1);
      RNA_id_pointer_create(&ma->id, &id_ptr);
    } break;
    case BC_ANIMATION_TYPE_CAMERA: {
      Camera *camera = (Camera *)ob->data;
      RNA_id_pointer_create(&camera->id, &id_ptr);
    } break;
    case BC_ANIMATION_TYPE_LIGHT: {
      Light *lamp = (Light *)ob->data;
      RNA_id_pointer_create(&lamp->id, &id_ptr);
    } break;
    default:
      fprintf(
          stderr, "BC_animation_curve_type %d not supported", this->curve_key.get_array_index());
      break;
  }
}

void BCAnimationCurve::delete_fcurve(FCurve *fcu)
{
  free_fcurve(fcu);
}

FCurve *BCAnimationCurve::create_fcurve(int array_index, const char *rna_path)
{
  FCurve *fcu = (FCurve *)MEM_callocN(sizeof(FCurve), "FCurve");
  fcu->flag = (FCURVE_VISIBLE | FCURVE_AUTO_HANDLES | FCURVE_SELECTED);
  fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
  fcu->array_index = array_index;
  return fcu;
}

void BCAnimationCurve::create_bezt(float frame, float output)
{
  FCurve *fcu = get_edit_fcurve();
  BezTriple bez;
  memset(&bez, 0, sizeof(BezTriple));
  bez.vec[1][0] = frame;
  bez.vec[1][1] = output;
  bez.ipo = U.ipo_new; /* use default interpolation mode here... */
  bez.f1 = bez.f2 = bez.f3 = SELECT;
  bez.h1 = bez.h2 = HD_AUTO;
  insert_bezt_fcurve(fcu, &bez, INSERTKEY_NOFLAGS);
  calchandles_fcurve(fcu);
}

BCAnimationCurve::~BCAnimationCurve()
{
  if (curve_is_local_copy && fcurve) {
    // fprintf(stderr, "removed fcurve %s\n", fcurve->rna_path);
    delete_fcurve(fcurve);
    this->fcurve = NULL;
  }
}

const bool BCAnimationCurve::is_of_animation_type(BC_animation_type type) const
{
  return curve_key.get_animation_type() == type;
}

const std::string BCAnimationCurve::get_channel_target() const
{
  const std::string path = curve_key.get_path();

  if (bc_startswith(path, "pose.bones")) {
    return bc_string_after(path, "pose.bones");
  }
  return bc_string_after(path, ".");
}

const std::string BCAnimationCurve::get_channel_type() const
{
  const std::string channel = get_channel_target();
  return bc_string_after(channel, ".");
}

const std::string BCAnimationCurve::get_channel_posebone() const
{
  const std::string channel = get_channel_target();
  std::string pose_bone_name = bc_string_before(channel, ".");
  if (pose_bone_name == channel) {
    pose_bone_name = "";
  }
  else {
    pose_bone_name = bc_string_after(pose_bone_name, "\"[");
    pose_bone_name = bc_string_before(pose_bone_name, "]\"");
  }
  return pose_bone_name;
}

const std::string BCAnimationCurve::get_animation_name(Object *ob) const
{
  std::string name;

  switch (curve_key.get_animation_type()) {
    case BC_ANIMATION_TYPE_OBJECT: {
      name = id_name(ob);
    } break;

    case BC_ANIMATION_TYPE_BONE: {
      if (fcurve == NULL || fcurve->rna_path == NULL) {
        name = "";
      }
      else {
        const char *boneName = BLI_str_quoted_substrN(fcurve->rna_path, "pose.bones[");
        name = (boneName) ? id_name(ob) + "_" + std::string(boneName) : "";
      }
    } break;

    case BC_ANIMATION_TYPE_CAMERA: {
      Camera *camera = (Camera *)ob->data;
      name = id_name(ob) + "-" + id_name(camera) + "-camera";
    } break;

    case BC_ANIMATION_TYPE_LIGHT: {
      Light *lamp = (Light *)ob->data;
      name = id_name(ob) + "-" + id_name(lamp) + "-light";
    } break;

    case BC_ANIMATION_TYPE_MATERIAL: {
      Material *ma = give_current_material(ob, this->curve_key.get_subindex() + 1);
      name = id_name(ob) + "-" + id_name(ma) + "-material";
    } break;

    default: {
      name = "";
    }
  }

  return name;
}

const int BCAnimationCurve::get_channel_index() const
{
  return curve_key.get_array_index();
}

const int BCAnimationCurve::get_subindex() const
{
  return curve_key.get_subindex();
}

const std::string BCAnimationCurve::get_rna_path() const
{
  return curve_key.get_path();
}

const int BCAnimationCurve::sample_count() const
{
  if (fcurve == NULL) {
    return 0;
  }
  return fcurve->totvert;
}

const int BCAnimationCurve::closest_index_above(const float sample_frame, const int start_at) const
{
  if (fcurve == NULL) {
    return -1;
  }

  const int cframe = fcurve->bezt[start_at].vec[1][0];  // inacurate!

  if (fabs(cframe - sample_frame) < 0.00001) {
    return start_at;
  }
  return (fcurve->totvert > start_at + 1) ? start_at + 1 : start_at;
}

const int BCAnimationCurve::closest_index_below(const float sample_frame) const
{
  if (fcurve == NULL) {
    return -1;
  }

  float lower_frame = sample_frame;
  float upper_frame = sample_frame;
  int lower_index = 0;
  int upper_index = 0;

  for (int fcu_index = 0; fcu_index < fcurve->totvert; ++fcu_index) {
    upper_index = fcu_index;

    const int cframe = fcurve->bezt[fcu_index].vec[1][0];  // inacurate!
    if (cframe <= sample_frame) {
      lower_frame = cframe;
      lower_index = fcu_index;
    }
    if (cframe >= sample_frame) {
      upper_frame = cframe;
      break;
    }
  }

  if (lower_index == upper_index) {
    return lower_index;
  }

  const float fraction = float(sample_frame - lower_frame) / (upper_frame - lower_frame);
  return (fraction < 0.5) ? lower_index : upper_index;
}

const int BCAnimationCurve::get_interpolation_type(float sample_frame) const
{
  const int index = closest_index_below(sample_frame);
  if (index < 0) {
    return BEZT_IPO_BEZ;
  }
  return fcurve->bezt[index].ipo;
}

const FCurve *BCAnimationCurve::get_fcurve() const
{
  return fcurve;
}

FCurve *BCAnimationCurve::get_edit_fcurve()
{
  if (!curve_is_local_copy) {
    const int index = curve_key.get_array_index();
    const std::string &path = curve_key.get_path();
    fcurve = create_fcurve(index, path.c_str());

    /* Caution here:
     * Replacing the pointer here is OK only because the original value
     * of FCurve was a const pointer into Blender territory. We do not
     * touch that! We use the local copy to prepare data for export. */

    curve_is_local_copy = true;
  }
  return fcurve;
}

void BCAnimationCurve::clean_handles()
{
  if (fcurve == NULL) {
    fcurve = get_edit_fcurve();
  }

  /* Keep old bezt data for copy)*/
  BezTriple *old_bezts = fcurve->bezt;
  int totvert = fcurve->totvert;
  fcurve->bezt = NULL;
  fcurve->totvert = 0;

  for (int i = 0; i < totvert; i++) {
    BezTriple *bezt = &old_bezts[i];
    float x = bezt->vec[1][0];
    float y = bezt->vec[1][1];
    insert_vert_fcurve(fcurve, x, y, (eBezTriple_KeyframeType)BEZKEYTYPE(bezt), INSERTKEY_NOFLAGS);
    BezTriple *lastb = fcurve->bezt + (fcurve->totvert - 1);
    lastb->f1 = lastb->f2 = lastb->f3 = 0;
  }

  /* now free the memory used by the old BezTriples */
  if (old_bezts) {
    MEM_freeN(old_bezts);
  }
}

const bool BCAnimationCurve::is_transform_curve() const
{
  std::string channel_type = this->get_channel_type();
  return (is_rotation_curve() || channel_type == "scale" || channel_type == "location");
}

const bool BCAnimationCurve::is_rotation_curve() const
{
  std::string channel_type = this->get_channel_type();
  return (channel_type == "rotation" || channel_type == "rotation_euler" ||
          channel_type == "rotation_quaternion");
}

const float BCAnimationCurve::get_value(const float frame)
{
  if (fcurve) {
    return evaluate_fcurve(fcurve, frame);
  }
  return 0;  // TODO: handle case where neither sample nor fcu exist
}

void BCAnimationCurve::update_range(float val)
{
  if (val < min) {
    min = val;
  }
  if (val > max) {
    max = val;
  }
}

void BCAnimationCurve::init_range(float val)
{
  min = max = val;
}

void BCAnimationCurve::adjust_range(const int frame_index)
{
  if (fcurve && fcurve->totvert > 1) {
    const float eval = evaluate_fcurve(fcurve, frame_index);

    int first_frame = fcurve->bezt[0].vec[1][0];
    if (first_frame == frame_index) {
      init_range(eval);
    }
    else {
      update_range(eval);
    }
  }
}

void BCAnimationCurve::add_value(const float val, const int frame_index)
{
  FCurve *fcu = get_edit_fcurve();
  fcu->auto_smoothing = FCURVE_SMOOTH_CONT_ACCEL;
  insert_vert_fcurve(fcu, frame_index, val, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NOFLAGS);

  if (fcu->totvert == 1) {
    init_range(val);
  }
  else {
    update_range(val);
  }
}

bool BCAnimationCurve::add_value_from_matrix(const BCSample &sample, const int frame_index)
{
  int array_index = curve_key.get_array_index();

  /* transformation curves are fed directly from the transformation matrix
   * to resolve parent inverse matrix issues with object hierarchies.
   * Maybe this can be unified with the
   */
  const std::string channel_target = get_channel_target();
  float val = 0;
  /* Pick the value from the sample according to the definition of the FCurve */
  bool good = sample.get_value(channel_target, array_index, &val);
  if (good) {
    add_value(val, frame_index);
  }
  return good;
}

bool BCAnimationCurve::add_value_from_rna(const int frame_index)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  float value = 0.0f;
  int array_index = curve_key.get_array_index();
  const std::string full_path = curve_key.get_full_path();

  /* get property to read from, and get value as appropriate */
  bool path_resolved = RNA_path_resolve_full(
      &id_ptr, full_path.c_str(), &ptr, &prop, &array_index);
  if (!path_resolved && array_index == 0) {
    const std::string rna_path = curve_key.get_path();
    path_resolved = RNA_path_resolve_full(&id_ptr, rna_path.c_str(), &ptr, &prop, &array_index);
  }

  if (path_resolved) {
    bool is_array = RNA_property_array_check(prop);
    if (is_array) {
      /* array */
      if ((array_index >= 0) && (array_index < RNA_property_array_length(&ptr, prop))) {
        switch (RNA_property_type(prop)) {
          case PROP_BOOLEAN:
            value = (float)RNA_property_boolean_get_index(&ptr, prop, array_index);
            break;
          case PROP_INT:
            value = (float)RNA_property_int_get_index(&ptr, prop, array_index);
            break;
          case PROP_FLOAT:
            value = RNA_property_float_get_index(&ptr, prop, array_index);
            break;
          default:
            break;
        }
      }
      else {
        fprintf(stderr,
                "Out of Bounds while reading data for Curve %s\n",
                curve_key.get_full_path().c_str());
        return false;
      }
    }
    else {
      /* not an array */
      switch (RNA_property_type(prop)) {
        case PROP_BOOLEAN:
          value = (float)RNA_property_boolean_get(&ptr, prop);
          break;
        case PROP_INT:
          value = (float)RNA_property_int_get(&ptr, prop);
          break;
        case PROP_FLOAT:
          value = RNA_property_float_get(&ptr, prop);
          break;
        case PROP_ENUM:
          value = (float)RNA_property_enum_get(&ptr, prop);
          break;
        default:
          fprintf(stderr,
                  "property type %d not supported for Curve %s\n",
                  RNA_property_type(prop),
                  curve_key.get_full_path().c_str());
          return false;
          break;
      }
    }
  }
  else {
    /* path couldn't be resolved */
    fprintf(stderr, "Path not recognized for Curve %s\n", curve_key.get_full_path().c_str());
    return false;
  }

  add_value(value, frame_index);
  return true;
}

void BCAnimationCurve::get_value_map(BCValueMap &value_map)
{
  value_map.clear();
  if (fcurve == NULL) {
    return;
  }

  for (int i = 0; i < fcurve->totvert; i++) {
    const float frame = fcurve->bezt[i].vec[1][0];
    const float val = fcurve->bezt[i].vec[1][1];
    value_map[frame] = val;
  }
}

void BCAnimationCurve::get_frames(BCFrames &frames) const
{
  frames.clear();
  if (fcurve) {
    for (int i = 0; i < fcurve->totvert; i++) {
      const float val = fcurve->bezt[i].vec[1][0];
      frames.push_back(val);
    }
  }
}

void BCAnimationCurve::get_values(BCValues &values) const
{
  values.clear();
  if (fcurve) {
    for (int i = 0; i < fcurve->totvert; i++) {
      const float val = fcurve->bezt[i].vec[1][1];
      values.push_back(val);
    }
  }
}

bool BCAnimationCurve::is_animated()
{
  static float MIN_DISTANCE = 0.00001;
  return fabs(max - min) > MIN_DISTANCE;
}

bool BCAnimationCurve::is_keyframe(int frame)
{
  if (this->fcurve == NULL) {
    return false;
  }

  for (int i = 0; i < fcurve->totvert; ++i) {
    const int cframe = nearbyint(fcurve->bezt[i].vec[1][0]);
    if (cframe == frame) {
      return true;
    }
    if (cframe > frame) {
      break;
    }
  }
  return false;
}

/* Needed for adding a BCAnimationCurve into a BCAnimationCurveSet */
inline bool operator<(const BCAnimationCurve &lhs, const BCAnimationCurve &rhs)
{
  std::string lhtgt = lhs.get_channel_target();
  std::string rhtgt = rhs.get_channel_target();
  if (lhtgt == rhtgt) {
    const int lha = lhs.get_channel_index();
    const int rha = rhs.get_channel_index();
    return lha < rha;
  }
  else {
    return lhtgt < rhtgt;
  }
}

BCCurveKey::BCCurveKey()
{
  this->key_type = BC_ANIMATION_TYPE_OBJECT;
  this->rna_path = "";
  this->curve_array_index = 0;
  this->curve_subindex = -1;
}

BCCurveKey::BCCurveKey(const BC_animation_type type,
                       const std::string path,
                       const int array_index,
                       const int subindex)
{
  this->key_type = type;
  this->rna_path = path;
  this->curve_array_index = array_index;
  this->curve_subindex = subindex;
}

void BCCurveKey::operator=(const BCCurveKey &other)
{
  this->key_type = other.key_type;
  this->rna_path = other.rna_path;
  this->curve_array_index = other.curve_array_index;
  this->curve_subindex = other.curve_subindex;
}

const std::string BCCurveKey::get_full_path() const
{
  return this->rna_path + '[' + std::to_string(this->curve_array_index) + ']';
}

const std::string BCCurveKey::get_path() const
{
  return this->rna_path;
}

const int BCCurveKey::get_array_index() const
{
  return this->curve_array_index;
}

const int BCCurveKey::get_subindex() const
{
  return this->curve_subindex;
}

void BCCurveKey::set_object_type(BC_animation_type object_type)
{
  this->key_type = object_type;
}

const BC_animation_type BCCurveKey::get_animation_type() const
{
  return this->key_type;
}

const bool BCCurveKey::operator<(const BCCurveKey &other) const
{
  /* needed for using this class as key in maps and sets */
  if (this->key_type != other.key_type) {
    return this->key_type < other.key_type;
  }

  if (this->curve_subindex != other.curve_subindex) {
    return this->curve_subindex < other.curve_subindex;
  }

  if (this->rna_path != other.rna_path) {
    return this->rna_path < other.rna_path;
  }

  return this->curve_array_index < other.curve_array_index;
}

BCBezTriple::BCBezTriple(BezTriple &bezt) : bezt(bezt)
{
}

const float BCBezTriple::get_frame() const
{
  return bezt.vec[1][0];
}

const float BCBezTriple::get_time(Scene *scene) const
{
  return FRA2TIME(bezt.vec[1][0]);
}

const float BCBezTriple::get_value() const
{
  return bezt.vec[1][1];
}

const float BCBezTriple::get_angle() const
{
  return RAD2DEGF(get_value());
}

void BCBezTriple::get_in_tangent(Scene *scene, float point[2], bool as_angle) const
{
  get_tangent(scene, point, as_angle, 0);
}

void BCBezTriple::get_out_tangent(Scene *scene, float point[2], bool as_angle) const
{
  get_tangent(scene, point, as_angle, 2);
}

void BCBezTriple::get_tangent(Scene *scene, float point[2], bool as_angle, int index) const
{
  point[0] = FRA2TIME(bezt.vec[index][0]);
  if (bezt.ipo != BEZT_IPO_BEZ) {
    /* We're in a mixed interpolation scenario, set zero as it's irrelevant but value might contain
     * unused data */
    point[0] = 0;
    point[1] = 0;
  }
  else if (as_angle) {
    point[1] = RAD2DEGF(bezt.vec[index][1]);
  }
  else {
    point[1] = bezt.vec[index][1];
  }
}

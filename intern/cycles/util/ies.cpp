/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>

#include "util/foreach.h"
#include "util/ies.h"
#include "util/math.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

// NOTE: For some reason gcc-7.2 does not instantiate this version of the
// allocator here (used in IESTextParser). Works fine for gcc-6, gcc-7.3 and gcc-8.
//
// TODO(sergey): Get to the root of this issue, or confirm this is a compiler
// issue.
template class GuardedAllocator<char>;

bool IESFile::load(const string &ies)
{
  clear();
  if (!parse(ies) || !process()) {
    clear();
    return false;
  }
  return true;
}

void IESFile::clear()
{
  intensity.clear();
  v_angles.clear();
  h_angles.clear();
}

int IESFile::packed_size()
{
  if (v_angles.size() && h_angles.size() > 0) {
    return 2 + h_angles.size() + v_angles.size() + h_angles.size() * v_angles.size();
  }
  return 0;
}

void IESFile::pack(float *data)
{
  if (v_angles.size() && h_angles.size()) {
    *(data++) = __int_as_float(h_angles.size());
    *(data++) = __int_as_float(v_angles.size());

    memcpy(data, &h_angles[0], h_angles.size() * sizeof(float));
    data += h_angles.size();
    memcpy(data, &v_angles[0], v_angles.size() * sizeof(float));
    data += v_angles.size();

    for (int h = 0; h < intensity.size(); h++) {
      memcpy(data, &intensity[h][0], v_angles.size() * sizeof(float));
      data += v_angles.size();
    }
  }
}

class IESTextParser {
 public:
  string text;
  char *data;
  bool error;

  IESTextParser(const string &str) : text(str), error(false)
  {
    std::replace(text.begin(), text.end(), ',', ' ');
    data = strstr(&text[0], "\nTILT=");
  }

  bool eof()
  {
    return (data == NULL) || (data[0] == '\0');
  }

  bool has_error()
  {
    return error;
  }

  double get_double()
  {
    if (eof()) {
      error = true;
      return 0.0;
    }
    char *old_data = data;
    double val = strtod(data, &data);
    if (data == old_data) {
      data = NULL;
      error = true;
      return 0.0;
    }
    return val;
  }

  long get_long()
  {
    if (eof()) {
      error = true;
      return 0;
    }
    char *old_data = data;
    long val = strtol(data, &data, 10);
    if (data == old_data) {
      data = NULL;
      error = true;
      return 0;
    }
    return val;
  }
};

bool IESFile::parse(const string &ies)
{
  if (ies.empty()) {
    return false;
  }

  IESTextParser parser(ies);
  if (parser.eof()) {
    return false;
  }

  /* Handle the tilt data block. */
  if (strncmp(parser.data, "\nTILT=INCLUDE", 13) == 0) {
    parser.data += 13;
    parser.get_double();              /* Lamp to Luminaire geometry */
    int num_tilt = parser.get_long(); /* Amount of tilt angles and factors */
    /* Skip over angles and factors. */
    for (int i = 0; i < 2 * num_tilt; i++) {
      parser.get_double();
    }
  }
  else {
    /* Skip to next line. */
    parser.data = strstr(parser.data + 1, "\n");
  }

  if (parser.eof()) {
    return false;
  }
  parser.data++;

  parser.get_long();                    /* Number of lamps */
  parser.get_double();                  /* Lumens per lamp */
  double factor = parser.get_double();  /* Candela multiplier */
  int v_angles_num = parser.get_long(); /* Number of vertical angles */
  int h_angles_num = parser.get_long(); /* Number of horizontal angles */
  type = (IESType)parser.get_long();    /* Photometric type */

  if (type != TYPE_A && type != TYPE_B && type != TYPE_C) {
    return false;
  }

  parser.get_long();             /* Unit of the geometry data */
  parser.get_double();           /* Width */
  parser.get_double();           /* Length */
  parser.get_double();           /* Height */
  factor *= parser.get_double(); /* Ballast factor */
  factor *= parser.get_double(); /* Ballast-Lamp Photometric factor */
  parser.get_double();           /* Input Watts */

  /* Intensity values in IES files are specified in candela (lumen/sr), a photometric quantity.
   * Cycles expects radiometric quantities, though, which requires a conversion.
   * However, the Luminous efficacy (ratio of lumens per Watt) depends on the spectral distribution
   * of the light source since lumens take human perception into account.
   * Since this spectral distribution is not known from the IES file, a typical one must be
   * assumed. The D65 standard illuminant has a Luminous efficacy of 177.83, which is used here to
   * convert to Watt/sr. A more advanced approach would be to add a Blackbody Temperature input to
   * the node and numerically integrate the Luminous efficacy from the resulting spectral
   * distribution. Also, the Watt/sr value must be multiplied by 4*pi to get the Watt value that
   * Cycles expects for lamp strength. Therefore, the conversion here uses 4*pi/177.83 as a Candela
   * to Watt factor.
   */
  factor *= 0.0706650768394;

  v_angles.reserve(v_angles_num);
  for (int i = 0; i < v_angles_num; i++) {
    v_angles.push_back((float)parser.get_double());
  }

  h_angles.reserve(h_angles_num);
  for (int i = 0; i < h_angles_num; i++) {
    h_angles.push_back((float)parser.get_double());
  }

  intensity.resize(h_angles_num);
  for (int i = 0; i < h_angles_num; i++) {
    intensity[i].reserve(v_angles_num);
    for (int j = 0; j < v_angles_num; j++) {
      intensity[i].push_back((float)(factor * parser.get_double()));
    }
  }

  return !parser.has_error();
}

static bool angle_close(float a, float b)
{
  return fabsf(a - b) < 1e-4f;
}

/* Processing functions to turn file contents into the format that Cycles expects.
 * Handles type conversion (the output format is based on Type C), symmetry/mirroring,
 * value shifting etc.
 * Note that this code is much more forgiving than the spec. For example, in type A and B,
 * the range of vertical angles officially must be either exactly 0°-90° or -90°-90°.
 * However, in practice, IES files are all over the place. Therefore, the handling is as
 * flexible as possible, and tries to turn any input into something useful. */

void IESFile::process_type_b()
{
  /* According to the standard, Type B defines a different coordinate system where the polar axis
   * is horizontal, not vertical.
   * To avoid over complicating the conversion logic, we just transpose the angles and use the
   * regular Type A/C coordinate system. Users can just rotate the light to get the "proper"
   * orientation. */
  vector<vector<float>> newintensity;
  newintensity.resize(v_angles.size());
  for (int i = 0; i < v_angles.size(); i++) {
    newintensity[i].reserve(h_angles.size());
    for (int j = 0; j < h_angles.size(); j++) {
      newintensity[i].push_back(intensity[j][i]);
    }
  }
  intensity.swap(newintensity);
  h_angles.swap(v_angles);

  if (angle_close(h_angles[0], 0.0f)) {
    /* File angles cover 0°-90°. Mirror that to -90°-90°, and shift to 0°-180° to match Cycles. */
    vector<float> new_h_angles;
    vector<vector<float>> new_intensity;
    int hnum = h_angles.size();
    new_h_angles.reserve(2 * hnum - 1);
    new_intensity.reserve(2 * hnum - 1);
    for (int i = hnum - 1; i > 0; i--) {
      new_h_angles.push_back(90.0f - h_angles[i]);
      new_intensity.push_back(intensity[i]);
    }
    for (int i = 0; i < hnum; i++) {
      new_h_angles.push_back(90.0f + h_angles[i]);
      new_intensity.push_back(intensity[i]);
    }
    h_angles.swap(new_h_angles);
    intensity.swap(new_intensity);
  }
  else {
    /* File angles cover -90°-90°. Shift to 0°-180° to match Cycles. */
    for (int i = 0; i < h_angles.size(); i++) {
      h_angles[i] += 90.0f;
    }
  }

  if (angle_close(v_angles[0], 0.0f)) {
    /* File angles cover 0°-90°. Mirror that to -90°-90°, and shift to 0°-180° to match Cycles. */
    vector<float> new_v_angles;
    int hnum = h_angles.size();
    int vnum = v_angles.size();
    new_v_angles.reserve(2 * vnum - 1);
    for (int i = vnum - 1; i > 0; i--) {
      new_v_angles.push_back(90.0f - v_angles[i]);
    }
    for (int i = 0; i < vnum; i++) {
      new_v_angles.push_back(90.0f + v_angles[i]);
    }
    for (int i = 0; i < hnum; i++) {
      vector<float> new_intensity;
      new_intensity.reserve(2 * vnum - 1);
      for (int j = vnum - 1; j > 0; j--) {
        new_intensity.push_back(intensity[i][j]);
      }
      new_intensity.insert(new_intensity.end(), intensity[i].begin(), intensity[i].end());
      intensity[i].swap(new_intensity);
    }
    v_angles.swap(new_v_angles);
  }
  else {
    /* File angles cover -90°-90°. Shift to 0°-180° to match Cycles. */
    for (int i = 0; i < v_angles.size(); i++) {
      v_angles[i] += 90.0f;
    }
  }
}

void IESFile::process_type_a()
{
  /* Convert vertical angles - just a simple offset. */
  for (int i = 0; i < v_angles.size(); i++) {
    v_angles[i] += 90.0f;
  }

  vector<float> new_h_angles;
  new_h_angles.reserve(h_angles.size());
  vector<vector<float>> new_intensity;
  new_intensity.reserve(h_angles.size());

  /* Type A goes from -90° to 90°, which is mapped to 270° to 90° respectively in Type C. */
  for (int i = h_angles.size() - 1; i >= 0; i--) {
    new_h_angles.push_back(180.0f - h_angles[i]);
    new_intensity.push_back(intensity[i]);
  }

  /* If the file angles start at 0°, we need to mirror around that.
   * Since the negative input range (which we generate here) maps to 180° to 270°,
   * it comes after the original entries in the output. */
  if (angle_close(h_angles[0], 0.0f)) {
    new_h_angles.reserve(2 * h_angles.size() - 1);
    new_intensity.reserve(2 * h_angles.size() - 1);
    for (int i = 1; i < h_angles.size(); i++) {
      new_h_angles.push_back(180.0f + h_angles[i]);
      new_intensity.push_back(intensity[i]);
    }
  }

  h_angles.swap(new_h_angles);
  intensity.swap(new_intensity);
}

void IESFile::process_type_c()
{
  if (angle_close(h_angles[0], 90.0f)) {
    /* Some files are stored from 90° to 270°, so rotate them to the regular 0°-180° range. */
    for (int i = 0; i < h_angles.size(); i++) {
      h_angles[i] -= 90.0f;
    }
  }

  if (h_angles.size() == 1) {
    h_angles[0] = 0.0f;
    h_angles.push_back(360.0f);
    intensity.push_back(intensity[0]);
  }

  if (angle_close(h_angles[h_angles.size() - 1], 90.0f)) {
    /* Only one quadrant is defined, so we need to mirror twice (from one to two, then to four).
     * Since the two->four mirroring step might also be required if we get an input of two
     * quadrants, we only do the first mirror here and later do the second mirror in either case.
     */
    int hnum = h_angles.size();
    for (int i = hnum - 2; i >= 0; i--) {
      h_angles.push_back(180.0f - h_angles[i]);
      intensity.push_back(intensity[i]);
    }
  }

  if (angle_close(h_angles[h_angles.size() - 1], 180.0f)) {
    /* Mirror half to the full range. */
    int hnum = h_angles.size();
    for (int i = hnum - 2; i >= 0; i--) {
      h_angles.push_back(360.0f - h_angles[i]);
      intensity.push_back(intensity[i]);
    }
  }

  /* Some files skip the 360° entry (contrary to standard) because it's supposed to be identical to
   * the 0° entry. If the file has a discernible order in its spacing, just fix this. */
  if (angle_close(h_angles[0], 0.0f) && !angle_close(h_angles[h_angles.size() - 1], 360.0f)) {
    int hnum = h_angles.size();
    float last_step = h_angles[hnum - 1] - h_angles[hnum - 2];
    float first_step = h_angles[1] - h_angles[0];
    float gap_step = 360.0f - h_angles[hnum - 1];
    if (angle_close(last_step, gap_step) || angle_close(first_step, gap_step)) {
      h_angles.push_back(360.0f);
      intensity.push_back(intensity[0]);
    }
  }
}

bool IESFile::process()
{
  if (h_angles.size() == 0 || v_angles.size() == 0) {
    return false;
  }

  if (type == TYPE_A) {
    process_type_a();
  }
  else if (type == TYPE_B) {
    process_type_b();
  }
  else if (type == TYPE_C) {
    process_type_c();
  }
  else {
    return false;
  }

  /* Convert from deg to rad. */
  for (int i = 0; i < v_angles.size(); i++) {
    v_angles[i] *= M_PI_F / 180.f;
  }
  for (int i = 0; i < h_angles.size(); i++) {
    h_angles[i] *= M_PI_F / 180.f;
  }

  return true;
}

IESFile::~IESFile()
{
  clear();
}

CCL_NAMESPACE_END

/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "SceneHash.h"

#include "BLI_sys_types.h"

#include <sstream>

namespace Freestyle {

string SceneHash::toString()
{
  stringstream ss;
  ss << hex << _sum;
  return ss.str();
}

void SceneHash::visitNodeViewLayer(NodeViewLayer &node)
{
  RenderData *r = &node.scene().r;
  adler32((uchar *)&r->xsch, sizeof(r->xsch));  // resolution_x
  adler32((uchar *)&r->ysch, sizeof(r->ysch));  // resolution_y
  adler32((uchar *)&r->size, sizeof(r->size));  // resolution_percentage

  FreestyleConfig *config = &node.sceneLayer().freestyle_config;
  adler32((uchar *)&config->flags, sizeof(config->flags));
  adler32((uchar *)&config->crease_angle, sizeof(config->crease_angle));
  adler32((uchar *)&config->sphere_radius, sizeof(config->sphere_radius));
  adler32((uchar *)&config->dkr_epsilon, sizeof(config->dkr_epsilon));
}

void SceneHash::visitNodeCamera(NodeCamera &cam)
{
  double *proj = cam.projectionMatrix();
  for (int i = 0; i < 16; i++) {
    adler32((uchar *)&proj[i], sizeof(double));
  }
}

void SceneHash::visitIndexedFaceSet(IndexedFaceSet &ifs)
{
  const float *v = ifs.vertices();
  const uint n = ifs.vsize();

  for (uint i = 0; i < n; i++) {
    adler32((uchar *)&v[i], sizeof(v[i]));
  }
}

static const int MOD_ADLER = 65521;

void SceneHash::adler32(const uchar *data, int size)
{
  uint32_t sum1 = _sum & 0xffff;
  uint32_t sum2 = (_sum >> 16) & 0xffff;

  for (int i = 0; i < size; i++) {
    sum1 = (sum1 + data[i]) % MOD_ADLER;
    sum2 = (sum1 + sum2) % MOD_ADLER;
  }
  _sum = sum1 | (sum2 << 16);
}

} /* namespace Freestyle */

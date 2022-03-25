/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "SceneHash.h"

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
  struct RenderData *r = &node.scene().r;
  adler32((unsigned char *)&r->xsch, sizeof(r->xsch));  // resolution_x
  adler32((unsigned char *)&r->ysch, sizeof(r->ysch));  // resolution_y
  adler32((unsigned char *)&r->size, sizeof(r->size));  // resolution_percentage

  struct FreestyleConfig *config = &node.sceneLayer().freestyle_config;
  adler32((unsigned char *)&config->flags, sizeof(config->flags));
  adler32((unsigned char *)&config->crease_angle, sizeof(config->crease_angle));
  adler32((unsigned char *)&config->sphere_radius, sizeof(config->sphere_radius));
  adler32((unsigned char *)&config->dkr_epsilon, sizeof(config->dkr_epsilon));
}

void SceneHash::visitNodeCamera(NodeCamera &cam)
{
  double *proj = cam.projectionMatrix();
  for (int i = 0; i < 16; i++) {
    adler32((unsigned char *)&proj[i], sizeof(double));
  }
}

void SceneHash::visitIndexedFaceSet(IndexedFaceSet &ifs)
{
  const float *v = ifs.vertices();
  const unsigned n = ifs.vsize();

  for (unsigned i = 0; i < n; i++) {
    adler32((unsigned char *)&v[i], sizeof(v[i]));
  }
}

static const int MOD_ADLER = 65521;

void SceneHash::adler32(const unsigned char *data, int size)
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

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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_TextureOperation.h"
#include "COM_WorkScheduler.h"

#include "BKE_image.h"
#include "BKE_node.h"

#include "BLI_listbase.h"
#include "BLI_threads.h"

TextureBaseOperation::TextureBaseOperation()
{
  this->addInputSocket(COM_DT_VECTOR);  // offset
  this->addInputSocket(COM_DT_VECTOR);  // size
  this->m_texture = nullptr;
  this->m_inputSize = nullptr;
  this->m_inputOffset = nullptr;
  this->m_rd = nullptr;
  this->m_pool = nullptr;
  this->m_sceneColorManage = false;
  setComplex(true);
}
TextureOperation::TextureOperation() : TextureBaseOperation()
{
  this->addOutputSocket(COM_DT_COLOR);
}
TextureAlphaOperation::TextureAlphaOperation() : TextureBaseOperation()
{
  this->addOutputSocket(COM_DT_VALUE);
}

void TextureBaseOperation::initExecution()
{
  this->m_inputOffset = getInputSocketReader(0);
  this->m_inputSize = getInputSocketReader(1);
  this->m_pool = BKE_image_pool_new();
  if (this->m_texture != nullptr && this->m_texture->nodetree != nullptr &&
      this->m_texture->use_nodes) {
    ntreeTexBeginExecTree(this->m_texture->nodetree);
  }
  NodeOperation::initExecution();
}
void TextureBaseOperation::deinitExecution()
{
  this->m_inputSize = nullptr;
  this->m_inputOffset = nullptr;
  BKE_image_pool_free(this->m_pool);
  this->m_pool = nullptr;
  if (this->m_texture != nullptr && this->m_texture->use_nodes &&
      this->m_texture->nodetree != nullptr && this->m_texture->nodetree->execdata != nullptr) {
    ntreeTexEndExecTree(this->m_texture->nodetree->execdata);
  }
  NodeOperation::deinitExecution();
}

void TextureBaseOperation::determineResolution(unsigned int resolution[2],
                                               unsigned int preferredResolution[2])
{
  if (preferredResolution[0] == 0 || preferredResolution[1] == 0) {
    int width = this->m_rd->xsch * this->m_rd->size / 100;
    int height = this->m_rd->ysch * this->m_rd->size / 100;
    resolution[0] = width;
    resolution[1] = height;
  }
  else {
    resolution[0] = preferredResolution[0];
    resolution[1] = preferredResolution[1];
  }
}

void TextureAlphaOperation::executePixelSampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float color[4];
  TextureBaseOperation::executePixelSampled(color, x, y, sampler);
  output[0] = color[3];
}

void TextureBaseOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  TexResult texres = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, nullptr};
  float textureSize[4];
  float textureOffset[4];
  float vec[3];
  int retval;
  const float cx = this->getWidth() / 2;
  const float cy = this->getHeight() / 2;
  float u = (x - cx) / this->getWidth() * 2;
  float v = (y - cy) / this->getHeight() * 2;

  /* When no interpolation/filtering happens in multitex() force nearest interpolation.
   * We do it here because (a) we can't easily say multitex() that we want nearest
   * interpolation and (b) in such configuration multitex() simply floor's the value
   * which often produces artifacts.
   */
  if (m_texture != nullptr && (m_texture->imaflag & TEX_INTERPOL) == 0) {
    u += 0.5f / cx;
    v += 0.5f / cy;
  }

  this->m_inputSize->readSampled(textureSize, x, y, sampler);
  this->m_inputOffset->readSampled(textureOffset, x, y, sampler);

  vec[0] = textureSize[0] * (u + textureOffset[0]);
  vec[1] = textureSize[1] * (v + textureOffset[1]);
  vec[2] = textureSize[2] * textureOffset[2];

  const int thread_id = WorkScheduler::current_thread_id();
  retval = multitex_ext(this->m_texture,
                        vec,
                        nullptr,
                        nullptr,
                        0,
                        &texres,
                        thread_id,
                        m_pool,
                        m_sceneColorManage,
                        false);

  if (texres.talpha) {
    output[3] = texres.ta;
  }
  else {
    output[3] = texres.tin;
  }

  if ((retval & TEX_RGB)) {
    output[0] = texres.tr;
    output[1] = texres.tg;
    output[2] = texres.tb;
  }
  else {
    output[0] = output[1] = output[2] = output[3];
  }
}

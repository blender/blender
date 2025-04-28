//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

//------------------------------------------------------------------------------

uint getGlobalInvocationIndex()
{
  uint invocations_per_row = gl_WorkGroupSize.x * gl_NumWorkGroups.x;
  return gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * invocations_per_row;
}

//------------------------------------------------------------------------------

struct Vertex {
  float vertexData[LENGTH];
};

void clear(out Vertex v)
{
  for (int i = 0; i < LENGTH; ++i) {
    v.vertexData[i] = 0;
  }
}

Vertex readVertex(int index)
{
  Vertex v;
  int vertexIndex = srcOffset + index * SRC_STRIDE;
  for (int i = 0; i < LENGTH; ++i) {
    v.vertexData[i] = srcVertexBuffer[vertexIndex + i];
  }
  return v;
}

void writeVertex(int index, Vertex v)
{
  int vertexIndex = dstOffset + index * DST_STRIDE;
  for (int i = 0; i < LENGTH; ++i) {
    dstVertexBuffer[vertexIndex + i] = v.vertexData[i];
  }
}

void addWithWeight(inout Vertex v, const Vertex src, float weight)
{
  for (int i = 0; i < LENGTH; ++i) {
    v.vertexData[i] += weight * src.vertexData[i];
  }
}

#if defined(OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES)
void writeDu(int index, Vertex du)
{
  int duIndex = duDesc.x + index * duDesc.z;
  for (int i = 0; i < LENGTH; ++i) {
    duBuffer[duIndex + i] = du.vertexData[i];
  }
}

void writeDv(int index, Vertex dv)
{
  int dvIndex = dvDesc.x + index * dvDesc.z;
  for (int i = 0; i < LENGTH; ++i) {
    dvBuffer[dvIndex + i] = dv.vertexData[i];
  }
}
#endif

//------------------------------------------------------------------------------

void main()
{
  int current = int(getGlobalInvocationIndex()) + batchStart;

  if (current >= batchEnd) {
    return;
  }

  Vertex dst;
  clear(dst);

  int offset = offsets_buf[current], size = sizes_buf[current];

  for (int stencil = 0; stencil < size; ++stencil) {
    int vindex = offset + stencil;
    addWithWeight(dst, readVertex(indices_buf[vindex]), weights_buf[vindex]);
  }

  writeVertex(current, dst);

#if defined(OPENSUBDIV_GLSL_COMPUTE_USE_1ST_DERIVATIVES)
  Vertex du, dv;
  clear(du);
  clear(dv);
  for (int i = 0; i < size; ++i) {
    // expects the compiler optimizes readVertex out here.
    Vertex src = readVertex(indices_buf[offset + i]);
    addWithWeight(du, src, du_weights_buf[offset + i]);
    addWithWeight(dv, src, dv_weights_buf[offset + i]);
  }

  if (duDesc.y > 0) {  // length
    writeDu(current, du);
  }
  if (dvDesc.y > 0) {
    writeDv(current, dv);
  }
#endif
}

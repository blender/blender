// Adopted from OpenSubdiv with the following license:
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
//       http: //www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.

#ifndef OPENSUBDIV_DEVICE_CONTEXT_CUDA_H_
#define OPENSUBDIV_DEVICE_CONTEXT_CUDA_H_

#ifdef OPENSUBDIV_HAS_CUDA

struct ID3D11Device;

class CudaDeviceContext {
 public:
  CudaDeviceContext();
  ~CudaDeviceContext();

  static bool HAS_CUDA_VERSION_4_0();

  // Initialze cuda device from the current GL context.
  bool Initialize();

  // Initialze cuda device from the ID3D11Device.
  bool Initialize(ID3D11Device* device);

  // Returns true if the cuda device has already been initialized.
  bool IsInitialized() const;

 private:
  bool initialized_;
};

#endif  // OPENSUBDIV_HAS_CUDA

#endif  // _OPENSUBDIV_DEVICE_CONTEXT_CUDA_H_

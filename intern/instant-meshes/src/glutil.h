/*
    glutil.h: Represents the state of an OpenGL shader together with attached
    buffers. The entire state can be serialized and unserialized from a file,
    which is useful for debugging.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "serializer.h"
#include <nanogui/glutil.h>
#include <half.hpp>

class SerializableGLShader : public nanogui::GLShader {
public:
    SerializableGLShader() : nanogui::GLShader() { }

    void load(const Serializer &serializer);
    void save(Serializer &serializer) const;

     // Upload an Eigen matrix and convert to half precision
     template <typename Matrix> void uploadAttrib_half(const std::string &name, const Matrix &_M, int version = -1) {
         Eigen::Matrix<half_float::half, Eigen::Dynamic, Eigen::Dynamic> M = _M.template cast<half_float::half>();
         uploadAttrib(name, M, version);
     }
};

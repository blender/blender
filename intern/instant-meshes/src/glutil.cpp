/*
    glutil.cpp: Represents the state of an OpenGL shader together with attached
    buffers. The entire state can be serialized and unserialized from a file,
    which is useful for debugging.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "glutil.h"

void SerializableGLShader::load(const Serializer &serializer) {
    serializer.pushPrefix(mName);
    std::vector<std::string> key_list = serializer.getKeys();
    std::set<std::string> keys;
    for (auto key : key_list) {
        if (key.find(".") != std::string::npos)
            keys.insert(key.substr(0, key.find(".")));
    }
    bind();
    for (auto key : keys) {
        if (mBufferObjects.find(key) == mBufferObjects.end()) {
            GLuint bufferID;
            glGenBuffers(1, &bufferID);
            mBufferObjects[key].id = bufferID;
        }
        Buffer &buf = mBufferObjects[key];
        MatrixXu8 data;

        serializer.pushPrefix(key);
        serializer.get("glType", buf.glType);
        serializer.get("compSize", buf.compSize);
        serializer.get("dim", buf.dim);
        serializer.get("size", buf.size);
        serializer.get("version", buf.version);
        serializer.get("data", data);
        serializer.popPrefix();

        size_t totalSize = (size_t) buf.size * (size_t) buf.compSize;
        if (key == "indices") {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf.id);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, totalSize,
                         (void *) data.data(), GL_DYNAMIC_DRAW);
        } else {
            int attribID = attrib(key);
            glEnableVertexAttribArray(attribID);
            glBindBuffer(GL_ARRAY_BUFFER, buf.id);
            glBufferData(GL_ARRAY_BUFFER, totalSize, (void *) data.data(),
                         GL_DYNAMIC_DRAW);
            glVertexAttribPointer(attribID, buf.dim, buf.glType,
                                  buf.compSize == 1 ? GL_TRUE : GL_FALSE, 0, 0);
        }
    }
    serializer.popPrefix();
}

void SerializableGLShader::save(Serializer &serializer) const {
    serializer.pushPrefix(mName);
    for (auto &item : mBufferObjects) {
        const Buffer &buf = item.second;
        size_t totalSize = (size_t) buf.size * (size_t) buf.compSize;
        serializer.pushPrefix(item.first);
        serializer.set("glType", buf.glType);
        serializer.set("compSize", buf.compSize);
        serializer.set("dim", buf.dim);
        serializer.set("size", buf.size);
        serializer.set("version", buf.version);
        MatrixXu8 temp(1, totalSize);

        if (item.first == "indices") {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf.id);
            glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, totalSize,
                               temp.data());
        } else {
            glBindBuffer(GL_ARRAY_BUFFER, buf.id);
            glGetBufferSubData(GL_ARRAY_BUFFER, 0, totalSize, temp.data());
        }
        serializer.set("data", temp);
        serializer.popPrefix();
    }
    serializer.popPrefix();
}

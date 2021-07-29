// Copyright (c) 2007, 2008 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBMV_IMAGE_IMAGE_H
#define LIBMV_IMAGE_IMAGE_H

#include <cmath>

#include "libmv/image/array_nd.h"

namespace libmv {

typedef Array3Du ByteImage;  // For backwards compatibility.
typedef Array3Df FloatImage;

// Type added only to manage special 2D array for feature detection
typedef Array3Di IntImage;
typedef Array3Ds ShortImage;

// An image class that is a thin wrapper around Array3D's of various types.
// TODO(keir): Decide if we should add reference counting semantics... Maybe it
// is the best solution after all.
class Image {
 public:

  // Create an image from an array. The image takes ownership of the array.
  Image(Array3Du *array) : array_type_(BYTE), array_(array) {}
  Image(Array3Df *array) : array_type_(FLOAT), array_(array) {}

  Image(const Image &img): array_type_(NONE), array_(NULL) {
    *this = img;
  }

  // Underlying data type.
  enum DataType {
    NONE,
    BYTE,
    FLOAT,
    INT,
    SHORT,
  };

  // Size in bytes that the image takes in memory.
  int MemorySizeInBytes() {
    int size;
    switch (array_type_) {
      case BYTE:
        size = reinterpret_cast<Array3Du *>(array_)->MemorySizeInBytes();
      break;
      case FLOAT:
        size = reinterpret_cast<Array3Df *>(array_)->MemorySizeInBytes();
      break;
      case INT:
        size = reinterpret_cast<Array3Di *>(array_)->MemorySizeInBytes();
      break;
      case SHORT:
        size = reinterpret_cast<Array3Ds *>(array_)->MemorySizeInBytes();
      break;
    default :
      size = 0;
      assert(0);
    }
    size += sizeof(*this);
    return size;
  }

  ~Image() {
    switch (array_type_) {
        case BYTE:
          delete reinterpret_cast<Array3Du *>(array_);

        break;
        case FLOAT:
          delete reinterpret_cast<Array3Df *>(array_);

        break;
        case INT:
          delete reinterpret_cast<Array3Di *>(array_);

        break;
        case SHORT:
          delete reinterpret_cast<Array3Ds *>(array_);

        break;
        default:
          assert(0);
      }
  }

  Image& operator= (const Image& f)  {
    if (this != &f) {
      array_type_ = f.array_type_;
      switch (array_type_) {
        case BYTE:
          delete reinterpret_cast<Array3Du *>(array_);
          array_ = new Array3Du(*(Array3Du *)f.array_);
        break;
        case FLOAT:
          delete reinterpret_cast<Array3Df *>(array_);
          array_ = new Array3Df(*(Array3Df *)f.array_);
        break;
        case INT:
          delete reinterpret_cast<Array3Di *>(array_);
          array_ = new Array3Di(*(Array3Di *)f.array_);
        break;
        case SHORT:
          delete reinterpret_cast<Array3Ds *>(array_);
          array_ = new Array3Ds(*(Array3Ds *)f.array_);
        break;
        default:
          assert(0);
      }
    }
    return *this;
  }

  Array3Du *AsArray3Du() const {
    if (array_type_ == BYTE) {
      return reinterpret_cast<Array3Du *>(array_);
    }
    return NULL;
  }

  Array3Df *AsArray3Df() const {
    if (array_type_ == FLOAT) {
      return reinterpret_cast<Array3Df *>(array_);
    }
    return NULL;
  }

 private:
  DataType array_type_;
  BaseArray *array_;
};

}  // namespace libmv

#endif  // LIBMV_IMAGE_IMAGE_IMAGE_H

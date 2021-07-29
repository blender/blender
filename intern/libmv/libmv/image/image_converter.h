// Copyright (c) 2009 libmv authors.
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

#ifndef LIBMV_IMAGE_IMAGE_CONVERTER_H
#define LIBMV_IMAGE_IMAGE_CONVERTER_H

#include "libmv/image/array_nd.h"

namespace libmv {

// The factor comes from http://www.easyrgb.com/
// RGB to XYZ : Y is the luminance channel
// var_R * 0.2126 + var_G * 0.7152 + var_B * 0.0722
template<typename T>
inline T RGB2GRAY(const T r, const T g, const T b) {
  return static_cast<T>(r * 0.2126 + g * 0.7152 + b * 0.0722);
}

/*
// Specialization for the uchar type. (that do not want to work...)
template<>
inline unsigned char RGB2GRAY<unsigned char>(const unsigned char r,
                                             const unsigned char g,
                                             const unsigned char b) {
  return (unsigned char)(r * 0.2126 + g * 0.7152 + b * 0.0722 +0.5);
}*/

template<class ImageIn, class ImageOut>
void Rgb2Gray(const ImageIn &imaIn, ImageOut *imaOut) {
  // It is all fine to cnvert RGBA image here as well,
  // all the additional channels will be nicely ignored.
  assert(imaIn.Depth() >= 3);

  imaOut->Resize(imaIn.Height(), imaIn.Width(), 1);
  // Convert each RGB pixel into Gray value (luminance)

  for (int j = 0; j < imaIn.Height(); ++j) {
    for (int i = 0; i < imaIn.Width(); ++i)  {
      (*imaOut)(j, i) = RGB2GRAY(imaIn(j, i, 0) , imaIn(j, i, 1), imaIn(j, i, 2));
    }
  }
}

// Convert given float image to an unsigned char array of pixels.
template<class Image>
unsigned char *FloatImageToUCharArray(const Image &image) {
  unsigned char *buffer =
      new unsigned char[image.Width() * image.Height() * image.Depth()];

  for (int y = 0; y < image.Height(); y++) {
    for (int x = 0; x < image.Width(); x++)  {
      for (int d = 0; d < image.Depth(); d++)  {
        int index = (y * image.Width() + x) * image.Depth() + d;
        buffer[index] = 255.0 * image(y, x, d);
      }
    }
  }

  return buffer;
}

}  // namespace libmv

#endif  // LIBMV_IMAGE_IMAGE_CONVERTER_H

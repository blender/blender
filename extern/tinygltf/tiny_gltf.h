//
// Header-only tiny glTF 2.0 loader and serializer.
//
//
// The MIT License (MIT)
//
// Copyright (c) 2015 - Present Syoyo Fujita, Aurélien Chatelain and many
// contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// Version: - v2.8.10
// See https://github.com/syoyo/tinygltf/releases for release history.
//
// Tiny glTF loader is using following third party libraries:
//
//  - jsonhpp: C++ JSON library.
//  - base64: base64 decode/encode library.
//  - stb_image: Image loading library.
//
#ifndef TINY_GLTF_H_
#define TINY_GLTF_H_

#include <array>
#include <cassert>
#include <cmath>  // std::fabs
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

// Auto-detect C++14 standard version
#if !defined(TINYGLTF_USE_CPP14) && defined(__cplusplus) && \
    (__cplusplus >= 201402L)
#define TINYGLTF_USE_CPP14
#endif

#ifdef __ANDROID__
#ifdef TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#include <android/asset_manager.h>
#endif
#endif

#ifdef __GNUC__
#if (__GNUC__ < 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ <= 8))
#define TINYGLTF_NOEXCEPT
#else
#define TINYGLTF_NOEXCEPT noexcept
#endif
#else
#define TINYGLTF_NOEXCEPT noexcept
#endif

#define DEFAULT_METHODS(x)             \
  ~x() = default;                      \
  x(const x &) = default;              \
  x(x &&) TINYGLTF_NOEXCEPT = default; \
  x &operator=(const x &) = default;   \
  x &operator=(x &&) TINYGLTF_NOEXCEPT = default;

namespace tinygltf {

#define TINYGLTF_MODE_POINTS (0)
#define TINYGLTF_MODE_LINE (1)
#define TINYGLTF_MODE_LINE_LOOP (2)
#define TINYGLTF_MODE_LINE_STRIP (3)
#define TINYGLTF_MODE_TRIANGLES (4)
#define TINYGLTF_MODE_TRIANGLE_STRIP (5)
#define TINYGLTF_MODE_TRIANGLE_FAN (6)

#define TINYGLTF_COMPONENT_TYPE_BYTE (5120)
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121)
#define TINYGLTF_COMPONENT_TYPE_SHORT (5122)
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT (5123)
#define TINYGLTF_COMPONENT_TYPE_INT (5124)
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT (5125)
#define TINYGLTF_COMPONENT_TYPE_FLOAT (5126)
#define TINYGLTF_COMPONENT_TYPE_DOUBLE \
  (5130)  // OpenGL double type. Note that some of glTF 2.0 validator does not
          // support double type even the schema seems allow any value of
          // integer:
          // https://github.com/KhronosGroup/glTF/blob/b9884a2fd45130b4d673dd6c8a706ee21ee5c5f7/specification/2.0/schema/accessor.schema.json#L22

#define TINYGLTF_TEXTURE_FILTER_NEAREST (9728)
#define TINYGLTF_TEXTURE_FILTER_LINEAR (9729)
#define TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST (9984)
#define TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST (9985)
#define TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR (9986)
#define TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR (9987)

#define TINYGLTF_TEXTURE_WRAP_REPEAT (10497)
#define TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE (33071)
#define TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT (33648)

// Redeclarations of the above for technique.parameters.
#define TINYGLTF_PARAMETER_TYPE_BYTE (5120)
#define TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE (5121)
#define TINYGLTF_PARAMETER_TYPE_SHORT (5122)
#define TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT (5123)
#define TINYGLTF_PARAMETER_TYPE_INT (5124)
#define TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT (5125)
#define TINYGLTF_PARAMETER_TYPE_FLOAT (5126)

#define TINYGLTF_PARAMETER_TYPE_FLOAT_VEC2 (35664)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 (35665)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_VEC4 (35666)

#define TINYGLTF_PARAMETER_TYPE_INT_VEC2 (35667)
#define TINYGLTF_PARAMETER_TYPE_INT_VEC3 (35668)
#define TINYGLTF_PARAMETER_TYPE_INT_VEC4 (35669)

#define TINYGLTF_PARAMETER_TYPE_BOOL (35670)
#define TINYGLTF_PARAMETER_TYPE_BOOL_VEC2 (35671)
#define TINYGLTF_PARAMETER_TYPE_BOOL_VEC3 (35672)
#define TINYGLTF_PARAMETER_TYPE_BOOL_VEC4 (35673)

#define TINYGLTF_PARAMETER_TYPE_FLOAT_MAT2 (35674)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_MAT3 (35675)
#define TINYGLTF_PARAMETER_TYPE_FLOAT_MAT4 (35676)

#define TINYGLTF_PARAMETER_TYPE_SAMPLER_2D (35678)

// End parameter types

#define TINYGLTF_TYPE_VEC2 (2)
#define TINYGLTF_TYPE_VEC3 (3)
#define TINYGLTF_TYPE_VEC4 (4)
#define TINYGLTF_TYPE_MAT2 (32 + 2)
#define TINYGLTF_TYPE_MAT3 (32 + 3)
#define TINYGLTF_TYPE_MAT4 (32 + 4)
#define TINYGLTF_TYPE_SCALAR (64 + 1)
#define TINYGLTF_TYPE_VECTOR (64 + 4)
#define TINYGLTF_TYPE_MATRIX (64 + 16)

#define TINYGLTF_IMAGE_FORMAT_JPEG (0)
#define TINYGLTF_IMAGE_FORMAT_PNG (1)
#define TINYGLTF_IMAGE_FORMAT_BMP (2)
#define TINYGLTF_IMAGE_FORMAT_GIF (3)

#define TINYGLTF_TEXTURE_FORMAT_ALPHA (6406)
#define TINYGLTF_TEXTURE_FORMAT_RGB (6407)
#define TINYGLTF_TEXTURE_FORMAT_RGBA (6408)
#define TINYGLTF_TEXTURE_FORMAT_LUMINANCE (6409)
#define TINYGLTF_TEXTURE_FORMAT_LUMINANCE_ALPHA (6410)

#define TINYGLTF_TEXTURE_TARGET_TEXTURE2D (3553)
#define TINYGLTF_TEXTURE_TYPE_UNSIGNED_BYTE (5121)

#define TINYGLTF_TARGET_ARRAY_BUFFER (34962)
#define TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER (34963)

#define TINYGLTF_SHADER_TYPE_VERTEX_SHADER (35633)
#define TINYGLTF_SHADER_TYPE_FRAGMENT_SHADER (35632)

#define TINYGLTF_DOUBLE_EPS (1.e-12)
#define TINYGLTF_DOUBLE_EQUAL(a, b) (std::fabs((b) - (a)) < TINYGLTF_DOUBLE_EPS)

#ifdef __ANDROID__
#ifdef TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#ifdef TINYGLTF_IMPLEMENTATION
AAssetManager *asset_manager = nullptr;
#else
extern AAssetManager *asset_manager;
#endif
#endif
#endif

typedef enum {
  NULL_TYPE,
  REAL_TYPE,
  INT_TYPE,
  BOOL_TYPE,
  STRING_TYPE,
  ARRAY_TYPE,
  BINARY_TYPE,
  OBJECT_TYPE
} Type;

typedef enum {
  Permissive,
  Strict
} ParseStrictness;

static inline int32_t GetComponentSizeInBytes(uint32_t componentType) {
  if (componentType == TINYGLTF_COMPONENT_TYPE_BYTE) {
    return 1;
  } else if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
    return 1;
  } else if (componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
    return 2;
  } else if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    return 2;
  } else if (componentType == TINYGLTF_COMPONENT_TYPE_INT) {
    return 4;
  } else if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
    return 4;
  } else if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    return 4;
  } else if (componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
    return 8;
  } else {
    // Unknown component type
    return -1;
  }
}

static inline int32_t GetNumComponentsInType(uint32_t ty) {
  if (ty == TINYGLTF_TYPE_SCALAR) {
    return 1;
  } else if (ty == TINYGLTF_TYPE_VEC2) {
    return 2;
  } else if (ty == TINYGLTF_TYPE_VEC3) {
    return 3;
  } else if (ty == TINYGLTF_TYPE_VEC4) {
    return 4;
  } else if (ty == TINYGLTF_TYPE_MAT2) {
    return 4;
  } else if (ty == TINYGLTF_TYPE_MAT3) {
    return 9;
  } else if (ty == TINYGLTF_TYPE_MAT4) {
    return 16;
  } else {
    // Unknown component type
    return -1;
  }
}

// TODO(syoyo): Move these functions to TinyGLTF class
bool IsDataURI(const std::string &in);
bool DecodeDataURI(std::vector<unsigned char> *out, std::string &mime_type,
                   const std::string &in, size_t reqBytes, bool checkSize);

#ifdef __clang__
#pragma clang diagnostic push
// Suppress warning for : static Value null_value
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wpadded"
#endif

// Simple class to represent JSON object
class Value {
 public:
  typedef std::vector<Value> Array;
  typedef std::map<std::string, Value> Object;

  Value() = default;

  explicit Value(bool b) : type_(BOOL_TYPE) { boolean_value_ = b; }
  explicit Value(int i) : type_(INT_TYPE) {
    int_value_ = i;
    real_value_ = i;
  }
  explicit Value(double n) : type_(REAL_TYPE) { real_value_ = n; }
  explicit Value(const std::string &s) : type_(STRING_TYPE) {
    string_value_ = s;
  }
  explicit Value(std::string &&s)
      : type_(STRING_TYPE), string_value_(std::move(s)) {}
  explicit Value(const char *s) : type_(STRING_TYPE) { string_value_ = s; }
  explicit Value(const unsigned char *p, size_t n) : type_(BINARY_TYPE) {
    binary_value_.resize(n);
    memcpy(binary_value_.data(), p, n);
  }
  explicit Value(std::vector<unsigned char> &&v) noexcept
      : type_(BINARY_TYPE),
        binary_value_(std::move(v)) {}
  explicit Value(const Array &a) : type_(ARRAY_TYPE) { array_value_ = a; }
  explicit Value(Array &&a) noexcept : type_(ARRAY_TYPE),
                                       array_value_(std::move(a)) {}

  explicit Value(const Object &o) : type_(OBJECT_TYPE) { object_value_ = o; }
  explicit Value(Object &&o) noexcept : type_(OBJECT_TYPE),
                                        object_value_(std::move(o)) {}

  DEFAULT_METHODS(Value)

  char Type() const { return static_cast<char>(type_); }

  bool IsBool() const { return (type_ == BOOL_TYPE); }

  bool IsInt() const { return (type_ == INT_TYPE); }

  bool IsNumber() const { return (type_ == REAL_TYPE) || (type_ == INT_TYPE); }

  bool IsReal() const { return (type_ == REAL_TYPE); }

  bool IsString() const { return (type_ == STRING_TYPE); }

  bool IsBinary() const { return (type_ == BINARY_TYPE); }

  bool IsArray() const { return (type_ == ARRAY_TYPE); }

  bool IsObject() const { return (type_ == OBJECT_TYPE); }

  // Use this function if you want to have number value as double.
  double GetNumberAsDouble() const {
    if (type_ == INT_TYPE) {
      return double(int_value_);
    } else {
      return real_value_;
    }
  }

  // Use this function if you want to have number value as int.
  // TODO(syoyo): Support int value larger than 32 bits
  int GetNumberAsInt() const {
    if (type_ == REAL_TYPE) {
      return int(real_value_);
    } else {
      return int_value_;
    }
  }

  // Accessor
  template <typename T>
  const T &Get() const;
  template <typename T>
  T &Get();

  // Lookup value from an array
  const Value &Get(int idx) const {
    static Value null_value;
    assert(IsArray());
    assert(idx >= 0);
    return (static_cast<size_t>(idx) < array_value_.size())
               ? array_value_[static_cast<size_t>(idx)]
               : null_value;
  }

  // Lookup value from a key-value pair
  const Value &Get(const std::string &key) const {
    static Value null_value;
    assert(IsObject());
    Object::const_iterator it = object_value_.find(key);
    return (it != object_value_.end()) ? it->second : null_value;
  }

  size_t ArrayLen() const {
    if (!IsArray()) return 0;
    return array_value_.size();
  }

  // Valid only for object type.
  bool Has(const std::string &key) const {
    if (!IsObject()) return false;
    Object::const_iterator it = object_value_.find(key);
    return (it != object_value_.end()) ? true : false;
  }

  // List keys
  std::vector<std::string> Keys() const {
    std::vector<std::string> keys;
    if (!IsObject()) return keys;  // empty

    for (Object::const_iterator it = object_value_.begin();
         it != object_value_.end(); ++it) {
      keys.push_back(it->first);
    }

    return keys;
  }

  size_t Size() const { return (IsArray() ? ArrayLen() : Keys().size()); }

  bool operator==(const tinygltf::Value &other) const;

 protected:
  int type_ = NULL_TYPE;

  int int_value_ = 0;
  double real_value_ = 0.0;
  std::string string_value_;
  std::vector<unsigned char> binary_value_;
  Array array_value_;
  Object object_value_;
  bool boolean_value_ = false;
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#define TINYGLTF_VALUE_GET(ctype, var)            \
  template <>                                     \
  inline const ctype &Value::Get<ctype>() const { \
    return var;                                   \
  }                                               \
  template <>                                     \
  inline ctype &Value::Get<ctype>() {             \
    return var;                                   \
  }
TINYGLTF_VALUE_GET(bool, boolean_value_)
TINYGLTF_VALUE_GET(double, real_value_)
TINYGLTF_VALUE_GET(int, int_value_)
TINYGLTF_VALUE_GET(std::string, string_value_)
TINYGLTF_VALUE_GET(std::vector<unsigned char>, binary_value_)
TINYGLTF_VALUE_GET(Value::Array, array_value_)
TINYGLTF_VALUE_GET(Value::Object, object_value_)
#undef TINYGLTF_VALUE_GET

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat"
#pragma clang diagnostic ignored "-Wpadded"
#endif

/// Aggregate object for representing a color
using ColorValue = std::array<double, 4>;

// === legacy interface ====
// TODO(syoyo): Deprecate `Parameter` class.
struct Parameter {
  bool bool_value = false;
  bool has_number_value = false;
  std::string string_value;
  std::vector<double> number_array;
  std::map<std::string, double> json_double_value;
  double number_value = 0.0;

  // context sensitive methods. depending the type of the Parameter you are
  // accessing, these are either valid or not
  // If this parameter represent a texture map in a material, will return the
  // texture index

  /// Return the index of a texture if this Parameter is a texture map.
  /// Returned value is only valid if the parameter represent a texture from a
  /// material
  int TextureIndex() const {
    const auto it = json_double_value.find("index");
    if (it != std::end(json_double_value)) {
      return int(it->second);
    }
    return -1;
  }

  /// Return the index of a texture coordinate set if this Parameter is a
  /// texture map. Returned value is only valid if the parameter represent a
  /// texture from a material
  int TextureTexCoord() const {
    const auto it = json_double_value.find("texCoord");
    if (it != std::end(json_double_value)) {
      return int(it->second);
    }
    // As per the spec, if texCoord is omitted, this parameter is 0
    return 0;
  }

  /// Return the scale of a texture if this Parameter is a normal texture map.
  /// Returned value is only valid if the parameter represent a normal texture
  /// from a material
  double TextureScale() const {
    const auto it = json_double_value.find("scale");
    if (it != std::end(json_double_value)) {
      return it->second;
    }
    // As per the spec, if scale is omitted, this parameter is 1
    return 1;
  }

  /// Return the strength of a texture if this Parameter is a an occlusion map.
  /// Returned value is only valid if the parameter represent an occlusion map
  /// from a material
  double TextureStrength() const {
    const auto it = json_double_value.find("strength");
    if (it != std::end(json_double_value)) {
      return it->second;
    }
    // As per the spec, if strength is omitted, this parameter is 1
    return 1;
  }

  /// Material factor, like the roughness or metalness of a material
  /// Returned value is only valid if the parameter represent a texture from a
  /// material
  double Factor() const { return number_value; }

  /// Return the color of a material
  /// Returned value is only valid if the parameter represent a texture from a
  /// material
  ColorValue ColorFactor() const {
    return {
        {// this aggregate initialize the std::array object, and uses C++11 RVO.
         number_array[0], number_array[1], number_array[2],
         (number_array.size() > 3 ? number_array[3] : 1.0)}};
  }

  Parameter() = default;
  DEFAULT_METHODS(Parameter)
  bool operator==(const Parameter &) const;
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

typedef std::map<std::string, Parameter> ParameterMap;
typedef std::map<std::string, Value> ExtensionMap;

struct AnimationChannel {
  int sampler{-1};          // required
  int target_node{-1};      // optional index of the node to target (alternative
                            // target should be provided by extension)
  std::string target_path;  // required with standard values of ["translation",
                            // "rotation", "scale", "weights"]
  Value extras;
  ExtensionMap extensions;
  Value target_extras;
  ExtensionMap target_extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
  std::string target_extras_json_string;
  std::string target_extensions_json_string;

  AnimationChannel() = default;
  DEFAULT_METHODS(AnimationChannel)
  bool operator==(const AnimationChannel &) const;
};

struct AnimationSampler {
  int input{-1};              // required
  int output{-1};             // required
  std::string interpolation;  // "LINEAR", "STEP","CUBICSPLINE" or user defined
                              // string. default "LINEAR"
  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  AnimationSampler() : interpolation("LINEAR") {}
  DEFAULT_METHODS(AnimationSampler)
  bool operator==(const AnimationSampler &) const;
};

struct Animation {
  std::string name;
  std::vector<AnimationChannel> channels;
  std::vector<AnimationSampler> samplers;
  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Animation() = default;
  DEFAULT_METHODS(Animation)
  bool operator==(const Animation &) const;
};

struct Skin {
  std::string name;
  int inverseBindMatrices{-1};  // required here but not in the spec
  int skeleton{-1};             // The index of the node used as a skeleton root
  std::vector<int> joints;      // Indices of skeleton nodes

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Skin() = default;
  DEFAULT_METHODS(Skin)
  bool operator==(const Skin &) const;
};

struct Sampler {
  std::string name;
  // glTF 2.0 spec does not define default value for `minFilter` and
  // `magFilter`. Set -1 in TinyGLTF(issue #186)
  int minFilter =
      -1;  // optional. -1 = no filter defined. ["NEAREST", "LINEAR",
           // "NEAREST_MIPMAP_NEAREST", "LINEAR_MIPMAP_NEAREST",
           // "NEAREST_MIPMAP_LINEAR", "LINEAR_MIPMAP_LINEAR"]
  int magFilter =
      -1;  // optional. -1 = no filter defined. ["NEAREST", "LINEAR"]
  int wrapS =
      TINYGLTF_TEXTURE_WRAP_REPEAT;  // ["CLAMP_TO_EDGE", "MIRRORED_REPEAT",
                                     // "REPEAT"], default "REPEAT"
  int wrapT =
      TINYGLTF_TEXTURE_WRAP_REPEAT;  // ["CLAMP_TO_EDGE", "MIRRORED_REPEAT",
                                     // "REPEAT"], default "REPEAT"
  // int wrapR = TINYGLTF_TEXTURE_WRAP_REPEAT;  // TinyGLTF extension. currently
  // not used.

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Sampler() = default;
  DEFAULT_METHODS(Sampler)
  bool operator==(const Sampler &) const;
};

struct Image {
  std::string name;
  int width{-1};
  int height{-1};
  int component{-1};
  int bits{-1};        // bit depth per channel. 8(byte), 16 or 32.
  int pixel_type{-1};  // pixel type(TINYGLTF_COMPONENT_TYPE_***). usually
                       // UBYTE(bits = 8) or USHORT(bits = 16)
  std::vector<unsigned char> image;
  int bufferView{-1};    // (required if no uri)
  std::string mimeType;  // (required if no uri) ["image/jpeg", "image/png",
                         // "image/bmp", "image/gif"]
  std::string uri;       // (required if no mimeType) uri is not decoded(e.g.
                         // whitespace may be represented as %20)
  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  // When this flag is true, data is stored to `image` in as-is format(e.g. jpeg
  // compressed for "image/jpeg" mime) This feature is good if you use custom
  // image loader function. (e.g. delayed decoding of images for faster glTF
  // parsing) Default parser for Image does not provide as-is loading feature at
  // the moment. (You can manipulate this by providing your own LoadImageData
  // function)
  bool as_is{false};

  Image() = default;
  DEFAULT_METHODS(Image)

  bool operator==(const Image &) const;
};

struct Texture {
  std::string name;

  int sampler{-1};
  int source{-1};
  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Texture() = default;
  DEFAULT_METHODS(Texture)

  bool operator==(const Texture &) const;
};

struct TextureInfo {
  int index{-1};    // required.
  int texCoord{0};  // The set index of texture's TEXCOORD attribute used for
                    // texture coordinate mapping.

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  TextureInfo() = default;
  DEFAULT_METHODS(TextureInfo)
  bool operator==(const TextureInfo &) const;
};

struct NormalTextureInfo {
  int index{-1};    // required
  int texCoord{0};  // The set index of texture's TEXCOORD attribute used for
                    // texture coordinate mapping.
  double scale{
      1.0};  // scaledNormal = normalize((<sampled normal texture value>
             // * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0))

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  NormalTextureInfo() = default;
  DEFAULT_METHODS(NormalTextureInfo)
  bool operator==(const NormalTextureInfo &) const;
};

struct OcclusionTextureInfo {
  int index{-1};    // required
  int texCoord{0};  // The set index of texture's TEXCOORD attribute used for
                    // texture coordinate mapping.
  double strength{1.0};  // occludedColor = lerp(color, color * <sampled
                         // occlusion texture value>, <occlusion strength>)

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  OcclusionTextureInfo() = default;
  DEFAULT_METHODS(OcclusionTextureInfo)
  bool operator==(const OcclusionTextureInfo &) const;
};

// pbrMetallicRoughness class defined in glTF 2.0 spec.
struct PbrMetallicRoughness {
  std::vector<double> baseColorFactor{1.0, 1.0, 1.0, 1.0};  // len = 4. default [1,1,1,1]
  TextureInfo baseColorTexture;
  double metallicFactor{1.0};   // default 1
  double roughnessFactor{1.0};  // default 1
  TextureInfo metallicRoughnessTexture;

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  PbrMetallicRoughness() = default;
  DEFAULT_METHODS(PbrMetallicRoughness)

  bool operator==(const PbrMetallicRoughness &) const;
};

// Each extension should be stored in a ParameterMap.
// members not in the values could be included in the ParameterMap
// to keep a single material model
struct Material {
  std::string name;

  std::vector<double> emissiveFactor{0.0, 0.0, 0.0};  // length 3. default [0, 0, 0]
  std::string alphaMode{"OPAQUE"}; // default "OPAQUE"
  double alphaCutoff{0.5};        // default 0.5
  bool doubleSided{false};        // default false
  std::vector<int> lods;          // level of detail materials (MSFT_lod)

  PbrMetallicRoughness pbrMetallicRoughness;

  NormalTextureInfo normalTexture;
  OcclusionTextureInfo occlusionTexture;
  TextureInfo emissiveTexture;

  // For backward compatibility
  // TODO(syoyo): Remove `values` and `additionalValues` in the next release.
  ParameterMap values;
  ParameterMap additionalValues;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Material() = default;
  DEFAULT_METHODS(Material)

  bool operator==(const Material &) const;
};

struct BufferView {
  std::string name;
  int buffer{-1};        // Required
  size_t byteOffset{0};  // minimum 0, default 0
  size_t byteLength{0};  // required, minimum 1. 0 = invalid
  size_t byteStride{0};  // minimum 4, maximum 252 (multiple of 4), default 0 =
                         // understood to be tightly packed
  int target{0};  // ["ARRAY_BUFFER", "ELEMENT_ARRAY_BUFFER"] for vertex indices
                  // or attribs. Could be 0 for other data
  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  bool dracoDecoded{false};  // Flag indicating this has been draco decoded

  BufferView() = default;
  DEFAULT_METHODS(BufferView)
  bool operator==(const BufferView &) const;
};

struct Accessor {
  int bufferView{-1};  // optional in spec but required here since sparse
                       // accessor are not supported
  std::string name;
  size_t byteOffset{0};
  bool normalized{false};  // optional.
  int componentType{-1};   // (required) One of TINYGLTF_COMPONENT_TYPE_***
  size_t count{0};         // required
  int type{-1};            // (required) One of TINYGLTF_TYPE_***   ..
  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  std::vector<double>
      minValues;  // optional. integer value is promoted to double
  std::vector<double>
      maxValues;  // optional. integer value is promoted to double

  struct Sparse {
    int count;
    bool isSparse;
    struct {
      size_t byteOffset;
      int bufferView;
      int componentType;  // a TINYGLTF_COMPONENT_TYPE_ value
      Value extras;
      ExtensionMap extensions;
      std::string extras_json_string;
      std::string extensions_json_string;
    } indices;
    struct {
      int bufferView;
      size_t byteOffset;
      Value extras;
      ExtensionMap extensions;
      std::string extras_json_string;
      std::string extensions_json_string;
    } values;
    Value extras;
    ExtensionMap extensions;
    std::string extras_json_string;
    std::string extensions_json_string;
  };

  Sparse sparse;

  ///
  /// Utility function to compute byteStride for a given bufferView object.
  /// Returns -1 upon invalid glTF value or parameter configuration.
  ///
  int ByteStride(const BufferView &bufferViewObject) const {
    if (bufferViewObject.byteStride == 0) {
      // Assume data is tightly packed.
      int componentSizeInBytes =
          GetComponentSizeInBytes(static_cast<uint32_t>(componentType));
      if (componentSizeInBytes <= 0) {
        return -1;
      }

      int numComponents = GetNumComponentsInType(static_cast<uint32_t>(type));
      if (numComponents <= 0) {
        return -1;
      }

      return componentSizeInBytes * numComponents;
    } else {
      // Check if byteStride is a multiple of the size of the accessor's
      // component type.
      int componentSizeInBytes =
          GetComponentSizeInBytes(static_cast<uint32_t>(componentType));
      if (componentSizeInBytes <= 0) {
        return -1;
      }

      if ((bufferViewObject.byteStride % uint32_t(componentSizeInBytes)) != 0) {
        return -1;
      }
      return static_cast<int>(bufferViewObject.byteStride);
    }

    // unreachable return 0;
  }

  Accessor()

  {
    sparse.isSparse = false;
  }
  DEFAULT_METHODS(Accessor)
  bool operator==(const tinygltf::Accessor &) const;
};

struct PerspectiveCamera {
  double aspectRatio{0.0};  // min > 0
  double yfov{0.0};         // required. min > 0
  double zfar{0.0};         // min > 0
  double znear{0.0};        // required. min > 0

  PerspectiveCamera() = default;
  DEFAULT_METHODS(PerspectiveCamera)
  bool operator==(const PerspectiveCamera &) const;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct OrthographicCamera {
  double xmag{0.0};   // required. must not be zero.
  double ymag{0.0};   // required. must not be zero.
  double zfar{0.0};   // required. `zfar` must be greater than `znear`.
  double znear{0.0};  // required

  OrthographicCamera() = default;
  DEFAULT_METHODS(OrthographicCamera)
  bool operator==(const OrthographicCamera &) const;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct Camera {
  std::string type;  // required. "perspective" or "orthographic"
  std::string name;

  PerspectiveCamera perspective;
  OrthographicCamera orthographic;

  Camera() = default;
  DEFAULT_METHODS(Camera)
  bool operator==(const Camera &) const;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct Primitive {
  std::map<std::string, int> attributes;  // (required) A dictionary object of
                                          // integer, where each integer
                                          // is the index of the accessor
                                          // containing an attribute.
  int material{-1};  // The index of the material to apply to this primitive
                     // when rendering.
  int indices{-1};   // The index of the accessor that contains the indices.
  int mode{-1};      // one of TINYGLTF_MODE_***
  std::vector<std::map<std::string, int> > targets;  // array of morph targets,
  // where each target is a dict with attributes in ["POSITION, "NORMAL",
  // "TANGENT"] pointing
  // to their corresponding accessors
  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Primitive() = default;
  DEFAULT_METHODS(Primitive)
  bool operator==(const Primitive &) const;
};

struct Mesh {
  std::string name;
  std::vector<Primitive> primitives;
  std::vector<double> weights;  // weights to be applied to the Morph Targets
  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Mesh() = default;
  DEFAULT_METHODS(Mesh)
  bool operator==(const Mesh &) const;
};

class Node {
 public:
  Node() = default;

  DEFAULT_METHODS(Node)

  bool operator==(const Node &) const;

  int camera{-1};  // the index of the camera referenced by this node

  std::string name;
  int skin{-1};
  int mesh{-1};
  int light{-1};    // light source index (KHR_lights_punctual)
  int emitter{-1};  // audio emitter index (KHR_audio)
  std::vector<int> lods; // level of detail nodes (MSFT_lod)
  std::vector<int> children;
  std::vector<double> rotation;     // length must be 0 or 4
  std::vector<double> scale;        // length must be 0 or 3
  std::vector<double> translation;  // length must be 0 or 3
  std::vector<double> matrix;       // length must be 0 or 16
  std::vector<double> weights;  // The weights of the instantiated Morph Target

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct Buffer {
  std::string name;
  std::vector<unsigned char> data;
  std::string
      uri;  // considered as required here but not in the spec (need to clarify)
            // uri is not decoded(e.g. whitespace may be represented as %20)
  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Buffer() = default;
  DEFAULT_METHODS(Buffer)
  bool operator==(const Buffer &) const;
};

struct Asset {
  std::string version = "2.0";  // required
  std::string generator;
  std::string minVersion;
  std::string copyright;
  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Asset() = default;
  DEFAULT_METHODS(Asset)
  bool operator==(const Asset &) const;
};

struct Scene {
  std::string name;
  std::vector<int> nodes;
  std::vector<int> audioEmitters;  // KHR_audio global emitters

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;

  Scene() = default;
  DEFAULT_METHODS(Scene)
  bool operator==(const Scene &) const;
};

struct SpotLight {
  double innerConeAngle{0.0};
  double outerConeAngle{0.7853981634};

  SpotLight() = default;
  DEFAULT_METHODS(SpotLight)
  bool operator==(const SpotLight &) const;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct Light {
  std::string name;
  std::vector<double> color;
  double intensity{1.0};
  std::string type;
  double range{0.0};  // 0.0 = infinite
  SpotLight spot;

  Light() = default;
  DEFAULT_METHODS(Light)

  bool operator==(const Light &) const;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct PositionalEmitter {
  double coneInnerAngle{6.283185307179586};
  double coneOuterAngle{6.283185307179586};
  double coneOuterGain{0.0};
  double maxDistance{100.0};
  double refDistance{1.0};
  double rolloffFactor{1.0};

  PositionalEmitter() = default;
  DEFAULT_METHODS(PositionalEmitter)
  bool operator==(const PositionalEmitter &) const;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct AudioEmitter {
  std::string name;
  double gain{1.0};
  bool loop{false};
  bool playing{false};
  std::string
      type;  // positional - Positional audio emitters. Using sound cones, the
             // orientation is +Z having the same front side for a glTF asset.
             // global - Global audio emitters are not affected by the position
             // of audio listeners. coneInnerAngle, coneOuterAngle,
             // coneOuterGain, distanceModel, maxDistance, refDistance, and
             // rolloffFactor should all be ignored when set.
  std::string
      distanceModel;  // linear - A linear distance model calculating the
                      // gain induced by the distance according to: 1.0
                      // - rolloffFactor * (distance - refDistance) /
                      // (maxDistance - refDistance)
                      // inverse - (default) An inverse distance model
                      // calculating the gain induced by the distance according
                      // to: refDistance / (refDistance + rolloffFactor *
                      // (Math.max(distance, refDistance) - refDistance))
                      // exponential - An exponential distance model calculating
                      // the gain induced by the distance according to:
                      // pow((Math.max(distance, refDistance) / refDistance,
                      // -rolloffFactor))
  PositionalEmitter positional;
  int source{-1};

  AudioEmitter() : type("global"), distanceModel("inverse") {}
  DEFAULT_METHODS(AudioEmitter)

  bool operator==(const AudioEmitter &) const;

  ExtensionMap extensions;
  Value extras;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

struct AudioSource {
  std::string name;
  std::string uri;
  int bufferView{-1};  // (required if no uri)
  std::string
      mimeType;  // (required if no uri) The audio's MIME type. Required if
                 // bufferView is defined. Unless specified by another
                 // extension, the only supported mimeType is audio/mpeg.

  AudioSource() = default;
  DEFAULT_METHODS(AudioSource)

  bool operator==(const AudioSource &) const;

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

class Model {
 public:
  Model() = default;
  DEFAULT_METHODS(Model)

  bool operator==(const Model &) const;

  std::vector<Accessor> accessors;
  std::vector<Animation> animations;
  std::vector<Buffer> buffers;
  std::vector<BufferView> bufferViews;
  std::vector<Material> materials;
  std::vector<Mesh> meshes;
  std::vector<Node> nodes;
  std::vector<Texture> textures;
  std::vector<Image> images;
  std::vector<Skin> skins;
  std::vector<Sampler> samplers;
  std::vector<Camera> cameras;
  std::vector<Scene> scenes;
  std::vector<Light> lights;
  std::vector<AudioEmitter> audioEmitters;
  std::vector<AudioSource> audioSources;

  int defaultScene{-1};
  std::vector<std::string> extensionsUsed;
  std::vector<std::string> extensionsRequired;

  Asset asset;

  Value extras;
  ExtensionMap extensions;

  // Filled when SetStoreOriginalJSONForExtrasAndExtensions is enabled.
  std::string extras_json_string;
  std::string extensions_json_string;
};

enum SectionCheck {
  NO_REQUIRE = 0x00,
  REQUIRE_VERSION = 0x01,
  REQUIRE_SCENE = 0x02,
  REQUIRE_SCENES = 0x04,
  REQUIRE_NODES = 0x08,
  REQUIRE_ACCESSORS = 0x10,
  REQUIRE_BUFFERS = 0x20,
  REQUIRE_BUFFER_VIEWS = 0x40,
  REQUIRE_ALL = 0x7f
};

///
/// URIEncodeFunction type. Signature for custom URI encoding of external
/// resources such as .bin and image files. Used by tinygltf to re-encode the
/// final location of saved files. object_type may be used to encode buffer and
/// image URIs differently, for example. See
/// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#uris
///
typedef bool (*URIEncodeFunction)(const std::string &in_uri,
                                  const std::string &object_type,
                                  std::string *out_uri, void *user_data);

///
/// URIDecodeFunction type. Signature for custom URI decoding of external
/// resources such as .bin and image files. Used by tinygltf when computing
/// filenames to write resources.
///
typedef bool (*URIDecodeFunction)(const std::string &in_uri,
                                  std::string *out_uri, void *user_data);

// Declaration of default uri decode function
bool URIDecode(const std::string &in_uri, std::string *out_uri,
               void *user_data);

///
/// A structure containing URI callbacks and a pointer to their user data.
///
struct URICallbacks {
  URIEncodeFunction encode;  // Optional encode method
  URIDecodeFunction decode;  // Required decode method

  void *user_data;  // An argument that is passed to all uri callbacks
};

///
/// LoadImageDataFunction type. Signature for custom image loading callbacks.
///
typedef bool (*LoadImageDataFunction)(Image *, const int, std::string *,
                                      std::string *, int, int,
                                      const unsigned char *, int,
                                      void *user_pointer);

///
/// WriteImageDataFunction type. Signature for custom image writing callbacks.
/// The out_uri parameter becomes the URI written to the gltf and may reference
/// a file or contain a data URI.
///
typedef bool (*WriteImageDataFunction)(const std::string *basepath,
                                       const std::string *filename,
                                       const Image *image, bool embedImages,
                                       const URICallbacks *uri_cb,
                                       std::string *out_uri,
                                       void *user_pointer);

#ifndef TINYGLTF_NO_STB_IMAGE
// Declaration of default image loader callback
bool LoadImageData(Image *image, const int image_idx, std::string *err,
                   std::string *warn, int req_width, int req_height,
                   const unsigned char *bytes, int size, void *);
#endif

#ifndef TINYGLTF_NO_STB_IMAGE_WRITE
// Declaration of default image writer callback
bool WriteImageData(const std::string *basepath, const std::string *filename,
                    const Image *image, bool embedImages,
                    const URICallbacks *uri_cb, std::string *out_uri, void *);
#endif

///
/// FilExistsFunction type. Signature for custom filesystem callbacks.
///
typedef bool (*FileExistsFunction)(const std::string &abs_filename, void *);

///
/// ExpandFilePathFunction type. Signature for custom filesystem callbacks.
///
typedef std::string (*ExpandFilePathFunction)(const std::string &, void *);

///
/// ReadWholeFileFunction type. Signature for custom filesystem callbacks.
///
typedef bool (*ReadWholeFileFunction)(std::vector<unsigned char> *,
                                      std::string *, const std::string &,
                                      void *);

///
/// WriteWholeFileFunction type. Signature for custom filesystem callbacks.
///
typedef bool (*WriteWholeFileFunction)(std::string *, const std::string &,
                                       const std::vector<unsigned char> &,
                                       void *);

///
/// GetFileSizeFunction type. Signature for custom filesystem callbacks.
///
typedef bool (*GetFileSizeFunction)(size_t *filesize_out, std::string *err,
                                    const std::string &abs_filename,
                                    void *userdata);

///
/// A structure containing all required filesystem callbacks and a pointer to
/// their user data.
///
struct FsCallbacks {
  FileExistsFunction FileExists;
  ExpandFilePathFunction ExpandFilePath;
  ReadWholeFileFunction ReadWholeFile;
  WriteWholeFileFunction WriteWholeFile;
  GetFileSizeFunction GetFileSizeInBytes;  // To avoid GetFileSize Win32 API,
                                           // add `InBytes` suffix.

  void *user_data;  // An argument that is passed to all fs callbacks
};

#ifndef TINYGLTF_NO_FS
// Declaration of default filesystem callbacks

bool FileExists(const std::string &abs_filename, void *);

///
/// Expand file path(e.g. `~` to home directory on posix, `%APPDATA%` to
/// `C:\\Users\\tinygltf\\AppData`)
///
/// @param[in] filepath File path string. Assume UTF-8
/// @param[in] userdata User data. Set to `nullptr` if you don't need it.
///
std::string ExpandFilePath(const std::string &filepath, void *userdata);

bool ReadWholeFile(std::vector<unsigned char> *out, std::string *err,
                   const std::string &filepath, void *);

bool WriteWholeFile(std::string *err, const std::string &filepath,
                    const std::vector<unsigned char> &contents, void *);

bool GetFileSizeInBytes(size_t *filesize_out, std::string *err,
                        const std::string &filepath, void *);
#endif

///
/// glTF Parser/Serializer context.
///
class TinyGLTF {
 public:
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat"
#endif

  TinyGLTF() = default;

#ifdef __clang__
#pragma clang diagnostic pop
#endif

  ~TinyGLTF() = default;

  ///
  /// Loads glTF ASCII asset from a file.
  /// Set warning message to `warn` for example it fails to load asserts.
  /// Returns false and set error string to `err` if there's an error.
  ///
  bool LoadASCIIFromFile(Model *model, std::string *err, std::string *warn,
                         const std::string &filename,
                         unsigned int check_sections = REQUIRE_VERSION);

  ///
  /// Loads glTF ASCII asset from string(memory).
  /// `length` = strlen(str);
  /// `base_dir` is a search path of glTF asset(e.g. images). Path Must be an
  /// expanded path (e.g. no tilde(`~`), no environment variables). Set warning
  /// message to `warn` for example it fails to load asserts. Returns false and
  /// set error string to `err` if there's an error.
  ///
  bool LoadASCIIFromString(Model *model, std::string *err, std::string *warn,
                           const char *str, const unsigned int length,
                           const std::string &base_dir,
                           unsigned int check_sections = REQUIRE_VERSION);

  ///
  /// Loads glTF binary asset from a file.
  /// Set warning message to `warn` for example it fails to load asserts.
  /// Returns false and set error string to `err` if there's an error.
  ///
  bool LoadBinaryFromFile(Model *model, std::string *err, std::string *warn,
                          const std::string &filename,
                          unsigned int check_sections = REQUIRE_VERSION);

  ///
  /// Loads glTF binary asset from memory.
  /// `length` = strlen(str);
  /// `base_dir` is a search path of glTF asset(e.g. images). Path Must be an
  /// expanded path (e.g. no tilde(`~`), no environment variables).
  /// Set warning message to `warn` for example it fails to load asserts.
  /// Returns false and set error string to `err` if there's an error.
  ///
  bool LoadBinaryFromMemory(Model *model, std::string *err, std::string *warn,
                            const unsigned char *bytes,
                            const unsigned int length,
                            const std::string &base_dir = "",
                            unsigned int check_sections = REQUIRE_VERSION);

  ///
  /// Write glTF to stream, buffers and images will be embedded
  ///
  bool WriteGltfSceneToStream(const Model *model, std::ostream &stream,
                              bool prettyPrint, bool writeBinary);

  ///
  /// Write glTF to file.
  ///
  bool WriteGltfSceneToFile(const Model *model, const std::string &filename,
                            bool embedImages, bool embedBuffers,
                            bool prettyPrint, bool writeBinary);

  ///
  /// Sets the parsing strictness.
  ///
  void SetParseStrictness(ParseStrictness strictness);

  ///
  /// Set callback to use for loading image data
  ///
  void SetImageLoader(LoadImageDataFunction LoadImageData, void *user_data);

  ///
  /// Unset(remove) callback of loading image data
  ///
  void RemoveImageLoader();

  ///
  /// Set callback to use for writing image data
  ///
  void SetImageWriter(WriteImageDataFunction WriteImageData, void *user_data);

  ///
  /// Set callbacks to use for URI encoding and decoding and their user data
  ///
  void SetURICallbacks(URICallbacks callbacks);

  ///
  /// Set callbacks to use for filesystem (fs) access and their user data
  ///
  void SetFsCallbacks(FsCallbacks callbacks);

  ///
  /// Set serializing default values(default = false).
  /// When true, default values are force serialized to .glTF.
  /// This may be helpful if you want to serialize a full description of glTF
  /// data.
  ///
  /// TODO(LTE): Supply parsing option as function arguments to
  /// `LoadASCIIFromFile()` and others, not by a class method
  ///
  void SetSerializeDefaultValues(const bool enabled) {
    serialize_default_values_ = enabled;
  }

  bool GetSerializeDefaultValues() const { return serialize_default_values_; }

  ///
  /// Store original JSON string for `extras` and `extensions`.
  /// This feature will be useful when the user want to reconstruct custom data
  /// structure from JSON string.
  ///
  void SetStoreOriginalJSONForExtrasAndExtensions(const bool enabled) {
    store_original_json_for_extras_and_extensions_ = enabled;
  }

  bool GetStoreOriginalJSONForExtrasAndExtensions() const {
    return store_original_json_for_extras_and_extensions_;
  }

  ///
  /// Specify whether preserve image channels when loading images or not.
  /// (Not effective when the user supplies their own LoadImageData callbacks)
  ///
  void SetPreserveImageChannels(bool onoff) {
    preserve_image_channels_ = onoff;
  }

  ///
  /// Set maximum allowed external file size in bytes.
  /// Default: 2GB
  /// Only effective for built-in ReadWholeFileFunction FS function.
  ///
  void SetMaxExternalFileSize(size_t max_bytes) {
    max_external_file_size_ = max_bytes;
  }

  size_t GetMaxExternalFileSize() const { return max_external_file_size_; }

  bool GetPreserveImageChannels() const { return preserve_image_channels_; }

 private:
  ///
  /// Loads glTF asset from string(memory).
  /// `length` = strlen(str);
  /// Set warning message to `warn` for example it fails to load asserts
  /// Returns false and set error string to `err` if there's an error.
  ///
  bool LoadFromString(Model *model, std::string *err, std::string *warn,
                      const char *str, const unsigned int length,
                      const std::string &base_dir, unsigned int check_sections);

  const unsigned char *bin_data_ = nullptr;
  size_t bin_size_ = 0;
  bool is_binary_ = false;

  ParseStrictness strictness_ = ParseStrictness::Strict;

  bool serialize_default_values_ = false;  ///< Serialize default values?

  bool store_original_json_for_extras_and_extensions_ = false;

  bool preserve_image_channels_ = false;  /// Default false(expand channels to
                                          /// RGBA) for backward compatibility.

  size_t max_external_file_size_{
      size_t((std::numeric_limits<int32_t>::max)())};  // Default 2GB

  // Warning & error messages
  std::string warn_;
  std::string err_;

  FsCallbacks fs = {
#ifndef TINYGLTF_NO_FS
      &tinygltf::FileExists,
      &tinygltf::ExpandFilePath,
      &tinygltf::ReadWholeFile,
      &tinygltf::WriteWholeFile,
      &tinygltf::GetFileSizeInBytes,

      nullptr  // Fs callback user data
#else
      nullptr, nullptr, nullptr, nullptr, nullptr,

      nullptr  // Fs callback user data
#endif
  };

  URICallbacks uri_cb = {
      // Use paths as-is by default. This will use JSON string escaping.
      nullptr,
      // Decode all URIs before using them as paths as the application may have
      // percent encoded them.
      &tinygltf::URIDecode,
      // URI callback user data
      nullptr};

  LoadImageDataFunction LoadImageData =
#ifndef TINYGLTF_NO_STB_IMAGE
      &tinygltf::LoadImageData;
#else
      nullptr;
#endif
  void *load_image_user_data_{nullptr};
  bool user_image_loader_{false};

  WriteImageDataFunction WriteImageData =
#ifndef TINYGLTF_NO_STB_IMAGE_WRITE
      &tinygltf::WriteImageData;
#else
      nullptr;
#endif
  void *write_image_user_data_{nullptr};
};

#ifdef __clang__
#pragma clang diagnostic pop  // -Wpadded
#endif

}  // namespace tinygltf

#endif  // TINY_GLTF_H_

#if defined(TINYGLTF_IMPLEMENTATION) || defined(__INTELLISENSE__)
#include <algorithm>
// #include <cassert>
#ifndef TINYGLTF_NO_FS
#include <sys/stat.h>  // for is_directory check

#include <cstdio>
#include <fstream>
#endif
#include <sstream>

#ifdef __clang__
// Disable some warnings for external files.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#if __has_warning("-Wreserved-id-macro")
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wc++98-compat"
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wweak-vtables"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#if __has_warning("-Wdouble-promotion")
#pragma clang diagnostic ignored "-Wdouble-promotion"
#endif
#if __has_warning("-Wcomma")
#pragma clang diagnostic ignored "-Wcomma"
#endif
#if __has_warning("-Wzero-as-null-pointer-constant")
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
#if __has_warning("-Wcast-qual")
#pragma clang diagnostic ignored "-Wcast-qual"
#endif
#if __has_warning("-Wmissing-variable-declarations")
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#endif
#if __has_warning("-Wmissing-prototypes")
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif
#if __has_warning("-Wcast-align")
#pragma clang diagnostic ignored "-Wcast-align"
#endif
#if __has_warning("-Wnewline-eof")
#pragma clang diagnostic ignored "-Wnewline-eof"
#endif
#if __has_warning("-Wunused-parameter")
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#if __has_warning("-Wmismatched-tags")
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif
#if __has_warning("-Wextra-semi-stmt")
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif
#endif

// Disable GCC warnings
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif  // __GNUC__

#ifndef TINYGLTF_NO_INCLUDE_JSON
#ifndef TINYGLTF_USE_RAPIDJSON
#include "json.hpp"
#else
#ifndef TINYGLTF_NO_INCLUDE_RAPIDJSON
#include "document.h"
#include "prettywriter.h"
#include "rapidjson.h"
#include "stringbuffer.h"
#include "writer.h"
#endif
#endif
#endif

#ifdef TINYGLTF_ENABLE_DRACO
#include "draco/compression/decode.h"
#include "draco/core/decoder_buffer.h"
#endif

#ifndef TINYGLTF_NO_STB_IMAGE
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE
#include "stb_image.h"
#endif
#endif

#ifndef TINYGLTF_NO_STB_IMAGE_WRITE
#ifndef TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include "stb_image_write.h"
#endif
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef _WIN32

// issue 143.
// Define NOMINMAX to avoid min/max defines,
// but undef it after included Windows.h
#ifndef NOMINMAX
#define TINYGLTF_INTERNAL_NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define TINYGLTF_INTERNAL_WIN32_LEAN_AND_MEAN
#endif
#ifndef __MINGW32__
#include <Windows.h>  // include API for expanding a file path
#else
#include <windows.h>
#endif

#ifdef TINYGLTF_INTERNAL_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#if defined(TINYGLTF_INTERNAL_NOMINMAX)
#undef NOMINMAX
#endif

#if defined(__GLIBCXX__)  // mingw

#include <fcntl.h>  // _O_RDONLY

#include <ext/stdio_filebuf.h>  // fstream (all sorts of IO stuff) + stdio_filebuf (=streambuf)

#endif

#elif !defined(__ANDROID__) && !defined(__OpenBSD__)
// #include <wordexp.h>
#endif

#if defined(__sparcv9) || defined(__powerpc__)
// Big endian
#else
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || MINIZ_X86_OR_X64_CPU
#define TINYGLTF_LITTLE_ENDIAN 1
#endif
#endif

namespace tinygltf {
namespace detail {
#ifdef TINYGLTF_USE_RAPIDJSON

#ifdef TINYGLTF_USE_RAPIDJSON_CRTALLOCATOR
// This uses the RapidJSON CRTAllocator.  It is thread safe and multiple
// documents may be active at once.
using json =
    rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using json_iterator = json::MemberIterator;
using json_const_iterator = json::ConstMemberIterator;
using json_const_array_iterator = json const *;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
rapidjson::CrtAllocator s_CrtAllocator;  // stateless and thread safe
rapidjson::CrtAllocator &GetAllocator() { return s_CrtAllocator; }
#else
// This uses the default RapidJSON MemoryPoolAllocator.  It is very fast, but
// not thread safe. Only a single JsonDocument may be active at any one time,
// meaning only a single gltf load/save can be active any one time.
using json = rapidjson::Value;
using json_iterator = json::MemberIterator;
using json_const_iterator = json::ConstMemberIterator;
using json_const_array_iterator = json const *;
rapidjson::Document *s_pActiveDocument = nullptr;
rapidjson::Document::AllocatorType &GetAllocator() {
  assert(s_pActiveDocument);  // Root json node must be JsonDocument type
  return s_pActiveDocument->GetAllocator();
}

#ifdef __clang__
#pragma clang diagnostic push
// Suppress JsonDocument(JsonDocument &&rhs) noexcept
#pragma clang diagnostic ignored "-Wunused-member-function"
#endif

struct JsonDocument : public rapidjson::Document {
  JsonDocument() {
    assert(s_pActiveDocument ==
           nullptr);  // When using default allocator, only one document can be
                      // active at a time, if you need multiple active at once,
                      // define TINYGLTF_USE_RAPIDJSON_CRTALLOCATOR
    s_pActiveDocument = this;
  }
  JsonDocument(const JsonDocument &) = delete;
  JsonDocument(JsonDocument &&rhs) noexcept
      : rapidjson::Document(std::move(rhs)) {
    s_pActiveDocument = this;
    rhs.isNil = true;
  }
  ~JsonDocument() {
    if (!isNil) {
      s_pActiveDocument = nullptr;
    }
  }

 private:
  bool isNil = false;
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif  // TINYGLTF_USE_RAPIDJSON_CRTALLOCATOR

#else
using nlohmann::json;
using json_iterator = json::iterator;
using json_const_iterator = json::const_iterator;
using json_const_array_iterator = json_const_iterator;
using JsonDocument = json;
#endif

void JsonParse(JsonDocument &doc, const char *str, size_t length,
               bool throwExc = false) {
#ifdef TINYGLTF_USE_RAPIDJSON
  (void)throwExc;
  doc.Parse(str, length);
#else
  doc = detail::json::parse(str, str + length, nullptr, throwExc);
#endif
}
}  // namespace detail
}  // namespace tinygltf

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat"
#endif

namespace tinygltf {

///
/// Internal LoadImageDataOption struct.
/// This struct is passed through `user_pointer` in LoadImageData.
/// The struct is not passed when the user supply their own LoadImageData
/// callbacks.
///
struct LoadImageDataOption {
  // true: preserve image channels(e.g. load as RGB image if the image has RGB
  // channels) default `false`(channels are expanded to RGBA for backward
  // compatibility).
  bool preserve_channels{false};
};

// Equals function for Value, for recursivity
static bool Equals(const tinygltf::Value &one, const tinygltf::Value &other) {
  if (one.Type() != other.Type()) return false;

  switch (one.Type()) {
    case NULL_TYPE:
      return true;
    case BOOL_TYPE:
      return one.Get<bool>() == other.Get<bool>();
    case REAL_TYPE:
      return TINYGLTF_DOUBLE_EQUAL(one.Get<double>(), other.Get<double>());
    case INT_TYPE:
      return one.Get<int>() == other.Get<int>();
    case OBJECT_TYPE: {
      auto oneObj = one.Get<tinygltf::Value::Object>();
      auto otherObj = other.Get<tinygltf::Value::Object>();
      if (oneObj.size() != otherObj.size()) return false;
      for (auto &it : oneObj) {
        auto otherIt = otherObj.find(it.first);
        if (otherIt == otherObj.end()) return false;

        if (!Equals(it.second, otherIt->second)) return false;
      }
      return true;
    }
    case ARRAY_TYPE: {
      if (one.Size() != other.Size()) return false;
      for (int i = 0; i < int(one.Size()); ++i)
        if (!Equals(one.Get(i), other.Get(i))) return false;
      return true;
    }
    case STRING_TYPE:
      return one.Get<std::string>() == other.Get<std::string>();
    case BINARY_TYPE:
      return one.Get<std::vector<unsigned char> >() ==
             other.Get<std::vector<unsigned char> >();
    default: {
      // unhandled type
      return false;
    }
  }
}

// Equals function for std::vector<double> using TINYGLTF_DOUBLE_EPSILON
static bool Equals(const std::vector<double> &one,
                   const std::vector<double> &other) {
  if (one.size() != other.size()) return false;
  for (int i = 0; i < int(one.size()); ++i) {
    if (!TINYGLTF_DOUBLE_EQUAL(one[size_t(i)], other[size_t(i)])) return false;
  }
  return true;
}

bool Accessor::operator==(const Accessor &other) const {
  return this->bufferView == other.bufferView &&
         this->byteOffset == other.byteOffset &&
         this->componentType == other.componentType &&
         this->count == other.count && this->extensions == other.extensions &&
         this->extras == other.extras &&
         Equals(this->maxValues, other.maxValues) &&
         Equals(this->minValues, other.minValues) && this->name == other.name &&
         this->normalized == other.normalized && this->type == other.type;
}
bool Animation::operator==(const Animation &other) const {
  return this->channels == other.channels &&
         this->extensions == other.extensions && this->extras == other.extras &&
         this->name == other.name && this->samplers == other.samplers;
}
bool AnimationChannel::operator==(const AnimationChannel &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->target_node == other.target_node &&
         this->target_path == other.target_path &&
         this->sampler == other.sampler;
}
bool AnimationSampler::operator==(const AnimationSampler &other) const {
  return this->extras == other.extras && this->extensions == other.extensions &&
         this->input == other.input &&
         this->interpolation == other.interpolation &&
         this->output == other.output;
}
bool Asset::operator==(const Asset &other) const {
  return this->copyright == other.copyright &&
         this->extensions == other.extensions && this->extras == other.extras &&
         this->generator == other.generator &&
         this->minVersion == other.minVersion && this->version == other.version;
}
bool Buffer::operator==(const Buffer &other) const {
  return this->data == other.data && this->extensions == other.extensions &&
         this->extras == other.extras && this->name == other.name &&
         this->uri == other.uri;
}
bool BufferView::operator==(const BufferView &other) const {
  return this->buffer == other.buffer && this->byteLength == other.byteLength &&
         this->byteOffset == other.byteOffset &&
         this->byteStride == other.byteStride && this->name == other.name &&
         this->target == other.target && this->extensions == other.extensions &&
         this->extras == other.extras &&
         this->dracoDecoded == other.dracoDecoded;
}
bool Camera::operator==(const Camera &other) const {
  return this->name == other.name && this->extensions == other.extensions &&
         this->extras == other.extras &&
         this->orthographic == other.orthographic &&
         this->perspective == other.perspective && this->type == other.type;
}
bool Image::operator==(const Image &other) const {
  return this->bufferView == other.bufferView &&
         this->component == other.component &&
         this->extensions == other.extensions && this->extras == other.extras &&
         this->height == other.height && this->image == other.image &&
         this->mimeType == other.mimeType && this->name == other.name &&
         this->uri == other.uri && this->width == other.width;
}
bool Light::operator==(const Light &other) const {
  return Equals(this->color, other.color) && this->name == other.name &&
         this->type == other.type;
}
bool AudioEmitter::operator==(const AudioEmitter &other) const {
  return this->name == other.name &&
         TINYGLTF_DOUBLE_EQUAL(this->gain, other.gain) &&
         this->loop == other.loop && this->playing == other.playing &&
         this->type == other.type &&
         this->distanceModel == other.distanceModel &&
         this->source == other.source;
}
bool AudioSource::operator==(const AudioSource &other) const {
  return this->name == other.name && this->uri == other.uri;
}
bool Material::operator==(const Material &other) const {
  return (this->pbrMetallicRoughness == other.pbrMetallicRoughness) &&
         (this->normalTexture == other.normalTexture) &&
         (this->occlusionTexture == other.occlusionTexture) &&
         (this->emissiveTexture == other.emissiveTexture) &&
         Equals(this->emissiveFactor, other.emissiveFactor) &&
         (this->alphaMode == other.alphaMode) &&
         TINYGLTF_DOUBLE_EQUAL(this->alphaCutoff, other.alphaCutoff) &&
         (this->doubleSided == other.doubleSided) &&
         (this->extensions == other.extensions) &&
         (this->extras == other.extras) && (this->values == other.values) &&
         (this->additionalValues == other.additionalValues) &&
         (this->name == other.name);
}
bool Mesh::operator==(const Mesh &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->name == other.name && Equals(this->weights, other.weights) &&
         this->primitives == other.primitives;
}
bool Model::operator==(const Model &other) const {
  return this->accessors == other.accessors &&
         this->animations == other.animations && this->asset == other.asset &&
         this->buffers == other.buffers &&
         this->bufferViews == other.bufferViews &&
         this->cameras == other.cameras &&
         this->defaultScene == other.defaultScene &&
         this->extensions == other.extensions &&
         this->extensionsRequired == other.extensionsRequired &&
         this->extensionsUsed == other.extensionsUsed &&
         this->extras == other.extras && this->images == other.images &&
         this->lights == other.lights && this->materials == other.materials &&
         this->meshes == other.meshes && this->nodes == other.nodes &&
         this->samplers == other.samplers && this->scenes == other.scenes &&
         this->skins == other.skins && this->textures == other.textures;
}
bool Node::operator==(const Node &other) const {
  return this->camera == other.camera && this->children == other.children &&
         this->extensions == other.extensions && this->extras == other.extras &&
         Equals(this->matrix, other.matrix) && this->mesh == other.mesh &&
         (this->light == other.light) && (this->emitter == other.emitter) &&
         this->name == other.name && Equals(this->rotation, other.rotation) &&
         Equals(this->scale, other.scale) && this->skin == other.skin &&
         Equals(this->translation, other.translation) &&
         Equals(this->weights, other.weights);
}
bool SpotLight::operator==(const SpotLight &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         TINYGLTF_DOUBLE_EQUAL(this->innerConeAngle, other.innerConeAngle) &&
         TINYGLTF_DOUBLE_EQUAL(this->outerConeAngle, other.outerConeAngle);
}
bool PositionalEmitter::operator==(const PositionalEmitter &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         TINYGLTF_DOUBLE_EQUAL(this->coneInnerAngle, other.coneInnerAngle) &&
         TINYGLTF_DOUBLE_EQUAL(this->coneOuterAngle, other.coneOuterAngle) &&
         TINYGLTF_DOUBLE_EQUAL(this->coneOuterGain, other.coneOuterGain) &&
         TINYGLTF_DOUBLE_EQUAL(this->maxDistance, other.maxDistance) &&
         TINYGLTF_DOUBLE_EQUAL(this->refDistance, other.refDistance) &&
         TINYGLTF_DOUBLE_EQUAL(this->rolloffFactor, other.rolloffFactor);
}
bool OrthographicCamera::operator==(const OrthographicCamera &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         TINYGLTF_DOUBLE_EQUAL(this->xmag, other.xmag) &&
         TINYGLTF_DOUBLE_EQUAL(this->ymag, other.ymag) &&
         TINYGLTF_DOUBLE_EQUAL(this->zfar, other.zfar) &&
         TINYGLTF_DOUBLE_EQUAL(this->znear, other.znear);
}
bool Parameter::operator==(const Parameter &other) const {
  if (this->bool_value != other.bool_value ||
      this->has_number_value != other.has_number_value)
    return false;

  if (!TINYGLTF_DOUBLE_EQUAL(this->number_value, other.number_value))
    return false;

  if (this->json_double_value.size() != other.json_double_value.size())
    return false;
  for (auto &it : this->json_double_value) {
    auto otherIt = other.json_double_value.find(it.first);
    if (otherIt == other.json_double_value.end()) return false;

    if (!TINYGLTF_DOUBLE_EQUAL(it.second, otherIt->second)) return false;
  }

  if (!Equals(this->number_array, other.number_array)) return false;

  if (this->string_value != other.string_value) return false;

  return true;
}
bool PerspectiveCamera::operator==(const PerspectiveCamera &other) const {
  return TINYGLTF_DOUBLE_EQUAL(this->aspectRatio, other.aspectRatio) &&
         this->extensions == other.extensions && this->extras == other.extras &&
         TINYGLTF_DOUBLE_EQUAL(this->yfov, other.yfov) &&
         TINYGLTF_DOUBLE_EQUAL(this->zfar, other.zfar) &&
         TINYGLTF_DOUBLE_EQUAL(this->znear, other.znear);
}
bool Primitive::operator==(const Primitive &other) const {
  return this->attributes == other.attributes && this->extras == other.extras &&
         this->indices == other.indices && this->material == other.material &&
         this->mode == other.mode && this->targets == other.targets;
}
bool Sampler::operator==(const Sampler &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->magFilter == other.magFilter &&
         this->minFilter == other.minFilter && this->name == other.name &&
         this->wrapS == other.wrapS && this->wrapT == other.wrapT;

  // this->wrapR == other.wrapR
}
bool Scene::operator==(const Scene &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->name == other.name && this->nodes == other.nodes;
}
bool Skin::operator==(const Skin &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->inverseBindMatrices == other.inverseBindMatrices &&
         this->joints == other.joints && this->name == other.name &&
         this->skeleton == other.skeleton;
}
bool Texture::operator==(const Texture &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->name == other.name && this->sampler == other.sampler &&
         this->source == other.source;
}
bool TextureInfo::operator==(const TextureInfo &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->index == other.index && this->texCoord == other.texCoord;
}
bool NormalTextureInfo::operator==(const NormalTextureInfo &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->index == other.index && this->texCoord == other.texCoord &&
         TINYGLTF_DOUBLE_EQUAL(this->scale, other.scale);
}
bool OcclusionTextureInfo::operator==(const OcclusionTextureInfo &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         this->index == other.index && this->texCoord == other.texCoord &&
         TINYGLTF_DOUBLE_EQUAL(this->strength, other.strength);
}
bool PbrMetallicRoughness::operator==(const PbrMetallicRoughness &other) const {
  return this->extensions == other.extensions && this->extras == other.extras &&
         (this->baseColorTexture == other.baseColorTexture) &&
         (this->metallicRoughnessTexture == other.metallicRoughnessTexture) &&
         Equals(this->baseColorFactor, other.baseColorFactor) &&
         TINYGLTF_DOUBLE_EQUAL(this->metallicFactor, other.metallicFactor) &&
         TINYGLTF_DOUBLE_EQUAL(this->roughnessFactor, other.roughnessFactor);
}
bool Value::operator==(const Value &other) const {
  return Equals(*this, other);
}

static void swap4(unsigned int *val) {
#ifdef TINYGLTF_LITTLE_ENDIAN
  (void)val;
#else
  unsigned int tmp = *val;
  unsigned char *dst = reinterpret_cast<unsigned char *>(val);
  unsigned char *src = reinterpret_cast<unsigned char *>(&tmp);

  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
#endif
}

static std::string JoinPath(const std::string &path0,
                            const std::string &path1) {
  if (path0.empty()) {
    return path1;
  } else {
    // check '/'
    char lastChar = *path0.rbegin();
    if (lastChar != '/') {
      return path0 + std::string("/") + path1;
    } else {
      return path0 + path1;
    }
  }
}

static std::string FindFile(const std::vector<std::string> &paths,
                            const std::string &filepath, FsCallbacks *fs) {
  if (fs == nullptr || fs->ExpandFilePath == nullptr ||
      fs->FileExists == nullptr) {
    // Error, fs callback[s] missing
    return std::string();
  }

  // https://github.com/syoyo/tinygltf/issues/416
  // Use strlen() since std::string's size/length reports the number of elements
  // in the buffer, not the length of string(null-terminated) strip
  // null-character in the middle of string.
  size_t slength = strlen(filepath.c_str());
  if (slength == 0) {
    return std::string();
  }

  std::string cleaned_filepath = std::string(filepath.c_str());

  for (size_t i = 0; i < paths.size(); i++) {
    std::string absPath =
        fs->ExpandFilePath(JoinPath(paths[i], cleaned_filepath), fs->user_data);
    if (fs->FileExists(absPath, fs->user_data)) {
      return absPath;
    }
  }

  return std::string();
}

static std::string GetFilePathExtension(const std::string &FileName) {
  if (FileName.find_last_of(".") != std::string::npos)
    return FileName.substr(FileName.find_last_of(".") + 1);
  return "";
}

static std::string GetBaseDir(const std::string &filepath) {
  if (filepath.find_last_of("/\\") != std::string::npos)
    return filepath.substr(0, filepath.find_last_of("/\\"));
  return "";
}

static std::string GetBaseFilename(const std::string &filepath) {
  auto idx = filepath.find_last_of("/\\");
  if (idx != std::string::npos) return filepath.substr(idx + 1);
  return filepath;
}

std::string base64_encode(unsigned char const *, unsigned int len);
std::string base64_decode(std::string const &s);

/*
   base64.cpp and base64.h

   Copyright (C) 2004-2008 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wconversion"
#endif

static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(unsigned char const *bytes_to_encode,
                          unsigned int in_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  const char *base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] =
          ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] =
          ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++) char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] =
        ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] =
        ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

    for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];

    while ((i++ < 3)) ret += '=';
  }

  return ret;
}

std::string base64_decode(std::string const &encoded_string) {
  int in_len = static_cast<int>(encoded_string.size());
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";

  while (in_len-- && (encoded_string[in_] != '=') &&
         is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_];
    in_++;
    if (i == 4) {
      for (i = 0; i < 4; i++)
        char_array_4[i] =
            static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

      char_array_3[0] =
          (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] =
          ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++) ret += char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++) char_array_4[j] = 0;

    for (j = 0; j < 4; j++)
      char_array_4[j] =
          static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] =
        ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

// https://github.com/syoyo/tinygltf/issues/228
// TODO(syoyo): Use uriparser https://uriparser.github.io/ for stricter Uri
// decoding?
//
// Uri Decoding from DLIB
// http://dlib.net/dlib/server/server_http.cpp.html
// --- dlib begin ------------------------------------------------------------
// Copyright (C) 2003  Davis E. King (davis@dlib.net)
// License: Boost Software License
// Boost Software License - Version 1.0 - August 17th, 2003

// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
namespace dlib {

inline unsigned char from_hex(unsigned char ch) {
  if (ch <= '9' && ch >= '0')
    ch -= '0';
  else if (ch <= 'f' && ch >= 'a')
    ch -= 'a' - 10;
  else if (ch <= 'F' && ch >= 'A')
    ch -= 'A' - 10;
  else
    ch = 0;
  return ch;
}

static const std::string urldecode(const std::string &str) {
  using namespace std;
  string result;
  string::size_type i;
  for (i = 0; i < str.size(); ++i) {
    if (str[i] == '+') {
      result += ' ';
    } else if (str[i] == '%' && str.size() > i + 2) {
      const unsigned char ch1 =
          from_hex(static_cast<unsigned char>(str[i + 1]));
      const unsigned char ch2 =
          from_hex(static_cast<unsigned char>(str[i + 2]));
      const unsigned char ch = static_cast<unsigned char>((ch1 << 4) | ch2);
      result += static_cast<char>(ch);
      i += 2;
    } else {
      result += str[i];
    }
  }
  return result;
}

}  // namespace dlib
// --- dlib end --------------------------------------------------------------

bool URIDecode(const std::string &in_uri, std::string *out_uri,
               void *user_data) {
  (void)user_data;
  *out_uri = dlib::urldecode(in_uri);
  return true;
}

static bool LoadExternalFile(std::vector<unsigned char> *out, std::string *err,
                             std::string *warn, const std::string &filename,
                             const std::string &basedir, bool required,
                             size_t reqBytes, bool checkSize,
                             size_t maxFileSize, FsCallbacks *fs) {
  if (fs == nullptr || fs->FileExists == nullptr ||
      fs->ExpandFilePath == nullptr || fs->ReadWholeFile == nullptr) {
    // This is a developer error, assert() ?
    if (err) {
      (*err) += "FS callback[s] not set\n";
    }
    return false;
  }

  std::string *failMsgOut = required ? err : warn;

  out->clear();

  std::vector<std::string> paths;
  paths.push_back(basedir);
  paths.push_back(".");

  std::string filepath = FindFile(paths, filename, fs);
  if (filepath.empty() || filename.empty()) {
    if (failMsgOut) {
      (*failMsgOut) += "File not found : " + filename + "\n";
    }
    return false;
  }

  // Check file size
  if (fs->GetFileSizeInBytes) {
    size_t file_size{0};
    std::string _err;
    bool ok =
        fs->GetFileSizeInBytes(&file_size, &_err, filepath, fs->user_data);
    if (!ok) {
      if (_err.size()) {
        if (failMsgOut) {
          (*failMsgOut) += "Getting file size failed : " + filename +
                           ", err = " + _err + "\n";
        }
      }
      return false;
    }

    if (file_size > maxFileSize) {
      if (failMsgOut) {
        (*failMsgOut) += "File size " + std::to_string(file_size) +
                         " exceeds maximum allowed file size " +
                         std::to_string(maxFileSize) + " : " + filepath + "\n";
      }
      return false;
    }
  }

  std::vector<unsigned char> buf;
  std::string fileReadErr;
  bool fileRead =
      fs->ReadWholeFile(&buf, &fileReadErr, filepath, fs->user_data);
  if (!fileRead) {
    if (failMsgOut) {
      (*failMsgOut) +=
          "File read error : " + filepath + " : " + fileReadErr + "\n";
    }
    return false;
  }

  size_t sz = buf.size();
  if (sz == 0) {
    if (failMsgOut) {
      (*failMsgOut) += "File is empty : " + filepath + "\n";
    }
    return false;
  }

  if (checkSize) {
    if (reqBytes == sz) {
      out->swap(buf);
      return true;
    } else {
      std::stringstream ss;
      ss << "File size mismatch : " << filepath << ", requestedBytes "
         << reqBytes << ", but got " << sz << std::endl;
      if (failMsgOut) {
        (*failMsgOut) += ss.str();
      }
      return false;
    }
  }

  out->swap(buf);
  return true;
}

void TinyGLTF::SetParseStrictness(ParseStrictness strictness) {
  strictness_ = strictness;
}

void TinyGLTF::SetImageLoader(LoadImageDataFunction func, void *user_data) {
  LoadImageData = func;
  load_image_user_data_ = user_data;
  user_image_loader_ = true;
}

void TinyGLTF::RemoveImageLoader() {
  LoadImageData =
#ifndef TINYGLTF_NO_STB_IMAGE
      &tinygltf::LoadImageData;
#else
      nullptr;
#endif

  load_image_user_data_ = nullptr;
  user_image_loader_ = false;
}

#ifndef TINYGLTF_NO_STB_IMAGE
bool LoadImageData(Image *image, const int image_idx, std::string *err,
                   std::string *warn, int req_width, int req_height,
                   const unsigned char *bytes, int size, void *user_data) {
  (void)warn;

  LoadImageDataOption option;
  if (user_data) {
    option = *reinterpret_cast<LoadImageDataOption *>(user_data);
  }

  int w = 0, h = 0, comp = 0, req_comp = 0;

  unsigned char *data = nullptr;

  // preserve_channels true: Use channels stored in the image file.
  // false: force 32-bit textures for common Vulkan compatibility. It appears
  // that some GPU drivers do not support 24-bit images for Vulkan
  req_comp = option.preserve_channels ? 0 : 4;
  int bits = 8;
  int pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;

  // It is possible that the image we want to load is a 16bit per channel image
  // We are going to attempt to load it as 16bit per channel, and if it worked,
  // set the image data accordingly. We are casting the returned pointer into
  // unsigned char, because we are representing "bytes". But we are updating
  // the Image metadata to signal that this image uses 2 bytes (16bits) per
  // channel:
  if (stbi_is_16_bit_from_memory(bytes, size)) {
    data = reinterpret_cast<unsigned char *>(
        stbi_load_16_from_memory(bytes, size, &w, &h, &comp, req_comp));
    if (data) {
      bits = 16;
      pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    }
  }

  // at this point, if data is still NULL, it means that the image wasn't
  // 16bit per channel, we are going to load it as a normal 8bit per channel
  // image as we used to do:
  // if image cannot be decoded, ignore parsing and keep it by its path
  // don't break in this case
  // FIXME we should only enter this function if the image is embedded. If
  // image->uri references
  // an image file, it should be left as it is. Image loading should not be
  // mandatory (to support other formats)
  if (!data) data = stbi_load_from_memory(bytes, size, &w, &h, &comp, req_comp);
  if (!data) {
    // NOTE: you can use `warn` instead of `err`
    if (err) {
      (*err) +=
          "Unknown image format. STB cannot decode image data for image[" +
          std::to_string(image_idx) + "] name = \"" + image->name + "\".\n";
    }
    return false;
  }

  if ((w < 1) || (h < 1)) {
    stbi_image_free(data);
    if (err) {
      (*err) += "Invalid image data for image[" + std::to_string(image_idx) +
                "] name = \"" + image->name + "\"\n";
    }
    return false;
  }

  if (req_width > 0) {
    if (req_width != w) {
      stbi_image_free(data);
      if (err) {
        (*err) += "Image width mismatch for image[" +
                  std::to_string(image_idx) + "] name = \"" + image->name +
                  "\"\n";
      }
      return false;
    }
  }

  if (req_height > 0) {
    if (req_height != h) {
      stbi_image_free(data);
      if (err) {
        (*err) += "Image height mismatch. for image[" +
                  std::to_string(image_idx) + "] name = \"" + image->name +
                  "\"\n";
      }
      return false;
    }
  }

  if (req_comp != 0) {
    // loaded data has `req_comp` channels(components)
    comp = req_comp;
  }

  image->width = w;
  image->height = h;
  image->component = comp;
  image->bits = bits;
  image->pixel_type = pixel_type;
  image->image.resize(static_cast<size_t>(w * h * comp) * size_t(bits / 8));
  std::copy(data, data + w * h * comp * (bits / 8), image->image.begin());
  stbi_image_free(data);

  return true;
}
#endif

void TinyGLTF::SetImageWriter(WriteImageDataFunction func, void *user_data) {
  WriteImageData = func;
  write_image_user_data_ = user_data;
}

#ifndef TINYGLTF_NO_STB_IMAGE_WRITE
static void WriteToMemory_stbi(void *context, void *data, int size) {
  std::vector<unsigned char> *buffer =
      reinterpret_cast<std::vector<unsigned char> *>(context);

  unsigned char *pData = reinterpret_cast<unsigned char *>(data);

  buffer->insert(buffer->end(), pData, pData + size);
}

bool WriteImageData(const std::string *basepath, const std::string *filename,
                    const Image *image, bool embedImages,
                    const URICallbacks *uri_cb, std::string *out_uri,
                    void *fsPtr) {
  const std::string ext = GetFilePathExtension(*filename);

  // Write image to temporary buffer
  std::string header;
  std::vector<unsigned char> data;

  if (ext == "png") {
    if ((image->bits != 8) ||
        (image->pixel_type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)) {
      // Unsupported pixel format
      return false;
    }

    if (!stbi_write_png_to_func(WriteToMemory_stbi, &data, image->width,
                                image->height, image->component,
                                &image->image[0], 0)) {
      return false;
    }
    header = "data:image/png;base64,";
  } else if (ext == "jpg") {
    if (!stbi_write_jpg_to_func(WriteToMemory_stbi, &data, image->width,
                                image->height, image->component,
                                &image->image[0], 100)) {
      return false;
    }
    header = "data:image/jpeg;base64,";
  } else if (ext == "bmp") {
    if (!stbi_write_bmp_to_func(WriteToMemory_stbi, &data, image->width,
                                image->height, image->component,
                                &image->image[0])) {
      return false;
    }
    header = "data:image/bmp;base64,";
  } else if (!embedImages) {
    // Error: can't output requested format to file
    return false;
  }

  if (embedImages) {
    // Embed base64-encoded image into URI
    if (data.size()) {
      *out_uri = header + base64_encode(&data[0],
                                        static_cast<unsigned int>(data.size()));
    } else {
      // Throw error?
    }
  } else {
    // Write image to disc
    FsCallbacks *fs = reinterpret_cast<FsCallbacks *>(fsPtr);
    if ((fs != nullptr) && (fs->WriteWholeFile != nullptr)) {
      const std::string imagefilepath = JoinPath(*basepath, *filename);
      std::string writeError;
      if (!fs->WriteWholeFile(&writeError, imagefilepath, data,
                              fs->user_data)) {
        // Could not write image file to disc; Throw error ?
        return false;
      }
    } else {
      // Throw error?
    }
    if (uri_cb->encode) {
      if (!uri_cb->encode(*filename, "image", out_uri, uri_cb->user_data)) {
        return false;
      }
    } else {
      *out_uri = *filename;
    }
  }

  return true;
}
#endif

void TinyGLTF::SetURICallbacks(URICallbacks callbacks) {
  assert(callbacks.decode);
  if (callbacks.decode) {
    uri_cb = callbacks;
  }
}

void TinyGLTF::SetFsCallbacks(FsCallbacks callbacks) { fs = callbacks; }

#ifdef _WIN32
static inline std::wstring UTF8ToWchar(const std::string &str) {
  int wstr_size =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
  std::wstring wstr((size_t)wstr_size, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0],
                      (int)wstr.size());
  return wstr;
}

static inline std::string WcharToUTF8(const std::wstring &wstr) {
  int str_size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(),
                                     nullptr, 0, nullptr, nullptr);
  std::string str((size_t)str_size, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0],
                      (int)str.size(), nullptr, nullptr);
  return str;
}
#endif

#ifndef TINYGLTF_NO_FS
// Default implementations of filesystem functions

bool FileExists(const std::string &abs_filename, void *) {
  bool ret;
#ifdef TINYGLTF_ANDROID_LOAD_FROM_ASSETS
  if (asset_manager) {
    AAsset *asset = AAssetManager_open(asset_manager, abs_filename.c_str(),
                                       AASSET_MODE_STREAMING);
    if (!asset) {
      return false;
    }
    AAsset_close(asset);
    ret = true;
  } else {
    return false;
  }
#else
#ifdef _WIN32
#if defined(_MSC_VER) || defined(_LIBCPP_VERSION)

  // First check if a file is a directory.
  DWORD result = GetFileAttributesW(UTF8ToWchar(abs_filename).c_str());
  if (result == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if (result & FILE_ATTRIBUTE_DIRECTORY) {
    return false;
  }

  FILE *fp = nullptr;
  errno_t err = _wfopen_s(&fp, UTF8ToWchar(abs_filename).c_str(), L"rb");
  if (err != 0) {
    return false;
  }
#elif defined(__GLIBCXX__)
  FILE *fp = fopen(abs_filename.c_str(), "rb");
  if (!fp) {
    return false;
  }
#else
  // TODO: is_directory check
  FILE *fp = nullptr;
  errno_t err = fopen_s(&fp, abs_filename.c_str(), "rb");
  if (err != 0) {
    return false;
  }
#endif

#else
  struct stat sb;
  if (stat(abs_filename.c_str(), &sb)) {
    return false;
  }
  if (S_ISDIR(sb.st_mode)) {
    return false;
  }

  FILE *fp = fopen(abs_filename.c_str(), "rb");
#endif
  if (fp) {
    ret = true;
    fclose(fp);
  } else {
    ret = false;
  }
#endif

  return ret;
}

std::string ExpandFilePath(const std::string &filepath, void *) {
  // https://github.com/syoyo/tinygltf/issues/368
  //
  // No file path expansion in built-in FS function anymore, since glTF URI
  // should not contain tilde('~') and environment variables, and for security
  // reason(`wordexp`).
  //
  // Users need to supply `base_dir`(in `LoadASCIIFromString`,
  // `LoadBinaryFromMemory`) in expanded absolute path.

  return filepath;

#if 0
#ifdef _WIN32
  // Assume input `filepath` is encoded in UTF-8
  std::wstring wfilepath = UTF8ToWchar(filepath);
  DWORD wlen = ExpandEnvironmentStringsW(wfilepath.c_str(), nullptr, 0);
  wchar_t *wstr = new wchar_t[wlen];
  ExpandEnvironmentStringsW(wfilepath.c_str(), wstr, wlen);

  std::wstring ws(wstr);
  delete[] wstr;
  return WcharToUTF8(ws);

#else

#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR) || \
    defined(__ANDROID__) || defined(__EMSCRIPTEN__) || defined(__OpenBSD__)
  // no expansion
  std::string s = filepath;
#else
  std::string s;
  wordexp_t p;

  if (filepath.empty()) {
    return "";
  }

  // Quote the string to keep any spaces in filepath intact.
  std::string quoted_path = "\"" + filepath + "\"";
  // char** w;
  int ret = wordexp(quoted_path.c_str(), &p, 0);
  if (ret) {
    // err
    s = filepath;
    return s;
  }

  // Use first element only.
  if (p.we_wordv) {
    s = std::string(p.we_wordv[0]);
    wordfree(&p);
  } else {
    s = filepath;
  }

#endif

  return s;
#endif
#endif
}

bool GetFileSizeInBytes(size_t *filesize_out, std::string *err,
                        const std::string &filepath, void *userdata) {
  (void)userdata;

#ifdef TINYGLTF_ANDROID_LOAD_FROM_ASSETS
  if (asset_manager) {
    AAsset *asset = AAssetManager_open(asset_manager, filepath.c_str(),
                                       AASSET_MODE_STREAMING);
    if (!asset) {
      if (err) {
        (*err) += "File open error : " + filepath + "\n";
      }
      return false;
    }
    size_t size = AAsset_getLength(asset);

    if (size == 0) {
      if (err) {
        (*err) += "Invalid file size : " + filepath +
                  " (does the path point to a directory?)";
      }
      return false;
    }

    return true;
  } else {
    if (err) {
      (*err) += "No asset manager specified : " + filepath + "\n";
    }
    return false;
  }
#else
#ifdef _WIN32
#if defined(__GLIBCXX__)  // mingw
  int file_descriptor =
      _wopen(UTF8ToWchar(filepath).c_str(), _O_RDONLY | _O_BINARY);
  __gnu_cxx::stdio_filebuf<char> wfile_buf(file_descriptor, std::ios_base::in);
  std::istream f(&wfile_buf);
#elif defined(_MSC_VER) || defined(_LIBCPP_VERSION)
  // For libcxx, assume _LIBCPP_HAS_OPEN_WITH_WCHAR is defined to accept
  // `wchar_t *`
  std::ifstream f(UTF8ToWchar(filepath).c_str(), std::ifstream::binary);
#else
  // Unknown compiler/runtime
  std::ifstream f(filepath.c_str(), std::ifstream::binary);
#endif
#else
  std::ifstream f(filepath.c_str(), std::ifstream::binary);
#endif
  if (!f) {
    if (err) {
      (*err) += "File open error : " + filepath + "\n";
    }
    return false;
  }

  // For directory(and pipe?), peek() will fail(Posix gnustl/libc++ only)
  f.peek();
  if (!f) {
    if (err) {
      (*err) +=
          "File read error. Maybe empty file or invalid file : " + filepath +
          "\n";
    }
    return false;
  }

  f.seekg(0, f.end);
  size_t sz = static_cast<size_t>(f.tellg());

  // std::cout << "sz = " << sz << "\n";
  f.seekg(0, f.beg);

  if (int64_t(sz) < 0) {
    if (err) {
      (*err) += "Invalid file size : " + filepath +
                " (does the path point to a directory?)";
    }
    return false;
  } else if (sz == 0) {
    if (err) {
      (*err) += "File is empty : " + filepath + "\n";
    }
    return false;
  } else if (sz >= (std::numeric_limits<std::streamoff>::max)()) {
    if (err) {
      (*err) += "Invalid file size : " + filepath + "\n";
    }
    return false;
  }

  (*filesize_out) = sz;
  return true;
#endif
}

bool ReadWholeFile(std::vector<unsigned char> *out, std::string *err,
                   const std::string &filepath, void *) {
#ifdef TINYGLTF_ANDROID_LOAD_FROM_ASSETS
  if (asset_manager) {
    AAsset *asset = AAssetManager_open(asset_manager, filepath.c_str(),
                                       AASSET_MODE_STREAMING);
    if (!asset) {
      if (err) {
        (*err) += "File open error : " + filepath + "\n";
      }
      return false;
    }
    size_t size = AAsset_getLength(asset);
    if (size == 0) {
      if (err) {
        (*err) += "Invalid file size : " + filepath +
                  " (does the path point to a directory?)";
      }
      return false;
    }
    out->resize(size);
    AAsset_read(asset, reinterpret_cast<char *>(&out->at(0)), size);
    AAsset_close(asset);
    return true;
  } else {
    if (err) {
      (*err) += "No asset manager specified : " + filepath + "\n";
    }
    return false;
  }
#else
#ifdef _WIN32
#if defined(__GLIBCXX__)  // mingw
  int file_descriptor =
      _wopen(UTF8ToWchar(filepath).c_str(), _O_RDONLY | _O_BINARY);
  __gnu_cxx::stdio_filebuf<char> wfile_buf(file_descriptor, std::ios_base::in);
  std::istream f(&wfile_buf);
#elif defined(_MSC_VER) || defined(_LIBCPP_VERSION)
  // For libcxx, assume _LIBCPP_HAS_OPEN_WITH_WCHAR is defined to accept
  // `wchar_t *`
  std::ifstream f(UTF8ToWchar(filepath).c_str(), std::ifstream::binary);
#else
  // Unknown compiler/runtime
  std::ifstream f(filepath.c_str(), std::ifstream::binary);
#endif
#else
  std::ifstream f(filepath.c_str(), std::ifstream::binary);
#endif
  if (!f) {
    if (err) {
      (*err) += "File open error : " + filepath + "\n";
    }
    return false;
  }

  // For directory(and pipe?), peek() will fail(Posix gnustl/libc++ only)
  f.peek();
  if (!f) {
    if (err) {
      (*err) +=
          "File read error. Maybe empty file or invalid file : " + filepath +
          "\n";
    }
    return false;
  }

  f.seekg(0, f.end);
  size_t sz = static_cast<size_t>(f.tellg());

  // std::cout << "sz = " << sz << "\n";
  f.seekg(0, f.beg);

  if (int64_t(sz) < 0) {
    if (err) {
      (*err) += "Invalid file size : " + filepath +
                " (does the path point to a directory?)";
    }
    return false;
  } else if (sz == 0) {
    if (err) {
      (*err) += "File is empty : " + filepath + "\n";
    }
    return false;
  } else if (sz >= (std::numeric_limits<std::streamoff>::max)()) {
    if (err) {
      (*err) += "Invalid file size : " + filepath + "\n";
    }
    return false;
  }

  out->resize(sz);
  f.read(reinterpret_cast<char *>(&out->at(0)),
         static_cast<std::streamsize>(sz));

  return true;
#endif
}

bool WriteWholeFile(std::string *err, const std::string &filepath,
                    const std::vector<unsigned char> &contents, void *) {
#ifdef _WIN32
#if defined(__GLIBCXX__)  // mingw
  int file_descriptor = _wopen(UTF8ToWchar(filepath).c_str(),
                               _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY);
  __gnu_cxx::stdio_filebuf<char> wfile_buf(
      file_descriptor, std::ios_base::out | std::ios_base::binary);
  std::ostream f(&wfile_buf);
#elif defined(_MSC_VER)
  std::ofstream f(UTF8ToWchar(filepath).c_str(), std::ofstream::binary);
#else  // clang?
  std::ofstream f(filepath.c_str(), std::ofstream::binary);
#endif
#else
  std::ofstream f(filepath.c_str(), std::ofstream::binary);
#endif
  if (!f) {
    if (err) {
      (*err) += "File open error for writing : " + filepath + "\n";
    }
    return false;
  }

  f.write(reinterpret_cast<const char *>(&contents.at(0)),
          static_cast<std::streamsize>(contents.size()));
  if (!f) {
    if (err) {
      (*err) += "File write error: " + filepath + "\n";
    }
    return false;
  }

  return true;
}

#endif  // TINYGLTF_NO_FS

static std::string MimeToExt(const std::string &mimeType) {
  if (mimeType == "image/jpeg") {
    return "jpg";
  } else if (mimeType == "image/png") {
    return "png";
  } else if (mimeType == "image/bmp") {
    return "bmp";
  } else if (mimeType == "image/gif") {
    return "gif";
  }

  return "";
}

static bool UpdateImageObject(const Image &image, std::string &baseDir,
                              int index, bool embedImages,
                              const URICallbacks *uri_cb,
                              WriteImageDataFunction *WriteImageData,
                              void *user_data, std::string *out_uri) {
  std::string filename;
  std::string ext;
  // If image has uri, use it as a filename
  if (image.uri.size()) {
    std::string decoded_uri;
    if (!uri_cb->decode(image.uri, &decoded_uri, uri_cb->user_data)) {
      // A decode failure results in a failure to write the gltf.
      return false;
    }
    filename = GetBaseFilename(decoded_uri);
    ext = GetFilePathExtension(filename);
  } else if (image.bufferView != -1) {
    // If there's no URI and the data exists in a buffer,
    // don't change properties or write images
  } else if (image.name.size()) {
    ext = MimeToExt(image.mimeType);
    // Otherwise use name as filename
    filename = image.name + "." + ext;
  } else {
    ext = MimeToExt(image.mimeType);
    // Fallback to index of image as filename
    filename = std::to_string(index) + "." + ext;
  }

  // If callback is set and image data exists, modify image data object. If
  // image data does not exist, this is not considered a failure and the
  // original uri should be maintained.
  bool imageWritten = false;
  if (*WriteImageData != nullptr && !filename.empty() && !image.image.empty()) {
    imageWritten = (*WriteImageData)(&baseDir, &filename, &image, embedImages,
                                     uri_cb, out_uri, user_data);
    if (!imageWritten) {
      return false;
    }
  }

  // Use the original uri if the image was not written.
  if (!imageWritten) {
    *out_uri = image.uri;
  }

  return true;
}

bool IsDataURI(const std::string &in) {
  std::string header = "data:application/octet-stream;base64,";
  if (in.find(header) == 0) {
    return true;
  }

  header = "data:image/jpeg;base64,";
  if (in.find(header) == 0) {
    return true;
  }

  header = "data:image/png;base64,";
  if (in.find(header) == 0) {
    return true;
  }

  header = "data:image/bmp;base64,";
  if (in.find(header) == 0) {
    return true;
  }

  header = "data:image/gif;base64,";
  if (in.find(header) == 0) {
    return true;
  }

  header = "data:text/plain;base64,";
  if (in.find(header) == 0) {
    return true;
  }

  header = "data:application/gltf-buffer;base64,";
  if (in.find(header) == 0) {
    return true;
  }

  return false;
}

bool DecodeDataURI(std::vector<unsigned char> *out, std::string &mime_type,
                   const std::string &in, size_t reqBytes, bool checkSize) {
  std::string header = "data:application/octet-stream;base64,";
  std::string data;
  if (in.find(header) == 0) {
    data = base64_decode(in.substr(header.size()));  // cut mime string.
  }

  if (data.empty()) {
    header = "data:image/jpeg;base64,";
    if (in.find(header) == 0) {
      mime_type = "image/jpeg";
      data = base64_decode(in.substr(header.size()));  // cut mime string.
    }
  }

  if (data.empty()) {
    header = "data:image/png;base64,";
    if (in.find(header) == 0) {
      mime_type = "image/png";
      data = base64_decode(in.substr(header.size()));  // cut mime string.
    }
  }

  if (data.empty()) {
    header = "data:image/bmp;base64,";
    if (in.find(header) == 0) {
      mime_type = "image/bmp";
      data = base64_decode(in.substr(header.size()));  // cut mime string.
    }
  }

  if (data.empty()) {
    header = "data:image/gif;base64,";
    if (in.find(header) == 0) {
      mime_type = "image/gif";
      data = base64_decode(in.substr(header.size()));  // cut mime string.
    }
  }

  if (data.empty()) {
    header = "data:text/plain;base64,";
    if (in.find(header) == 0) {
      mime_type = "text/plain";
      data = base64_decode(in.substr(header.size()));
    }
  }

  if (data.empty()) {
    header = "data:application/gltf-buffer;base64,";
    if (in.find(header) == 0) {
      data = base64_decode(in.substr(header.size()));
    }
  }

  // TODO(syoyo): Allow empty buffer? #229
  if (data.empty()) {
    return false;
  }

  if (checkSize) {
    if (data.size() != reqBytes) {
      return false;
    }
    out->resize(reqBytes);
  } else {
    out->resize(data.size());
  }
  std::copy(data.begin(), data.end(), out->begin());
  return true;
}

namespace detail {
bool GetInt(const detail::json &o, int &val) {
#ifdef TINYGLTF_USE_RAPIDJSON
  if (!o.IsDouble()) {
    if (o.IsInt()) {
      val = o.GetInt();
      return true;
    } else if (o.IsUint()) {
      val = static_cast<int>(o.GetUint());
      return true;
    } else if (o.IsInt64()) {
      val = static_cast<int>(o.GetInt64());
      return true;
    } else if (o.IsUint64()) {
      val = static_cast<int>(o.GetUint64());
      return true;
    }
  }

  return false;
#else
  auto type = o.type();

  if ((type == detail::json::value_t::number_integer) ||
      (type == detail::json::value_t::number_unsigned)) {
    val = static_cast<int>(o.get<int64_t>());
    return true;
  }

  return false;
#endif
}

#ifdef TINYGLTF_USE_RAPIDJSON
bool GetDouble(const detail::json &o, double &val) {
  if (o.IsDouble()) {
    val = o.GetDouble();
    return true;
  }

  return false;
}
#endif

bool GetNumber(const detail::json &o, double &val) {
#ifdef TINYGLTF_USE_RAPIDJSON
  if (o.IsNumber()) {
    val = o.GetDouble();
    return true;
  }

  return false;
#else
  if (o.is_number()) {
    val = o.get<double>();
    return true;
  }

  return false;
#endif
}

bool GetString(const detail::json &o, std::string &val) {
#ifdef TINYGLTF_USE_RAPIDJSON
  if (o.IsString()) {
    val = o.GetString();
    return true;
  }

  return false;
#else
  if (o.type() == detail::json::value_t::string) {
    val = o.get<std::string>();
    return true;
  }

  return false;
#endif
}

bool IsArray(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.IsArray();
#else
  return o.is_array();
#endif
}

detail::json_const_array_iterator ArrayBegin(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.Begin();
#else
  return o.begin();
#endif
}

detail::json_const_array_iterator ArrayEnd(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.End();
#else
  return o.end();
#endif
}

bool IsObject(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.IsObject();
#else
  return o.is_object();
#endif
}

detail::json_const_iterator ObjectBegin(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.MemberBegin();
#else
  return o.begin();
#endif
}

detail::json_const_iterator ObjectEnd(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.MemberEnd();
#else
  return o.end();
#endif
}

// Making this a const char* results in a pointer to a temporary when
// TINYGLTF_USE_RAPIDJSON is off.
std::string GetKey(detail::json_const_iterator &it) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return it->name.GetString();
#else
  return it.key().c_str();
#endif
}

bool FindMember(const detail::json &o, const char *member,
                detail::json_const_iterator &it) {
#ifdef TINYGLTF_USE_RAPIDJSON
  if (!o.IsObject()) {
    return false;
  }
  it = o.FindMember(member);
  return it != o.MemberEnd();
#else
  it = o.find(member);
  return it != o.end();
#endif
}

bool FindMember(detail::json &o, const char *member,
                detail::json_iterator &it) {
#ifdef TINYGLTF_USE_RAPIDJSON
  if (!o.IsObject()) {
    return false;
  }
  it = o.FindMember(member);
  return it != o.MemberEnd();
#else
  it = o.find(member);
  return it != o.end();
#endif
}

void Erase(detail::json &o, detail::json_iterator &it) {
#ifdef TINYGLTF_USE_RAPIDJSON
  o.EraseMember(it);
#else
  o.erase(it);
#endif
}

bool IsEmpty(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.ObjectEmpty();
#else
  return o.empty();
#endif
}

const detail::json &GetValue(detail::json_const_iterator &it) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return it->value;
#else
  return it.value();
#endif
}

detail::json &GetValue(detail::json_iterator &it) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return it->value;
#else
  return it.value();
#endif
}

std::string JsonToString(const detail::json &o, int spacing = -1) {
#ifdef TINYGLTF_USE_RAPIDJSON
  using namespace rapidjson;
  StringBuffer buffer;
  if (spacing == -1) {
    Writer<StringBuffer> writer(buffer);
    // TODO: Better error handling.
    // https://github.com/syoyo/tinygltf/issues/332
    if (!o.Accept(writer)) {
      return "tiny_gltf::JsonToString() failed rapidjson conversion";
    }
  } else {
    PrettyWriter<StringBuffer> writer(buffer);
    writer.SetIndent(' ', uint32_t(spacing));
    if (!o.Accept(writer)) {
      return "tiny_gltf::JsonToString() failed rapidjson conversion";
    }
  }
  return buffer.GetString();
#else
  return o.dump(spacing);
#endif
}

}  // namespace detail

static bool ParseJsonAsValue(Value *ret, const detail::json &o) {
  Value val{};
#ifdef TINYGLTF_USE_RAPIDJSON
  using rapidjson::Type;
  switch (o.GetType()) {
    case Type::kObjectType: {
      Value::Object value_object;
      for (auto it = o.MemberBegin(); it != o.MemberEnd(); ++it) {
        Value entry;
        ParseJsonAsValue(&entry, it->value);
        if (entry.Type() != NULL_TYPE)
          value_object.emplace(detail::GetKey(it), std::move(entry));
      }
      if (value_object.size() > 0) val = Value(std::move(value_object));
    } break;
    case Type::kArrayType: {
      Value::Array value_array;
      value_array.reserve(o.Size());
      for (auto it = o.Begin(); it != o.End(); ++it) {
        Value entry;
        ParseJsonAsValue(&entry, *it);
        if (entry.Type() != NULL_TYPE)
          value_array.emplace_back(std::move(entry));
      }
      if (value_array.size() > 0) val = Value(std::move(value_array));
    } break;
    case Type::kStringType:
      val = Value(std::string(o.GetString()));
      break;
    case Type::kFalseType:
    case Type::kTrueType:
      val = Value(o.GetBool());
      break;
    case Type::kNumberType:
      if (!o.IsDouble()) {
        int i = 0;
        detail::GetInt(o, i);
        val = Value(i);
      } else {
        double d = 0.0;
        detail::GetDouble(o, d);
        val = Value(d);
      }
      break;
    case Type::kNullType:
      break;
      // all types are covered, so no `case default`
  }
#else
  switch (o.type()) {
    case detail::json::value_t::object: {
      Value::Object value_object;
      for (auto it = o.begin(); it != o.end(); it++) {
        Value entry;
        ParseJsonAsValue(&entry, it.value());
        if (entry.Type() != NULL_TYPE)
          value_object.emplace(it.key(), std::move(entry));
      }
      if (value_object.size() > 0) val = Value(std::move(value_object));
    } break;
    case detail::json::value_t::array: {
      Value::Array value_array;
      value_array.reserve(o.size());
      for (auto it = o.begin(); it != o.end(); it++) {
        Value entry;
        ParseJsonAsValue(&entry, it.value());
        if (entry.Type() != NULL_TYPE)
          value_array.emplace_back(std::move(entry));
      }
      if (value_array.size() > 0) val = Value(std::move(value_array));
    } break;
    case detail::json::value_t::string:
      val = Value(o.get<std::string>());
      break;
    case detail::json::value_t::boolean:
      val = Value(o.get<bool>());
      break;
    case detail::json::value_t::number_integer:
    case detail::json::value_t::number_unsigned:
      val = Value(static_cast<int>(o.get<int64_t>()));
      break;
    case detail::json::value_t::number_float:
      val = Value(o.get<double>());
      break;
    case detail::json::value_t::null:
    case detail::json::value_t::discarded:
    case detail::json::value_t::binary:
      // default:
      break;
  }
#endif
  const bool isNotNull = val.Type() != NULL_TYPE;

  if (ret) *ret = std::move(val);

  return isNotNull;
}

static bool ParseExtrasProperty(Value *ret, const detail::json &o) {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, "extras", it)) {
    return false;
  }

  return ParseJsonAsValue(ret, detail::GetValue(it));
}

static bool ParseBooleanProperty(bool *ret, std::string *err,
                                 const detail::json &o,
                                 const std::string &property,
                                 const bool required,
                                 const std::string &parent_node = "") {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  auto &value = detail::GetValue(it);

  bool isBoolean;
  bool boolValue = false;
#ifdef TINYGLTF_USE_RAPIDJSON
  isBoolean = value.IsBool();
  if (isBoolean) {
    boolValue = value.GetBool();
  }
#else
  isBoolean = value.is_boolean();
  if (isBoolean) {
    boolValue = value.get<bool>();
  }
#endif
  if (!isBoolean) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not a bool type.\n";
      }
    }
    return false;
  }

  if (ret) {
    (*ret) = boolValue;
  }

  return true;
}

static bool ParseIntegerProperty(int *ret, std::string *err,
                                 const detail::json &o,
                                 const std::string &property,
                                 const bool required,
                                 const std::string &parent_node = "") {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  int intValue;
  bool isInt = detail::GetInt(detail::GetValue(it), intValue);
  if (!isInt) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not an integer type.\n";
      }
    }
    return false;
  }

  if (ret) {
    (*ret) = intValue;
  }

  return true;
}

static bool ParseUnsignedProperty(size_t *ret, std::string *err,
                                  const detail::json &o,
                                  const std::string &property,
                                  const bool required,
                                  const std::string &parent_node = "") {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  auto &value = detail::GetValue(it);

  size_t uValue = 0;
  bool isUValue;
#ifdef TINYGLTF_USE_RAPIDJSON
  isUValue = false;
  if (value.IsUint()) {
    uValue = value.GetUint();
    isUValue = true;
  } else if (value.IsUint64()) {
    uValue = value.GetUint64();
    isUValue = true;
  }
#else
  isUValue = value.is_number_unsigned();
  if (isUValue) {
    uValue = value.get<size_t>();
  }
#endif
  if (!isUValue) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not a positive integer.\n";
      }
    }
    return false;
  }

  if (ret) {
    (*ret) = uValue;
  }

  return true;
}

static bool ParseNumberProperty(double *ret, std::string *err,
                                const detail::json &o,
                                const std::string &property,
                                const bool required,
                                const std::string &parent_node = "") {
  detail::json_const_iterator it;

  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  double numberValue;
  bool isNumber = detail::GetNumber(detail::GetValue(it), numberValue);

  if (!isNumber) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not a number type.\n";
      }
    }
    return false;
  }

  if (ret) {
    (*ret) = numberValue;
  }

  return true;
}

static bool ParseNumberArrayProperty(std::vector<double> *ret, std::string *err,
                                     const detail::json &o,
                                     const std::string &property, bool required,
                                     const std::string &parent_node = "") {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  if (!detail::IsArray(detail::GetValue(it))) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not an array";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  ret->clear();
  auto end = detail::ArrayEnd(detail::GetValue(it));
  for (auto i = detail::ArrayBegin(detail::GetValue(it)); i != end; ++i) {
    double numberValue;
    const bool isNumber = detail::GetNumber(*i, numberValue);
    if (!isNumber) {
      if (required) {
        if (err) {
          (*err) += "'" + property + "' property is not a number.\n";
          if (!parent_node.empty()) {
            (*err) += " in " + parent_node;
          }
          (*err) += ".\n";
        }
      }
      return false;
    }
    ret->push_back(numberValue);
  }

  return true;
}

static bool ParseIntegerArrayProperty(std::vector<int> *ret, std::string *err,
                                      const detail::json &o,
                                      const std::string &property,
                                      bool required,
                                      const std::string &parent_node = "") {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  if (!detail::IsArray(detail::GetValue(it))) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not an array";
        if (!parent_node.empty()) {
          (*err) += " in " + parent_node;
        }
        (*err) += ".\n";
      }
    }
    return false;
  }

  ret->clear();
  auto end = detail::ArrayEnd(detail::GetValue(it));
  for (auto i = detail::ArrayBegin(detail::GetValue(it)); i != end; ++i) {
    int numberValue;
    bool isNumber = detail::GetInt(*i, numberValue);
    if (!isNumber) {
      if (required) {
        if (err) {
          (*err) += "'" + property + "' property is not an integer type.\n";
          if (!parent_node.empty()) {
            (*err) += " in " + parent_node;
          }
          (*err) += ".\n";
        }
      }
      return false;
    }
    ret->push_back(numberValue);
  }

  return true;
}

static bool ParseStringProperty(
    std::string *ret, std::string *err, const detail::json &o,
    const std::string &property, bool required,
    const std::string &parent_node = std::string()) {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing";
        if (parent_node.empty()) {
          (*err) += ".\n";
        } else {
          (*err) += " in `" + parent_node + "'.\n";
        }
      }
    }
    return false;
  }

  std::string strValue;
  if (!detail::GetString(detail::GetValue(it), strValue)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not a string type.\n";
      }
    }
    return false;
  }

  if (ret) {
    (*ret) = std::move(strValue);
  }

  return true;
}

static bool ParseStringIntegerProperty(std::map<std::string, int> *ret,
                                       std::string *err, const detail::json &o,
                                       const std::string &property,
                                       bool required,
                                       const std::string &parent = "") {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        if (!parent.empty()) {
          (*err) +=
              "'" + property + "' property is missing in " + parent + ".\n";
        } else {
          (*err) += "'" + property + "' property is missing.\n";
        }
      }
    }
    return false;
  }

  const detail::json &dict = detail::GetValue(it);

  // Make sure we are dealing with an object / dictionary.
  if (!detail::IsObject(dict)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not an object.\n";
      }
    }
    return false;
  }

  ret->clear();

  detail::json_const_iterator dictIt(detail::ObjectBegin(dict));
  detail::json_const_iterator dictItEnd(detail::ObjectEnd(dict));

  for (; dictIt != dictItEnd; ++dictIt) {
    int intVal;
    if (!detail::GetInt(detail::GetValue(dictIt), intVal)) {
      if (required) {
        if (err) {
          (*err) += "'" + property + "' value is not an integer type.\n";
        }
      }
      return false;
    }

    // Insert into the list.
    (*ret)[detail::GetKey(dictIt)] = intVal;
  }
  return true;
}

static bool ParseJSONProperty(std::map<std::string, double> *ret,
                              std::string *err, const detail::json &o,
                              const std::string &property, bool required) {
  detail::json_const_iterator it;
  if (!detail::FindMember(o, property.c_str(), it)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is missing. \n'";
      }
    }
    return false;
  }

  const detail::json &obj = detail::GetValue(it);

  if (!detail::IsObject(obj)) {
    if (required) {
      if (err) {
        (*err) += "'" + property + "' property is not a JSON object.\n";
      }
    }
    return false;
  }

  ret->clear();

  detail::json_const_iterator it2(detail::ObjectBegin(obj));
  detail::json_const_iterator itEnd(detail::ObjectEnd(obj));
  for (; it2 != itEnd; ++it2) {
    double numVal;
    if (detail::GetNumber(detail::GetValue(it2), numVal))
      ret->emplace(std::string(detail::GetKey(it2)), numVal);
  }

  return true;
}

static bool ParseParameterProperty(Parameter *param, std::string *err,
                                   const detail::json &o,
                                   const std::string &prop, bool required) {
  // A parameter value can either be a string or an array of either a boolean or
  // a number. Booleans of any kind aren't supported here. Granted, it
  // complicates the Parameter structure and breaks it semantically in the sense
  // that the client probably works off the assumption that if the string is
  // empty the vector is used, etc. Would a tagged union work?
  if (ParseStringProperty(&param->string_value, err, o, prop, false)) {
    // Found string property.
    return true;
  } else if (ParseNumberArrayProperty(&param->number_array, err, o, prop,
                                      false)) {
    // Found a number array.
    return true;
  } else if (ParseNumberProperty(&param->number_value, err, o, prop, false)) {
    param->has_number_value = true;
    return true;
  } else if (ParseJSONProperty(&param->json_double_value, err, o, prop,
                               false)) {
    return true;
  } else if (ParseBooleanProperty(&param->bool_value, err, o, prop, false)) {
    return true;
  } else {
    if (required) {
      if (err) {
        (*err) += "parameter must be a string or number / number array.\n";
      }
    }
    return false;
  }
}

static bool ParseExtensionsProperty(ExtensionMap *ret, std::string *err,
                                    const detail::json &o) {
  (void)err;

  detail::json_const_iterator it;
  if (!detail::FindMember(o, "extensions", it)) {
    return false;
  }

  auto &obj = detail::GetValue(it);
  if (!detail::IsObject(obj)) {
    return false;
  }
  ExtensionMap extensions;
  detail::json_const_iterator extIt =
      detail::ObjectBegin(obj);  // it.value().begin();
  detail::json_const_iterator extEnd = detail::ObjectEnd(obj);
  for (; extIt != extEnd; ++extIt) {
    auto &itObj = detail::GetValue(extIt);
    if (!detail::IsObject(itObj)) continue;
    std::string key(detail::GetKey(extIt));
    if (!ParseJsonAsValue(&extensions[key], itObj)) {
      if (!key.empty()) {
        // create empty object so that an extension object is still of type
        // object
        extensions[key] = Value{Value::Object{}};
      }
    }
  }
  if (ret) {
    (*ret) = std::move(extensions);
  }
  return true;
}

template <typename GltfType>
static bool ParseExtrasAndExtensions(GltfType *target, std::string *err,
                                     const detail::json &o,
                                     bool store_json_strings) {
  ParseExtensionsProperty(&target->extensions, err, o);
  ParseExtrasProperty(&target->extras, o);

  if (store_json_strings) {
    {
      detail::json_const_iterator it;
      if (detail::FindMember(o, "extensions", it)) {
        target->extensions_json_string =
            detail::JsonToString(detail::GetValue(it));
      }
    }
    {
      detail::json_const_iterator it;
      if (detail::FindMember(o, "extras", it)) {
        target->extras_json_string = detail::JsonToString(detail::GetValue(it));
      }
    }
  }
  return true;
}

static bool ParseAsset(Asset *asset, std::string *err, const detail::json &o,
                       bool store_original_json_for_extras_and_extensions) {
  ParseStringProperty(&asset->version, err, o, "version", true, "Asset");
  ParseStringProperty(&asset->generator, err, o, "generator", false, "Asset");
  ParseStringProperty(&asset->minVersion, err, o, "minVersion", false, "Asset");
  ParseStringProperty(&asset->copyright, err, o, "copyright", false, "Asset");

  ParseExtrasAndExtensions(asset, err, o,
                           store_original_json_for_extras_and_extensions);
  return true;
}

static bool ParseImage(Image *image, const int image_idx, std::string *err,
                       std::string *warn, const detail::json &o,
                       bool store_original_json_for_extras_and_extensions,
                       const std::string &basedir, const size_t max_file_size,
                       FsCallbacks *fs, const URICallbacks *uri_cb,
                       LoadImageDataFunction *LoadImageData = nullptr,
                       void *load_image_user_data = nullptr) {
  // A glTF image must either reference a bufferView or an image uri

  // schema says oneOf [`bufferView`, `uri`]
  // TODO(syoyo): Check the type of each parameters.
  detail::json_const_iterator it;
  bool hasBufferView = detail::FindMember(o, "bufferView", it);
  bool hasURI = detail::FindMember(o, "uri", it);

  ParseStringProperty(&image->name, err, o, "name", false);

  if (hasBufferView && hasURI) {
    // Should not both defined.
    if (err) {
      (*err) +=
          "Only one of `bufferView` or `uri` should be defined, but both are "
          "defined for image[" +
          std::to_string(image_idx) + "] name = \"" + image->name + "\"\n";
    }
    return false;
  }

  if (!hasBufferView && !hasURI) {
    if (err) {
      (*err) += "Neither required `bufferView` nor `uri` defined for image[" +
                std::to_string(image_idx) + "] name = \"" + image->name +
                "\"\n";
    }
    return false;
  }

  ParseExtrasAndExtensions(image, err, o,
                           store_original_json_for_extras_and_extensions);

  if (hasBufferView) {
    int bufferView = -1;
    if (!ParseIntegerProperty(&bufferView, err, o, "bufferView", true)) {
      if (err) {
        (*err) += "Failed to parse `bufferView` for image[" +
                  std::to_string(image_idx) + "] name = \"" + image->name +
                  "\"\n";
      }
      return false;
    }

    std::string mime_type;
    ParseStringProperty(&mime_type, err, o, "mimeType", false);

    int width = 0;
    ParseIntegerProperty(&width, err, o, "width", false);

    int height = 0;
    ParseIntegerProperty(&height, err, o, "height", false);

    // Just only save some information here. Loading actual image data from
    // bufferView is done after this `ParseImage` function.
    image->bufferView = bufferView;
    image->mimeType = mime_type;
    image->width = width;
    image->height = height;

    return true;
  }

  // Parse URI & Load image data.

  std::string uri;
  std::string tmp_err;
  if (!ParseStringProperty(&uri, &tmp_err, o, "uri", true)) {
    if (err) {
      (*err) += "Failed to parse `uri` for image[" + std::to_string(image_idx) +
                "] name = \"" + image->name + "\".\n";
    }
    return false;
  }

  std::vector<unsigned char> img;

  if (IsDataURI(uri)) {
    if (!DecodeDataURI(&img, image->mimeType, uri, 0, false)) {
      if (err) {
        (*err) += "Failed to decode 'uri' for image[" +
                  std::to_string(image_idx) + "] name = \"" + image->name +
                  "\"\n";
      }
      return false;
    }
  } else {
    // Assume external file
    // Keep texture path (for textures that cannot be decoded)
    image->uri = uri;
#ifdef TINYGLTF_NO_EXTERNAL_IMAGE
    return true;
#else
    std::string decoded_uri;
    if (!uri_cb->decode(uri, &decoded_uri, uri_cb->user_data)) {
      if (warn) {
        (*warn) += "Failed to decode 'uri' for image[" +
                   std::to_string(image_idx) + "] name = \"" + image->name +
                   "\"\n";
      }

      // Image loading failure is not critical to overall gltf loading.
      return true;
    }

    if (!LoadExternalFile(&img, err, warn, decoded_uri, basedir,
                          /* required */ false, /* required bytes */ 0,
                          /* checksize */ false,
                          /* max file size */ max_file_size, fs)) {
      if (warn) {
        (*warn) += "Failed to load external 'uri' for image[" +
                   std::to_string(image_idx) + "] name = \"" + decoded_uri +
                   "\"\n";
      }
      // If the image cannot be loaded, keep uri as image->uri.
      return true;
    }

    if (img.empty()) {
      if (warn) {
        (*warn) += "Image data is empty for image[" +
                   std::to_string(image_idx) + "] name = \"" + image->name +
                   "\" \n";
      }
      return false;
    }
#endif
  }

  if (*LoadImageData == nullptr) {
    if (err) {
      (*err) += "No LoadImageData callback specified.\n";
    }
    return false;
  }
  return (*LoadImageData)(image, image_idx, err, warn, 0, 0, &img.at(0),
                          static_cast<int>(img.size()), load_image_user_data);
}

static bool ParseTexture(Texture *texture, std::string *err,
                         const detail::json &o,
                         bool store_original_json_for_extras_and_extensions,
                         const std::string &basedir) {
  (void)basedir;
  int sampler = -1;
  int source = -1;
  ParseIntegerProperty(&sampler, err, o, "sampler", false);

  ParseIntegerProperty(&source, err, o, "source", false);

  texture->sampler = sampler;
  texture->source = source;

  ParseExtrasAndExtensions(texture, err, o,
                           store_original_json_for_extras_and_extensions);

  ParseStringProperty(&texture->name, err, o, "name", false);

  return true;
}

static bool ParseTextureInfo(
    TextureInfo *texinfo, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  if (texinfo == nullptr) {
    return false;
  }

  if (!ParseIntegerProperty(&texinfo->index, err, o, "index",
                            /* required */ true, "TextureInfo")) {
    return false;
  }

  ParseIntegerProperty(&texinfo->texCoord, err, o, "texCoord", false);

  ParseExtrasAndExtensions(texinfo, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseNormalTextureInfo(
    NormalTextureInfo *texinfo, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  if (texinfo == nullptr) {
    return false;
  }

  if (!ParseIntegerProperty(&texinfo->index, err, o, "index",
                            /* required */ true, "NormalTextureInfo")) {
    return false;
  }

  ParseIntegerProperty(&texinfo->texCoord, err, o, "texCoord", false);
  ParseNumberProperty(&texinfo->scale, err, o, "scale", false);

  ParseExtrasAndExtensions(texinfo, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseOcclusionTextureInfo(
    OcclusionTextureInfo *texinfo, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  if (texinfo == nullptr) {
    return false;
  }

  if (!ParseIntegerProperty(&texinfo->index, err, o, "index",
                            /* required */ true, "NormalTextureInfo")) {
    return false;
  }

  ParseIntegerProperty(&texinfo->texCoord, err, o, "texCoord", false);
  ParseNumberProperty(&texinfo->strength, err, o, "strength", false);

  ParseExtrasAndExtensions(texinfo, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseBuffer(Buffer *buffer, std::string *err, const detail::json &o,
                        bool store_original_json_for_extras_and_extensions,
                        FsCallbacks *fs, const URICallbacks *uri_cb,
                        const std::string &basedir,
                        const size_t max_buffer_size, bool is_binary = false,
                        const unsigned char *bin_data = nullptr,
                        size_t bin_size = 0) {
  size_t byteLength;
  if (!ParseUnsignedProperty(&byteLength, err, o, "byteLength", true,
                             "Buffer")) {
    return false;
  }

  // In glTF 2.0, uri is not mandatory anymore
  buffer->uri.clear();
  ParseStringProperty(&buffer->uri, err, o, "uri", false, "Buffer");

  // having an empty uri for a non embedded image should not be valid
  if (!is_binary && buffer->uri.empty()) {
    if (err) {
      (*err) += "'uri' is missing from non binary glTF file buffer.\n";
    }
  }

  detail::json_const_iterator type;
  if (detail::FindMember(o, "type", type)) {
    std::string typeStr;
    if (detail::GetString(detail::GetValue(type), typeStr)) {
      if (typeStr.compare("arraybuffer") == 0) {
        // buffer.type = "arraybuffer";
      }
    }
  }

  if (is_binary) {
    // Still binary glTF accepts external dataURI.
    if (!buffer->uri.empty()) {
      // First try embedded data URI.
      if (IsDataURI(buffer->uri)) {
        std::string mime_type;
        if (!DecodeDataURI(&buffer->data, mime_type, buffer->uri, byteLength,
                           true)) {
          if (err) {
            (*err) +=
                "Failed to decode 'uri' : " + buffer->uri + " in Buffer\n";
          }
          return false;
        }
      } else {
        // External .bin file.
        std::string decoded_uri;
        if (!uri_cb->decode(buffer->uri, &decoded_uri, uri_cb->user_data)) {
          return false;
        }
        if (!LoadExternalFile(&buffer->data, err, /* warn */ nullptr,
                              decoded_uri, basedir, /* required */ true,
                              byteLength, /* checkSize */ true,
                              /* max_file_size */ max_buffer_size, fs)) {
          return false;
        }
      }
    } else {
      // load data from (embedded) binary data

      if ((bin_size == 0) || (bin_data == nullptr)) {
        if (err) {
          (*err) +=
              "Invalid binary data in `Buffer', or GLB with empty BIN chunk.\n";
        }
        return false;
      }

      if (byteLength > bin_size) {
        if (err) {
          std::stringstream ss;
          ss << "Invalid `byteLength'. Must be equal or less than binary size: "
                "`byteLength' = "
             << byteLength << ", binary size = " << bin_size << std::endl;
          (*err) += ss.str();
        }
        return false;
      }

      // Read buffer data
      buffer->data.resize(static_cast<size_t>(byteLength));
      memcpy(&(buffer->data.at(0)), bin_data, static_cast<size_t>(byteLength));
    }

  } else {
    if (IsDataURI(buffer->uri)) {
      std::string mime_type;
      if (!DecodeDataURI(&buffer->data, mime_type, buffer->uri, byteLength,
                         true)) {
        if (err) {
          (*err) += "Failed to decode 'uri' : " + buffer->uri + " in Buffer\n";
        }
        return false;
      }
    } else {
      // Assume external .bin file.
      std::string decoded_uri;
      if (!uri_cb->decode(buffer->uri, &decoded_uri, uri_cb->user_data)) {
        return false;
      }
      if (!LoadExternalFile(&buffer->data, err, /* warn */ nullptr, decoded_uri,
                            basedir, /* required */ true, byteLength,
                            /* checkSize */ true,
                            /* max file size */ max_buffer_size, fs)) {
        return false;
      }
    }
  }

  ParseStringProperty(&buffer->name, err, o, "name", false);

  ParseExtrasAndExtensions(buffer, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseBufferView(
    BufferView *bufferView, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  int buffer = -1;
  if (!ParseIntegerProperty(&buffer, err, o, "buffer", true, "BufferView")) {
    return false;
  }

  size_t byteOffset = 0;
  ParseUnsignedProperty(&byteOffset, err, o, "byteOffset", false);

  size_t byteLength = 1;
  if (!ParseUnsignedProperty(&byteLength, err, o, "byteLength", true,
                             "BufferView")) {
    return false;
  }

  size_t byteStride = 0;
  if (!ParseUnsignedProperty(&byteStride, err, o, "byteStride", false)) {
    // Spec says: When byteStride of referenced bufferView is not defined, it
    // means that accessor elements are tightly packed, i.e., effective stride
    // equals the size of the element.
    // We cannot determine the actual byteStride until Accessor are parsed, thus
    // set 0(= tightly packed) here(as done in OpenGL's VertexAttribPoiner)
    byteStride = 0;
  }

  if ((byteStride > 252) || ((byteStride % 4) != 0)) {
    if (err) {
      std::stringstream ss;
      ss << "Invalid `byteStride' value. `byteStride' must be the multiple of "
            "4 : "
         << byteStride << std::endl;

      (*err) += ss.str();
    }
    return false;
  }

  int target = 0;
  ParseIntegerProperty(&target, err, o, "target", false);
  if ((target == TINYGLTF_TARGET_ARRAY_BUFFER) ||
      (target == TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER)) {
    // OK
  } else {
    target = 0;
  }
  bufferView->target = target;

  ParseStringProperty(&bufferView->name, err, o, "name", false);

  ParseExtrasAndExtensions(bufferView, err, o,
                           store_original_json_for_extras_and_extensions);

  bufferView->buffer = buffer;
  bufferView->byteOffset = byteOffset;
  bufferView->byteLength = byteLength;
  bufferView->byteStride = byteStride;
  return true;
}

static bool ParseSparseAccessor(
    Accessor::Sparse *sparse, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  sparse->isSparse = true;

  int count = 0;
  if (!ParseIntegerProperty(&count, err, o, "count", true, "SparseAccessor")) {
    return false;
  }

  ParseExtrasAndExtensions(sparse, err, o,
                           store_original_json_for_extras_and_extensions);

  detail::json_const_iterator indices_iterator;
  detail::json_const_iterator values_iterator;
  if (!detail::FindMember(o, "indices", indices_iterator)) {
    (*err) = "the sparse object of this accessor doesn't have indices";
    return false;
  }

  if (!detail::FindMember(o, "values", values_iterator)) {
    (*err) = "the sparse object of this accessor doesn't have values";
    return false;
  }

  const detail::json &indices_obj = detail::GetValue(indices_iterator);
  const detail::json &values_obj = detail::GetValue(values_iterator);

  int indices_buffer_view = 0, component_type = 0;
  size_t indices_byte_offset = 0;
  if (!ParseIntegerProperty(&indices_buffer_view, err, indices_obj,
                            "bufferView", true, "SparseAccessor")) {
    return false;
  }
  ParseUnsignedProperty(&indices_byte_offset, err, indices_obj, "byteOffset",
                       false);
  if (!ParseIntegerProperty(&component_type, err, indices_obj, "componentType",
                            true, "SparseAccessor")) {
    return false;
  }

  int values_buffer_view = 0;
  size_t values_byte_offset = 0;
  if (!ParseIntegerProperty(&values_buffer_view, err, values_obj, "bufferView",
                            true, "SparseAccessor")) {
    return false;
  }
  ParseUnsignedProperty(&values_byte_offset, err, values_obj, "byteOffset",
                       false);

  sparse->count = count;
  sparse->indices.bufferView = indices_buffer_view;
  sparse->indices.byteOffset = indices_byte_offset;
  sparse->indices.componentType = component_type;
  ParseExtrasAndExtensions(&sparse->indices, err, indices_obj,
                           store_original_json_for_extras_and_extensions);

  sparse->values.bufferView = values_buffer_view;
  sparse->values.byteOffset = values_byte_offset;
  ParseExtrasAndExtensions(&sparse->values, err, values_obj,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseAccessor(Accessor *accessor, std::string *err,
                          const detail::json &o,
                          bool store_original_json_for_extras_and_extensions) {
  int bufferView = -1;
  ParseIntegerProperty(&bufferView, err, o, "bufferView", false, "Accessor");

  size_t byteOffset = 0;
  ParseUnsignedProperty(&byteOffset, err, o, "byteOffset", false, "Accessor");

  bool normalized = false;
  ParseBooleanProperty(&normalized, err, o, "normalized", false, "Accessor");

  size_t componentType = 0;
  if (!ParseUnsignedProperty(&componentType, err, o, "componentType", true,
                             "Accessor")) {
    return false;
  }

  size_t count = 0;
  if (!ParseUnsignedProperty(&count, err, o, "count", true, "Accessor")) {
    return false;
  }

  std::string type;
  if (!ParseStringProperty(&type, err, o, "type", true, "Accessor")) {
    return false;
  }

  if (type.compare("SCALAR") == 0) {
    accessor->type = TINYGLTF_TYPE_SCALAR;
  } else if (type.compare("VEC2") == 0) {
    accessor->type = TINYGLTF_TYPE_VEC2;
  } else if (type.compare("VEC3") == 0) {
    accessor->type = TINYGLTF_TYPE_VEC3;
  } else if (type.compare("VEC4") == 0) {
    accessor->type = TINYGLTF_TYPE_VEC4;
  } else if (type.compare("MAT2") == 0) {
    accessor->type = TINYGLTF_TYPE_MAT2;
  } else if (type.compare("MAT3") == 0) {
    accessor->type = TINYGLTF_TYPE_MAT3;
  } else if (type.compare("MAT4") == 0) {
    accessor->type = TINYGLTF_TYPE_MAT4;
  } else {
    std::stringstream ss;
    ss << "Unsupported `type` for accessor object. Got \"" << type << "\"\n";
    if (err) {
      (*err) += ss.str();
    }
    return false;
  }

  ParseStringProperty(&accessor->name, err, o, "name", false);

  accessor->minValues.clear();
  accessor->maxValues.clear();
  ParseNumberArrayProperty(&accessor->minValues, err, o, "min", false,
                           "Accessor");

  ParseNumberArrayProperty(&accessor->maxValues, err, o, "max", false,
                           "Accessor");

  accessor->count = count;
  accessor->bufferView = bufferView;
  accessor->byteOffset = byteOffset;
  accessor->normalized = normalized;
  {
    if (componentType >= TINYGLTF_COMPONENT_TYPE_BYTE &&
        componentType <= TINYGLTF_COMPONENT_TYPE_DOUBLE) {
      // OK
      accessor->componentType = int(componentType);
    } else {
      std::stringstream ss;
      ss << "Invalid `componentType` in accessor. Got " << componentType
         << "\n";
      if (err) {
        (*err) += ss.str();
      }
      return false;
    }
  }

  ParseExtrasAndExtensions(accessor, err, o,
                           store_original_json_for_extras_and_extensions);

  // check if accessor has a "sparse" object:
  detail::json_const_iterator iterator;
  if (detail::FindMember(o, "sparse", iterator)) {
    // here this accessor has a "sparse" subobject
    return ParseSparseAccessor(&accessor->sparse, err,
                               detail::GetValue(iterator),
                               store_original_json_for_extras_and_extensions);
  }

  return true;
}

#ifdef TINYGLTF_ENABLE_DRACO

static void DecodeIndexBuffer(draco::Mesh *mesh, size_t componentSize,
                              std::vector<uint8_t> &outBuffer) {
  if (componentSize == 4) {
    assert(sizeof(mesh->face(draco::FaceIndex(0))[0]) == componentSize);
    memcpy(outBuffer.data(), &mesh->face(draco::FaceIndex(0))[0],
           outBuffer.size());
  } else {
    size_t faceStride = componentSize * 3;
    for (draco::FaceIndex f(0); f < mesh->num_faces(); ++f) {
      const draco::Mesh::Face &face = mesh->face(f);
      if (componentSize == 2) {
        uint16_t indices[3] = {(uint16_t)face[0].value(),
                               (uint16_t)face[1].value(),
                               (uint16_t)face[2].value()};
        memcpy(outBuffer.data() + f.value() * faceStride, &indices[0],
               faceStride);
      } else {
        uint8_t indices[3] = {(uint8_t)face[0].value(),
                              (uint8_t)face[1].value(),
                              (uint8_t)face[2].value()};
        memcpy(outBuffer.data() + f.value() * faceStride, &indices[0],
               faceStride);
      }
    }
  }
}

template <typename T>
static bool GetAttributeForAllPoints(draco::Mesh *mesh,
                                     const draco::PointAttribute *pAttribute,
                                     std::vector<uint8_t> &outBuffer) {
  size_t byteOffset = 0;
  T values[4] = {0, 0, 0, 0};
  for (draco::PointIndex i(0); i < mesh->num_points(); ++i) {
    const draco::AttributeValueIndex val_index = pAttribute->mapped_index(i);
    if (!pAttribute->ConvertValue<T>(val_index, pAttribute->num_components(),
                                     values))
      return false;

    memcpy(outBuffer.data() + byteOffset, &values[0],
           sizeof(T) * pAttribute->num_components());
    byteOffset += sizeof(T) * pAttribute->num_components();
  }

  return true;
}

static bool GetAttributeForAllPoints(uint32_t componentType, draco::Mesh *mesh,
                                     const draco::PointAttribute *pAttribute,
                                     std::vector<uint8_t> &outBuffer) {
  bool decodeResult = false;
  switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      decodeResult =
          GetAttributeForAllPoints<uint8_t>(mesh, pAttribute, outBuffer);
      break;
    case TINYGLTF_COMPONENT_TYPE_BYTE:
      decodeResult =
          GetAttributeForAllPoints<int8_t>(mesh, pAttribute, outBuffer);
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      decodeResult =
          GetAttributeForAllPoints<uint16_t>(mesh, pAttribute, outBuffer);
      break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
      decodeResult =
          GetAttributeForAllPoints<int16_t>(mesh, pAttribute, outBuffer);
      break;
    case TINYGLTF_COMPONENT_TYPE_INT:
      decodeResult =
          GetAttributeForAllPoints<int32_t>(mesh, pAttribute, outBuffer);
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      decodeResult =
          GetAttributeForAllPoints<uint32_t>(mesh, pAttribute, outBuffer);
      break;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      decodeResult =
          GetAttributeForAllPoints<float>(mesh, pAttribute, outBuffer);
      break;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      decodeResult =
          GetAttributeForAllPoints<double>(mesh, pAttribute, outBuffer);
      break;
    default:
      return false;
  }

  return decodeResult;
}

static bool ParseDracoExtension(Primitive *primitive, Model *model,
                                std::string *err, std::string *warn,
                                const Value &dracoExtensionValue,
                                ParseStrictness strictness) {
  (void)err;
  auto bufferViewValue = dracoExtensionValue.Get("bufferView");
  if (!bufferViewValue.IsInt()) return false;
  auto attributesValue = dracoExtensionValue.Get("attributes");
  if (!attributesValue.IsObject()) return false;

  auto attributesObject = attributesValue.Get<Value::Object>();
  int bufferView = bufferViewValue.Get<int>();

  BufferView &view = model->bufferViews[bufferView];
  Buffer &buffer = model->buffers[view.buffer];
  // BufferView has already been decoded
  if (view.dracoDecoded) return true;
  view.dracoDecoded = true;

  const char *bufferViewData =
      reinterpret_cast<const char *>(buffer.data.data() + view.byteOffset);
  size_t bufferViewSize = view.byteLength;

  // decode draco
  draco::DecoderBuffer decoderBuffer;
  decoderBuffer.Init(bufferViewData, bufferViewSize);
  draco::Decoder decoder;
  auto decodeResult = decoder.DecodeMeshFromBuffer(&decoderBuffer);
  if (!decodeResult.ok()) {
    return false;
  }
  const std::unique_ptr<draco::Mesh> &mesh = decodeResult.value();

  // create new bufferView for indices
  if (primitive->indices >= 0) {
    if (strictness == ParseStrictness::Permissive) {
      const draco::PointIndex::ValueType numPoint = mesh->num_points();
      // handle the situation where the stored component type does not match the
      // required type for the actual number of stored points
      int supposedComponentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
      if (numPoint < static_cast<draco::PointIndex::ValueType>(
                         std::numeric_limits<uint8_t>::max())) {
        supposedComponentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
      } else if (
          numPoint < static_cast<draco::PointIndex::ValueType>(
                         std::numeric_limits<uint16_t>::max())) {
        supposedComponentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
      } else {
        supposedComponentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
      }

      if (supposedComponentType > model->accessors[primitive->indices].componentType) {
        if (warn) {
          (*warn) +=
              "GLTF component type " + std::to_string(model->accessors[primitive->indices].componentType) +
              " is not sufficient for number of stored points,"
              " treating as " + std::to_string(supposedComponentType) + "\n";
        }
        model->accessors[primitive->indices].componentType = supposedComponentType;
      }
    }

    int32_t componentSize = GetComponentSizeInBytes(
        model->accessors[primitive->indices].componentType);
    Buffer decodedIndexBuffer;
    decodedIndexBuffer.data.resize(mesh->num_faces() * 3 * componentSize);

    DecodeIndexBuffer(mesh.get(), componentSize, decodedIndexBuffer.data);

    model->buffers.emplace_back(std::move(decodedIndexBuffer));

    BufferView decodedIndexBufferView;
    decodedIndexBufferView.buffer = int(model->buffers.size() - 1);
    decodedIndexBufferView.byteLength =
        int(mesh->num_faces() * 3 * componentSize);
    decodedIndexBufferView.byteOffset = 0;
    decodedIndexBufferView.byteStride = 0;
    decodedIndexBufferView.target = TINYGLTF_TARGET_ARRAY_BUFFER;
    model->bufferViews.emplace_back(std::move(decodedIndexBufferView));

    model->accessors[primitive->indices].bufferView =
        int(model->bufferViews.size() - 1);
    model->accessors[primitive->indices].count = int(mesh->num_faces() * 3);
  }

  for (const auto &attribute : attributesObject) {
    if (!attribute.second.IsInt()) return false;
    auto primitiveAttribute = primitive->attributes.find(attribute.first);
    if (primitiveAttribute == primitive->attributes.end()) return false;

    int dracoAttributeIndex = attribute.second.Get<int>();
    const auto pAttribute = mesh->GetAttributeByUniqueId(dracoAttributeIndex);
    const auto componentType =
        model->accessors[primitiveAttribute->second].componentType;

    // Create a new buffer for this decoded buffer
    Buffer decodedBuffer;
    size_t bufferSize = mesh->num_points() * pAttribute->num_components() *
                        GetComponentSizeInBytes(componentType);
    decodedBuffer.data.resize(bufferSize);

    if (!GetAttributeForAllPoints(componentType, mesh.get(), pAttribute,
                                  decodedBuffer.data))
      return false;

    model->buffers.emplace_back(std::move(decodedBuffer));

    BufferView decodedBufferView;
    decodedBufferView.buffer = int(model->buffers.size() - 1);
    decodedBufferView.byteLength = bufferSize;
    decodedBufferView.byteOffset = pAttribute->byte_offset();
    decodedBufferView.byteStride = pAttribute->byte_stride();
    decodedBufferView.target = primitive->indices >= 0
                                   ? TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER
                                   : TINYGLTF_TARGET_ARRAY_BUFFER;
    model->bufferViews.emplace_back(std::move(decodedBufferView));

    model->accessors[primitiveAttribute->second].bufferView =
        int(model->bufferViews.size() - 1);
    model->accessors[primitiveAttribute->second].count =
        int(mesh->num_points());
  }

  return true;
}
#endif

static bool ParsePrimitive(Primitive *primitive, Model *model,
                           std::string *err, std::string *warn,
                           const detail::json &o,
                           bool store_original_json_for_extras_and_extensions,
                           ParseStrictness strictness) {
  int material = -1;
  ParseIntegerProperty(&material, err, o, "material", false);
  primitive->material = material;

  int mode = TINYGLTF_MODE_TRIANGLES;
  ParseIntegerProperty(&mode, err, o, "mode", false);
  primitive->mode = mode;  // Why only triangles were supported ?

  int indices = -1;
  ParseIntegerProperty(&indices, err, o, "indices", false);
  primitive->indices = indices;
  if (!ParseStringIntegerProperty(&primitive->attributes, err, o, "attributes",
                                  true, "Primitive")) {
    return false;
  }

  // Look for morph targets
  detail::json_const_iterator targetsObject;
  if (detail::FindMember(o, "targets", targetsObject) &&
      detail::IsArray(detail::GetValue(targetsObject))) {
    auto targetsObjectEnd = detail::ArrayEnd(detail::GetValue(targetsObject));
    for (detail::json_const_array_iterator i =
             detail::ArrayBegin(detail::GetValue(targetsObject));
         i != targetsObjectEnd; ++i) {
      std::map<std::string, int> targetAttribues;

      const detail::json &dict = *i;
      if (detail::IsObject(dict)) {
        detail::json_const_iterator dictIt(detail::ObjectBegin(dict));
        detail::json_const_iterator dictItEnd(detail::ObjectEnd(dict));

        for (; dictIt != dictItEnd; ++dictIt) {
          int iVal;
          if (detail::GetInt(detail::GetValue(dictIt), iVal))
            targetAttribues[detail::GetKey(dictIt)] = iVal;
        }
        primitive->targets.emplace_back(std::move(targetAttribues));
      }
    }
  }

  ParseExtrasAndExtensions(primitive, err, o,
                           store_original_json_for_extras_and_extensions);

#ifdef TINYGLTF_ENABLE_DRACO
  auto dracoExtension =
      primitive->extensions.find("KHR_draco_mesh_compression");
  if (dracoExtension != primitive->extensions.end()) {
    ParseDracoExtension(primitive, model, err, warn, dracoExtension->second, strictness);
  }
#else
  (void)model;
  (void)warn;
  (void)strictness;
#endif

  return true;
}

static bool ParseMesh(Mesh *mesh, Model *model,
                      std::string *err, std::string *warn,
                      const detail::json &o,
                      bool store_original_json_for_extras_and_extensions,
                      ParseStrictness strictness) {
  ParseStringProperty(&mesh->name, err, o, "name", false);

  mesh->primitives.clear();
  detail::json_const_iterator primObject;
  if (detail::FindMember(o, "primitives", primObject) &&
      detail::IsArray(detail::GetValue(primObject))) {
    detail::json_const_array_iterator primEnd =
        detail::ArrayEnd(detail::GetValue(primObject));
    for (detail::json_const_array_iterator i =
             detail::ArrayBegin(detail::GetValue(primObject));
         i != primEnd; ++i) {
      Primitive primitive;
      if (ParsePrimitive(&primitive, model, err, warn, *i,
                         store_original_json_for_extras_and_extensions,
                         strictness)) {
        // Only add the primitive if the parsing succeeds.
        mesh->primitives.emplace_back(std::move(primitive));
      }
    }
  }

  // Should probably check if has targets and if dimensions fit
  ParseNumberArrayProperty(&mesh->weights, err, o, "weights", false);

  ParseExtrasAndExtensions(mesh, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseNode(Node *node, std::string *err, const detail::json &o,
                      bool store_original_json_for_extras_and_extensions) {
  ParseStringProperty(&node->name, err, o, "name", false);

  int skin = -1;
  ParseIntegerProperty(&skin, err, o, "skin", false);
  node->skin = skin;

  // Matrix and T/R/S are exclusive
  if (!ParseNumberArrayProperty(&node->matrix, err, o, "matrix", false)) {
    ParseNumberArrayProperty(&node->rotation, err, o, "rotation", false);
    ParseNumberArrayProperty(&node->scale, err, o, "scale", false);
    ParseNumberArrayProperty(&node->translation, err, o, "translation", false);
  }

  int camera = -1;
  ParseIntegerProperty(&camera, err, o, "camera", false);
  node->camera = camera;

  int mesh = -1;
  ParseIntegerProperty(&mesh, err, o, "mesh", false);
  node->mesh = mesh;

  node->children.clear();
  ParseIntegerArrayProperty(&node->children, err, o, "children", false);

  ParseNumberArrayProperty(&node->weights, err, o, "weights", false);

  ParseExtrasAndExtensions(node, err, o,
                           store_original_json_for_extras_and_extensions);

  // KHR_lights_punctual: parse light source reference
  int light = -1;
  if (node->extensions.count("KHR_lights_punctual") != 0) {
    auto const &light_ext = node->extensions["KHR_lights_punctual"];
    if (light_ext.Has("light")) {
      light = light_ext.Get("light").GetNumberAsInt();
    } else {
      if (err) {
        *err +=
            "Node has extension KHR_lights_punctual, but does not reference "
            "a light source.\n";
      }
      return false;
    }
  }
  node->light = light;

  // KHR_audio: parse audio source reference
  int emitter = -1;
  if (node->extensions.count("KHR_audio") != 0) {
    auto const &audio_ext = node->extensions["KHR_audio"];
    if (audio_ext.Has("emitter")) {
      emitter = audio_ext.Get("emitter").GetNumberAsInt();
    } else {
      if (err) {
        *err +=
            "Node has extension KHR_audio, but does not reference "
            "a audio emitter.\n";
      }
      return false;
    }
  }
  node->emitter = emitter;

  node->lods.clear();
  if (node->extensions.count("MSFT_lod") != 0) {
    auto const &msft_lod_ext = node->extensions["MSFT_lod"];
    if (msft_lod_ext.Has("ids")) {
      auto idsArr = msft_lod_ext.Get("ids");
      for (size_t i = 0; i < idsArr.ArrayLen(); ++i) {
        node->lods.emplace_back(idsArr.Get(i).GetNumberAsInt());
      }
    } else {
      if (err) {
        *err +=
            "Node has extension MSFT_lod, but does not reference "
            "other nodes via their ids.\n";
      }
      return false;
    }
  }

  return true;
}

static bool ParseScene(Scene *scene, std::string *err, const detail::json &o,
                       bool store_original_json_for_extras_and_extensions) {
  ParseStringProperty(&scene->name, err, o, "name", false);
  ParseIntegerArrayProperty(&scene->nodes, err, o, "nodes", false);

  ParseExtrasAndExtensions(scene, err, o,
                           store_original_json_for_extras_and_extensions);

  // Parse KHR_audio global emitters
  if (scene->extensions.count("KHR_audio") != 0) {
    auto const &audio_ext = scene->extensions["KHR_audio"];
    if (audio_ext.Has("emitters")) {
      auto emittersArr = audio_ext.Get("emitters");
      for (size_t i = 0; i < emittersArr.ArrayLen(); ++i) {
        scene->audioEmitters.emplace_back(emittersArr.Get(i).GetNumberAsInt());
      }
    } else {
      if (err) {
        *err +=
            "Node has extension KHR_audio, but does not reference "
            "a audio emitter.\n";
      }
      return false;
    }
  }

  return true;
}

static bool ParsePbrMetallicRoughness(
    PbrMetallicRoughness *pbr, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  if (pbr == nullptr) {
    return false;
  }

  std::vector<double> baseColorFactor;
  if (ParseNumberArrayProperty(&baseColorFactor, err, o, "baseColorFactor",
                               /* required */ false)) {
    if (baseColorFactor.size() != 4) {
      if (err) {
        (*err) +=
            "Array length of `baseColorFactor` parameter in "
            "pbrMetallicRoughness must be 4, but got " +
            std::to_string(baseColorFactor.size()) + "\n";
      }
      return false;
    }
    pbr->baseColorFactor = baseColorFactor;
  }

  {
    detail::json_const_iterator it;
    if (detail::FindMember(o, "baseColorTexture", it)) {
      ParseTextureInfo(&pbr->baseColorTexture, err, detail::GetValue(it),
                       store_original_json_for_extras_and_extensions);
    }
  }

  {
    detail::json_const_iterator it;
    if (detail::FindMember(o, "metallicRoughnessTexture", it)) {
      ParseTextureInfo(&pbr->metallicRoughnessTexture, err,
                       detail::GetValue(it),
                       store_original_json_for_extras_and_extensions);
    }
  }

  ParseNumberProperty(&pbr->metallicFactor, err, o, "metallicFactor", false);
  ParseNumberProperty(&pbr->roughnessFactor, err, o, "roughnessFactor", false);

  ParseExtrasAndExtensions(pbr, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseMaterial(Material *material, std::string *err, std::string *warn,
                          const detail::json &o,
                          bool store_original_json_for_extras_and_extensions,
                          ParseStrictness strictness) {
  ParseStringProperty(&material->name, err, o, "name", /* required */ false);

  if (ParseNumberArrayProperty(&material->emissiveFactor, err, o,
                               "emissiveFactor",
                               /* required */ false)) {
    if (strictness==ParseStrictness::Permissive && material->emissiveFactor.size() == 4) {
      if (warn) {
        (*warn) +=
            "Array length of `emissiveFactor` parameter in "
            "material must be 3, but got 4\n";
      }
      material->emissiveFactor.resize(3);
    }
    else if (material->emissiveFactor.size() != 3) {
      if (err) {
        (*err) +=
            "Array length of `emissiveFactor` parameter in "
            "material must be 3, but got " +
            std::to_string(material->emissiveFactor.size()) + "\n";
      }
      return false;
    }
  } else {
    // fill with default values
    material->emissiveFactor = {0.0, 0.0, 0.0};
  }

  ParseStringProperty(&material->alphaMode, err, o, "alphaMode",
                      /* required */ false);
  ParseNumberProperty(&material->alphaCutoff, err, o, "alphaCutoff",
                      /* required */ false);
  ParseBooleanProperty(&material->doubleSided, err, o, "doubleSided",
                       /* required */ false);

  {
    detail::json_const_iterator it;
    if (detail::FindMember(o, "pbrMetallicRoughness", it)) {
      ParsePbrMetallicRoughness(&material->pbrMetallicRoughness, err,
                                detail::GetValue(it),
                                store_original_json_for_extras_and_extensions);
    }
  }

  {
    detail::json_const_iterator it;
    if (detail::FindMember(o, "normalTexture", it)) {
      ParseNormalTextureInfo(&material->normalTexture, err,
                             detail::GetValue(it),
                             store_original_json_for_extras_and_extensions);
    }
  }

  {
    detail::json_const_iterator it;
    if (detail::FindMember(o, "occlusionTexture", it)) {
      ParseOcclusionTextureInfo(&material->occlusionTexture, err,
                                detail::GetValue(it),
                                store_original_json_for_extras_and_extensions);
    }
  }

  {
    detail::json_const_iterator it;
    if (detail::FindMember(o, "emissiveTexture", it)) {
      ParseTextureInfo(&material->emissiveTexture, err, detail::GetValue(it),
                       store_original_json_for_extras_and_extensions);
    }
  }

  // Old code path. For backward compatibility, we still store material values
  // as Parameter. This will create duplicated information for
  // example(pbrMetallicRoughness), but should be negligible in terms of memory
  // consumption.
  // TODO(syoyo): Remove in the next major release.
  material->values.clear();
  material->additionalValues.clear();

  detail::json_const_iterator it(detail::ObjectBegin(o));
  detail::json_const_iterator itEnd(detail::ObjectEnd(o));

  for (; it != itEnd; ++it) {
    std::string key(detail::GetKey(it));
    if (key == "pbrMetallicRoughness") {
      if (detail::IsObject(detail::GetValue(it))) {
        const detail::json &values_object = detail::GetValue(it);

        detail::json_const_iterator itVal(detail::ObjectBegin(values_object));
        detail::json_const_iterator itValEnd(detail::ObjectEnd(values_object));

        for (; itVal != itValEnd; ++itVal) {
          Parameter param;
          if (ParseParameterProperty(&param, err, values_object,
                                     detail::GetKey(itVal), false)) {
            material->values.emplace(detail::GetKey(itVal), std::move(param));
          }
        }
      }
    } else if (key == "extensions" || key == "extras") {
      // done later, skip, otherwise poorly parsed contents will be saved in the
      // parametermap and serialized again later
    } else {
      Parameter param;
      if (ParseParameterProperty(&param, err, o, key, false)) {
        // names of materials have already been parsed. Putting it in this map
        // doesn't correctly reflect the glTF specification
        if (key != "name")
          material->additionalValues.emplace(std::move(key), std::move(param));
      }
    }
  }

  material->extensions.clear();  // Note(agnat): Why?
  ParseExtrasAndExtensions(material, err, o,
                           store_original_json_for_extras_and_extensions);

  material->lods.clear();
  if (material->extensions.count("MSFT_lod") != 0) {
    auto const &msft_lod_ext = material->extensions["MSFT_lod"];
    if (msft_lod_ext.Has("ids")) {
      auto idsArr = msft_lod_ext.Get("ids");
      for (size_t i = 0; i < idsArr.ArrayLen(); ++i) {
        material->lods.emplace_back(idsArr.Get(i).GetNumberAsInt());
      }
    } else {
      if (err) {
        *err +=
            "Material has extension MSFT_lod, but does not reference "
            "other materials via their ids.\n";
      }
      return false;
    }
  }

  return true;
}

static bool ParseAnimationChannel(
    AnimationChannel *channel, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  int samplerIndex = -1;
  int targetIndex = -1;
  if (!ParseIntegerProperty(&samplerIndex, err, o, "sampler", true,
                            "AnimationChannel")) {
    if (err) {
      (*err) += "`sampler` field is missing in animation channels\n";
    }
    return false;
  }

  detail::json_const_iterator targetIt;
  if (detail::FindMember(o, "target", targetIt) &&
      detail::IsObject(detail::GetValue(targetIt))) {
    const detail::json &target_object = detail::GetValue(targetIt);

    ParseIntegerProperty(&targetIndex, err, target_object, "node", false);

    if (!ParseStringProperty(&channel->target_path, err, target_object, "path",
                             true)) {
      if (err) {
        (*err) += "`path` field is missing in animation.channels.target\n";
      }
      return false;
    }
    ParseExtensionsProperty(&channel->target_extensions, err, target_object);
    ParseExtrasProperty(&channel->target_extras, target_object);
    if (store_original_json_for_extras_and_extensions) {
      {
        detail::json_const_iterator it;
        if (detail::FindMember(target_object, "extensions", it)) {
          channel->target_extensions_json_string =
              detail::JsonToString(detail::GetValue(it));
        }
      }
      {
        detail::json_const_iterator it;
        if (detail::FindMember(target_object, "extras", it)) {
          channel->target_extras_json_string =
              detail::JsonToString(detail::GetValue(it));
        }
      }
    }
  }

  channel->sampler = samplerIndex;
  channel->target_node = targetIndex;

  ParseExtrasAndExtensions(channel, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseAnimation(Animation *animation, std::string *err,
                           const detail::json &o,
                           bool store_original_json_for_extras_and_extensions) {
  {
    detail::json_const_iterator channelsIt;
    if (detail::FindMember(o, "channels", channelsIt) &&
        detail::IsArray(detail::GetValue(channelsIt))) {
      detail::json_const_array_iterator channelEnd =
          detail::ArrayEnd(detail::GetValue(channelsIt));
      for (detail::json_const_array_iterator i =
               detail::ArrayBegin(detail::GetValue(channelsIt));
           i != channelEnd; ++i) {
        AnimationChannel channel;
        if (ParseAnimationChannel(
                &channel, err, *i,
                store_original_json_for_extras_and_extensions)) {
          // Only add the channel if the parsing succeeds.
          animation->channels.emplace_back(std::move(channel));
        }
      }
    }
  }

  {
    detail::json_const_iterator samplerIt;
    if (detail::FindMember(o, "samplers", samplerIt) &&
        detail::IsArray(detail::GetValue(samplerIt))) {
      const detail::json &sampler_array = detail::GetValue(samplerIt);

      detail::json_const_array_iterator it = detail::ArrayBegin(sampler_array);
      detail::json_const_array_iterator itEnd = detail::ArrayEnd(sampler_array);

      for (; it != itEnd; ++it) {
        const detail::json &s = *it;

        AnimationSampler sampler;
        int inputIndex = -1;
        int outputIndex = -1;
        if (!ParseIntegerProperty(&inputIndex, err, s, "input", true)) {
          if (err) {
            (*err) += "`input` field is missing in animation.sampler\n";
          }
          return false;
        }
        ParseStringProperty(&sampler.interpolation, err, s, "interpolation",
                            false);
        if (!ParseIntegerProperty(&outputIndex, err, s, "output", true)) {
          if (err) {
            (*err) += "`output` field is missing in animation.sampler\n";
          }
          return false;
        }
        sampler.input = inputIndex;
        sampler.output = outputIndex;
        ParseExtrasAndExtensions(&sampler, err, o,
                                 store_original_json_for_extras_and_extensions);

        animation->samplers.emplace_back(std::move(sampler));
      }
    }
  }

  ParseStringProperty(&animation->name, err, o, "name", false);

  ParseExtrasAndExtensions(animation, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseSampler(Sampler *sampler, std::string *err,
                         const detail::json &o,
                         bool store_original_json_for_extras_and_extensions) {
  ParseStringProperty(&sampler->name, err, o, "name", false);

  int minFilter = -1;
  int magFilter = -1;
  int wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
  int wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
  // int wrapR = TINYGLTF_TEXTURE_WRAP_REPEAT;
  ParseIntegerProperty(&minFilter, err, o, "minFilter", false);
  ParseIntegerProperty(&magFilter, err, o, "magFilter", false);
  ParseIntegerProperty(&wrapS, err, o, "wrapS", false);
  ParseIntegerProperty(&wrapT, err, o, "wrapT", false);
  // ParseIntegerProperty(&wrapR, err, o, "wrapR", false);  // tinygltf
  // extension

  // TODO(syoyo): Check the value is allowed one.
  // (e.g. we allow 9728(NEAREST), but don't allow 9727)

  sampler->minFilter = minFilter;
  sampler->magFilter = magFilter;
  sampler->wrapS = wrapS;
  sampler->wrapT = wrapT;
  // sampler->wrapR = wrapR;

  ParseExtrasAndExtensions(sampler, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseSkin(Skin *skin, std::string *err, const detail::json &o,
                      bool store_original_json_for_extras_and_extensions) {
  ParseStringProperty(&skin->name, err, o, "name", false, "Skin");

  std::vector<int> joints;
  if (!ParseIntegerArrayProperty(&joints, err, o, "joints", false, "Skin")) {
    return false;
  }
  skin->joints = std::move(joints);

  int skeleton = -1;
  ParseIntegerProperty(&skeleton, err, o, "skeleton", false, "Skin");
  skin->skeleton = skeleton;

  int invBind = -1;
  ParseIntegerProperty(&invBind, err, o, "inverseBindMatrices", true, "Skin");
  skin->inverseBindMatrices = invBind;

  ParseExtrasAndExtensions(skin, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParsePerspectiveCamera(
    PerspectiveCamera *camera, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  double yfov = 0.0;
  if (!ParseNumberProperty(&yfov, err, o, "yfov", true, "OrthographicCamera")) {
    return false;
  }

  double znear = 0.0;
  if (!ParseNumberProperty(&znear, err, o, "znear", true,
                           "PerspectiveCamera")) {
    return false;
  }

  double aspectRatio = 0.0;  // = invalid
  ParseNumberProperty(&aspectRatio, err, o, "aspectRatio", false,
                      "PerspectiveCamera");

  double zfar = 0.0;  // = invalid
  ParseNumberProperty(&zfar, err, o, "zfar", false, "PerspectiveCamera");

  camera->aspectRatio = aspectRatio;
  camera->zfar = zfar;
  camera->yfov = yfov;
  camera->znear = znear;

  ParseExtrasAndExtensions(camera, err, o,
                           store_original_json_for_extras_and_extensions);

  // TODO(syoyo): Validate parameter values.

  return true;
}

static bool ParseSpotLight(SpotLight *light, std::string *err,
                           const detail::json &o,
                           bool store_original_json_for_extras_and_extensions) {
  ParseNumberProperty(&light->innerConeAngle, err, o, "innerConeAngle", false);
  ParseNumberProperty(&light->outerConeAngle, err, o, "outerConeAngle", false);

  ParseExtrasAndExtensions(light, err, o,
                           store_original_json_for_extras_and_extensions);

  // TODO(syoyo): Validate parameter values.

  return true;
}

static bool ParseOrthographicCamera(
    OrthographicCamera *camera, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  double xmag = 0.0;
  if (!ParseNumberProperty(&xmag, err, o, "xmag", true, "OrthographicCamera")) {
    return false;
  }

  double ymag = 0.0;
  if (!ParseNumberProperty(&ymag, err, o, "ymag", true, "OrthographicCamera")) {
    return false;
  }

  double zfar = 0.0;
  if (!ParseNumberProperty(&zfar, err, o, "zfar", true, "OrthographicCamera")) {
    return false;
  }

  double znear = 0.0;
  if (!ParseNumberProperty(&znear, err, o, "znear", true,
                           "OrthographicCamera")) {
    return false;
  }

  ParseExtrasAndExtensions(camera, err, o,
                           store_original_json_for_extras_and_extensions);

  camera->xmag = xmag;
  camera->ymag = ymag;
  camera->zfar = zfar;
  camera->znear = znear;

  // TODO(syoyo): Validate parameter values.

  return true;
}

static bool ParseCamera(Camera *camera, std::string *err, const detail::json &o,
                        bool store_original_json_for_extras_and_extensions) {
  if (!ParseStringProperty(&camera->type, err, o, "type", true, "Camera")) {
    return false;
  }

  if (camera->type.compare("orthographic") == 0) {
    detail::json_const_iterator orthoIt;
    if (!detail::FindMember(o, "orthographic", orthoIt)) {
      if (err) {
        std::stringstream ss;
        ss << "Orthographic camera description not found." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    const detail::json &v = detail::GetValue(orthoIt);
    if (!detail::IsObject(v)) {
      if (err) {
        std::stringstream ss;
        ss << "\"orthographic\" is not a JSON object." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    if (!ParseOrthographicCamera(
            &camera->orthographic, err, v,
            store_original_json_for_extras_and_extensions)) {
      return false;
    }
  } else if (camera->type.compare("perspective") == 0) {
    detail::json_const_iterator perspIt;
    if (!detail::FindMember(o, "perspective", perspIt)) {
      if (err) {
        std::stringstream ss;
        ss << "Perspective camera description not found." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    const detail::json &v = detail::GetValue(perspIt);
    if (!detail::IsObject(v)) {
      if (err) {
        std::stringstream ss;
        ss << "\"perspective\" is not a JSON object." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    if (!ParsePerspectiveCamera(
            &camera->perspective, err, v,
            store_original_json_for_extras_and_extensions)) {
      return false;
    }
  } else {
    if (err) {
      std::stringstream ss;
      ss << "Invalid camera type: \"" << camera->type
         << "\". Must be \"perspective\" or \"orthographic\"" << std::endl;
      (*err) += ss.str();
    }
    return false;
  }

  ParseStringProperty(&camera->name, err, o, "name", false);

  ParseExtrasAndExtensions(camera, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseLight(Light *light, std::string *err, const detail::json &o,
                       bool store_original_json_for_extras_and_extensions) {
  if (!ParseStringProperty(&light->type, err, o, "type", true)) {
    return false;
  }

  if (light->type == "spot") {
    detail::json_const_iterator spotIt;
    if (!detail::FindMember(o, "spot", spotIt)) {
      if (err) {
        std::stringstream ss;
        ss << "Spot light description not found." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    const detail::json &v = detail::GetValue(spotIt);
    if (!detail::IsObject(v)) {
      if (err) {
        std::stringstream ss;
        ss << "\"spot\" is not a JSON object." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    if (!ParseSpotLight(&light->spot, err, v,
                        store_original_json_for_extras_and_extensions)) {
      return false;
    }
  }

  ParseStringProperty(&light->name, err, o, "name", false);
  ParseNumberArrayProperty(&light->color, err, o, "color", false);
  ParseNumberProperty(&light->range, err, o, "range", false);
  ParseNumberProperty(&light->intensity, err, o, "intensity", false);

  ParseExtrasAndExtensions(light, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParsePositionalEmitter(
    PositionalEmitter *positional, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  ParseNumberProperty(&positional->coneInnerAngle, err, o, "coneInnerAngle",
                      false);
  ParseNumberProperty(&positional->coneOuterAngle, err, o, "coneOuterAngle",
                      false);
  ParseNumberProperty(&positional->coneOuterGain, err, o, "coneOuterGain",
                      false);
  ParseNumberProperty(&positional->maxDistance, err, o, "maxDistance", false);
  ParseNumberProperty(&positional->refDistance, err, o, "refDistance", false);
  ParseNumberProperty(&positional->rolloffFactor, err, o, "rolloffFactor",
                      false);

  ParseExtrasAndExtensions(positional, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseAudioEmitter(
    AudioEmitter *emitter, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  if (!ParseStringProperty(&emitter->type, err, o, "type", true)) {
    return false;
  }

  if (emitter->type == "positional") {
    detail::json_const_iterator positionalIt;
    if (!detail::FindMember(o, "positional", positionalIt)) {
      if (err) {
        std::stringstream ss;
        ss << "Positional emitter description not found." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    const detail::json &v = detail::GetValue(positionalIt);
    if (!detail::IsObject(v)) {
      if (err) {
        std::stringstream ss;
        ss << "\"positional\" is not a JSON object." << std::endl;
        (*err) += ss.str();
      }
      return false;
    }

    if (!ParsePositionalEmitter(
            &emitter->positional, err, v,
            store_original_json_for_extras_and_extensions)) {
      return false;
    }
  }

  ParseStringProperty(&emitter->name, err, o, "name", false);
  ParseNumberProperty(&emitter->gain, err, o, "gain", false);
  ParseBooleanProperty(&emitter->loop, err, o, "loop", false);
  ParseBooleanProperty(&emitter->playing, err, o, "playing", false);
  ParseStringProperty(&emitter->distanceModel, err, o, "distanceModel", false);
  ParseIntegerProperty(&emitter->source, err, o, "source", true);

  ParseExtrasAndExtensions(emitter, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

static bool ParseAudioSource(
    AudioSource *source, std::string *err, const detail::json &o,
    bool store_original_json_for_extras_and_extensions) {
  ParseStringProperty(&source->name, err, o, "name", false);
  ParseStringProperty(&source->uri, err, o, "uri", false);

  if (source->uri.empty()) {
    ParseIntegerProperty(&source->bufferView, err, o, "bufferView", true);
    ParseStringProperty(&source->mimeType, err, o, "mimeType", true);
  }

  ParseExtrasAndExtensions(source, err, o,
                           store_original_json_for_extras_and_extensions);

  return true;
}

namespace detail {

template <typename Callback>
bool ForEachInArray(const detail::json &_v, const char *member, Callback &&cb) {
  detail::json_const_iterator itm;
  if (detail::FindMember(_v, member, itm) &&
      detail::IsArray(detail::GetValue(itm))) {
    const detail::json &root = detail::GetValue(itm);
    auto it = detail::ArrayBegin(root);
    auto end = detail::ArrayEnd(root);
    for (; it != end; ++it) {
      if (!cb(*it)) return false;
    }
  }
  return true;
};

}  // end of namespace detail

bool TinyGLTF::LoadFromString(Model *model, std::string *err, std::string *warn,
                              const char *json_str,
                              unsigned int json_str_length,
                              const std::string &base_dir,
                              unsigned int check_sections) {
  if (json_str_length < 4) {
    if (err) {
      (*err) = "JSON string too short.\n";
    }
    return false;
  }

  detail::JsonDocument v;

#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || \
     defined(_CPPUNWIND)) &&                               \
    !defined(TINYGLTF_NOEXCEPTION)
  try {
    detail::JsonParse(v, json_str, json_str_length, true);

  } catch (const std::exception &e) {
    if (err) {
      (*err) = e.what();
    }
    return false;
  }
#else
  {
    detail::JsonParse(v, json_str, json_str_length);

    if (!detail::IsObject(v)) {
      // Assume parsing was failed.
      if (err) {
        (*err) = "Failed to parse JSON object\n";
      }
      return false;
    }
  }
#endif

  if (!detail::IsObject(v)) {
    // root is not an object.
    if (err) {
      (*err) = "Root element is not a JSON object\n";
    }
    return false;
  }

  {
    bool version_found = false;
    detail::json_const_iterator it;
    if (detail::FindMember(v, "asset", it) &&
        detail::IsObject(detail::GetValue(it))) {
      auto &itObj = detail::GetValue(it);
      detail::json_const_iterator version_it;
      std::string versionStr;
      if (detail::FindMember(itObj, "version", version_it) &&
          detail::GetString(detail::GetValue(version_it), versionStr)) {
        version_found = true;
      }
    }
    if (version_found) {
      // OK
    } else if (check_sections & REQUIRE_VERSION) {
      if (err) {
        (*err) += "\"asset\" object not found in .gltf or not an object type\n";
      }
      return false;
    }
  }

  // scene is not mandatory.
  // FIXME Maybe a better way to handle it than removing the code

  auto IsArrayMemberPresent = [](const detail::json &_v,
                                 const char *name) -> bool {
    detail::json_const_iterator it;
    return detail::FindMember(_v, name, it) &&
           detail::IsArray(detail::GetValue(it));
  };

  {
    if ((check_sections & REQUIRE_SCENES) &&
        !IsArrayMemberPresent(v, "scenes")) {
      if (err) {
        (*err) += "\"scenes\" object not found in .gltf or not an array type\n";
      }
      return false;
    }
  }

  {
    if ((check_sections & REQUIRE_NODES) && !IsArrayMemberPresent(v, "nodes")) {
      if (err) {
        (*err) += "\"nodes\" object not found in .gltf\n";
      }
      return false;
    }
  }

  {
    if ((check_sections & REQUIRE_ACCESSORS) &&
        !IsArrayMemberPresent(v, "accessors")) {
      if (err) {
        (*err) += "\"accessors\" object not found in .gltf\n";
      }
      return false;
    }
  }

  {
    if ((check_sections & REQUIRE_BUFFERS) &&
        !IsArrayMemberPresent(v, "buffers")) {
      if (err) {
        (*err) += "\"buffers\" object not found in .gltf\n";
      }
      return false;
    }
  }

  {
    if ((check_sections & REQUIRE_BUFFER_VIEWS) &&
        !IsArrayMemberPresent(v, "bufferViews")) {
      if (err) {
        (*err) += "\"bufferViews\" object not found in .gltf\n";
      }
      return false;
    }
  }

  model->buffers.clear();
  model->bufferViews.clear();
  model->accessors.clear();
  model->meshes.clear();
  model->cameras.clear();
  model->nodes.clear();
  model->extensionsUsed.clear();
  model->extensionsRequired.clear();
  model->extensions.clear();
  model->defaultScene = -1;

  // 1. Parse Asset
  {
    detail::json_const_iterator it;
    if (detail::FindMember(v, "asset", it) &&
        detail::IsObject(detail::GetValue(it))) {
      const detail::json &root = detail::GetValue(it);

      ParseAsset(&model->asset, err, root,
                 store_original_json_for_extras_and_extensions_);
    }
  }

  using detail::ForEachInArray;

  // 2. Parse extensionUsed
  {
    ForEachInArray(v, "extensionsUsed", [&](const detail::json &o) {
      std::string str;
      detail::GetString(o, str);
      model->extensionsUsed.emplace_back(std::move(str));
      return true;
    });
  }

  {
    ForEachInArray(v, "extensionsRequired", [&](const detail::json &o) {
      std::string str;
      detail::GetString(o, str);
      model->extensionsRequired.emplace_back(std::move(str));
      return true;
    });
  }

  // 3. Parse Buffer
  {
    bool success = ForEachInArray(v, "buffers", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`buffers' does not contain an JSON object.";
        }
        return false;
      }
      Buffer buffer;
      if (!ParseBuffer(&buffer, err, o,
                       store_original_json_for_extras_and_extensions_, &fs,
                       &uri_cb, base_dir, max_external_file_size_, is_binary_,
                       bin_data_, bin_size_)) {
        return false;
      }

      model->buffers.emplace_back(std::move(buffer));
      return true;
    });

    if (!success) {
      return false;
    }
  }
  // 4. Parse BufferView
  {
    bool success = ForEachInArray(v, "bufferViews", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`bufferViews' does not contain an JSON object.";
        }
        return false;
      }
      BufferView bufferView;
      if (!ParseBufferView(&bufferView, err, o,
                           store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->bufferViews.emplace_back(std::move(bufferView));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 5. Parse Accessor
  {
    bool success = ForEachInArray(v, "accessors", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`accessors' does not contain an JSON object.";
        }
        return false;
      }
      Accessor accessor;
      if (!ParseAccessor(&accessor, err, o,
                         store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->accessors.emplace_back(std::move(accessor));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 6. Parse Mesh
  {
    bool success = ForEachInArray(v, "meshes", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`meshes' does not contain an JSON object.";
        }
        return false;
      }
      Mesh mesh;
      if (!ParseMesh(&mesh, model, err, warn, o,
                     store_original_json_for_extras_and_extensions_,
                     strictness_)) {
        return false;
      }

      model->meshes.emplace_back(std::move(mesh));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // Assign missing bufferView target types
  // - Look for missing Mesh indices
  // - Look for missing Mesh attributes
  for (auto &mesh : model->meshes) {
    for (auto &primitive : mesh.primitives) {
      if (primitive.indices >
          -1)  // has indices from parsing step, must be Element Array Buffer
      {
        if (size_t(primitive.indices) >= model->accessors.size()) {
          if (err) {
            (*err) += "primitive indices accessor out of bounds";
          }
          return false;
        }

        const auto bufferView =
            model->accessors[size_t(primitive.indices)].bufferView;
        if (bufferView < 0) {
          // skip, bufferView could be null(-1) for certain extensions
        } else if (size_t(bufferView) >= model->bufferViews.size()) {
          if (err) {
            (*err) += "accessor[" + std::to_string(primitive.indices) +
                      "] invalid bufferView";
          }
          return false;
        } else {
          model->bufferViews[size_t(bufferView)].target =
              TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
          // we could optionally check if accessors' bufferView type is Scalar, as
          // it should be
        }
      }

      for (auto &attribute : primitive.attributes) {
        const auto accessorsIndex = size_t(attribute.second);
        if (accessorsIndex < model->accessors.size()) {
          const auto bufferView = model->accessors[accessorsIndex].bufferView;
          // bufferView could be null(-1) for sparse morph target
          if (bufferView >= 0 && bufferView < (int)model->bufferViews.size()) {
            model->bufferViews[size_t(bufferView)].target =
                TINYGLTF_TARGET_ARRAY_BUFFER;
          }
        }
      }

      for (auto &target : primitive.targets) {
        for (auto &attribute : target) {
          const auto accessorsIndex = size_t(attribute.second);
          if (accessorsIndex < model->accessors.size()) {
            const auto bufferView = model->accessors[accessorsIndex].bufferView;
            // bufferView could be null(-1) for sparse morph target
            if (bufferView >= 0 &&
                bufferView < (int)model->bufferViews.size()) {
              model->bufferViews[size_t(bufferView)].target =
                  TINYGLTF_TARGET_ARRAY_BUFFER;
            }
          }
        }
      }
    }
  }

  // 7. Parse Node
  {
    bool success = ForEachInArray(v, "nodes", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`nodes' does not contain an JSON object.";
        }
        return false;
      }
      Node node;
      if (!ParseNode(&node, err, o,
                     store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->nodes.emplace_back(std::move(node));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 8. Parse scenes.
  {
    bool success = ForEachInArray(v, "scenes", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`scenes' does not contain an JSON object.";
        }
        return false;
      }

      Scene scene;
      if (!ParseScene(&scene, err, o,
                      store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->scenes.emplace_back(std::move(scene));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 9. Parse default scenes.
  {
    detail::json_const_iterator rootIt;
    int iVal;
    if (detail::FindMember(v, "scene", rootIt) &&
        detail::GetInt(detail::GetValue(rootIt), iVal)) {
      model->defaultScene = iVal;
    }
  }

  // 10. Parse Material
  {
    bool success = ForEachInArray(v, "materials", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`materials' does not contain an JSON object.";
        }
        return false;
      }
      Material material;
      ParseStringProperty(&material.name, err, o, "name", false);

      if (!ParseMaterial(&material, err, warn, o,
                         store_original_json_for_extras_and_extensions_,
                         strictness_)) {
        return false;
      }

      model->materials.emplace_back(std::move(material));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 11. Parse Image
  void *load_image_user_data{nullptr};

  LoadImageDataOption load_image_option;

  if (user_image_loader_) {
    // Use user supplied pointer
    load_image_user_data = load_image_user_data_;
  } else {
    load_image_option.preserve_channels = preserve_image_channels_;
    load_image_user_data = reinterpret_cast<void *>(&load_image_option);
  }

  {
    int idx = 0;
    bool success = ForEachInArray(v, "images", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "image[" + std::to_string(idx) + "] is not a JSON object.";
        }
        return false;
      }
      Image image;
      if (!ParseImage(&image, idx, err, warn, o,
                      store_original_json_for_extras_and_extensions_, base_dir,
                      max_external_file_size_, &fs, &uri_cb,
                      &this->LoadImageData, load_image_user_data)) {
        return false;
      }

      if (image.bufferView != -1) {
        // Load image from the buffer view.
        if (size_t(image.bufferView) >= model->bufferViews.size()) {
          if (err) {
            std::stringstream ss;
            ss << "image[" << idx << "] bufferView \"" << image.bufferView
               << "\" not found in the scene." << std::endl;
            (*err) += ss.str();
          }
          return false;
        }

        const BufferView &bufferView =
            model->bufferViews[size_t(image.bufferView)];
        if (size_t(bufferView.buffer) >= model->buffers.size()) {
          if (err) {
            std::stringstream ss;
            ss << "image[" << idx << "] buffer \"" << bufferView.buffer
               << "\" not found in the scene." << std::endl;
            (*err) += ss.str();
          }
          return false;
        }
        const Buffer &buffer = model->buffers[size_t(bufferView.buffer)];

        if (*LoadImageData == nullptr) {
          if (err) {
            (*err) += "No LoadImageData callback specified.\n";
          }
          return false;
        }
        bool ret = LoadImageData(
            &image, idx, err, warn, image.width, image.height,
            &buffer.data[bufferView.byteOffset],
            static_cast<int>(bufferView.byteLength), load_image_user_data);
        if (!ret) {
          return false;
        }
      }

      model->images.emplace_back(std::move(image));
      ++idx;
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 12. Parse Texture
  {
    bool success = ForEachInArray(v, "textures", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`textures' does not contain an JSON object.";
        }
        return false;
      }
      Texture texture;
      if (!ParseTexture(&texture, err, o,
                        store_original_json_for_extras_and_extensions_,
                        base_dir)) {
        return false;
      }

      model->textures.emplace_back(std::move(texture));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 13. Parse Animation
  {
    bool success = ForEachInArray(v, "animations", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`animations' does not contain an JSON object.";
        }
        return false;
      }
      Animation animation;
      if (!ParseAnimation(&animation, err, o,
                          store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->animations.emplace_back(std::move(animation));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 14. Parse Skin
  {
    bool success = ForEachInArray(v, "skins", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`skins' does not contain an JSON object.";
        }
        return false;
      }
      Skin skin;
      if (!ParseSkin(&skin, err, o,
                     store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->skins.emplace_back(std::move(skin));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 15. Parse Sampler
  {
    bool success = ForEachInArray(v, "samplers", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`samplers' does not contain an JSON object.";
        }
        return false;
      }
      Sampler sampler;
      if (!ParseSampler(&sampler, err, o,
                        store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->samplers.emplace_back(std::move(sampler));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 16. Parse Camera
  {
    bool success = ForEachInArray(v, "cameras", [&](const detail::json &o) {
      if (!detail::IsObject(o)) {
        if (err) {
          (*err) += "`cameras' does not contain an JSON object.";
        }
        return false;
      }
      Camera camera;
      if (!ParseCamera(&camera, err, o,
                       store_original_json_for_extras_and_extensions_)) {
        return false;
      }

      model->cameras.emplace_back(std::move(camera));
      return true;
    });

    if (!success) {
      return false;
    }
  }

  // 17. Parse Extras & Extensions
  ParseExtrasAndExtensions(model, err, v,
                           store_original_json_for_extras_and_extensions_);

  // 18. Specific extension implementations
  {
    detail::json_const_iterator rootIt;
    if (detail::FindMember(v, "extensions", rootIt) &&
        detail::IsObject(detail::GetValue(rootIt))) {
      const detail::json &root = detail::GetValue(rootIt);

      detail::json_const_iterator it(detail::ObjectBegin(root));
      detail::json_const_iterator itEnd(detail::ObjectEnd(root));
      for (; it != itEnd; ++it) {
        // parse KHR_lights_punctual extension
        std::string key(detail::GetKey(it));
        if ((key == "KHR_lights_punctual") &&
            detail::IsObject(detail::GetValue(it))) {
          const detail::json &object = detail::GetValue(it);
          detail::json_const_iterator itLight;
          if (detail::FindMember(object, "lights", itLight)) {
            const detail::json &lights = detail::GetValue(itLight);
            if (!detail::IsArray(lights)) {
              continue;
            }

            auto arrayIt(detail::ArrayBegin(lights));
            auto arrayItEnd(detail::ArrayEnd(lights));
            for (; arrayIt != arrayItEnd; ++arrayIt) {
              Light light;
              if (!ParseLight(&light, err, *arrayIt,
                              store_original_json_for_extras_and_extensions_)) {
                return false;
              }
              model->lights.emplace_back(std::move(light));
            }
          }
        }
        // parse KHR_audio extension
        if ((key == "KHR_audio") && detail::IsObject(detail::GetValue(it))) {
          const detail::json &object = detail::GetValue(it);
          detail::json_const_iterator itKhrAudio;
          if (detail::FindMember(object, "emitters", itKhrAudio)) {
            const detail::json &emitters = detail::GetValue(itKhrAudio);
            if (!detail::IsArray(emitters)) {
              continue;
            }

            auto arrayIt(detail::ArrayBegin(emitters));
            auto arrayItEnd(detail::ArrayEnd(emitters));
            for (; arrayIt != arrayItEnd; ++arrayIt) {
              AudioEmitter emitter;
              if (!ParseAudioEmitter(
                      &emitter, err, *arrayIt,
                      store_original_json_for_extras_and_extensions_)) {
                return false;
              }
              model->audioEmitters.emplace_back(std::move(emitter));
            }
          }

          if (detail::FindMember(object, "sources", itKhrAudio)) {
            const detail::json &sources = detail::GetValue(itKhrAudio);
            if (!detail::IsArray(sources)) {
              continue;
            }

            auto arrayIt(detail::ArrayBegin(sources));
            auto arrayItEnd(detail::ArrayEnd(sources));
            for (; arrayIt != arrayItEnd; ++arrayIt) {
              AudioSource source;
              if (!ParseAudioSource(
                      &source, err, *arrayIt,
                      store_original_json_for_extras_and_extensions_)) {
                return false;
              }
              model->audioSources.emplace_back(std::move(source));
            }
          }
        }
      }
    }
  }

  return true;
}

bool TinyGLTF::LoadASCIIFromString(Model *model, std::string *err,
                                   std::string *warn, const char *str,
                                   unsigned int length,
                                   const std::string &base_dir,
                                   unsigned int check_sections) {
  is_binary_ = false;
  bin_data_ = nullptr;
  bin_size_ = 0;

  return LoadFromString(model, err, warn, str, length, base_dir,
                        check_sections);
}

bool TinyGLTF::LoadASCIIFromFile(Model *model, std::string *err,
                                 std::string *warn, const std::string &filename,
                                 unsigned int check_sections) {
  std::stringstream ss;

  if (fs.ReadWholeFile == nullptr) {
    // Programmer error, assert() ?
    ss << "Failed to read file: " << filename
       << ": one or more FS callback not set" << std::endl;
    if (err) {
      (*err) = ss.str();
    }
    return false;
  }

  std::vector<unsigned char> data;
  std::string fileerr;
  bool fileread = fs.ReadWholeFile(&data, &fileerr, filename, fs.user_data);
  if (!fileread) {
    ss << "Failed to read file: " << filename << ": " << fileerr << std::endl;
    if (err) {
      (*err) = ss.str();
    }
    return false;
  }

  size_t sz = data.size();
  if (sz == 0) {
    if (err) {
      (*err) = "Empty file.";
    }
    return false;
  }

  std::string basedir = GetBaseDir(filename);

  bool ret = LoadASCIIFromString(
      model, err, warn, reinterpret_cast<const char *>(&data.at(0)),
      static_cast<unsigned int>(data.size()), basedir, check_sections);

  return ret;
}

bool TinyGLTF::LoadBinaryFromMemory(Model *model, std::string *err,
                                    std::string *warn,
                                    const unsigned char *bytes,
                                    unsigned int size,
                                    const std::string &base_dir,
                                    unsigned int check_sections) {
  if (size < 20) {
    if (err) {
      (*err) = "Too short data size for glTF Binary.";
    }
    return false;
  }

  if (bytes[0] == 'g' && bytes[1] == 'l' && bytes[2] == 'T' &&
      bytes[3] == 'F') {
    // ok
  } else {
    if (err) {
      (*err) = "Invalid magic.";
    }
    return false;
  }

  unsigned int version;        // 4 bytes
  unsigned int length;         // 4 bytes
  unsigned int chunk0_length;  // 4 bytes
  unsigned int chunk0_format;  // 4 bytes;

  memcpy(&version, bytes + 4, 4);
  swap4(&version);
  memcpy(&length, bytes + 8, 4); // Total glb size, including header and all chunks.
  swap4(&length);
  memcpy(&chunk0_length, bytes + 12, 4);  // JSON data length
  swap4(&chunk0_length);
  memcpy(&chunk0_format, bytes + 16, 4);
  swap4(&chunk0_format);

  // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-gltf-layout
  //
  // In case the Bin buffer is not present, the size is exactly 20 + size of
  // JSON contents,
  // so use "greater than" operator.
  //
  // https://github.com/syoyo/tinygltf/issues/372
  // Use 64bit uint to avoid integer overflow.
  uint64_t header_and_json_size = 20ull + uint64_t(chunk0_length);

  if (header_and_json_size > (std::numeric_limits<uint32_t>::max)()) {
    // Do not allow 4GB or more GLB data.
    if (err) {
      (*err) = "Invalid glTF binary. GLB data exceeds 4GB.";
    }
    return false;
  }

  if ((header_and_json_size > uint64_t(size)) || (chunk0_length < 1) ||
      (length > size) || (header_and_json_size > uint64_t(length)) ||
      (chunk0_format != 0x4E4F534A)) {  // 0x4E4F534A = JSON format.
    if (err) {
      (*err) = "Invalid glTF binary.";
    }
    return false;
  }

  // Padding check
  // The start and the end of each chunk must be aligned to a 4-byte boundary.
  // No padding check for chunk0 start since its 4byte-boundary is ensured.
  if ((header_and_json_size % 4) != 0) {
    if (err) {
      (*err) = "JSON Chunk end does not aligned to a 4-byte boundary.";
    }
    return false;
  }

  // std::cout << "header_and_json_size = " << header_and_json_size << "\n";
  // std::cout << "length = " << length << "\n";

  // Chunk1(BIN) data
  // The spec says: When the binary buffer is empty or when it is stored by
  // other means, this chunk SHOULD be omitted. So when header + JSON data ==
  // binary size, Chunk1 is omitted.
  if (header_and_json_size == uint64_t(length)) {
    bin_data_ = nullptr;
    bin_size_ = 0;
  } else {
    // Read Chunk1 info(BIN data)
    //
    // issue-440:
    // 'SHOULD' in glTF spec means 'RECOMMENDED',
    // So there is a situation that Chunk1(BIN) is composed of zero-sized BIN data
    // (chunksize(0) + binformat(BIN) = 8bytes).
    // 
    if ((header_and_json_size + 8ull) > uint64_t(length)) {
      if (err) {
        (*err) =
            "Insufficient storage space for Chunk1(BIN data). At least Chunk1 "
            "Must have 8 or more bytes, but got " +
            std::to_string((header_and_json_size + 8ull) - uint64_t(length)) +
            ".\n";
      }
      return false;
    }

    unsigned int chunk1_length{0};  // 4 bytes
    unsigned int chunk1_format{0};  // 4 bytes;
    memcpy(&chunk1_length, bytes + header_and_json_size,
           4);  // Bin data length
    swap4(&chunk1_length);
    memcpy(&chunk1_format, bytes + header_and_json_size + 4, 4);
    swap4(&chunk1_format);

    if (chunk1_format != 0x004e4942) {
      if (err) {
        (*err) = "Invalid chunkType for Chunk1.";
      }
      return false;
    }

    if (chunk1_length == 0) {

      if (header_and_json_size + 8 > uint64_t(length)) {
        if (err) {
          (*err) = "BIN Chunk header location exceeds the GLB size.";
        }
        return false;
      }

      bin_data_ = nullptr;

    } else {

      // When BIN chunk size is not zero, at least Chunk1 should have 12 bytes(8 bytes(header) + 4 bytes(bin
      // payload could be 1~3 bytes, but need to be aligned to 4 bytes)

      if (chunk1_length < 4) {
        if (err) {
          (*err) = "Insufficient Chunk1(BIN) data size.";
        }
        return false;
      }

      if ((chunk1_length % 4) != 0) {
        if (strictness_==ParseStrictness::Permissive) {
          if (warn) {
            (*warn) += "BIN Chunk end is not aligned to a 4-byte boundary.\n";
          }
        }
        else {
          if (err) {
            (*err) = "BIN Chunk end is not aligned to a 4-byte boundary.";
          }
          return false;
        }
      }

      // +8 chunk1 header size.
      if (uint64_t(chunk1_length) + header_and_json_size + 8 > uint64_t(length)) {
        if (err) {
          (*err) = "BIN Chunk data length exceeds the GLB size.";
        }
        return false;
      }

      bin_data_ = bytes + header_and_json_size +
                  8;  // 4 bytes (bin_buffer_length) + 4 bytes(bin_buffer_format)
    }

    bin_size_ = size_t(chunk1_length);
  }

  is_binary_ = true;

  bool ret = LoadFromString(model, err, warn,
                            reinterpret_cast<const char *>(&bytes[20]),
                            chunk0_length, base_dir, check_sections);
  if (!ret) {
    return ret;
  }

  return true;
}

bool TinyGLTF::LoadBinaryFromFile(Model *model, std::string *err,
                                  std::string *warn,
                                  const std::string &filename,
                                  unsigned int check_sections) {
  std::stringstream ss;

  if (fs.ReadWholeFile == nullptr) {
    // Programmer error, assert() ?
    ss << "Failed to read file: " << filename
       << ": one or more FS callback not set" << std::endl;
    if (err) {
      (*err) = ss.str();
    }
    return false;
  }

  std::vector<unsigned char> data;
  std::string fileerr;
  bool fileread = fs.ReadWholeFile(&data, &fileerr, filename, fs.user_data);
  if (!fileread) {
    ss << "Failed to read file: " << filename << ": " << fileerr << std::endl;
    if (err) {
      (*err) = ss.str();
    }
    return false;
  }

  std::string basedir = GetBaseDir(filename);

  bool ret = LoadBinaryFromMemory(model, err, warn, &data.at(0),
                                  static_cast<unsigned int>(data.size()),
                                  basedir, check_sections);

  return ret;
}

///////////////////////
// GLTF Serialization
///////////////////////
namespace detail {
detail::json JsonFromString(const char *s) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return detail::json(s, detail::GetAllocator());
#else
  return detail::json(s);
#endif
}

void JsonAssign(detail::json &dest, const detail::json &src) {
#ifdef TINYGLTF_USE_RAPIDJSON
  dest.CopyFrom(src, detail::GetAllocator());
#else
  dest = src;
#endif
}

void JsonAddMember(detail::json &o, const char *key, detail::json &&value) {
#ifdef TINYGLTF_USE_RAPIDJSON
  if (!o.IsObject()) {
    o.SetObject();
  }

  // Issue 420.
  // AddMember may create duplicated key, so use [] API when a key already
  // exists.
  // https://github.com/Tencent/rapidjson/issues/771#issuecomment-254386863
  detail::json_const_iterator it;
  if (detail::FindMember(o, key, it)) {
    o[key] = std::move(value);  // replace
  } else {
    o.AddMember(detail::json(key, detail::GetAllocator()), std::move(value),
                detail::GetAllocator());
  }
#else
  o[key] = std::move(value);
#endif
}

void JsonPushBack(detail::json &o, detail::json &&value) {
#ifdef TINYGLTF_USE_RAPIDJSON
  o.PushBack(std::move(value), detail::GetAllocator());
#else
  o.push_back(std::move(value));
#endif
}

bool JsonIsNull(const detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  return o.IsNull();
#else
  return o.is_null();
#endif
}

void JsonSetObject(detail::json &o) {
#ifdef TINYGLTF_USE_RAPIDJSON
  o.SetObject();
#else
  o = o.object({});
#endif
}

void JsonReserveArray(detail::json &o, size_t s) {
#ifdef TINYGLTF_USE_RAPIDJSON
  o.SetArray();
  o.Reserve(static_cast<rapidjson::SizeType>(s), detail::GetAllocator());
#endif
  (void)(o);
  (void)(s);
}
}  // namespace detail

// typedef std::pair<std::string, detail::json> json_object_pair;

template <typename T>
static void SerializeNumberProperty(const std::string &key, T number,
                                    detail::json &obj) {
  // obj.insert(
  //    json_object_pair(key, detail::json(static_cast<double>(number))));
  // obj[key] = static_cast<double>(number);
  detail::JsonAddMember(obj, key.c_str(), detail::json(number));
}

#ifdef TINYGLTF_USE_RAPIDJSON
template <>
void SerializeNumberProperty(const std::string &key, size_t number,
                             detail::json &obj) {
  detail::JsonAddMember(obj, key.c_str(),
                        detail::json(static_cast<uint64_t>(number)));
}
#endif

template <typename T>
static void SerializeNumberArrayProperty(const std::string &key,
                                         const std::vector<T> &value,
                                         detail::json &obj) {
  if (value.empty()) return;

  detail::json ary;
  detail::JsonReserveArray(ary, value.size());
  for (const auto &s : value) {
    detail::JsonPushBack(ary, detail::json(s));
  }
  detail::JsonAddMember(obj, key.c_str(), std::move(ary));
}

static void SerializeStringProperty(const std::string &key,
                                    const std::string &value,
                                    detail::json &obj) {
  detail::JsonAddMember(obj, key.c_str(),
                        detail::JsonFromString(value.c_str()));
}

static void SerializeStringArrayProperty(const std::string &key,
                                         const std::vector<std::string> &value,
                                         detail::json &obj) {
  detail::json ary;
  detail::JsonReserveArray(ary, value.size());
  for (auto &s : value) {
    detail::JsonPushBack(ary, detail::JsonFromString(s.c_str()));
  }
  detail::JsonAddMember(obj, key.c_str(), std::move(ary));
}

static bool ValueToJson(const Value &value, detail::json *ret) {
  detail::json obj;
#ifdef TINYGLTF_USE_RAPIDJSON
  switch (value.Type()) {
    case REAL_TYPE:
      obj.SetDouble(value.Get<double>());
      break;
    case INT_TYPE:
      obj.SetInt(value.Get<int>());
      break;
    case BOOL_TYPE:
      obj.SetBool(value.Get<bool>());
      break;
    case STRING_TYPE:
      obj.SetString(value.Get<std::string>().c_str(), detail::GetAllocator());
      break;
    case ARRAY_TYPE: {
      obj.SetArray();
      obj.Reserve(static_cast<rapidjson::SizeType>(value.ArrayLen()),
                  detail::GetAllocator());
      for (unsigned int i = 0; i < value.ArrayLen(); ++i) {
        Value elementValue = value.Get(int(i));
        detail::json elementJson;
        if (ValueToJson(value.Get(int(i)), &elementJson))
          obj.PushBack(std::move(elementJson), detail::GetAllocator());
      }
      break;
    }
    case BINARY_TYPE:
      // TODO
      // obj = detail::json(value.Get<std::vector<unsigned char>>());
      return false;
      break;
    case OBJECT_TYPE: {
      obj.SetObject();
      Value::Object objMap = value.Get<Value::Object>();
      for (auto &it : objMap) {
        detail::json elementJson;
        if (ValueToJson(it.second, &elementJson)) {
          obj.AddMember(detail::json(it.first.c_str(), detail::GetAllocator()),
                        std::move(elementJson), detail::GetAllocator());
        }
      }
      break;
    }
    case NULL_TYPE:
    default:
      return false;
  }
#else
  switch (value.Type()) {
    case REAL_TYPE:
      obj = detail::json(value.Get<double>());
      break;
    case INT_TYPE:
      obj = detail::json(value.Get<int>());
      break;
    case BOOL_TYPE:
      obj = detail::json(value.Get<bool>());
      break;
    case STRING_TYPE:
      obj = detail::json(value.Get<std::string>());
      break;
    case ARRAY_TYPE: {
      for (unsigned int i = 0; i < value.ArrayLen(); ++i) {
        Value elementValue = value.Get(int(i));
        detail::json elementJson;
        if (ValueToJson(value.Get(int(i)), &elementJson))
          obj.push_back(elementJson);
      }
      break;
    }
    case BINARY_TYPE:
      // TODO
      // obj = json(value.Get<std::vector<unsigned char>>());
      return false;
      break;
    case OBJECT_TYPE: {
      Value::Object objMap = value.Get<Value::Object>();
      for (auto &it : objMap) {
        detail::json elementJson;
        if (ValueToJson(it.second, &elementJson)) obj[it.first] = elementJson;
      }
      break;
    }
    case NULL_TYPE:
    default:
      return false;
  }
#endif
  if (ret) *ret = std::move(obj);
  return true;
}

static void SerializeValue(const std::string &key, const Value &value,
                           detail::json &obj) {
  detail::json ret;
  if (ValueToJson(value, &ret)) {
    detail::JsonAddMember(obj, key.c_str(), std::move(ret));
  }
}

static void SerializeGltfBufferData(const std::vector<unsigned char> &data,
                                    detail::json &o) {
  std::string header = "data:application/octet-stream;base64,";
  if (data.size() > 0) {
    std::string encodedData =
        base64_encode(&data[0], static_cast<unsigned int>(data.size()));
    SerializeStringProperty("uri", header + encodedData, o);
  } else {
    // Issue #229
    // size 0 is allowed. Just emit mime header.
    SerializeStringProperty("uri", header, o);
  }
}

static bool SerializeGltfBufferData(const std::vector<unsigned char> &data,
                                    const std::string &binFilename) {
#ifdef _WIN32
#if defined(__GLIBCXX__)  // mingw
  int file_descriptor = _wopen(UTF8ToWchar(binFilename).c_str(),
                               _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY);
  __gnu_cxx::stdio_filebuf<char> wfile_buf(
      file_descriptor, std::ios_base::out | std::ios_base::binary);
  std::ostream output(&wfile_buf);
  if (!wfile_buf.is_open()) return false;
#elif defined(_MSC_VER)
  std::ofstream output(UTF8ToWchar(binFilename).c_str(), std::ofstream::binary);
  if (!output.is_open()) return false;
#else
  std::ofstream output(binFilename.c_str(), std::ofstream::binary);
  if (!output.is_open()) return false;
#endif
#else
  std::ofstream output(binFilename.c_str(), std::ofstream::binary);
  if (!output.is_open()) return false;
#endif
  if (data.size() > 0) {
    output.write(reinterpret_cast<const char *>(&data[0]),
                 std::streamsize(data.size()));
  } else {
    // Issue #229
    // size 0 will be still valid buffer data.
    // write empty file.
  }
  return true;
}

#if 0  // FIXME(syoyo): not used. will be removed in the future release.
static void SerializeParameterMap(ParameterMap &param, detail::json &o) {
  for (ParameterMap::iterator paramIt = param.begin(); paramIt != param.end();
       ++paramIt) {
    if (paramIt->second.number_array.size()) {
      SerializeNumberArrayProperty<double>(paramIt->first,
                                           paramIt->second.number_array, o);
    } else if (paramIt->second.json_double_value.size()) {
      detail::json json_double_value;
      for (std::map<std::string, double>::iterator it =
               paramIt->second.json_double_value.begin();
           it != paramIt->second.json_double_value.end(); ++it) {
        if (it->first == "index") {
          json_double_value[it->first] = paramIt->second.TextureIndex();
        } else {
          json_double_value[it->first] = it->second;
        }
      }

      o[paramIt->first] = json_double_value;
    } else if (!paramIt->second.string_value.empty()) {
      SerializeStringProperty(paramIt->first, paramIt->second.string_value, o);
    } else if (paramIt->second.has_number_value) {
      o[paramIt->first] = paramIt->second.number_value;
    } else {
      o[paramIt->first] = paramIt->second.bool_value;
    }
  }
}
#endif

static void SerializeExtensionMap(const ExtensionMap &extensions,
                                  detail::json &o) {
  if (!extensions.size()) return;

  detail::json extMap;
  for (ExtensionMap::const_iterator extIt = extensions.begin();
       extIt != extensions.end(); ++extIt) {
    // Allow an empty object for extension(#97)
    detail::json ret;
    bool isNull = true;
    if (ValueToJson(extIt->second, &ret)) {
      isNull = detail::JsonIsNull(ret);
      detail::JsonAddMember(extMap, extIt->first.c_str(), std::move(ret));
    }
    if (isNull) {
      if (!(extIt->first.empty())) {  // name should not be empty, but for sure
        // create empty object so that an extension name is still included in
        // json.
        detail::json empty;
        detail::JsonSetObject(empty);
        detail::JsonAddMember(extMap, extIt->first.c_str(), std::move(empty));
      }
    }
  }
  detail::JsonAddMember(o, "extensions", std::move(extMap));
}

static void SerializeExtras(const Value &extras, detail::json &o) {
  if (extras.Type() != NULL_TYPE) SerializeValue("extras", extras, o);
}

template <typename GltfType>
void SerializeExtrasAndExtensions(const GltfType &obj, detail::json &o) {
  SerializeExtensionMap(obj.extensions, o);
  SerializeExtras(obj.extras, o);
}

static void SerializeGltfAccessor(const Accessor &accessor, detail::json &o) {
  if (accessor.bufferView >= 0)
    SerializeNumberProperty<int>("bufferView", accessor.bufferView, o);

  if (accessor.byteOffset != 0)
    SerializeNumberProperty<size_t>("byteOffset", accessor.byteOffset, o);

  SerializeNumberProperty<int>("componentType", accessor.componentType, o);
  SerializeNumberProperty<size_t>("count", accessor.count, o);

  if ((accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) ||
      (accessor.componentType == TINYGLTF_COMPONENT_TYPE_DOUBLE)) {
    SerializeNumberArrayProperty<double>("min", accessor.minValues, o);
    SerializeNumberArrayProperty<double>("max", accessor.maxValues, o);
  } else {
    // Issue #301. Serialize as integer.
    // Assume int value is within [-2**31-1, 2**31-1]
    {
      std::vector<int> values;
      std::transform(accessor.minValues.begin(), accessor.minValues.end(),
                     std::back_inserter(values),
                     [](double v) { return static_cast<int>(v); });

      SerializeNumberArrayProperty<int>("min", values, o);
    }

    {
      std::vector<int> values;
      std::transform(accessor.maxValues.begin(), accessor.maxValues.end(),
                     std::back_inserter(values),
                     [](double v) { return static_cast<int>(v); });

      SerializeNumberArrayProperty<int>("max", values, o);
    }
  }

  if (accessor.normalized)
    SerializeValue("normalized", Value(accessor.normalized), o);
  std::string type;
  switch (accessor.type) {
    case TINYGLTF_TYPE_SCALAR:
      type = "SCALAR";
      break;
    case TINYGLTF_TYPE_VEC2:
      type = "VEC2";
      break;
    case TINYGLTF_TYPE_VEC3:
      type = "VEC3";
      break;
    case TINYGLTF_TYPE_VEC4:
      type = "VEC4";
      break;
    case TINYGLTF_TYPE_MAT2:
      type = "MAT2";
      break;
    case TINYGLTF_TYPE_MAT3:
      type = "MAT3";
      break;
    case TINYGLTF_TYPE_MAT4:
      type = "MAT4";
      break;
  }

  SerializeStringProperty("type", type, o);
  if (!accessor.name.empty()) SerializeStringProperty("name", accessor.name, o);

  SerializeExtrasAndExtensions(accessor, o);

  // sparse
  if (accessor.sparse.isSparse) {
    detail::json sparse;
    SerializeNumberProperty<int>("count", accessor.sparse.count, sparse);
    {
      detail::json indices;
      SerializeNumberProperty<int>("bufferView",
                                   accessor.sparse.indices.bufferView, indices);
      SerializeNumberProperty<size_t>("byteOffset",
                                   accessor.sparse.indices.byteOffset, indices);
      SerializeNumberProperty<int>(
          "componentType", accessor.sparse.indices.componentType, indices);
      SerializeExtrasAndExtensions(accessor.sparse.indices, indices);
      detail::JsonAddMember(sparse, "indices", std::move(indices));
    }
    {
      detail::json values;
      SerializeNumberProperty<int>("bufferView",
                                   accessor.sparse.values.bufferView, values);
      SerializeNumberProperty<size_t>("byteOffset",
                                   accessor.sparse.values.byteOffset, values);
      SerializeExtrasAndExtensions(accessor.sparse.values, values);
      detail::JsonAddMember(sparse, "values", std::move(values));
    }
    SerializeExtrasAndExtensions(accessor.sparse, sparse);
    detail::JsonAddMember(o, "sparse", std::move(sparse));
  }
}

static void SerializeGltfAnimationChannel(const AnimationChannel &channel,
                                          detail::json &o) {
  SerializeNumberProperty("sampler", channel.sampler, o);
  {
    detail::json target;

    if (channel.target_node >= 0) {
      SerializeNumberProperty("node", channel.target_node, target);
    }

    SerializeStringProperty("path", channel.target_path, target);

    SerializeExtensionMap(channel.target_extensions, target);
    SerializeExtras(channel.target_extras, target);

    detail::JsonAddMember(o, "target", std::move(target));
  }

  SerializeExtrasAndExtensions(channel, o);
}

static void SerializeGltfAnimationSampler(const AnimationSampler &sampler,
                                          detail::json &o) {
  SerializeNumberProperty("input", sampler.input, o);
  SerializeNumberProperty("output", sampler.output, o);
  SerializeStringProperty("interpolation", sampler.interpolation, o);

  SerializeExtrasAndExtensions(sampler, o);
}

static void SerializeGltfAnimation(const Animation &animation,
                                   detail::json &o) {
  if (!animation.name.empty())
    SerializeStringProperty("name", animation.name, o);

  {
    detail::json channels;
    detail::JsonReserveArray(channels, animation.channels.size());
    for (unsigned int i = 0; i < animation.channels.size(); ++i) {
      detail::json channel;
      AnimationChannel gltfChannel = animation.channels[i];
      SerializeGltfAnimationChannel(gltfChannel, channel);
      detail::JsonPushBack(channels, std::move(channel));
    }

    detail::JsonAddMember(o, "channels", std::move(channels));
  }

  {
    detail::json samplers;
    detail::JsonReserveArray(samplers, animation.samplers.size());
    for (unsigned int i = 0; i < animation.samplers.size(); ++i) {
      detail::json sampler;
      AnimationSampler gltfSampler = animation.samplers[i];
      SerializeGltfAnimationSampler(gltfSampler, sampler);
      detail::JsonPushBack(samplers, std::move(sampler));
    }
    detail::JsonAddMember(o, "samplers", std::move(samplers));
  }

  SerializeExtrasAndExtensions(animation, o);
}

static void SerializeGltfAsset(const Asset &asset, detail::json &o) {
  if (!asset.generator.empty()) {
    SerializeStringProperty("generator", asset.generator, o);
  }

  if (!asset.copyright.empty()) {
    SerializeStringProperty("copyright", asset.copyright, o);
  }

  auto version = asset.version;
  if (version.empty()) {
    // Just in case
    // `version` must be defined
    version = "2.0";
  }

  // TODO(syoyo): Do we need to check if `version` is greater or equal to 2.0?
  SerializeStringProperty("version", version, o);

  SerializeExtrasAndExtensions(asset, o);
}

static void SerializeGltfBufferBin(const Buffer &buffer, detail::json &o,
                                   std::vector<unsigned char> &binBuffer) {
  SerializeNumberProperty("byteLength", buffer.data.size(), o);
  binBuffer = buffer.data;

  if (buffer.name.size()) SerializeStringProperty("name", buffer.name, o);

  SerializeExtrasAndExtensions(buffer, o);
}

static void SerializeGltfBuffer(const Buffer &buffer, detail::json &o) {
  SerializeNumberProperty("byteLength", buffer.data.size(), o);
  SerializeGltfBufferData(buffer.data, o);

  if (buffer.name.size()) SerializeStringProperty("name", buffer.name, o);

  SerializeExtrasAndExtensions(buffer, o);
}

static bool SerializeGltfBuffer(const Buffer &buffer, detail::json &o,
                                const std::string &binFilename,
                                const std::string &binUri) {
  if (!SerializeGltfBufferData(buffer.data, binFilename)) return false;
  SerializeNumberProperty("byteLength", buffer.data.size(), o);
  SerializeStringProperty("uri", binUri, o);

  if (buffer.name.size()) SerializeStringProperty("name", buffer.name, o);

  SerializeExtrasAndExtensions(buffer, o);
  return true;
}

static void SerializeGltfBufferView(const BufferView &bufferView,
                                    detail::json &o) {
  SerializeNumberProperty("buffer", bufferView.buffer, o);
  SerializeNumberProperty<size_t>("byteLength", bufferView.byteLength, o);

  // byteStride is optional, minimum allowed is 4
  if (bufferView.byteStride >= 4) {
    SerializeNumberProperty<size_t>("byteStride", bufferView.byteStride, o);
  }
  // byteOffset is optional, default is 0
  if (bufferView.byteOffset > 0) {
    SerializeNumberProperty<size_t>("byteOffset", bufferView.byteOffset, o);
  }
  // Target is optional, check if it contains a valid value
  if (bufferView.target == TINYGLTF_TARGET_ARRAY_BUFFER ||
      bufferView.target == TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER) {
    SerializeNumberProperty("target", bufferView.target, o);
  }
  if (bufferView.name.size()) {
    SerializeStringProperty("name", bufferView.name, o);
  }

  SerializeExtrasAndExtensions(bufferView, o);
}

static void SerializeGltfImage(const Image &image, const std::string &uri,
                               detail::json &o) {
  // From 2.7.0, we look for `uri` parameter, not `Image.uri`
  // if uri is empty, the mimeType and bufferview should be set
  if (uri.empty()) {
    SerializeStringProperty("mimeType", image.mimeType, o);
    SerializeNumberProperty<int>("bufferView", image.bufferView, o);
  } else {
    SerializeStringProperty("uri", uri, o);
  }

  if (image.name.size()) {
    SerializeStringProperty("name", image.name, o);
  }

  SerializeExtrasAndExtensions(image, o);
}

static void SerializeGltfTextureInfo(const TextureInfo &texinfo,
                                     detail::json &o) {
  SerializeNumberProperty("index", texinfo.index, o);

  if (texinfo.texCoord != 0) {
    SerializeNumberProperty("texCoord", texinfo.texCoord, o);
  }

  SerializeExtrasAndExtensions(texinfo, o);
}

static void SerializeGltfNormalTextureInfo(const NormalTextureInfo &texinfo,
                                           detail::json &o) {
  SerializeNumberProperty("index", texinfo.index, o);

  if (texinfo.texCoord != 0) {
    SerializeNumberProperty("texCoord", texinfo.texCoord, o);
  }

  if (!TINYGLTF_DOUBLE_EQUAL(texinfo.scale, 1.0)) {
    SerializeNumberProperty("scale", texinfo.scale, o);
  }

  SerializeExtrasAndExtensions(texinfo, o);
}

static void SerializeGltfOcclusionTextureInfo(
    const OcclusionTextureInfo &texinfo, detail::json &o) {
  SerializeNumberProperty("index", texinfo.index, o);

  if (texinfo.texCoord != 0) {
    SerializeNumberProperty("texCoord", texinfo.texCoord, o);
  }

  if (!TINYGLTF_DOUBLE_EQUAL(texinfo.strength, 1.0)) {
    SerializeNumberProperty("strength", texinfo.strength, o);
  }

  SerializeExtrasAndExtensions(texinfo, o);
}

static void SerializeGltfPbrMetallicRoughness(const PbrMetallicRoughness &pbr,
                                              detail::json &o) {
  std::vector<double> default_baseColorFactor = {1.0, 1.0, 1.0, 1.0};
  if (!Equals(pbr.baseColorFactor, default_baseColorFactor)) {
    SerializeNumberArrayProperty<double>("baseColorFactor", pbr.baseColorFactor,
                                         o);
  }

  if (!TINYGLTF_DOUBLE_EQUAL(pbr.metallicFactor, 1.0)) {
    SerializeNumberProperty("metallicFactor", pbr.metallicFactor, o);
  }

  if (!TINYGLTF_DOUBLE_EQUAL(pbr.roughnessFactor, 1.0)) {
    SerializeNumberProperty("roughnessFactor", pbr.roughnessFactor, o);
  }

  if (pbr.baseColorTexture.index > -1) {
    detail::json texinfo;
    SerializeGltfTextureInfo(pbr.baseColorTexture, texinfo);
    detail::JsonAddMember(o, "baseColorTexture", std::move(texinfo));
  }

  if (pbr.metallicRoughnessTexture.index > -1) {
    detail::json texinfo;
    SerializeGltfTextureInfo(pbr.metallicRoughnessTexture, texinfo);
    detail::JsonAddMember(o, "metallicRoughnessTexture", std::move(texinfo));
  }

  SerializeExtrasAndExtensions(pbr, o);
}

static void SerializeGltfMaterial(const Material &material, detail::json &o) {
  if (material.name.size()) {
    SerializeStringProperty("name", material.name, o);
  }

  // QUESTION(syoyo): Write material parameters regardless of its default value?

  if (!TINYGLTF_DOUBLE_EQUAL(material.alphaCutoff, 0.5)) {
    SerializeNumberProperty("alphaCutoff", material.alphaCutoff, o);
  }

  if (material.alphaMode.compare("OPAQUE") != 0) {
    SerializeStringProperty("alphaMode", material.alphaMode, o);
  }

  if (material.doubleSided != false)
    detail::JsonAddMember(o, "doubleSided", detail::json(material.doubleSided));

  if (material.normalTexture.index > -1) {
    detail::json texinfo;
    SerializeGltfNormalTextureInfo(material.normalTexture, texinfo);
    detail::JsonAddMember(o, "normalTexture", std::move(texinfo));
  }

  if (material.occlusionTexture.index > -1) {
    detail::json texinfo;
    SerializeGltfOcclusionTextureInfo(material.occlusionTexture, texinfo);
    detail::JsonAddMember(o, "occlusionTexture", std::move(texinfo));
  }

  if (material.emissiveTexture.index > -1) {
    detail::json texinfo;
    SerializeGltfTextureInfo(material.emissiveTexture, texinfo);
    detail::JsonAddMember(o, "emissiveTexture", std::move(texinfo));
  }

  std::vector<double> default_emissiveFactor = {0.0, 0.0, 0.0};
  if (!Equals(material.emissiveFactor, default_emissiveFactor)) {
    SerializeNumberArrayProperty<double>("emissiveFactor",
                                         material.emissiveFactor, o);
  }

  {
    detail::json pbrMetallicRoughness;
    SerializeGltfPbrMetallicRoughness(material.pbrMetallicRoughness,
                                      pbrMetallicRoughness);
    // Issue 204
    // Do not serialize `pbrMetallicRoughness` if pbrMetallicRoughness has all
    // default values(json is null). Otherwise it will serialize to
    // `pbrMetallicRoughness : null`, which cannot be read by other glTF
    // importers (and validators).
    //
    if (!detail::JsonIsNull(pbrMetallicRoughness)) {
      detail::JsonAddMember(o, "pbrMetallicRoughness",
                            std::move(pbrMetallicRoughness));
    }
  }

#if 0  // legacy way. just for the record.
  if (material.values.size()) {
    detail::json pbrMetallicRoughness;
    SerializeParameterMap(material.values, pbrMetallicRoughness);
    detail::JsonAddMember(o, "pbrMetallicRoughness", std::move(pbrMetallicRoughness));
  }

  SerializeParameterMap(material.additionalValues, o);
#endif

  SerializeExtrasAndExtensions(material, o);

  // MSFT_lod
  if (!material.lods.empty()) {
    detail::json_iterator it;
    if (!detail::FindMember(o, "extensions", it)) {
      detail::json extensions;
      detail::JsonSetObject(extensions);
      detail::JsonAddMember(o, "extensions", std::move(extensions));
      detail::FindMember(o, "extensions", it);
    }
    auto &extensions = detail::GetValue(it);
    if (!detail::FindMember(extensions, "MSFT_lod", it)) {
      detail::json lod;
      detail::JsonSetObject(lod);
      detail::JsonAddMember(extensions, "MSFT_lod", std::move(lod));
      detail::FindMember(extensions, "MSFT_lod", it);
    }
    SerializeNumberArrayProperty<int>("ids", material.lods, detail::GetValue(it));
  } else {
    detail::json_iterator ext_it;
    if (detail::FindMember(o, "extensions", ext_it)) {
      auto &extensions = detail::GetValue(ext_it);
      detail::json_iterator lp_it;
      if (detail::FindMember(extensions, "MSFT_lod", lp_it)) {
        detail::Erase(extensions, lp_it);
      }
      if (detail::IsEmpty(extensions)) {
        detail::Erase(o, ext_it);
      }
    }
  }
}

static void SerializeGltfMesh(const Mesh &mesh, detail::json &o) {
  detail::json primitives;
  detail::JsonReserveArray(primitives, mesh.primitives.size());
  for (unsigned int i = 0; i < mesh.primitives.size(); ++i) {
    detail::json primitive;
    const Primitive &gltfPrimitive = mesh.primitives[i];  // don't make a copy
    {
      detail::json attributes;
      for (auto attrIt = gltfPrimitive.attributes.begin();
           attrIt != gltfPrimitive.attributes.end(); ++attrIt) {
        SerializeNumberProperty<int>(attrIt->first, attrIt->second, attributes);
      }

      detail::JsonAddMember(primitive, "attributes", std::move(attributes));
    }

    // Indices is optional
    if (gltfPrimitive.indices > -1) {
      SerializeNumberProperty<int>("indices", gltfPrimitive.indices, primitive);
    }
    // Material is optional
    if (gltfPrimitive.material > -1) {
      SerializeNumberProperty<int>("material", gltfPrimitive.material,
                                   primitive);
    }
    SerializeNumberProperty<int>("mode", gltfPrimitive.mode, primitive);

    // Morph targets
    if (gltfPrimitive.targets.size()) {
      detail::json targets;
      detail::JsonReserveArray(targets, gltfPrimitive.targets.size());
      for (unsigned int k = 0; k < gltfPrimitive.targets.size(); ++k) {
        detail::json targetAttributes;
        std::map<std::string, int> targetData = gltfPrimitive.targets[k];
        for (std::map<std::string, int>::iterator attrIt = targetData.begin();
             attrIt != targetData.end(); ++attrIt) {
          SerializeNumberProperty<int>(attrIt->first, attrIt->second,
                                       targetAttributes);
        }
        detail::JsonPushBack(targets, std::move(targetAttributes));
      }
      detail::JsonAddMember(primitive, "targets", std::move(targets));
    }

    SerializeExtrasAndExtensions(gltfPrimitive, primitive);

    detail::JsonPushBack(primitives, std::move(primitive));
  }

  detail::JsonAddMember(o, "primitives", std::move(primitives));

  if (mesh.weights.size()) {
    SerializeNumberArrayProperty<double>("weights", mesh.weights, o);
  }

  if (mesh.name.size()) {
    SerializeStringProperty("name", mesh.name, o);
  }

  SerializeExtrasAndExtensions(mesh, o);
}

static void SerializeSpotLight(const SpotLight &spot, detail::json &o) {
  SerializeNumberProperty("innerConeAngle", spot.innerConeAngle, o);
  SerializeNumberProperty("outerConeAngle", spot.outerConeAngle, o);
  SerializeExtrasAndExtensions(spot, o);
}

static void SerializeGltfLight(const Light &light, detail::json &o) {
  if (!light.name.empty()) SerializeStringProperty("name", light.name, o);
  SerializeNumberProperty("intensity", light.intensity, o);
  if (light.range > 0.0) {
    SerializeNumberProperty("range", light.range, o);
  }
  SerializeNumberArrayProperty("color", light.color, o);
  SerializeStringProperty("type", light.type, o);
  if (light.type == "spot") {
    detail::json spot;
    SerializeSpotLight(light.spot, spot);
    detail::JsonAddMember(o, "spot", std::move(spot));
  }
  SerializeExtrasAndExtensions(light, o);
}

static void SerializeGltfPositionalEmitter(const PositionalEmitter &positional,
                                           detail::json &o) {
  if (!TINYGLTF_DOUBLE_EQUAL(positional.coneInnerAngle, 6.283185307179586))
    SerializeNumberProperty("coneInnerAngle", positional.coneInnerAngle, o);
  if (!TINYGLTF_DOUBLE_EQUAL(positional.coneOuterAngle, 6.283185307179586))
    SerializeNumberProperty("coneOuterAngle", positional.coneOuterAngle, o);
  if (positional.coneOuterGain > 0.0)
    SerializeNumberProperty("coneOuterGain", positional.coneOuterGain, o);
  if (!TINYGLTF_DOUBLE_EQUAL(positional.maxDistance, 100.0))
    SerializeNumberProperty("maxDistance", positional.maxDistance, o);
  if (!TINYGLTF_DOUBLE_EQUAL(positional.refDistance, 1.0))
    SerializeNumberProperty("refDistance", positional.refDistance, o);
  if (!TINYGLTF_DOUBLE_EQUAL(positional.rolloffFactor, 1.0))
    SerializeNumberProperty("rolloffFactor", positional.rolloffFactor, o);

  SerializeExtrasAndExtensions(positional, o);
}

static void SerializeGltfAudioEmitter(const AudioEmitter &emitter,
                                      detail::json &o) {
  if (!emitter.name.empty()) SerializeStringProperty("name", emitter.name, o);
  if (!TINYGLTF_DOUBLE_EQUAL(emitter.gain, 1.0))
    SerializeNumberProperty("gain", emitter.gain, o);
  if (emitter.loop) SerializeNumberProperty("loop", emitter.loop, o);
  if (emitter.playing) SerializeNumberProperty("playing", emitter.playing, o);
  if (!emitter.type.empty()) SerializeStringProperty("type", emitter.type, o);
  if (!emitter.distanceModel.empty())
    SerializeStringProperty("distanceModel", emitter.distanceModel, o);
  if (emitter.type == "positional") {
    detail::json positional;
    SerializeGltfPositionalEmitter(emitter.positional, positional);
    detail::JsonAddMember(o, "positional", std::move(positional));
  }
  SerializeNumberProperty("source", emitter.source, o);
  SerializeExtrasAndExtensions(emitter, o);
}

static void SerializeGltfAudioSource(const AudioSource &source,
                                     detail::json &o) {
  std::string name;
  std::string uri;
  std::string mimeType;  // (required if no uri) ["audio/mp3", "audio/ogg",
                         // "audio/wav", "audio/m4a"]

  if (!source.name.empty()) SerializeStringProperty("name", source.name, o);
  if (source.uri.empty()) {
    SerializeStringProperty("mimeType", source.mimeType, o);
    SerializeNumberProperty<int>("bufferView", source.bufferView, o);
  } else {
    SerializeStringProperty("uri", source.uri, o);
  }
  SerializeExtrasAndExtensions(source, o);
}

static void SerializeGltfNode(const Node &node, detail::json &o) {
  if (node.translation.size() > 0) {
    SerializeNumberArrayProperty<double>("translation", node.translation, o);
  }
  if (node.rotation.size() > 0) {
    SerializeNumberArrayProperty<double>("rotation", node.rotation, o);
  }
  if (node.scale.size() > 0) {
    SerializeNumberArrayProperty<double>("scale", node.scale, o);
  }
  if (node.matrix.size() > 0) {
    SerializeNumberArrayProperty<double>("matrix", node.matrix, o);
  }
  if (node.mesh != -1) {
    SerializeNumberProperty<int>("mesh", node.mesh, o);
  }

  if (node.skin != -1) {
    SerializeNumberProperty<int>("skin", node.skin, o);
  }

  if (node.camera != -1) {
    SerializeNumberProperty<int>("camera", node.camera, o);
  }

  if (node.weights.size() > 0) {
    SerializeNumberArrayProperty<double>("weights", node.weights, o);
  }

  SerializeExtrasAndExtensions(node, o);

  // Note(agnat): If the asset was loaded from disk, the node may already
  // contain the KHR_lights_punctual extension. If it was constructed in
  // memory it does not. In any case we update the JSON property using
  // the value from the struct. Last, if the node does not have a light
  // reference but the extension is still present, we remove it.
  if (node.light != -1) {
    detail::json_iterator it;
    if (!detail::FindMember(o, "extensions", it)) {
      detail::json extensions;
      detail::JsonSetObject(extensions);
      detail::JsonAddMember(o, "extensions", std::move(extensions));
      detail::FindMember(o, "extensions", it);
    }
    auto &extensions = detail::GetValue(it);
    if (!detail::FindMember(extensions, "KHR_lights_punctual", it)) {
      detail::json lights_punctual;
      detail::JsonSetObject(lights_punctual);
      detail::JsonAddMember(extensions, "KHR_lights_punctual",
                            std::move(lights_punctual));
      detail::FindMember(extensions, "KHR_lights_punctual", it);
    }
    SerializeNumberProperty("light", node.light, detail::GetValue(it));
  } else {
    // node has no light ref (any longer)... so we clean up
    detail::json_iterator ext_it;
    if (detail::FindMember(o, "extensions", ext_it)) {
      auto &extensions = detail::GetValue(ext_it);
      detail::json_iterator lp_it;
      if (detail::FindMember(extensions, "KHR_lights_punctual", lp_it)) {
        detail::Erase(extensions, lp_it);
      }
      if (detail::IsEmpty(extensions)) {
        detail::Erase(o, ext_it);
      }
    }
  }

  // KHR_audio
  if (node.emitter != -1) {
    detail::json_iterator it;
    if (!detail::FindMember(o, "extensions", it)) {
      detail::json extensions;
      detail::JsonSetObject(extensions);
      detail::JsonAddMember(o, "extensions", std::move(extensions));
      detail::FindMember(o, "extensions", it);
    }
    auto &extensions = detail::GetValue(it);
    if (!detail::FindMember(extensions, "KHR_audio", it)) {
      detail::json audio;
      detail::JsonSetObject(audio);
      detail::JsonAddMember(extensions, "KHR_audio", std::move(audio));
      detail::FindMember(extensions, "KHR_audio", it);
    }
    SerializeNumberProperty("emitter", node.emitter, detail::GetValue(it));
  } else {
    detail::json_iterator ext_it;
    if (detail::FindMember(o, "extensions", ext_it)) {
      auto &extensions = detail::GetValue(ext_it);
      detail::json_iterator lp_it;
      if (detail::FindMember(extensions, "KHR_audio", lp_it)) {
        detail::Erase(extensions, lp_it);
      }
      if (detail::IsEmpty(extensions)) {
        detail::Erase(o, ext_it);
      }
    }
  }

  // MSFT_lod
  if (!node.lods.empty()) {
    detail::json_iterator it;
    if (!detail::FindMember(o, "extensions", it)) {
      detail::json extensions;
      detail::JsonSetObject(extensions);
      detail::JsonAddMember(o, "extensions", std::move(extensions));
      detail::FindMember(o, "extensions", it);
    }
    auto &extensions = detail::GetValue(it);
    if (!detail::FindMember(extensions, "MSFT_lod", it)) {
      detail::json lod;
      detail::JsonSetObject(lod);
      detail::JsonAddMember(extensions, "MSFT_lod", std::move(lod));
      detail::FindMember(extensions, "MSFT_lod", it);
    }
    SerializeNumberArrayProperty<int>("ids", node.lods, detail::GetValue(it));
  } else {
    detail::json_iterator ext_it;
    if (detail::FindMember(o, "extensions", ext_it)) {
      auto &extensions = detail::GetValue(ext_it);
      detail::json_iterator lp_it;
      if (detail::FindMember(extensions, "MSFT_lod", lp_it)) {
        detail::Erase(extensions, lp_it);
      }
      if (detail::IsEmpty(extensions)) {
        detail::Erase(o, ext_it);
      }
    }
  }

  if (!node.name.empty()) SerializeStringProperty("name", node.name, o);
  SerializeNumberArrayProperty<int>("children", node.children, o);
}

static void SerializeGltfSampler(const Sampler &sampler, detail::json &o) {
  if (!sampler.name.empty()) {
    SerializeStringProperty("name", sampler.name, o);
  }
  if (sampler.magFilter != -1) {
    SerializeNumberProperty("magFilter", sampler.magFilter, o);
  }
  if (sampler.minFilter != -1) {
    SerializeNumberProperty("minFilter", sampler.minFilter, o);
  }
  // SerializeNumberProperty("wrapR", sampler.wrapR, o);
  SerializeNumberProperty("wrapS", sampler.wrapS, o);
  SerializeNumberProperty("wrapT", sampler.wrapT, o);

  SerializeExtrasAndExtensions(sampler, o);
}

static void SerializeGltfOrthographicCamera(const OrthographicCamera &camera,
                                            detail::json &o) {
  SerializeNumberProperty("zfar", camera.zfar, o);
  SerializeNumberProperty("znear", camera.znear, o);
  SerializeNumberProperty("xmag", camera.xmag, o);
  SerializeNumberProperty("ymag", camera.ymag, o);

  SerializeExtrasAndExtensions(camera, o);
}

static void SerializeGltfPerspectiveCamera(const PerspectiveCamera &camera,
                                           detail::json &o) {
  SerializeNumberProperty("zfar", camera.zfar, o);
  SerializeNumberProperty("znear", camera.znear, o);
  if (camera.aspectRatio > 0) {
    SerializeNumberProperty("aspectRatio", camera.aspectRatio, o);
  }

  if (camera.yfov > 0) {
    SerializeNumberProperty("yfov", camera.yfov, o);
  }

  SerializeExtrasAndExtensions(camera, o);
}

static void SerializeGltfCamera(const Camera &camera, detail::json &o) {
  SerializeStringProperty("type", camera.type, o);
  if (!camera.name.empty()) {
    SerializeStringProperty("name", camera.name, o);
  }

  if (camera.type.compare("orthographic") == 0) {
    detail::json orthographic;
    SerializeGltfOrthographicCamera(camera.orthographic, orthographic);
    detail::JsonAddMember(o, "orthographic", std::move(orthographic));
  } else if (camera.type.compare("perspective") == 0) {
    detail::json perspective;
    SerializeGltfPerspectiveCamera(camera.perspective, perspective);
    detail::JsonAddMember(o, "perspective", std::move(perspective));
  } else {
    // ???
  }

  SerializeExtrasAndExtensions(camera, o);
}

static void SerializeGltfScene(const Scene &scene, detail::json &o) {
  SerializeNumberArrayProperty<int>("nodes", scene.nodes, o);

  if (scene.name.size()) {
    SerializeStringProperty("name", scene.name, o);
  }
  SerializeExtrasAndExtensions(scene, o);

  // KHR_audio
  if (!scene.audioEmitters.empty()) {
    detail::json_iterator it;
    if (!detail::FindMember(o, "extensions", it)) {
      detail::json extensions;
      detail::JsonSetObject(extensions);
      detail::JsonAddMember(o, "extensions", std::move(extensions));
      detail::FindMember(o, "extensions", it);
    }
    auto &extensions = detail::GetValue(it);
    if (!detail::FindMember(extensions, "KHR_audio", it)) {
      detail::json audio;
      detail::JsonSetObject(audio);
      detail::JsonAddMember(extensions, "KHR_audio", std::move(audio));
      detail::FindMember(o, "KHR_audio", it);
    }
    SerializeNumberArrayProperty("emitters", scene.audioEmitters,
                                 detail::GetValue(it));
  } else {
    detail::json_iterator ext_it;
    if (detail::FindMember(o, "extensions", ext_it)) {
      auto &extensions = detail::GetValue(ext_it);
      detail::json_iterator lp_it;
      if (detail::FindMember(extensions, "KHR_audio", lp_it)) {
        detail::Erase(extensions, lp_it);
      }
      if (detail::IsEmpty(extensions)) {
        detail::Erase(o, ext_it);
      }
    }
  }
}

static void SerializeGltfSkin(const Skin &skin, detail::json &o) {
  // required
  SerializeNumberArrayProperty<int>("joints", skin.joints, o);

  if (skin.inverseBindMatrices >= 0) {
    SerializeNumberProperty("inverseBindMatrices", skin.inverseBindMatrices, o);
  }

  if (skin.skeleton >= 0) {
    SerializeNumberProperty("skeleton", skin.skeleton, o);
  }

  if (skin.name.size()) {
    SerializeStringProperty("name", skin.name, o);
  }

  SerializeExtrasAndExtensions(skin, o);
}

static void SerializeGltfTexture(const Texture &texture, detail::json &o) {
  if (texture.sampler > -1) {
    SerializeNumberProperty("sampler", texture.sampler, o);
  }
  if (texture.source > -1) {
    SerializeNumberProperty("source", texture.source, o);
  }
  if (texture.name.size()) {
    SerializeStringProperty("name", texture.name, o);
  }
  SerializeExtrasAndExtensions(texture, o);
}

///
/// Serialize all properties except buffers and images.
///
static void SerializeGltfModel(const Model *model, detail::json &o) {
  // ACCESSORS
  if (model->accessors.size()) {
    detail::json accessors;
    detail::JsonReserveArray(accessors, model->accessors.size());
    for (unsigned int i = 0; i < model->accessors.size(); ++i) {
      detail::json accessor;
      SerializeGltfAccessor(model->accessors[i], accessor);
      detail::JsonPushBack(accessors, std::move(accessor));
    }
    detail::JsonAddMember(o, "accessors", std::move(accessors));
  }

  // ANIMATIONS
  if (model->animations.size()) {
    detail::json animations;
    detail::JsonReserveArray(animations, model->animations.size());
    for (unsigned int i = 0; i < model->animations.size(); ++i) {
      if (model->animations[i].channels.size()) {
        detail::json animation;
        SerializeGltfAnimation(model->animations[i], animation);
        detail::JsonPushBack(animations, std::move(animation));
      }
    }

    detail::JsonAddMember(o, "animations", std::move(animations));
  }

  // ASSET
  detail::json asset;
  SerializeGltfAsset(model->asset, asset);
  detail::JsonAddMember(o, "asset", std::move(asset));

  // BUFFERVIEWS
  if (model->bufferViews.size()) {
    detail::json bufferViews;
    detail::JsonReserveArray(bufferViews, model->bufferViews.size());
    for (unsigned int i = 0; i < model->bufferViews.size(); ++i) {
      detail::json bufferView;
      SerializeGltfBufferView(model->bufferViews[i], bufferView);
      detail::JsonPushBack(bufferViews, std::move(bufferView));
    }
    detail::JsonAddMember(o, "bufferViews", std::move(bufferViews));
  }

  // Extensions required
  if (model->extensionsRequired.size()) {
    SerializeStringArrayProperty("extensionsRequired",
                                 model->extensionsRequired, o);
  }

  // MATERIALS
  if (model->materials.size()) {
    detail::json materials;
    detail::JsonReserveArray(materials, model->materials.size());
    for (unsigned int i = 0; i < model->materials.size(); ++i) {
      detail::json material;
      SerializeGltfMaterial(model->materials[i], material);

      if (detail::JsonIsNull(material)) {
        // Issue 294.
        // `material` does not have any required parameters
        // so the result may be null(unmodified) when all material parameters
        // have default value.
        //
        // null is not allowed thus we create an empty JSON object.
        detail::JsonSetObject(material);
      }
      detail::JsonPushBack(materials, std::move(material));
    }
    detail::JsonAddMember(o, "materials", std::move(materials));
  }

  // MESHES
  if (model->meshes.size()) {
    detail::json meshes;
    detail::JsonReserveArray(meshes, model->meshes.size());
    for (unsigned int i = 0; i < model->meshes.size(); ++i) {
      detail::json mesh;
      SerializeGltfMesh(model->meshes[i], mesh);
      detail::JsonPushBack(meshes, std::move(mesh));
    }
    detail::JsonAddMember(o, "meshes", std::move(meshes));
  }

  // NODES
  if (model->nodes.size()) {
    detail::json nodes;
    detail::JsonReserveArray(nodes, model->nodes.size());
    for (unsigned int i = 0; i < model->nodes.size(); ++i) {
      detail::json node;
      SerializeGltfNode(model->nodes[i], node);

      if (detail::JsonIsNull(node)) {
        // Issue 457.
        // `node` does not have any required parameters,
        // so the result may be null(unmodified) when all node parameters
        // have default value.
        //
        // null is not allowed thus we create an empty JSON object.
        detail::JsonSetObject(node);
      }
      detail::JsonPushBack(nodes, std::move(node));
    }
    detail::JsonAddMember(o, "nodes", std::move(nodes));
  }

  // SCENE
  if (model->defaultScene > -1) {
    SerializeNumberProperty<int>("scene", model->defaultScene, o);
  }

  // SCENES
  if (model->scenes.size()) {
    detail::json scenes;
    detail::JsonReserveArray(scenes, model->scenes.size());
    for (unsigned int i = 0; i < model->scenes.size(); ++i) {
      detail::json currentScene;
      SerializeGltfScene(model->scenes[i], currentScene);
      if (detail::JsonIsNull(currentScene)) {
        // Issue 464.
        // `scene` does not have any required parameters,
        // so the result may be null(unmodified) when all scene parameters
        // have default value.
        //
        // null is not allowed thus we create an empty JSON object.
        detail::JsonSetObject(currentScene);
      }
      detail::JsonPushBack(scenes, std::move(currentScene));
    }
    detail::JsonAddMember(o, "scenes", std::move(scenes));
  }

  // SKINS
  if (model->skins.size()) {
    detail::json skins;
    detail::JsonReserveArray(skins, model->skins.size());
    for (unsigned int i = 0; i < model->skins.size(); ++i) {
      detail::json skin;
      SerializeGltfSkin(model->skins[i], skin);
      detail::JsonPushBack(skins, std::move(skin));
    }
    detail::JsonAddMember(o, "skins", std::move(skins));
  }

  // TEXTURES
  if (model->textures.size()) {
    detail::json textures;
    detail::JsonReserveArray(textures, model->textures.size());
    for (unsigned int i = 0; i < model->textures.size(); ++i) {
      detail::json texture;
      SerializeGltfTexture(model->textures[i], texture);
      detail::JsonPushBack(textures, std::move(texture));
    }
    detail::JsonAddMember(o, "textures", std::move(textures));
  }

  // SAMPLERS
  if (model->samplers.size()) {
    detail::json samplers;
    detail::JsonReserveArray(samplers, model->samplers.size());
    for (unsigned int i = 0; i < model->samplers.size(); ++i) {
      detail::json sampler;
      SerializeGltfSampler(model->samplers[i], sampler);
      detail::JsonPushBack(samplers, std::move(sampler));
    }
    detail::JsonAddMember(o, "samplers", std::move(samplers));
  }

  // CAMERAS
  if (model->cameras.size()) {
    detail::json cameras;
    detail::JsonReserveArray(cameras, model->cameras.size());
    for (unsigned int i = 0; i < model->cameras.size(); ++i) {
      detail::json camera;
      SerializeGltfCamera(model->cameras[i], camera);
      detail::JsonPushBack(cameras, std::move(camera));
    }
    detail::JsonAddMember(o, "cameras", std::move(cameras));
  }

  // EXTRAS & EXTENSIONS
  SerializeExtrasAndExtensions(*model, o);

  auto extensionsUsed = model->extensionsUsed;

  // LIGHTS as KHR_lights_punctual
  if (model->lights.size()) {
    detail::json lights;
    detail::JsonReserveArray(lights, model->lights.size());
    for (unsigned int i = 0; i < model->lights.size(); ++i) {
      detail::json light;
      SerializeGltfLight(model->lights[i], light);
      detail::JsonPushBack(lights, std::move(light));
    }
    detail::json khr_lights_cmn;
    detail::JsonAddMember(khr_lights_cmn, "lights", std::move(lights));
    detail::json ext_j;

    {
      detail::json_const_iterator it;
      if (detail::FindMember(o, "extensions", it)) {
        detail::JsonAssign(ext_j, detail::GetValue(it));
      }
    }

    detail::JsonAddMember(ext_j, "KHR_lights_punctual",
                          std::move(khr_lights_cmn));

    detail::JsonAddMember(o, "extensions", std::move(ext_j));

    // Also add "KHR_lights_punctual" to `extensionsUsed`
    {
      auto has_khr_lights_punctual =
          std::find_if(extensionsUsed.begin(), extensionsUsed.end(),
                       [](const std::string &s) {
                         return (s.compare("KHR_lights_punctual") == 0);
                       });

      if (has_khr_lights_punctual == extensionsUsed.end()) {
        extensionsUsed.push_back("KHR_lights_punctual");
      }
    }
  }

  // KHR_audio
  if (!model->audioEmitters.empty() || !model->audioSources.empty()) {
    detail::json emitters;
    detail::JsonReserveArray(emitters, model->audioEmitters.size());
    for (unsigned int i = 0; i < model->audioEmitters.size(); ++i) {
      detail::json emitter;
      SerializeGltfAudioEmitter(model->audioEmitters[i], emitter);
      detail::JsonPushBack(emitters, std::move(emitter));
    }
    detail::json khr_audio_cmn;
    detail::JsonAddMember(khr_audio_cmn, "emitters", std::move(emitters));

    detail::json sources;
    detail::JsonReserveArray(sources, model->audioSources.size());
    for (unsigned int i = 0; i < model->audioSources.size(); ++i) {
      detail::json source;
      SerializeGltfAudioSource(model->audioSources[i], source);
      detail::JsonPushBack(sources, std::move(source));
    }
    detail::JsonAddMember(khr_audio_cmn, "sources", std::move(sources));

    detail::json ext_j;
    {
      detail::json_const_iterator it;
      if (detail::FindMember(o, "extensions", it)) {
        detail::JsonAssign(ext_j, detail::GetValue(it));
      }
    }

    detail::JsonAddMember(ext_j, "KHR_audio", std::move(khr_audio_cmn));

    detail::JsonAddMember(o, "extensions", std::move(ext_j));

    // Also add "KHR_audio" to `extensionsUsed`
    {
      auto has_khr_audio = std::find_if(
          extensionsUsed.begin(), extensionsUsed.end(),
          [](const std::string &s) { return (s.compare("KHR_audio") == 0); });

      if (has_khr_audio == extensionsUsed.end()) {
        extensionsUsed.push_back("KHR_audio");
      }
    }
  }

  // Extensions used
  if (extensionsUsed.size()) {
    SerializeStringArrayProperty("extensionsUsed", extensionsUsed, o);
  }
}

static bool WriteGltfStream(std::ostream &stream, const std::string &content) {
  stream << content << std::endl;
  return stream.good();
}

static bool WriteGltfFile(const std::string &output,
                          const std::string &content) {
#ifdef _WIN32
#if defined(_MSC_VER)
  std::ofstream gltfFile(UTF8ToWchar(output).c_str());
#elif defined(__GLIBCXX__)
  int file_descriptor = _wopen(UTF8ToWchar(output).c_str(),
                               _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY);
  __gnu_cxx::stdio_filebuf<char> wfile_buf(
      file_descriptor, std::ios_base::out | std::ios_base::binary);
  std::ostream gltfFile(&wfile_buf);
  if (!wfile_buf.is_open()) return false;
#else
  std::ofstream gltfFile(output.c_str());
  if (!gltfFile.is_open()) return false;
#endif
#else
  std::ofstream gltfFile(output.c_str());
  if (!gltfFile.is_open()) return false;
#endif
  return WriteGltfStream(gltfFile, content);
}

static bool WriteBinaryGltfStream(std::ostream &stream,
                                  const std::string &content,
                                  const std::vector<unsigned char> &binBuffer) {
  const std::string header = "glTF";
  const int version = 2;

  const uint32_t content_size = uint32_t(content.size());
  const uint32_t binBuffer_size = uint32_t(binBuffer.size());
  // determine number of padding bytes required to ensure 4 byte alignment
  const uint32_t content_padding_size =
      content_size % 4 == 0 ? 0 : 4 - content_size % 4;
  const uint32_t bin_padding_size =
      binBuffer_size % 4 == 0 ? 0 : 4 - binBuffer_size % 4;

  // 12 bytes for header, JSON content length, 8 bytes for JSON chunk info.
  // Chunk data must be located at 4-byte boundary, which may require padding
  const uint32_t length =
      12 + 8 + content_size + content_padding_size +
      (binBuffer_size ? (8 + binBuffer_size + bin_padding_size) : 0);

  stream.write(header.c_str(), std::streamsize(header.size()));
  stream.write(reinterpret_cast<const char *>(&version), sizeof(version));
  stream.write(reinterpret_cast<const char *>(&length), sizeof(length));

  // JSON chunk info, then JSON data
  const uint32_t model_length = uint32_t(content.size()) + content_padding_size;
  const uint32_t model_format = 0x4E4F534A;
  stream.write(reinterpret_cast<const char *>(&model_length),
               sizeof(model_length));
  stream.write(reinterpret_cast<const char *>(&model_format),
               sizeof(model_format));
  stream.write(content.c_str(), std::streamsize(content.size()));

  // Chunk must be multiplies of 4, so pad with spaces
  if (content_padding_size > 0) {
    const std::string padding = std::string(size_t(content_padding_size), ' ');
    stream.write(padding.c_str(), std::streamsize(padding.size()));
  }
  if (binBuffer.size() > 0) {
    // BIN chunk info, then BIN data
    const uint32_t bin_length = uint32_t(binBuffer.size()) + bin_padding_size;
    const uint32_t bin_format = 0x004e4942;
    stream.write(reinterpret_cast<const char *>(&bin_length),
                 sizeof(bin_length));
    stream.write(reinterpret_cast<const char *>(&bin_format),
                 sizeof(bin_format));
    stream.write(reinterpret_cast<const char *>(binBuffer.data()),
                 std::streamsize(binBuffer.size()));
    // Chunksize must be multiplies of 4, so pad with zeroes
    if (bin_padding_size > 0) {
      const std::vector<unsigned char> padding =
          std::vector<unsigned char>(size_t(bin_padding_size), 0);
      stream.write(reinterpret_cast<const char *>(padding.data()),
                   std::streamsize(padding.size()));
    }
  }

  stream.flush();
  return stream.good();
}

static bool WriteBinaryGltfFile(const std::string &output,
                                const std::string &content,
                                const std::vector<unsigned char> &binBuffer) {
#ifdef _WIN32
#if defined(_MSC_VER)
  std::ofstream gltfFile(UTF8ToWchar(output).c_str(), std::ios::binary);
#elif defined(__GLIBCXX__)
  int file_descriptor = _wopen(UTF8ToWchar(output).c_str(),
                               _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY);
  __gnu_cxx::stdio_filebuf<char> wfile_buf(
      file_descriptor, std::ios_base::out | std::ios_base::binary);
  std::ostream gltfFile(&wfile_buf);
#else
  std::ofstream gltfFile(output.c_str(), std::ios::binary);
#endif
#else
  std::ofstream gltfFile(output.c_str(), std::ios::binary);
#endif
  return WriteBinaryGltfStream(gltfFile, content, binBuffer);
}

bool TinyGLTF::WriteGltfSceneToStream(const Model *model, std::ostream &stream,
                                      bool prettyPrint = true,
                                      bool writeBinary = false) {
  detail::JsonDocument output;

  /// Serialize all properties except buffers and images.
  SerializeGltfModel(model, output);

  // BUFFERS
  std::vector<unsigned char> binBuffer;
  if (model->buffers.size()) {
    detail::json buffers;
    detail::JsonReserveArray(buffers, model->buffers.size());
    for (unsigned int i = 0; i < model->buffers.size(); ++i) {
      detail::json buffer;
      if (writeBinary && i == 0 && model->buffers[i].uri.empty()) {
        SerializeGltfBufferBin(model->buffers[i], buffer, binBuffer);
      } else {
        SerializeGltfBuffer(model->buffers[i], buffer);
      }
      detail::JsonPushBack(buffers, std::move(buffer));
    }
    detail::JsonAddMember(output, "buffers", std::move(buffers));
  }

  // IMAGES
  if (model->images.size()) {
    detail::json images;
    detail::JsonReserveArray(images, model->images.size());
    for (unsigned int i = 0; i < model->images.size(); ++i) {
      detail::json image;

      std::string dummystring;
      // UpdateImageObject need baseDir but only uses it if embeddedImages is
      // enabled, since we won't write separate images when writing to a stream
      // we
      std::string uri;
      if (!UpdateImageObject(model->images[i], dummystring, int(i), true,
                             &uri_cb, &this->WriteImageData,
                             this->write_image_user_data_, &uri)) {
        return false;
      }
      SerializeGltfImage(model->images[i], uri, image);
      detail::JsonPushBack(images, std::move(image));
    }
    detail::JsonAddMember(output, "images", std::move(images));
  }

  if (writeBinary) {
    return WriteBinaryGltfStream(stream, detail::JsonToString(output),
                                 binBuffer);
  } else {
    return WriteGltfStream(stream,
                           detail::JsonToString(output, prettyPrint ? 2 : -1));
  }
}

bool TinyGLTF::WriteGltfSceneToFile(const Model *model,
                                    const std::string &filename,
                                    bool embedImages = false,
                                    bool embedBuffers = false,
                                    bool prettyPrint = true,
                                    bool writeBinary = false) {
  detail::JsonDocument output;
  std::string defaultBinFilename = GetBaseFilename(filename);
  std::string defaultBinFileExt = ".bin";
  std::string::size_type pos =
      defaultBinFilename.rfind('.', defaultBinFilename.length());

  if (pos != std::string::npos) {
    defaultBinFilename = defaultBinFilename.substr(0, pos);
  }
  std::string baseDir = GetBaseDir(filename);
  if (baseDir.empty()) {
    baseDir = "./";
  }
  /// Serialize all properties except buffers and images.
  SerializeGltfModel(model, output);

  // BUFFERS
  std::vector<std::string> usedFilenames;
  std::vector<unsigned char> binBuffer;
  if (model->buffers.size()) {
    detail::json buffers;
    detail::JsonReserveArray(buffers, model->buffers.size());
    for (unsigned int i = 0; i < model->buffers.size(); ++i) {
      detail::json buffer;
      if (writeBinary && i == 0 && model->buffers[i].uri.empty()) {
        SerializeGltfBufferBin(model->buffers[i], buffer, binBuffer);
      } else if (embedBuffers) {
        SerializeGltfBuffer(model->buffers[i], buffer);
      } else {
        std::string binSavePath;
        std::string binFilename;
        std::string binUri;
        if (!model->buffers[i].uri.empty() &&
            !IsDataURI(model->buffers[i].uri)) {
          binUri = model->buffers[i].uri;
          if (!uri_cb.decode(binUri, &binFilename, uri_cb.user_data)) {
            return false;
          }
        } else {
          binFilename = defaultBinFilename + defaultBinFileExt;
          bool inUse = true;
          int numUsed = 0;
          while (inUse) {
            inUse = false;
            for (const std::string &usedName : usedFilenames) {
              if (binFilename.compare(usedName) != 0) continue;
              inUse = true;
              binFilename = defaultBinFilename + std::to_string(numUsed++) +
                            defaultBinFileExt;
              break;
            }
          }

          if (uri_cb.encode) {
            if (!uri_cb.encode(binFilename, "buffer", &binUri,
                               uri_cb.user_data)) {
              return false;
            }
          } else {
            binUri = binFilename;
          }
        }
        usedFilenames.push_back(binFilename);
        binSavePath = JoinPath(baseDir, binFilename);
        if (!SerializeGltfBuffer(model->buffers[i], buffer, binSavePath,
                                 binUri)) {
          return false;
        }
      }
      detail::JsonPushBack(buffers, std::move(buffer));
    }
    detail::JsonAddMember(output, "buffers", std::move(buffers));
  }

  // IMAGES
  if (model->images.size()) {
    detail::json images;
    detail::JsonReserveArray(images, model->images.size());
    for (unsigned int i = 0; i < model->images.size(); ++i) {
      detail::json image;

      std::string uri;
      if (!UpdateImageObject(model->images[i], baseDir, int(i), embedImages,
                             &uri_cb, &this->WriteImageData,
                             this->write_image_user_data_, &uri)) {
        return false;
      }
      SerializeGltfImage(model->images[i], uri, image);
      detail::JsonPushBack(images, std::move(image));
    }
    detail::JsonAddMember(output, "images", std::move(images));
  }

  if (writeBinary) {
    return WriteBinaryGltfFile(filename, detail::JsonToString(output),
                               binBuffer);
  } else {
    return WriteGltfFile(filename,
                         detail::JsonToString(output, (prettyPrint ? 2 : -1)));
  }
}

}  // namespace tinygltf

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif  // TINYGLTF_IMPLEMENTATION

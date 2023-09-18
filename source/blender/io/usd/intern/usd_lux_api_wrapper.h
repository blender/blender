
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/shapingAPI.h>

namespace blender::io::usd {

namespace usdtokens {
extern const pxr::TfToken Intensity;
extern const pxr::TfToken Exposure;
extern const pxr::TfToken Diffuse;
extern const pxr::TfToken Specular;
extern const pxr::TfToken Normalize;
extern const pxr::TfToken Color;

extern const pxr::TfToken Radius;
extern const pxr::TfToken Width;
extern const pxr::TfToken Height;
extern const pxr::TfToken Angle;

extern const pxr::TfToken EnableColorTemperature;
extern const pxr::TfToken ColorTemperature;

extern const pxr::TfToken ShapingFocus;
extern const pxr::TfToken ShapingFocusTint;
extern const pxr::TfToken ShapingConeAngle;
extern const pxr::TfToken ShapingConeSoftness;
extern const pxr::TfToken ShapingIesFile;
extern const pxr::TfToken ShapingIesNormalize;
}  // namespace usdtokens

#define MAKEFUNCS(name, ptype, ctype) \
  inline pxr::UsdAttribute Get##name##Attr() const \
  { \
    if (prim_.GetAttribute(pxr::UsdLuxTokens->inputs##name).HasAuthoredValue()) { \
      return prim_.GetAttribute(pxr::UsdLuxTokens->inputs##name); \
    } \
    return prim_.GetAttribute(usdtokens::name); \
  } \
  inline void Set##name##Attr(const ctype value, \
                              pxr::UsdTimeCode time = pxr::UsdTimeCode::Default()) \
  { \
    prim_.GetAttribute(pxr::UsdLuxTokens->inputs##name).Set(value, time); \
    prim_.GetAttribute(usdtokens::name).Set(value, time); \
  } \
  inline pxr::UsdAttribute Create##name##Attr() const \
  { \
    prim_.CreateAttribute(usdtokens::name, ptype, true); \
    return api_.Create##name##Attr(); \
  }

#define MAKEFUNCS_NOAPI(name, ptype, ctype) \
  inline pxr::UsdAttribute Get##name##Attr() const \
  { \
    if (prim_.GetAttribute(pxr::UsdLuxTokens->inputs##name).HasAuthoredValue()) { \
      return prim_.GetAttribute(pxr::UsdLuxTokens->inputs##name); \
    } \
    return prim_.GetAttribute(usdtokens::name); \
  } \
  inline void Set##name##Attr(const ctype value, \
                              pxr::UsdTimeCode time = pxr::UsdTimeCode::Default()) \
  { \
    prim_.GetAttribute(pxr::UsdLuxTokens->inputs##name).Set(value, time); \
    prim_.GetAttribute(usdtokens::name).Set(value, time); \
  } \
  inline pxr::UsdAttribute Create##name##Attr() const \
  { \
    prim_.CreateAttribute(usdtokens::name, ptype, true); \
    return prim_.CreateAttribute(pxr::UsdLuxTokens->inputs##name, ptype, true); \
  }

class UsdLuxWrapper {
  pxr::UsdPrim prim_;
  pxr::UsdLuxLightAPI api_;

 public:
  explicit UsdLuxWrapper(const pxr::UsdPrim &prim = pxr::UsdPrim()) : prim_(prim), api_(prim) {}

  explicit operator bool() const
  {
    return static_cast<bool>(api_);
  }

  pxr::UsdPrim GetPrim() const
  {
    return prim_;
  }

  /// Destructor.
  virtual ~UsdLuxWrapper() = default;

  MAKEFUNCS(Intensity, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS(Exposure, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS(Specular, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS(Color, pxr::SdfValueTypeNames->Color3f, pxr::GfVec3f);
  MAKEFUNCS(Diffuse, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS(Normalize, pxr::SdfValueTypeNames->Bool, bool);

  // rect / disk light support
  MAKEFUNCS_NOAPI(Radius, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS_NOAPI(Width, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS_NOAPI(Height, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS_NOAPI(Angle, pxr::SdfValueTypeNames->Float, float);
};

class UsdShapingWrapper {
  pxr::UsdPrim prim_;
  pxr::UsdLuxShapingAPI api_;

 public:
  explicit UsdShapingWrapper(const pxr::UsdPrim &prim = pxr::UsdPrim()) : prim_(prim), api_(prim)
  {
  }

  explicit operator bool() const
  {
    return bool(api_);
  }

  pxr::UsdPrim GetPrim() const
  {
    return prim_;
  }

  MAKEFUNCS(ShapingConeAngle, pxr::SdfValueTypeNames->Float, float);
  MAKEFUNCS(ShapingConeSoftness, pxr::SdfValueTypeNames->Float, float);
};

#undef MAKEFUNCS_NOAPI
#undef MAKEFUNCS

}  // namespace blender::io::usd

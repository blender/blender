
#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_sculpt_brush_types.h"

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_rect.h"
#include "BLI_smallhash.h"
#include "BLI_utildefines.h"

#include "BKE_brush_engine.h"

#include "BKE_brush.h"
#include "BKE_brush_engine.h"
#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_curvemapping_cache.h"
#include "BKE_curveprofile.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_paint.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>

#include "intern/brush_channel_names.hh"

#define IS_CACHE_CURVE(curve) BKE_curvemapping_in_cache(curve)
// frees curve if it wasn't cached, returns cache curved
#define GET_CACHE_CURVE(curve) BKE_curvemapping_cache_get(brush_curve_cache, curve, true)
#define RELEASE_CACHE_CURVE(curve) BKE_curvemapping_cache_release(brush_curve_cache, curve)
#define RELEASE_OR_FREE_CURVE(curve) \
  curve ? (BKE_curvemapping_cache_release_or_free(brush_curve_cache, curve), nullptr) : nullptr
#define CURVE_ADDREF(curve) BKE_curvemapping_cache_aquire(brush_curve_cache, curve)

extern struct CurveMappingCache *brush_curve_cache;
extern BrushChannelType brush_builtin_channels[];
extern int brush_builtin_channel_len;

/* build compile time list of channel idnames */

/** return eithers a reference to a brush channel type,
   or if T is BrushCurve the (non-reference) BrushCurveIF wrapper type*/
#define BRUSH_VALUE_REF(T) \
  typename std::conditional<std::is_same_v<T, BrushCurve>, BrushCurveIF, T &>::type

template<class T> struct extract_float_array {
  using type = typename std::conditional<std::is_array_v<T>, float, T>::type;
};

namespace blender {
namespace brush {

class BrushCurveIF {
 public:
  BrushCurveIF(BrushCurve *curve) : _curve(curve)
  {
  }
  BrushCurveIF(const BrushCurveIF &b)
  {
    _curve = b._curve;
  }

  float evaluate(float f, float maxval = 1.0f)
  {
    /* ensure that curve is in valid state */
    initCurve(false);

    return BKE_brush_curve_strength_ex(_curve->preset, _curve->curve, f, maxval);
  }

  void initCurve(bool forceCreate = false)
  {
    if ((forceCreate || _curve->preset == BRUSH_CURVE_CUSTOM) && !_curve->curve) {
      CurveMapping *cumap = _curve->curve = static_cast<CurveMapping *>(
          MEM_callocN(sizeof(CurveMapping), "channel CurveMapping"));

      int preset = CURVE_PRESET_LINE;

      /* brush and curvemapping presets aren't perfectly compatible,
         try to convert in reasonable manner*/
      switch (_curve->preset) {
        case BRUSH_CURVE_SMOOTH:
        case BRUSH_CURVE_SMOOTHER:
          preset = CURVE_PRESET_SMOOTH;
          break;

        case BRUSH_CURVE_SHARP:
          preset = CURVE_PRESET_SHARP;
          break;
        case BRUSH_CURVE_POW4:
          preset = CURVE_PRESET_POW3;
          break;
      }

      struct rctf rect;
      rect.xmin = rect.ymin = 0.0f;
      rect.xmax = rect.ymax = 1.0f;

      BKE_curvemapping_set_defaults(cumap, 1, 0.0f, 0.0f, 1.0f, 1.0f);
      BKE_curvemap_reset(cumap->cm, &rect, preset, _curve->preset_slope_negative ? 0 : 1);

      BKE_curvemapping_init(cumap);
    }
  }

  void ensureWrite()
  {
    initCurve(true);

    if (IS_CACHE_CURVE(_curve->curve)) {
      _curve->curve = BKE_curvemapping_copy(_curve->curve);
    }
  }

  CurveMapping *curve()
  {
    initCurve(false);
    return _curve->curve;
  }

  eBrushCurvePreset &preset()
  {
    eBrushCurvePreset *p = reinterpret_cast<eBrushCurvePreset *>(&_curve->preset);
    return *p;
  }

 private:
  BrushCurve *_curve;
};

template<typename T> class BrushChannelIF {
 public:
  BrushChannelIF()
  {
    _channel = nullptr;
  }

  BrushChannelIF(BrushChannel *source) : _channel(source)
  {
  }

  const int type()
  {
    return _channel->type;
  }

  eBrushChannelFlag &flag()
  {
    eBrushChannelFlag *f = reinterpret_cast<eBrushChannelFlag *>(&_channel->flag);

    return *f;
  }

  BrushChannelIF(const BrushChannelIF<T> &b)
  {
    _channel = b._channel;
  }

  BrushChannelIF<T> &operator=(const BrushChannelIF<T> &a) = default;

  const BrushChannel &channel()
  {
    return *_channel;
  }

  const char *idname()
  {
    return _channel->idname;
  }

  const bool isValid() const
  {
    return _channel != nullptr;
  }

  /**
  Returns a reference to value of a brush channel.

  Note that if T is BrushCurve then a BrushCurveIF
  wrapper will be returned instead.

  */
  BRUSH_VALUE_REF(T) value()
  {
    if constexpr (std::is_same_v<T, float>) {
      return _channel->fvalue;
    }
    else if constexpr (std::is_same_v<T, int>) {
      return _channel->ivalue;
    }
    else if constexpr (std::is_same_v<T, bool>) {
      bool *boolval = reinterpret_cast<bool *>(&_channel->ivalue);
      return *boolval;
    }
    else if constexpr (std::is_same_v<T, BrushCurveIF>) {
      return BrushCurveIF(&_channel->curve);
    }
    else if constexpr (std::is_same_v<T, float[3]>) {
      float(*vec3)[3] = reinterpret_cast<float(*)[3]>(_channel->vector);

      return vec3[0];
    }
    else if constexpr (std::is_same_v<T, float[4]>) {
      return _channel->vector;
    }

    T ret;
    return ret;
  }

  /* vectorIndex is only used for float[3] and float[4] specializations*/
  typename extract_float_array<T>::type evaluate(BrushMappingData *mapping = nullptr,
                                                 int vectorIndex = 0)
  {
    if constexpr (std::is_same_v<T, float>) {
      return (float)_evaluate((double)_channel->fvalue, 0, mapping);
    }
    else if constexpr (std::is_same_v<T, int>) {
      return (int)_evaluate((double)_channel->ivalue, 0, mapping);
    }
    else if constexpr (std::is_same_v<T, bool>) {
      return fabs(_evaluate((double)(_channel->ivalue & 1), 0, mapping)) > FLT_EPSILON;
    }
    else if constexpr (std::is_same_v<T, float[3]> || std::is_same_v<T, float[4]>) {
      return (float)_evaluate((double)_channel->vector[vectorIndex], vectorIndex, mapping);
    }

    static_assert(!std::is_same_v<T, BrushCurveIF>, "cannot use evaluate with brush curves");
  }

 private:
  double _evaluate(double val, unsigned int idx, BrushMappingData *mapping = nullptr)
  {
    if (idx == 3 && !(_channel->flag & BRUSH_CHANNEL_APPLY_MAPPING_TO_ALPHA)) {
      return val;
    }

    if (mapping) {
      double factor = val;

      for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
        BrushMapping *mp = _channel->mappings + i;

        if (!(mp->flag & BRUSH_MAPPING_ENABLED)) {
          continue;
        }

        float inputf = (reinterpret_cast<float *>(mapping))[i] * mp->premultiply;

        switch ((BrushMappingFunc)mp->mapfunc) {
          case BRUSH_MAPFUNC_NONE:
            break;
          case BRUSH_MAPFUNC_SAW:
            inputf -= floorf(inputf);
            break;
          case BRUSH_MAPFUNC_TENT:
            inputf -= floorf(inputf);
            inputf = 1.0f - fabs(inputf - 0.5f) * 2.0f;
            break;
          case BRUSH_MAPFUNC_COS:
            inputf = 1.0f - (cos(inputf * (float)M_PI * 2.0f) * 0.5f + 0.5f);
            break;
          case BRUSH_MAPFUNC_CUTOFF:
            /*Cutoff is meant to create a fadeout effect,
              which requires inverting the input.  To avoid
              user confusion we just do it here instead of making
              them check the inverse checkbox.*/
            inputf = 1.0f - inputf;
            CLAMP(inputf, 0.0f, mp->func_cutoff * 2.0f);
            break;
          case BRUSH_MAPFUNC_SQUARE:
            inputf -= floorf(inputf);
            inputf = inputf > mp->func_cutoff ? 1.0f : 0.0f;  //(float)(inputf > 0.5f);
            break;
          default:
            break;
        }

        if (mp->flag & BRUSH_MAPPING_INVERT) {
          inputf = 1.0f - inputf;
        }

        /* ensure curve tables exist */
        BKE_curvemapping_init(mp->curve);

        double f2 = (float)BKE_curvemapping_evaluateF(mp->curve, 0, inputf);
        f2 = mp->min + (mp->max - mp->min) * f2;

        /* make sure to update blend_items in rna_brush_engine.c
          when adding new mode implementations */
        switch (mp->blendmode) {
          case MA_RAMP_BLEND:
            break;
          case MA_RAMP_MULT:
            f2 *= factor;
            break;
          case MA_RAMP_DIV:
            f2 = factor / (f2 == 0.0f ? 0.0001f : f2);
            break;
          case MA_RAMP_ADD:
            f2 += factor;
            break;
          case MA_RAMP_SUB:
            f2 = factor - f2;
            break;
          case MA_RAMP_DIFF:
            f2 = std::abs(factor - f2);
            break;
          default:
            printf("Unsupported brush mapping blend mode for %s (%s); will mix instead\n",
                   _channel->name,
                   _channel->idname);
            break;
        }

        factor += (f2 - factor) * mp->factor;
      }

      val = factor;
      CLAMP(val, (double)_channel->def->min, (double)_channel->def->max);
    }

    return val;
  }

  BrushChannel *_channel;
};

class BrushChannelSetIF {
 public:
  BrushChannelSetIF(BrushChannelSet *chset) : _chset(chset)
  {
  }

  BrushChannelSetIF(const BrushChannelSetIF &b)
  {
    _chset = b._chset;
  }

  bool isValid()
  {
    return _chset != nullptr;
  }

  void destroy()
  {
    if (_chset) {
      if (_chset->namemap) {
        BLI_ghash_free(_chset->namemap, nullptr, nullptr);
      }

      LISTBASE_FOREACH (BrushChannel *, ch, &_chset->channels) {
        if (ch->curve.curve) {
          RELEASE_OR_FREE_CURVE(ch->curve.curve);
        }
      }

      BLI_freelistN(&_chset->channels);

      MEM_SAFE_FREE(_chset);
    }
    _chset = nullptr;
  }

  void ensureChannel(const char *idname)
  {
  }

  template<typename T> BrushChannelIF<T> lookup(const char *idname)
  {
    BrushChannel *ch = static_cast<BrushChannel *>(
        BLI_ghash_lookup(_chset->namemap, static_cast<const void *>(idname)));
    BrushChannelIF<T> chif(ch);

    return chif;
  }

  template<typename T>
  BRUSH_VALUE_REF(T)
  getFinalValue(BrushChannelSet *parentset,
                BrushChannelIF<T> ch,
                BrushMappingData *mapping = nullptr,
                int vectorIndex = 0)
  {
    return getFinalValue(BrushChannelSetIF(parentset), ch, mapping, vectorIndex);
  }

  /**
  Looks up channel with same idname as ch in parentset.
  If it exists, returns the evaluated value of that channel
  taking all inheritance flags and input mappings into
  account.

  if it doesn't exist then the value of ch with
  input mappings evaluated will be returned.

  Note that if T is BrushCurve then we simply return
  a BrushCurveIF wrapper of either ch or the
  one in parentset, depending on inheritance flags.
  */
  template<typename T>
  typename extract_float_array<T>::type getFinalValue(BrushChannelSetIF &parentSet,
                                                      BrushChannelIF<T> ch,
                                                      BrushMappingData *mapping = nullptr,
                                                      int vectorIndex = 0)
  {
    BrushChannelIF<T> ch2;

    if (parentSet.isValid()) {
      ch2 = parentSet.lookup<T>(ch.idname());
    }

    if (!parentSet.isValid() || !ch2.isValid()) {
      if constexpr (std::is_same_v<T, BrushCurve>) {  // curve?
        return ch.value();
      }
      else if constexpr (std::is_array_v<T>) {
        return ch.evaluate(mapping, vectorIndex);
      }
    }

    if constexpr (std::is_same_v<T, BrushCurve>) {
      return (int)ch.flag() & (int)BRUSH_CHANNEL_INHERIT ? ch2.value() : ch.value();
    }

    BrushChannel _cpy = (int)ch.flag() & (int)BRUSH_CHANNEL_INHERIT ? ch2.channel() : ch.channel();
    BrushChannelIF<T> cpy(&_cpy);

    BKE_brush_channel_apply_mapping_flags(&_cpy, &ch.channel(), &ch2.channel());

    return cpy.evaluate(mapping, vectorIndex);
  }

  /*
We want to validate channel names at compile time,
but we can't do compile-time validation of string literals
without c++20.  Instead we use macros to make
lots of accessor methods.

examples:

  BrushChannelIF<float> strength();
  BrushChannelIF<float> radius();
  BrushChannelIF<bool> dyntopo_disabled();

  auto ch = chset->strength();
  BrushChanneIF<float> ch = chset->strength();

  if (ch->isValid()) {
    float val = ch->value();
    ch->value() = val * val;
  }
*/

#define BRUSH_CHANNEL_MAKE_CPP_LOOKUPS
#include "intern/brush_channel_define.h"

 private:
  BrushChannelSet *_chset;
};

}  // namespace brush
}  // namespace blender

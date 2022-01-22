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
#include <type_traits>

/* build compile time list of channel idnames */

namespace blender {
namespace brush {

template<typename T> class BrushChannelIF {
 public:
  BrushChannelIF(BrushChannel *source) : _channel(source)
  {
  }

  /* evaluation functions */

  /** int evaluator */
  std::enable_if_t<std::is_same<T, int>::value> evaluate(BrushMappingData *mapdata = nullptr)
  {
    double val = static_cast<double>(_channel->ivalue);

    val = _evaluate(val, t, 0UL, mapdata);
    return static_cast<int>(val);
  }

  /** float evaluator */
  std::enable_if_t<std::is_same<T, float>::value> evaluate(BrushMappingData *mapdata = nullptr)
  {
    double val = static_cast<double>(_channel->fvalue);

    val = _evaluate(val, t, 0UL, mapdata);
    return static_cast<float>(val);
  }

  /** bool evaluator */
  std::enable_if_t<std::is_same<T, bool>::value> evaluate(BrushMappingData *mapdata = nullptr)
  {
    double val = static_cast<double>(_channel->fvalue);

    val = _evaluate(val, t, 0UL, mapdata);
    return std::floor(val) != 0;
  }

  /** Curve evaluator. Unlike other channel types, this takes a float
      argument, runs it through the brush curve and returns the result as
      a float.

      \param t value to evaluate with brush curve
      */
  std::enable_if_t<
      std::conditional<std::is_same<T, BrushCurve>::value, float, std::false_type>::value>
  evaluate(float t, BrushMappingData *mapdata = nullptr)
  {
    t = BKE_brush_curve_strength_ex(_channel->curve.preset, _channel->curve.curve, 1.0f - t, 1.0f);
    double val = static_cast<double>(t);

    return static_cast<float>(_evaluate(val, t, 0UL, mapdata));
  }

  /** value getter for int channels */
  std::add_lvalue_reference_t<std::enable_if_t<std::is_same<T, int>::value>> value()
  {
    return _channel->ivalue;
  }

  /** value getter for float channels */
  std::add_lvalue_reference_t<std::enable_if_t<std::is_same<T, float>::value>> value()
  {
    return _channel->fvalue;
  }

  /** value getter for bool channels */
  std::add_lvalue_reference_t<std::enable_if_t<std::is_same<T, bool>::value>> value()
  {
    return *(reinterpret_cast<bool *>(&_channel->ivalue));
  }

  /** value getter for BrushCurve channels */
  std::add_lvalue_reference_t<std::enable_if_t<std::is_same<T, BrushCurve>::value>> value()
  {
    return _channel->curve;
  }

 private:
  double _evaluate(double val, float t, unsigned int idx, BrushMappingData *mapdata = nullptr)
  {
    if (idx == 3 && !(ch->flag & BRUSH_CHANNEL_APPLY_MAPPING_TO_ALPHA)) {
      return f;
    }

    if (mapdata) {
      double factor = f;  // 1.0f;

      for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
        BrushMapping *mp = _channel->mappings + i;

        if (!(mp->flag & BRUSH_MAPPING_ENABLED)) {
          continue;
        }

        float inputf = ((float *)mapdata)[i] * mp->premultiply;

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
            f2 = fabsf(factor - f2);
            break;
          default:
            printf("Unsupported brush mapping blend mode for %s (%s); will mix instead\n",
                   ch->name,
                   ch->idname);
            break;
        }

        factor += (f2 - factor) * mp->factor;
      }

      f = factor;
      CLAMP(f, _channel->def->min, _channel->def->max);
    }

    return f;
  }

  BrushChannel *_channel;
};

template<typename T> class BrushChannelSetIF {
 public:
  BrushChannelSetIF(BrushChannelSet *chset) : _chset(chset)
  {
  }


 private:
  BrushChannelSet *_chset;
};

}  // namespace brush
}  // namespace blender

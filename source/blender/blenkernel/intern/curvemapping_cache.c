#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_rand.h"
#include "BLI_rect.h"
#include "BLI_smallhash.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_sculpt_brush_types.h"

#include "BKE_brush.h"
#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_node.h"
#include "BKE_paint.h"

#include "BKE_brush_engine.h"
#include "BKE_curvemapping_cache.h"
#include "BKE_curveprofile.h"

#include "BLO_read_write.h"

/*
The brush system needs to make lots of copies of BrushChannels
as it resolves relationships between commands, brushes and the tool settings.

Unfortunately each brush command has five parameter curves, and copying
them over and over again has proven to be slow.

Solution: a kind of small string optimization approach but for curves
*/

static int curvmapping_curve_count(const CurveMapping *cumap)
{
  int count = 0;

  for (int i = 0; i < CM_TOT; i++) {
    if (cumap->cm[i].curve) {
      count++;
    }
  }

  return count;
}

ATTR_NO_OPT bool BKE_curvemapping_equals(const CurveMapping *a, const CurveMapping *b)
{
  int count = curvmapping_curve_count(a);

  if (curvmapping_curve_count(b) != count) {
    return false;
  }

  bool ok = true;

  ok = ok && a->tone == b->tone;

  for (int i = 0; i < 3; i++) {
    ok = ok && a->black[i] == b->black[i];
    ok = ok && a->white[i] == b->white[i];
    ok = ok && a->bwmul[i] == b->bwmul[i];
  }

  if (!ok) {
    return false;
  }

  for (int i = 0; i < count; i++) {
    const CurveMap *c1 = a->cm + i;
    const CurveMap *c2 = b->cm + i;

    if (!c1 || !c2) {
      return false;
    }

    if (c1->totpoint != c2->totpoint) {
      return false;
    }

    for (int j = 0; j < c1->totpoint; j++) {
      ok = ok && fabsf(c1->curve[j].x - c2->curve[j].x) <= FLT_EPSILON * 8;
      ok = ok && fabsf(c1->curve[j].y - c2->curve[j].y) <= FLT_EPSILON * 8;
    }

    if (!ok) {
      return false;
    }
  }

  return true;
}

/*truncate float to avoid rounding errors
  messing up the hash*/
#define FRACT_TRUNCATE_STEPS 8192.0f
ATTR_NO_OPT BLI_INLINE unsigned int get_float_hash(float f)
{
  float f_floor = floor(f);
  float fract = f - f_floor;

  fract = floorf(fract * FRACT_TRUNCATE_STEPS) / FRACT_TRUNCATE_STEPS;

  uint *ret = (uint *)&f;
  return *ret;
}

#define HASHFLOAT(f) h = get_float_hash(f), hash ^= BLI_hash_int(h + hi++)
#define HASHINT(f) h = get_float_hash(f), hash ^= BLI_hash_int(f + hi++)

ATTR_NO_OPT uint BKE_curvemapping_calc_hash(const CurveMapping *cumap)
{
  uint hash = 0;
  uint h = 0;
  uint hi = 0;
  int totcurve = curvmapping_curve_count(cumap);

  HASHINT(totcurve);
  HASHINT(cumap->tone);

  for (int i = 0; i < 3; i++) {
    HASHFLOAT(cumap->white[i]);
    HASHFLOAT(cumap->black[i]);
    HASHFLOAT(cumap->bwmul[i]);
  }

  for (int i = 0; i < totcurve; i++) {
    if (!cumap->cm[i].curve) {
      break;
    }

    const CurveMap *cu = cumap->cm + i;

    for (int j = 0; j < cu->totpoint; j++) {
      HASHFLOAT(cu->curve[j].x);
      HASHFLOAT(cu->curve[j].y);
    }
  }

  return hash & ((1 << 29) - 1);
}

static bool curves_equals(const void *a, const void *b)
{
  // ghash requires we invert here

  return !BKE_curvemapping_equals(a, b);
}

static unsigned int curve_hash(const void *c)
{
  return BKE_curvemapping_calc_hash(c);
}

CurveMappingCache *BKE_curvemapping_cache_create()
{
  CurveMappingCache *ret = MEM_callocN(sizeof(*ret), "CurveMappingCache");

  ret->gh = BLI_ghash_new(curve_hash, curves_equals, "CurveMappingCache ghash");

  return ret;
}

void BKE_curvemapping_cache_aquire(CurveMappingCache *cache, CurveMapping *curve)
{
  curve->cache_users++;
}

ATTR_NO_OPT void BKE_curvemapping_cache_release(CurveMappingCache *cache, CurveMapping *curve)
{
  curve->cache_users--;

  if ((curve->flag & CUMA_PART_OF_CACHE) && curve->cache_users < 0) {
    if (!BLI_ghash_remove(cache->gh, curve, NULL, NULL)) {
      printf("error, curve was not in cache! %p\n", curve);
    }

    BKE_curvemapping_free(curve);
  }
}

bool BKE_curvemapping_in_cache(CurveMapping *curve)
{
  return curve->flag & CUMA_PART_OF_CACHE;
}

ATTR_NO_OPT CurveMapping *BKE_curvemapping_cache_get(CurveMappingCache *cache,
                                                     CurveMapping *curve,
                                                     bool free_input)
{
  void **key, **val;

  CurveMapping *lookup;

  if (BLI_ghash_ensure_p_ex(cache->gh, curve, &key, &val)) {
    lookup = *key;

    if (free_input && lookup != curve && !(curve->flag & CUMA_PART_OF_CACHE)) {
      BKE_curvemapping_free(curve);
    }

    lookup->cache_users++;

    return lookup;
  }

  printf("adding curve key %d\n", BKE_curvemapping_calc_hash(curve));

  CurveMapping *curve2 = BKE_curvemapping_copy(curve);

  *key = curve2;
  *val = curve2;

  curve2->flag |= CUMA_PART_OF_CACHE;
#if 1
  printf("%d %d",
         (int)BKE_curvemapping_calc_hash(curve2),
         (int)BKE_curvemapping_equals(curve, curve2));

  CurveMap *cu = curve2->cm;
  printf("{\n");

  for (int i = 0; i < cu->totpoint; i++) {
    printf("  %f, %f\n", cu->curve[i].x, cu->curve[i].y);
  }
  printf("}\n");
#endif
  if (free_input && !(curve->flag & CUMA_PART_OF_CACHE)) {
    BKE_curvemapping_free(curve);
  }

  curve2->cache_users = 1;

  return curve2;
}

void BKE_curvemapping_cache_free(CurveMappingCache *cache)
{
  GHashIterator gi;

  GHASH_ITER (gi, cache->gh) {
    CurveMapping *curve = BLI_ghashIterator_getKey(&gi);
    BKE_curvemapping_free(curve);
  }

  BLI_ghash_free(cache->gh, NULL, NULL);
  MEM_freeN(cache);
}

static CurveMappingCache *the_global_cache = NULL;

void BKE_curvemapping_cache_exit()
{
  if (the_global_cache) {
    BKE_curvemapping_cache_free(the_global_cache);
  }
}

CurveMappingCache *BKE_curvemapping_cache_global()
{
  if (!the_global_cache) {
    the_global_cache = BKE_curvemapping_cache_create();
  }

  return the_global_cache;
}

// releases a curve if it's in the cache, otherwise frees it
ATTR_NO_OPT void BKE_curvemapping_cache_release_or_free(CurveMappingCache *cache,
                                                        CurveMapping *curve)
{
  if (curve->flag & CUMA_PART_OF_CACHE) {
    BKE_curvemapping_cache_release(cache, curve);
  }
  else {
    BKE_curvemapping_free(curve);
  }

#if 0
      CurveMap *cu1 = curve->cm;
      CurveMap *cu2 = curve->cm;

      if (cu1->totpoint != cu2->totpoint) {
        printf("%s: curvemapping cache error; totpoint differed: %d %d\n",
               __func__,
               cu1->totpoint,
               cu2->totpoint);
        return;
      }

      printf("curve tables: {\n");

      for (int i = 0; i < cu1->totpoint; i++) {
        CurveMapPoint *p1 = cu1->curve + i;
        CurveMapPoint *p2 = cu1->curve + i;
        printf("  %f, %f,  | %f, %f\n", p1->x, p1->y, p2->x, p2->y);
      }
      printf("}\n");
#endif
}

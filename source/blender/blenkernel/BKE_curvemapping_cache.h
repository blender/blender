struct GHash;
struct CurveMapping;

typedef struct CurveMappingCache {
  struct GHash *gh;
} CurveMappingCache;

bool BKE_curvemapping_equals(const struct CurveMapping *a, const struct CurveMapping *b);
uint BKE_curvemapping_calc_hash(const struct CurveMapping *cumap);

CurveMappingCache *BKE_curvemapping_cache_create(void);
CurveMapping *BKE_curvemapping_cache_get(CurveMappingCache *cache,
                                         CurveMapping *curve,
                                         bool free_input);
void BKE_curvemapping_cache_free(CurveMappingCache *cache);

// takes a curve that's already in the cache and increases its user count
void BKE_curvemapping_cache_aquire(CurveMappingCache *cache, CurveMapping *curve);
void BKE_curvemapping_cache_release(CurveMappingCache *cache, CurveMapping *curve);

bool BKE_curvemapping_in_cache(CurveMapping *curve);
void BKE_curvemapping_cache_release_or_free(CurveMappingCache *cache, CurveMapping *curve);

void BKE_curvemapping_cache_exit();

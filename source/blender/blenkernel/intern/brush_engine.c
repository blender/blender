#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
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

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_sculpt_brush_types.h"

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

#include "BLO_read_write.h"

#if defined(_MSC_VER) && !defined(__clang__)
#  pragma warning(error : 4018) /* signed/unsigned mismatch */
#  pragma warning(error : 4245) /* conversion from 'int' to 'unsigned int' */
#  pragma warning(error : 4389) /* signed/unsigned mismatch */
#  pragma warning(error : 4002) /* too many actual parameters for macro 'identifier' */
#  pragma warning(error : 4003) /* not enough actual parameters for macro 'identifier' */
#  pragma warning( \
      error : 4022) /* 'function': pointer mismatch for actual parameter 'parameter number' */
#  pragma warning(error : 4033) /* 'function' must return a value */
#endif

#define IS_CACHE_CURVE(curve) BKE_curvemapping_in_cache(curve)

// frees curve if it wasn't cached, returns cache curved
#define GET_CACHE_CURVE(curve) BKE_curvemapping_cache_get(brush_curve_cache, curve, true)
#define RELEASE_CACHE_CURVE(curve) BKE_curvemapping_cache_release(brush_curve_cache, curve)
#define RELEASE_OR_FREE_CURVE(curve) \
  curve ? (BKE_curvemapping_cache_release_or_free(brush_curve_cache, curve), NULL) : NULL
#define CURVE_ADDREF(curve) BKE_curvemapping_cache_aquire(brush_curve_cache, curve)

#ifdef DEBUG_CURVE_MAPPING_ALLOC
static struct {
  char tag[4192];
} namestack[256] = {0};
int namestack_i = 1;

void namestack_push(const char *name)
{
  namestack_i++;

  strcpy(namestack[namestack_i].tag, namestack[namestack_i - 1].tag);
  strcat(namestack[namestack_i].tag, ".");
  strcat(namestack[namestack_i].tag, name);
}

void *namestack_pop(void *passthru)
{
  namestack_i--;
  return passthru;
}

#  define namestack_head_name strdup(namestack[namestack_i].tag)

void BKE_curvemapping_copy_data_tag_ex(CurveMapping *target,
                                       const CurveMapping *cumap,
                                       const char *tag);

#  define BKE_curvemapping_copy_data(dst, src) \
    BKE_curvemapping_copy_data_tag_ex(dst, src, namestack_head_name)
#else
#  define namestack_push(name)
#  define namestack_pop(passthru)
#endif

struct CurveMappingCache *brush_curve_cache = NULL;
extern BrushChannelType brush_builtin_channels[];
extern int brush_builtin_channel_len;

void BKE_brush_channel_system_init()
{
  brush_curve_cache = BKE_curvemapping_cache_create();
}

void BKE_brush_channel_system_exit()
{
  BKE_curvemapping_cache_free(brush_curve_cache);
}
// returns true if curve was duplicated
bool BKE_brush_mapping_ensure_write(BrushMapping *mp)
{
  if (IS_CACHE_CURVE(mp->curve)) {
    mp->curve = BKE_curvemapping_copy(mp->curve);
    return true;
  }

  return false;
}

void BKE_brush_channel_curve_assign(BrushChannel *ch, BrushCurve *curve)
{
  RELEASE_OR_FREE_CURVE(ch->curve.curve);

  if (curve->curve) {
    if (IS_CACHE_CURVE(curve->curve)) {
      ch->curve.curve = curve->curve;
      CURVE_ADDREF(curve->curve);
    }
    else {
      ch->curve.curve = BKE_curvemapping_copy(curve->curve);
      BKE_curvemapping_init(curve->curve);
    }
  }
  else {
    ch->curve.curve = NULL;
  }

  ch->curve.preset = curve->preset;
}

// returns true if curve was duplicated
bool BKE_brush_channel_curve_ensure_write(BrushCurve *curve)
{
  BKE_brush_channel_curvemapping_get(curve, true);

  if (IS_CACHE_CURVE(curve->curve)) {
    curve->curve = BKE_curvemapping_copy(curve->curve);
    return true;
  }

  return false;
}

static bool check_corrupted_curve(BrushMapping *dst)
{
  CurveMapping *curve = dst->curve;

  if (BKE_curvemapping_in_cache(curve)) {
    return false;
  }

  const float clip_size_x = BLI_rctf_size_x(&curve->curr);
  const float clip_size_y = BLI_rctf_size_y(&curve->curr);

  // fix corrupted curve
  if (clip_size_x == 0.0f || clip_size_y == 0.0f) {
    for (int i = 0; i < 4; i++) {
      BKE_curvemapping_free_data(curve);
      memset(&dst->curve, 0, sizeof(CurveMapping));

      BKE_curvemapping_set_defaults(dst->curve, 1, 0.0, 0.0, 1.0, 1.0);

      BKE_curvemap_reset(curve->cm + i,
                         &(struct rctf){.xmin = 0, .ymin = 0.0, .xmax = 1.0, .ymax = 1.0},
                         CURVE_PRESET_LINE,
                         1);
      BKE_curvemapping_init(dst->curve);
    }

    return false;
  }

  return true;
}

/*
Brush command lists.

Command lists are built dynamically from
brush flags, pen input settings, etc.

Eventually they will be generated by node
networks.  BrushCommandPreset will be
generated from the node group inputs.
*/

void BKE_brush_channeltype_rna_check(BrushChannelType *def,
                                     int (*getIconFromName)(const char *name))
{
  if (def->rna_enumdef) {
    return;
  }

  if (!def->user_defined) {
    // builtin channel types are never freed, don't use guardedalloc
    def->rna_enumdef = malloc(sizeof(EnumPropertyItem) * ARRAY_SIZE(def->enumdef));
  }
  else {
    def->rna_enumdef = MEM_calloc_arrayN(
        ARRAY_SIZE(def->enumdef), sizeof(EnumPropertyItem), "def->rna_enumdef");
  }

  for (int i = 0; i < ARRAY_SIZE(def->enumdef); i++) {
    if (def->enumdef[i].value == -1 || i == ARRAY_SIZE(def->enumdef) - 1) {
      def->rna_enumdef[i].value = 0;
      def->rna_enumdef[i].identifier = NULL;
      def->rna_enumdef[i].icon = 0;
      def->rna_enumdef[i].name = NULL;
      def->rna_enumdef[i].value = 0;
      break;
    }

    EnumPropertyItem *item = def->rna_enumdef + i;

    item->value = def->enumdef[i].value;
    item->identifier = def->enumdef[i].identifier;
    item->icon = getIconFromName ? getIconFromName(def->enumdef[i].icon) : -1;
    item->name = def->enumdef[i].name;
    item->description = def->enumdef[i].description;

    // detect seperaters
    if (!item->value && !item->identifier[0] && !item->name[0] && !item->description[0]) {
      item->description = NULL;
      item->name = NULL;
      item->icon = 0;
    }
  }
}

void BKE_brush_channel_free_data(BrushChannel *ch)
{
  MEM_SAFE_FREE(ch->category);

  if (ch->curve.curve) {
    RELEASE_OR_FREE_CURVE(ch->curve.curve);
  }

  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    BrushMapping *mp = ch->mappings + i;

    RELEASE_OR_FREE_CURVE(mp->curve);
  }
}

void BKE_brush_channel_free(BrushChannel *ch)
{
  BKE_brush_channel_free_data(ch);
  MEM_freeN(ch);
}

static void copy_channel_data_keep_mappings(BrushChannel *dst, BrushChannel *src)
{
  BLI_strncpy(dst->name, src->name, sizeof(dst->name));
  BLI_strncpy(dst->idname, src->idname, sizeof(dst->idname));

  dst->def = src->def;
  dst->flag = src->flag;
  dst->type = src->type;
  dst->ui_order = src->ui_order;

  switch (src->type) {
    case BRUSH_CHANNEL_TYPE_CURVE:
      dst->curve.preset = src->curve.preset;

      if (dst->curve.curve && IS_CACHE_CURVE(dst->curve.curve)) {
        RELEASE_CACHE_CURVE(dst->curve.curve);
      }
      else if (dst->curve.curve) {
        BKE_curvemapping_free(dst->curve.curve);
      }

      if (src->curve.curve && !IS_CACHE_CURVE(src->curve.curve)) {
        dst->curve.curve = BKE_curvemapping_cache_get(brush_curve_cache, src->curve.curve, false);
      }
      else {
        dst->curve.curve = src->curve.curve;
        if (dst->curve.curve) {
          CURVE_ADDREF(dst->curve.curve);
        }
      }
      break;
    case BRUSH_CHANNEL_TYPE_FLOAT:
      dst->fvalue = src->fvalue;
      break;
    case BRUSH_CHANNEL_TYPE_BOOL:
    case BRUSH_CHANNEL_TYPE_ENUM:
    case BRUSH_CHANNEL_TYPE_BITMASK:
    case BRUSH_CHANNEL_TYPE_INT:
      dst->ivalue = src->ivalue;
      break;
    case BRUSH_CHANNEL_TYPE_VEC3:
    case BRUSH_CHANNEL_TYPE_VEC4:
      copy_v4_v4(dst->vector, src->vector);
      break;
  }
}

void BKE_brush_channel_copy_data(BrushChannel *dst, BrushChannel *src, bool keep_mapping)
{
  if (keep_mapping) {
    copy_channel_data_keep_mappings(dst, src);
    return;
  }

  if (src->type == BRUSH_CHANNEL_TYPE_CURVE) {
    if (dst->curve.curve && IS_CACHE_CURVE(dst->curve.curve)) {
      RELEASE_CACHE_CURVE(dst->curve.curve);
    }
    else if (dst->curve.curve) {
      BKE_curvemapping_free(dst->curve.curve);
    }
  }

  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    BrushMapping *mp = dst->mappings + i;

    if (!mp->curve) {
      continue;
    }

    if (IS_CACHE_CURVE(mp->curve)) {
      RELEASE_CACHE_CURVE(mp->curve);
    }
    else {
      BKE_curvemapping_free(mp->curve);
    }

    mp->curve = NULL;
  }

  // preserve linked list pointers
  void *next = dst->next, *prev = dst->prev;

  *dst = *src;

  if (dst->category) {
    dst->category = BLI_strdup(dst->category);
  }

  if (src->curve.curve) {
    if (!IS_CACHE_CURVE(src->curve.curve)) {
      // dst->curve = GET_CACHE_CURVE(src->curve);

      // hrm, let's not modify src->curve, GET_CACHE_CURVE might free it
      dst->curve.curve = BKE_curvemapping_cache_get(brush_curve_cache, src->curve.curve, false);
    }
    else {
      CURVE_ADDREF(dst->curve.curve);
    }
  }

  dst->next = next;
  dst->prev = prev;

  namestack_push(__func__);

  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    dst->mappings[i].curve = NULL;

    BKE_brush_mapping_copy_data(dst->mappings + i, src->mappings + i);
    dst->mappings[i].type = i;
  }

  namestack_pop(NULL);
}

void BKE_brush_channel_init(BrushChannel *ch, BrushChannelType *def)
{
  // preserve linked list pointers
  BrushChannel *next = ch->next, *prev = ch->prev;

  memset(ch, 0, sizeof(*ch));
  ch->next = next;
  ch->prev = prev;

  strcpy(ch->name, def->name);
  strcpy(ch->idname, def->idname);

  ch->flag = def->flag;

  ch->curve.preset = def->curve_preset;
  ch->fvalue = def->fvalue;
  ch->ivalue = def->ivalue;
  copy_v4_v4(ch->vector, def->vector);

  ch->type = def->type;
  ch->def = def;

  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    BrushMapping *mp = ch->mappings + i;

    if (mp->curve) {
      RELEASE_OR_FREE_CURVE(mp->curve);
    }

    CurveMapping *curve = mp->curve = (CurveMapping *)MEM_callocN(sizeof(*curve),
                                                                  "CurveMapping for BrushMapping");
    mp->type = i;

    float min, max;

    BrushMappingDef *mdef = (&def->mappings.pressure) + i;

    if (mdef->min != mdef->max) {
      min = mdef->min;
      max = mdef->max;
    }
    else {
      min = 0.0f;
      max = 1.0f;
    }

    if (mdef->inv) {
      mp->flag |= BRUSH_MAPPING_INVERT;
    }

    int slope = CURVEMAP_SLOPE_POSITIVE;

    BKE_curvemapping_set_defaults(curve, 1, 0, min, 1, max);

    for (int j = 0; j < 1; j++) {
      BKE_curvemap_reset(&curve->cm[j],
                         &(struct rctf){.xmin = 0, .ymin = min, .xmax = 1, .ymax = max},
                         mdef->curve,
                         slope);
    }

    BKE_curvemapping_init(curve);

    mp->curve = GET_CACHE_CURVE(curve);  // frees curve and returns cached copy

    mp->blendmode = mdef->blendmode;
    mp->factor = mdef->factor == 0.0f ? 1.0f : mdef->factor;

    if (mdef->enabled) {
      mp->flag |= BRUSH_MAPPING_ENABLED;
    }
  }
}

CurveMapping *BKE_brush_channel_curvemapping_get(BrushCurve *curve, bool force_create)
{
  if ((force_create || curve->preset == BRUSH_CURVE_CUSTOM) && !curve->curve) {
    CurveMapping *cumap = curve->curve = MEM_callocN(sizeof(CurveMapping), "channel CurveMapping");

    BKE_curvemapping_set_defaults(cumap, 1, 0.0f, 0.0f, 1.0f, 1.0f);

    BKE_curvemap_reset(cumap->cm,
                       &(struct rctf){.xmin = 0, .ymin = 0.0, .xmax = 1.0, .ymax = 1.0},
                       CURVE_PRESET_LINE,
                       1);

    BKE_curvemapping_init(cumap);
  }

  return curve->curve;
}

float BKE_brush_channel_curve_evaluate(BrushChannel *ch, float val, const float maxval)
{
  BKE_brush_channel_curvemapping_get(&ch->curve, false);

  return BKE_brush_curve_strength_ex(ch->curve.preset, ch->curve.curve, val, maxval);
}

BrushChannelSet *BKE_brush_channelset_create()
{
  BrushChannelSet *chset = (BrushChannelSet *)MEM_callocN(sizeof(BrushChannelSet),
                                                          "BrushChannelSet");

  chset->namemap = BLI_ghash_str_new("BrushChannelSet");

  return chset;
}

void BKE_brush_channelset_free(BrushChannelSet *chset)
{
  BrushChannel *ch, *next;

  BLI_ghash_free(chset->namemap, NULL, NULL);

  for (ch = chset->channels.first; ch; ch = next) {
    next = ch->next;

    BKE_brush_channel_free(ch);
  }

  MEM_freeN(chset);
}

static int _rng_seed = 0;

void BKE_brush_channel_ensure_unque_name(BrushChannelSet *chset, BrushChannel *ch)
{
  BrushChannel *ch2;
  int i = 1;
  char idname[512];

  strcpy(idname, ch->idname);
  bool bad = true;

  RNG *rng = BLI_rng_new(_rng_seed++);

  while (bad) {
    bad = false;

    for (ch2 = chset->channels.first; ch2; ch2 = ch2->next) {
      if (ch2 != ch && STREQ(ch2->idname, ch->idname)) {
        bad = true;
        sprintf(idname, "%s %d", ch->idname, i);

        printf("%s: name collision: %s\n", __func__, idname);

        if (strlen(idname) > sizeof(ch->idname) - 1) {
          // we've hit the limit of idname;
          // start randomizing characters
          printf(
              "Cannot build unique name for brush channel; will have to randomize a few "
              "characters\n");
          printf("  requested idname: %s, ran out of buffer space at: %s\n", ch->idname, idname);

          int j = BLI_rng_get_int(rng) % strlen(ch->idname);
          int chr = (BLI_rng_get_int(rng) % ('a' - 'A')) + 'A';

          i = 0;
          ch->idname[j] = chr;
          strcpy(idname, ch->idname);
        }

        i++;
        break;
      }
    }
  }

  BLI_strncpy(ch->idname, idname, sizeof(ch->idname));

  // BLI_strncpy
  BLI_rng_free(rng);
}

void BKE_brush_channelset_add(BrushChannelSet *chset, BrushChannel *ch)
{
  BKE_brush_channel_ensure_unque_name(chset, ch);

  BLI_addtail(&chset->channels, ch);
  BLI_ghash_insert(chset->namemap, ch->idname, ch);

  chset->totchannel++;
}

void BKE_brush_channel_rename(BrushChannelSet *chset, BrushChannel *ch, const char *newname)
{
  BLI_ghash_remove(chset->namemap, ch->idname, NULL, NULL);
  BLI_strncpy(ch->idname, newname, sizeof(ch->idname));
  BKE_brush_channel_ensure_unque_name(chset, ch);
  BLI_ghash_insert(chset->namemap, ch->idname, ch);
}

void BKE_brush_channelset_remove(BrushChannelSet *chset, BrushChannel *ch)
{
  BLI_ghash_remove(chset->namemap, ch->idname, NULL, NULL);
  BLI_remlink(&chset->channels, ch);

  chset->totchannel--;
}

bool BKE_brush_channelset_remove_named(BrushChannelSet *chset, const char *idname)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);
  if (ch) {
    BKE_brush_channelset_remove(chset, ch);
    return true;
  }

  return false;
}

void BKE_brush_channelset_add_duplicate(BrushChannelSet *chset, BrushChannel *ch)
{
  namestack_push(__func__);

#ifdef DEBUG_CURVE_MAPPING_ALLOC
  BrushChannel *chnew = MEM_callocN(sizeof(*chnew), namestack_head_name);
#else
  BrushChannel *chnew = MEM_callocN(sizeof(*chnew), "brush channel copy");
#endif

  BKE_brush_channel_copy_data(chnew, ch, false);
  BKE_brush_channelset_add(chset, chnew);

  namestack_pop(NULL);
}

BrushChannel *BKE_brush_channelset_lookup(BrushChannelSet *chset, const char *idname)
{
  return BLI_ghash_lookup(chset->namemap, idname);
}

BrushChannel *BKE_brush_channelset_lookup_final(BrushChannelSet *child,
                                                BrushChannelSet *parent,
                                                const char *idname)
{
  BrushChannel *ch = child ? BKE_brush_channelset_lookup(child, idname) : NULL;
  BrushChannel *pch = parent ? BKE_brush_channelset_lookup(parent, idname) : NULL;

  if (ch && pch) {
    if (ch->flag & BRUSH_CHANNEL_INHERIT) {
      return pch;
    }

    return ch;
  }
  else if (pch) {
    return pch;
  }
  else {
    return ch;
  }
}

bool BKE_brush_channelset_has(BrushChannelSet *chset, const char *idname)
{
  return BKE_brush_channelset_lookup(chset, idname) != NULL;
}

BrushChannelType brush_default_channel_type = {
    .name = "Channel",
    .idname = "CHANNEL",
    .min = 0.0f,
    .max = 1.0f,
    .soft_min = 0.0f,
    .soft_max = 1.0f,
    .type = BRUSH_CHANNEL_TYPE_FLOAT,
    .flag = 0,
    .ivalue = 0,
    .fvalue = 0.0f,
    .mappings = {.pressure = {.curve = CURVE_PRESET_LINE,
                              .enabled = false,
                              .inv = false,
                              .blendmode = MA_RAMP_BLEND}}};

BrushChannelType *BKE_brush_default_channel_def()
{
  return &brush_default_channel_type;
}

void BKE_brush_channel_def_copy(BrushChannelType *dst, BrushChannelType *src)
{
  memcpy(dst, src, sizeof(*dst));
}

BrushChannelType *BKE_brush_builtin_channel_def_find(const char *name)
{
  for (int i = 0; i < brush_builtin_channel_len; i++) {
    BrushChannelType *def = brush_builtin_channels + i;

    if (STREQ(def->idname, name)) {
      return def;
    }
  }

  return NULL;
}

BrushChannel *BKE_brush_channelset_add_builtin(BrushChannelSet *chset, const char *idname)
{
  BrushChannelType *def = BKE_brush_builtin_channel_def_find(idname);

  if (!def) {
    printf("%s: Could not find brush %s\n", __func__, idname);
    return NULL;
  }

  namestack_push(__func__);

  BrushChannel *ch = MEM_callocN(sizeof(*ch), "BrushChannel");

  BKE_brush_channel_init(ch, def);
  BKE_brush_channelset_add(chset, ch);

  namestack_pop(NULL);

  return ch;
}

BrushChannel *BKE_brush_channelset_ensure_builtin(BrushChannelSet *chset, const char *idname)
{
  namestack_push(__func__);

  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);

  if (ch) {
    namestack_pop(NULL);
    return ch;
  }

  ch = BKE_brush_channelset_add_builtin(chset, idname);

  namestack_pop(NULL);
  return ch;
}

void BKE_brush_channelset_clear_inherit(BrushChannelSet *chset)
{
  BrushChannel *ch;

  for (ch = chset->channels.first; ch; ch = ch->next) {
    ch->flag &= ~(BRUSH_CHANNEL_INHERIT | BRUSH_CHANNEL_INHERIT_IF_UNSET);
  }
}

void BKE_brush_channelset_ensure_existing(BrushChannelSet *chset, BrushChannel *existing)
{
  if (BKE_brush_channelset_has(chset, existing->idname)) {
    return;
  }

  namestack_push(__func__);
  BKE_brush_channelset_add_duplicate(chset, existing);
  namestack_pop(NULL);
}

void BKE_brush_channelset_merge(BrushChannelSet *dst,
                                BrushChannelSet *child,
                                BrushChannelSet *parent)
{
  // first add missing channels
  namestack_push(__func__);

  for (int step = 0; step < 2; step++) {
    BrushChannelSet *chset = step ? parent : child;
    BrushChannel *ch;

    for (ch = chset->channels.first; ch; ch = ch->next) {
      BrushChannel *ch2 = BKE_brush_channelset_lookup(dst, ch->idname);

      if (ch2 && step > 0) {
        continue;
      }

      if (!ch2) {
        BKE_brush_channelset_add_duplicate(dst, ch);
      }
      else {
        BKE_brush_channel_copy_data(ch2, ch, false);
      }
    }
  }

  BrushChannel *pch;

  for (pch = parent->channels.first; pch; pch = pch->next) {
    BrushChannel *mch = BKE_brush_channelset_lookup(dst, pch->idname);
    BrushChannel *ch = BKE_brush_channelset_lookup(child, pch->idname);

    if (!ch) {
      continue;
    }

    if (ch->flag & BRUSH_CHANNEL_INHERIT) {
      BKE_brush_channel_copy_data(mch, pch, true);
    }

    /*apply mapping inheritance flags, which are respected
      for non inherited channels.  note that absense
      of BRUSH_MAPPING_FLAG doen't prevent mapping inheritance
      if BRUSH_CHANNEL_INHERIT in ch->flag *is* set.*/
    for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
      if (ch->mappings[i].flag & BRUSH_MAPPING_INHERIT) {
        BKE_brush_mapping_copy_data(mch->mappings + i, pch->mappings + i);
      }
    }

    if (ch->flag & BRUSH_CHANNEL_INHERIT) {
      continue;
    }

    /*TODO: should inherit if unset should always apply, i.e. this block should be moved above the
     * previous one?*/
    if (ch->type == BRUSH_CHANNEL_TYPE_BITMASK && (ch->flag & BRUSH_CHANNEL_INHERIT_IF_UNSET)) {
      mch->ivalue = ch->ivalue | pch->ivalue;
    }
  }

  namestack_pop(NULL);
}

#ifdef DEBUG_CURVE_MAPPING_ALLOC
BrushChannelSet *_BKE_brush_channelset_copy(BrushChannelSet *src)
#else
BrushChannelSet *BKE_brush_channelset_copy(BrushChannelSet *src)
#endif
{
  BrushChannelSet *chset = BKE_brush_channelset_create();

  if (!src->totchannel) {
    return chset;
  }

  namestack_push(__func__);

  BrushChannel *ch;
  for (ch = src->channels.first; ch; ch = ch->next) {
    BKE_brush_channelset_add_duplicate(chset, ch);
  }

  namestack_pop(NULL);

  return chset;
}

void BKE_brush_channelset_apply_mapping(BrushChannelSet *chset, BrushMappingData *mapdata)
{
  BrushChannel *ch;
  int n;

  for (ch = chset->channels.first; ch; ch = ch->next) {
    switch (ch->type) {
      case BRUSH_CHANNEL_TYPE_FLOAT:
        ch->fvalue = BKE_brush_channel_get_float(ch, mapdata);
        break;
      case BRUSH_CHANNEL_TYPE_INT:
      case BRUSH_CHANNEL_TYPE_ENUM:
      case BRUSH_CHANNEL_TYPE_BITMASK:
      case BRUSH_CHANNEL_TYPE_BOOL:
        ch->ivalue = BKE_brush_channel_get_int(ch, mapdata);
        break;
      case BRUSH_CHANNEL_TYPE_VEC4:
        n = 4;
      case BRUSH_CHANNEL_TYPE_VEC3:
        n = 3;

        for (int i = 0; i < n; i++) {
          ch->vector[i] = (float)BKE_brush_channel_eval_mappings(
              ch, mapdata, (double)ch->vector[i], i);
        }
        break;
    }

    // disable input mapping
    for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
      ch->mappings[i].flag &= ~BRUSH_MAPPING_ENABLED;
    }
  }
}

void BKE_brush_commandlist_start(BrushCommandList *list,
                                 Brush *brush,
                                 BrushChannelSet *chset_final)
{
  for (int i = 0; i < list->totcommand; i++) {
    BrushCommand *cmd = list->commands + i;

    // Build final list of command parameters
    if (cmd->params_final) {
      BKE_brush_channelset_free(cmd->params_final);
    }
    cmd->params_final = BKE_brush_channelset_create();

    BKE_brush_channelset_merge(cmd->params_final, cmd->params, chset_final);

    if (cmd->params_mapped) {
      BKE_brush_channelset_free(cmd->params_mapped);
    }

    cmd->params_mapped = BKE_brush_channelset_copy(cmd->params_final);
  }
}

void BKE_brush_resolve_channels(Brush *brush, Sculpt *sd)
{
  if (brush->channels_final) {
    BKE_brush_channelset_free(brush->channels_final);
  }

  brush->channels_final = BKE_brush_channelset_create();

  BKE_brush_channelset_merge(brush->channels_final, brush->channels, sd->channels);
}

static bool channel_has_mappings(BrushChannel *ch)
{
  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    if (ch->mappings[i].flag & BRUSH_MAPPING_ENABLED) {
      return true;
    }
  }

  return false;
}

// idx is used by vector channels
double BKE_brush_channel_eval_mappings(BrushChannel *ch,
                                       BrushMappingData *mapdata,
                                       double f,
                                       int idx)
{

  if (idx == 3 && !(ch->flag & BRUSH_CHANNEL_APPLY_MAPPING_TO_ALPHA)) {
    return f;
  }

  if (mapdata) {
    double factor = 1.0f;

    for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
      BrushMapping *mp = ch->mappings + i;

      if (!(mp->flag & BRUSH_MAPPING_ENABLED)) {
        continue;
      }

      float inputf = ((float *)mapdata)[i];

      if (mp->flag & BRUSH_MAPPING_INVERT) {
        inputf = 1.0 - inputf;
      }

      double f2 = (float)BKE_curvemapping_evaluateF(mp->curve, 0, inputf);

      switch (mp->blendmode) {
        case MA_RAMP_BLEND:
          break;
        case MA_RAMP_MULT:
          f2 *= factor;
          break;
        case MA_RAMP_DIV:
          f2 = factor / (0.00001f + f2);
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

    f *= factor;
  }

  return f;
}

int BKE_brush_channelset_get_int(BrushChannelSet *chset,
                                 const char *idname,
                                 BrushMappingData *mapdata)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);

  if (!ch) {
    printf("%s, unknown channel %s", __func__, idname);
    return 0;
  }

  return BKE_brush_channel_get_int(ch, mapdata);
}

float BKE_brush_channel_get_int(BrushChannel *ch, BrushMappingData *mapdata)
{

  if (channel_has_mappings(ch)) {
    return (int)BKE_brush_channel_eval_mappings(ch, mapdata, (double)ch->ivalue, 0);
  }
  else {
    return ch->ivalue;
  }
}

void BKE_brush_channel_set_int(BrushChannel *ch, int val)
{
  ch->ivalue = val;
}

int BKE_brush_channelset_get_final_int(BrushChannelSet *child,
                                       BrushChannelSet *parent,
                                       const char *idname,
                                       BrushMappingData *mapdata)
{
  if (!parent) {
    return BKE_brush_channelset_get_int(child, idname, mapdata);
  }

  BrushChannel *ch = BKE_brush_channelset_lookup(child, idname);

  if (!ch || (ch->flag & BRUSH_CHANNEL_INHERIT)) {
    BrushChannel *pch = BKE_brush_channelset_lookup(parent, idname);

    if (pch) {
      return BKE_brush_channel_get_int(pch, mapdata);
    }
  }

  if (ch) {
    return BKE_brush_channel_get_int(ch, mapdata);
  }

  printf("%s: failed to find brush channel %s\n", __func__, idname);

  return 0;
}

void BKE_brush_channelset_set_final_int(BrushChannelSet *child,
                                        BrushChannelSet *parent,
                                        const char *idname,
                                        int value)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(child, idname);

  if (!parent) {
    BKE_brush_channelset_set_int(child, idname, value);
  }

  if (!ch || (ch->flag & BRUSH_CHANNEL_INHERIT)) {
    BrushChannel *pch = BKE_brush_channelset_lookup(parent, idname);

    if (pch) {
      BKE_brush_channel_set_int(pch, value);
      return;
    }
  }

  if (!ch) {
    printf("%s: failed to find brush channel %s\n", __func__, idname);
    return;
  }

  BKE_brush_channel_set_int(ch, value);
}

float BKE_brush_channelset_get_final_float(BrushChannelSet *brushset,
                                           BrushChannelSet *toolset,
                                           const char *idname,
                                           BrushMappingData *mapdata)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(brushset, idname);

  if (!ch || (ch->flag & BRUSH_CHANNEL_INHERIT)) {
    BrushChannel *pch = BKE_brush_channelset_lookup(toolset, idname);

    if (pch) {
      return BKE_brush_channel_get_float(pch, mapdata);
    }
  }

  if (ch) {
    return BKE_brush_channel_get_float(ch, mapdata);
  }

  printf("%s: failed to find brush channel %s\n", __func__, idname);

  return 0.0f;
}

void BKE_brush_channelset_set_final_float(BrushChannelSet *brushset,
                                          BrushChannelSet *toolset,
                                          const char *idname,
                                          float value)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(brushset, idname);

  if (!ch || (ch->flag & BRUSH_CHANNEL_INHERIT)) {
    BrushChannel *pch = BKE_brush_channelset_lookup(toolset, idname);

    if (pch) {
      BKE_brush_channel_set_float(pch, value);
      return;
    }
  }

  if (!ch) {
    printf("%s: failed to find brush channel %s\n", __func__, idname);
    return;
  }

  BKE_brush_channel_set_float(ch, value);
}

float BKE_brush_channelset_get_float(BrushChannelSet *chset,
                                     const char *idname,
                                     BrushMappingData *mapdata)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);

  if (!ch) {
    printf("%s, unknown channel %s", __func__, idname);
    return 0.0f;
  }

  return BKE_brush_channel_get_float(ch, mapdata);
}

float BKE_brush_channel_get_float(BrushChannel *ch, BrushMappingData *mapdata)
{
  return (float)BKE_brush_channel_eval_mappings(ch, mapdata, (double)ch->fvalue, 0);
}

void BKE_brush_channel_set_vector(BrushChannel *ch, float vec[4])
{
  if (ch->type == BRUSH_CHANNEL_TYPE_VEC4) {
    copy_v4_v4(ch->vector, vec);
  }
  else {
    copy_v3_v3(ch->vector, vec);
  }
}

int BKE_brush_channel_get_vector_size(BrushChannel *ch)
{
  switch (ch->type) {
    case BRUSH_CHANNEL_TYPE_VEC3:
      return 3;
    case BRUSH_CHANNEL_TYPE_VEC4:
      return 4;
    default:
      return 1;
  }
}

int BKE_brush_channel_get_vector(BrushChannel *ch, float out[4], BrushMappingData *mapdata)
{
  int size = 3;
  if (ch->type == BRUSH_CHANNEL_TYPE_VEC4) {
    size = 4;
  }

  for (int i = 0; i < 4; i++) {
    out[i] = BKE_brush_channel_eval_mappings(ch, mapdata, (float)ch->vector[i], i);
  }

  return size;
}

float BKE_brush_channelset_get_final_vector(BrushChannelSet *brushset,
                                            BrushChannelSet *toolset,
                                            const char *idname,
                                            float r_vec[4],
                                            BrushMappingData *mapdata)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(brushset, idname);

  if (!ch || (ch->flag & BRUSH_CHANNEL_INHERIT)) {
    BrushChannel *pch = BKE_brush_channelset_lookup(toolset, idname);

    if (pch) {
      return BKE_brush_channel_get_vector(pch, r_vec, mapdata);
    }
  }

  if (ch) {
    return BKE_brush_channel_get_vector(ch, r_vec, mapdata);
  }

  printf("%s: failed to find brush channel %s\n", __func__, idname);

  return 0.0f;
}

void BKE_brush_channelset_set_final_vector(BrushChannelSet *brushset,
                                           BrushChannelSet *toolset,
                                           const char *idname,
                                           float vec[4])
{
  BrushChannel *ch = BKE_brush_channelset_lookup(brushset, idname);

  if (!ch || (ch->flag & BRUSH_CHANNEL_INHERIT)) {
    BrushChannel *pch = BKE_brush_channelset_lookup(toolset, idname);

    if (pch) {
      BKE_brush_channel_set_vector(pch, vec);
      return;
    }
  }

  if (!ch) {
    printf("%s: failed to find brush channel %s\n", __func__, idname);
    return;
  }

  BKE_brush_channel_set_vector(ch, vec);
}

int BKE_brush_channelset_get_vector(BrushChannelSet *chset,
                                    const char *idname,
                                    float r_vec[4],
                                    BrushMappingData *mapdata)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);

  if (!ch) {
    printf("%s, unknown channel %s", __func__, idname);
    return 0.0f;
  }

  return BKE_brush_channel_get_vector(ch, r_vec, mapdata);
}

bool BKE_brush_channelset_set_vector(BrushChannelSet *chset, const char *idname, float vec[4])
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);

  if (!ch) {
    printf("%s, unknown channel %s", __func__, idname);
    return false;
  }

  BKE_brush_channel_set_vector(ch, vec);

  return true;
}

void BKE_brush_channel_set_float(BrushChannel *ch, float val)
{
  ch->fvalue = val;
}

bool BKE_brush_channelset_set_float(BrushChannelSet *chset, const char *idname, float val)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);

  if (!ch) {
    printf("%s, unknown channel %s", __func__, idname);
    return 0;
  }

  BKE_brush_channel_set_float(ch, val);

  return true;
}

bool BKE_brush_channelset_set_int(BrushChannelSet *chset, const char *idname, int val)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, idname);

  if (!ch) {
    printf("%s, unknown channel %s", __func__, idname);
    return 0;
  }

  BKE_brush_channel_set_int(ch, val);

  return true;
}

void BKE_brush_channelset_flag_clear(BrushChannelSet *chset, const char *channel, int flag)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, channel);

  if (!ch) {
    printf("%s: unknown channel '%s'\n", __func__, channel);
    return;
  }

  ch->flag &= ~flag;
}

void BKE_brush_channelset_flag_set(BrushChannelSet *chset, const char *channel, int flag)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, channel);

  if (!ch) {
    printf("%s: unknown channel '%s'\n", __func__, channel);
    return;
  }

  ch->flag |= flag;
}

BrushCommandList *BKE_brush_commandlist_create()
{
  return MEM_callocN(sizeof(BrushCommandList), "BrushCommandList");
}
void BKE_brush_commandlist_free(BrushCommandList *cl)
{
  for (int i = 0; i < cl->totcommand; i++) {
    BrushCommand *cmd = cl->commands + i;

    if (cmd->params) {
      BKE_brush_channelset_free(cmd->params);
    }

    if (cmd->params_final) {
      BKE_brush_channelset_free(cmd->params_final);
    }

    if (cmd->params_mapped) {
      BKE_brush_channelset_free(cmd->params_mapped);
    }
  }

  MEM_SAFE_FREE(cl->commands);

  MEM_freeN(cl);
}
BrushCommand *BKE_brush_commandlist_add(BrushCommandList *cl,
                                        BrushChannelSet *chset_template,
                                        bool auto_inherit)
{
  cl->totcommand++;

  if (!cl->commands) {
    cl->commands = MEM_callocN(sizeof(BrushCommand) * cl->totcommand, "BrushCommand");
  }
  else {
    cl->commands = MEM_recallocN_id(
        cl->commands, sizeof(BrushCommand) * cl->totcommand, "cl->commands");
  }

  BrushCommand *cmd = cl->commands + cl->totcommand - 1;

  if (chset_template) {
    cmd->params = BKE_brush_channelset_copy(chset_template);

    if (auto_inherit) {
      BrushChannel *ch;

      for (ch = cmd->params->channels.first; ch; ch = ch->next) {
        ch->flag |= BRUSH_CHANNEL_INHERIT;
      }
    }
  }
  else {
    cmd->params = BKE_brush_channelset_create();
  }

  cmd->params_final = NULL;

  return cmd;
}

#ifdef ADDCH
#  undef ADDCH
#endif

#define ADDCH(name) BKE_brush_channelset_ensure_builtin(chset, name)

BrushCommand *BKE_brush_command_init(BrushCommand *command, int tool)
{
  BrushChannelSet *chset = command->params;

  namestack_push(__func__);

  command->tool = tool;

  ADDCH("spacing");
  ADDCH("radius");
  ADDCH("strength");
  ADDCH("hard_edge_mode");

  switch (tool) {
    case SCULPT_TOOL_DRAW:
      break;
    case SCULPT_TOOL_SMOOTH:
      ADDCH("boundary_smooth");
      ADDCH("projection");
      ADDCH("boundary_smooth");
      ADDCH("fset_slide");
      ADDCH("preserve_faceset_boundary");
      break;
    case SCULPT_TOOL_TOPOLOGY_RAKE:
      ADDCH("fset_slide");
      ADDCH("preserve_faceset_boundary");
      ADDCH("boundary_smooth");
      ADDCH("projection");
      ADDCH("topology_rake_mode");
      break;
    case SCULPT_TOOL_DYNTOPO:
      ADDCH("fset_slide");
      ADDCH("preserve_faceset_boundary");
      break;
  }

  BrushChannel *ch;
  for (ch = command->params->channels.first; ch; ch = ch->next) {
    ch->flag |= BRUSH_CHANNEL_INHERIT;
  }

  namestack_pop(NULL);

  return command;
}

#define float_set_uninherit(chset, channel, val) \
  _float_set_uninherit(chset, MAKE_BUILTIN_CH_NAME(channel), val)

#define int_set_uninherit(chset, channel, val) \
  _int_set_uninherit(chset, MAKE_BUILTIN_CH_NAME(channel), val)

static void _float_set_uninherit(BrushChannelSet *chset, const char *channel, float val)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, channel);

  if (!ch) {
    printf("%s: unknown channel %s\n", __func__, channel);
    return;
  }

  ch->fvalue = val;
  ch->flag &= ~BRUSH_CHANNEL_INHERIT;
}

static void _int_set_uninherit(BrushChannelSet *chset, const char *channel, int val)
{
  BrushChannel *ch = BKE_brush_channelset_lookup(chset, channel);

  if (!ch) {
    printf("%s: unknown channel %s\n", __func__, channel);
    return;
  }

  ch->ivalue = val;
  ch->flag &= ~BRUSH_CHANNEL_INHERIT;
}

/*flag all mappings to use inherited curves even if owning channel
  is not set to inherit.*/
void BKE_brush_commandset_inherit_all_mappings(BrushChannelSet *chset)
{
  BrushChannel *ch;
  for (ch = chset->channels.first; ch; ch = ch->next) {
    for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
      ch->mappings[i].flag |= BRUSH_MAPPING_INHERIT;
    }
  }
}

static void commandlist_add_dyntopo(BrushChannelSet *chset,
                                    BrushCommandList *cl,
                                    Brush *brush,
                                    int tool,
                                    bool hard_edge_mode,
                                    float radius_base)
{

  if (!BKE_brush_channelset_get_int(chset, "dyntopo_disabled", NULL)) {
    BrushCommand *cmd = BKE_brush_command_init(BKE_brush_commandlist_add(cl, chset, true),
                                               SCULPT_TOOL_DYNTOPO);
    BKE_builtin_apply_hard_edge_mode(cmd->params, hard_edge_mode);

    float spacing = BKE_brush_channelset_get_float(chset, "dyntopo_spacing", NULL);
    float radius2 = BKE_brush_channelset_get_float(chset, "dyntopo_radius_scale", NULL);

    radius2 *= radius_base;

    int_set_uninherit(cmd->params, use_ctrl_invert, false);
    float_set_uninherit(cmd->params, spacing, spacing);
    float_set_uninherit(cmd->params, radius, radius2);

    BKE_brush_commandset_inherit_all_mappings(cmd->params);
  }
}
static void bke_builtin_commandlist_create_paint(Brush *brush,
                                                 BrushChannelSet *chset,
                                                 BrushCommandList *cl,
                                                 int tool,
                                                 BrushMappingData *mapdata)
{
  BrushCommand *cmd;

  cmd = BKE_brush_commandlist_add(cl, chset, true);
  BKE_brush_command_init(cmd, tool);
  BKE_brush_commandset_inherit_all_mappings(cmd->params);

  float radius = BRUSHSET_GET_FLOAT(chset, radius, NULL);

  /* build autosmooth command */
  float autosmooth_scale = BRUSHSET_GET_FLOAT(chset, autosmooth_radius_scale, NULL);
  float autosmooth_projection = BRUSHSET_GET_FLOAT(chset, autosmooth_projection, NULL);

  float autosmooth_spacing;

  if (BRUSHSET_GET_INT(chset, autosmooth_use_spacing, NULL)) {
    autosmooth_spacing = BRUSHSET_GET_FLOAT(chset, autosmooth_spacing, NULL);
  }
  else {
    autosmooth_spacing = BRUSHSET_GET_FLOAT(chset, spacing, NULL);
  }

  float autosmooth = BRUSHSET_GET_FLOAT(chset, autosmooth, NULL);
  if (autosmooth > 0.0f) {
    cmd = BKE_brush_command_init(BKE_brush_commandlist_add(cl, chset, true), SCULPT_TOOL_SMOOTH);

    BrushChannel *ch = BRUSHSET_ENSURE_BUILTIN(cmd->params, falloff_curve);
    BrushChannel *ch2 = BRUSHSET_LOOKUP(chset, autosmooth_falloff_curve);

    if (ch2) {
      BKE_brush_channel_curve_assign(ch, &ch2->curve);
      ch->flag &= ~BRUSH_CHANNEL_INHERIT;
    }
    else {
      ch->flag |= BRUSH_CHANNEL_INHERIT;
    }

    int_set_uninherit(cmd->params, use_ctrl_invert, false);
    float_set_uninherit(cmd->params, strength, autosmooth);
    float_set_uninherit(cmd->params, radius, radius * autosmooth_scale);
    float_set_uninherit(cmd->params, projection, autosmooth_projection);
    float_set_uninherit(cmd->params, spacing, autosmooth_spacing);

    BKE_brush_commandset_inherit_all_mappings(cmd->params);
  }

  float vcol_boundary = BKE_brush_channelset_get_float(chset, "vcol_boundary_factor", NULL);
#define GETF(key) BKE_brush_channelset_get_float(chset, key, NULL)

  if (vcol_boundary > 0.0f) {
    cmd = BKE_brush_command_init(BKE_brush_commandlist_add(cl, chset, true),
                                 SCULPT_TOOL_VCOL_BOUNDARY);

    float_set_uninherit(cmd->params, radius, radius * GETF("vcol_boundary_radius_scale"));
    float_set_uninherit(cmd->params, spacing, GETF("vcol_boundary_spacing"));
    float_set_uninherit(cmd->params, strength, vcol_boundary);

    BKE_brush_commandset_inherit_all_mappings(cmd->params);
  }

#undef GETF

  bool hard_edge_mode = BRUSHSET_GET_INT(chset, hard_edge_mode, NULL);
  commandlist_add_dyntopo(chset, cl, brush, tool, hard_edge_mode, radius);

  // float
}

void BKE_builtin_apply_hard_edge_mode(BrushChannelSet *chset, bool do_apply)
{
  if (!do_apply) {
    return;
  }

  // hard edge mode overrides fset_slide to be 0.0.
  BrushChannel *ch = BRUSHSET_LOOKUP(chset, fset_slide);

  if (ch) {
    // clear inheritance flag
    ch->flag &= ~BRUSH_CHANNEL_INHERIT;
    ch->fvalue = 0.0f;
  }

  // make sure preserve faceset boundaries is on
  ch = BRUSHSET_LOOKUP(chset, preserve_faceset_boundary);

  if (ch) {
    ch->flag &= ~BRUSH_CHANNEL_INHERIT;
    ch->ivalue = 1;
  }

  // turn off dyntopo surface smoothing
  ch = BRUSHSET_LOOKUP(chset, dyntopo_disable_smooth);
  if (ch) {
    ch->flag &= ~BRUSH_CHANNEL_INHERIT;
    ch->ivalue = 1;
  }
}

void BKE_builtin_commandlist_create(Brush *brush,
                                    BrushChannelSet *chset,
                                    BrushCommandList *cl,
                                    int tool,
                                    BrushMappingData *mapdata)
{
  BrushCommand *cmd;
  BrushChannel *ch;

  bool hard_edge_mode = BRUSHSET_GET_INT(chset, hard_edge_mode, NULL);

  /* add main tool */
  if (ELEM(tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
    bke_builtin_commandlist_create_paint(brush, chset, cl, tool, mapdata);
    return;
  }

  cmd = BKE_brush_commandlist_add(cl, chset, true);
  BKE_brush_command_init(cmd, tool);
  BKE_builtin_apply_hard_edge_mode(cmd->params, hard_edge_mode);
  BKE_brush_commandset_inherit_all_mappings(cmd->params);

  float radius = BKE_brush_channelset_get_float(chset, "radius", NULL);

  bool no_autosmooth = ELEM(tool, SCULPT_TOOL_BOUNDARY, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK);
  bool no_rake = ELEM(tool, SCULPT_TOOL_BOUNDARY, SCULPT_TOOL_MASK);
  ;

  /* build autosmooth command */
  float autosmooth_scale = BKE_brush_channelset_get_float(chset, "autosmooth_radius_scale", NULL);
  float autosmooth_projection = BKE_brush_channelset_get_float(
      chset, "autosmooth_projection", NULL);

  bool is_cloth = tool = SCULPT_TOOL_CLOTH;
  is_cloth = is_cloth ||
             (ELEM(tool, SCULPT_TOOL_BOUNDARY, SCULPT_TOOL_POSE) &&
              BRUSHSET_GET_INT(chset, deform_target, NULL) == BRUSH_DEFORM_TARGET_CLOTH_SIM);
  float cloth_radius_mul = 1.0f;

  if (is_cloth && (ch = BRUSHSET_LOOKUP(chset, cloth_sim_limit))) {
    cloth_radius_mul += ch->fvalue;
    autosmooth_scale *= cloth_radius_mul;
  }

  float autosmooth_spacing;

  if (BKE_brush_channelset_get_int(chset, "autosmooth_use_spacing", NULL)) {
    autosmooth_spacing = BKE_brush_channelset_get_float(chset, "autosmooth_spacing", NULL);
  }
  else {
    autosmooth_spacing = BKE_brush_channelset_get_float(chset, "spacing", NULL);
  }

  float autosmooth = BKE_brush_channelset_get_float(chset, "autosmooth", NULL);
  if (!no_autosmooth && autosmooth > 0.0f) {
    cmd = BKE_brush_command_init(BKE_brush_commandlist_add(cl, chset, true), SCULPT_TOOL_SMOOTH);
    BKE_builtin_apply_hard_edge_mode(cmd->params, hard_edge_mode);
    BKE_brush_commandset_inherit_all_mappings(cmd->params);

    BrushChannel *ch = BRUSHSET_ENSURE_BUILTIN(cmd->params, falloff_curve);
    BrushChannel *ch2 = BRUSHSET_LOOKUP(chset, autosmooth_falloff_curve);

    if (ch2) {
      BKE_brush_channel_curve_assign(ch, &ch2->curve);
      ch->flag &= ~BRUSH_CHANNEL_INHERIT;
    }
    else {
      ch->flag |= BRUSH_CHANNEL_INHERIT;
    }

    ch = BRUSHSET_LOOKUP(cmd->params, strength);
    ch2 = BRUSHSET_LOOKUP(chset, autosmooth);

    for (int j = 0; j < BRUSH_MAPPING_MAX; j++) {
      BKE_brush_mapping_copy_data(ch->mappings + j, ch2->mappings + j);
    }

    int_set_uninherit(cmd->params, use_ctrl_invert, false);
    float_set_uninherit(cmd->params, strength, autosmooth);
    float_set_uninherit(cmd->params, radius, radius * autosmooth_scale);
    float_set_uninherit(cmd->params, projection, autosmooth_projection);
    float_set_uninherit(cmd->params, spacing, autosmooth_spacing);
  }

  float topology_rake_scale = BKE_brush_channelset_get_float(
                                  chset, "topology_rake_radius_scale", NULL) *
                              cloth_radius_mul;
  float topology_rake_projection = BKE_brush_channelset_get_float(
      chset, "topology_rake_projection", NULL);

  /* build topology rake command*/
  float topology_rake = BKE_brush_channelset_get_float(chset, "topology_rake", NULL);
  float topology_rake_spacing;

  if (BKE_brush_channelset_get_int(chset, "topology_rake_use_spacing", NULL)) {
    topology_rake_spacing = BKE_brush_channelset_get_float(chset, "topology_rake_spacing", NULL);
  }
  else {
    topology_rake_spacing = BKE_brush_channelset_get_float(chset, "spacing", NULL);
  }

  if (!no_rake && topology_rake > 0.0f) {
    cmd = BKE_brush_command_init(BKE_brush_commandlist_add(cl, chset, true),
                                 SCULPT_TOOL_TOPOLOGY_RAKE);
    BKE_builtin_apply_hard_edge_mode(cmd->params, hard_edge_mode);

    BrushChannel *ch = BRUSHSET_ENSURE_BUILTIN(cmd->params, falloff_curve);
    BrushChannel *ch2 = BRUSHSET_LOOKUP(chset, topology_rake_falloff_curve);

    if (ch2) {
      BKE_brush_channel_curve_assign(ch, &ch2->curve);
      ch->flag &= ~BRUSH_CHANNEL_INHERIT;
    }
    else {
      ch->flag |= BRUSH_CHANNEL_INHERIT;
    }

    int_set_uninherit(cmd->params, use_ctrl_invert, false);
    float_set_uninherit(cmd->params, strength, topology_rake);
    float_set_uninherit(cmd->params, radius, radius * topology_rake_scale);
    float_set_uninherit(cmd->params, projection, topology_rake_projection);
    float_set_uninherit(cmd->params, spacing, topology_rake_spacing);

    BKE_brush_commandset_inherit_all_mappings(cmd->params);
  }

  /* build dyntopo command */
  commandlist_add_dyntopo(chset, cl, brush, tool, hard_edge_mode, radius);
}

void BKE_brush_channelset_read_lib(BlendLibReader *reader, ID *id, BrushChannelSet *chset)
{
}

void BKE_brush_channelset_expand(BlendExpander *expander, ID *id, BrushChannelSet *chset)
{
}

void BKE_brush_channelset_foreach_id(LibraryForeachIDData *data, BrushChannelSet *chset)
{
}

void BKE_brush_channelset_read(BlendDataReader *reader, BrushChannelSet *chset)
{
  BLO_read_list(reader, &chset->channels);

  chset->namemap = BLI_ghash_str_new("BrushChannelSet");

  BrushChannel *ch;
  // regenerate chset->totchannel just to be safe
  chset->totchannel = 0;

  for (ch = chset->channels.first; ch; ch = ch->next) {
    chset->totchannel++;

    BLI_ghash_insert(chset->namemap, ch->idname, ch);

    BLO_read_data_address(reader, &ch->curve.curve);
    if (ch->curve.curve) {
      BKE_curvemapping_blend_read(reader, ch->curve.curve);
      BKE_curvemapping_init(ch->curve.curve);
    }

    for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
      BrushMapping *mp = ch->mappings + i;

      BLO_read_data_address(reader, &mp->curve);

      CurveMapping *curve = mp->curve;

      if (curve) {
        BKE_curvemapping_blend_read(reader, curve);
        BKE_curvemapping_init(curve);
      }
      else {
        curve = mp->curve = MEM_callocN(sizeof(CurveMapping), "CurveMapping");

        BKE_curvemapping_set_defaults(curve, 1, 0.0f, 0.0f, 1.0f, 1.0f);
        BKE_curvemap_reset(curve->cm,
                           &(struct rctf){.xmin = 0, .ymin = 0.0, .xmax = 1.0, .ymax = 1.0},
                           CURVE_PRESET_LINE,
                           !(mp->flag & BRUSH_MAPPING_INVERT));

        BKE_curvemapping_init(curve);
      }

      ch->mappings[i].curve = GET_CACHE_CURVE(curve);  // frees curve, returns new one

      // paranoia check to make sure BrushMapping.type is correct
      mp->type = i;
    }

    ch->def = BKE_brush_builtin_channel_def_find(ch->idname);

    if (!ch->def) {
      printf("failed to find brush definition for %s\n", ch->idname);
      ch->def = BKE_brush_default_channel_def();
    }
    else {
      // ensure ->type is correct
      ch->type = ch->def->type;
    }
  }
}

void BKE_brush_channelset_write(BlendWriter *writer, BrushChannelSet *chset)
{
  BLO_write_struct(writer, BrushChannelSet, chset);
  BLO_write_struct_list(writer, BrushChannel, &chset->channels);

  BrushChannel *ch;
  for (ch = chset->channels.first; ch; ch = ch->next) {
    if (ch->curve.curve) {
      BKE_curvemapping_blend_write(writer, ch->curve.curve);
    }

    for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
      BKE_brush_mapping_ensure_write(ch->mappings + i);
      BKE_curvemapping_blend_write(writer, ch->mappings[i].curve);
    }
  }
}

const char *BKE_brush_mapping_type_to_str(BrushMappingType mapping)
{
  switch (mapping) {
    case BRUSH_MAPPING_PRESSURE:
      return "Pressure";
    case BRUSH_MAPPING_ANGLE:
      return "Angle";
    case BRUSH_MAPPING_SPEED:
      return "Speed";
    case BRUSH_MAPPING_XTILT:
      return "X Tilt";
    case BRUSH_MAPPING_YTILT:
      return "Y Tilt";
    case BRUSH_MAPPING_MAX:
      return "Error";
  }

  return "Error";
}

const char *BKE_brush_mapping_type_to_typename(BrushMappingType mapping)
{
  switch (mapping) {
    case BRUSH_MAPPING_PRESSURE:
      return "PRESSURE";
    case BRUSH_MAPPING_ANGLE:
      return "ANGLE";
    case BRUSH_MAPPING_SPEED:
      return "SPEED";
    case BRUSH_MAPPING_XTILT:
      return "XTILT";
    case BRUSH_MAPPING_YTILT:
      return "YTILT";
    case BRUSH_MAPPING_MAX:
      return "Error";
  }

  return "Error";
}

void BKE_brush_mapping_copy_data(BrushMapping *dst, BrushMapping *src)
{
  RELEASE_OR_FREE_CURVE(dst->curve);

  if (!IS_CACHE_CURVE(src->curve)) {
    // dst->curve = GET_CACHE_CURVE(src->curve);

    // hrm, let's not modify src->curve, GET_CACHE_CURVE might free it
    dst->curve = BKE_curvemapping_cache_get(brush_curve_cache, src->curve, false);
  }
  else {
    dst->curve = src->curve;
    CURVE_ADDREF(dst->curve);
  }

  dst->blendmode = src->blendmode;
  dst->factor = src->factor;
  dst->flag = src->flag;
  dst->input_channel = src->input_channel;

  memcpy(dst->name, src->name, sizeof(dst->name));
}

void BKE_brush_channelset_to_unified_settings(BrushChannelSet *chset, UnifiedPaintSettings *ups)
{
  BrushChannel *ch;

  if (ch = BRUSHSET_LOOKUP(chset, radius)) {
    ups->size = ch->fvalue;
  }

  if (ch = BRUSHSET_LOOKUP(chset, strength)) {
    ups->alpha = ch->fvalue;
  }

  if (ch = BRUSHSET_LOOKUP(chset, weight)) {
    ups->weight = ch->fvalue;
  }
}

BrushTex *BKE_brush_tex_create()
{
  BrushTex *bt = MEM_callocN(sizeof(BrushTex), "BrushTex");

  bt->channels = BKE_brush_channelset_create();

  return bt;
}

void BKE_brush_tex_free(BrushTex *btex)
{
  BKE_brush_channelset_free(btex->channels);
  MEM_freeN(btex);
}

const char *BKE_brush_channel_category_get(BrushChannel *ch)
{
  return ch->category ? ch->category : ch->def->category;
}

void BKE_brush_channel_category_set(BrushChannel *ch, const char *str)
{
  MEM_SAFE_FREE(ch->category);

  ch->category = BLI_strdup(str);
}

/* idea for building built-in preset node graphs:
from brush_builder import Builder;

def build(input, output):
input.add("Strength", "float", "strength").range(0.0, 3.0)
input.add("Radius", "float", "radius").range(0.01, 1024.0)
input.add("Autosmooth", "float", "autosmooth").range(0.0, 4.0)
input.add("Topology Rake", "float", "topology rake").range(0.0, 4.0)
input.add("Smooth Radius Scale", "float", "autosmooth_radius_scale").range(0.01, 5.0)
input.add("Rake Radius Scale", "float", "toporake_radius_scale").range(0.01, 5.0)

draw = input.make.tool("DRAW")
draw.radius = input.radius
draw.strength = input.strength

smooth = input.make.tool("SMOOTH")
smooth.radius = input.radius * input.autosmooth_radius_scale
smooth.strength = input.autosmooth;
smooth.flow = draw.outflow

rake = input.make.tool("TOPORAKE")
rake.radius = input.radius * input.toporake_radius_scale
rake.strength = input.topology;
rake.flow = smooth.outflow

output.out = rake.outflow

preset = Builder(build)

*/

/*
bNodeType sculpt_tool_node = {
  .idname = "SculptTool",
  .ui_name = "SculptTool",
};*/
/* cland-format on */

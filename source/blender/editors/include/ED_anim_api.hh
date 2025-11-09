/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_nla.hh"

#include "BLI_enum_flags.hh"
#include "BLI_sys_types.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include <optional>

struct AnimData;
struct Depsgraph;
struct ID;
struct ListBase;

struct ARegion;
struct ARegionType;
struct FModifier;
struct Main;
struct NlaStrip;
struct NlaTrack;
struct PanelType;
struct ReportList;
struct ScrArea;
struct SpaceLink;
struct View2D;
struct ViewLayer;
struct rctf;
struct bContext;
struct wmKeyConfig;

struct Object;
struct Scene;

struct bDopeSheet;

struct FCurve;
struct FModifier;
struct bAction;

struct uiBlock;

struct PointerRNA;
struct PropertyRNA;

struct MPathTarget;

namespace blender::animrig {
class Action;
class Slot;
}  // namespace blender::animrig

/* ************************************************ */
/* ANIMATION CHANNEL FILTERING */
/* `anim_filter.cc` */

/* -------------------------------------------------------------------- */
/** \name Context
 * \{ */

/** Main Data container types. */
enum eAnimCont_Types {
  /** Invalid or no data. */
  ANIMCONT_NONE = 0,
  /** Action (#bAction). */
  ANIMCONT_ACTION = 1,
  /** Shape-key (#Key). */
  ANIMCONT_SHAPEKEY = 2,
  /** Grease pencil (screen). */
  ANIMCONT_GPENCIL = 3,
  /** Dope-sheet (#bDopesheet). */
  ANIMCONT_DOPESHEET = 4,
  /** Animation F-Curves (#bDopesheet). */
  ANIMCONT_FCURVES = 5,
  /** Drivers (#bDopesheet). */
  ANIMCONT_DRIVERS = 6,
  /** NLA (#bDopesheet). */
  ANIMCONT_NLA = 7,
  /** Animation channel (#bAnimListElem). */
  ANIMCONT_CHANNEL = 8,
  /** Mask dope-sheet. */
  ANIMCONT_MASK = 9,
  /** "timeline" editor (#bDopeSheet). */
  ANIMCONT_TIMELINE = 10,
};

/**
 * This struct defines a structure used for animation-specific
 * 'context' information.
 */
struct bAnimContext {
  /** data to be filtered for use in animation editor */
  void *data;
  /** Type of `data`. */
  eAnimCont_Types datatype;

  /** Editor mode, which depends on `spacetype` (below). */
  eAnimEdit_Context dopesheet_mode;
  eGraphEdit_Mode grapheditor_mode;

  /**
   * Filters from the dopesheet/graph editor settings. These may reflect the corresponding bits in
   * ads->filterflag and ads->filterflag2, but can also be overriden by the dopesheet mode to force
   * certain filters (without having to write to ads->filterflag/flag2).
   */
  struct {
    eDopeSheet_FilterFlag flag;
    eDopeSheet_FilterFlag2 flag2;
  } filters;

  /** area->spacetype */
  eSpace_Type spacetype;
  /** active region -> type (channels or main) */
  eRegion_Type regiontype;

  /** editor host */
  ScrArea *area;
  /** editor data */
  SpaceLink *sl;
  /** region within editor */
  ARegion *region;

  /** dopesheet data for editor (or which is being used) */
  bDopeSheet *ads;

  /** Current Main */
  Main *bmain;
  /** active scene */
  Scene *scene;
  /** active scene layer */
  ViewLayer *view_layer;
  /** active dependency graph */
  Depsgraph *depsgraph;
  /** active object */
  Object *obact;

  /**
   * Active Action, only set when the Dope Sheet shows a single Action (in its
   * Action and Shape Key modes).
   */
  bAction *active_action;
  /** The ID that is animated by `active_action`, and that was used to obtain the pointer. */
  ID *active_action_user;

  /** active set of markers */
  ListBase *markers;

  /** pointer to current reports list */
  ReportList *reports;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Channels
 * \{ */

/**
 * Some types for easier type-testing
 *
 * \note need to keep the order of these synchronized with the channels define code (ACF_XXX must
 * have the same value as ANIMTYPE_XXX below) which is used for drawing and handling channel lists
 * for.
 */
enum eAnim_ChannelType {
  ANIMTYPE_NONE = 0,
  ANIMTYPE_ANIMDATA,
  ANIMTYPE_SPECIALDATA__UNUSED,

  ANIMTYPE_SUMMARY,

  ANIMTYPE_SCENE,
  ANIMTYPE_OBJECT,
  ANIMTYPE_GROUP,
  ANIMTYPE_FCURVE,

  ANIMTYPE_NLACONTROLS,
  ANIMTYPE_NLACURVE,

  ANIMTYPE_FILLACT_LAYERED, /* Layered Actions. */
  ANIMTYPE_ACTION_SLOT,
  ANIMTYPE_FILLACTD, /* Legacy Actions. */
  ANIMTYPE_FILLDRIVERS,

  ANIMTYPE_DSMAT,
  ANIMTYPE_DSLAM,
  ANIMTYPE_DSCAM,
  ANIMTYPE_DSCACHEFILE,
  ANIMTYPE_DSCUR,
  ANIMTYPE_DSSKEY,
  ANIMTYPE_DSWOR,
  ANIMTYPE_DSNTREE,
  ANIMTYPE_DSPART,
  ANIMTYPE_DSMBALL,
  ANIMTYPE_DSARM,
  ANIMTYPE_DSMESH,
  ANIMTYPE_DSTEX,
  ANIMTYPE_DSLAT,
  ANIMTYPE_DSLINESTYLE,
  ANIMTYPE_DSSPK,
  ANIMTYPE_DSGPENCIL,
  ANIMTYPE_DSMCLIP,
  ANIMTYPE_DSHAIR,
  ANIMTYPE_DSPOINTCLOUD,
  ANIMTYPE_DSVOLUME,
  ANIMTYPE_DSLIGHTPROBE,

  ANIMTYPE_SHAPEKEY,

  ANIMTYPE_GPLAYER,

  ANIMTYPE_GREASE_PENCIL_DATABLOCK,
  ANIMTYPE_GREASE_PENCIL_LAYER_GROUP,
  ANIMTYPE_GREASE_PENCIL_LAYER,

  ANIMTYPE_MASKDATABLOCK,
  ANIMTYPE_MASKLAYER,

  ANIMTYPE_NLATRACK,
  ANIMTYPE_NLAACTION,

  ANIMTYPE_PALETTE,

  /* always as last item, the total number of channel types... */
  ANIMTYPE_NUM_TYPES,
};

/** Types of keyframe data in #bAnimListElem. */
enum eAnim_KeyType {
  ALE_NONE = 0, /* no keyframe data */
  ALE_FCURVE,   /* F-Curve */
  ALE_GPFRAME,  /* Grease Pencil Frames (Legacy) */
  ALE_MASKLAY,  /* Mask */
  ALE_NLASTRIP, /* NLA Strips */

  ALE_ALL,            /* All channels summary */
  ALE_SCE,            /* Scene summary */
  ALE_OB,             /* Object summary */
  ALE_ACT,            /* Action summary (legacy). */
  ALE_GROUP,          /* Action Group summary (legacy). */
  ALE_ACTION_LAYERED, /* Action summary (layered). */
  ALE_ACTION_SLOT,    /* Action slot summary. */

  ALE_GREASE_PENCIL_CEL,   /* Grease Pencil Cels. */
  ALE_GREASE_PENCIL_DATA,  /* Grease Pencil Cels summary. */
  ALE_GREASE_PENCIL_GROUP, /* Grease Pencil Layer Groups. */
};

/**
 * Flags for specifying the types of updates (i.e. recalculation/refreshing) that
 * needs to be performed to the data contained in a channel following editing.
 * For use with ANIM_animdata_update()
 */
enum eAnim_Update_Flags {
  /** Referenced data and dependencies get refreshed. */
  ANIM_UPDATE_DEPS = (1 << 0),
  /** Keyframes need to be sorted. */
  ANIM_UPDATE_ORDER = (1 << 1),
  /** Recalculate handles. */
  ANIM_UPDATE_HANDLES = (1 << 2),
};
ENUM_OPERATORS(eAnim_Update_Flags);

/* used for most tools which change keyframes (flushed by ANIM_animdata_update) */
#define ANIM_UPDATE_DEFAULT (ANIM_UPDATE_DEPS | ANIM_UPDATE_ORDER | ANIM_UPDATE_HANDLES)
#define ANIM_UPDATE_DEFAULT_NOHANDLES (ANIM_UPDATE_DEFAULT & ~ANIM_UPDATE_HANDLES)

/**
 * This struct defines a structure used for quick and uniform access for
 * channels of animation data.
 */
struct bAnimListElem {
  bAnimListElem *next, *prev;

  /** source data this elem represents */
  void *data;
  /** One of the ANIMTYPE_* values. */
  eAnim_ChannelType type;
  /** copy of elem's flags for quick access */
  int flag;
  /** for un-named data, the index of the data in its collection */
  int index;
  /**
   * For data that is owned by a specific slot, its handle.
   *
   * This is not declared as #blender::animrig::slot_handle_t to avoid all the users of this
   * header file to get the `animrig` module as extra dependency (which would spread to the undo
   * system, line-art, etc). It's probably best to split off this struct definition from the rest
   * of this header, as most code that uses this header doesn't need to know the definition of this
   * struct.
   *
   * TODO: split off into separate header file.
   */
  int32_t slot_handle;

  /** Tag the element for updating. */
  eAnim_Update_Flags update;
  /** tag the included data. Temporary always */
  char tag;

  /** Type of motion data to expect. */
  eAnim_KeyType datatype;
  /** motion data - mostly F-Curves, but can be other types too */
  void *key_data;

  /**
   * \note
   * id here is the "IdAdtTemplate"-style datablock (e.g. Object, Material, Texture, NodeTree)
   * from which evaluation of the RNA-paths takes place. It's used to figure out how deep
   * channels should be nested (e.g. for Textures/NodeTrees) in the tree, and allows property
   * lookups (e.g. for sliders and for inserting keyframes) to work. If we had instead used
   * bAction or something similar, none of this would be possible: although it's trivial
   * to use an IdAdtTemplate type to find the source action a channel (e.g. F-Curve) comes from
   * (i.e. in the AnimEditors, it *must* be the active action, as only that can be edited),
   * it's impossible to go the other way (i.e. one action may be used in multiple places).
   */
  /** ID block that channel is attached to */
  ID *id;
  /** source of the animation data attached to ID block */
  AnimData *adt;
  /** Main containing the ID. */
  Main *bmain;

  /**
   * For list elements that correspond to an f-curve, a channel group, or an
   * action slot, this is the ID which owns that data.
   *
   * For channel groups and action slots, that will always be an Action. For
   * f-curves it's more complicated, because f-curves are sometimes owned by
   * other ID types (e.g. driver f-curves are owned by objects, materials,
   * etc.), so you have to be careful.
   *
   * NOTE: this is different from id above. The id above will be set to
   * an object if the f-curve is coming from action associated with that object.
   *
   * TODO: the responsibilities for this are getting overloaded, which makes it
   * difficult to use confidently, and also makes its name misleading. Split off
   * a separate `bAction` pointer that is simply null when the data isn't owned
   * by an action.
   */
  ID *fcurve_owner_id;

  /**
   * for per-element F-Curves
   * (e.g. NLA Control Curves), the element that this represents (e.g. NlaStrip) */
  void *owner;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filtering
 * \{ */

/* filtering flags  - under what circumstances should a channel be returned */
enum eAnimFilter_Flags {
  /**
   * Data which channel represents is fits the dope-sheet filters
   * (i.e. scene visibility criteria).
   *
   * XXX: it's hard to think of any examples where this *ISN'T* the case...
   * perhaps becomes implicit?
   */
  ANIMFILTER_DATA_VISIBLE = (1 << 0),
  /** channel is visible within the channel-list hierarchy
   * (i.e. F-Curves within Groups in ActEdit) */
  ANIMFILTER_LIST_VISIBLE = (1 << 1),
  /** channel has specifically been tagged as visible in Graph Editor (* Graph Editor Only) */
  ANIMFILTER_CURVE_VISIBLE = (1 << 2),

  /** include summary channels and "expanders" (for drawing/mouse-selection in channel list) */
  ANIMFILTER_LIST_CHANNELS = (1 << 3),

  /** for its type, channel should be "active" one */
  ANIMFILTER_ACTIVE = (1 << 4),
  /** channel is a child of the active group (* Actions specialty) */
  ANIMFILTER_ACTGROUPED = (1 << 5),

  /** channel must be selected/not-selected, but both must not be set together */
  ANIMFILTER_SEL = (1 << 6),
  ANIMFILTER_UNSEL = (1 << 7),

  /** editability status - must be editable to be included */
  ANIMFILTER_FOREDIT = (1 << 8),
  /** only selected animchannels should be considerable as editable - mainly
   * for Graph Editor's option for keys on select curves only */
  ANIMFILTER_SELEDIT = (1 << 9),

  /**
   * Flags used to enforce certain data types.
   *
   * \note The ones for curves and NLA tracks were redundant and have been removed for now.
   */
  ANIMFILTER_ANIMDATA = (1 << 10),

  /** duplicate entries for animation data attached to multi-user blocks must not occur */
  ANIMFILTER_NODUPLIS = (1 << 11),

  /**
   * Avoid channels that don't have any F-curve data under them.
   *
   * Note that this isn't just direct fcurve channels, but also includes e.g.
   * channel groups with fcurve channels as members.
   */
  ANIMFILTER_FCURVESONLY = (1 << 12),

  /** for checking if we should keep some collapsed channel around (internal use only!) */
  ANIMFILTER_TMP_PEEK = (1 << 30),

  /** Ignore ONLYSEL flag from #bDopeSheet.filterflag (internal use only!) */
  ANIMFILTER_TMP_IGNORE_ONLYSEL = (1u << 31),

};
ENUM_OPERATORS(eAnimFilter_Flags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flag Checking Macros
 * \{ */

/* XXX check on all of these flags again. */

/* Dope-sheet only. */
/* 'Scene' channels */
#define SEL_SCEC(sce) (CHECK_TYPE_INLINE(sce, Scene *), ((sce->flag & SCE_DS_SELECTED)))
#define EXPANDED_SCEC(sce) (CHECK_TYPE_INLINE(sce, Scene *), ((sce->flag & SCE_DS_COLLAPSED) == 0))
/* 'Sub-Scene' channels (flags stored in Data block) */
#define FILTER_WOR_SCED(wo) (CHECK_TYPE_INLINE(wo, World *), (wo->flag & WO_DS_EXPAND))
#define FILTER_LS_SCED(linestyle) ((linestyle->flag & LS_DS_EXPAND))
/* 'Object' channels */
#define SEL_OBJC(base) (CHECK_TYPE_INLINE(base, Base *), ((base->flag & SELECT)))
#define EXPANDED_OBJC(ob) \
  (CHECK_TYPE_INLINE(ob, Object *), (((ob)->nlaflag & OB_ADS_COLLAPSED) == 0))
/* 'Sub-object' channels (flags stored in Data block) */
#define FILTER_SKE_OBJD(key) (CHECK_TYPE_INLINE(key, Key *), ((key->flag & KEY_DS_EXPAND)))
#define FILTER_MAT_OBJD(ma) (CHECK_TYPE_INLINE(ma, Material *), ((ma->flag & MA_DS_EXPAND)))
#define FILTER_LAM_OBJD(la) (CHECK_TYPE_INLINE(la, Light *), ((la->flag & LA_DS_EXPAND)))
#define FILTER_CAM_OBJD(ca) (CHECK_TYPE_INLINE(ca, Camera *), ((ca->flag & CAM_DS_EXPAND)))
#define FILTER_CACHEFILE_OBJD(cf) \
  (CHECK_TYPE_INLINE(cf, CacheFile *), (((cf)->flag & CACHEFILE_DS_EXPAND)))
#define FILTER_CUR_OBJD(cu) (CHECK_TYPE_INLINE(cu, Curve *), ((cu->flag & CU_DS_EXPAND)))
#define FILTER_PART_OBJD(part) \
  (CHECK_TYPE_INLINE(part, ParticleSettings *), (((part)->flag & PART_DS_EXPAND)))
#define FILTER_MBALL_OBJD(mb) (CHECK_TYPE_INLINE(mb, MetaBall *), ((mb->flag2 & MB_DS_EXPAND)))
#define FILTER_ARM_OBJD(arm) (CHECK_TYPE_INLINE(arm, bArmature *), ((arm->flag & ARM_DS_EXPAND)))
#define FILTER_MESH_OBJD(me) (CHECK_TYPE_INLINE(me, Mesh *), ((me->flag & ME_DS_EXPAND)))
#define FILTER_LATTICE_OBJD(lt) (CHECK_TYPE_INLINE(lt, Lattice *), ((lt->flag & LT_DS_EXPAND)))
#define FILTER_SPK_OBJD(spk) (CHECK_TYPE_INLINE(spk, Speaker *), ((spk->flag & SPK_DS_EXPAND)))
#define FILTER_CURVES_OBJD(ha) (CHECK_TYPE_INLINE(ha, Curves *), ((ha->flag & HA_DS_EXPAND)))
#define FILTER_POINTS_OBJD(pt) (CHECK_TYPE_INLINE(pt, PointCloud *), ((pt->flag & PT_DS_EXPAND)))
#define FILTER_VOLUME_OBJD(vo) (CHECK_TYPE_INLINE(vo, Volume *), ((vo->flag & VO_DS_EXPAND)))
#define FILTER_LIGHTPROBE_OBJD(probe) \
  (CHECK_TYPE_INLINE(probe, LightProbe *), ((probe->flag & LIGHTPROBE_DS_EXPAND)))
/* Variable use expanders */
#define FILTER_NTREE_DATA(ntree) \
  (CHECK_TYPE_INLINE(ntree, bNodeTree *), (((ntree)->flag & NTREE_DS_EXPAND)))
#define FILTER_TEX_DATA(tex) (CHECK_TYPE_INLINE(tex, Tex *), ((tex->flag & TEX_DS_EXPAND)))

/* 'Sub-object/Action' channels (flags stored in Action) */
#define SEL_ACTC(actc) ((actc->flag & ACT_SELECTED))
#define EXPANDED_ACTC(actc) ((actc->flag & ACT_COLLAPSED) == 0)
/* 'Sub-AnimData' channels */
#define EXPANDED_DRVD(adt) ((adt->flag & ADT_DRIVERS_COLLAPSED) == 0)
#define EXPANDED_ADT(adt) ((adt->flag & ADT_UI_EXPANDED) != 0)

/* Actions (also used for Dope-sheet). */
/** Action Channel Group. */
#define EDITABLE_AGRP(agrp) (((agrp)->flag & AGRP_PROTECTED) == 0)
#define EXPANDED_AGRP(ac, agrp) \
  (((!(ac) || ((ac)->spacetype != SPACE_GRAPH)) && ((agrp)->flag & AGRP_EXPANDED)) || \
   (((ac) && ((ac)->spacetype == SPACE_GRAPH)) && ((agrp)->flag & AGRP_EXPANDED_G)))
#define SEL_AGRP(agrp) (((agrp)->flag & AGRP_SELECTED) || ((agrp)->flag & AGRP_ACTIVE))
/** F-Curve Channels. */
#define EDITABLE_FCU(fcu) (((fcu)->flag & FCURVE_PROTECTED) == 0)
#define SEL_FCU(fcu) ((fcu)->flag & FCURVE_SELECTED)

/* ShapeKey mode only */
#define EDITABLE_SHAPEKEY(kb) ((kb->flag & KEYBLOCK_LOCKED) == 0)
#define SEL_SHAPEKEY(kb) (kb->flag & KEYBLOCK_SEL)

/* Grease Pencil only */
/** Grease Pencil data-block settings. */
#define EXPANDED_GPD(gpd) (gpd->flag & GP_DATA_EXPAND)
/** Grease Pencil Layer settings. */
#define EDITABLE_GPL(gpl) ((gpl->flag & GP_LAYER_LOCKED) == 0)
#define SEL_GPL(gpl) (gpl->flag & GP_LAYER_SELECT)

/* Mask Only */
/** Grease Pencil data-block settings. */
#define EXPANDED_MASK(mask) (mask->flag & MASK_ANIMF_EXPAND)
/** Grease Pencil Layer settings. */
#define EDITABLE_MASK(masklay) ((masklay->flag & MASK_LAYERFLAG_LOCKED) == 0)
#define SEL_MASKLAY(masklay) (masklay->flag & SELECT)

/* NLA only */
#define SEL_NLT(nlt) (nlt->flag & NLATRACK_SELECTED)
#define EDITABLE_NLT(nlt) ((nlt->flag & NLATRACK_PROTECTED) == 0)

/* Movie clip only */
#define EXPANDED_MCLIP(clip) (clip->flag & MCLIP_DATA_EXPAND)

/* Palette only */
#define EXPANDED_PALETTE(palette) (palette->flag & PALETTE_DATA_EXPAND)

/* AnimData - NLA mostly... */
#define SEL_ANIMDATA(adt) (adt->flag & ADT_UI_SELECTED)

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA Track Defines
 * \{ */

/** NLA track heights */
#define NLATRACK_FIRST_TOP(ac) \
  (UI_view2d_scale_get_y(&(ac)->region->v2d) * -UI_TIME_SCRUB_MARGIN_Y - NLATRACK_SKIP)
#define NLATRACK_HEIGHT(snla) \
  (((snla) && ((snla)->flag & SNLA_NOSTRIPCURVES)) ? (0.8f * U.widget_unit) : \
                                                     (1.2f * U.widget_unit))
#define NLATRACK_SKIP (0.1f * U.widget_unit)
#define NLATRACK_STEP(snla) (NLATRACK_HEIGHT(snla) + NLATRACK_SKIP)
/** Additional offset to give some room at the end. */
#define NLATRACK_TOT_HEIGHT(ac, item_amount) \
  (-NLATRACK_FIRST_TOP(ac) + NLATRACK_STEP(((SpaceNla *)(ac)->sl)) * (item_amount + 1))

/** Track widths */
#define NLATRACK_NAMEWIDTH (10 * U.widget_unit)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

/**
 * Add the channel and sub-channels for an Action Slot to `anim_data`, filtered
 * according to `filter_mode`.
 *
 * \param action: the action containing the slot to generate the channels for.
 *
 * \param slot: the slot to generate the channels for.
 *
 * \param filter_mode: the filters to use for deciding what channels get
 * included.
 *
 * \param animated_id: the particular animated ID that the slot channels are
 * being generated for. This is needed for filtering channels based on bone
 * selection, and also for resolving the names of animated properties. This
 * should never be null, but it's okay(ish) if it's an ID not actually animated
 * by the slot, in which case it will act as a fallback in case an ID actually
 * animated by the slot can't be found.
 *
 * \return The number of items added to `anim_data`.
 */
size_t ANIM_animfilter_action_slot(bAnimContext *ac,
                                   ListBase * /* bAnimListElem */ anim_data,
                                   blender::animrig::Action &action,
                                   blender::animrig::Slot &slot,
                                   eAnimFilter_Flags filter_mode,
                                   ID *animated_id);

/**
 * This function filters the active data source to leave only animation channels suitable for
 * usage by the caller. It will return the length of the list
 *
 * \param anim_data: Is a pointer to a #ListBase,
 * to which the filtered animation channels will be placed for use.
 * \param filter_mode: how should the data be filtered - bit-mapping accessed flags.
 */
size_t ANIM_animdata_filter(bAnimContext *ac,
                            ListBase *anim_data,
                            eAnimFilter_Flags filter_mode,
                            void *data,
                            eAnimCont_Types datatype);

/**
 * Obtain current anim-data context from Blender Context info
 * - AnimContext to write to is provided as pointer to var on stack so that we don't have
 *   allocation/freeing costs (which are not that avoidable with channels).
 * - Clears data and sets the information from Blender Context which is useful
 * \return whether the operation was successful.
 */
bool ANIM_animdata_get_context(const bContext *C, bAnimContext *ac);

/**
 * Obtain current anim-data context,
 * given that context info from Blender context has already been set:
 * - AnimContext to write to is provided as pointer to var on stack so that we don't have
 *   allocation/freeing costs (which are not that avoidable with channels).
 * \return whether the operation was successful.
 *
 * \note This may also update the space data. For example, `SpaceAction::action`
 * is set to the currently active object's Action.
 */
bool ANIM_animdata_context_getdata(bAnimContext *ac);

/**
 * Acts on bAnimListElem eAnim_Update_Flags.
 */
void ANIM_animdata_update(bAnimContext *ac, ListBase *anim_data);

void ANIM_animdata_freelist(ListBase *anim_data);

/**
 * Check if the given animation container can contain grease pencil layer keyframes.
 */
bool ANIM_animdata_can_have_greasepencil(const eAnimCont_Types type);

bAction *ANIM_active_action_from_area(Scene *scene,
                                      ViewLayer *view_layer,
                                      const ScrArea *area,
                                      ID **r_action_user = nullptr);

/* ************************************************ */
/* ANIMATION CHANNELS LIST */
/* anim_channels_*.c */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing TypeInfo
 * \{ */

/** Role or level of anim-channel in the hierarchy. */
enum eAnimChannel_Role {
  /** datablock expander - a "composite" channel type */
  ACHANNEL_ROLE_EXPANDER = -1,
  /** special purposes - not generally for hierarchy processing */
  /* ACHANNEL_ROLE_SPECIAL = 0, */ /* UNUSED */
  /** data channel - a channel representing one of the actual building blocks of channels */
  ACHANNEL_ROLE_CHANNEL = 1,
};

/* flag-setting behavior */
enum eAnimChannels_SetFlag {
  /** turn off */
  ACHANNEL_SETFLAG_CLEAR = 0,
  /** turn on */
  ACHANNEL_SETFLAG_ADD = 1,
  /** on->off, off->on */
  ACHANNEL_SETFLAG_INVERT = 2,
  /** some on -> all off / all on */
  ACHANNEL_SETFLAG_TOGGLE = 3,
  /** Turn off, keep active flag. */
  ACHANNEL_SETFLAG_EXTEND_RANGE = 4,
};

/* types of settings for AnimChannels */
enum eAnimChannel_Settings {
  ACHANNEL_SETTING_SELECT = 0,
  /** WARNING: for drawing UI's, need to check if this is off (maybe inverse this later). */
  ACHANNEL_SETTING_PROTECT = 1,
  ACHANNEL_SETTING_MUTE = 2,
  ACHANNEL_SETTING_EXPAND = 3,
  /** only for Graph Editor */
  ACHANNEL_SETTING_VISIBLE = 4,
  /** only for NLA Tracks */
  ACHANNEL_SETTING_SOLO = 5,
  /** only for NLA Actions */
  ACHANNEL_SETTING_PINNED = 6,
  ACHANNEL_SETTING_MOD_OFF = 7,
  /** channel is pinned and always visible */
  ACHANNEL_SETTING_ALWAYS_VISIBLE = 8,
};

/** Drawing, mouse handling, and flag setting behavior. */
struct bAnimChannelType {
  /* -- Type data -- */
  /* name of the channel type, for debugging */
  const char *channel_type_name;
  /* "level" or role in hierarchy - for finding the active channel */
  eAnimChannel_Role channel_role;

  /* -- Drawing -- */
  /** Get RGB color that is used to draw the majority of the backdrop. */
  void (*get_backdrop_color)(bAnimContext *ac, bAnimListElem *ale, float r_color[3]);

  /** Get RGB color that represents this channel.
   * \return true when r_color was updated, false when there is no color for this channel.
   */
  bool (*get_channel_color)(const bAnimListElem *ale, uint8_t r_color[3]);

  /** Draw backdrop strip for channel. */
  void (*draw_backdrop)(bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc);
  /** Get depth of indentation (relative to the depth channel is nested at). */
  short (*get_indent_level)(bAnimContext *ac, bAnimListElem *ale);
  /** Get offset in pixels for the start of the channel (in addition to the indent depth). */
  short (*get_offset)(bAnimContext *ac, bAnimListElem *ale);

  /** Get name (for channel lists). */
  void (*name)(bAnimListElem *ale, char *name);
  /** Get RNA property+pointer for editing the name. */
  bool (*name_prop)(bAnimListElem *ale, PointerRNA *r_ptr, PropertyRNA **r_prop);
  /** Get icon (for channel lists). */
  int (*icon)(bAnimListElem *ale);

  /* -- Settings -- */
  /** Check if the given setting is valid in the current context. */
  bool (*has_setting)(bAnimContext *ac, bAnimListElem *ale, eAnimChannel_Settings setting);
  /** Get the flag used for this setting. */
  int (*setting_flag)(bAnimContext *ac, eAnimChannel_Settings setting, bool *r_neg);
  /**
   * Get the pointer to int/short where data is stored,
   * with type being `sizeof(ptr_data)` which should be fine for runtime use.
   * - assume that setting has been checked to be valid for current context.
   */
  void *(*setting_ptr)(bAnimListElem *ale, eAnimChannel_Settings setting, short *r_type);

  /**
   * Called after a setting was changed via ANIM_channel_setting_set().
   *
   * \param ale: is marked as `const`, as it could have been duplicated and taken out of context.
   * This means that any hypothetical changes to `ale->update`, for example, will not be seen by
   * any `ANIM_animdata_update()` call. So better to keep this `const` and avoid any manipulation.
   * Also, because of the duplications, the ale's `prev` and `next` pointers will be dangling.
   */
  void (*setting_post_update)(Main &bmain,
                              const bAnimListElem &ale,
                              eAnimChannel_Settings setting);
};

/** \} */
/* -------------------------------------------------------------------- */
/** \name Channel dimensions API
 * \{ */

float ANIM_UI_get_keyframe_scale_factor();
float ANIM_UI_get_channel_height();
float ANIM_UI_get_channel_skip();
float ANIM_UI_get_first_channel_top(View2D *v2d);
float ANIM_UI_get_channel_step();
float ANIM_UI_get_channels_total_height(View2D *v2d, int item_count);
float ANIM_UI_get_channel_name_width();
float ANIM_UI_get_channel_button_width();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing API
 * \{ */

/**
 * Get type info from given channel type.
 */
const bAnimChannelType *ANIM_channel_get_typeinfo(const bAnimListElem *ale);

/**
 * Print debug info string for the given channel.
 */
void ANIM_channel_debug_print_info(bAnimContext &ac, bAnimListElem *ale, short indent_level);

/**
 * Retrieves the Action associated with this animation channel.
 */
bAction *ANIM_channel_action_get(const bAnimListElem *ale);

/**
 * Draw the given channel.
 */
void ANIM_channel_draw(
    bAnimContext *ac, bAnimListElem *ale, float yminc, float ymaxc, size_t channel_index);
/**
 * Draw UI widgets the given channel.
 */
void ANIM_channel_draw_widgets(const bContext *C,
                               bAnimContext *ac,
                               bAnimListElem *ale,
                               uiBlock *block,
                               const rctf *rect,
                               size_t channel_index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editing API
 * \{ */

/**
 * Check if some setting for a channel is enabled
 * Returns: 1 = On, 0 = Off, -1 = Invalid.
 */
short ANIM_channel_setting_get(bAnimContext *ac,
                               bAnimListElem *ale,
                               eAnimChannel_Settings setting);

/**
 * Change value of some setting for a channel.
 */
void ANIM_channel_setting_set(bAnimContext *ac,
                              bAnimListElem *ale,
                              eAnimChannel_Settings setting,
                              eAnimChannels_SetFlag mode);

/**
 * Flush visibility (for Graph Editor) changes up/down hierarchy for changes in the given setting
 * - anim_data: list of the all the anim channels that can be chosen
 *   -> filtered using ANIMFILTER_CHANNELS only, since if we took VISIBLE too,
 *      then the channels under closed expanders get ignored...
 * - ale_setting: the anim channel (not in the anim_data list directly, though occurring there)
 *   with the new state of the setting that we want flushed up/down the hierarchy
 * - setting: type of setting to set
 * - on: whether the visibility setting has been enabled or disabled
 */
void ANIM_flush_setting_anim_channels(bAnimContext *ac,
                                      ListBase *anim_data,
                                      bAnimListElem *ale_setting,
                                      eAnimChannel_Settings setting,
                                      eAnimChannels_SetFlag mode);

void ANIM_frame_channel_y_extents(bContext *C, bAnimContext *ac);

/**
 * Set selection state of all animation channels in the context.
 */
void ANIM_anim_channels_select_set(bAnimContext *ac, eAnimChannels_SetFlag sel);

/**
 * Toggle selection state of all animation channels in the context.
 */
void ANIM_anim_channels_select_toggle(bAnimContext *ac);

/**
 * Set the given animation-channel as the active one for the active context.
 */
void ANIM_set_active_channel(bAnimContext *ac,
                             void *data,
                             eAnimCont_Types datatype,
                             eAnimFilter_Flags filter,
                             void *channel_data,
                             eAnim_ChannelType channel_type);

/**
 * Return whether channel is active.
 */
bool ANIM_is_active_channel(bAnimListElem *ale);

/**
 * Deselects the keys displayed within the open animation editors. Depending on the display
 * settings of those editors, the keys may not be from an action of the selected objects.
 */
void ANIM_deselect_keys_in_animation_editors(bContext *C);

/* ************************************************ */
/* DRAWING API */
/* `anim_draw.cc` */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Current Frame Drawing
 *
 * Main call to draw current-frame indicator in an Animation Editor.
 * \{ */

/* flags for Current Frame Drawing */
enum eAnimEditDraw_CurrentFrame {
  /** Plain time indicator with no special indicators. */
  /* DRAWCFRA_PLAIN = 0, */ /* UNUSED */
  /** Time indication in seconds or frames. */
  DRAWCFRA_UNIT_SECONDS = (1 << 0),
  /** Draw indicator extra wide (for timeline). */
  DRAWCFRA_WIDE = (1 << 1),
};

/**
 * General call for drawing current frame indicator in animation editor.
 */
void ANIM_draw_cfra(const bContext *C, View2D *v2d, short flag);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview Range Drawing
 *
 * Main call to draw preview range curtains.
 * \{ */

/**
 * Draw preview range 'curtains' for highlighting where the animation data is.
 */
void ANIM_draw_previewrange(const Scene *scene, View2D *v2d, int end_frame_width);

/**
 * Draw range of the current sequencer scene strip when using scene time syncing.
 */
void ANIM_draw_scene_strip_range(const bContext *C, View2D *v2d);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Range Drawing
 *
 * Main call to draw normal frame range indicators.
 * \{ */

/**
 * Draw frame range guides (for scene frame range) in background.
 */
void ANIM_draw_framerange(Scene *scene, View2D *v2d);

/**
 * Draw manually set intended playback frame range guides for the action in the background.
 * Allows specifying a subset of the Y range of the view.
 */
void ANIM_draw_action_framerange(
    AnimData *adt, bAction *action, View2D *v2d, float ymin, float ymax);

/* ************************************************* */
/* F-MODIFIER TOOLS */

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Panel Drawing
 * \{ */

bool ANIM_nla_context_track_ptr(const bContext *C, PointerRNA *r_ptr);
bool ANIM_nla_context_strip_ptr(const bContext *C, PointerRNA *r_ptr);

NlaTrack *ANIM_nla_context_track(const bContext *C);
NlaStrip *ANIM_nla_context_strip(const bContext *C);
FCurve *ANIM_graph_context_fcurve(const bContext *C);

/** Needed for abstraction between the graph editor and the NLA editor. */
using PanelTypePollFn = bool (*)(const bContext *C, PanelType *pt);
/** Avoid including `UI_interface.hh` here. */
using uiListPanelIDFromDataFunc = void (*)(void *data_link, char *r_idname);

/**
 * Checks if the panels match the active strip / curve, rebuilds them if they don't.
 */
void ANIM_fmodifier_panels(const bContext *C,
                           ID *owner_id,
                           ListBase *fmodifiers,
                           uiListPanelIDFromDataFunc panel_id_fn);

void ANIM_modifier_panels_register_graph_and_NLA(ARegionType *region_type,
                                                 const char *modifier_panel_prefix,
                                                 PanelTypePollFn poll_function);
void ANIM_modifier_panels_register_graph_only(ARegionType *region_type,
                                              const char *modifier_panel_prefix,
                                              PanelTypePollFn poll_function);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy/Paste Buffer
 * \{ */

/**
 * Free the copy/paste buffer.
 */
void ANIM_fmodifiers_copybuf_free();

/**
 * Copy the given F-Modifiers to the buffer, returning whether anything was copied or not
 * assuming that the buffer has been cleared already with #ANIM_fmodifiers_copybuf_free()
 * \param active: Only copy the active modifier.
 */
bool ANIM_fmodifiers_copy_to_buf(ListBase *modifiers, bool active);

/**
 * 'Paste' the F-Modifier(s) from the buffer to the specified list
 * \param replace: Free all the existing modifiers to leave only the pasted ones.
 */
bool ANIM_fmodifiers_paste_from_buf(ListBase *modifiers, bool replace, FCurve *curve);

/* ************************************************* */
/* ASSORTED TOOLS */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation F-Curves <-> Icons/Names Mapping
 * \{ */

/* `anim_ipo_utils.cc` */

/**
 * Get icon + name for channel-list displays for F-Curve.
 *
 * Write into "name" buffer, the name of the property
 * (retrieved using RNA from the curve's settings),
 * and return the icon used for the struct that this property refers to
 *
 * \warning name buffer we're writing to cannot exceed 256 chars
 * (check anim_channels_defines.cc for details).
 *
 * \return the icon of whatever struct the F-Curve's RNA path resolves to.
 * Returns #std::nullopt if the path could not be resolved.
 */
std::optional<int> getname_anim_fcurve(char *name, ID *id, FCurve *fcu);

/**
 * Get the name of an F-Curve that's animating a specific slot.
 *
 * This function iterates the Slot's users to find an ID that allows it to resolve its RNA path.
 */
std::string getname_anim_fcurve_for_slot(Main &bmain,
                                         const blender::animrig::Slot &slot,
                                         FCurve &fcurve);

/**
 * Automatically determine a color for the nth F-Curve.
 */
void getcolor_fcurve_rainbow(int cur, int tot, float out[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA Drawing
 *
 * \note Technically, this is not in the animation module (it's in space_nla)
 * but these are sometimes needed by various animation API's.
 * \{ */

/**
 * Get color to use for NLA Action channel's background.
 * \note color returned includes fine-tuned alpha!
 */
void nla_action_get_color(AnimData *adt, bAction *act, float color[4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA-Mapping
 * \{ */

/* `anim_draw.cc` */

/**
 * Check whether NLA time remapping should be done on this bAnimListElem.
 *
 * \returns true by default, false only when this ale indicates an NLA control curve (like animated
 * influence) or a driver.
 */
bool ANIM_nla_mapping_allowed(const bAnimListElem *ale);

/**
 * Do NLA time remapping, but only if `ANIM_nla_mapping_allowed(ale)` returns `true`.
 *
 * \see #ANIM_nla_mapping_allowed
 * \see #BKE_nla_tweakedit_remap
 */
float ANIM_nla_tweakedit_remap(bAnimListElem *ale, float cframe, eNlaTime_ConvertModes mode);

/**
 * Apply/Unapply NLA mapping to all keyframes in the nominated F-Curve
 * \param restore: Whether to map points back to non-mapped time.
 * \param only_keys: Whether to only adjust the location of the center point of beztriples.
 *
 * TODO: this is only used by `fcurve_to_keylist()` at this point. Perhaps with
 * some refactoring we can make `fcurve_to_keylist()` use
 * `ANIM_nla_mapping_apply_if_needed_fcurve()` instead, and then we can get rid
 * of this.
 */
void ANIM_nla_mapping_apply_fcurve(AnimData *adt, FCurve *fcu, bool restore, bool only_keys);

/**
 * Same as above, but only if `ANIM_nla_mapping_allowed(ale)` returns `true`.
 */
void ANIM_nla_mapping_apply_if_needed_fcurve(bAnimListElem *ale,
                                             FCurve *fcu,
                                             bool restore,
                                             bool only_keys);

/* ..... */

/**
 * Perform validation & auto-blending/extend refreshes after some operations
 * \note defined in `space_nla/nla_edit.cc`, not in `animation/`.
 */
void ED_nla_postop_refresh(bAnimContext *ac);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unit Conversion Mappings
 * \{ */

/* `anim_draw.cc` */

/** Flags for conversion mapping. */
enum eAnimUnitConv_Flags {
  ANIM_UNITCONV_NONE = 0,
  /** Restore to original internal values. */
  ANIM_UNITCONV_RESTORE = (1 << 0),
  /** Ignore handles (i.e. only touch main keyframes). */
  ANIM_UNITCONV_ONLYKEYS = (1 << 1),
  /** Only touch selected BezTriples. */
  ANIM_UNITCONV_ONLYSEL = (1 << 2),
  /** Only touch selected vertices. */
  ANIM_UNITCONV_SELVERTS = (1 << 3),
  /* ANIM_UNITCONV_SKIPKNOTS = (1 << 4), */ /* UNUSED */
  /** Scale FCurve i a way it fits to -1..1 space. */
  ANIM_UNITCONV_NORMALIZE = (1 << 5),
  /**
   * Only when normalization is used: use scale factor from previous run,
   * prevents curves from jumping all over the place when tweaking them.
   */
  ANIM_UNITCONV_NORMALIZE_FREEZE = (1 << 6),
};

/**
 * Get flags used for normalization in ANIM_unit_mapping_get_factor.
 */
short ANIM_get_normalization_flags(SpaceLink *space_link);
/**
 * Get unit conversion factor for given ID + F-Curve.
 */
float ANIM_unit_mapping_get_factor(Scene *scene, ID *id, FCurve *fcu, short flag, float *r_offset);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility macros
 * \{ */

/**
 * Set/Clear/Toggle macro.
 * \param channel: Channel with a 'flag' member that we're setting.
 * \param smode: 0=clear, 1=set, 2=invert.
 * \param sflag: bit-flag to set.
 */
#define ACHANNEL_SET_FLAG(channel, smode, sflag) \
  { \
    if (smode == ACHANNEL_SETFLAG_INVERT) { \
      (channel)->flag ^= (sflag); \
    } \
    else if (smode == ACHANNEL_SETFLAG_ADD) { \
      (channel)->flag |= (sflag); \
    } \
    else { \
      (channel)->flag &= ~(sflag); \
    } \
  } \
  ((void)0)

/**
 * Set/Clear/Toggle macro, where the flag is negative.
 * \param channel: channel with a 'flag' member that we're setting.
 * \param smode: 0=clear, 1=set, 2=invert.
 * \param sflag: Bit-flag to set.
 */
#define ACHANNEL_SET_FLAG_NEG(channel, smode, sflag) \
  { \
    if (smode == ACHANNEL_SETFLAG_INVERT) { \
      (channel)->flag ^= (sflag); \
    } \
    else if (smode == ACHANNEL_SETFLAG_ADD) { \
      (channel)->flag &= ~(sflag); \
    } \
    else { \
      (channel)->flag |= (sflag); \
    } \
  } \
  ((void)0)

/** \} */

/* `anim_deps.cc` */

/* -------------------------------------------------------------------- */
/** \name Animation Updates
 * \{ */

/**
 * Tags the given ID block for refreshes (if applicable) due to Animation Editor editing.
 */
void ANIM_id_update(Main *bmain, ID *id);
/**
 * Tags the given anim list element for refreshes (if applicable) due to Animation Editor editing.
 */
void ANIM_list_elem_update(Main *bmain, Scene *scene, bAnimListElem *ale);

/* data -> channels syncing */

/**
 * Main call to be exported to animation editors.
 */
void ANIM_sync_animchannels_to_data(const bContext *C);

void ANIM_center_frame(bContext *C, int smooth_viewtx);

/**
 * Add horizontal margin to the rectangle.
 *
 * This function assumes that the X-min/X-max are set to a frame range to show.
 *
 * \return The new rectangle with horizontal margin added, for visual comfort.
 */
rctf ANIM_frame_range_view2d_add_xmargin(const View2D &view_2d, rctf view_rect);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operators
 * \{ */

/* generic animation channels */
void ED_operatortypes_animchannels();
void ED_keymap_animchannels(wmKeyConfig *keyconf);

/* generic time editing */
void ED_operatortypes_anim();
void ED_keymap_anim(wmKeyConfig *keyconf);

/* space_graph */
void ED_operatormacros_graph();
/* space_action */
void ED_operatormacros_action();
/* space_nla */
void ED_operatormacros_nla();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Editor Exports
 * \{ */

/* XXX: Should we be doing these here, or at all? */

/**
 * Action Editor - Action Management.
 * Helper function to find the active AnimData block from the Action Editor context.
 */
AnimData *ED_actedit_animdata_from_context(const bContext *C, ID **r_adt_id_owner);
void ED_animedit_unlink_action(
    bContext *C, ID *id, AnimData *adt, bAction *act, ReportList *reports, bool force_delete);

/**
 * Set up UI configuration for Drivers Editor
 * (drivers editor window) and RNA (mode switching).
 * \note Currently called from window-manager.
 */
void ED_drivers_editor_init(bContext *C, ScrArea *area);

/**
 * Delete an F-Curve from its owner.
 *
 * This can delete an F-Curve from an Action (both directly assigned and via an
 * NLA strip), Drivers, and NLA control curves.
 */
void ED_anim_ale_fcurve_delete(bAnimContext &ac, bAnimListElem &ale);

/* ************************************************ */

enum eAnimvizCalcRange {
  /** Update motion paths at the current frame only. */
  ANIMVIZ_CALC_RANGE_CURRENT_FRAME,

  /** Try to limit updates to a close neighborhood of the current frame. */
  ANIMVIZ_CALC_RANGE_CHANGED,

  /** Update an entire range of the motion paths. */
  ANIMVIZ_CALC_RANGE_FULL,
};

Depsgraph *animviz_depsgraph_build(Main *bmain,
                                   Scene *scene,
                                   ViewLayer *view_layer,
                                   blender::Span<MPathTarget *> targets);

void animviz_calc_motionpaths(Depsgraph *depsgraph,
                              Main *bmain,
                              Scene *scene,
                              blender::MutableSpan<MPathTarget *> targets,
                              eAnimvizCalcRange range,
                              bool restore);

/**
 * Update motion path computation range (in `ob.avs` or `armature.avs`) from user choice in
 * `ob.avs.path_range` or `arm.avs.path_range`, depending on active user mode.
 *
 * \param ob: Object to compute range for (must be provided)
 * \param scene: Used when scene range is chosen.
 */
void animviz_motionpath_compute_range(Object *ob, Scene *scene);

/**
 * Populate the given vector with MPathTarget elements for the given object.
 * Will look for pose bones as well. `animviz_free_motionpath_targets` needs to be called
 * to free the memory allocated in this function.
 */
void animviz_build_motionpath_targets(Object *ob, blender::Vector<MPathTarget *> &r_targets);

/**
 * Free the elements of the vector populated with `animviz_build_motionpath_targets`.
 * After this function the Vector will have a length of 0.
 */
void animviz_free_motionpath_targets(blender::Vector<MPathTarget *> &targets);

/** \} */

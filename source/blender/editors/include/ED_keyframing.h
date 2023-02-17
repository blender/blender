/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_anim_types.h"
#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct ListBase;
struct Main;
struct Scene;

struct KeyingSet;

struct AnimationEvalContext;
struct BezTriple;
struct FCurve;
struct bAction;

struct bPoseChannel;

struct ReportList;
struct bContext;

struct EnumPropertyItem;
struct PointerRNA;
struct PropertyRNA;

struct NlaKeyframingContext;

/* -------------------------------------------------------------------- */
/** \name Key-Framing Management
 * \{ */

/**
 * Get the active settings for key-framing settings from context (specifically the given scene)
 * \param use_autokey_mode: include settings from key-framing mode in the result
 * (i.e. replace only).
 */
eInsertKeyFlags ANIM_get_keyframing_flags(struct Scene *scene, bool use_autokey_mode);

/* -------- */

/**
 * Get (or add relevant data to be able to do so) the Active Action for the given
 * Animation Data block, given an ID block where the Animation Data should reside.
 */
struct bAction *ED_id_action_ensure(struct Main *bmain, struct ID *id);

/**
 * Get (or add relevant data to be able to do so) F-Curve from the Active Action,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 */
struct FCurve *ED_action_fcurve_ensure(struct Main *bmain,
                                       struct bAction *act,
                                       const char group[],
                                       struct PointerRNA *ptr,
                                       const char rna_path[],
                                       int array_index);

/**
 * Find the F-Curve from the Active Action,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 */
struct FCurve *ED_action_fcurve_find(struct bAction *act, const char rna_path[], int array_index);

/* -------- */

/**
 * \brief Lesser Key-framing API call.
 *
 * Update integer/discrete flags of the FCurve (used when creating/inserting keyframes,
 * but also through RNA when editing an ID prop, see #37103).
 */
void update_autoflags_fcurve(struct FCurve *fcu,
                             struct bContext *C,
                             struct ReportList *reports,
                             struct PointerRNA *ptr);

/* -------- */

/**
 * \brief Lesser Key-framing API call.
 *
 * Use this when validation of necessary animation data isn't necessary as it already
 * exists, and there is a #BezTriple that can be directly copied into the array.
 *
 * This function adds a given #BezTriple to an F-Curve. It will allocate
 * memory for the array if needed, and will insert the #BezTriple into a
 * suitable place in chronological order.
 *
 * \note any recalculate of the F-Curve that needs to be done will need to be done by the caller.
 */
int insert_bezt_fcurve(struct FCurve *fcu, const struct BezTriple *bezt, eInsertKeyFlags flag);

/**
 * \brief Main Key-framing API call.
 *
 * Use this when validation of necessary animation data isn't necessary as it
 * already exists. It will insert a keyframe using the current value being keyframed.
 * Returns the index at which a keyframe was added (or -1 if failed).
 *
 * This function is a wrapper for #insert_bezt_fcurve(), and should be used when
 * adding a new keyframe to a curve, when the keyframe doesn't exist anywhere else yet.
 * It returns the index at which the keyframe was added.
 *
 * \param keyframe_type: The type of keyframe (#eBezTriple_KeyframeType).
 * \param flag: Optional flags (#eInsertKeyFlags) for controlling how keys get added
 * and/or whether updates get done.
 */
int insert_vert_fcurve(struct FCurve *fcu,
                       float x,
                       float y,
                       eBezTriple_KeyframeType keyframe_type,
                       eInsertKeyFlags flag);

/* -------- */

/**
 * \brief Secondary Insert Key-framing API call.
 *
 * Use this when validation of necessary animation data is not necessary,
 * since an RNA-pointer to the necessary data being keyframed,
 * and a pointer to the F-Curve to use have both been provided.
 *
 * This function can't keyframe quaternion channels on some NLA strip types.
 *
 * \param keytype: The "keyframe type" (eBezTriple_KeyframeType), as shown in the Dope Sheet.
 *
 * \param flag: Used for special settings that alter the behavior of the keyframe insertion.
 * These include the 'visual' key-framing modes, quick refresh,
 * and extra keyframe filtering.
 * \return Success.
 */
bool insert_keyframe_direct(struct ReportList *reports,
                            struct PointerRNA ptr,
                            struct PropertyRNA *prop,
                            struct FCurve *fcu,
                            const struct AnimationEvalContext *anim_eval_context,
                            eBezTriple_KeyframeType keytype,
                            struct NlaKeyframingContext *nla,
                            eInsertKeyFlags flag);

/* -------- */

/**
 * \brief Main Insert Key-framing API call.
 *
 * Use this to create any necessary animation data, and then insert a keyframe
 * using the current value being keyframed, in the relevant place.
 *
 * \param flag: Used for special settings that alter the behavior of the keyframe insertion.
 * These include the 'visual' key-framing modes, quick refresh, and extra keyframe filtering.
 *
 * \param array_index: The index to key or -1 keys all array indices.
 * \return The number of key-frames inserted.
 */
int insert_keyframe(struct Main *bmain,
                    struct ReportList *reports,
                    struct ID *id,
                    struct bAction *act,
                    const char group[],
                    const char rna_path[],
                    int array_index,
                    const struct AnimationEvalContext *anim_eval_context,
                    eBezTriple_KeyframeType keytype,
                    struct ListBase *nla_cache,
                    eInsertKeyFlags flag);

/**
 * \brief Main Delete Key-Framing API call.
 *
 * Use this to delete keyframe on current frame for relevant channel.
 * Will perform checks just in case.
 * \return The number of key-frames deleted.
 */
int delete_keyframe(struct Main *bmain,
                    struct ReportList *reports,
                    struct ID *id,
                    struct bAction *act,
                    const char rna_path[],
                    int array_index,
                    float cfra);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keying Sets
 * \{ */

/* forward decl. for this struct which is declared a bit later... */
struct ExtensionRNA;
struct KeyingSetInfo;

/* Polling Callback for KeyingSets */
typedef bool (*cbKeyingSet_Poll)(struct KeyingSetInfo *ksi, struct bContext *C);
/* Context Iterator Callback for KeyingSets */
typedef void (*cbKeyingSet_Iterator)(struct KeyingSetInfo *ksi,
                                     struct bContext *C,
                                     struct KeyingSet *ks);
/* Property Specifier Callback for KeyingSets (called from iterators) */
typedef void (*cbKeyingSet_Generate)(struct KeyingSetInfo *ksi,
                                     struct bContext *C,
                                     struct KeyingSet *ks,
                                     struct PointerRNA *ptr);

/* Callback info for 'Procedural' KeyingSets to use */
typedef struct KeyingSetInfo {
  struct KeyingSetInfo *next, *prev;

  /* info */
  /* identifier used for class name, which KeyingSet instances reference as "Typeinfo Name" */
  char idname[64];
  /* identifier so that user can hook this up to a KeyingSet (used as label). */
  char name[64];
  /* short help/description. */
  char description[240]; /* RNA_DYN_DESCR_MAX */
  /* keying settings */
  short keyingflag;

  /* polling callbacks */
  /* callback for polling the context for whether the right data is available */
  cbKeyingSet_Poll poll;

  /* generate callbacks */
  /* iterator to use to go through collections of data in context
   * - this callback is separate from the 'adding' stage, allowing
   *   BuiltIn KeyingSets to be manually specified to use
   */
  cbKeyingSet_Iterator iter;
  /* generator to use to add properties based on the data found by iterator */
  cbKeyingSet_Generate generate;

  /* RNA integration */
  struct ExtensionRNA rna_ext;
} KeyingSetInfo;

/* -------- */

/**
 * Add another data source for Relative Keying Sets to be evaluated with.
 */
void ANIM_relative_keyingset_add_source(ListBase *dsources,
                                        struct ID *id,
                                        struct StructRNA *srna,
                                        void *data);

/* mode for modify_keyframes */
typedef enum eModifyKey_Modes {
  MODIFYKEY_MODE_INSERT = 0,
  MODIFYKEY_MODE_DELETE,
} eModifyKey_Modes;

/* return codes for errors (with Relative KeyingSets) */
typedef enum eModifyKey_Returns {
  /* context info was invalid for using the Keying Set */
  MODIFYKEY_INVALID_CONTEXT = -1,
  /* there isn't any typeinfo for generating paths from context */
  MODIFYKEY_MISSING_TYPEINFO = -2,
} eModifyKey_Returns;

/**
 * Given a #KeyingSet and context info, validate Keying Set's paths.
 * This is only really necessary with relative/built-in KeyingSets
 * where their list of paths is dynamically generated based on the
 * current context info.
 *
 * \return 0 if succeeded, otherwise an error code: #eModifyKey_Returns.
 */
eModifyKey_Returns ANIM_validate_keyingset(struct bContext *C,
                                           ListBase *dsources,
                                           struct KeyingSet *ks);

/**
 * Use the specified #KeyingSet and context info (if required)
 * to add/remove various Keyframes on the specified frame.
 *
 * Modify keyframes for the channels specified by the KeyingSet.
 * This takes into account many of the different combinations of using KeyingSets.
 *
 * \returns the number of channels that key-frames were added or
 * an #eModifyKey_Returns value (always a negative number).
 */
int ANIM_apply_keyingset(struct bContext *C,
                         ListBase *dsources,
                         struct bAction *act,
                         struct KeyingSet *ks,
                         short mode,
                         float cfra);

/* -------- */

/**
 * Find builtin #KeyingSet by name.
 *
 * \return The first builtin #KeyingSet with the given name, which occurs after the given one
 * (or start of list if none given).
 */
struct KeyingSet *ANIM_builtin_keyingset_get_named(struct KeyingSet *prevKS, const char name[]);

/**
 * Find KeyingSet type info given a name.
 */
KeyingSetInfo *ANIM_keyingset_info_find_name(const char name[]);

/**
 * Check if the ID appears in the paths specified by the #KeyingSet.
 */
bool ANIM_keyingset_find_id(struct KeyingSet *ks, ID *id);

/**
 * Add the given KeyingSetInfo to the list of type infos,
 * and create an appropriate builtin set too.
 */
void ANIM_keyingset_info_register(KeyingSetInfo *ksi);
/**
 * Remove the given #KeyingSetInfo from the list of type infos,
 * and also remove the builtin set if appropriate.
 */
void ANIM_keyingset_info_unregister(struct Main *bmain, KeyingSetInfo *ksi);

/* cleanup on exit */
/* --------------- */

void ANIM_keyingset_infos_exit(void);

/* -------- */

/**
 * Get the active Keying Set for the given scene.
 */
struct KeyingSet *ANIM_scene_get_active_keyingset(const struct Scene *scene);

/**
 * Get the index of the Keying Set provided, for the given Scene.
 */
int ANIM_scene_get_keyingset_index(struct Scene *scene, struct KeyingSet *ks);

/**
 * Get Keying Set to use for Auto-Key-Framing some transforms.
 */
struct KeyingSet *ANIM_get_keyingset_for_autokeying(const struct Scene *scene,
                                                    const char *transformKSName);

void ANIM_keyingset_visit_for_search(const struct bContext *C,
                                     struct PointerRNA *ptr,
                                     struct PropertyRNA *prop,
                                     const char *edit_text,
                                     StringPropertySearchVisitFunc visit_fn,
                                     void *visit_user_data);

void ANIM_keyingset_visit_for_search_no_poll(const struct bContext *C,
                                             struct PointerRNA *ptr,
                                             struct PropertyRNA *prop,
                                             const char *edit_text,
                                             StringPropertySearchVisitFunc visit_fn,
                                             void *visit_user_data);
/**
 * Dynamically populate an enum of Keying Sets.
 */
const struct EnumPropertyItem *ANIM_keying_sets_enum_itemf(struct bContext *C,
                                                           struct PointerRNA *ptr,
                                                           struct PropertyRNA *prop,
                                                           bool *r_free);

/**
 * Get the keying set from enum values generated in #ANIM_keying_sets_enum_itemf.
 *
 * Type is the Keying Set the user specified to use when calling the operator:
 * \param type:
 * - == 0: use scene's active Keying Set.
 * -  > 0: use a user-defined Keying Set from the active scene.
 * -  < 0: use a builtin Keying Set.
 */
KeyingSet *ANIM_keyingset_get_from_enum_type(struct Scene *scene, int type);
KeyingSet *ANIM_keyingset_get_from_idname(struct Scene *scene, const char *idname);

/**
 * Check if #KeyingSet can be used in the current context.
 */
bool ANIM_keyingset_context_ok_poll(struct bContext *C, struct KeyingSet *ks);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drivers
 * \{ */

/* Flags for use by driver creation calls */
typedef enum eCreateDriverFlags {
  /** create drivers with a default variable for nicer UI */
  CREATEDRIVER_WITH_DEFAULT_DVAR = (1 << 0),
  /** create drivers with Generator FModifier (for backwards compat) */
  CREATEDRIVER_WITH_FMODIFIER = (1 << 1),
} eCreateDriverFlags;

/* Heuristic to use for connecting target properties to driven ones */
typedef enum eCreateDriver_MappingTypes {
  /** 1 to Many - Use the specified index, and drive all elements with it */
  CREATEDRIVER_MAPPING_1_N = 0,
  /** 1 to 1 - Only for the specified index on each side */
  CREATEDRIVER_MAPPING_1_1 = 1,
  /** Many to Many - Match up the indices one by one (only for drivers on vectors/arrays) */
  CREATEDRIVER_MAPPING_N_N = 2,

  /** None (Single Prop):
   * Do not create driver with any targets; these will get added later instead */
  CREATEDRIVER_MAPPING_NONE = 3,
  /** None (All Properties):
   * Do not create driver with any targets; these will get added later instead */
  CREATEDRIVER_MAPPING_NONE_ALL = 4,
} eCreateDriver_MappingTypes;

/**
 * Mapping Types enum for operators.
 * \note Used by #ANIM_OT_driver_button_add and #UI_OT_eyedropper_driver.
 */
extern EnumPropertyItem prop_driver_create_mapping_types[];

/* -------- */

typedef enum eDriverFCurveCreationMode {
  DRIVER_FCURVE_LOOKUP_ONLY = 0, /* Don't add anything if not found. */
  DRIVER_FCURVE_KEYFRAMES = 1,   /* Add with keyframes, for visual tweaking. */
  DRIVER_FCURVE_GENERATOR = 2,   /* Add with generator, for script backwards compatibility. */
  DRIVER_FCURVE_EMPTY = 3        /* Add without data, for pasting. */
} eDriverFCurveCreationMode;

/**
 * Get (or add relevant data to be able to do so) F-Curve from the driver stack,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 *
 * \note This low-level function shouldn't be used directly for most tools,
 * although there are special cases where this approach is preferable.
 */
struct FCurve *verify_driver_fcurve(struct ID *id,
                                    const char rna_path[],
                                    int array_index,
                                    eDriverFCurveCreationMode creation_mode);

struct FCurve *alloc_driver_fcurve(const char rna_path[],
                                   int array_index,
                                   eDriverFCurveCreationMode creation_mode);

/* -------- */

/**
 * \brief Main Driver Management API calls
 *
 * Add a new driver for the specified property on the given ID block,
 * and make it be driven by the specified target.
 *
 * This is intended to be used in conjunction with a modal "eyedropper"
 * for picking the variable that is going to be used to drive this one.
 *
 * \param flag: eCreateDriverFlags
 * \param driver_type: eDriver_Types
 * \param mapping_type: eCreateDriver_MappingTypes
 */
int ANIM_add_driver_with_target(struct ReportList *reports,
                                struct ID *dst_id,
                                const char dst_path[],
                                int dst_index,
                                struct ID *src_id,
                                const char src_path[],
                                int src_index,
                                short flag,
                                int driver_type,
                                short mapping_type);

/* -------- */

/**
 * \brief Main Driver Management API calls.
 *
 * Add a new driver for the specified property on the given ID block
 */
int ANIM_add_driver(struct ReportList *reports,
                    struct ID *id,
                    const char rna_path[],
                    int array_index,
                    short flag,
                    int type);

/**
 * \brief Main Driver Management API calls.
 *
 * Remove the driver for the specified property on the given ID block (if available).
 */
bool ANIM_remove_driver(
    struct ReportList *reports, struct ID *id, const char rna_path[], int array_index, short flag);

/* -------- */

/**
 * Clear copy-paste buffer for drivers.
 * \note This function frees any MEM_calloc'ed copy/paste buffer data.
 */
void ANIM_drivers_copybuf_free(void);

/**
 * Clear copy-paste buffer for driver variable sets.
 * \note This function frees any MEM_calloc'ed copy/paste buffer data.
 */
void ANIM_driver_vars_copybuf_free(void);

/* -------- */

/**
 * Returns whether there is a driver in the copy/paste buffer to paste.
 */
bool ANIM_driver_can_paste(void);

/**
 * \brief Main Driver Management API calls.
 *
 * Make a copy of the driver for the specified property on the given ID block.
 */
bool ANIM_copy_driver(
    struct ReportList *reports, struct ID *id, const char rna_path[], int array_index, short flag);

/**
 * \brief Main Driver Management API calls.
 *
 * Add a new driver for the specified property on the given ID block or replace an existing one
 * with the driver + driver-curve data from the buffer.
 */
bool ANIM_paste_driver(
    struct ReportList *reports, struct ID *id, const char rna_path[], int array_index, short flag);

/* -------- */

/**
 * Checks if there are driver variables in the copy/paste buffer.
 */
bool ANIM_driver_vars_can_paste(void);

/**
 * Copy the given driver's variables to the buffer.
 */
bool ANIM_driver_vars_copy(struct ReportList *reports, struct FCurve *fcu);

/**
 * Paste the variables in the buffer to the given FCurve.
 */
bool ANIM_driver_vars_paste(struct ReportList *reports, struct FCurve *fcu, bool replace);

/* -------- */

/**
 * Create a driver & variable that reads the specified property,
 * and store it in the buffers for Paste Driver and Paste Variables.
 */
void ANIM_copy_as_driver(struct ID *target_id, const char *target_path, const char *var_name);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-Key-Framing
 *
 * Notes:
 * - All the defines for this (User-Pref settings and Per-Scene settings)
 *   are defined in DNA_userdef_types.h
 * - Scene settings take precedence over those for user-preferences, with old files
 *   inheriting user-preferences settings for the scene settings
 * - "On/Off + Mode" are stored per Scene, but "settings" are currently stored as user-preferences.
 * \{ */

/* Auto-Keying macros for use by various tools. */

/** Check if auto-key-framing is enabled (per scene takes precedence).
 */
#define IS_AUTOKEY_ON(scene) \
  ((scene) ? ((scene)->toolsettings->autokey_mode & AUTOKEY_ON) : (U.autokey_mode & AUTOKEY_ON))
/** Check the mode for auto-keyframing (per scene takes precedence). */
#define IS_AUTOKEY_MODE(scene, mode) \
  ((scene) ? ((scene)->toolsettings->autokey_mode == AUTOKEY_MODE_##mode) : \
             (U.autokey_mode == AUTOKEY_MODE_##mode))
/** Check if a flag is set for auto-key-framing (per scene takes precedence). */
#define IS_AUTOKEY_FLAG(scene, flag) \
  ((scene) ? (((scene)->toolsettings->autokey_flag & AUTOKEY_FLAG_##flag) || \
              (U.autokey_flag & AUTOKEY_FLAG_##flag)) : \
             (U.autokey_flag & AUTOKEY_FLAG_##flag))

/**
 * Auto-keyframing feature - checks for whether anything should be done for the current frame.
 */
bool autokeyframe_cfra_can_key(const struct Scene *scene, struct ID *id);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframe Checking
 * \{ */

/**
 * \brief Lesser Keyframe Checking API call.
 *
 * Checks if some F-Curve has a keyframe for a given frame.
 * \note Used for the buttons to check for keyframes.
 */
bool fcurve_frame_has_keyframe(const struct FCurve *fcu, float frame, short filter);

/**
 * \brief Lesser Keyframe Checking API call.
 *
 * - Returns whether the current value of a given property differs from the interpolated value.
 * - Used for button drawing.
 */
bool fcurve_is_changed(struct PointerRNA ptr,
                       struct PropertyRNA *prop,
                       struct FCurve *fcu,
                       const struct AnimationEvalContext *anim_eval_context);

/**
 * \brief Main Keyframe Checking API call.
 *
 * Checks whether a keyframe exists for the given ID-block one the given frame.
 * It is recommended to call this method over the other keyframe-checkers directly,
 * in case some detail of the implementation changes...
 * \param frame: The value of this is quite often result of #BKE_scene_ctime_get()
 */
bool id_frame_has_keyframe(struct ID *id, float frame, short filter);

/**
 * Filter flags for #id_frame_has_keyframe.
 *
 * \warning do not alter order of these, as also stored in files (for `v3d->keyflags`).
 */
typedef enum eAnimFilterFlags {
  /* general */
  ANIMFILTER_KEYS_LOCAL = (1 << 0),  /* only include locally available anim data */
  ANIMFILTER_KEYS_MUTED = (1 << 1),  /* include muted elements */
  ANIMFILTER_KEYS_ACTIVE = (1 << 2), /* only include active-subelements */

  /* object specific */
  ANIMFILTER_KEYS_NOMAT = (1 << 9),   /* don't include material keyframes */
  ANIMFILTER_KEYS_NOSKEY = (1 << 10), /* don't include shape keys (for geometry) */
} eAnimFilterFlags;

/* Utility functions for auto key-frame. */

bool ED_autokeyframe_object(struct bContext *C,
                            struct Scene *scene,
                            struct Object *ob,
                            struct KeyingSet *ks);
bool ED_autokeyframe_pchan(struct bContext *C,
                           struct Scene *scene,
                           struct Object *ob,
                           struct bPoseChannel *pchan,
                           struct KeyingSet *ks);

/**
 * Use for auto-key-framing
 * \param only_if_property_keyed: if true, auto-key-framing only creates keyframes on already keyed
 * properties. This is by design when using buttons. For other callers such as gizmos or VSE
 * preview transform, creating new animation/keyframes also on non-keyed properties is desired.
 */
bool ED_autokeyframe_property(struct bContext *C,
                              struct Scene *scene,
                              PointerRNA *ptr,
                              PropertyRNA *prop,
                              int rnaindex,
                              float cfra,
                              bool only_if_property_keyed);

/* Names for builtin keying sets so we don't confuse these with labels/text,
 * defined in python script: `keyingsets_builtins.py`. */

#define ANIM_KS_LOCATION_ID "Location"
#define ANIM_KS_ROTATION_ID "Rotation"
#define ANIM_KS_SCALING_ID "Scaling"
#define ANIM_KS_LOC_ROT_SCALE_ID "LocRotScale"
#define ANIM_KS_LOC_ROT_SCALE_CPROP_ID "LocRotScaleCProp"
#define ANIM_KS_AVAILABLE_ID "Available"
#define ANIM_KS_WHOLE_CHARACTER_ID "WholeCharacter"
#define ANIM_KS_WHOLE_CHARACTER_SELECTED_ID "WholeCharacterSelected"

/** \} */

#ifdef __cplusplus
}
#endif

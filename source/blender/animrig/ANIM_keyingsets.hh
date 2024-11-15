/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functionality to interact with keying sets.
 */

#pragma once

struct KeyingSet;
struct ExtensionRNA;
/* Forward declaration for this struct which is declared a bit later. */
struct KeyingSetInfo;
struct bContext;
struct ID;
struct Scene;
struct PointerRNA;

/* Names for builtin keying sets so we don't confuse these with labels/text,
 * defined in python script: `keyingsets_builtins.py`. */

static constexpr const char *ANIM_KS_LOCATION_ID = "Location";
static constexpr const char *ANIM_KS_ROTATION_ID = "Rotation";
static constexpr const char *ANIM_KS_SCALING_ID = "Scaling";
static constexpr const char *ANIM_KS_LOC_ROT_SCALE_ID = "LocRotScale";
static constexpr const char *ANIM_KS_LOC_ROT_SCALE_CPROP_ID = "LocRotScaleCProp";
static constexpr const char *ANIM_KS_AVAILABLE_ID = "Available";
static constexpr const char *ANIM_KS_WHOLE_CHARACTER_ID = "WholeCharacter";
static constexpr const char *ANIM_KS_WHOLE_CHARACTER_SELECTED_ID = "WholeCharacterSelected";

/** Polling Callback for KeyingSets. */
using cbKeyingSet_Poll = bool (*)(KeyingSetInfo *ksi, bContext *C);
/** Context Iterator Callback for KeyingSets. */
using cbKeyingSet_Iterator = void (*)(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks);
/** Property Specifier Callback for KeyingSets (called from iterators) */
using cbKeyingSet_Generate = void (*)(KeyingSetInfo *ksi,
                                      bContext *C,
                                      KeyingSet *ks,
                                      PointerRNA *ptr);

/** Callback info for 'Procedural' KeyingSets to use. */
struct KeyingSetInfo {
  KeyingSetInfo *next, *prev;

  /* info */
  /** Identifier used for class name, which KeyingSet instances reference as "Type-info Name". */
  char idname[64];
  /** identifier so that user can hook this up to a KeyingSet (used as label). */
  char name[64];
  /** Short help/description. */
  char description[1024]; /* #RNA_DYN_DESCR_MAX */
  /** Keying settings. */
  short keyingflag;

  /* polling callbacks */
  /** callback for polling the context for whether the right data is available. */
  cbKeyingSet_Poll poll;

  /* generate callbacks */
  /**
   * Iterator to use to go through collections of data in context
   * - this callback is separate from the 'adding' stage, allowing
   *   BuiltIn KeyingSets to be manually specified to use.
   */
  cbKeyingSet_Iterator iter;
  /** Generator to use to add properties based on the data found by iterator. */
  cbKeyingSet_Generate generate;

  /** RNA integration. */
  ExtensionRNA rna_ext;
};

namespace blender::animrig {

/** Mode for modify_keyframes. */
enum class ModifyKeyMode {
  INSERT = 0,
  /* Not calling it just `DELETE` because that interferes with a macro on windows. */
  DELETE_KEY,
};

/** Return codes for errors (with Relative KeyingSets). */
enum class ModifyKeyReturn {
  SUCCESS = 0,
  /** Context info was invalid for using the Keying Set. */
  INVALID_CONTEXT = -1,
  /** There isn't any type-info for generating paths from context. */
  MISSING_TYPEINFO = -2,
};

/* -------------------------------------------------------------------- */
/** \name Keyingset Usage
 * \{ */

/**
 * Given a #KeyingSet and context info, validate Keying Set's paths.
 * This is only really necessary with relative/built-in KeyingSets
 * where their list of paths is dynamically generated based on the
 * current context info.
 *
 * \note Passing sources as pointer because it can be a nullptr.
 */
ModifyKeyReturn validate_keyingset(bContext *C,
                                   blender::Vector<PointerRNA> *sources,
                                   KeyingSet *keyingset);

/**
 * Use the specified #KeyingSet and context info (if required)
 * to add/remove various Keyframes on the specified frame.
 *
 * Modify keyframes for the channels specified by the KeyingSet.
 * This takes into account many of the different combinations of using KeyingSets.
 *
 * \returns the number of channels that key-frames were added or
 * an #ModifyKeyReturn error (always a negative number).
 */
int apply_keyingset(bContext *C,
                    blender::Vector<PointerRNA> *sources,
                    KeyingSet *keyingset,
                    ModifyKeyMode mode,
                    float cfra);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Queries
 * \{ */

/**
 * Find builtin #KeyingSet by name.
 *
 * \return The first builtin #KeyingSet with the given name
 */
KeyingSet *builtin_keyingset_get_named(const char name[]);

/**
 * Find KeyingSet type info given a name.
 */
KeyingSetInfo *keyingset_info_find_name(const char name[]);

/**
 * Check if the ID appears in the paths specified by the #KeyingSet.
 */
bool keyingset_find_id(KeyingSet *keyingset, ID *id);

/**
 * Get Keying Set to use for Auto-Key-Framing some transforms.
 */
KeyingSet *get_keyingset_for_autokeying(const Scene *scene, const char *transformKSName);

/**
 * Get the active Keying Set for the given scene.
 */
KeyingSet *scene_get_active_keyingset(const Scene *scene);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation, Destruction
 * \{ */

/**
 * Add the given KeyingSetInfo to the list of type infos,
 * and create an appropriate builtin set too.
 */
void keyingset_info_register(KeyingSetInfo *keyingset_info);
/**
 * Remove the given #KeyingSetInfo from the list of type infos,
 * and also remove the builtin set if appropriate.
 */
void keyingset_info_unregister(Main *bmain, KeyingSetInfo *keyingset_info);

void keyingset_infos_exit();

/**
 * Add another data source for Relative Keying Sets to be evaluated with.
 */
void relative_keyingset_add_source(blender::Vector<PointerRNA> &sources,
                                   ID *id,
                                   StructRNA *srna,
                                   void *data);
void relative_keyingset_add_source(blender::Vector<PointerRNA> &sources, ID *id);

/** \} */

}  // namespace blender::animrig

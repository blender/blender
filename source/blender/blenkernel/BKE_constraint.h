/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct Depsgraph;
struct ID;
struct ListBase;
struct Object;
struct Scene;
struct bConstraint;
struct bConstraintTarget;
struct bPoseChannel;

/* ---------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

/* special struct for use in constraint evaluation */
typedef struct bConstraintOb {
  /** to get evaluated armature. */
  struct Depsgraph *depsgraph;
  /** for system time, part of de-globalization, code nicer later with local time (ton) */
  struct Scene *scene;
  /** if pchan, then armature that it comes from, otherwise constraint owner */
  struct Object *ob;
  /** pose channel that owns the constraints being evaluated */
  struct bPoseChannel *pchan;

  /** matrix where constraints are accumulated + solved */
  float matrix[4][4];
  /** original matrix (before constraint solving) */
  float startmat[4][4];
  /** space matrix for custom object space */
  float space_obj_world_matrix[4][4];

  /** type of owner. */
  short type;
  /** rotation order for constraint owner (as defined in #eEulerRotationOrders in
   * BLI_math_rotation.h) */
  short rotOrder;
} bConstraintOb;

/* ---------------------------------------------------------------------------- */

/* Callback format for performing operations on ID-pointers for Constraints */
typedef void (*ConstraintIDFunc)(struct bConstraint *con,
                                 struct ID **idpoin,
                                 bool is_reference,
                                 void *userdata);

/* ....... */

/**
 * Constraint Type-Info (shorthand in code = `cti`):
 * This struct provides function pointers for runtime, so that functions can be
 * written more generally (with fewer/no special exceptions for various constraints).
 *
 * Callers of these functions must check that they actually point to something useful,
 * as some constraints don't define some of these.
 *
 * WARNING:
 * it is not too advisable to reorder order of members of this struct,
 * as you'll have to edit quite a few #NUM_CONSTRAINT_TYPES of these
 * structs.
 */
typedef struct bConstraintTypeInfo {
  /* admin/ident */
  /** CONSTRAINT_TYPE_### */
  short type;
  /** size in bytes of the struct */
  short size;
  /** name of constraint in interface */
  char name[32];
  /** name of struct for SDNA */
  char struct_name[32];

  /* data management function pointers - special handling */
  /** free any data that is allocated separately (optional) */
  void (*free_data)(struct bConstraint *con);
  /** run the provided callback function on all the ID-blocks linked to the constraint */
  void (*id_looper)(struct bConstraint *con, ConstraintIDFunc func, void *userdata);
  /** copy any special data that is allocated separately (optional) */
  void (*copy_data)(struct bConstraint *con, struct bConstraint *src);
  /**
   * Set settings for data that will be used for #bConstraint.data
   * (memory already allocated using #MEM_callocN).
   */
  void (*new_data)(void *cdata);

  /* target handling function pointers */
  /**
   * For multi-target constraints: return that list;
   * otherwise make a temporary list (returns number of targets).
   */
  int (*get_constraint_targets)(struct bConstraint *con, struct ListBase *list);
  /**
   * For single-target constraints only:
   * flush data back to source data, and the free memory used.
   */
  void (*flush_constraint_targets)(struct bConstraint *con, struct ListBase *list, bool no_copy);

  /* evaluation */
  /** set the ct->matrix for the given constraint target (at the given ctime) */
  void (*get_target_matrix)(struct Depsgraph *depsgraph,
                            struct bConstraint *con,
                            struct bConstraintOb *cob,
                            struct bConstraintTarget *ct,
                            float ctime);
  /**
   * Evaluate the constraint for the given time.
   * solved as separate loop.
   */
  void (*evaluate_constraint)(struct bConstraint *con,
                              struct bConstraintOb *cob,
                              struct ListBase *targets);
} bConstraintTypeInfo;

/* Function Prototypes for bConstraintTypeInfo's */

/**
 * This function should always be used to get the appropriate type-info, as it
 * has checks which prevent segfaults in some weird cases.
 */
const bConstraintTypeInfo *BKE_constraint_typeinfo_get(struct bConstraint *con);
/**
 * This function should be used for getting the appropriate type-info when only
 * a constraint type is known.
 */
const bConstraintTypeInfo *BKE_constraint_typeinfo_from_type(int type);

/* ---------------------------------------------------------------------------- */

/* Constraint function prototypes */

/**
 * Find the first available, non-duplicate name for a given constraint.
 */
void BKE_constraint_unique_name(struct bConstraint *con, struct ListBase *list);

/**
 * Allocate and duplicate a single constraint, outside of any object/pose context.
 */
struct bConstraint *BKE_constraint_duplicate_ex(struct bConstraint *src, int flag, bool do_extern);

/**
 * Add a copy of the given constraint for the given bone.
 */
struct bConstraint *BKE_constraint_copy_for_pose(struct Object *ob,
                                                 struct bPoseChannel *pchan,
                                                 struct bConstraint *src);
/**
 * Add a copy of the given constraint for the given object.
 */
struct bConstraint *BKE_constraint_copy_for_object(struct Object *ob, struct bConstraint *src);

void BKE_constraints_free(struct ListBase *list);
/**
 * Free all constraints from a constraint-stack.
 */
void BKE_constraints_free_ex(struct ListBase *list, bool do_id_user);
void BKE_constraints_copy(struct ListBase *dst, const struct ListBase *src, bool do_extern);
/**
 * Duplicate all of the constraints in a constraint stack.
 */
void BKE_constraints_copy_ex(struct ListBase *dst,
                             const struct ListBase *src,
                             int flag,
                             bool do_extern);
/**
 * Run the given callback on all ID-blocks in list of constraints.
 */
void BKE_constraints_id_loop(struct ListBase *list,
                             ConstraintIDFunc func,
                             const int flag,
                             void *userdata);
void BKE_constraint_free_data(struct bConstraint *con);
/**
 * Free data of a specific constraint if it has any info.
 * Be sure to run #BIK_clear_data() when freeing an IK constraint,
 * unless #DAG_relations_tag_update is called.
 */
void BKE_constraint_free_data_ex(struct bConstraint *con, bool do_id_user);

bool BKE_constraint_target_uses_bbone(struct bConstraint *con, struct bConstraintTarget *ct);

/* Constraint API function prototypes */

/**
 * Finds the 'active' constraint in a constraint stack.
 */
struct bConstraint *BKE_constraints_active_get(struct ListBase *list);
/**
 * Set the given constraint as the active one (clearing all the others).
 */
void BKE_constraints_active_set(ListBase *list, struct bConstraint *con);
struct bConstraint *BKE_constraints_find_name(struct ListBase *list, const char *name);

/**
 * Finds the constraint that owns the given target within the object.
 */
struct bConstraint *BKE_constraint_find_from_target(struct Object *ob,
                                                    struct bConstraintTarget *tgt,
                                                    struct bPoseChannel **r_pchan);

/**
 * Check whether given constraint is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param con: May be NULL, in which case we consider it as a non-local constraint case.
 */
bool BKE_constraint_is_nonlocal_in_liboverride(const struct Object *ob,
                                               const struct bConstraint *con);

/**
 * Add new constraint for the given object.
 */
struct bConstraint *BKE_constraint_add_for_object(struct Object *ob, const char *name, short type);
/**
 * Add new constraint for the given bone.
 */
struct bConstraint *BKE_constraint_add_for_pose(struct Object *ob,
                                                struct bPoseChannel *pchan,
                                                const char *name,
                                                short type);

bool BKE_constraint_remove_ex(ListBase *list,
                              struct Object *ob,
                              struct bConstraint *con,
                              bool clear_dep);
/**
 * Remove the specified constraint from the given constraint stack.
 */
bool BKE_constraint_remove(ListBase *list, struct bConstraint *con);

/**
 * Apply the specified constraint in the given constraint stack.
 */
bool BKE_constraint_apply_for_object(struct Depsgraph *depsgraph,
                                     struct Scene *scene,
                                     struct Object *ob,
                                     struct bConstraint *con);
bool BKE_constraint_apply_and_remove_for_object(struct Depsgraph *depsgraph,
                                                struct Scene *scene,
                                                ListBase /*bConstraint*/ *constraints,
                                                struct Object *ob,
                                                struct bConstraint *con);

bool BKE_constraint_apply_for_pose(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   struct bPoseChannel *pchan,
                                   struct bConstraint *con);
bool BKE_constraint_apply_and_remove_for_pose(struct Depsgraph *depsgraph,
                                              struct Scene *scene,
                                              ListBase /*bConstraint*/ *constraints,
                                              struct Object *ob,
                                              struct bConstraint *con,
                                              struct bPoseChannel *pchan);

void BKE_constraint_panel_expand(struct bConstraint *con);

/* Constraint Evaluation function prototypes */

/**
 * Package an object/bone for use in constraint evaluation.
 *
 * This function MEM_calloc's a #bConstraintOb struct,
 * that will need to be freed after evaluation.
 */
struct bConstraintOb *BKE_constraints_make_evalob(struct Depsgraph *depsgraph,
                                                  struct Scene *scene,
                                                  struct Object *ob,
                                                  void *subdata,
                                                  short datatype);
/**
 * Cleanup after constraint evaluation.
 */
void BKE_constraints_clear_evalob(struct bConstraintOb *cob);

/**
 * This function is responsible for the correct transformations/conversions
 * of a matrix from one space to another for constraint evaluation.
 * For now, this is only implemented for objects and pose-channels.
 */
void BKE_constraint_mat_convertspace(struct Object *ob,
                                     struct bPoseChannel *pchan,
                                     struct bConstraintOb *cob,
                                     float mat[4][4],
                                     short from,
                                     short to,
                                     bool keep_scale);

/**
 * This function is a relic from the prior implementations of the constraints system, when all
 * constraints either had one or no targets. It used to be called during the main constraint
 * solving loop, but is now only used for the remaining cases for a few constraints.
 *
 * None of the actual calculations of the matrices should be done here! Also, this function is
 * not to be used by any new constraints, particularly any that have multiple targets.
 */
void BKE_constraint_target_matrix_get(struct Depsgraph *depsgraph,
                                      struct Scene *scene,
                                      struct bConstraint *con,
                                      int index,
                                      short ownertype,
                                      void *ownerdata,
                                      float mat[4][4],
                                      float ctime);

/**
 * Retrieves the list of all constraint targets, including the custom space target.
 * Must be followed by a call to BKE_constraint_targets_flush to free memory.
 *
 * \param r_targets: Pointer to the list to be initialized with target data.
 * \returns the number of targets stored in the list.
 */
int BKE_constraint_targets_get(struct bConstraint *con, struct ListBase *r_targets);

/**
 * Copies changed data from the list produced by #BKE_constraint_targets_get back to the constraint
 * data structures and frees memory.
 *
 * \param targets: List of targets filled by BKE_constraint_targets_get.
 * \param no_copy: Only free memory without copying changes (read-only mode).
 */
void BKE_constraint_targets_flush(struct bConstraint *con, struct ListBase *targets, bool no_copy);

/**
 * Get the list of targets required for solving a constraint.
 */
void BKE_constraint_targets_for_solving_get(struct Depsgraph *depsgraph,
                                            struct bConstraint *con,
                                            struct bConstraintOb *ob,
                                            struct ListBase *targets,
                                            float ctime);

/**
 * Initialize the Custom Space matrix inside `cob` (if required by the constraint).
 *
 * \param cob: Constraint evaluation context (contains the matrix to be initialized).
 * \param con: Constraint that is about to be evaluated.
 */
void BKE_constraint_custom_object_space_init(struct bConstraintOb *cob, struct bConstraint *con);

/**
 * This function is called whenever constraints need to be evaluated. Currently, all
 * constraints that can be evaluated are every time this gets run.
 *
 * #BKE_constraints_make_evalob and #BKE_constraints_clear_evalob should be called before and
 * after running this function, to sort out cob.
 */
void BKE_constraints_solve(struct Depsgraph *depsgraph,
                           struct ListBase *conlist,
                           struct bConstraintOb *cob,
                           float ctime);

void BKE_constraint_blend_write(struct BlendWriter *writer, struct ListBase *conlist);
void BKE_constraint_blend_read_data(struct BlendDataReader *reader,
                                    struct ID *id_owner,
                                    struct ListBase *lb);
void BKE_constraint_blend_read_lib(struct BlendLibReader *reader,
                                   struct ID *id,
                                   struct ListBase *conlist);
void BKE_constraint_blend_read_expand(struct BlendExpander *expander, struct ListBase *lb);

#ifdef __cplusplus
}
#endif

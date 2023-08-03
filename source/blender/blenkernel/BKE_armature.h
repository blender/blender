/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */
#include "BLI_listbase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimationEvalContext;
struct BMEditMesh;
struct Bone;
struct Depsgraph;
struct IDProperty;
struct ListBase;
struct Main;
struct Mesh;
struct Object;
struct PoseTree;
struct Scene;
struct bAction;
struct bArmature;
struct bConstraint;
struct bGPDstroke;
struct bPose;
struct bPoseChannel;

typedef struct EditBone {
  struct EditBone *next, *prev;
  /** User-Defined Properties on this Bone */
  struct IDProperty *prop;
  /**
   * Edit-bones have a one-way link  (i.e. children refer
   * to parents.  This is converted to a two-way link for
   * normal bones when leaving edit-mode.
   */
  struct EditBone *parent;
  /** (64 == MAXBONENAME) */
  char name[64];
  /**
   * Roll along axis.  We'll ultimately use the axis/angle method
   * for determining the transformation matrix of the bone.  The axis
   * is tail-head while roll provides the angle. Refer to Graphics
   * Gems 1 p. 466 (section IX.6) if it's not already in here somewhere.
   */
  float roll;

  /** Orientation and length is implicit during editing */
  float head[3];
  float tail[3];
  /**
   * All joints are considered to have zero rotation with respect to
   * their parents. Therefore any rotations specified during the
   * animation are automatically relative to the bones' rest positions.
   */
  int flag;
  int layer;
  char inherit_scale_mode;

  /* Envelope distance & weight */
  float dist, weight;
  /** put them in order! transform uses this as scale */
  float xwidth, length, zwidth;
  float rad_head, rad_tail;

  /* Bendy-Bone parameters */
  short segments;
  float roll1, roll2;
  float curve_in_x, curve_in_z;
  float curve_out_x, curve_out_z;
  float ease1, ease2;
  float scale_in[3], scale_out[3];

  /** for envelope scaling */
  float oldlength;

  /** Type of next/prev bone handles */
  char bbone_prev_type;
  char bbone_next_type;
  /** B-Bone flags. */
  int bbone_flag;
  short bbone_prev_flag;
  short bbone_next_flag;
  /** Next/prev bones to use as handle references when calculating bbones (optional) */
  struct EditBone *bbone_prev;
  struct EditBone *bbone_next;

  /* Used for display */
  /** in Armature space, rest pos matrix */
  float disp_mat[4][4];
  /** in Armature space, rest pos matrix */
  float disp_tail_mat[4][4];
  /** in Armature space, rest pos matrix (32 == MAX_BBONE_SUBDIV) */
  float disp_bbone_mat[32][4][4];

  /** connected child temporary during drawing */
  struct EditBone *bbone_child;

  /* Used to store temporary data */
  union {
    struct EditBone *ebone;
    struct Bone *bone;
    void *p;
    int i;
  } temp;
} EditBone;

typedef struct PoseTarget {
  struct PoseTarget *next, *prev;

  struct bConstraint *con; /* the constraint of this target */
  int tip;                 /* index of tip pchan in PoseTree */
} PoseTarget;

typedef struct PoseTree {
  struct PoseTree *next, *prev;

  int type;       /* type of IK that this serves (CONSTRAINT_TYPE_KINEMATIC or ..._SPLINEIK) */
  int totchannel; /* number of pose channels */

  struct ListBase targets;     /* list of targets of the tree */
  struct bPoseChannel **pchan; /* array of pose channels */
  int *parent;                 /* and their parents */

  float (*basis_change)[3][3]; /* basis change result from solver */
  int iterations;              /* iterations from the constraint */
  int stretch;                 /* disable stretching */
} PoseTree;

/* Core armature functionality. */

struct bArmature *BKE_armature_add(struct Main *bmain, const char *name);
struct bArmature *BKE_armature_from_object(struct Object *ob);
int BKE_armature_bonelist_count(const struct ListBase *lb);
void BKE_armature_bonelist_free(struct ListBase *lb, bool do_id_user);
void BKE_armature_editbonelist_free(struct ListBase *lb, bool do_id_user);

void BKE_armature_copy_bone_transforms(struct bArmature *armature_dst,
                                       const struct bArmature *armature_src);

void BKE_armature_transform(struct bArmature *arm, const float mat[4][4], bool do_props);

/* Bounding box. */
struct BoundBox *BKE_armature_boundbox_get(struct Object *ob);

/**
 * Calculate the axis-aligned bounds of `pchan` in world-space,
 * taking into account custom transform when set.
 *
 * `r_min` and `r_max` are expanded to fit `pchan` so the caller must initialize them
 * (typically using #INIT_MINMAX).
 *
 * \note The bounds are calculated based on the head & tail of the bone
 * or the custom object's bounds (if the bone uses a custom object).
 * Visual elements such as the envelopes radius & bendy-bone spline segments are *not* included,
 * making this not so useful for viewport culling.
 *
 * \param use_empty_drawtype: When enabled, the draw type of empty custom-objects is taken into
 * account when calculating the bounds.
 */
void BKE_pchan_minmax(const struct Object *ob,
                      const struct bPoseChannel *pchan,
                      const bool use_empty_drawtype,
                      float r_min[3],
                      float r_max[3]);
/**
 * Calculate the axis aligned bounds of the pose of `ob` in world-space.
 *
 * `r_min` and `r_max` are expanded to fit `ob->pose` so the caller must initialize them
 * (typically using #INIT_MINMAX).
 *
 * \note This uses #BKE_pchan_minmax, see its documentation for details on bounds calculation.
 */
bool BKE_pose_minmax(
    struct Object *ob, float r_min[3], float r_max[3], bool use_hidden, bool use_select);

/**
 * Finds the best possible extension to the name on a particular axis.
 * (For renaming, check for unique names afterwards)
 * \param strip_number: removes number extensions (TODO: not used).
 * \param axis: The axis to name on.
 * \param head: The head co-ordinate of the bone on the specified axis.
 * \param tail: The tail co-ordinate of the bone on the specified axis.
 */
bool bone_autoside_name(char name[64], int strip_number, short axis, float head, float tail);

/**
 * Walk the list until the bone is found (slow!),
 * use #BKE_armature_bone_from_name_map for multiple lookups.
 */
struct Bone *BKE_armature_find_bone_name(struct bArmature *arm, const char *name);

void BKE_armature_bone_hash_make(struct bArmature *arm);
void BKE_armature_bone_hash_free(struct bArmature *arm);

bool BKE_armature_bone_flag_test_recursive(const struct Bone *bone, int flag);

void BKE_armature_refresh_layer_used(struct Depsgraph *depsgraph, struct bArmature *arm);

/**
 * Using `vec` with dist to bone `b1 - b2`.
 */
float distfactor_to_bone(
    const float vec[3], const float b1[3], const float b2[3], float rad1, float rad2, float rdist);

/**
 * Updates vectors and matrices on rest-position level, only needed
 * after editing armature itself, now only on reading file.
 */
void BKE_armature_where_is(struct bArmature *arm);
/**
 * Recursive part, calculates rest-position of entire tree of children.
 * \note Used when exiting edit-mode too.
 */
void BKE_armature_where_is_bone(struct Bone *bone,
                                const struct Bone *bone_parent,
                                bool use_recursion);
/**
 * Clear pointers of object's pose
 * (needed in remap case, since we cannot always wait for a complete pose rebuild).
 */
void BKE_pose_clear_pointers(struct bPose *pose);
void BKE_pose_remap_bone_pointers(struct bArmature *armature, struct bPose *pose);
/**
 * Update the links for the B-Bone handles from Bone data.
 */
void BKE_pchan_rebuild_bbone_handles(struct bPose *pose, struct bPoseChannel *pchan);
void BKE_pose_channels_clear_with_null_bone(struct bPose *pose, bool do_id_user);
/**
 * Only after leave edit-mode, duplicating, validating older files, library syncing.
 *
 * \note pose->flag is set for it.
 *
 * \param bmain: May be NULL, only used to tag depsgraph as being dirty.
 */
void BKE_pose_rebuild(struct Main *bmain,
                      struct Object *ob,
                      struct bArmature *arm,
                      bool do_id_user);
/**
 * Ensures object's pose is rebuilt if needed.
 *
 * \param bmain: May be NULL, only used to tag depsgraph as being dirty.
 */
void BKE_pose_ensure(struct Main *bmain,
                     struct Object *ob,
                     struct bArmature *arm,
                     bool do_id_user);
/**
 * \note This is the only function adding poses.
 * \note This only reads anim data from channels, and writes to channels.
 */
void BKE_pose_where_is(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob);
/**
 * The main armature solver, does all constraints excluding IK.
 *
 * \param pchan: pose-channel - validated, as having bone and parent pointer.
 * \param do_extra: when zero skips loc/size/rot, constraints and strip modifiers.
 */
void BKE_pose_where_is_bone(struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            struct Object *ob,
                            struct bPoseChannel *pchan,
                            float ctime,
                            bool do_extra);
/**
 * Calculate tail of pose-channel.
 */
void BKE_pose_where_is_bone_tail(struct bPoseChannel *pchan);

/**
 * Evaluate the action and apply it to the pose. If any pose bones are selected, only FCurves that
 * relate to those bones are evaluated.
 */
void BKE_pose_apply_action_selected_bones(struct Object *ob,
                                          struct bAction *action,
                                          struct AnimationEvalContext *anim_eval_context);
/**
 * Evaluate the action and apply it to the pose. Ignore selection state of the bones.
 */
void BKE_pose_apply_action_all_bones(struct Object *ob,
                                     struct bAction *action,
                                     struct AnimationEvalContext *anim_eval_context);

void BKE_pose_apply_action_blend(struct Object *ob,
                                 struct bAction *action,
                                 struct AnimationEvalContext *anim_eval_context,
                                 float blend_factor);

void vec_roll_to_mat3(const float vec[3], float roll, float r_mat[3][3]);

/**
 * Calculates the rest matrix of a bone based on its vector and a roll around that vector.
 */
void vec_roll_to_mat3_normalized(const float nor[3], float roll, float r_mat[3][3]);
/**
 * Computes vector and roll based on a rotation.
 * "mat" must contain only a rotation, and no scaling.
 */
void mat3_to_vec_roll(const float mat[3][3], float r_vec[3], float *r_roll);
/**
 * Computes roll around the vector that best approximates the matrix.
 * If `vec` is the Y vector from purely rotational `mat`, result should be exact.
 */
void mat3_vec_to_roll(const float mat[3][3], const float vec[3], float *r_roll);

/* Common Conversions Between Co-ordinate Spaces */

/**
 * Convert World-Space Matrix to Pose-Space Matrix.
 */
void BKE_armature_mat_world_to_pose(struct Object *ob,
                                    const float inmat[4][4],
                                    float outmat[4][4]);
/**
 * Convert World-Space Location to Pose-Space Location
 * \note this cannot be used to convert to pose-space location of the supplied
 * pose-channel into its local space (i.e. 'visual'-keyframing).
 */
void BKE_armature_loc_world_to_pose(struct Object *ob, const float inloc[3], float outloc[3]);
/**
 * Convert Pose-Space Matrix to Bone-Space Matrix.
 * \note this cannot be used to convert to pose-space transforms of the supplied
 * pose-channel into its local space (i.e. 'visual'-keyframing).
 */
void BKE_armature_mat_pose_to_bone(struct bPoseChannel *pchan,
                                   const float inmat[4][4],
                                   float outmat[4][4]);
/**
 * Convert Pose-Space Location to Bone-Space Location
 * \note this cannot be used to convert to pose-space location of the supplied
 * pose-channel into its local space (i.e. 'visual'-keyframing).
 */
void BKE_armature_loc_pose_to_bone(struct bPoseChannel *pchan,
                                   const float inloc[3],
                                   float outloc[3]);
/**
 * Convert Bone-Space Matrix to Pose-Space Matrix.
 */
void BKE_armature_mat_bone_to_pose(struct bPoseChannel *pchan,
                                   const float inmat[4][4],
                                   float outmat[4][4]);
/**
 * Remove rest-position effects from pose-transform for obtaining
 * 'visual' transformation of pose-channel.
 * (used by the Visual-Keyframing stuff).
 */
void BKE_armature_mat_pose_to_delta(float delta_mat[4][4],
                                    float pose_mat[4][4],
                                    float arm_mat[4][4]);

void BKE_armature_mat_pose_to_bone_ex(struct Depsgraph *depsgraph,
                                      struct Object *ob,
                                      struct bPoseChannel *pchan,
                                      const float inmat[4][4],
                                      float outmat[4][4]);

/**
 * Same as #BKE_object_mat3_to_rot().
 */
void BKE_pchan_mat3_to_rot(struct bPoseChannel *pchan, const float mat[3][3], bool use_compat);
/**
 * Same as #BKE_object_rot_to_mat3().
 */
void BKE_pchan_rot_to_mat3(const struct bPoseChannel *pchan, float r_mat[3][3]);
/**
 * Apply a 4x4 matrix to the pose bone,
 * similar to #BKE_object_apply_mat4().
 */
void BKE_pchan_apply_mat4(struct bPoseChannel *pchan, const float mat[4][4], bool use_compat);
/**
 * Convert the loc/rot/size to \a r_chanmat (typically #bPoseChannel.chan_mat).
 */
void BKE_pchan_to_mat4(const struct bPoseChannel *pchan, float r_chanmat[4][4]);

/**
 * Convert the loc/rot/size to mat4 (`pchan.chan_mat`),
 * used in `constraint.cc` too.
 */
void BKE_pchan_calc_mat(struct bPoseChannel *pchan);

/**
 * Simple helper, computes the offset bone matrix:
 * `offs_bone = yoffs(b-1) + root(b) + bonemat(b)`.
 */
void BKE_bone_offset_matrix_get(const struct Bone *bone, float offs_bone[4][4]);

/* Transformation inherited from the parent bone. These matrices apply the effects of
 * HINGE/NO_SCALE/NO_LOCAL_LOCATION options over the pchan loc/rot/scale transformations. */
typedef struct BoneParentTransform {
  float rotscale_mat[4][4]; /* parent effect on rotation & scale pose channels */
  float loc_mat[4][4];      /* parent effect on location pose channel */
  float post_scale[3];      /* additional scale to apply with post-multiply */
} BoneParentTransform;

/* Matrix-like algebra operations on the transform */
void BKE_bone_parent_transform_clear(struct BoneParentTransform *bpt);
void BKE_bone_parent_transform_invert(struct BoneParentTransform *bpt);
void BKE_bone_parent_transform_combine(const struct BoneParentTransform *in1,
                                       const struct BoneParentTransform *in2,
                                       struct BoneParentTransform *result);

void BKE_bone_parent_transform_apply(const struct BoneParentTransform *bpt,
                                     const float inmat[4][4],
                                     float outmat[4][4]);

/**
 * Get the current parent transformation for the given pose bone.
 *
 * Construct the matrices (rot/scale and loc)
 * to apply the PoseChannels into the armature (object) space.
 * I.e. (roughly) the `pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b)` in the
 *     `pose_mat(b)= pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b)`
 * ...function.
 *
 * This allows to get the transformations of a bone in its object space,
 * *before* constraints (and IK) get applied (used by pose evaluation code).
 * And reverse: to find pchan transformations needed to place a bone at a given loc/rot/scale
 * in object space (used by interactive transform, and snapping code).
 *
 * Note that, with the HINGE/NO_SCALE/NO_LOCAL_LOCATION options, the location matrix
 * will differ from the rotation/scale matrix...
 *
 * \note This cannot be used to convert to pose-space transforms of the supplied
 * pose-channel into its local space (i.e. 'visual'-keyframing).
 * (NOTE(@mont29): I don't understand that, so I keep it :p).
 */
void BKE_bone_parent_transform_calc_from_pchan(const struct bPoseChannel *pchan,
                                               struct BoneParentTransform *r_bpt);
/**
 * Compute the parent transform using data decoupled from specific data structures.
 *
 * \param bone_flag: #Bone.flag containing settings.
 * \param offs_bone: delta from parent to current arm_mat (or just arm_mat if no parent).
 * \param parent_arm_mat: arm_mat of parent, or NULL.
 * \param parent_pose_mat: pose_mat of parent, or NULL.
 * \param r_bpt: OUTPUT parent transform.
 */
void BKE_bone_parent_transform_calc_from_matrices(int bone_flag,
                                                  int inherit_scale_mode,
                                                  const float offs_bone[4][4],
                                                  const float parent_arm_mat[4][4],
                                                  const float parent_pose_mat[4][4],
                                                  struct BoneParentTransform *r_bpt);

/**
 * Rotation Mode Conversions - Used for Pose-Channels + Objects.
 *
 * Called from RNA when rotation mode changes
 * - the result should be that the rotations given in the provided pointers have had conversions
 *   applied (as appropriate), such that the rotation of the element hasn't 'visually' changed.
 */
void BKE_rotMode_change_values(
    float quat[4], float eul[3], float axis[3], float *angle, short oldMode, short newMode);

/* B-Bone support */
#define MAX_BBONE_SUBDIV 32

typedef struct Mat4 {
  float mat[4][4];
} Mat4;

typedef struct BBoneSplineParameters {
  int segments;
  float length;

  /* Non-uniform scale correction. */
  bool do_scale;
  float scale[3];

  /* Handle control bone data. */
  bool use_prev, prev_bbone;
  bool use_next, next_bbone;

  float prev_h[3], next_h[3];
  float prev_mat[4][4], next_mat[4][4];

  /* Control values. */
  float ease1, ease2;
  float roll1, roll2;
  float scale_in[3], scale_out[3];
  float curve_in_x, curve_in_z, curve_out_x, curve_out_z;
} BBoneSplineParameters;

/**
 * Get "next" and "prev" bones - these are used for handle calculations.
 */
void BKE_pchan_bbone_handles_get(struct bPoseChannel *pchan,
                                 struct bPoseChannel **r_prev,
                                 struct bPoseChannel **r_next);
/**
 * Compute B-Bone spline parameters for the given channel.
 */
void BKE_pchan_bbone_spline_params_get(struct bPoseChannel *pchan,
                                       bool rest,
                                       struct BBoneSplineParameters *r_param);

/**
 * Fills the array with the desired amount of bone->segments elements.
 * This calculation is done within unit bone space.
 */
void BKE_pchan_bbone_spline_setup(struct bPoseChannel *pchan,
                                  bool rest,
                                  bool for_deform,
                                  Mat4 *result_array);

/**
 * Computes the bezier handle vectors and rolls coming from custom handles.
 */
void BKE_pchan_bbone_handles_compute(const BBoneSplineParameters *param,
                                     float h1[3],
                                     float *r_roll1,
                                     float h2[3],
                                     float *r_roll2,
                                     bool ease,
                                     bool offsets);
/**
 * Fills the array with the desired amount of `bone->segments` elements.
 * This calculation is done within unit bone space.
 */
int BKE_pchan_bbone_spline_compute(struct BBoneSplineParameters *param,
                                   bool for_deform,
                                   Mat4 *result_array);

/**
 * Compute and cache the B-Bone shape in the channel runtime struct.
 */
void BKE_pchan_bbone_segments_cache_compute(struct bPoseChannel *pchan);
/**
 * Copy cached B-Bone segments from one channel to another.
 */
void BKE_pchan_bbone_segments_cache_copy(struct bPoseChannel *pchan,
                                         struct bPoseChannel *pchan_from);

/**
 * Calculate index and blend factor for the two B-Bone segment nodes
 * affecting the specified point along the bone.
 *
 * \param pchan: Pose channel.
 * \param head_tail: head-tail position along the bone (auto-clamped between 0 and 1).
 * \param r_index: OUTPUT index of the first segment joint affecting the point.
 * \param r_blend_next: OUTPUT blend factor between the first and the second segment in [0..1]
 */
void BKE_pchan_bbone_deform_clamp_segment_index(const struct bPoseChannel *pchan,
                                                float head_tail,
                                                int *r_index,
                                                float *r_blend_next);

/**
 * Calculate index and blend factor for the two B-Bone segment nodes
 * affecting the specified point in object (pose) space.
 *
 * \param pchan: Pose channel.
 * \param co: Pose space coordinates of the point being deformed.
 * \param r_index: OUTPUT index of the first segment joint affecting the point.
 * \param r_blend_next: OUTPUT blend factor between the first and the second segment in [0..1]
 */
void BKE_pchan_bbone_deform_segment_index(const struct bPoseChannel *pchan,
                                          const float *co,
                                          int *r_index,
                                          float *r_blend_next);

/* like EBONE_VISIBLE */
#define PBONE_VISIBLE(arm, bone) \
  (CHECK_TYPE_INLINE(arm, bArmature *), \
   CHECK_TYPE_INLINE(bone, Bone *), \
   (((bone)->layer & (arm)->layer) && !((bone)->flag & BONE_HIDDEN_P)))

#define PBONE_SELECTABLE(arm, bone) \
  (PBONE_VISIBLE(arm, bone) && !((bone)->flag & BONE_UNSELECTABLE))

#define PBONE_SELECTED(arm, bone) (((bone)->flag & BONE_SELECTED) & PBONE_VISIBLE(arm, bone))

/* context.selected_pose_bones */
#define FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN(_ob, _pchan) \
  for (bPoseChannel *_pchan = (bPoseChannel *)(_ob)->pose->chanbase.first; _pchan; \
       _pchan = _pchan->next) \
  { \
    if (PBONE_VISIBLE(((bArmature *)(_ob)->data), (_pchan)->bone) && \
        ((_pchan)->bone->flag & BONE_SELECTED)) \
    {
#define FOREACH_PCHAN_SELECTED_IN_OBJECT_END \
  } \
  } \
  ((void)0)
/* context.visible_pose_bones */
#define FOREACH_PCHAN_VISIBLE_IN_OBJECT_BEGIN(_ob, _pchan) \
  for (bPoseChannel *_pchan = (bPoseChannel *)(_ob)->pose->chanbase.first; _pchan; \
       _pchan = _pchan->next) \
  { \
    if (PBONE_VISIBLE(((bArmature *)(_ob)->data), (_pchan)->bone)) {
#define FOREACH_PCHAN_VISIBLE_IN_OBJECT_END \
  } \
  } \
  ((void)0)

/* Evaluation helpers */
struct bKinematicConstraint;
struct bPose;
struct bSplineIKConstraint;

struct bPoseChannel *BKE_armature_ik_solver_find_root(struct bPoseChannel *pchan,
                                                      struct bKinematicConstraint *data);
struct bPoseChannel *BKE_armature_splineik_solver_find_root(struct bPoseChannel *pchan,
                                                            struct bSplineIKConstraint *data);

void BKE_pose_splineik_init_tree(struct Scene *scene, struct Object *ob, float ctime);
void BKE_splineik_execute_tree(struct Depsgraph *depsgraph,
                               struct Scene *scene,
                               struct Object *ob,
                               struct bPoseChannel *pchan_root,
                               float ctime);

void BKE_pose_pchan_index_rebuild(struct bPose *pose);

void BKE_pose_eval_init(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *object);

void BKE_pose_eval_init_ik(struct Depsgraph *depsgraph,
                           struct Scene *scene,
                           struct Object *object);

void BKE_pose_eval_bone(struct Depsgraph *depsgraph,
                        struct Scene *scene,
                        struct Object *object,
                        int pchan_index);

void BKE_pose_constraints_evaluate(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *object,
                                   int pchan_index);

void BKE_pose_bone_done(struct Depsgraph *depsgraph, struct Object *object, int pchan_index);

void BKE_pose_eval_bbone_segments(struct Depsgraph *depsgraph,
                                  struct Object *object,
                                  int pchan_index);

void BKE_pose_iktree_evaluate(struct Depsgraph *depsgraph,
                              struct Scene *scene,
                              struct Object *object,
                              int rootchan_index);

void BKE_pose_splineik_evaluate(struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Object *object,
                                int rootchan_index);

void BKE_pose_eval_done(struct Depsgraph *depsgraph, struct Object *object);

void BKE_pose_eval_cleanup(struct Depsgraph *depsgraph,
                           struct Scene *scene,
                           struct Object *object);

/* -------------------------------------------------------------------- */
/** \name Deform 3D Coordinates by Armature (`armature_deform.cc`)
 * \{ */

/* Note that we could have a 'BKE_armature_deform_coords' that doesn't take object data
 * currently there are no callers for this though. */

void BKE_armature_deform_coords_with_gpencil_stroke(const struct Object *ob_arm,
                                                    const struct Object *ob_target,
                                                    float (*vert_coords)[3],
                                                    float (*vert_deform_mats)[3][3],
                                                    int vert_coords_len,
                                                    int deformflag,
                                                    float (*vert_coords_prev)[3],
                                                    const char *defgrp_name,
                                                    struct bGPDstroke *gps_target);

void BKE_armature_deform_coords_with_mesh(const struct Object *ob_arm,
                                          const struct Object *ob_target,
                                          float (*vert_coords)[3],
                                          float (*vert_deform_mats)[3][3],
                                          int vert_coords_len,
                                          int deformflag,
                                          float (*vert_coords_prev)[3],
                                          const char *defgrp_name,
                                          const struct Mesh *me_target);

void BKE_armature_deform_coords_with_editmesh(const struct Object *ob_arm,
                                              const struct Object *ob_target,
                                              float (*vert_coords)[3],
                                              float (*vert_deform_mats)[3][3],
                                              int vert_coords_len,
                                              int deformflag,
                                              float (*vert_coords_prev)[3],
                                              const char *defgrp_name,
                                              struct BMEditMesh *em_target);

/** \} */

#ifdef __cplusplus
}
#endif

/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Original design: Reevan McKay
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 * Contributor(s): Animation recode, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_action_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_ACTION_TYPES_H__
#define __DNA_ACTION_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h" /* ThemeWireColor */

struct SpaceLink;
struct Object;
struct Group;
struct GHash;

/* ************************************************ */
/* Visualisation */

/* Motion Paths ------------------------------------ */
/* (used for Pose Channels and Objects) */

/* Data point for motion path (mpv) */
typedef struct bMotionPathVert {
	float co[3];				/* coordinates of point in 3D-space */
	int flag;					/* quick settings */
} bMotionPathVert;

/* bMotionPathVert->flag */
typedef enum eMotionPathVert_Flag {
		/* vert is selected */
	MOTIONPATH_VERT_SEL		= (1<<0)
} eMotionPathVert_Flag;

/* ........ */

/* Motion Path data cache (mpath)
 * 	- for elements providing transforms (i.e. Objects or PoseChannels) 
 */
typedef struct bMotionPath {
	bMotionPathVert *points;	/* path samples */
	int	length;					/* the number of cached verts */
	
	int start_frame;			/* for drawing paths, the start frame number */
	int	end_frame;				/* for drawing paths, the end frame number */
	
	int flag;					/* baking settings - eMotionPath_Flag */ 
} bMotionPath;

/* bMotionPath->flag */
typedef enum eMotionPath_Flag {
		/* (for bones) path represents the head of the bone */
	MOTIONPATH_FLAG_BHEAD		= (1<<0),
		/* motion path is being edited */
	MOTIONPATH_FLAG_EDIT		= (1<<1)
} eMotionPath_Flag;

/* Visualisation General --------------------------- */
/* for Objects or Poses (but NOT PoseChannels) */

/* Animation Visualisation Settings (avs) */
typedef struct bAnimVizSettings {
	/* Onion-Skinning Settings ----------------- */
	int	ghost_sf, ghost_ef;			/* start and end frames of ghost-drawing range (only used for GHOST_TYPE_RANGE) */
	int ghost_bc, ghost_ac;			/* number of frames before/after current frame to show */
	
	short ghost_type;				/* eOnionSkin_Types */
	short ghost_step;				/* number of frames between each ghost shown (not for GHOST_TYPE_KEYS) */
	
	short ghost_flag;				/* eOnionSkin_Flag */
	
	/* General Settings ------------------------ */
	short recalc;					/* eAnimViz_RecalcFlags */
	
	/* Motion Path Settings ------------------- */
	short path_type;				/* eMotionPath_Types */
	short path_step;				/* number of frames between points indicated on the paths */
	
	short path_viewflag;			/* eMotionPaths_ViewFlag */
	short path_bakeflag;			/* eMotionPaths_BakeFlag */
	
	int path_sf, path_ef;			/* start and end frames of path-calculation range */
	int	path_bc, path_ac;			/* number of frames before/after current frame to show */
} bAnimVizSettings;


/* bAnimVizSettings->recalc */
typedef enum eAnimViz_RecalcFlags {
		/* motionpaths need recalculating */
	ANIMVIZ_RECALC_PATHS	= (1<<0)
} eAnimViz_RecalcFlags;


/* bAnimVizSettings->ghost_type */
typedef enum eOnionSkin_Types {
		/* no ghosts at all */
	GHOST_TYPE_NONE = 0,
		/* around current frame */
	GHOST_TYPE_ACFRA,
		/* show ghosts within the specified frame range */
	GHOST_TYPE_RANGE,
		/* show ghosts on keyframes within the specified range only */
	GHOST_TYPE_KEYS
} eOnionSkin_Types;

/* bAnimVizSettings->ghost_flag */
typedef enum eOnionSkin_Flag {
		/* only show selected bones in ghosts */
	GHOST_FLAG_ONLYSEL 	= (1<<0)
} eOnionSkin_Flag;


/* bAnimVizSettings->path_type */
typedef enum eMotionPaths_Types {
		/* show the paths along their entire ranges */
	MOTIONPATH_TYPE_RANGE = 0,
		/* only show the parts of the paths around the current frame */
	MOTIONPATH_TYPE_ACFRA
} eMotionPath_Types;

/* bAnimVizSettings->path_viewflag */
typedef enum eMotionPaths_ViewFlag {
		/* show frames on path */
	MOTIONPATH_VIEW_FNUMS		= (1<<0),
		/* show keyframes on path */
	MOTIONPATH_VIEW_KFRAS		= (1<<1),
		/* show keyframe/frame numbers */
	MOTIONPATH_VIEW_KFNOS		= (1<<2),
		/* find keyframes in whole action (instead of just in matching group name) */
	MOTIONPATH_VIEW_KFACT		= (1<<3)
} eMotionPath_ViewFlag;

/* bAnimVizSettings->path_bakeflag */
typedef enum eMotionPaths_BakeFlag {
		/* motion paths directly associated with this block of settings needs updating */
	MOTIONPATH_BAKE_NEEDS_RECALC	= (1<<0),
		/* for bones - calculate head-points for curves instead of tips */
	MOTIONPATH_BAKE_HEADS			= (1<<1),
		/* motion paths exist for AnimVizSettings instance - set when calc for first time, and unset when clearing */
	MOTIONPATH_BAKE_HAS_PATHS		= (1<<2)
} eMotionPath_BakeFlag;

/* ************************************************ */
/* Poses */

/* PoseChannel ------------------------------------ */

/* PoseChannel 
 *
 * A PoseChannel stores the results of Actions and transform information 
 * with respect to the restposition of Armature bones 
 */
typedef struct bPoseChannel {
	struct bPoseChannel	*next, *prev;
	
	IDProperty 			*prop;		/* User-Defined Properties on this PoseChannel */			
	
	ListBase			constraints;/* Constraints that act on this PoseChannel */
	char				name[64];	/* need to match bone name length: MAXBONENAME */
	
	short				flag;		/* dynamic, for detecting transform changes */
	short				ikflag;		/* settings for IK bones */
	short				protectflag; /* protect channels from being transformed */
	short				agrp_index; /* index of action-group this bone belongs to (0 = default/no group) */
	char				constflag;  /* for quick detecting which constraints affect this channel */
	char                selectflag;	/* copy of bone flag, so you can work with library armatures, not for runtime use */
	char				pad0[6];

	struct Bone			*bone;		/* set on read file or rebuild pose */
	struct bPoseChannel *parent;	/* set on read file or rebuild pose */
	struct bPoseChannel *child;		/* set on read file or rebuild pose, the 'ik' child, for b-bones */
	
	struct ListBase		 iktree;		/* "IK trees" - only while evaluating pose */
	struct ListBase 	siktree;		/* Spline-IK "trees" - only while evaluating pose */
	
	bMotionPath *mpath;				/* motion path cache for this bone */
	struct Object *custom;			/* draws custom object instead of default bone shape */
	struct bPoseChannel *custom_tx;	/* odd feature, display with another bones transform.
	                                 * needed in rare cases for advanced rigs,
	                                 * since the alternative is highly complicated - campbell */

		/* transforms - written in by actions or transform */
	float		loc[3];				
	float		size[3];
	
		/* rotations - written in by actions or transform (but only one representation gets used at any time) */
	float 		eul[3];					/* euler rotation */
	float		quat[4];				/* quaternion rotation */
	float 		rotAxis[3], rotAngle;	/* axis-angle rotation */
	short 		rotmode;				/* eRotationModes - rotation representation to use */
	short 		pad;
	
	float		chan_mat[4][4];		/* matrix result of loc/quat/size , and where we put deform in, see next line */
	float		pose_mat[4][4];		/* constraints accumulate here. in the end, pose_mat = bone->arm_mat * chan_mat
	                                 * this matrix is object space */
	float		constinv[4][4];		/* inverse result of constraints.
	                                 * doesn't include effect of restposition, parent, and local transform*/
	
	float		pose_head[3];		/* actually pose_mat[3] */
	float		pose_tail[3];		/* also used for drawing help lines... */
	
	float		limitmin[3], limitmax[3];	/* DOF constraint, note! - these are stored in degrees, not radians */
	float		stiffness[3];				/* DOF stiffness */
	float		ikstretch;
	float		ikrotweight;		/* weight of joint rotation constraint */
	float		iklinweight;		/* weight of joint stretch constraint */

	void		*temp;				/* use for outliner */
} bPoseChannel;


/* PoseChannel (transform) flags */
typedef enum ePchan_Flag {
		/* has transforms */
	POSE_LOC		=	(1<<0),
	POSE_ROT		=	(1<<1),
	POSE_SIZE		=	(1<<2),
		/* old IK/cache stuff... */
	POSE_IK_MAT		=	(1<<3),
	POSE_UNUSED2	=	(1<<4),
	POSE_UNUSED3	=	(1<<5),
	POSE_UNUSED4	=	(1<<6),
	POSE_UNUSED5	=	(1<<7),
		/* has Standard IK */
	POSE_HAS_IK		=	(1<<8),
		/* IK/Pose solving*/
	POSE_CHAIN		=	(1<<9),
	POSE_DONE		=   (1<<10),
		/* visualisation */
	POSE_KEY		=	(1<<11),
	POSE_STRIDE		=	(1<<12),
		/* standard IK solving */
	POSE_IKTREE		=   (1<<13),
		/* has Spline IK */
	POSE_HAS_IKS	= 	(1<<14),
		/* spline IK solving */
	POSE_IKSPLINE	= 	(1<<15)
} ePchan_Flag;

/* PoseChannel constflag (constraint detection) */
typedef enum ePchan_ConstFlag {
	PCHAN_HAS_IK		= (1<<0),
	PCHAN_HAS_CONST		= (1<<1),
		/* only used for drawing Posemode, not stored in channel */
	PCHAN_HAS_ACTION	= (1<<2),
	PCHAN_HAS_TARGET	= (1<<3),
		/* only for drawing Posemode too */
	PCHAN_HAS_STRIDE	= (1<<4),
		/* spline IK */
	PCHAN_HAS_SPLINEIK	= (1<<5)
} ePchan_ConstFlag;

/* PoseChannel->ikflag */
typedef enum ePchan_IkFlag {
	BONE_IK_NO_XDOF = (1<<0),
	BONE_IK_NO_YDOF = (1<<1),
	BONE_IK_NO_ZDOF = (1<<2),

	BONE_IK_XLIMIT	= (1<<3),
	BONE_IK_YLIMIT	= (1<<4),
	BONE_IK_ZLIMIT	= (1<<5),
	
	BONE_IK_ROTCTL  = (1<<6),
	BONE_IK_LINCTL  = (1<<7),

	BONE_IK_NO_XDOF_TEMP = (1<<10),
	BONE_IK_NO_YDOF_TEMP = (1<<11),
	BONE_IK_NO_ZDOF_TEMP = (1<<12)
} ePchan_IkFlag;

/* PoseChannel->rotmode and Object->rotmode */
typedef enum eRotationModes {
		/* quaternion rotations (default, and for older Blender versions) */
	ROT_MODE_QUAT	= 0,
		/* euler rotations - keep in sync with enum in BLI_math.h */
	ROT_MODE_EUL = 1,		/* Blender 'default' (classic) - must be as 1 to sync with BLI_math_rotation.h defines */
	ROT_MODE_XYZ = 1,
	ROT_MODE_XZY,
	ROT_MODE_YXZ,
	ROT_MODE_YZX,
	ROT_MODE_ZXY,
	ROT_MODE_ZYX,
	/* NOTE: space is reserved here for 18 other possible 
	 * euler rotation orders not implemented 
	 */
		/* axis angle rotations */
	ROT_MODE_AXISANGLE = -1,

	ROT_MODE_MIN = ROT_MODE_AXISANGLE,	/* sentinel for Py API */
	ROT_MODE_MAX = ROT_MODE_ZYX
} eRotationModes;

/* Pose ------------------------------------ */

/* Pose-Object. 
 *
 * It is only found under ob->pose. It is not library data, even
 * though there is a define for it (hack for the outliner).
 */
typedef struct bPose {
	ListBase chanbase; 			/* list of pose channels, PoseBones in RNA */
	struct GHash *chanhash;		/* ghash for quicker string lookups */
	
	short flag, pad;
	unsigned int proxy_layer;	/* proxy layer: copy from armature, gets synced */
	int pad1;
	
	float ctime;				/* local action time of this pose */
	float stride_offset[3];		/* applied to object */
	float cyclic_offset[3];		/* result of match and cycles, applied in where_is_pose() */
	
	
	ListBase agroups;			/* list of bActionGroups */
	
	int active_group;			/* index of active group (starts from 1) */
	int	iksolver;				/* ik solver to use, see ePose_IKSolverType */
	void *ikdata;				/* temporary IK data, depends on the IK solver. Not saved in file */
	void *ikparam;				/* IK solver parameters, structure depends on iksolver */ 
	
	bAnimVizSettings avs;		/* settings for visualization of bone animation */
	char proxy_act_bone[64];    /* proxy active bone name, MAXBONENAME */
} bPose;


/* Pose->flag */
typedef enum ePose_Flags {
		/* results in armature_rebuild_pose being called */
	POSE_RECALC = (1<<0),
		/* prevents any channel from getting overridden by anim from IPO */
	POSE_LOCKED	= (1<<1),
		/* clears the POSE_LOCKED flag for the next time the pose is evaluated */
	POSE_DO_UNLOCK	= (1<<2),
		/* pose has constraints which depend on time (used when depsgraph updates for a new frame) */
	POSE_CONSTRAINTS_TIMEDEPEND = (1<<3),
		/* recalculate bone paths */
	POSE_RECALCPATHS = (1<<4),
		/* set by armature_rebuild_pose to give a chance to the IK solver to rebuild IK tree */
	POSE_WAS_REBUILT = (1<<5),
		/* set by game_copy_pose to indicate that this pose is used in the game engine */
	POSE_GAME_ENGINE = (1<<6)
} ePose_Flags;

/* IK Solvers ------------------------------------ */

/* bPose->iksolver and bPose->ikparam->iksolver */
typedef enum ePose_IKSolverType {
	IKSOLVER_LEGACY = 0,
	IKSOLVER_ITASC
} ePose_IKSolverType;

/* header for all bPose->ikparam structures */
typedef struct bIKParam {
	int   iksolver;
} bIKParam;

/* bPose->ikparam when bPose->iksolver=1 */
typedef struct bItasc {
	int   iksolver;
	float precision;
	short numiter;
	short numstep;
	float minstep;
	float maxstep;
	short solver;	
	short flag;
	float feedback;
	float maxvel;	/* max velocity to SDLS solver */
	float dampmax;	/* maximum damping for DLS solver */
	float dampeps;	/* threshold of singular value from which the damping start progressively */
} bItasc;

/* bItasc->flag */
typedef enum eItasc_Flags {
	ITASC_AUTO_STEP = (1<<0),
	ITASC_INITIAL_REITERATION = (1<<1),
	ITASC_REITERATION = (1<<2),
	ITASC_SIMULATION = (1<<3)
} eItasc_Flags;

/* bItasc->solver */
typedef enum eItasc_Solver {
	ITASC_SOLVER_SDLS = 0,	/* selective damped least square, suitable for CopyPose constraint */
	ITASC_SOLVER_DLS		/* damped least square with numerical filtering of damping */
} eItasc_Solver;

/* ************************************************ */
/* Action */

/* Groups -------------------------------------- */

/* Action-Channel Group (agrp)
 *
 * These are stored as a list per-Action, and are only used to 
 * group that Action's channels in an Animation Editor. 
 *
 * Even though all FCurves live in a big list per Action, each group they are in also
 * holds references to the achans within that list which belong to it. Care must be taken to
 * ensure that action-groups never end up being the sole 'owner' of a channel.
 * 
 * This is also exploited for bone-groups. Bone-Groups are stored per bPose, and are used 
 * primarily to color bones in the 3d-view. There are other benefits too, but those are mostly related
 * to Action-Groups.
 *
 * Note that these two uses each have their own RNA 'ActionGroup' and 'BoneGroup'.
 */
typedef struct bActionGroup {
	struct bActionGroup *next, *prev;
	
	ListBase channels;			/* Note: this must not be touched by standard listbase functions which would clear links to other channels */
	
	int flag;					/* settings for this action-group */
	int customCol;				/* index of custom color set to use when used for bones (0=default - used for all old files, -1=custom set) */				
	char name[64];				/* name of the group */
	
	ThemeWireColor cs;			/* color set to use when customCol == -1 */
} bActionGroup;

/* Action Group flags */
typedef enum eActionGroup_Flag {
		/* group is selected */
	AGRP_SELECTED 	= (1<<0),
		/* group is 'active' / last selected one */
	AGRP_ACTIVE 	= (1<<1),
		/* keyframes/channels belonging to it cannot be edited */
	AGRP_PROTECTED 	= (1<<2),
		/* for UI (DopeSheet), sub-channels are shown */
	AGRP_EXPANDED 	= (1<<3),
		/* sub-channels are not evaluated */
	AGRP_MUTED		= (1<<4),
		/* sub-channels are not visible in Graph Editor */
	AGRP_NOTVISIBLE	= (1<<5),
		/* for UI (Graph Editor), sub-channels are shown */
	AGRP_EXPANDED_G	= (1<<6),
	
	AGRP_TEMP		= (1<<30),
	AGRP_MOVED 		= (1<<31)
} eActionGroup_Flag;


/* Actions -------------------------------------- */

/* Action - reusable F-Curve 'bag'  (act) 
 *
 * This contains F-Curves that may affect settings from more than one ID blocktype and/or 
 * datablock (i.e. sub-data linked/used directly to the ID block that the animation data is linked to), 
 * but with the restriction that the other unrelated data (i.e. data that is not directly used or linked to
 * by the source ID block).
 *
 * It serves as a 'unit' of reusable animation information (i.e. keyframes/motion data), that 
 * affects a group of related settings (as defined by the user). 
 */
typedef struct bAction {
	ID 	id;				/* ID-serialisation for relinking */
	
	ListBase curves;	/* function-curves (FCurve) */
	ListBase chanbase;	/* legacy data - Action Channels (bActionChannel) in pre-2.5 animation system */
	ListBase groups;	/* groups of function-curves (bActionGroup) */
	ListBase markers;	/* markers local to the Action (used to provide Pose-Libraries) */
	
	int flag;			/* settings for this action */
	int active_marker;	/* index of the active marker */
	
	int idroot;			/* type of ID-blocks that action can be assigned to (if 0, will be set to whatever ID first evaluates it) */
	int pad;
} bAction;


/* Flags for the action */
typedef enum eAction_Flags {
		/* flags for displaying in UI */
	ACT_COLLAPSED	= (1<<0),
	ACT_SELECTED	= (1<<1),
	
		/* flags for evaluation/editing */
	ACT_MUTED		= (1<<9),
	ACT_PROTECTED	= (1<<10),
	ACT_DISABLED	= (1<<11)
} eAction_Flags;


/* ************************************************ */
/* Action/Dopesheet Editor */

/* Storage for Dopesheet/Grease-Pencil Editor data */
typedef struct bDopeSheet {
	ID 		*source;			/* currently ID_SCE (for Dopesheet), and ID_SC (for Grease Pencil) */
	ListBase chanbase;			/* cache for channels (only initialized when pinned) */  // XXX not used!
	
	struct Group *filter_grp;	/* object group for ADS_FILTER_ONLYOBGROUP filtering option */
	char searchstr[64];			/* string to search for in displayed names of F-Curves for ADS_FILTER_BY_FCU_NAME filtering option */
	
	int filterflag;				/* flags to use for filtering data */
	int flag;					/* standard flags */
	
	int renameIndex;			/* index+1 of channel to rename - only gets set by renaming operator */
	int pad;
} bDopeSheet;


/* DopeSheet filter-flag */
typedef enum eDopeSheet_FilterFlag {
		/* general filtering */
	ADS_FILTER_ONLYSEL			= (1<<0),	/* only include channels relating to selected data */
	
		/* temporary filters */
	ADS_FILTER_ONLYDRIVERS		= (1<<1),	/* for 'Drivers' editor - only include Driver data from AnimData */
	ADS_FILTER_ONLYNLA			= (1<<2),	/* for 'NLA' editor - only include NLA data from AnimData */
	ADS_FILTER_SELEDIT			= (1<<3),	/* for Graph Editor - used to indicate whether to include a filtering flag or not */
	
		/* general filtering 2 */
	ADS_FILTER_SUMMARY			= (1<<4),	/* for 'DopeSheet' Editors - include 'summary' line */
	ADS_FILTER_ONLYOBGROUP		= (1<<5),	/* only the objects in the specified object group get used */
	
		/* datatype-based filtering */
	ADS_FILTER_NOSHAPEKEYS 		= (1<<6),
	ADS_FILTER_NOMESH			= (1<<7),
	ADS_FILTER_NOOBJ			= (1<<8),	/* for animdata on object level, if we only want to concentrate on materials/etc. */
	ADS_FILTER_NOLAT			= (1<<9),
	ADS_FILTER_NOCAM			= (1<<10),
	ADS_FILTER_NOMAT			= (1<<11),
	ADS_FILTER_NOLAM			= (1<<12),
	ADS_FILTER_NOCUR			= (1<<13),
	ADS_FILTER_NOWOR			= (1<<14),
	ADS_FILTER_NOSCE			= (1<<15),
	ADS_FILTER_NOPART			= (1<<16),
	ADS_FILTER_NOMBA			= (1<<17),
	ADS_FILTER_NOARM			= (1<<18),
	ADS_FILTER_NONTREE			= (1<<19),
	ADS_FILTER_NOTEX			= (1<<20),
	ADS_FILTER_NOSPK			= (1<<21),
	
		/* NLA-specific filters */
	ADS_FILTER_NLA_NOACT		= (1<<25),	/* if the AnimData block has no NLA data, don't include to just show Action-line */
	
		/* general filtering 3 */
	ADS_FILTER_INCL_HIDDEN		= (1<<26),	/* include 'hidden' channels too (i.e. those from hidden Objects/Bones) */
	ADS_FILTER_BY_FCU_NAME		= (1<<27),	/* for F-Curves, filter by the displayed name (i.e. to isolate all Location curves only) */
	
		/* combination filters (some only used at runtime) */
	ADS_FILTER_NOOBDATA = (ADS_FILTER_NOCAM|ADS_FILTER_NOMAT|ADS_FILTER_NOLAM|ADS_FILTER_NOCUR|ADS_FILTER_NOPART|ADS_FILTER_NOARM|ADS_FILTER_NOSPK)
} eDopeSheet_FilterFlag;	

/* DopeSheet general flags */
typedef enum eDopeSheet_Flag {
	ADS_FLAG_SUMMARY_COLLAPSED	= (1<<0),	/* when summary is shown, it is collapsed, so all other channels get hidden */
	ADS_FLAG_SHOW_DBFILTERS		= (1<<1)	/* show filters for datablocks */
} eDopeSheet_Flag;



/* Action Editor Space. This is defined here instead of in DNA_space_types.h */
typedef struct SpaceAction {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;

	short blockhandler[8];

	View2D v2d  DNA_DEPRECATED; /* copied to region */
	
	bAction		*action;		/* the currently active action */
	bDopeSheet 	ads;			/* the currently active context (when not showing action) */
	
	char  mode, autosnap;		/* mode: editing context; autosnap: automatic keyframe snapping mode   */
	short flag; 				/* flag: bitmapped settings; */
	float timeslide;			/* for Time-Slide transform mode drawing - current frame? */
} SpaceAction;

/* SpaceAction flag */
typedef enum eSAction_Flag {
		/* during transform (only set for TimeSlide) */
	SACTION_MOVING	= (1<<0),	
		/* show sliders */
	SACTION_SLIDERS	= (1<<1),	
		/* draw time in seconds instead of time in frames */
	SACTION_DRAWTIME = (1<<2),
		/* don't filter action channels according to visibility */
	//SACTION_NOHIDE = (1<<3), // XXX depreceated... old animation system
		/* don't kill overlapping keyframes after transform */
	SACTION_NOTRANSKEYCULL = (1<<4),
		/* don't include keyframes that are out of view */
	//SACTION_HORIZOPTIMISEON = (1<<5), // XXX depreceated... old irrelevant trick
		/* show pose-markers (local to action) in Action Editor mode  */
	SACTION_POSEMARKERS_SHOW = (1<<6),
		/* don't draw action channels using group colors (where applicable) */
	SACTION_NODRAWGCOLORS = (1<<7), // XXX depreceated... irrelevant for current groups implementation
		/* don't draw current frame number beside frame indicator */
	SACTION_NODRAWCFRANUM = (1<<8),
		/* temporary flag to force channel selections to be synced with main */
	SACTION_TEMP_NEEDCHANSYNC = (1<<9),
		/* don't perform realtime updates */
	SACTION_NOREALTIMEUPDATES =	(1<<10),
		/* move markers as well as keyframes */
	SACTION_MARKERS_MOVE = (1<<11)
} eSAction_Flag;	

/* SpaceAction Mode Settings */
typedef enum eAnimEdit_Context {
		/* action on the active object */
	SACTCONT_ACTION	= 0,
		/* list of all shapekeys on the active object, linked with their F-Curves */
	SACTCONT_SHAPEKEY,
		/* editing of gpencil data */
	SACTCONT_GPENCIL,
		/* dopesheet (default) */
	SACTCONT_DOPESHEET
} eAnimEdit_Context;

/* SpaceAction AutoSnap Settings (also used by other Animation Editors) */
typedef enum eAnimEdit_AutoSnap {
		/* no auto-snap */
	SACTSNAP_OFF = 0,	
		/* snap to 1.0 frame/second intervals */
	SACTSNAP_STEP,
		/* snap to actual frames/seconds (nla-action time) */
	SACTSNAP_FRAME,
		/* snap to nearest marker */
	SACTSNAP_MARKER
} eAnimEdit_AutoSnap;


/* ************************************************ */
/* Legacy Data */

/* WARNING: Action Channels are now depreceated... they were part of the old animation system!
 * 		  (ONLY USED FOR DO_VERSIONS...)
 * 
 * Action Channels belong to Actions. They are linked with an IPO block, and can also own 
 * Constraint Channels in certain situations. 
 *
 * Action-Channels can only belong to one group at a time, but they still live the Action's
 * list of achans (to preserve backwards compatibility, and also minimize the code
 * that would need to be recoded). Grouped achans are stored at the start of the list, according
 * to the position of the group in the list, and their position within the group. 
 */
typedef struct bActionChannel {
	struct bActionChannel	*next, *prev;
	bActionGroup 			*grp;					/* Action Group this Action Channel belongs to */
	
	struct Ipo				*ipo;					/* IPO block this action channel references */
	ListBase				constraintChannels;		/* Constraint Channels (when Action Channel represents an Object or Bone) */
	
	int		flag;			/* settings accessed via bitmapping */
	char	name[64];		/* channel name, MAX_NAME */
	int		temp;			/* temporary setting - may be used to indicate group that channel belongs to during syncing  */
} bActionChannel;

/* Action Channel flags (ONLY USED FOR DO_VERSIONS...) */
typedef enum ACHAN_FLAG {
	ACHAN_SELECTED	= (1<<0),
	ACHAN_HILIGHTED = (1<<1),
	ACHAN_HIDDEN	= (1<<2),
	ACHAN_PROTECTED = (1<<3),
	ACHAN_EXPANDED 	= (1<<4),
	ACHAN_SHOWIPO	= (1<<5),
	ACHAN_SHOWCONS 	= (1<<6),
	ACHAN_MOVED     = (1<<31)
} ACHAN_FLAG; 

#endif

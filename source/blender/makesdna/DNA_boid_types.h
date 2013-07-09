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
 * The Original Code is Copyright (C) 2009 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_boid_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_BOID_TYPES_H__
#define __DNA_BOID_TYPES_H__

#include "DNA_listBase.h"

typedef enum BoidRuleType {
	eBoidRuleType_None = 0,
	eBoidRuleType_Goal = 1,             /* go to goal assigned object or loudest assigned signal source */
	eBoidRuleType_Avoid = 2,            /* get away from assigned object or loudest assigned signal source */
	eBoidRuleType_AvoidCollision = 3,   /* manoeuver to avoid collisions with other boids and deflector object in near future */
	eBoidRuleType_Separate = 4,         /* keep from going through other boids */
	eBoidRuleType_Flock = 5,            /* move to center of neighbors and match their velocity */
	eBoidRuleType_FollowLeader = 6,     /* follow a boid or assigned object */
	eBoidRuleType_AverageSpeed = 7,     /* maintain speed, flight level or wander*/
	eBoidRuleType_Fight = 8,            /* go to closest enemy and attack when in range */
	//eBoidRuleType_Protect = 9,        /* go to enemy closest to target and attack when in range */
	//eBoidRuleType_Hide = 10,          /* find a deflector move to it's other side from closest enemy */
	//eBoidRuleType_FollowPath = 11,    /* move along a assigned curve or closest curve in a group */
	//eBoidRuleType_FollowWall = 12,    /* move next to a deflector object's in direction of it's tangent */
	NUM_BOID_RULE_TYPES
} BoidRuleType;

/* boidrule->flag */
#define BOIDRULE_CURRENT		1
#define BOIDRULE_IN_AIR			4
#define BOIDRULE_ON_LAND		8
typedef struct BoidRule {
	struct BoidRule *next, *prev;
	int type, flag;
	char name[32];
} BoidRule;
#define BRULE_GOAL_AVOID_PREDICT	1
#define BRULE_GOAL_AVOID_ARRIVE		2
#define BRULE_GOAL_AVOID_SIGNAL		4
typedef struct BoidRuleGoalAvoid {
	BoidRule rule;
	struct Object *ob;
	int options;
	float fear_factor;

	/* signals */
	int signal_id, channels;
} BoidRuleGoalAvoid;
#define BRULE_ACOLL_WITH_BOIDS		1
#define BRULE_ACOLL_WITH_DEFLECTORS	2
typedef struct BoidRuleAvoidCollision {
	BoidRule rule;
	int options;
	float look_ahead;
} BoidRuleAvoidCollision;
#define BRULE_LEADER_IN_LINE		1
typedef struct BoidRuleFollowLeader {
	BoidRule rule;
	struct Object *ob;
	float loc[3], oloc[3];
	float cfra, distance;
	int options, queue_size;
} BoidRuleFollowLeader;
typedef struct BoidRuleAverageSpeed {
	BoidRule rule;
	float wander, level, speed, rt;
} BoidRuleAverageSpeed;
typedef struct BoidRuleFight {
	BoidRule rule;
	float distance, flee_distance;
} BoidRuleFight;

typedef enum BoidMode {
	eBoidMode_InAir = 0,
	eBoidMode_OnLand = 1,
	eBoidMode_Climbing = 2,
	eBoidMode_Falling = 3,
	eBoidMode_Liftoff = 4,
	NUM_BOID_MODES
} BoidMode;


typedef struct BoidData {
	float health, acc[3];
	short state_id, mode;
} BoidData;

// planned for near future
//typedef enum BoidConditionMode {
//	eBoidConditionType_Then = 0,
//	eBoidConditionType_And = 1,
//	eBoidConditionType_Or = 2,
//	NUM_BOID_CONDITION_MODES
//} BoidConditionMode;
//typedef enum BoidConditionType {
//	eBoidConditionType_None = 0,
//	eBoidConditionType_Signal = 1,
//	eBoidConditionType_NoSignal = 2,
//	eBoidConditionType_HealthBelow = 3,
//	eBoidConditionType_HealthAbove = 4,
//	eBoidConditionType_See = 5,
//	eBoidConditionType_NotSee = 6,
//	eBoidConditionType_StateTime = 7,
//	eBoidConditionType_Touching = 8,
//	NUM_BOID_CONDITION_TYPES
//} BoidConditionType;
//typedef struct BoidCondition {
//	struct BoidCondition *next, *prev;
//	int state_id;
//	short type, mode;
//	float threshold, probability;
//
//	/* signals */
//	int signal_id, channels;
//} BoidCondition;

typedef enum BoidRulesetType {
	eBoidRulesetType_Fuzzy = 0,
	eBoidRulesetType_Random = 1,
	eBoidRulesetType_Average = 2,
	NUM_BOID_RULESET_TYPES
} BoidRulesetType;
#define BOIDSTATE_CURRENT	1
typedef struct BoidState {
	struct BoidState *next, *prev;
	ListBase rules;
	ListBase conditions;
	ListBase actions;
	char name[32];
	int id, flag;
	
	/* rules */
	int ruleset_type;
	float rule_fuzziness;

	/* signal */
	int signal_id, channels;
	float volume, falloff;
} BoidState;

// planned for near future
//typedef struct BoidSignal {
//	struct BoidSignal *next, *prev;
//	float loc[3];
//	float volume, falloff;
//	int id;
//} BoidSignal;
//typedef struct BoidSignalDefine {
//	struct BoidSignalDefine *next, *prev;
//	int id, rt;
//	char name[32];
//} BoidSignalDefine;

//typedef struct BoidSimulationData {
//	ListBase signal_defines;/* list of defined signals */
//	ListBase signals[20];	/* gathers signals from all channels */
//	struct KDTree *signaltrees[20];
//	char channel_names[20][32];
//	int last_signal_id;		/* used for incrementing signal ids */
//	int flag;				/* switches for drawing stuff */
//} BoidSimulationData;

typedef struct BoidSettings {
	int options, last_state_id;

	float landing_smoothness, height;
	float banking, pitch;

	float health, aggression;
	float strength, accuracy, range;

	/* flying related */
	float air_min_speed, air_max_speed;
	float air_max_acc, air_max_ave;
	float air_personal_space;

	/* walk/run related */
	float land_jump_speed, land_max_speed;
	float land_max_acc, land_max_ave;
	float land_personal_space;
	float land_stick_force;

	struct ListBase states;
} BoidSettings;

/* boidsettings->options */
#define BOID_ALLOW_FLIGHT	1
#define BOID_ALLOW_LAND		2
#define BOID_ALLOW_CLIMB	4

/* boidrule->options */
//#define BOID_RULE_FOLLOW_LINE	1		/* follow leader */
//#define BOID_RULE_PREDICT		2		/* goal/avoid */
//#define BOID_RULE_ARRIVAL		4		/* goal */
//#define BOID_RULE_LAND			8		/* goal */
//#define BOID_RULE_WITH_BOIDS	16		/* avoid collision */
//#define BOID_RULE_WITH_DEFLECTORS	32	/* avoid collision */

#endif

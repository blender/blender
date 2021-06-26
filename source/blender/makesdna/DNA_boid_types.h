/*
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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eBoidRuleType {
  eBoidRuleType_None = 0,
  /** go to goal assigned object or loudest assigned signal source */
  eBoidRuleType_Goal = 1,
  /** get away from assigned object or loudest assigned signal source */
  eBoidRuleType_Avoid = 2,
  /** Maneuver to avoid collisions with other boids and deflector object in near future. */
  eBoidRuleType_AvoidCollision = 3,
  /** keep from going through other boids */
  eBoidRuleType_Separate = 4,
  /** move to center of neighbors and match their velocity */
  eBoidRuleType_Flock = 5,
  /** follow a boid or assigned object */
  eBoidRuleType_FollowLeader = 6,
  /** Maintain speed, flight level or wander. */
  eBoidRuleType_AverageSpeed = 7,
  /** go to closest enemy and attack when in range */
  eBoidRuleType_Fight = 8,
#if 0
  /** go to enemy closest to target and attack when in range */
  eBoidRuleType_Protect = 9,
  /** find a deflector move to its other side from closest enemy */
  eBoidRuleType_Hide = 10,
  /** move along a assigned curve or closest curve in a group */
  eBoidRuleType_FollowPath = 11,
  /** move next to a deflector object's in direction of its tangent */
  eBoidRuleType_FollowWall = 12,
#endif
} eBoidRuleType;

/* boidrule->flag */
#define BOIDRULE_CURRENT (1 << 0)
#define BOIDRULE_IN_AIR (1 << 2)
#define BOIDRULE_ON_LAND (1 << 3)
typedef struct BoidRule {
  struct BoidRule *next, *prev;
  int type, flag;
  char name[32];
} BoidRule;
#define BRULE_GOAL_AVOID_PREDICT (1 << 0)
#define BRULE_GOAL_AVOID_ARRIVE (1 << 1)
#define BRULE_GOAL_AVOID_SIGNAL (1 << 2)
typedef struct BoidRuleGoalAvoid {
  BoidRule rule;
  struct Object *ob;
  int options;
  float fear_factor;

  /* signals */
  int signal_id, channels;
} BoidRuleGoalAvoid;
#define BRULE_ACOLL_WITH_BOIDS (1 << 0)
#define BRULE_ACOLL_WITH_DEFLECTORS (1 << 1)
typedef struct BoidRuleAvoidCollision {
  BoidRule rule;
  int options;
  float look_ahead;
} BoidRuleAvoidCollision;
#define BRULE_LEADER_IN_LINE (1 << 0)
typedef struct BoidRuleFollowLeader {
  BoidRule rule;
  struct Object *ob;
  float loc[3], oloc[3];
  float cfra, distance;
  int options, queue_size;
} BoidRuleFollowLeader;
typedef struct BoidRuleAverageSpeed {
  BoidRule rule;
  float wander, level, speed;
  char _pad0[4];
} BoidRuleAverageSpeed;
typedef struct BoidRuleFight {
  BoidRule rule;
  float distance, flee_distance;
} BoidRuleFight;

typedef enum eBoidMode {
  eBoidMode_InAir = 0,
  eBoidMode_OnLand = 1,
  eBoidMode_Climbing = 2,
  eBoidMode_Falling = 3,
  eBoidMode_Liftoff = 4,
} eBoidMode;

typedef struct BoidData {
  float health, acc[3];
  short state_id, mode;
} BoidData;

// planned for near future
// typedef enum BoidConditionMode {
//  eBoidConditionType_Then = 0,
//  eBoidConditionType_And = 1,
//  eBoidConditionType_Or = 2,
//  NUM_BOID_CONDITION_MODES
//} BoidConditionMode;
// typedef enum BoidConditionType {
//  eBoidConditionType_None = 0,
//  eBoidConditionType_Signal = 1,
//  eBoidConditionType_NoSignal = 2,
//  eBoidConditionType_HealthBelow = 3,
//  eBoidConditionType_HealthAbove = 4,
//  eBoidConditionType_See = 5,
//  eBoidConditionType_NotSee = 6,
//  eBoidConditionType_StateTime = 7,
//  eBoidConditionType_Touching = 8,
//  NUM_BOID_CONDITION_TYPES
//} BoidConditionType;
// typedef struct BoidCondition {
//  struct BoidCondition *next, *prev;
//  int state_id;
//  short type, mode;
//  float threshold, probability;
//
//  /* signals */
//  int signal_id, channels;
//} BoidCondition;

typedef enum eBoidRulesetType {
  eBoidRulesetType_Fuzzy = 0,
  eBoidRulesetType_Random = 1,
  eBoidRulesetType_Average = 2,
} eBoidRulesetType;
#define BOIDSTATE_CURRENT 1
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
// typedef struct BoidSignal {
//  struct BoidSignal *next, *prev;
//  float loc[3];
//  float volume, falloff;
//  int id;
//} BoidSignal;
// typedef struct BoidSignalDefine {
//  struct BoidSignalDefine *next, *prev;
//  int id, _pad[4];
//  char name[32];
//} BoidSignalDefine;

// typedef struct BoidSimulationData {
//  ListBase signal_defines;/* list of defined signals */
//  ListBase signals[20];   /* gathers signals from all channels */
//  struct KDTree_3d *signaltrees[20];
//  char channel_names[20][32];
//  int last_signal_id;     /* used for incrementing signal ids */
//  int flag;               /* switches for drawing stuff */
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
#define BOID_ALLOW_FLIGHT (1 << 0)
#define BOID_ALLOW_LAND (1 << 1)
#define BOID_ALLOW_CLIMB (1 << 2)

/* boidrule->options */
//#define BOID_RULE_FOLLOW_LINE     (1 << 0)        /* follow leader */
//#define BOID_RULE_PREDICT         (1 << 1)        /* goal/avoid */
//#define BOID_RULE_ARRIVAL         (1 << 2)        /* goal */
//#define BOID_RULE_LAND            (1 << 3)        /* goal */
//#define BOID_RULE_WITH_BOIDS      (1 << 4)        /* avoid collision */
//#define BOID_RULE_WITH_DEFLECTORS (1 << 5)    /* avoid collision */

#ifdef __cplusplus
}
#endif

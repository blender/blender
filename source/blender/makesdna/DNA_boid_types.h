/* SPDX-FileCopyrightText: 2009 by Janne Karhu. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_listBase.h"

enum eBoidRuleType {
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
};

/* boidrule->flag */
enum {
  BOIDRULE_CURRENT = 1 << 0,
  BOIDRULE_IN_AIR = 1 << 2,
  BOIDRULE_ON_LAND = 1 << 3,
};

#define BRULE_LEADER_IN_LINE (1 << 0)

#define BOIDSTATE_CURRENT 1

enum {
  BRULE_GOAL_AVOID_PREDICT = 1 << 0,
  BRULE_GOAL_AVOID_ARRIVE = 1 << 1,
  BRULE_GOAL_AVOID_SIGNAL = 1 << 2,
};

enum {
  BRULE_ACOLL_WITH_BOIDS = 1 << 0,
  BRULE_ACOLL_WITH_DEFLECTORS = 1 << 1,
};

enum eBoidMode {
  eBoidMode_InAir = 0,
  eBoidMode_OnLand = 1,
  eBoidMode_Climbing = 2,
  eBoidMode_Falling = 3,
  eBoidMode_Liftoff = 4,
};

enum eBoidRulesetType {
  eBoidRulesetType_Fuzzy = 0,
  eBoidRulesetType_Random = 1,
  eBoidRulesetType_Average = 2,
};

/** #BoidSettings::options */
enum {
  BOID_ALLOW_FLIGHT = 1 << 0,
  BOID_ALLOW_LAND = 1 << 1,
  BOID_ALLOW_CLIMB = 1 << 2,
};

typedef struct BoidRule {
  struct BoidRule *next, *prev;
  int type, flag;
  char name[32];
} BoidRule;

typedef struct BoidRuleGoalAvoid {
  BoidRule rule;
  struct Object *ob;
  int options;
  float fear_factor;

  /* signals */
  int signal_id, channels;
} BoidRuleGoalAvoid;

typedef struct BoidRuleAvoidCollision {
  BoidRule rule;
  int options;
  float look_ahead;
} BoidRuleAvoidCollision;

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

typedef struct BoidData {
  float health, acc[3];
  short state_id, mode;
} BoidData;

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

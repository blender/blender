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

struct BoidRule {
  struct BoidRule *next = nullptr, *prev = nullptr;
  int type = 0, flag = 0;
  char name[32] = "";
};

struct BoidRuleGoalAvoid {
  BoidRule rule;
  struct Object *ob = nullptr;
  int options = 0;
  float fear_factor = 0;

  /* signals */
  int signal_id = 0, channels = 0;
};

struct BoidRuleAvoidCollision {
  BoidRule rule;
  int options = 0;
  float look_ahead = 0;
};

struct BoidRuleFollowLeader {
  BoidRule rule;
  struct Object *ob = nullptr;
  float loc[3] = {}, oloc[3] = {};
  float cfra = 0, distance = 0;
  int options = 0, queue_size = 0;
};

struct BoidRuleAverageSpeed {
  BoidRule rule;
  float wander = 0, level = 0, speed = 0;
  char _pad0[4] = {};
};

struct BoidRuleFight {
  BoidRule rule;
  float distance = 0, flee_distance = 0;
};

struct BoidData {
  float health = 0, acc[3] = {};
  short state_id = 0, mode = 0;
};

struct BoidState {
  struct BoidState *next = nullptr, *prev = nullptr;
  ListBaseT<BoidRule> rules = {nullptr, nullptr};
  ListBase conditions = {nullptr, nullptr};
  ListBase actions = {nullptr, nullptr};
  char name[32] = "";
  int id = 0, flag = 0;

  /* rules */
  int ruleset_type = 0;
  float rule_fuzziness = 0;

  /* signal */
  int signal_id = 0, channels = 0;
  float volume = 0, falloff = 0;
};

struct BoidSettings {
  int options = 0, last_state_id = 0;

  float landing_smoothness = 0, height = 0;
  float banking = 0, pitch = 0;

  float health = 0, aggression = 0;
  float strength = 0, accuracy = 0, range = 0;

  /* flying related */
  float air_min_speed = 0, air_max_speed = 0;
  float air_max_acc = 0, air_max_ave = 0;
  float air_personal_space = 0;

  /* walk/run related */
  float land_jump_speed = 0, land_max_speed = 0;
  float land_max_acc = 0, land_max_ave = 0;
  float land_personal_space = 0;
  float land_stick_force = 0;

  ListBaseT<BoidState> states = {nullptr, nullptr};
};

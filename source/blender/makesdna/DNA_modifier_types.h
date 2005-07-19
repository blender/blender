/**
 * $Id$ 
 */

#ifndef DNA_MODIFIER_TYPES_H
#define DNA_MODIFIER_TYPES_H

typedef enum ModifierType {
	eModifierType_None = 0,
	eModifierType_Subsurf,
	eModifierType_Lattice,
	eModifierType_Curve,

	NUM_MODIFIER_TYPES
} ModifierType;

	/* These numerical values are explicitly chosen so that 
	 * (mode&1) is true for realtime calc and (mode&2) is true
	 * for render calc.
	 */
typedef enum ModifierMode {
	eModifierMode_Disabled = 0,
	eModifierMode_OnlyRealtime = 1,
	eModifierMode_OnlyRender = 2,
	eModifierMode_RealtimeAndRender = 3,
} ModifierMode;

typedef struct ModifierData {
	struct ModifierData *next, *prev;

	int type, mode;
} ModifierData;

typedef struct SubsurfModifierData {
	ModifierData modifier;

	short subdivType, levels, renderLevels, pad;
} SubsurfModifierData;

typedef struct LatticeModifierData {
	ModifierData modifier;

	struct Object *object;
} LatticeModifierData;

typedef struct CurveModifierData {
	ModifierData modifier;

	struct Object *object;
} CurveModifierData;

#endif

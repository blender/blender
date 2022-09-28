/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct ID;
struct ListBase;
struct Object;
struct ParticleSystem;
struct Scene;
struct ViewerPath;
struct GeometrySet;

/* ---------------------------------------------------- */
/* Dupli-Geometry */

/**
 * \return a #ListBase of #DupliObject.
 */
struct ListBase *object_duplilist(struct Depsgraph *depsgraph,
                                  struct Scene *sce,
                                  struct Object *ob);
/**
 * \return a #ListBase of #DupliObject for the preview geometry referenced by the #ViewerPath.
 */
struct ListBase *object_duplilist_preview(struct Depsgraph *depsgraph,
                                          struct Scene *scene,
                                          struct Object *ob,
                                          const struct ViewerPath *viewer_path);
void free_object_duplilist(struct ListBase *lb);

typedef struct DupliObject {
  struct DupliObject *next, *prev;
  /* Object whose geometry is instanced. */
  struct Object *ob;
  /* Data owned by the object above that is instanced. This might not be the same as `ob->data`. */
  struct ID *ob_data;
  float mat[4][4];
  float orco[3], uv[2];

  short type; /* from Object.transflag */
  char no_draw;
  /* If this dupli object is belongs to a preview, this is non-null. */
  const struct GeometrySet *preview_base_geometry;
  /* Index of the top-level instance this dupli is part of or -1 when unused. */
  int preview_instance_index;

  /* Persistent identifier for a dupli object, for inter-frame matching of
   * objects with motion blur, or inter-update matching for syncing. */
  int persistent_id[8]; /* MAX_DUPLI_RECUR */

  /* Particle this dupli was generated from. */
  struct ParticleSystem *particle_system;

  /* Random ID for shading */
  unsigned int random_id;
} DupliObject;

#ifdef __cplusplus
}
#endif

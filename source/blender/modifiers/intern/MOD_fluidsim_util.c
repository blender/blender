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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <stddef.h>
#include <zlib.h>

#include "BLI_utildefines.h"

#ifdef WITH_MOD_FLUID
#  include "BLI_blenlib.h"
#  include "BLI_math.h"
#endif

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_fluidsim_types.h"

#include "BKE_fluidsim.h" /* ensure definitions here match */
#include "BKE_mesh.h"
#ifdef WITH_MOD_FLUID
#  include "BKE_global.h"
#  include "BKE_library.h"
#endif

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_fluidsim_util.h"
#include "MOD_modifiertypes.h"

#include "MEM_guardedalloc.h"

// headers for fluidsim bobj meshes
#include "LBM_fluidsim.h"

void fluidsim_init(FluidsimModifierData *fluidmd)
{
#ifdef WITH_MOD_FLUID
  if (fluidmd) {
    FluidsimSettings *fss = MEM_callocN(sizeof(FluidsimSettings), "fluidsimsettings");

    fluidmd->fss = fss;

    if (!fss)
      return;

    fss->fmd = fluidmd;
    fss->type = OB_FLUIDSIM_ENABLE;
    fss->threads = 0;
    fss->show_advancedoptions = 0;

    fss->resolutionxyz = 65;
    fss->previewresxyz = 45;
    fss->realsize = 0.5;
    fss->guiDisplayMode = OB_FSDOM_PREVIEW;
    fss->renderDisplayMode = OB_FSDOM_FINAL;

    fss->viscosityValue = 1.0;
    fss->viscosityExponent = 6;

    fss->grav[0] = 0.0;
    fss->grav[1] = 0.0;
    fss->grav[2] = -9.81;

    fss->animStart = 0.0;
    fss->animEnd = 4.0;
    fss->animRate = 1.0;
    fss->gstar = 0.005;  // used as normgstar
    fss->maxRefine = -1;
    /* maxRefine is set according to resolutionxyz during bake */

    /* fluid/inflow settings
     * fss->iniVel --> automatically set to 0 */

    modifier_path_init(fss->surfdataPath, sizeof(fss->surfdataPath), OB_FLUIDSIM_SURF_DIR_DEFAULT);

    /* first init of bounding box */
    /* no bounding box needed */

    /* todo - reuse default init from elbeem! */
    fss->typeFlags = OB_FSBND_PARTSLIP | OB_FSSG_NOOBS;
    fss->domainNovecgen = 0;
    fss->volumeInitType = 1; /* volume */
    fss->partSlipValue = 0.2;

    fss->generateTracers = 0;
    fss->generateParticles = 0.0;
    fss->surfaceSmoothing = 1.0;
    fss->surfaceSubdivs = 0.0;
    fss->particleInfSize = 0.0;
    fss->particleInfAlpha = 0.0;

    /* init fluid control settings */
    fss->attractforceStrength = 0.2;
    fss->attractforceRadius = 0.75;
    fss->velocityforceStrength = 0.2;
    fss->velocityforceRadius = 0.75;
    fss->cpsTimeStart = fss->animStart;
    fss->cpsTimeEnd = fss->animEnd;
    fss->cpsQuality = 10.0;  // 1.0 / 10.0 => means 0.1 width

    /*
     * BAD TODO: this is done in buttons_object.c in the moment
     * Mesh *mesh = ob->data;
     * // calculate bounding box
     * fluid_get_bb(mesh->mvert, mesh->totvert, ob->obmat, fss->bbStart, fss->bbSize);
     */

    fss->meshVelocities = NULL;

    fss->lastgoodframe = -1;

    fss->flag |= OB_FLUIDSIM_ACTIVE;
  }
#else
  (void)fluidmd; /* unused */
#endif
  return;
}

void fluidsim_free(FluidsimModifierData *fluidmd)
{
  if (fluidmd && fluidmd->fss) {
    if (fluidmd->fss->meshVelocities) {
      MEM_freeN(fluidmd->fss->meshVelocities);
    }
    MEM_SAFE_FREE(fluidmd->fss);
  }

  return;
}

#ifdef WITH_MOD_FLUID
/* read .bobj.gz file into a fluidsimMesh struct */
static Mesh *fluidsim_read_obj(const char *filename, const MPoly *mp_example)
{
  int wri = 0, i;
  int gotBytes;
  gzFile gzf;
  int numverts = 0, numfaces = 0;
  Mesh *mesh = NULL;
  MPoly *mp;
  MLoop *ml;
  MVert *mv;
  short *normals, *no_s;
  float no[3];

  const short mp_mat_nr = mp_example->mat_nr;
  const char mp_flag = mp_example->flag;

  /* ------------------------------------------------
   * get numverts + numfaces first
   * ------------------------------------------------ */
  gzf = BLI_gzopen(filename, "rb");
  if (!gzf) {
    return NULL;
  }

  /* read numverts */
  gotBytes = gzread(gzf, &wri, sizeof(wri));
  numverts = wri;

  /* skip verts */
  gotBytes = gzseek(gzf, numverts * 3 * sizeof(float), SEEK_CUR) != -1;

  /* read number of normals */
  if (gotBytes)
    gotBytes = gzread(gzf, &wri, sizeof(wri));

  /* skip normals */
  gotBytes = gzseek(gzf, numverts * 3 * sizeof(float), SEEK_CUR) != -1;

  /* get no. of triangles */
  if (gotBytes)
    gotBytes = gzread(gzf, &wri, sizeof(wri));
  numfaces = wri;

  gzclose(gzf);
  /* ------------------------------------------------ */

  if (!numfaces || !numverts || !gotBytes)
    return NULL;

  gzf = BLI_gzopen(filename, "rb");
  if (!gzf) {
    return NULL;
  }

  mesh = BKE_mesh_new_nomain(numverts, 0, 0, numfaces * 3, numfaces);

  if (!mesh) {
    gzclose(gzf);
    return NULL;
  }

  /* read numverts */
  gotBytes = gzread(gzf, &wri, sizeof(wri));

  /* read vertex position from file */
  mv = mesh->mvert;

  for (i = 0; i < numverts; i++, mv++)
    gotBytes = gzread(gzf, mv->co, sizeof(float) * 3);

  /* should be the same as numverts */
  gotBytes = gzread(gzf, &wri, sizeof(wri));
  if (wri != numverts) {
    if (mesh)
      BKE_id_free(NULL, mesh);
    gzclose(gzf);
    return NULL;
  }

  normals = MEM_calloc_arrayN(numverts, 3 * sizeof(short), "fluid_tmp_normals");
  if (!normals) {
    if (mesh)
      BKE_id_free(NULL, mesh);
    gzclose(gzf);
    return NULL;
  }

  /* read normals from file (but don't save them yet) */
  for (i = numverts, no_s = normals; i > 0; i--, no_s += 3) {
    gotBytes = gzread(gzf, no, sizeof(float) * 3);
    normal_float_to_short_v3(no_s, no);
  }

  /* read no. of triangles */
  gotBytes = gzread(gzf, &wri, sizeof(wri));

  if (wri != numfaces) {
    printf("Fluidsim: error in reading data from file.\n");
    if (mesh)
      BKE_id_free(NULL, mesh);
    gzclose(gzf);
    MEM_freeN(normals);
    return NULL;
  }

  /* read triangles from file */
  mp = mesh->mpoly;
  ml = mesh->mloop;
  for (i = 0; i < numfaces; i++, mp++, ml += 3) {
    int face[3];

    gotBytes = gzread(gzf, face, sizeof(int) * 3);

    /* initialize from existing face */
    mp->mat_nr = mp_mat_nr;
    mp->flag = mp_flag;

    mp->loopstart = i * 3;
    mp->totloop = 3;

    ml[0].v = face[0];
    ml[1].v = face[1];
    ml[2].v = face[2];
  }

  gzclose(gzf);

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_apply_vert_normals(mesh, (short(*)[3])normals);
  MEM_freeN(normals);

  // CDDM_calc_normals(result);
  return mesh;
}

void fluid_get_bb(MVert *mvert,
                  int totvert,
                  float obmat[4][4],
                  /*RET*/ float start[3],
                  /*RET*/ float size[3])
{
  float bbsx = 0.0, bbsy = 0.0, bbsz = 0.0;
  float bbex = 1.0, bbey = 1.0, bbez = 1.0;
  int i;
  float vec[3];

  if (totvert == 0) {
    zero_v3(start);
    zero_v3(size);
    return;
  }

  copy_v3_v3(vec, mvert[0].co);
  mul_m4_v3(obmat, vec);
  bbsx = vec[0];
  bbsy = vec[1];
  bbsz = vec[2];
  bbex = vec[0];
  bbey = vec[1];
  bbez = vec[2];

  for (i = 1; i < totvert; i++) {
    copy_v3_v3(vec, mvert[i].co);
    mul_m4_v3(obmat, vec);

    if (vec[0] < bbsx) {
      bbsx = vec[0];
    }
    if (vec[1] < bbsy) {
      bbsy = vec[1];
    }
    if (vec[2] < bbsz) {
      bbsz = vec[2];
    }
    if (vec[0] > bbex) {
      bbex = vec[0];
    }
    if (vec[1] > bbey) {
      bbey = vec[1];
    }
    if (vec[2] > bbez) {
      bbez = vec[2];
    }
  }

  /* return values... */
  if (start) {
    start[0] = bbsx;
    start[1] = bbsy;
    start[2] = bbsz;
  }
  if (size) {
    size[0] = bbex - bbsx;
    size[1] = bbey - bbsy;
    size[2] = bbez - bbsz;
  }
}

//-------------------------------------------------------------------------------
// old interface
//-------------------------------------------------------------------------------

void fluid_estimate_memory(Object *ob, FluidsimSettings *fss, char *value)
{
  Mesh *mesh;

  value[0] = '\0';

  if (ob->type == OB_MESH) {
    /* use mesh bounding box and object scaling */
    mesh = ob->data;

    fluid_get_bb(mesh->mvert, mesh->totvert, ob->obmat, fss->bbStart, fss->bbSize);
    elbeemEstimateMemreq(
        fss->resolutionxyz, fss->bbSize[0], fss->bbSize[1], fss->bbSize[2], fss->maxRefine, value);
  }
}

/* read zipped fluidsim velocities into the co's of the fluidsimsettings normals struct */
static void fluidsim_read_vel_cache(FluidsimModifierData *fluidmd, Mesh *mesh, char *filename)
{
  int wri, i, j;
  float wrf;
  gzFile gzf;
  FluidsimSettings *fss = fluidmd->fss;
  int len = strlen(filename);
  int totvert = mesh->totvert;
  FluidVertexVelocity *velarray = NULL;

  /* mesh and vverts have to be valid from loading... */

  if (fss->meshVelocities)
    MEM_freeN(fss->meshVelocities);

  if (len < 7) {
    return;
  }

  if (fss->domainNovecgen > 0)
    return;

  fss->meshVelocities = MEM_calloc_arrayN(
      mesh->totvert, sizeof(FluidVertexVelocity), "Fluidsim_velocities");
  fss->totvert = totvert;

  velarray = fss->meshVelocities;

  /* .bobj.gz, correct filename
   * 87654321 */
  filename[len - 6] = 'v';
  filename[len - 5] = 'e';
  filename[len - 4] = 'l';

  gzf = BLI_gzopen(filename, "rb");
  if (!gzf) {
    MEM_freeN(fss->meshVelocities);
    fss->meshVelocities = NULL;
    return;
  }

  gzread(gzf, &wri, sizeof(wri));
  if (wri != totvert) {
    MEM_freeN(fss->meshVelocities);
    fss->meshVelocities = NULL;
    return;
  }

  for (i = 0; i < totvert; i++) {
    for (j = 0; j < 3; j++) {
      gzread(gzf, &wrf, sizeof(wrf));
      velarray[i].vel[j] = wrf;
    }
  }

  gzclose(gzf);
}

static Mesh *fluidsim_read_cache(
    Object *ob, Mesh *orgmesh, FluidsimModifierData *fluidmd, int framenr, int useRenderParams)
{
  int curFrame = framenr /* - 1 */ /*scene->r.sfra*/; /* start with 0 at start frame */
  /* why start with 0 as start frame?? Animations + time are frozen for frame 0 anyway.
   * (See physics_fluid.c for that. - DG) */
  /* If we start with frame 0, we need to remap all animation channels, too,
   * because they will all be 1 frame late if using frame-1! - DG */

  char targetFile[FILE_MAX];
  FluidsimSettings *fss = fluidmd->fss;
  Mesh *newmesh = NULL;
  MPoly *mpoly;
  MPoly mp_example = {0};

  const int displaymode = useRenderParams ? fss->renderDisplayMode : fss->guiDisplayMode;

  switch (displaymode) {
    case OB_FSDOM_GEOM:
      /* just display original object */
      return NULL;
    case OB_FSDOM_PREVIEW:
      /* use preview mesh */
      BLI_join_dirfile(
          targetFile, sizeof(targetFile), fss->surfdataPath, OB_FLUIDSIM_SURF_PREVIEW_OBJ_FNAME);
      break;
    case OB_FSDOM_FINAL:
      /* use final mesh */
      BLI_join_dirfile(
          targetFile, sizeof(targetFile), fss->surfdataPath, OB_FLUIDSIM_SURF_FINAL_OBJ_FNAME);
      break;
    default:
      BLI_assert(!"Wrong fluidsim display type");
      return NULL;
  }

  /* offset baked frame */
  curFrame += fss->frameOffset;

  BLI_path_abs(targetFile, modifier_path_relbase_from_global(ob));
  BLI_path_frame(targetFile, curFrame, 0);  // fixed #frame-no

  /* assign material + flags to new mesh.
   * if there's no faces in original mesh, keep materials and flags unchanged */
  mpoly = orgmesh->mpoly;
  if (mpoly) {
    mp_example = *mpoly;
  }
  /* else leave NULL'd */

  newmesh = fluidsim_read_obj(targetFile, &mp_example);

  if (!newmesh) {
    /* switch, abort background rendering when fluidsim mesh is missing */
    const char *strEnvName2 = "BLENDER_ELBEEMBOBJABORT";  // from blendercall.cpp

    if (G.background == 1) {
      if (BLI_getenv(strEnvName2)) {
        int elevel = atoi(BLI_getenv(strEnvName2));
        if (elevel > 0) {
          printf("Env. var %s set, fluid sim mesh '%s' not found, aborting render...\n",
                 strEnvName2,
                 targetFile);
          exit(1);
        }
      }
    }

    /* display org. object upon failure which is in new mesh */
    return NULL;
  }

  /* load vertex velocities, if they exist...
   * TODO? use generate flag as loading flag as well?
   * warning, needs original .bobj.gz mesh loading filename */
  if (displaymode == OB_FSDOM_FINAL) {
    fluidsim_read_vel_cache(fluidmd, newmesh, targetFile);
  }
  else {
    if (fss->meshVelocities)
      MEM_freeN(fss->meshVelocities);

    fss->meshVelocities = NULL;
  }

  return newmesh;
}
#endif  // WITH_MOD_FLUID

Mesh *fluidsimModifier_do(FluidsimModifierData *fluidmd,
                          const ModifierEvalContext *ctx,
                          Mesh *mesh)
{
#ifdef WITH_MOD_FLUID
  Object *ob = ctx->object;
  Depsgraph *depsgraph = ctx->depsgraph;
  const bool useRenderParams = (ctx->flag & MOD_APPLY_RENDER) != 0;
  //  const bool isFinalCalc = (ctx->flag & MOD_APPLY_USECACHE) != 0;
  Mesh *result = NULL;
  int framenr;
  FluidsimSettings *fss = NULL;

  framenr = (int)DEG_get_ctime(depsgraph);

  /* only handle fluidsim domains */
  if (fluidmd && fluidmd->fss && (fluidmd->fss->type != OB_FLUIDSIM_DOMAIN))
    return mesh;

  /* sanity check */
  if (!fluidmd || !fluidmd->fss)
    return mesh;

  fss = fluidmd->fss;

  /* timescale not supported yet
   * clmd->sim_parms->timescale = timescale; */

  /* support reversing of baked fluid frames here */
  if ((fss->flag & OB_FLUIDSIM_REVERSE) && (fss->lastgoodframe >= 0)) {
    framenr = fss->lastgoodframe - framenr + 1;
    CLAMP(framenr, 1, fss->lastgoodframe);
  }

  /* try to read from cache */
  /* if the frame is there, fine, otherwise don't do anything */
  if ((result = fluidsim_read_cache(ob, mesh, fluidmd, framenr, useRenderParams)))
    return result;

  return mesh;
#else
  /* unused */
  UNUSED_VARS(fluidmd, ctx, mesh);
  return NULL;
#endif
}

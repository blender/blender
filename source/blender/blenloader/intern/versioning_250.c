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
 */

/** \file
 * \ingroup blenloader
 */

#ifndef WIN32
#  include <unistd.h> /* for read close */
#else
#  include <zlib.h> /* odd include order-issue */
#  include <io.h>   // for open close read
#  include "winsock2.h"
#  include "BLI_winstuff.h"
#endif

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_smoke_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_colortools.h"
#include "BKE_global.h"  // for G
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"  // for ME_ defines (patching)
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_texture.h"
#include "BKE_sound.h"

#include "NOD_socket.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include <errno.h>

/* 2.50 patch */
static void area_add_header_region(ScrArea *sa, ListBase *lb)
{
  ARegion *ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");

  BLI_addtail(lb, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  if (sa->headertype == 1) {
    ar->alignment = RGN_ALIGN_BOTTOM;
  }
  else {
    ar->alignment = RGN_ALIGN_TOP;
  }

  /* initialize view2d data for header region, to allow panning */
  /* is copy from ui_view2d.c */
  ar->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
  ar->v2d.keepofs = V2D_LOCKOFS_Y;
  ar->v2d.keeptot = V2D_KEEPTOT_STRICT;
  ar->v2d.align = V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y;
  ar->v2d.flag = (V2D_PIXELOFS_X | V2D_PIXELOFS_Y);
}

static void sequencer_init_preview_region(ARegion *ar)
{
  // XXX a bit ugly still, copied from space_sequencer
  /* NOTE: if you change values here, also change them in space_sequencer.c, sequencer_new */
  ar->regiontype = RGN_TYPE_PREVIEW;
  ar->alignment = RGN_ALIGN_TOP;
  ar->flag |= RGN_FLAG_HIDDEN;
  ar->v2d.keepzoom = V2D_KEEPASPECT | V2D_KEEPZOOM;
  ar->v2d.minzoom = 0.00001f;
  ar->v2d.maxzoom = 100000.0f;
  ar->v2d.tot.xmin = -960.0f; /* 1920 width centered */
  ar->v2d.tot.ymin = -540.0f; /* 1080 height centered */
  ar->v2d.tot.xmax = 960.0f;
  ar->v2d.tot.ymax = 540.0f;
  ar->v2d.min[0] = 0.0f;
  ar->v2d.min[1] = 0.0f;
  ar->v2d.max[0] = 12000.0f;
  ar->v2d.max[1] = 12000.0f;
  ar->v2d.cur = ar->v2d.tot;
  ar->v2d.align = V2D_ALIGN_FREE;  // (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y);
  ar->v2d.keeptot = V2D_KEEPTOT_FREE;
}

static void area_add_window_regions(ScrArea *sa, SpaceLink *sl, ListBase *lb)
{
  ARegion *ar;
  ARegion *ar_main;

  if (sl) {
    /* first channels for ipo action nla... */
    switch (sl->spacetype) {
      case SPACE_GRAPH:
        ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_CHANNELS;
        ar->alignment = RGN_ALIGN_LEFT;
        ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);

        /* for some reason, this doesn't seem to go auto like for NLA... */
        ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_UI;
        ar->alignment = RGN_ALIGN_RIGHT;
        ar->v2d.scroll = V2D_SCROLL_RIGHT;
        ar->v2d.flag = RGN_FLAG_HIDDEN;
        break;

      case SPACE_ACTION:
        ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_CHANNELS;
        ar->alignment = RGN_ALIGN_LEFT;
        ar->v2d.scroll = V2D_SCROLL_BOTTOM;
        ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
        break;

      case SPACE_NLA:
        ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_CHANNELS;
        ar->alignment = RGN_ALIGN_LEFT;
        ar->v2d.scroll = V2D_SCROLL_BOTTOM;
        ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

        /* for some reason, some files still don't get this auto */
        ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_UI;
        ar->alignment = RGN_ALIGN_RIGHT;
        ar->v2d.scroll = V2D_SCROLL_RIGHT;
        ar->v2d.flag = RGN_FLAG_HIDDEN;
        break;

      case SPACE_NODE:
        ar = MEM_callocN(sizeof(ARegion), "nodetree area for node");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_UI;
        ar->alignment = RGN_ALIGN_LEFT;
        ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
        ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
        /* temporarily hide it */
        ar->flag = RGN_FLAG_HIDDEN;
        break;
      case SPACE_FILE:
        ar = MEM_callocN(sizeof(ARegion), "nodetree area for node");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_CHANNELS;
        ar->alignment = RGN_ALIGN_LEFT;

        ar = MEM_callocN(sizeof(ARegion), "ui area for file");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_UI;
        ar->alignment = RGN_ALIGN_TOP;
        break;
      case SPACE_SEQ:
        ar_main = (ARegion *)lb->first;
        for (; ar_main; ar_main = ar_main->next) {
          if (ar_main->regiontype == RGN_TYPE_WINDOW) {
            break;
          }
        }
        ar = MEM_callocN(sizeof(ARegion), "preview area for sequencer");
        BLI_insertlinkbefore(lb, ar_main, ar);
        sequencer_init_preview_region(ar);
        break;
      case SPACE_VIEW3D:
        /* toolbar */
        ar = MEM_callocN(sizeof(ARegion), "toolbar for view3d");

        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_TOOLS;
        ar->alignment = RGN_ALIGN_LEFT;
        ar->flag = RGN_FLAG_HIDDEN;

        /* tool properties */
        ar = MEM_callocN(sizeof(ARegion), "tool properties for view3d");

        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_TOOL_PROPS;
        ar->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
        ar->flag = RGN_FLAG_HIDDEN;

        /* buttons/list view */
        ar = MEM_callocN(sizeof(ARegion), "buttons for view3d");

        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_UI;
        ar->alignment = RGN_ALIGN_RIGHT;
        ar->flag = RGN_FLAG_HIDDEN;
#if 0
      case SPACE_PROPERTIES:
        /* context UI region */
        ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");
        BLI_addtail(lb, ar);
        ar->regiontype = RGN_TYPE_UI;
        ar->alignment = RGN_ALIGN_RIGHT;

        break;
#endif
    }
  }

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "area region from do_versions");

  BLI_addtail(lb, ar);
  ar->winrct = sa->totrct;

  ar->regiontype = RGN_TYPE_WINDOW;

  if (sl) {
    /* if active spacetype has view2d data, copy that over to main region */
    /* and we split view3d */
    switch (sl->spacetype) {
      case SPACE_VIEW3D:
        blo_do_versions_view3d_split_250((View3D *)sl, lb);
        break;

      case SPACE_OUTLINER: {
        SpaceOutliner *soops = (SpaceOutliner *)sl;

        memcpy(&ar->v2d, &soops->v2d, sizeof(View2D));

        ar->v2d.scroll &= ~V2D_SCROLL_LEFT;
        ar->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
        ar->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
        ar->v2d.keepzoom |= (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_KEEPASPECT);
        ar->v2d.keeptot = V2D_KEEPTOT_STRICT;
        ar->v2d.minzoom = ar->v2d.maxzoom = 1.0f;
        // ar->v2d.flag |= V2D_IS_INITIALISED;
        break;
      }
      case SPACE_GRAPH: {
        SpaceGraph *sipo = (SpaceGraph *)sl;
        memcpy(&ar->v2d, &sipo->v2d, sizeof(View2D));

        /* init mainarea view2d */
        ar->v2d.scroll |= (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
        ar->v2d.scroll |= (V2D_SCROLL_LEFT | V2D_SCROLL_VERTICAL_HANDLES);

        ar->v2d.min[0] = FLT_MIN;
        ar->v2d.min[1] = FLT_MIN;

        ar->v2d.max[0] = MAXFRAMEF;
        ar->v2d.max[1] = FLT_MAX;

        // ar->v2d.flag |= V2D_IS_INITIALISED;
        break;
      }
      case SPACE_NLA: {
        SpaceNla *snla = (SpaceNla *)sl;
        memcpy(&ar->v2d, &snla->v2d, sizeof(View2D));

        ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
        ar->v2d.tot.ymax = 0.0f;

        ar->v2d.scroll |= (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
        ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
        ar->v2d.align = V2D_ALIGN_NO_POS_Y;
        ar->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
        break;
      }
      case SPACE_ACTION: {
        SpaceAction *saction = (SpaceAction *)sl;

        /* We totally reinit the view for the Action Editor,
         * as some old instances had some weird cruft set. */
        ar->v2d.tot.xmin = -20.0f;
        ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
        ar->v2d.tot.xmax = (float)((sa->winx > 120) ? (sa->winx) : 120);
        ar->v2d.tot.ymax = 0.0f;

        ar->v2d.cur = ar->v2d.tot;

        ar->v2d.min[0] = 0.0f;
        ar->v2d.min[1] = 0.0f;

        ar->v2d.max[0] = MAXFRAMEF;
        ar->v2d.max[1] = FLT_MAX;

        ar->v2d.minzoom = 0.01f;
        ar->v2d.maxzoom = 50;
        ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
        ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
        ar->v2d.keepzoom = V2D_LOCKZOOM_Y;
        ar->v2d.align = V2D_ALIGN_NO_POS_Y;
        ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

        /* for old files with ShapeKey editors open + an action set, clear the action as
         * it doesn't make sense in the new system (i.e. violates concept that ShapeKey edit
         * only shows ShapeKey-rooted actions only)
         */
        if (saction->mode == SACTCONT_SHAPEKEY) {
          saction->action = NULL;
        }
        break;
      }
      case SPACE_SEQ: {
        SpaceSeq *sseq = (SpaceSeq *)sl;
        memcpy(&ar->v2d, &sseq->v2d, sizeof(View2D));

        ar->v2d.scroll |= (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
        ar->v2d.scroll |= (V2D_SCROLL_LEFT | V2D_SCROLL_VERTICAL_HANDLES);
        ar->v2d.align = V2D_ALIGN_NO_NEG_Y;
        ar->v2d.flag |= V2D_IS_INITIALISED;
        break;
      }
      case SPACE_NODE: {
        SpaceNode *snode = (SpaceNode *)sl;
        memcpy(&ar->v2d, &snode->v2d, sizeof(View2D));

        ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
        ar->v2d.keepzoom = V2D_LIMITZOOM | V2D_KEEPASPECT;
        break;
      }
      case SPACE_PROPERTIES: {
        SpaceProperties *sbuts = (SpaceProperties *)sl;
        memcpy(&ar->v2d, &sbuts->v2d, sizeof(View2D));

        ar->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
        break;
      }
      case SPACE_FILE: {
        // SpaceFile *sfile = (SpaceFile *)sl;
        ar->v2d.tot.xmin = ar->v2d.tot.ymin = 0;
        ar->v2d.tot.xmax = ar->winx;
        ar->v2d.tot.ymax = ar->winy;
        ar->v2d.cur = ar->v2d.tot;
        ar->regiontype = RGN_TYPE_WINDOW;
        ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
        ar->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
        ar->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
        break;
      }
      case SPACE_TEXT: {
        SpaceText *st = (SpaceText *)sl;
        st->flags |= ST_FIND_WRAP;
      }
        // case SPACE_XXX: // FIXME... add other ones
        //  memcpy(&ar->v2d, &((SpaceXxx *)sl)->v2d, sizeof(View2D));
        //  break;
    }
  }
}

static void do_versions_windowmanager_2_50(bScreen *screen)
{
  ScrArea *sa;
  SpaceLink *sl;

  /* add regions */
  for (sa = screen->areabase.first; sa; sa = sa->next) {
    /* we keep headertype variable to convert old files only */
    if (sa->headertype) {
      area_add_header_region(sa, &sa->regionbase);
    }

    area_add_window_regions(sa, sa->spacedata.first, &sa->regionbase);

    /* space imageselect is deprecated */
    for (sl = sa->spacedata.first; sl; sl = sl->next) {
      if (sl->spacetype == SPACE_IMASEL) {
        sl->spacetype = SPACE_EMPTY; /* spacedata then matches */
      }
    }

    /* space sound is deprecated */
    for (sl = sa->spacedata.first; sl; sl = sl->next) {
      if (sl->spacetype == SPACE_SOUND) {
        sl->spacetype = SPACE_EMPTY; /* spacedata then matches */
      }
    }

    /* pushed back spaces also need regions! */
    if (sa->spacedata.first) {
      sl = sa->spacedata.first;
      for (sl = sl->next; sl; sl = sl->next) {
        if (sa->headertype) {
          area_add_header_region(sa, &sl->regionbase);
        }
        area_add_window_regions(sa, sl, &sl->regionbase);
      }
    }
  }
}

static void versions_gpencil_add_main(ListBase *lb, ID *id, const char *name)
{
  BLI_addtail(lb, id);
  id->us = 1;
  id->flag = LIB_FAKEUSER;
  *((short *)id->name) = ID_GD;

  BKE_id_new_name_validate(lb, id, name);
  /* alphabetic insertion: is in BKE_id_new_name_validate */

  if (G.debug & G_DEBUG) {
    printf("Converted GPencil to ID: %s\n", id->name + 2);
  }
}

static void do_versions_gpencil_2_50(Main *main, bScreen *screen)
{
  ScrArea *sa;
  SpaceLink *sl;

  /* add regions */
  for (sa = screen->areabase.first; sa; sa = sa->next) {
    for (sl = sa->spacedata.first; sl; sl = sl->next) {
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;
        if (v3d->gpd) {
          versions_gpencil_add_main(&main->gpencils, (ID *)v3d->gpd, "GPencil View3D");
          v3d->gpd = NULL;
        }
      }
      else if (sl->spacetype == SPACE_NODE) {
        SpaceNode *snode = (SpaceNode *)sl;
        if (snode->gpd) {
          versions_gpencil_add_main(&main->gpencils, (ID *)snode->gpd, "GPencil Node");
          snode->gpd = NULL;
        }
      }
      else if (sl->spacetype == SPACE_SEQ) {
        SpaceSeq *sseq = (SpaceSeq *)sl;
        if (sseq->gpd) {
          versions_gpencil_add_main(&main->gpencils, (ID *)sseq->gpd, "GPencil Node");
          sseq->gpd = NULL;
        }
      }
      else if (sl->spacetype == SPACE_IMAGE) {
        SpaceImage *sima = (SpaceImage *)sl;
#if 0 /* see comment on r28002 */
        if (sima->gpd) {
          versions_gpencil_add_main(&main->gpencil, (ID *)sima->gpd, "GPencil Image");
          sima->gpd = NULL;
        }
#else
        sima->gpd = NULL;
#endif
      }
    }
  }
}

static void do_version_mdef_250(Main *main)
{
  Object *ob;
  ModifierData *md;
  MeshDeformModifierData *mmd;

  for (ob = main->objects.first; ob; ob = ob->id.next) {
    for (md = ob->modifiers.first; md; md = md->next) {
      if (md->type == eModifierType_MeshDeform) {
        mmd = (MeshDeformModifierData *)md;

        if (mmd->bindcos) {
          /* make bindcos NULL in order to trick older versions
           * into thinking that the mesh was not bound yet */
          mmd->bindcagecos = mmd->bindcos;
          mmd->bindcos = NULL;

          modifier_mdef_compact_influences(md);
        }
      }
    }
  }
}

static void do_version_constraints_radians_degrees_250(ListBase *lb)
{
  bConstraint *con;

  for (con = lb->first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
      bKinematicConstraint *data = con->data;
      data->poleangle *= (float)(M_PI / 180.0);
    }
    else if (con->type == CONSTRAINT_TYPE_ROTLIMIT) {
      bRotLimitConstraint *data = con->data;

      data->xmin *= (float)(M_PI / 180.0);
      data->xmax *= (float)(M_PI / 180.0);
      data->ymin *= (float)(M_PI / 180.0);
      data->ymax *= (float)(M_PI / 180.0);
      data->zmin *= (float)(M_PI / 180.0);
      data->zmax *= (float)(M_PI / 180.0);
    }
  }
}

static void do_version_bone_roll_256(Bone *bone)
{
  Bone *child;
  float submat[3][3];

  copy_m3_m4(submat, bone->arm_mat);
  mat3_to_vec_roll(submat, NULL, &bone->arm_roll);

  for (child = bone->childbase.first; child; child = child->next) {
    do_version_bone_roll_256(child);
  }
}

/* deprecated, only keep this for readfile.c */
/* XXX Deprecated function to add a socket in ntree->inputs/ntree->outputs list
 * (previously called node_group_add_socket). This function has been superseded
 * by the implementation of proxy nodes. It is still necessary though
 * for do_versions of pre-2.56.2 code (r35033), so later proxy nodes
 * can be generated consistently from ntree socket lists.
 */
static bNodeSocket *do_versions_node_group_add_socket_2_56_2(bNodeTree *ngroup,
                                                             const char *name,
                                                             int type,
                                                             int in_out)
{
  //  bNodeSocketType *stype = ntreeGetSocketType(type);
  bNodeSocket *gsock = MEM_callocN(sizeof(bNodeSocket), "bNodeSocket");

  BLI_strncpy(gsock->name, name, sizeof(gsock->name));
  gsock->type = type;

  gsock->next = gsock->prev = NULL;
  gsock->new_sock = NULL;
  gsock->link = NULL;
  /* assign new unique index */
  gsock->own_index = ngroup->cur_index++;
  gsock->limit = (in_out == SOCK_IN ? 0xFFF : 1);

  //  if (stype->value_structsize > 0)
  //      gsock->default_value = MEM_callocN(stype->value_structsize, "default socket value");

  BLI_addtail(in_out == SOCK_IN ? &ngroup->inputs : &ngroup->outputs, gsock);

  ngroup->update |= (in_out == SOCK_IN ? NTREE_UPDATE_GROUP_IN : NTREE_UPDATE_GROUP_OUT);

  return gsock;
}

/* Create default_value structs for node sockets from the internal bNodeStack value.
 * These structs were used from 2.59.2 on, but are replaced in the subsequent do_versions for
 * custom nodes by generic ID property values. This conversion happened _after_ do_versions
 * originally due to messy type initialization for node sockets.
 * Now created here intermediately for convenience and to keep do_versions consistent.
 *
 * Node compatibility code is gross ...
 */
static void do_versions_socket_default_value_259(bNodeSocket *sock)
{
  bNodeSocketValueFloat *valfloat;
  bNodeSocketValueVector *valvector;
  bNodeSocketValueRGBA *valrgba;

  if (sock->default_value) {
    return;
  }

  switch (sock->type) {
    case SOCK_FLOAT:
      valfloat = sock->default_value = MEM_callocN(sizeof(bNodeSocketValueFloat),
                                                   "default socket value");
      valfloat->value = sock->ns.vec[0];
      valfloat->min = sock->ns.min;
      valfloat->max = sock->ns.max;
      valfloat->subtype = PROP_NONE;
      break;
    case SOCK_VECTOR:
      valvector = sock->default_value = MEM_callocN(sizeof(bNodeSocketValueVector),
                                                    "default socket value");
      copy_v3_v3(valvector->value, sock->ns.vec);
      valvector->min = sock->ns.min;
      valvector->max = sock->ns.max;
      valvector->subtype = PROP_NONE;
      break;
    case SOCK_RGBA:
      valrgba = sock->default_value = MEM_callocN(sizeof(bNodeSocketValueRGBA),
                                                  "default socket value");
      copy_v4_v4(valrgba->value, sock->ns.vec);
      break;
  }
}

void blo_do_versions_250(FileData *fd, Library *lib, Main *bmain)
{
  /* WATCH IT!!!: pointers from libdata have not been converted */

  if (bmain->versionfile < 250) {
    bScreen *screen;
    Scene *scene;
    Base *base;
    Material *ma;
    Camera *cam;
    Curve *cu;
    Scene *sce;
    Tex *tx;
    ParticleSettings *part;
    Object *ob;
    // PTCacheID *pid;
    // ListBase pidlist;

    bSound *sound;
    Sequence *seq;

    for (sound = bmain->sounds.first; sound; sound = sound->id.next) {
      if (sound->newpackedfile) {
        sound->packedfile = sound->newpackedfile;
        sound->newpackedfile = NULL;
      }
    }

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->ed && scene->ed->seqbasep) {
        SEQ_BEGIN (scene->ed, seq) {
          if (seq->type == SEQ_TYPE_SOUND_HD) {
            char str[FILE_MAX];
            BLI_join_dirfile(str, sizeof(str), seq->strip->dir, seq->strip->stripdata->name);
            BLI_path_abs(str, BKE_main_blendfile_path(bmain));
            seq->sound = BKE_sound_new_file(bmain, str);
          }
#define SEQ_USE_PROXY_CUSTOM_DIR (1 << 19)
#define SEQ_USE_PROXY_CUSTOM_FILE (1 << 21)
          /* don't know, if anybody used that this way, but just in case, upgrade to new way... */
          if ((seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) && !(seq->flag & SEQ_USE_PROXY_CUSTOM_DIR)) {
            BLI_snprintf(seq->strip->proxy->dir, FILE_MAXDIR, "%s/BL_proxy", seq->strip->dir);
          }
#undef SEQ_USE_PROXY_CUSTOM_DIR
#undef SEQ_USE_PROXY_CUSTOM_FILE
        }
        SEQ_END;
      }
    }

    for (screen = bmain->screens.first; screen; screen = screen->id.next) {
      do_versions_windowmanager_2_50(screen);
      do_versions_gpencil_2_50(bmain, screen);
    }

    /* shader, composite and texture node trees have id.name empty, put something in
     * to have them show in RNA viewer and accessible otherwise.
     */
    for (ma = bmain->materials.first; ma; ma = ma->id.next) {
      if (ma->nodetree && ma->nodetree->id.name[0] == '\0') {
        strcpy(ma->nodetree->id.name, "NTShader Nodetree");
      }
    }

    /* and composite trees */
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      enum { R_PANORAMA = (1 << 10) };
      if (sce->nodetree && sce->nodetree->id.name[0] == '\0') {
        strcpy(sce->nodetree->id.name, "NTCompositing Nodetree");
      }

      /* move to cameras */
      if (sce->r.mode & R_PANORAMA) {
        for (base = sce->base.first; base; base = base->next) {
          ob = blo_do_versions_newlibadr(fd, lib, base->object);

          if (ob->type == OB_CAMERA && !ob->id.lib) {
            cam = blo_do_versions_newlibadr(fd, lib, ob->data);
            cam->flag |= CAM_PANORAMA;
          }
        }

        sce->r.mode &= ~R_PANORAMA;
      }
    }

    /* and texture trees */
    for (tx = bmain->textures.first; tx; tx = tx->id.next) {
      bNode *node;

      if (tx->nodetree) {
        if (tx->nodetree->id.name[0] == '\0') {
          strcpy(tx->nodetree->id.name, "NTTexture Nodetree");
        }

        /* which_output 0 is now "not specified" */
        for (node = tx->nodetree->nodes.first; node; node = node->next) {
          if (node->type == TEX_NODE_OUTPUT) {
            node->custom1++;
          }
        }
      }
    }

    /* particle draw and render types */
    for (part = bmain->particles.first; part; part = part->id.next) {
      if (part->draw_as) {
        if (part->draw_as == PART_DRAW_DOT) {
          part->ren_as = PART_DRAW_HALO;
          part->draw_as = PART_DRAW_REND;
        }
        else if (part->draw_as <= PART_DRAW_AXIS) {
          part->ren_as = PART_DRAW_HALO;
        }
        else {
          part->ren_as = part->draw_as;
          part->draw_as = PART_DRAW_REND;
        }
      }
      part->path_end = 1.0f;
      part->clength = 1.0f;
    }

    /* set old pointcaches to have disk cache flag */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {

      // BKE_ptcache_ids_from_object(&pidlist, ob);

      // for (pid = pidlist.first; pid; pid = pid->next)
      //  pid->cache->flag |= PTCACHE_DISK_CACHE;

      // BLI_freelistN(&pidlist);
    }

    /* type was a mixed flag & enum. move the 2d flag elsewhere */
    for (cu = bmain->curves.first; cu; cu = cu->id.next) {
      Nurb *nu;

      for (nu = cu->nurb.first; nu; nu = nu->next) {
        nu->flag |= (nu->type & CU_2D);
        nu->type &= CU_TYPE;
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 1)) {
    Object *ob;
    Tex *tex;
    Scene *sce;
    ToolSettings *ts;
    // PTCacheID *pid;
    // ListBase pidlist;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      // BKE_ptcache_ids_from_object(&pidlist, ob);

      // for (pid = pidlist.first; pid; pid = pid->next) {
      //  if (BLI_listbase_is_empty(pid->ptcaches))
      //      pid->ptcaches->first = pid->ptcaches->last = pid->cache;
      //}

      // BLI_freelistN(&pidlist);

      if (ob->type == OB_MESH) {
        Mesh *me = blo_do_versions_newlibadr(fd, lib, ob->data);
        void *olddata = ob->data;
        ob->data = me;

        /* XXX - library meshes crash on loading most yoFrankie levels,
         * the multires pointer gets invalid -  Campbell */
        if (me && me->id.lib == NULL && me->mr && me->mr->level_count > 1) {
          multires_load_old(ob, me);
        }

        ob->data = olddata;
      }

      if (ob->totcol && ob->matbits == NULL) {
        int a;

        ob->matbits = MEM_calloc_arrayN(ob->totcol, sizeof(char), "ob->matbits");
        for (a = 0; a < ob->totcol; a++) {
          ob->matbits[a] = (ob->colbits & (1 << a)) != 0;
        }
      }
    }

    /* texture filter */
    for (tex = bmain->textures.first; tex; tex = tex->id.next) {
      if (tex->afmax == 0) {
        tex->afmax = 8;
      }
    }

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      ts = sce->toolsettings;
      if (!ts->uv_selectmode || ts->vgroup_weight == 0.0f) {
        ts->selectmode = SCE_SELECT_VERTEX;

        /* autokeying - setting should be taken from the user-prefs
         * but the userprefs version may not have correct flags set
         * (i.e. will result in blank box when enabled)
         */
        ts->autokey_mode = U.autokey_mode;
        if (ts->autokey_mode == 0) {
          ts->autokey_mode = 2; /* 'add/replace' but not on */
        }
        ts->uv_selectmode = UV_SELECT_VERTEX;
        ts->vgroup_weight = 1.0f;
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 2)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->flag & 8192) {  // OB_POSEMODE = 8192
        ob->mode |= OB_MODE_POSE;
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 4)) {
    Scene *sce;
    Object *ob;
    ParticleSettings *part;
    bool do_gravity = false;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->unit.scale_length == 0.0f) {
        sce->unit.scale_length = 1.0f;
      }
    }

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      /* fluid-sim stuff */
      FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(
          ob, eModifierType_Fluidsim);
      if (fluidmd) {
        fluidmd->fss->fmd = fluidmd;
      }

      /* rotation modes were added,
       * but old objects would now default to being 'quaternion based' */
      ob->rotmode = ROT_MODE_EUL;
    }

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->audio.main == 0.0f) {
        sce->audio.main = 1.0f;
      }

      sce->r.ffcodecdata.audio_mixrate = sce->audio.mixrate;
      sce->r.ffcodecdata.audio_volume = sce->audio.main;
      sce->audio.distance_model = 2;
      sce->audio.doppler_factor = 1.0f;
      sce->audio.speed_of_sound = 343.3f;
    }

    /* Add default gravity to scenes */
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if ((sce->physics_settings.flag & PHYS_GLOBAL_GRAVITY) == 0 &&
          is_zero_v3(sce->physics_settings.gravity)) {
        sce->physics_settings.gravity[0] = sce->physics_settings.gravity[1] = 0.0f;
        sce->physics_settings.gravity[2] = -9.81f;
        sce->physics_settings.flag = PHYS_GLOBAL_GRAVITY;
        do_gravity = true;
      }
    }

    /* Assign proper global gravity weights for dynamics
     * (only z-coordinate is taken into account) */
    if (do_gravity) {
      for (part = bmain->particles.first; part; part = part->id.next) {
        part->effector_weights->global_gravity = part->acc[2] / -9.81f;
      }
    }

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;

      if (do_gravity) {
        for (md = ob->modifiers.first; md; md = md->next) {
          ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob,
                                                                              eModifierType_Cloth);
          if (clmd) {
            clmd->sim_parms->effector_weights->global_gravity = clmd->sim_parms->gravity[2] /
                                                                -9.81f;
          }
        }

        if (ob->soft) {
          ob->soft->effector_weights->global_gravity = ob->soft->grav / 9.81f;
        }
      }

      /* Normal wind shape is plane */
      if (ob->pd) {
        if (ob->pd->forcefield == PFIELD_WIND) {
          ob->pd->shape = PFIELD_SHAPE_PLANE;
        }

        if (ob->pd->flag & PFIELD_PLANAR) {
          ob->pd->shape = PFIELD_SHAPE_PLANE;
        }
        else if (ob->pd->flag & PFIELD_SURFACE) {
          ob->pd->shape = PFIELD_SHAPE_SURFACE;
        }

        ob->pd->flag |= PFIELD_DO_LOCATION;
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 6)) {
    Object *ob;

    /* New variables for axis-angle rotations and/or quaternion rotations were added,
     * and need proper initialization */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      /* new variables for all objects */
      ob->quat[0] = 1.0f;
      ob->rotAxis[1] = 1.0f;

      /* bones */
      if (ob->pose) {
        bPoseChannel *pchan;

        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          /* just need to initalise rotation axis properly... */
          pchan->rotAxis[1] = 1.0f;
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 7)) {
    Mesh *me;
    Nurb *nu;
    Lattice *lt;
    Curve *cu;
    Key *key;
    const float *data;
    int a, tot;

    /* shape keys are no longer applied to the mesh itself, but rather
     * to the derivedmesh/displist, so here we ensure that the basis
     * shape key is always set in the mesh coordinates. */
    for (me = bmain->meshes.first; me; me = me->id.next) {
      if ((key = blo_do_versions_newlibadr(fd, lib, me->key)) && key->refkey) {
        data = key->refkey->data;
        tot = MIN2(me->totvert, key->refkey->totelem);

        for (a = 0; a < tot; a++, data += 3) {
          copy_v3_v3(me->mvert[a].co, data);
        }
      }
    }

    for (lt = bmain->lattices.first; lt; lt = lt->id.next) {
      if ((key = blo_do_versions_newlibadr(fd, lib, lt->key)) && key->refkey) {
        data = key->refkey->data;
        tot = MIN2(lt->pntsu * lt->pntsv * lt->pntsw, key->refkey->totelem);

        for (a = 0; a < tot; a++, data += 3) {
          copy_v3_v3(lt->def[a].vec, data);
        }
      }
    }

    for (cu = bmain->curves.first; cu; cu = cu->id.next) {
      if ((key = blo_do_versions_newlibadr(fd, lib, cu->key)) && key->refkey) {
        data = key->refkey->data;

        for (nu = cu->nurb.first; nu; nu = nu->next) {
          if (nu->bezt) {
            BezTriple *bezt = nu->bezt;

            for (a = 0; a < nu->pntsu; a++, bezt++) {
              copy_v3_v3(bezt->vec[0], data);
              data += 3;
              copy_v3_v3(bezt->vec[1], data);
              data += 3;
              copy_v3_v3(bezt->vec[2], data);
              data += 3;
              bezt->tilt = *data;
              data++;
            }
          }
          else if (nu->bp) {
            BPoint *bp = nu->bp;

            for (a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
              copy_v3_v3(bp->vec, data);
              data += 3;
              bp->tilt = *data;
              data++;
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 8)) {
    {
      Scene *sce = bmain->scenes.first;
      while (sce) {
        if (sce->r.frame_step == 0) {
          sce->r.frame_step = 1;
        }

        sce = sce->id.next;
      }
    }

    {
      /* ensure all nodes have unique names */
      bNodeTree *ntree = bmain->nodetrees.first;
      while (ntree) {
        bNode *node = ntree->nodes.first;

        while (node) {
          nodeUniqueName(ntree, node);
          node = node->next;
        }

        ntree = ntree->id.next;
      }
    }

    {
      Object *ob = bmain->objects.first;
      while (ob) {
        /* shaded mode disabled for now */
        if (ob->dt == OB_MATERIAL) {
          ob->dt = OB_TEXTURE;
        }
        ob = ob->id.next;
      }
    }

    {
      bScreen *screen;
      ScrArea *sa;
      SpaceLink *sl;

      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (sa = screen->areabase.first; sa; sa = sa->next) {
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              if (v3d->drawtype == OB_MATERIAL) {
                v3d->drawtype = OB_SOLID;
              }
            }
          }
        }
      }
    }

    /* only convert old 2.50 files with color management */
    if (bmain->versionfile == 250) {
      Scene *sce = bmain->scenes.first;
      Material *ma = bmain->materials.first;
      Tex *tex = bmain->textures.first;
      int i, convert = 0;

      /* convert to new color management system:
       * while previously colors were stored as srgb,
       * now they are stored as linear internally,
       * with screen gamma correction in certain places in the UI. */

      /* don't know what scene is active, so we'll convert if any scene has it enabled... */
      while (sce) {
        if (sce->r.color_mgt_flag & R_COLOR_MANAGEMENT) {
          convert = 1;
        }
        sce = sce->id.next;
      }

      if (convert) {
        while (ma) {
          srgb_to_linearrgb_v3_v3(&ma->r, &ma->r);
          srgb_to_linearrgb_v3_v3(&ma->specr, &ma->specr);
          ma = ma->id.next;
        }

        while (tex) {
          if (tex->coba) {
            ColorBand *band = (ColorBand *)tex->coba;
            for (i = 0; i < band->tot; i++) {
              CBData *data = band->data + i;
              srgb_to_linearrgb_v3_v3(&data->r, &data->r);
            }
          }
          tex = tex->id.next;
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 9)) {
    Scene *sce;
    Mesh *me;
    Object *ob;

    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (!sce->toolsettings->particle.selectmode) {
        sce->toolsettings->particle.selectmode = SCE_SELECT_PATH;
      }
    }

    if (bmain->versionfile == 250 && bmain->subversionfile > 1) {
      for (me = bmain->meshes.first; me; me = me->id.next) {
        multires_load_old_250(me);
      }

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        MultiresModifierData *mmd = (MultiresModifierData *)modifiers_findByType(
            ob, eModifierType_Multires);

        if (mmd) {
          mmd->totlvl--;
          mmd->lvl--;
          mmd->sculptlvl = mmd->lvl;
          mmd->renderlvl = mmd->lvl;
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 10)) {
    Object *ob;

    /* properly initialize hair clothsim data on old files */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;
          if (clmd->sim_parms->velocity_smooth < 0.01f) {
            clmd->sim_parms->velocity_smooth = 0.f;
          }
        }
      }
    }
  }

  /* fix bad area setup in subversion 10 */
  if (bmain->versionfile == 250 && bmain->subversionfile == 10) {
    /* fix for new view type in sequencer */
    bScreen *screen;
    ScrArea *sa;
    SpaceLink *sl;

    /* remove all preview window in wrong spaces */
    for (screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (sa = screen->areabase.first; sa; sa = sa->next) {
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype != SPACE_SEQ) {
            ARegion *ar;
            ListBase *regionbase;

            if (sl == sa->spacedata.first) {
              regionbase = &sa->regionbase;
            }
            else {
              regionbase = &sl->regionbase;
            }

            for (ar = regionbase->first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_PREVIEW) {
                break;
              }
            }

            if (ar && (ar->regiontype == RGN_TYPE_PREVIEW)) {
              SpaceType *st = BKE_spacetype_from_id(SPACE_SEQ);
              BKE_area_region_free(st, ar);
              BLI_freelinkN(regionbase, ar);
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 11)) {
    {
      /* fix for new view type in sequencer */
      bScreen *screen;
      ScrArea *sa;
      SpaceLink *sl;

      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (sa = screen->areabase.first; sa; sa = sa->next) {
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_SEQ) {
              ARegion *ar;
              ARegion *ar_main;
              ListBase *regionbase;
              SpaceSeq *sseq = (SpaceSeq *)sl;

              if (sl == sa->spacedata.first) {
                regionbase = &sa->regionbase;
              }
              else {
                regionbase = &sl->regionbase;
              }

              if (sseq->view == 0) {
                sseq->view = SEQ_VIEW_SEQUENCE;
              }
              if (sseq->mainb == 0) {
                sseq->mainb = SEQ_DRAW_IMG_IMBUF;
              }

              ar_main = (ARegion *)regionbase->first;
              for (; ar_main; ar_main = ar_main->next) {
                if (ar_main->regiontype == RGN_TYPE_WINDOW) {
                  break;
                }
              }
              ar = MEM_callocN(sizeof(ARegion), "preview area for sequencer");
              BLI_insertlinkbefore(regionbase, ar_main, ar);
              sequencer_init_preview_region(ar);
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 12)) {
    Object *ob;
    Brush *brush;

    /* anim viz changes */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      /* initialize object defaults */
      animviz_settings_init(&ob->avs);

      /* if armature, copy settings for pose from armature data
       * performing initialization where appropriate
       */
      if (ob->pose && ob->data) {
        bArmature *arm = blo_do_versions_newlibadr(fd, lib, ob->data);
        if (arm) { /* XXX - why does this fail in some cases? */
          bAnimVizSettings *avs = &ob->pose->avs;

          /* path settings --------------------- */
          /* ranges */
          avs->path_bc = 10;
          avs->path_ac = 10;

          avs->path_sf = 1;
          avs->path_ef = 250;

          /* flags */
          if (arm->pathflag & ARM_PATH_FNUMS) {
            avs->path_viewflag |= MOTIONPATH_VIEW_FNUMS;
          }
          if (arm->pathflag & ARM_PATH_KFRAS) {
            avs->path_viewflag |= MOTIONPATH_VIEW_KFRAS;
          }
          if (arm->pathflag & ARM_PATH_KFNOS) {
            avs->path_viewflag |= MOTIONPATH_VIEW_KFNOS;
          }

          /* bake flags */
          if (arm->pathflag & ARM_PATH_HEADS) {
            avs->path_bakeflag |= MOTIONPATH_BAKE_HEADS;
          }

          /* type */
          if (arm->pathflag & ARM_PATH_ACFRA) {
            avs->path_type = MOTIONPATH_TYPE_ACFRA;
          }

          /* stepsize */
          avs->path_step = 1;
        }
        else {
          animviz_settings_init(&ob->pose->avs);
        }
      }
    }

    /* brush texture changes */
    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      BKE_texture_mtex_default(&brush->mtex);
      BKE_texture_mtex_default(&brush->mask_mtex);
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 13)) {
    /* NOTE: if you do more conversion, be sure to do it outside of this and
     * increase subversion again, otherwise it will not be correct */
    Object *ob;

    /* convert degrees to radians for internal use */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      bPoseChannel *pchan;

      do_version_constraints_radians_degrees_250(&ob->constraints);

      if (ob->pose) {
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          pchan->limitmin[0] *= (float)(M_PI / 180.0);
          pchan->limitmin[1] *= (float)(M_PI / 180.0);
          pchan->limitmin[2] *= (float)(M_PI / 180.0);
          pchan->limitmax[0] *= (float)(M_PI / 180.0);
          pchan->limitmax[1] *= (float)(M_PI / 180.0);
          pchan->limitmax[2] *= (float)(M_PI / 180.0);

          do_version_constraints_radians_degrees_250(&pchan->constraints);
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 14)) {
    /* fix for bad View2D extents for Animation Editors */
    bScreen *screen;
    ScrArea *sa;
    SpaceLink *sl;

    for (screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (sa = screen->areabase.first; sa; sa = sa->next) {
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          ListBase *regionbase;
          ARegion *ar;

          if (sl == sa->spacedata.first) {
            regionbase = &sa->regionbase;
          }
          else {
            regionbase = &sl->regionbase;
          }

          if (ELEM(sl->spacetype, SPACE_ACTION, SPACE_NLA)) {
            for (ar = (ARegion *)regionbase->first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_WINDOW) {
                ar->v2d.cur.ymax = ar->v2d.tot.ymax = 0.0f;
                ar->v2d.cur.ymin = ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
              }
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 17)) {
    Scene *sce;
    Sequence *seq;

    /* initialize to sane default so toggling on border shows something */
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->r.border.xmin == 0.0f && sce->r.border.ymin == 0.0f && sce->r.border.xmax == 0.0f &&
          sce->r.border.ymax == 0.0f) {
        sce->r.border.xmin = 0.0f;
        sce->r.border.ymin = 0.0f;
        sce->r.border.xmax = 1.0f;
        sce->r.border.ymax = 1.0f;
      }

      if ((sce->r.ffcodecdata.flags & FFMPEG_MULTIPLEX_AUDIO) == 0) {
        sce->r.ffcodecdata.audio_codec = 0x0;  // CODEC_ID_NONE
      }

      SEQ_BEGIN (sce->ed, seq) {
        seq->volume = 1.0f;
      }
      SEQ_END;
    }

    /* particle brush strength factor was changed from int to float */
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      ParticleEditSettings *pset = &sce->toolsettings->particle;
      int a;

      for (a = 0; a < ARRAY_SIZE(pset->brush); a++) {
        pset->brush[a].strength /= 100.0f;
      }
    }

    /* sequencer changes */
    {
      bScreen *screen;
      ScrArea *sa;
      SpaceLink *sl;

      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (sa = screen->areabase.first; sa; sa = sa->next) {
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_SEQ) {
              ARegion *ar_preview;
              ListBase *regionbase;

              if (sl == sa->spacedata.first) {
                regionbase = &sa->regionbase;
              }
              else {
                regionbase = &sl->regionbase;
              }

              ar_preview = (ARegion *)regionbase->first;
              for (; ar_preview; ar_preview = ar_preview->next) {
                if (ar_preview->regiontype == RGN_TYPE_PREVIEW) {
                  break;
                }
              }
              if (ar_preview && (ar_preview->regiontype == RGN_TYPE_PREVIEW)) {
                sequencer_init_preview_region(ar_preview);
              }
            }
          }
        }
      }
    } /* sequencer changes */
  }

  if (bmain->versionfile <= 251) { /* 2.5.1 had no subversions */
    bScreen *sc;

    /* Blender 2.5.2 - subversion 0 introduced a new setting: V3D_HIDE_OVERLAYS.
     * This bit was used in the past for V3D_TRANSFORM_SNAP, which is now deprecated.
     * Here we clear it for old files so they don't come in with V3D_HIDE_OVERLAYS set,
     * which would cause cameras, lights, etc to become invisible */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->flag2 &= ~V3D_HIDE_OVERLAYS;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 252 || (bmain->versionfile == 252 && bmain->subversionfile < 1)) {
    Brush *brush;
    Object *ob;
    Scene *scene;
    bNodeTree *ntree;

    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      if (brush->curve) {
        brush->curve->preset = CURVE_PRESET_SMOOTH;
      }
    }

    /* properly initialize active flag for fluidsim modifiers */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Fluidsim) {
          FluidsimModifierData *fmd = (FluidsimModifierData *)md;
          fmd->fss->flag |= OB_FLUIDSIM_ACTIVE;
          fmd->fss->flag |= OB_FLUIDSIM_OVERRIDE_TIME;
        }
      }
    }

    /* adjustment to color balance node values */
    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->nodetree) {
        bNode *node = scene->nodetree->nodes.first;

        while (node) {
          if (node->type == CMP_NODE_COLORBALANCE) {
            NodeColorBalance *n = (NodeColorBalance *)node->storage;
            n->lift[0] += 1.f;
            n->lift[1] += 1.f;
            n->lift[2] += 1.f;
          }
          node = node->next;
        }
      }
    }
    /* check inside node groups too */
    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      bNode *node = ntree->nodes.first;

      while (node) {
        if (node->type == CMP_NODE_COLORBALANCE) {
          NodeColorBalance *n = (NodeColorBalance *)node->storage;
          n->lift[0] += 1.f;
          n->lift[1] += 1.f;
          n->lift[2] += 1.f;
        }

        node = node->next;
      }
    }
  }

  /* old-track -> constraints (this time we're really doing it!) */
  if (bmain->versionfile < 252 || (bmain->versionfile == 252 && bmain->subversionfile < 2)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      blo_do_version_old_trackto_to_constraints(ob);
    }
  }

  if (bmain->versionfile < 252 || (bmain->versionfile == 252 && bmain->subversionfile < 5)) {
    bScreen *sc;

    /* Image editor scopes */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;

      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = (SpaceImage *)sl;
            scopes_new(&sima->scopes);
          }
        }
      }
    }
  }

  if (bmain->versionfile < 253) {
    Object *ob;
    Scene *scene;
    bScreen *sc;
    Tex *tex;
    Brush *brush;

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_NODE) {
            SpaceNode *snode = (SpaceNode *)sl;
            ListBase *regionbase;
            ARegion *ar;

            if (sl == sa->spacedata.first) {
              regionbase = &sa->regionbase;
            }
            else {
              regionbase = &sl->regionbase;
            }

            if (snode->v2d.minzoom > 0.09f) {
              snode->v2d.minzoom = 0.09f;
            }
            if (snode->v2d.maxzoom < 2.31f) {
              snode->v2d.maxzoom = 2.31f;
            }

            for (ar = regionbase->first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_WINDOW) {
                if (ar->v2d.minzoom > 0.09f) {
                  ar->v2d.minzoom = 0.09f;
                }
                if (ar->v2d.maxzoom < 2.31f) {
                  ar->v2d.maxzoom = 2.31f;
                }
              }
            }
          }
        }
      }
    }

    do_version_mdef_250(bmain);

    /* parent type to modifier */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->parent) {
        Object *parent = (Object *)blo_do_versions_newlibadr(fd, lib, ob->parent);
        if (parent) { /* parent may not be in group */
          enum { PARCURVE = 1 };
          if (parent->type == OB_ARMATURE && ob->partype == PARSKEL) {
            ArmatureModifierData *amd;
            bArmature *arm = (bArmature *)blo_do_versions_newlibadr(fd, lib, parent->data);

            amd = (ArmatureModifierData *)modifier_new(eModifierType_Armature);
            amd->object = ob->parent;
            BLI_addtail((ListBase *)&ob->modifiers, amd);
            amd->deformflag = arm->deformflag;
            ob->partype = PAROBJECT;
          }
          else if (parent->type == OB_LATTICE && ob->partype == PARSKEL) {
            LatticeModifierData *lmd;

            lmd = (LatticeModifierData *)modifier_new(eModifierType_Lattice);
            lmd->object = ob->parent;
            BLI_addtail((ListBase *)&ob->modifiers, lmd);
            ob->partype = PAROBJECT;
          }
          else if (parent->type == OB_CURVE && ob->partype == PARCURVE) {
            CurveModifierData *cmd;

            cmd = (CurveModifierData *)modifier_new(eModifierType_Curve);
            cmd->object = ob->parent;
            BLI_addtail((ListBase *)&ob->modifiers, cmd);
            ob->partype = PAROBJECT;
          }
        }
      }
    }

    /* initialize scene active layer */
    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      int i;
      for (i = 0; i < 20; i++) {
        if (scene->lay & (1 << i)) {
          scene->layact = 1 << i;
          break;
        }
      }
    }

    for (tex = bmain->textures.first; tex; tex = tex->id.next) {
      /* If you're picky, this isn't correct until we do a version bump
       * since you could set saturation to be 0.0. */
      if (tex->saturation == 0.0f) {
        tex->saturation = 1.0f;
      }
    }

    {
      Curve *cu;
      for (cu = bmain->curves.first; cu; cu = cu->id.next) {
        cu->smallcaps_scale = 0.75f;
      }
    }

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene) {
        Sequence *seq;
        SEQ_BEGIN (scene->ed, seq) {
          if (seq->sat == 0.0f) {
            seq->sat = 1.0f;
          }
        }
        SEQ_END;
      }
    }

    /* GSOC 2010 Sculpt - New settings for Brush */

    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      /* Sanity Check */

      /* infinite number of dabs */
      if (brush->spacing == 0) {
        brush->spacing = 10;
      }

      /* will have no effect */
      if (brush->alpha == 0) {
        brush->alpha = 0.5f;
      }

      /* bad radius */
      if (brush->unprojected_radius == 0) {
        brush->unprojected_radius = 0.125f;
      }

      /* unusable size */
      if (brush->size == 0) {
        brush->size = 35;
      }

      /* can't see overlay */
      if (brush->texture_overlay_alpha == 0) {
        brush->texture_overlay_alpha = 33;
      }

      /* same as draw brush */
      if (brush->crease_pinch_factor == 0) {
        brush->crease_pinch_factor = 0.5f;
      }

      /* will sculpt no vertexes */
      if (brush->plane_trim == 0) {
        brush->plane_trim = 0.5f;
      }

      /* same as smooth stroke off */
      if (brush->smooth_stroke_radius == 0) {
        brush->smooth_stroke_radius = 75;
      }

      /* will keep cursor in one spot */
      if (brush->smooth_stroke_radius == 1) {
        brush->smooth_stroke_factor = 0.9f;
      }

      /* same as dots */
      if (brush->rate == 0) {
        brush->rate = 0.1f;
      }

      /* New Settings */
      if (bmain->versionfile < 252 || (bmain->versionfile == 252 && bmain->subversionfile < 5)) {
        brush->flag |= BRUSH_SPACE_ATTEN;  // explicitly enable adaptive space

        /* spacing was originally in pixels, convert it to percentage for new version
         * size should not be zero due to sanity check above
         */
        brush->spacing = (int)(100 * ((float)brush->spacing) / ((float)brush->size));

        if (brush->add_col[0] == 0 && brush->add_col[1] == 0 && brush->add_col[2] == 0) {
          brush->add_col[0] = 1.00f;
          brush->add_col[1] = 0.39f;
          brush->add_col[2] = 0.39f;
        }

        if (brush->sub_col[0] == 0 && brush->sub_col[1] == 0 && brush->sub_col[2] == 0) {
          brush->sub_col[0] = 0.39f;
          brush->sub_col[1] = 0.39f;
          brush->sub_col[2] = 1.00f;
        }
      }
    }
  }

  /* GSOC Sculpt 2010 - Sanity check on Sculpt/Paint settings */
  if (bmain->versionfile < 253) {
    Scene *sce;
    for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
      if (sce->toolsettings->sculpt_paint_unified_alpha == 0) {
        sce->toolsettings->sculpt_paint_unified_alpha = 0.5f;
      }

      if (sce->toolsettings->sculpt_paint_unified_unprojected_radius == 0) {
        sce->toolsettings->sculpt_paint_unified_unprojected_radius = 0.125f;
      }

      if (sce->toolsettings->sculpt_paint_unified_size == 0) {
        sce->toolsettings->sculpt_paint_unified_size = 35;
      }
    }
  }

  if (bmain->versionfile < 253 || (bmain->versionfile == 253 && bmain->subversionfile < 1)) {
    Object *ob;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;

      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Smoke) {
          SmokeModifierData *smd = (SmokeModifierData *)md;

          if ((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
            smd->domain->vorticity = 2.0f;
            smd->domain->time_scale = 1.0f;

            if (!(smd->domain->flags & (1 << 4))) {
              continue;
            }

            /* delete old MOD_SMOKE_INITVELOCITY flag */
            smd->domain->flags &= ~(1 << 4);

            /* for now just add it to all flow objects in the scene */
            {
              Object *ob2;
              for (ob2 = bmain->objects.first; ob2; ob2 = ob2->id.next) {
                ModifierData *md2;
                for (md2 = ob2->modifiers.first; md2; md2 = md2->next) {
                  if (md2->type == eModifierType_Smoke) {
                    SmokeModifierData *smd2 = (SmokeModifierData *)md2;

                    if ((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
                      smd2->flow->flags |= MOD_SMOKE_FLOW_INITVELOCITY;
                    }
                  }
                }
              }
            }
          }
          else if ((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) {
            smd->flow->vel_multi = 1.0f;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 255 || (bmain->versionfile == 255 && bmain->subversionfile < 1)) {
    Brush *br;
    ParticleSettings *part;
    bScreen *sc;

    for (br = bmain->brushes.first; br; br = br->id.next) {
      if (br->ob_mode == 0) {
        br->ob_mode = OB_MODE_ALL_PAINT;
      }
    }

    for (part = bmain->particles.first; part; part = part->id.next) {
      if (part->boids) {
        part->boids->pitch = 1.0f;
      }

      part->flag &= ~PART_HAIR_REGROW; /* this was a deprecated flag before */
      part->kink_amp_clump = 1.f;      /* keep old files looking similar */
    }

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_INFO) {
            SpaceInfo *sinfo = (SpaceInfo *)sl;
            ARegion *ar;

            sinfo->rpt_mask = INFO_RPT_OP;

            for (ar = sa->regionbase.first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_WINDOW) {
                ar->v2d.scroll = (V2D_SCROLL_RIGHT);
                ar->v2d.align = V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y; /* align bottom left */
                ar->v2d.keepofs = V2D_LOCKOFS_X;
                ar->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM |
                                    V2D_KEEPASPECT);
                ar->v2d.keeptot = V2D_KEEPTOT_BOUNDS;
                ar->v2d.minzoom = ar->v2d.maxzoom = 1.0f;
              }
            }
          }
        }
      }
    }
  }

  if (bmain->versionfile < 255 || (bmain->versionfile == 255 && bmain->subversionfile < 3)) {
    Object *ob;

    /* ocean res is now squared, reset old ones - will be massive */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Ocean) {
          OceanModifierData *omd = (OceanModifierData *)md;
          omd->resolution = 7;
          omd->oceancache = NULL;
        }
      }
    }
  }

  if (bmain->versionfile < 256) {
    bScreen *sc;
    ScrArea *sa;
    Key *key;

    /* Fix for sample line scope initializing with no height */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      sa = sc->areabase.first;
      while (sa) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = (SpaceImage *)sl;
            if (sima->sample_line_hist.height == 0) {
              sima->sample_line_hist.height = 100;
            }
          }
        }
        sa = sa->next;
      }
    }

    /* old files could have been saved with slidermin = slidermax = 0.0, but the UI in
     * 2.4x would never reveal this to users as a dummy value always ended up getting used
     * instead
     */
    for (key = bmain->shapekeys.first; key; key = key->id.next) {
      KeyBlock *kb;

      for (kb = key->block.first; kb; kb = kb->next) {
        if (IS_EQF(kb->slidermin, kb->slidermax) && IS_EQF(kb->slidermax, 0.0f)) {
          kb->slidermax = kb->slidermin + 1.0f;
        }
      }
    }
  }

  if (bmain->versionfile < 256 || (bmain->versionfile == 256 && bmain->subversionfile < 1)) {
    /* fix for bones that didn't have arm_roll before */
    bArmature *arm;
    Bone *bone;
    Object *ob;

    for (arm = bmain->armatures.first; arm; arm = arm->id.next) {
      for (bone = arm->bonebase.first; bone; bone = bone->next) {
        do_version_bone_roll_256(bone);
      }
    }

    /* fix for objects which have zero dquat's
     * since this is multiplied with the quat rather than added */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (is_zero_v4(ob->dquat)) {
        unit_qt(ob->dquat);
      }
      if (is_zero_v3(ob->drotAxis) && ob->drotAngle == 0.0f) {
        unit_axis_angle(ob->drotAxis, &ob->drotAngle);
      }
    }
  }

  if (bmain->versionfile < 256 || (bmain->versionfile == 256 && bmain->subversionfile < 2)) {
    bNodeTree *ntree;
    bNode *node;
    bNodeSocket *sock, *gsock;
    bNodeLink *link;

    /* node sockets are not exposed automatically any more,
     * this mimics the old behavior by adding all unlinked sockets to groups.
     */
    for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
      /* this adds copies and links from all unlinked internal sockets to group inputs/outputs. */

      /* first make sure the own_index for new sockets is valid */
      for (node = ntree->nodes.first; node; node = node->next) {
        for (sock = node->inputs.first; sock; sock = sock->next) {
          if (sock->own_index >= ntree->cur_index) {
            ntree->cur_index = sock->own_index + 1;
          }
        }
        for (sock = node->outputs.first; sock; sock = sock->next) {
          if (sock->own_index >= ntree->cur_index) {
            ntree->cur_index = sock->own_index + 1;
          }
        }
      }

      /* add ntree->inputs/ntree->outputs sockets for all unlinked sockets in the group tree. */
      for (node = ntree->nodes.first; node; node = node->next) {
        for (sock = node->inputs.first; sock; sock = sock->next) {
          if (!sock->link && !nodeSocketIsHidden(sock)) {

            gsock = do_versions_node_group_add_socket_2_56_2(
                ntree, sock->name, sock->type, SOCK_IN);

            /* initialize the default socket value */
            copy_v4_v4(gsock->ns.vec, sock->ns.vec);

            /* XXX nodeAddLink does not work with incomplete (node==NULL) links any longer,
             * have to create these directly here.
             * These links are updated again in subsequent do_version!
             */
            link = MEM_callocN(sizeof(bNodeLink), "link");
            BLI_addtail(&ntree->links, link);
            link->fromnode = NULL;
            link->fromsock = gsock;
            link->tonode = node;
            link->tosock = sock;
            ntree->update |= NTREE_UPDATE_LINKS;

            sock->link = link;
          }
        }
        for (sock = node->outputs.first; sock; sock = sock->next) {
          if (nodeCountSocketLinks(ntree, sock) == 0 && !nodeSocketIsHidden(sock)) {
            gsock = do_versions_node_group_add_socket_2_56_2(
                ntree, sock->name, sock->type, SOCK_OUT);

            /* initialize the default socket value */
            copy_v4_v4(gsock->ns.vec, sock->ns.vec);

            /* XXX nodeAddLink does not work with incomplete (node==NULL) links any longer,
             * have to create these directly here.
             * These links are updated again in subsequent do_version!
             */
            link = MEM_callocN(sizeof(bNodeLink), "link");
            BLI_addtail(&ntree->links, link);
            link->fromnode = node;
            link->fromsock = sock;
            link->tonode = NULL;
            link->tosock = gsock;
            ntree->update |= NTREE_UPDATE_LINKS;

            gsock->link = link;
          }
        }
      }

      /* External group node socket need to adjust their own_index to point at
       * associated 'ntree' inputs/outputs internal sockets. This happens in
       * do_versions_after_linking_250, after lib linking. */
    }
  }

  if (bmain->versionfile < 256 || (bmain->versionfile == 256 && bmain->subversionfile < 3)) {
    bScreen *sc;
    Brush *brush;
    Object *ob;
    ParticleSettings *part;

    /* redraws flag in SpaceTime has been moved to Screen level */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      if (sc->redraws_flag == 0) {
        /* just initialize to default? */
        /* XXX: we could also have iterated through areas,
         * and taken them from the first timeline available... */
        sc->redraws_flag = TIME_ALL_3D_WIN | TIME_ALL_ANIM_WIN;
      }
    }

    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      if (brush->height == 0) {
        brush->height = 0.4f;
      }
    }

    /* replace 'rim material' option for in offset*/
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Solidify) {
          SolidifyModifierData *smd = (SolidifyModifierData *)md;
          if (smd->flag & MOD_SOLIDIFY_RIM_MATERIAL) {
            smd->mat_ofs_rim = 1;
            smd->flag &= ~MOD_SOLIDIFY_RIM_MATERIAL;
          }
        }
      }
    }

    /* particle draw color from material */
    for (part = bmain->particles.first; part; part = part->id.next) {
      if (part->draw & PART_DRAW_MAT_COL) {
        part->draw_col = PART_DRAW_COL_MAT;
      }
    }
  }

  if (bmain->versionfile < 256 || (bmain->versionfile == 256 && bmain->subversionfile < 6)) {
    Mesh *me;

    for (me = bmain->meshes.first; me; me = me->id.next) {
      BKE_mesh_calc_normals_tessface(me->mvert, me->totvert, me->mface, me->totface, NULL);
    }
  }

  if (bmain->versionfile < 256 || (bmain->versionfile == 256 && bmain->subversionfile < 2)) {
    /* update blur area sizes from 0..1 range to 0..100 percentage */
    Scene *scene;
    bNode *node;
    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->nodetree) {
        for (node = scene->nodetree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_BLUR) {
            NodeBlurData *nbd = node->storage;
            nbd->percentx *= 100.0f;
            nbd->percenty *= 100.0f;
          }
        }
      }
    }
  }

  if (bmain->versionfile < 258 || (bmain->versionfile == 258 && bmain->subversionfile < 1)) {
    /* screen view2d settings were not properly initialized T27164.
     * v2d->scroll caused the bug but best reset other values too
     * which are in old blend files only.
     * Need to make less ugly - possibly an iterator? */
    bScreen *screen;

    for (screen = bmain->screens.first; screen; screen = screen->id.next) {
      ScrArea *sa;
      /* add regions */
      for (sa = screen->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl = sa->spacedata.first;
        if (sl->spacetype == SPACE_IMAGE) {
          ARegion *ar;
          for (ar = sa->regionbase.first; ar; ar = ar->next) {
            if (ar->regiontype == RGN_TYPE_WINDOW) {
              View2D *v2d = &ar->v2d;
              v2d->minzoom = v2d->maxzoom = v2d->scroll = v2d->keeptot = v2d->keepzoom =
                  v2d->keepofs = v2d->align = 0;
            }
          }
        }

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_IMAGE) {
            ARegion *ar;
            for (ar = sl->regionbase.first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_WINDOW) {
                View2D *v2d = &ar->v2d;
                v2d->minzoom = v2d->maxzoom = v2d->scroll = v2d->keeptot = v2d->keepzoom =
                    v2d->keepofs = v2d->align = 0;
              }
            }
          }
        }
      }
    }

    {
      ParticleSettings *part;
      for (part = bmain->particles.first; part; part = part->id.next) {
        /* Initialize particle billboard scale */
        part->bb_size[0] = part->bb_size[1] = 1.0f;
      }
    }
  }

  if (bmain->versionfile < 259 || (bmain->versionfile == 259 && bmain->subversionfile < 1)) {
    {
      Scene *scene;
      Sequence *seq;

      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->r.ffcodecdata.audio_channels = 2;
        scene->audio.volume = 1.0f;
        SEQ_BEGIN (scene->ed, seq) {
          seq->pitch = 1.0f;
        }
        SEQ_END;
      }
    }

    {
      bScreen *screen;
      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        ScrArea *sa;

        /* add regions */
        for (sa = screen->areabase.first; sa; sa = sa->next) {
          SpaceLink *sl = sa->spacedata.first;
          if (sl->spacetype == SPACE_SEQ) {
            ARegion *ar;
            for (ar = sa->regionbase.first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_WINDOW) {
                if (ar->v2d.min[1] == 4.0f) {
                  ar->v2d.min[1] = 0.5f;
                }
              }
            }
          }
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_SEQ) {
              ARegion *ar;
              for (ar = sl->regionbase.first; ar; ar = ar->next) {
                if (ar->regiontype == RGN_TYPE_WINDOW) {
                  if (ar->v2d.min[1] == 4.0f) {
                    ar->v2d.min[1] = 0.5f;
                  }
                }
              }
            }
          }
        }
      }
    }

    {
      /* Make "auto-clamped" handles a per-keyframe setting instead of per-FCurve
       *
       * We're only patching F-Curves in Actions here, since it is assumed that most
       * drivers out there won't be using this (and if they are, they're in the minority).
       * While we should aim to fix everything ideally, in practice it's far too hard
       * to get to every animdata block, not to mention the performance hit that'd have
       */
      bAction *act;
      FCurve *fcu;

      for (act = bmain->actions.first; act; act = act->id.next) {
        for (fcu = act->curves.first; fcu; fcu = fcu->next) {
          BezTriple *bezt;
          uint i = 0;

          /* only need to touch curves that had this flag set */
          if ((fcu->flag & FCURVE_AUTO_HANDLES) == 0) {
            continue;
          }
          if ((fcu->totvert == 0) || (fcu->bezt == NULL)) {
            continue;
          }

          /* only change auto-handles to auto-clamped */
          for (bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
            if (bezt->h1 == HD_AUTO) {
              bezt->h1 = HD_AUTO_ANIM;
            }
            if (bezt->h2 == HD_AUTO) {
              bezt->h2 = HD_AUTO_ANIM;
            }
          }

          fcu->flag &= ~FCURVE_AUTO_HANDLES;
        }
      }
    }
  }

  if (bmain->versionfile < 259 || (bmain->versionfile == 259 && bmain->subversionfile < 2)) {
    {
      /* Convert default socket values from bNodeStack */
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        bNode *node;
        bNodeSocket *sock;

        for (node = ntree->nodes.first; node; node = node->next) {
          for (sock = node->inputs.first; sock; sock = sock->next) {
            do_versions_socket_default_value_259(sock);
          }
          for (sock = node->outputs.first; sock; sock = sock->next) {
            do_versions_socket_default_value_259(sock);
          }
        }

        for (sock = ntree->inputs.first; sock; sock = sock->next) {
          do_versions_socket_default_value_259(sock);
        }
        for (sock = ntree->outputs.first; sock; sock = sock->next) {
          do_versions_socket_default_value_259(sock);
        }

        ntree->update |= NTREE_UPDATE;
      }
      FOREACH_NODETREE_END;
    }

    {
      /* Initialize group tree nodetypes.
       * These are used to distinguish tree types and
       * associate them with specific node types for polling.
       */
      bNodeTree *ntree;
      /* all node trees in bmain->nodetree are considered groups */
      for (ntree = bmain->nodetrees.first; ntree; ntree = ntree->id.next) {
        ntree->nodetype = NODE_GROUP;
      }
    }
  }

  if (bmain->versionfile < 259 || (bmain->versionfile == 259 && bmain->subversionfile < 4)) {
    {
      /* Adaptive time step for particle systems */
      ParticleSettings *part;
      for (part = bmain->particles.first; part; part = part->id.next) {
        part->courant_target = 0.2f;
        part->time_flag &= ~PART_TIME_AUTOSF;
      }
    }
  }
}

/* updates group node socket identifier so that
 * external links to/from the group node are preserved.
 */
static void lib_node_do_versions_group_indices(bNode *gnode)
{
  bNodeTree *ngroup = (bNodeTree *)gnode->id;
  bNodeSocket *sock;
  bNodeLink *link;

  for (sock = gnode->outputs.first; sock; sock = sock->next) {
    int old_index = sock->to_index;

    for (link = ngroup->links.first; link; link = link->next) {
      if (link->tonode == NULL && link->fromsock->own_index == old_index) {
        strcpy(sock->identifier, link->fromsock->identifier);
        /* deprecated */
        sock->own_index = link->fromsock->own_index;
        sock->to_index = 0;
        sock->groupsock = NULL;
      }
    }
  }
  for (sock = gnode->inputs.first; sock; sock = sock->next) {
    int old_index = sock->to_index;

    for (link = ngroup->links.first; link; link = link->next) {
      if (link->fromnode == NULL && link->tosock->own_index == old_index) {
        strcpy(sock->identifier, link->tosock->identifier);
        /* deprecated */
        sock->own_index = link->tosock->own_index;
        sock->to_index = 0;
        sock->groupsock = NULL;
      }
    }
  }
}

void do_versions_after_linking_250(Main *bmain)
{
  if (bmain->versionfile < 256 || (bmain->versionfile == 256 && bmain->subversionfile < 2)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      /* updates external links for all group nodes in a tree */
      bNode *node;
      for (node = ntree->nodes.first; node; node = node->next) {
        if (node->type == NODE_GROUP) {
          bNodeTree *ngroup = (bNodeTree *)node->id;
          if (ngroup) {
            lib_node_do_versions_group_indices(node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }
}

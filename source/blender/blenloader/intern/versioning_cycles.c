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

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <float.h>
#include <string.h>

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_color_types.h"
#include "DNA_light_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_camera_types.h"

#include "BKE_colortools.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "IMB_colormanagement.h"

#include "BLO_readfile.h"
#include "readfile.h"

static float *cycles_node_socket_float_value(bNodeSocket *socket)
{
  bNodeSocketValueFloat *socket_data = socket->default_value;
  return &socket_data->value;
}

static float *cycles_node_socket_rgba_value(bNodeSocket *socket)
{
  bNodeSocketValueRGBA *socket_data = socket->default_value;
  return socket_data->value;
}

static IDProperty *cycles_properties_from_ID(ID *id)
{
  IDProperty *idprop = IDP_GetProperties(id, false);
  return (idprop) ? IDP_GetPropertyTypeFromGroup(idprop, "cycles", IDP_GROUP) : NULL;
}

static float cycles_property_float(IDProperty *idprop, const char *name, float default_value)
{
  IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_FLOAT);
  return (prop) ? IDP_Float(prop) : default_value;
}

static float cycles_property_int(IDProperty *idprop, const char *name, int default_value)
{
  IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_INT);
  return (prop) ? IDP_Int(prop) : default_value;
}

static bool cycles_property_boolean(IDProperty *idprop, const char *name, bool default_value)
{
  IDProperty *prop = IDP_GetPropertyTypeFromGroup(idprop, name, IDP_INT);
  return (prop) ? IDP_Int(prop) : default_value;
}

static void displacement_node_insert(bNodeTree *ntree)
{
  bool need_update = false;

  /* Iterate backwards from end so we don't encounter newly added links. */
  bNodeLink *prevlink;
  for (bNodeLink *link = ntree->links.last; link; link = prevlink) {
    prevlink = link->prev;

    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNodeSocket *fromsock = link->fromsock;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(tonode->type == SH_NODE_OUTPUT_MATERIAL && fromnode->type != SH_NODE_DISPLACEMENT &&
          STREQ(tosock->identifier, "Displacement"))) {
      continue;
    }

    /* Replace link with displacement node. */
    nodeRemLink(ntree, link);

    /* Add displacement node. */
    bNode *node = nodeAddStaticNode(NULL, ntree, SH_NODE_DISPLACEMENT);
    node->locx = 0.5f * (fromnode->locx + tonode->locx);
    node->locy = 0.5f * (fromnode->locy + tonode->locy);

    bNodeSocket *scale_socket = nodeFindSocket(node, SOCK_IN, "Scale");
    bNodeSocket *midlevel_socket = nodeFindSocket(node, SOCK_IN, "Midlevel");
    bNodeSocket *height_socket = nodeFindSocket(node, SOCK_IN, "Height");
    bNodeSocket *displacement_socket = nodeFindSocket(node, SOCK_OUT, "Displacement");

    /* Set default values for compatibility. */
    *cycles_node_socket_float_value(scale_socket) = 0.1f;
    *cycles_node_socket_float_value(midlevel_socket) = 0.0f;

    /* Link to input and material output node. */
    nodeAddLink(ntree, fromnode, fromsock, node, height_socket);
    nodeAddLink(ntree, node, displacement_socket, tonode, tosock);

    need_update = true;
  }

  if (need_update) {
    ntreeUpdateTree(NULL, ntree);
  }
}

static void displacement_principled_nodes(bNode *node)
{
  if (node->type == SH_NODE_DISPLACEMENT) {
    if (node->custom1 != SHD_SPACE_WORLD) {
      node->custom1 = SHD_SPACE_OBJECT;
    }
  }
  else if (node->type == SH_NODE_BSDF_PRINCIPLED) {
    if (node->custom2 != SHD_SUBSURFACE_RANDOM_WALK) {
      node->custom2 = SHD_SUBSURFACE_BURLEY;
    }
  }
}

static bool node_has_roughness(bNode *node)
{
  return ELEM(node->type,
              SH_NODE_BSDF_ANISOTROPIC,
              SH_NODE_BSDF_GLASS,
              SH_NODE_BSDF_GLOSSY,
              SH_NODE_BSDF_REFRACTION);
}

static void square_roughness_node_insert(bNodeTree *ntree)
{
  bool need_update = false;

  /* Update default values */
  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    if (node_has_roughness(node)) {
      bNodeSocket *roughness_input = nodeFindSocket(node, SOCK_IN, "Roughness");
      float *roughness_value = cycles_node_socket_float_value(roughness_input);
      *roughness_value = sqrtf(max_ff(*roughness_value, 0.0f));
    }
  }

  /* Iterate backwards from end so we don't encounter newly added links. */
  bNodeLink *prevlink;
  for (bNodeLink *link = ntree->links.last; link; link = prevlink) {
    prevlink = link->prev;

    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNodeSocket *fromsock = link->fromsock;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(node_has_roughness(tonode) && STREQ(tosock->identifier, "Roughness"))) {
      continue;
    }

    /* Replace links with sqrt node */
    nodeRemLink(ntree, link);

    /* Add sqrt node. */
    bNode *node = nodeAddStaticNode(NULL, ntree, SH_NODE_MATH);
    node->custom1 = NODE_MATH_POW;
    node->locx = 0.5f * (fromnode->locx + tonode->locx);
    node->locy = 0.5f * (fromnode->locy + tonode->locy);

    /* Link to input and material output node. */
    *cycles_node_socket_float_value(node->inputs.last) = 0.5f;
    nodeAddLink(ntree, fromnode, fromsock, node, node->inputs.first);
    nodeAddLink(ntree, node, node->outputs.first, tonode, tosock);

    need_update = true;
  }

  if (need_update) {
    ntreeUpdateTree(NULL, ntree);
  }
}

static void mapping_node_order_flip(bNode *node)
{
  /* Flip euler order of mapping shader node */
  if (node->type == SH_NODE_MAPPING) {
    TexMapping *texmap = node->storage;

    float quat[4];
    eulO_to_quat(quat, texmap->rot, EULER_ORDER_ZYX);
    quat_to_eulO(texmap->rot, EULER_ORDER_XYZ, quat);
  }
}

static void vector_curve_node_remap(bNode *node)
{
  /* Remap values of vector curve node from normalized to absolute values */
  if (node->type == SH_NODE_CURVE_VEC) {
    CurveMapping *mapping = node->storage;
    mapping->flag &= ~CUMA_DO_CLIP;

    for (int curve_index = 0; curve_index < CM_TOT; curve_index++) {
      CurveMap *cm = &mapping->cm[curve_index];
      if (cm->curve) {
        for (int i = 0; i < cm->totpoint; i++) {
          cm->curve[i].x = (cm->curve[i].x * 2.0f) - 1.0f;
          cm->curve[i].y = (cm->curve[i].y - 0.5f) * 2.0f;
        }
      }
    }

    curvemapping_changed_all(mapping);
  }
}

static void ambient_occlusion_node_relink(bNodeTree *ntree)
{
  bool need_update = false;

  /* Set default values. */
  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    if (node->type == SH_NODE_AMBIENT_OCCLUSION) {
      node->custom1 = 1; /* samples */
      node->custom2 &= ~SHD_AO_LOCAL;

      bNodeSocket *distance_socket = nodeFindSocket(node, SOCK_IN, "Distance");
      *cycles_node_socket_float_value(distance_socket) = 0.0f;
    }
  }

  /* Iterate backwards from end so we don't encounter newly added links. */
  bNodeLink *prevlink;
  for (bNodeLink *link = ntree->links.last; link; link = prevlink) {
    prevlink = link->prev;

    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(fromnode->type == SH_NODE_AMBIENT_OCCLUSION)) {
      continue;
    }

    /* Replace links with color socket. */
    nodeRemLink(ntree, link);
    bNodeSocket *color_socket = nodeFindSocket(fromnode, SOCK_OUT, "Color");
    nodeAddLink(ntree, fromnode, color_socket, tonode, tosock);

    need_update = true;
  }

  if (need_update) {
    ntreeUpdateTree(NULL, ntree);
  }
}

static void image_node_colorspace(bNode *node)
{
  if (node->id == NULL) {
    return;
  }

  int color_space;
  if (node->type == SH_NODE_TEX_IMAGE) {
    NodeTexImage *tex = node->storage;
    color_space = tex->color_space;
  }
  else if (node->type == SH_NODE_TEX_ENVIRONMENT) {
    NodeTexEnvironment *tex = node->storage;
    color_space = tex->color_space;
  }
  else {
    return;
  }

  const int SHD_COLORSPACE_NONE = 0;
  Image *image = (Image *)node->id;
  if (color_space == SHD_COLORSPACE_NONE) {
    STRNCPY(image->colorspace_settings.name,
            IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DATA));
  }
}

static void light_emission_node_to_energy(Light *light, float *energy, float color[3])
{
  *energy = 1.0;
  copy_v3_fl(color, 1.0f);

  /* If nodetree has animation or drivers, don't try to convert. */
  bNodeTree *ntree = light->nodetree;
  if (ntree == NULL || ntree->adt) {
    return;
  }

  /* Find emission node */
  bNode *output_node = ntreeShaderOutputNode(ntree, SHD_OUTPUT_CYCLES);
  if (output_node == NULL) {
    return;
  }

  bNode *emission_node = NULL;
  for (bNodeLink *link = ntree->links.first; link; link = link->next) {
    if (link->tonode == output_node && link->fromnode->type == SH_NODE_EMISSION) {
      emission_node = link->fromnode;
      break;
    }
  }

  if (emission_node == NULL) {
    return;
  }

  /* Don't convert if anything is linked */
  bNodeSocket *strength_socket = nodeFindSocket(emission_node, SOCK_IN, "Strength");
  bNodeSocket *color_socket = nodeFindSocket(emission_node, SOCK_IN, "Color");

  if ((strength_socket->flag & SOCK_IN_USE) || (color_socket->flag & SOCK_IN_USE)) {
    return;
  }

  float *strength_value = cycles_node_socket_float_value(strength_socket);
  float *color_value = cycles_node_socket_rgba_value(color_socket);

  *energy = *strength_value;
  copy_v3_v3(color, color_value);

  *strength_value = 1.0f;
  copy_v4_fl(color_value, 1.0f);
  light->use_nodes = false;
}

static void light_emission_unify(Light *light, const char *engine)
{
  if (light->type != LA_SUN) {
    light->energy *= 100.0f;
  }

  /* Attempt to extract constant energy and color from nodes. */
  bool use_nodes = light->use_nodes;
  float energy, color[3];
  light_emission_node_to_energy(light, &energy, color);

  if (STREQ(engine, "CYCLES")) {
    if (use_nodes) {
      /* Energy extracted from nodes */
      light->energy = energy;
      copy_v3_v3(&light->r, color);
    }
    else {
      /* Default cycles multipliers if there are no nodes */
      if (light->type == LA_SUN) {
        light->energy = 1.0f;
      }
      else {
        light->energy = 100.0f;
      }
    }
  }
  else {
    /* Disable nodes if scene was configured for Eevee */
    light->use_nodes = false;
  }
}

void blo_do_versions_cycles(FileData *UNUSED(fd), Library *UNUSED(lib), Main *bmain)
{
  /* Particle shape shared with Eevee. */
  if (!MAIN_VERSION_ATLEAST(bmain, 280, 16)) {
    for (ParticleSettings *part = bmain->particles.first; part; part = part->id.next) {
      IDProperty *cpart = cycles_properties_from_ID(&part->id);

      if (cpart) {
        part->shape = cycles_property_float(cpart, "shape", 0.0);
        part->rad_root = cycles_property_float(cpart, "root_width", 1.0);
        part->rad_tip = cycles_property_float(cpart, "tip_width", 0.0);
        part->rad_scale = cycles_property_float(cpart, "radius_scale", 0.01);
        if (cycles_property_boolean(cpart, "use_closetip", true)) {
          part->shape_flag |= PART_SHAPE_CLOSE_TIP;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 68)) {
    /* Unify Cycles and Eevee film transparency. */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (STREQ(scene->r.engine, RE_engine_id_CYCLES)) {
        IDProperty *cscene = cycles_properties_from_ID(&scene->id);
        if (cscene) {
          bool cycles_film_transparency = cycles_property_boolean(
              cscene, "film_transparent", false);
          scene->r.alphamode = cycles_film_transparency ? R_ALPHAPREMUL : R_ADDSKY;
        }
      }
    }
  }
}

void do_versions_after_linking_cycles(Main *bmain)
{
  if (!MAIN_VERSION_ATLEAST(bmain, 280, 66)) {
    /* Shader node tree changes. After lib linking so we have all the typeinfo
     * pointers and updated sockets and we can use the high level node API to
     * manipulate nodes. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_SHADER) {
        continue;
      }

      if (!MAIN_VERSION_ATLEAST(bmain, 273, 5)) {
        /* Euler order was ZYX in previous versions. */
        for (bNode *node = ntree->nodes.first; node; node = node->next) {
          mapping_node_order_flip(node);
        }
      }

      if (!MAIN_VERSION_ATLEAST(bmain, 276, 6)) {
        for (bNode *node = ntree->nodes.first; node; node = node->next) {
          vector_curve_node_remap(node);
        }
      }

      if (!MAIN_VERSION_ATLEAST(bmain, 279, 2) ||
          (MAIN_VERSION_ATLEAST(bmain, 280, 0) && !MAIN_VERSION_ATLEAST(bmain, 280, 4))) {
        displacement_node_insert(ntree);
      }

      if (!MAIN_VERSION_ATLEAST(bmain, 279, 3)) {
        for (bNode *node = ntree->nodes.first; node; node = node->next) {
          displacement_principled_nodes(node);
        }
      }

      if (!MAIN_VERSION_ATLEAST(bmain, 279, 4) ||
          (MAIN_VERSION_ATLEAST(bmain, 280, 0) && !MAIN_VERSION_ATLEAST(bmain, 280, 5))) {
        /* Switch to squared roughness convention */
        square_roughness_node_insert(ntree);
      }

      if (!MAIN_VERSION_ATLEAST(bmain, 279, 5)) {
        ambient_occlusion_node_relink(ntree);
      }

      if (!MAIN_VERSION_ATLEAST(bmain, 280, 66)) {
        for (bNode *node = ntree->nodes.first; node; node = node->next) {
          image_node_colorspace(node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 64)) {
    /* Unfiy Cycles and Eevee settings. */
    Scene *scene = bmain->scenes.first;
    const char *engine = (scene) ? scene->r.engine : "CYCLES";

    for (Light *light = bmain->lights.first; light; light = light->id.next) {
      light_emission_unify(light, engine);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 69)) {
    /* Unify Cycles and Eevee depth of field. */
    Scene *scene = bmain->scenes.first;
    const char *engine = (scene) ? scene->r.engine : "CYCLES";

    if (STREQ(engine, RE_engine_id_CYCLES)) {
      for (Camera *camera = bmain->cameras.first; camera; camera = camera->id.next) {
        IDProperty *ccamera = cycles_properties_from_ID(&camera->id);
        if (ccamera) {
          const bool is_fstop = cycles_property_int(ccamera, "aperture_type", 0) == 1;

          camera->dof.aperture_fstop = cycles_property_float(ccamera, "aperture_fstop", 5.6f);
          camera->dof.aperture_blades = cycles_property_int(ccamera, "aperture_blades", 0);
          camera->dof.aperture_rotation = cycles_property_float(ccamera, "aperture_rotation", 0.0);
          camera->dof.aperture_ratio = cycles_property_float(ccamera, "aperture_ratio", 1.0f);
          camera->dof.flag |= CAM_DOF_ENABLED;

          float aperture_size = cycles_property_float(ccamera, "aperture_size", 0.0f);

          if (is_fstop) {
            continue;
          }
          else if (aperture_size > 0.0f) {
            if (camera->type == CAM_ORTHO) {
              camera->dof.aperture_fstop = 1.0f / (2.0f * aperture_size);
            }
            else {
              camera->dof.aperture_fstop = (camera->lens * 1e-3f) / (2.0f * aperture_size);
            }

            continue;
          }
        }

        /* No depth of field, set default settings. */
        camera->dof.aperture_fstop = 2.8f;
        camera->dof.aperture_blades = 0;
        camera->dof.aperture_rotation = 0.0f;
        camera->dof.aperture_ratio = 1.0f;
        camera->dof.flag &= ~CAM_DOF_ENABLED;
      }
    }
  }
}

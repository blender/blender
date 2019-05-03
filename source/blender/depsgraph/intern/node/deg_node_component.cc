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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node_component.h"

#include <stdio.h>
#include <cstring> /* required for STREQ later on. */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

extern "C" {
#include "DNA_object_types.h"

#include "BKE_action.h"
} /* extern "C" */

#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_operation.h"

namespace DEG {

/* *********** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

ComponentNode::OperationIDKey::OperationIDKey()
    : opcode(OperationCode::OPERATION), name(""), name_tag(-1)
{
}

ComponentNode::OperationIDKey::OperationIDKey(OperationCode opcode)
    : opcode(opcode), name(""), name_tag(-1)
{
}

ComponentNode::OperationIDKey::OperationIDKey(OperationCode opcode, const char *name, int name_tag)
    : opcode(opcode), name(name), name_tag(name_tag)
{
}

string ComponentNode::OperationIDKey::identifier() const
{
  const string codebuf = to_string(static_cast<int>(opcode));
  return "OperationIDKey(" + codebuf + ", " + name + ")";
}

bool ComponentNode::OperationIDKey::operator==(const OperationIDKey &other) const
{
  return (opcode == other.opcode) && (STREQ(name, other.name)) && (name_tag == other.name_tag);
}

static unsigned int comp_node_hash_key(const void *key_v)
{
  const ComponentNode::OperationIDKey *key =
      reinterpret_cast<const ComponentNode::OperationIDKey *>(key_v);
  int opcode_as_int = static_cast<int>(key->opcode);
  return BLI_ghashutil_combine_hash(BLI_ghashutil_uinthash(opcode_as_int),
                                    BLI_ghashutil_strhash_p(key->name));
}

static bool comp_node_hash_key_cmp(const void *a, const void *b)
{
  const ComponentNode::OperationIDKey *key_a =
      reinterpret_cast<const ComponentNode::OperationIDKey *>(a);
  const ComponentNode::OperationIDKey *key_b =
      reinterpret_cast<const ComponentNode::OperationIDKey *>(b);
  return !(*key_a == *key_b);
}

static void comp_node_hash_key_free(void *key_v)
{
  typedef ComponentNode::OperationIDKey OperationIDKey;
  OperationIDKey *key = reinterpret_cast<OperationIDKey *>(key_v);
  OBJECT_GUARDED_DELETE(key, OperationIDKey);
}

static void comp_node_hash_value_free(void *value_v)
{
  OperationNode *op_node = reinterpret_cast<OperationNode *>(value_v);
  OBJECT_GUARDED_DELETE(op_node, OperationNode);
}

ComponentNode::ComponentNode()
    : entry_operation(NULL), exit_operation(NULL), affects_directly_visible(false)
{
  operations_map = BLI_ghash_new(comp_node_hash_key, comp_node_hash_key_cmp, "Depsgraph id hash");
}

/* Initialize 'component' node - from pointer data given */
void ComponentNode::init(const ID * /*id*/, const char * /*subdata*/)
{
  /* hook up eval context? */
  // XXX: maybe this needs a special API?
}

/* Free 'component' node */
ComponentNode::~ComponentNode()
{
  clear_operations();
  if (operations_map != NULL) {
    BLI_ghash_free(operations_map, comp_node_hash_key_free, comp_node_hash_value_free);
  }
}

string ComponentNode::identifier() const
{
  const string idname = this->owner->name;
  const string typebuf = "" + to_string(static_cast<int>(type)) + ")";
  return typebuf + name + " : " + idname +
         "( affects_directly_visible: " + (affects_directly_visible ? "true" : "false") + ")";
}

OperationNode *ComponentNode::find_operation(OperationIDKey key) const
{
  OperationNode *node = NULL;
  if (operations_map != NULL) {
    node = (OperationNode *)BLI_ghash_lookup(operations_map, &key);
  }
  else {
    for (OperationNode *op_node : operations) {
      if (op_node->opcode == key.opcode && op_node->name_tag == key.name_tag &&
          STREQ(op_node->name.c_str(), key.name)) {
        node = op_node;
        break;
      }
    }
  }
  return node;
}

OperationNode *ComponentNode::find_operation(OperationCode opcode,
                                             const char *name,
                                             int name_tag) const
{
  OperationIDKey key(opcode, name, name_tag);
  return find_operation(key);
}

OperationNode *ComponentNode::get_operation(OperationIDKey key) const
{
  OperationNode *node = find_operation(key);
  if (node == NULL) {
    fprintf(stderr,
            "%s: find_operation(%s) failed\n",
            this->identifier().c_str(),
            key.identifier().c_str());
    BLI_assert(!"Request for non-existing operation, should not happen");
    return NULL;
  }
  return node;
}

OperationNode *ComponentNode::get_operation(OperationCode opcode,
                                            const char *name,
                                            int name_tag) const
{
  OperationIDKey key(opcode, name, name_tag);
  return get_operation(key);
}

bool ComponentNode::has_operation(OperationIDKey key) const
{
  return find_operation(key) != NULL;
}

bool ComponentNode::has_operation(OperationCode opcode, const char *name, int name_tag) const
{
  OperationIDKey key(opcode, name, name_tag);
  return has_operation(key);
}

OperationNode *ComponentNode::add_operation(const DepsEvalOperationCb &op,
                                            OperationCode opcode,
                                            const char *name,
                                            int name_tag)
{
  OperationNode *op_node = find_operation(opcode, name, name_tag);
  if (!op_node) {
    DepsNodeFactory *factory = type_get_factory(NodeType::OPERATION);
    op_node = (OperationNode *)factory->create_node(this->owner->id_orig, "", name);

    /* register opnode in this component's operation set */
    OperationIDKey *key = OBJECT_GUARDED_NEW(OperationIDKey, opcode, name, name_tag);
    BLI_ghash_insert(operations_map, key, op_node);

    /* set backlink */
    op_node->owner = this;
  }
  else {
    fprintf(stderr,
            "add_operation: Operation already exists - %s has %s at %p\n",
            this->identifier().c_str(),
            op_node->identifier().c_str(),
            op_node);
    BLI_assert(!"Should not happen!");
  }

  /* attach extra data */
  op_node->evaluate = op;
  op_node->opcode = opcode;
  op_node->name = name;
  op_node->name_tag = name_tag;

  return op_node;
}

void ComponentNode::set_entry_operation(OperationNode *op_node)
{
  BLI_assert(entry_operation == NULL);
  entry_operation = op_node;
}

void ComponentNode::set_exit_operation(OperationNode *op_node)
{
  BLI_assert(exit_operation == NULL);
  exit_operation = op_node;
}

void ComponentNode::clear_operations()
{
  if (operations_map != NULL) {
    BLI_ghash_clear(operations_map, comp_node_hash_key_free, comp_node_hash_value_free);
  }
  for (OperationNode *op_node : operations) {
    OBJECT_GUARDED_DELETE(op_node, OperationNode);
  }
  operations.clear();
}

void ComponentNode::tag_update(Depsgraph *graph, eUpdateSource source)
{
  OperationNode *entry_op = get_entry_operation();
  if (entry_op != NULL && entry_op->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
    return;
  }
  for (OperationNode *op_node : operations) {
    op_node->tag_update(graph, source);
  }
  // It is possible that tag happens before finalization.
  if (operations_map != NULL) {
    GHASH_FOREACH_BEGIN (OperationNode *, op_node, operations_map) {
      op_node->tag_update(graph, source);
    }
    GHASH_FOREACH_END();
  }
}

OperationNode *ComponentNode::get_entry_operation()
{
  if (entry_operation) {
    return entry_operation;
  }
  else if (operations_map != NULL && BLI_ghash_len(operations_map) == 1) {
    OperationNode *op_node = NULL;
    /* TODO(sergey): This is somewhat slow. */
    GHASH_FOREACH_BEGIN (OperationNode *, tmp, operations_map) {
      op_node = tmp;
    }
    GHASH_FOREACH_END();
    /* Cache for the subsequent usage. */
    entry_operation = op_node;
    return op_node;
  }
  else if (operations.size() == 1) {
    return operations[0];
  }
  return NULL;
}

OperationNode *ComponentNode::get_exit_operation()
{
  if (exit_operation) {
    return exit_operation;
  }
  else if (operations_map != NULL && BLI_ghash_len(operations_map) == 1) {
    OperationNode *op_node = NULL;
    /* TODO(sergey): This is somewhat slow. */
    GHASH_FOREACH_BEGIN (OperationNode *, tmp, operations_map) {
      op_node = tmp;
    }
    GHASH_FOREACH_END();
    /* Cache for the subsequent usage. */
    exit_operation = op_node;
    return op_node;
  }
  else if (operations.size() == 1) {
    return operations[0];
  }
  return NULL;
}

void ComponentNode::finalize_build(Depsgraph * /*graph*/)
{
  operations.reserve(BLI_ghash_len(operations_map));
  GHASH_FOREACH_BEGIN (OperationNode *, op_node, operations_map) {
    operations.push_back(op_node);
  }
  GHASH_FOREACH_END();
  BLI_ghash_free(operations_map, comp_node_hash_key_free, NULL);
  operations_map = NULL;
}

/* Bone Component ========================================= */

/* Initialize 'bone component' node - from pointer data given */
void BoneComponentNode::init(const ID *id, const char *subdata)
{
  /* generic component-node... */
  ComponentNode::init(id, subdata);

  /* name of component comes is bone name */
  /* TODO(sergey): This sets name to an empty string because subdata is
   * empty. Is it a bug? */
  // this->name = subdata;

  /* bone-specific node data */
  Object *object = (Object *)id;
  this->pchan = BKE_pose_channel_find_name(object->pose, subdata);
}

/* Register all components. =============================== */

DEG_COMPONENT_NODE_DEFINE(Animation, ANIMATION, ID_RECALC_ANIMATION);
/* TODO(sergey): Is this a correct tag? */
DEG_COMPONENT_NODE_DEFINE(BatchCache, BATCH_CACHE, ID_RECALC_SHADING);
DEG_COMPONENT_NODE_DEFINE(Bone, BONE, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(Cache, CACHE, 0);
DEG_COMPONENT_NODE_DEFINE(CopyOnWrite, COPY_ON_WRITE, ID_RECALC_COPY_ON_WRITE);
DEG_COMPONENT_NODE_DEFINE(Geometry, GEOMETRY, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(LayerCollections, LAYER_COLLECTIONS, 0);
DEG_COMPONENT_NODE_DEFINE(Parameters, PARAMETERS, 0);
DEG_COMPONENT_NODE_DEFINE(Particles, PARTICLE_SYSTEM, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(ParticleSettings, PARTICLE_SETTINGS, 0);
DEG_COMPONENT_NODE_DEFINE(PointCache, POINT_CACHE, 0);
DEG_COMPONENT_NODE_DEFINE(Pose, EVAL_POSE, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(Proxy, PROXY, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(Sequencer, SEQUENCER, 0);
DEG_COMPONENT_NODE_DEFINE(Shading, SHADING, ID_RECALC_SHADING);
DEG_COMPONENT_NODE_DEFINE(ShadingParameters, SHADING_PARAMETERS, ID_RECALC_SHADING);
DEG_COMPONENT_NODE_DEFINE(Transform, TRANSFORM, ID_RECALC_TRANSFORM);
DEG_COMPONENT_NODE_DEFINE(ObjectFromLayer, OBJECT_FROM_LAYER, 0);
DEG_COMPONENT_NODE_DEFINE(Dupli, DUPLI, 0);
DEG_COMPONENT_NODE_DEFINE(Synchronization, SYNCHRONIZATION, 0);
DEG_COMPONENT_NODE_DEFINE(GenericDatablock, GENERIC_DATABLOCK, 0);

/* Node Types Register =================================== */

void deg_register_component_depsnodes()
{
  register_node_typeinfo(&DNTI_ANIMATION);
  register_node_typeinfo(&DNTI_BONE);
  register_node_typeinfo(&DNTI_CACHE);
  register_node_typeinfo(&DNTI_BATCH_CACHE);
  register_node_typeinfo(&DNTI_COPY_ON_WRITE);
  register_node_typeinfo(&DNTI_GEOMETRY);
  register_node_typeinfo(&DNTI_LAYER_COLLECTIONS);
  register_node_typeinfo(&DNTI_PARAMETERS);
  register_node_typeinfo(&DNTI_PARTICLE_SYSTEM);
  register_node_typeinfo(&DNTI_PARTICLE_SETTINGS);
  register_node_typeinfo(&DNTI_POINT_CACHE);
  register_node_typeinfo(&DNTI_PROXY);
  register_node_typeinfo(&DNTI_EVAL_POSE);
  register_node_typeinfo(&DNTI_SEQUENCER);
  register_node_typeinfo(&DNTI_SHADING);
  register_node_typeinfo(&DNTI_SHADING_PARAMETERS);
  register_node_typeinfo(&DNTI_TRANSFORM);
  register_node_typeinfo(&DNTI_OBJECT_FROM_LAYER);
  register_node_typeinfo(&DNTI_DUPLI);
  register_node_typeinfo(&DNTI_SYNCHRONIZATION);
  register_node_typeinfo(&DNTI_GENERIC_DATABLOCK);
}

}  // namespace DEG

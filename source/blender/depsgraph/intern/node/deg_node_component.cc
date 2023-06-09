/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node_component.h"

#include <cstdio>
#include <cstring> /* required for STREQ later on. */

#include "BLI_ghash.h"
#include "BLI_hash.hh"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_action.h"

#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace blender::deg {

/* *********** */
/* Outer Nodes */

/* -------------------------------------------------------------------- */
/** \name Standard Component Methods
 * \{ */

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
  const string codebuf = to_string(int(opcode));
  return "OperationIDKey(" + codebuf + ", " + name + ")";
}

bool ComponentNode::OperationIDKey::operator==(const OperationIDKey &other) const
{
  return (opcode == other.opcode) && STREQ(name, other.name) && (name_tag == other.name_tag);
}

uint64_t ComponentNode::OperationIDKey::hash() const
{
  const int opcode_as_int = int(opcode);
  return BLI_ghashutil_combine_hash(
      name_tag,
      BLI_ghashutil_combine_hash(BLI_ghashutil_uinthash(opcode_as_int),
                                 BLI_ghashutil_strhash_p(name)));
}

ComponentNode::ComponentNode()
    : entry_operation(nullptr),
      exit_operation(nullptr),
      possibly_affects_visible_id(false),
      affects_visible_id(false)
{
  operations_map = new Map<ComponentNode::OperationIDKey, OperationNode *>();
}

void ComponentNode::init(const ID * /*id*/, const char * /*subdata*/)
{
  /* hook up eval context? */
  /* XXX: maybe this needs a special API? */
}

/* Free 'component' node */
ComponentNode::~ComponentNode()
{
  clear_operations();
  delete operations_map;
}

string ComponentNode::identifier() const
{
  const string type_name = type_get_factory(type)->type_name();
  const string name_part = name[0] ? (string(" '") + name + "'") : "";

  return "[" + type_name + "]" + name_part + " : " +
         "(affects_visible_id: " + (affects_visible_id ? "true" : "false") + ")";
}

OperationNode *ComponentNode::find_operation(OperationIDKey key) const
{
  OperationNode *node = nullptr;
  if (operations_map != nullptr) {
    node = operations_map->lookup_default(key, nullptr);
  }
  else {
    for (OperationNode *op_node : operations) {
      if (op_node->opcode == key.opcode && op_node->name_tag == key.name_tag &&
          STREQ(op_node->name.c_str(), key.name))
      {
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
  if (node == nullptr) {
    fprintf(stderr,
            "%s: find_operation(%s) failed\n",
            this->identifier().c_str(),
            key.identifier().c_str());
    BLI_assert_msg(0, "Request for non-existing operation, should not happen");
    return nullptr;
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
  return find_operation(key) != nullptr;
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
    OperationIDKey key(opcode, op_node->name.c_str(), name_tag);
    operations_map->add(key, op_node);

    /* Set back-link. */
    op_node->owner = this;
  }
  else {
    fprintf(stderr,
            "add_operation: Operation already exists - %s has %s at %p\n",
            this->identifier().c_str(),
            op_node->identifier().c_str(),
            op_node);
    BLI_assert_msg(0, "Should not happen!");
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
  BLI_assert(entry_operation == nullptr);
  entry_operation = op_node;
}

void ComponentNode::set_exit_operation(OperationNode *op_node)
{
  BLI_assert(exit_operation == nullptr);
  exit_operation = op_node;
}

void ComponentNode::clear_operations()
{
  if (operations_map != nullptr) {
    for (OperationNode *op_node : operations_map->values()) {
      delete op_node;
    }
    operations_map->clear();
  }
  for (OperationNode *op_node : operations) {
    delete op_node;
  }
  operations.clear();
}

void ComponentNode::tag_update(Depsgraph *graph, eUpdateSource source)
{
  /* Note that the node might already be tagged for an update due invisible state of the node
   * during previous dependency evaluation. Here the node gets re-tagged, so we need to give
   * the evaluated clues that evaluation needs to happen again. */
  for (OperationNode *op_node : operations) {
    op_node->tag_update(graph, source);
  }
  /* It is possible that tag happens before finalization. */
  if (operations_map != nullptr) {
    for (OperationNode *op_node : operations_map->values()) {
      op_node->tag_update(graph, source);
    }
  }
}

OperationNode *ComponentNode::get_entry_operation()
{
  if (entry_operation) {
    return entry_operation;
  }
  if (operations_map != nullptr && operations_map->size() == 1) {
    OperationNode *op_node = nullptr;
    /* TODO(sergey): This is somewhat slow. */
    for (OperationNode *tmp : operations_map->values()) {
      op_node = tmp;
    }
    /* Cache for the subsequent usage. */
    entry_operation = op_node;
    return op_node;
  }
  if (operations.size() == 1) {
    return operations[0];
  }
  return nullptr;
}

OperationNode *ComponentNode::get_exit_operation()
{
  if (exit_operation) {
    return exit_operation;
  }
  if (operations_map != nullptr && operations_map->size() == 1) {
    OperationNode *op_node = nullptr;
    /* TODO(sergey): This is somewhat slow. */
    for (OperationNode *tmp : operations_map->values()) {
      op_node = tmp;
    }
    /* Cache for the subsequent usage. */
    exit_operation = op_node;
    return op_node;
  }
  if (operations.size() == 1) {
    return operations[0];
  }
  return nullptr;
}

void ComponentNode::finalize_build(Depsgraph * /*graph*/)
{
  operations.reserve(operations_map->size());
  for (OperationNode *op_node : operations_map->values()) {
    operations.append(op_node);
  }
  delete operations_map;
  operations_map = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Component
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Register All Components
 * \{ */

DEG_COMPONENT_NODE_DEFINE(Animation, ANIMATION, ID_RECALC_ANIMATION);
/* TODO(sergey): Is this a correct tag? */
DEG_COMPONENT_NODE_DEFINE(BatchCache, BATCH_CACHE, ID_RECALC_SHADING);
DEG_COMPONENT_NODE_DEFINE(Bone, BONE, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(Cache, CACHE, 0);
DEG_COMPONENT_NODE_DEFINE(CopyOnWrite, COPY_ON_WRITE, ID_RECALC_COPY_ON_WRITE);
DEG_COMPONENT_NODE_DEFINE(ImageAnimation, IMAGE_ANIMATION, 0);
DEG_COMPONENT_NODE_DEFINE(Geometry, GEOMETRY, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(LayerCollections, LAYER_COLLECTIONS, 0);
DEG_COMPONENT_NODE_DEFINE(Parameters, PARAMETERS, 0);
DEG_COMPONENT_NODE_DEFINE(Particles, PARTICLE_SYSTEM, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(ParticleSettings, PARTICLE_SETTINGS, 0);
DEG_COMPONENT_NODE_DEFINE(PointCache, POINT_CACHE, 0);
DEG_COMPONENT_NODE_DEFINE(Pose, EVAL_POSE, ID_RECALC_GEOMETRY);
DEG_COMPONENT_NODE_DEFINE(Sequencer, SEQUENCER, 0);
DEG_COMPONENT_NODE_DEFINE(Shading, SHADING, ID_RECALC_SHADING);
DEG_COMPONENT_NODE_DEFINE(Transform, TRANSFORM, ID_RECALC_TRANSFORM);
DEG_COMPONENT_NODE_DEFINE(ObjectFromLayer, OBJECT_FROM_LAYER, 0);
DEG_COMPONENT_NODE_DEFINE(Hierarchy, HIERARCHY, 0);
DEG_COMPONENT_NODE_DEFINE(Dupli, DUPLI, 0);
DEG_COMPONENT_NODE_DEFINE(Synchronization, SYNCHRONIZATION, 0);
DEG_COMPONENT_NODE_DEFINE(Audio, AUDIO, 0);
DEG_COMPONENT_NODE_DEFINE(Armature, ARMATURE, 0);
DEG_COMPONENT_NODE_DEFINE(GenericDatablock, GENERIC_DATABLOCK, 0);
DEG_COMPONENT_NODE_DEFINE(Visibility, VISIBILITY, 0);
DEG_COMPONENT_NODE_DEFINE(Simulation, SIMULATION, 0);
DEG_COMPONENT_NODE_DEFINE(NTreeOutput, NTREE_OUTPUT, ID_RECALC_NTREE_OUTPUT);
DEG_COMPONENT_NODE_DEFINE(NTreeGeometryPreprocess, NTREE_GEOMETRY_PREPROCESS, 0);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Types Register
 * \{ */

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
  register_node_typeinfo(&DNTI_IMAGE_ANIMATION);
  register_node_typeinfo(&DNTI_EVAL_POSE);
  register_node_typeinfo(&DNTI_SEQUENCER);
  register_node_typeinfo(&DNTI_SHADING);
  register_node_typeinfo(&DNTI_TRANSFORM);
  register_node_typeinfo(&DNTI_OBJECT_FROM_LAYER);
  register_node_typeinfo(&DNTI_HIERARCHY);
  register_node_typeinfo(&DNTI_DUPLI);
  register_node_typeinfo(&DNTI_SYNCHRONIZATION);
  register_node_typeinfo(&DNTI_AUDIO);
  register_node_typeinfo(&DNTI_ARMATURE);
  register_node_typeinfo(&DNTI_GENERIC_DATABLOCK);
  register_node_typeinfo(&DNTI_VISIBILITY);
  register_node_typeinfo(&DNTI_SIMULATION);
  register_node_typeinfo(&DNTI_NTREE_OUTPUT);
  register_node_typeinfo(&DNTI_NTREE_GEOMETRY_PREPROCESS);
}

/** \} */

}  // namespace blender::deg

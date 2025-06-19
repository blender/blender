/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * This file contains the AbstractHierarchyIterator. It is intended for exporters for file
 * formats that concern an entire hierarchy of objects (rather than, for example, an OBJ file that
 * contains only a single mesh). Examples are Universal Scene Description (USD) and Alembic.
 * AbstractHierarchyIterator is intended to be subclassed to support concrete file formats.
 *
 * The AbstractHierarchyIterator makes a distinction between the actual object hierarchy and the
 * export hierarchy. The former is the parent/child structure in Blender, which can have multiple
 * parent-like objects. For example, a duplicated object can have both a duplicator and a parent,
 * both determining the final transform. The export hierarchy is the hierarchy as written to the
 * file, and every object has only one export-parent.
 *
 * Currently the AbstractHierarchyIterator does not make any decisions about *what* to export.
 * Selections like "selected only" or "no hair systems" are left to concrete subclasses.
 */

#pragma once

#include "IO_dupli_persistent_id.hh"

#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"

#include "DEG_depsgraph.hh"

#include <string>

struct Depsgraph;
struct DupliObject;
struct ID;
struct Main;
struct Object;
struct ParticleSystem;

namespace blender::io {

class AbstractHierarchyWriter;
class DupliParentFinder;

/* HierarchyContext structs are created by the AbstractHierarchyIterator. Each HierarchyContext
 * struct contains everything necessary to export a single object to a file. */
struct HierarchyContext {
  /*********** Determined during hierarchy iteration: ***************/
  Object *object; /* Evaluated object. */
  Object *export_parent;
  Object *duplicator;
  PersistentID persistent_id;
  float matrix_world[4][4];
  std::string export_name;

  /* When weak_export=true, the object will be exported only as transform, and only if is an
   * ancestor of an object with weak_export=false.
   *
   * In other words: when weak_export=true but this object has no children, or all descendants also
   * have weak_export=true, this object (and by recursive reasoning all its descendants) will be
   * excluded from the export.
   *
   * The export hierarchy is kept as close to the hierarchy in Blender as possible. As such, an
   * object that serves as a parent for another object, but which should NOT be exported itself, is
   * exported only as transform (i.e. as empty). This happens with objects that are invisible when
   * exporting with "Visible Only" enabled, for example. */
  bool weak_export;

  /* When true, this object should check its parents for animation data when determining whether
   * it's animated. This is necessary when a parent object in Blender is not part of the export. */
  bool animation_check_include_parent;

  /* The flag makes unambiguous the fact that the current context targets object or data. This is
   * notably used in USDHierarchyIterator::create_usd_export_context: options like
   * merge_parent_xform option is meaningless for object, it only makes sense for data. */
  bool is_object_data_context;

  /* This flag tells, within a object data context, if an object is the parent of other objects.
   * This is useful when exporting UsdGeomGprim: those cannot be nested into each other. For
   * example, an UsdGeomMesh cannot have other UsdGeomMesh as descendants and other hierarchy
   * strategies need to be adopted.
   */
  bool is_parent;

  /* When true this is duplisource object. This flag is used to identify instance prototypes. */
  bool is_duplisource;

  /* This flag tells whether an object is a valid point instance of other objects.
   * If true, it means the object has a valid reference path and its value can be included
   * in the instances data of UsdGeomPointInstancer. */
  bool is_point_instance;

  /* This flag tells if an object is a valid prototype of a point instancer. */
  bool is_point_proto;

  /* True if this context is a descendant of any context with is_point_instance set to true.
   * This helps skip redundant instancing data during export. */
  bool has_point_instance_ancestor;

  /*********** Determined during writer creation: ***************/
  float parent_matrix_inv_world[4][4]; /* Inverse of the parent's world matrix. */
  std::string export_path; /* Hierarchical path, such as "/grandparent/parent/object_name". */
  ParticleSystem *particle_system; /* Only set for particle/hair writers. */

  /* Hierarchical path of the object this object is duplicating; only set when this object should
   * be stored as a reference to its original. It can happen that the original is not part of the
   * exported objects, in which case this string is empty even though 'duplicator' is set. */
  std::string original_export_path;

  /* Export path of the higher-up exported data. For transforms, this is the export path of the
   * parent object. For object data, this is the export path of that object's transform.
   *
   * From the exported file's point of view, this is the path to the parent in that file. The term
   * "parent" is not used here to avoid confusion with Blender's meaning of the word (which always
   * refers to a different object). */
  std::string higher_up_export_path;

  /* Return a HierarchyContext representing the root of the export hierarchy. */
  static const HierarchyContext *root();

  /* For handling instanced collections, instances created by particles, etc. */
  bool is_instance() const;
  void mark_as_instance_of(const std::string &reference_export_path);
  void mark_as_not_instanced();
  bool is_prototype() const;

  /* For handling point instancing (Instance on Points geometry node). */
  bool is_point_instancer() const;

  bool is_object_visible(enum eEvaluationMode evaluation_mode) const;
};

/* Abstract writer for objects. Create concrete subclasses to write to USD, Alembic, etc.
 *
 * Instantiated by the AbstractHierarchyIterator on the first frame an object exists. Generally
 * that's the first frame to be exported, but can be later, for example when objects are
 * instantiated by particles. The AbstractHierarchyWriter::write() function is called on every
 * frame the object exists in the dependency graph and should be exported.
 */
class AbstractHierarchyWriter {
 public:
  virtual ~AbstractHierarchyWriter() = default;
  virtual void write(HierarchyContext &context) = 0;
  /* TODO(Sybren): add function like absent() that's called when a writer was previously created,
   * but wasn't used while exporting the current frame (for example, a particle-instanced mesh of
   * which the particle is no longer alive). */
 protected:
  /* Return true if the data written by this writer changes over time.
   * Note that this function assumes this is an object data writer. Transform writers should not
   * call this but implement their own logic. */
  virtual bool check_is_animated(const HierarchyContext &context) const;

  /* Helper functions for animation checks. */
  static bool check_has_physics(const HierarchyContext &context);
  static bool check_has_deforming_physics(const HierarchyContext &context);
};

/* Determines which subset of the writers actually gets to write. */
struct ExportSubset {
  bool transforms : 1;
  bool shapes : 1;
};

/* EnsuredWriter represents an AbstractHierarchyWriter* combined with information whether it was
 * newly created or not. It's returned by AbstractHierarchyIterator::ensure_writer(). */
class EnsuredWriter {
 private:
  AbstractHierarchyWriter *writer_;

  /* Is set to truth when ensure_writer() did not find existing writer and created a new one.
   * Is set to false when writer has been re-used or when allocation of the new one has failed
   * (`writer` will be `nullptr` in that case and bool(ensured_writer) will be false). */
  bool newly_created_;

  EnsuredWriter(AbstractHierarchyWriter *writer, bool newly_created);

 public:
  EnsuredWriter();

  static EnsuredWriter empty();
  static EnsuredWriter existing(AbstractHierarchyWriter *writer);
  static EnsuredWriter newly_created(AbstractHierarchyWriter *writer);

  bool is_newly_created() const;

  /* These operators make an EnsuredWriter* act as an AbstractHierarchyWriter* */
  operator bool() const;
  AbstractHierarchyWriter *operator->();
};

/* Unique identifier for a (potentially duplicated) object.
 *
 * Instances of this class serve as key in the export graph of the
 * AbstractHierarchyIterator. */
class ObjectIdentifier {
 public:
  Object *object;
  Object *duplicated_by; /* nullptr for real objects. */
  PersistentID persistent_id;

 protected:
  ObjectIdentifier(Object *object, Object *duplicated_by, const PersistentID &persistent_id);

 public:
  static ObjectIdentifier for_graph_root();
  static ObjectIdentifier for_real_object(Object *object);
  static ObjectIdentifier for_hierarchy_context(const HierarchyContext *context);
  static ObjectIdentifier for_duplicated_object(const DupliObject *dupli_object,
                                                Object *duplicated_by);

  bool is_root() const;

  uint64_t hash() const
  {
    return get_default_hash(object, duplicated_by, persistent_id);
  }
};

bool operator==(const ObjectIdentifier &obj_ident_a, const ObjectIdentifier &obj_ident_b);

/* AbstractHierarchyIterator iterates over objects in a dependency graph, and constructs export
 * writers. These writers are then called to perform the actual writing to a USD or Alembic file.
 *
 * Dealing with file- and scene-level data (for example, creating a USD scene, setting the frame
 * rate, etc.) is not part of the AbstractHierarchyIterator class structure, and should be done
 * in separate code.
 */
class AbstractHierarchyIterator {
 public:
  /* Mapping from export path to writer. */
  using WriterMap = blender::Map<std::string, AbstractHierarchyWriter *>;
  /* All the children of some object, as per the export hierarchy. */
  using ExportChildren = blender::Set<HierarchyContext *>;
  /* Mapping from an object and its duplicator to the object's export-children. */
  using ExportGraph = blender::Map<ObjectIdentifier, ExportChildren>;
  /* Mapping from ID to its export path. This is used for instancing; given an
   * instanced datablock, the export path of the original can be looked up. */
  using ExportPathMap = blender::Map<ID *, std::string>;
  /* Mapping from ID name to a set of names logically residing "under" it. Used for unique
   * name generation. */
  using ExportUsedNameMap = blender::Map<std::string, blender::Set<std::string>>;
  /* IDs of all duplisource objects, used to identify instance prototypes. */
  using DupliSources = blender::Set<ID *>;

 protected:
  ExportGraph export_graph_;
  ExportPathMap duplisource_export_path_;
  Main *bmain_;
  Depsgraph *depsgraph_;
  WriterMap writers_;
  ExportSubset export_subset_;
  DupliSources duplisources_;
  ExportUsedNameMap used_names_;

 public:
  explicit AbstractHierarchyIterator(Main *bmain, Depsgraph *depsgraph);
  virtual ~AbstractHierarchyIterator();

  /* Iterate over the depsgraph, create writers, and tell the writers to write.
   * Main entry point for the AbstractHierarchyIterator, must be called for every to-be-exported
   * (sub)frame. */
  virtual void iterate_and_write();

  /* Release all writers. Call after all frames have been exported. */
  void release_writers();

  /* Determine which subset of writers is used for exporting.
   * Set this before calling iterate_and_write().
   *
   * Note that writers are created for each iterated object, regardless of this option. When a
   * writer is created it will also write the current iteration, to ensure the hierarchy is
   * complete. The `export_subset` option is only in effect when the writer already existed from a
   * previous iteration. */
  void set_export_subset(ExportSubset export_subset);

  /* Convert the given name to something that is valid for the exported file format.
   * This base implementation is a no-op; override in a concrete subclass. */
  virtual std::string make_valid_name(const std::string &name) const;

  virtual std::string make_unique_name(const std::string &original_name,
                                       Set<std::string> &used_names);

  /* Return the name of this ID datablock that is valid for the exported file format. Overriding is
   * only necessary if make_valid_name(id->name+2) is not suitable for the exported file format.
   * NULL-safe: when `id == nullptr` this returns an empty string. */
  virtual std::string get_id_name(const ID *id) const;

  /* Given a HierarchyContext of some Object *, return an export path that is valid for its
   * object->data. Overriding is necessary when the exported format does NOT expect the object's
   * data to be a child of the object. */
  virtual std::string get_object_data_path(const HierarchyContext *context) const;

 private:
  void debug_print_export_graph(const ExportGraph &graph) const;

  void export_graph_construct();
  void connect_loose_objects();
  void export_graph_prune();
  void export_graph_clear();

  void visit_object(Object *object, Object *export_parent, bool weak_export);
  void visit_dupli_object(const DupliObject *dupli_object,
                          Object *duplicator,
                          const DupliParentFinder &dupli_parent_finder);

  void context_update_for_graph_index(HierarchyContext *context,
                                      const ObjectIdentifier &graph_index) const;

  void determine_export_paths(const HierarchyContext *parent_context);
  bool determine_duplication_references(const HierarchyContext *parent_context,
                                        const std::string &indent);

  /* These three functions create writers and call their write() method. */
  void make_writers(const HierarchyContext *parent_context);
  void make_writer_object_data(const HierarchyContext *context);
  void make_writers_particle_systems(const HierarchyContext *transform_context);

  /* Return the appropriate HierarchyContext for the data of the object represented by
   * object_context. */
  HierarchyContext context_for_object_data(const HierarchyContext *object_context) const;

  /* Convenience wrappers around get_id_name(). */
  std::string get_object_name(const Object *object) const;
  std::string get_object_name(const Object *object, const Object *parent);
  std::string get_object_data_name(const Object *object) const;

  using create_writer_func =
      AbstractHierarchyWriter *(AbstractHierarchyIterator::*)(const HierarchyContext *);
  /* Ensure that a writer exists; if it doesn't, call create_func(context).
   *
   * The create_func function should be one of the create_XXXX_writer(context) functions declared
   * below. */
  EnsuredWriter ensure_writer(const HierarchyContext *context, create_writer_func create_func);

 protected:
  /* Construct a valid path for the export file format. This class concatenates by using '/' as a
   * path separator, which is valid for both Alembic and USD. */
  virtual std::string path_concatenate(const std::string &parent_path,
                                       const std::string &child_path) const;

  /* Return whether this object should be marked as 'weak export' or not.
   *
   * When this returns false, writers for the transform and data are created,
   * and dupli-objects dupli-object generated from this object will be passed to
   * should_visit_dupli_object().
   *
   * When this returns true, only a transform writer is created and marked as
   * 'weak export'. In this case, the transform writer will be removed before
   * exporting starts, unless a descendant of this object is to be exported.
   * Dupli-object generated from this object will also be skipped.
   *
   * See HierarchyContext::weak_export.
   */
  virtual bool mark_as_weak_export(const Object *object) const;

  virtual bool should_visit_dupli_object(const DupliObject *dupli_object) const;

  virtual ObjectIdentifier determine_graph_index_object(const HierarchyContext *context);
  virtual ObjectIdentifier determine_graph_index_dupli(
      const HierarchyContext *context,
      const DupliObject *dupli_object,
      const DupliParentFinder &dupli_parent_finder);

  /* These functions should create an AbstractHierarchyWriter subclass instance, or return
   * nullptr if the object or its data should not be exported. Returning a nullptr for
   * data/hair/particle will NOT prevent the transform to be written.
   *
   * The returned writer is owned by the AbstractHierarchyWriter, and should be freed in
   * delete_object_writer().
   *
   * The created AbstractHierarchyWriter instances should NOT keep a copy of the context pointer.
   * The context can be stack-allocated and go out of scope. */
  virtual AbstractHierarchyWriter *create_transform_writer(const HierarchyContext *context) = 0;
  virtual AbstractHierarchyWriter *create_data_writer(const HierarchyContext *context) = 0;
  virtual AbstractHierarchyWriter *create_hair_writer(const HierarchyContext *context) = 0;
  virtual AbstractHierarchyWriter *create_particle_writer(const HierarchyContext *context) = 0;

  /* Called by release_writers() to free what the create_XXX_writer() functions allocated. */
  virtual void release_writer(AbstractHierarchyWriter *writer) = 0;

  /* Return true if data writers should be created for this context. */
  virtual bool include_data_writers(const HierarchyContext *) const
  {
    return true;
  }

  /* Return true if children of the context should be converted to writers. */
  virtual bool include_child_writers(const HierarchyContext *) const
  {
    return true;
  }

  AbstractHierarchyWriter *get_writer(const std::string &export_path) const;
  ExportChildren *graph_children(const HierarchyContext *context);
};

}  // namespace blender::io

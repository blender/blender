/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "COLLADAFWColor.h"
#include "COLLADAFWController.h"
#include "COLLADAFWEffect.h"
#include "COLLADAFWEffectCommon.h"
#include "COLLADAFWIWriter.h"
#include "COLLADAFWImage.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWMaterial.h"
#include "COLLADAFWMorphController.h"
#include "COLLADAFWSkinController.h"

#include "BKE_constraint.h"
#include "BKE_object.h"

#include "AnimationImporter.h"
#include "ArmatureImporter.h"
#include "ControllerExporter.h"
#include "ImportSettings.h"
#include "MeshImporter.h"
#include "TransformReader.h"

struct bContext;

/** Importer class. */
class DocumentImporter : COLLADAFW::IWriter {
 public:
  /** Enumeration to denote the stage of import */
  enum ImportStage {
    Fetching_Scene_data,      /* First pass to collect all data except controller */
    Fetching_Controller_data, /* Second pass to collect controller data */
  };
  /** Constructor */
  DocumentImporter(bContext *C, const ImportSettings *import_settings);

  /** Destructor */
  ~DocumentImporter();

  /** Function called by blender UI */
  bool import();

  /** these should not be here */
  Object *create_camera_object(COLLADAFW::InstanceCamera *, Scene *);
  Object *create_light_object(COLLADAFW::InstanceLight *, Scene *);
  Object *create_instance_node(Object *, COLLADAFW::Node *, COLLADAFW::Node *, Scene *, bool);
  /**
   * To create constraints off node <extra> tags. Assumes only constraint data in
   * current <extra> with blender profile.
   */
  void create_constraints(ExtraTags *et, Object *ob);
  std::vector<Object *> *write_node(COLLADAFW::Node *, COLLADAFW::Node *, Scene *, Object *, bool);
  void write_profile_COMMON(COLLADAFW::EffectCommon *, Material *);

  void translate_anim_recursive(COLLADAFW::Node *, COLLADAFW::Node *, Object *);

  /**
   * This method will be called if an error in the loading process occurred and the loader cannot
   * continue to load. The writer should undo all operations that have been performed.
   * \param errorMessage: A message containing information about the error that occurred.
   */
  void cancel(const COLLADAFW::String &errorMessage);

  /** This is the method called. The writer hast to prepare to receive data. */
  void start();

  /**
   * This method is called after the last write* method.
   * No other methods will be called after this.
   */
  void finish();

  /**
   * When this method is called, the writer must write the global document asset.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeGlobalAsset(const COLLADAFW::FileInfo *);
  /**
   * If the imported file was made with Blender, return the Blender version used,
   * otherwise return an empty std::string
   */
  std::string get_import_version(const COLLADAFW::FileInfo *asset);

  /**
   * When this method is called, the writer must write the scene.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeScene(const COLLADAFW::Scene *);

  /**
   * When this method is called, the writer must write the entire visual scene.
   * Return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeVisualScene(const COLLADAFW::VisualScene *);

  /**
   * When this method is called, the writer must handle all nodes contained in the
   * library nodes.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeLibraryNodes(const COLLADAFW::LibraryNodes *);

  /**
   * This function is called only for animations that pass COLLADAFW::validate.
   */
  bool writeAnimation(const COLLADAFW::Animation *);

  /**
   * Called on post-process stage after writeVisualScenes.
   */
  bool writeAnimationList(const COLLADAFW::AnimationList *);

#if WITH_OPENCOLLADA_ANIMATION_CLIP
  /* Please enable this when building with Collada 1.6.65 or newer (also in DocumentImporter.cpp)
   */
  bool writeAnimationClip(const COLLADAFW::AnimationClip *animationClip);
#endif

  /**
   * When this method is called, the writer must write the geometry.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeGeometry(const COLLADAFW::Geometry *);

  /**
   * When this method is called, the writer must write the material.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeMaterial(const COLLADAFW::Material *);

  /**
   * When this method is called, the writer must write the effect.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeEffect(const COLLADAFW::Effect *);

  /**
   * When this method is called, the writer must write the camera.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeCamera(const COLLADAFW::Camera *);

  /**
   * When this method is called, the writer must write the image.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeImage(const COLLADAFW::Image *);

  /**
   * When this method is called, the writer must write the light.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeLight(const COLLADAFW::Light *);

  /**
   * When this method is called, the writer must write the skin controller data.
   * \return The writer should return true, if writing succeeded, false otherwise.
   */
  bool writeSkinControllerData(const COLLADAFW::SkinControllerData *);

  /** This is called on post-process, before writeVisualScenes. */
  bool writeController(const COLLADAFW::Controller *);

  bool writeFormulas(const COLLADAFW::Formulas *);

  bool writeKinematicsScene(const COLLADAFW::KinematicsScene *);

  /** Add element and data for UniqueId */
  bool addExtraTags(const COLLADAFW::UniqueId &uid, ExtraTags *extra_tags);
  /** Get an existing #ExtraTags for uid */
  ExtraTags *getExtraTags(const COLLADAFW::UniqueId &uid);

  bool is_armature(COLLADAFW::Node *node);

 private:
  const ImportSettings *import_settings;

  /** Current import stage we're in. */
  ImportStage mImportStage;

  bContext *mContext;
  ViewLayer *view_layer;

  UnitConverter unit_converter;
  ArmatureImporter armature_importer;
  MeshImporter mesh_importer;
  AnimationImporter anim_importer;

  /** TagsMap typedef for uid_tags_map. */
  typedef std::map<std::string, ExtraTags *> TagsMap;
  /** Tags map of unique id as a string and ExtraTags instance. */
  TagsMap uid_tags_map;

  UidImageMap uid_image_map;
  std::map<COLLADAFW::UniqueId, Material *> uid_material_map;
  std::map<COLLADAFW::UniqueId, Material *> uid_effect_map;
  std::map<COLLADAFW::UniqueId, Camera *> uid_camera_map;
  std::map<COLLADAFW::UniqueId, Light *> uid_light_map;
  std::map<Material *, TexIndexTextureArrayMap> material_texture_mapping_map;
  std::multimap<COLLADAFW::UniqueId, Object *> object_map;
  std::map<COLLADAFW::UniqueId, COLLADAFW::Node *> node_map;
  std::vector<const COLLADAFW::VisualScene *> vscenes;
  std::vector<Object *> libnode_ob;

  std::map<COLLADAFW::UniqueId, COLLADAFW::Node *>
      root_map; /* find root joint by child joint uid, for bone tree evaluation during resampling
                 */
  std::map<COLLADAFW::UniqueId, const COLLADAFW::Object *> FW_object_map;

  std::string import_from_version;

  void report_unknown_reference(const COLLADAFW::Node &node, const std::string object_type);
};

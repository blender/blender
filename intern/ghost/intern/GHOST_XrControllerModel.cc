/* SPDX-FileCopyrightText: 2021-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cassert>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "GHOST_Types.h"
#include "GHOST_XrException.hh"
#include "GHOST_Xr_intern.hh"

#include "GHOST_XrControllerModel.hh"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STBIWDEF static inline
#include "tiny_gltf.h"

struct GHOST_XrControllerModelNode {
  int32_t parent_idx = -1;
  int32_t component_idx = -1;
  float local_transform[4][4];
};

/* -------------------------------------------------------------------- */
/** \name glTF Utilities
 *
 * Adapted from Microsoft OpenXR-Mixed Reality Samples (MIT License):
 * https://github.com/microsoft/OpenXR-MixedReality
 * \{ */

struct GHOST_XrPrimitive {
  std::vector<GHOST_XrControllerModelVertex> vertices;
  std::vector<uint32_t> indices;
};

/**
 * Validate that an accessor does not go out of bounds of the buffer view that it references and
 * that the buffer view does not exceed the bounds of the buffer that it references
 */
static void validate_accessor(const tinygltf::Accessor &accessor,
                              const tinygltf::BufferView &buffer_view,
                              const tinygltf::Buffer &buffer,
                              size_t byte_stride,
                              size_t element_size)
{
  /* Make sure the accessor does not go out of range of the buffer view. */
  if (accessor.byteOffset + (accessor.count - 1) * byte_stride + element_size >
      buffer_view.byteLength)
  {
    throw GHOST_XrException("glTF: Accessor goes out of range of bufferview.");
  }

  /* Make sure the buffer view does not go out of range of the buffer. */
  if (buffer_view.byteOffset + buffer_view.byteLength > buffer.data.size()) {
    throw GHOST_XrException("glTF: BufferView goes out of range of buffer.");
  }
}

template<float (GHOST_XrControllerModelVertex::*field)[3]>
static void read_vertices(const tinygltf::Accessor &accessor,
                          const tinygltf::BufferView &buffer_view,
                          const tinygltf::Buffer &buffer,
                          GHOST_XrPrimitive &primitive)
{
  if (accessor.type != TINYGLTF_TYPE_VEC3) {
    throw GHOST_XrException(
        "glTF: Accessor for primitive attribute has incorrect type (VEC3 expected).");
  }

  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
    throw GHOST_XrException(
        "glTF: Accessor for primitive attribute has incorrect component type (FLOAT expected).");
  }

  /* If stride is not specified, it is tightly packed. */
  constexpr size_t packed_size = sizeof(float) * 3;
  const size_t stride = buffer_view.byteStride == 0 ? packed_size : buffer_view.byteStride;
  validate_accessor(accessor, buffer_view, buffer, stride, packed_size);

  /* Resize the vertices vector, if necessary, to include room for the attribute data.
   * If there are multiple attributes for a primitive, the first one will resize, and the
   * subsequent will not need to. */
  primitive.vertices.resize(accessor.count);

  /* Copy the attribute value over from the glTF buffer into the appropriate vertex field. */
  const uint8_t *buffer_ptr = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
  for (size_t i = 0; i < accessor.count; i++, buffer_ptr += stride) {
    memcpy(primitive.vertices[i].*field, buffer_ptr, stride);
  }
}

static void load_attribute_accessor(const tinygltf::Model &gltf_model,
                                    const std::string &attribute_name,
                                    int accessor_id,
                                    GHOST_XrPrimitive &primitive)
{
  const auto &accessor = gltf_model.accessors.at(accessor_id);

  if (accessor.bufferView == -1) {
    throw GHOST_XrException("glTF: Accessor for primitive attribute specifies no bufferview.");
  }

  const tinygltf::BufferView &buffer_view = gltf_model.bufferViews.at(accessor.bufferView);
  if (buffer_view.target != TINYGLTF_TARGET_ARRAY_BUFFER && buffer_view.target != 0) {
    throw GHOST_XrException(
        "glTF: Accessor for primitive attribute uses bufferview with invalid 'target' type.");
  }

  const tinygltf::Buffer &buffer = gltf_model.buffers.at(buffer_view.buffer);

  if (attribute_name.compare("POSITION") == 0) {
    read_vertices<&GHOST_XrControllerModelVertex::position>(
        accessor, buffer_view, buffer, primitive);
  }
  else if (attribute_name.compare("NORMAL") == 0) {
    read_vertices<&GHOST_XrControllerModelVertex::normal>(
        accessor, buffer_view, buffer, primitive);
  }
}

/**
 * Reads index data from a glTF primitive into a GHOST_XrPrimitive. glTF indices may be 8bit, 16bit
 * or 32bit integers. This will coalesce indices from the source type(s) into a 32bit integer.
 */
template<typename TSrcIndex>
static void read_indices(const tinygltf::Accessor &accessor,
                         const tinygltf::BufferView &buffer_view,
                         const tinygltf::Buffer &buffer,
                         GHOST_XrPrimitive &primitive)
{

  /* Allow 0 (not specified) even though spec doesn't seem to allow this (BoomBox GLB fails). */
  if (buffer_view.target != TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER && buffer_view.target != 0) {
    throw GHOST_XrException(
        "glTF: Accessor for indices uses bufferview with invalid 'target' type.");
  }

  constexpr size_t component_size_bytes = sizeof(TSrcIndex);
  /* Index buffer must be packed per glTF spec. */
  if (buffer_view.byteStride != 0 && buffer_view.byteStride != component_size_bytes) {
    throw GHOST_XrException(
        "glTF: Accessor for indices uses bufferview with invalid 'byteStride'.");
  }

  validate_accessor(accessor, buffer_view, buffer, component_size_bytes, component_size_bytes);

  /* Since only triangles are supported, enforce that the number of indices is divisible by 3. */
  if ((accessor.count % 3) != 0) {
    throw GHOST_XrException("glTF: Unexpected number of indices for triangle primitive");
  }

  const TSrcIndex *index_buffer = reinterpret_cast<const TSrcIndex *>(
      buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset);
  for (uint32_t i = 0; i < accessor.count; i++) {
    primitive.indices.push_back(*(index_buffer + i));
  }
}

/**
 * Reads index data from a glTF primitive into a GHOST_XrPrimitive.
 */
static void load_index_accessor(const tinygltf::Model &gltf_model,
                                const tinygltf::Accessor &accessor,
                                GHOST_XrPrimitive &primitive)
{
  if (accessor.type != TINYGLTF_TYPE_SCALAR) {
    throw GHOST_XrException("glTF: Accessor for indices specifies invalid 'type'.");
  }

  if (accessor.bufferView == -1) {
    throw GHOST_XrException("glTF: Index accessor without bufferView is currently not supported.");
  }

  const tinygltf::BufferView &buffer_view = gltf_model.bufferViews.at(accessor.bufferView);
  const tinygltf::Buffer &buffer = gltf_model.buffers.at(buffer_view.buffer);

  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
    read_indices<uint8_t>(accessor, buffer_view, buffer, primitive);
  }
  else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    read_indices<uint16_t>(accessor, buffer_view, buffer, primitive);
  }
  else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
    read_indices<uint32_t>(accessor, buffer_view, buffer, primitive);
  }
  else {
    throw GHOST_XrException("glTF: Accessor for indices specifies invalid 'componentType'.");
  }
}

static GHOST_XrPrimitive read_primitive(const tinygltf::Model &gltf_model,
                                        const tinygltf::Primitive &gltf_primitive)
{
  if (gltf_primitive.mode != TINYGLTF_MODE_TRIANGLES) {
    throw GHOST_XrException(
        "glTF: Unsupported primitive mode. Only TINYGLTF_MODE_TRIANGLES is supported.");
  }

  GHOST_XrPrimitive primitive;

  /* glTF vertex data is stored in an attribute dictionary.Loop through each attribute and insert
   * it into the GHOST_XrPrimitive. */
  for (const auto &[attr_name, accessor_idx] : gltf_primitive.attributes) {
    load_attribute_accessor(gltf_model, attr_name, accessor_idx, primitive);
  }

  if (gltf_primitive.indices != -1) {
    /* If indices are specified for the glTF primitive, read them into the GHOST_XrPrimitive. */
    load_index_accessor(gltf_model, gltf_model.accessors.at(gltf_primitive.indices), primitive);
  }

  return primitive;
}

/**
 * Calculate node local and world transforms.
 */
static void calc_node_transforms(const tinygltf::Node &gltf_node,
                                 const float parent_transform[4][4],
                                 float r_local_transform[4][4],
                                 float r_world_transform[4][4])
{
  /* A node may specify either a 4x4 matrix or TRS (Translation - Rotation - Scale) values, but not
   * both. */
  if (gltf_node.matrix.size() == 16) {
    const std::vector<double> &dm = gltf_node.matrix;
    float m[4][4] = {{float(dm[0]), float(dm[1]), float(dm[2]), float(dm[3])},
                     {float(dm[4]), float(dm[5]), float(dm[6]), float(dm[7])},
                     {float(dm[8]), float(dm[9]), float(dm[10]), float(dm[11])},
                     {float(dm[12]), float(dm[13]), float(dm[14]), float(dm[15])}};
    memcpy(r_local_transform, m, sizeof(float[4][4]));
  }
  else {
    /* No matrix is present, so construct a matrix from the TRS values (each one is optional). */
    std::vector<double> translation = gltf_node.translation;
    std::vector<double> rotation = gltf_node.rotation;
    std::vector<double> scale = gltf_node.scale;
    Eigen::Matrix4f &m = *(Eigen::Matrix4f *)r_local_transform;
    Eigen::Quaternionf q;
    Eigen::Matrix3f scalemat;

    if (translation.size() != 3) {
      translation.resize(3);
      translation[0] = translation[1] = translation[2] = 0.0;
    }
    if (rotation.size() != 4) {
      rotation.resize(4);
      rotation[0] = rotation[1] = rotation[2] = 0.0;
      rotation[3] = 1.0;
    }
    if (scale.size() != 3) {
      scale.resize(3);
      scale[0] = scale[1] = scale[2] = 1.0;
    }

    q.w() = float(rotation[3]);
    q.x() = float(rotation[0]);
    q.y() = float(rotation[1]);
    q.z() = float(rotation[2]);
    q.normalize();

    scalemat.setIdentity();
    scalemat(0, 0) = float(scale[0]);
    scalemat(1, 1) = float(scale[1]);
    scalemat(2, 2) = float(scale[2]);

    m.setIdentity();
    m.block<3, 3>(0, 0) = q.toRotationMatrix() * scalemat;
    m.block<3, 1>(0, 3) = Eigen::Vector3f(
        float(translation[0]), float(translation[1]), float(translation[2]));
  }

  *(Eigen::Matrix4f *)r_world_transform = *(Eigen::Matrix4f *)parent_transform *
                                          *(Eigen::Matrix4f *)r_local_transform;
}

static void load_node(const tinygltf::Model &gltf_model,
                      int gltf_node_id,
                      int32_t parent_idx,
                      const float parent_transform[4][4],
                      const std::string &parent_name,
                      const std::vector<XrControllerModelNodePropertiesMSFT> &node_properties,
                      std::vector<GHOST_XrControllerModelVertex> &vertices,
                      std::vector<uint32_t> &indices,
                      std::vector<GHOST_XrControllerModelComponent> &components,
                      std::vector<GHOST_XrControllerModelNode> &nodes,
                      std::vector<int32_t> &node_state_indices)
{
  const tinygltf::Node &gltf_node = gltf_model.nodes.at(gltf_node_id);
  float world_transform[4][4];

  GHOST_XrControllerModelNode &node = nodes.emplace_back();
  const int32_t node_idx = int32_t(nodes.size() - 1);
  node.parent_idx = parent_idx;
  calc_node_transforms(gltf_node, parent_transform, node.local_transform, world_transform);

  for (size_t i = 0; i < node_properties.size(); ++i) {
    if ((node_state_indices[i] < 0) && (parent_name == node_properties[i].parentNodeName) &&
        (gltf_node.name == node_properties[i].nodeName))
    {
      node_state_indices[i] = node_idx;
      break;
    }
  }

  if (gltf_node.mesh != -1) {
    const tinygltf::Mesh &gltf_mesh = gltf_model.meshes.at(gltf_node.mesh);

    GHOST_XrControllerModelComponent &component = components.emplace_back();
    node.component_idx = components.size() - 1;
    memcpy(component.transform, world_transform, sizeof(component.transform));
    component.vertex_offset = vertices.size();
    component.index_offset = indices.size();

    for (const tinygltf::Primitive &gltf_primitive : gltf_mesh.primitives) {
      /* Read the primitive data from the glTF buffers. */
      const GHOST_XrPrimitive primitive = read_primitive(gltf_model, gltf_primitive);

      const size_t start_vertex = vertices.size();
      size_t offset = start_vertex;
      size_t count = primitive.vertices.size();
      vertices.resize(offset + count);
      memcpy(vertices.data() + offset,
             primitive.vertices.data(),
             count * sizeof(decltype(primitive.vertices)::value_type));

      offset = indices.size();
      count = primitive.indices.size();
      indices.resize(offset + count);
      for (size_t i = 0; i < count; i += 3) {
        indices[offset + i + 0] = start_vertex + primitive.indices[i + 0];
        indices[offset + i + 1] = start_vertex + primitive.indices[i + 2];
        indices[offset + i + 2] = start_vertex + primitive.indices[i + 1];
      }
    }

    component.vertex_count = vertices.size() - component.vertex_offset;
    component.index_count = indices.size() - component.index_offset;
  }

  /* Recursively load all children. */
  for (const int child_node_id : gltf_node.children) {
    load_node(gltf_model,
              child_node_id,
              node_idx,
              world_transform,
              gltf_node.name,
              node_properties,
              vertices,
              indices,
              components,
              nodes,
              node_state_indices);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OpenXR Extension Functions
 *
 * \{ */

static PFN_xrGetControllerModelKeyMSFT g_xrGetControllerModelKeyMSFT = nullptr;
static PFN_xrLoadControllerModelMSFT g_xrLoadControllerModelMSFT = nullptr;
static PFN_xrGetControllerModelPropertiesMSFT g_xrGetControllerModelPropertiesMSFT = nullptr;
static PFN_xrGetControllerModelStateMSFT g_xrGetControllerModelStateMSFT = nullptr;
static XrInstance g_instance = XR_NULL_HANDLE;

#define INIT_EXTENSION_FUNCTION(name) \
  CHECK_XR( \
      xrGetInstanceProcAddr(instance, #name, reinterpret_cast<PFN_xrVoidFunction *>(&g_##name)), \
      "Failed to get pointer to extension function: " #name);

static void init_controller_model_extension_functions(XrInstance instance)
{
  if (instance != g_instance) {
    g_instance = instance;
    g_xrGetControllerModelKeyMSFT = nullptr;
    g_xrLoadControllerModelMSFT = nullptr;
    g_xrGetControllerModelPropertiesMSFT = nullptr;
    g_xrGetControllerModelStateMSFT = nullptr;
  }

  if (g_xrGetControllerModelKeyMSFT == nullptr) {
    INIT_EXTENSION_FUNCTION(xrGetControllerModelKeyMSFT);
  }
  if (g_xrLoadControllerModelMSFT == nullptr) {
    INIT_EXTENSION_FUNCTION(xrLoadControllerModelMSFT);
  }
  if (g_xrGetControllerModelPropertiesMSFT == nullptr) {
    INIT_EXTENSION_FUNCTION(xrGetControllerModelPropertiesMSFT);
  }
  if (g_xrGetControllerModelStateMSFT == nullptr) {
    INIT_EXTENSION_FUNCTION(xrGetControllerModelStateMSFT);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHOST_XrControllerModel
 *
 * \{ */

GHOST_XrControllerModel::GHOST_XrControllerModel(XrInstance instance,
                                                 const char *subaction_path_str)
{
  init_controller_model_extension_functions(instance);

  CHECK_XR(xrStringToPath(instance, subaction_path_str, &m_subaction_path),
           (std::string("Failed to get user path \"") + subaction_path_str + "\".").data());
}

GHOST_XrControllerModel::~GHOST_XrControllerModel()
{
  if (m_load_task.valid()) {
    m_load_task.wait();
  }
}

void GHOST_XrControllerModel::load(XrSession session)
{
  if (m_data_loaded || m_load_task.valid()) {
    return;
  }

  /* Get model key. */
  XrControllerModelKeyStateMSFT key_state{XR_TYPE_CONTROLLER_MODEL_KEY_STATE_MSFT};
  CHECK_XR(g_xrGetControllerModelKeyMSFT(session, m_subaction_path, &key_state),
           "Failed to get controller model key state.");

  if (key_state.modelKey != XR_NULL_CONTROLLER_MODEL_KEY_MSFT) {
    m_model_key = key_state.modelKey;
    /* Load asynchronously. */
    m_load_task = std::async(std::launch::async,
                             [&, session = session]() { return loadControllerModel(session); });
  }
}

void GHOST_XrControllerModel::loadControllerModel(XrSession session)
{
  /* Load binary buffers. */
  uint32_t buf_size = 0;
  CHECK_XR(g_xrLoadControllerModelMSFT(session, m_model_key, 0, &buf_size, nullptr),
           "Failed to get controller model buffer size.");

  std::vector<uint8_t> buf((size_t(buf_size)));
  CHECK_XR(g_xrLoadControllerModelMSFT(session, m_model_key, buf_size, &buf_size, buf.data()),
           "Failed to load controller model binary buffers.");

  /* Convert to glTF model. */
  tinygltf::TinyGLTF gltf_loader;
  tinygltf::Model gltf_model;
  std::string err_msg;
  {
    /* Workaround for TINYGLTF_NO_STB_IMAGE define. Set custom image loader to prevent failure when
     * parsing image data. */
    auto load_img_func = [](tinygltf::Image *img,
                            const int p0,
                            std::string *p1,
                            std::string *p2,
                            int p3,
                            int p4,
                            const uchar *p5,
                            int p6,
                            void *user_pointer) -> bool {
      (void)img;
      (void)p0;
      (void)p1;
      (void)p2;
      (void)p3;
      (void)p4;
      (void)p5;
      (void)p6;
      (void)user_pointer;
      return true;
    };
    gltf_loader.SetImageLoader(load_img_func, nullptr);
  }

  if (!gltf_loader.LoadBinaryFromMemory(&gltf_model, &err_msg, nullptr, buf.data(), buf_size)) {
    throw GHOST_XrException(("Failed to load glTF controller model: " + err_msg).c_str());
  }

  /* Get node properties. */
  XrControllerModelPropertiesMSFT model_properties{XR_TYPE_CONTROLLER_MODEL_PROPERTIES_MSFT};
  model_properties.nodeCapacityInput = 0;
  CHECK_XR(g_xrGetControllerModelPropertiesMSFT(session, m_model_key, &model_properties),
           "Failed to get controller model node properties count.");

  std::vector<XrControllerModelNodePropertiesMSFT> node_properties(
      model_properties.nodeCountOutput, {XR_TYPE_CONTROLLER_MODEL_NODE_PROPERTIES_MSFT});
  model_properties.nodeCapacityInput = uint32_t(node_properties.size());
  model_properties.nodeProperties = node_properties.data();
  CHECK_XR(g_xrGetControllerModelPropertiesMSFT(session, m_model_key, &model_properties),
           "Failed to get controller model node properties.");

  m_node_state_indices.resize(node_properties.size(), -1);

  /* Get mesh vertex data. */
  const tinygltf::Scene &default_scene = gltf_model.scenes.at(
      (gltf_model.defaultScene == -1) ? 0 : gltf_model.defaultScene);
  const int32_t root_idx = -1;
  const std::string root_name = "";
  float root_transform[4][4] = {{0}};
  root_transform[0][0] = root_transform[1][1] = root_transform[2][2] = root_transform[3][3] = 1.0f;

  for (const int node_id : default_scene.nodes) {
    load_node(gltf_model,
              node_id,
              root_idx,
              root_transform,
              root_name,
              node_properties,
              m_vertices,
              m_indices,
              m_components,
              m_nodes,
              m_node_state_indices);
  }

  m_data_loaded = true;
}

void GHOST_XrControllerModel::updateComponents(XrSession session)
{
  if (!m_data_loaded) {
    return;
  }

  /* Get node states. */
  XrControllerModelStateMSFT model_state{XR_TYPE_CONTROLLER_MODEL_STATE_MSFT};
  model_state.nodeCapacityInput = 0;
  CHECK_XR(g_xrGetControllerModelStateMSFT(session, m_model_key, &model_state),
           "Failed to get controller model node state count.");

  const uint32_t count = model_state.nodeCountOutput;
  std::vector<XrControllerModelNodeStateMSFT> node_states(
      count, {XR_TYPE_CONTROLLER_MODEL_NODE_STATE_MSFT});
  model_state.nodeCapacityInput = count;
  model_state.nodeStates = node_states.data();
  CHECK_XR(g_xrGetControllerModelStateMSFT(session, m_model_key, &model_state),
           "Failed to get controller model node states.");

  /* Update node local transforms. */
  assert(m_node_state_indices.size() == count);

  for (uint32_t state_idx = 0; state_idx < count; ++state_idx) {
    const int32_t &node_idx = m_node_state_indices[state_idx];
    if (node_idx >= 0) {
      const XrPosef &pose = node_states[state_idx].nodePose;
      Eigen::Matrix4f &m = *(Eigen::Matrix4f *)m_nodes[node_idx].local_transform;
      Eigen::Quaternionf q(
          pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
      m.setIdentity();
      m.block<3, 3>(0, 0) = q.toRotationMatrix();
      m.block<3, 1>(0, 3) = Eigen::Vector3f(pose.position.x, pose.position.y, pose.position.z);
    }
  }

  /* Calculate component transforms (in world space). */
  std::vector<Eigen::Matrix4f> world_transforms(m_nodes.size());
  uint32_t i = 0;
  for (const GHOST_XrControllerModelNode &node : m_nodes) {
    world_transforms[i] = (node.parent_idx >= 0) ? world_transforms[node.parent_idx] *
                                                       *(Eigen::Matrix4f *)node.local_transform :
                                                   *(Eigen::Matrix4f *)node.local_transform;
    if (node.component_idx >= 0) {
      memcpy(m_components[node.component_idx].transform,
             world_transforms[i].data(),
             sizeof(m_components[node.component_idx].transform));
    }
    ++i;
  }
}

void GHOST_XrControllerModel::getData(GHOST_XrControllerModelData &r_data)
{
  if (m_data_loaded) {
    r_data.count_vertices = uint32_t(m_vertices.size());
    r_data.vertices = m_vertices.data();
    r_data.count_indices = uint32_t(m_indices.size());
    r_data.indices = m_indices.data();
    r_data.count_components = uint32_t(m_components.size());
    r_data.components = m_components.data();
  }
  else {
    r_data.count_vertices = 0;
    r_data.vertices = nullptr;
    r_data.count_indices = 0;
    r_data.indices = nullptr;
    r_data.count_components = 0;
    r_data.components = nullptr;
  }
}

/** \} */

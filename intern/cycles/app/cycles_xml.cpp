/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdio>

#include <algorithm>

#include "graph/node_xml.h"

#include "scene/background.h"
#include "scene/camera.h"
#include "scene/film.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/osl.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include "util/log.h"
#include "util/path.h"
#include "util/projection.h"
#include "util/string.h"
#include "util/transform.h"
#include "util/xml.h"

#include "app/cycles_xml.h"

CCL_NAMESPACE_BEGIN

/* XML reading state */

struct XMLReadState : public XMLReader {
  Scene *scene = nullptr;   /* Scene pointer. */
  Transform tfm;            /* Current transform state. */
  bool smooth = false;      /* Smooth normal state. */
  Shader *shader = nullptr; /* Current shader. */
  string base;              /* Base path to current file. */
  float dicing_rate = 1.0f; /* Current dicing rate. */
  Object *object = nullptr; /* Current object. */

  XMLReadState()
  {
    tfm = transform_identity();
  }
};

/* Attribute Reading */

static bool xml_read_int(int *value, const xml_node node, const char *name)
{
  const xml_attribute attr = node.attribute(name);

  if (attr) {
    *value = atoi(attr.value());
    return true;
  }

  return false;
}

static bool xml_read_int_array(vector<int> &value, const xml_node node, const char *name)
{
  const xml_attribute attr = node.attribute(name);

  if (attr) {
    vector<string> tokens;
    string_split(tokens, attr.value());

    for (const string &token : tokens) {
      value.push_back(atoi(token.c_str()));
    }

    return true;
  }

  return false;
}

static bool xml_read_float(float *value, const xml_node node, const char *name)
{
  const xml_attribute attr = node.attribute(name);

  if (attr) {
    *value = (float)atof(attr.value());
    return true;
  }

  return false;
}

static bool xml_read_float_array(vector<float> &value, const xml_node node, const char *name)
{
  const xml_attribute attr = node.attribute(name);

  if (attr) {
    vector<string> tokens;
    string_split(tokens, attr.value());

    for (const string &token : tokens) {
      value.push_back((float)atof(token.c_str()));
    }

    return true;
  }

  return false;
}

static bool xml_read_float3(float3 *value, const xml_node node, const char *name)
{
  vector<float> array;

  if (xml_read_float_array(array, node, name) && array.size() == 3) {
    *value = make_float3(array[0], array[1], array[2]);
    return true;
  }

  return false;
}

static bool xml_read_float3_array(vector<float3> &value, const xml_node node, const char *name)
{
  vector<float> array;

  if (xml_read_float_array(array, node, name)) {
    for (size_t i = 0; i < array.size(); i += 3) {
      value.push_back(make_float3(array[i + 0], array[i + 1], array[i + 2]));
    }

    return true;
  }

  return false;
}

static bool xml_read_float4(float4 *value, const xml_node node, const char *name)
{
  vector<float> array;

  if (xml_read_float_array(array, node, name) && array.size() == 4) {
    *value = make_float4(array[0], array[1], array[2], array[3]);
    return true;
  }

  return false;
}

static bool xml_read_string(string *str, const xml_node node, const char *name)
{
  const xml_attribute attr = node.attribute(name);

  if (attr) {
    *str = attr.value();
    return true;
  }

  return false;
}

static bool xml_equal_string(const xml_node node, const char *name, const char *value)
{
  const xml_attribute attr = node.attribute(name);

  if (attr) {
    return string_iequals(attr.value(), value);
  }

  return false;
}

/* Camera */

static void xml_read_camera(XMLReadState &state, const xml_node node)
{
  Camera *cam = state.scene->camera;

  int width = -1;
  int height = -1;
  xml_read_int(&width, node, "width");
  xml_read_int(&height, node, "height");

  cam->set_full_width(width);
  cam->set_full_height(height);

  xml_read_node(state, cam, node);

  cam->set_matrix(state.tfm);

  cam->need_flags_update = true;
  cam->update(state.scene);
}

/* Shader */

static void xml_read_shader_graph(XMLReadState &state, Shader *shader, const xml_node graph_node)
{
  xml_read_node(state, shader, graph_node);

  unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

  /* local state, shader nodes can't link to nodes outside the shader graph */
  XMLReader graph_reader;
  graph_reader.node_map[ustring("output")] = graph->output();

  for (xml_node node = graph_node.first_child(); node; node = node.next_sibling()) {
    ustring node_name(node.name());

    if (node_name == "connect") {
      /* connect nodes */
      vector<string> from_tokens;
      vector<string> to_tokens;

      string_split(from_tokens, node.attribute("from").value());
      string_split(to_tokens, node.attribute("to").value());

      if (from_tokens.size() == 2 && to_tokens.size() == 2) {
        const ustring from_node_name(from_tokens[0]);
        const ustring from_socket_name(from_tokens[1]);
        const ustring to_node_name(to_tokens[0]);
        const ustring to_socket_name(to_tokens[1]);

        /* find nodes and sockets */
        ShaderOutput *output = nullptr;
        ShaderInput *input = nullptr;

        if (graph_reader.node_map.find(from_node_name) != graph_reader.node_map.end()) {
          ShaderNode *fromnode = (ShaderNode *)graph_reader.node_map[from_node_name];

          for (ShaderOutput *out : fromnode->outputs) {
            if (string_iequals(out->socket_type.name.string(), from_socket_name.string())) {
              output = out;
            }
          }

          if (!output) {
            LOG_ERROR << "Unknown output socket name \"" << from_node_name << "\" on \""
                      << from_socket_name << "\".";
          }
        }
        else {
          LOG_ERROR << "Unknown shader node name \"" << from_node_name << "\"";
        }

        if (graph_reader.node_map.find(to_node_name) != graph_reader.node_map.end()) {
          ShaderNode *tonode = (ShaderNode *)graph_reader.node_map[to_node_name];

          for (ShaderInput *in : tonode->inputs) {
            if (string_iequals(in->socket_type.name.string(), to_socket_name.string())) {
              input = in;
            }
          }

          if (!input) {
            LOG_ERROR << "Unknown input socket name \"" << to_socket_name << "\" on \""
                      << to_node_name << "\"";
          }
        }
        else {
          LOG_ERROR << "Unknown shader node name \"" << to_node_name << "\"";
        }

        /* connect */
        if (output && input) {
          graph->connect(output, input);
        }
      }
      else {
        LOG_ERROR << "Invalid from or to value for connect node.";
      }

      continue;
    }

    ShaderNode *snode = nullptr;

#ifdef WITH_OSL
    if (node_name == "osl_shader") {
      ShaderManager *manager = state.scene->shader_manager.get();

      if (manager->use_osl()) {
        std::string filepath;

        if (xml_read_string(&filepath, node, "src")) {
          if (path_is_relative(filepath)) {
            filepath = path_join(state.base, filepath);
          }

          snode = OSLShaderManager::osl_node(graph.get(), state.scene, filepath, "");

          if (!snode) {
            LOG_ERROR << "Failed to create OSL node from \"" << filepath << "\"";
            continue;
          }
        }
        else {
          LOG_ERROR << "OSL node missing \"src\" attribute.";
          continue;
        }
      }
      else {
        LOG_ERROR << "OSL node without using --shadingsys osl.";
        continue;
      }
    }
    else
#endif
    {
      /* exception for name collision */
      if (node_name == "background") {
        node_name = "background_shader";
      }

      const NodeType *node_type = NodeType::find(node_name);

      if (!node_type) {
        LOG_ERROR << "Unknown shader node \"" << node.name() << "\"";
        continue;
      }
      if (node_type->type != NodeType::SHADER) {
        LOG_ERROR << "Node type \"" << node_type->name << "\" is not a shader node";
        continue;
      }
      if (node_type->create == nullptr) {
        LOG_ERROR << "Can't create abstract node type \""
                  << "\"";
        continue;
      }

      snode = graph->create_node(node_type);
    }

    xml_read_node(graph_reader, snode, node);

    if (node_name == "image_texture") {
      ImageTextureNode *img = (ImageTextureNode *)snode;
      const ustring filename(path_join(state.base, img->get_filename().string()));
      img->set_filename(filename);
    }
    else if (node_name == "environment_texture") {
      EnvironmentTextureNode *env = (EnvironmentTextureNode *)snode;
      const ustring filename(path_join(state.base, env->get_filename().string()));
      env->set_filename(filename);
    }
  }

  shader->set_graph(std::move(graph));
  shader->tag_update(state.scene);
}

static void xml_read_shader(XMLReadState &state, const xml_node node)
{
  Shader *shader = state.scene->create_node<Shader>();
  xml_read_shader_graph(state, shader, node);
}

/* Background */

static void xml_read_background(XMLReadState &state, const xml_node node)
{
  /* Background Settings */
  xml_read_node(state, state.scene->background, node);

  /* Background Shader */
  Shader *shader = state.scene->default_background;
  xml_read_shader_graph(state, shader, node);
}

/* Mesh */

static Mesh *xml_add_mesh(Scene *scene, const Transform &tfm, Object *object)
{
  if (object && object->get_geometry()->is_mesh()) {
    /* Use existing object and mesh */
    object->set_tfm(tfm);
    Geometry *geometry = object->get_geometry();
    return static_cast<Mesh *>(geometry);
  }

  /* Create mesh */
  Mesh *mesh = scene->create_node<Mesh>();

  /* Create object. */
  object = scene->create_node<Object>();
  object->set_geometry(mesh);
  object->set_tfm(tfm);

  return mesh;
}

static void xml_read_mesh(const XMLReadState &state, const xml_node node)
{
  /* add mesh */
  Mesh *mesh = xml_add_mesh(state.scene, state.tfm, state.object);
  array<Node *> used_shaders = mesh->get_used_shaders();
  used_shaders.push_back_slow(state.shader);
  mesh->set_used_shaders(used_shaders);

  /* read state */
  const int shader = 0;
  const bool smooth = state.smooth;

  /* read vertices and polygons */
  vector<float3> P;
  vector<float3> VN; /* Vertex normals */
  vector<float> UV;
  vector<float> T;  /* UV tangents */
  vector<float> TS; /* UV tangent signs */
  vector<int> verts;
  vector<int> nverts;

  xml_read_float3_array(P, node, "P");
  xml_read_int_array(verts, node, "verts");
  xml_read_int_array(nverts, node, "nverts");

  if (xml_equal_string(node, "subdivision", "catmull-clark")) {
    mesh->set_subdivision_type(Mesh::SUBDIVISION_CATMULL_CLARK);
  }
  else if (xml_equal_string(node, "subdivision", "linear")) {
    mesh->set_subdivision_type(Mesh::SUBDIVISION_LINEAR);
  }

  array<float3> P_array;
  P_array = P;

  if (mesh->get_subdivision_type() == Mesh::SUBDIVISION_NONE) {
    /* create vertices */

    mesh->set_verts(P_array);

    size_t num_triangles = 0;
    for (size_t i = 0; i < nverts.size(); i++) {
      num_triangles += nverts[i] - 2;
    }
    mesh->reserve_mesh(mesh->get_verts().size(), num_triangles);

    /* create triangles */
    int index_offset = 0;

    for (size_t i = 0; i < nverts.size(); i++) {
      for (int j = 0; j < nverts[i] - 2; j++) {
        const int v0 = verts[index_offset];
        const int v1 = verts[index_offset + j + 1];
        const int v2 = verts[index_offset + j + 2];

        assert(v0 < (int)P.size());
        assert(v1 < (int)P.size());
        assert(v2 < (int)P.size());

        mesh->add_triangle(v0, v1, v2, shader, smooth);
      }

      index_offset += nverts[i];
    }

    /* Vertex normals */
    if (xml_read_float3_array(VN, node, Attribute::standard_name(ATTR_STD_VERTEX_NORMAL))) {
      Attribute *attr = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL);
      float3 *fdata = attr->data_float3();

      /* Loop over the normals */
      for (auto n : VN) {
        fdata[0] = n;
        fdata++;
      }
    }

    /* UV map */
    if (xml_read_float_array(UV, node, "UV") ||
        xml_read_float_array(UV, node, Attribute::standard_name(ATTR_STD_UV)))
    {
      Attribute *attr = mesh->attributes.add(ATTR_STD_UV);
      float2 *fdata = attr->data_float2();

      /* Loop over the triangles */
      index_offset = 0;
      for (size_t i = 0; i < nverts.size(); i++) {
        for (int j = 0; j < nverts[i] - 2; j++) {
          const int v0 = index_offset;
          const int v1 = index_offset + j + 1;
          const int v2 = index_offset + j + 2;

          assert(v0 * 2 + 1 < (int)UV.size());
          assert(v1 * 2 + 1 < (int)UV.size());
          assert(v2 * 2 + 1 < (int)UV.size());

          fdata[0] = make_float2(UV[v0 * 2], UV[v0 * 2 + 1]);
          fdata[1] = make_float2(UV[v1 * 2], UV[v1 * 2 + 1]);
          fdata[2] = make_float2(UV[v2 * 2], UV[v2 * 2 + 1]);
          fdata += 3;
        }

        index_offset += nverts[i];
      }
    }

    /* Tangents */
    if (xml_read_float_array(T, node, Attribute::standard_name(ATTR_STD_UV_TANGENT))) {
      Attribute *attr = mesh->attributes.add(ATTR_STD_UV_TANGENT);
      float3 *fdata = attr->data_float3();

      /* Loop over the triangles */
      index_offset = 0;
      for (size_t i = 0; i < nverts.size(); i++) {
        for (int j = 0; j < nverts[i] - 2; j++) {
          const int v0 = index_offset;
          const int v1 = index_offset + j + 1;
          const int v2 = index_offset + j + 2;

          assert(v0 * 3 + 2 < (int)T.size());
          assert(v1 * 3 + 2 < (int)T.size());
          assert(v2 * 3 + 2 < (int)T.size());

          fdata[0] = make_float3(T[v0 * 3], T[v0 * 3 + 1], T[v0 * 3 + 2]);
          fdata[1] = make_float3(T[v1 * 3], T[v1 * 3 + 1], T[v1 * 3 + 2]);
          fdata[2] = make_float3(T[v2 * 3], T[v2 * 3 + 1], T[v2 * 3 + 2]);
          fdata += 3;
        }
        index_offset += nverts[i];
      }
    }

    /* Tangent signs */
    if (xml_read_float_array(TS, node, Attribute::standard_name(ATTR_STD_UV_TANGENT_SIGN))) {
      Attribute *attr = mesh->attributes.add(ATTR_STD_UV_TANGENT_SIGN);
      float *fdata = attr->data_float();

      /* Loop over the triangles */
      index_offset = 0;
      for (size_t i = 0; i < nverts.size(); i++) {
        for (int j = 0; j < nverts[i] - 2; j++) {
          const int v0 = index_offset;
          const int v1 = index_offset + j + 1;
          const int v2 = index_offset + j + 2;

          assert(v0 < (int)TS.size());
          assert(v1 < (int)TS.size());
          assert(v2 < (int)TS.size());

          fdata[0] = TS[v0];
          fdata[1] = TS[v1];
          fdata[2] = TS[v2];
          fdata += 3;
        }
        index_offset += nverts[i];
      }
    }
  }
  else {
    /* create vertices */
    mesh->set_verts(P_array);

    size_t num_corners = 0;
    for (size_t i = 0; i < nverts.size(); i++) {
      num_corners += nverts[i];
    }
    mesh->reserve_subd_faces(nverts.size(), num_corners);

    /* create subd_faces */
    int index_offset = 0;

    for (size_t i = 0; i < nverts.size(); i++) {
      mesh->add_subd_face(&verts[index_offset], nverts[i], shader, smooth);
      index_offset += nverts[i];
    }

    /* UV map */
    if (xml_read_float_array(UV, node, "UV") ||
        xml_read_float_array(UV, node, Attribute::standard_name(ATTR_STD_UV)))
    {
      Attribute *attr = mesh->subd_attributes.add(ATTR_STD_UV);
      float3 *fdata = attr->data_float3();

      index_offset = 0;
      for (size_t i = 0; i < nverts.size(); i++) {
        for (int j = 0; j < nverts[i]; j++) {
          *(fdata++) = make_float3(UV[index_offset++]);
        }
      }
    }

    /* setup subd params */
    float dicing_rate = state.dicing_rate;
    xml_read_float(&dicing_rate, node, "dicing_rate");
    dicing_rate = std::max(0.1f, dicing_rate);

    mesh->set_subd_dicing_rate(dicing_rate);
    mesh->set_subd_objecttoworld(state.tfm);
  }

  /* we don't yet support arbitrary attributes, for now add vertex
   * coordinates as generated coordinates if requested */
  if (mesh->need_attribute(state.scene, ATTR_STD_GENERATED)) {
    Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED);
    std::copy_n(mesh->get_verts().data(), mesh->get_verts().size(), attr->data_float3());
  }
}

/* Light */

static void xml_read_light(XMLReadState &state, const xml_node node)
{
  Scene *scene = state.scene;

  /* Create light. */
  Light *light = scene->create_node<Light>();

  array<Node *> used_shaders;
  used_shaders.push_back_slow(state.shader);
  light->set_used_shaders(used_shaders);

  /* Create object. */
  Object *object = scene->create_node<Object>();
  object->set_tfm(state.tfm);
  object->set_visibility(PATH_RAY_ALL_VISIBILITY & ~PATH_RAY_CAMERA);
  object->set_geometry(light);

  xml_read_node(state, light, node);
}

/* Transform */

static void xml_read_transform(const xml_node node, Transform &tfm)
{
  if (node.attribute("matrix")) {
    vector<float> matrix;
    if (xml_read_float_array(matrix, node, "matrix") && matrix.size() == 16) {
      const ProjectionTransform projection = *(ProjectionTransform *)matrix.data();
      tfm = tfm * projection_to_transform(projection_transpose(projection));
    }
  }

  if (node.attribute("translate")) {
    float3 translate = zero_float3();
    xml_read_float3(&translate, node, "translate");
    tfm = tfm * transform_translate(translate);
  }

  if (node.attribute("rotate")) {
    float4 rotate = zero_float4();
    xml_read_float4(&rotate, node, "rotate");
    tfm = tfm * transform_rotate(DEG2RADF(rotate.x), make_float3(rotate.y, rotate.z, rotate.w));
  }

  if (node.attribute("scale")) {
    float3 scale = zero_float3();
    xml_read_float3(&scale, node, "scale");
    tfm = tfm * transform_scale(scale);
  }
}

/* State */

static void xml_read_state(XMLReadState &state, const xml_node node)
{
  /* Read shader */
  string shadername;

  if (xml_read_string(&shadername, node, "shader")) {
    bool found = false;

    for (Shader *shader : state.scene->shaders) {
      if (shader->name == shadername) {
        state.shader = shader;
        found = true;
        break;
      }
    }

    if (!found) {
      LOG_ERROR << "Unknown shader \"" << shadername << "\"";
    }
  }

  /* Read object */
  string objectname;

  if (xml_read_string(&objectname, node, "object")) {
    bool found = false;

    for (Object *object : state.scene->objects) {
      if (object->name == objectname) {
        state.object = object;
        found = true;
        break;
      }
    }

    if (!found) {
      LOG_ERROR << "Unknown object \"" << objectname << "\"";
    }
  }

  xml_read_float(&state.dicing_rate, node, "dicing_rate");

  /* read smooth/flat */
  if (xml_equal_string(node, "interpolation", "smooth")) {
    state.smooth = true;
  }
  else if (xml_equal_string(node, "interpolation", "flat")) {
    state.smooth = false;
  }
}

/* Object */

static void xml_read_object(XMLReadState &state, const xml_node node)
{
  Scene *scene = state.scene;

  /* create mesh */
  Mesh *mesh = scene->create_node<Mesh>();

  /* create object */
  Object *object = scene->create_node<Object>();
  object->set_geometry(mesh);
  object->set_tfm(state.tfm);

  xml_read_node(state, object, node);
}

/* Scene */

static void xml_read_include(XMLReadState &state, const string &src);

static void xml_read_scene(XMLReadState &state, const xml_node scene_node)
{
  for (xml_node node = scene_node.first_child(); node; node = node.next_sibling()) {
    if (string_iequals(node.name(), "film")) {
      xml_read_node(state, state.scene->film, node);
    }
    else if (string_iequals(node.name(), "integrator")) {
      xml_read_node(state, state.scene->integrator, node);
    }
    else if (string_iequals(node.name(), "camera")) {
      xml_read_camera(state, node);
    }
    else if (string_iequals(node.name(), "shader")) {
      xml_read_shader(state, node);
    }
    else if (string_iequals(node.name(), "background")) {
      xml_read_background(state, node);
    }
    else if (string_iequals(node.name(), "mesh")) {
      xml_read_mesh(state, node);
    }
    else if (string_iequals(node.name(), "light")) {
      xml_read_light(state, node);
    }
    else if (string_iequals(node.name(), "transform")) {
      XMLReadState substate = state;

      xml_read_transform(node, substate.tfm);
      xml_read_scene(substate, node);
    }
    else if (string_iequals(node.name(), "state")) {
      XMLReadState substate = state;

      xml_read_state(substate, node);
      xml_read_scene(substate, node);
    }
    else if (string_iequals(node.name(), "include")) {
      string src;

      if (xml_read_string(&src, node, "src")) {
        xml_read_include(state, src);
      }
    }
    else if (string_iequals(node.name(), "object")) {
      XMLReadState substate = state;

      xml_read_object(substate, node);
      xml_read_scene(substate, node);
    }
    else {
      LOG_ERROR << "Unknown node \"" << node.name() << "\"";
    }
  }
}

/* Include */

static void xml_read_include(XMLReadState &state, const string &src)
{
  /* open XML document */
  xml_document doc;
  xml_parse_result parse_result;

  const string path = path_join(state.base, src);
  parse_result = doc.load_file(path.c_str());

  if (parse_result) {
    XMLReadState substate = state;
    substate.base = path_dirname(path);

    const xml_node cycles = doc.child("cycles");
    xml_read_scene(substate, cycles);
  }
  else {
    LOG_ERROR << "\"" << src << "\" read error: " << parse_result.description();
    exit(EXIT_FAILURE);
  }
}

/* File */

void xml_read_file(Scene *scene, const char *filepath)
{
  XMLReadState state;

  state.scene = scene;
  state.tfm = transform_identity();
  state.shader = scene->default_surface;
  state.smooth = false;
  state.dicing_rate = 1.0f;
  state.base = path_dirname(filepath);

  xml_read_include(state, path_filename(filepath));

  scene->params.bvh_type = BVH_TYPE_STATIC;
}

CCL_NAMESPACE_END

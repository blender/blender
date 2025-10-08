/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Convert material node-trees to GLSL.
 */

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"

#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_cryptomatte.hh"

#include "IMB_colormanagement.hh"

#include "GPU_capabilities.hh"
#include "GPU_shader.hh"
#include "GPU_uniform_buffer.hh"
#include "GPU_vertex_format.hh"

#include "gpu_codegen.hh"
#include "gpu_material_library.hh"
#include "gpu_shader_dependency_private.hh"

#include <cstdarg>
#include <cstring>

using namespace blender;
using namespace blender::gpu::shader;

/* -------------------------------------------------------------------- */
/** \name Type > string conversion
 * \{ */

static std::ostream &operator<<(std::ostream &stream, const GPUInput *input)
{
  switch (input->source) {
    case GPU_SOURCE_FUNCTION_CALL:
    case GPU_SOURCE_OUTPUT:
      return stream << (input->is_zone_io ? "zone" : "tmp") << input->id;
    case GPU_SOURCE_CONSTANT:
      return stream << (input->is_zone_io ? "zone" : "cons") << input->id;
    case GPU_SOURCE_UNIFORM:
      return stream << "node_tree.u" << input->id << (input->is_duplicate ? "b" : "");
    case GPU_SOURCE_ATTR:
      return stream << "var_attrs.v" << input->attr->id;
    case GPU_SOURCE_UNIFORM_ATTR:
      return stream << "UNI_ATTR(unf_attrs[resource_id].attr" << input->uniform_attr->id << ")";
    case GPU_SOURCE_LAYER_ATTR:
      return stream << "attr_load_layer(" << input->layer_attr->hash_code << ")";
    case GPU_SOURCE_STRUCT:
      return stream << "strct" << input->id;
    case GPU_SOURCE_TEX:
      return stream << input->texture->sampler_name;
    case GPU_SOURCE_TEX_TILED_MAPPING:
      return stream << input->texture->tiled_mapping_name;
    default:
      BLI_assert(0);
      return stream;
  }
}

static std::ostream &operator<<(std::ostream &stream, const GPUOutput *output)
{
  return stream << (output->is_zone_io ? "zone" : "tmp") << output->id;
}

/* Print data constructor (i.e: vec2(1.0f, 1.0f)). */
static std::ostream &operator<<(std::ostream &stream, const Span<float> &span)
{
  stream << (GPUType)span.size() << "(";
  /* Use uint representation to allow exact same bit pattern even if NaN. This is
   * because we can pass UINTs as floats for constants. */
  const Span<uint32_t> uint_span = span.cast<uint32_t>();
  for (const uint32_t &element : uint_span) {
    char formatted_float[32];
    SNPRINTF(formatted_float, "uintBitsToFloat(%uu)", element);
    stream << formatted_float;
    if (&element != &uint_span.last()) {
      stream << ", ";
    }
  }
  stream << ")";
  return stream;
}

/* Trick type to change overload and keep a somewhat nice syntax. */
struct GPUConstant : public GPUInput {};

static std::ostream &operator<<(std::ostream &stream, const GPUConstant *input)
{
  stream << Span<float>(input->vec, input->type);
  return stream;
}

namespace blender::gpu::shader {
/* Needed to use the << operators from nested namespaces. :(
 * https://stackoverflow.com/questions/5195512/namespaces-and-operator-resolution */
using ::operator<<;
}  // namespace blender::gpu::shader

/** \} */

/* -------------------------------------------------------------------- */
/** \name GLSL code generation
 * \{ */

const char *GPUCodegenCreateInfo::NameBuffer::append_sampler_name(const char name[32])
{
  auto index = sampler_names.size();
  sampler_names.append(std::make_unique<NameEntry>());
  char *name_buffer = sampler_names[index]->data();
  memcpy(name_buffer, name, 32);
  return name_buffer;
}

GPUCodegen::GPUCodegen(GPUMaterial *mat_, GPUNodeGraph *graph_, const char *debug_name)
    : mat(*mat_), graph(*graph_)
{
  BLI_hash_mm2a_init(&hm2a_, GPU_material_uuid_get(&mat));
  BLI_hash_mm2a_add_int(&hm2a_, GPU_material_flag(&mat));
  create_info = MEM_new<GPUCodegenCreateInfo>(__func__, debug_name);
  output.create_info = reinterpret_cast<GPUShaderCreateInfo *>(
      static_cast<ShaderCreateInfo *>(create_info));
}

GPUCodegen::~GPUCodegen()
{
  MEM_SAFE_FREE(cryptomatte_input_);
  MEM_delete(create_info);
  BLI_freelistN(&ubo_inputs_);
};

bool GPUCodegen::should_optimize_heuristic() const
{
  /* If each of the maximal attributes are exceeded, we can optimize, but we should also ensure
   * the baseline is met. */
  bool do_optimize = (nodes_total_ >= 60 || textures_total_ >= 4 || uniforms_total_ >= 64) &&
                     (textures_total_ >= 1 && uniforms_total_ >= 8 && nodes_total_ >= 4);
  return do_optimize;
}

void GPUCodegen::generate_attribs()
{
  if (BLI_listbase_is_empty(&graph.attributes)) {
    output.attr_load.clear();
    return;
  }

  GPUCodegenCreateInfo &info = *create_info;

  info.interface_generated = MEM_new<StageInterfaceInfo>(__func__, "codegen_iface", "var_attrs");
  StageInterfaceInfo &iface = *info.interface_generated;
  info.vertex_out(iface);

  /* Input declaration, loading / assignment to interface and geometry shader passthrough. */
  std::stringstream load_ss;

  /* Index of the attribute as ordered in graph.attributes. */
  int attr_n = 0;
  int slot = 15;
  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph.attributes) {
    if (slot == -1) {
      BLI_assert_msg(0, "Too many attributes");
      break;
    }
    STRNCPY(info.name_buffer.attr_names[slot], attr->input_name);
    SNPRINTF(info.name_buffer.var_names[slot], "v%d", attr->id);

    StringRefNull attr_name = info.name_buffer.attr_names[slot];
    StringRefNull var_name = info.name_buffer.var_names[slot];

    GPUType input_type, iface_type;

    load_ss << "var_attrs." << var_name;
    if (attr->is_hair_length || attr->is_hair_intercept) {
      iface_type = input_type = GPU_FLOAT;
      load_ss << " = attr_load_" << input_type << "(domain, " << attr_name << ", " << attr_n
              << ");\n";
    }
    else {
      switch (attr->type) {
        case CD_ORCO:
          /* Need vec4 to detect usage of default attribute. */
          input_type = GPU_VEC4;
          iface_type = GPU_VEC3;
          load_ss << " = attr_load_orco(domain, " << attr_name << ", " << attr_n << ");\n";
          break;
        case CD_TANGENT:
          iface_type = input_type = GPU_VEC4;
          load_ss << " = attr_load_tangent(domain, " << attr_name << ", " << attr_n << ");\n";
          break;
        default:
          iface_type = input_type = GPU_VEC4;
          load_ss << " = attr_load_" << input_type << "(domain, " << attr_name << ", " << attr_n
                  << ");\n";
          break;
      }
    }
    attr_n++;

    info.vertex_in(slot--, to_type(input_type), attr_name);
    iface.smooth(to_type(iface_type), var_name);
  }

  output.attr_load = load_ss.str();
}

void GPUCodegen::generate_resources()
{
  GPUCodegenCreateInfo &info = *create_info;

  std::stringstream ss;

  /* Textures. */
  int slot = 0;
  LISTBASE_FOREACH (GPUMaterialTexture *, tex, &graph.textures) {
    if (tex->colorband) {
      const char *name = info.name_buffer.append_sampler_name(tex->sampler_name);
      info.sampler(slot++, ImageType::Float1DArray, name, Frequency::BATCH);
    }
    else if (tex->sky) {
      const char *name = info.name_buffer.append_sampler_name(tex->sampler_name);
      info.sampler(0, ImageType::Float2DArray, name, Frequency::BATCH);
    }
    else if (tex->tiled_mapping_name[0] != '\0') {
      const char *name = info.name_buffer.append_sampler_name(tex->sampler_name);
      info.sampler(slot++, ImageType::Float2DArray, name, Frequency::BATCH);

      const char *name_mapping = info.name_buffer.append_sampler_name(tex->tiled_mapping_name);
      info.sampler(slot++, ImageType::Float1DArray, name_mapping, Frequency::BATCH);
    }
    else {
      const char *name = info.name_buffer.append_sampler_name(tex->sampler_name);
      info.sampler(slot++, ImageType::Float2D, name, Frequency::BATCH);
    }
  }

  /* Increment heuristic. */
  textures_total_ = slot;

  if (!BLI_listbase_is_empty(&ubo_inputs_)) {
    /* NOTE: generate_uniform_buffer() should have sorted the inputs before this. */
    ss << "struct NodeTree {\n";
    LISTBASE_FOREACH (LinkData *, link, &ubo_inputs_) {
      GPUInput *input = (GPUInput *)(link->data);
      if (input->source == GPU_SOURCE_CRYPTOMATTE) {
        ss << input->type << " crypto_hash;\n";
      }
      else {
        ss << input->type << " u" << input->id << (input->is_duplicate ? "b" : "") << ";\n";
      }
    }
    ss << "};\n\n";

    info.uniform_buf(GPU_NODE_TREE_UBO_SLOT, "NodeTree", GPU_UBO_BLOCK_NAME, Frequency::BATCH);
  }

  if (!BLI_listbase_is_empty(&graph.uniform_attrs.list)) {
    ss << "struct UniformAttrs {\n";
    LISTBASE_FOREACH (GPUUniformAttr *, attr, &graph.uniform_attrs.list) {
      ss << "vec4 attr" << attr->id << ";\n";
    }
    ss << "};\n\n";

    /* TODO(fclem): Use the macro for length. Currently not working for EEVEE. */
    /* DRW_RESOURCE_CHUNK_LEN = 512 */
    info.uniform_buf(2, "UniformAttrs", GPU_ATTRIBUTE_UBO_BLOCK_NAME "[512]", Frequency::BATCH);
  }

  if (!BLI_listbase_is_empty(&graph.layer_attrs)) {
    info.additional_info("draw_layer_attributes");
  }

  info.typedef_source_generated = ss.str();
}

void GPUCodegen::node_serialize(Set<StringRefNull> &used_libraries,
                                std::stringstream &eval_ss,
                                const GPUNode *node)
{
  gpu_material_library_use_function(used_libraries, node->name);

  auto source_reference = [&](GPUInput *input) {
    BLI_assert(ELEM(input->source, GPU_SOURCE_OUTPUT, GPU_SOURCE_ATTR));
    /* These inputs can have non matching types. Do conversion. */
    GPUType to = input->type;
    GPUType from = (input->source == GPU_SOURCE_ATTR) ? input->attr->gputype :
                                                        input->link->output->type;
    if (from != to) {
      /* Use defines declared inside codegen_lib (e.g. vec4_from_float). */
      eval_ss << to << "_from_" << from << "(";
    }

    if (input->source == GPU_SOURCE_ATTR) {
      eval_ss << input;
    }
    else {
      eval_ss << input->link->output;
    }

    if (from != to) {
      /* Special case that needs luminance coefficients as argument. */
      if (from == GPU_VEC4 && to == GPU_FLOAT) {
        float coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(coefficients);
        eval_ss << ", " << blender::Span<float>(coefficients, 3);
      }
      eval_ss << ")";
    }
  };

  /* Declare constants. */
  LISTBASE_FOREACH (GPUInput *, input, &node->inputs) {
    auto type = [&]() {
      /* Don't declare zone io variables twice. */
      std::stringstream ss;
      if (!input->is_duplicate) {
        ss << input->type;
      }
      return ss.str();
    };
    switch (input->source) {
      case GPU_SOURCE_FUNCTION_CALL:
        eval_ss << type() << " " << input << "; " << input->function_call << input << ");\n";
        break;
      case GPU_SOURCE_STRUCT:
        eval_ss << input->type << " " << input << " = CLOSURE_DEFAULT;\n";
        break;
      case GPU_SOURCE_CONSTANT:
        if (!input->is_duplicate) {
          eval_ss << type() << " " << input << " = " << (GPUConstant *)input << ";\n";
        }
        break;
      case GPU_SOURCE_OUTPUT:
      case GPU_SOURCE_ATTR:
        if (input->is_zone_io) {
          eval_ss << type() << " " << input << " = ";
          source_reference(input);
          eval_ss << ";\n";
        }
        break;
      default:
        if (input->is_zone_io && (!input->is_duplicate || !input->link)) {
          eval_ss << type() << " zone" << input->id << " = " << input << ";\n";
        }
        break;
    }
  }
  /* Declare temporary variables for node output storage. */
  LISTBASE_FOREACH (GPUOutput *, output, &node->outputs) {
    if (output->is_zone_io) {
      break;
    }
    eval_ss << output->type << " " << output << ";\n";
  }

  /* Function call. */
  eval_ss << node->name << "(";
  /* Input arguments. */
  LISTBASE_FOREACH (GPUInput *, input, &node->inputs) {
    if (input->is_zone_io) {
      break;
    }
    switch (input->source) {
      case GPU_SOURCE_OUTPUT:
      case GPU_SOURCE_ATTR: {
        source_reference(input);
        break;
      }
      default:
        eval_ss << input;
        break;
    }
    GPUOutput *output = static_cast<GPUOutput *>(node->outputs.first);
    if ((input->next && !input->next->is_zone_io) || (output && !output->is_zone_io)) {
      eval_ss << ", ";
    }
  }
  /* Output arguments. */
  LISTBASE_FOREACH (GPUOutput *, output, &node->outputs) {
    if (output->is_zone_io) {
      break;
    }
    eval_ss << output;
    if (output->next && !output->next->is_zone_io) {
      eval_ss << ", ";
    }
  }
  eval_ss << ");\n\n";

  /* Increment heuristic. */
  nodes_total_++;
}

static Vector<StringRefNull> set_to_vector_stable(Set<StringRefNull> &set)
{
  Vector<StringRefNull> source_files;
  for (const StringRefNull &str : set) {
    source_files.append(str);
  }
  /* Sort dependencies to avoid random order causing shader caching to fail (see #108289). */
  std::sort(source_files.begin(), source_files.end());
  return source_files;
}

GPUGraphOutput GPUCodegen::graph_serialize(GPUNodeTag tree_tag,
                                           GPUNodeLink *output_link,
                                           const char *output_default)
{
  if (output_link == nullptr && output_default == nullptr) {
    return {};
  }

  Set<StringRefNull> used_libraries;
  std::stringstream eval_ss;
  bool has_nodes = false;
  /* NOTE: The node order is already top to bottom (or left to right in node editor)
   * because of the evaluation order inside ntreeExecGPUNodes(). */
  LISTBASE_FOREACH (GPUNode *, node, &graph.nodes) {
    if ((node->tag & tree_tag) == 0) {
      continue;
    }
    node_serialize(used_libraries, eval_ss, node);
    has_nodes = true;
  }

  if (!has_nodes) {
    return {};
  }

  if (output_link) {
    eval_ss << "return " << output_link->output << ";\n";
  }
  else {
    /* Default output in case there are only AOVs. */
    eval_ss << "return " << output_default << ";\n";
  }

  std::string str = eval_ss.str();
  BLI_hash_mm2a_add(&hm2a_, reinterpret_cast<const uchar *>(str.c_str()), str.size());
  return {str, set_to_vector_stable(used_libraries)};
}

GPUGraphOutput GPUCodegen::graph_serialize(GPUNodeTag tree_tag)
{
  std::stringstream eval_ss;
  Set<StringRefNull> used_libraries;
  LISTBASE_FOREACH (GPUNode *, node, &graph.nodes) {
    if (node->tag & tree_tag) {
      node_serialize(used_libraries, eval_ss, node);
    }
  }
  std::string str = eval_ss.str();
  BLI_hash_mm2a_add(&hm2a_, reinterpret_cast<const uchar *>(str.c_str()), str.size());
  return {str, set_to_vector_stable(used_libraries)};
}

void GPUCodegen::generate_cryptomatte()
{
  cryptomatte_input_ = MEM_callocN<GPUInput>(__func__);
  cryptomatte_input_->type = GPU_FLOAT;
  cryptomatte_input_->source = GPU_SOURCE_CRYPTOMATTE;

  float material_hash = 0.0f;
  Material *material = GPU_material_get_material(&mat);
  if (material) {
    bke::cryptomatte::CryptomatteHash hash(material->id.name + 2,
                                           BLI_strnlen(material->id.name + 2, MAX_NAME - 2));
    material_hash = hash.float_encoded();
  }
  cryptomatte_input_->vec[0] = material_hash;

  BLI_addtail(&ubo_inputs_, BLI_genericNodeN(cryptomatte_input_));
}

void GPUCodegen::generate_uniform_buffer()
{
  /* Extract uniform inputs. */
  LISTBASE_FOREACH (GPUNode *, node, &graph.nodes) {
    LISTBASE_FOREACH (GPUInput *, input, &node->inputs) {
      if (input->source == GPU_SOURCE_UNIFORM && !input->link) {
        /* We handle the UBO uniforms separately. */
        BLI_addtail(&ubo_inputs_, BLI_genericNodeN(input));
        uniforms_total_++;
      }
    }
  }
  if (!BLI_listbase_is_empty(&ubo_inputs_)) {
    /* This sorts the inputs based on size. */
    GPU_material_uniform_buffer_create(&mat, &ubo_inputs_);
  }
}

/* Sets id for unique names for all inputs, resources and temp variables. */
void GPUCodegen::set_unique_ids()
{
  blender::Map<int, GPUNode *> zone_starts;
  blender::Map<int, GPUNode *> zone_ends;

  int id = 1;
  LISTBASE_FOREACH (GPUNode *, node, &graph.nodes) {
    LISTBASE_FOREACH (GPUInput *, input, &node->inputs) {
      input->id = id++;
    }
    LISTBASE_FOREACH (GPUOutput *, output, &node->outputs) {
      output->id = id++;
    }
    if (node->zone_index != -1) {
      auto &map = node->is_zone_end ? zone_ends : zone_starts;
      map.add(node->zone_index, node);
    }
  }

  auto find_zone_io = [](auto first) {
    while (first && !first->is_zone_io && first->next) {
      first = first->next;
    }
    return first;
  };

  /* Assign the same id to inputs and outputs of start and end zones. */
  for (GPUNode *end : zone_ends.values()) {

    GPUInput *end_input = find_zone_io((GPUInput *)end->inputs.first);
    GPUOutput *end_output = find_zone_io((GPUOutput *)end->outputs.first);

    GPUNode *start = zone_starts.lookup(end->zone_index);

    GPUInput *start_input = find_zone_io((GPUInput *)start->inputs.first);
    GPUOutput *start_output = find_zone_io((GPUOutput *)start->outputs.first);

    for (; start_input; start_input = start_input->next,
                        start_output = start_output->next,
                        end_input = end_input->next,
                        end_output = end_output->next)
    {
      start_output->id = start_input->id;
      end_input->id = start_input->id;
      end_output->id = start_input->id;
    }
  }
}

void GPUCodegen::generate_graphs()
{
  set_unique_ids();

  output.surface = graph_serialize(
      GPU_NODE_TAG_SURFACE | GPU_NODE_TAG_AOV, graph.outlink_surface, "CLOSURE_DEFAULT");
  output.volume = graph_serialize(GPU_NODE_TAG_VOLUME, graph.outlink_volume, "CLOSURE_DEFAULT");
  output.displacement = graph_serialize(
      GPU_NODE_TAG_DISPLACEMENT, graph.outlink_displacement, nullptr);
  output.thickness = graph_serialize(GPU_NODE_TAG_THICKNESS, graph.outlink_thickness, nullptr);
  if (!BLI_listbase_is_empty(&graph.outlink_compositor)) {
    output.composite = graph_serialize(GPU_NODE_TAG_COMPOSITOR);
  }

  if (!BLI_listbase_is_empty(&graph.material_functions)) {
    LISTBASE_FOREACH (GPUNodeGraphFunctionLink *, func_link, &graph.material_functions) {
      std::stringstream eval_ss;
      /* Untag every node in the graph to avoid serializing nodes from other functions */
      LISTBASE_FOREACH (GPUNode *, node, &graph.nodes) {
        node->tag &= ~GPU_NODE_TAG_FUNCTION;
      }
      /* Tag only the nodes needed for the current function */
      gpu_nodes_tag(&graph, func_link->outlink, GPU_NODE_TAG_FUNCTION);
      GPUGraphOutput graph = graph_serialize(GPU_NODE_TAG_FUNCTION, func_link->outlink);
      eval_ss << "float " << func_link->name << "() {\n" << graph.serialized << "}\n\n";
      output.material_functions.append({eval_ss.str(), graph.dependencies});
    }
    /* Leave the function tags as they were before serialization */
    LISTBASE_FOREACH (GPUNodeGraphFunctionLink *, funclink, &graph.material_functions) {
      gpu_nodes_tag(&graph, funclink->outlink, GPU_NODE_TAG_FUNCTION);
    }
  }

  LISTBASE_FOREACH (GPUMaterialAttribute *, attr, &graph.attributes) {
    BLI_hash_mm2a_add(&hm2a_, (uchar *)attr->name, strlen(attr->name));
  }

  hash_ = BLI_hash_mm2a_end(&hm2a_);
}

/** \} */

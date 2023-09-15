/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/material.h"
#include "hydra/node_util.h"
#include "hydra/session.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include <pxr/imaging/hd/sceneDelegate.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(CyclesMaterialTokens,
    ((cyclesSurface, "cycles:surface"))
    ((cyclesDisplacement, "cycles:displacement"))
    ((cyclesVolume, "cycles:volume"))
    (UsdPreviewSurface)
    (UsdUVTexture)
    (UsdPrimvarReader_float)
    (UsdPrimvarReader_float2)
    (UsdPrimvarReader_float3)
    (UsdPrimvarReader_float4)
    (UsdPrimvarReader_int)
    (UsdTransform2d)
    (a)
    (rgb)
    (r)
    (g)
    (b)
    (result)
    (st)
    (wrapS)
    (wrapT)
    (periodic)
);
// clang-format on

// Simple class to handle remapping of USDPreviewSurface nodes and parameters to Cycles equivalents
class UsdToCyclesMapping {
  using ParamMap = std::unordered_map<TfToken, ustring, TfToken::HashFunctor>;

 public:
  UsdToCyclesMapping(const char *nodeType, ParamMap paramMap)
      : _nodeType(nodeType), _paramMap(std::move(paramMap))
  {
  }

  ustring nodeType() const
  {
    return _nodeType;
  }

  virtual std::string parameterName(const TfToken &name,
                                    const ShaderInput *inputConnection,
                                    VtValue *value = nullptr) const
  {
    // UsdNode.name -> Node.input
    // These all follow a simple pattern that we can just remap
    // based on the name or 'Node.input' type
    if (inputConnection) {
      if (name == CyclesMaterialTokens->a) {
        return "alpha";
      }
      if (name == CyclesMaterialTokens->rgb) {
        return "color";
      }
      // TODO: Is there a better mapping than 'color'?
      if (name == CyclesMaterialTokens->r || name == CyclesMaterialTokens->g ||
          name == CyclesMaterialTokens->b)
      {
        return "color";
      }

      if (name == CyclesMaterialTokens->result) {
        switch (inputConnection->socket_type.type) {
          case SocketType::BOOLEAN:
          case SocketType::FLOAT:
          case SocketType::INT:
          case SocketType::UINT:
            return "alpha";
          case SocketType::COLOR:
          case SocketType::VECTOR:
          case SocketType::POINT:
          case SocketType::NORMAL:
          default:
            return "color";
        }
      }
    }

    // Simple mapping case
    const auto it = _paramMap.find(name);
    return it != _paramMap.end() ? it->second.string() : name.GetString();
  }

 private:
  const ustring _nodeType;
  ParamMap _paramMap;
};

class UsdToCyclesTexture : public UsdToCyclesMapping {
 public:
  using UsdToCyclesMapping::UsdToCyclesMapping;

  std::string parameterName(const TfToken &name,
                            const ShaderInput *inputConnection,
                            VtValue *value) const override
  {
    if (value) {
      // Remap UsdUVTexture.wrapS and UsdUVTexture.wrapT to cycles_image_texture.extension
      if (name == CyclesMaterialTokens->wrapS || name == CyclesMaterialTokens->wrapT) {
        std::string valueString = VtValue::Cast<std::string>(*value).Get<std::string>();

        // A value of 'repeat' in USD is equivalent to 'periodic' in Cycles
        if (valueString == "repeat") {
          *value = VtValue(CyclesMaterialTokens->periodic);
        }

        return "extension";
      }
    }

    return UsdToCyclesMapping::parameterName(name, inputConnection, value);
  }
};

namespace {

class UsdToCycles {
  const UsdToCyclesMapping UsdPreviewSurface = {
      "principled_bsdf",
      {
          {TfToken("diffuseColor"), ustring("base_color")},
          {TfToken("emissiveColor"), ustring("emission")},
          {TfToken("specularColor"), ustring("specular")},
          {TfToken("clearcoatRoughness"), ustring("coat_roughness")},
          {TfToken("opacity"), ustring("alpha")},
          // opacityThreshold
          // occlusion
          // displacement
      }};
  const UsdToCyclesTexture UsdUVTexture = {
      "image_texture",
      {
          {CyclesMaterialTokens->st, ustring("vector")},
          {CyclesMaterialTokens->wrapS, ustring("extension")},
          {CyclesMaterialTokens->wrapT, ustring("extension")},
          {TfToken("file"), ustring("filename")},
          {TfToken("sourceColorSpace"), ustring("colorspace")},
      }};
  const UsdToCyclesMapping UsdPrimvarReader = {"attribute",
                                               {{TfToken("varname"), ustring("attribute")}}};

 public:
  const UsdToCyclesMapping *findUsd(const TfToken &usdNodeType)
  {
    if (usdNodeType == CyclesMaterialTokens->UsdPreviewSurface) {
      return &UsdPreviewSurface;
    }
    if (usdNodeType == CyclesMaterialTokens->UsdUVTexture) {
      return &UsdUVTexture;
    }
    if (usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float2 ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float3 ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float4 ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_int)
    {
      return &UsdPrimvarReader;
    }

    return nullptr;
  }
  const UsdToCyclesMapping *findCycles(const ustring &cyclesNodeType)
  {
    return nullptr;
  }
};
TfStaticData<UsdToCycles> sUsdToCyles;

}  // namespace

HdCyclesMaterial::HdCyclesMaterial(const SdfPath &sprimId) : HdMaterial(sprimId) {}

HdCyclesMaterial::~HdCyclesMaterial() {}

HdDirtyBits HdCyclesMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyResource | DirtyBits::DirtyParams;
}

void HdCyclesMaterial::Sync(HdSceneDelegate *sceneDelegate,
                            HdRenderParam *renderParam,
                            HdDirtyBits *dirtyBits)
{
  if (*dirtyBits == DirtyBits::Clean) {
    return;
  }

  Initialize(renderParam);

  const SceneLock lock(renderParam);

  const bool dirtyParams = (*dirtyBits & DirtyBits::DirtyParams);
  const bool dirtyResource = (*dirtyBits & DirtyBits::DirtyResource);

  VtValue value;
  const SdfPath &id = GetId();

  if (dirtyResource || dirtyParams) {
    value = sceneDelegate->GetMaterialResource(id);

#if 1
    const HdMaterialNetwork2 *network = nullptr;
    std::unique_ptr<HdMaterialNetwork2> networkConverted;
    if (value.IsHolding<HdMaterialNetwork2>()) {
      network = &value.UncheckedGet<HdMaterialNetwork2>();
    }
    else if (value.IsHolding<HdMaterialNetworkMap>()) {
      const auto &networkOld = value.UncheckedGet<HdMaterialNetworkMap>();
      // In the case of only parameter updates, there is no need to waste time converting to a
      // HdMaterialNetwork2, as supporting HdMaterialNetworkMap for parameters only is trivial.
      if (!_nodes.empty() && !dirtyResource) {
        for (const auto &networkEntry : networkOld.map) {
          UpdateParameters(networkEntry.second);
        }
        _shader->tag_modified();
      }
      else {
        networkConverted = std::make_unique<HdMaterialNetwork2>();
#  if PXR_VERSION >= 2205
        *networkConverted = HdConvertToHdMaterialNetwork2(networkOld);
#  else
        HdMaterialNetwork2ConvertFromHdMaterialNetworkMap(networkOld, networkConverted.get());
#  endif
        network = networkConverted.get();
      }
    }
    else {
      TF_RUNTIME_ERROR("Could not get a HdMaterialNetwork2.");
    }

    if (network) {
      if (!_nodes.empty() && !dirtyResource) {
        UpdateParameters(*network);
        _shader->tag_modified();
      }
      else {
        PopulateShaderGraph(*network);
      }
    }
#endif
  }

  if (_shader->is_modified()) {
    _shader->tag_update(lock.scene);
  }

  *dirtyBits = DirtyBits::Clean;
}

void HdCyclesMaterial::UpdateParameters(NodeDesc &nodeDesc,
                                        const std::map<TfToken, VtValue> &parameters,
                                        const SdfPath &nodePath)
{
  for (const auto &param : parameters) {
    VtValue value = param.second;

    // See if the parameter name is in USDPreviewSurface terms, and needs to be converted
    const UsdToCyclesMapping *inputMapping = nodeDesc.mapping;
    const std::string inputName = inputMapping ?
                                      inputMapping->parameterName(param.first, nullptr, &value) :
                                      param.first.GetString();

    // Find the input to write the parameter value to
    const SocketType *input = nullptr;
    for (const SocketType &socket : nodeDesc.node->type->inputs) {
      if (string_iequals(socket.name.string(), inputName) || socket.ui_name == inputName) {
        input = &socket;
        break;
      }
    }

    if (!input) {
      TF_WARN("Could not find parameter '%s' on node '%s' ('%s')",
              param.first.GetText(),
              nodePath.GetText(),
              nodeDesc.node->name.c_str());
      continue;
    }

    SetNodeValue(nodeDesc.node, *input, value);
  }
}

void HdCyclesMaterial::UpdateParameters(const HdMaterialNetwork &network)
{
  for (const HdMaterialNode &nodeEntry : network.nodes) {
    const SdfPath &nodePath = nodeEntry.path;

    const auto nodeIt = _nodes.find(nodePath);
    if (nodeIt == _nodes.end()) {
      TF_RUNTIME_ERROR("Could not update parameters on missing node '%s'", nodePath.GetText());
      continue;
    }

    UpdateParameters(nodeIt->second, nodeEntry.parameters, nodePath);
  }
}

void HdCyclesMaterial::UpdateParameters(const HdMaterialNetwork2 &network)
{
  for (const auto &nodeEntry : network.nodes) {
    const SdfPath &nodePath = nodeEntry.first;

    const auto nodeIt = _nodes.find(nodePath);
    if (nodeIt == _nodes.end()) {
      TF_RUNTIME_ERROR("Could not update parameters on missing node '%s'", nodePath.GetText());
      continue;
    }

    UpdateParameters(nodeIt->second, nodeEntry.second.parameters, nodePath);
  }
}

void HdCyclesMaterial::UpdateConnections(NodeDesc &nodeDesc,
                                         const HdMaterialNode2 &matNode,
                                         const SdfPath &nodePath,
                                         ShaderGraph *shaderGraph)
{
  for (const auto &connection : matNode.inputConnections) {
    const TfToken &dstSocketName = connection.first;

    const UsdToCyclesMapping *inputMapping = nodeDesc.mapping;
    const std::string inputName = inputMapping ?
                                      inputMapping->parameterName(dstSocketName, nullptr) :
                                      dstSocketName.GetString();

    // Find the input to connect to on the passed in node
    ShaderInput *input = nullptr;
    for (ShaderInput *in : nodeDesc.node->inputs) {
      if (string_iequals(in->socket_type.name.string(), inputName)) {
        input = in;
        break;
      }
    }

    if (!input) {
      TF_WARN("Ignoring connection on '%s.%s', input '%s' was not found",
              nodePath.GetText(),
              dstSocketName.GetText(),
              dstSocketName.GetText());
      continue;
    }

    // Now find the output to connect from
    const auto &connectedNodes = connection.second;
    if (connectedNodes.empty()) {
      continue;
    }

    // TODO: Hydra allows multiple connections of the same input
    //       Unsure how to handle this in Cycles, so just use the first
    if (connectedNodes.size() > 1) {
      TF_WARN(
          "Ignoring multiple connections to '%s.%s'", nodePath.GetText(), dstSocketName.GetText());
    }

    const SdfPath &upstreamNodePath = connectedNodes.front().upstreamNode;
    const TfToken &upstreamOutputName = connectedNodes.front().upstreamOutputName;

    const auto srcNodeIt = _nodes.find(upstreamNodePath);
    if (srcNodeIt == _nodes.end()) {
      TF_WARN("Ignoring connection from '%s.%s' to '%s.%s', node '%s' was not found",
              upstreamNodePath.GetText(),
              upstreamOutputName.GetText(),
              nodePath.GetText(),
              dstSocketName.GetText(),
              upstreamNodePath.GetText());
      continue;
    }

    const UsdToCyclesMapping *outputMapping = srcNodeIt->second.mapping;
    const std::string outputName = outputMapping ?
                                       outputMapping->parameterName(upstreamOutputName, input) :
                                       upstreamOutputName.GetString();

    ShaderOutput *output = nullptr;
    for (ShaderOutput *out : srcNodeIt->second.node->outputs) {
      if (string_iequals(out->socket_type.name.string(), outputName)) {
        output = out;
        break;
      }
    }

    if (!output) {
      TF_WARN("Ignoring connection from '%s.%s' to '%s.%s', output '%s' was not found",
              upstreamNodePath.GetText(),
              upstreamOutputName.GetText(),
              nodePath.GetText(),
              dstSocketName.GetText(),
              upstreamOutputName.GetText());
      continue;
    }

    shaderGraph->connect(output, input);
  }
}

void HdCyclesMaterial::PopulateShaderGraph(const HdMaterialNetwork2 &networkMap)
{
  _nodes.clear();

  auto graph = new ShaderGraph();

  // Iterate all the nodes first and build a complete but unconnected graph with parameters set
  for (const auto &nodeEntry : networkMap.nodes) {
    NodeDesc nodeDesc = {};
    const SdfPath &nodePath = nodeEntry.first;

    const auto nodeIt = _nodes.find(nodePath);
    // Create new node only if it does not exist yet
    if (nodeIt != _nodes.end()) {
      nodeDesc = nodeIt->second;
    }
    else {
      // E.g. cycles_principled_bsdf or UsdPreviewSurface
      const std::string &nodeTypeId = nodeEntry.second.nodeTypeId.GetString();

      ustring cyclesType(nodeTypeId);
      // Interpret a node type ID prefixed with cycles_<type> or cycles:<type> as a node of <type>
      if (nodeTypeId.rfind("cycles", 0) == 0) {
        cyclesType = nodeTypeId.substr(7);
        nodeDesc.mapping = sUsdToCyles->findCycles(cyclesType);
      }
      else {
        // Check if any remapping is needed (e.g. for USDPreviewSurface to Cycles nodes)
        nodeDesc.mapping = sUsdToCyles->findUsd(nodeEntry.second.nodeTypeId);
        if (nodeDesc.mapping) {
          cyclesType = nodeDesc.mapping->nodeType();
        }
      }

      // If it's a native Cycles' node-type, just do the lookup now.
      if (const NodeType *nodeType = NodeType::find(cyclesType)) {
        nodeDesc.node = static_cast<ShaderNode *>(nodeType->create(nodeType));
        nodeDesc.node->set_owner(graph);

        graph->add(nodeDesc.node);

        _nodes.emplace(nodePath, nodeDesc);
      }
      else {
        TF_RUNTIME_ERROR("Could not create node '%s'", nodePath.GetText());
        continue;
      }
    }

    UpdateParameters(nodeDesc, nodeEntry.second.parameters, nodePath);
  }

  // Now that all nodes have been constructed, iterate the network again and build up any
  // connections between nodes
  for (const auto &nodeEntry : networkMap.nodes) {
    const SdfPath &nodePath = nodeEntry.first;

    const auto nodeIt = _nodes.find(nodePath);
    if (nodeIt == _nodes.end()) {
      TF_RUNTIME_ERROR("Could not find node '%s' to connect", nodePath.GetText());
      continue;
    }

    UpdateConnections(nodeIt->second, nodeEntry.second, nodePath, graph);
  }

  // Finally connect the terminals to the graph output (Surface, Volume, Displacement)
  for (const auto &terminalEntry : networkMap.terminals) {
    const TfToken &terminalName = terminalEntry.first;
    const HdMaterialConnection2 &connection = terminalEntry.second;

    const auto nodeIt = _nodes.find(connection.upstreamNode);
    if (nodeIt == _nodes.end()) {
      TF_RUNTIME_ERROR("Could not find terminal node '%s'", connection.upstreamNode.GetText());
      continue;
    }

    ShaderNode *const node = nodeIt->second.node;

    const char *inputName = nullptr;
    const char *outputName = nullptr;
    if (terminalName == HdMaterialTerminalTokens->surface ||
        terminalName == CyclesMaterialTokens->cyclesSurface)
    {
      inputName = "Surface";
      // Find default output name based on the node if none is provided
      if (node->type->name == "add_closure" || node->type->name == "mix_closure") {
        outputName = "Closure";
      }
      else if (node->type->name == "emission") {
        outputName = "Emission";
      }
      else {
        outputName = "BSDF";
      }
    }
    else if (terminalName == HdMaterialTerminalTokens->displacement ||
             terminalName == CyclesMaterialTokens->cyclesDisplacement)
    {
      inputName = outputName = "Displacement";
    }
    else if (terminalName == HdMaterialTerminalTokens->volume ||
             terminalName == CyclesMaterialTokens->cyclesVolume)
    {
      inputName = outputName = "Volume";
    }

    if (!connection.upstreamOutputName.IsEmpty()) {
      outputName = connection.upstreamOutputName.GetText();
    }

    ShaderInput *const input = inputName ? graph->output()->input(inputName) : nullptr;
    if (!input) {
      TF_RUNTIME_ERROR("Could not find terminal input '%s.%s'",
                       connection.upstreamNode.GetText(),
                       inputName ? inputName : "<null>");
      continue;
    }

    ShaderOutput *const output = outputName ? node->output(outputName) : nullptr;
    if (!output) {
      TF_RUNTIME_ERROR("Could not find terminal output '%s.%s'",
                       connection.upstreamNode.GetText(),
                       outputName ? outputName : "<null>");
      continue;
    }

    graph->connect(output, input);
  }

  // Create the instanceId AOV output
  {
    const ustring instanceId(HdAovTokens->instanceId.GetString());

    OutputAOVNode *aovNode = graph->create_node<OutputAOVNode>();
    aovNode->set_name(instanceId);
    graph->add(aovNode);

    AttributeNode *instanceIdNode = graph->create_node<AttributeNode>();
    instanceIdNode->set_attribute(instanceId);
    graph->add(instanceIdNode);

    graph->connect(instanceIdNode->output("Fac"), aovNode->input("Value"));
  }

  _shader->set_graph(graph);
}

void HdCyclesMaterial::Finalize(HdRenderParam *renderParam)
{
  if (!_shader) {
    return;
  }

  const SceneLock lock(renderParam);
  const bool keep_nodes = static_cast<const HdCyclesSession *>(renderParam)->keep_nodes;

  _nodes.clear();

  if (!keep_nodes) {
    lock.scene->delete_node(_shader);
  }
  _shader = nullptr;
}

void HdCyclesMaterial::Initialize(HdRenderParam *renderParam)
{
  if (_shader) {
    return;
  }

  const SceneLock lock(renderParam);

  _shader = lock.scene->create_node<Shader>();
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE

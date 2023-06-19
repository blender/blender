/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/session.h"
#include "scene/shader.h"
// Have to include shader.h before background.h so that 'set_shader' uses the correct 'set'
// overload taking a 'Node *', rather than the one taking a 'bool'
#include "scene/background.h"
#include "scene/light.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "session/session.h"

HDCYCLES_NAMESPACE_OPEN_SCOPE

namespace {

const std::unordered_map<TfToken, PassType, TfToken::HashFunctor> kAovToPass = {
    {HdAovTokens->color, PASS_COMBINED},
    {HdAovTokens->depth, PASS_DEPTH},
    {HdAovTokens->normal, PASS_NORMAL},
    {HdAovTokens->primId, PASS_OBJECT_ID},
    {HdAovTokens->instanceId, PASS_AOV_VALUE},
};

}  // namespace

SceneLock::SceneLock(const HdRenderParam *renderParam)
    : scene(static_cast<const HdCyclesSession *>(renderParam)->session->scene),
      sceneLock(scene->mutex)
{
}

SceneLock::~SceneLock() {}

HdCyclesSession::HdCyclesSession(Session *session_, const bool keep_nodes)
    : session(session_), keep_nodes(true), _ownCyclesSession(false)
{
}

HdCyclesSession::HdCyclesSession(const SessionParams &params)
    : session(new Session(params, SceneParams())), keep_nodes(false), _ownCyclesSession(true)
{
  Scene *const scene = session->scene;

  // Create background with ambient light
  {
    ShaderGraph *graph = new ShaderGraph();

    BackgroundNode *bgNode = graph->create_node<BackgroundNode>();
    bgNode->set_color(one_float3());
    graph->add(bgNode);

    graph->connect(bgNode->output("Background"), graph->output()->input("Surface"));

    scene->default_background->set_graph(graph);
    scene->default_background->tag_update(scene);
  }

  // Wire up object color in default surface material
  {
    ShaderGraph *graph = new ShaderGraph();

    ObjectInfoNode *objectNode = graph->create_node<ObjectInfoNode>();
    graph->add(objectNode);

    DiffuseBsdfNode *diffuseNode = graph->create_node<DiffuseBsdfNode>();
    graph->add(diffuseNode);

    graph->connect(objectNode->output("Color"), diffuseNode->input("Color"));
    graph->connect(diffuseNode->output("BSDF"), graph->output()->input("Surface"));

#if 1
    // Create the instanceId AOV output
    const ustring instanceId(HdAovTokens->instanceId.GetString());

    OutputAOVNode *aovNode = graph->create_node<OutputAOVNode>();
    aovNode->set_name(instanceId);
    graph->add(aovNode);

    AttributeNode *instanceIdNode = graph->create_node<AttributeNode>();
    instanceIdNode->set_attribute(instanceId);
    graph->add(instanceIdNode);

    graph->connect(instanceIdNode->output("Fac"), aovNode->input("Value"));
#endif

    scene->default_surface->set_graph(graph);
    scene->default_surface->tag_update(scene);
  }
}

HdCyclesSession::~HdCyclesSession()
{
  if (_ownCyclesSession) {
    delete session;
  }
}

void HdCyclesSession::UpdateScene()
{
  Scene *const scene = session->scene;

  // Update background depending on presence of a background light
  if (scene->light_manager->need_update()) {
    Light *background_light = nullptr;
    for (Light *light : scene->lights) {
      if (light->get_light_type() == LIGHT_BACKGROUND) {
        background_light = light;
        break;
      }
    }

    if (!background_light) {
      scene->background->set_shader(scene->default_background);
      scene->background->set_transparent(true);
    }
    else {
      scene->background->set_shader(background_light->get_shader());
      scene->background->set_transparent(false);
    }

    scene->background->tag_update(scene);
  }
}

void HdCyclesSession::SyncAovBindings(const HdRenderPassAovBindingVector &aovBindings)
{
  Scene *const scene = session->scene;

  // Delete all existing passes
  scene->delete_nodes(set<Pass *>(scene->passes.begin(), scene->passes.end()));

  // Update passes with requested AOV bindings
  _aovBindings = aovBindings;
  for (const HdRenderPassAovBinding &aovBinding : aovBindings) {
    const auto cyclesAov = kAovToPass.find(aovBinding.aovName);
    if (cyclesAov == kAovToPass.end()) {
      // TODO: Use PASS_AOV_COLOR and PASS_AOV_VALUE for these?
      TF_WARN("Unknown pass %s", aovBinding.aovName.GetText());
      continue;
    }

    const PassType type = cyclesAov->second;
    const PassMode mode = PassMode::DENOISED;

    Pass *pass = scene->create_node<Pass>();
    pass->set_type(type);
    pass->set_mode(mode);
    pass->set_name(ustring(aovBinding.aovName.GetString()));
  }
}

void HdCyclesSession::RemoveAovBinding(HdRenderBuffer *renderBuffer)
{
  for (HdRenderPassAovBinding &aovBinding : _aovBindings) {
    if (renderBuffer == aovBinding.renderBuffer) {
      aovBinding.renderBuffer = nullptr;
      break;
    }
  }

  if (renderBuffer == _displayAovBinding.renderBuffer) {
    _displayAovBinding.renderBuffer = nullptr;
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE

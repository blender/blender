/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/light.h"
#include "hydra/session.h"
#include "scene/light.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/hash.h"

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/usd/sdf/assetPath.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

extern Transform convert_transform(const GfMatrix4d &matrix);

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (visibleInPrimaryRay)
);
// clang-format on

HdCyclesLight::HdCyclesLight(const SdfPath &sprimId, const TfToken &lightType)
    : HdLight(sprimId), _lightType(lightType)
{
}

HdCyclesLight::~HdCyclesLight() {}

HdDirtyBits HdCyclesLight::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyTransform | DirtyBits::DirtyParams;
}

void HdCyclesLight::Sync(HdSceneDelegate *sceneDelegate,
                         HdRenderParam *renderParam,
                         HdDirtyBits *dirtyBits)
{
  if (*dirtyBits == DirtyBits::Clean) {
    return;
  }

  Initialize(renderParam);

  const SceneLock lock(renderParam);

  VtValue value;
  const SdfPath &id = GetId();

  if (*dirtyBits & DirtyBits::DirtyTransform) {
    const float metersPerUnit =
        static_cast<HdCyclesSession *>(renderParam)->GetStageMetersPerUnit();

    const Transform tfm = transform_scale(make_float3(metersPerUnit)) *
#if PXR_VERSION >= 2011
                          convert_transform(sceneDelegate->GetTransform(id));
#else
                          convert_transform(
                              sceneDelegate->GetLightParamValue(id, HdTokens->transform)
                                  .Get<GfMatrix4d>());
#endif
    _light->set_tfm(tfm);

    _light->set_co(transform_get_column(&tfm, 3));
    _light->set_dir(-transform_get_column(&tfm, 2));

    if (_lightType == HdPrimTypeTokens->diskLight || _lightType == HdPrimTypeTokens->rectLight) {
      _light->set_axisu(transform_get_column(&tfm, 0));
      _light->set_axisv(transform_get_column(&tfm, 1));
    }
  }

  if (*dirtyBits & DirtyBits::DirtyParams) {
    float3 strength = make_float3(1.0f, 1.0f, 1.0f);

    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->color);
    if (!value.IsEmpty()) {
      const auto color = value.Get<GfVec3f>();
      strength = make_float3(color[0], color[1], color[2]);
    }

    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure);
    if (!value.IsEmpty()) {
      strength *= exp2(value.Get<float>());
    }

    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity);
    if (!value.IsEmpty()) {
      strength *= value.Get<float>();
    }

    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize);
    _light->set_normalize(value.IsHolding<bool>() && value.UncheckedGet<bool>());

    value = sceneDelegate->GetLightParamValue(id, _tokens->visibleInPrimaryRay);
    if (!value.IsEmpty()) {
      _light->set_use_camera(value.Get<bool>());
    }

    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->shadowEnable);
    if (!value.IsEmpty()) {
      _light->set_cast_shadow(value.Get<bool>());
    }

    if (_lightType == HdPrimTypeTokens->distantLight) {
      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->angle);
      if (!value.IsEmpty()) {
        _light->set_angle(GfDegreesToRadians(value.Get<float>()));
      }
    }
    else if (_lightType == HdPrimTypeTokens->diskLight) {
      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius);
      if (!value.IsEmpty()) {
        const float size = value.Get<float>() * 2.0f;
        _light->set_sizeu(size);
        _light->set_sizev(size);
      }
    }
    else if (_lightType == HdPrimTypeTokens->rectLight) {
      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->width);
      if (!value.IsEmpty()) {
        _light->set_sizeu(value.Get<float>());
      }

      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->height);
      if (!value.IsEmpty()) {
        _light->set_sizev(value.Get<float>());
      }
    }
    else if (_lightType == HdPrimTypeTokens->sphereLight) {
      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius);
      if (!value.IsEmpty()) {
        _light->set_size(value.Get<float>());
      }

      bool shaping = false;

      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->shapingConeAngle);
      if (!value.IsEmpty()) {
        _light->set_spot_angle(GfDegreesToRadians(value.Get<float>()) * 2.0f);
        shaping = true;
      }

      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->shapingConeSoftness);
      if (!value.IsEmpty()) {
        _light->set_spot_smooth(value.Get<float>());
        shaping = true;
      }

      _light->set_light_type(shaping ? LIGHT_SPOT : LIGHT_POINT);
    }

    const bool visible = sceneDelegate->GetVisible(id);
    // Disable invisible lights by zeroing the strength
    // So 'LightManager::test_enabled_lights' updates the enabled flag correctly
    if (!visible) {
      strength = zero_float3();
    }

    _light->set_strength(strength);
    _light->set_is_enabled(visible);

    PopulateShaderGraph(sceneDelegate);
  }
  // Need to update shader graph when transform changes in case transform was baked into it
  else if (_light->tfm_is_modified() && (_lightType == HdPrimTypeTokens->domeLight ||
                                         _light->get_shader()->has_surface_spatial_varying))
  {
    PopulateShaderGraph(sceneDelegate);
  }

  if (_light->is_modified()) {
    _light->tag_update(lock.scene);
  }

  *dirtyBits = DirtyBits::Clean;
}

void HdCyclesLight::PopulateShaderGraph(HdSceneDelegate *sceneDelegate)
{
  auto graph = new ShaderGraph();
  ShaderNode *outputNode = nullptr;

  if (_lightType == HdPrimTypeTokens->domeLight) {
    BackgroundNode *bgNode = graph->create_node<BackgroundNode>();
    // Bake strength into shader graph, since only the shader is used for background lights
    bgNode->set_color(_light->get_strength());
    graph->add(bgNode);

    graph->connect(bgNode->output("Background"), graph->output()->input("Surface"));

    outputNode = bgNode;
  }
  else {
    EmissionNode *emissionNode = graph->create_node<EmissionNode>();
    emissionNode->set_color(one_float3());
    emissionNode->set_strength(1.0f);
    graph->add(emissionNode);

    graph->connect(emissionNode->output("Emission"), graph->output()->input("Surface"));

    outputNode = emissionNode;
  }

  VtValue value;
  const SdfPath &id = GetId();
  bool hasSpatialVarying = false;
  bool hasColorTemperature = false;

  if (sceneDelegate != nullptr) {
    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->enableColorTemperature);
    const bool enableColorTemperature = value.IsHolding<bool>() && value.UncheckedGet<bool>();

    if (enableColorTemperature) {
      value = sceneDelegate->GetLightParamValue(id, HdLightTokens->colorTemperature);
      if (value.IsHolding<float>()) {
        BlackbodyNode *blackbodyNode = graph->create_node<BlackbodyNode>();
        blackbodyNode->set_temperature(value.UncheckedGet<float>());
        graph->add(blackbodyNode);

        if (_lightType == HdPrimTypeTokens->domeLight) {
          VectorMathNode *mathNode = graph->create_node<VectorMathNode>();
          mathNode->set_math_type(NODE_VECTOR_MATH_MULTIPLY);
          mathNode->set_vector2(_light->get_strength());
          graph->add(mathNode);

          graph->connect(blackbodyNode->output("Color"), mathNode->input("Vector1"));
          graph->connect(mathNode->output("Vector"), outputNode->input("Color"));
        }
        else {
          graph->connect(blackbodyNode->output("Color"), outputNode->input("Color"));
        }

        hasColorTemperature = true;
      }
    }

    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->shapingIesFile);
    if (value.IsHolding<SdfAssetPath>()) {
      std::string filename = value.UncheckedGet<SdfAssetPath>().GetResolvedPath();
      if (filename.empty()) {
        filename = value.UncheckedGet<SdfAssetPath>().GetAssetPath();
      }

      TextureCoordinateNode *coordNode = graph->create_node<TextureCoordinateNode>();
      coordNode->set_ob_tfm(_light->get_tfm());
      coordNode->set_use_transform(true);
      graph->add(coordNode);

      IESLightNode *iesNode = graph->create_node<IESLightNode>();
      iesNode->set_filename(ustring(filename));

      graph->connect(coordNode->output("Normal"), iesNode->input("Vector"));
      graph->connect(iesNode->output("Fac"), outputNode->input("Strength"));

      hasSpatialVarying = true;
    }

    value = sceneDelegate->GetLightParamValue(id, HdLightTokens->textureFile);
    if (value.IsHolding<SdfAssetPath>()) {
      std::string filename = value.UncheckedGet<SdfAssetPath>().GetResolvedPath();
      if (filename.empty()) {
        filename = value.UncheckedGet<SdfAssetPath>().GetAssetPath();
      }

      ImageSlotTextureNode *textureNode = nullptr;
      if (_lightType == HdPrimTypeTokens->domeLight) {
        Transform tfm = _light->get_tfm();
        transform_set_column(&tfm, 3, zero_float3());  // Remove translation

        TextureCoordinateNode *coordNode = graph->create_node<TextureCoordinateNode>();
        coordNode->set_ob_tfm(tfm);
        coordNode->set_use_transform(true);
        graph->add(coordNode);

        textureNode = graph->create_node<EnvironmentTextureNode>();
        static_cast<EnvironmentTextureNode *>(textureNode)->set_filename(ustring(filename));
        graph->add(textureNode);

        graph->connect(coordNode->output("Object"), textureNode->input("Vector"));

        hasSpatialVarying = true;
      }
      else {
        GeometryNode *coordNode = graph->create_node<GeometryNode>();
        graph->add(coordNode);

        textureNode = graph->create_node<ImageTextureNode>();
        static_cast<ImageTextureNode *>(textureNode)->set_filename(ustring(filename));
        graph->add(textureNode);

        graph->connect(coordNode->output("Parametric"), textureNode->input("Vector"));
      }

      if (hasColorTemperature) {
        VectorMathNode *mathNode = graph->create_node<VectorMathNode>();
        mathNode->set_math_type(NODE_VECTOR_MATH_MULTIPLY);
        graph->add(mathNode);

        graph->connect(textureNode->output("Color"), mathNode->input("Vector1"));
        ShaderInput *const outputNodeInput = outputNode->input("Color");
        graph->connect(outputNodeInput->link, mathNode->input("Vector2"));
        graph->disconnect(outputNodeInput);
        graph->connect(mathNode->output("Vector"), outputNodeInput);
      }
      else if (_lightType == HdPrimTypeTokens->domeLight) {
        VectorMathNode *mathNode = graph->create_node<VectorMathNode>();
        mathNode->set_math_type(NODE_VECTOR_MATH_MULTIPLY);
        mathNode->set_vector2(_light->get_strength());
        graph->add(mathNode);

        graph->connect(textureNode->output("Color"), mathNode->input("Vector1"));
        graph->connect(mathNode->output("Vector"), outputNode->input("Color"));
      }
      else {
        graph->connect(textureNode->output("Color"), outputNode->input("Color"));
      }
    }
  }

  Shader *const shader = _light->get_shader();
  shader->set_graph(graph);
  shader->tag_update((Scene *)_light->get_owner());

  shader->has_surface_spatial_varying = hasSpatialVarying;
}

void HdCyclesLight::Finalize(HdRenderParam *renderParam)
{
  if (!_light) {
    return;
  }

  const SceneLock lock(renderParam);
  const bool keep_nodes = static_cast<const HdCyclesSession *>(renderParam)->keep_nodes;

  if (!keep_nodes) {
    lock.scene->delete_node(_light);
  }

  _light = nullptr;
}

void HdCyclesLight::Initialize(HdRenderParam *renderParam)
{
  if (_light) {
    return;
  }

  const SceneLock lock(renderParam);

  _light = lock.scene->create_node<Light>();
  _light->name = GetId().GetString();

  _light->set_random_id(hash_uint2(hash_string(_light->name.c_str()), 0));

  if (_lightType == HdPrimTypeTokens->domeLight) {
    _light->set_light_type(LIGHT_BACKGROUND);
  }
  else if (_lightType == HdPrimTypeTokens->distantLight) {
    _light->set_light_type(LIGHT_DISTANT);
  }
  else if (_lightType == HdPrimTypeTokens->diskLight) {
    _light->set_light_type(LIGHT_AREA);
    _light->set_ellipse(true);
    _light->set_size(1.0f);
  }
  else if (_lightType == HdPrimTypeTokens->rectLight) {
    _light->set_light_type(LIGHT_AREA);
    _light->set_ellipse(false);
    _light->set_size(1.0f);
  }
  else if (_lightType == HdPrimTypeTokens->sphereLight) {
    _light->set_light_type(LIGHT_POINT);
    _light->set_size(1.0f);
  }

  _light->set_use_mis(true);
  _light->set_use_camera(false);

  Shader *const shader = lock.scene->create_node<Shader>();
  _light->set_shader(shader);

  // Create default shader graph
  PopulateShaderGraph(nullptr);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE

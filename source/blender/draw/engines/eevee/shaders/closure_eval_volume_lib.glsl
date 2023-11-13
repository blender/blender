/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void output_aov(vec4 color, float value, uint hash)
{
  /* Unsupported. */
}

/* Surface BSDFs. */
Closure closure_eval(ClosureDiffuse diffuse)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureTranslucent translucent)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureReflection reflection)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureRefraction refraction)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureEmission emission)
{
  Closure closure = CLOSURE_DEFAULT;
  closure.emission = emission.emission;
  return closure;
}
Closure closure_eval(ClosureTransparency transparency)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureReflection reflection, ClosureRefraction refraction)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureDiffuse diffuse, ClosureReflection reflection)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureDiffuse diffuse, ClosureReflection reflection, ClosureReflection coat)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureDiffuse diffuse,
                     ClosureReflection reflection,
                     ClosureReflection coat,
                     ClosureRefraction refraction)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureReflection reflection, ClosureReflection coat)
{
  return CLOSURE_DEFAULT;
}
Closure closure_eval(ClosureHair hair)
{
  return CLOSURE_DEFAULT;
}

Closure closure_eval(ClosureVolumeScatter volume_scatter)
{
  Closure closure = CLOSURE_DEFAULT;
  closure.scatter = volume_scatter.scattering;
  closure.anisotropy = volume_scatter.anisotropy;
  return closure;
}
Closure closure_eval(ClosureVolumeAbsorption volume_absorption)
{
  Closure closure = CLOSURE_DEFAULT;
  closure.absorption = volume_absorption.absorption;
  return closure;
}
Closure closure_eval(ClosureVolumeScatter volume_scatter,
                     ClosureVolumeAbsorption volume_absorption,
                     ClosureEmission emission)
{
  Closure closure = CLOSURE_DEFAULT;
  closure.absorption = volume_absorption.absorption;
  closure.scatter = volume_scatter.scattering;
  closure.anisotropy = volume_scatter.anisotropy;
  closure.emission = emission.emission;
  return closure;
}

vec4 closure_to_rgba(Closure closure)
{
  /* Not supported */
  return vec4(0.0);
}

Closure closure_mix(inout Closure cl1, inout Closure cl2, float fac)
{
  Closure cl;
  cl.absorption = mix(cl1.absorption, cl2.absorption, fac);
  cl.scatter = mix(cl1.scatter, cl2.scatter, fac);
  cl.emission = mix(cl1.emission, cl2.emission, fac);
  cl.anisotropy = mix(cl1.anisotropy, cl2.anisotropy, fac);
  return cl;
}

Closure closure_add(inout Closure cl1, inout Closure cl2)
{
  Closure cl;
  cl.absorption = cl1.absorption + cl2.absorption;
  cl.scatter = cl1.scatter + cl2.scatter;
  cl.emission = cl1.emission + cl2.emission;
  cl.anisotropy = (cl1.anisotropy + cl2.anisotropy) / 2.0; /* Average phase (no multi lobe) */
  return cl;
}

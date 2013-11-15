/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/* note: the interfaces here are just as an example, need to figure
 * out the right functions and parameters to use */

/* ISOTROPIC VOLUME CLOSURE */

ccl_device int volume_isotropic_setup(ShaderClosure *sc, float density)
{
	sc->type = CLOSURE_VOLUME_ISOTROPIC_ID;
	sc->data0 = density;

	return SD_VOLUME;
}

ccl_device float3 volume_isotropic_eval_phase(const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	return make_float3(1.0f, 1.0f, 1.0f);
}

/* TRANSPARENT VOLUME CLOSURE */

ccl_device int volume_transparent_setup(ShaderClosure *sc, float density)
{
	sc->type = CLOSURE_VOLUME_TRANSPARENT_ID;
	sc->data0 = density;

	return SD_VOLUME;
}

ccl_device float3 volume_transparent_eval_phase(const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	return make_float3(1.0f, 1.0f, 1.0f);
}

/* VOLUME CLOSURE */

ccl_device float3 volume_eval_phase(KernelGlobals *kg, const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
#ifdef __OSL__
	if(kg->osl && sc->prim)
		return OSLShader::volume_eval_phase(sc, omega_in, omega_out);
#endif

	float3 eval;

	switch(sc->type) {
		case CLOSURE_VOLUME_ISOTROPIC_ID:
			eval = volume_isotropic_eval_phase(sc, omega_in, omega_out);
			break;
		case CLOSURE_VOLUME_TRANSPARENT_ID:
			eval = volume_transparent_eval_phase(sc, omega_in, omega_out);
			break;
		default:
			eval = make_float3(0.0f, 0.0f, 0.0f);
			break;
	}

	return eval;
}

CCL_NAMESPACE_END


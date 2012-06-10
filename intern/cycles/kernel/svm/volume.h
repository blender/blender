/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

CCL_NAMESPACE_BEGIN

/* note: the interfaces here are just as an example, need to figure
 * out the right functions and parameters to use */

/* ISOTROPIC VOLUME CLOSURE */

__device void volume_isotropic_setup(ShaderData *sd, ShaderClosure *sc, float density)
{
	sc->type = CLOSURE_VOLUME_ISOTROPIC_ID;
	sd->flag |= SD_VOLUME;
	sc->data0 = density;
}

__device float3 volume_isotropic_eval_phase(const ShaderData *sd, const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	return make_float3(1.0f, 1.0f, 1.0f);
}

/* TRANSPARENT VOLUME CLOSURE */

__device void volume_transparent_setup(ShaderData *sd, ShaderClosure *sc, float density)
{
	sc->type = CLOSURE_VOLUME_TRANSPARENT_ID;
	sd->flag |= SD_VOLUME;
	sc->data0 = density;
}

__device float3 volume_transparent_eval_phase(const ShaderData *sd, const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	return make_float3(1.0f, 1.0f, 1.0f);
}

/* VOLUME CLOSURE */

__device float3 volume_eval_phase(const ShaderData *sd, const ShaderClosure *sc, const float3 omega_in, const float3 omega_out)
{
	float3 eval;

	switch(sc->type) {
		case CLOSURE_VOLUME_ISOTROPIC_ID:
			eval = volume_isotropic_eval_phase(sd, sc, omega_in, omega_out);
			break;
		case CLOSURE_VOLUME_TRANSPARENT_ID:
			eval = volume_transparent_eval_phase(sd, sc, omega_in, omega_out);
			break;
		default:
			eval = make_float3(0.0f, 0.0f, 0.0f);
			break;
	}

	return eval;
}

CCL_NAMESPACE_END


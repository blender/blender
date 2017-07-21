/* Based on work from Morgan McGuire and Michael Mara at Williams College 2014
 * Released as open source under the BSD 2-Clause License
 * http://opensource.org/licenses/BSD-2-Clause
 * http://casual-effects.blogspot.fr/2014/08/screen-space-ray-tracing.html */

#define MAX_STEP 256
#define MAX_REFINE_STEP 32 /* Should be max allowed stride */

uniform mat4 PixelProjMatrix; /* View > NDC > Texel : maps view coords to texel coord */
uniform vec2 ssrParameters;

#define ssrStride     ssrParameters.x
#define ssrThickness  ssrParameters.y

void swapIfBigger(inout float a, inout float b)
{
	if (a > b) {
		float temp = a;
		a = b;
		b = temp;
	}
}

/* Return the length of the ray if there is a hit, and -1.0 if not hit occured */
float raycast(sampler2D depth_texture, vec3 ray_origin, vec3 ray_dir)
{
	float near = get_view_z_from_depth(0.0); /* TODO optimize */

	/* Clip ray to a near plane in 3D */
	float ray_length = 1e16;
	if ((ray_origin.z + ray_dir.z * ray_length) > near)
		ray_length = (near - ray_origin.z) / ray_dir.z;

	vec3 ray_end = ray_dir * ray_length + ray_origin;

	/* Project into screen space */
	vec4 H0 = PixelProjMatrix * vec4(ray_origin, 1.0);
	vec4 H1 = PixelProjMatrix * vec4(ray_end, 1.0);

	/* There are a lot of divisions by w that can be turned into multiplications
	* at some minor precision loss...and we need to interpolate these 1/w values
	* anyway. */
	float k0 = 1.0 / H0.w;
	float k1 = 1.0 / H1.w;

	/* Switch the original points to values that interpolate linearly in 2D */
	vec3 Q0 = ray_origin * k0;
	vec3 Q1 = ray_end * k1;

	/* Screen-space endpoints */
	vec2 P0 = H0.xy * k0;
	vec2 P1 = H1.xy * k1;

	/* [Optional clipping to frustum sides here] */

	/* If the line is degenerate, make it cover at least one pixel
	 * to not have to handle zero-pixel extent as a special case later */
	P1 += vec2((distance_squared(P0, P1) < 0.0001) ? 0.01 : 0.0);

	vec2 delta = P1 - P0;

	/* Permute so that the primary iteration is in x to reduce large branches later.
	 * After this, "x" is the primary iteration direction and "y" is the secondary one
	 * If it is a more-vertical line, create a permutation that swaps x and y in the output
	 * and directly swizzle the inputs. */
	bool permute = false;
	if (abs(delta.x) < abs(delta.y)) {
		permute = true;
		delta = delta.yx;
		P1 = P1.yx;
		P0 = P0.yx;
	}

	/* Track the derivatives */
	float step_sign = sign(delta.x);
	float invdx = step_sign / delta.x;
	vec2 dP = vec2(step_sign, invdx * delta.y);
	vec3 dQ = (Q1 - Q0) * invdx;
	float dk = (k1 - k0) * invdx;

	/* Slide each value from the start of the ray to the end */
	vec4 pqk = vec4(P0, Q0.z, k0);

	/* Scale derivatives by the desired pixel stride */
	vec4 dPQK = vec4(dP, dQ.z, dk) * ssrStride;

	/* We track the ray depth at +/- 1/2 pixel to treat pixels as clip-space solid
	 * voxels. Because the depth at -1/2 for a given pixel will be the same as at
	 * +1/2 for the previous iteration, we actually only have to compute one value
	 * per iteration. */
	float prev_zmax = ray_origin.z;
	float zmax;

	/* P1.x is never modified after this point, so pre-scale it by
	 * the step direction for a signed comparison */
	float end = P1.x * step_sign;

	bool hit = false;
	float raw_depth;
	for (float hitstep = 0.0; hitstep < MAX_STEP && !hit; hitstep++) {
		/* Ray finished & no hit*/
		if ((pqk.x * step_sign) > end) break;

		/* step through current cell */
		pqk += dPQK;

		ivec2 hitpixel = ivec2(permute ? pqk.yx : pqk.xy);
		raw_depth = texelFetch(depth_texture, ivec2(hitpixel), 0).r;

		float zmin = prev_zmax;
		zmax = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);
		prev_zmax = zmax;
		swapIfBigger(zmin, zmax); /* ??? why don't we need this ??? */

		float vmax = get_view_z_from_depth(raw_depth);
		float vmin = vmax - ssrThickness;

		/* Check if we are somewhere near the surface. */
		/* Note: we consider hitting the screen borders (raw_depth == 0.0)
		 * as valid to check for occluder in the refine pass */
		if (!((zmin > vmax) || (zmax < vmin)) || (raw_depth == 0.0)) {
			/* Below surface, cannot trace further */
			hit = true;
		}
	}

	if (hit) {
		/* Rewind back a step. */
		pqk -= dPQK;

		/* And do a finer trace over this segment. */
		dPQK /= ssrStride;

		prev_zmax = (dPQK.z * -0.5 + pqk.z) / (dPQK.w * -0.5 + pqk.w);

		for (float refinestep = 0.0; refinestep < ssrStride * 2.0 && refinestep < MAX_REFINE_STEP * 2.0; refinestep++) {
			/* step through current cell */
			pqk += dPQK;

			ivec2 hitpixel = ivec2(permute ? pqk.yx : pqk.xy);
			raw_depth = texelFetch(depth_texture, hitpixel, 0).r;

			float zmin = prev_zmax;
			zmax = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);
			prev_zmax = zmax;
			swapIfBigger(zmin, zmax);

			float vmax = get_view_z_from_depth(raw_depth);
			float vmin = vmax - ssrThickness;

			/* Check if we are somewhere near the surface. */
			if (!((zmin > vmax) || (zmax < vmin)) || (raw_depth == 0.0)) {
				/* Below surface, cannot trace further */
				break;
			}
		}
	}

	/* Background case. */
	hit = hit && (raw_depth != 1.0);

	/* Return length */
	return (hit) ? (zmax - ray_origin.z) / ray_dir.z : -1.0;
}

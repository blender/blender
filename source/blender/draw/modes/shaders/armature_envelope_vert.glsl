
uniform mat4 ViewMatrixInverse;
uniform mat4 ViewProjectionMatrix;

/* ---- Instanciated Attribs ---- */
in vec4 pos;  /* w encodes head (== 0.0f), tail (== 1.0f). */

/* ---- Per instance Attribs ---- */
/* Assumed to be in world coordinate already. */
in vec4 headSphere;
in vec4 tailSphere;
in vec4 color;
in vec3 xAxis;

flat out vec4 finalColor;
out vec3 normalView;

struct Bone { vec3 p1, vec; float r1, rdif, vec_rsq, h_bias, h_scale; };

float sdf_bone(vec3 p, Bone b)
{
	/* Simple capsule sdf with a minor touch and optimisations. */
	vec3 pa = p - b.p1;
	float h = dot(pa, b.vec) * b.vec_rsq;
	h = h * b.h_scale + b.h_bias; /* comment this line for sharp transition. */
	h = clamp(h, 0.0, 1.0);
	return length(pa - b.vec * h) - (b.r1 + b.rdif * h);
}

void main()
{
	/* Raytracing against cone need a parametric definition of the cone
	 * using axis, angle and apex. But if both sphere are nearly the same
	 * size, the apex become ill defined, and so does the angle.
	 * So to circumvent this, we use the numerical solution: raymarching.
	 * But due to the the cost of raymarching per pixel, we choose to use it
	 * to position vertices from a VBO with the distance field.
	 * The nice thing is that we start the raymarching really near the surface
	 * so we actually need very few iterations to get a good result. */

	Bone b;
	/* Precompute everything we can to speedup iterations. */
	b.p1 = headSphere.xyz;
	b.r1 = headSphere.w;
	b.rdif = tailSphere.w - headSphere.w;
	b.vec = tailSphere.xyz - headSphere.xyz;
	float vec_lsq = max(1e-8, dot(b.vec, b.vec));
	b.vec_rsq = 1.0 / vec_lsq;
	float sinb = (tailSphere.w - headSphere.w) * b.vec_rsq;
	float ofs1 = sinb * headSphere.w;
	float ofs2 = sinb * tailSphere.w;
	b.h_scale = 1.0 - ofs1 + ofs2;
	b.h_bias = ofs1 * b.h_scale;

	/* Radius for the initial position */
	float rad = b.r1 + b.rdif * pos.w;

	float vec_len = sqrt(vec_lsq);
	vec3 y_axis = b.vec / max(1e-8, vec_len);
	vec3 z_axis = normalize(cross(xAxis, -y_axis));
	vec3 x_axis = cross(y_axis, -z_axis); /* cannot trust xAxis to be orthogonal. */

	vec3 sp = pos.xyz * rad;
	if (pos.w == 1.0) {
		/* Prevent tail sphere to cover the head sphere if head is really big. */
		sp.y = max(sp.y, -(vec_len - headSphere.w));
		sp.y += vec_len; /* Position tail on the Bone axis. */
	}

	/* p is vertex position in world space */
	vec3 p = mat3(x_axis, y_axis, z_axis) * sp;
	p += headSphere.xyz;

	/* push vert towards the capsule boundary */
	vec3 dir = vec3(normalize(pos.xz + 1e-8), 0.0);
	dir = mat3(x_axis, y_axis, z_axis) * dir.xzy;

	/* Signed distance field gives us the distance to the surface.
	 * Use a few iteration for precision. */
	p = p - dir * sdf_bone(p, b);
	p = p - dir * sdf_bone(p, b);
	p = p - dir * sdf_bone(p, b);
	p = p - dir * sdf_bone(p, b);
	p = p - dir * sdf_bone(p, b);

	const float eps = 0.0005;
	normalView.x = sdf_bone(p + ViewMatrixInverse[0].xyz * eps, b);
	normalView.y = sdf_bone(p + ViewMatrixInverse[1].xyz * eps, b);
	normalView.z = sdf_bone(p + ViewMatrixInverse[2].xyz * eps, b);

	gl_Position = ViewProjectionMatrix * vec4(p, 1.0);

	finalColor = color;
}

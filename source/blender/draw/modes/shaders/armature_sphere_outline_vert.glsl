
uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;
uniform vec2 viewportSize;
uniform float lineThickness = 2.0;

/* ---- Instantiated Attribs ---- */
in vec2 pos0;
in vec2 pos1;

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec4 outlineColorSize;

flat out vec4 finalColor;

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

vec2 compute_dir(vec2 v0, vec2 v1, vec2 c)
{
	vec2 dir = normalize(v1 - v0);
	dir = vec2(dir.y, -dir.x);
	/* The model matrix can be scaled negativly.
	 * Use projected sphere center to determine
	 * the outline direction. */
	vec2 cv = c - v0;
	dir = (dot(dir, cv) > 0.0) ? -dir : dir;
	return dir;
}

void main()
{
	mat4 model_view_matrix = ViewMatrix * InstanceModelMatrix;
	mat4 sphereMatrix = inverse(model_view_matrix);

	bool is_persp = (ProjectionMatrix[3][3] == 0.0);

	/* This is the local space camera ray (not normalize).
	 * In perspective mode it's also the viewspace position
	 * of the sphere center. */
	vec3 cam_ray = (is_persp) ? model_view_matrix[3].xyz : vec3(0.0, 0.0, -1.0);
	cam_ray = mat3(sphereMatrix) * cam_ray;

	/* Sphere center distance from the camera (persp) in local space. */
	float cam_dist = length(cam_ray);

	/* Compute view aligned orthonormal space. */
	vec3 z_axis = cam_ray / cam_dist;
	vec3 x_axis = normalize(cross(sphereMatrix[1].xyz, z_axis));
	vec3 y_axis = cross(z_axis, x_axis);
	float z_ofs = 0.0;

	if (is_persp) {
		/* For perspective, the projected sphere radius
		 * can be bigger than the center disc. Compute the
		 * max angular size and compensate by sliding the disc
		 * towards the camera and scale it accordingly. */
		const float half_pi = 3.1415926 * 0.5;
		const float rad = 0.05;
		/* Let be (in local space):
		 * V the view vector origin.
		 * O the sphere origin.
		 * T the point on the target circle.
		 * We compute the angle between (OV) and (OT). */
		float a = half_pi - asin(rad / cam_dist);
		float cos_b = cos(a);
		float sin_b = sqrt(clamp(1.0 - cos_b * cos_b, 0.0, 1.0));

		x_axis *= sin_b;
		y_axis *= sin_b;
		z_ofs = -rad * cos_b;
	}

	/* Camera oriented position (but still in local space) */
	vec3 cam_pos0 = x_axis * pos0.x + y_axis * pos0.y + z_axis * z_ofs;
	vec3 cam_pos1 = x_axis * pos1.x + y_axis * pos1.y + z_axis * z_ofs;

	vec4 V  = model_view_matrix * vec4(cam_pos0, 1.0);
	vec4 p0 = ProjectionMatrix * V;
	vec4 p1 = ProjectionMatrix * (model_view_matrix * vec4(cam_pos1, 1.0));
	vec4 c  = ProjectionMatrix * vec4(model_view_matrix[3].xyz, 1.0);

	vec2 ssc = proj(c);
	vec2 ss0 = proj(p0);
	vec2 ss1 = proj(p1);
	vec2 edge_dir = compute_dir(ss0, ss1, ssc);

	bool outer = ((gl_VertexID & 1) == 1);

	vec2 t = outlineColorSize.w * (lineThickness / viewportSize);
	t *= (is_persp) ? abs(V.z) : 1.0;
	t  = (outer) ? t : vec2(0.0);

	gl_Position = p0;
	gl_Position.xy += t * edge_dir;

	finalColor = vec4(outlineColorSize.rgb, 1.0);
}

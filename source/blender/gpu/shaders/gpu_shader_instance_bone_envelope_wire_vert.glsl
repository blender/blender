

/* This shader takes a 2D shape, puts it in 3D Object space such that is stays aligned with view and bone,
 * and scales head/tail/distance according to per-instance attributes
 * (and 'role' of current vertex, encoded in zw input, head or tail, and inner or outer for distance outline).
 * It is used for both the distance outline drawing, and the wire version of envelope bone.
 * Note that if one of head/tail radius is negative, it assumes it only works on the other end of the bone
 * (used to draw head/tail spheres). */

uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;

/* ---- Instanciated Attribs ---- */
in vec4 pos;  /* z encodes head (== 0.0f), tail (== 1.0f) or in-between; w encodes inner (0.0f) or outer border. */

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;

in float radius_head;
in float radius_tail;
in float distance;


flat out vec4 finalColor;


void main()
{
	/* We get head/tail in object space. */
	mat4 bone_mat = InstanceModelMatrix;
	vec4 head = bone_mat * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	vec4 tail = bone_mat * vec4(0.0f, 1.0f, 0.0f, 1.0f);

	/* We generate our XY axes in object space, Y axis being aligned with bone in view space. */
	mat4 obview_mat = ViewMatrix;
	mat4 iobview_mat = inverse(obview_mat);

	vec4 view_bone_vec = obview_mat * normalize(tail - head);
	view_bone_vec.z = 0.0f;
	if (length(view_bone_vec.xy) <= 1e-5f) {
		/* A bit weak, but will do the job for now.
		 * Ideally we could compute head/tail radius in view space, and take larger one... */
		if (view_bone_vec.x > view_bone_vec.y) {
			view_bone_vec.x = 1e-5f;
		}
		else {
			view_bone_vec.y = 1e-5f;
		}
	}
	vec3 bone_axis_y = normalize((iobview_mat * view_bone_vec).xyz);
	vec3 bone_axis_x = normalize(cross(bone_axis_y, iobview_mat[2].xyz));

	/* Where does this comes from???? Don't know why, but is mandatory anyway... :/ */
	float size = 2.0f;

	head.xyz *= size;
	tail.xyz *= size;

	bool head_only = (radius_tail < 0.0f);
	bool tail_only = (radius_head < 0.0f);
	/* == 0: head; == 1: tail; in-between: along bone. */
	float head_fac = head_only ? 0.0f : (tail_only ? 1.0f : pos.z);
	bool do_distance_offset = (pos.w != 0.0f) && (distance >= 0.0f);

	vec2 xy_pos = pos.xy;
	vec4 ob_pos;

	vec4 ob_bone_origin;
	float radius;

	/* head */
	if (head_fac <= 0.0f) {
		radius = radius_head;
		ob_bone_origin = head;
	}
	/* tail */
	else if (head_fac >= 1.0f) {
		radius = radius_tail;
		ob_bone_origin = tail;
	}
	/* Body of the bone */
#if 0  /* Note: not used currently! */
	else {
		float tail_fac = 1.0f - head_fac;
		radius = radius_head * head_fac + radius_tail * tail_fac;
		ob_bone_origin = head * head_fac + tail * tail_fac;
	}
#endif

	if (do_distance_offset) {
		radius += distance;
	}

	xy_pos = xy_pos * radius * size;

	ob_pos = ob_bone_origin + vec4(bone_axis_x * xy_pos.x + bone_axis_y * xy_pos.y, 1.0f);
	gl_Position = ProjectionMatrix * obview_mat * ob_pos;
	finalColor = color;
}


layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec2 ssPos[];
in float facingOut[];

flat out vec3 ssVec0;
flat out vec3 ssVec1;
flat out vec3 ssVec2;
out float facing;

#define NO_EDGE vec3(10000.0);

vec3 compute_vec(vec2 v0, vec2 v1)
{
	vec2 v = normalize(v1 - v0);
	v = vec2(-v.y, v.x);
	return vec3(v, -dot(v, v0));
}

void main(void)
{
	vec3 facings = vec3(facingOut[0], facingOut[1], facingOut[2]);
	bvec3 do_edge = greaterThan(abs(facings), vec3(1.0));
	facings = fract(facings) - clamp(-sign(facings), 0.0, 1.0);

	ssVec0 = do_edge.x ? compute_vec(ssPos[0], ssPos[1]) : NO_EDGE;
	ssVec1 = do_edge.y ? compute_vec(ssPos[1], ssPos[2]) : NO_EDGE;
	ssVec2 = do_edge.z ? compute_vec(ssPos[2], ssPos[0]) : NO_EDGE;

	gl_Position = gl_in[0].gl_Position;
	facing = facings.x;
	EmitVertex();
	gl_Position = gl_in[1].gl_Position;
	facing = facings.y;
	EmitVertex();
	gl_Position = gl_in[2].gl_Position;
	facing = facings.z;
	EmitVertex();
	EndPrimitive();
}

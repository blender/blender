
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

uniform int Layer;

in vec4 vPos[];
flat in int face[];

out vec3 worldPosition;
out vec3 viewPosition; /* Required. otherwise generate linking error. */
out vec3 worldNormal; /* Required. otherwise generate linking error. */

const vec3 maj_axes[6] = vec3[6](vec3(1.0,  0.0,  0.0), vec3(-1.0,  0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, -1.0,  0.0), vec3( 0.0,  0.0, 1.0), vec3( 0.0,  0.0, -1.0));
const vec3 x_axis[6]   = vec3[6](vec3(0.0,  0.0, -1.0), vec3( 0.0,  0.0, 1.0), vec3(1.0, 0.0, 0.0), vec3(1.0,  0.0,  0.0), vec3( 1.0,  0.0, 0.0), vec3(-1.0,  0.0,  0.0));
const vec3 y_axis[6]   = vec3[6](vec3(0.0, -1.0,  0.0), vec3( 0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0,  0.0, -1.0), vec3( 0.0, -1.0, 0.0), vec3( 0.0, -1.0,  0.0));

void main() {
	int f = face[0];
	gl_Layer = Layer + f;

	for (int v = 0; v < 3; ++v) {
		gl_Position = vPos[v];
		worldPosition = x_axis[f] * vPos[v].x + y_axis[f] * vPos[v].y + maj_axes[f];
		EmitVertex();
	}

	EndPrimitive();
}
#extension GL_ARB_gpu_shader5 : enable

#ifdef GL_ARB_gpu_shader5
#define USE_INVOC_EXT
#endif

#define DOUBLE_MANIFOLD

#ifdef DOUBLE_MANIFOLD
#  ifdef USE_INVOC_EXT
#    define invoc_ct 4
#  else
#    define vert_ct 12
#  endif
#else
#  ifdef USE_INVOC_EXT
#    define invoc_ct 2
#  else
#    define vert_ct 6
#  endif
#endif

#ifdef USE_INVOC_EXT
layout(triangles, invocations = invoc_ct) in;
layout(triangle_strip, max_vertices = 3) out;
#else
layout(triangles) in;
layout(triangle_strip, max_vertices = vert_ct) out;
#endif

uniform vec3 lightDirection = vec3(0.57, 0.57, -0.57);

in VertexData {
	vec3 pos;           /* local position */
	vec4 frontPosition; /* final ndc position */
	vec4 backPosition;
} vData[];

vec4 get_pos(int v, bool backface)
{
	return (backface) ? vData[v].backPosition : vData[v].frontPosition;
}

void emit_cap(const bool front)
{
	if (front) {
		gl_Position = vData[0].frontPosition; EmitVertex();
		gl_Position = vData[1].frontPosition; EmitVertex();
		gl_Position = vData[2].frontPosition; EmitVertex();
	}
	else {
		gl_Position = vData[0].backPosition; EmitVertex();
		gl_Position = vData[2].backPosition; EmitVertex();
		gl_Position = vData[1].backPosition; EmitVertex();
	}
	EndPrimitive();
}

void main()
{
	vec3 v10 = vData[0].pos - vData[1].pos;
	vec3 v12 = vData[2].pos - vData[1].pos;

	vec3 n = cross(v12, v10);
	float facing = dot(n, lightDirection);

	bool backface = facing > 0.0;

	if (!backface) {
#ifdef USE_INVOC_EXT
		bool do_front = (gl_InvocationID & 1) == 0;
		emit_cap(do_front);
#else
		emit_cap(true);
		emit_cap(false);
#  ifdef DOUBLE_MANIFOLD
		emit_cap(true);
		emit_cap(false);
#  endif
#endif
	}
}

layout(triangles) in;
layout(triangle_strip, max_vertices=9) out;

uniform mat4 ModelMatrix;
uniform mat4 ModelViewProjectionMatrix;

uniform vec3 lightDirection = vec3(0.57, 0.57, -0.57);

in VertexData {
	flat vec4 lightDirectionMS;
	vec4 frontPosition;
	vec4 backPosition;
} vertexData[];

vec3 face_normal(vec3 v1, vec3 v2, vec3 v3) {
	return normalize(cross(v2 - v1, v3 - v1));
}
void main()
{
	vec4 light_direction = vertexData[0].lightDirectionMS;
	vec4 v1 = gl_in[0].gl_Position;
	vec4 v2 = gl_in[1].gl_Position;
	vec4 v3 = gl_in[2].gl_Position;
	bool backface = dot(face_normal(v1.xyz, v2.xyz, v3.xyz), light_direction.xyz) > 0.0;

	int index0 = backface?0:2;
	int index2 = backface?2:0;

	/* back cap */
	gl_Position = vertexData[index0].backPosition;
	EmitVertex();
	gl_Position = vertexData[1].backPosition;
	EmitVertex();
	gl_Position = vertexData[index2].backPosition;
	EmitVertex();

	/* sides */
	gl_Position = vertexData[index2].frontPosition;
	EmitVertex();
	gl_Position = vertexData[index0].backPosition;
	EmitVertex();
	gl_Position = vertexData[index0].frontPosition;
	EmitVertex();
	gl_Position = vertexData[1].backPosition;
	EmitVertex();
	gl_Position = vertexData[1].frontPosition;
	EmitVertex();
	gl_Position = vertexData[index2].frontPosition;
	EmitVertex();
	EndPrimitive();
}

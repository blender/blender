
uniform mat3 NormalMatrix;

uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;
uniform vec2 viewportSize;

/* ---- Instanciated Attribs ---- */
in vec3 pos;
in vec3 nor;
in vec3 snor;

/* ---- Per instance Attribs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;

out vec4 pPos;
out float vZ;
out float vFacing;
out vec2 ssPos;
out vec2 ssNor;
out vec4 vCol;

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void main()
{
	/* This is slow and run per vertex, but it's still faster than
	 * doing it per instance on CPU and sending it on via instance attrib */
	mat3 NormalMatrix = transpose(inverse(mat3(ViewMatrix * InstanceModelMatrix)));

	vec4 viewpos = ViewMatrix * (InstanceModelMatrix * vec4(pos, 1.0));
	pPos = ProjectionMatrix * viewpos;
	vZ = abs(viewpos.z);

	/* if perspective */
	vec3 V = (ProjectionMatrix[3][3] == 0.0) ? normalize(-viewpos.xyz) : vec3(0.0, 0.0, 1.0);

	/* TODO FIX: there is still a problem with this vector
	 * when the bone is scaled or in persp mode. But it's
	 * barelly visible at the outline corners. */
	ssNor = normalize((NormalMatrix * snor).xy);

	vFacing = dot(V, normalize(NormalMatrix * nor));

	ssPos = proj(pPos);

	vCol = color;
}

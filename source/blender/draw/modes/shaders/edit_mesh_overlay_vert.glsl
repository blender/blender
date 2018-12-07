
/* Solid Wirefram implementation
 * Mike Erwin, Clément Foucault */

uniform mat3 NormalMatrix;
uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat4 ModelViewProjectionMatrix;
uniform ivec4 dataMask = ivec4(0xFF);

uniform float ofs = 1e-5;

in vec3 pos;
#ifdef VERTEX_FACING
in vec3 vnor;
#endif

#ifdef EDGE_FIX
in ivec4 data;

out vec4 pPos;
out ivec4 vData;
#  ifdef VERTEX_FACING
out float vFacing;
#  endif

void main()
{
	pPos = ModelViewProjectionMatrix * vec4(pos, 1.0);
	pPos.z -= ofs * ((ProjectionMatrix[3][3] == 0.0) ? 1.0 : 0.0);
	vData = data & dataMask;
#  ifdef VERTEX_FACING
	vec4 vpos = ModelViewMatrix * vec4(pos, 1.0);
	vec3 view_normal = normalize(NormalMatrix * vnor);
	vec3 view_vec = (ProjectionMatrix[3][3] == 0.0)
		? normalize(vpos.xyz)
		: vec3(0.0, 0.0, 1.0);
	vFacing = dot(view_vec, view_normal);
#  endif
}

#else /* EDGE_FIX */

/* Consecutive data of the nth vertex.
 * Only valid for first vertex in the triangle.
 * Assuming GL_FRIST_VERTEX_CONVENTION. */
in ivec4 data0;
in ivec4 data1;
in ivec4 data2;

flat out vec3 edgesCrease;
flat out vec3 edgesBweight;
flat out vec4 faceColor;
flat out ivec3 flag;
#  ifdef VERTEX_SELECTION
out vec3 vertexColor;
#  endif
#  ifdef VERTEX_FACING
out float facing;
#  endif

out vec3 barycentric;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_Position.z -= ofs * ((ProjectionMatrix[3][3] == 0.0) ? 1.0 : 0.0);

	int v_0 = (gl_VertexID / 3) * 3;
	int vidx = gl_VertexID % 3;
	barycentric = vec3(equal(ivec3(0, 1, 2), ivec3(vidx)));

	/* Edge */
	ivec4 vData[3] = ivec4[3](data0, data1, data2);
	ivec3 eflag;
	for (int v = 0; v < 3; ++v) {
		vData[v] = vData[v] & dataMask;
		flag[v] = eflag[v] = vData[v].y | (vData[v].x << 8);
		edgesCrease[v] = vData[v].z / 255.0;
		edgesBweight[v] = vData[v].w / 255.0;
	}

	/* Face */
	if ((vData[0].x & FACE_ACTIVE) != 0)
		faceColor = colorFaceSelect;
	else if ((vData[0].x & FACE_SELECTED) != 0)
		faceColor = colorFaceSelect;
	else if ((vData[0].x & FACE_FREESTYLE) != 0)
		faceColor = colorFaceFreestyle;
	else
		faceColor = colorFace;

#  ifdef VERTEX_SELECTION
	vertexColor = EDIT_MESH_vertex_color(data0.x).rgb;
#  endif
#  ifdef VERTEX_FACING
	vec4 vPos = ModelViewMatrix * vec4(pos, 1.0);
	vec3 view_normal = normalize(NormalMatrix * vnor);
	vec3 view_vec = (ProjectionMatrix[3][3] == 0.0)
		? normalize(vPos.xyz)
		: vec3(0.0, 0.0, 1.0);
	facing = dot(view_vec, view_normal);
#  endif
}

#endif

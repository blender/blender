
uniform mat4 ViewMatrixInverse;
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attrs ---- */
in vec3 pos;
in vec3 nor;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;

out vec3 normal;
flat out vec4 finalColor;

void main()
{
  gl_Position = ViewProjectionMatrix * (InstanceModelMatrix * vec4(pos, 1.0));

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  mat3 normal_mat = transpose(inverse(mat3(InstanceModelMatrix)));
  normal = normalize((transpose(mat3(ViewMatrixInverse)) * (normal_mat * nor)));

  finalColor = color;
}

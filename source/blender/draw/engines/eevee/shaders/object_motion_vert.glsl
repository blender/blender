
uniform mat4 currModelMatrix;
uniform mat4 prevModelMatrix;
uniform mat4 nextModelMatrix;
uniform bool useDeform;

in vec3 pos;
in vec3 prv; /* Previous frame position. */
in vec3 nxt; /* Next frame position. */

out vec3 currWorldPos;
out vec3 prevWorldPos;
out vec3 nextWorldPos;

void main()
{
  prevWorldPos = (prevModelMatrix * vec4(useDeform ? prv : pos, 1.0)).xyz;
  currWorldPos = (currModelMatrix * vec4(pos, 1.0)).xyz;
  nextWorldPos = (nextModelMatrix * vec4(useDeform ? nxt : pos, 1.0)).xyz;
  /* Use jittered projmatrix to be able to match exact sample depth (depth equal test).
   * Note that currModelMatrix needs to also be equal to ModelMatrix for the samples to match. */
  gl_Position = ViewProjectionMatrix * vec4(currWorldPos, 1.0);
}

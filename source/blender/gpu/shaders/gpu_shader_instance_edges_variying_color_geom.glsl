
// Draw "fancy" wireframe, displaying front-facing, back-facing and
// silhouette lines differently.
// Mike Erwin, April 2015

// After working with this shader a while, convinced we should make
// separate shaders for perpective & ortho. (Oct 2016)

// Due to perspective, the line segment's endpoints might disagree on
// whether the adjacent faces are front facing. This geometry shader
// decides which edge type to use if endpoints disagree.

uniform mat4 ProjectionMatrix;

uniform bool drawFront = true;
uniform bool drawBack = true;
uniform bool drawSilhouette = true;

layout(lines) in;
layout(line_strip, max_vertices = 2) out;

in vec4 MV_pos[];
in float edgeClass[];
in vec3 fCol[];

flat out vec4 finalColor;

void emitLine(vec4 color)
{
  gl_Position = ProjectionMatrix * MV_pos[0];
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_set_clip_distance(gl_in[0].gl_ClipDistance);
#endif
  EmitVertex();

  gl_Position = ProjectionMatrix * MV_pos[1];
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_set_clip_distance(gl_in[1].gl_ClipDistance);
#endif
  finalColor = color;
  EmitVertex();

  EndPrimitive();
}

void main()
{
  float finalEdgeClass = max(edgeClass[0], edgeClass[1]);

  if (finalEdgeClass > 0.0f) {
    // front-facing edge
    if (drawFront) {
      emitLine(vec4(fCol[0], 0.75));
    }
  }
  else if (finalEdgeClass < 0.0f) {
    // back-facing edge
    if (drawBack) {
      emitLine(vec4(fCol[0], 0.5));
    }
  }
  else {
    // exactly one face is front-facing, silhouette edge
    if (drawSilhouette) {
      emitLine(vec4(fCol[0], 1.0));
    }
  }
}

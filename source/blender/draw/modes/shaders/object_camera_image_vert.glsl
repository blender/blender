uniform mat4 TransformMat;
uniform float flipX;
uniform float flipY;
uniform float depth;

in vec2 texCoord;
in vec2 pos;

out vec2 texCoord_interp;

void main()
{
  vec4 position = TransformMat * vec4((pos - 0.5) * 2.0, 1.0, 1.0);
  gl_Position = vec4(position.xy, depth, 1.0);

  vec2 uv_mul = vec2(flipX, flipY);
  texCoord_interp = texCoord * uv_mul;
}

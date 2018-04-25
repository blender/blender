uniform int object_id = 0;
uniform vec3 object_color = vec3(1.0, 0.0, 1.0);

in vec3 normal_viewport;

out uint objectId;
out vec3 diffuseColor;
out vec2 normalViewport;

void main()
{
	objectId = uint(object_id);
	diffuseColor = object_color;
	normalViewport = normal_encode(normal_viewport);
}

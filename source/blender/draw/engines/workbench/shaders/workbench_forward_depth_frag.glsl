uniform int object_id = 0;
layout(location=0) out uint objectId;

void main()
{
	objectId = uint(object_id);
}



#include "DummyClass.h"
#include "autoseam_C_API.h"

AUTOSEAM_DummyClassHandle autoseam_create_dummyclass()
{
	DummyClass *dummy = new DummyClass();
	return (AUTOSEAM_DummyClassHandle) dummy;
}

void autoseam_delete_dummyclass(AUTOSEAM_DummyClassHandle handle)
{
	DummyClass *dummy = reinterpret_cast<DummyClass*>(handle);
	delete dummy;
}

void autoseam_solve3x3(AUTOSEAM_DummyClassHandle handle, float* vec)
{
	DummyClass *dummy = reinterpret_cast<DummyClass*>(handle);
	dummy->solve3x3(vec);
}

void autoseam_get_solution(AUTOSEAM_DummyClassHandle handle, float* vec)
{
	DummyClass *dummy = reinterpret_cast<DummyClass*>(handle);
	dummy->get_solution(vec);
}
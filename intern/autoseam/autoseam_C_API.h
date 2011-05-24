#ifndef AUTOSEAM_C_API_INCLUDED
#define AUTOSEAM_C_API_INCLUDED

// simple c function

#ifdef __cplusplus // so we can include the header form C++ as well
extern "C" {
#endif 

// This is to simplify passing pointers to classes back into the API
#define AUTOSEAM_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name
AUTOSEAM_DECLARE_HANDLE(AUTOSEAM_DummyClassHandle);

AUTOSEAM_DummyClassHandle autoseam_create_dummyclass();
void autoseam_delete_dummyclass(AUTOSEAM_DummyClassHandle handle);

void autoseam_solve3x3(AUTOSEAM_DummyClassHandle handle, float* vec);
void autoseam_get_solution(AUTOSEAM_DummyClassHandle handle, float* vec);

#ifdef __cplusplus
}
#endif 

#endif

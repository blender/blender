#ifndef SOLID_TYPES_H
#define SOLID_TYPES_H

#define DT_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name
    
typedef double        DT_Scalar;        
typedef unsigned int  DT_Count;
typedef unsigned int  DT_Index;
typedef unsigned int  DT_Size;

typedef DT_Scalar DT_Vector3[3]; 
typedef DT_Scalar DT_Quaternion[4]; 

#endif


#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include "solid_types.h"

#ifdef __cplusplus
extern "C" { 
#endif

DT_DECLARE_HANDLE(IP_IpoHandle);

typedef struct IP_ControlPoint {
	DT_Scalar m_key;
	DT_Scalar m_keyValue;
} IP_ControlPoint;

IP_IpoHandle IP_CreateLinear(const IP_ControlPoint *cpoints, int num_cpoints);

void         IP_DeleteInterpolator(IP_IpoHandle ipo);

DT_Scalar IP_GetValue(IP_IpoHandle ipo, DT_Scalar key);

#ifdef __cplusplus
}
#endif

#endif

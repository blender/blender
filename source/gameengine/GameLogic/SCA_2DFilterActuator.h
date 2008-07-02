#ifndef __SCA_2DFILETRACTUATOR_H__
#define __SCA_2DFILETRACTUATOR_H__

#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"
#include "SCA_IActuator.h"


class SCA_2DFilterActuator : public SCA_IActuator
{
    Py_Header;

private:
	
	RAS_2DFilterManager::RAS_2DFILTER_MODE m_type;
	short m_flag;
	float m_float_arg;
	int   m_int_arg;
	short m_texture_flag;
	STR_String	m_shaderText;
	RAS_IRasterizer* m_rasterizer;
	RAS_IRenderTools* m_rendertools;

public:

    SCA_2DFilterActuator(
        class SCA_IObject* gameobj,
        RAS_2DFilterManager::RAS_2DFILTER_MODE type,
		short flag,
		float float_arg,
		int int_arg,
		short texture_flag,
		RAS_IRasterizer* rasterizer,
		RAS_IRenderTools* rendertools,
        PyTypeObject* T=&Type
        );

	void	SetShaderText(STR_String text);
    virtual ~SCA_2DFilterActuator();
    virtual bool Update();

    virtual CValue* GetReplica();
    virtual PyObject* _getattr(const STR_String& attr);

};
#endif

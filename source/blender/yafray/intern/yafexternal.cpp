#include "yafexternal.h"
#include <iostream>
namespace yafray {

parameter_t::parameter_t(const std::string &s):type(TYPE_STRING),used(false),str(s) 
{
}

parameter_t::parameter_t(float f):type(TYPE_FLOAT),used(false),fnum(f) 
{
}

parameter_t::parameter_t(const colorA_t &c):type(TYPE_COLOR),used(false)
			,C(c) 
{
}

parameter_t::parameter_t(const point3d_t &p):type(TYPE_POINT),used(false),P(p) 
{
}

parameter_t::parameter_t():type(TYPE_NONE),used(false) 
{
}

parameter_t::~parameter_t()
{
}

parameter_t & paramMap_t::operator [] (const std::string &key)
{
	return dicc[key];
}

void paramMap_t::clear()
{
	dicc.clear();
}

}

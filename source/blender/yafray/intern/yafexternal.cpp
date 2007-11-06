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

paramMap_t::paramMap_t()
{
}

paramMap_t::~paramMap_t() 
{
}

bool paramMap_t::getParam(const std::string &name,const std::string *&s)
{
	if(includes(name,TYPE_STRING))
	{ 
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		s=&(i->second.getStr());
	}
		else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,bool &b)
{
	std::string str;
	if(includes(name,TYPE_STRING)) 
	{
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		str=i->second.getStr();
		if(str=="on") b=true;
		else if(str=="off") b=false;
		else return false;
	}
	else return false;
	return true;
}

bool paramMap_t::getParam(const std::string &name,float &f)
{
	if(includes(name,TYPE_FLOAT)) 
	{
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		f=i->second.getFnum();
	}
	else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,double &f)
{
	if(includes(name,TYPE_FLOAT)) 
	{
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		f=i->second.getFnum();
	}
	else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,int &in)
{
	if(includes(name,TYPE_FLOAT)) 
	{
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		in=(int)(i->second.getFnum());
	}
	else return false;
	return true;
}

bool paramMap_t::getParam(const std::string &name,point3d_t &p)
{
	if(includes(name,TYPE_POINT)) 
	{
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		p=i->second.getP();
	}
	else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,color_t &c)
{
	if(includes(name,TYPE_COLOR)) 
	{
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		c=i->second.getC();
	}
	else return false;
	return true;
}

bool paramMap_t::getParam(const std::string &name,colorA_t &c)
{
	if(includes(name,TYPE_COLOR)) 
	{
		std::map<std::string,parameter_t>::iterator i=dicc.find(name);
		c=i->second.getAC();
	}
	else return false;
	return true;
}

bool paramMap_t::includes(const std::string &label,int type)const
{
	std::map<std::string,parameter_t>::const_iterator i=dicc.find(label);
	if(i==dicc.end()) return false;
	if((*i).second.type!=type) return false;
	return true;
}

void paramMap_t::checkUnused(const std::string &env)const
{
	for(std::map<std::string,parameter_t>::const_iterator i=dicc.begin();i!=dicc.end();++i)
		if(!( (*i).second.used ))
			std::cout<<"[WARNING]:Unused param "<<(*i).first<<" in "<<env<<"\n";
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

#include "yafexternal.h"
#include <iostream>
namespace yafray {

bool paramMap_t::getParam(const std::string &name,std::string &s)
{
	if(includes(name,TYPE_STRING)) s=(*this)[name].getStr();
		else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,bool &b)
{
	std::string str;
	if(includes(name,TYPE_STRING)) 
	{
		str=(*this)[name].getStr();
		if(str=="on") b=true;
		else if(str=="off") b=false;
		else return false;
	}
	else return false;
	return true;
}

bool paramMap_t::getParam(const std::string &name,float &f)
{
	if(includes(name,TYPE_FLOAT)) f=(*this)[name].getFnum();
		else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,double &f)
{
	if(includes(name,TYPE_FLOAT)) f=(*this)[name].getFnum();
		else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,int &i)
{
	if(includes(name,TYPE_FLOAT)) i=(int)(*this)[name].getFnum();
		else return false;
	return true;
}

bool paramMap_t::getParam(const std::string &name,point3d_t &p)
{
	if(includes(name,TYPE_POINT)) p=(*this)[name].getP();
		else return false;
	return true;
}
			
bool paramMap_t::getParam(const std::string &name,color_t &c)
{
	if(includes(name,TYPE_COLOR)) c=(*this)[name].getC();
		else return false;
	return true;
}

bool paramMap_t::getParam(const std::string &name,colorA_t &c)
{
	if(includes(name,TYPE_COLOR)) c=(*this)[name].getAC();
		else return false;
	return true;
}

bool paramMap_t::includes(const std::string &label,int type)
{
	const_iterator i=find(label);
	if(i==end()) return false;
	if((*i).second.type!=type) return false;
	return true;
}

void paramMap_t::checkUnused(const std::string &env)
{
	for(const_iterator i=begin();i!=end();++i)
		if(!( (*i).second.used ))
			std::cout<<"[WARNING]:Unused param "<<(*i).first<<" in "<<env<<"\n";
}

}

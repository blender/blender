// ------------------------------------
#include "BL_Material.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

MTex* getImageFromMaterial(Material *mat, int index)
{
	if(!mat) return 0;
	
	if(!(index >=0 && index < MAX_MTEX) ) return 0;
	
	MTex *m = mat->mtex[index];
	return m?m:0;
}

int getNumTexChannels( Material *mat )
{
	int count = -1;
	if(!mat) return -1;

	for(count =0; (count < 10) && mat->mtex[count] != 0; count++) {}
	return count;
}

BL_Material::BL_Material()
{
	Initialize();
}

void BL_Material::Initialize()
{
	rgb[0] = 0;
	rgb[1] = 0;
	rgb[2] = 0;
	rgb[3] = 0;
	IdMode = 0;
	ras_mode = 0;
	glslmat = 0;
	tile = 0;
	matname = "NoMaterial";
	matcolor[0] = 0.5f;
	matcolor[1] = 0.5f;
	matcolor[2] = 0.5f;
	matcolor[3] = 0.5f;
	speccolor[0] = 1.f;
	speccolor[1] = 1.f;
	speccolor[2] = 1.f;
	transp = 0;
	hard = 50.f;
	spec_f = 0.5f;
	alpha = 1.f;
	emit = 0.f;
	mode = 0;
	material = 0;
	tface = 0;
	materialindex = 0;
	amb=0.5f;
	num_enabled = 0;
	num_users = 1;
	share = false;

	int i;
	for(i=0; i<4; i++)
	{
		uv[i] = MT_Point2(0.f,1.f);
		uv2[i] = MT_Point2(0.f, 1.f);
	}

	for(i=0; i<MAXTEX; i++) // :(
	{
		mapping[i].mapping = 0;
		mapping[i].offsets[0] = 0.f;
		mapping[i].offsets[1] = 0.f;
		mapping[i].offsets[2] = 0.f;
		mapping[i].scale[0]   = 1.f;
		mapping[i].scale[1]   = 1.f;
		mapping[i].scale[2]   = 1.f;
		mapping[i].projplane[0] = PROJX;
		mapping[i].projplane[1] = PROJY;
		mapping[i].projplane[2] = PROJZ;
		mapping[i].objconame = "";
		mtexname[i] = "NULL";
		imageId[i]="NULL";
		flag[i] = 0;
		texname[i] = "NULL";
		tilexrep[i] = 1;
		tileyrep[i] = 1;
		color_blend[i] = 1.f;
		blend_mode[i]	= 0;
		img[i] = 0;
		cubemap[i] = 0;
	}
}

void BL_Material::SetConversionRGB(unsigned int *nrgb) {
	rgb[0]=*nrgb++;
	rgb[1]=*nrgb++;
	rgb[2]=*nrgb++;
	rgb[3]=*nrgb;
}

void BL_Material::GetConversionRGB(unsigned int *nrgb) {
	*nrgb++ = rgb[0];
	*nrgb++ = rgb[1];
	*nrgb++ = rgb[2];
	*nrgb   = rgb[3];
}

void BL_Material::SetConversionUV(const STR_String& name, MT_Point2 *nuv) {
	uvName = name;
	uv[0] = *nuv++;
	uv[1] = *nuv++;
	uv[2] = *nuv++;
	uv[3] = *nuv;
}

void BL_Material::GetConversionUV(MT_Point2 *nuv){
	*nuv++ = uv[0];
	*nuv++ = uv[1];
	*nuv++ = uv[2];
	*nuv   = uv[3];
}
void BL_Material::SetConversionUV2(const STR_String& name, MT_Point2 *nuv) {
	uv2Name = name;
	uv2[0] = *nuv++;
	uv2[1] = *nuv++;
	uv2[2] = *nuv++;
	uv2[3] = *nuv;
}

void BL_Material::GetConversionUV2(MT_Point2 *nuv){
	*nuv++ = uv2[0];
	*nuv++ = uv2[1];
	*nuv++ = uv2[2];
	*nuv   = uv2[3];
}


void BL_Material::SetSharedMaterial(bool v)
{
	if((v && num_users == -1) || num_users > 1 )
		share = true;
	else 
		share = false;
}

bool BL_Material::IsShared()
{
	return share;
}

void BL_Material::SetUsers(int num)
{
	num_users = num;
}


/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_MemoryBuffer.h"
#include "MEM_guardedalloc.h"
//#include "BKE_global.h"

unsigned int MemoryBuffer::determineBufferSize()
{
	return getWidth() * getHeight();
}

int MemoryBuffer::getWidth() const
{
	return this->m_rect.xmax - this->m_rect.xmin;
}
int MemoryBuffer::getHeight() const
{
	return this->m_rect.ymax - this->m_rect.ymin;
}

MemoryBuffer::MemoryBuffer(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect)
{
	BLI_rcti_init(&this->m_rect, rect->xmin, rect->xmax, rect->ymin, rect->ymax);
	this->m_memoryProxy = memoryProxy;
	this->m_chunkNumber = chunkNumber;
	this->m_buffer = (float *)MEM_mallocN(sizeof(float) * determineBufferSize() * COM_NUMBER_OF_CHANNELS, "COM_MemoryBuffer");
	this->m_state = COM_MB_ALLOCATED;
	this->m_datatype = COM_DT_COLOR;
	this->m_chunkWidth = this->m_rect.xmax - this->m_rect.xmin;
}

MemoryBuffer::MemoryBuffer(MemoryProxy *memoryProxy, rcti *rect)
{
	BLI_rcti_init(&this->m_rect, rect->xmin, rect->xmax, rect->ymin, rect->ymax);
	this->m_memoryProxy = memoryProxy;
	this->m_chunkNumber = -1;
	this->m_buffer = (float *)MEM_mallocN(sizeof(float) * determineBufferSize() * COM_NUMBER_OF_CHANNELS, "COM_MemoryBuffer");
	this->m_state = COM_MB_TEMPORARILY;
	this->m_datatype = COM_DT_COLOR;
	this->m_chunkWidth = this->m_rect.xmax - this->m_rect.xmin;
}
MemoryBuffer *MemoryBuffer::duplicate()
{
	MemoryBuffer *result = new MemoryBuffer(this->m_memoryProxy, &this->m_rect);
	memcpy(result->m_buffer, this->m_buffer, this->determineBufferSize() * COM_NUMBER_OF_CHANNELS * sizeof(float));
	return result;
}
void MemoryBuffer::clear()
{
	memset(this->m_buffer, 0, this->determineBufferSize() * COM_NUMBER_OF_CHANNELS * sizeof(float));
}

float *MemoryBuffer::convertToValueBuffer()
{
	const unsigned int size = this->determineBufferSize();
	unsigned int i;

	float *result = (float *)MEM_mallocN(sizeof(float) * size, __func__);

	const float *fp_src = this->m_buffer;
	float       *fp_dst = result;

	for (i = 0; i < size; i++, fp_dst++, fp_src += COM_NUMBER_OF_CHANNELS) {
		*fp_dst = *fp_src;
	}

	return result;
}

float MemoryBuffer::getMaximumValue()
{
	float result = this->m_buffer[0];
	const unsigned int size = this->determineBufferSize();
	unsigned int i;

	const float *fp_src = this->m_buffer;

	for (i = 0; i < size; i++, fp_src += COM_NUMBER_OF_CHANNELS) {
		float value = *fp_src;
		if (value > result) {
			result = value;
		}
	}

	return result;
}

float MemoryBuffer::getMaximumValue(rcti *rect)
{
	rcti rect_clamp;

	/* first clamp the rect by the bounds or we get un-initialized values */
	BLI_rcti_isect(rect, &this->m_rect, &rect_clamp);

	if (!BLI_rcti_is_empty(&rect_clamp)) {
		MemoryBuffer *temp = new MemoryBuffer(NULL, &rect_clamp);
		temp->copyContentFrom(this);
		float result = temp->getMaximumValue();
		delete temp;
		return result;
	}
	else {
		BLI_assert(0);
		return 0.0f;
	}
}

MemoryBuffer::~MemoryBuffer()
{
	if (this->m_buffer) {
		MEM_freeN(this->m_buffer);
		this->m_buffer = NULL;
	}
}

void MemoryBuffer::copyContentFrom(MemoryBuffer *otherBuffer)
{
	if (!otherBuffer) {
		BLI_assert(0);
		return;
	}
	unsigned int otherY;
	unsigned int minX = max(this->m_rect.xmin, otherBuffer->m_rect.xmin);
	unsigned int maxX = min(this->m_rect.xmax, otherBuffer->m_rect.xmax);
	unsigned int minY = max(this->m_rect.ymin, otherBuffer->m_rect.ymin);
	unsigned int maxY = min(this->m_rect.ymax, otherBuffer->m_rect.ymax);
	int offset;
	int otherOffset;


	for (otherY = minY; otherY < maxY; otherY++) {
		otherOffset = ((otherY - otherBuffer->m_rect.ymin) * otherBuffer->m_chunkWidth + minX - otherBuffer->m_rect.xmin) * COM_NUMBER_OF_CHANNELS;
		offset = ((otherY - this->m_rect.ymin) * this->m_chunkWidth + minX - this->m_rect.xmin) * COM_NUMBER_OF_CHANNELS;
		memcpy(&this->m_buffer[offset], &otherBuffer->m_buffer[otherOffset], (maxX - minX) * COM_NUMBER_OF_CHANNELS * sizeof(float));
	}
}

void MemoryBuffer::writePixel(int x, int y, const float color[4])
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
	    y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * COM_NUMBER_OF_CHANNELS;
		copy_v4_v4(&this->m_buffer[offset], color);
	}
}

void MemoryBuffer::addPixel(int x, int y, const float color[4])
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
	    y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * COM_NUMBER_OF_CHANNELS;
		add_v4_v4(&this->m_buffer[offset], color);
	}
}


// table of (exp(ar) - exp(a)) / (1 - exp(a)) for r in range [0, 1] and a = -2
// used instead of actual gaussian, otherwise at high texture magnifications circular artifacts are visible
#define EWA_MAXIDX 255
static const float EWA_WTS[EWA_MAXIDX + 1] = {
	1.f, 0.990965f, 0.982f, 0.973105f, 0.96428f, 0.955524f, 0.946836f, 0.938216f, 0.929664f,
	0.921178f, 0.912759f, 0.904405f, 0.896117f, 0.887893f, 0.879734f, 0.871638f, 0.863605f,
	0.855636f, 0.847728f, 0.839883f, 0.832098f, 0.824375f, 0.816712f, 0.809108f, 0.801564f,
	0.794079f, 0.786653f, 0.779284f, 0.771974f, 0.76472f, 0.757523f, 0.750382f, 0.743297f,
	0.736267f, 0.729292f, 0.722372f, 0.715505f, 0.708693f, 0.701933f, 0.695227f, 0.688572f,
	0.68197f, 0.67542f, 0.66892f, 0.662471f, 0.656073f, 0.649725f, 0.643426f, 0.637176f,
	0.630976f, 0.624824f, 0.618719f, 0.612663f, 0.606654f, 0.600691f, 0.594776f, 0.588906f,
	0.583083f, 0.577305f, 0.571572f, 0.565883f, 0.56024f, 0.55464f, 0.549084f, 0.543572f,
	0.538102f, 0.532676f, 0.527291f, 0.521949f, 0.516649f, 0.511389f, 0.506171f, 0.500994f,
	0.495857f, 0.490761f, 0.485704f, 0.480687f, 0.475709f, 0.470769f, 0.465869f, 0.461006f,
	0.456182f, 0.451395f, 0.446646f, 0.441934f, 0.437258f, 0.432619f, 0.428017f, 0.42345f,
	0.418919f, 0.414424f, 0.409963f, 0.405538f, 0.401147f, 0.39679f, 0.392467f, 0.388178f,
	0.383923f, 0.379701f, 0.375511f, 0.371355f, 0.367231f, 0.363139f, 0.359079f, 0.355051f,
	0.351055f, 0.347089f, 0.343155f, 0.339251f, 0.335378f, 0.331535f, 0.327722f, 0.323939f,
	0.320186f, 0.316461f, 0.312766f, 0.3091f, 0.305462f, 0.301853f, 0.298272f, 0.294719f,
	0.291194f, 0.287696f, 0.284226f, 0.280782f, 0.277366f, 0.273976f, 0.270613f, 0.267276f,
	0.263965f, 0.26068f, 0.257421f, 0.254187f, 0.250979f, 0.247795f, 0.244636f, 0.241502f,
	0.238393f, 0.235308f, 0.232246f, 0.229209f, 0.226196f, 0.223206f, 0.220239f, 0.217296f,
	0.214375f, 0.211478f, 0.208603f, 0.20575f, 0.20292f, 0.200112f, 0.197326f, 0.194562f,
	0.191819f, 0.189097f, 0.186397f, 0.183718f, 0.18106f, 0.178423f, 0.175806f, 0.17321f,
	0.170634f, 0.168078f, 0.165542f, 0.163026f, 0.16053f, 0.158053f, 0.155595f, 0.153157f,
	0.150738f, 0.148337f, 0.145955f, 0.143592f, 0.141248f, 0.138921f, 0.136613f, 0.134323f,
	0.132051f, 0.129797f, 0.12756f, 0.125341f, 0.123139f, 0.120954f, 0.118786f, 0.116635f,
	0.114501f, 0.112384f, 0.110283f, 0.108199f, 0.106131f, 0.104079f, 0.102043f, 0.100023f,
	0.0980186f, 0.09603f, 0.094057f, 0.0920994f, 0.0901571f, 0.08823f, 0.0863179f, 0.0844208f,
	0.0825384f, 0.0806708f, 0.0788178f, 0.0769792f, 0.0751551f, 0.0733451f, 0.0715493f, 0.0697676f,
	0.0679997f, 0.0662457f, 0.0645054f, 0.0627786f, 0.0610654f, 0.0593655f, 0.0576789f, 0.0560055f,
	0.0543452f, 0.0526979f, 0.0510634f, 0.0494416f, 0.0478326f, 0.0462361f, 0.0446521f, 0.0430805f,
	0.0415211f, 0.039974f, 0.0384389f, 0.0369158f, 0.0354046f, 0.0339052f, 0.0324175f, 0.0309415f,
	0.029477f, 0.0280239f, 0.0265822f, 0.0251517f, 0.0237324f, 0.0223242f, 0.020927f, 0.0195408f,
	0.0181653f, 0.0168006f, 0.0154466f, 0.0141031f, 0.0127701f, 0.0114476f, 0.0101354f, 0.00883339f,
	0.00754159f, 0.00625989f, 0.00498819f, 0.00372644f, 0.00247454f, 0.00123242f, 0.f
};

static void ellipse_bounds(float A, float B, float C, float F, float &xmax, float &ymax)
{
	float denom = 4.0f * A * C - B * B;
	if (denom > 0.0f && A != 0.0f && C != 0.0f) {
		xmax = sqrt(F) / (2.0f * A) * (sqrt(F * (4.0f * A - B * B / C)) + B * B * sqrt(F / (C * denom)));
		ymax = sqrt(F) / (2.0f * C) * (sqrt(F * (4.0f * C - B * B / A)) + B * B * sqrt(F / (A * denom)));
	}
	else {
		xmax = 0.0f;
		ymax = 0.0f;
	}
}

static void ellipse_params(float Ux, float Uy, float Vx, float Vy,
                           float &A, float &B, float &C, float &F, float &umax, float &vmax)
{
	A = Vx * Vx + Vy * Vy;
	B = -2.0f * (Ux * Vx + Uy * Vy);
	C = Ux * Ux + Uy * Uy;
	F = A * C - B * B * 0.25f;

	float factor = (F != 0.0f ? (float)(EWA_MAXIDX + 1) / F : 0.0f);
	A *= factor;
	B *= factor;
	C *= factor;
	F = (float)(EWA_MAXIDX + 1);

	ellipse_bounds(A, B, C, sqrtf(F), umax, vmax);
}

/**
 * Filtering method based on
 * "Creating raster omnimax images from multiple perspective views using the elliptical weighted average filter"
 * by Ned Greene and Paul S. Heckbert (1986)
 */
void MemoryBuffer::readEWA(float result[4], const float uv[2], const float derivatives[2][2], PixelSampler sampler)
{
	zero_v4(result);
	int width = this->getWidth(), height = this->getHeight();
	if (width == 0 || height == 0)
		return;

	float u = uv[0], v = uv[1];
	float Ux = derivatives[0][0], Vx = derivatives[1][0], Uy = derivatives[0][1], Vy = derivatives[1][1];
	float A, B, C, F, ue, ve;
	ellipse_params(Ux, Uy, Vx, Vy, A, B, C, F, ue, ve);

	/* Note: highly eccentric ellipses can lead to large texture space areas to filter!
	 * This is limited somewhat by the EWA_WTS size in the loop, but a nicer approach
	 * could be the one found in
	 * "High Quality Elliptical Texture Filtering on GPU"
	 * by Pavlos Mavridis and Georgios Papaioannou
	 * in which the eccentricity of the ellipse is clamped.
	 */

	int U0 = (int)u;
	int V0 = (int)v;
	/* pixel offset for interpolation */
	float ufac = u - floor(u), vfac = v - floor(v);
	/* filter size */
	int u1 = (int)(u - ue);
	int u2 = (int)(u + ue);
	int v1 = (int)(v - ve);
	int v2 = (int)(v + ve);

	/* sane clamping to avoid unnecessarily huge loops */
	/* note: if eccentricity gets clamped (see above),
	 * the ue/ve limits can also be lowered accordingly
	 */
	if (U0 - u1 > EWA_MAXIDX) u1 = U0 - EWA_MAXIDX;
	if (u2 - U0 > EWA_MAXIDX) u2 = U0 + EWA_MAXIDX;
	if (V0 - v1 > EWA_MAXIDX) v1 = V0 - EWA_MAXIDX;
	if (v2 - V0 > EWA_MAXIDX) v2 = V0 + EWA_MAXIDX;

	float DDQ = 2.0f * A;
	float U = u1 - U0;
	float ac1 = A * (2.0f * U + 1.0f);
	float ac2 = A * U * U;
	float BU = B * U;

	float sum = 0.0f;
	for (int v = v1; v <= v2; ++v) {
		float V = v - V0;

		float DQ = ac1 + B * V;
		float Q = (C * V + BU) * V + ac2;
		for (int u = u1; u <= u2; ++u) {
			if (Q < F) {
				float tc[4];
				const float wt = EWA_WTS[CLAMPIS((int)Q, 0, EWA_MAXIDX)];
				switch (sampler) {
					case COM_PS_NEAREST: read(tc, u, v); break;
					case COM_PS_BILINEAR: readBilinear(tc, (float)u + ufac, (float)v + vfac); break;
					case COM_PS_BICUBIC: readBilinear(tc, (float)u + ufac, (float)v + vfac); break; /* XXX no readBicubic method yet */
					default: zero_v4(tc); break;
				}
				madd_v4_v4fl(result, tc, wt);
				sum += wt;
			}
			Q += DQ;
			DQ += DDQ;
		}
	}
	
	mul_v4_fl(result, (sum != 0.0f ? 1.0f / sum : 0.0f));
}

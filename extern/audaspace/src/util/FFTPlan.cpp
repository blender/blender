/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
******************************************************************************/

#include "util/FFTPlan.h"

AUD_NAMESPACE_BEGIN
FFTPlan::FFTPlan(double measureTime) :
	FFTPlan(DEFAULT_N, measureTime)
{
}

FFTPlan::FFTPlan(int n, double measureTime) :
	m_N(n), m_bufferSize(((n/2)+1)*2*sizeof(fftwf_complex))
{
	fftwf_set_timelimit(measureTime);
	void* buf = fftwf_malloc(m_bufferSize);
	m_fftPlanR2C = fftwf_plan_dft_r2c_1d(m_N, (float*)buf, (fftwf_complex*)buf, FFTW_EXHAUSTIVE);
	m_fftPlanC2R = fftwf_plan_dft_c2r_1d(m_N, (fftwf_complex*)buf, (float*)buf, FFTW_EXHAUSTIVE);
	fftwf_free(buf);
}

FFTPlan::~FFTPlan()
{
	fftwf_destroy_plan(m_fftPlanC2R);
	fftwf_destroy_plan(m_fftPlanR2C);
}

int FFTPlan::getSize()
{
	return m_N;
}

void FFTPlan::FFT(void* buffer)
{
	fftwf_execute_dft_r2c(m_fftPlanR2C, (float*)buffer, (fftwf_complex*)buffer);
}

void FFTPlan::IFFT(void* buffer)
{
	fftwf_execute_dft_c2r(m_fftPlanC2R, (fftwf_complex*)buffer, (float*)buffer);
}

void* FFTPlan::getBuffer()
{
	return fftwf_malloc(m_bufferSize);
}

void FFTPlan::freeBuffer(void* buffer)
{
	fftwf_free(buffer);
}

AUD_NAMESPACE_END

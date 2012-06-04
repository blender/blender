/* 
 * Adapted from Open Shading Language with this license: 
 * 
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al. 
 * All Rights Reserved. 
 * 
 * Modifications Copyright 2011, Blender Foundation. 
 *  
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met: 
 * * Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer. 
 * * Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution. 
 * * Neither the name of Sony Pictures Imageworks nor the names of its 
 *   contributors may be used to endorse or promote products derived from 
 *   this software without specific prior written permission. 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/

CCL_NAMESPACE_BEGIN

/* EMISSION CLOSURE */

/// Return the probability distribution function in the direction I,
/// given the parameters and the light's surface normal.  This MUST match
/// the PDF computed by sample().
__device float emissive_pdf(const float3 Ng, const float3 I)
{
	float cosNO = fabsf(dot(Ng, I));
	return (cosNO > 0.0f)? 1.0f: 0.0f;
}

__device float3 emissive_eval(const float3 Ng, const float3 I)
{
	float res = emissive_pdf(Ng, I);
	
	return make_float3(res, res, res);
}

__device float3 svm_emissive_eval(ShaderData *sd, ShaderClosure *sc)
{
	return emissive_eval(sd->Ng, sd->I);
}

CCL_NAMESPACE_END


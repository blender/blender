/*
 * Copyright 2006, NVIDIA Corporation Ignacio Castano <icastano@nvidia.com>
 *
 * Modifications copyright (c) 2011, Blender Foundation.
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __SUBD_STENCIL__
#define __SUBD_STENCIL__

#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class StencilMask
{
public:
	StencilMask();
	StencilMask(int size);

	void resize(int size);

	StencilMask& operator=(float value);

	void operator+=(const StencilMask& mask);
	void operator-=(const StencilMask& mask);
	void operator*=(float scale);
	void operator/=(float scale);

	int size() const { return weights.size(); }

	float operator[](int i) const { return weights[i]; }
	float& operator[](int i) { return weights[i]; }

	float sum() const;
	bool is_normalized() const;
	void normalize();

private:
	vector<float> weights;
};

CCL_NAMESPACE_END

#endif /* __SUBD_STENCIL__ */


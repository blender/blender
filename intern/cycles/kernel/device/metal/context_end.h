/*
 * Copyright 2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
}
; /* end of MetalKernelContext class definition */

/* Silently redirect into the MetalKernelContext instance */
/* NOTE: These macros will need maintaining as entry-points change. */

#undef kernel_integrator_state
#define kernel_integrator_state context.launch_params_metal.__integrator_state

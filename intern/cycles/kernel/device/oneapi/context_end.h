/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */
}
; /* end of ONEAPIKernelContext class definition */

#undef kernel_data
#define kernel_data (kernel_params->data)

#undef kernel_integrator_state
#define kernel_integrator_state (kernel_params->integrator_state)

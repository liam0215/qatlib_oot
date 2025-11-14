/***************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2023 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *  version: QAT20.L.1.2.30-00109
 *
 ***************************************************************************/

/**
 ***************************************************************************
 * @file icp_sal.h
 *
 * @ingroup SalCommon
 *
 * Functions for both user space and kernel space.
 *
 ***************************************************************************/

#ifndef ICP_SAL_H
#define ICP_SAL_H

#ifdef ICP_DC_ERROR_SIMULATION
/*
 * icp_sal_dc_simulate_error
 *
 * @description:
 *  This function injects a simulated compression error for a defined
 *  number of compression requests
 *
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] numErrors              Num DC Errors
 *                                   0 - No Error injection
 *                                   1-0xFE - Num Errors to Inject
 *                                   0xFF - Always inject Error
 * @param[in] dcError                DC Error Type
 * @retval CPA_STATUS_SUCCESS        No error
 * @retval CPA_STATUS_FAIL           Operation failed
 */
CpaStatus icp_sal_dc_simulate_error(Cpa8U numErrors, Cpa8S dcError);
#endif

/*
 * icp_sal_get_dc_errors
 *
 * @description:
 *  This function returns the occurrences of compression errors specified
 *  in the input parameter
 *
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 * @param[in] dcError                DC Error Type
 *
 * returns                           Number of failing requests of type dcError
 */
Cpa64U icp_sal_get_dc_error(Cpa8S dcError);

/*
 * icp_sal_cnv_simulate_error
 *
 * @description:
 *  This function enables the CnVError injection for the
 *  session passed in. All Compression requests sent within
 *  the session are injected with CnV errors. This error injection
 *  is for the duration of the session. Resetting the session
 *  results in setting being cleared.
 *  CnV error injection does not apply to Data Plane API.
 *
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      The session has been initialized via cpaDcInitSession function
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] dcInstance             Instance Handle
 * @param[in] pSessionHandle         Session Handle
 *
 * @retval CPA_STATUS_UNSUPPORTED    Unsupported feature
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in
 * @retval CPA_STATUS_SUCCESS        No error
 *
 */
CpaStatus icp_sal_cnv_simulate_error(CpaInstanceHandle dcInstance,
                                     CpaDcSessionHandle pSessionHandle);

/*
 * icp_sal_ns_cnv_simulate_error
 *
 * @description:
 *  This function enables the CnVError injection for the
 *  sessionless case. All Compression requests sent
 *  to the dcInstance that is  passed in as a parameter,
 *  are injected with CnV errors. This CnV error injection
 *  does not apply to Data Plane API.
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] dcInstance             Instance Handle
 *
 * @retval CPA_STATUS_UNSUPPORTED    Unsupported feature
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in
 * @retval CPA_STATUS_SUCCESS        No error
 *
 */
CpaStatus icp_sal_ns_cnv_simulate_error(CpaInstanceHandle dcInstance);

/*
 * icp_sal_ns_cnv_reset_error
 *
 * @description:
 *  This function resets the CnVError injection for the
 *  specific dcInstance that is  passed in as a parameter.
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] dcInstance             Instance Handle
 *
 * @retval CPA_STATUS_UNSUPPORTED    Unsupported feature
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in
 * @retval CPA_STATUS_SUCCESS        No error
 *
 */
CpaStatus icp_sal_ns_cnv_reset_error(CpaInstanceHandle dcInstance);
#endif

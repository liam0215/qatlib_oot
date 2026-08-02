/******************************************************************************
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
 *  version: QAT20.L.1.2.30-00178
 *
 *****************************************************************************/

/**
 *****************************************************************************
 * @file icp_sal_dc_err_sim.c
 *
 * @defgroup SalCommon
 *
 * @ingroup SalCommon
 *
 *****************************************************************************/

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/
#include "cpa.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "dc_error_counter.h"
#include "dc_session.h"
#include "dc_ns_datapath.h"
#ifdef ICP_DC_ERROR_SIMULATION
#include "dc_err_sim.h"
#endif
#include "icp_sal.h"

#ifdef ICP_DC_ERROR_SIMULATION
CpaStatus icp_sal_dc_simulate_error(Cpa8U numErrors, Cpa8S dcError)
{
    return dcSetNumError(numErrors, dcError);
}
#endif

Cpa64U icp_sal_get_dc_error(Cpa8S dcError)
{
    return getDcErrorCounter(dcError);
}

CpaStatus icp_sal_cnv_simulate_error(CpaInstanceHandle dcInstance,
                                     CpaDcSessionHandle pSessionHandle)
{
    return dcSetCnvError(dcInstance, pSessionHandle);
}

CpaStatus icp_sal_ns_cnv_simulate_error(CpaInstanceHandle dcInstance)
{
    return dcNsEnableCnvErrorInj(dcInstance, CPA_TRUE);
}

CpaStatus icp_sal_ns_cnv_reset_error(CpaInstanceHandle dcInstance)
{
    return dcNsEnableCnvErrorInj(dcInstance, CPA_FALSE);
}

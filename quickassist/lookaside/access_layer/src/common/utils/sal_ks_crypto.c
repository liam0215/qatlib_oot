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
 * @file sal_ks_crypto.c
 *
 * @description
 *    Kernel space support is not enabled for crypto. This file has
 *dummy implementation with unsupported return value for crypto APIs
 *for kernel space.
 *
 ***************************************************************************/

#include "cpa.h"
#include "lac_common.h"
#include "cpa_cy_sym.h"
#include "cpa_cy_common.h"
#include "cpa_cy_im.h"

CpaStatus cpaCySymInitSession(const CpaInstanceHandle instanceHandle,
                              const CpaCySymCbFunc pSymCb,
                              const CpaCySymSessionSetupData *pSessionSetupData,
                              CpaCySymSessionCtx sessionCtx)
{
    LAC_LOG_ERROR("cpaCySymInitSession is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCySymSessionCtxGetSize(
    const CpaInstanceHandle instanceHandle,
    const CpaCySymSessionSetupData *pSessionSetupData,
    Cpa32U *pSessionCtxSizeInBytes)
{
    LAC_LOG_ERROR("cpaCySymSessionCtxGetSize is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCyGetNumInstances(Cpa16U *pNumInstances)
{
    LAC_LOG_ERROR("cpaCyGetNumInstances is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCyStartInstance(CpaInstanceHandle instanceHandle)
{
    LAC_LOG_ERROR("cpaCyStartInstance is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCySymRemoveSession(const CpaInstanceHandle instanceHandle,
                                CpaCySymSessionCtx pSessionCtx)
{
    LAC_LOG_ERROR("cpaCySymRemoveSession is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCyGetInstances(Cpa16U numInstances, CpaInstanceHandle *cyInstances)
{
    LAC_LOG_ERROR("cpaCyGetInstances is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCySetAddressTranslation(const CpaInstanceHandle instanceHandle,
                                     CpaVirtualToPhysical virtual2Physical)
{
    LAC_LOG_ERROR("cpaCySetAddressTranslation is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCyBufferListGetMetaSize(const CpaInstanceHandle instanceHandle,
                                     Cpa32U numBuffers,
                                     Cpa32U *pSizeInBytes)
{
    LAC_LOG_ERROR("cpaCyBufferListGetMetaSize is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCyStopInstance(CpaInstanceHandle instanceHandle)
{
    LAC_LOG_ERROR("cpaCyStopInstance is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

CpaStatus cpaCySymPerformOp(const CpaInstanceHandle instanceHandle,
                            void *pCallbackTag,
                            const CpaCySymOpData *pOpData,
                            const CpaBufferList *pSrcBuffer,
                            CpaBufferList *pDstBuffer,
                            CpaBoolean *pVerifyResult)
{
    LAC_LOG_ERROR("cpaCySymPerformOp is unsupported");
    return CPA_STATUS_UNSUPPORTED;
}

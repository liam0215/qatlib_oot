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
 *  version: QAT20.L.1.2.30-00178
 *
 ***************************************************************************/

/*
 * This file contains functions for using symmetric algorithms, specifically
 * using QAT API to perform a "chained" cipher and hash operation. It encrypts
 * text using the AES-256 algorithm in CBC mode, and then performs a SHA-256
 * hash on the ciphertext.
 */

#include "cpa_cy_sym.h"
#include "cpa_sample_utils.h"
#include "icp_sal_user.h"

#define DIGEST_LENGTH 32
#define TIMEOUT_MS 5000 /* 5 seconds */
#define RETRY_LIMIT (100)
#define RECOVER_WAIT_LOOPS 1000
#define HB_ERR_TRIG_ITER 3
#define MAX_ITER 10
#define FIRST_ITER 1
#define DEVICE 0

typedef enum recovery_state_e
{
    PRE_RECOVERY,
    WAIT_FOR_RECOVERY,
    POST_RECOVERY,
    RECOVERY_SUCCESSFUL,
    RECOVERY_FAILED
} hb_recovery_state;

CpaBoolean sessionReuse = CPA_FALSE;
volatile hb_recovery_state recovery = PRE_RECOVERY;
Cpa64U numOpPostRecovery = 0;
Cpa64U numOpPreRecovery = 0;
Cpa64U numOpFailed = 0;
Cpa64U numOp = 0;
static sampleThread gPayloadThread;
static volatile int gPayload = 0;
extern int gDebugParam;
Cpa32U monitorTest = 0;

/* AES key, 256 bits long */
static Cpa8U sampleCipherKey[] = {
    0xEE, 0xE2, 0x7B, 0x5B, 0x10, 0xFD, 0xD2, 0x58, 0x49, 0x77, 0xF1, 0x22,
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x41, 0x4C, 0x41, 0x4E, 0xA1, 0xAA, 0x01, 0x3F};


/* Initialization vector */
static Cpa8U sampleCipherIv[] = {
    0x7E, 0x9B, 0x4C, 0x1D, 0x82, 0x4A, 0xC5, 0xDF, 0x99, 0x4C, 0xA1, 0x44,
    0xAA, 0x8D, 0x37, 0x27};

/* Source data to encrypt */
static Cpa8U sampleAlgChainingSrc[] = {
    0xD7, 0x1B, 0xA4, 0xCA, 0xEC, 0xBD, 0x15, 0xE2, 0x52, 0x6A, 0x21, 0x0B,
    0x81, 0x77, 0x0C, 0x90, 0x68, 0xF6, 0x86, 0x50, 0xC6, 0x2C, 0x6E, 0xED,
    0x2F, 0x68, 0x39, 0x71, 0x75, 0x1D, 0x94, 0xF9, 0x0B, 0x21, 0x39, 0x06,
    0xBE, 0x20, 0x94, 0xC3, 0x43, 0x4F, 0x92, 0xC9, 0x07, 0xAA, 0xFE, 0x7F,
    0xCF, 0x05, 0x28, 0x6B, 0x82, 0xC4, 0xD7, 0x5E, 0xF3, 0xC7, 0x74, 0x68,
    0xCF, 0x05, 0x28, 0x6B, 0x82, 0xC4, 0xD7, 0x5E, 0xF3, 0xC7, 0x74, 0x68,
    0x80, 0x8B, 0x28, 0x8D, 0xCD, 0xCA, 0x94, 0xB8, 0xF5, 0x66, 0x0C, 0x00,
    0x5C, 0x69, 0xFC, 0xE8, 0x7F, 0x0D, 0x81, 0x97, 0x48, 0xC3, 0x6D, 0x24};

/* Expected output of the encryption operation with the specified
 * cipher (CPA_CY_SYM_CIPHER_AES_CBC), key (sampleCipherKey) and
 * initialization vector (sampleCipherIv) */
static Cpa8U expectedOutput[] = {
    0xC1, 0x92, 0x33, 0x36, 0xF9, 0x50, 0x4F, 0x5B, 0xD9, 0x79, 0xE1, 0xF6,
    0xC7, 0x7A, 0x7D, 0x75, 0x47, 0xB7, 0xE2, 0xB9, 0xA1, 0x1B, 0xB9, 0xEE,
    0x16, 0xF9, 0x1A, 0x87, 0x59, 0xBC, 0xF2, 0x94, 0x7E, 0x71, 0x59, 0x52,
    0x3B, 0xB7, 0xF6, 0xB0, 0xB8, 0xE6, 0xC3, 0x9C, 0xA2, 0x4B, 0x5A, 0x8A,
    0x25, 0x61, 0xAB, 0x65, 0x4E, 0xB5, 0xD1, 0x3D, 0xB2, 0x7D, 0xA3, 0x9D,
    0x1E, 0x71, 0x45, 0x14, 0x5E, 0x9B, 0xB4, 0x75, 0xD3, 0xA8, 0xED, 0x40,
    0x01, 0x19, 0x2B, 0xEB, 0x04, 0x35, 0xAA, 0xA9, 0xA7, 0x95, 0x69, 0x77,
    0x40, 0xD9, 0x1D, 0xE4, 0xE7, 0x1A, 0xF9, 0x35, 0x06, 0x61, 0x3F, 0xAF,
    /* Digest */
    0xEE, 0x6F, 0x90, 0x7C, 0xB5, 0xF4, 0xDE, 0x75, 0xD3, 0xBC, 0x11, 0x63,
    0xE7, 0xF0, 0x5D, 0x15, 0x5E, 0x61, 0x16, 0x13, 0x83, 0x1A, 0xD6, 0x56,
    0x44, 0xA7, 0xF6, 0xA2, 0x6D, 0xAB, 0x1A, 0xF2};

/*
 * Callback function
 *
 * This function is "called back" (invoked by the implementation of
 * the API) when the asynchronous operation has completed.  The
 * context in which it is invoked depends on the implementation, but
 * as described in the API it should not sleep (since it may be called
 * in a context which does not permit sleeping, e.g. a Linux bottom
 * half).
 *
 * This function can perform whatever processing is appropriate to the
 * application.  For example, it may free memory, continue processing,
 * etc.  In this example, the function only sets the complete variable
 * to indicate it has been called.
 */
static void hb_SymCallback(void *pCallbackTag,
                           CpaStatus status,
                           const CpaCySymOp operationType,
                           void *pOpData,
                           CpaBufferList *pDstBuffer,
                           CpaBoolean verifyResult)
{
    PRINT_DBG("Callback called with status = %d.\n", status);

    if (NULL != pCallbackTag)
    {
        /** indicate that the function has been called */
        COMPLETE((struct COMPLETION_STRUCT *)pCallbackTag);
    }
}

/*
 * Perform chaining operation (cipher + hash)
 */
static CpaStatus hb_SymPerformOp(CpaInstanceHandle cyInstHandle, void *vp)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa8U *pBufferMeta = NULL;
    Cpa32U bufferMetaSize = 0;
    CpaBufferList *pBufferList = NULL;
    CpaFlatBuffer *pFlatBuffer = NULL;
    CpaCySymOpData *pOpData = NULL;
    Cpa32U bufferSize = sizeof(sampleAlgChainingSrc) + DIGEST_LENGTH;
    Cpa32U numBuffers = 1;
    Cpa32U bufferListMemSize =
        sizeof(CpaBufferList) + (numBuffers * sizeof(CpaFlatBuffer));
    Cpa8U *pSrcBuffer = NULL;
    Cpa8U *pIvBuffer = NULL;
    Cpa16U retries = 0;
    CpaCySymSessionCtx sessionCtx = NULL;
    sessionCtx = (CpaCySymSessionCtx)vp;

    /* The following variables are allocated on the stack because we block
     * until the callback comes back. If a non-blocking approach was to be
     * used then these variables should be dynamically allocated */
    struct COMPLETION_STRUCT complete;

    /* get meta information size */
    PRINT_DBG("cpaCyBufferListGetMetaSize\n");
    status =
        cpaCyBufferListGetMetaSize(cyInstHandle, numBuffers, &bufferMetaSize);

    if (status == CPA_STATUS_SUCCESS)
    {
        status = PHYS_CONTIG_ALLOC(&pBufferMeta, bufferMetaSize);
    }

    if (status == CPA_STATUS_SUCCESS)
    {
        /* allocate memory for bufferlist and array of flat buffers in a
         * contiguous area and carve it up to reduce number of memory
         * allocations required. */
        status = OS_MALLOC(&pBufferList, bufferListMemSize);
    }

    if (status == CPA_STATUS_SUCCESS)
    {
        status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);
    }

    if (status == CPA_STATUS_SUCCESS)
    {
        status = PHYS_CONTIG_ALLOC(&pIvBuffer, sizeof(sampleCipherIv));
    }

    if (status == CPA_STATUS_SUCCESS)
    {
        /* copy source into buffer */
        memcpy(pSrcBuffer, sampleAlgChainingSrc, sizeof(sampleAlgChainingSrc));

        /* copy IV into buffer */
        memcpy(pIvBuffer, sampleCipherIv, sizeof(sampleCipherIv));

        /* increment by sizeof(CpaBufferList) to get at the
         * array of flatbuffers */
        pFlatBuffer = (CpaFlatBuffer *)(pBufferList + 1);

        pBufferList->pBuffers = pFlatBuffer;
        pBufferList->numBuffers = 1;
        pBufferList->pPrivateMetaData = pBufferMeta;

        pFlatBuffer->dataLenInBytes = bufferSize;
        pFlatBuffer->pData = pSrcBuffer;

        status = OS_MALLOC(&pOpData, sizeof(CpaCySymOpData));
    }

    if (status == CPA_STATUS_SUCCESS)
    {
        /* Populate the structure containing the operational data that is
         * needed to run the algorithm */
        if (sessionCtx == NULL)
        {
            PRINT_ERR("Null sessionCtx \n");
            status = CPA_STATUS_FATAL;
            goto cleanup;
        }
        pOpData->sessionCtx = sessionCtx;
        pOpData->packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
        pOpData->pIv = pIvBuffer;
        pOpData->ivLenInBytes = sizeof(sampleCipherIv);
        pOpData->cryptoStartSrcOffsetInBytes = 0;
        pOpData->hashStartSrcOffsetInBytes = 0;
        pOpData->messageLenToCipherInBytes = sizeof(sampleAlgChainingSrc);
        pOpData->messageLenToHashInBytes = sizeof(sampleAlgChainingSrc);
        /* pDigestResult does need to be set as digestIsAppended was set
         * at sessionInit */
    }

    if (status != CPA_STATUS_SUCCESS)
    {
        /* There has been a problem in allocating resources for operation.
         * Indicate this via a different status. */
        PRINT_ERR("Error in setting up Sym operation\n");
        status = CPA_STATUS_FATAL;
        goto cleanup;
    }

    /* initialization for callback; the "complete" variable is used by the
     * callback function to indicate it has been called*/
    COMPLETION_INIT(&complete);

    do
    {
        /* Submit symmetric operation */
        status = cpaCySymPerformOp(
            cyInstHandle,
            (void *)&complete, /* data sent as is to the callback function*/
            pOpData,           /* operational data struct */
            pBufferList,       /* source buffer list */
            pBufferList,       /* same src & dst for an in-place operation */
            NULL);
    } while ((status == CPA_STATUS_RETRY) && (++retries <= RETRY_LIMIT));

    if (status != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("cpaCySymPerformOp failed. (status = %d)\n", status);
        status = CPA_STATUS_FAIL;
        goto cleanup;
    }

    /* wait until the completion of the operation */
    if (!COMPLETION_WAIT(&complete, TIMEOUT_MS))
    {
        PRINT_ERR("timeout or interruption in cpaCySymPerformOp\n");
        status = CPA_STATUS_FAIL;
        goto cleanup;
    }

    PRINT_DBG("Total Retries in this loop: %d\n", retries);
    if (memcmp(pSrcBuffer, expectedOutput, bufferSize) == 0)
    {
        PRINT_DBG("Output matches expected output\n");
    }
    else
    {
        PRINT_ERR("Output buffer does not match the expected\n");
        status = CPA_STATUS_FAIL;
    }

cleanup:
    PHYS_CONTIG_FREE(pSrcBuffer);
    PHYS_CONTIG_FREE(pIvBuffer);
    OS_FREE(pBufferList);
    PHYS_CONTIG_FREE(pBufferMeta);
    OS_FREE(pOpData);

    COMPLETION_DESTROY(&complete);

    return status;
}

CpaStatus hb_SetupSymSessionCtx(CpaInstanceHandle cyInstHandle, void **pntr)
{
    CpaCySymSessionSetupData sessionSetupData = {0};
    Cpa32U sessionCtxSize = 0;
    CpaStatus status;
    CpaCySymSessionCtx *sessionCtx = (CpaCySymSessionCtx *)pntr;

    sessionSetupData.verifyDigest = CPA_FALSE;
    /* populate symmetric session data structure */
    sessionSetupData.sessionPriority = CPA_CY_PRIORITY_NORMAL;
    sessionSetupData.symOperation = CPA_CY_SYM_OP_ALGORITHM_CHAINING;
    sessionSetupData.algChainOrder =
        CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;

    sessionSetupData.cipherSetupData.cipherAlgorithm =
        CPA_CY_SYM_CIPHER_AES_CBC;
    sessionSetupData.cipherSetupData.pCipherKey = sampleCipherKey;
    sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
        sizeof(sampleCipherKey);
    sessionSetupData.cipherSetupData.cipherDirection =
        CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;

    sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA256;
    sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
    sessionSetupData.hashSetupData.digestResultLenInBytes = DIGEST_LENGTH;
    sessionSetupData.hashSetupData.authModeSetupData.authKey = sampleCipherKey;
    sessionSetupData.hashSetupData.authModeSetupData.authKeyLenInBytes =
        sizeof(sampleCipherKey);

    /* The resulting MAC is to be placed immediately after the ciphertext */
    sessionSetupData.digestIsAppended = CPA_TRUE;
    sessionSetupData.verifyDigest = CPA_FALSE;
    /* Determine size of session context to allocate */
    status = cpaCySymSessionCtxGetSize(
        cyInstHandle, &sessionSetupData, &sessionCtxSize);
    if (status == CPA_STATUS_SUCCESS)
    {
        /* Allocate session context */
        status = PHYS_CONTIG_ALLOC(sessionCtx, sessionCtxSize);
    }
    if (status == CPA_STATUS_SUCCESS)
    {
        /* Initialize the session */
        status = cpaCySymInitSession(
            cyInstHandle, hb_SymCallback, &sessionSetupData, *sessionCtx);
    }

    return status;
}

CpaStatus hb_SymCleanupSession(CpaInstanceHandle symInstHandle, void *pntr)
{
    CpaCySymSessionCtx sessionCtx = (CpaCySymSessionCtx)pntr;
    cpaCySymRemoveSession(symInstHandle, sessionCtx);
    PHYS_CONTIG_FREE(sessionCtx);
    return CPA_STATUS_SUCCESS;
}

CpaStatus hb_SymCleanupInstance(CpaInstanceHandle symInstHandle, void *pntr)
{
    CpaCySymSessionCtx sessionCtx = (CpaCySymSessionCtx)pntr;
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaCySymStats64 symStats = {0};
    /* Remove the session - session init has already succeeded */
    status = cpaCySymQueryStats64(symInstHandle, &symStats);

    if (status != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("cpaCySymQueryStats failed, status = %d\n", status);
    }
    else
    {
        PRINT_DBG("Number of symmetric operation completed: %llu\n",
                  (unsigned long long)symStats.numSymOpCompleted);
    }

    hb_SymCleanupSession(symInstHandle, sessionCtx);

    /* Stop the polling thread */
    sampleCyStopPolling();

    cpaCyStopInstance(symInstHandle);
    return CPA_STATUS_SUCCESS;
}

CpaStatus hb_Payload(void)
{
    CpaInstanceHandle cyInstHandle = NULL;
    CpaStatus status = CPA_STATUS_FAIL;
    void *ctx;
    char tname[128];
    Cpa32U numWaitLoops = 0;
    snprintf(tname,
             128,
             "%s-%s",
             "symmetric",
             (sessionReuse == CPA_TRUE) ? "Session Reuse" : "Instance Reuse");
    /*
     * In this simplified version of instance discovery, we discover
     * exactly one instance of a crypto service.
     */
    sampleSymGetInstance(&cyInstHandle);
    if (cyInstHandle == NULL)
    {
        PRINT("hb_TestRecovery Failed as cyInstHandle is NULL\n");
        return CPA_STATUS_FAIL;
    }

    /* Start Cryptographic component */
    PRINT_DBG("cpaCyStartInstance\n");
    status = cpaCyStartInstance(cyInstHandle);

    if (CPA_STATUS_SUCCESS == status)
    {
        /*
         * Set the address translation function for the instance
         */
        status = cpaCySetAddressTranslation(cyInstHandle, sampleVirtToPhys);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /*
         * If the instance is polled start the polling thread. Note that
         * how the polling is done is implementation-dependant.
         */
        sampleCyStartPolling(cyInstHandle);
    }

    status = hb_SetupSymSessionCtx(cyInstHandle, &ctx);
    if (status != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("\n%s:Error setting up Session\n", tname);
        return CPA_STATUS_FAIL;
    }

    do
    {
        /*
         * recovery states
         * current state         |next state             |next state
         *                       |    on Success.        |       on Failure.
         * -------------------------------------------------------------------
         * PRE_RECOVERY (s)      |PRE_RECOVERY (s)       |WAIT_FOR_RECOVERY (s)
         * WAIT_FOR_RECOVERY (s) |POST_RECOVERY (t)      |RECOVERY_FAILED (s)
         * POST_RECOVERY (t)     |RECOVERY_SUCCESSFUL (s)|RECOVERY_FAILED (s)
         * -------------------------------------------------------------------
         * (s) - steady state       (t) - transient state
         */
        if (recovery == PRE_RECOVERY)
        {
            status = hb_SymPerformOp(cyInstHandle, ctx);
            numOp++;
            if (status == CPA_STATUS_SUCCESS)
            {
                numOpPreRecovery++;
            }
            else
            {
                numOpFailed++;
            }
        }
        else if (recovery == WAIT_FOR_RECOVERY)
        {
            /* While waiting for recovery do not submit any further
             * jobs, irrespective of the status of the previous job.
             * The loop is not infinite as the recovery state changes to
             * recovery successful or it times out.
             */
            OS_SLEEP(10);
            numWaitLoops++;
            if (numWaitLoops > RECOVER_WAIT_LOOPS)
            {
                PRINT_ERR("\n%s:Recovery Failed!\n", tname);
                recovery = RECOVERY_FAILED;
                status = CPA_STATUS_FAIL;
                break;
            }
        }
        else if (recovery == POST_RECOVERY)
        {
            /* Finished recovery. Submit the payload again.
             * Do not setup a new session if session recovery is to tested.
             */
            if (sessionReuse == CPA_FALSE)
            {
                hb_SymCleanupSession(cyInstHandle, ctx);
                PRINT("\n%s:Re-initializing Session\n", tname);
                status = hb_SetupSymSessionCtx(cyInstHandle, &ctx);
                if (status != CPA_STATUS_SUCCESS)
                {
                    PRINT_ERR("\n%s: Session Setup failed post recovery \n",
                              tname);
                    status = CPA_STATUS_FAIL;
                    recovery = RECOVERY_FAILED;
                    break;
                }
            }
            else
            {
                PRINT("\n%s: Reusing same session before failure\n", tname);
            }
            recovery = RECOVERY_SUCCESSFUL;
        }
        else if (recovery == RECOVERY_SUCCESSFUL)
        {
            /* RECOVERY_SUCCESSFUL state is same as PRE_RECOVERY QAT state,
             * to process payload.
             */
            status = hb_SymPerformOp(cyInstHandle, ctx);
            numOp++;
            if (status == CPA_STATUS_SUCCESS)
            {
                numOpPostRecovery++;
            }
            else
            {
                numOpFailed++;
            }
        }
        /* Operation fails due to error injection but monitor
         * does not detect waiting for recovery earlier, then first failure
         * here is expected and valid. To avoid such false positive,
         * multiple failures need to be detected.
         */
    } while ((gPayload) && (numOpFailed <= 2));

    PRINT_DBG(
        "\n%s: Number of Operation Pre Recovery: %ld Post Recovery: %ld\n",
        tname,
        numOpPreRecovery,
        numOpPostRecovery);

    hb_SymCleanupInstance(cyInstHandle, ctx);

    return status;
}

void payload_setup(void)
{
    gPayload = 1;
    CpaStatus stat = CPA_STATUS_SUCCESS;

    stat = icp_sal_userStart("SSL");
    if (stat != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("Failed to start user process SSL\n");
    }

    stat = hb_Payload();
    if (stat != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("\nhb_Payload failed\n");
    }
    else
    {
        PRINT_DBG("\nhb_Payload finished \n");
    }

    icp_sal_userStop();
    sampleThreadExit();
    return;
}

void startPayload(void)
{
    recovery = PRE_RECOVERY;
    numOpPreRecovery = 0;
    numOpPostRecovery = 0;
    numOp = 0;
    numOpFailed = 0;

    /* Start payload thread */
    sampleThreadCreate(&gPayloadThread, payload_setup, NULL);
    return;
}

void stopPayload(void)
{
    gPayload = 0;
    monitorTest = 0;
    OS_SLEEP(2000); /* sleep 2000 milliseconds */
    PRINT("\nNumber of Operation completed   :  \
           \n               Total Operations : %ld  \
           \n               Pre Recovery     : %ld  \
           \n               Post Recovery    : %ld  \
           \n               Failed Operations: %ld\n\n\n",
          numOp,
          numOpPreRecovery,
          numOpPostRecovery,
          numOpFailed);
    return;
}

CpaStatus hbTest()
{
    CpaStatus status = CPA_STATUS_FAIL;
    CpaStatus devStatus = CPA_STATUS_SUCCESS;
    Cpa32U iter = 0;

    /* starts payload */
    startPayload();
    while (1)
    {
        /* enables device polling for hb status */
        status = icp_sal_poll_device_events();
        if (status != CPA_STATUS_SUCCESS)
        {
            PRINT_ERR("Polling for Instance Event Failed!\n");
        }
        /* check device status */
        devStatus = icp_sal_check_device(DEVICE);

        /*
         * icp_sal_check_device() function returns
         * CPA_STATUS_SUCCESS - device available
         * CPA_STATUS_FAIL    - device in failure
         * CPA_STATUS_UNSUPPORTED - unsupported device
         */
        switch (devStatus)
        {
            case CPA_STATUS_SUCCESS:
                fprintf(stderr, "A");
                if (recovery == WAIT_FOR_RECOVERY)
                {
                    recovery = POST_RECOVERY;
                }
                break;
            case CPA_STATUS_UNSUPPORTED:
                fprintf(stderr, "U");
                break;
            case CPA_STATUS_FAIL:
                fprintf(stderr, "F");
                recovery = WAIT_FOR_RECOVERY;
                break;
            default:
                fprintf(stderr, "Unknown status.");
                return CPA_STATUS_FAIL;
        }
        iter++;

        /* triggers heartbeat simulated error */
        if ((iter == HB_ERR_TRIG_ITER) && (monitorTest != 1))
        {
            PRINT("\ncalling icp_sal_heartbeat_simulate_failure \n");
            icp_sal_heartbeat_simulate_failure(DEVICE);
        }

        if ((iter == FIRST_ITER) && (recovery == WAIT_FOR_RECOVERY ||
                                     devStatus == CPA_STATUS_UNSUPPORTED))
        {
            PRINT_ERR("\nDevice not supported or in Failure state \n");
            return CPA_STATUS_FAIL;
        }

        /* stop payload */
        if (iter > MAX_ITER)
        {
            stopPayload();
            if (recovery == WAIT_FOR_RECOVERY || recovery == RECOVERY_FAILED)
            {
                PRINT_ERR("\nDevice recovery failed \n");
                return CPA_STATUS_FAIL;
            }
            break;
        }
        OS_SLEEP(1000); /* sleep 1000 milliseconds */
    }
    return status;
}

CpaStatus hbMonitorTest()
{
    monitorTest = 1;
    PRINT("Heartbeat Availability check. \n");
    return hbTest();
}

CpaStatus hbErrorSimulationTest()
{
    sessionReuse = CPA_FALSE;
    PRINT(
        "Heartbeat Availability check with Error injection, Instance reuse.\n");
    return hbTest();
}

CpaStatus hbSessionReuseTest()
{
    sessionReuse = CPA_TRUE;
    PRINT(
        "Heartbeat Availability check with Error injection, Session reuse.\n");
    return hbTest();
}

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
 ******************************************************************************
 * @file  cpa_heartbeat_sample_user.c
 *
 *****************************************************************************/
#include "cpa_sample_utils.h"
#include "icp_sal_user.h"

extern CpaStatus hbMonitorTest();
extern CpaStatus hbErrorSimulationTest();
extern CpaStatus hbInstanceReuseTest();
extern CpaStatus hbSessionReuseTest();

int gDebugParam = 0;

int main(int argc, const char **argv)
{
    CpaStatus stat = CPA_STATUS_SUCCESS;

    if (argc > 1)
    {
        gDebugParam = atoi(argv[1]);
    }

    PRINT("Starting Heartbeat Functional tests ...\n");

    stat = hbMonitorTest();
    if (stat != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("hbMonitor test failed\n");
        goto finish;
    }

    stat = hbErrorSimulationTest();
    if (stat != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("hbErrorSimulation test failed\n");
        goto finish;
    }

    stat = hbSessionReuseTest();
    if (stat != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("hbSessionReuse test failed\n");
    }

finish:
    if (stat != CPA_STATUS_SUCCESS)
    {
        PRINT_ERR("\nHeartbeat Sample Code App failed\n");
    }
    else
    {
        PRINT("\nHeartbeat Sample Code App finished\n");
    }

    return (int)stat;
}

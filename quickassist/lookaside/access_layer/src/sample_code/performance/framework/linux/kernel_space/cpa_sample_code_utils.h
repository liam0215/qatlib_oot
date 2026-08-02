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
* @file cpa_sample_code_utils.h
*
* This file provides linux kernel os abstraction functions
*
*****************************************************************************/
#ifndef _KERNEL_SPACE_SAMPLECODEUTILS_H__
#define _KERNEL_SPACE_SAMPLECODEUTILS_H__

#include "cpa.h"
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/module.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
#include <linux/vmalloc.h>
#endif
#include <linux/fs.h>
#include "cpa_cy_sym.h"
#include <crypto/hash.h>

/*these are defined to allow an application compile in user space or kernel
 * space, in kernel space these functions are not not needed*/
#define sample_code_thread_mutex_init(mutex_ptr)
#define sample_code_thread_mutex_destroy(mutex_ptr)
#define sample_code_thread_mutex_lock(mutex_ptr)
#define sample_code_thread_mutex_unlock(mutex_ptr)
#define sample_code_thread_cond_init(condPtr)
#define sample_code_thread_cond_broadcast(condPtr)
#define sample_code_thread_cond_destroy(cPtr)
typedef Cpa32U sample_code_thread_barrier_t;
/*there is no such thing as threadExit in kernel space,
 * we provide this macro so that our code
 * builds in kernel space and user space*/
#define sampleCodeThreadExit() return

void sample_code_wait_threads_arrived(Cpa32U sleepTimeout, Cpa32U maxRetries);

#define sample_code_thread_mutex_t Cpa32U
#define sample_code_thread_cond_t Cpa32U
typedef struct semaphore *sample_code_semaphore_t;
typedef struct
{
    struct task_struct *task;
    Cpa32U threadID;
} sample_code_thread_t;
extern int verboseOutput;
static inline void printKernelSpace(const char *format, ...)
{
    if (CPA_TRUE == verboseOutput)
    {
        va_list args;
        va_start(args, format);
        vprintk(format, args);
        va_end(args);
    }
}
#ifdef PRINT
#undef PRINT
#endif
#define PRINT(args...) printKernelSpace(KERN_CONT args)

ssize_t sampleCodeKernelWrite(struct file *file,
                              const char *buf,
                              size_t count,
                              loff_t *pos);

#if (KERNEL_VERSION(3, 17, 0) > LINUX_VERSION_CODE)
#ifndef ktime_get_real_ts64
#define ktime_get_real_ts64 getnstimeofday
#define timespec64 timespec
#endif
#endif

/* SuSE version macro is the same as Linux kernel version */
#ifndef SLE_VERSION
#define SLE_VERSION(a, b, c) KERNEL_VERSION(a, b, c)
#endif
#ifdef CONFIG_SUSE_KERNEL
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
/* SLES15 GA is 4.12.y based */
#define SLE_VERSION_CODE SLE_VERSION(15, 0, 0)
#endif /* LINUX_VERSION_CODE == KERNEL_VERSION(x,y,z) */
#endif /* CONFIG_SUSE_KERNEL */
#ifndef SLE_VERSION_CODE
#define SLE_VERSION_CODE 0
#endif /* SLE_VERSION_CODE */

#endif

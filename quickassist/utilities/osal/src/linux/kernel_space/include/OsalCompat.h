/**
 * @file OsalCompat.h
 * @par
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 * 
 *   GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2007-2023 Intel Corporation. All rights reserved.
 * 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 * 
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 * 
 *   Contact Information:
 *   Intel Corporation
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
 * 
 *  version: QAT20.L.1.2.30-00178
 */

#ifndef OSAL_COMPAT_H
#define OSAL_COMPAT_H

#define SLE_VERSION(a, b, c) KERNEL_VERSION(a, b, c)

#if defined(SUSE_MAJOR_VERSION) && defined(SUSE_MINOR_VERSION)
#define SLE_VERSION_CODE SLE_VERSION(SUSE_MAJOR_VERSION, SUSE_MINOR_VERSION, 0)
#else
#define SLE_VERSION_CODE 0
#endif

#if ((!SLE_VERSION_CODE && KERNEL_VERSION(5, 11, 0) > LINUX_VERSION_CODE) || \
        (SLE_VERSION_CODE && SLE_VERSION(15, 3, 0) > SLE_VERSION_CODE))
#define QAT_SINGLE_SHA_HEADER
#endif

#ifndef RHEL_RELEASE_CODE
#define RHEL_RELEASE_CODE 0
#endif

#ifndef RHEL_RELEASE_VERSION
#define RHEL_RELEASE_VERSION(a, b) (((a) << 8) + (b))
#endif
#define RHEL_MAX_RELEASE_CODE 2403

#if (KERNEL_VERSION(6, 4, 0) > LINUX_VERSION_CODE) && \
        !((RHEL_RELEASE_CODE && RHEL_RELEASE_VERSION(9, 4) <= RHEL_RELEASE_CODE) && \
	   RHEL_RELEASE_CODE != RHEL_MAX_RELEASE_CODE)
#undef class_create
#define class_create(name)                              \
({                                                      \
        static struct lock_class_key __key;             \
        __class_create(THIS_MODULE, name, &__key);      \
})
#endif

#if (KERNEL_VERSION(6, 4, 0) > LINUX_VERSION_CODE)
#undef DEFINE_SEMAPHORE
#define DEFINE_SEMAPHORE(name, n)       \
        struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)
#endif

#endif

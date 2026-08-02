/**
 * @file kernel_space/OsalMMUMgmt.c (linux)
 *
 * @brief IOMMU module.
 *
 *
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

#include "Osal.h"
#include "OsalOsTypes.h"
#include "OsalDevDrv.h"
#include <linux/pci.h>
#include <linux/iommu.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
chr_drv_info_t *drv_info = NULL;

static int osalCreateDevice(chr_drv_info_t *drv_info)
{
    int ret = 0;
    drv_info = kzalloc(sizeof(chr_drv_info_t), GFP_KERNEL);
    if (!drv_info)
    {
        osalLog(OSAL_LOG_LVL_ERROR,
                OSAL_LOG_DEV_STDOUT,
                "failed to allocate memory for drv_info\n");
        return OSAL_FAIL;
    }

    ret = chr_drv_create_device(drv_info, NULL);
    if (ret != OSAL_SUCCESS)
    {
        osalLog(OSAL_LOG_LVL_ERROR,
                OSAL_LOG_DEV_STDOUT,
                "failed to create device driver\n");
        chr_drv_destroy_device(drv_info);
        kfree(drv_info);
        return OSAL_FAIL;
    }

    return OSAL_SUCCESS;
}

#endif

#ifndef ICP_WITHOUT_IOMMU

static struct iommu_domain *domain = NULL;

int osalIOMMUMap(UINT64 iova, UINT64 phaddr, size_t size)
{
    OSAL_MEM_ASSERT(domain);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,2,45) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    return iommu_map(domain, (unsigned long)iova,
              (phys_addr_t)phaddr, get_order(size),
              IOMMU_READ|IOMMU_WRITE|IOMMU_CACHE);
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34)
    return iommu_map_range(domain,(unsigned long)iova,
              (phys_addr_t)phaddr,size,
              IOMMU_READ|IOMMU_WRITE|IOMMU_CACHE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
    return iommu_map(domain,
                     (unsigned long)iova,
                     (phys_addr_t)phaddr,
                     size,
                     IOMMU_READ | IOMMU_WRITE | IOMMU_CACHE,
                     GFP_KERNEL);
#else
    return iommu_map(domain, (unsigned long)iova,
              (phys_addr_t)phaddr, size,
              IOMMU_READ|IOMMU_WRITE|IOMMU_CACHE);
#endif
}

int osalIOMMUUnmap(UINT64 iova, size_t size)
{
    OSAL_MEM_ASSERT(domain);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,2,45) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    return iommu_unmap(domain, (unsigned long)iova, get_order(size));
#else

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34)
    iommu_unmap_range(domain, (unsigned long)iova, size);
#else
    if ( size != iommu_unmap(domain, (unsigned long)iova, size))
    {
        osalLog(OSAL_LOG_LVL_ERROR,
                OSAL_LOG_DEV_STDERR,
                "osalIOMMUUnmap(): Failed to unmap \n");
        return OSAL_FAIL;
    }
#endif
    return OSAL_SUCCESS;
#endif
}

UINT64 osalIOMMUVirtToPhys(UINT64 iova)
{
    OSAL_MEM_ASSERT(domain);
    return (UINT64)iommu_iova_to_phys(domain, (unsigned long)iova);
}

int osalIOMMUAttachDev(void *dev)
{
    OSAL_MEM_ASSERT(domain);
    if( NULL == dev ) {
        osalLog(OSAL_LOG_LVL_ERROR,
                OSAL_LOG_DEV_STDERR,
                "osalIOMMUAttachDev(): Invalid device \n");
        return -ENODEV;
    }
    return iommu_attach_device(domain, dev);
}

void osalIOMMUDetachDev(void *dev)
{
    OSAL_MEM_ASSERT(domain);
    if( NULL == dev ) {
        osalLog(OSAL_LOG_LVL_WARNING,
                OSAL_LOG_DEV_STDERR,
                "osalIOMMUDetachDev(): Invalid device \n");
        return;
    }
    iommu_detach_device(domain, dev);
}

size_t osalIOMMUgetRemappingSize(size_t size)
{
    /* To improve memory usage efficiency 
     * a bit-map based allocation algorithm will be 
     * implemented with the page as the smallest allocation unit
     * therefore remapping size is at least PAGE_SIZE
     */
    int pages = size % PAGE_SIZE ? size/PAGE_SIZE + 1 : size/PAGE_SIZE;        	
    size_t new_size = (pages * PAGE_SIZE);
    return new_size;
}

int osalIOMMUInit(void)
{
    struct iommu_domain* dummy_domain = NULL;
/* depending on the linux version, we first check
 * if iommu is available and allocate the iommu domain
 * to a local variable.
 */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,1,10)
    if (!iommu_found()) {
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
    if (!iommu_present(&pci_bus_type)) {
#else
    if (osalCreateDevice(drv_info) != OSAL_SUCCESS &&
        !device_iommu_mapped(drv_info->drv_class_dev))
    {
        if (!drv_info)
        {
            chr_drv_destroy_device(drv_info);
            kfree(drv_info);
        }
#endif
        osalLog(OSAL_LOG_LVL_ERROR,
                OSAL_LOG_DEV_STDERR,
                "osalIOMMUInit(): iommu not found \n");
        return OSAL_FAIL;
    }

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,1,10)
    dummy_domain = iommu_domain_alloc();
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
    dummy_domain = iommu_domain_alloc(&pci_bus_type);
#else
    dummy_domain = iommu_paging_domain_alloc(drv_info->drv_class_dev);
#endif
    if ( __sync_bool_compare_and_swap((volatile struct iommu_domain **)&domain,NULL,dummy_domain))
    {
/* If domain is NULL it is initialized with dummy_domain.
*/
        if ( NULL == dummy_domain )
        {
            osalLog(OSAL_LOG_LVL_ERROR,
                    OSAL_LOG_DEV_STDERR,
                    "osalIOMMUInit(): Failed to init \n");
            return OSAL_FAIL;
        }
        return OSAL_SUCCESS;
    }

/* If the domain was already allocated then it is
 * still valid and dummy_domain is released.
 */
    if ( dummy_domain ) 
    {
        iommu_domain_free(dummy_domain);    
    }     
    return OSAL_SUCCESS;
}

void osalIOMMUExit(void)
{
    struct iommu_domain* existing_domain =  
            (struct iommu_domain*)__sync_lock_test_and_set(
                                    (volatile struct iommu_domain **)&domain,NULL);
    if( existing_domain )
    {
        iommu_domain_free(existing_domain);
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
    chr_drv_destroy_device(drv_info);
    kfree(drv_info);
#endif
}

#else
int osalIOMMUMap(UINT64 iova, UINT64 phaddr, size_t size)
{
    return 0;
}

int osalIOMMUUnmap(UINT64 iova, size_t size)
{
    return 0;
}

UINT64 osalIOMMUVirtToPhys(UINT64 iova)
{
    return iova;
}

int osalIOMMUAttachDev(void *dev)
{
    return OSAL_SUCCESS;
}

void osalIOMMUDetachDev(void *dev)
{
}

size_t osalIOMMUgetRemappingSize(size_t size)
{
    return size;
}

int osalIOMMUInit(void)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,1,10)
    if (iommu_found()) {
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
    if (iommu_present(&pci_bus_type)) {
#else
    if (osalCreateDevice(drv_info) != OSAL_SUCCESS &&
        device_iommu_mapped(drv_info->drv_class_dev))
    {
        if (!drv_info)
        {
            chr_drv_destroy_device(drv_info);
            kfree(drv_info);
        }
#endif
#ifndef ICP_SRIOV
        osalLog(OSAL_LOG_LVL_ERROR,
                OSAL_LOG_DEV_STDERR,
                "osalIOMMUInit(): iommu is enabled, driver not built with "
                "SRIOV \n");
        return OSAL_FAIL;
#endif
    }
    return OSAL_SUCCESS;
}

void osalIOMMUExit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
    chr_drv_destroy_device(drv_info);
    kfree(drv_info);
#endif
}
#endif

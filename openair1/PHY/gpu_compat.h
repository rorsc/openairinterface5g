/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

/*
  gpu_compat.h  --  C-compatible gpu* names for the CUDA runtime API
*/

#include <cuda_runtime.h>
#include <cuda_runtime_api.h>

typedef cudaError_t    gpuError_t;
typedef cudaStream_t   gpuStream_t;
typedef cudaEvent_t    gpuEvent_t;

#define gpuSuccess cudaSuccess

#define gpuMemcpyHostToDevice   cudaMemcpyHostToDevice
#define gpuMemcpyDeviceToHost   cudaMemcpyDeviceToHost

#define gpuGetErrorName     cudaGetErrorName
#define gpuGetErrorString   cudaGetErrorString
#define gpuPeekAtLastError  cudaPeekAtLastError

#define gpuHostAlloc           cudaHostAlloc
#define gpuHostAllocMapped     cudaHostAllocMapped
#define gpuHostAllocDefault    cudaHostAllocDefault
#define gpuHostAllocPortable   cudaHostAllocPortable
#define gpuFreeHost            cudaFreeHost
#define gpuHostGetDevicePointer cudaHostGetDevicePointer

#define gpuMemcpy              cudaMemcpy
#define gpuMemcpyAsync         cudaMemcpyAsync

#define gpuGetDeviceProperties cudaGetDeviceProperties

#define gpuDeviceProp_t        struct cudaDeviceProp
#define gpuDeviceGetAttribute  cudaDeviceGetAttribute
#define gpuDevAttrManagedMemory cudaDevAttrManagedMemory
#define gpuDevAttrConcurrentManagedAccess cudaDevAttrConcurrentManagedAccess
#define gpuDevAttrUnifiedAddressing cudaDevAttrUnifiedAddressing
#define gpuDevAttrPageableMemoryAccess cudaDevAttrPageableMemoryAccess
#define gpuDevAttrPageableMemoryAccessUsesHostPageTables cudaDevAttrPageableMemoryAccessUsesHostPageTables
#define gpuDevAttrHostRegisterSupported cudaDevAttrHostRegisterSupported
#define gpuDevAttrIntegrated   cudaDevAttrIntegrated

#define gpuMalloc              cudaMalloc
#define gpuFree                cudaFree
#define gpuMemset              cudaMemset
#define gpuMemsetAsync         cudaMemsetAsync

#define gpuGetLastError     cudaGetLastError
#define gpuDeviceSynchronize cudaDeviceSynchronize

#define gpuStreamBeginCapture  cudaStreamBeginCapture
#define gpuStreamCaptureModeThreadLocal cudaStreamCaptureModeThreadLocal

#define gpuStreamCreateWithFlags        cudaStreamCreateWithFlags
#define gpuStreamDestroy       cudaStreamDestroy
#define gpuStreamSynchronize   cudaStreamSynchronize
#define gpuStreamNonBlocking   cudaStreamNonBlocking
#define gpuStreamEndCapture    cudaStreamEndCapture

#define gpuEventRecord         cudaEventRecord

#define gpuGraph_t             cudaGraph_t
#define gpuGraphExec_t         cudaGraphExec_t
#define gpuGraphInstantiate    cudaGraphInstantiate
#define gpuGraphLaunch         cudaGraphLaunch
#define gpuGraphDestroy        cudaGraphDestroy
#define gpuGraphExecDestroy    cudaGraphExecDestroy

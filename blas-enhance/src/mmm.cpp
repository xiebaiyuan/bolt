// Copyright (C) 2019. Huawei Technologies Co., Ltd. All rights reserved.

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "blas-enhance.h"
#ifdef _USE_GENERAL
#include "cpu/general/blas_general.h"
#endif
#ifdef _USE_NEON
#include "cpu/arm/blas_arm.h"
#endif

EE matrix_matrix_multiply_tmp_bytes(TensorDesc matrixADesc, TensorDesc matrixBDesc, U32* bytes, Arch arch)
{
    DataType matrixADataType, matrixBDataType;
    U32 matrixA_M, matrixA_K, matrixB_K, matrixB_N;
    CHECK_STATUS(tensor2dGet(matrixADesc, &matrixADataType, &matrixA_M, &matrixA_K));
    CHECK_STATUS(tensor2dGet(matrixBDesc, &matrixBDataType, &matrixB_K, &matrixB_N));

    EE ret = NOT_SUPPORTED;
    if (arch == CPU_GENERAL) {
#ifdef _USE_GENERAL
        ret = SUCCESS;
#endif
#ifdef _USE_NEON
    } else {
        ret = matrix_matrix_multiply_tmp_bytes_arm(matrixA_M, matrixA_K, matrixB_K, matrixB_N, matrixADataType, bytes);
#endif
    }
    return ret;
}

EE matrix_matrix_multiply(TensorDesc matrixADesc, const void* matrixAData,
     TensorDesc matrixBDesc, const void* matrixBData,
     U32 bytes, void* tmp,
     TensorDesc matrixCDesc, void* matrixCData,
     Arch arch)
{
    if (bytes != 0 && tmp == nullptr) {
        CHECK_STATUS(NULL_POINTER);
    }
    if (nullptr == matrixAData || nullptr == matrixBData || nullptr == matrixCData) {
        CHECK_STATUS(NULL_POINTER);
    }
    
    DataType matrixADataType, matrixBDataType, matrixCDataType;
    DataFormat matrixADataFormat, matrixBDataFormat;
    U32 matrixA_M, matrixA_K, matrixB_K, matrixB_N, matrixC_M, matrixC_N;
    CHECK_STATUS(tensor2dfGet(matrixADesc, &matrixADataType, &matrixADataFormat, &matrixA_M, &matrixA_K));
    CHECK_STATUS(tensor2dfGet(matrixBDesc, &matrixBDataType, &matrixBDataFormat, &matrixB_K, &matrixB_N));
    CHECK_STATUS(tensor2dGet(matrixCDesc, &matrixCDataType, &matrixC_M, &matrixC_N));

    if (matrixADataType != matrixBDataType)
        CHECK_STATUS(NOT_MATCH);
    if (matrixADataType != matrixCDataType)
        if (matrixADataType != DT_I8 || matrixCDataType != DT_I32)
            CHECK_STATUS(NOT_MATCH);

    bool transposeA = false, transposeB = false;
    if (matrixADataFormat == DF_TRANSPOSE) {
        std::swap(matrixA_M, matrixA_K);
        transposeA = true;
    }
    if (matrixBDataFormat == DF_TRANSPOSE) {
        std::swap(matrixB_K, matrixB_N);
        transposeB = true;
    }
    if (matrixA_M != matrixC_M || matrixB_N != matrixC_N || matrixA_K != matrixB_K)
        CHECK_STATUS(NOT_MATCH);

    EE ret = NOT_SUPPORTED;
    if (arch == CPU_GENERAL) {
#ifdef _USE_GENERAL
        ret = mmm_general(matrixC_N, matrixC_M, matrixA_K, transposeA, transposeB, matrixADataType, matrixAData, matrixBData, matrixCData);
#endif
#ifdef _USE_NEON
    } else {
        TensorDesc tranDescB;
        U8 *dataB = (U8*)matrixBData;
        if (matrixBDataFormat != targetFormat4MatrixB(matrixBDataType)) {
            U32 K = matrixA_K;
            if (DT_I8 == matrixADataType) {
                K = pad_to_4_multiple(K);
            }
            dataB = ((U8*)tmp) + matrixA_M * K * bytesOf(matrixADataType);
            ret = matrix_matrix_multiply_transform_rhs(matrixBDesc, matrixBData, &tranDescB, dataB);
        }
        ret = mmm_arm(matrixC_N, matrixC_M, matrixA_K, matrixADataType, matrixAData, dataB, tmp, matrixCData, arch);
#endif
    }
    return ret;
}

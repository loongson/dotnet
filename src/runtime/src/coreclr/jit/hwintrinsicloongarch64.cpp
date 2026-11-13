// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "jitpch.h"
#include "hwintrinsic.h"

#ifdef FEATURE_HW_INTRINSICS

//------------------------------------------------------------------------
// lookupInstructionSet: Gets the InstructionSet for a given class name
//
// Arguments:
//    className -- The name of the class associated with the InstructionSet to lookup
//
// Return Value:
//    The InstructionSet associated with className
CORINFO_InstructionSet Compiler::lookupInstructionSet(const char* className)
{
    assert(className != nullptr);
    if (className[0] == 'B')
    {
        if (strcmp(className, "BH") == 0)
        {
            return InstructionSet_LAM_BH;
        }
    }
    else if (className[0] == 'C')
    {
        if (strcmp(className, "CAS") == 0)
        {
            return InstructionSet_LAM_CAS;
        }
    }
    else if (className[0] == 'L')
    {
        if (strcmp(className, "LoongArch64Base") == 0)
        {
            return InstructionSet_LoongArch64Base;
        }
        if (strcmp(className, "Lsx") == 0)
        {
            return InstructionSet_LSX;
        }
        if (strcmp(className, "Lasx") == 0)
        {
            return InstructionSet_LASX;
        }
    }
    else if (className[0] == 'V')
    {
        if (strncmp(className, "Vector128", 9) == 0)
        {
            return InstructionSet_Vector128;
        }
        else if (strncmp(className, "Vector256", 9) == 0)
        {
            return InstructionSet_Vector256;
        }
    }
    //TODO for LA-SIMD: add other features ?

    return InstructionSet_ILLEGAL;
}

//------------------------------------------------------------------------
// lookupIsa: Gets the InstructionSet for a given class name and enclsoing class name
//
// Arguments:
//    className -- The name of the class associated with the InstructionSet to lookup
//    enclosingClassName -- The name of the enclosing class or nullptr if one doesn't exist
//
// Return Value:
//    The InstructionSet associated with className and enclosingClassName
CORINFO_InstructionSet Compiler::lookupIsa(const char* className, const char* innerEnclosingClassName, const char* outerEnclosingClassName)
{
    assert(className != nullptr);

    // No nested class is the most common, so fast path it
    return lookupInstructionSet(className);

}

//------------------------------------------------------------------------
// lookupImmBounds: Gets the lower and upper bounds for the imm-value of a given NamedIntrinsic
//
// Arguments:
//    intrinsic -- NamedIntrinsic associated with the HWIntrinsic to lookup
//    simdType  -- vector size
//    baseType  -- base type of the Vector128/256<T>
//    lowerBound [OUT] - The lower incl. bound for a value of the intrinsic immediate operand
//    upperBound [OUT] - The upper incl. bound for a value of the intrinsic immediate operand
//
void HWIntrinsicInfo::lookupImmBounds(
    NamedIntrinsic intrinsic, int simdSize, var_types baseType,  int immNumber, int* lowerBound, int* upperBound)
{
    HWIntrinsicCategory category = HWIntrinsicInfo::lookupCategory(intrinsic);

    bool hasImmediateOperand = HasImmediateOperand(intrinsic);
    assert(hasImmediateOperand);

    assert(lowerBound != nullptr);
    assert(upperBound != nullptr);

    int immLowerBound = 0;
    int immUpperBound = 0;

    //TODO LA-SIMD: process all imm's bound here.   qqqqq

    assert(immLowerBound <= immUpperBound);

    *lowerBound = immLowerBound;
    *upperBound = immUpperBound;
}

//------------------------------------------------------------------------
// impNonConstFallback: generate alternate code when the imm-arg is not a compile-time constant
//
// Arguments:
//    intrinsic       -- intrinsic ID
//    simdType        -- Vector type
//    simdBaseJitType -- base JIT type of the Vector128/256<T>
//
// Return Value:
//     return the IR of semantic alternative on non-const imm-arg
//
GenTree* Compiler::impNonConstFallback(NamedIntrinsic intrinsic, var_types simdType, CorInfoType simdBaseJitType)
{
    return nullptr;
}

//------------------------------------------------------------------------
// impSpecialIntrinsic: Import a hardware intrinsic that requires special handling as a GT_HWINTRINSIC node if possible
//
// Arguments:
//    intrinsic       -- id of the intrinsic function.
//    clsHnd          -- class handle containing the intrinsic function.
//    method          -- method handle of the intrinsic function.
//    sig             -- signature of the intrinsic call.
//    simdBaseJitType -- generic argument of the intrinsic.
//    retType         -- return type of the intrinsic.
//
// Return Value:
//    The GT_HWINTRINSIC node, or nullptr if not a supported intrinsic
//
GenTree* Compiler::impSpecialIntrinsic(NamedIntrinsic        intrinsic,
                                       CORINFO_CLASS_HANDLE  clsHnd,
                                       CORINFO_METHOD_HANDLE method,
                                       CORINFO_SIG_INFO* sig R2RARG(CORINFO_CONST_LOOKUP* entryPoint),
                                       CorInfoType           simdBaseJitType,
                                       var_types             retType,
                                       unsigned              simdSize,
                                       bool                  mustExpand)
{
    //FIXME for LA-SIMD: should implement based on the LA64's ISA-feature,
    //                   which is quite different with arm64/x64.
    //should confirma
    //assert(!"unimplemented yet on LA");
    const HWIntrinsicCategory category = HWIntrinsicInfo::lookupCategory(intrinsic);
    const int                 numArgs  = sig->numArgs;

    bool isScalar = (category == HW_Category_Scalar);

    assert(numArgs >= 0);

    var_types simdBaseType = JitType2PreciseVarType(simdBaseJitType);
    assert(varTypeIsArithmetic(simdBaseType));

    GenTree* retNode = nullptr;
    GenTree* op1     = nullptr;
    GenTree* op2     = nullptr;
    GenTree* op3     = nullptr;
    GenTree* op4     = nullptr;

#ifdef DEBUG
    bool isValidScalarIntrinsic = false;
#endif

#if defined(FEATURE_READYTORUN)
    CORINFO_CONST_LOOKUP emptyEntryPoint;

    emptyEntryPoint.addr       = nullptr;
    emptyEntryPoint.accessType = IAT_VALUE;
#endif // FEATURE_READYTORUN

    bool isMinMaxIntrinsic = false;
    bool isMax             = false;
    bool isMagnitude       = false;
    bool isNative          = false;
    bool isNumber          = false;

    switch (intrinsic)
    {
        case NI_Vector128_Abs:
        case NI_Vector256_Abs:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdAbsNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_Addition:
        case NI_Vector256_op_Addition:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(GT_ADD, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_AddSaturate:
        case NI_Vector256_AddSaturate:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            if (varTypeIsFloating(simdBaseType))
            {
                retNode = gtNewSimdBinOpNode(GT_ADD, retType, op1, op2, simdBaseJitType, simdSize);
            }
            else
            {
                intrinsic = NI_LSX_AddSaturate;

                if (simdSize == 32)
                {
                    intrinsic = NI_LASX_AddSaturate;
                }

                retNode = gtNewSimdHWIntrinsicNode(retType, op1, op2, intrinsic, simdBaseJitType, simdSize);
            }
            break;
        }

        case NI_Vector128_AndNot:
        case NI_Vector256_AndNot:
        {
            assert(sig->numArgs == 2);

            // We don't want to support creating AND_NOT nodes prior to LIR
            // as it can break important optimizations. We'll produces this
            // in lowering instead so decompose into the individual operations
            // on import

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            op2     = gtFoldExpr(gtNewSimdUnOpNode(GT_NOT, retType, op2, simdBaseJitType, simdSize));
            retNode = gtNewSimdBinOpNode(GT_AND, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_LSX_OrNot:
        case NI_LASX_OrNot:
        {
            assert(sig->numArgs == 2);

            // We don't want to support creating OR_NOT nodes prior to LIR
            // as it can break important optimizations. We'll produces this
            // in lowering instead so decompose into the individual operations
            // on import

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            op2     = gtFoldExpr(gtNewSimdUnOpNode(GT_NOT, retType, op2, simdBaseJitType, simdSize));
            retNode = gtNewSimdBinOpNode(GT_OR, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector256_As:
        case NI_Vector256_AsByte:
        case NI_Vector256_AsDouble:
        case NI_Vector256_AsInt16:
        case NI_Vector256_AsInt32:
        case NI_Vector256_AsInt64:
        case NI_Vector256_AsNInt:
        case NI_Vector256_AsNUInt:
        case NI_Vector256_AsSByte:
        case NI_Vector256_AsSingle:
        case NI_Vector256_AsUInt16:
        case NI_Vector256_AsUInt32:
        case NI_Vector256_AsUInt64:
        case NI_Vector128_As:
        case NI_Vector128_AsByte:
        case NI_Vector128_AsDouble:
        case NI_Vector128_AsInt16:
        case NI_Vector128_AsInt32:
        case NI_Vector128_AsInt64:
        case NI_Vector128_AsNInt:
        case NI_Vector128_AsNUInt:
        case NI_Vector128_AsSByte:
        case NI_Vector128_AsSingle:
        case NI_Vector128_AsUInt16:
        case NI_Vector128_AsUInt32:
        case NI_Vector128_AsUInt64:
        case NI_Vector128_AsVector4:
        {
            assert(!sig->hasThis());
            assert(numArgs == 1);

            // We fold away the cast here, as it only exists to satisfy
            // the type system. It is safe to do this here since the retNode type
            // and the signature return type are both the same TYP_SIMD.

            retNode = impSIMDPopStack();
            SetOpLclRelatedToSIMDIntrinsic(retNode);
            assert(retNode->gtType == getSIMDTypeForSize(getSIMDTypeSizeInBytes(sig->retTypeSigClass)));
            break;
        }

        case NI_Vector128_AsVector2:
        {
            assert(sig->numArgs == 1);
            assert((simdSize == 16) && (simdBaseType == TYP_FLOAT));
            assert(retType == TYP_SIMD8);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdGetLowerNode(TYP_SIMD8, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_AsVector3:
        {
            assert(sig->numArgs == 1);
            assert((simdSize == 16) && (simdBaseType == TYP_FLOAT));
            assert(retType == TYP_SIMD12);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdHWIntrinsicNode(retType, op1, intrinsic, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_AsVector128:
        {
            assert(!sig->hasThis());
            assert(numArgs == 1);
            assert(retType == TYP_SIMD16);

            switch (getSIMDTypeForSize(simdSize))
            {
                case TYP_SIMD8:
                {
                    assert((simdSize == 8) && (simdBaseType == TYP_FLOAT));

                    op1 = impSIMDPopStack();

                    if (op1->IsCnsVec())
                    {
                        GenTreeVecCon* vecCon = op1->AsVecCon();
                        vecCon->gtType        = TYP_SIMD16;

                        vecCon->gtSimdVal.f32[2] = 0.0f;
                        vecCon->gtSimdVal.f32[3] = 0.0f;

                        return vecCon;
                    }

                    op1 = gtNewSimdHWIntrinsicNode(retType, op1, NI_Vector128_AsVector128Unsafe, simdBaseJitType, 8);

                    GenTree* idx  = gtNewIconNode(2, TYP_INT);
                    GenTree* zero = gtNewZeroConNode(TYP_FLOAT);
                    op1           = gtNewSimdWithElementNode(retType, op1, idx, zero, simdBaseJitType, 16);

                    idx     = gtNewIconNode(3, TYP_INT);
                    zero    = gtNewZeroConNode(TYP_FLOAT);
                    retNode = gtNewSimdWithElementNode(retType, op1, idx, zero, simdBaseJitType, 16);

                    break;
                }

                case TYP_SIMD12:
                {
                    assert((simdSize == 12) && (simdBaseType == TYP_FLOAT));

                    op1 = impSIMDPopStack();

                    if (op1->IsCnsVec())
                    {
                        GenTreeVecCon* vecCon = op1->AsVecCon();
                        vecCon->gtType        = TYP_SIMD16;

                        vecCon->gtSimdVal.f32[3] = 0.0f;
                        return vecCon;
                    }

                    op1 = gtNewSimdHWIntrinsicNode(retType, op1, NI_Vector128_AsVector128Unsafe, simdBaseJitType, 12);

                    GenTree* idx  = gtNewIconNode(3, TYP_INT);
                    GenTree* zero = gtNewZeroConNode(TYP_FLOAT);
                    retNode       = gtNewSimdWithElementNode(retType, op1, idx, zero, simdBaseJitType, 16);
                    break;
                }

                case TYP_SIMD16:
                {
                    // We fold away the cast here, as it only exists to satisfy
                    // the type system. It is safe to do this here since the retNode type
                    // and the signature return type are both the same TYP_SIMD.

                    retNode = impSIMDPopStack();
                    SetOpLclRelatedToSIMDIntrinsic(retNode);
                    assert(retNode->gtType == getSIMDTypeForSize(getSIMDTypeSizeInBytes(sig->retTypeSigClass)));
                    break;
                }

                case TYP_SIMD32:
                {
                    // Vector<T> is TYP_SIMD32, so we should treat this as a call to Vector256.GetLower
                    return impSpecialIntrinsic(NI_Vector256_GetLower, clsHnd, method, sig R2RARG(&emptyEntryPoint),
                                               simdBaseJitType, retType, simdSize, mustExpand);
                }

                default:
                {
                    unreached();
                }
            }

            break;
        }

        case NI_Vector128_AsVector128Unsafe:
        {
            assert(sig->numArgs == 1);
            assert(retType == TYP_SIMD16);
            assert(simdBaseJitType == CORINFO_TYPE_FLOAT);
            assert((simdSize == 8) || (simdSize == 12));

            op1     = impSIMDPopStack();
            retNode = gtNewSimdHWIntrinsicNode(retType, op1, NI_Vector128_AsVector128Unsafe, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_AsVector:
        case NI_Vector256_AsVector:
        case NI_Vector256_AsVector256:
        {
            assert(sig->numArgs == 1);
            uint32_t vectorTByteLength = getVectorTByteLength();

            if (vectorTByteLength == simdSize)
            {
                // We fold away the cast here, as it only exists to satisfy
                // the type system. It is safe to do this here since the retNode type
                // and the signature return type are both the same TYP_SIMD.

                retNode = impSIMDPopStack();
                SetOpLclRelatedToSIMDIntrinsic(retNode);
                assert(retNode->gtType == getSIMDTypeForSize(getSIMDTypeSizeInBytes(sig->retTypeSigClass)));

                break;
            }

            // Vector<T> is a different size than the source/target SIMD type, so we should
            // treat this as a call to the appropriate narrowing or widening intrinsic.

            NamedIntrinsic convertIntrinsic = NI_Illegal;

            switch (vectorTByteLength)
            {
                case LSX_REGSIZE_BYTES:
                {
                    switch (intrinsic)
                    {
                        case NI_Vector256_AsVector:
                            convertIntrinsic = NI_Vector256_GetLower;
                            break;
                        case NI_Vector256_AsVector256:
                            convertIntrinsic = NI_Vector128_ToVector256;
                            break;
                        default:
                            unreached();
                    }
                    break;
                }

                case LASX_REGSIZE_BYTES:
                {
                    switch (intrinsic)
                    {
                        case NI_Vector128_AsVector:
                            convertIntrinsic = NI_Vector128_ToVector256;
                            break;
                        default:
                            unreached();
                    }
                    break;
                }


                default:
                {
                    unreached();
                }
            }

            unsigned convertSize = simdSize;
            bool     sizeFound   = HWIntrinsicInfo::tryLookupSimdSize(convertIntrinsic, &convertSize);
            assert(sizeFound);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdHWIntrinsicNode(retType, op1, convertIntrinsic, simdBaseJitType, convertSize);

            break;

        }

        case NI_Vector128_op_BitwiseAnd:
        case NI_Vector256_op_BitwiseAnd:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(GT_AND, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_BitwiseOr:
        case NI_Vector256_op_BitwiseOr:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(GT_OR, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Ceiling:
        case NI_Vector256_Ceiling:
        {
            assert(sig->numArgs == 1);

            if (!varTypeIsFloating(simdBaseType))
            {
                retNode = impSIMDPopStack();
                break;
            }

            op1     = impSIMDPopStack();
            retNode = gtNewSimdCeilNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ConditionalSelect:
        case NI_Vector256_ConditionalSelect:
        {
            assert(sig->numArgs == 3);

            op3 = impSIMDPopStack();
            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCndSelNode(retType, op1, op2, op3, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ConvertToDouble:
        case NI_Vector256_ConvertToDouble:
        {
            assert(sig->numArgs == 1);
            assert((simdBaseType == TYP_LONG) || (simdBaseType == TYP_ULONG));
            intrinsic = (simdSize == 32) ? NI_LASX_ConvertToDouble : NI_LSX_ConvertToDouble;

            op1     = impSIMDPopStack();
            retNode = gtNewSimdHWIntrinsicNode(retType, op1, intrinsic, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ConvertToInt32Native:
        case NI_Vector256_ConvertToInt32Native:
        {
            if (BlockNonDeterministicIntrinsics(mustExpand))
            {
                break;
            }
            FALLTHROUGH;
        }

        case NI_Vector128_ConvertToInt32:
        case NI_Vector256_ConvertToInt32:
        {
            assert(sig->numArgs == 1);
            assert(simdBaseType == TYP_FLOAT);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdCvtNativeNode(retType, op1, CORINFO_TYPE_INT, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ConvertToInt64Native:
        case NI_Vector256_ConvertToInt64Native:
        {
            if (BlockNonDeterministicIntrinsics(mustExpand))
            {
                break;
            }
            FALLTHROUGH;
        }

        case NI_Vector128_ConvertToInt64:
        case NI_Vector256_ConvertToInt64:
        {
            assert(sig->numArgs == 1);
            assert(simdBaseType == TYP_DOUBLE);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdCvtNativeNode(retType, op1, CORINFO_TYPE_LONG, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ConvertToSingle:
        case NI_Vector256_ConvertToSingle:
        {
            assert(sig->numArgs == 1);
            assert((simdBaseType == TYP_INT) || (simdBaseType == TYP_UINT));
            intrinsic = (simdSize == 32) ? NI_LASX_ConvertToSingle : NI_LSX_ConvertToSingle;

            op1     = impSIMDPopStack();
            retNode = gtNewSimdHWIntrinsicNode(retType, op1, intrinsic, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ConvertToUInt32Native:
        case NI_Vector256_ConvertToUInt32Native:
        {
            if (BlockNonDeterministicIntrinsics(mustExpand))
            {
                break;
            }
            FALLTHROUGH;
        }

        case NI_Vector128_ConvertToUInt32:
        case NI_Vector256_ConvertToUInt32:
        {
            assert(sig->numArgs == 1);
            assert(simdBaseType == TYP_FLOAT);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdCvtNativeNode(retType, op1, CORINFO_TYPE_UINT, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ConvertToUInt64Native:
        case NI_Vector256_ConvertToUInt64Native:
        {
            if (BlockNonDeterministicIntrinsics(mustExpand))
            {
                break;
            }
            FALLTHROUGH;
        }

        case NI_Vector128_ConvertToUInt64:
        case NI_Vector256_ConvertToUInt64:
        {
            assert(sig->numArgs == 1);
            assert(simdBaseType == TYP_DOUBLE);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdCvtNativeNode(retType, op1, CORINFO_TYPE_ULONG, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Create:
        case NI_Vector256_Create:
        {
            if (sig->numArgs == 1)
            {
                op1     = impPopStack().val;
                retNode = gtNewSimdCreateBroadcastNode(retType, op1, simdBaseJitType, simdSize);
                break;
            }

            uint32_t simdLength = getSIMDVectorLength(simdSize, simdBaseType);
            assert(sig->numArgs == simdLength);

            bool isConstant = true;

            if (varTypeIsFloating(simdBaseType))
            {
                for (uint32_t index = 0; index < sig->numArgs; index++)
                {
                    GenTree* arg = impStackTop(index).val;

                    if (!arg->IsCnsFltOrDbl())
                    {
                        isConstant = false;
                        break;
                    }
                }
            }
            else
            {
                assert(varTypeIsIntegral(simdBaseType));

                for (uint32_t index = 0; index < sig->numArgs; index++)
                {
                    GenTree* arg = impStackTop(index).val;

                    if (!arg->IsIntegralConst())
                    {
                        isConstant = false;
                        break;
                    }
                }
            }

            if (isConstant)
            {
                // Some of the below code assumes 16 or 32 byte SIMD types
                assert((simdSize == 16) || (simdSize == 32));

                GenTreeVecCon* vecCon = gtNewVconNode(retType);

                switch (simdBaseType)
                {
                    case TYP_BYTE:
                    case TYP_UBYTE:
                    {
                        uint8_t cnsVal = 0;

                        for (uint32_t index = 0; index < sig->numArgs; index++)
                        {
                            cnsVal = static_cast<uint8_t>(impPopStack().val->AsIntConCommon()->IntegralValue());
                            vecCon->gtSimdVal.u8[simdLength - 1 - index] = cnsVal;
                        }
                        break;
                    }

                    case TYP_SHORT:
                    case TYP_USHORT:
                    {
                        uint16_t cnsVal = 0;

                        for (uint32_t index = 0; index < sig->numArgs; index++)
                        {
                            cnsVal = static_cast<uint16_t>(impPopStack().val->AsIntConCommon()->IntegralValue());
                            vecCon->gtSimdVal.u16[simdLength - 1 - index] = cnsVal;
                        }
                        break;
                    }

                    case TYP_INT:
                    case TYP_UINT:
                    {
                        uint32_t cnsVal = 0;

                        for (uint32_t index = 0; index < sig->numArgs; index++)
                        {
                            cnsVal = static_cast<uint32_t>(impPopStack().val->AsIntConCommon()->IntegralValue());
                            vecCon->gtSimdVal.u32[simdLength - 1 - index] = cnsVal;
                        }
                        break;
                    }

                    case TYP_LONG:
                    case TYP_ULONG:
                    {
                        uint64_t cnsVal = 0;

                        for (uint32_t index = 0; index < sig->numArgs; index++)
                        {
                            cnsVal = static_cast<uint64_t>(impPopStack().val->AsIntConCommon()->IntegralValue());
                            vecCon->gtSimdVal.u64[simdLength - 1 - index] = cnsVal;
                        }
                        break;
                    }

                    case TYP_FLOAT:
                    {
                        float cnsVal = 0;

                        for (uint32_t index = 0; index < sig->numArgs; index++)
                        {
                            cnsVal = static_cast<float>(impPopStack().val->AsDblCon()->DconValue());
                            vecCon->gtSimdVal.f32[simdLength - 1 - index] = cnsVal;
                        }
                        break;
                    }

                    case TYP_DOUBLE:
                    {
                        double cnsVal = 0;

                        for (uint32_t index = 0; index < sig->numArgs; index++)
                        {
                            cnsVal = static_cast<double>(impPopStack().val->AsDblCon()->DconValue());
                            vecCon->gtSimdVal.f64[simdLength - 1 - index] = cnsVal;
                        }
                        break;
                    }

                    default:
                    {
                        unreached();
                    }
                }

                retNode = vecCon;
                break;
            }

            IntrinsicNodeBuilder nodeBuilder(getAllocator(CMK_ASTNode), sig->numArgs);

            // TODO-CQ: We don't handle contiguous args for anything except TYP_FLOAT today

            GenTree* prevArg           = nullptr;
            bool     areArgsContiguous = (simdBaseType == TYP_FLOAT);

            for (int i = sig->numArgs - 1; i >= 0; i--)
            {
                GenTree* arg = impPopStack().val;

                if (areArgsContiguous)
                {
                    if (prevArg != nullptr)
                    {
                        // Recall that we are popping the args off the stack in reverse order.
                        areArgsContiguous = areArgumentsContiguous(arg, prevArg);
                    }

                    prevArg = arg;
                }

                nodeBuilder.AddOperand(i, arg);
            }

            if (areArgsContiguous)
            {
                op1                 = nodeBuilder.GetOperand(0);
                GenTree* op1Address = CreateAddressNodeForSimdHWIntrinsicCreate(op1, simdBaseType, simdSize);
                retNode             = gtNewIndir(retType, op1Address);
            }
            else
            {
                retNode =
                    gtNewSimdHWIntrinsicNode(retType, std::move(nodeBuilder), intrinsic, simdBaseJitType, simdSize);
            }
            break;
        }

        case NI_Vector128_CreateScalar:
        case NI_Vector256_CreateScalar:
        {
            assert(sig->numArgs == 1);

            op1     = impPopStack().val;
            retNode = gtNewSimdCreateScalarNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_CreateSequence:
        case NI_Vector256_CreateSequence:
        {
            assert(sig->numArgs == 2);

            if (varTypeIsLong(simdBaseType) && !impStackTop(0).val->OperIsConst())
            {
                // TODO-LOONGARCH64-CQ: We should support long/ulong multiplication.
                break;
            }

            impSpillSideEffect(true, stackState.esStackDepth -
                                         2 DEBUGARG("Spilling op1 side effects for SimdAsHWIntrinsic"));

            op2 = impPopStack().val;
            op1 = impPopStack().val;

            retNode = gtNewSimdCreateSequenceNode(retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_CreateScalarUnsafe:
        case NI_Vector256_CreateScalarUnsafe:
        {
            assert(sig->numArgs == 1);

            op1     = impPopStack().val;
            retNode = gtNewSimdCreateScalarUnsafeNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_Division:
        case NI_Vector256_op_Division:
        {
            assert(sig->numArgs == 2);

            if (!varTypeIsFloating(simdBaseType))
            {
                // We can't trivially handle division for integral types using SIMD
                break;
            }

            CORINFO_ARG_LIST_HANDLE arg1     = sig->args;
            CORINFO_ARG_LIST_HANDLE arg2     = info.compCompHnd->getArgNext(arg1);
            var_types               argType  = TYP_UNKNOWN;
            CORINFO_CLASS_HANDLE    argClass = NO_CLASS_HANDLE;

            argType = JITtype2varType(strip(info.compCompHnd->getArgType(sig, arg2, &argClass)));
            op2     = getArgForHWIntrinsic(argType, argClass);

            argType = JITtype2varType(strip(info.compCompHnd->getArgType(sig, arg1, &argClass)));
            op1     = getArgForHWIntrinsic(argType, argClass);

            retNode = gtNewSimdBinOpNode(GT_DIV, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Dot:
        case NI_Vector256_Dot:
        {
            assert(sig->numArgs == 2);

            if (!varTypeIsLong(simdBaseType))
            {
                var_types simdType = getSIMDTypeForSize(simdSize);

                op2 = impSIMDPopStack();
                op1 = impSIMDPopStack();

                retNode = gtNewSimdDotProdNode(simdType, op1, op2, simdBaseJitType, simdSize);
                retNode = gtNewSimdGetElementNode(retType, retNode, gtNewIconNode(0), simdBaseJitType, simdSize);
            }
            break;
        }

        case NI_Vector128_Equals:
        case NI_Vector256_Equals:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpNode(GT_EQ, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_Equality:
        case NI_Vector256_op_Equality:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAllNode(GT_EQ, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_EqualsAny:
        case NI_Vector256_EqualsAny:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAnyNode(GT_EQ, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ExtractMostSignificantBits:
        case NI_Vector256_ExtractMostSignificantBits:
        {
            assert((simdSize == 16) || (simdSize == 32));
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdHWIntrinsicNode(retType, op1, intrinsic, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Floor:
        case NI_Vector256_Floor:
        {
            assert(sig->numArgs == 1);

            if (!varTypeIsFloating(simdBaseType))
            {
                retNode = impSIMDPopStack();
                break;
            }

            op1     = impSIMDPopStack();
            retNode = gtNewSimdFloorNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_FusedMultiplyAdd:
        case NI_Vector256_FusedMultiplyAdd:
        {
            assert(sig->numArgs == 3);
            assert(varTypeIsFloating(simdBaseType));

            impSpillSideEffect(true, stackState.esStackDepth -
                                         3 DEBUGARG("Spilling op1 side effects for FusedMultiplyAdd"));

            impSpillSideEffect(true, stackState.esStackDepth -
                                         2 DEBUGARG("Spilling op2 side effects for FusedMultiplyAdd"));

            op3 = impSIMDPopStack();
            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdFmaNode(retType, op1, op2, op3, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_get_AllBitsSet:
        case NI_Vector256_get_AllBitsSet:
        {
            assert(sig->numArgs == 0);
            retNode = gtNewAllBitsSetConNode(retType);
            break;
        }

        case NI_Vector128_get_Indices:
        case NI_Vector256_get_Indices:
        {
            assert(sig->numArgs == 0);
            retNode = gtNewSimdGetIndicesNode(retType, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_get_One:
        case NI_Vector256_get_One:
        {
            assert(sig->numArgs == 0);
            retNode = gtNewOneConNode(retType, simdBaseType);
            break;
        }

        case NI_Vector128_get_Zero:
        case NI_Vector256_get_Zero:
        {
            assert(sig->numArgs == 0);
            retNode = gtNewZeroConNode(retType);
            break;
        }

        case NI_Vector128_GetElement:
        case NI_Vector256_GetElement:
        {
            assert(!sig->hasThis());
            assert(numArgs == 2);

            op2 = impPopStack().val;
            op1 = impSIMDPopStack();

            retNode = gtNewSimdGetElementNode(retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GetLower:
        case NI_Vector256_GetLower:
        {
            assert(sig->numArgs == 1);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdGetLowerNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GetUpper:
        case NI_Vector256_GetUpper:
        {
            assert(sig->numArgs == 1);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdGetUpperNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GreaterThan:
        case NI_Vector256_GreaterThan:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpNode(GT_GT, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GreaterThanAll:
        case NI_Vector256_GreaterThanAll:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAllNode(GT_GT, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GreaterThanAny:
        case NI_Vector256_GreaterThanAny:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAnyNode(GT_GT, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GreaterThanOrEqual:
        case NI_Vector256_GreaterThanOrEqual:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpNode(GT_GE, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GreaterThanOrEqualAll:
        case NI_Vector256_GreaterThanOrEqualAll:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAllNode(GT_GE, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_GreaterThanOrEqualAny:
        case NI_Vector256_GreaterThanOrEqualAny:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAnyNode(GT_GE, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsEvenInteger:
        case NI_Vector256_IsEvenInteger:
        {
            assert(sig->numArgs == 1);

            if (varTypeIsFloating(simdBaseType))
            {
                // The code for handling floating-point is decently complex but also expected
                // to be rare, so we fallback to the managed implementation, which is accelerated
                break;
            }

            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsEvenIntegerNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsFinite:
        case NI_Vector256_IsFinite:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsFiniteNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsInfinity:
        case NI_Vector256_IsInfinity:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsInfinityNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsInteger:
        case NI_Vector256_IsInteger:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsIntegerNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsNaN:
        case NI_Vector256_IsNaN:
        {
            assert(sig->numArgs == 1);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsNaNNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsNegative:
        case NI_Vector256_IsNegative:
        {
            assert(sig->numArgs == 1);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsNegativeNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsNegativeInfinity:
        case NI_Vector256_IsNegativeInfinity:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsNegativeInfinityNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsNormal:
        case NI_Vector256_IsNormal:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsNormalNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsOddInteger:
        case NI_Vector256_IsOddInteger:
        {
            assert(sig->numArgs == 1);

            if (varTypeIsFloating(simdBaseType))
            {
                // The code for handling floating-point is decently complex but also expected
                // to be rare, so we fallback to the managed implementation, which is accelerated
                break;
            }

            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsOddIntegerNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsPositive:
        case NI_Vector256_IsPositive:
        {
            assert(sig->numArgs == 1);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsPositiveNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsPositiveInfinity:
        case NI_Vector256_IsPositiveInfinity:
        {
            assert(sig->numArgs == 1);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsPositiveInfinityNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsSubnormal:
        case NI_Vector256_IsSubnormal:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsSubnormalNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_IsZero:
        case NI_Vector256_IsZero:
        {
            assert(sig->numArgs == 1);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdIsZeroNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LessThan:
        case NI_Vector256_LessThan:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpNode(GT_LT, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LessThanAll:
        case NI_Vector256_LessThanAll:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAllNode(GT_LT, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LessThanAny:
        case NI_Vector256_LessThanAny:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAnyNode(GT_LT, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LessThanOrEqual:
        case NI_Vector256_LessThanOrEqual:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpNode(GT_LE, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LessThanOrEqualAll:
        case NI_Vector256_LessThanOrEqualAll:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAllNode(GT_LE, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LessThanOrEqualAny:
        case NI_Vector256_LessThanOrEqualAny:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAnyNode(GT_LE, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_LSX_LoadVector128:
        case NI_LASX_LoadVector128:
        case NI_Vector128_LoadUnsafe:
        case NI_Vector256_LoadUnsafe:
        {
            if (sig->numArgs == 2)
            {
                op2 = impPopStack().val;
            }
            else
            {
                assert(sig->numArgs == 1);
            }

            op1 = impPopStack().val;

            if (op1->OperIs(GT_CAST) && op1->gtGetOp1()->TypeIs(TYP_BYREF))
            {
                // If what we have is a BYREF, that's what we really want, so throw away the cast.
                op1 = op1->gtGetOp1();
            }

            if (sig->numArgs == 2)
            {
                op3 = gtNewIconNode(genTypeSize(simdBaseType), op2->TypeGet());
                op2 = gtNewOperNode(GT_MUL, op2->TypeGet(), op2, op3);
                op1 = gtNewOperNode(GT_ADD, op1->TypeGet(), op1, op2);
            }

            retNode = gtNewSimdLoadNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LoadAligned:
        case NI_Vector256_LoadAligned:
        {
            assert(sig->numArgs == 1);

            if (opts.OptimizationDisabled())
            {
                //--should confirm
                // LOONGARCH64 doesn't have aligned loads, but aligned loads are only validated to be
                // aligned when optimizations are disable, so only skip the intrinsic handling
                // if optimizations are enabled
                break;
            }

            op1 = impPopStack().val;

            if (op1->OperIs(GT_CAST) && op1->gtGetOp1()->TypeIs(TYP_BYREF))
            {
                // If what we have is a BYREF, that's what we really want, so throw away the cast.
                op1 = op1->gtGetOp1();
            }

            retNode = gtNewSimdLoadAlignedNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_LoadAlignedNonTemporal:
        case NI_Vector256_LoadAlignedNonTemporal:
        {
            assert(sig->numArgs == 1);

            if (opts.OptimizationDisabled())
            {
                //--should confirm
                // LOONGARCH64 doesn't have aligned loads, but aligned loads are only validated to be
                // aligned when optimizations are disable, so only skip the intrinsic handling
                // if optimizations are enabled
                break;
            }

            op1 = impPopStack().val;

            if (op1->OperIs(GT_CAST) && op1->gtGetOp1()->TypeIs(TYP_BYREF))
            {
                // If what we have is a BYREF, that's what we really want, so throw away the cast.
                op1 = op1->gtGetOp1();
            }

            retNode = gtNewSimdLoadNonTemporalNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Max:
        case NI_Vector256_Max:
        {
            isMinMaxIntrinsic = true;
            isMax             = true;
            break;
        }

        case NI_Vector128_MaxMagnitude:
        case NI_Vector256_MaxMagnitude:
        {
            isMinMaxIntrinsic = true;
            isMax             = true;
            isMagnitude       = true;
            break;
        }

        case NI_Vector128_MaxMagnitudeNumber:
        case NI_Vector256_MaxMagnitudeNumber:
        {
            isMinMaxIntrinsic = true;
            isMax             = true;
            isMagnitude       = true;
            isNumber          = true;
            break;
        }

        case NI_Vector128_MaxNative:
        case NI_Vector256_MaxNative:
        {
            isMinMaxIntrinsic = true;
            isMax             = true;
            isNative          = true;
            break;
        }

        case NI_Vector128_MaxNumber:
        case NI_Vector256_MaxNumber:
        {
            isMinMaxIntrinsic = true;
            isMax             = true;
            isNumber          = true;
            break;
        }

        case NI_Vector128_Min:
        case NI_Vector256_Min:
        {
            isMinMaxIntrinsic = true;
            break;
        }

        case NI_Vector128_MinMagnitude:
        case NI_Vector256_MinMagnitude:
        {
            isMinMaxIntrinsic = true;
            isMagnitude       = true;
            break;
        }

        case NI_Vector128_MinMagnitudeNumber:
        case NI_Vector256_MinMagnitudeNumber:
        {
            isMinMaxIntrinsic = true;
            isMagnitude       = true;
            isNumber          = true;
            break;
        }

        case NI_Vector128_MinNative:
        case NI_Vector256_MinNative:
        {
            isMinMaxIntrinsic = true;
            isNative          = true;
            break;
        }

        case NI_Vector128_MinNumber:
        case NI_Vector256_MinNumber:
        {
            isMinMaxIntrinsic = true;
            isNumber          = true;
            break;
        }

        case NI_Vector128_op_Multiply:
        case NI_Vector256_op_Multiply:
        {
            assert(sig->numArgs == 2);

            CORINFO_ARG_LIST_HANDLE arg1     = sig->args;
            CORINFO_ARG_LIST_HANDLE arg2     = info.compCompHnd->getArgNext(arg1);
            var_types               argType  = TYP_UNKNOWN;
            CORINFO_CLASS_HANDLE    argClass = NO_CLASS_HANDLE;

            argType = JITtype2varType(strip(info.compCompHnd->getArgType(sig, arg2, &argClass)));
            op2     = getArgForHWIntrinsic(argType, argClass);

            argType = JITtype2varType(strip(info.compCompHnd->getArgType(sig, arg1, &argClass)));
            op1     = getArgForHWIntrinsic(argType, argClass);

            retNode = gtNewSimdBinOpNode(GT_MUL, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_MultiplyAddEstimate:
        case NI_Vector256_MultiplyAddEstimate:
        {
            assert(sig->numArgs == 3);

            if (BlockNonDeterministicIntrinsics(mustExpand))
            {
                break;
            }

            if (varTypeIsFloating(simdBaseType))
            {
                impSpillSideEffect(true, stackState.esStackDepth -
                                             3 DEBUGARG("Spilling op1 side effects for MultiplyAddEstimate"));

                impSpillSideEffect(true, stackState.esStackDepth -
                                             2 DEBUGARG("Spilling op2 side effects for MultiplyAddEstimate"));
            }

            op3 = impSIMDPopStack();
            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            if (varTypeIsFloating(simdBaseType))
            {
                retNode = gtNewSimdFmaNode(retType, op1, op2, op3, simdBaseJitType, simdSize);
            }
            else
            {
                GenTree* mulNode = gtNewSimdBinOpNode(GT_MUL, retType, op1, op2, simdBaseJitType, simdSize);
                retNode          = gtNewSimdBinOpNode(GT_ADD, retType, mulNode, op3, simdBaseJitType, simdSize);
            }
            break;
        }

        case NI_Vector128_Narrow:
        case NI_Vector256_Narrow:
        {
            //should confirm
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdNarrowNode(retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_NarrowWithSaturation:
        case NI_Vector256_NarrowWithSaturation:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            if (varTypeIsFloating(simdBaseType))
            {
                retNode = gtNewSimdNarrowNode(retType, op1, op2, simdBaseJitType, simdSize);
            }
            else
            {
                intrinsic = varTypeIsUnsigned(simdBaseType) ? NI_LSX_ShiftRightLogicalNarrowingSaturateLower : NI_LSX_ShiftRightArithmeticNarrowingSaturateLower;
                if (simdSize == 32)
                {
                    intrinsic = varTypeIsUnsigned(simdBaseType) ? NI_LASX_ShiftRightLogicalNarrowingSaturateLower : NI_LASX_ShiftRightArithmeticNarrowingSaturateLower;
                }

                GenTree* idx = gtNewIconNode(0x0);
                GenTree* tmp1 = gtNewSimdHWIntrinsicNode(retType, op2, op1, idx, intrinsic, simdBaseJitType, simdSize);

                if (simdSize == 32)
                {
                    GenTree* icon = gtNewIconNode(0xD8);
                    return gtNewSimdHWIntrinsicNode(TYP_SIMD32, tmp1, icon, NI_LASX_Permute, CORINFO_TYPE_LONG, simdSize);
                }
                return tmp1;
            }
            break;
        }

        case NI_Vector128_op_UnaryNegation:
        case NI_Vector256_op_UnaryNegation:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdUnOpNode(GT_NEG, retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_OnesComplement:
        case NI_Vector256_op_OnesComplement:
        {
            assert(sig->numArgs == 1);
            op1     = impSIMDPopStack();
            retNode = gtNewSimdUnOpNode(GT_NOT, retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_Inequality:
        case NI_Vector256_op_Inequality:
        {
            assert(sig->numArgs == 2);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdCmpOpAnyNode(GT_NE, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_UnaryPlus:
        case NI_Vector256_op_UnaryPlus:
        {
            assert(sig->numArgs == 1);
            retNode = impSIMDPopStack();
            break;
        }

        case NI_Vector128_op_Subtraction:
        case NI_Vector256_op_Subtraction:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(GT_SUB, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_LeftShift:
        case NI_Vector256_op_LeftShift:
        {
            assert(sig->numArgs == 2);

            op2 = impPopStack().val;
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(GT_LSH, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_RightShift:
        case NI_Vector256_op_RightShift:
        {
            assert(sig->numArgs == 2);
            genTreeOps op = varTypeIsUnsigned(simdBaseType) ? GT_RSZ : GT_RSH;

            op2 = impPopStack().val;
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(op, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_UnsignedRightShift:
        case NI_Vector256_op_UnsignedRightShift:
        {
            assert(sig->numArgs == 2);

            op2 = impPopStack().val;
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(GT_RSZ, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Round:
        case NI_Vector256_Round:
        {
            if (sig->numArgs != 1)
            {
                break;
            }

            if (!varTypeIsFloating(simdBaseType))
            {
                retNode = impSIMDPopStack();
                break;
            }

            op1     = impSIMDPopStack();
            retNode = gtNewSimdRoundNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_ShiftLeft:
        case NI_Vector256_ShiftLeft:
        {
            assert(sig->numArgs == 2);

            if (!varTypeIsSIMD(impStackTop(0).val))
            {
                // We just want the inlining profitability boost for the helper intrinsics/
                // that have operator alternatives like `simd << int`
                break;
            }

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            intrinsic = NI_LSX_ShiftLeftLogical;
            if (simdSize == 32)
            {
                intrinsic = NI_LASX_ShiftLeftLogical;
            }

            retNode = gtNewSimdHWIntrinsicNode(retType, op1, op2, intrinsic, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Shuffle:
        case NI_Vector256_Shuffle:
        case NI_Vector128_ShuffleNative:
        case NI_Vector256_ShuffleNative:
        case NI_Vector128_ShuffleNativeFallback:
        case NI_Vector256_ShuffleNativeFallback:
        {
            assert((sig->numArgs == 2) || (sig->numArgs == 3));

            // The Native variants are non-deterministic on loongarch64
            bool isShuffleNative = (intrinsic != NI_Vector128_Shuffle) && (intrinsic != NI_Vector256_Shuffle);
            if (isShuffleNative && (genTypeSize(simdBaseType) > 1) && BlockNonDeterministicIntrinsics(mustExpand))
            {
                break;
            }

            GenTree* indices = impStackTop(0).val;

            // Check if the required intrinsics to emit are available.
            bool canBecomeValidForShuffle = false;
            if (!IsValidForShuffle(indices, simdSize, simdBaseType, &canBecomeValidForShuffle, isShuffleNative))
            {
                // All cases on loongarch64 are either valid or invalid, they cannot become valid later
                assert(!canBecomeValidForShuffle);
                break;
            }

            if (!indices->IsCnsVec())
            {
                assert(sig->numArgs == 2);

                if (opts.OptimizationEnabled())
                {
                    // Only enable late stage rewriting if optimizations are enabled
                    // as we won't otherwise encounter a constant at the later point
                    op2 = impSIMDPopStack();
                    op1 = impSIMDPopStack();

                    retNode = gtNewSimdHWIntrinsicNode(retType, op1, op2, intrinsic, simdBaseJitType, simdSize);

                    retNode->AsHWIntrinsic()->SetMethodHandle(this, method R2RARG(*entryPoint));
                    break;
                }

            }

            if (sig->numArgs == 2)
            {
                op2 = impSIMDPopStack();
                op1 = impSIMDPopStack();

                retNode = gtNewSimdShuffleNode(retType, op1, op2, simdBaseJitType, simdSize, isShuffleNative);
            }
            break;
        }

        case NI_Vector128_Sqrt:
        case NI_Vector256_Sqrt:
        {
            assert(sig->numArgs == 1);

            if (varTypeIsFloating(simdBaseType))
            {
                op1     = impSIMDPopStack();
                retNode = gtNewSimdSqrtNode(retType, op1, simdBaseJitType, simdSize);
            }
            break;
        }

        case NI_LSX_Store:
        case NI_LASX_Store:
        {
            //should confirm
            CORINFO_ARG_LIST_HANDLE arg1     = sig->args;
            CORINFO_ARG_LIST_HANDLE arg2     = info.compCompHnd->getArgNext(arg1);
            var_types               argType  = TYP_UNKNOWN;
            CORINFO_CLASS_HANDLE    argClass = NO_CLASS_HANDLE;
            argType = JITtype2varType(strip(info.compCompHnd->getArgType(sig, arg2, &argClass)));
            op2     = impPopStack().val;

            if (op2->TypeGet() == TYP_STRUCT)
            {
                info.compNeedsConsecutiveRegisters = true;
                unsigned fieldCount                = info.compCompHnd->getClassNumInstanceFields(argClass);

                if (!op2->OperIs(GT_LCL_VAR))
                {
                    unsigned tmp = lvaGrabTemp(true DEBUGARG("StoreVectorN"));

                    impStoreToTemp(tmp, op2, CHECK_SPILL_NONE);
                    op2 = gtNewLclvNode(tmp, argType);
                }
                op2     = gtConvertTableOpToFieldList(op2, fieldCount);
                argType = JITtype2varType(strip(info.compCompHnd->getArgType(sig, arg1, &argClass)));
                op1     = getArgForHWIntrinsic(argType, argClass);

                if (op1->OperIs(GT_CAST))
                {
                    // Although the API specifies a pointer, if what we have is a BYREF, that's what
                    // we really want, so throw away the cast.
                    if (op1->gtGetOp1()->TypeGet() == TYP_BYREF)
                    {
                        op1 = op1->gtGetOp1();
                    }
                }

                retNode = gtNewSimdHWIntrinsicNode(retType, op1, op2, intrinsic, simdBaseJitType, simdSize);
            }
            else
            {
                if (op2->TypeGet() == TYP_SIMD16)
                {
                    simdSize = 16;
                }

                var_types simdType = getSIMDTypeForSize(simdSize);
                op1                = impPopStack().val;

                if (op1->OperIs(GT_CAST) && op1->gtGetOp1()->TypeIs(TYP_BYREF))
                {
                    // If what we have is a BYREF, that's what we really want, so throw away the cast.
                    op1 = op1->gtGetOp1();
                }

                retNode = gtNewSimdStoreNode(op1, op2, simdBaseJitType, simdSize);
            }
            break;
        }

        case NI_Vector128_StoreUnsafe:
        case NI_Vector256_StoreUnsafe:
        {
            assert(retType == TYP_VOID);
            var_types simdType = getSIMDTypeForSize(simdSize);

            if (sig->numArgs == 3)
            {
                impSpillSideEffect(true, stackState.esStackDepth -
                                             3 DEBUGARG("Spilling op1 side effects for HWIntrinsic"));

                op3 = impPopStack().val;
            }
            else
            {
                assert(sig->numArgs == 2);

                impSpillSideEffect(true, stackState.esStackDepth -
                                             2 DEBUGARG("Spilling op1 side effects for HWIntrinsic"));
            }

            op2 = impPopStack().val;

            if (op2->OperIs(GT_CAST) && op2->gtGetOp1()->TypeIs(TYP_BYREF))
            {
                // If what we have is a BYREF, that's what we really want, so throw away the cast.
                op2 = op2->gtGetOp1();
            }

            if (sig->numArgs == 3)
            {
                op4 = gtNewIconNode(genTypeSize(simdBaseType), op3->TypeGet());
                op3 = gtNewOperNode(GT_MUL, op3->TypeGet(), op3, op4);
                op2 = gtNewOperNode(GT_ADD, op2->TypeGet(), op2, op3);
            }

            op1 = impSIMDPopStack();

            retNode = gtNewSimdStoreNode(op2, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_StoreAligned:
        case NI_Vector256_StoreAligned:
        {
            assert(sig->numArgs == 2);
            assert(retType == TYP_VOID);

            if (opts.OptimizationDisabled())
            {
                // LOONGARCH64 doesn't have aligned stores, but aligned stores are only validated to be
                // aligned when optimizations are disable, so only skip the intrinsic handling
                // if optimizations are enabled
                break;
            }

            var_types simdType = getSIMDTypeForSize(simdSize);

            impSpillSideEffect(true,
                               stackState.esStackDepth - 2 DEBUGARG("Spilling op1 side effects for HWIntrinsic"));

            op2 = impPopStack().val;

            if (op2->OperIs(GT_CAST) && op2->gtGetOp1()->TypeIs(TYP_BYREF))
            {
                // If what we have is a BYREF, that's what we really want, so throw away the cast.
                op2 = op2->gtGetOp1();
            }

            op1 = impSIMDPopStack();

            retNode = gtNewSimdStoreAlignedNode(op2, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_StoreAlignedNonTemporal:
        case NI_Vector256_StoreAlignedNonTemporal:
        {
            assert(sig->numArgs == 2);
            assert(retType == TYP_VOID);

            if (opts.OptimizationDisabled())
            {
                // LOONGARCH64 doesn't have aligned stores, but aligned stores are only validated to be
                // aligned when optimizations are disable, so only skip the intrinsic handling
                // if optimizations are enabled
                break;
            }

            var_types simdType = getSIMDTypeForSize(simdSize);

            impSpillSideEffect(true,
                               stackState.esStackDepth - 2 DEBUGARG("Spilling op1 side effects for HWIntrinsic"));

            op2 = impPopStack().val;

            if (op2->OperIs(GT_CAST) && op2->gtGetOp1()->TypeIs(TYP_BYREF))
            {
                // If what we have is a BYREF, that's what we really want, so throw away the cast.
                op2 = op2->gtGetOp1();
            }

            op1 = impSIMDPopStack();

            retNode = gtNewSimdStoreNonTemporalNode(op2, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_SubtractSaturate:
        case NI_Vector256_SubtractSaturate:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            if (varTypeIsFloating(simdBaseType))
            {
                retNode = gtNewSimdBinOpNode(GT_SUB, retType, op1, op2, simdBaseJitType, simdSize);
            }
            else
            {
                intrinsic = NI_LSX_SubtractSaturate;

                if (simdSize == 32)
                {
                    intrinsic = NI_LASX_SubtractSaturate;
                }

                retNode = gtNewSimdHWIntrinsicNode(retType, op1, op2, intrinsic, simdBaseJitType, simdSize);
            }
            break;
        }

        case NI_Vector128_Sum:
        case NI_Vector256_Sum:
        {
            assert(sig->numArgs == 1);
            var_types simdType = getSIMDTypeForSize(simdSize);

            op1     = impSIMDPopStack();
            retNode = gtNewSimdSumNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_Truncate:
        case NI_Vector256_Truncate:
        {
            assert(sig->numArgs == 1);

            if (!varTypeIsFloating(simdBaseType))
            {
                retNode = impSIMDPopStack();
                break;
            }

            op1     = impSIMDPopStack();
            retNode = gtNewSimdTruncNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_WidenLower:
        case NI_Vector256_WidenLower:
        {
            assert(sig->numArgs == 1);

            op1 = impSIMDPopStack();

            retNode = gtNewSimdWidenLowerNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_WidenUpper:
        case NI_Vector256_WidenUpper:
        {
            assert(sig->numArgs == 1);

            op1 = impSIMDPopStack();

            retNode = gtNewSimdWidenUpperNode(retType, op1, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_WithElement:
        case NI_Vector256_WithElement:
        {
            assert(numArgs == 3);
            GenTree* indexOp = impStackTop(1).val;

            if (!indexOp->OperIsConst())
            {
                if (!opts.OptimizationEnabled())
                {
                    // Only enable late stage rewriting if optimizations are enabled
                    // as we won't otherwise encounter a constant at the later point
                    return nullptr;
                }

                op3 = impPopStack().val;
                op2 = impPopStack().val;
                op1 = impSIMDPopStack();

                retNode = gtNewSimdHWIntrinsicNode(retType, op1, op2, op3, intrinsic, simdBaseJitType, simdSize);

                retNode->AsHWIntrinsic()->SetMethodHandle(this, method R2RARG(*entryPoint));
                break;
            }

            ssize_t imm8  = indexOp->AsIntCon()->IconValue();
            ssize_t count = simdSize / genTypeSize(simdBaseType);

            if ((imm8 >= count) || (imm8 < 0))
            {
                // Using software fallback if index is out of range (throw exception)
                return nullptr;
            }

            GenTree* valueOp = impPopStack().val;
            impPopStack(); // pop the indexOp that we already have.
            GenTree* vectorOp = impSIMDPopStack();

            retNode = gtNewSimdWithElementNode(retType, vectorOp, indexOp, valueOp, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_WithLower:
        {
            assert(sig->numArgs == 2);

            op2     = impPopStack().val;
            op1     = impSIMDPopStack();
            retNode = gtNewSimdWithLowerNode(retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector256_WithLower:
        {
            assert(sig->numArgs == 2);

            op2     = impSIMDPopStack();
            op1     = impSIMDPopStack();
            retNode = gtNewSimdWithLowerNode(retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_WithUpper:
        {
            assert(sig->numArgs == 2);

            op2     = impPopStack().val;
            op1     = impSIMDPopStack();
            retNode = gtNewSimdWithUpperNode(retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector256_WithUpper:
        {
            assert(sig->numArgs == 2);

            op2     = impSIMDPopStack();
            op1     = impSIMDPopStack();
            retNode = gtNewSimdWithUpperNode(retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        case NI_Vector128_op_ExclusiveOr:
        case NI_Vector256_op_ExclusiveOr:
        {
            assert(sig->numArgs == 2);

            op2 = impSIMDPopStack();
            op1 = impSIMDPopStack();

            retNode = gtNewSimdBinOpNode(GT_XOR, retType, op1, op2, simdBaseJitType, simdSize);
            break;
        }

        default:
        {
            return nullptr;
        }

    }

    if (isMinMaxIntrinsic)
    {
        assert(sig->numArgs == 2);
        assert(retNode == nullptr);

        if (isNative && BlockNonDeterministicIntrinsics(mustExpand))
        {
            return nullptr;
        }

        op2 = impSIMDPopStack();
        op1 = impSIMDPopStack();

        if (isNative)
        {
            assert(!isMagnitude && !isNumber);
            retNode = gtNewSimdMinMaxNativeNode(retType, op1, op2, simdBaseJitType, simdSize, isMax);
        }
        else
        {
            retNode = gtNewSimdMinMaxNode(retType, op1, op2, simdBaseJitType, simdSize, isMax, isMagnitude, isNumber);
        }
    }

    assert(!isScalar || isValidScalarIntrinsic);

    return retNode;
}

//------------------------------------------------------------------------
// getHWIntrinsicImmOps: Gets the immediate Ops for an intrinsic
//
// Arguments:
//    intrinsic       -- NamedIntrinsic associated with the HWIntrinsic to lookup
//    sig             -- signature of the intrinsic call.
//    immOp1Ptr [OUT] -- The first immediate Op
//    immOp2Ptr [OUT] -- The second immediate Op, if any. Otherwise unchanged.
//
void Compiler::getHWIntrinsicImmOps(NamedIntrinsic    intrinsic,
                                    CORINFO_SIG_INFO* sig,
                                    GenTree**         immOp1Ptr,
                                    GenTree**         immOp2Ptr)
{
    //should confirm
    //assert(!"unimplemented yet on LA");
    if (!HWIntrinsicInfo::HasImmediateOperand(intrinsic))
    {
        return;
    }

    // Position of the immediates from top of stack
    int imm1Pos = -1;
    int imm2Pos = -1;

    HWIntrinsicInfo::GetImmOpsPositions(intrinsic, sig, &imm1Pos, &imm2Pos);

    if (imm1Pos >= 0)
    {
        *immOp1Ptr = impStackTop(imm1Pos).val;
        assert(HWIntrinsicInfo::isImmOp(intrinsic, *immOp1Ptr));
    }

    if (imm2Pos >= 0)
    {
        *immOp2Ptr = impStackTop(imm2Pos).val;
        assert(HWIntrinsicInfo::isImmOp(intrinsic, *immOp2Ptr));
    }
}

#endif // FEATURE_HW_INTRINSICS

// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#ifdef FEATURE_HW_INTRINSICS

#include "codegen.h"
// HWIntrinsicImmOpHelper: constructs the helper class instance.
//       This also determines what type of "switch" table is being used (if an immediate operand is not constant) and do
//       some preparation work:
//
//       a) If an immediate operand can be either 0 or 1, this creates <nonZeroLabel>.
//
//       b) If an immediate operand can take any value in [0, upperBound), this extract a internal register from an
//       intrinsic node. The register will be later used to store computed branch target address.
//
// Arguments:
//    codeGen   -- an instance of CodeGen class.
//    immOp     -- an immediate operand of the intrinsic.
//    intrin    -- a hardware intrinsic tree node.
//    numInstrs -- number of instructions that will be in each switch entry. Default 1.
//
// Note: This class is designed to be used in the following way
//       HWIntrinsicImmOpHelper helper(this, immOp, intrin);
//
//       for (helper.EmitBegin(); !helper.Done(); helper.EmitCaseEnd())
//       {
//         -- emit an instruction for a given value of helper.ImmValue()
//       }
//
//       This allows to combine logic for cases when immOp->isContainedIntOrIImmed() is either true or false in a form
//       of a for-loop.
//
CodeGen::HWIntrinsicImmOpHelper::HWIntrinsicImmOpHelper(CodeGen*            codeGen,
                                                        GenTree*            immOp,
                                                        GenTreeHWIntrinsic* intrin,
                                                        int                 numInstrs)
    : codeGen(codeGen)
    , endLabel(nullptr)
    , nonZeroLabel(nullptr)
    , branchTargetReg(REG_NA)
    , numInstrs(numInstrs)
{
    assert(!"unimplemented yet on LA");
}

// HWIntrinsicImmOpHelper: Variant constructor of the helper class instance.
//       This is used when the immediate does not exist in a GenTree. For example, the immediate has been created
//       during codegen from other immediate values.
//
// Arguments:
//    codeGen       -- an instance of CodeGen class.
//    immReg        -- the register containing the immediate.
//    immLowerBound -- the lower bound of the register.
//    immUpperBound -- the lower bound of the register.
//    intrin        -- a hardware intrinsic tree node.
//
// Note: This instance is designed to be used via the same for loop as the standard constructor.
//
CodeGen::HWIntrinsicImmOpHelper::HWIntrinsicImmOpHelper(
    CodeGen* codeGen, regNumber immReg, int immLowerBound, int immUpperBound, GenTreeHWIntrinsic* intrin, int numInstrs)
    : codeGen(codeGen)
    , endLabel(nullptr)
    , nonZeroLabel(nullptr)
    , immValue(immLowerBound)
    , immLowerBound(immLowerBound)
    , immUpperBound(immUpperBound)
    , nonConstImmReg(immReg)
    , branchTargetReg(REG_NA)
    , numInstrs(numInstrs)
{
    assert(!"unimplemented yet on LA");
}

//------------------------------------------------------------------------
// EmitBegin: emits the beginning of a "switch" table, no-op if an immediate operand is constant.
//
// Note: The function is called at the beginning of code generation and emits
//    a) If an immediate operand can be either 0 or 1
//
//       cbnz <nonZeroLabel>, nonConstImmReg
//
//    b) If an immediate operand can take any value in [0, upperBound) range
//
//       adr branchTargetReg, <beginLabel>
//       add branchTargetReg, branchTargetReg, nonConstImmReg, lsl #3
//       br  branchTargetReg
//
//       When an immediate operand is non constant this also defines <beginLabel> right after the emitted code.
//
void CodeGen::HWIntrinsicImmOpHelper::EmitBegin()
{
    assert(!"unimplemented yet on LA");
}

//------------------------------------------------------------------------
// EmitCaseEnd: emits the end of a "case", no-op if an immediate operand is constant.
//
// Note: The function is called at the end of each "case" (i.e. after an instruction has been emitted for a given
// immediate value ImmValue())
//       and emits
//
//       b <endLabel>
//
//       After the last "case" this defines <endLabel>.
//
//       If an immediate operand is either 0 or 1 it also defines <nonZeroLabel> after the first "case".
//
void CodeGen::HWIntrinsicImmOpHelper::EmitCaseEnd()
{
    assert(!"unimplemented yet on LA");
}

//------------------------------------------------------------------------
// genHWIntrinsic: Generates the code for a given hardware intrinsic node.
//
// Arguments:
//    node - The hardware intrinsic node
//
void CodeGen::genHWIntrinsic(GenTreeHWIntrinsic* node)
{
    //should confirm
    const HWIntrinsic intrin(node);
    const instruction ins = HWIntrinsicInfo::lookupIns(intrin.id, intrin.baseType, compiler);
    emitAttr attr = emitActualTypeSize(intrin.baseType);
    emitAttr elementSize = emitTypeSize(intrin.baseType);

    regNumber targetReg = node->GetRegNum();

    genConsumeMultiOpOperands(node);

    switch (intrin.category)
    {

        case HW_Category_2R:
        {
            switch (intrin.id)
            {
                case NI_LSX_Ceiling:
                case NI_LASX_Ceiling:
                case NI_LSX_ConvertToDoubleUpper:
                case NI_LASX_ConvertToDoubleUpper:
                case NI_LSX_Floor:
                case NI_LASX_Floor:
                case NI_LSX_RoundToNegativeInfinity:
                case NI_LASX_RoundToNegativeInfinity:
                case NI_LSX_RoundToPositiveInfinity:
                case NI_LASX_RoundToPositiveInfinity:
                case NI_LSX_RoundToZero:
                case NI_LASX_RoundToZero:
                case NI_LSX_Sqrt:
                case NI_LASX_Sqrt:
                case NI_LSX_Reciprocal:
                case NI_LASX_Reciprocal:
                case NI_LSX_ReciprocalEstimate:
                case NI_LASX_ReciprocalEstimate:
                case NI_LSX_ReciprocalSqrt:
                case NI_LASX_ReciprocalSqrt:
                case NI_LSX_ReciprocalSqrtEstimate:
                case NI_LASX_ReciprocalSqrtEstimate:
                case NI_LSX_ConvertToInt32RoundToZero:
                case NI_LASX_ConvertToInt32RoundToZero:
                case NI_LSX_ConvertToInt64RoundToZero:
                case NI_LASX_ConvertToInt64RoundToZero:
                case NI_LSX_ConvertToUInt32RoundToZero:
                case NI_LASX_ConvertToUInt32RoundToZero:
                case NI_LSX_ConvertToUInt64RoundToZero:
                case NI_LASX_ConvertToUInt64RoundToZero:
                {
                    assert(ins != INS_invalid);
                    assert((intrin.numOperands == 1) && (intrin.op1 != nullptr) && (varTypeIsFloating(intrin.baseType)));

                    regNumber op1Reg = intrin.op1->GetRegNum();
                    GetEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg);
                    break;
                }

                case NI_LSX_LeadingZeroCount:
                case NI_LASX_LeadingZeroCount:
                case NI_LSX_ConvertToSingle:
                case NI_LASX_ConvertToSingle:
                case NI_LSX_ConvertToDouble:
                case NI_LASX_ConvertToDouble:
                {
                    assert((intrin.numOperands == 1) && (intrin.op1 != nullptr));

                    regNumber op1Reg = intrin.op1->GetRegNum();
                    GetEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg);
                    break;
                }

                case NI_LSX_DuplicateToVector128:
                case NI_LASX_DuplicateToVector256:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.op1 != nullptr);
                    regNumber op1Reg = intrin.op1->GetRegNum();

                    if (intrin.op1->isContainedFltOrDblImmed())
                    {
                        const double dataValue = intrin.op1->AsDblCon()->DconValue();
                        op1Reg = targetReg;

                        if (*(int64_t*)&dataValue == 0)
                        {
                            if (node->TypeIs(TYP_SIMD32))
                            {
                                GetEmitter()->emitIns_R_R_R(INS_xvxor_v, attr, targetReg, targetReg, targetReg, INS_OPTS_NONE);
                            }
                            else
                            {
                                GetEmitter()->emitIns_R_R_R(INS_vxor_v, attr, targetReg, targetReg, targetReg, INS_OPTS_NONE);
                            }
                            break;
                        }
                        else
                        {
                            CORINFO_FIELD_HANDLE hnd = GetEmitter()->emitFltOrDblConst(dataValue, elementSize);
                            instruction ins1 = elementSize == EA_4BYTE ? INS_fld_s : INS_fld_d;
                            assert(targetReg >= REG_F0);
                            GetEmitter()->emitIns_R_C(ins1, elementSize, op1Reg, REG_NA, hnd, 0);
                        }
                    }

                    if (intrin.op1->isContainedIntOrIImmed())
                    {
                        const ssize_t dataValue = intrin.op1->AsIntCon()->gtIconVal;
                        if (0 == dataValue)
                        {
                            op1Reg = REG_R0;
                        }
                        else
                        {
                            op1Reg = REG_SCRATCH;
                            GetEmitter()->emitIns_I_la(elementSize, op1Reg, dataValue);
                        }
                    }

                    if ((intrin.id == NI_LSX_DuplicateToVector128) && varTypeIsFloating(intrin.baseType))
                    {
                        //instruction ins1 = elementSize == EA_4BYTE ? INS_vreplvei_w : INS_vreplvei_d;
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, op1Reg, 0);
                    }
                    else
                    {
                        GetEmitter()->emitIns_R_R(ins, elementSize, targetReg, op1Reg);
                    }
                    break;
                }

                case NI_LSX_PopCount:
                case NI_LASX_PopCount:
                {
                    assert(ins != INS_invalid);
                    assert((intrin.op1 != nullptr) && (intrin.numOperands == 1) && (varTypeIsIntegral(intrin.baseType)));

                    GetEmitter()->emitIns_R_R(ins, attr, targetReg, intrin.op1->GetRegNum());
                    break;
                }
                case NI_LSX_ZeroExtendWideningUpper:
                case NI_LSX_SignExtendWideningUpper:
                case NI_LASX_ZeroExtendWideningUpper:
                case NI_LASX_SignExtendWideningUpper:
                {
                    assert(ins != INS_invalid);
                    GenTree* op1 = intrin.op1;
                    regNumber op1Reg = op1->GetRegNum();
                    assert(emitter::isFloatReg(op1Reg) && (emitter::isFloatReg(targetReg)) && (varTypeIsIntegral(intrin.baseType)));

                    GetEmitter()->emitIns_R_R(ins, EA_16BYTE, targetReg, op1Reg);
                    break;
                }

                case NI_LSX_HorizontalSum:
                {
                    GenTree* op1 = intrin.op1;
                    regNumber op1Reg = op1->GetRegNum();

                    if (varTypeIsFloating(intrin.baseType))
                    {
                        //vbsrl.v  vd,vj,ui5_bytes
                        //vfadd.s/d

                        int srl = 0;
                        switch (elementSize)
                        {
                            case EA_4BYTE:
                                srl = 4;
                                break;
                            case EA_8BYTE:
                                srl = 8;
                                break;
                            default:
                                unreached();
                        }

                        GetEmitter()->emitIns_R_R_I(INS_vbsrl_v, EA_16BYTE, REG_SCRATCH_FLT, op1Reg, srl);

                        if (srl == 4)
                        {
                            if (node->GetSimdSize() > 8)
                            {
                                GetEmitter()->emitIns_R_R_R(INS_vfadd_s, EA_16BYTE, REG_SCRATCH_FLT, op1Reg, REG_SCRATCH_FLT);

                                GetEmitter()->emitIns_R_R_I(INS_vbsrl_v, EA_16BYTE, targetReg, REG_SCRATCH_FLT, 8);
                                GetEmitter()->emitIns_R_R_R(INS_vfadd_s, EA_16BYTE, targetReg, targetReg, REG_SCRATCH_FLT);
                            }
                            else
                            {
                                GetEmitter()->emitIns_R_R_R(INS_vfadd_s, EA_16BYTE, targetReg, op1Reg, REG_SCRATCH_FLT);
                            }
                        }
                        else
                        {
                            GetEmitter()->emitIns_R_R_R(INS_vfadd_d, EA_16BYTE, targetReg, op1Reg, REG_SCRATCH_FLT);
                        }
                    }
                    else
                    {
                        //vhaddw.h/w/d/q.b/h/w/d  vd,vj,vk
                        instruction ins1 = INS_invalid;
                        switch (elementSize)
                        {
                            case EA_1BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_vhaddw_h_b : INS_vhaddw_hu_bu;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_16BYTE, targetReg, op1Reg, op1Reg);
                                op1Reg = targetReg;
                                FALLTHROUGH;
                            case EA_2BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_vhaddw_w_h : INS_vhaddw_wu_hu;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_16BYTE, targetReg, op1Reg, op1Reg);
                                op1Reg = targetReg;
                                FALLTHROUGH;
                            case EA_4BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_vhaddw_d_w : INS_vhaddw_du_wu;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_16BYTE, targetReg, op1Reg, op1Reg);
                                op1Reg = targetReg;
                                FALLTHROUGH;
                            case EA_8BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_vhaddw_q_d : INS_vhaddw_qu_du;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_16BYTE, targetReg, op1Reg, op1Reg);
                                break;
                            default:
                                unreached();
                        }
                    }
                    break;
                }

                case NI_LASX_HorizontalSum:
                {
                    GenTree* op1 = intrin.op1;
                    regNumber op1Reg = op1->GetRegNum();

                    assert(node->GetSimdSize() == 32);

                    if (varTypeIsFloating(intrin.baseType))
                    {
                        //xvbsrl.v    xd,xj,ui5_bytes
                        //xvfadd.s/d  xd,xj,xk

                        int srl = 0;
                        switch (elementSize)
                        {
                            case EA_4BYTE:
                                srl = 4;
                                break;
                            case EA_8BYTE:
                                srl = 8;
                                break;
                            default:
                                unreached();
                        }

                        GetEmitter()->emitIns_R_R_I(INS_xvbsrl_v, EA_32BYTE, REG_SCRATCH_FLT, op1Reg, srl);

                        if (srl == 4)
                        {
                            GetEmitter()->emitIns_R_R_R(INS_xvfadd_s, EA_32BYTE, REG_SCRATCH_FLT, op1Reg, REG_SCRATCH_FLT);

                            GetEmitter()->emitIns_R_R_I(INS_xvbsrl_v, EA_32BYTE, targetReg, REG_SCRATCH_FLT, 8);
                            GetEmitter()->emitIns_R_R_R(INS_xvfadd_s, EA_32BYTE, targetReg, targetReg, REG_SCRATCH_FLT);

                            GetEmitter()->emitIns_R_R_I(INS_xvpickve_w, EA_32BYTE, REG_SCRATCH_FLT, targetReg, 4);
                            GetEmitter()->emitIns_R_R_R(INS_xvfadd_s, EA_32BYTE, targetReg, targetReg, REG_SCRATCH_FLT);
                        }
                        else
                        {
                            GetEmitter()->emitIns_R_R_R(INS_xvfadd_d, EA_32BYTE, targetReg, op1Reg, REG_SCRATCH_FLT);

                            GetEmitter()->emitIns_R_R_I(INS_xvpickve_d, EA_32BYTE, REG_SCRATCH_FLT, targetReg, 2);
                            GetEmitter()->emitIns_R_R_R(INS_xvfadd_d, EA_32BYTE, targetReg, targetReg, REG_SCRATCH_FLT);
                        }
                    }
                    else
                    {
                        //xvhaddw.h/w/d/q.b/h/w/d  xd,xj,xk
                        instruction ins1 = INS_invalid;
                        switch (elementSize)
                        {
                            case EA_1BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_xvhaddw_h_b : INS_xvhaddw_hu_bu;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_32BYTE, targetReg, op1Reg, op1Reg);
                                op1Reg = targetReg;
                                FALLTHROUGH;
                            case EA_2BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_xvhaddw_w_h : INS_xvhaddw_wu_hu;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_32BYTE, targetReg, op1Reg, op1Reg);
                                op1Reg = targetReg;
                                FALLTHROUGH;
                            case EA_4BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_xvhaddw_d_w : INS_xvhaddw_du_wu;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_32BYTE, targetReg, op1Reg, op1Reg);
                                op1Reg = targetReg;
                                FALLTHROUGH;
                            case EA_8BYTE:
                                ins1 = varTypeIsUnsigned(intrin.baseType) ? INS_xvhaddw_q_d : INS_xvhaddw_qu_du;
                                GetEmitter()->emitIns_R_R_R(ins1, EA_32BYTE, targetReg, op1Reg, op1Reg);
                                break;
                            default:
                                unreached();
                        }

                        GetEmitter()->emitIns_R_R_I(INS_xvpickve_d, EA_32BYTE, REG_SCRATCH_FLT, targetReg, 2);
                        GetEmitter()->emitIns_R_R_R(INS_xvadd_d, EA_32BYTE, targetReg, targetReg, REG_SCRATCH_FLT);
                    }
                    break;
                }

                case NI_LSX_MoveMask:
                case NI_LASX_MoveMask:
                {
                    assert(ins != INS_invalid);
                    assert((intrin.numOperands == 1) && (intrin.op1 != nullptr));

                    regNumber op1Reg = intrin.op1->GetRegNum();
                    GetEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg);
                    break;
                }

                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():2R.");
                    unreached();
            }
            break;
        }

        case HW_Category_3R:
        {
            GenTree* op1 = intrin.op1;
            GenTree* op2 = intrin.op2;

            switch (intrin.id)
            {
                case NI_LSX_Add:
                case NI_LASX_Add:
                case NI_LSX_AddSaturate:
                case NI_LASX_AddSaturate:
                case NI_LSX_Subtract:
                case NI_LASX_Subtract:
                case NI_LSX_SubtractSaturate:
                case NI_LASX_SubtractSaturate:
                case NI_LSX_CompareEqual:
                case NI_LASX_CompareEqual:
                case NI_LSX_CompareEqualUnordered:
                case NI_LASX_CompareEqualUnordered:
                case NI_LSX_CompareNotEqual:
                case NI_LASX_CompareNotEqual:
                case NI_LSX_CompareNotEqualUnordered:
                case NI_LASX_CompareNotEqualUnordered:
                case NI_LSX_CompareLessThan:
                case NI_LASX_CompareLessThan:
                case NI_LSX_CompareLessThanUnordered:
                case NI_LASX_CompareLessThanUnordered:
                case NI_LSX_CompareLessThanOrEqual:
                case NI_LASX_CompareLessThanOrEqual:
                case NI_LSX_CompareLessThanOrEqualUnordered:
                case NI_LASX_CompareLessThanOrEqualUnordered:
                case NI_LSX_And:
                case NI_LASX_And:
                case NI_LSX_Or:
                case NI_LASX_Or:
                case NI_LSX_OrNot:
                case NI_LASX_OrNot:
                case NI_LSX_NotOr:
                case NI_LASX_NotOr:
                case NI_LSX_Xor:
                case NI_LASX_Xor:
                case NI_LSX_Max:
                case NI_LASX_Max:
                case NI_LSX_Min:
                case NI_LASX_Min:
                case NI_LSX_MaxNumMag:
                case NI_LASX_MaxNumMag:
                case NI_LSX_MinNumMag:
                case NI_LASX_MinNumMag:
                case NI_LSX_Multiply:
                case NI_LASX_Multiply:
                case NI_LSX_MultiplyAdd:
                case NI_LASX_MultiplyAdd:
                case NI_LSX_MultiplyHigh:
                case NI_LASX_MultiplyHigh:
                case NI_LSX_MultiplySubtract:
                case NI_LASX_MultiplySubtract:
                case NI_LSX_MultiplyWideningLower:
                case NI_LASX_MultiplyWideningLower:
                case NI_LSX_MultiplyWideningLowerAndAdd:
                case NI_LASX_MultiplyWideningLowerAndAdd:
                case NI_LSX_MultiplyWideningUpper:
                case NI_LASX_MultiplyWideningUpper:
                case NI_LSX_MultiplyWideningUpperAndAdd:
                case NI_LASX_MultiplyWideningUpperAndAdd:
                case NI_LSX_Divide:
                case NI_LASX_Divide:
                {
                    assert(ins != INS_invalid);
                    assert(!op1->isContainedIntOrIImmed() && (!op2->isContainedIntOrIImmed()) && (varTypeIsSIMD(op1) || varTypeIsSIMD(op2)));
                    regNumber reg1 = op1->GetRegNum();
                    if (op1->TypeGet() == TYP_FLOAT)
                    {
                        assert(intrin.baseType == TYP_FLOAT);
                        assert(varTypeIsSIMD(op2));
                        if (compiler->compOpportunisticallyDependsOn(InstructionSet_LASX))
                        {
                            GetEmitter()->emitIns_R_R(INS_xvreplve0_w, attr, reg1, reg1);
                        }
                        else
                        {
                            assert(op2->TypeGet() != TYP_SIMD32);
                            GetEmitter()->emitIns_R_R_I(INS_vreplvei_w, attr, reg1, reg1, 0);
                        }
                    }
                    else if (op1->TypeGet() == TYP_DOUBLE)
                    {
                        assert(intrin.baseType == TYP_DOUBLE);
                        assert(varTypeIsSIMD(op2));
                        if (compiler->compOpportunisticallyDependsOn(InstructionSet_LASX))
                        {
                            GetEmitter()->emitIns_R_R(INS_xvreplve0_d, attr, reg1, reg1);
                        }
                        else
                        {
                            assert(op2->TypeGet() != TYP_SIMD32);
                            GetEmitter()->emitIns_R_R_I(INS_vreplvei_d, attr, reg1, reg1, 0);
                        }
                    }

                    regNumber reg2 = op2->GetRegNum();
                    if (op2->TypeGet() == TYP_FLOAT)
                    {
                        assert(intrin.baseType == TYP_FLOAT);
                        assert(varTypeIsSIMD(op1));
                        if (compiler->compOpportunisticallyDependsOn(InstructionSet_LASX))
                        {
                            GetEmitter()->emitIns_R_R(INS_xvreplve0_w, attr, reg2, reg2);
                        }
                        else
                        {
                            assert(op2->TypeGet() != TYP_SIMD32);
                            GetEmitter()->emitIns_R_R_I(INS_vreplvei_w, attr, reg2, reg2, 0);
                        }
                    }
                    else if (op2->TypeGet() == TYP_DOUBLE)
                    {
                        assert(intrin.baseType == TYP_DOUBLE);
                        assert(varTypeIsSIMD(op1));
                        if (compiler->compOpportunisticallyDependsOn(InstructionSet_LASX))
                        {
                            GetEmitter()->emitIns_R_R(INS_xvreplve0_d, attr, reg2, reg2);
                        }
                        else
                        {
                            assert(op2->TypeGet() != TYP_SIMD32);
                            GetEmitter()->emitIns_R_R_I(INS_vreplvei_d, attr, reg2, reg2, 0);
                        }
                    }

                    GetEmitter()->emitIns_R_R_R(ins, attr, targetReg, reg1, reg2);
                    break;
                }
                case NI_LSX_AndNot:
                case NI_LASX_AndNot:
                case NI_LSX_CompareGreaterThanOrEqual:
                case NI_LASX_CompareGreaterThanOrEqual:
                case NI_LSX_CompareGreaterThanOrEqualUnordered:
                case NI_LASX_CompareGreaterThanOrEqualUnordered:
                case NI_LSX_CompareGreaterThan:
                case NI_LASX_CompareGreaterThan:
                case NI_LSX_CompareGreaterThanUnordered:
                case NI_LASX_CompareGreaterThanUnordered:
                case NI_LSX_VectorElementsFusionHight:
                case NI_LSX_VectorElementsFusionLower:
                {
                    assert(ins != INS_invalid);
                    assert(varTypeIsSIMD(op1) && varTypeIsSIMD(op2) && (!op1->isContainedIntOrIImmed()) && (!op2->isContainedIntOrIImmed()));// TODO-LA-SIMD: supporting imm!
                    GetEmitter()->emitIns_R_R_R(ins, attr, targetReg, op2->GetRegNum(), op1->GetRegNum());
                    break;
                }

                case NI_LSX_ConvertDoubleToSingle:
                case NI_LASX_ConvertDoubleToSingle:
                case NI_LSX_ShiftLeftLogical:
                case NI_LASX_ShiftLeftLogical:
                case NI_LSX_ShiftRightLogical:
                case NI_LASX_ShiftRightLogical:
                case NI_LSX_ShiftRightArithmetic:
                case NI_LASX_ShiftRightArithmetic:
                case NI_LSX_AddWideningLowerAndUpper:
                case NI_LASX_AddWideningLowerAndUpper:
                {
                    assert(ins != INS_invalid);
                    assert(varTypeIsSIMD(op1) && varTypeIsSIMD(op2) && (!op1->isContainedIntOrIImmed()) && (!op2->isContainedIntOrIImmed()));
                    GetEmitter()->emitIns_R_R_R(ins, attr, targetReg, op1->GetRegNum(), op2->GetRegNum());
                    break;
                }

                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():3R.");
                    unreached();
            }
            break;
        }

        case HW_Category_4R:
        {
            GenTree* op1 = intrin.op1;
            GenTree* op2 = intrin.op2;
            GenTree* op3 = intrin.op3;
            assert((op1 != nullptr) && (op2 != nullptr) && (op3 != nullptr));

            switch (intrin.id)
            {
                case NI_LSX_BitwiseSelect:
                case NI_LASX_BitwiseSelect:
                {
                    assert(ins != INS_invalid);
                    assert(varTypeIsSIMD(op1) && varTypeIsSIMD(op2) && varTypeIsSIMD(op3));
                    assert(!op1->isContainedIntOrIImmed() && !op2->isContainedIntOrIImmed() && !op3->isContainedIntOrIImmed());
                    GetEmitter()->emitIns_R_R_R_R(ins, attr, targetReg, op3->GetRegNum(), op2->GetRegNum(), op1->GetRegNum());
                    break;
                }
                case NI_LSX_FusedMultiplyAdd:
                case NI_LASX_FusedMultiplyAdd:
                case NI_LSX_FusedMultiplyAddNegated:
                case NI_LASX_FusedMultiplyAddNegated:
                case NI_LSX_FusedMultiplySubtract:
                case NI_LASX_FusedMultiplySubtract:
                case NI_LSX_FusedMultiplySubtractNegated:
                case NI_LASX_FusedMultiplySubtractNegated:
                {
                    assert(ins != INS_invalid);
                    assert(varTypeIsSIMD(op1) && varTypeIsSIMD(op2) && varTypeIsSIMD(op3));
                    assert(!op1->isContainedIntOrIImmed() && !op2->isContainedIntOrIImmed() && !op3->isContainedIntOrIImmed());
                    GetEmitter()->emitIns_R_R_R_R(ins, attr, targetReg, op1->GetRegNum(), op2->GetRegNum(), op3->GetRegNum());
                    break;
                }
                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():4R.");
                    unreached();
            }
            break;
        }

        case HW_Category_1R_1I:
        {
            GenTree* op1 = node->Op(1);
            GenTree* op2 = node->Op(2);
            switch (intrin.id)
            {
                case NI_Vector128_get_Zero:
                case NI_Vector256_get_Zero:
                {
                    assert(ins != INS_invalid);
                    assert(varTypeIsSIMD(node) && (op1 == nullptr) && (op2 == nullptr));
                    GetEmitter()->emitIns_R_I(ins, attr, targetReg, 0);
                    break;
                }

                case NI_Vector128_get_AllBitsSet:
                case NI_Vector256_get_AllBitsSet:
                {
                    assert(ins != INS_invalid);
                    assert(varTypeIsSIMD(node) && (op1 == nullptr) && (op2 == nullptr));
                    GetEmitter()->emitIns_R_I(ins, attr, targetReg, 0xfff);
                    break;
                }

                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():1R1I.");
            }
            break;
        }

        case HW_Category_2R_1I:
        {
            switch (intrin.id)
            {
                case NI_Vector128_CreateScalarUnsafe:
                case NI_Vector256_CreateScalarUnsafe:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.numOperands == 1);
                    assert(intrin.op1 != nullptr);

                    regNumber op1Reg = intrin.op1->GetRegNum();

                    if (intrin.op1->isContainedFltOrDblImmed())
                    {
                        const double dataValue = intrin.op1->AsDblCon()->DconValue();

                        if (*(int64_t*)&dataValue == 0)
                        {
                            GetEmitter()->emitIns_R_R(INS_movgr2fr_d, EA_8BYTE, targetReg, REG_R0);
                        }
                        else
                        {
                            CORINFO_FIELD_HANDLE hnd = GetEmitter()->emitFltOrDblConst(dataValue, elementSize);
                            assert(targetReg >= REG_F0);
                            instruction ins = elementSize == EA_4BYTE ? INS_fld_s : INS_fld_d;
                            GetEmitter()->emitIns_R_C(ins, elementSize, targetReg, REG_NA, hnd, 0);
                        }
                    }
                    else
                    {
                        if (intrin.op1->isContainedIntOrIImmed())
                        {
                            const ssize_t dataValue = intrin.op1->AsIntCon()->gtIconVal;
                            if (0 == dataValue)
                            {
                                op1Reg = REG_R0;
                            }
                            else
                            {
                                op1Reg = REG_SCRATCH;
                                GetEmitter()->emitIns_I_la(elementSize, op1Reg, dataValue);
                            }
                        }
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, op1Reg, 0);
                    }
                    break;
                }

                case NI_LSX_Insert:
                case NI_LASX_Insert:
                {
                    assert(ins != INS_invalid);
                    assert((targetReg >= REG_F0) && (targetReg <= REG_F31));
                    assert(!intrin.op3->isContainedFltOrDblImmed());

                    ssize_t elementIndex = intrin.op2->AsIntCon()->gtIconVal;
                    assert(elementIndex < 32);
                    var_types opType = intrin.baseType;

                    if (targetReg != intrin.op1->GetRegNum())
                    {
                        //Notes: We use NI_L(A)SX_Insert to realize NI_Vector{128|256}_WithElement.
                        assert(elementSize != 8);
                        instruction ins = INS_vbsll_v;
                        if ((intrin.id == NI_LASX_Insert) && (compiler->compOpportunisticallyDependsOn(InstructionSet_LASX)))
                        {
                            ins = INS_xvbsll_v;
                        }
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, intrin.op1->GetRegNum(), 0);
                    }

                    assert(targetReg != intrin.op3->GetRegNum());
                    regNumber srcReg = intrin.op3->GetRegNum();
                    if ((intrin.id == NI_LSX_Insert) && varTypeIsFloating(intrin.baseType))
                    {
                        //TODO for LA-SIMD: use LASX to optimize if support LASX?
                        instruction ins1 = elementSize == EA_4BYTE ? INS_vpickve2gr_w : INS_vpickve2gr_d;
                        GetEmitter()->emitIns_R_R_I(ins1, elementSize, REG_R21, srcReg, 0);
                        srcReg = REG_R21;
                    }

                    if (((elementSize == EA_1BYTE) || (elementSize == EA_2BYTE)) && (intrin.id == NI_LASX_Insert))
                    {//Insert the TYP_(U)BYTE/TYP_(U)SHORT values into the 256bits-vector-reg with NI_LASX_Insert.
                     //for the LA architecture, there is no xvinsgr2vr.b/h instruction, so when using vinsgr2vr.b/h here,
                     //it is still necessary to ensure that the high 128 bits remain unchanged.
                        assert(compiler->compOpportunisticallyDependsOn(InstructionSet_LASX));
                        ssize_t index1 = elementSize == EA_1BYTE ? (elementIndex / 8) : (elementIndex / 4);
                        ssize_t index2 = elementSize == EA_1BYTE ? ((elementIndex - (index1 * 8)) % 8) : ((elementIndex - (index1 * 4)) % 4);
                        GetEmitter()->emitIns_R_R_I(INS_xvpickve_d, EA_8BYTE, REG_SCRATCH_FLT, targetReg, index1);
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, REG_SCRATCH_FLT, intrin.op3->GetRegNum(), index2);
                        GetEmitter()->emitIns_R_R_I(INS_xvinsve0_d, EA_8BYTE, targetReg, REG_SCRATCH_FLT, index1);
                    }
                    else
                    {
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, srcReg, elementIndex);
                    }
                    break;
                }

                case NI_Vector128_ToScalar:
                case NI_Vector256_ToScalar:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.op1 != nullptr);
                    regNumber op1Reg = intrin.op1->GetRegNum();

                    // no-op if vector is float/double, targetReg == op1Reg and fetching for 0th index.
                    if (varTypeIsFloating(intrin.baseType) && (targetReg == op1Reg))
                    {
                        break;
                    }

                    if ((intrin.id == NI_Vector128_ToScalar) && varTypeIsFloating(intrin.baseType))
                    {
                        //TODO for LA-SIMD: use LASX to optimize if support LASX?
                        instruction ins1 = intrin.baseType == TYP_FLOAT ? INS_vpickve2gr_w : INS_vpickve2gr_d;
                        GetEmitter()->emitIns_R_R_I(ins1, elementSize, REG_R21, op1Reg, 0);
                        op1Reg = REG_R21;
                    }

                    GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, op1Reg, 0);
                    break;
                }

                case NI_Vector128_ToVector256:
                {
                    //This is a impSpecialIntrinsic, can recognise LSX/LASX.
                    GetEmitter()->emitIns_R_R_I(INS_xvinsgr2vr_d, EA_8BYTE, intrin.op1->GetRegNum(), REG_R0, 2);
                    GetEmitter()->emitIns_R_R_I(INS_xvinsgr2vr_d, EA_8BYTE, intrin.op1->GetRegNum(), REG_R0, 3);

                    if (targetReg == intrin.op1->GetRegNum())
                    {
                        break;
                    }

                    GetEmitter()->emitIns_R_I(INS_xvldi, EA_32BYTE, targetReg, 0);
                    FALLTHROUGH;
                }
                case NI_Vector128_ToVector256Unsafe:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.numOperands == 1);
                    assert(intrin.op1 != nullptr);
                    if (targetReg == intrin.op1->GetRegNum())
                    {
                        break;
                    }
                    GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, intrin.op1->GetRegNum(), 0);
                    break;
                }

                case NI_LSX_FirstNegativeInteger:
                {
                    assert(ins != INS_invalid);
                    GenTree* op1 = intrin.op1;
                    regNumber op1Reg = op1->GetRegNum();

                    assert((intrin.baseType == TYP_UBYTE) || (intrin.baseType == TYP_USHORT));
                    assert(emitter::isFloatReg(op1Reg) && emitter::isIntegerRegister(targetReg));

                    switch (elementSize)
                    {
                        case EA_1BYTE:
                            GetEmitter()->emitIns_R_R_I(ins, EA_16BYTE, REG_SCRATCH_FLT, op1Reg, 0);
                            GetEmitter()->emitIns_R_R_I(INS_vpickve2gr_bu, EA_16BYTE, targetReg, REG_SCRATCH_FLT, 0);
                            break;
                        case EA_2BYTE:
                            GetEmitter()->emitIns_R_R_I(ins, EA_16BYTE, REG_SCRATCH_FLT, op1Reg, 0);
                            GetEmitter()->emitIns_R_R_I(INS_vpickve2gr_hu, EA_16BYTE, targetReg, REG_SCRATCH_FLT, 0);
                            break;
                        default:
                            unreached();
                    }
                    break;
                }

                case NI_LASX_Permute:
                case NI_LSX_ZeroExtendWideningLower:
                case NI_LASX_ZeroExtendWideningLower:
                case NI_LSX_SignExtendWideningLower:
                case NI_LASX_SignExtendWideningLower:
                case NI_LSX_ShiftRightArithmeticImm:
                case NI_LASX_ShiftRightArithmeticImm:
                case NI_LSX_ShiftLeftLogicalImm:
                case NI_LASX_ShiftLeftLogicalImm:
                case NI_LSX_ShiftRightLogicalImm:
                case NI_LASX_ShiftRightLogicalImm:
                {
                    assert(varTypeIsIntegral(intrin.baseType));
                    FALLTHROUGH;
                }
                case NI_LSX_Permute:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.numOperands == 2);
                    assert((intrin.op1 != nullptr) && (intrin.op2 != nullptr));
                    regNumber op1Reg = intrin.op1->GetRegNum();
                    assert(emitter::isFloatReg(op1Reg) && emitter::isFloatReg(targetReg));
                    const ssize_t dataValue = intrin.op2->AsIntCon()->gtIconVal;
                    GetEmitter()->emitIns_R_R_I(ins, attr, targetReg, op1Reg, dataValue);
                    break;
                }

                case NI_LASX_PermuteQ:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.numOperands == 3);
                    assert((intrin.op1 != nullptr) && (intrin.op2 != nullptr) && (intrin.op3 != nullptr));
                    regNumber op1Reg = intrin.op1->GetRegNum();
                    regNumber op2Reg = intrin.op2->GetRegNum();
                    const ssize_t dataValue = intrin.op3->AsIntCon()->gtIconVal;
                    if (targetReg != op1Reg)
                    {
                        assert(elementSize != 8);
                        instruction ins = INS_xvbsll_v;
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, op1Reg, 0);
                    }
                    GetEmitter()->emitIns_R_R_I(ins, attr, targetReg, op2Reg, dataValue);
                    break;
                }

                case NI_LSX_ShiftRightLogicalNarrowingLower:
                case NI_LASX_ShiftRightLogicalNarrowingLower:
                case NI_LSX_ShiftRightLogicalNarrowingSaturateLower:
                case NI_LASX_ShiftRightLogicalNarrowingSaturateLower:
                case NI_LSX_ShiftRightArithmeticNarrowingSaturateLower:
                case NI_LASX_ShiftRightArithmeticNarrowingSaturateLower:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.numOperands == 3);
                    assert((intrin.op1 != nullptr) && (intrin.op2 != nullptr) && (intrin.op3 != nullptr));
                    regNumber op1Reg = intrin.op1->GetRegNum();
                    regNumber op2Reg = intrin.op2->GetRegNum();
                    ssize_t saValue = intrin.op3->AsIntCon()->gtIconVal;
                    assert((targetReg >= REG_F0) && (targetReg <= REG_F31));
                    assert(emitter::isFloatReg(op2Reg) && emitter::isFloatReg(targetReg) && varTypeIsIntegral(intrin.baseType));

                    if (targetReg != op1Reg)
                    {
                        assert(elementSize != 8);
                        instruction ins = INS_vbsll_v;
                        if ((intrin.id == NI_LASX_ShiftRightLogicalNarrowingLower) && (compiler->compOpportunisticallyDependsOn(InstructionSet_LASX)))
                        {
                            ins = INS_xvbsll_v;
                        }
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, intrin.op1->GetRegNum(), 0);
                    }
                    GetEmitter()->emitIns_R_R_I(ins, attr, targetReg, op2Reg, saValue);
                    break;
                }

                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():2R1I.");
            }
            break;
        }

        case HW_Category_2R_2I:
        {
            assert(!"TODO-LA-SIMD: genHWIntrinsic():2R2I.");
            break;
        }

        case HW_Category_Scalar:
        {
            switch (intrin.id)
            {
                case NI_LoongArch64Base_LeadingZeroCount:
                case NI_LoongArch64Base_LeadingSignCount:
                case NI_LoongArch64Base_TrailingZeroCount:
                case NI_LoongArch64Base_TrailingOneCount:
                case NI_LoongArch64Base_ReverseElementBits:
                case NI_LoongArch64Base_Reciprocal:
                case NI_LoongArch64Base_ReciprocalEstimate:
                case NI_LoongArch64Base_ReciprocalSqrt:
                case NI_LoongArch64Base_ReciprocalSqrtEstimate:
                {
                    assert(ins != INS_invalid);
                    assert((intrin.numOperands == 1) && (intrin.op1 != nullptr));

                    regNumber op1Reg = intrin.op1->GetRegNum();
                    GetEmitter()->emitIns_R_R(ins, attr, targetReg, op1Reg);
                    break;
                }

                case NI_LoongArch64Base_CyclicRedundancyCheckIEEE8023:
                case NI_LoongArch64Base_CyclicRedundancyCheckCastagnoli:
                case NI_LoongArch64Base_MultiplyHigh:
                {
                    assert(ins != INS_invalid);
                    assert((intrin.numOperands == 2) && (intrin.op1 != nullptr) && (intrin.op2 != nullptr));
                    // FIXME: should confirm whether CRC32[C] need zero-extended ?
                    GetEmitter()->emitIns_R_R_R(ins, attr, targetReg, intrin.op2->GetRegNum(), intrin.op1->GetRegNum());
                    break;
                }

                case NI_LoongArch64Base_FusedMultiplyAdd:
                {
                    assert(ins != INS_invalid);
                    assert((intrin.numOperands == 3) && (intrin.op1 != nullptr) && (intrin.op2 != nullptr) && (intrin.op3 != nullptr));
                    GetEmitter()->emitIns_R_R_R_R(ins, attr, targetReg, intrin.op1->GetRegNum(), intrin.op2->GetRegNum(), intrin.op3->GetRegNum());
                    break;
                }

                case NI_LSX_HasElementsNotZero:
                case NI_LSX_AllElementsIsZero:
                case NI_LSX_AllElementsNotZero:
                case NI_LSX_HasElementsIsZero:
                case NI_LASX_HasElementsNotZero:
                case NI_LASX_AllElementsIsZero:
                case NI_LASX_AllElementsNotZero:
                case NI_LASX_HasElementsIsZero:
                {
                    assert(ins != INS_invalid);
                    GenTree* op1 = intrin.op1;
                    regNumber op1Reg = op1->GetRegNum();

                    assert(emitter::isFloatReg(op1Reg));
                    assert(emitter::isIntegerRegister(targetReg));

                    GetEmitter()->emitIns_R_R_I(INS_ori, EA_8BYTE, targetReg, REG_R0, 0);
                    GetEmitter()->emitIns_R_I(ins, EA_16BYTE, op1Reg, 2 /* cc */);
                    GetEmitter()->emitIns_R_I(INS_movcf2gr, EA_8BYTE, targetReg, 2 /* cc */);
                    break;
                }

                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():HW_Category_Scalar.");
            }
            break;
        }

        case HW_Category_MemoryLoad:
        {
            assert(ins != INS_invalid);
            regNumber op1Reg = intrin.op1->GetRegNum();

            assert(emitter::isFloatReg(targetReg) && emitter::isIntegerRegister(op1Reg));

            GetEmitter()->emitIns_R_R_I(ins, EA_16BYTE, targetReg, op1Reg, 0);
            break;
        }

        case HW_Category_MemoryStore:
        {
            assert(ins != INS_invalid);
            regNumber op1Reg = intrin.op1->GetRegNum(); // addr
            regNumber op2Reg = intrin.op2->GetRegNum(); // data

            assert(emitter::isIntegerRegister(op1Reg) && emitter::isFloatReg(op2Reg));

            GetEmitter()->emitIns_R_R_I(ins, EA_16BYTE, op2Reg, op1Reg, 0);
            break;
        }

        case HW_Category_SIMD:
        {
            switch (intrin.id)
            {
                case NI_LSX_Abs:
                case NI_LASX_Abs:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.op1 != nullptr);
                    assert(intrin.numOperands == 1);

                    if (varTypeIsFloating(intrin.baseType))
                    {
                        assert((intrin.baseType == TYP_FLOAT) || (intrin.baseType == TYP_DOUBLE));
                        int index = intrin.baseType == TYP_FLOAT ? 31 : 63;
                        GetEmitter()->emitIns_R_R_I(ins, attr, targetReg, intrin.op1->GetRegNum(), index);
                    }
                    else
                    {
                        if (intrin.id == NI_LSX_Abs)
                        {
                            GetEmitter()->emitIns_R_I(INS_vldi, EA_16BYTE, REG_SCRATCH_FLT, 0);
                        }
                        else
                        {
                            GetEmitter()->emitIns_R_I(INS_xvldi, EA_32BYTE, REG_SCRATCH_FLT, 0);
                        }
                        GetEmitter()->emitIns_R_R_R(ins, attr, targetReg, intrin.op1->GetRegNum(), REG_SCRATCH_FLT);
                    }
                    break;
                }
                case NI_LSX_Negate:
                case NI_LASX_Negate:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.op1 != nullptr);
                    assert(intrin.numOperands == 1);
                    assert(varTypeIsIntegral(intrin.baseType));

                    GetEmitter()->emitIns_R_R(ins, attr, targetReg, intrin.op1->GetRegNum());
                    break;
                }
                case NI_LSX_RoundToNearest:
                case NI_LASX_RoundToNearest:
                {
                    assert(ins != INS_invalid);
                    assert(intrin.op1 != nullptr);
                    assert(intrin.numOperands == 1);
                    assert(varTypeIsFloating(intrin.baseType));

                    GetEmitter()->emitIns_R_R(ins, attr, targetReg, intrin.op1->GetRegNum());
                    break;
                }
                case NI_Vector128_GetLower:
                case NI_Vector256_GetLower:
                case NI_Vector128_AsVector128Unsafe:
                {
                    assert(ins != INS_invalid);
                    if (targetReg == intrin.op1->GetRegNum())
                    {
                        break;
                    }
                    const int byteIndex = 0;
                    GetEmitter()->emitIns_R_R_I(ins, EA_16BYTE, targetReg, intrin.op1->GetRegNum(), byteIndex);
                    break;
                }
                case NI_Vector128_GetUpper:
                {
                    assert(ins != INS_invalid);
                    const int byteIndex = 1;
                    GetEmitter()->emitIns_R_R_I(ins, EA_16BYTE, targetReg, intrin.op1->GetRegNum(), byteIndex);
                    break;
                }
                case NI_Vector256_GetUpper:
                {
                    assert(ins != INS_invalid);
                    const int byteIndex = 0x11;
                    GetEmitter()->emitIns_R_R_I(ins, EA_32BYTE, targetReg, intrin.op1->GetRegNum(), byteIndex);
                    break;
                }
                case NI_Vector128_AsVector3:
                {
                    // AsVector3 can be a no-op when it's already in the right register, otherwise
                    // we just need to move the value over. Vector3 operations will themselves mask
                    // out the upper element when it's relevant, so it's not worth us spending extra
                    // cycles doing so here.

                    assert(ins != INS_invalid);
                    if (targetReg == intrin.op1->GetRegNum())
                    {
                        break;
                    }
                    const int byteIndex = 0;
                    GetEmitter()->emitIns_R_R_I(ins, attr, targetReg, intrin.op1->GetRegNum(), byteIndex);
                    break;
                }
                case NI_Vector256_WithLower:
                case NI_Vector256_WithUpper:
                {
                    // Vector256.WithLower (this Vector256<T>, Vector128<T>)
                    //   |--- upper 128 bits from Vector256<T> ; lower 128 bits from Vector128<T> , idx =0x30
                    // Vector256.WithUpper (this Vector256<T>, Vector128<T>)
                    //   |--- lower 128 bits from Vector256<T> ; upper 128 bits from Vector128<T> , idx =0x2
                    assert((ins != INS_invalid) && (intrin.numOperands == 2) && (intrin.op1 != nullptr) && (intrin.op2 != nullptr));
                    regNumber op1Reg = intrin.op1->GetRegNum();
                    if (targetReg != op1Reg)
                    {
                        instruction ins = INS_xvbsll_v;
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, op1Reg, 0);
                    }

                    const int idx = (intrin.id == NI_Vector256_WithLower) ? 0x30 : 0x2;
                    GetEmitter()->emitIns_R_R_I(ins, attr, targetReg, intrin.op2->GetRegNum(), idx);
                    break;
                }
                case NI_LSX_VectorTableLookup:
                case NI_LASX_VectorTableLookup:
                {
                    assert(intrin.numOperands == 3);
                    assert((ins != INS_invalid) && (intrin.op1 != nullptr) && (intrin.op2 != nullptr) && (intrin.op3 != nullptr));
                    GetEmitter()->emitIns_R_R_R_R(ins, attr, targetReg, intrin.op1->GetRegNum(), intrin.op2->GetRegNum(), intrin.op3->GetRegNum());
                    break;
                }
                case NI_LSX_VectorTableLookup1:
                case NI_LASX_VectorTableLookup1:
                {
                    assert(intrin.numOperands == 3);
                    assert((ins != INS_invalid) && (intrin.op1 != nullptr) && (intrin.op2 != nullptr) && (intrin.op3 != nullptr));
                    if (targetReg != intrin.op3->GetRegNum())
                    {
                        instruction ins = INS_vbsll_v;
                        if ((intrin.id == NI_LASX_VectorTableLookup1) && (compiler->compOpportunisticallyDependsOn(InstructionSet_LASX)))
                        {
                            ins = INS_xvbsll_v;
                        }
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, intrin.op3->GetRegNum(), 0);
                    }
                    GetEmitter()->emitIns_R_R_R(ins, attr, targetReg, intrin.op1->GetRegNum(), intrin.op2->GetRegNum());
                    break;
                }
                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():SIMD.");
                    unreached();
            }
            break;
        }

        case HW_Category_Helper:
        {
            switch (intrin.id)
            {
                case NI_Vector128_GetElement:
                case NI_Vector256_GetElement:
                {
                    //This is a impSpecialIntrinsic, can recognise LSX/LASX.
                    assert(ins != INS_invalid);
                    assert(intrin.numOperands == 2);
                    assert(!intrin.op1->isContained());

                    assert(intrin.op2->OperIsConst());
                    assert(intrin.op2->isContained());

                    var_types simdType = Compiler::getSIMDTypeForSize(node->GetSimdSize());
                    if (simdType == TYP_SIMD12)
                    {
                        // op1 of TYP_SIMD12 should be considered as TYP_SIMD16
                        simdType = TYP_SIMD16;
                    }

                    ssize_t indexValue = intrin.op2->AsIntCon()->IconValue();
                    assert(indexValue < 32);
                    regNumber op1Reg = intrin.op1->GetRegNum();

                    if (!GetEmitter()->isValidVectorIndex(emitTypeSize(simdType), emitTypeSize(intrin.baseType), indexValue))
                    {
                        // We only need to generate code for the get if the index is valid
                        // If the index is invalid, previously generated for the range check will throw
                        break;
                    }
                    if ((varTypeIsFloating(intrin.baseType) && (targetReg == op1Reg) && (indexValue == 0)))
                    {
                        // no-op if vector is float/double, targetReg == op1Reg and fetching for 0th index.
                        break;
                    }

                    if ((intrin.id == NI_Vector128_GetElement) && varTypeIsFloating(intrin.baseType))
                    {
                        //TODO for LA-SIMD: use LASX to optimize if support LASX?
                        instruction ins1 = elementSize == EA_4BYTE ? INS_vpickve2gr_w : INS_vpickve2gr_d;
                        GetEmitter()->emitIns_R_R_I(ins1, elementSize, REG_R21, op1Reg, indexValue);
                        op1Reg = REG_R21;
                        indexValue = 0;
                    }

                    if (((elementSize == EA_1BYTE) && (indexValue > 15)) || ((elementSize == EA_2BYTE) && (indexValue > 7)))
                    {//GetElement of the TYP_(U)BYTE/TYP_(U)SHORT values in the upper 128bits of the 256bits-vector-reg with NI_Vector256_GetElement.
                        assert((intrin.id == NI_Vector256_GetElement) && compiler->compOpportunisticallyDependsOn(InstructionSet_LASX));
                        ssize_t index1 = elementSize == EA_1BYTE ? (indexValue / 8) : (indexValue / 4);
                        ssize_t index2 = elementSize == EA_1BYTE ? ((indexValue - 16) % 8) : ((indexValue - 8) % 4);
                        GetEmitter()->emitIns_R_R_I(INS_xvpickve_d, EA_8BYTE, REG_SCRATCH_FLT, op1Reg, index1);
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, REG_SCRATCH_FLT, index2);
                    }
                    else
                    {
                        GetEmitter()->emitIns_R_R_I(ins, elementSize, targetReg, op1Reg, indexValue);
                    }
                    break;
                }

                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():Helper.-A");
                    unreached();
            }
            break;
        }

        case HW_Category_Special:
        {
            switch (intrin.id)
            {
                case NI_LSX_ShiftRightLogicalNarrowingLowerScalar:
                {
                    assert(ins != INS_invalid);
                    GenTree* op1 = intrin.op1;
                    regNumber op1Reg = op1->GetRegNum();
                    assert(emitter::isFloatReg(op1Reg) && emitter::isIntegerRegister(targetReg));
                    assert(varTypeIsIntegral(intrin.baseType));
                    assert(intrin.op2->isContainedIntOrIImmed());
                    GetEmitter()->emitIns_R_R_I(ins, EA_16BYTE, REG_SCRATCH_FLT, op1Reg, intrin.op2->AsIntCon()->gtIconVal);
                    GetEmitter()->emitIns_R_R(INS_movfr2gr_d, EA_8BYTE, targetReg, REG_SCRATCH_FLT);
                    break;
                }
                default:
                    assert(!"TODO-LA-SIMD: genHWIntrinsic():Special.");
            }
            break;
        }

        default:
            assert(!"unimplemented yet on LA");
            //unreached();
     }

     genProduceReg(node);
}

#endif // FEATURE_HW_INTRINSICS

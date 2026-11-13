// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX             Lowering for LOONGARCH64 common code                          XX
XX                                                                           XX
XX  This encapsulates common logic for lowering trees for the LOONGARCH64    XX
XX  architectures.  For a more detailed view of what is lowering, please     XX
XX  take a look at Lower.cpp                                                 XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#ifdef TARGET_LOONGARCH64 // This file is ONLY used for LOONGARCH64 architectures

#include "jit.h"
#include "sideeffects.h"
#include "lower.h"
#include "lsra.h"

#ifdef FEATURE_HW_INTRINSICS
#include "hwintrinsic.h"
#endif

//------------------------------------------------------------------------
// IsCallTargetInRange: Can a call target address be encoded in-place?
//
// Return Value:
//    True if the addr fits into the range.
//
bool Lowering::IsCallTargetInRange(void* addr)
{
    // The CallTarget is always in range on LA64.
    // TODO-LOONGARCH64-CQ: using B/BL for optimization.
    return true;
}

//------------------------------------------------------------------------
// IsContainableImmed: Is an immediate encodable in-place?
//
// Return Value:
//    True if the immediate can be folded into an instruction,
//    for example small enough and non-relocatable.
//
bool Lowering::IsContainableImmed(GenTree* parentNode, GenTree* childNode) const
{
    if (!varTypeIsFloating(parentNode->TypeGet()))
    {
        // Make sure we have an actual immediate
        if (!childNode->IsCnsIntOrI())
            return false;
        if (childNode->AsIntCon()->ImmedValNeedsReloc(comp))
            return false;

        // TODO-CrossBitness: we wouldn't need the cast below if GenTreeIntCon::gtIconVal had target_ssize_t type.
        target_ssize_t immVal = (target_ssize_t)childNode->AsIntCon()->gtIconVal;

        switch (parentNode->OperGet())
        {
            case GT_CMPXCHG:
            case GT_XORR:
            case GT_XADD:
            case GT_XCHG:
                return (immVal == 0);

            case GT_ADD:
            case GT_EQ:
            case GT_NE:
            case GT_LT:
            case GT_LE:
            case GT_GE:
            case GT_GT:
            case GT_BOUNDS_CHECK:
                return emitter::isValidSimm12(immVal);
            case GT_AND:
            case GT_OR:
            case GT_XOR:
                return emitter::isValidUimm12(immVal);
            case GT_JCMP:
                assert(immVal == 0);
                return true;

            case GT_STORE_LCL_FLD:
            case GT_STORE_LCL_VAR:
                if (immVal == 0)
                    return true;
                break;

            default:
                break;
        }
    }

    return false;
}

//------------------------------------------------------------------------
// LowerMul: Lower a GT_MUL/GT_MULHI/GT_MUL_LONG node.
//
// Performs contaiment checks.
//
// TODO-LoongArch64-CQ: recognize GT_MULs that can be turned into MUL_LONGs,
// as those are cheaper.
//
// Arguments:
//    mul - The node to lower
//
// Return Value:
//    The next node to lower.
//
GenTree* Lowering::LowerMul(GenTreeOp* mul)
{
    assert(mul->OperIsMul());

    ContainCheckMul(mul);

    return mul->gtNext;
}

//------------------------------------------------------------------------
// Lowering::LowerJTrue: Lowers a JTRUE node.
//
// Arguments:
//    jtrue - the JTRUE node
//
// Return Value:
//    The next node to lower (usually nullptr).
//
GenTree* Lowering::LowerJTrue(GenTreeOp* jtrue)
{
    GenTree*     op = jtrue->gtGetOp1();
    GenCondition cond;
    GenTree*     cmpOp1;
    GenTree*     cmpOp2;

    if (op->OperIsCompare())
    {
        // We do not expect any other relops on LA64
        assert(op->OperIs(GT_EQ, GT_NE, GT_LT, GT_LE, GT_GE, GT_GT));

        cond = GenCondition::FromRelop(op);

        cmpOp1 = op->gtGetOp1();
        cmpOp2 = op->gtGetOp2();

        // LA64's float compare and condition-branch instructions, have
        // condition flags indicating the comparing results.
        // For LoongArch64, the floating compare result is saved to the specific register,
        // where there are 8 bits for saveing at most eight different results, that is the FCC0 ~ FCC7.
        // This is very different with the AArch64 and AMD64.
        // For AArch64 and AMD64:                       |  // For LoongArch64
        // cmp  $f1, $f2     <--just compare.           |  fcmp.cond cc,$f1,$f2  <--the condition is here.
        // branch.condition  <--the condition is here.  |  branch true or false by the cc flag.
        if (varTypeIsFloating(cmpOp1))
        {
            op->gtType = TYP_VOID;
            op->gtFlags |= GTF_SET_FLAGS;
            assert(op->OperIs(GT_EQ, GT_NE, GT_LT, GT_LE, GT_GE, GT_GT));

            jtrue->SetOper(GT_JCC);
            jtrue->AsCC()->gtCondition = GenCondition::NE; // For LA64 is only NE or EQ.
            return nullptr;
        }

        // We will fall through and turn this into a JCMP(op1, op2, kind), but need to remove the relop here.
        BlockRange().Remove(op);
    }
    else
    {
        cond = GenCondition(GenCondition::NE);

        cmpOp1 = op;
        cmpOp2 = comp->gtNewZeroConNode(cmpOp1->TypeGet());

        BlockRange().InsertBefore(jtrue, cmpOp2);

        // Fall through and turn this into a JCMP(op1, 0, NE).
    }

    // for LA64's integer compare and condition-branch instructions,
    // it's very similar to the IL instructions.
    jtrue->ChangeOper(GT_JCMP);
    jtrue->gtOp1                 = cmpOp1;
    jtrue->gtOp2                 = cmpOp2;
    jtrue->AsOpCC()->gtCondition = cond;

    if (cmpOp2->IsCnsIntOrI())
    {
        cmpOp2->SetContained();
    }

    return jtrue->gtNext;
}

//------------------------------------------------------------------------
// LowerBinaryArithmetic: lowers the given binary arithmetic node.
//
// Arguments:
//    node - the arithmetic node to lower
//
// Returns:
//    The next node to lower.
//
GenTree* Lowering::LowerBinaryArithmetic(GenTreeOp* binOp)
{
    if (comp->opts.OptimizationEnabled() && binOp->OperIs(GT_AND))
    {
        GenTree* opNode  = nullptr;
        GenTree* notNode = nullptr;
        if (binOp->gtGetOp1()->OperIs(GT_NOT))
        {
            notNode = binOp->gtGetOp1();
            opNode  = binOp->gtGetOp2();
        }
        else if (binOp->gtGetOp2()->OperIs(GT_NOT))
        {
            notNode = binOp->gtGetOp2();
            opNode  = binOp->gtGetOp1();
        }

        if (notNode != nullptr)
        {
            binOp->gtOp1 = opNode;
            binOp->gtOp2 = notNode->AsUnOp()->gtGetOp1();
            binOp->ChangeOper(GT_AND_NOT);
            BlockRange().Remove(notNode);
        }
    }

    ContainCheckBinary(binOp);

    return binOp->gtNext;
}

//------------------------------------------------------------------------
// LowerStoreLoc: Lower a store of a lclVar
//
// Arguments:
//    storeLoc - the local store (GT_STORE_LCL_FLD or GT_STORE_LCL_VAR)
//
// Notes:
//    This involves:
//    - Widening operations of unsigneds.
//
// Returns:
//   Next node to lower.
//
GenTree* Lowering::LowerStoreLoc(GenTreeLclVarCommon* storeLoc)
{
    if (storeLoc->OperIs(GT_STORE_LCL_FLD))
    {
        // We should only encounter this for lclVars that are lvDoNotEnregister.
        verifyLclFldDoNotEnregister(storeLoc->GetLclNum());
    }
    ContainCheckStoreLoc(storeLoc);
    return storeLoc->gtNext;
}

//------------------------------------------------------------------------
// LowerStoreIndir: Determine addressing mode for an indirection, and whether operands are contained.
//
// Arguments:
//    node       - The indirect store node (GT_STORE_IND) of interest
//
// Return Value:
//    Next node to lower.
//
GenTree* Lowering::LowerStoreIndir(GenTreeStoreInd* node)
{
    ContainCheckStoreIndir(node);
    return node->gtNext;
}

//------------------------------------------------------------------------
// LowerBlockStore: Set block store type
//
// Arguments:
//    blkNode       - The block store node of interest
//
// Return Value:
//    None.
//
void Lowering::LowerBlockStore(GenTreeBlk* blkNode)
{
    GenTree* dstAddr = blkNode->Addr();
    GenTree* src     = blkNode->Data();
    unsigned size    = blkNode->Size();

    if (blkNode->OperIsInitBlkOp())
    {
        if (src->OperIs(GT_INIT_VAL))
        {
            src->SetContained();
            src = src->AsUnOp()->gtGetOp1();
        }

        if ((size <= comp->getUnrollThreshold(Compiler::UnrollKind::Memset)) && src->OperIs(GT_CNS_INT))
        {
            blkNode->gtBlkOpKind = GenTreeBlk::BlkOpKindUnroll;

            // The fill value of an initblk is interpreted to hold a
            // value of (unsigned int8) however a constant of any size
            // may practically reside on the evaluation stack. So extract
            // the lower byte out of the initVal constant and replicate
            // it to a larger constant whose size is sufficient to support
            // the largest width store of the desired inline expansion.

            ssize_t fill = src->AsIntCon()->IconValue() & 0xFF;
            if (fill == 0)
            {
                src->SetContained();
            }
            else if (size >= REGSIZE_BYTES)
            {
                fill *= 0x0101010101010101LL;
                src->gtType = TYP_LONG;
            }
            else
            {
                fill *= 0x01010101;
            }
            src->AsIntCon()->SetIconValue(fill);

            ContainBlockStoreAddress(blkNode, size, dstAddr, nullptr);
        }
        else if (blkNode->IsZeroingGcPointersOnHeap())
        {
            blkNode->gtBlkOpKind = GenTreeBlk::BlkOpKindLoop;
            // We're going to use REG_R0 for zero
            src->SetContained();
        }
        else
        {
            LowerBlockStoreAsHelperCall(blkNode);
            return;
        }
    }
    else
    {
        assert(src->OperIs(GT_IND, GT_LCL_VAR, GT_LCL_FLD));
        src->SetContained();

        if (src->OperIs(GT_LCL_VAR))
        {
            // TODO-1stClassStructs: for now we can't work with STORE_BLOCK source in register.
            const unsigned srcLclNum = src->AsLclVar()->GetLclNum();
            comp->lvaSetVarDoNotEnregister(srcLclNum DEBUGARG(DoNotEnregisterReason::BlockOp));
        }

        ClassLayout* layout               = blkNode->GetLayout();
        bool         doCpObj              = layout->HasGCPtr();
        unsigned     copyBlockUnrollLimit = comp->getUnrollThreshold(Compiler::UnrollKind::Memcpy);

        if (doCpObj && (size <= copyBlockUnrollLimit))
        {
            // No write barriers are needed on the stack.
            // If the layout contains a byref, then we know it must live on the stack.
            if (blkNode->IsAddressNotOnHeap(comp))
            {
                // If the size is small enough to unroll then we need to mark the block as non-interruptible
                // to actually allow unrolling. The generated code does not report GC references loaded in the
                // temporary register(s) used for copying.
                doCpObj                  = false;
                blkNode->gtBlkOpGcUnsafe = true;
            }
        }

        // CopyObj or CopyBlk
        if (doCpObj)
        {
            // Try to use bulk copy helper
            if (TryLowerBlockStoreAsGcBulkCopyCall(blkNode))
            {
                return;
            }

            assert(dstAddr->TypeIs(TYP_BYREF, TYP_I_IMPL));
            blkNode->gtBlkOpKind = GenTreeBlk::BlkOpKindCpObjUnroll;
        }
        else if (blkNode->OperIs(GT_STORE_BLK) && (size <= copyBlockUnrollLimit))
        {
            blkNode->gtBlkOpKind = GenTreeBlk::BlkOpKindUnroll;

            if (src->OperIs(GT_IND))
            {
                ContainBlockStoreAddress(blkNode, size, src->AsIndir()->Addr(), src->AsIndir());
            }

            ContainBlockStoreAddress(blkNode, size, dstAddr, nullptr);
        }
        else
        {
            assert(blkNode->OperIs(GT_STORE_BLK));
            LowerBlockStoreAsHelperCall(blkNode);
        }
    }
}

//------------------------------------------------------------------------
// ContainBlockStoreAddress: Attempt to contain an address used by an unrolled block store.
//
// Arguments:
//    blkNode - the block store node
//    size - the block size
//    addr - the address node to try to contain
//    addrParent - the parent of addr, in case this is checking containment of the source address.
//
void Lowering::ContainBlockStoreAddress(GenTreeBlk* blkNode, unsigned size, GenTree* addr, GenTree* addrParent)

{
    assert(blkNode->OperIs(GT_STORE_BLK) && (blkNode->gtBlkOpKind == GenTreeBlk::BlkOpKindUnroll));
    assert(size < INT32_MAX);

    if (addr->OperIs(GT_LCL_ADDR) && IsContainableLclAddr(addr->AsLclFld(), size))
    {
        addr->SetContained();
        return;
    }

    if (!addr->OperIs(GT_ADD) || addr->gtOverflow() || !addr->AsOp()->gtGetOp2()->OperIs(GT_CNS_INT))
    {
        return;
    }

    GenTreeIntCon* offsetNode = addr->AsOp()->gtGetOp2()->AsIntCon();
    ssize_t        offset     = offsetNode->IconValue();

    // TODO-LoongArch64: not including the ldptr and SIMD offset which not used right now.
    if (!emitter::isValidSimm12(offset) || !emitter::isValidSimm12(offset + static_cast<int>(size)))
    {
        return;
    }

    if (!IsInvariantInRange(addr, blkNode, addrParent))
    {
        return;
    }

    BlockRange().Remove(offsetNode);

    addr->ChangeOper(GT_LEA);
    addr->AsAddrMode()->SetIndex(nullptr);
    addr->AsAddrMode()->SetScale(0);
    addr->AsAddrMode()->SetOffset(static_cast<int>(offset));
    addr->SetContained();
}

//------------------------------------------------------------------------
// LowerPutArgStk: Lower a GT_PUTARG_STK
//
// Arguments:
//    putArgNode - The node to lower
//
void Lowering::LowerPutArgStk(GenTreePutArgStk* putArgNode)
{
    GenTree* src = putArgNode->Data();

    if (src->TypeIs(TYP_STRUCT))
    {
        // STRUCT args (FIELD_LIST / OBJ) will always be contained.
        MakeSrcContained(putArgNode, src);

        // Currently, codegen does not support LCL_VAR/LCL_FLD sources, so we morph them to OBJs.
        // TODO-ADDR: support the local nodes in codegen and remove this code.
        if (src->OperIsLocalRead())
        {
            unsigned     lclNum  = src->AsLclVarCommon()->GetLclNum();
            ClassLayout* layout  = nullptr;
            GenTree*     lclAddr = nullptr;

            if (src->OperIs(GT_LCL_VAR))
            {
                layout  = comp->lvaGetDesc(lclNum)->GetLayout();
                lclAddr = comp->gtNewLclVarAddrNode(lclNum);

                comp->lvaSetVarDoNotEnregister(lclNum DEBUGARG(DoNotEnregisterReason::IsStructArg));
            }
            else
            {
                layout  = src->AsLclFld()->GetLayout();
                lclAddr = comp->gtNewLclAddrNode(lclNum, src->AsLclFld()->GetLclOffs());
            }

            src->ChangeOper(GT_BLK);
            src->AsBlk()->SetAddr(lclAddr);
            src->AsBlk()->Initialize(layout);

            BlockRange().InsertBefore(src, lclAddr);
        }

        // Codegen supports containment of local addresses under BLKs.
        if (src->OperIs(GT_BLK) && src->AsBlk()->Addr()->IsLclVarAddr() &&
            IsContainableLclAddr(src->AsBlk()->Addr()->AsLclFld(), src->AsBlk()->Size()))
        {
            // TODO-LOONGARCH64-CQ: support containment of LCL_ADDR with non-zero offset too.
            MakeSrcContained(src, src->AsBlk()->Addr());
        }
    }
}

//------------------------------------------------------------------------
// LowerCast: Lower GT_CAST(srcType, DstType) nodes.
//
// Arguments:
//    tree - GT_CAST node to be lowered
//
// Return Value:
//    None.
//
void Lowering::LowerCast(GenTree* tree)
{
    assert(tree->OperIs(GT_CAST));

    JITDUMP("LowerCast for: ");
    DISPNODE(tree);
    JITDUMP("\n");

    GenTree*  op1     = tree->AsOp()->gtOp1;
    var_types dstType = tree->CastToType();
    var_types srcType = genActualType(op1->TypeGet());

    if (varTypeIsFloating(srcType))
    {
        // Overflow casts should have been converted to helper call in morph.
        noway_assert(!tree->gtOverflow());
        // Small types should have had an intermediate int cast inserted in morph.
        assert(!varTypeIsSmall(dstType));
    }

    assert(!varTypeIsSmall(srcType));

    // Now determine if we have operands that should be contained.
    ContainCheckCast(tree->AsCast());
}

//------------------------------------------------------------------------
// LowerRotate: Lower GT_ROL and GT_ROR nodes.
//
// Arguments:
//    tree - the node to lower
//
// Return Value:
//    None.
//
void Lowering::LowerRotate(GenTree* tree)
{
    if (tree->OperIs(GT_ROL))
    {
        // Convert ROL into ROR.
        GenTree* rotatedValue        = tree->AsOp()->gtOp1;
        unsigned rotatedValueBitSize = genTypeSize(rotatedValue->gtType) * 8;
        GenTree* rotateLeftIndexNode = tree->AsOp()->gtOp2;

        if (rotateLeftIndexNode->IsCnsIntOrI())
        {
            ssize_t rotateLeftIndex                    = rotateLeftIndexNode->AsIntCon()->gtIconVal;
            ssize_t rotateRightIndex                   = rotatedValueBitSize - rotateLeftIndex;
            rotateLeftIndexNode->AsIntCon()->gtIconVal = rotateRightIndex;
        }
        else
        {
            GenTree* tmp = comp->gtNewOperNode(GT_NEG, genActualType(rotateLeftIndexNode->gtType), rotateLeftIndexNode);
            BlockRange().InsertAfter(rotateLeftIndexNode, tmp);
            tree->AsOp()->gtOp2 = tmp;
        }
        tree->ChangeOper(GT_ROR);
    }
    ContainCheckShiftRotate(tree->AsOp());
}

#ifdef FEATURE_HW_INTRINSICS
//----------------------------------------------------------------------------------------------
// Lowering::LowerHWIntrinsic: Perform containment analysis for a hardware intrinsic node.
//
//  Arguments:
//     node - The hardware intrinsic node.
//
GenTree* Lowering::LowerHWIntrinsic(GenTreeHWIntrinsic* node)
{
    if (node->TypeGet() == TYP_SIMD12)
    {
        // GT_HWINTRINSIC node requiring to produce TYP_SIMD12 in fact
        // produces a TYP_SIMD16 result
        node->gtType = TYP_SIMD16;
    }

    NamedIntrinsic intrinsicId = node->GetHWIntrinsicId();

    bool       isScalar = false;
    genTreeOps oper     = node->GetOperForHWIntrinsicId(&isScalar);

    switch (oper)
    {
        case GT_AND:
        case GT_OR:
        {
            // We want to recognize (~op1 & op2) and transform it
            // into {LAX|LASX}.AndNot(op1, op2) as well as (op1 & ~op2)
            // transforming it into {LAX|LASX}.AndNot(op2, op1)
            //
            // We want to similarly handle (~op1 | op2) and (op1 | ~op2)

            bool transform = false;

            GenTree* op1 = node->Op(1);
            GenTree* op2 = node->Op(2);

            if (op2->OperIsHWIntrinsic())
            {
                GenTreeHWIntrinsic* op2Intrin = op2->AsHWIntrinsic();

                bool       op2IsScalar = false;
                genTreeOps op2Oper     = op2Intrin->GetOperForHWIntrinsicId(&op2IsScalar);

                if (op2Oper == GT_NOT)
                {
                    assert(!op2IsScalar);
                    transform = true;

                    op2 = op2Intrin->Op(1);
                    BlockRange().Remove(op2Intrin);

                    if (oper == GT_AND)
                    {
                        std::swap(op1, op2);
                    }
                }
            }

            if (!transform && op1->OperIsHWIntrinsic())
            {
                GenTreeHWIntrinsic* opIntrin = op1->AsHWIntrinsic();

                bool       op1IsScalar = false;
                genTreeOps op1Oper     = opIntrin->GetOperForHWIntrinsicId(&op1IsScalar);

                if (op1Oper == GT_NOT)
                {
                    assert(!op1IsScalar);
                    transform = true;

                    op1 = opIntrin->Op(1);
                    BlockRange().Remove(opIntrin);

                    if (oper == GT_OR)
                    {
                        std::swap(op1, op2);
                    }
                }
            }

            if (transform)
            {
                unsigned    simdSize        = node->GetSimdSize();
                assert((simdSize == 32) || (simdSize == 16));

                if (oper == GT_AND)
                {
                    oper        = GT_AND_NOT;
                    if (simdSize == 32)
                    {
                        intrinsicId = NI_LASX_AndNot;
                    }
                    else
                    {
                        intrinsicId = NI_LSX_AndNot;
                    }
                }
                else
                {
                    assert(oper == GT_OR);
                    oper        = GT_NONE;
                    if (simdSize == 32)
                    {
                        intrinsicId = NI_LASX_OrNot;
                    }
                    else
                    {
                        intrinsicId = NI_LSX_OrNot;
                    }
                }

                node->ChangeHWIntrinsicId(intrinsicId, op1, op2);
                oper = GT_AND_NOT;
            }
            break;
        }

        default:
        {
            break;
        }
    }

    switch (intrinsicId)
    {
        case NI_Vector128_Create:
        case NI_Vector256_Create:
        case NI_Vector128_CreateScalar:
        case NI_Vector256_CreateScalar:
        {
            // We don't directly support the Vector128.Create or Vector256.Create methods in codegen
            // and instead lower them to other intrinsic nodes in LowerHWIntrinsicCreate so we expect
            // that the node is modified to either not be a HWIntrinsic node or that it is no longer
            // the same intrinsic as when it came in.

            return LowerHWIntrinsicCreate(node);
        }

        case NI_Vector128_Dot:
        case NI_Vector256_Dot:
        {
            return LowerHWIntrinsicDot(node);
        }

        case NI_Vector128_GetElement:
        case NI_Vector256_GetElement:
        {
            GenTree* op1 = node->Op(1);
            GenTree* op2 = node->Op(2);

            bool isContainableMemory = IsContainableMemoryOp(op1) && IsSafeToContainMem(node, op1);

            if (isContainableMemory || !op2->OperIsConst())
            {
                unsigned    simdSize        = node->GetSimdSize();
                CorInfoType simdBaseJitType = node->GetSimdBaseJitType();
                var_types   simdBaseType    = node->GetSimdBaseType();
                var_types   simdType        = Compiler::getSIMDTypeForSize(simdSize);

                // We're either already loading from memory or we need to since
                // we don't know what actual index is going to be retrieved.

                unsigned lclNum  = BAD_VAR_NUM;
                unsigned lclOffs = 0;

                if (!isContainableMemory)
                {
                    // We aren't already in memory, so we need to spill there

                    comp->getSIMDInitTempVarNum(simdType);
                    lclNum = comp->lvaSIMDInitTempVarNum;

                    GenTree* storeLclVar = comp->gtNewStoreLclVarNode(lclNum, op1);
                    BlockRange().InsertBefore(node, storeLclVar);
                    LowerNode(storeLclVar);
                }
                else if (op1->IsLocal())
                {
                    // We're an existing local that is loaded from memory
                    GenTreeLclVarCommon* lclVar = op1->AsLclVarCommon();

                    lclNum  = lclVar->GetLclNum();
                    lclOffs = lclVar->GetLclOffs();

                    BlockRange().Remove(op1);
                }

                if (lclNum != BAD_VAR_NUM)
                {
                    // We need to get the address of the local
                    op1 = comp->gtNewLclAddrNode(lclNum, lclOffs, TYP_BYREF);
                    BlockRange().InsertBefore(node, op1);
                    LowerNode(op1);
                }
                else
                {
                    assert(op1->isIndir());

                    // We need to get the underlying address
                    GenTree* addr = op1->AsIndir()->Addr();
                    BlockRange().Remove(op1);
                    op1 = addr;
                }

                GenTree* offset       = op2;
                unsigned baseTypeSize = genTypeSize(simdBaseType);

                if (offset->OperIsConst())
                {
                    // We have a constant index, so scale it up directly
                    GenTreeIntConCommon* index = offset->AsIntCon();
                    index->SetIconValue(index->IconValue() * baseTypeSize);
                }
                else
                {
                    // We have a non-constant index, so scale it up via mul but
                    // don't lower the GT_MUL node since the indir will try to
                    // create an addressing mode and will do folding itself. We
                    // do, however, skip the multiply for scale == 1

                    if (baseTypeSize != 1)
                    {
                        GenTreeIntConCommon* scale = comp->gtNewIconNode(baseTypeSize);
                        BlockRange().InsertBefore(node, scale);

                        offset = comp->gtNewOperNode(GT_MUL, offset->TypeGet(), offset, scale);
                        BlockRange().InsertBefore(node, offset);
                    }
                }

                // Add the offset, don't lower the GT_ADD node since the indir will
                // try to create an addressing mode and will do folding itself. We
                // do, however, skip the add for offset == 0
                GenTree* addr = op1;

                if (!offset->IsIntegralConst(0))
                {
                    addr = comp->gtNewOperNode(GT_ADD, addr->TypeGet(), addr, offset);
                    BlockRange().InsertBefore(node, addr);
                }
                else
                {
                    BlockRange().Remove(offset);
                }

                // Finally we can indirect the memory address to get the actual value
                GenTreeIndir* indir = comp->gtNewIndir(JITtype2varType(simdBaseJitType), addr);
                BlockRange().InsertBefore(node, indir);

                LIR::Use use;
                if (BlockRange().TryGetUse(node, &use))
                {
                    use.ReplaceWith(indir);
                }
                else
                {
                    indir->SetUnusedValue();
                }

                BlockRange().Remove(node);
                return LowerNode(indir);
            }

            assert(op2->OperIsConst());
            break;
        }

        case NI_Vector128_op_Equality:
        case NI_Vector256_op_Equality:
        {
            return LowerHWIntrinsicCmpOp(node, GT_EQ);
        }

        case NI_Vector128_op_Inequality:
        case NI_Vector256_op_Inequality:
        {
            return LowerHWIntrinsicCmpOp(node, GT_NE);
        }

        case NI_Vector128_WithLower:
        case NI_Vector128_WithUpper:
        {
            // Converts to equivalent managed code:
            //   LSX.Insert(vector.AsUInt64(), value.AsUInt64()).As<ulong, T>(), 0;
            // -or-
            //   LSX.Insert(vector.AsUInt64(), value.AsUInt64()).As<ulong, T>(), 1;

            int index = (intrinsicId == NI_Vector128_WithUpper) ? 1 : 0;

            GenTree* op1 = node->Op(1);
            GenTree* op2 = node->Op(2);

            GenTree* op3 = comp->gtNewIconNode(index);
            BlockRange().InsertBefore(node, op3);
            LowerNode(op3);

            node->SetSimdBaseJitType(CORINFO_TYPE_ULONG);
            node->ResetHWIntrinsicId(NI_LSX_Insert, comp, op1, op3, op2);
            break;
        }

        default:
            break;

    }

    ContainCheckHWIntrinsic(node);
    return node->gtNext;
}

//----------------------------------------------------------------------------------------------
// Lowering::IsValidConstForMovImm: Determines if the given node can be replaced by a mov/fmov immediate instruction
//
//  Arguments:
//     node - The hardware intrinsic node.
//
//  Returns:
//     true if the node can be replaced by a mov/fmov immediate instruction; otherwise, false
//
bool Lowering::IsValidConstForMovImm(GenTreeHWIntrinsic* node)
{
    assert(HWIntrinsicInfo::IsVectorCreate(node->GetHWIntrinsicId()) ||
           HWIntrinsicInfo::IsVectorCreateScalar(node->GetHWIntrinsicId()) ||
           HWIntrinsicInfo::IsVectorCreateScalarUnsafe(node->GetHWIntrinsicId()) ||
           (node->GetHWIntrinsicId() == NI_LSX_DuplicateToVector128) ||
           (node->GetHWIntrinsicId() == NI_LASX_DuplicateToVector256) ||
           (node->GetHWIntrinsicId() == NI_LSX_DuplicateToVector128) ||
           (node->GetHWIntrinsicId() == NI_LASX_DuplicateToVector256));
    assert(node->GetOperandCount() == 1);

    GenTree* const op1 = node->Op(1);

    if (op1->IsCnsIntOrI())
    {
        return true;
    }
    else if (op1->IsCnsFltOrDbl())
    {
        assert(varTypeIsFloating(node->GetSimdBaseType()));
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------------------
// Lowering::LowerHWIntrinsicCmpOp: Lowers a Vector128 or Vector256 comparison intrinsic
//
//  Arguments:
//     node  - The hardware intrinsic node.
//     cmpOp - The comparison operation, currently must be GT_EQ or GT_NE
//
GenTree* Lowering::LowerHWIntrinsicCmpOp(GenTreeHWIntrinsic* node, genTreeOps cmpOp)
{
    NamedIntrinsic intrinsicId     = node->GetHWIntrinsicId();
    CorInfoType    simdBaseJitType = node->GetSimdBaseJitType();
    var_types      simdBaseType    = node->GetSimdBaseType();
    unsigned       simdSize        = node->GetSimdSize();
    var_types      simdType        = Compiler::getSIMDTypeForSize(simdSize);

    assert((intrinsicId == NI_Vector256_op_Equality) || (intrinsicId == NI_Vector256_op_Inequality) ||
           (intrinsicId == NI_Vector128_op_Equality) || (intrinsicId == NI_Vector128_op_Inequality));

    assert(varTypeIsSIMD(simdType));
    assert(varTypeIsArithmetic(simdBaseType));
    assert(simdSize != 0);
    assert(node->TypeIs(TYP_INT));
    assert((cmpOp == GT_EQ) || (cmpOp == GT_NE));

    // We have the following (with the appropriate simd size and where the intrinsic could be op_Inequality):
    //          /--*  op2  simd
    //          /--*  op1  simd
    //   node = *  HWINTRINSIC   simd   T op_Equality

    GenTree* op1 = node->Op(1);
    GenTree* op2 = node->Op(2);

    // Optimize comparison against Vector128/256<>.Zero via AllElementsIsZero:
    //
    //   bool eq = v == Vector128/256<integer>.Zero
    //
    // to:
    //
    //   bool eq = LASX/LSX.AllElementsIsZero(v);
    //
    GenTree* op     = nullptr;
    GenTree* opZero = nullptr;
    if (op1->IsVectorZero())
    {
        op     = op2;
        opZero = op1;
    }
    else if (op2->IsVectorZero())
    {
        op     = op1;
        opZero = op2;
    }

    if (!varTypeIsFloating(simdBaseType) && (op != nullptr) && (simdSize != 12))
    {
        BlockRange().Remove(opZero);

        NamedIntrinsic newIntrinsicId = NI_Illegal;
        if (simdSize == 32)
        {
            newIntrinsicId = (cmpOp == GT_EQ) ? NI_LASX_AllElementsIsZero : NI_LASX_HasElementsNotZero;
        }
        else
        {
            newIntrinsicId = (cmpOp == GT_EQ) ? NI_LSX_AllElementsIsZero : NI_LSX_HasElementsNotZero;
        }

        node->ResetHWIntrinsicId(newIntrinsicId, op);
        node->gtType = TYP_INT;
        LowerNode(node);
        return node->gtNext;
    }

   NamedIntrinsic cmpIntrinsic;

    switch (simdBaseType)
    {
        case TYP_BYTE:
        case TYP_UBYTE:
        case TYP_SHORT:
        case TYP_USHORT:
        case TYP_INT:
        case TYP_UINT:
        case TYP_FLOAT:
        case TYP_LONG:
        case TYP_ULONG:
        case TYP_DOUBLE:
        {
            cmpIntrinsic = simdSize == 32 ? NI_LASX_CompareEqual : NI_LSX_CompareEqual;
            break;
        }

        default:
        {
            unreached();
        }
    }

    GenTree* cmp = comp->gtNewSimdHWIntrinsicNode(simdType, op1, op2, cmpIntrinsic, simdBaseJitType, simdSize);
    BlockRange().InsertBefore(node, cmp);
    LowerNode(cmp);

    if ((simdType == TYP_SIMD8) || (simdType == TYP_SIMD12))
    {
        assert(simdBaseType == TYP_FLOAT);

        // TODO for LA-SIMD: For TYP_SIMD8 we should open InstructionSet_Vector64 to optimize use fcmp.cond.s?
        //
        // For TYP_SIMD12 we don't want the upper bits to participate in the comparison.
        // So, we have to set the upper bits to all ones.
        int       idx     = simdType == TYP_SIMD8 ? 1 : 3;
        var_types setType = simdType == TYP_SIMD8 ? TYP_LONG : TYP_INT;
        simdBaseJitType   = simdType == TYP_SIMD8 ? CORINFO_TYPE_LONG : CORINFO_TYPE_INT;

        GenTree* idxCns = comp->gtNewIconNode(idx, setType);
        BlockRange().InsertAfter(cmp, idxCns);

        GenTree* insCns = comp->gtNewIconNode(-1, setType);
        BlockRange().InsertAfter(idxCns, insCns);

        GenTree* tmp = comp->gtNewSimdHWIntrinsicNode(TYP_SIMD16, cmp, idxCns, insCns, NI_LSX_Insert,
                                               simdBaseJitType, 16);
        BlockRange().InsertAfter(insCns, tmp);
        LowerNode(tmp);

        cmp = tmp;
    }

    NamedIntrinsic newIntrinsicId = NI_Illegal;
    if (simdSize == 32)
    {
        newIntrinsicId = (cmpOp == GT_EQ) ? NI_LASX_AllElementsNotZero : NI_LASX_HasElementsIsZero;
    }
    else
    {
        newIntrinsicId = (cmpOp == GT_EQ) ? NI_LSX_AllElementsNotZero : NI_LSX_HasElementsIsZero;
    }

    node->ResetHWIntrinsicId(newIntrinsicId, cmp);
    node->gtType = TYP_INT;
    LowerNode(node);
    return node->gtNext;
}

//----------------------------------------------------------------------------------------------
// Lowering::LowerHWIntrinsicCreate: Lowers a Vector128 or Vector256 Create call
//
//  Arguments:
//     node - The hardware intrinsic node.
//
GenTree* Lowering::LowerHWIntrinsicCreate(GenTreeHWIntrinsic* node)
{
    //NYI_LOONGARCH64("LowerHWIntrinsicCreate");
    NamedIntrinsic intrinsicId     = node->GetHWIntrinsicId();
    var_types      simdType        = node->TypeGet();
    CorInfoType    simdBaseJitType = node->GetSimdBaseJitType();
    var_types      simdBaseType    = node->GetSimdBaseType();
    unsigned       simdSize        = node->GetSimdSize();
    simd_t         simdVal         = {};

    if ((simdSize == 8) && (simdType == TYP_DOUBLE))
    {
        // TODO-Cleanup: Struct retyping means we have the wrong type here. We need to
        //               manually fix it up so the simdType checks below are correct.
        simdType = TYP_SIMD8;
    }

    assert(varTypeIsSIMD(simdType));
    assert(varTypeIsArithmetic(simdBaseType));
    assert(simdSize != 0);

    bool   isConstant     = GenTreeVecCon::IsHWIntrinsicCreateConstant<simd_t>(node, simdVal);
    bool   isCreateScalar = (intrinsicId == NI_Vector256_CreateScalar) || (intrinsicId == NI_Vector128_CreateScalar);
    size_t argCnt         = node->GetOperandCount();

    // Check if we have a cast that we can remove. Note that "IsValidConstForMovImm"
    // will reset Op(1) if it finds such a cast, so we do not need to handle it here.
    // TODO-Casts: why are casts from constants checked for here?
    if (isConstant && (argCnt == 1) && IsValidConstForMovImm(node))
    {
        // Set isConstant to false so we get lowered to a DuplicateToVector
        // intrinsic, which will itself mark the node as contained.
        isConstant = false;
    }

    if (isConstant)
    {
        assert((simdSize == 8) || (simdSize == 12) || (simdSize == 16) || (simdSize == 32));

        for (GenTree* arg : node->Operands())
        {
            BlockRange().Remove(arg);
        }

        GenTreeVecCon* vecCon = comp->gtNewVconNode(simdType);

        vecCon->gtSimdVal = simdVal;
        BlockRange().InsertBefore(node, vecCon);

        LIR::Use use;
        if (BlockRange().TryGetUse(node, &use))
        {
            use.ReplaceWith(vecCon);
        }
        else
        {
            vecCon->SetUnusedValue();
        }

        BlockRange().Remove(node);

        return LowerNode(vecCon);
    }
    else if (argCnt == 1)
    {
        if (isCreateScalar)
        {
            GenTree* op1 = node->Op(1);

            if (simdType == TYP_SIMD32)
            {
                switch (simdBaseType)
                {
                    case TYP_BYTE:
                    case TYP_UBYTE:
                    case TYP_SHORT:
                    case TYP_USHORT:
                    {
                        // The smallest scalar SIMD load that zeroes upper elements is 32 bits for NI_Vector256_CreateScalar, so for CreateScalar,
                        // we must ensure that the upper bits of that 32-bit value are zero if the base type is small.
                        //
                        // The most likely case is that op1 is a cast from int/long to the base type:
                        // *  CAST      int <- short <- int/long
                        // If the base type is signed, that cast will be sign-extending, but we need zero extension,
                        // so we may be able to simply retype the cast to the unsigned type of the same size.
                        // This is valid only if the cast is not checking overflow and is not containing a load.
                        //
                        // It's also possible we have a memory load of the base type:
                        // *  IND       short
                        // We can likewise change the type of the indir to force zero extension on load.
                        //
                        // If we can't safely retype one of the above patterns and don't already have a cast to the
                        // correct unsigned type, we will insert our own cast.

                        node->SetSimdBaseJitType(CORINFO_TYPE_INT);
                        var_types unsignedType = varTypeToUnsigned(simdBaseType);

                        if (op1->OperIs(GT_CAST) && !op1->gtOverflow() && !op1->AsCast()->CastOp()->isContained() &&
                            (genTypeSize(op1->CastToType()) == genTypeSize(simdBaseType)))
                        {
                            op1->AsCast()->gtCastType = unsignedType;
                        }
                        else if (op1->OperIs(GT_IND, GT_LCL_FLD) && (genTypeSize(op1) == genTypeSize(simdBaseType)))
                        {
                            op1->gtType = unsignedType;
                        }
                        else if (!op1->OperIs(GT_CAST) || (op1->AsCast()->CastToType() != unsignedType))
                        {
                            GenTree* tmp        = comp->gtNewCastNode(TYP_INT, op1, /* fromUnsigned */ false, unsignedType);
                            node->Op(1) = tmp;
                            BlockRange().InsertAfter(op1, tmp);
                            LowerNode(tmp);
                            op1 = tmp;
                        }

                        break;
                    }

                    default:
                    {
                        break;
                    }
                }
            }

            GenTree* tmp = comp->gtNewZeroConNode(simdType);
            BlockRange().InsertBefore(op1, tmp);
            LowerNode(tmp);

            GenTree* idx = comp->gtNewIconNode(0);
            BlockRange().InsertAfter(tmp, idx);
            LowerNode(idx);

            node->ResetHWIntrinsicId(simdType == TYP_SIMD32 ? NI_LASX_Insert : NI_LSX_Insert, comp, tmp, idx, op1);
            return LowerNode(node);
        }

        // We have the following (where simd is simd16 or simd32):
        //          /--*  op1  T
        //   node = *  HWINTRINSIC   simd   T Create

        // We will be constructing the following parts:
        //           /--*  op1  T
        //   node  = *  HWINTRINSIC   simd   T DuplicateToVector

        // This is roughly the following managed code:
        //   return {LSX|LASX}.DuplicateToVector(op1);

        node->ChangeHWIntrinsicId((simdType == TYP_SIMD32) ? NI_LASX_DuplicateToVector256 : NI_LSX_DuplicateToVector128);

        return LowerNode(node);
    }

    // We have the following (where simd is simd16 or simd32):
    //          /--*  op1 T
    //          +--*  ... T
    //          +--*  opN T
    //   node = *  HWINTRINSIC   simd   T Create

    // We will be constructing the following parts:
    //          /--*  op1  T
    //   tmp1 = *  HWINTRINSIC   simd16  T CreateScalarUnsafe
    //   ...

    // This is roughly the following managed code:
    //   var tmp1 = Vector128.CreateScalarUnsafe(op1);
    //   ...

    GenTree* tmp1 = InsertNewSimdCreateScalarUnsafeNode(simdType, node->Op(1), simdBaseJitType, simdSize);
    LowerNode(tmp1);

    // We will be constructing the following parts:
    //   ...
    //   idx  =    CNS_INT       int    N
    //          /--*  tmp1 simd
    //          +--*  idx  int
    //          +--*  opN  T
    //   tmp1 = *  HWINTRINSIC   simd   T Insert
    //   ...

    // This is roughly the following managed code:
    //   ...
    //   tmp1 = {LSX|LASX}.Insert(tmp1, N, opN);
    //   ...

    unsigned N   = 0;
    GenTree* opN = nullptr;
    GenTree* idx = nullptr;

    NamedIntrinsic Insert = (simdType == TYP_SIMD16) ? NI_LSX_Insert : NI_LASX_Insert;
    for (N = 1; N < argCnt - 1; N++)
    {
        opN = node->Op(N + 1);

        // Place the insert as early as possible to avoid creating a lot of long lifetimes.
        GenTree* insertionPoint = LIR::LastNode(tmp1, opN);
        idx                     = comp->gtNewIconNode(N);
        tmp1 = comp->gtNewSimdHWIntrinsicNode(simdType, tmp1, idx, opN, Insert, simdBaseJitType, simdSize);
        BlockRange().InsertAfter(insertionPoint, idx, tmp1);
        LowerNode(tmp1);
    }

    assert(N == (argCnt - 1));

    // For the last insert, we will reuse the existing node and so handle it here, outside the loop.
    opN = node->Op(argCnt);
    idx = comp->gtNewIconNode(N);
    BlockRange().InsertBefore(opN, idx);

    node->ResetHWIntrinsicId(Insert, comp, tmp1, idx, opN);

    return LowerNode(node);
}

//----------------------------------------------------------------------------------------------
// Lowering::LowerHWIntrinsicDot: Lowers a Vector64 or Vector128 Dot call
//
//  Arguments:
//     node - The hardware intrinsic node.
//
GenTree* Lowering::LowerHWIntrinsicDot(GenTreeHWIntrinsic* node)
{

    NamedIntrinsic intrinsicId     = node->GetHWIntrinsicId();
    CorInfoType    simdBaseJitType = node->GetSimdBaseJitType();
    var_types      simdBaseType    = node->GetSimdBaseType();
    unsigned       simdSize        = node->GetSimdSize();
    var_types      simdType        = Compiler::getSIMDTypeForSize(simdSize);
    unsigned       simd16Count     = comp->getSIMDVectorLength(16, simdBaseType);

    assert((intrinsicId == NI_Vector128_Dot) || (intrinsicId == NI_Vector256_Dot));
    assert(varTypeIsSIMD(simdType));
    assert(varTypeIsArithmetic(simdBaseType));
    assert(simdSize != 0);
    assert(varTypeIsSIMD(node));

    GenTree* op1 = node->Op(1);
    GenTree* op2 = node->Op(2);

    // Spare GenTrees to be used for the lowering logic below
    // Defined upfront to avoid naming conflicts, etc...
    GenTree* idx  = nullptr;
    GenTree* tmp1 = nullptr;
    GenTree* tmp2 = nullptr;
    GenTree* tmp3 = nullptr;

    if (simdSize == 12)
    {
        assert(simdBaseType == TYP_FLOAT);

        // For 12 byte SIMD, we need to clear the upper 4 bytes:
        //   idx  =    CNS_INT       int    0x03
        //   tmp1 = *  CNS_DBL       float  0.0
        //          /--*  op1  simd16
        //          +--*  idx  int
        //          +--*  tmp1 simd16
        //   op1  = *  HWINTRINSIC   simd16 T Insert
        //   ...

        // This is roughly the following managed code:
        //    op1 = LSX.Insert(op1, 0x03, 0.0f);
        //    ...

        idx = comp->gtNewIconNode(0x03, TYP_INT);
        BlockRange().InsertAfter(op1, idx);

        tmp1 = comp->gtNewZeroConNode(TYP_FLOAT);
        BlockRange().InsertAfter(idx, tmp1);

        op1 = comp->gtNewSimdHWIntrinsicNode(simdType, op1, idx, tmp1, NI_LSX_Insert, simdBaseJitType, simdSize);
        BlockRange().InsertAfter(tmp1, op1);
        LowerNode(op1);

        idx = comp->gtNewIconNode(0x03, TYP_INT);
        BlockRange().InsertAfter(op2, idx);

        tmp2 = comp->gtNewZeroConNode(TYP_FLOAT);
        BlockRange().InsertAfter(idx, tmp2);

        op2 = comp->gtNewSimdHWIntrinsicNode(simdType, op2, idx, tmp2, NI_LSX_Insert, simdBaseJitType, simdSize);
        BlockRange().InsertAfter(tmp2, op2);
        LowerNode(op2);
    }

    // We will be constructing the following parts:
    //          /--*  op1  simd16
    //          +--*  op2  simd16
    //   tmp1 = *  HWINTRINSIC   simd16 T Multiply
    //   ...
                                      
    // This is roughly the following managed code:
    //   var tmp1 = Isa.Multiply(op1, op2);
    //   ...

    NamedIntrinsic multiply = (intrinsicId == NI_Vector256_Dot) ? NI_LASX_Multiply : NI_LSX_Multiply;
    tmp2 = comp->gtNewSimdHWIntrinsicNode(simdType, op1, op2, multiply, simdBaseJitType, simdSize);
    BlockRange().InsertBefore(node, tmp2);
    LowerNode(tmp2);

    // The LA's HorizontalSum is implemented by multi-instructions within the CodeGen.
    NamedIntrinsic horizontalSum = (intrinsicId == NI_Vector256_Dot) ? NI_LASX_HorizontalSum: NI_LSX_HorizontalSum;
    tmp1 = comp->gtNewSimdHWIntrinsicNode(simdType, tmp2, horizontalSum, simdBaseJitType, simdSize);
    BlockRange().InsertAfter(tmp2, tmp1);
    LowerNode(tmp1);

    // We're producing a vector result, so just return the result directly
    LIR::Use use;

    if (BlockRange().TryGetUse(node, &use))
    {
        use.ReplaceWith(tmp2);
    }
    else
    {
        tmp2->SetUnusedValue();
    }

    BlockRange().Remove(node);
    return tmp2->gtNext;
}

#endif // FEATURE_HW_INTRINSICS

//------------------------------------------------------------------------
// Containment analysis
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// ContainCheckCallOperands: Determine whether operands of a call should be contained.
//
// Arguments:
//    call       - The call node of interest
//
// Return Value:
//    None.
//
void Lowering::ContainCheckCallOperands(GenTreeCall* call)
{
    // There are no contained operands for LoongArch64.
}

//------------------------------------------------------------------------
// ContainCheckStoreIndir: determine whether the sources of a STOREIND node should be contained.
//
// Arguments:
//    node - pointer to the node
//
void Lowering::ContainCheckStoreIndir(GenTreeStoreInd* node)
{
    GenTree* src = node->Data();
    if (!varTypeIsFloating(src->TypeGet()) && src->IsIntegralConst(0))
    {
        // an integer zero for 'src' can be contained.
        MakeSrcContained(node, src);
    }

    ContainCheckIndir(node);
}

//------------------------------------------------------------------------
// ContainCheckIndir: Determine whether operands of an indir should be contained.
//
// Arguments:
//    indirNode - The indirection node of interest
//
// Notes:
//    This is called for both store and load indirections.
//
// Return Value:
//    None.
//
void Lowering::ContainCheckIndir(GenTreeIndir* indirNode)
{
    // If this is the rhs of a block copy it will be handled when we handle the store.
    if (indirNode->TypeIs(TYP_STRUCT))
    {
        return;
    }

#ifdef FEATURE_SIMD
    //FIXME for LA-SIMD:
    // If indirTree is of TYP_SIMD12, don't mark addr as contained
    // so that it always get computed to a register.  This would
    // mean codegen side logic doesn't need to handle all possible
    // addr expressions that could be contained.
    //
    // TODO-LOONGARCH64-CQ: handle other addr mode expressions that could be marked
    // as contained.
    if (indirNode->TypeGet() == TYP_SIMD12)
    {
        return;
    }
#endif // FEATURE_SIMD

    GenTree* addr = indirNode->Addr();
    if (addr->OperIs(GT_LEA) && IsInvariantInRange(addr, indirNode))
    {
        MakeSrcContained(indirNode, addr);
    }
    else if (addr->OperIs(GT_LCL_ADDR) && IsContainableLclAddr(addr->AsLclFld(), indirNode->Size()))
    {
        // These nodes go into an addr mode:
        // - GT_LCL_ADDR is a stack addr mode.
        MakeSrcContained(indirNode, addr);
    }
}

//------------------------------------------------------------------------
// ContainCheckBinary: Determine whether a binary op's operands should be contained.
//
// Arguments:
//    node - the node we care about
//
void Lowering::ContainCheckBinary(GenTreeOp* node)
{
    // Check and make op2 contained (if it is a containable immediate)
    CheckImmedAndMakeContained(node, node->gtOp2);
}

//------------------------------------------------------------------------
// ContainCheckMul: Determine whether a mul op's operands should be contained.
//
// Arguments:
//    node - the node we care about
//
void Lowering::ContainCheckMul(GenTreeOp* node)
{
    ContainCheckBinary(node);
}

//------------------------------------------------------------------------
// ContainCheckDivOrMod: determine which operands of a div/mod should be contained.
//
// Arguments:
//    node - the node we care about
//
void Lowering::ContainCheckDivOrMod(GenTreeOp* node)
{
    assert(node->OperIs(GT_MOD, GT_UMOD, GT_DIV, GT_UDIV));
}

//------------------------------------------------------------------------
// ContainCheckShiftRotate: Determine whether a mul op's operands should be contained.
//
// Arguments:
//    node - the node we care about
//
void Lowering::ContainCheckShiftRotate(GenTreeOp* node)
{
    GenTree* shiftBy = node->gtOp2;
    assert(node->OperIsShiftOrRotate());

    if (shiftBy->IsCnsIntOrI())
    {
        MakeSrcContained(node, shiftBy);
    }
}

//------------------------------------------------------------------------
// ContainCheckStoreLoc: determine whether the source of a STORE_LCL* should be contained.
//
// Arguments:
//    node - pointer to the node
//
void Lowering::ContainCheckStoreLoc(GenTreeLclVarCommon* storeLoc) const
{
    assert(storeLoc->OperIsLocalStore());
    GenTree* op1 = storeLoc->gtGetOp1();

    if (op1->OperIs(GT_BITCAST))
    {
        // If we know that the source of the bitcast will be in a register, then we can make
        // the bitcast itself contained. This will allow us to store directly from the other
        // type if this node doesn't get a register.
        GenTree* bitCastSrc = op1->gtGetOp1();
        if (!bitCastSrc->isContained() && !bitCastSrc->IsRegOptional())
        {
            op1->SetContained();
            return;
        }
    }

    const LclVarDsc* varDsc = comp->lvaGetDesc(storeLoc);

#ifdef FEATURE_SIMD
    if (storeLoc->TypeIs(TYP_SIMD8, TYP_SIMD12))
    {
        // If this is a store to memory, we can initialize a zero vector in memory from REG_ZR.
        if ((op1->IsIntegralConst(0) || op1->IsVectorZero()) && varDsc->lvDoNotEnregister)
        {
            // For an InitBlk we want op1 to be contained
            MakeSrcContained(storeLoc, op1);
        }
        return;
    }
#endif // FEATURE_SIMD
    if (IsContainableImmed(storeLoc, op1))
    {
        MakeSrcContained(storeLoc, op1);
    }

    // If the source is a containable immediate, make it contained, unless it is
    // an int-size or larger store of zero to memory, because we can generate smaller code
    // by zeroing a register and then storing it.
    var_types type = varDsc->GetRegisterType(storeLoc);
    if (IsContainableImmed(storeLoc, op1) && (!op1->IsIntegralConst(0) || varTypeIsSmall(type)))
    {
        MakeSrcContained(storeLoc, op1);
    }
}

//------------------------------------------------------------------------
// ContainCheckCast: determine whether the source of a CAST node should be contained.
//
// Arguments:
//    node - pointer to the node
//
void Lowering::ContainCheckCast(GenTreeCast* node)
{
    // There are no contained operands for LoongArch64.
}

//------------------------------------------------------------------------
// ContainCheckCompare: determine whether the sources of a compare node should be contained.
//
// Arguments:
//    node - pointer to the node
//
void Lowering::ContainCheckCompare(GenTreeOp* cmp)
{
    CheckImmedAndMakeContained(cmp, cmp->gtOp2);
}

//------------------------------------------------------------------------
// ContainCheckSelect : determine whether the source of a select should be contained.
//
// Arguments:
//    node - pointer to the node
//
void Lowering::ContainCheckSelect(GenTreeOp* node)
{
    noway_assert(!"GT_SELECT nodes are not supported on loongarch64");
}

//------------------------------------------------------------------------
// ContainCheckBoundsChk: determine whether any source of a bounds check node should be contained.
//
// Arguments:
//    node - pointer to the node
//
void Lowering::ContainCheckBoundsChk(GenTreeBoundsChk* node)
{
    assert(node->OperIs(GT_BOUNDS_CHECK));
    if (!CheckImmedAndMakeContained(node, node->GetIndex()))
    {
        CheckImmedAndMakeContained(node, node->GetArrayLength());
    }
}

#ifdef FEATURE_HW_INTRINSICS
//----------------------------------------------------------------------------------------------
// ContainCheckHWIntrinsic: Perform containment analysis for a hardware intrinsic node.
//
//  Arguments:
//     node - The hardware intrinsic node.
//
void Lowering::ContainCheckHWIntrinsic(GenTreeHWIntrinsic* node)
{
    //FIXME for LA-SIMD: should redesign!!! qqqqq.
    //TODO for LA-SIMD: maybe subdivision and classification further!
    const HWIntrinsic intrin(node);

    bool hasImmediateOperand = HWIntrinsicInfo::HasImmediateOperand(intrin.id);

    if (HWIntrinsicInfo::MaybeSrcContained(intrin.id))
    {
        switch (intrin.numOperands)
        {
            case 4:
                assert(varTypeIsIntegral(intrin.op4));
                if (intrin.op4->IsCnsIntOrI())
                {
                    MakeSrcContained(node, intrin.op4);
                }
                break;

            case 3:
                assert(varTypeIsIntegral(intrin.op3));
                if (intrin.op3->IsCnsIntOrI())
                {
                    MakeSrcContained(node, intrin.op3);
                }
                break;

            case 2:
                assert(varTypeIsIntegral(intrin.op2));
                if (intrin.op2->IsCnsIntOrI())
                {
                    MakeSrcContained(node, intrin.op2);
                }
                break;

            default:
                unreached();
        }
    }
    else if (hasImmediateOperand || HWIntrinsicInfo::SupportsContainment(intrin.id))
    {
        switch (intrin.id)
        {
            case NI_Vector128_GetElement:
            case NI_Vector256_GetElement:
            {
                assert(!IsContainableMemoryOp(intrin.op1) || !IsSafeToContainMem(node, intrin.op1));
                assert(intrin.op2->OperIsConst());

                // Loading a constant index from register
                MakeSrcContained(node, intrin.op2);
                break;
            }
            case NI_LSX_Insert:
            case NI_LASX_Insert:
            {
                assert(hasImmediateOperand);
                assert(varTypeIsIntegral(intrin.op2) && intrin.op2->IsCnsIntOrI());
                assert(intrin.op2->AsIntCon()->gtIconVal < 32);

                MakeSrcContained(node, intrin.op2);
                break;
            }
            case NI_Vector128_CreateScalarUnsafe:
            case NI_Vector256_CreateScalarUnsafe:
            case NI_LSX_DuplicateToVector128:
            case NI_LASX_DuplicateToVector256:
            {
                if (IsValidConstForMovImm(node))
                {
                    MakeSrcContained(node, node->Op(1));
                }
                break;
            }
            case NI_LSX_CompareEqual:
            case NI_LASX_CompareEqual:
            {
                assert(!"L(A)SX_CompareEqual optimize.");
                break;
            }

            default:
                unreached();
        }
    }
}
#endif // FEATURE_HW_INTRINSICS

#endif // TARGET_LOONGARCH64

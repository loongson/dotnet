// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using LoongArchCrc = System.Runtime.Intrinsics.LoongArch.LoongArch64Base;

namespace System.IO.Hashing
{
    public partial class Crc32
    {
        private static uint UpdateScalarLoongArch64(uint crc, ReadOnlySpan<byte> source)
        {
            Debug.Assert(LoongArchCrc.IsSupported, "LOONGARCH CRC support is required.");

            // Compute in 8 byte chunks
            if (source.Length >= sizeof(ulong))
            {
                ref byte ptr = ref MemoryMarshal.GetReference(source);
                int longLength = source.Length & ~0x7; // Exclude trailing bytes not a multiple of 8

                for (int i = 0; i < longLength; i += sizeof(ulong))
                {
                    crc = LoongArchCrc.CyclicRedundancyCheckIEEE8023(crc,
                        Unsafe.ReadUnaligned<ulong>(ref Unsafe.Add(ref ptr, i)));
                }

                source = source.Slice(longLength);
            }

            // Compute remaining bytes
            for (int i = 0; i < source.Length; i++)
            {
                crc = LoongArchCrc.CyclicRedundancyCheckIEEE8023(crc, source[i]);
            }

            return crc;
        }

    }
}

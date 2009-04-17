/*
 * Copyright (C) 2008-2009  Advanced Micro Devices, Inc.
 * Copyright (C) 2008-2009  Matthias Hopf
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _R600_ID_H
#define _R600_ID_H

#include <stdint.h>
#include <stdlib.h>

enum chip_type_e {
    CHIP_TYPE_ARCH_MASK = 0xf000,
    CHIP_TYPE_ARCH_R6xx = 1 << 12,
    CHIP_TYPE_ARCH_R7xx = 2 << 12,
    
    CHIP_TYPE_R600 = CHIP_TYPE_ARCH_R6xx + 1,
    CHIP_TYPE_RV610,
    CHIP_TYPE_RV630,
    CHIP_TYPE_M72,
    CHIP_TYPE_M74,
    CHIP_TYPE_M76,
    CHIP_TYPE_RV670,
    CHIP_TYPE_M88,
    CHIP_TYPE_R680,
    CHIP_TYPE_RV620,
    CHIP_TYPE_M82,
    CHIP_TYPE_RV635,
    CHIP_TYPE_M86,
    CHIP_TYPE_RS780,
    CHIP_TYPE_RV770 = CHIP_TYPE_ARCH_R7xx + 1,
    CHIP_TYPE_R700,
    CHIP_TYPE_M98,
    CHIP_TYPE_RV730,
    CHIP_TYPE_M96,
    CHIP_TYPE_RV710,
};

enum chip_flag_e {
    CHIP_FLAG_MOBILITY = 1 << 0,
    CHIP_FLAG_VERIFIED = 1 << 16,
} ;

typedef struct chip_s {
    uint16_t  id;
    uint16_t  type;
    char     *name;
    uint32_t  flags;
} chip_t;

extern chip_t r600_chips[];

#endif /* _R600_ID_H */

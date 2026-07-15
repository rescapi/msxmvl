// msxmvl clean-room reimplementation of MSXgl "fixed_point" module.
// Public names + signatures match MSXgl exactly (drop-in replacement).
// Derived from MSXgl engine/src/fixed_point.h (interface only).
//
// The module is almost entirely compile-time macros (Qm.n IDs, SET/GET/FRAC
// helpers). Only four runtime functions exist: QMN_Set8/Get8/Set16/Get16.
// The MSXgl header declares them `inline`; here they are ordinary extern
// functions implemented in fixed_point.c so a difftest driver can link against
// either build. No function declares a register-preservation contract.
#ifndef MSXMVL_FIXED_POINT_H
#define MSXMVL_FIXED_POINT_H

#include "types.h"

//=============================================================================
// DEFINES
//=============================================================================

// Macro: Q - Generate a fixed point number ID
#define Q(m, n)						(((m) << 8) + (n))

//-----------------------------------------------------------------------------
// 8-BIT FIXED POINT
//-----------------------------------------------------------------------------

// Q8.0 [-128:127]
#define Q8_0						Q(8, 0)
#define Q8_0_TYPE					i8
#define Q8_0_SET(a)					(i8)(a)
#define Q8_0_GET(a)					(i8)(a)
#define Q8_0_FRAC(a)				0

// Q7.1 [-64:63.5]
#define Q7_1						Q(7, 1)
#define Q7_1_TYPE					i8
#define Q7_1_SET(a)					(i8)((a) * 2)
#define Q7_1_GET(a)					(i8)((a) / 2)
#define Q7_1_FRAC(a)				((a) >= 0 ? (a) & 0x01 : -(-(a) & 0x01))

// Q6.2 [-32:31.75]
#define Q6_2						Q(6, 2)
#define Q6_2_TYPE					i8
#define Q6_2_SET(a)					(i8)((a) * 4)
#define Q6_2_GET(a)					(i8)((a) / 4)
#define Q6_2_FRAC(a)				(((a) >= 0) ? (a) & 0x03 : -(-(a) & 0x03))

// Q5.3 [-16:15.87]
#define Q5_3						Q(5, 3)
#define Q5_3_TYPE					i8
#define Q5_3_SET(a)					(i8)((a) * 8)
#define Q5_3_GET(a)					(i8)((a) / 8)
#define Q5_3_FRAC(a)				(((a) >= 0) ? (a) & 0x07 : -(-(a) & 0x07))

// Q4.4 [-8:7.94]
#define Q4_4						Q(4, 4)
#define Q4_4_TYPE					i8
#define Q4_4_SET(a)					(i8)((a) * 16)
#define Q4_4_GET(a)					(i8)((a) / 16)
#define Q4_4_FRAC(a)				((a) >= 0 ? (a) & 0x0F : -(-(a) & 0x0F))

// Q3.5 [-4:3.97]
#define Q3_5						Q(3, 5)
#define Q3_5_TYPE					i8
#define Q3_5_SET(a)					(i8)((a) * 32)
#define Q3_5_GET(a)					(i8)((a) / 32)
#define Q3_5_FRAC(a)				(((a) >= 0) ? (a) & 0x1F : -(-(a) & 0x1F))

// Q2.6 [-2:1.98]
#define Q2_6						Q(2, 6)
#define Q2_6_TYPE					i8
#define Q2_6_SET(a)					(i8)((a) * 64)
#define Q2_6_GET(a)					(i8)((a) / 64)
#define Q2_6_FRAC(a)				(((a) >= 0) ? (a) & 0x3F : -(-(a) & 0x3F))

// Q1.7 [-1:0.99]
#define Q1_7						Q(1, 7)
#define Q1_7_TYPE					i8
#define Q1_7_SET(a)					(i8)((a) * 128)
#define Q1_7_GET(a)					(i8)((a) / 128)
#define Q1_7_FRAC(a)				(((a) >= 0) ? (a) & 0x7F : -(-(a) & 0x7F))

// Function: QMN_Set8
// Convert source unit 8-bit value to a Qm.n fixed point number.
//   q - Fixed point number ID ; a - Source unit value ; Return: Qm.n number
i8 QMN_Set8(u16 q, i8 a);

// Function: QMN_Get8
// Convert a Qm.n fixed point number to source unit 8-bit value.
//   q - Fixed point number ID ; a - Qm.n number ; Return: Source unit value
i8 QMN_Get8(u16 q, i8 a);

//-----------------------------------------------------------------------------
// 16-BIT FIXED POINT
//-----------------------------------------------------------------------------

// Q16.0 [-32768:32767]
#define Q16_0						Q(16, 0)
#define Q16_0_TYPE					i16
#define Q16_0_SET(a)				(i16)(a)
#define Q16_0_GET(a)				(i16)(a)
#define Q16_0_FRAC(a)				0

// Q15.1 [-16384:16383.5]
#define Q15_1						Q(15, 1)
#define Q15_1_TYPE					i16
#define Q15_1_SET(a)				(i16)((a) * 2)
#define Q15_1_GET(a)				(i16)((a) / 2)
#define Q15_1_FRAC(a)				(((a) >= 0) ? (a) & 0x0001 : -(-(a) & 0x0001))

// Q14.2 [-8192:8191.75]
#define Q14_2						Q(14, 2)
#define Q14_2_TYPE					i16
#define Q14_2_SET(a)				(i16)((a) * 4)
#define Q14_2_GET(a)				(i16)((a) / 4)
#define Q14_2_FRAC(a)				(((a) >= 0) ? (a) & 0x0003 : -(-(a) & 0x0003))

// Q13.3 [-4096:4095.87]
#define Q13_3						Q(13, 3)
#define Q13_3_TYPE					i16
#define Q13_3_SET(a)				(i16)((a) * 8)
#define Q13_3_GET(a)				(i16)((a) / 8)
#define Q13_3_FRAC(a)				(((a) >= 0) ? (a) & 0x0007 : -(-(a) & 0x0007))

// Q12.4 [-2048:2047.94]
#define Q12_4						Q(12, 4)
#define Q12_4_TYPE					i16
#define Q12_4_SET(a)				(i16)((a) * 16)
#define Q12_4_GET(a)				(i16)((a) / 16)
#define Q12_4_FRAC(a)				(((a) >= 0) ? (a) & 0x000F : -(-(a) & 0x000F))

// Q11.5 [-1024:1023.97]
#define Q11_5						Q(11, 5)
#define Q11_5_TYPE					i16
#define Q11_5_SET(a)				(i16)((a) * 32)
#define Q11_5_GET(a)				(i16)((a) / 32)
#define Q11_5_FRAC(a)				(((a) >= 0) ? (a) & 0x001F : -(-(a) & 0x001F))

// Q10.6 [-512:511.98]
#define Q10_6						Q(10, 6)
#define Q10_6_TYPE					i16
#define Q10_6_SET(a)				(i16)((a) * 64)
#define Q10_6_GET(a)				(i16)((a) / 64)
#define Q10_6_FRAC(a)				(((a) >= 0) ? (a) & 0x003F : -(-(a) & 0x003F))

// Q9.7 [-256:255.99]
#define Q9_7						Q(9, 7)
#define Q9_7_TYPE					i16
#define Q9_7_SET(a)					(i16)((a) * 128)
#define Q9_7_GET(a)					(i16)((a) / 128)
#define Q9_7_FRAC(a)				(((a) >= 0) ? (a) & 0x007F : -(-(a) & 0x007F))

// Q8.8 [-128:127.99]
#define Q8_8						Q(8, 8)
#define Q8_8_TYPE					i16
#define Q8_8_SET(a)					(i16)((a) * 256)
#define Q8_8_GET(a)					(i16)((a) / 256)
#define Q8_8_FRAC(a)				(((a) >= 0) ? (a) & 0x00FF : -(-(a) & 0x00FF))

// Q7.9 [-64:63.99]
#define Q7_9						Q(7, 9)
#define Q7_9_TYPE					i16
#define Q7_9_SET(a)					(i16)((a) * 512)
#define Q7_9_GET(a)					(i16)((a) / 512)
#define Q7_9_FRAC(a)				(((a) >= 0) ? (a) & 0x01FF : -(-(a) & 0x01FF))

// Q6.10 [-32:31.99]
#define Q6_10						Q(6, 10)
#define Q6_10_TYPE					i16
#define Q6_10_SET(a)				(i16)((a) * 1024)
#define Q6_10_GET(a)				(i16)((a) / 1024)
#define Q6_10_FRAC(a)				(((a) >= 0) ? (a) & 0x03FF : -(-(a) & 0x03FF))

// Q5.11 [-16:15.99]
#define Q5_11						Q(5, 11)
#define Q5_11_TYPE					i16
#define Q5_11_SET(a)				(i16)((a) * 2048)
#define Q5_11_GET(a)				(i16)((a) / 2048)
#define Q5_11_FRAC(a)				(((a) >= 0) ? (a) & 0x07FF : -(-(a) & 0x07FF))

// Q4.12 [-8:7.99]
#define Q4_12						Q(4, 12)
#define Q4_12_TYPE					i16
#define Q4_12_SET(a)				(i16)((a) * 4096)
#define Q4_12_GET(a)				(i16)((a) / 4096)
#define Q4_12_FRAC(a)				(((a) >= 0) ? (a) & 0x0FFF : -(-(a) & 0x0FFF))

// Q3.13 [-4:3.99]
#define Q3_13						Q(3, 13)
#define Q3_13_TYPE					i16
#define Q3_13_SET(a)				(i16)((a) * 8192)
#define Q3_13_GET(a)				(i16)((a) / 8192)
#define Q3_13_FRAC(a)				(((a) >= 0) ? (a) & 0x1FFF : -(-(a) & 0x1FFF))

// Q2.14 [-2:1.99]
#define Q2_14						Q(2, 14)
#define Q2_14_TYPE					i16
#define Q2_14_SET(a)				(i16)((a) * 16384)
#define Q2_14_GET(a)				(i16)((a) / 16384)
#define Q2_14_FRAC(a)				(((a) >= 0) ? (a) & 0x3FFF : -(-(a) & 0x3FFF))

// Q1.15 [-1:0.99]
#define Q1_15						Q(1, 15)
#define Q1_15_TYPE					i16
#define Q1_15_SET(a)				(i16)((a) * 32768)
#define Q1_15_GET(a)				(i16)((a) / 32768)
#define Q1_15_FRAC(a)				(((a) >= 0) ? (a) & 0x7FFF : -(-(a) & 0x7FFF))

// Function: QMN_Set16
// Convert source unit 16-bit value to a Qm.n fixed point number.
//   q - Fixed point number ID ; a - Source unit value ; Return: Qm.n number
i16 QMN_Set16(u16 q, i16 a);

// Function: QMN_Get16
// Convert a Qm.n fixed point number to source unit 16-bit value.
//   q - Fixed point number ID ; a - Qm.n number ; Return: Source unit value
i16 QMN_Get16(u16 q, i16 a);

#endif // MSXMVL_FIXED_POINT_H

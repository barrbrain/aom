#if !defined(_odintrin_H)
# define _odintrin_H (1)

#include "vp10/common/enums.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_ports/bitops.h"

/*Smallest blocks are 4x4*/
# define OD_LOG_BSIZE0 (2)
/*There are 5 block sizes total (4x4, 8x8, 16x16, 32x32 and 64x64).*/
# define OD_NBSIZES    (5)
/*The log of the maximum length of the side of a block.*/
# define OD_LOG_BSIZE_MAX (OD_LOG_BSIZE0 + OD_NBSIZES - 1)
/*The maximum length of the side of a block.*/
# define OD_BSIZE_MAX     (1 << OD_LOG_BSIZE_MAX)

typedef int od_coeff;

typedef int16_t dering_in;

#define OD_DIVU_SMALL(_x, _d) ((_x) / (_d))

#define OD_MINI VPXMIN
#define OD_CLAMPI(min, val, max) clamp((val), (min), (max))

/*Enable special features for gcc and compatible compilers.*/
# if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#  define OD_GNUC_PREREQ(maj, min, pat)                             \
  ((__GNUC__ << 16) + (__GNUC_MINOR__ << 8) + __GNUC_PATCHLEVEL__ >= ((maj) << 16) + ((min) << 8) + pat)
# else
#  define OD_GNUC_PREREQ(maj, min, pat) (0)
# endif

int od_ilog(uint32_t _v);

/*Count leading zeros.
  This macro should only be used for implementing od_ilog(), if it is defined.
  All other code should use OD_ILOG() instead.*/
# if defined(_MSC_VER)
#  include <intrin.h>
#  if !defined(snprintf)
#   define snprintf _snprintf
#  endif
/*In _DEBUG mode this is not an intrinsic by default.*/
#  pragma intrinsic(_BitScanReverse)

static __inline int od_bsr(unsigned long x) {
  unsigned long ret;
  _BitScanReverse(&ret, x);
  return (int)ret;
}
#  define OD_CLZ0 (1)
#  define OD_CLZ(x) (-od_bsr(x))
# elif defined(ENABLE_TI_DSPLIB)
#  include "dsplib.h"
#  define OD_CLZ0 (31)
#  define OD_CLZ(x) (_lnorm(x))
# elif OD_GNUC_PREREQ(3, 4, 0)
#  if INT_MAX >= 2147483647
#   define OD_CLZ0 ((int)sizeof(unsigned)*CHAR_BIT)
#   define OD_CLZ(x) (__builtin_clz(x))
#  elif LONG_MAX >= 2147483647L
#   define OD_CLZ0 ((int)sizeof(unsigned long)*CHAR_BIT)
#   define OD_CLZ(x) (__builtin_clzl(x))
#  endif
# endif
# if defined(OD_CLZ)
#  define OD_ILOG_NZ(x) (OD_CLZ0 - OD_CLZ(x))
/*Note that __builtin_clz is not defined when x == 0, according to the gcc
   documentation (and that of the x86 BSR instruction that implements it), so
   we have to special-case it.
  We define a special version of the macro to use when x can be zero.*/
#  define OD_ILOG(x) (OD_ILOG_NZ(x) & -!!(x))
# else
#  define OD_ILOG_NZ(x) (od_ilog(x))
#  define OD_ILOG(x) (od_ilog(x))
# endif

#endif

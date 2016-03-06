#if !defined(_odintrin_H)
# define _odintrin_H (1)

#include <limits.h>
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

/*Enable special features for gcc and compatible compilers.*/
# if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#  define OD_GNUC_PREREQ(maj, min, pat)                             \
  ((__GNUC__ << 16) + (__GNUC_MINOR__ << 8) + __GNUC_PATCHLEVEL__ >= ((maj) << 16) + ((min) << 8) + pat)
# else
#  define OD_GNUC_PREREQ(maj, min, pat) (0)
# endif

#if OD_GNUC_PREREQ(4, 0, 0)
# pragma GCC visibility push(default)
#endif

#if OD_GNUC_PREREQ(3, 4, 0)
# define OD_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#else
# define OD_WARN_UNUSED_RESULT
#endif

#if OD_GNUC_PREREQ(3, 4, 0)
# define OD_ARG_NONNULL(x) __attribute__((__nonnull__(x)))
#else
# define OD_ARG_NONNULL(x)
#endif

/*Modern gcc (4.x) can compile the naive versions of min and max with cmov if
   given an appropriate architecture, but the branchless bit-twiddling versions
   are just as fast, and do not require any special target architecture.
  Earlier gcc versions (3.x) compiled both code to the same assembly
   instructions, because of the way they represented ((b) > (a)) internally.*/
/*#define OD_MAXI(a, b) ((a) < (b) ? (b) : (a))*/
# define OD_MAXI(a, b) ((a) ^ (((a) ^ (b)) & -((b) > (a))))
/*#define OD_MINI(a, b) ((a) > (b) ? (b) : (a))*/
# define OD_MINI(a, b) ((a) ^ (((b) ^ (a)) & -((b) < (a))))
/*This has a chance of compiling branchless, and is just as fast as the
   bit-twiddling method, which is slightly less portable, since it relies on a
   sign-extended rightshift, which is not guaranteed by ANSI (but present on
   every relevant platform).*/
# define OD_SIGNI(a) (((a) > 0) - ((a) < 0))
/*Slightly more portable than relying on a sign-extended right-shift (which is
   not guaranteed by ANSI), and just as fast, since gcc (3.x and 4.x both)
   compile it into the right-shift anyway.*/
# define OD_SIGNMASK(a) (-((a) < 0))
/*Unlike copysign(), simply inverts the sign of a if b is negative.*/
# define OD_FLIPSIGNI(a, b) (((a) + OD_SIGNMASK(b)) ^ OD_SIGNMASK(b))
# define OD_COPYSIGNI(a, b) OD_FLIPSIGNI(abs(a), b)
/*Clamps an integer into the given range.
  If a > c, then the lower bound a is respected over the upper bound c (this
   behavior is required to meet our documented API behavior).
  a: The lower bound.
  b: The value to clamp.
  c: The upper boud.*/
# define OD_CLAMPI(a, b, c) (OD_MAXI(a, OD_MINI(b, c)))

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

# if defined(OD_ENABLE_ASSERTIONS)
#  if OD_GNUC_PREREQ(2, 5, 0)
__attribute__((noreturn))
#  endif
void od_fatal_impl(const char *_str, const char *_file, int _line);

#  define OD_FATAL(_str) (od_fatal_impl(_str, __FILE__, __LINE__))

#  define OD_ASSERT(_cond) \
  do { \
    if (!(_cond)) { \
      OD_FATAL("assertion failed: " # _cond); \
    } \
  } \
  while (0)

#  define OD_ASSERT2(_cond, _message) \
  do { \
    if (!(_cond)) { \
      OD_FATAL("assertion failed: " # _cond "\n" _message); \
    } \
  } \
  while (0)

#  define OD_ALWAYS_TRUE(_cond) OD_ASSERT(_cond)

# else
#  define OD_ASSERT(_cond)
#  define OD_ASSERT2(_cond, _message)
#  define OD_ALWAYS_TRUE(_cond) ((void)(_cond))
# endif


/** Copy n elements of memory from src to dst. The 0* term provides
    compile-time type checking  */
#if !defined(OVERRIDE_OD_COPY)
# define OD_COPY(dst, src, n) \
  (memcpy((dst), (src), sizeof(*(dst))*(n) + 0*((dst) - (src))))
#endif

/** Copy n elements of memory from src to dst, allowing overlapping regions.
    The 0* term provides compile-time type checking */
#if !defined(OVERRIDE_OD_MOVE)
# define OD_MOVE(dst, src, n) \
 (memmove((dst), (src), sizeof(*(dst))*(n) + 0*((dst) - (src)) ))
#endif

#endif

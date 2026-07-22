
#ifndef BSONCXX_ABI_EXPORT_H
#define BSONCXX_ABI_EXPORT_H

#ifdef BSONCXX_STATIC
#  define BSONCXX_ABI_EXPORT
#  define BSONCXX_ABI_NO_EXPORT
#else
#  ifndef BSONCXX_ABI_EXPORT
#    ifdef BSONCXX_EXPORT
        /* We are building this library */
#      define BSONCXX_ABI_EXPORT 
#    else
        /* We are using this library */
#      define BSONCXX_ABI_EXPORT 
#    endif
#  endif

#  ifndef BSONCXX_ABI_NO_EXPORT
#    define BSONCXX_ABI_NO_EXPORT 
#  endif
#endif

#ifndef BSONCXX_DEPRECATED
#  define BSONCXX_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef BSONCXX_DEPRECATED_EXPORT
#  define BSONCXX_DEPRECATED_EXPORT BSONCXX_ABI_EXPORT BSONCXX_DEPRECATED
#endif

#ifndef BSONCXX_DEPRECATED_NO_EXPORT
#  define BSONCXX_DEPRECATED_NO_EXPORT BSONCXX_ABI_NO_EXPORT BSONCXX_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef BSONCXX_ABI_NO_DEPRECATED
#    define BSONCXX_ABI_NO_DEPRECATED
#  endif
#endif

#undef BSONCXX_DEPRECATED_EXPORT
#undef BSONCXX_DEPRECATED_NO_EXPORT

#if defined(_MSC_VER)
#define BSONCXX_ABI_CDECL __cdecl
#else
#define BSONCXX_ABI_CDECL
#endif

#define BSONCXX_ABI_EXPORT_CDECL(...) BSONCXX_ABI_EXPORT __VA_ARGS__ BSONCXX_ABI_CDECL

#endif /* BSONCXX_ABI_EXPORT_H */

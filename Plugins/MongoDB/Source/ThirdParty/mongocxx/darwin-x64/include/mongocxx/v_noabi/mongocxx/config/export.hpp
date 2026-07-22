
#ifndef MONGOCXX_ABI_EXPORT_H
#define MONGOCXX_ABI_EXPORT_H

#ifdef MONGOCXX_STATIC
#  define MONGOCXX_ABI_EXPORT
#  define MONGOCXX_ABI_NO_EXPORT
#else
#  ifndef MONGOCXX_ABI_EXPORT
#    ifdef MONGOCXX_EXPORTS
        /* We are building this library */
#      define MONGOCXX_ABI_EXPORT 
#    else
        /* We are using this library */
#      define MONGOCXX_ABI_EXPORT 
#    endif
#  endif

#  ifndef MONGOCXX_ABI_NO_EXPORT
#    define MONGOCXX_ABI_NO_EXPORT 
#  endif
#endif

#ifndef MONGOCXX_DEPRECATED
#  define MONGOCXX_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef MONGOCXX_DEPRECATED_EXPORT
#  define MONGOCXX_DEPRECATED_EXPORT MONGOCXX_ABI_EXPORT MONGOCXX_DEPRECATED
#endif

#ifndef MONGOCXX_DEPRECATED_NO_EXPORT
#  define MONGOCXX_DEPRECATED_NO_EXPORT MONGOCXX_ABI_NO_EXPORT MONGOCXX_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef MONGOCXX_ABI_NO_DEPRECATED
#    define MONGOCXX_ABI_NO_DEPRECATED
#  endif
#endif

#undef MONGOCXX_DEPRECATED_EXPORT
#undef MONGOCXX_DEPRECATED_NO_EXPORT

#if defined(_MSC_VER)
#define MONGOCXX_ABI_CDECL __cdecl
#else
#define MONGOCXX_ABI_CDECL
#endif

#define MONGOCXX_ABI_EXPORT_CDECL(...) MONGOCXX_ABI_EXPORT __VA_ARGS__ MONGOCXX_ABI_CDECL

#endif /* MONGOCXX_ABI_EXPORT_H */

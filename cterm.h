#ifndef CTERM_H
#define CTERM_H

#ifndef CTERM_API
  #ifdef CTERM_STATIC
    #define CTERM_API static
  #else
    #define CTERM_API extern
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

CTERM_API int  cterm_init(void);     /* 0 on success, -1 on failure */
CTERM_API void cterm_shutdown(void);
CTERM_API int  cterm_read(unsigned char *buf, int cap); /* bytes read, 0 if none */

#ifdef __cplusplus
}
#endif
#endif /* CTERM_H */

#ifdef CTERM_IMPLEMENTATION
#ifndef CTERM_IMPLEMENTATION_DONE
#define CTERM_IMPLEMENTATION_DONE

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
#else
  #include <termios.h>
  #include <unistd.h>
#endif

#if defined(_WIN32)
/* TODO: Windows backend */
CTERM_API int  cterm_init(void) { return -1; }
CTERM_API void cterm_shutdown(void) {}
CTERM_API int  cterm_read(unsigned char *buf, int cap) { (void)buf; (void)cap; return 0; }
#else
/* TODO: POSIX backend */
CTERM_API int  cterm_init(void) { return -1; }
CTERM_API void cterm_shutdown(void) {}
CTERM_API int  cterm_read(unsigned char *buf, int cap) { (void)buf; (void)cap; return 0; }
#endif

#endif /* CTERM_IMPLEMENTATION_DONE */
#endif /* CTERM_IMPLEMENTATION */

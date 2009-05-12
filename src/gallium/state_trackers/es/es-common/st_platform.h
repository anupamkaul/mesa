#ifndef ST_PLATFORM_H
#define ST_PLATFORM_H

/* The irregular use of GL_API, GL_APIENTRY, APIENTRY, and other
 * similar macros is making the build difficult, particularly on
 * Windows.
 *
 * One particular issue is that GL_API has two different meanings,
 * depending on whether you intend to use a header to create
 * a library by defining its entry points, or whether you
 * are using the header in the more traditional sense, to allow
 * access and use of those same functions.  Unfortunately for
 * us, we *both* intend to create GLES entry points, and *also*
 * use OpenGL/Mesa entry points.  Since we're both importing
 * and exporting, GL_API is broken.
 *
 * APIENTRY, GLAPIENTRY, and GL_APIENTRY are broken for logistic
 * reasons; some are defined on some platforms, some not, and 
 * they're inconsistently used.
 *
 * This header will be used for the time being; it defines *three*
 * macros, one for functions being imported, one for functions being
 * exported, and one for call protocol.
 *
 * At some time, all the uses of these macros and their GL_*
 * counterparts should be examined and unified.
 */

#if defined(WIN32) || defined(_WIN32_WCE)

/*#define ST_IMPORT	__declspec(dllimport)*/
#define ST_IMPORT
#define ST_EXPORT	__declspec(dllexport)
#if defined(UNDER_CE)
#define ST_APIENTRY
#else
#define ST_APIENTRY __stdcall
#endif

#else

#define ST_IMPORT	extern
#define ST_EXPORT
#define ST_APIENTRY

#endif

#endif /* ST_PLATFORM_H */

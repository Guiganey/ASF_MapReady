/*Standard ASF Utility include file.*/
#ifndef __ASF_H
#define __ASF_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "caplib.h"
#include "asf_meta.h"    /* For get_(data)_line(s) and put_(data)_line(s) */
#include "error.h"
#include "log.h"
#include "cla.h"
#include "asf_complex.h" /* For get_complexFloat_line(s) and
                          * put_complexFloat_line(s) */

#include "asf_version.h"

#ifndef PI
# define PI 3.14159265358979323846
#endif
#ifndef M_PI
# define M_PI PI
#endif
#define D2R (PI/180.0)
#define R2D (180.0/PI)

#ifndef FALSE
# define FALSE (0)
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

#if defined(win32)
#define DIR_SEPARATOR '\\'
#else
#define DIR_SEPARATOR '/'
#endif

#define EXTENSION_SEPARATOR '.'


/* Print an error with printf-style formatting codes and args, then die.  */
void bail(const char *message, ...)/* ; is coming, don't worry.  */
/* The GNU C compiler can give us some special help if compiling with
   -Wformat or a warning option that implies it.  */
#ifdef __GNUC__
     /* Function attribute format says function is variadic a la
        printf, with argument 1 its format spec and argument 2 its
        first optional argument corresponding to the spec.  Function
        attribute noreturn says function doesn't return.  */
     __attribute__ ((format (printf, 1, 2), noreturn))
#endif
; /* <-- Semicolon for bail prototype.  */

/******************************************************************************
 * Deprecated program timing routines.  There are better ways (like
 * looking at yor watch even) to time programs than using these
 * routines.  Call StartWatch(Log) at the beginning, and then call
 * StopWatch(Log) at the end to automaticly print out some information
 * about how much CPU time the program took to standard output or to a
 * log file.  If the program takes more than 72 minutes, the reported
 * time may be totally wrong on some systems, due to clock wrap
 * around.  So its probably best not to use these in new code
 * (consider them deprecated).  */
void StartWatch(void);
void StopWatch(void);
void StartWatchLog(FILE *log_fp);
void StopWatchLog(FILE *log_fp);
char* date_stamp(void);
char* date_time_stamp(void);

/******************************************************************************
 * FileUtil:
 * A collection of file name and I/O utilities. Implemented * in
 * asf.a/fileUtil.c */

/* Return a pointer into string name pointing to the dot ('.')
   character in the trailing dot extension, or a NULL pointer if name
   doesn't include any dots.  */
char *findExt(char *name);
/* The maximum allowable length in characters (not including trailing
   null character) of result strings from the appendExt routine.  */
#define MAX_APPENDEXT_RESULT_STRING_LENGTH 255
/* Deprecated: use create_name and preallocated memory instead.  This
   function is badly misnamed.  What it actuall does: First, if newExt
   is NULL (not an empty string, but a NULL pointer, return a new copy
   of name.  Otherwise, return in new memory a string consisting of a
   copy of name with the rightmost dot extension, if present, replaced
   with newExt.  If no dot extension exists originally, the new
   extension is appended.  */
char *appendExt(const char *name,const char *newExt);
/* Return true iff file name exists and is readable.  */
int fileExists(const char *name);
/* Return true iff fileExists(appendExt(name, newExt)) would return
   true, but without the memory leak fileExists(appendExt(name,
   newExt)) would produce. */
int extExists(const char *name,const char *newExt);

void append_ext_if_needed(char *file_name, const char *newExt, 
                          const char *alsoAllowedExt);

/* Creates name out by clipping off the rightmost dot extension, if
   any, of in, and then appending newExt.  out must point to existing
   memory large enough to store the result.  */
void create_name(char *out,const char *in,const char *newExt);

/* Takes a string and fills one pre-allocated array with any path prior to
   the file name, and fills another pre-allocated array with the file name */
void split_dir_and_file(const char *inString, char *dirName, char *fileName);

#define PREPENDED_EXTENSION -1
#define APPENDED_EXTENSION   1
/* Fills 'extension' with the file's extension and returns TRUE if there is
   indeed an extension. Otherwise it returns FALSE and fills 'extension' with
   an empty string. If 'side' is PREPENDED_EXTENSION, then it looks for the
   extension at the front of the file name. If 'side' is APPENDED_EXTENSION, it
   looks for the extension at the end of the file name. Otherwise it returns
   FALSE and fills 'extension' with an empty string. The extension separator
   is a '.' It assumes 'fileName' is only the file name (no path included) */
int split_base_and_ext(char *fileName,int side,char *baseName,char *extension);

/* first tries to open the given image name, then appends ".img" and tries again
   It returns a pointer to the opened file.*/
FILE *fopenImage(const char *name,const char *accessType);

/* Copy the file specified by "src" to the file specified by "dst". */
void fileCopy(const char *src, const char *dst);


/******************************************************************************
 * ioLine: Grab any data type and fill a buffer of _type_ data.
 * Assumes that the datae file contains data in big endian order and
 * returns data in host byte order. Implemented in asf.a/ioLine.c */

/* Size of line chunk to read or write.  */
#define CHUNK_OF_LINES 32

int get_float_line(FILE *file, meta_parameters *meta, int line_number,
		float *dest);
int get_float_lines(FILE *file, meta_parameters *meta, int line_number,
		int num_lines_to_get, float *dest);
int get_double_line(FILE *file, meta_parameters *meta, int line_number,
		double *dest);
int get_double_lines(FILE *file, meta_parameters *meta, int line_number,
		int num_lines_to_get, double *dest);
int get_complexFloat_line(FILE *file, meta_parameters *meta, int line_number,
		complexFloat *dest);
int get_complexFloat_lines(FILE *file, meta_parameters *meta, int line_number,
		int num_lines_to_get, complexFloat *dest);
int put_float_line(FILE *file, meta_parameters *meta, int line_number,
		const float *source);
int put_float_lines(FILE *file, meta_parameters *meta, int line_number,
		int num_lines_to_put, const float *source);
int put_double_line(FILE *file, meta_parameters *meta, int line_number,
		const double *source);
int put_double_lines(FILE *file, meta_parameters *meta, int line_number,
		int num_lines_to_put, const double *source);
int put_complexFloat_line(FILE *file, meta_parameters *meta, int line_number,
		const complexFloat *source);
int put_complexFloat_lines(FILE *file, meta_parameters *meta, int line_number,
		int num_lines_to_put, const complexFloat *source);

/***************************************************************************
 * Get the location of the ASF Share Directory */
const char * get_asf_share_dir();
FILE * fopen_share_file(const char * filename, const char * mode);

#endif

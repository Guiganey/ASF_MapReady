/****************************************************************
NAME:  c2p

SYNOPSIS:    c2p inCPXfile outAPfile

DESCRIPTION:
    Convert complex images to amp & phase files

    inCPXfile is a complex image file. An extension of .cpx will be appended.
    outAPfile is the base name for the resulting amplitude and phase files.
    An extension of .amp & .phase will be appended.


EXTERNAL ASSOCIATES:
    NAME:                USAGE:
    ---------------------------------------------------------------

FILE REFERENCES:
    NAME:                USAGE:
    ---------------------------------------------------------------

PROGRAM HISTORY:
    VERS:   DATE:        PURPOSE:
    ---------------------------------------------------------------
    1.0     9/96   M. Shindle
                    & R. Fatland - Original Development
    1.05    5/00   P. Denny      - output a .ddr file
    1.25    6/03   P. Denny      - Forget about the DDR file, use meta
                                    Don't let input & output name be the same
                                    Use get/put_*_line routines instead of
                                     FREAD/FWRITE 
  
HARDWARE/SOFTWARE LIMITATIONS:

ALGORITHM DESCRIPTION:

ALGORITHM REFERENCES:

BUGS:

****************************************************************/
/****************************************************************************
*								            *
*   c2p:  complex to polar (amp + phase) stream converter		    *
* Copyright (c) 2004, Geophysical Institute, University of Alaska Fairbanks   *
* All rights reserved.                                                        *
*                                                                             *
* You should have received an ASF SOFTWARE License Agreement with this source *
* code. Please consult this agreement for license grant information.          *
*                                                                             *
*                                                                             *
*       For more information contact us at:                                   *
*                                                                             *
*	Alaska Satellite Facility	    	                              *
*	Geophysical Institute			www.asf.alaska.edu            *
*       University of Alaska Fairbanks		uso@asf.alaska.edu	      *
*	P.O. Box 757320							      *
*	Fairbanks, AK 99775-7320					      *
*									      *
******************************************************************************/
#include "asf.h"
#include "asf_meta.h"
#include "asf_insar.h"
#include "ifm.h"

#define VERSION		1.25
#define BUF		256
#define SAME		0
/* #define CHUNK_OF_LINES -- defined in ../../include/asf.h */

void usage(char *name);

int main (int argc, char *argv[])
{
  char cpxfile[BUF], ampfile[BUF], phsfile[BUF]; /* File Names                */
  FILE *fdCpx, *fdAmp, *fdPhase;          /* File Pointers                    */
  int line, sample;                       /* Line & sample indices for looping*/
  int percentComplete;                    /* Percent of data processed        */
  int blockSize;                          /* Number of samples gotten         */
  float *amp, *aP, *phase, *pP;           /* Output data buffers              */
  complexFloat *cpx, *cP;                 /* Input data buffers               */
  meta_parameters *inMeta, *ampMeta, *phsMeta; /* In/Out meta structs          */

/* Make sure there are the correct number of args in the command line */
  if (argc != 3) { usage(argv[0]); }
  
/* Make sure input and output names are different */
  if (strcmp(argv[1],argv[2])==SAME) {
    printf("c2p: Input and output names cannot be the same. Exiting.\n");
    exit(EXIT_FAILURE);
  }
  
/* Get commandline args */
  create_name (cpxfile, argv[1], ".cpx");
  create_name (ampfile, argv[2], "_amp.img"); 
  create_name (phsfile, argv[2], "_phase.img"); 

/* Read the input meta data. Assume data_type is COMPLEX_* & make it so. */
  inMeta = meta_read(argv[1]);
  inMeta->general->data_type = meta_polar2complex(inMeta->general->data_type);
  
/* Create & write a meta files for the output images */
  ampMeta = meta_copy(inMeta);
  ampMeta->general->data_type = meta_complex2polar(inMeta->general->data_type);
  ampMeta->general->image_data_type = AMPLITUDE_IMAGE;
  meta_write(ampMeta,ampfile);
  phsMeta = meta_copy(inMeta);
  phsMeta->general->data_type = meta_complex2polar(inMeta->general->data_type);
  phsMeta->general->image_data_type = PHASE_IMAGE;
  meta_write(phsMeta,phsfile);
  
/* malloc buffers, check and open files */
  cpx = (complexFloat *) MALLOC (sizeof(complexFloat)
                         * inMeta->general->sample_count * CHUNK_OF_LINES);
  amp = (float *) MALLOC (sizeof(float) * ampMeta->general->sample_count
                  * CHUNK_OF_LINES);
  phase = (float *) MALLOC (sizeof(float) * phsMeta->general->sample_count
                    * CHUNK_OF_LINES);
  fdCpx = fopenImage(cpxfile, "rb");
  fdAmp = fopenImage(ampfile, "wb");  
  fdPhase = fopenImage(phsfile, "wb");

/* Run thru the complex file, writing real data to amp and imag data to phase */
  printf("\n");
  percentComplete = 0;
  for (line=0; line<inMeta->general->line_count; line+=CHUNK_OF_LINES) {
    if ((line*100/inMeta->general->line_count == percentComplete)) {
      printf("\rConverting complex to amp and phase: %3d%% complete.",
             percentComplete++);
      fflush(NULL);
    }
    blockSize = get_complexFloat_lines(fdCpx,inMeta,line,CHUNK_OF_LINES,cpx);
    cP = cpx;
    aP = amp;
    pP = phase;
    for (sample=0; sample<blockSize; sample++) {
      if (cP->real!=0.0 || cP->imag!=0.0) {
        *aP = sqrt((cP->real)*(cP->real) + (cP->imag)*(cP->imag));
        *pP = atan2(cP->imag, cP->real);
      }
      else { *aP = 0.0; *pP = 0.0; }
      cP++;
      aP++;
      pP++;
    }
    put_float_lines(fdAmp,   ampMeta, line, CHUNK_OF_LINES, amp);
    put_float_lines(fdPhase, phsMeta, line, CHUNK_OF_LINES, phase);
  }
  printf("\rConverted complex to amp and phase:  100%% complete.\n\n");
  
/* close, free, halt */
  FCLOSE(fdCpx);
  FCLOSE(fdAmp);
  FCLOSE(fdPhase);
  FREE(cpx);
  FREE(amp);
  FREE(phase);
  meta_free(inMeta);
  meta_free(ampMeta);
  meta_free(phsMeta);
  
  return 0;
}


void usage(char *name)
{
 printf("\n"
	"USAGE:\n"
	"   %s <in> <out>\n",name);
 printf("\n"
	"REQUIRED ARGUMENTS:\n"
	"   <in>  The input file, assumed to be complex data.\n"
	"          will add .cpx extension to create <in>.cpx\n"
	"   <out> The base file name for the output;\n"
	"          Writes out files named <out>.amp, <out>.phase,\n"
	"          and <out>.meta\n");
 printf("\n"
	"DESCRIPTION:\n"
	"   Converts a complex image file to polar image files (amp + phase)\n");
 printf("\n"
	"Version %.2f, ASF SAR Tools\n"
	"\n",VERSION);
 exit(1); 
}

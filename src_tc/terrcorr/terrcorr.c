/*************************************************************************
NAME:   terrcorr --  Terrain Correction Main Program
 
SYNOPSIS:  terrcorr [-cf{1-4}klms] SAR DEM  
        -c              Clip the DEM file to the extents of the SAR image
        -m              Modify radiometric calibration sigma0 range, 
                        coefficients, and noise versus range LUT.
        -s              Skip the sar preprocessing step
        -k              Create a MASK value image
        -f{1-4}         Create a radiometrically terrain corrected product
                        using the numbered formula (see SARSIM)         
        -l              leave temporary files tiep.tpl, coef.ppf, 
                        pdem.img, csar.dat, and sSAR.img 
        SAR             Input SAR File -- ASF Type 100 or 200
        DEM             Input Digital Elevation Model file
 
DESCRIPTION: 
       This shell incorporates all of the steps involved in performing terrain
    correction of low or full-resolution ASF SAR images at varying resolutions.
    This process includes DEM preprocesssing, SAR preprocessing, geocoding,
    geometric rectification, and radiometric normalization.  
      
EXTERNAL ASSOCIATES:
    NAME:                USAGE:
    ---------------------------------------------------------------
    calibrate            Applies radiometric calibration to SAR image
    resample             Resamples a SAR image to the desired resolution
    demclip              Clips a DEM to a desired geographic location
    sarsim               Creates a simulated SAR image
    despike              Runs 2 despiking passes on an image
    correlate		 performs image to image correlation
    sargeom              Creates terrain corrected SAR image
    rtc_add  		 Applies radiometric terrain correction factors
 
FILE REFERENCES:
    NAME:             USAGE:
    ---------------------------------------------------------------
I   SAR.dat (or .D)   SAR data file
N   SAR.ldr (or .L)   SAR leader file
P   SAR.trl (or .L)   SAR trailer file
U   DEM.img           SUN LAS format DEM image
T   DEM.meta           SUN LAS format data descriptor record for DEM
S   
  
T   cSAR.dat (or .D)  Calibrated SAR image
M   sSAR.img          Simulated SAR image
P   rSAR<form>.img    Radiometric Terrain Corection factors
 
    pSAR.img          Preprocessed SAR
O   pSAR.meta          Preprocessed SAR
U   SAR_mask.img          Mask Value Image
T   SAR_mask.meta
P   SAR_cor.img       Terrain Corrected Image
U   SAR_cor.meta
T   SAR_cor<form>.img Radiometrically Terrain Corrected Image
S   SAR_cor<form>.meta
    fsSAR.img         Filtered Simulated SAR image
    fsSAR.meta         Filtered Simulated SAR image
 
PROGRAM HISTORY:
    VERS:   DATE:    AUTHOR:    PURPOSE:
    ---------------------------------------------------------------
    1.0     8/29/94  Tom Logan  Initial Implementation, converted 
                                from original Unix C shell script
    1.1     2/95     T. Logan   Add scalable pixel size option
                                Add las 6.0 (img DEM) option 
    1.2     6/95     T. Logan   Added modify calibration option
                                Modified to use new correlation process
    1.3     8/95     T. Logan   Modified to pass Mask and RTC factor 
                                switches to SARSIM.  Added call to rtc_mult.
                                Renamed output files.  
    2.0    10/95     T. Logan   Ported to Solaris
    2.1	    1/96     T. Logan   Added creation of zero coefficients file.
    2.2     5/96     T. Logan   Modified to use new metadata routines
    2.3     9/96     T. Logan   Modified to handle RADARSAT era data files
    2.4     8/97     T. Logan   Incorporated two-pass correlation 
 
HARDWARE/SOFTWARE LIMITATIONS:
 
ALGORITHM DESCRIPTION:
  Preprocess the input SAR image
    -- Apply radiometric calibrations
    -- Resample the image to desired resolution, OR
    -- Strip the SAR image CEOS wrapper
  Preprocess the input DEM image
    -- If necessary, clip the DEM to a desired area of coverage
  Create Simulated Image from DEM and original
    -- Using SAR metadata and DEM height values create sim image
    -- if (MSK) Create Mask Valued image
    -- if (RTC) Create RTC multipliers image
  Perform Correlation Process, Produce mapping Coefficients
    -- Smooth images by despiking filter
    -- Search the SAR image for areas of high contrast to use as tie points
    -- Correlate images to produce tie-points
    -- Edit tie-points and make 2D mapping function coefficients
  Create Terrain Corrected Image & Outputs
    -- Using the coefficients found, locate SAR image pixel values in the 
       positions given by DEM file
    -- if (RTC) perform rtc_mult to create RTC SAR image
  Clean Up Processing Files
    -- Remove intermediate processing files
 
ALGORITHM REFERENCES:   Based on EDC code by Charles Wivell
 
BUGS:
*************************************************************************/
/***************** Copyright Notice ***********************
                        English:
         You can freely use, modify, and re-distribute 
this code and/or binaries as long as you don't sell them,
and make sure to carry this notice along with them.
However, if you want to sell them, you can contact the 
University of Alaska Technology Corporation, below.


                        Legalese:
                 COPYRIGHT NOTIFICATION

(C) COPYRIGHT 1997 UNIVERSITY OF ALASKA. ALL RIGHTS RESERVED

This software discloses material protectable under copyright 
laws of the United States. Permission is hereby granted to 
use, reproduce, and prepare derivative works for noncommercial 
purposes at no charge, provided that this original copyright 
notice, and disclaimer are retained and any changes are 
clearly documented. Any entity desiring permission to 
incorporate this software or a work based on the software 
into a product for sale must contact the University of 
Alaska Technology Corporation.


This software was authored by:

RICK GURITZ      rguritz@images.alaska    (907)474-7886
Alaska SAR Facility, Geophysical Institute
P.O. Box 757320, University of Alaska Fairbanks
Fairbanks, Alaska 99775�7320
FAX: (907)474-5195

Any questions or comments on the software may be directed 
to one of the authors: Rick Guritz, Tom Logan, Mike Shindle,
Rob Fatland, Orion Lawlor, and Dorothy Corbett; or to
http://www.images.alaska.edu


NEITHER THE UNIVERSITY OF ALASKA NOR ANY SUBUNIT THEREOF, 
NOR ANY OF THEIR EMPLOYEES MAKES ANY WARRANTY, EXPRESS 
OR IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR 
RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR 
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD 
NOT INFRINGE PRIVATELY OWNED RIGHTS.
LICENSING INQUIRES MAY BE DIRECTED TO THE UNIVERSITY 
OF ALASKA TECHNOLOGY DEVELOPMENT CORPORATION AT (907)451-0718.
************************************************************/
#include "asf.h"
#include <unistd.h>
#include "ceos.h"
#include "ddr.h"
#define  VERSION 2.3

void meta_or_ddr(char *whichOne, char *fileName);
void display(char *text);
void execute(char *cmd);
void bye(void);
void make_zero(void);

int main(argc, argv)
        int argc; char **argv;
{
char cmd[255], temp[255], SAR[255], SARtrl[255], DEM[255],
     cSAR[255], pSAR[255], mSAR[255], rSAR[255],
     SIM[255], fSIM[255], COR[255], rCOR[255];
char metaExt[255], *imgExt=".img";
char SIM_rtc[259], pSAR_mask[260];
 
/*char    ascdesc;                 Ascending/Descending pass flag            */
float   pixsiz;                 /* Processing pixel size in meters           */
int     leave,                  /* Flag -- Leave temporary processing files? */
        clip,                   /* Flag -- Clip the DEM to match SAR image?  */
        mask,                   /* Flag -- Create a mask image?              */
        rtcf,                   /* Flag -- Apply radiometric terrain corr.   */
        form,                   /* Formula type for rtc image                */
        era,			/* Era of data files -- 0 PreRadarsat 1 Post */
        modify,                 /* Flag -- Modify calibration parameters?    */
	skip;			/* Flag -- Skip SAR preprocessing?	     */
int     error;                  /* Return status -- error checking           */
int     i, j;                   /* A humble little loop counter              */
struct  VFDRECV  asf_facdr;     /* Value of Facility Data Record             */
struct  DDR	 ddr;		/* LAS Data Descriptor Record		     */
/*****************************************************************************
                  S E T    U P    P R O C E S S I N G
******************************************************************************/
StartWatch();

printf("\n\n");
printf("==================================================================\n");
printf("        Terrain Correction Software for ASF ERS1 SAR images\n");
printf("==================================================================\n");
printf("\n");
 
/* Get Inputs from Command Line 
 ------------------------------*/
leave = 0;              /* Leave temporary processing files?    */
modify = 0;             /* Modify calibration coefficients?     */
clip = 0;               /* Clip the DEM file to match the SAR?  */
error = 0;              
mask = 0;		/* Create mask image file?		*/
rtcf = 0;		/* Create Radiometric TC file?          */
form = 0;		/* Number of formula to use for RTC     */
skip = 0;		/* Skip the SAR preprocessin step?	*/
 
if (argc >= 3)
  for (i = 1; i < argc; i++)
   {
     if (argv[i][0] == '-')
       {
         for (j = 1; argv[i][j] != 0; j++)
              switch (argv[i][j])
               {
                 case 'c': clip = 1; break;
                 case 'l': leave = 1; break;
                 case 'm': modify = 1; break;
		 case 's': skip = 1; break;
                 case 'k': mask = 1; break;
                 case 'f': 
                           j++;
 			   form = atoi(&argv[i][j]);
                           rtcf = 1;
                           break;
                 default:
                     printf("\n\7\7\n INVALID OPTION: %s\n\7\n",argv[i]);
                     error = 1;
                     break;
               } 
       }
     else
       {
        strcpy(SAR,argv[i++]);
        strcpy(DEM,argv[i++]);
       }
    }
else error = 1;
 
if (error)
  {
   printf(" Usage: %s [-cf{1-4}klms] SAR DEM\n\n",argv[0]);
   printf(" The inputs to this shell are as follows:\n\n");
   printf("   SAR         input SAR file, must be an original ASF \n");
   printf("               groundstation LOW or FULL-RES image set\n");
   printf("   DEM         input DEM file of SAR area of coverage, in LAS 6\n");
   printf("               .img format.  The .meta metadata file is used also\n");
   printf("\n");
   printf(" The options accepted are:\n\n");
   printf("   -c          Clip the DEM file to the area of the SAR image\n");
   printf("               (thus reducing processing time).\n");
   printf("   -f{1-4}     Apply radiometric terrain corrections using the\n");
   printf("               formula requested (1 LI  2 GO  3 SQ  4 VX).\n");
   printf("   -k          Create a mask value image.\n");
   printf("   -l          Leave temporary processing files intact.\n");
   printf("   -m          Modify radiometric calibration sigma0 range, \n");
   printf("               coefficients, and noise vs. range LUT\n");
   printf("   -s          Skip the SAR preprocessing steps\n");
   printf("\n");
   printf(" Version %.2f,  ASF SAR Tools\n\n",VERSION);
   exit(EXIT_FAILURE);
  }

strcat(strcpy(SIM,"s"),SAR);
strcat(strcpy(fSIM,"f"),SIM);
strcat(strcpy(cSAR,"c"),SAR);
strcat(strcpy(pSAR,"p"),SAR);
strcat(strcpy(COR,SAR),"_cor");
if (mask) strcat(strcpy(mSAR,"m"),SAR);
if (rtcf)
 {
  strcat(strcpy(rSAR,"r"),SAR);
  switch (form)
   {
     case 1: strcat(strcpy(rCOR,COR),"li"); strcat(rSAR,"li"); break;
     case 2: strcat(strcpy(rCOR,COR),"go"); strcat(rSAR,"go"); break;
     case 3: strcat(strcpy(rCOR,COR),"sq"); strcat(rSAR,"sq"); break;
     case 4: strcat(strcpy(rCOR,COR),"vx"); strcat(rSAR,"vx"); break;
   }
 }


/* Read DEM and SAR image metadata
 --------------------------------*/
if (c_getddr(DEM, &ddr)!= 0)
  { printf("Unable to get ddr file for image %s\n",DEM); exit(EXIT_FAILURE); }
pixsiz = ddr.pdist_x;

era = set_era(SAR,SARtrl,2);
get_asf_facdr(SARtrl,&asf_facdr);
/*ascdesc = asf_facdr.ascdesc[0];*/

printf("Preprocessing the SAR image\n"); 
/*------------------------------------------------------------------------
                   Preprocess the input SAR image 
 ------------------------------------------------------------------------*/
  if (!skip)
   {
    /* Apply Radiometric Calibration
     ------------------------------*/
    display("Applying Radiometric Calibration to SAR Image");
    if (modify) sprintf(cmd,"calibrate %s %s -m\n",SAR,cSAR);
    else sprintf(cmd,"calibrate %s %s\n",SAR,cSAR);
    execute(cmd);

    /* Resample Image / Strip CEOS Header 
     -------------------------------------*/
    if (asf_facdr.rapixspc < pixsiz)
      {
	display("Resampling and Low-Pass Filtering SAR Image");
        sprintf(cmd,"resample %s %s %f\n",cSAR,pSAR,pixsiz);
        execute(cmd);

        printf("\n");
        sprintf(cmd,"rm -f %s*\n",cSAR);
        if (system(cmd)==-1) bye();
      } 
    else if (asf_facdr.rapixspc == pixsiz)
      {
      	meta_or_ddr(metaExt,cSAR);
	sprintf(cmd,"mv %s%s %s%s\n",cSAR,imgExt,pSAR,imgExt); execute(cmd);	
	sprintf(cmd,"mv %s%s %s%s\n",cSAR,metaExt,pSAR,metaExt); execute(cmd);	
      }
    else
      {
        printf("\nERROR: SAR image has a lower resolution than the DEM\n");
        bye();
      } 
 
    /* Copy metadata for resampled image
     -----------------------------------*/
    printf("\n");
    if (!era)
     {
       sprintf(temp,".ldr");
       sprintf(cmd,"cp %s%s %s%s\n",SAR,temp,pSAR,temp);
       execute(cmd);
       printf("\n");
       sprintf(temp,".trl");
       sprintf(cmd,"cp %s%s %s%s\n",SAR,temp,pSAR,temp);
       execute(cmd);
     }
    else
     {
       sprintf(temp,".L");
       sprintf(cmd,"cp %s%s %s%s\n",SAR,temp,pSAR,temp);
       execute(cmd);
     }

   }  /* End if !skip */
  else strcpy(pSAR,SAR);

/*--------------------------------------------------------------------------
                         Preprocess the input DEM image
 --------------------------------------------------------------------------*/
 if (clip)
  {
   display("Clipping input DEM file to area of SAR image");
   sprintf(cmd,"demclip %s %s pDEM\n",DEM,SAR);
   execute(cmd);
   strcpy(DEM,"pDEM");
  }

/*--------------------------------------------------------------------------
                 Create Simulated Image from DEM and original
 --------------------------------------------------------------------------*/
  printf("\n");
  display("Creating Simulated SAR image");
  if (mask && rtcf) sprintf(cmd,"sarsim %s %s %s -kr%i\n",pSAR,DEM,SIM,form);
  else if (mask)    sprintf(cmd,"sarsim %s %s %s -k\n",pSAR,DEM,SIM);
  else if (rtcf)    sprintf(cmd,"sarsim %s %s %s -r%i\n",pSAR,DEM,SIM,form);
  else              sprintf(cmd,"sarsim %s %s %s\n",pSAR,DEM,SIM);
  execute(cmd);

  if (rtcf)
    {
     strcat(strcpy(SIM_rtc,SIM),"_rtc");
     sprintf(cmd,"mv %s%s %s%s\n",SIM_rtc,imgExt,rSAR,imgExt); execute(cmd);
     meta_or_ddr(metaExt, SIM_rtc);
     sprintf(cmd,"mv %s%s %s%s\n",SIM_rtc,metaExt,rSAR,metaExt); execute(cmd);
    }
  if (mask)  /* Fix Mask Image File Name */
    {
     strcat(strcpy(pSAR_mask,pSAR),"_mask");
     sprintf(cmd,"mv %s%s %s%s\n",pSAR_mask,imgExt,SAR,imgExt); execute(cmd);
     meta_or_ddr(metaExt, pSAR_mask);
     sprintf(cmd,"mv %s%s %s%s\n",pSAR_mask,metaExt,SAR,metaExt); execute(cmd);
    }


/*-----------------------------------------------------------------------
        Despike Filter SIM and SAR images for correlation process
 -----------------------------------------------------------------------*/
  display("Despike Filtering SIM Image");
  sprintf(cmd,"despike %s %s\n",SIM,fSIM);  execute(cmd);
 
/*-----------------------------------------------------------------------
        Perform Correlation Process, Produce mapping Coefficients
 -----------------------------------------------------------------------*/
  display("Performing Correlation of Images");
  sprintf(cmd,"correlate %s %s coef %s\n",fSIM,pSAR,SAR);
  execute(cmd);
/*--------------------------------------------------------------------------
                        Create Terrain Corrected Image 
 --------------------------------------------------------------------------*/
  display("Creating Terrain Corrected Image");
  sprintf(cmd,"sargeom %s %s coef %s\n",DEM,pSAR,COR); execute(cmd);

/*--------------------------------------------------------------------------
               Create Radiometrically Terrain Corrected Image 
 --------------------------------------------------------------------------*/
  if (rtcf)
   {
     display("Creating Radiometrically Terrain Corrected Image");
     sprintf(cmd,"rtc_add %s %s %s\n",COR,rSAR,rCOR); execute(cmd);
     meta_or_ddr(metaExt, COR);
     sprintf(cmd,"cp %s%s %s%s\n",COR,metaExt,rCOR,metaExt); execute(cmd);
   }

/*--------------------------------------------------------------------------
                         Clean Up Processing Files
 --------------------------------------------------------------------------*/
  display("Cleaning Processing Files");

  if (!leave)
   {
    /* sprintf(cmd,"rm -f *.tpl\n"); execute(cmd); */
    meta_or_ddr(metaExt, SIM);
    sprintf(cmd,"rm -f %s%s %s%s\n",SIM,imgExt,SIM,metaExt); execute(cmd);
    sprintf(cmd,"rm -f cor_sar?.* cor_sim?.*\n"); execute(cmd);

    if (clip)
     {
      sprintf(cmd,"rm -f %s%s\n",DEM,imgExt); execute(cmd);
      meta_or_ddr(metaExt, DEM);
      sprintf(cmd,"rm -f %s%s\n",DEM,metaExt); execute(cmd);
     }
   }
 
  if (rtcf)
   {
     sprintf(cmd,"rm -f %s%s\n",rSAR,imgExt); execute(cmd);
     meta_or_ddr(metaExt, rSAR);
     sprintf(cmd,"rm -f %s%s\n",rSAR,metaExt); execute(cmd);
   }

  if (!skip)
   {
    if (!era)
     {
      sprintf(temp,".ldr"); sprintf(cmd,"rm -f %s%s\n",pSAR,temp); execute(cmd);
      sprintf(temp,".trl"); sprintf(cmd,"rm -f %s%s\n",pSAR,temp); execute(cmd);
     }
    else
     {sprintf(temp,".L"); sprintf(cmd,"rm -f %s%s\n",pSAR,temp); execute(cmd);}
   }



StopWatch();

printf("\n\n");
printf("==================================================================\n");
printf("             End of Terrain Correction Process\n");
printf("==================================================================\n");
printf("\n\n");

return(0);
}



void meta_or_ddr(char *whichOne, char *fileName)
{
  char metaName[255], ddrName[255];
  
  create_name(metaName, fileName, ".meta");
  create_name(ddrName,  fileName, ".ddr");

  if (fileExists(metaName)) {
    strcpy (whichOne, ".meta");
  }
  else if (fileExists(ddrName)) {
    strcpy (whichOne, ".ddr");
  }
  else { /* Default to .meta */
    strcpy (whichOne, ".meta");
  }
}
    

void display(char *text)
{
  printf("\n-----------------------------------------------------------\n");
  printf("%s\n",text);
  printf("-----------------------------------------------------------\n\n");
}

void execute(char *cmd) 
{
	printf("%s",cmd); 
	fflush(stdin);
	if (system(cmd)!=0) 
		bye(); 
}

void bye(void) { printf("Program Aborted.\n"); exit(EXIT_FAILURE); }

void make_zero(void)
{
  FILE *fp;

  fp = fopen("coef.ppf","w");
  fprintf(fp,"AZ1COEF        R 1 0.0\n");
  fprintf(fp,"AZ2COEF        R 1 0.0\n");
  fprintf(fp,"AZ3COEF        R 1 0.0\n");
  fprintf(fp,"GR1COEF        R 1 0.0\n");
  fprintf(fp,"GR2COEF        R 1 0.0\n");
  fprintf(fp,"GR3COEF        R 1 0.0\n");
  fclose(fp);
}



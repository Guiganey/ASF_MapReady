/* Cal_params:
	Fetches noise and scaling factors from
input CEOS image, and returns them to
calibrate.*/


#include "asf.h"
#include "ceos.h"
#include "calibrate.h"
#include <assert.h>

/**Harcodings to fix calibration of ASF data***
 --------------------------------------------
   Currently contains:

   double sspswb010_noise_vec[256];
   double sspswb011_noise_vec[256];
   double sspswb013_noise_vec[256];
   double sspswb014_noise_vec[256];
   double sspswb015_noise_vec[256];
   double sspswb016_noise_vec[256]; (identical to 15)

*********************************************/
#include "noise_vectors.h"
#include "matrix.h"

#ifndef PI
# define PI 3.14159265358979323846
#endif
#define GRID 16
#define TABLE 512

#define SQR(X) ((X)*(X))

// Routine internal to find_quadratic:
// Return the value of the given term of the quadratic equation.
double get_term(int termNo, double x, double y)
{
  switch(termNo)
    {
    case 0:/*A*/return 1;
    case 1:/*B*/return x;
    case 2:/*C*/return y;
    case 3:/*D*/return x*x;
    case 4:/*E*/return x*y;
    case 5:/*F*/return y*y;
    case 6:/*G*/return x*x*y;
    case 7:/*H*/return x*y*y;
    case 8:/*I*/return x*x*y*y;
    case 9:/*J*/return x*x*x;
    case 10:/*K*/return y*y*y;
    default:/*??*/
      asfPrintError("Unknown term number %d passed to get_term!\n", termNo);
      return 0.0;
    }
}

// Fit a quadratic warping function to the given points 
// in a least-squares fashion
quadratic_2d find_quadratic(const double *out, const double *x,
                            const double *y, int numPts)
{
  int nTerms=11;
  matrix *m=matrix_alloc(nTerms,nTerms+1);
  int row,col;
  int i;
  quadratic_2d c;
  // For each data point, add terms to matrix
  for (i=0;i<numPts;i++) {
    for (row=0;row<nTerms;row++) {
      double partial_Q=get_term(row,x[i],y[i]);
      for (col=0;col<nTerms;col++)
	m->coeff[row][col]+=partial_Q*get_term(col,x[i],y[i]);
      m->coeff[row][nTerms]+=partial_Q*out[i];
    }
  }
  // Now solve matrix to find coefficients
  // matrix_print(m,"\nLeast-Squares Matrix:\n",stdout);
  matrix_solve(m);
  c.A=m->coeff[0][nTerms];c.B=m->coeff[1][nTerms];c.C=m->coeff[2][nTerms];
  c.D=m->coeff[3][nTerms];c.E=m->coeff[4][nTerms];c.F=m->coeff[5][nTerms];
  c.G=m->coeff[6][nTerms];c.H=m->coeff[7][nTerms];c.I=m->coeff[8][nTerms];
  c.J=m->coeff[9][nTerms];c.K=m->coeff[10][nTerms];
  return c;
}

double get_satellite_height(double time, stateVector stVec)
{
  return sqrt(stVec.pos.x*stVec.pos.x +
	      stVec.pos.y*stVec.pos.y +
	      stVec.pos.z*stVec.pos.z);
}

double get_earth_radius(double time, stateVector stVec, double re, double rp)
{
  double er = sqrt(stVec.pos.x*stVec.pos.x +
		   stVec.pos.y*stVec.pos.y +
		   stVec.pos.z*stVec.pos.z);
  double lat = asin(stVec.pos.z/er);
  return (re*rp) / sqrt(rp*rp*cos(lat)*cos(lat) + re*re*sin(lat)*sin(lat));
}

double get_slant_range(meta_parameters *meta, double er, double ht, int sample)
{
  double minPhi = acos((ht*ht + er*er -
		       SQR(meta->sar->slant_range_first_pixel))/(2.0*ht*er));
  double phi = minPhi + sample*(meta->general->x_pixel_size/er);
  double slantRng = sqrt(ht*ht + er*er - 2.0*ht*er*cos(phi));
  return slantRng + meta->sar->slant_shift;
}

double get_look_angle(double er, double ht, double sr)
{
  return acos((sr*sr+ht*ht-er*er)/(2.0*sr*ht));
}

double get_incidence_angle(double er, double ht, double sr)
{
  return PI-acos((sr*sr+er*er-ht*ht)/(2.0*sr*er));
}

quadratic_2d get_incid(char *sarName, meta_parameters *meta)
{
  int ll, kk;
  quadratic_2d q;
  stateVector stVec;
  int projected = FALSE;
  double *line, *sample, *incidence_angle;
  double earth_radius, satellite_height, time, range;
  double firstIncid, re, rp;

  incidence_angle = (double *) MALLOC(sizeof(double)*GRID*GRID);
  line = (double *) MALLOC(sizeof(double)*GRID*GRID);
  sample = (double *) MALLOC(sizeof(double)*GRID*GRID);
  int nl = meta->general->line_count;
  int ns = meta->general->sample_count;
  re = meta->general->re_major;
  rp = meta->general->re_minor;
  if (meta->projection) // other conditions needed ???
    projected = TRUE;
  for (ll=0; ll<GRID; ll++)
    for (kk=0; kk<GRID; kk++) {
      line[ll*GRID+kk] = ll * nl / GRID;
      sample[ll*GRID+kk] = kk * ns / GRID;
      if (projected) {
	earth_radius = meta_get_earth_radius(meta, ll, kk);
	satellite_height = meta_get_sat_height(meta, ll, kk);
	range = get_slant_range(meta, earth_radius, satellite_height,
				sample[ll*GRID+kk]);
      }
      else {
	time = meta_get_time(meta, line[ll*GRID+kk], sample[ll*GRID+kk]);
	stVec = meta_get_stVec(meta, time);
	earth_radius = get_earth_radius(time, stVec, re, rp);
	satellite_height = get_satellite_height(time, stVec);
	range = get_slant_range(meta, earth_radius, satellite_height,
				sample[ll*GRID+kk]);
      }
      incidence_angle[ll*GRID+kk] =
        get_incidence_angle(earth_radius, satellite_height, range)*R2D;

      if (ll==0 && kk==0)
        firstIncid = incidence_angle[0];
    }

  q = find_quadratic(incidence_angle, line, sample, GRID*GRID);
  q.A = firstIncid;

  // Clean up
  FREE(line);
  FREE(sample);
  FREE(incidence_angle);

  return q;
}

/**************************************************************************
create_cal_params
Constructs a cal_params record, by reading the
given inSAR CEOS file.
**************************************************************************/

cal_params *create_cal_params(const char *inSAR, meta_parameters *meta)
{
  int ii, kk;
  struct dataset_sum_rec dssr; // Data set summary record
  double *noise;
  char sarName[512], *facilityStr, *processorStr;
  cal_params *cal = (cal_params *) MALLOC(sizeof(cal_params));
  cal->asf = NULL;
  cal->asf_scansar = NULL;
  cal->esa = NULL;
  cal->rsat = NULL;
  cal->alos = NULL;
  
  strcpy (sarName, inSAR);
  
  // Check for the varioius processors
  get_dssr(sarName, &dssr);
  facilityStr = trim_spaces(dssr.fac_id);
  processorStr = trim_spaces(dssr.sys_id);

  if (strncmp(facilityStr, "ASF", 3)== 0 &&
      strncmp(processorStr, "FOCUS", 5) != 0 &&
      meta->projection && meta->projection->type == SCANSAR_PROJECTION)
  {
    // ASF internal processor (PP or SSP), ScanSar data
    asf_scansar_cal_params *asf = MALLOC(sizeof(asf_scansar_cal_params));
    cal->asf_scansar = asf;

    // Get values for calibration coefficients and LUT
    struct VRADDR rdr; // Radiometric data record
    get_raddr(sarName, &rdr);
    
    // hardcodings for not-yet-calibrated fields
    if (rdr.a[0] == -99.0 || rdr.a[1]==0.0 ) {
      asf->a0 = 1.1E4;
      asf->a1 = 2.2E-5;
      asf->a2 = 0.0;
    } 
    else {
      asf->a0 = rdr.a[0];
      asf->a1 = rdr.a[1];
      asf->a2 = rdr.a[2];
    }

    // Set the Noise Correction Vector to correct version
    if (strncmp(dssr.cal_params_file,"SSPSWB010.CALPARMS",18)==0) {
      asfPrintStatus("\n   Substituting hardcoded noise vector sspswb010\n");
      noise = sspswb010_noise_vec;
    } 
    else if (strncmp(dssr.cal_params_file,"SSPSWB011.CALPARMS",18)==0) {
      asfPrintStatus("\n   Substituting hardcoded noise vector sspswb011\n");
      noise = sspswb011_noise_vec;
    } 
    else if (strncmp(dssr.cal_params_file,"SSPSWB013.CALPARMS",18)==0) {
      asfPrintStatus("\n   Substituting hardcoded noise vector sspswb013\n");
      noise = sspswb013_noise_vec;
    } 
    else if (strncmp(dssr.cal_params_file,"SSPSWB014.CALPARMS",18)==0) {
      asfPrintStatus("\n   Substituting hardcoded noise vector sspswb014\n");
      noise = sspswb014_noise_vec;
    } 
    else if (strncmp(dssr.cal_params_file,"SSPSWB015.CALPARMS",18)==0) {
      asfPrintStatus("\n   Substituting hardcoded noise vector sspswb015\n");
      noise = sspswb015_noise_vec;
    } 
    else if (strncmp(dssr.cal_params_file,"SSPSWB016.CALPARMS",18)==0) {
      asfPrintStatus("\n   Substituting hardcoded noise vector sspswb016\n");
      noise = sspswb015_noise_vec;
      // 16 and 15 were identical antenna patterns, only metadata fields were 
      // changed, so the noise vector for 16 is the same and that for 15. JBN
    } 
    else 
      noise = rdr.noise;
/*
    int tab = TABLE;
    asf->tablePix = ((meta->general->sample_count + (tab-1))/tab);
    numLines = asf->numLines = meta->general->line_count;
  
    float noise_index=0, frac;
    int index, base;
    for (ii=0; ii<numLines; ++ii) {
      for (kk=0; kk<TABLE; kk++) {
        noise_index = (meta_look(meta, (float)ii,
                                 (float)kk*asf->tablePix)*180.0/PI-16.3)*10.0;
        base = kk + (ii/(numLines/tab)*tab);
	//if (ii == 100 && kk == 0)
        printf("ii: %d, kk: %d, lines: %d, noise index: %f, "
               "base: %d\n", ii, kk, numLines, noise_index, base);
        assert(base>=0);
        assert(base<512);
	// Clamp noise_index to within the noise table
	if (noise_index <= 0)
	  asf->noise[base] = noise[0];
	else if (noise_index >= 255)
	  asf->noise[base] = noise[255];
	else {
	  // Use linear interpolation on noise array
	  index = (int)noise_index;
	  frac = noise_index - index;
	  asf->noise[base] = 
	    noise[index] + frac*(noise[index+1] - noise[index]);
	}
      }
    }
*/

    asf->tablePix = 256;
    for (kk=0; kk<asf->tablePix; ++kk)
      asf->noise[kk] = noise[kk];

    asf->meta = meta;
  }
  else if (strncmp(facilityStr, "ASF", 3)== 0 &&
           strncmp(processorStr, "FOCUS", 5) != 0)
  {
    // ASF internal processor (PP or SSP) (non-Scansar)
    asf_cal_params *asf = (asf_cal_params *) MALLOC(sizeof(asf_cal_params));
    cal->asf = asf;

    // Get values for calibration coefficients and LUT
    struct VRADDR rdr; // Radiometric data record
    get_raddr(sarName, &rdr);
    
    // hardcodings for not-yet-calibrated fields
    if (rdr.a[0] == -99.0 || rdr.a[1]==0.0 ) {
      asf->a0 = 1.1E4;
      asf->a1 = 2.2E-5;
      asf->a2 = 0.0;
    } 
    else {
      asf->a0 = rdr.a[0];
      asf->a1 = rdr.a[1];
      asf->a2 = rdr.a[2];
    }

    // grab the noise vector
    for (kk=0; kk<256; ++kk)
      asf->noise[kk] = rdr.noise[kk];
    asf->sample_count = meta->general->sample_count;
  }
  else if ((strncmp(facilityStr, "ASF", 3) == 0 &&
	    strncmp(dssr.sys_id, "FOCUS", 5) == 0) ||
	   (strncmp(facilityStr, "CDPF", 4) == 0 ||
	    strncmp(facilityStr, "RSI", 3) == 0 ||
	    (strncmp(facilityStr, "CSTARS", 6) == 0 && 
	     strncmp(dssr.mission_id, "RSAT", 4) == 0))) {
    // Radarsat style calibration
    rsat_cal_params *rsat = 
      (rsat_cal_params *) MALLOC(sizeof(rsat_cal_params));
    cal->rsat = rsat;
    rsat->slc = FALSE;
    rsat->focus = FALSE;
    if (strncmp(dssr.product_type, "SLANT RANGE COMPLEX", 19) == 0 ||
	strncmp(dssr.product_type, 
		"SPECIAL PRODUCT(SINGL-LOOK COMP)", 32) == 0) {
      rsat->slc = TRUE;
    }
    if (strncmp(dssr.sys_id, "FOCUS", 5) == 0)
      rsat->focus = TRUE;

    // Read lookup up table from radiometric data record
    struct RSI_VRADDR radr;
    get_rsi_raddr(sarName, &radr);
    rsat->n = radr.n_samp;
    rsat->lut = (double *) MALLOC(sizeof(double) * rsat->n);
    for (ii=0; ii<rsat->n; ii++) {
      if (strncmp(dssr.sys_id, "FOCUS", 5) == 0)
	rsat->lut[ii] = radr.lookup_tab[0];
      else
	rsat->lut[ii] = radr.lookup_tab[ii];
    }
    rsat->samp_inc = radr.samp_inc;
    rsat->a3 = radr.offset;
    
  }
  else if (strncmp(facilityStr, "ES", 2) == 0 ||
	   strncmp(facilityStr, "D-PAF", 5) == 0 ||
	   strncmp(facilityStr, "I-PAF", 2) == 0 ||
	   strncmp(facilityStr, "Beijing", 7) == 0 ||
	   (strncmp(facilityStr, "CSTARS", 6) == 0 &&
	    (strncmp(dssr.mission_id, "E", 1) == 0 ||
	     strncmp(dssr.mission_id, "J", 1) == 0)))
    {
    // ESA style calibration
    esa_cal_params *esa = (esa_cal_params *) MALLOC(sizeof(esa_cal_params));
    cal->esa = esa;

    // Read calibration coefficient and reference incidence angle
    struct ESA_FACDR facdr;
    get_esa_facdr(sarName, &facdr);
    esa->k = facdr.abs_cal_const;
    esa->ref_incid = dssr.incident_ang;
  }
  else if (strncmp(facilityStr, "EOC", 3) == 0) {
    // ALOS processor
    struct alos_rad_data_rec ardr; // ALOS Radiometric Data record
    alos_cal_params *alos = 
      (alos_cal_params *) MALLOC(sizeof(alos_cal_params));
    cal->alos = alos;

    // Reading calibration coefficient
    get_ardr(sarName, &ardr);
    if (strncmp(dssr.lev_code, "1.1", 3) == 0) // SLC
      alos->cf = ardr.calibration_factor - 32;
    else if (strncmp(dssr.lev_code, "1.5", 3) == 0) // regular detected
      alos->cf = ardr.calibration_factor;
  }
  else
    // should never get here
    asfPrintError("Unknown calibration parameter scheme!\n");
    
  // Determine polynomial for incidence angle calculation
  cal->incid = get_incid(sarName, meta);

  return cal;
}

/*----------------------------------------------------------------------
  Get_cal_dn:
        Convert amplitude image data number into calibrated image data
        number (in power scale), given the current noise value.
----------------------------------------------------------------------*/
float get_cal_dn(cal_params *cal, int line, int sample, float inDn, int dbFlag)
{
  double scaledPower, calValue, incidence_angle, invIncAngle;
  int x=line, y=sample;
  quadratic_2d q;
  
  // Calculate incidence angle
  q = cal->incid;
  incidence_angle = q.A + q.B*x + q.C*y + q.D*x*x + q.E*x*y+ q.F*y*y + 
    q.G*x*x*y + q.H*x*y*y + q.I*x*x*y*y + q.J*x*x*x + q.K*y*y*y;

  // Calculate according to the calibration data type
  if (cal->asf) { // ASF style data (PP and SSP)

    if (cal->radiometry == r_SIGMA || cal->radiometry == r_SIGMA_DB)
      invIncAngle = 1.0;
    else if (cal->radiometry == r_GAMMA || cal->radiometry == r_GAMMA_DB)
      invIncAngle = 1/cos(incidence_angle*D2R);
    else if (cal->radiometry == r_BETA || cal->radiometry == r_BETA_DB)
      invIncAngle = 1/sin(incidence_angle*D2R);

    asf_cal_params *p = cal->asf;
    double index = (double)sample*256./(double)(p->sample_count);
    int base = (int) index;
    double frac = index - base;
    double *noise = p->noise;

    // Determine the noise value
    double noiseValue = noise[base] + frac*(noise[base+1] - noise[base]);

    // Convert (amplitude) data number to scaled, noise-removed power
    scaledPower = 
      (p->a1*(inDn*inDn-p->a0*noiseValue) + p->a2)*invIncAngle;
  }
  else if (cal->asf_scansar) { // ASF style ScanSar data

    asf_scansar_cal_params *p = cal->asf_scansar;
    meta_parameters *meta = p->meta;

    double er = meta_get_earth_radius(meta,line,sample);
    double ht = meta_get_sat_height(meta,line,sample);
    //double incid2 = R2D*meta_incid(meta,line,sample);
    double look = R2D*look_from_incid(incidence_angle*D2R,er,ht);
    //double look2 = R2D*look_from_incid(incid2*D2R,er,ht);
    //double look3 = R2D*meta_look(meta,line,sample);
    //if (fabs(look-look2)>.001)
      //  printf("%d %d %f %f %f %f %f\n", line, sample, look, look2, look3, incidence_angle, incid2);
      //printf("%d %d %f %f\n", line, sample, look, incidence_angle);

    if (cal->radiometry == r_SIGMA || cal->radiometry == r_SIGMA_DB)
      invIncAngle = 1.0;
    else if (cal->radiometry == r_GAMMA || cal->radiometry == r_GAMMA_DB)
      invIncAngle = 1/cos(incidence_angle*D2R);
    else if (cal->radiometry == r_BETA || cal->radiometry == r_BETA_DB)
      invIncAngle = 1/sin(incidence_angle*D2R);

    double index = (look-16.3)*10.0;
    double noiseValue;
    double *noise = p->noise;

    if (index <= 0)
      noiseValue = noise[0];
    else if (index >= 255)
      noiseValue = noise[255];
    else {
      // Use linear interpolation on noise array
      int base = (int)index;
      double frac = index - base;

      noiseValue = noise[base] + frac*(noise[base+1] - noise[base]);
    }

    // Convert (amplitude) data number to scaled, noise-removed power
    scaledPower = 
      (p->a1*(inDn*inDn-p->a0*noiseValue) + p->a2)*invIncAngle;
  }
  else if (cal->esa) { // ESA style ERS and JERS data

    if (cal->radiometry == r_GAMMA || cal->radiometry == r_GAMMA_DB)
      invIncAngle = 1/cos(incidence_angle*D2R);

    esa_cal_params *p = cal->esa;

    if (cal->radiometry == r_BETA || cal->radiometry == r_BETA_DB)
      scaledPower = inDn*inDn/p->k;
    else if (cal->radiometry == r_SIGMA || cal->radiometry == r_SIGMA_DB)
      scaledPower = 
	inDn*inDn/p->k*sin(p->ref_incid*D2R)/sin(incidence_angle*D2R);
    if (cal->radiometry == r_GAMMA || cal->radiometry == r_GAMMA_DB)
      scaledPower = 
	inDn*inDn/p->k*sin(p->ref_incid*D2R)/sin(incidence_angle*D2R) /
	invIncAngle;

  }
  else if (cal->rsat) { // CDPF style Radarsat data
    
    if (cal->radiometry == r_BETA || cal->radiometry == r_BETA_DB)
      invIncAngle = 1.0;
    if (cal->radiometry == r_SIGMA || cal->radiometry == r_SIGMA_DB)
      invIncAngle = 1/tan(incidence_angle*D2R);
    else if (cal->radiometry == r_GAMMA || cal->radiometry == r_GAMMA_DB)
      invIncAngle = tan(incidence_angle*D2R);

    rsat_cal_params *p = cal->rsat;
    double a2;
    if (cal->rsat->focus)
      a2 = p->lut[0];
    else if (sample < (p->samp_inc*(p->n-1))) {
      int i_low = sample/p->samp_inc;
      int i_up = i_low + 1;
      a2 = p->lut[i_low] +
	((p->lut[i_up] - p->lut[i_low])*((sample/p->samp_inc) - i_low));
    }
    else 
      a2 = p->lut[p->n-1] +
	((p->lut[p->n-1] - p->lut[p->n-2])*((sample/p->samp_inc) - p->n-1));
    if (p->slc)
      scaledPower = (inDn*inDn)/(a2*a2)*invIncAngle;
    else
      scaledPower = (inDn*inDn + p->a3)/a2*invIncAngle;
  }
  else if (cal->alos) { // ALOS data
    
    if (cal->radiometry == r_SIGMA || cal->radiometry == r_SIGMA_DB)
      invIncAngle = 1.0;
    else if (cal->radiometry == r_GAMMA || cal->radiometry == r_GAMMA_DB)
      invIncAngle = 1/cos(incidence_angle*D2R);
    else if (cal->radiometry == r_BETA || cal->radiometry == r_BETA_DB)
      invIncAngle = 1/sin(incidence_angle*D2R);

    alos_cal_params *p = cal->alos;

    scaledPower = pow(10, p->cf/10.0)*inDn*inDn*invIncAngle;
  }
  else
    // should never get here
    asfPrintError("Unknown calibration data type!\n");
  
  // We don't want to convert the scaled power image into dB values
  // since it messes up the statistics
  // We set all values lower than the noise floor (0.001 is the equivalent
  // to -30 dB) to the mininum value of 0.001, removing outliers.
  if (scaledPower > 0.001 && inDn > 0.0) {
    if (dbFlag)
      calValue = 10.0 * log10(scaledPower);
    else
      calValue = scaledPower;
  }
  else {
    if (dbFlag)
      calValue = -30.0;
    else
      calValue = 0.001;
  }

  return calValue;
}

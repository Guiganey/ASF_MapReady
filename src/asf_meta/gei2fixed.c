/*
gei2fixed: part of JPL's earthloc software.
	Transform a geocentric equatorial inertial state vector to
fixed-earth.
	Inputs:
		stateVec.pos.x=X position of s/c (m)
		stateVec[1]=Y position of s/c (m)
		stateVec[2]=Z position of s/c (m)
		stateVec[3]=X velocity of s/c (m/s)
		stateVec[4]=Y velocity of s/c (m/s)
		stateVec[5]=Z velocity of s/c (m/s)
		gha=Greenwich Hour Angle at time of stateVec.
	Outputs:
		stateVec is now in the fixed earth
		coordinate system.

*/
#include "asf.h"
#include "geolocate.h"

void gei2fixed(stateVector *stateVec,double gha)
{
	double radius,phi,phiVel;
	double angularVelocity=(366.225/365.225)*2.0*M_PI/86400.0;
	double ghaRads=gha*M_PI/180.0;
	stateVector stateOut;
	
	/*Compute the radius, longitude, and rotation amount for the position.*/
	radius=sqrt(stateVec->pos.x*stateVec->pos.x+stateVec->pos.y*stateVec->pos.y);
	phi=atan2_check(stateVec->pos.y,stateVec->pos.x);
	phi-=ghaRads;
	stateOut.pos.x=cos(phi)*radius;
	stateOut.pos.y=sin(phi)*radius;
	stateOut.pos.z=stateVec->pos.z;
	
	/*Compute the radius, longitude, and rotation amount for the velocity.
		(this equation has an extra term since the earth is rotating).*/
	radius=sqrt(stateVec->vel.x*stateVec->vel.x+stateVec->vel.y*stateVec->vel.y);
	phiVel=atan2_check(stateVec->vel.y,stateVec->vel.x);
	phiVel-=ghaRads;
	stateOut.vel.x=cos(phiVel)*radius+angularVelocity*stateOut.pos.y;
	stateOut.vel.y=sin(phiVel)*radius-angularVelocity*stateOut.pos.x;
	stateOut.vel.z=stateVec->vel.z;
	
	/*Finally, copy stateOut into stateVec.*/
	(*stateVec)=stateOut;
}

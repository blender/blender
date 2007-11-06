/*************************************************************************
 *									 *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.	 *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org 	 *
 *									 *
 * This library is free software; you can redistribute it and/or	 *
 * modify it under the terms of EITHER: 				 *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *	 Software Foundation; either version 2.1 of the License, or (at  *
 *	 your option) any later version. The text of the GNU Lesser	 *
 *	 General Public License is included with this library in the	 *
 *	 file LICENSE.TXT.						 *
 *   (2) The BSD-style license that is included with this library in	 *
 *	 the file LICENSE-BSD.TXT.					 *
 *									 *
 * This library is distributed in the hope that it will be useful,	 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of	 *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files	 *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.			 *
 *									 *
 *************************************************************************/

// C++ interface for everything


#ifndef _ODE_ODECPP_H_
#define _ODE_ODECPP_H_
#ifdef __cplusplus

#include <ode/error.h>


class dWorld {
  dWorldID _id;

  // intentionally undefined, don't use these
  dWorld (const dWorld &);
  void operator= (const dWorld &);

public:
  dWorld()
    { _id = dWorldCreate(); }
  ~dWorld()
    { dWorldDestroy (_id); }

  dWorldID id() const
    { return _id; }
  operator dWorldID() const
    { return _id; }

  void setGravity (dReal x, dReal y, dReal z)
    { dWorldSetGravity (_id,x,y,z); }
  void getGravity (dVector3 g) const
    { dWorldGetGravity (_id,g); }

  void setERP (dReal erp)
    { dWorldSetERP(_id, erp); }
  dReal getERP() const
    { return dWorldGetERP(_id); }

  void setCFM (dReal cfm)
    { dWorldSetCFM(_id, cfm); }
  dReal getCFM() const
    { return dWorldGetCFM(_id); }

  void step (dReal stepsize)
    { dWorldStep (_id,stepsize); }

  void impulseToForce (dReal stepsize, dReal ix, dReal iy, dReal iz,
		       dVector3 force)
    { dWorldImpulseToForce (_id,stepsize,ix,iy,iz,force); }
};


class dBody {
  dBodyID _id;

  // intentionally undefined, don't use these
  dBody (const dBody &);
  void operator= (const dBody &);

public:
  dBody()
    { _id = 0; }
  dBody (dWorldID world)
    { _id = dBodyCreate (world); }
  ~dBody()
    { if (_id) dBodyDestroy (_id); }

  void create (dWorldID world) {
    if (_id) dBodyDestroy (_id);
    _id = dBodyCreate (world);
  }

  dBodyID id() const
    { return _id; }
  operator dBodyID() const
    { return _id; }

  void setData (void *data)
    { dBodySetData (_id,data); }
  void *getData() const
    { return dBodyGetData (_id); }

  void setPosition (dReal x, dReal y, dReal z)
    { dBodySetPosition (_id,x,y,z); }
  void setRotation (const dMatrix3 R)
    { dBodySetRotation (_id,R); }
  void setQuaternion (const dQuaternion q)
    { dBodySetQuaternion (_id,q); }
  void setLinearVel  (dReal x, dReal y, dReal z)
    { dBodySetLinearVel (_id,x,y,z); }
  void setAngularVel (dReal x, dReal y, dReal z)
    { dBodySetAngularVel (_id,x,y,z); }

  const dReal * getPosition() const
    { return dBodyGetPosition (_id); }
  const dReal * getRotation() const
    { return dBodyGetRotation (_id); }
  const dReal * getQuaternion() const
    { return dBodyGetQuaternion (_id); }
  const dReal * getLinearVel() const
    { return dBodyGetLinearVel (_id); }
  const dReal * getAngularVel() const
    { return dBodyGetAngularVel (_id); }

  void setMass (const dMass *mass)
    { dBodySetMass (_id,mass); }
  void getMass (dMass *mass) const
    { dBodyGetMass (_id,mass); }

  void addForce (dReal fx, dReal fy, dReal fz)
    { dBodyAddForce (_id, fx, fy, fz); }
  void addTorque (dReal fx, dReal fy, dReal fz)
    { dBodyAddTorque (_id, fx, fy, fz); }
  void addRelForce (dReal fx, dReal fy, dReal fz)
    { dBodyAddRelForce (_id, fx, fy, fz); }
  void addRelTorque (dReal fx, dReal fy, dReal fz)
    { dBodyAddRelTorque (_id, fx, fy, fz); }
  void addForceAtPos (dReal fx, dReal fy, dReal fz,
		      dReal px, dReal py, dReal pz)
    { dBodyAddForceAtPos (_id, fx, fy, fz, px, py, pz); }
  void addForceAtRelPos (dReal fx, dReal fy, dReal fz,
		      dReal px, dReal py, dReal pz)
    { dBodyAddForceAtRelPos (_id, fx, fy, fz, px, py, pz); }
  void addRelForceAtPos (dReal fx, dReal fy, dReal fz,
			 dReal px, dReal py, dReal pz)
    { dBodyAddRelForceAtPos (_id, fx, fy, fz, px, py, pz); }
  void addRelForceAtRelPos (dReal fx, dReal fy, dReal fz,
			    dReal px, dReal py, dReal pz)
    { dBodyAddRelForceAtRelPos (_id, fx, fy, fz, px, py, pz); }

  const dReal * getForce() const
    { return dBodyGetForce(_id); }
  const dReal * getTorque() const
    { return dBodyGetTorque(_id); }
  void setForce (dReal x, dReal y, dReal z)
    { dBodySetForce (_id,x,y,z); }
  void setTorque (dReal x, dReal y, dReal z)
    { dBodySetTorque (_id,x,y,z); }

  void enable()
    { dBodyEnable (_id); }
  void disable()
    { dBodyDisable (_id); }
  int isEnabled() const
    { return dBodyIsEnabled (_id); }

  void getRelPointPos (dReal px, dReal py, dReal pz, dVector3 result) const
    { dBodyGetRelPointPos (_id, px, py, pz, result); }
  void getRelPointVel (dReal px, dReal py, dReal pz, dVector3 result) const
    { dBodyGetRelPointVel (_id, px, py, pz, result); }
  void getPointVel (dReal px, dReal py, dReal pz, dVector3 result) const
    { dBodyGetPointVel (_id,px,py,pz,result); }
  void getPosRelPoint (dReal px, dReal py, dReal pz, dVector3 result) const
    { dBodyGetPosRelPoint (_id,px,py,pz,result); }
  void vectorToWorld (dReal px, dReal py, dReal pz, dVector3 result) const
    { dBodyVectorToWorld (_id,px,py,pz,result); }
  void vectorFromWorld (dReal px, dReal py, dReal pz, dVector3 result) const
    { dBodyVectorFromWorld (_id,px,py,pz,result); }

  void setFiniteRotationMode (int mode)
    { dBodySetFiniteRotationMode (_id, mode); }
  void setFiniteRotationAxis (dReal x, dReal y, dReal z)
    { dBodySetFiniteRotationAxis (_id, x, y, z); }

  int getFiniteRotationMode() const
    { return dBodyGetFiniteRotationMode (_id); }
  void getFiniteRotationAxis (dVector3 result) const
    { dBodyGetFiniteRotationAxis (_id, result); }

  int getNumJoints() const
    { return dBodyGetNumJoints (_id); }
  dJointID getJoint (int index) const
    { return dBodyGetJoint (_id, index); }

  void setGravityMode (int mode)
    { dBodySetGravityMode (_id,mode); }
  int getGravityMode() const
    { return dBodyGetGravityMode (_id); }

  int isConnectedTo (dBodyID body) const
    { return dAreConnected (_id, body); }
};


class dJointGroup {
  dJointGroupID _id;

  // intentionally undefined, don't use these
  dJointGroup (const dJointGroup &);
  void operator= (const dJointGroup &);

public:
  dJointGroup (int dummy_arg=0)
    { _id = dJointGroupCreate (0); }
  ~dJointGroup()
    { dJointGroupDestroy (_id); }
  void create (int dummy_arg=0) {
    if (_id) dJointGroupDestroy (_id);
    _id = dJointGroupCreate (0);
  }

  dJointGroupID id() const
    { return _id; }
  operator dJointGroupID() const
    { return _id; }

  void empty()
    { dJointGroupEmpty (_id); }
};


class dJoint {
private:
  // intentionally undefined, don't use these
  dJoint (const dJoint &) ;
  void operator= (const dJoint &);

protected:
  dJointID _id;

public:
  dJoint()
    { _id = 0; }
  ~dJoint()
    { if (_id) dJointDestroy (_id); }

  dJointID id() const
    { return _id; }
  operator dJointID() const
    { return _id; }

  void attach (dBodyID body1, dBodyID body2)
    { dJointAttach (_id, body1, body2); }

  void setData (void *data)
    { dJointSetData (_id, data); }
  void *getData (void *data) const
    { return dJointGetData (_id); }

  int getType() const
    { return dJointGetType (_id); }

  dBodyID getBody (int index) const
    { return dJointGetBody (_id, index); }
};


class dBallJoint : public dJoint {
private:
  // intentionally undefined, don't use these
  dBallJoint (const dBallJoint &);
  void operator= (const dBallJoint &);

public:
  dBallJoint() { }
  dBallJoint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateBall (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateBall (world, group);
  }

  void setAnchor (dReal x, dReal y, dReal z)
    { dJointSetBallAnchor (_id, x, y, z); }
  void getAnchor (dVector3 result) const
    { dJointGetBallAnchor (_id, result); }
} ;


class dHingeJoint : public dJoint {
  // intentionally undefined, don't use these
  dHingeJoint (const dHingeJoint &);
  void operator = (const dHingeJoint &);

public:
  dHingeJoint() { }
  dHingeJoint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateHinge (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateHinge (world, group);
  }

  void setAnchor (dReal x, dReal y, dReal z)
    { dJointSetHingeAnchor (_id, x, y, z); }
  void getAnchor (dVector3 result) const
    { dJointGetHingeAnchor (_id, result); }

  void setAxis (dReal x, dReal y, dReal z)
    { dJointSetHingeAxis (_id, x, y, z); }
  void getAxis (dVector3 result) const
    { dJointGetHingeAxis (_id, result); }

  dReal getAngle() const
    { return dJointGetHingeAngle (_id); }
  dReal getAngleRate() const
    { return dJointGetHingeAngleRate (_id); }

  void setParam (int parameter, dReal value)
    { dJointSetHingeParam (_id, parameter, value); }
  dReal getParam (int parameter) const
    { return dJointGetHingeParam (_id, parameter); }
};


class dSliderJoint : public dJoint {
  // intentionally undefined, don't use these
  dSliderJoint (const dSliderJoint &);
  void operator = (const dSliderJoint &);

public:
  dSliderJoint() { }
  dSliderJoint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateSlider (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateSlider (world, group);
  }

  void setAxis (dReal x, dReal y, dReal z)
    { dJointSetSliderAxis (_id, x, y, z); }
  void getAxis (dVector3 result) const
    { dJointGetSliderAxis (_id, result); }

  dReal getPosition() const
    { return dJointGetSliderPosition (_id); }
  dReal getPositionRate() const
    { return dJointGetSliderPositionRate (_id); }

  void setParam (int parameter, dReal value)
    { dJointSetSliderParam (_id, parameter, value); }
  dReal getParam (int parameter) const
    { return dJointGetSliderParam (_id, parameter); }
};


class dUniversalJoint : public dJoint {
  // intentionally undefined, don't use these
  dUniversalJoint (const dUniversalJoint &);
  void operator = (const dUniversalJoint &);

public:
  dUniversalJoint() { }
  dUniversalJoint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateUniversal (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateUniversal (world, group);
  }

  void setAnchor (dReal x, dReal y, dReal z)
    { dJointSetUniversalAnchor (_id, x, y, z); }
  void setAxis1 (dReal x, dReal y, dReal z)
    { dJointSetUniversalAxis1 (_id, x, y, z); }
  void setAxis2 (dReal x, dReal y, dReal z)
    { dJointSetUniversalAxis2 (_id, x, y, z); }

  void getAnchor (dVector3 result) const
    { dJointGetUniversalAnchor (_id, result); }
  void getAxis1 (dVector3 result) const
    { dJointGetUniversalAxis1 (_id, result); }
  void getAxis2 (dVector3 result) const
    { dJointGetUniversalAxis2 (_id, result); }
};


class dHinge2Joint : public dJoint {
  // intentionally undefined, don't use these
  dHinge2Joint (const dHinge2Joint &);
  void operator = (const dHinge2Joint &);

public:
  dHinge2Joint() { }
  dHinge2Joint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateHinge2 (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateHinge2 (world, group);
  }

  void setAnchor (dReal x, dReal y, dReal z)
    { dJointSetHinge2Anchor (_id, x, y, z); }
  void setAxis1 (dReal x, dReal y, dReal z)
    { dJointSetHinge2Axis1 (_id, x, y, z); }
  void setAxis2 (dReal x, dReal y, dReal z)
    { dJointSetHinge2Axis2 (_id, x, y, z); }

  void getAnchor (dVector3 result) const
    { dJointGetHinge2Anchor (_id, result); }
  void getAxis1 (dVector3 result) const
    { dJointGetHinge2Axis1 (_id, result); }
  void getAxis2 (dVector3 result) const
    { dJointGetHinge2Axis2 (_id, result); }

  dReal getAngle1() const
    { return dJointGetHinge2Angle1 (_id); }
  dReal getAngle1Rate() const
    { return dJointGetHinge2Angle1Rate (_id); }
  dReal getAngle2Rate() const
    { return dJointGetHinge2Angle2Rate (_id); }

  void setParam (int parameter, dReal value)
    { dJointSetHinge2Param (_id, parameter, value); }
  dReal getParam (int parameter) const
    { return dJointGetHinge2Param (_id, parameter); }
};


class dFixedJoint : public dJoint {
  // intentionally undefined, don't use these
  dFixedJoint (const dFixedJoint &);
  void operator = (const dFixedJoint &);

public:
  dFixedJoint() { }
  dFixedJoint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateFixed (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateFixed (world, group);
  }

  void set()
    { dJointSetFixed (_id); }
};


class dContactJoint : public dJoint {
  // intentionally undefined, don't use these
  dContactJoint (const dContactJoint &);
  void operator = (const dContactJoint &);

public:
  dContactJoint() { }
  dContactJoint (dWorldID world, dJointGroupID group, dContact *contact)
    { _id = dJointCreateContact (world, group, contact); }

  void create (dWorldID world, dJointGroupID group, dContact *contact) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateContact (world, group, contact);
  }
};


class dNullJoint : public dJoint {
  // intentionally undefined, don't use these
  dNullJoint (const dNullJoint &);
  void operator = (const dNullJoint &);

public:
  dNullJoint() { }
  dNullJoint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateNull (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateNull (world, group);
  }
};


class dAMotorJoint : public dJoint {
  // intentionally undefined, don't use these
  dAMotorJoint (const dAMotorJoint &);
  void operator = (const dAMotorJoint &);

public:
  dAMotorJoint() { }
  dAMotorJoint (dWorldID world, dJointGroupID group=0)
    { _id = dJointCreateAMotor (world, group); }

  void create (dWorldID world, dJointGroupID group=0) {
    if (_id) dJointDestroy (_id);
    _id = dJointCreateAMotor (world, group);
  }

  void setMode (int mode)
    { dJointSetAMotorMode (_id, mode); }
  int getMode() const
    { return dJointGetAMotorMode (_id); }

  void setNumAxes (int num)
    { dJointSetAMotorNumAxes (_id, num); }
  int getNumAxes() const
    { return dJointGetAMotorNumAxes (_id); }

  void setAxis (int anum, int rel, dReal x, dReal y, dReal z)
    { dJointSetAMotorAxis (_id, anum, rel, x, y, z); }
  void getAxis (int anum, dVector3 result) const
    { dJointGetAMotorAxis (_id, anum, result); }
  int getAxisRel (int anum) const
    { return dJointGetAMotorAxisRel (_id, anum); }

  void setAngle (int anum, dReal angle)
    { dJointSetAMotorAngle (_id, anum, angle); }
  dReal getAngle (int anum) const
    { return dJointGetAMotorAngle (_id, anum); }
  dReal getAngleRate (int anum)
    { return dJointGetAMotorAngleRate (_id,anum); }

  void setParam (int parameter, dReal value)
    { dJointSetAMotorParam (_id, parameter, value); }
  dReal getParam (int parameter) const
    { return dJointGetAMotorParam (_id, parameter); }
};


class dGeom {
  // intentionally undefined, don't use these
  dGeom (dGeom &);
  void operator= (dGeom &);

protected:
  dGeomID _id;

public:
  dGeom()
    { _id = 0; }
  ~dGeom()
    { if (_id) dGeomDestroy (_id); }

  dGeomID id() const
    { return _id; }
  operator dGeomID() const
    { return _id; }

  void destroy() {
    if (_id) dGeomDestroy (_id);
    _id = 0;
  }

  int getClass() const
    { return dGeomGetClass (_id); }

  void setData (void *data)
    { dGeomSetData (_id,data); }
  void *getData() const
    { return dGeomGetData (_id); }

  void setBody (dBodyID b)
    { dGeomSetBody (_id,b); }
  dBodyID getBody() const
    { return dGeomGetBody (_id); }

  void setPosition (dReal x, dReal y, dReal z)
    { dGeomSetPosition (_id,x,y,z); }
  const dReal * getPosition() const
    { return dGeomGetPosition (_id); }

  void setRotation (const dMatrix3 R)
    { dGeomSetRotation (_id,R); }
  const dReal * getRotation() const
    { return dGeomGetRotation (_id); }

  void getAABB (dReal aabb[6]) const
    { dGeomGetAABB (_id, aabb); }
  const dReal *getSpaceAABB() const
    { return dGeomGetSpaceAABB (_id); }
};


class dSpace {
  // intentionally undefined, don't use these
  dSpace (dSpace &);
  void operator= (dSpace &);

protected:
  dSpaceID _id;

  // the default constructor is protected so that you
  // can't instance this class. you must instance one
  // of its subclasses instead.
  dSpace () { _id = 0; }

public:
  ~dSpace()
    { dSpaceDestroy (_id); }

  dSpaceID id() const
    { return _id; }
  operator dSpaceID() const
    { return _id; }

  void add (dGeomID x)
    { dSpaceAdd (_id, x); }
  void remove (dGeomID x)
    { dSpaceRemove (_id, x); }
  int query (dGeomID x)
    { return dSpaceQuery (_id,x); }

  void collide (void *data, dNearCallback *callback)
    { dSpaceCollide (_id,data,callback); }
};


class dSimpleSpace : public dSpace {
  // intentionally undefined, don't use these
  dSimpleSpace (dSimpleSpace &);
  void operator= (dSimpleSpace &);

public:
  dSimpleSpace ()
    { _id = dSimpleSpaceCreate(); }
};


class dHashSpace : public dSpace {
  // intentionally undefined, don't use these
  dHashSpace (dHashSpace &);
  void operator= (dHashSpace &);

public:
  dHashSpace ()
    { _id = dHashSpaceCreate(); }
  void setLevels (int minlevel, int maxlevel)
    { dHashSpaceSetLevels (_id,minlevel,maxlevel); }
};


class dSphere : public dGeom {
  // intentionally undefined, don't use these
  dSphere (dSphere &);
  void operator= (dSphere &);

public:
  dSphere () { }
  dSphere (dSpaceID space, dReal radius)
    { _id = dCreateSphere (space, radius); }

  void create (dSpaceID space, dReal radius) {
    if (_id) dGeomDestroy (_id);
    _id = dCreateSphere (space, radius);
  }

  void setRadius (dReal radius)
    { dGeomSphereSetRadius (_id, radius); }
  dReal getRadius() const
    { return dGeomSphereGetRadius (_id); }
};


class dBox : public dGeom {
  // intentionally undefined, don't use these
  dBox (dBox &);
  void operator= (dBox &);

public:
  dBox () { }
  dBox (dSpaceID space, dReal lx, dReal ly, dReal lz)
    { _id = dCreateBox (space,lx,ly,lz); }

  void create (dSpaceID space, dReal lx, dReal ly, dReal lz) {
    if (_id) dGeomDestroy (_id);
    _id = dCreateBox (space,lx,ly,lz);
  }

  void setLengths (dReal lx, dReal ly, dReal lz)
    { dGeomBoxSetLengths (_id, lx, ly, lz); }
  void getLengths (dVector3 result) const
    { dGeomBoxGetLengths (_id,result); }
};


class dPlane : public dGeom {
  // intentionally undefined, don't use these
  dPlane (dPlane &);
  void operator= (dPlane &);

public:
  dPlane() { }
  dPlane (dSpaceID space, dReal a, dReal b, dReal c, dReal d)
    { _id = dCreatePlane (space,a,b,c,d); }

  void create (dSpaceID space, dReal a, dReal b, dReal c, dReal d) {
    if (_id) dGeomDestroy (_id);
    _id = dCreatePlane (space,a,b,c,d);
  }

  void setParams (dReal a, dReal b, dReal c, dReal d)
    { dGeomPlaneSetParams (_id, a, b, c, d); }
  void getParams (dVector4 result) const
    { dGeomPlaneGetParams (_id,result); }
};


class dCCylinder : public dGeom {
  // intentionally undefined, don't use these
  dCCylinder (dCCylinder &);
  void operator= (dCCylinder &);

public:
  dCCylinder() { }
  dCCylinder (dSpaceID space, dReal radius, dReal length)
    { _id = dCreateCCylinder (space,radius,length); }

  void create (dSpaceID space, dReal radius, dReal length) {
    if (_id) dGeomDestroy (_id);
    _id = dCreateCCylinder (space,radius,length);
  }

  void setParams (dReal radius, dReal length)
    { dGeomCCylinderSetParams (_id, radius, length); }
  void getParams (dReal *radius, dReal *length) const
    { dGeomCCylinderGetParams (_id,radius,length); }
};


class dGeomGroup : public dGeom {
  // intentionally undefined, don't use these
  dGeomGroup (dGeomGroup &);
  void operator= (dGeomGroup &);

public:
  dGeomGroup() { }
  dGeomGroup (dSpaceID space)
    { _id = dCreateGeomGroup (space); }

  void create (dSpaceID space=0) {
    if (_id) dGeomDestroy (_id);
    _id = dCreateGeomGroup (space);
  }

  void add (dGeomID x)
    { dGeomGroupAdd (_id, x); }
  void remove (dGeomID x)
    { dGeomGroupRemove (_id, x); }

  int getNumGeoms() const
    { return dGeomGroupGetNumGeoms (_id); }
  dGeomID getGeom (int i) const
    { return dGeomGroupGetGeom (_id, i); }
};


class dGeomTransform : public dGeom {
  // intentionally undefined, don't use these
  dGeomTransform (dGeomTransform &);
  void operator= (dGeomTransform &);

public:
  dGeomTransform() { }
  dGeomTransform (dSpaceID space)
    { _id = dCreateGeomTransform (space); }

  void create (dSpaceID space=0) {
    if (_id) dGeomDestroy (_id);
    _id = dCreateGeomTransform (space);
  }

  void setGeom (dGeomID geom)
    { dGeomTransformSetGeom (_id, geom); }
  dGeomID getGeom() const
    { return dGeomTransformGetGeom (_id); }

  void setCleanup (int mode)
    { dGeomTransformSetCleanup (_id,mode); }
  int getCleanup (dGeomID g)
    { return dGeomTransformGetCleanup (_id); }

  void setInfo (int mode)
    { dGeomTransformSetInfo (_id,mode); }
  int getInfo()
    { return dGeomTransformGetInfo (_id); }
};

#endif

#endif


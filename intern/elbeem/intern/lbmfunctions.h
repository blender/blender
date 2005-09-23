/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann Solver templated helper functions
 * 
 *****************************************************************************/
#ifndef LBMFUNCTIONS_H



#if LBM_USE_GUI==1
#define USE_GLUTILITIES
#include "../gui/gui_utilities.h"

//! display a single node
template<typename D> 
void 
debugDisplayNode(fluidDispSettings *dispset, D *lbm, typename D::CellIdentifier cell ) {
	//debugOut(" DD: "<<cell->getAsString() , 10);
	ntlVec3Gfx org      = lbm->getCellOrigin( cell );
	ntlVec3Gfx halfsize = lbm->getCellSize( cell );
	int    set      = lbm->getCellSet( cell );
	//debugOut(" DD: "<<cell->getAsString()<<" "<< (dispset->type) , 10);

	bool     showcell = true;
	int      linewidth = 1;
	ntlColor col(0.5);
	LbmFloat cscale = dispset->scale;

#define DRAWDISPCUBE(col,scale) \
	{	glLineWidth( linewidth ); \
	  glColor3f( (col)[0], (col)[1], (col)[2]); \
		ntlVec3Gfx s = org-(halfsize * (scale)); \
		ntlVec3Gfx e = org+(halfsize * (scale)); \
		drawCubeWire( s,e ); }

	switch(dispset->type) {
		case FLUIDDISPNothing: {
				showcell = false;
			} break;
		case FLUIDDISPCelltypes: {
				CellFlagType flag = lbm->getCellFlag(cell, set );
				cscale = 0.5;

				if(flag& CFInvalid  ) { if(!guiShowInvalid  ) return; }
				if(flag& CFUnused   ) { if(!guiShowInvalid  ) return; }
				if(flag& CFEmpty    ) { if(!guiShowEmpty    ) return; }
				if(flag& CFInter    ) { if(!guiShowInterface) return; }
				if(flag& CFNoDelete ) { if(!guiShowNoDelete ) return; }
				if(flag& CFBnd      ) { if(!guiShowBnd      ) return; }

				// only dismiss one of these types 
 				if(flag& CFGrFromCoarse)  { if(!guiShowCoarseInner  ) return; } // inner not really interesting
				else
				if(flag& CFGrFromFine) { if(!guiShowCoarseBorder ) return; }
				else
				if(flag& CFFluid    )    { if(!guiShowFluid    ) return; }

				if(flag& CFNoDelete) { // debug, mark nodel cells
					ntlColor ccol(0.7,0.0,0.0);
					DRAWDISPCUBE(ccol, 0.1);
				}
				if(flag& CFPersistMask) { // mark persistent flags
					ntlColor ccol(0.5);
					DRAWDISPCUBE(ccol, 0.125);
				}
				if(flag& CFNoBndFluid) { // mark persistent flags
					ntlColor ccol(0,0,1);
					DRAWDISPCUBE(ccol, 0.075);
				}

				/*if(flag& CFAccelerator) {
					cscale = 0.55;
					col = ntlColor(0,1,0);
				} */
				if(flag& CFInvalid) {
					cscale = 0.50;
					col = ntlColor(0.0,0,0.0);
				}
				/*else if(flag& CFSpeedSet) {
					cscale = 0.55;
					col = ntlColor(0.2,1,0.2);
				}*/
				else if(flag& CFBnd) {
					cscale = 0.59;
					col = ntlColor(0.4);
				}

				else if(flag& CFInter) {
					cscale = 0.55;
					col = ntlColor(0,1,1);

				} else if(flag& CFGrFromCoarse) {
					// draw as - with marker
					ntlColor col2(0.0,1.0,0.3);
					DRAWDISPCUBE(col2, 0.1);
					cscale = 0.5;
					showcell=false; // DEBUG
				}
				else if(flag& CFFluid) {
					cscale = 0.5;
					if(flag& CFGrToFine) {
						ntlColor col2(0.5,0.0,0.5);
						DRAWDISPCUBE(col2, 0.1);
						col = ntlColor(0,0,1);
					}
					if(flag& CFGrFromFine) {
						ntlColor col2(1.0,1.0,0.0);
						DRAWDISPCUBE(col2, 0.1);
						col = ntlColor(0,0,1);
					} else if(flag& CFGrFromCoarse) {
						// draw as fluid with marker
						ntlColor col2(0.0,1.0,0.3);
						DRAWDISPCUBE(col2, 0.1);
						col = ntlColor(0,0,1);
					} else {
						col = ntlColor(0,0,1);
					}
				}
				else if(flag& CFEmpty) {
					showcell=false;
				}

			} break;
		case FLUIDDISPVelocities: {
				// dont use cube display
				LbmVec vel = lbm->getCellVelocity( cell, set );
				glBegin(GL_LINES);
				glColor3f( 0.0,0.0,0.0 );
				glVertex3f( org[0], org[1], org[2] );
				org += vec2G(vel * 10.0 * cscale);
				glColor3f( 1.0,1.0,1.0 );
				glVertex3f( org[0], org[1], org[2] );
				glEnd();
				showcell = false;
			} break;
		case FLUIDDISPCellfills: {
				CellFlagType flag = lbm->getCellFlag( cell,set );
				cscale = 0.5;

				if(flag& CFFluid) {
					cscale = 0.75;
					col = ntlColor(0,0,0.5);
				}
				else if(flag& CFInter) {
					cscale = 0.75 * lbm->getCellMass(cell,set);
					col = ntlColor(0,1,1);
				}
				else {
					showcell=false;
				}

					if( ABS(lbm->getCellMass(cell,set)) < 10.0 ) {
						cscale = 0.75 * lbm->getCellMass(cell,set);
					} else {
						showcell = false;
					}
					if(cscale>0.0) {
						col = ntlColor(0,1,1);
					} else {
						col = ntlColor(1,1,0);
					}
			// TODO
			} break;
		case FLUIDDISPDensity: {
				LbmFloat rho = lbm->getCellDensity(cell,set);
				cscale = rho*rho * 0.25;
				col = ntlColor( MIN(0.5+cscale,1.0) , MIN(0.0+cscale,1.0), MIN(0.0+cscale,1.0) );
				cscale *= 2.0;
			} break;
		case FLUIDDISPGrid: {
				cscale = 0.59;
				col = ntlColor(1.0);
			} break;
		default: {
				cscale = 0.5;
				col = ntlColor(1.0,0.0,0.0);
			} break;
	}

	if(!showcell) return;
	DRAWDISPCUBE(col, cscale);
}

//! debug display function
//  D has to implement the CellIterator interface
template<typename D>
void 
lbmDebugDisplay(fluidDispSettings *dispset, D *lbm) {
	//je nach solver...?
	if(!dispset->on) return;
	glDisable( GL_LIGHTING ); // dont light lines

	typename D::CellIdentifier cid = lbm->getFirstCell();
	for(; lbm->noEndCell( cid );
	      lbm->advanceCell( cid ) ) {
		// display...
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
		::debugDisplayNode<>(dispset, lbm, cid );
#else
		debugDisplayNode<D>(dispset, lbm, cid );
#endif
	}
	delete cid;

	glEnable( GL_LIGHTING ); // dont light lines
}

//! debug display function
//  D has to implement the CellIterator interface
template<typename D>
void 
lbmMarkedCellDisplay(D *lbm) {
	fluidDispSettings dispset;
	// trick - display marked cells as grid displa -> white, big
	dispset.type = FLUIDDISPGrid;
	dispset.on = true;
	glDisable( GL_LIGHTING ); // dont light lines
	
	typename D::CellIdentifier cid = lbm->markedGetFirstCell();
	while(cid) {
	//for(; lbm->markedNoEndCell( cid );
	      //cid = lbm->markedAdvanceCell( cid ) ) {
		// display... FIXME? this is a bit inconvenient...
		//MarkedCellIdentifier *mid = dynamic_cast<MarkedCellIdentifier *>( cid );
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
		//::debugDisplayNode<>(&dispset, lbm, mid->mpCell );
		::debugDisplayNode<>(&dispset, lbm, cid );
#else
		//debugDisplayNode<D>(&dispset, lbm, mid->mpCell );
		debugDisplayNode<D>(&dispset, lbm, cid );
#endif
		cid = lbm->markedAdvanceCell();
	}
	delete cid;

	glEnable( GL_LIGHTING ); // dont light lines
}

#endif // LBM_USE_GUI

//! display a single node
template<typename D> 
void 
debugPrintNodeInfo(D *lbm, typename D::CellIdentifier cell, string printInfo,
		// force printing of one set? default = -1 = off
		int forceSet=-1) {
  bool printDF     = false;
  bool printRho    = false;
  bool printVel    = false;
  bool printFlag   = false;
  bool printGeom   = false;
  bool printMass=false;
	bool printBothSets = false;

	for(size_t i=0; i<printInfo.length()-0; i++) {
		char what = printInfo[i];
		switch(what) {
			case '+': // all on
								printDF = true; printRho = true; printVel = true; printFlag = true; printGeom = true; printMass = true; 
								printBothSets = true; break;
			case '-': // all off
								printDF = false; printRho = false; printVel = false; printFlag = false; printGeom = false; printMass = false; 
								printBothSets = false; break;
			case 'd': printDF = true; break;
			case 'r': printRho = true; break;
			case 'v': printVel = true; break;
			case 'f': printFlag = true; break;
			case 'g': printGeom = true; break;
			case 'm': printMass = true; break;
			case 's': printBothSets = true; break;
			default: 
				errFatal("debugPrintNodeInfo","Invalid node info id "<<what,SIMWORLD_GENERICERROR); return;
		}
	}

	ntlVec3Gfx org      = lbm->getCellOrigin( cell );
	ntlVec3Gfx halfsize = lbm->getCellSize( cell );
	int    set      = lbm->getCellSet( cell );
	debMsgStd("debugPrintNodeInfo",DM_NOTIFY, "Printing cell info '"<<printInfo<<"' for node: "<<cell->getAsString()<<" from "<<lbm->getName()<<" currSet:"<<set , 1);
	if(printGeom) debMsgStd("                  ",DM_MSG, "Org:"<<org<<" Halfsize:"<<halfsize<<" ", 1);

	int setmax = 2;
	if(!printBothSets) setmax = 1;
	if(forceSet>=0) setmax = 1;

	for(int s=0; s<setmax; s++) {
		int workset = set;
		if(s==1){ workset = (set^1); }		
		if(forceSet>=0) workset = forceSet;
		debMsgStd("                  ",DM_MSG, "Printing set:"<<workset<<" orgSet:"<<set, 1);
		
		if(printDF) {
			for(int l=0; l<lbm->getDfNum(); l++) { // FIXME ??
				debMsgStd("                  ",DM_MSG, "  Df"<<l<<": "<<lbm->getCellDf(cell,workset,l), 1);
			}
		}
		if(printRho) {
			debMsgStd("                  ",DM_MSG, "  Rho: "<<lbm->getCellDensity(cell,workset), 1);
		}
		if(printVel) {
			debMsgStd("                  ",DM_MSG, "  Vel: "<<lbm->getCellVelocity(cell,workset), 1);
		}
		if(printFlag) {
			CellFlagType flag = lbm->getCellFlag(cell,workset);
			debMsgStd("                  ",DM_MSG, "  Flg: "<< flag<<" "<<convertFlags2String( flag ) <<" "<<convertCellFlagType2String( flag ), 1);
		}
		if(printMass) {
			debMsgStd("                  ",DM_MSG, "  Mss: "<<lbm->getCellMass(cell,workset), 1);
		}
	}
}

#define LBMFUNCTIONS_H
#endif


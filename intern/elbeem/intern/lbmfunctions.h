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
#endif


#if LBM_USE_GUI==1

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


				if(flag& CFNoDelete) { // TEST SOLVER debug, mark nodel cells
					glLineWidth( linewidth );
					ntlColor col(0.7,0.0,0.0);
					glColor3f( col[0], col[1], col[2]);
					ntlVec3Gfx s = org-(halfsize * 0.1);
					ntlVec3Gfx e = org+(halfsize * 0.1);
					drawCubeWire( s,e );
				}

				/*if(flag& CFAccelerator) {
					cscale = 0.55;
					col = ntlColor(0,1,0);
					//showcell=false; // DEBUG
				} */
				if(flag& CFInvalid) {
					cscale = 0.50;
					col = ntlColor(0.0,0,0.0);
					//showcell=false; // DEBUG
				}
				/*else if(flag& CFSpeedSet) {
					cscale = 0.55;
					col = ntlColor(0.2,1,0.2);
					//showcell=false; // DEBUG
				}*/
				else if(flag& CFBnd) {
					cscale = 0.59;
					col = ntlColor(0.0);
					col = ntlColor(0.4); // DEBUG
					//if(lbm->getSizeZ()>2) { showcell=false; } // DEBUG, 3D no obstacles
				}

				/*else if(flag& CFIfFluid) { // TEST SOLVER if inner fluid if
					cscale = 0.55;
					col = ntlColor(0,1,0);
				}
				else if(flag& CFIfEmpty) { // TEST SOLVER if outer empty if
					cscale = 0.55;
					col = ntlColor(0,0.5,0.5);
				}*/
				else if(flag& CFInter) {
					cscale = 0.55;
					col = ntlColor(0,1,1);

				} else if(flag& CFGrFromCoarse) {
					// draw as - with marker
					ntlColor col2(0.0,1.0,0.3);
					glColor3f( col2[0], col2[1], col2[2]);
					ntlVec3Gfx s = org-(halfsize * 0.4);
					ntlVec3Gfx e = org+(halfsize * 0.4);
					drawCubeWire( s,e );
					cscale = 0.5;
					//col = ntlColor(0,0,1);
					showcell=false; // DEBUG
				}
				else if(flag& CFFluid) {
					cscale = 0.5;
					/*if(flag& CFCoarseInner) {
						col = ntlColor(0.3, 0.3, 1.0);
					} else */
					if(flag& CFGrToFine) {
						glLineWidth( linewidth );
						ntlColor col2(0.5,0.0,0.5);
						glColor3f( col2[0], col2[1], col2[2]);
						ntlVec3Gfx s = org-(halfsize * 0.31);
						ntlVec3Gfx e = org+(halfsize * 0.31);
						drawCubeWire( s,e );
						col = ntlColor(0,0,1);
					}
					if(flag& CFGrFromFine) {
						glLineWidth( linewidth );
						ntlColor col2(1.0,1.0,0.0);
						glColor3f( col2[0], col2[1], col2[2]);
						ntlVec3Gfx s = org-(halfsize * 0.56);
						ntlVec3Gfx e = org+(halfsize * 0.56);
						drawCubeWire( s,e );
						col = ntlColor(0,0,1);
					} else if(flag& CFGrFromCoarse) {
						// draw as fluid with marker
						ntlColor col2(0.0,1.0,0.3);
						glColor3f( col2[0], col2[1], col2[2]);
						ntlVec3Gfx s = org-(halfsize * 0.41);
						ntlVec3Gfx e = org+(halfsize * 0.41);
						drawCubeWire( s,e );
						col = ntlColor(0,0,1);
					} else {
						col = ntlColor(0,0,1);
						//showcell=false; // DEBUG
					}
				}
				else if(flag& CFEmpty) {
					showcell=false;
				}

				// smaller for new lbmqt
				//cscale *= 0.5;

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
	glLineWidth( linewidth );
	glColor4f( col[0], col[1], col[2], 0.0);

	ntlVec3Gfx s = org-(halfsize * cscale);
	ntlVec3Gfx e = org+(halfsize * cscale);
	//if(D::cDimension==2) {
		//s[2] = e[2] = (s[2]+e[2])*0.5;
	//}
	drawCubeWire( s,e );
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
	for(; lbm->markedNoEndCell( cid );
	      lbm->markedAdvanceCell( cid ) ) {
		// display... FIXME? this is a bit inconvenient...
		//MarkedCellIdentifier *mid = dynamic_cast<MarkedCellIdentifier *>( cid );
#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
		//::debugDisplayNode<>(&dispset, lbm, mid->mpCell );
		::debugDisplayNode<>(&dispset, lbm, cid );
#else
		//debugDisplayNode<D>(&dispset, lbm, mid->mpCell );
		debugDisplayNode<D>(&dispset, lbm, cid );
#endif
	}
	delete cid;

	glEnable( GL_LIGHTING ); // dont light lines
}

#endif

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
			default: errMsg("debugPrintNodeInfo","Invalid node info id "<<what); exit(1);
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


//
//  Filename         : Pow23GridDensityProvider.h
//  Author(s)        : Alexander Beels
//  Purpose          : Class to define a cell grid surrounding
//                     the projected image of a scene
//  Date of creation : 2011-2-8
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef POW23GRIDDENSITYPROVIDER_H
#define POW23GRIDDENSITYPROVIDER_H

#include "GridDensityProvider.h"

class Pow23GridDensityProvider : public GridDensityProvider {
	// Disallow copying and assignment
	Pow23GridDensityProvider (const Pow23GridDensityProvider& other);
	Pow23GridDensityProvider& operator= (const Pow23GridDensityProvider& other);

public:
	Pow23GridDensityProvider(OccluderSource& source, const real proscenium[4], unsigned numFaces);
	Pow23GridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox, const GridHelpers::Transform& transform, unsigned numFaces);
	Pow23GridDensityProvider(OccluderSource& source, unsigned numFaces);
	virtual ~Pow23GridDensityProvider ();

protected:
	unsigned numFaces;

private:
	void initialize (const real proscenium[4]); 
};

class Pow23GridDensityProviderFactory : public GridDensityProviderFactory {
public:
	Pow23GridDensityProviderFactory(unsigned numFaces);
	~Pow23GridDensityProviderFactory ();

	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const real proscenium[4]);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source, const BBox<Vec3r>& bbox, const GridHelpers::Transform& transform);
	auto_ptr<GridDensityProvider> newGridDensityProvider(OccluderSource& source);
protected:
	unsigned numFaces;
};

#endif // POW23GRIDDENSITYPROVIDER_H


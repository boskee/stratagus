//   ___________		     _________		      _____  __
//   \_	  _____/______   ____   ____ \_   ___ \____________ _/ ____\/  |_
//    |    __) \_  __ \_/ __ \_/ __ \/    \  \/\_  __ \__  \\   __\\   __\ 
//    |     \   |  | \/\  ___/\  ___/\     \____|  | \// __ \|  |   |  |
//    \___  /   |__|    \___  >\___  >\______  /|__|  (____  /__|   |__|
//	  \/		    \/	   \/	     \/		   \/
//  ______________________                           ______________________
//			  T H E   W A R   B E G I N S
//	   FreeCraft - A free fantasy real time strategy game engine
//
/**@name construct.h	-	The constructions headerfile. */
//
//	(c) Copyright 1998-2001 by Lutz Sammer
//
//	FreeCraft is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the License,
//	or (at your option) any later version.
//
//	FreeCraft is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	$Id$

#ifndef __CONSTRUCT_H__
#define __CONSTRUCT_H__

//@{

// FIXME: constructions must be configurable, referenced by indenifiers...
// FIXME: need support for more races.

/*----------------------------------------------------------------------------
--	Documentation
----------------------------------------------------------------------------*/

/**
**	@struct _construction_ construct.h
**
**	\#include "construct.h"
**
**	typedef struct _construction_ Construction;
**
**		Each building perhaps also units can have its own construction
**		frames. This construction frames are currently not animated,
**		this is planned for the future. What construction frames a
**		building has, is currently done by numbers see
**		UnitType::Construction. This will be soon changed to
**		identifiers.
**
**	The construction structure members:
**
**	Construction::Ident
**
**		FIXME: write the documentation
**
**	Construction::File[]
**
**		FIXME: write the documentation
**
**	Construction::Nr
**
**		Slot number of the construction, used for saving. This should
**		be removed, if we use symbol identifiers.
**
**	Construction::Width
**
**		FIXME: write the documentation
**
**	Construction::Height
**
**		FIXME: write the documentation
**
**	Construction::Sprite
**
**		FIXME: write the documentation
**
**	@todo	
**		Need ::ConstructionByName, ::TilesetByName, ...
**		Only fixed number of constructions supported, more than
**		a single construction is not supported, animated constructions
**		aren't supported.
*/

/*----------------------------------------------------------------------------
--	Includes
----------------------------------------------------------------------------*/

#include "tileset.h"
#include "video.h"

/*----------------------------------------------------------------------------
--	Declarations
----------------------------------------------------------------------------*/

    /// Construction shown during construction of a building
typedef struct _construction_ {
    //const void* OType;		/// Object type (future extensions)

    char*	Ident;			/// construction identifier
    char*	File[TilesetMax];	/// sprite file

    int		Nr;			/// Number for save

    int		Width;			/// sprite width
    int		Height;			/// sprite height

// --- FILLED UP ---

    Graphic*	Sprite;			/// construction sprite image
} Construction;

/*----------------------------------------------------------------------------
--	Macros
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
--	Functions
----------------------------------------------------------------------------*/

    ///	Initialize the constructions module
extern void InitConstructions(void);
    ///	Load the graphics for constructions
extern void LoadConstructions(void);
    /// Save current construction state
extern void SaveConstructions(FILE* file);
    ///	Clean up the constructions module
extern void CleanConstructions(void);
    ///	Draw a construction
extern void DrawConstruction(const Construction*,int image,int x,int y);
    /// Get construction by wc number
extern Construction* ConstructionByWcNum(int num);

    /// Register ccl features
extern void ConstructionCclRegister(void);

//@}

#endif	// !__CONSTRUCT_H__

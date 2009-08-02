/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "SchematicGroup.h"


//======================================================================

SchematicGroup::SchematicGroup()
{
	mSchematics.reserve(64);
}

//======================================================================

SchematicGroup::~SchematicGroup()
{
	SchematicsList::iterator it = mSchematics.begin();
	while(it != mSchematics.end())
	{
		delete(*it);
		mSchematics.erase(it);
		it = mSchematics.begin();
	}
}

//======================================================================


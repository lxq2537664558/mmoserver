/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/

#include "EVAbility.h"
#include "ZoneServer/Objects/Creature Object/CreatureObject.h"
#include "ZoneServer\Objects\Player Object\PlayerObject.h"
#include "Zoneserver/ObjectController/ObjectController.h"
#include "ZoneServer/ObjectController/ObjectControllerCommandMap.h"
#include "ZoneServer\Services\equipment\equipment_service.h"
#include "ZoneServer\WorldManager.h"

EVAbility::EVAbility(ObjectController* controller)
    : EnqueueValidator(controller)
{}

EVAbility::~EVAbility()
{}

bool EVAbility::validate(uint32 &reply1, uint32 &reply2, uint64 targetId, uint32 opcode, ObjectControllerCmdProperties*& cmdProperties)
{
    CreatureObject* creature = dynamic_cast<CreatureObject*>(mController->getObject());

    // check if we need to have an ability
    if(creature && cmdProperties && cmdProperties->mAbilityCrc != 0)
    {
		if(creature->getCreoGroup() == CreoGroup_Player){
			auto equipment_service = gWorldManager->getKernel()->GetServiceManager()->GetService<swganh::equipment::EquipmentService>("EquipmentService");
			auto ghost = dynamic_cast<PlayerObject*>(equipment_service->GetEquippedObject(creature, "ghost"));
		
			// check if the player has it
			if(!(ghost->verifyAbility(cmdProperties->mAbilityCrc)))
			{
				reply1 = 2;
				reply2 = 0;
				DLOG(info) << "EVAbility::validate no ability - opcode : " << opcode << "ability" << cmdProperties->mAbilityCrc;
				return(false);
			}
		}
		else 
		{
			reply1 = 2;
			reply2 = 0;
			DLOG(info) << "EVAbility::validate no Player !!!!!! : " << creature->getId();
			return false;
		}
    }

    return(true);
}


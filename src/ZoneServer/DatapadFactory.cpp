/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "DatapadFactory.h"
#include "Datapad.h"
#include "IntangibleObject.h"
#include "Item.h"
#include "ManufacturingSchematic.h"
#include "ObjectFactoryCallback.h"
#include "TangibleFactory.h"
#include "VehicleFactory.h"
#include "WaypointFactory.h"
#include "WaypointObject.h"
#include "WorldManager.h"

#include "LogManager/LogManager.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"
#include "Utils/utils.h"

#include <assert.h>

//=============================================================================

bool				DatapadFactory::mInsFlag    = false;
DatapadFactory*		DatapadFactory::mSingleton  = NULL;

//======================================================================================================================

DatapadFactory*	DatapadFactory::Init(Database* database)
{
	if(!mInsFlag)
	{
		mSingleton = new DatapadFactory(database);
		mInsFlag = true;
		return mSingleton;
	}
	else
		return mSingleton;
}

//=============================================================================

DatapadFactory::DatapadFactory(Database* database) : FactoryBase(database)
{
	mWaypointFactory = WaypointFactory::Init(mDatabase);

	_setupDatabindings();
}

//=============================================================================

DatapadFactory::~DatapadFactory()
{
	_destroyDatabindings();

	mInsFlag = false;
	delete(mSingleton);
}

//=============================================================================

void DatapadFactory::handleDatabaseJobComplete(void* ref,DatabaseResult* result)
{
	QueryContainerBase* asyncContainer = reinterpret_cast<QueryContainerBase*>(ref);

	switch(asyncContainer->mQueryType)
	{
		case DPFQuery_MainDatapadData:
		{
			//get the count of all Waypoints and Schematics
			Datapad* datapad = _createDatapad(result);

			QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,DPFQuery_ObjectCount,asyncContainer->mClient);
			asContainer->mObject = datapad;

			mDatabase->ExecuteSqlAsync(this,asContainer,"SELECT sf_getDatapadObjectCount(%"PRIu64")",datapad->getId());

		}
		break;

		case DPFQuery_ObjectCount:
		{
			Datapad* datapad = dynamic_cast<Datapad*>(asyncContainer->mObject);

			uint32 objectCount;
			DataBinding* binding = mDatabase->CreateDataBinding(1);

			binding->addField(DFT_uint32,0,4);
			result->GetNextRow(binding,&objectCount);

			datapad->setObjectLoadCounter(objectCount);

			if(objectCount != 0)
			{
				uint64 dtpId = datapad->getId();

				datapad->setLoadState(LoadState_Loading);

				// query contents
				QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(asyncContainer->mOfCallback,DPFQuery_Objects,asyncContainer->mClient);
				asContainer->mObject = datapad;

				mDatabase->ExecuteSqlAsync(this,asContainer,
						"(SELECT \'waypoints\',waypoints.waypoint_id FROM waypoints WHERE owner_id = %"PRIu64")"
						" UNION (SELECT \'manschematics\',items.id FROM items WHERE (parent_id=%"PRIu64"))"
						" UNION (SELECT \'vehicles\',vehicles.id FROM vehicles WHERE (parent=%"PRIu64"))"
						,dtpId-3,dtpId,dtpId);

			}
			else
			{
				datapad->setLoadState(LoadState_Loaded);
				asyncContainer->mOfCallback->handleObjectReady(datapad,asyncContainer->mClient);
			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case DPFQuery_ItemId:
		{
			uint64 id;
			DataBinding* binding = mDatabase->CreateDataBinding(1);

			binding->addField(DFT_uint64,0,8);
			result->GetNextRow(binding,&id);

			gTangibleFactory->requestObject(this,id,TanGroup_Item,0,asyncContainer->mClient);
			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case DPFQuery_Objects:
		{
			Datapad* datapad = dynamic_cast<Datapad*>(asyncContainer->mObject);
			datapad->mWaypointUpdateCounter = 0;
			datapad->mManSUpdateCounter = 0;

			Type1_QueryContainer queryContainer;

			DataBinding*	binding = mDatabase->CreateDataBinding(2);
			binding->addField(DFT_bstring,offsetof(Type1_QueryContainer,mString),64,0);
			binding->addField(DFT_uint64,offsetof(Type1_QueryContainer,mId),8,1);

			uint64 count = result->getRowCount();

			//InLoadingContainer* ilc = new(mILCPool.ordered_malloc()) InLoadingContainer(inventory,asyncContainer->mOfCallback,asyncContainer->mClient);
			//ilc->mLoadCounter = count;

			mObjectLoadMap.insert(std::make_pair(datapad->getId(),new(mILCPool.ordered_malloc()) InLoadingContainer(datapad,asyncContainer->mOfCallback,asyncContainer->mClient,static_cast<uint32>(count))));

			for(uint32 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&queryContainer);

				if(strcmp(queryContainer.mString.getAnsi(),"waypoints") == 0)
				{
					++datapad->mWaypointUpdateCounter;
					mWaypointFactory->requestObject(this,queryContainer.mId,0,0,asyncContainer->mClient);
				}

				else if(strcmp(queryContainer.mString.getAnsi(),"manschematics") == 0)
				{
					++datapad->mManSUpdateCounter;
					gTangibleFactory->requestObject(this,queryContainer.mId,TanGroup_Item,0,asyncContainer->mClient);

				}
				else if(strcmp(queryContainer.mString.getAnsi(),"vehicles") == 0)
				{
					//datapad counter gets updated in vehicle factory
					gVehicleFactory->requestObject(this,queryContainer.mId,0,0,asyncContainer->mClient);

				}

			}

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		default:break;
	}

	mQueryContainerPool.free(asyncContainer);

}

//=============================================================================

void DatapadFactory::requestObject(ObjectFactoryCallback* ofCallback,uint64 id,uint16 subGroup,uint16 subType,DispatchClient* client)
{
	mDatabase->ExecuteSqlAsync(this,new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(ofCallback,DPFQuery_MainDatapadData,client),
								"SELECT datapads.id,datapad_types.object_string,datapad_types.name,datapad_types.file"
								" FROM datapads INNER JOIN datapad_types ON (datapads.datapad_type = datapad_types.id)"
								" WHERE (datapads.id = %"PRIu64")",id);
}

//=============================================================================

Datapad* DatapadFactory::_createDatapad(DatabaseResult* result)
{
	Datapad* datapad = new Datapad();

	uint64 count = result->getRowCount();
	assert(count == 1);

	// get our results
	result->GetNextRow(mDatapadBinding,(void*)datapad);
	datapad->setParentId(datapad->mId - 3);

	return datapad;
}

//=============================================================================

void DatapadFactory::_setupDatabindings()
{
	// datapad binding
	mDatapadBinding = mDatabase->CreateDataBinding(4);
	mDatapadBinding->addField(DFT_uint64,offsetof(Datapad,mId),8,0);
	mDatapadBinding->addField(DFT_bstring,offsetof(Datapad,mModel),256,1);
	mDatapadBinding->addField(DFT_bstring,offsetof(Datapad,mName),64,2);
	mDatapadBinding->addField(DFT_bstring,offsetof(Datapad,mNameFile),64,3);
}

//=============================================================================

void DatapadFactory::_destroyDatabindings()
{
	mDatabase->DestroyDataBinding(mDatapadBinding);
}

//=============================================================================

void DatapadFactory::handleObjectReady(Object* object,DispatchClient* client)
{

	Datapad* datapad(0);
	uint64 theID(0);

	switch(object->getType())
	{
		case ObjType_Waypoint:
		{
			theID	= object->getParentId()+3;
			mIlc	= _getObject(theID);
			datapad = dynamic_cast<Datapad*>(mIlc->mObject);

			if(!mIlc)
			{
				gLogger->logMsg("DatapadFactory: Failed getting ilc");
				return;
			}
			mIlc->mLoadCounter--;

			datapad->addWaypoint(dynamic_cast<WaypointObject*>(object));
		}
		break;

		case ObjType_Tangible:
		{

			Item* item = dynamic_cast<Item*>(object);
			if(item->getItemType()== ItemType_ManSchematic)
			{
				theID	= object->getParentId();
				mIlc	= _getObject(theID);
				datapad = dynamic_cast<Datapad*>(mIlc->mObject);

				if(!mIlc)
				{
					gLogger->logMsg("DatapadFactory: Failed getting ilc");
					return;
				}

				//parentId of schematics is the datapad!
				//add the msco to the datapad

				datapad->addManufacturingSchematic(dynamic_cast<ManufacturingSchematic*>(object));

				//now load the associated item
				QueryContainerBase* asContainer = new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(this,DPFQuery_ItemId,mIlc->mClient);
				asContainer->mObject = datapad;
				asContainer->mId = item->getId();//queryContainer.mId;
				int8 sql[256];
				sprintf(sql,"SELECT items.id FROM items WHERE (parent_id=%"PRIu64")",item->getId());
				mDatabase->ExecuteSqlAsync(this,asContainer,sql);

				mObjectLoadMap.insert(std::make_pair(item->getId(),new(mILCPool.ordered_malloc()) InLoadingContainer(datapad,0,0,1)));

			}
			else
			{
				uint64 id						= object->getParentId();
				//we are loading the corresponding tangible object to have all the items attributes available for display/usage
				//find the corresponding schematic

				//temp mILc
				InLoadingContainer*mIlcDPad		= _getObject(id);
				if(!mIlcDPad)
				{
					gLogger->logMsg("DatapadFactory: Failed getting ilc");
					return;
				}
				datapad							= dynamic_cast<Datapad*>(mIlcDPad->mObject);
				_removeFromObjectLoadMap(id);
				mILCPool.free(mIlcDPad);

				//regular mIlc
				theID							= datapad->getId();
				mIlc							= _getObject(theID);

				if(!mIlc)
				{
					gLogger->logMsg("DatapadFactory: Failed getting ilc");
					return;
				}

				ManufacturingSchematic* schem	= datapad->getManufacturingSchematicById(id);

				//this is the item associated to the Man schematic
				//set the man schematic pointer and decrease the loadcount
				mIlc->mLoadCounter--;
				schem->setItem(dynamic_cast<Item*>(object));
			}


		}
		break;

		case ObjType_Intangible:
		{
			theID	= object->getParentId();
			mIlc	= _getObject(theID);
			if((datapad = dynamic_cast<Datapad*>(mIlc->mObject)))
			{
				if(!mIlc)
				{
					gLogger->logMsg("DatapadFactory: Failed getting ilc");
					return;
				}
				mIlc->mLoadCounter--;

				if(IntangibleObject* itno = dynamic_cast<IntangibleObject*>(object))
				{
					if(datapad->getCapacity())
					{
						datapad->addData(itno);
						Object* ob = gWorldManager->getObjectById(object->getId());
						if(!ob)
							gWorldManager->addObject(object,true);
					}
					else
					{
						gLogger->logMsg("DatapadFactory: Datapad at max Capacity!!!");
						delete(object);
					}
				}
			}

		}
		break;
		default:break;
	}

	if(!(mIlc->mLoadCounter))
	{
		if(!(_removeFromObjectLoadMap(theID)))
			gLogger->logMsg("DatapadFactory: Failed removing object from loadmap");

		mIlc->mOfCallback->handleObjectReady(datapad,mIlc->mClient);

		mILCPool.free(mIlc);
	}
}

//=============================================================================


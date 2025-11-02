#include "PhysicsHooks.h"
#include "Escort.h"
#include "Settings.h"
#include <kenshi/PhysicsActual.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Globals.h>
#include <core/Functions.h>

#include <boost/unordered_map.hpp>


boost::unordered_map<std::pair<std::string, int>, NXU::NxuPhysicsCollection*> physXCollectionCache;

NXU::NxuPhysicsCollection* (*loadPhysXResource_orig)(const std::string& filename, int type);// RVA = 0x7E4850
NXU::NxuPhysicsCollection* loadPhysXResource_hook(const std::string& filename, int type)
{
	if (Settings::GetCachePhysXColliders())
	{
		boost::unordered_map<std::pair<std::string, int>, NXU::NxuPhysicsCollection*>::iterator it = physXCollectionCache.find(std::make_pair(filename, type));
		NxVec3 scale;
		scale.x = 1;
		scale.y = 1;
		scale.z = 1;
		if (it == physXCollectionCache.end())
		{
			// I can't find a function for cloning a collection, so I create a scaled copy instead
			NXU::NxuPhysicsCollection* newCollection = loadPhysXResource_orig(filename, type);
			physXCollectionCache.emplace(std::make_pair(filename, type), scaleCopyCollection(newCollection, (filename + "_CLONE_RK").c_str(), scale, ((PhysicsActual*)ou->physics)->physicsSDK));

			return newCollection;
		}
		return scaleCopyCollection(it->second, filename.c_str(), scale, ((PhysicsActual*)ou->physics)->physicsSDK);
	}
	else
	{
		return loadPhysXResource_hook(filename, type);
	}
}

void PhysicsHooks::Init()
{
	loadPhysXResource_orig = Escort::JmpReplaceHook<NXU::NxuPhysicsCollection* (const std::string&, int)>((void*)GetRealAddress(&loadPhysXResource), loadPhysXResource_hook, 7);
}
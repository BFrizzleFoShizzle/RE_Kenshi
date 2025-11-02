
#include "OgreHooks.h"

#include "Settings.h"
#include "debug.h"
#include "OgreDDSCodec2.h"

#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreTexture.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Globals.h>
#include <kenshi/OptionsHolder.h>

Ogre::DDSCodec2 downscaleCodec = Ogre::DDSCodec2();

// the game creates a ResourceLoadingListener to downscale DDS files, but it reads the whole file. Here's an attempt at improving it.
// Note: Kenshi's lowest texture quality setting removes a max of 4 mips
class TextureDownscaler : public Ogre::ResourceLoadingListener
{
public:
	TextureDownscaler(Ogre::ResourceLoadingListener* ddsListener)
		: ddsListener(ddsListener)
	{
		
	}

	Ogre::DataStreamPtr resourceLoading(const Ogre::String& name, const Ogre::String& group, Ogre::Resource* resource)
	{
		Ogre::Texture* texture = dynamic_cast<Ogre::Texture*>(resource);

		// if it's a DDS texture and texture quality is below maximum, override Kenshi's texture resizing with our own (it's better)
		// don't downres GUI
		if (texture && name.size() > 4 && name.substr(name.size() - 4) == ".dds" && group != "GUI" && Settings::GetSkipUnusedMipmapReads() && options->ddsTextureMipMapGimping > 0)
		{
			return Ogre::DataStreamPtr();
		}
		// TODO ZDDS
		// 
		// Note: this isn't what causes texture file streams to get converted to MemoryDataStream
		return ddsListener->resourceLoading(name, group, resource);

		// old implementation - moved to resourceStreamOpened because it's applied more globally
		/*
		// we're trying to get our own result, let Ogre do it for us
		if (skip)
			return Ogre::DataStreamPtr();

		Ogre::Texture* texture = dynamic_cast<Ogre::Texture*>(resource);
		if (texture && name.size() > 4 && name.substr(name.size() - 4) == ".dds")
		{
			DebugLog("DDS resource loading: " + name + " " + group);
			// set skip flag so we don't recurse infinitely
			skip = true;
			Ogre::DataStreamPtr ddsFile = Ogre::ResourceGroupManager::getSingleton().openResource(name, group, true, resource);
			DebugLog(typeid(*ddsFile).name());
			skip = false;
			Ogre::DataStreamPtr out = downscaleCodec.downsize(ddsFile, Kenshi::GetTextureQuality());
			// if something goes wrong, default to old behaviour
			if(out.isNull())

			return out;
		}

		return ddsListener->resourceLoading(name, group, resource);
		*/
	}

	void resourceStreamOpened(const Ogre::String& name, const Ogre::String& group, Ogre::Resource* resource, Ogre::DataStreamPtr& dataStream)
	{
		Ogre::Texture* texture = dynamic_cast<Ogre::Texture*>(resource);
		// don't downres GUI
		// (texture == resource) means "is type texture or value null"
		if ((texture == resource) && name.size() > 4 && name.substr(name.size() - 4) == ".dds" && group != "GUI" && Settings::GetSkipUnusedMipmapReads() && options->ddsTextureMipMapGimping > 0)
		{
			int mipsDropped = options->ddsTextureMipMapGimping;
			// "_LO" decreases mips dropped by 1 (if it isn't zero)
			if (name.size() > 7 && name.substr(name.size() - 7) == "_LO.dds")
				mipsDropped = std::max(0, mipsDropped - 1);
			// Not in Landscape + without "_HI" decreases mips dropped by 1 (if it isn't zero) (stacks with _LO)
			if (group != "Landscape" && (name.size() < 7 || name.substr(name.size() - 7) != "_HI.dds"))
				mipsDropped = std::max(0, mipsDropped - 1);
			// I *think* this is what the code's doing
			// TODO double-check this
			if (group == "Overlaymaps")
				mipsDropped = 0;

			if (mipsDropped > 0)
			{
				Ogre::MemoryDataStreamPtr out = downscaleCodec.downsize(dataStream, mipsDropped);
				if (out.isNull())
				{
					// Downscaler has decided the file shouldn't be downscaled (or errored)
					size_t oldSize = dataStream->size();
					dataStream->seek(0);
					ddsListener->resourceStreamOpened(name, group, resource, dataStream);
					size_t newSize = dataStream->size();
					// if our downscaler doesn't downscale but vanilla does, that's a problem...
					if (oldSize != newSize)
						ErrorLog("Modded downscaler had incorrect behaviour for file: " + name + " " + group + " please report this bug to BFrizz");
				}
				else
				{
					dataStream = out;
				}
			}
			else
			{
				// no mips removed, convert to MemoryDataStream if needed and we're done
				if (dynamic_cast<Ogre::MemoryDataStream*>(dataStream.getPointer()) == nullptr)
					dataStream = Ogre::DataStreamPtr(new Ogre::MemoryDataStream(dataStream));
			}

			// the previous branches should have sorted things out as best as we can at this point
			return;
		}

		ddsListener->resourceStreamOpened(name, group, resource, dataStream);

		return;
	}

	bool resourceCollision(Ogre::Resource* resource, Ogre::ResourceManager* resourceManager)
	{
		return ddsListener->resourceCollision(resource, resourceManager);
	}

private:
	Ogre::ResourceLoadingListener* ddsListener;
};


// no DDS files are loaded before mod initialize, so this works
void OgreHooks::InitFinalizeMods()
{
	DebugLog("Registering LoadingListener");
	Ogre::ResourceGroupManager::getSingletonPtr()->setLoadingListener(OGRE_NEW TextureDownscaler(Ogre::ResourceGroupManager::getSingletonPtr()->getLoadingListener()));
}

void OgreHooks::Init()
{

}
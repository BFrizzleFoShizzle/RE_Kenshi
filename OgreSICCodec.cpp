#include "OgreSICCodec.h"
#include "Debug.h"

#include "Ogre/OgreLogManager.h"
#include "Ogre/OgreDataStream.h"
#include "Ogre/OgreTimer.h"

#include "SIC.h"

namespace Ogre
{
    SICCodec* SICCodec::msInstance = 0;

    void SICCodec::startup(void)
    {
        if (!msInstance)
        {
            LogManager::getSingleton().logMessage(LML_NORMAL, "SIC codec registering");

            msInstance = OGRE_NEW SICCodec();
            Codec::registerCodec(msInstance);
        }
    }

    void SICCodec::shutdown(void)
    {
        if (msInstance)
        {
            Codec::unregisterCodec(msInstance);
            OGRE_DELETE msInstance;
            msInstance = 0;
        }
    }

    SICCodec::SICCodec() :
        mType("sic")
    {
    }

    DataStreamPtr SICCodec::encode(MemoryDataStreamPtr& input, Codec::CodecDataPtr& pData) const
    {
        OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED,
            "SIC encoding not supported",
            "SICCodec::encode");
    }

    void SICCodec::encodeToFile(MemoryDataStreamPtr& input, const String& outFileName,
        Codec::CodecDataPtr& pData) const
    {
        OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED,
            "SIC encoding not supported",
            "SICCodec::encode");
    }

    static uint64_t decompressTime = 0;
    static uint64_t ioTime = 0;
    static uint64_t totalTime = 0;
    Codec::DecodeResult SICCodec::decode(DataStreamPtr& stream) const
    {
        //DebugLog("SIC codec called");
        Ogre::Timer timer;
        size_t remainingBytes = stream->size() - stream->tell();

        // TODO Ogre alloc?
        char* fileBytes; 

        MemoryDataStreamPtr memoryStream = stream.dynamicCast<MemoryDataStream>();
        if (!memoryStream.isNull())
        {
            fileBytes = (char*)memoryStream->getCurrentPtr();
        }
        else
        {
            // All vanilla textures seem to be loaded from MemoryDataStream but I'll leave this here anyway
            fileBytes = static_cast<char*>(OGRE_MALLOC(stream->size() - stream->tell(), Ogre::MEMCATEGORY_GENERAL));
            stream->read(fileBytes, remainingBytes);
        }

        ioTime += timer.getMicroseconds();
        SIC::SICHeader header = SIC::GetHeader(fileBytes, remainingBytes);
        size_t outSize = SIC::GetDecompressedSize(fileBytes, remainingBytes);
        
        // error
        if (outSize == 0)
        {
            ErrorLog("SICCodec Error decoding image");
            OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR,
                "Error decoding image",
                "SICCodec::decode");
        }

        ImageData* imgData = OGRE_NEW ImageData();

        imgData->depth = 1; // only 2D formats handled by this codec
        imgData->width = header.width;
        imgData->height = header.height;
        imgData->num_mipmaps = 0; // no mipmaps in non-DDS 
        imgData->flags = 0;
        if(header.encodeType == SIC::EncodeType::BGR)
            imgData->format = PF_BYTE_BGR;
        else if (header.encodeType == SIC::EncodeType::BGRA)
            imgData->format = PF_BYTE_BGRA;
        else if (header.encodeType == SIC::EncodeType::R8)
            imgData->format = PF_L8;
        else if (header.encodeType == SIC::EncodeType::R16)
            imgData->format = PF_L16;
        else
        {
            // error
            ErrorLog("SICCodec Error decoding image");
            OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR,
                "Error decoding image",
                "SICCodec::decode");
        }

        // decode/decompress
        char* outBytes = static_cast<char*>(OGRE_MALLOC(outSize, Ogre::MEMCATEGORY_GENERAL));
        size_t finalOutSize = SIC::Decompress(&outBytes[0], outSize, &fileBytes[0], remainingBytes);

        decompressTime += timer.getMicroseconds();

        // error
        if (finalOutSize == 0)
        {
            ErrorLog("SICCodec Error decoding image");
            OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR,
                "Error decoding image",
                "SICCodec::decode");
        }

        MemoryDataStreamPtr decompressedMemoryStream = MemoryDataStreamPtr(OGRE_NEW MemoryDataStream(outBytes, finalOutSize, true, true));

        // All vanilla textures seem to be loaded from MemoryDataStream but I'll leave this here anyway
        if (memoryStream.isNull())
            OGRE_FREE(fileBytes, Ogre::MEMCATEGORY_GENERAL);

        DecodeResult result;
        result.first = decompressedMemoryStream;
        result.second = CodecDataPtr(imgData);

        totalTime += timer.getMicroseconds();


        //DebugLog("Timers: " + std::to_string((long double)decompressTime / 1000.0) + " " + std::to_string((long double)totalTime / 1000.0)
        //   + " " + std::to_string((long double)ioTime / 1000.0));
        return result;
    }

    String SICCodec::getType() const
    {
        return mType;
    }

    String SICCodec::magicNumberToFileExt(const char* magicNumberPtr, size_t maxbytes) const
    {
        if (maxbytes >= sizeof(uint32))
        {
            uint32 fileType;

            memcpy(&fileType, magicNumberPtr, sizeof(uint32));

            // TODO this shouldn't be hard-coded
            if (0x74496853 == fileType)
            {
                return String("sic");
            }
        }

        return BLANKSTRING;
    }
}
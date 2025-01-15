#pragma once

// Loosely based on OgreDDSCodec
#include <Ogre/OgreImageCodec.h>

namespace Ogre
{
    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Image
    *  @{
    */

    /** "Shitty Image Codec" or "Simple Image Codec" - basically just Zstd-encoded raw pixels
    @remarks
        Faster to decode than BMP.ZST (less copies + reformatting)
    */
    class SICCodec : public ImageCodec
    {
    private:
        String mType;

        /// Single registered codec instance
        static SICCodec* msInstance;
    public:
        SICCodec();
        virtual ~SICCodec() { }

        /// @copydoc Codec::encode
        DataStreamPtr encode(MemoryDataStreamPtr& input, CodecDataPtr& pData) const;
        /// @copydoc Codec::encodeToFile
        void encodeToFile(MemoryDataStreamPtr& input, const String& outFileName,
            CodecDataPtr& pData) const;
        /// @copydoc Codec::decode
        DecodeResult decode(DataStreamPtr& input) const;

        /// @copydoc Codec::magicNumberToFileExt
        String magicNumberToFileExt(const char* magicNumberPtr, size_t maxbytes) const;

        virtual String getType(void) const;

        /// Static method to startup and register the CRN codec
        static void startup(void);
        /// Static method to shutdown and unregister the CRN codec
        static void shutdown(void);
    };
    /** @} */
    /** @} */

} // namespace


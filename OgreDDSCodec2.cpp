/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "OgreDDSCodec2.h"

#include <ogre/OgreStableHeaders.h>

#include <ogre/OgreRoot.h>
#include <ogre/OgreRenderSystem.h>
#include <ogre/OgreImage.h>
#include <ogre/OgreException.h>
#include <ogre/OgreLogManager.h>
#include <ogre/OgreTimer.h>
#include <ogre/OgreBitwise.h>
#include <ogre/OgrePixelBox.h>

#include "debug.h"

namespace Ogre {
    // Internal DDS structure definitions
#define FOURCC(c0, c1, c2, c3) (c0 | (c1 << 8) | (c2 << 16) | (c3 << 24))

#if OGRE_COMPILER == OGRE_COMPILER_MSVC
#pragma pack (push, 1)
#else
#pragma pack (1)
#endif

    // Nested structure
    struct DDSPixelFormat
    {
        uint32 size;
        uint32 flags;
        uint32 fourCC;
        uint32 rgbBits;
        uint32 redMask;
        uint32 greenMask;
        uint32 blueMask;
        uint32 alphaMask;
    };

    // Nested structure
    struct DDSCaps
    {
        uint32 caps1;
        uint32 caps2;
        uint32 caps3;
        uint32 caps4;
    };
    // Main header, note preceded by 'DDS '
    struct DDSHeader
    {
        uint32 size;
        uint32 flags;
        uint32 height;
        uint32 width;
        uint32 sizeOrPitch;
        uint32 depth;
        uint32 mipMapCount;
        uint32 reserved1[11];
        DDSPixelFormat pixelFormat;
        DDSCaps caps;
        uint32 reserved2;
    };

    // Extended header
    struct DDSExtendedHeader
    {
        uint32 dxgiFormat;
        uint32 resourceDimension;
        uint32 miscFlag; // see D3D11_RESOURCE_MISC_FLAG
        uint32 arraySize;
        uint32 reserved;
    };


    // An 8-byte DXT colour block, represents a 4x4 texel area. Used by all DXT formats
    struct DXTColourBlock
    {
        // 2 colour ranges
        uint16 colour_0;
        uint16 colour_1;
        // 16 2-bit indexes, each byte here is one row
        uint8 indexRow[4];
    };
    // An 8-byte DXT explicit alpha block, represents a 4x4 texel area. Used by DXT2/3
    struct DXTExplicitAlphaBlock
    {
        // 16 4-bit values, each 16-bit value is one row
        uint16 alphaRow[4];
    };
    // An 8-byte DXT interpolated alpha block, represents a 4x4 texel area. Used by DXT4/5
    struct DXTInterpolatedAlphaBlock
    {
        // 2 alpha ranges
        uint8 alpha_0;
        uint8 alpha_1;
        // 16 3-bit indexes. Unfortunately 3 bits doesn't map too well to row bytes
        // so just stored raw
        uint8 indexes[6];
    };

#if OGRE_COMPILER == OGRE_COMPILER_MSVC
#pragma pack (pop)
#else
#pragma pack ()
#endif

    const uint32 DDS_MAGIC = FOURCC('D', 'D', 'S', ' ');
    const uint32 DDS_PIXELFORMAT_SIZE = 8 * sizeof(uint32);
    const uint32 DDS_CAPS_SIZE = 4 * sizeof(uint32);
    const uint32 DDS_HEADER_SIZE = 19 * sizeof(uint32) + DDS_PIXELFORMAT_SIZE + DDS_CAPS_SIZE;

    const uint32 DDSD_CAPS = 0x00000001;
    const uint32 DDSD_HEIGHT = 0x00000002;
    const uint32 DDSD_WIDTH = 0x00000004;
    const uint32 DDSD_PIXELFORMAT = 0x00001000;
    const uint32 DDSD_DEPTH = 0x00800000;
    const uint32 DDPF_ALPHAPIXELS = 0x00000001;
    const uint32 DDPF_FOURCC = 0x00000004;
    const uint32 DDPF_RGB = 0x00000040;
    const uint32 DDSCAPS_COMPLEX = 0x00000008;
    const uint32 DDSCAPS_TEXTURE = 0x00001000;
    const uint32 DDSCAPS_MIPMAP = 0x00400000;
    const uint32 DDSCAPS2_CUBEMAP = 0x00000200;
    const uint32 DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400;
    const uint32 DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800;
    const uint32 DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000;
    const uint32 DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000;
    const uint32 DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000;
    const uint32 DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000;
    const uint32 DDSCAPS2_VOLUME = 0x00200000;

    // Currently unused
//    const uint32 DDSD_PITCH = 0x00000008;
//    const uint32 DDSD_MIPMAPCOUNT = 0x00020000;
//    const uint32 DDSD_LINEARSIZE = 0x00080000;

    // Special FourCC codes
    const uint32 D3DFMT_R16F = 111;
    const uint32 D3DFMT_G16R16F = 112;
    const uint32 D3DFMT_A16B16G16R16F = 113;
    const uint32 D3DFMT_R32F = 114;
    const uint32 D3DFMT_G32R32F = 115;
    const uint32 D3DFMT_A32B32G32R32F = 116;


    //---------------------------------------------------------------------
    DDSCodec2* DDSCodec2::msInstance = 0;
    //---------------------------------------------------------------------
    void DDSCodec2::startup(void)
    {
        if (!msInstance)
        {

            LogManager::getSingleton().logMessage(
                LML_NORMAL,
                "DDS codec registering");

            msInstance = OGRE_NEW DDSCodec2();
            // unregister original codec
            Codec::unregisterCodec(Codec::getCodec("dds"));
            Codec::registerCodec(msInstance);
        }

    }
    //---------------------------------------------------------------------
    void DDSCodec2::shutdown(void)
    {
        if (msInstance)
        {
            Codec::unregisterCodec(msInstance);
            OGRE_DELETE msInstance;
            msInstance = 0;
        }

    }
    //---------------------------------------------------------------------
    DDSCodec2::DDSCodec2() :
        mType("dds")
    {
    }
    //---------------------------------------------------------------------
    DataStreamPtr DDSCodec2::encode(MemoryDataStreamPtr& input, Codec::CodecDataPtr& pData) const
    {
        DebugLog("ENCODING");
        OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED,
            "DDS encoding not supported",
            "DDSCodec2::encode");
    }
    //---------------------------------------------------------------------
    void DDSCodec2::encodeToFile(MemoryDataStreamPtr& input,
        const String& outFileName, Codec::CodecDataPtr& pData) const
    {
        DebugLog("ENCODING");
        // Unwrap codecDataPtr - data is cleaned by calling function
        ImageData* imgData = static_cast<ImageData*>(pData.getPointer());


        // Check size for cube map faces
        bool isCubeMap = (imgData->size ==
            Image::calculateSize(imgData->num_mipmaps, 6, imgData->width,
                imgData->height, imgData->depth, imgData->format));

        // Establish texture attributes
        bool isVolume = (imgData->depth > 1);
        bool isFloat32r = (imgData->format == PF_FLOAT32_R);
        bool isFloat16 = (imgData->format == PF_FLOAT16_RGBA);
        bool isFloat32 = (imgData->format == PF_FLOAT32_RGBA);
        bool notImplemented = false;
        String notImplementedString = "";

        // Check for all the 'not implemented' conditions
        if ((isVolume == true) && (imgData->width != imgData->height))
        {
            // Square textures only
            notImplemented = true;
            notImplementedString += " non square textures";
        }

        uint32 size = 1;
        while (size < imgData->width)
        {
            size <<= 1;
        }
        if (size != imgData->width)
        {
            // Power two textures only
            notImplemented = true;
            notImplementedString += " non power two textures";
        }

        switch (imgData->format)
        {
        case PF_A8R8G8B8:
        case PF_X8R8G8B8:
        case PF_R8G8B8:
        case PF_A8B8G8R8:
        case PF_X8B8G8R8:
        case PF_B8G8R8:
        case PF_FLOAT32_R:
        case PF_FLOAT16_RGBA:
        case PF_FLOAT32_RGBA:
            break;
        default:
            // No crazy FOURCC or 565 et al. file formats at this stage
            notImplemented = true;
            notImplementedString = " unsupported pixel format";
            break;
        }



        // Except if any 'not implemented' conditions were met
        if (notImplemented)
        {
            OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED,
                "DDS encoding for" + notImplementedString + " not supported",
                "DDSCodec::encodeToFile");
        }
        else
        {
            // Build header and write to disk

            // Variables for some DDS header flags
            bool hasAlpha = false;
            uint32 ddsHeaderFlags = 0;
            uint32 ddsHeaderRgbBits = 0;
            uint32 ddsHeaderSizeOrPitch = 0;
            uint32 ddsHeaderCaps1 = 0;
            uint32 ddsHeaderCaps2 = 0;
            uint32 ddsMagic = DDS_MAGIC;

            // Initalise the header flags
            ddsHeaderFlags = (isVolume) ? DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_DEPTH | DDSD_PIXELFORMAT :
                DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;

            bool flipRgbMasks = false;

            // Initalise the rgbBits flags
            switch (imgData->format)
            {
            case PF_A8B8G8R8:
                flipRgbMasks = true;
            case PF_A8R8G8B8:
                ddsHeaderRgbBits = 8 * 4;
                hasAlpha = true;
                break;
            case PF_X8B8G8R8:
                flipRgbMasks = true;
            case PF_X8R8G8B8:
                ddsHeaderRgbBits = 8 * 4;
                break;
            case PF_B8G8R8:
            case PF_R8G8B8:
                ddsHeaderRgbBits = 8 * 3;
                break;
            case PF_FLOAT32_R:
                ddsHeaderRgbBits = 32;
                break;
            case PF_FLOAT16_RGBA:
                ddsHeaderRgbBits = 16 * 4;
                hasAlpha = true;
                break;
            case PF_FLOAT32_RGBA:
                ddsHeaderRgbBits = 32 * 4;
                hasAlpha = true;
                break;
            default:
                ddsHeaderRgbBits = 0;
                break;
            }

            // Initalise the SizeOrPitch flags (power two textures for now)
            ddsHeaderSizeOrPitch = static_cast<uint32>(ddsHeaderRgbBits * imgData->width);

            // Initalise the caps flags
            ddsHeaderCaps1 = (isVolume || isCubeMap) ? DDSCAPS_COMPLEX | DDSCAPS_TEXTURE : DDSCAPS_TEXTURE;
            if (isVolume)
            {
                ddsHeaderCaps2 = DDSCAPS2_VOLUME;
            }
            else if (isCubeMap)
            {
                ddsHeaderCaps2 = DDSCAPS2_CUBEMAP |
                    DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX |
                    DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY |
                    DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ;
            }

            if (imgData->num_mipmaps > 0)
                ddsHeaderCaps1 |= DDSCAPS_MIPMAP;

            // Populate the DDS header information
            DDSHeader ddsHeader;
            ddsHeader.size = DDS_HEADER_SIZE;
            ddsHeader.flags = ddsHeaderFlags;
            ddsHeader.width = (uint32)imgData->width;
            ddsHeader.height = (uint32)imgData->height;
            ddsHeader.depth = (uint32)(isVolume ? imgData->depth : 0);
            ddsHeader.depth = (uint32)(isCubeMap ? 6 : ddsHeader.depth);
            ddsHeader.mipMapCount = imgData->num_mipmaps + 1;
            ddsHeader.sizeOrPitch = ddsHeaderSizeOrPitch;
            for (uint32 reserved1 = 0; reserved1 < 11; reserved1++) // XXX nasty constant 11
            {
                ddsHeader.reserved1[reserved1] = 0;
            }
            ddsHeader.reserved2 = 0;

            ddsHeader.pixelFormat.size = DDS_PIXELFORMAT_SIZE;
            ddsHeader.pixelFormat.flags = (hasAlpha) ? DDPF_RGB | DDPF_ALPHAPIXELS : DDPF_RGB;
            ddsHeader.pixelFormat.flags = (isFloat32r || isFloat16 || isFloat32) ? DDPF_FOURCC : ddsHeader.pixelFormat.flags;
            if (isFloat32r) {
                ddsHeader.pixelFormat.fourCC = D3DFMT_R32F;
            }
            else if (isFloat16) {
                ddsHeader.pixelFormat.fourCC = D3DFMT_A16B16G16R16F;
            }
            else if (isFloat32) {
                ddsHeader.pixelFormat.fourCC = D3DFMT_A32B32G32R32F;
            }
            else {
                ddsHeader.pixelFormat.fourCC = 0;
            }
            ddsHeader.pixelFormat.rgbBits = ddsHeaderRgbBits;

            ddsHeader.pixelFormat.alphaMask = (hasAlpha) ? 0xFF000000 : 0x00000000;
            ddsHeader.pixelFormat.alphaMask = (isFloat32r) ? 0x00000000 : ddsHeader.pixelFormat.alphaMask;
            ddsHeader.pixelFormat.redMask = (isFloat32r) ? 0xFFFFFFFF : 0x00FF0000;
            ddsHeader.pixelFormat.greenMask = (isFloat32r) ? 0x00000000 : 0x0000FF00;
            ddsHeader.pixelFormat.blueMask = (isFloat32r) ? 0x00000000 : 0x000000FF;

            if (flipRgbMasks)
                std::swap(ddsHeader.pixelFormat.redMask, ddsHeader.pixelFormat.blueMask);

            ddsHeader.caps.caps1 = ddsHeaderCaps1;
            ddsHeader.caps.caps2 = ddsHeaderCaps2;
            //          ddsHeader.caps.reserved[0] = 0;
            //          ddsHeader.caps.reserved[1] = 0;

                        // Swap endian
            flipEndian(&ddsMagic, sizeof(uint32));
            flipEndian(&ddsHeader, 4, sizeof(DDSHeader) / 4);

            char* tmpData = 0;
            char const* dataPtr = (char const*)input->getPtr();

            if (imgData->format == PF_B8G8R8)
            {
                PixelBox src(imgData->size / 3, 1, 1, PF_B8G8R8, input->getPtr());
                tmpData = new char[imgData->size];
                PixelBox dst(imgData->size / 3, 1, 1, PF_R8G8B8, tmpData);

                PixelUtil::bulkPixelConversion(src, dst);

                dataPtr = tmpData;
            }

            try
            {
                // Write the file
                std::ofstream of;
                of.open(outFileName.c_str(), std::ios_base::binary | std::ios_base::out);
                of.write((const char*)&ddsMagic, sizeof(uint32));
                of.write((const char*)&ddsHeader, DDS_HEADER_SIZE);
                // XXX flipEndian on each pixel chunk written unless isFloat32r ?
                of.write(dataPtr, (uint32)imgData->size);
                of.close();
            }
            catch (...)
            {
                delete[] tmpData;
            }
        }
    }
    //---------------------------------------------------------------------
    PixelFormat DDSCodec2::convertDXToOgreFormat(uint32 dxfmt) const
    {
        switch (dxfmt) {
        case 80: // DXGI_FORMAT_BC4_UNORM
            return PF_BC4_UNORM;
        case 81: // DXGI_FORMAT_BC4_SNORM
            return PF_BC4_SNORM;
        case 83: // DXGI_FORMAT_BC5_UNORM
            return PF_BC5_UNORM;
        case 84: // DXGI_FORMAT_BC5_SNORM
            return PF_BC5_SNORM;
        case 95: // DXGI_FORMAT_BC6H_UF16
            return PF_BC6H_UF16;
        case 96: // DXGI_FORMAT_BC6H_SF16
            return PF_BC6H_SF16;
        case 98: // DXGI_FORMAT_BC7_UNORM
            return PF_BC7_UNORM;
        case 99: // DXGI_FORMAT_BC7_UNORM_SRGB
            return PF_BC7_UNORM_SRGB;
        default:
            OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,
                "Unsupported DirectX format found in DDS file",
                "DDSCodec2::convertDXToOgreFormat");
        }
    }
    //---------------------------------------------------------------------
    PixelFormat DDSCodec2::convertFourCCFormat(uint32 fourcc) const
    {
        // convert dxt pixel format
        switch (fourcc)
        {
        case FOURCC('D', 'X', 'T', '1'):
            return PF_DXT1;
        case FOURCC('D', 'X', 'T', '2'):
            return PF_DXT2;
        case FOURCC('D', 'X', 'T', '3'):
            return PF_DXT3;
        case FOURCC('D', 'X', 'T', '4'):
            return PF_DXT4;
        case FOURCC('D', 'X', 'T', '5'):
            return PF_DXT5;
        case FOURCC('A', 'T', 'I', '1'):
        case FOURCC('B', 'C', '4', 'U'):
            return PF_BC4_UNORM;
        case FOURCC('B', 'C', '4', 'S'):
            return PF_BC4_SNORM;
        case FOURCC('A', 'T', 'I', '2'):
        case FOURCC('B', 'C', '5', 'U'):
            return PF_BC5_UNORM;
        case FOURCC('B', 'C', '5', 'S'):
            return PF_BC5_SNORM;
        case D3DFMT_R16F:
            return PF_FLOAT16_R;
        case D3DFMT_G16R16F:
            return PF_FLOAT16_GR;
        case D3DFMT_A16B16G16R16F:
            return PF_FLOAT16_RGBA;
        case D3DFMT_R32F:
            return PF_FLOAT32_R;
        case D3DFMT_G32R32F:
            return PF_FLOAT32_GR;
        case D3DFMT_A32B32G32R32F:
            return PF_FLOAT32_RGBA;
            // We could support 3Dc here, but only ATI cards support it, not nVidia
        default:
            OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND,
                "Unsupported FourCC format found in DDS file",
                "DDSCodec2::convertFourCCFormat");
        };

    }
    //---------------------------------------------------------------------
    PixelFormat DDSCodec2::convertPixelFormat(uint32 rgbBits, uint32 rMask,
        uint32 gMask, uint32 bMask, uint32 aMask) const
    {
        // General search through pixel formats
        for (int i = PF_UNKNOWN + 1; i < PF_COUNT; ++i)
        {
            PixelFormat pf = static_cast<PixelFormat>(i);
            if (PixelUtil::getNumElemBits(pf) == rgbBits)
            {
                uint64 testMasks[4];
                PixelUtil::getBitMasks(pf, testMasks);
                int testBits[4];
                PixelUtil::getBitDepths(pf, testBits);
                if (testMasks[0] == rMask && testMasks[1] == gMask &&
                    testMasks[2] == bMask &&
                    // for alpha, deal with 'X8' formats by checking bit counts
                    (testMasks[3] == aMask || (aMask == 0 && testBits[3] == 0)))
                {
                    return pf;
                }
            }

        }

        OGRE_EXCEPT(Exception::ERR_ITEM_NOT_FOUND, "Cannot determine pixel format",
            "DDSCodec2::convertPixelFormat");
    }
    static  uint64_t totalIOTime = 0;
    static  uint64_t totalDecodeTime = 0;
    static  uint64_t totalTime = 0;


    MemoryDataStreamPtr DDSCodec2::downsize(DataStreamPtr& input, int numLevelsToRemove)
    {
        //DebugLog(typeid(*input).name());
        // guaranteed reads required to figure out size of downscaled memory/total read size required
        // DXTColourBlock is read for DXT1, DDSExtendedHeader is read for BC6/7 - should never need both
        size_t maxHeaderSize = sizeof(DDS_MAGIC) + sizeof(DDSHeader) + std::max(sizeof(DDSExtendedHeader), sizeof(DXTColourBlock));
        uint8_t* headerBuffer = (uint8_t*)OGRE_MALLOC(maxHeaderSize, Ogre::MemoryCategory::MEMCATEGORY_GENERAL);
        input->read(headerBuffer, maxHeaderSize);
        uint8_t* readPos = headerBuffer;

        // Read 4 character code
        uint32 fileType = *(uint32_t*)readPos;
        flipEndian(&fileType, sizeof(uint32));
        readPos += sizeof(uint32_t);

        if (DDS_MAGIC != fileType)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                "This is not a DDS file!", "DDSCodec2::downsizeAndPad");
        }

        // Read header in full
        DDSHeader header = *(DDSHeader*)readPos;
        readPos += sizeof(DDSHeader);

        // Endian flip if required, all 32-bit values
        flipEndian(&header, 4, sizeof(DDSHeader) / 4);

        // Check some sizes
        if (header.size != DDS_HEADER_SIZE)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                "DDS header size mismatch!", "DDSCodec2::downsizeAndPad");
        }
        if (header.pixelFormat.size != DDS_PIXELFORMAT_SIZE)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                "DDS header size mismatch!", "DDSCodec2::downsizeAndPad");
        }

        ImageData imgData = ImageData();

        imgData.depth = 1; // (deal with volume later)
        imgData.width = header.width;
        imgData.height = header.height;
        size_t numFaces = 1; // assume one face until we know otherwise

        if (header.caps.caps1 & DDSCAPS_MIPMAP)
        {
            imgData.num_mipmaps = static_cast<uint8>(header.mipMapCount - 1);
        }
        else
        {
            // no mips, nothing to do
            //DebugLog("DDS doesn't have mips");
            input->seek(0);
            return Ogre::MemoryDataStreamPtr();
        }
        imgData.flags = 0;

        bool decompressDXT = false;
        // Figure out basic image type
        if (header.caps.caps2 & DDSCAPS2_CUBEMAP)
        {
            imgData.flags |= IF_CUBEMAP;
            numFaces = 6;
            // vanilla doesn't scale cube maps
            return Ogre::MemoryDataStreamPtr();
        }
        else if (header.caps.caps2 & DDSCAPS2_VOLUME)
        {
            imgData.flags |= IF_3D_TEXTURE;
            imgData.depth = header.depth;
        }
        // Pixel format
        PixelFormat sourceFormat = PF_UNKNOWN;

        if (header.pixelFormat.flags & DDPF_FOURCC)
        {
            // Check if we have an DX10 style extended header and read it. This is necessary for B6H and B7 formats
            if (header.pixelFormat.fourCC == FOURCC('D', 'X', '1', '0'))
            {
                DDSExtendedHeader extHeader = *(DDSExtendedHeader*)readPos;
                readPos += sizeof(DDSExtendedHeader);

                // Endian flip if required, all 32-bit values
                flipEndian(&header, sizeof(DDSExtendedHeader));
                sourceFormat = convertDXToOgreFormat(extHeader.dxgiFormat);
            }
            else
            {
                sourceFormat = convertFourCCFormat(header.pixelFormat.fourCC);
            }
        }
        else
        {
            sourceFormat = convertPixelFormat(header.pixelFormat.rgbBits,
                header.pixelFormat.redMask, header.pixelFormat.greenMask,
                header.pixelFormat.blueMask,
                header.pixelFormat.flags & DDPF_ALPHAPIXELS ?
                header.pixelFormat.alphaMask : 0);
        }


        if (PixelUtil::isCompressed(sourceFormat))
        {
            // TODO is this even needed?
            if (Root::getSingleton().getRenderSystem() == NULL ||
                !Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(RSC_TEXTURE_COMPRESSION_DXT)
                || (!Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(RSC_AUTOMIPMAP)
                    && !imgData.num_mipmaps))
            {
                // We'll need to decompress
                decompressDXT = true;
                // Convert format
                switch (sourceFormat)
                {
                case PF_DXT1:
                    // source can be either 565 or 5551 depending on whether alpha present
                    // unfortunately you have to read a block to figure out which
                    // Note that we upgrade to 32-bit pixel formats here, even 
                    // though the source is 16-bit; this is because the interpolated
                    // values will benefit from the 32-bit results, and the source
                    // from which the 16-bit samples are calculated may have been
                    // 32-bit so can benefit from this.
                    DXTColourBlock block;
                    // Note: we don't update readPos after this as it has to be read again later
                    block = *(DXTColourBlock*)readPos;

                    flipEndian(&(block.colour_0), sizeof(uint16));
                    flipEndian(&(block.colour_1), sizeof(uint16));

                    // colour_0 <= colour_1 means transparency in DXT1
                    if (block.colour_0 <= block.colour_1)
                    {
                        imgData.format = PF_BYTE_RGBA;
                    }
                    else
                    {
                        imgData.format = PF_BYTE_RGB;
                    }
                    break;
                case PF_DXT2:
                case PF_DXT3:
                case PF_DXT4:
                case PF_DXT5:
                    // full alpha present, formats vary only in encoding 
                    imgData.format = PF_BYTE_RGBA;
                    break;
                default:
                    // all other cases need no special format handling
                    break;
                }
            }
            else
            {
                // Use original format
                imgData.format = sourceFormat;
                // Keep DXT data compressed
                imgData.flags |= IF_COMPRESSED;
            }
        }
        else // not compressed
        {
            // Don't test against DDPF_RGB since greyscale DDS doesn't set this
            // just derive any other kind of format
            imgData.format = sourceFormat;
        }


        // Calculate total size from number of mipmaps, faces and size
        imgData.size = Image::calculateSize(imgData.num_mipmaps, numFaces,
            imgData.width, imgData.height, imgData.depth, imgData.format);
        // after processing mip sizes, this will be at the offset of the first necessary mip level
        size_t dataToSkip = (readPos - headerBuffer);
        size_t  faceDataToSkip = 0;
        // this will be number of bytes of file to read for each face
        size_t faceBytesToRead = 0;
        // figure out size of bytes for read so we can do it in one

        // if numLevelsToRemove is > mipMapCount, leave 1 mip
        int64_t mipsLeft = std::max((long long)imgData.num_mipmaps - numLevelsToRemove, 1ll);
        size_t finalMipsToRemove = std::max((long long)imgData.num_mipmaps - mipsLeft, 0ll);
        //DebugLog("Removing " + std::to_string(finalMipsToRemove) + " levels.");
        size_t faceSize = 0;

        for (size_t mip = 0; mip <= imgData.num_mipmaps; ++mip)
        {
            uint32 width = std::max(1u, imgData.width >> mip);
            uint32 height = std::max(1u, imgData.height >> mip);
            uint32 depth = std::max(1u, imgData.depth >> mip);

            // NOTE: the game errors out if a loaded DDS's largest mip has a dimension lower than one block
            // (effectively meaning the largest mip must have a width and height of at least 4)
            while ((width <= 4 || height <= 4) && mip < finalMipsToRemove)
                --finalMipsToRemove;

            if (mip < finalMipsToRemove)
                faceDataToSkip += PixelUtil::getMemorySize(width, height, depth, imgData.format);
            else
                faceBytesToRead += PixelUtil::getMemorySize(width, height, depth, imgData.format);
        }

        // now we're ready to read the used mips

        // Create output buffer
        size_t headerSize = (readPos - headerBuffer);
        MemoryDataStreamPtr output = Ogre::MemoryDataStreamPtr(OGRE_NEW MemoryDataStream((readPos - headerBuffer) + (faceBytesToRead * numFaces)));
        // Read in data
        uint8_t* outputPtr = output->getPtr();
        uint8_t* pixelDataStart = outputPtr + (readPos - headerBuffer);

        input->seek(dataToSkip);
        for (size_t i = 0; i < numFaces; ++i)
        {
            input->skip(faceDataToSkip);
            input->read(pixelDataStart, faceBytesToRead);
            pixelDataStart += faceBytesToRead;
        }
        // write modified header
        header.width = std::max(1u, header.width >> finalMipsToRemove);
        header.height = std::max(1u, header.height >> finalMipsToRemove);
        header.depth = header.depth >> finalMipsToRemove;
        header.mipMapCount = std::max(1ull, header.mipMapCount - finalMipsToRemove);
        // copy all junk from original
        memcpy(outputPtr, headerBuffer, headerSize);
        *(uint32_t*)&outputPtr[0] = DDS_MAGIC;
        // overwrite old header with new header
        *(Ogre::DDSHeader*)&outputPtr[sizeof(uint32)] = header;
        // GC
        OGRE_FREE(headerBuffer, Ogre::MemoryCategory::MEMCATEGORY_GENERAL);

        // return buffer
        return output;
    }

    //---------------------------------------------------------------------
    Codec::DecodeResult DDSCodec2::decode(DataStreamPtr& stream) const
    {
        //DebugLog(typeid(*stream).name());
        //DebugLog("Starting decode");
        //Ogre::Timer  timer;
        //timer.reset();
        //totalIOTime += timer.getMicroseconds();

        // Read 4 character code
        uint32 fileType;
        stream->read(&fileType, sizeof(uint32));
        flipEndian(&fileType, sizeof(uint32));

        if (DDS_MAGIC != fileType)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                "This is not a DDS file!", "DDSCodec2::decode");
        }

        // Read header in full
        DDSHeader header;
        stream->read(&header, sizeof(DDSHeader));

        // Endian flip if required, all 32-bit values
        flipEndian(&header, 4, sizeof(DDSHeader) / 4);

        // Check some sizes
        if (header.size != DDS_HEADER_SIZE)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                "DDS header size mismatch!", "DDSCodec2::decode");
        }
        if (header.pixelFormat.size != DDS_PIXELFORMAT_SIZE)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                "DDS header size mismatch!", "DDSCodec2::decode");
        }

        ImageData* imgData = OGRE_NEW ImageData();
        MemoryDataStreamPtr output;

        imgData->depth = 1; // (deal with volume later)
        imgData->width = header.width;
        imgData->height = header.height;
        size_t numFaces = 1; // assume one face until we know otherwise

        if (header.caps.caps1 & DDSCAPS_MIPMAP)
        {
            imgData->num_mipmaps = static_cast<uint8>(header.mipMapCount - 1);
        }
        else
        {
            imgData->num_mipmaps = 0;
        }
        imgData->flags = 0;

        bool decompressDXT = false;
        // Figure out basic image type
        if (header.caps.caps2 & DDSCAPS2_CUBEMAP)
        {
            imgData->flags |= IF_CUBEMAP;
            numFaces = 6;
        }
        else if (header.caps.caps2 & DDSCAPS2_VOLUME)
        {
            imgData->flags |= IF_3D_TEXTURE;
            imgData->depth = header.depth;
        }
        // Pixel format
        PixelFormat sourceFormat = PF_UNKNOWN;

        if (header.pixelFormat.flags & DDPF_FOURCC)
        {
            // Check if we have an DX10 style extended header and read it. This is necessary for B6H and B7 formats
            if (header.pixelFormat.fourCC == FOURCC('D', 'X', '1', '0'))
            {
                DDSExtendedHeader extHeader;
                stream->read(&extHeader, sizeof(DDSExtendedHeader));

                // Endian flip if required, all 32-bit values
                flipEndian(&header, sizeof(DDSExtendedHeader));
                sourceFormat = convertDXToOgreFormat(extHeader.dxgiFormat);
            }
            else
            {
                sourceFormat = convertFourCCFormat(header.pixelFormat.fourCC);
            }
        }
        else
        {
            sourceFormat = convertPixelFormat(header.pixelFormat.rgbBits,
                header.pixelFormat.redMask, header.pixelFormat.greenMask,
                header.pixelFormat.blueMask,
                header.pixelFormat.flags & DDPF_ALPHAPIXELS ?
                header.pixelFormat.alphaMask : 0);
        }

        if (PixelUtil::isCompressed(sourceFormat))
        {
            if (Root::getSingleton().getRenderSystem() == NULL ||
                !Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(RSC_TEXTURE_COMPRESSION_DXT)
                || (!Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(RSC_AUTOMIPMAP)
                    && !imgData->num_mipmaps))
            {
                // We'll need to decompress
                decompressDXT = true;
                // Convert format
                switch (sourceFormat)
                {
                case PF_DXT1:
                    // source can be either 565 or 5551 depending on whether alpha present
                    // unfortunately you have to read a block to figure out which
                    // Note that we upgrade to 32-bit pixel formats here, even 
                    // though the source is 16-bit; this is because the interpolated
                    // values will benefit from the 32-bit results, and the source
                    // from which the 16-bit samples are calculated may have been
                    // 32-bit so can benefit from this.
                    DXTColourBlock block;
                    stream->read(&block, sizeof(DXTColourBlock));
                    flipEndian(&(block.colour_0), sizeof(uint16));
                    flipEndian(&(block.colour_1), sizeof(uint16));
                    // skip back since we'll need to read this again
                    stream->skip(0 - (long)sizeof(DXTColourBlock));
                    // colour_0 <= colour_1 means transparency in DXT1
                    if (block.colour_0 <= block.colour_1)
                    {
                        imgData->format = PF_BYTE_RGBA;
                    }
                    else
                    {
                        imgData->format = PF_BYTE_RGB;
                    }
                    break;
                case PF_DXT2:
                case PF_DXT3:
                case PF_DXT4:
                case PF_DXT5:
                    // full alpha present, formats vary only in encoding 
                    imgData->format = PF_BYTE_RGBA;
                    break;
                default:
                    // all other cases need no special format handling
                    break;
                }
            }
            else
            {
                // Use original format
                imgData->format = sourceFormat;
                // Keep DXT data compressed
                imgData->flags |= IF_COMPRESSED;
            }
        }
        else // not compressed
        {
            // Don't test against DDPF_RGB since greyscale DDS doesn't set this
            // just derive any other kind of format
            imgData->format = sourceFormat;
        }

        // Calculate total size from number of mipmaps, faces and size
        imgData->size = Image::calculateSize(imgData->num_mipmaps, numFaces,
            imgData->width, imgData->height, imgData->depth, imgData->format);

        // Bind output buffer
        output.bind(OGRE_NEW MemoryDataStream(imgData->size));


        // Now deal with the data
        void* destPtr = output->getPtr();

        // all mips for a face, then each face
        for (size_t i = 0; i < numFaces; ++i)
        {
            uint32 width = imgData->width;
            uint32 height = imgData->height;
            uint32 depth = imgData->depth;

            for (size_t mip = 0; mip <= imgData->num_mipmaps; ++mip)
            {
                size_t dstPitch = width * PixelUtil::getNumElemBytes(imgData->format);

                if (PixelUtil::isCompressed(sourceFormat))
                {
                    // Compressed data
                    if (decompressDXT)
                    {
                        OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR,
                            "DDS file not supported by hardware!", "DDSCodec2::decode");
                    }
                    else
                    {
                        // load directly
                        // DDS format lies! sizeOrPitch is not always set for DXT!!
                        size_t dxtSize = PixelUtil::getMemorySize(width, height, depth, imgData->format);
                        stream->read(destPtr, dxtSize);
                        destPtr = static_cast<void*>(static_cast<uchar*>(destPtr) + dxtSize);
                    }

                }
                else
                {
                    // Note: We assume the source and destination have the same pitch
                    for (size_t z = 0; z < depth; ++z)
                    {
                        for (size_t y = 0; y < height; ++y)
                        {
                            stream->read(destPtr, dstPitch);
                            destPtr = static_cast<void*>(static_cast<uchar*>(destPtr) + dstPitch);
                        }
                    }
                }

                /// Next mip
                if (width != 1) width /= 2;
                if (height != 1) height /= 2;
                if (depth != 1) depth /= 2;
            }

        }
        //totalDecodeTime += timer.getMicroseconds();
        DecodeResult ret;
        ret.first = output;
        ret.second = CodecDataPtr(imgData);
        //totalTime += timer.getMicroseconds();
        //DebugLog("Timers: " + std::to_string((long double)totalIOTime / 1000.0) + " " + std::to_string((long double)totalDecodeTime / 1000.0)
        //    + " " + std::to_string((long double)totalTime / 1000.0));
        //DebugLog("Successful decode");
        return ret;
    }
    //---------------------------------------------------------------------    
    String DDSCodec2::getType() const
    {
        return mType;
    }
    //---------------------------------------------------------------------    
    void DDSCodec2::flipEndian(void* pData, size_t size, size_t count)
    {
#if OGRE_ENDIAN == OGRE_ENDIAN_BIG
        Bitwise::bswapChunks(pData, size, count);
#endif
    }
    //---------------------------------------------------------------------    
    void DDSCodec2::flipEndian(void* pData, size_t size)
    {
#if OGRE_ENDIAN == OGRE_ENDIAN_BIG
        Bitwise::bswapBuffer(pData, size);
#endif
    }
    //---------------------------------------------------------------------
    String DDSCodec2::magicNumberToFileExt(const char* magicNumberPtr, size_t maxbytes) const
    {
        if (maxbytes >= sizeof(uint32))
        {
            uint32 fileType;
            memcpy(&fileType, magicNumberPtr, sizeof(uint32));
            flipEndian(&fileType, sizeof(uint32));

            if (DDS_MAGIC == fileType)
            {
                return String("dds");
            }
        }

        return BLANKSTRING;

    }

}


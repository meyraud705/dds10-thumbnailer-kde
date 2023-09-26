/*  SPDX-FileCopyrightText: 2022 Mathieu Eyraud 
    SPDX-License-Identifier: GPL-2.0-or-later
    
    https://github.com/meyraud705/dds10-thumbnailer-kde
    
    dds10-thumbnailer-kde
    Copyright (C) 2022 Mathieu Eyraud

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <new>
#include <memory>
#include <cstring>

#include <QtCore/QFile>
#include <QtGui/QImage>
#include <QtCore/QDebug>

#include <kio/thumbcreator.h>

// https://github.com/iOrange/bcdec
#define BCDEC_STATIC
#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

// https://github.com/microsoft/DirectX-Headers/blob/main/include/directx/dxgiformat.h
#include "dxgiformat.h"
// https://github.com/microsoft/DirectXTK/blob/main/Src/DDS.h
#include "DDS.h"

class DDSCreator : public ThumbCreator
{
    public:
        DDSCreator() = default;
        virtual ~DDSCreator() = default;
        
        bool create(const QString &path, int width, int height, QImage &img) override;
        Flags flags() const override;
};

extern "C" {
    Q_DECL_EXPORT ThumbCreator *new_creator()
    {
        return new DDSCreator();
    }
}

// Pixel format conversion
typedef void (*PFN_Convert)(uchar* lin_dst, const uchar* line_src, std::size_t width);
void Convert_NOOP8(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    std::memcpy(line_dst, line_src, width*1);
}
void Convert_NOOP16(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    std::memcpy(line_dst, line_src, width*2);
}
void Convert_NOOP24(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    std::memcpy(line_dst, line_src, width*3);
}
void Convert_NOOP32(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    std::memcpy(line_dst, line_src, width*4);
}
void Convert_RGXX8888_RG88(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    for (std::size_t j = 0; j < width; ++j) {
        line_dst[4 * j + 0] = line_src[2 * j + 0];
        line_dst[4 * j + 1] = line_src[2 * j + 1];
        line_dst[4 * j + 2] = 0x00;
        line_dst[4 * j + 3] = 0xff;
    }
}
void Convert_XRGB4444(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    for (std::size_t j = 0; j < width; ++j) {
        line_dst[2 * j + 0] = line_src[2 * j + 0];
        line_dst[2 * j + 1] = line_src[2 * j + 1] & 0xff00; // set unused bit to 0
    }
}
void Convert_XRGB1555(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    for (std::size_t j = 0; j < width; ++j) {
        line_dst[2 * j + 0] = line_src[2 * j + 0];
        line_dst[2 * j + 1] = line_src[2 * j + 1] & 0xfffe; // set unused bit to 0
    }
}
void Convert_XRGB32(uchar* line_dst, const uchar* line_src, std::size_t width)
{
    for (std::size_t j = 0; j < width; ++j) {
        line_dst[4 * j + 0] = line_src[4 * j + 0];
        line_dst[4 * j + 1] = line_src[4 * j + 1];
        line_dst[4 * j + 2] = line_src[4 * j + 2];
        line_dst[4 * j + 3] = 0xff;
    }
}

// Compressed format ///////////////////////////////////////////////////////////
#define FOURCC(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define FOURCC_DDS  FOURCC('D', 'D', 'S', ' ')
#define FOURCC_DXT1 FOURCC('D', 'X', 'T', '1')
#define FOURCC_BC1  FOURCC('B', 'C', '1', ' ')
#define FOURCC_DXT3 FOURCC('D', 'X', 'T', '3')
#define FOURCC_BC2  FOURCC('B', 'C', '2', ' ')
#define FOURCC_DXT5 FOURCC('D', 'X', 'T', '5')
#define FOURCC_BC3  FOURCC('B', 'C', '3', ' ')
#define FOURCC_ATI1 FOURCC('A', 'T', 'I', '1')
#define FOURCC_BC4  FOURCC('B', 'C', '4', ' ')
#define FOURCC_BC4U FOURCC('B', 'C', '4', 'U')
#define FOURCC_BC4S FOURCC('B', 'C', '4', 'S')
#define FOURCC_ATI2 FOURCC('A', 'T', 'I', '2')
#define FOURCC_BC5  FOURCC('B', 'C', '5', ' ')
#define FOURCC_BC5U FOURCC('B', 'C', '5', 'U')
#define FOURCC_BC5S FOURCC('B', 'C', '5', 'S')
#define FOURCC_DX10 FOURCC('D', 'X', '1', '0')

#define max(a, b) ((a<b)?b:a)

typedef std::size_t (*PFN_CompressedSize)(std::size_t w, std::size_t h);
static std::size_t CompressedSize8(std::size_t w, std::size_t h)  {return max(1, (w + 3) / 4) * max(1, (h + 3) / 4) * 8;}
static std::size_t CompressedSize16(std::size_t w, std::size_t h) {return max(1, (w + 3) / 4) * max(1, (h + 3) / 4) * 16;}

typedef void (*PFN_Decode)(const void* compressedBlock, void* decompressedBlock, int destinationPitch);
static void DecodeBC6(const void* compressedBlock, void* decompressedBlock, int destinationPitch)
{
    bcdec_bc6h_float(compressedBlock, decompressedBlock, destinationPitch, 0); // only unsigned
}

constexpr struct {
    std::size_t block_size; ///< compressed block size
    std::size_t pixel_size; ///< uncompressed pixel size
    PFN_CompressedSize CompressedSize;
    PFN_Decode Decode;
    QImage::Format format_out;
    PFN_Convert Convert;
} bc_table[8] = {
    /*     */ {0                    , 0              , nullptr         , nullptr  , QImage::Format_Invalid,  Convert_NOOP32},
    /* BC1 */ {BCDEC_BC1_BLOCK_SIZE , 4              , CompressedSize8 , bcdec_bc1, QImage::Format_RGBA8888, Convert_NOOP32},
    /* BC2 */ {BCDEC_BC2_BLOCK_SIZE , 4              , CompressedSize16, bcdec_bc2, QImage::Format_RGBA8888, Convert_NOOP32},
    /* BC3 */ {BCDEC_BC3_BLOCK_SIZE , 4              , CompressedSize16, bcdec_bc3, QImage::Format_RGBA8888, Convert_NOOP32},
    /* BC4 */ {BCDEC_BC4_BLOCK_SIZE , 1              , CompressedSize8 , bcdec_bc4, QImage::Format_Grayscale8, Convert_NOOP8},
    /* BC5 */ {BCDEC_BC5_BLOCK_SIZE , 2              , CompressedSize16, bcdec_bc5, QImage::Format_RGBA8888, Convert_RGXX8888_RG88}, // no RG format in Qt
    /* BC6 */ {BCDEC_BC6H_BLOCK_SIZE, 3*sizeof(float), CompressedSize16, DecodeBC6, QImage::Format_RGBA8888, Convert_NOOP32},
    /* BC7 */ {BCDEC_BC7_BLOCK_SIZE , 4              , CompressedSize16, bcdec_bc7, QImage::Format_RGBA8888, Convert_NOOP32},
};

// Uncompressed format /////////////////////////////////////////////////////////
constexpr struct {
    uint32_t component;
    uint32_t bit_count;
    uint32_t Rmask;
    uint32_t Gmask;
    uint32_t Bmask;
    uint32_t Amask;
    QImage::Format format_out;
    PFN_Convert Convert;
} uncompressed_table[] = {
    /* D3DFMT_X4R4G4B4    */ {DDS_RGB,       16, 0x0f00, 0x00f0, 0x000f, 0x0, QImage::Format_RGB444, Convert_XRGB4444},
    /* D3DFMT_X1R5G5B5    */ {DDS_RGB,       16, 0x7c00, 0x03e0, 0x001f, 0x0, QImage::Format_RGB555, Convert_XRGB1555},
    /* D3FMT_R5G6B5       */ {DDS_RGB,       16, 0xf800, 0x07e0, 0x001f, 0x0, QImage::Format_RGB16,  Convert_NOOP16},
    /* D3DFMT_R8G8B8      */ {DDS_RGB,       24, 0xff0000, 0x00ff00, 0x0000ff, 0x0, QImage::Format_BGR888, Convert_NOOP24},
    // /* D3DFMT_G16R16      */ {DDS_RGB,       32, 0x0000ffff, 0xffff0000, 0x0, 0x0,  },
    /* D3DFMT_X8R8G8B8    */ {DDS_RGB,       32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x0, QImage::Format_RGB32,  Convert_XRGB32},
    // /* D3DFMT_X8B8G8R8    */ {DDS_RGB,       32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x0, },
    
    // /* D3DFMT_A8R3G3B2    */ {DDS_RGBA,      16, 0x00e0, 0x001c, 0x0003, 0xff00},
    // /* D3DFMT_A4R4G4B4    */ {DDS_RGBA,      16, 0x0f00, 0x00f0, 0x000f, 0xf000, },
    // /* D3DFMT_A1R5G5B5    */ {DDS_RGBA,      16, 0x7c00, 0x03e0, 0x001f, 0x8000},
    // /* D3DFMT_G16R16      */ {DDS_RGBA,      32, 0x0000ffff, 0xffff0000, 0x0, 0x0},
    /* D3DFMT_A8R8G8B8    */ {DDS_RGBA,      32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, QImage::Format_ARGB32, Convert_NOOP32},
    // /* D3DFMT_A8B8G8R8    */ {DDS_RGBA,      32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000, },
    // /* D3DFMT_A2R10G10B10 */ {DDS_RGBA,      32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000, },
    // /* D3DFMT_A2B10G10R10 */ {DDS_RGBA,      32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000, },
    
    /* D3DFMT_L8          */ {DDS_LUMINANCE,  8, 0xff, 0x0, 0x0, 0x0, QImage::Format_Grayscale8, Convert_NOOP8},
    // /* D3DFMT_A4L4        */ {DDS_LUMINANCE,  8, 0x0f, 0xf0, 0x0, 0x0, },
    // /* D3DFMT_A8L8        */ {DDS_LUMINANCE, 16, 0x00ff, 0xff00, 0x0, 0x0, },
    /* D3DFMT_L16         */ {DDS_LUMINANCE, 16, 0xffff, 0x0, 0x0, 0x0, QImage::Format_Grayscale16, Convert_NOOP16},
    
    /* D3DFMT_A8          */ {DDS_ALPHA,      8, 0x0, 0x0, 0x0, 0xff, QImage::Format_Grayscale8, Convert_NOOP8},
};

uint32_t UncompressedId(DirectX::DDS_PIXELFORMAT* ddspf)
{
    for (uint32_t i = 0; i < sizeof(uncompressed_table) / sizeof(uncompressed_table[0]); ++i) {
        if (ddspf->flags == uncompressed_table[i].component
            && ddspf->RGBBitCount == uncompressed_table[i].bit_count) {
            if (uncompressed_table[i].component == DDS_ALPHA) {
                if (ddspf->ABitMask == uncompressed_table[i].Amask) {return i;}
            } else if (uncompressed_table[i].component == DDS_LUMINANCE) {
                if (ddspf->RBitMask == uncompressed_table[i].Rmask) {return i;}
            } else if (uncompressed_table[i].component == DDS_RGB) {
                if (ddspf->RBitMask == uncompressed_table[i].Rmask
                    && ddspf->GBitMask == uncompressed_table[i].Gmask
                    && ddspf->BBitMask == uncompressed_table[i].Bmask) {return i;}
            } else if (uncompressed_table[i].component == DDS_RGBA) {
                if (ddspf->RBitMask == uncompressed_table[i].Rmask
                    && ddspf->GBitMask == uncompressed_table[i].Gmask
                    && ddspf->BBitMask == uncompressed_table[i].Bmask
                    && ddspf->ABitMask == uncompressed_table[i].Amask) {return i;}
            }
        }
    }
    return static_cast<uint32_t>(-1);
}

// Thumbnailer /////////////////////////////////////////////////////////////////
bool DDSCreator::create(const QString &path, int width, int height, QImage &img)
{
    std::unique_ptr<uchar[]> uncompressed_data = nullptr;
    QImage::Format out_format = QImage::Format_Invalid;
    PFN_Convert convert = nullptr; // function to convert uncompressed_data to QImage format
    std::size_t out_pitch = 0;
    
    QFile file_dds(path);
    if (!file_dds.open(QIODevice::ReadOnly)) {
        qDebug() << "[DDS thumbnailer]" << path << ": could not open file";
        return false;
    }
    
    // Verify the type of file
    unsigned int file_code = 0;
    if (file_dds.read(reinterpret_cast<char*>(&file_code), 4) != 4) {
        qDebug() << "[DDS thumbnailer]" << path << ": missing file type";
        return false;
    }
    if (FOURCC_DDS != file_code) {
        qDebug() << "[DDS thumbnailer]" << path << ": not a DDS";
        return false;
    }
    
    // Read DDS header
    DirectX::DDS_HEADER header;
    if (file_dds.read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
        qDebug() << "[DDS thumbnailer]" << path << ": missing header";
        return false;
    }
    
    std::size_t dds_width = header.width;
    std::size_t dds_height = header.height;
    if (dds_height == 0 || dds_width == 0) {
        qDebug() << "[DDS thumbnailer]" << path << ": invalid size (0x0)";
        return false;
    }
    if (dds_height > 16384 || dds_width > 16384) {
        qDebug() << "[DDS thumbnailer]" << path << ": invalid size (" << dds_width << "x" << dds_height << ")";
        return false;
    }
    
    if (header.ddspf.flags == DDS_FOURCC) { // Compressed format
        unsigned int bc_codec = 0;
        DirectX::DDS_HEADER_DXT10 header10 = {DXGI_FORMAT_UNKNOWN};
        switch (header.ddspf.fourCC) {
        case FOURCC_BC1:
        case FOURCC_DXT1:
            bc_codec = 1; break;
        case FOURCC_BC2:
        case FOURCC_DXT3:
            bc_codec = 2; break;
        case FOURCC_BC3:
        case FOURCC_DXT5:
            bc_codec = 3; break;
        case FOURCC_BC4:
        case FOURCC_BC4U:
        case FOURCC_BC4S:
        case FOURCC_ATI1:
            bc_codec = 4; break;
        case FOURCC_BC5:
        case FOURCC_BC5U:
        case FOURCC_BC5S:
        case FOURCC_ATI2:
            bc_codec = 5; break;
        case FOURCC_DX10: // DX10 extended header
            if (file_dds.read(reinterpret_cast<char*>(&header10), sizeof(header10)) != sizeof(header10)) {
                qDebug() << "[DDS thumbnailer]" << path << ": missing DX10 header";
                return false;
            }
            if (header10.resourceDimension != DirectX::DDS_DIMENSION_TEXTURE2D) {
                // only 2D texture supported
                qDebug() << "[DDS thumbnailer]" << path << ": not supported (2d texture only)";
                return false;
            }
            if (header10.miscFlag & 0x4) {
                // array of texture not supported
                qDebug() << "[DDS thumbnailer]" << path << ": not supported (array)";
                return false;
            }
            
            switch (header10.dxgiFormat) {
            case DXGI_FORMAT_BC1_TYPELESS  :
            case DXGI_FORMAT_BC1_UNORM     :
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                bc_codec = 1; break;
            case DXGI_FORMAT_BC2_TYPELESS  :
            case DXGI_FORMAT_BC2_UNORM     :
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                bc_codec = 2; break;
            case DXGI_FORMAT_BC3_TYPELESS  :
            case DXGI_FORMAT_BC3_UNORM     :
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                bc_codec = 3; break;
            case DXGI_FORMAT_BC4_TYPELESS  :
            case DXGI_FORMAT_BC4_UNORM     :
            case DXGI_FORMAT_BC4_SNORM     :
                bc_codec = 4; break;
            case DXGI_FORMAT_BC5_TYPELESS  :
            case DXGI_FORMAT_BC5_UNORM     :
            case DXGI_FORMAT_BC5_SNORM     :
                bc_codec = 5; break;
            case DXGI_FORMAT_BC6H_TYPELESS :
            case DXGI_FORMAT_BC6H_UF16     :
            case DXGI_FORMAT_BC6H_SF16     :
                bc_codec = 6; break;
            case DXGI_FORMAT_BC7_TYPELESS  :
            case DXGI_FORMAT_BC7_UNORM     :
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                bc_codec = 7; break;
            default:
                break;
            }
        default:
            break;
        }
        
        if (bc_codec == 0) {
            qDebug() << "[DDS thumbnailer]" << path << ": unknown bc type: " << bc_codec << " "
            << header.ddspf.fourCC << " " << header10.dxgiFormat;
            return false;
        }
        if (bc_codec == 6) { // TODO: support for bc6
            qDebug() << "[DDS thumbnailer]" << path << ": not supported (bc6)";
            return false;
        }
        
        std::size_t block_size = bc_table[bc_codec].block_size;
        // block is fully decoded even if texture size is not multiple of 4
        std::size_t out_height = (dds_height + 3) / 4 * 4;
        out_pitch = (dds_width * bc_table[bc_codec].pixel_size + (block_size-1)) / block_size * block_size;
        convert = bc_table[bc_codec].Convert;
        out_format = bc_table[bc_codec].format_out;
        
        // Read image data
        std::size_t compressed_size = bc_table[bc_codec].CompressedSize(dds_width, dds_height);
        std::unique_ptr<uchar[]> compressed_data (new uchar[compressed_size]);
        std::unique_ptr<uchar[]> tmp (new uchar[out_pitch * out_height]);
        uncompressed_data = std::move(tmp);
        
        if (file_dds.read(reinterpret_cast<char*>(compressed_data.get()), compressed_size) != compressed_size) {
            qDebug() << "[DDS thumbnailer]" << path << ": missing image data";
            return false;
        }
        file_dds.close();
        
        // Decompress
        uchar *dst = uncompressed_data.get(), *src = compressed_data.get();
        for (std::size_t i = 0; i < dds_height; i += 4) { // bcdec decodes a 4x4 block at once
            uchar* dst_pixel = dst;
            for (std::size_t j = 0; j < dds_width; j += 4) {
                bc_table[bc_codec].Decode(src, dst_pixel, out_pitch);
                dst_pixel += 4 * bc_table[bc_codec].pixel_size;
                src += bc_table[bc_codec].block_size;
            }
            dst += 4 * out_pitch;
        }
    } else { // uncompressed format
        std::size_t dds_bitcount = header.ddspf.RGBBitCount; // dds_bitcount is checked in UncompressedId()
        
        uint32_t id = UncompressedId(&header.ddspf);
        if (id == static_cast<uint32_t>(-1)) {
            qDebug() << "[DDS thumbnailer]" << path << ": unsupported uncompressed format";
            return false;
        }
        
        convert = uncompressed_table[id].Convert;
        out_format = uncompressed_table[id].format_out;
        out_pitch = (dds_width * dds_bitcount + 7) / 8;
        
        // read image
        std::size_t img_size = dds_height * out_pitch;
        std::unique_ptr<uchar[]> tmp (new uchar[img_size]);
        uncompressed_data = std::move(tmp);
        
        if (file_dds.read(reinterpret_cast<char*>(uncompressed_data.get()), img_size) != img_size) {
            qDebug() << "[DDS thumbnailer]" << path << ": missing image data";
            return false;
        }
        file_dds.close();
    }
    
    // fill the QImage
    img = QImage(dds_width, dds_height, out_format);
    for (std::size_t i = 0; i < dds_height; ++i) {
        uchar* line = img.scanLine(i);
        convert(line, &uncompressed_data[i*out_pitch], dds_width);
    }
    
    return true;
}

ThumbCreator::Flags DDSCreator::flags() const
{
    return ThumbCreator::None;
}

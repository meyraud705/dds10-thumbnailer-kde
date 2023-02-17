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
} bc_table[8] = {
    /*     */ {0                    , 0              , nullptr         , nullptr  },
    /* BC1 */ {BCDEC_BC1_BLOCK_SIZE , 4              , CompressedSize8 , bcdec_bc1},
    /* BC2 */ {BCDEC_BC2_BLOCK_SIZE , 4              , CompressedSize16, bcdec_bc2},
    /* BC3 */ {BCDEC_BC3_BLOCK_SIZE , 4              , CompressedSize16, bcdec_bc3},
    /* BC4 */ {BCDEC_BC4_BLOCK_SIZE , 1              , CompressedSize8 , bcdec_bc4},
    /* BC5 */ {BCDEC_BC5_BLOCK_SIZE , 2              , CompressedSize16, bcdec_bc5},
    /* BC6 */ {BCDEC_BC6H_BLOCK_SIZE, 3*sizeof(float), CompressedSize16, DecodeBC6},
    /* BC7 */ {BCDEC_BC7_BLOCK_SIZE , 4              , CompressedSize16, bcdec_bc7},
};

bool DDSCreator::create(const QString &path, int width, int height, QImage &img)
{
    QFile file_dds(path);
    if (!file_dds.open(QIODevice::ReadOnly)) {
        qDebug() << "[DDS thumbnailer]" << path << ": could not open file";
        return false;
    }
    
    // verify the type of file
    unsigned int file_code = 0;
    if (file_dds.read(reinterpret_cast<char*>(&file_code), 4) != 4) {
        qDebug() << "[DDS thumbnailer]" << path << ": missing file type";
        return false;
    }
    if (FOURCC_DDS != file_code) {
        qDebug() << "[DDS thumbnailer]" << path << ": not a DDS";
        return false;
    }
    
    // read DDS header
    DirectX::DDS_HEADER header;
    if (file_dds.read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
        qDebug() << "[DDS thumbnailer]" << path << ": missing header";
        return false;
    }
    
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
    
    // check image properties
    if (bc_codec == 0) {
        qDebug() << "[DDS thumbnailer]" << path << ": unknown bc type: " << bc_codec << " "
        << header.ddspf.fourCC << " " << header10.dxgiFormat;
        return false;
    }
    if (bc_codec == 6) { // TODO: support for bc6
        qDebug() << "[DDS thumbnailer]" << path << ": not supported (bc6)";
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
    
    // decompress
    std::size_t compressed_size = bc_table[bc_codec].CompressedSize(dds_width, dds_height);
    std::unique_ptr<char[]> compressed_data (new char[compressed_size]);
    std::unique_ptr<char[]> uncompressed_data (new char[bc_table[bc_codec].pixel_size * dds_width * dds_height]);
    
    if (file_dds.read(compressed_data.get(), compressed_size) != compressed_size) {
        qDebug() << "[DDS thumbnailer]" << path << ": missing image data";
        return false;
    }
    file_dds.close();
    
    char *dst, *src = compressed_data.get();
    for (std::size_t i = 0; i < dds_height; i += 4) { // bcdec decodes a 4x4 block at once
        for (std::size_t j = 0; j < dds_width; j += 4) {
            dst = uncompressed_data.get() + (i * dds_width + j) * bc_table[bc_codec].pixel_size;
            bc_table[bc_codec].Decode(src, dst, dds_width * bc_table[bc_codec].pixel_size);
            src += bc_table[bc_codec].block_size;
        }
    }
    
    // Fill the QImage
    if (bc_codec == 4) { // ATI1: R
        img = QImage(dds_width, dds_height, QImage::Format_Grayscale8);
        for (std::size_t i = 0; i < dds_height; ++i) {
            uchar* line = img.scanLine(i);
            std::memcpy(line, &uncompressed_data[i * dds_width], dds_width);
        }
    } else if (bc_codec == 5) { // ATI2: RG
        img = QImage(dds_width, dds_height, QImage::Format_RGBA8888);
        // convert ATI2 to RGBA
        for (std::size_t i = 0; i < dds_height; ++i) {
            uchar* line = img.scanLine(i);
            for (std::size_t j = 0; j < dds_width; ++j) {
                line[4 * j + 0] = uncompressed_data[2 * (i * dds_width + j)];
                line[4 * j + 1] = uncompressed_data[2 * (i * dds_width + j) + 1];
                line[4 * j + 2] = 0x00;
                line[4 * j + 3] = 0xFF;
            }
        }
    } else if (bc_codec == 6) { // RGB
        // TODO: support for bc6
        return false;
    } else { // RGBA
        img = QImage(dds_width, dds_height, QImage::Format_RGBA8888);
        for (std::size_t i = 0; i < dds_height; ++i) {
            uchar* line = img.scanLine(i);
            std::memcpy(line, &uncompressed_data[i * dds_width * 4], dds_width * 4);
        }
    }
    
    return true;
}

ThumbCreator::Flags DDSCreator::flags() const
{
    return ThumbCreator::None;
}

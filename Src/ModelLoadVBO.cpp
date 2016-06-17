//--------------------------------------------------------------------------------------
// File: ModelLoadVBO.cpp
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Model.h"

#include "Effects.h"
#include "VertexTypes.h"

#include "DirectXHelpers.h"
#include "PlatformHelpers.h"
#include "BinaryReader.h"

#include "vbo.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(sizeof(VertexPositionNormalTexture) == 32, "VBO vertex size mismatch");

namespace
{
    //--------------------------------------------------------------------------------------
    // Shared VB input element description
    INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;
    std::shared_ptr<std::vector<D3D12_INPUT_ELEMENT_DESC>> g_vbdecl;

    BOOL CALLBACK InitializeDecl(PINIT_ONCE initOnce, PVOID Parameter, PVOID *lpContext)
    {
        UNREFERENCED_PARAMETER(initOnce);
        UNREFERENCED_PARAMETER(Parameter);
        UNREFERENCED_PARAMETER(lpContext);

        g_vbdecl = std::make_shared<std::vector<D3D12_INPUT_ELEMENT_DESC>>(VertexPositionNormalTexture::InputLayout.pInputElementDescs,
            VertexPositionNormalTexture::InputLayout.pInputElementDescs + VertexPositionNormalTexture::InputLayout.NumElements);

        return TRUE;
    }
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
std::unique_ptr<Model> DirectX::Model::CreateFromVBO(ID3D12Device* d3dDevice, const uint8_t* meshData, size_t dataSize,
                                                     std::shared_ptr<IEffect> ieffect, bool ccw, bool pmalpha)
{
    if (!InitOnceExecuteOnce(&g_InitOnce, InitializeDecl, nullptr, nullptr))
        throw std::exception("One-time initialization failed");

    if ( !d3dDevice || !meshData )
        throw std::exception("Device and meshData cannot be null");

    if (!ieffect)
        throw std::exception("Effect is required in DirectXTK for DirectX 12");

    // File Header
    if ( dataSize < sizeof(VBO::header_t) )
        throw std::exception("End of file");
    auto header = reinterpret_cast<const VBO::header_t*>( meshData );

    if ( !header->numVertices || !header->numIndices )
        throw std::exception("No vertices or indices found");

    size_t vertSize = sizeof(VertexPositionNormalTexture) * header->numVertices;

    if (dataSize < (vertSize + sizeof(VBO::header_t)))
        throw std::exception("End of file");
    auto verts = reinterpret_cast<const VertexPositionNormalTexture*>( meshData + sizeof(VBO::header_t) );

    size_t indexSize = sizeof(uint16_t) * header->numIndices;

    if (dataSize < (sizeof(VBO::header_t) + vertSize + indexSize))
        throw std::exception("End of file");
    auto indices = reinterpret_cast<const uint16_t*>( meshData + sizeof(VBO::header_t) + vertSize );

    // Create vertex buffer
    auto vb = GraphicsMemory::Get().Allocate(vertSize);
    memcpy(vb.Memory(), verts, vertSize);
    
    // Create index buffer
    auto ib = GraphicsMemory::Get().Allocate(indexSize);
    memcpy(ib.Memory(), indices, indexSize);

    auto part = new ModelMeshPart();
    part->indexCount = header->numIndices;
    part->startIndex = 0;
    part->vertexStride = static_cast<UINT>( sizeof(VertexPositionNormalTexture) );
    part->indexBuffer = std::move(ib);
    part->vertexBuffer = std::move(vb);
    part->effect = ieffect;
    part->vbDecl = g_vbdecl;

    auto mesh = std::make_shared<ModelMesh>();
    mesh->ccw = ccw;
    mesh->pmalpha = pmalpha;
    BoundingSphere::CreateFromPoints(mesh->boundingSphere, header->numVertices, &verts->position, sizeof(VertexPositionNormalTexture));
    BoundingBox::CreateFromPoints(mesh->boundingBox, header->numVertices, &verts->position, sizeof(VertexPositionNormalTexture));
    mesh->meshParts.emplace_back(part);

    std::unique_ptr<Model> model(new Model());
    model->meshes.emplace_back(mesh);
 
    return model;
}


//--------------------------------------------------------------------------------------
_Use_decl_annotations_
std::unique_ptr<Model> DirectX::Model::CreateFromVBO(ID3D12Device* d3dDevice, const wchar_t* szFileName,
                                                     std::shared_ptr<IEffect> ieffect, bool ccw, bool pmalpha)
{
    size_t dataSize = 0;
    std::unique_ptr<uint8_t[]> data;
    HRESULT hr = BinaryReader::ReadEntireFile( szFileName, data, &dataSize );
    if ( FAILED(hr) )
    {
        DebugTrace( "CreateFromVBO failed (%08X) loading '%ls'\n", hr, szFileName );
        throw std::exception( "CreateFromVBO" );
    }

    auto model = CreateFromVBO( d3dDevice, data.get(), dataSize, ieffect, ccw, pmalpha );

    model->name = szFileName;

    return model;
}

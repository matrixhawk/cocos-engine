/****************************************************************************
 Copyright (c) 2013-2016 Chukong Technologies Inc.

 http://www.cocos2d-x.org

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
 ****************************************************************************/


#include "renderer/CCQuadCommand.h"

#include "renderer/ccGLStateCache.h"
#include "renderer/CCGLProgram.h"
#include "renderer/CCMaterial.h"
#include "renderer/CCRenderer.h"
#include "renderer/CCPass.h"
#include "renderer/CCTexture2D.h"
#include "xxhash/xxhash.h"

NS_CC_BEGIN

int QuadCommand::__indexCapacity = -1;
GLushort* QuadCommand::__indices = nullptr;

QuadCommand::QuadCommand():
_indexSize(-1),
_ownedIndices()
{
}

QuadCommand::~QuadCommand()
{
    for (auto& indices : _ownedIndices)
    {
        CC_SAFE_DELETE_ARRAY(indices);
    }
}

void QuadCommand::init(float globalOrder, GLuint textureID, GLProgramState* glProgramState, const BlendFunc& blendType, V3F_C4B_T2F_Quad* quads, ssize_t quadCount,
                       const Mat4& mv, uint32_t flags)
{
    CCASSERT(glProgramState, "Invalid GLProgramState");
    CCASSERT(glProgramState->getVertexAttribsFlags() == 0, "No custom attributes are supported in QuadCommand");

    if (quadCount * 6 > _indexSize)
        reIndex((int)quadCount*6);

    Triangles triangles;
    triangles.verts = &quads->tl;
    triangles.vertCount = (int)quadCount * 4;
    triangles.indices = __indices;
    triangles.indexCount = (int)quadCount * 6;
    TrianglesCommand::init(globalOrder, textureID, glProgramState, blendType, triangles, mv, flags);
}

void QuadCommand::reIndex(int indicesCount)
{
    // first time init: create a decent buffer size for indices to prevent too much resizing
    if (__indexCapacity == -1)
    {
        indicesCount = std::max(indicesCount, 2048);
    }

    if (indicesCount > __indexCapacity)
    {
        // if resizing is needed, get needed size plus 25%, but not bigger that max size
        indicesCount *= 1.25;
        indicesCount = std::min(indicesCount, 65536);

        CCLOG("cocos2d: QuadCommand: resizing index size from [%d] to [%d]", __indexCapacity, indicesCount);

        _ownedIndices.push_back(__indices);
        __indices = new (std::nothrow) GLushort[indicesCount];
        __indexCapacity = indicesCount;
    }

    for( int i=0; i < __indexCapacity/6; i++)
    {
        __indices[i*6+0] = (GLushort) (i*4+0);
        __indices[i*6+1] = (GLushort) (i*4+1);
        __indices[i*6+2] = (GLushort) (i*4+2);
        __indices[i*6+3] = (GLushort) (i*4+3);
        __indices[i*6+4] = (GLushort) (i*4+2);
        __indices[i*6+5] = (GLushort) (i*4+1);
    }

    _indexSize = indicesCount;
}

void QuadCommand::init(float globalOrder, Texture2D* texture, GLProgramState* glProgramState, const BlendFunc& blendType, V3F_C4B_T2F_Quad* quads, ssize_t quadCount,
    const Mat4& mv, uint32_t flags)
{
    init(globalOrder, texture->getName(), glProgramState, blendType, quads, quadCount, mv, flags);
    _alphaTextureID = texture->getAlphaTextureName();
}

NS_CC_END

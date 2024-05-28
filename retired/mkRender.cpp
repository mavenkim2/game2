internal void D_EndFrame()
{
    RenderState *renderState = engine->GetRenderState();
    R_PassMesh *pass         = R_GetPassFromKind(R_PassType_Mesh)->passMesh;

    // TODO: these need to go somewhere else
    {
        R_SetupViewFrustum();
        for (ViewLight *light = pass->viewLight; light != 0; light = light->next)
        {
            R_CullModelsToLight(light);
            if (light->type == LightType_Directional)
            {
                R_CascadedShadowMap(light, renderState->shadowMapMatrices, renderState->cascadeDistances);
            }
        }

        renderState->drawParams = D_PrepareMeshes(pass->list.first, pass->list.mTotalSurfaceCount);

        // for each light, prepare the meshes that are ONLY SHADOWS for that light. in the future, will
        // need to consider models that cast shadows into the view frustum from multiple lights.
        for (ViewLight *light = pass->viewLight; light != 0; light = light->next)
        {
            light->drawParams = D_PrepareMeshes(light->modelNodes, light->mNumShadowSurfaces);
        }
    }
    renderState->vertexCache.VC_BeginGPUSubmit();
    R_SwapFrameData();
}

internal void R_ShadowMapFrusta(i32 splits, f32 splitWeight, Mat4 *outMatrices, f32 *outSplits)
{
    RenderState *renderState = engine->GetRenderState();
    f32 nearZStart           = renderState->nearZ;
    f32 farZEnd              = renderState->farZ;
    f32 nearZ                = nearZStart;
    f32 farZ                 = farZEnd;
    f32 lambda               = splitWeight;
    f32 ratio                = farZEnd / nearZStart;

    for (i32 i = 0; i < splits + 1; i++)
    {
        f32 si = (i + 1) / (f32)(splits + 1);
        if (i > 0)
        {
            nearZ = farZ - (farZ * 0.005f);
        }
        // NOTE: ???
        farZ = 1.005f * lambda * (nearZStart * Powf(ratio, si)) +
               (1 - lambda) * (nearZStart + (farZEnd - nearZStart) * si);

        Mat4 matrix    = Perspective4(renderState->fov, renderState->aspectRatio, nearZ, farZ);
        Mat4 result    = matrix * renderState->viewMatrix;
        outMatrices[i] = Inverse(result);
        if (i <= splits)
        {
            outSplits[i] = farZ;
        }
    }
}

// Increase the resolution by bounding to the frustum
// Rect2 bounds;
// bounds.minP = {FLT_MAX, FLT_MAX};
// bounds.maxP = {-FLT_MAX, -FLT_MAX};
// for (i32 i = 0; i < ArrayLength(frustumVertices); i++)
// {
//     V4 ndc = outLightViewProjectionMatrices[cascadeIndex] * MakeV4(frustumVertices[i], 1.0);
//     // Assert(ndc.x >= -ndc.w && ndc.x <= ndc.w);
//     // Assert(ndc.y >= -ndc.w && ndc.y <= ndc.w);
//     // Assert(ndc.z >= -ndc.w && ndc.z <= ndc.w);
//     ndc.xyz /= ndc.w;
//     bounds.minX = Min(bounds.minX, ndc.x);
//     bounds.minY = Min(bounds.minY, ndc.y);
//
//     bounds.maxX = Max(bounds.maxX, ndc.x);
//     bounds.maxY = Max(bounds.maxX, ndc.y);
// }
// V2 boundCenter  = (bounds.minP + bounds.maxP) / 2.f;
// V2 boundExtents = bounds.maxP - bounds.minP;
// Mat4 grow       = Scale(MakeV3(1.f / boundExtents.x, 1.f / boundExtents.y, 1.f)) * Translate4(-MakeV3(boundCenter, 0.f));
//
// outLightViewProjectionMatrices[cascadeIndex] = grow * outLightViewProjectionMatrices[cascadeIndex];

// V3 frustumVertices[cNumCascades][8];
// for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
// {
//     R_GetFrustumCorners(mvpMatrices[cascadeIndex], BoundsZeroToOneCube, frustumVertices[cascadeIndex]);
// }
//
// // Step 2. Find light world to view matrix (first get center point of frusta)
//
// V3 centers[cNumCascades];
// // Light direction is specified from surface->light origin
// V3 worldUp = renderState->camera.right;
// Mat4 lightViewMatrices[cNumCascades];
// for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
// {
//     for (i32 i = 0; i < 8; i++)
//     {
//         centers[cascadeIndex] += frustumVertices[cascadeIndex][i];
//     }
//     centers[cascadeIndex] /= 8;
//     lightViewMatrices[cascadeIndex] = LookAt4(centers[cascadeIndex] + inLight->dir, centers[cascadeIndex], worldUp);
// }
//
// Rect3 bounds[cNumCascades];
// for (i32 cascadeIndex = 0; cascadeIndex < cNumCascades; cascadeIndex++)
// {
//     Init(&bounds[cascadeIndex]);
//     // Loop over each corner of each frusta
//     for (i32 i = 0; i < 8; i++)
//     {
//         V4 result = Transform(lightViewMatrices[cascadeIndex], frustumVertices[cascadeIndex][i]);
//         AddBounds(bounds[cascadeIndex], result.xyz);
//     }
// }
//
// // TODO: instead of tightly fitting the frusta, the light's box could be tighter
//
// for (i32 i = 0; i < cNumCascades; i++)
// {
//     // When viewing down the -z axis, the max is the near plane and the min is the far plane.
//     Rect3 *currentBounds = &bounds[i];
//     // The orthographic projection expects 0 < n < f
//
//     // TODO: use the bounds of the light instead
//     f32 zNear = -currentBounds->maxZ - 50;
//     f32 zFar  = -currentBounds->minZ;
//
//     f32 extent = zFar - zNear;
//
//     V3 shadowCameraPos = centers[i] - inLight->dir * zNear;
//     Mat4 fixedLookAt   = LookAt4(shadowCameraPos, centers[i], worldUp);
//
//     outLightViewProjectionMatrices[i] = Orthographic4(currentBounds->minX, currentBounds->maxX, currentBounds->minY, currentBounds->maxY, 0, extent) * fixedLookAt;
//     outCascadeDistances[i]            = cascadeDistances[i];
// }

internal void D_PushModel(VC_Handle vertexBuffer, VC_Handle indexBuffer, Mat4 transform)
{
    D_State *d_state       = engine->GetDrawState();
    R_PassMesh *pass       = R_GetPassFromKind(R_PassType_Mesh)->passMesh;
    R_MeshParamsNode *node = PushStruct(d_state->arena, R_MeshParamsNode);

    pass->list.mTotalSurfaceCount += 1;

    node->val.numSurfaces = 1;
    D_Surface *surface    = (D_Surface *)R_FrameAlloc(sizeof(*surface));
    node->val.surfaces    = surface;

    surface->vertexBuffer = vertexBuffer;
    surface->indexBuffer  = indexBuffer;

    node->val.transform = transform;
    QueuePush(pass->list.first, pass->list.last, node);
}

internal void D_PushModel(AS_Handle loadedModel, Mat4 transform, Mat4 &mvp, Mat4 *skinningMatrices = 0,
                          u32 skinningMatricesCount = 0)
{
    D_State *d_state         = engine->GetDrawState();
    RenderState *renderState = engine->GetRenderState();
    if (!IsModelHandleNil(loadedModel))
    {
        LoadedModel *model = GetModel(loadedModel);

        Rect3 bounds = model->bounds;
        bounds.minP  = transform * bounds.minP;
        bounds.maxP  = transform * bounds.maxP;

        if (bounds.minP.x > bounds.maxP.x)
        {
            Swap(f32, bounds.minP.x, bounds.maxP.x);
        }
        if (bounds.minP.y > bounds.maxP.y)
        {
            Swap(f32, bounds.minP.y, bounds.maxP.y);
        }
        if (bounds.minP.z > bounds.maxP.z)
        {
            Swap(f32, bounds.minP.z, bounds.maxP.z);
        }
        DrawBox(bounds, {1, 0, 0, 1});

        R_PassMesh *pass       = R_GetPassFromKind(R_PassType_Mesh)->passMesh;
        R_MeshParamsNode *node = PushStruct(d_state->arena, R_MeshParamsNode);

        node->val.mIsDirectlyVisible = D_IsInBounds(model->bounds, mvp);
        node->val.mBounds            = bounds;
        pass->list.mTotalSurfaceCount += model->numMeshes;

        node->val.numSurfaces = model->numMeshes;
        D_Surface *surfaces   = (D_Surface *)R_FrameAlloc(node->val.numSurfaces * sizeof(*surfaces));
        node->val.surfaces    = surfaces;

        for (u32 i = 0; i < model->numMeshes; i++)
        {
            Mesh *mesh         = &model->meshes[i];
            D_Surface *surface = &surfaces[i];

            surface->vertexBuffer = mesh->surface.mVertexBuffer;
            surface->indexBuffer  = mesh->surface.mIndexBuffer;

            // TODO: ptr or copy?
            surface->material = &mesh->material;
        }

        node->val.transform = transform;

        if (skinningMatricesCount)
        {
            node->val.jointHandle = renderState->vertexCache.VC_AllocateBuffer(BufferType_Uniform, BufferUsage_Dynamic, skinningMatrices,
                                                                               sizeof(skinningMatrices[0]), skinningMatricesCount);
        }
        QueuePush(pass->list.first, pass->list.last, node);
    }
}

// prepare to submit to gpu
internal R_MeshPreparedDrawParams *D_PrepareMeshes(R_MeshParamsNode *head, i32 inCount)
{
    RenderState *renderState = engine->GetRenderState();
    // R_PassMesh *pass         = renderState->passes[R_PassType_Mesh].passMesh;

    // Per mesh
    i32 drawCount                        = 0;
    R_MeshPreparedDrawParams *drawParams = (R_MeshPreparedDrawParams *)R_FrameAlloc(sizeof(R_MeshPreparedDrawParams));
    drawParams->mIndirectBuffers         = (R_IndirectCmd *)R_FrameAlloc(sizeof(R_IndirectCmd) * inCount);
    drawParams->mPerMeshDrawParams       = (R_MeshPerDrawParams *)R_FrameAlloc(sizeof(R_MeshPerDrawParams) * inCount);
    for (R_MeshParamsNode *node = head; node != 0; node = node->next)
    {
        R_MeshParams *params = &node->val;

        i32 jointOffset = -1;
        if (params->jointHandle != 0)
        {
            if (!renderState->vertexCache.CheckCurrent(params->jointHandle))
            {
                continue;
            }
            jointOffset = (i32)(renderState->vertexCache.GetOffset(params->jointHandle) / sizeof(Mat4));
        }

        // Per material
        for (u32 surfaceCount = 0; surfaceCount < params->numSurfaces; surfaceCount++)
        {
            R_IndirectCmd *indirectBuffer     = &drawParams->mIndirectBuffers[drawCount];
            R_MeshPerDrawParams *perMeshParam = &drawParams->mPerMeshDrawParams[drawCount];
            D_Surface *surface                = &params->surfaces[surfaceCount];

            perMeshParam->mTransform   = params->transform;
            perMeshParam->mJointOffset = jointOffset;
            perMeshParam->mIsPBR       = true;

            indirectBuffer->mCount         = (u32)(renderState->vertexCache.GetSize(surface->indexBuffer) / sizeof(u32));
            indirectBuffer->mInstanceCount = 1;
            indirectBuffer->mFirstIndex    = (u32)(renderState->vertexCache.GetOffset(surface->indexBuffer) / sizeof(u32));
            // TODO: this could change
            indirectBuffer->mBaseVertex   = (u32)(renderState->vertexCache.GetOffset(surface->vertexBuffer) / sizeof(MeshVertex));
            indirectBuffer->mBaseInstance = 0;

            drawCount++;
            if (!surface->material)
            {
                continue;
            }
            for (i32 textureIndex = 0; textureIndex < TextureType_Count; textureIndex++)
            {
                R_Handle textureHandle = GetTextureRenderHandle(surface->material->textureHandles[textureIndex]);
                if (textureIndex == TextureType_MR && textureHandle.u64[0] == 0)
                {
                    perMeshParam->mIsPBR = false;
                }
                perMeshParam->mIndex[textureIndex] = (u64)textureHandle.u32[0];
                // TODO: change the shader to reflect that this is a float :)
                perMeshParam->mSlice[textureIndex] = textureHandle.u32[1];
            }
        }
    }
    return drawParams;
}

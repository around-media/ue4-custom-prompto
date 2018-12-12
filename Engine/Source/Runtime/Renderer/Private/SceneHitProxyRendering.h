// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneHitProxyRendering.h: Scene hit proxy rendering.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "HitProxies.h"
#include "DrawingPolicy.h"
#include "MeshPassProcessor.h"

class FPrimitiveSceneProxy;
class FScene;
class FStaticMesh;

class FHitProxyMeshProcessor : public FMeshPassProcessor
{
public:

	FHitProxyMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, bool InbOpaqueOnly, const FDrawingPolicyRenderState& InRenderState, FMeshPassDrawListContext& InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 MeshId = -1) override final;

	FDrawingPolicyRenderState PassDrawRenderState;


private:
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 MeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	const bool bOpaqueOnly;
};


#if WITH_EDITOR

class FEditorSelectionMeshProcessor : public FMeshPassProcessor
{
public:

	FEditorSelectionMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext& InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 MeshId = -1) override final;

	FDrawingPolicyRenderState PassDrawRenderState;

private:
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 MeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	int32 GetStencilValue(const FSceneView* View, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

	/** This map is needed to ensure that individually selected proxies rendered more than once a frame (if they have multiple sections) share a common outline */
	static TMap<const FPrimitiveSceneProxy*, int32> ProxyToStencilIndex;
	/** This map is needed to ensure that proxies rendered more than once a frame (if they have multiple sections) share a common outline */
	static TMap<FName, int32> ActorNameToStencilIndex;
};

#endif
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistortionRendering.h: Distortion rendering implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "DrawingPolicy.h"
#include "MeshPassProcessor.h"

class FPrimitiveSceneProxy;
struct FDrawingPolicyRenderState;

/** 
* Set of distortion scene prims  
*/
class FDistortionPrimSet
{
public:

	/** 
	* Iterate over the distortion prims and draw their accumulated offsets
	* @param ViewInfo - current view used to draw items
	* @param DPGIndex - current DPG used to draw items
	* @return true if anything was drawn
	*/
	bool DrawAccumulatedOffsets(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, bool bInitializeOffsets);

	/**
	* Adds a new primitives to the list of distortion prims
	* @param PrimitiveSceneProxies - primitive info to add.
	*/
	void Append(FPrimitiveSceneProxy** PrimitiveSceneProxies, int32 NumProxies)
	{
		Prims.Append(PrimitiveSceneProxies, NumProxies);
	}

	/** 
	* @return number of prims to render
	*/
	int32 NumPrims() const
	{
		return Prims.Num();
	}

	/** 
	* @return a prim currently set to render
	*/
	const FPrimitiveSceneProxy* GetPrim(int32 i)const
	{
		check(i>=0 && i<NumPrims());
		return Prims[i];
	}

private:
	/** list of distortion prims added from the scene */
	TArray<FPrimitiveSceneProxy*, SceneRenderingAllocator> Prims;
};


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FDistortionPassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FSceneTexturesUniformParameters, SceneTextures)
	SHADER_PARAMETER(FVector4, DistortionParams)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FMobileDistortionPassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FMobileSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER(FVector4, DistortionParams)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern void SetupMobileDistortionPassUniformBuffer(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FMobileDistortionPassUniformParameters& DistortionPassParameters);

class FDistortionMeshProcessor : public FMeshPassProcessor
{
public:

	FDistortionMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, const FDrawingPolicyRenderState& InPassDrawRenderState, FMeshPassDrawListContext& InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 MeshId = -1) override final;

	FDrawingPolicyRenderState PassDrawRenderState;

private:
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 MeshId,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);
};
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUScene.cpp
=============================================================================*/

#include "GPUScene.h"
#include "CoreMinimal.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "ByteBuffer.h"
#include "SpriteIndexBuffer.h"
#include "SceneFilterRendering.h"
#include "ClearQuad.h"
#include "RendererModule.h"

int32 GGPUSceneUploadEveryFrame = 0;
FAutoConsoleVariableRef CVarGPUSceneUploadEveryFrame(
	TEXT("r.GPUScene.UploadEveryFrame"),
	GGPUSceneUploadEveryFrame,
	TEXT("Whether to upload the entire scene's primitive data every frame.  Useful for debugging."),
	ECVF_RenderThreadSafe
	);

int32 GGPUSceneValidatePrimitiveBuffer = 0;
FAutoConsoleVariableRef CVarGPUSceneValidatePrimitiveBuffer(
	TEXT("r.GPUScene.ValidatePrimitiveBuffer"),
	GGPUSceneValidatePrimitiveBuffer,
	TEXT("Whether to readback the GPU primitive data and assert if it doesn't match the RT primitive data.  Useful for debugging."),
	ECVF_RenderThreadSafe
	);

int32 GGPUSceneMaxPooledUploadBufferSize = 256000;
FAutoConsoleVariableRef CVarGGPUSceneMaxPooledUploadBufferSize(
	TEXT("r.GPUScene.MaxPooledUploadBufferSize"),
	GGPUSceneMaxPooledUploadBufferSize,
	TEXT("Maximum size of GPU Scene upload buffer size to pool."),
	ECVF_RenderThreadSafe
	);

// Allocate a range.  Returns allocated StartOffset.
int32 FGrowOnlySpanAllocator::Allocate(int32 Num)
{
	const int32 FoundIndex = SearchFreeList(Num);

	// Use an existing free span if one is found
	if (FoundIndex != INDEX_NONE)
	{
		FLinearAllocation FreeSpan = FreeSpans[FoundIndex];

		if (FreeSpan.Num > Num)
		{
			// Update existing free span with remainder
			FreeSpans[FoundIndex] = FLinearAllocation(FreeSpan.StartOffset + Num, FreeSpan.Num - Num);
		}
		else
		{
			// Fully consumed the free span
			FreeSpans.RemoveAtSwap(FoundIndex);
		}
				
		return FreeSpan.StartOffset;
	}

	// New allocation
	int32 StartOffset = MaxSize;
	MaxSize = MaxSize + Num;

	return StartOffset;
}

// Free an already allocated range.  
void FGrowOnlySpanAllocator::Free(int32 BaseOffset, int32 Num)
{
	check(BaseOffset + Num <= MaxSize);

	FLinearAllocation NewFreeSpan(BaseOffset, Num);

#if DO_CHECK
	// Detect double delete
	for (int32 i = 0; i < FreeSpans.Num(); i++)
	{
		FLinearAllocation CurrentSpan = FreeSpans[i];
		check(!(CurrentSpan.Contains(NewFreeSpan)));
	}
#endif

	bool bMergedIntoExisting = false;

	int32 SpanBeforeIndex = INDEX_NONE;
	int32 SpanAfterIndex = INDEX_NONE;

	// Search for existing free spans we can merge with
	for (int32 i = 0; i < FreeSpans.Num(); i++)
	{
		FLinearAllocation CurrentSpan = FreeSpans[i];

		if (CurrentSpan.StartOffset == NewFreeSpan.StartOffset + NewFreeSpan.Num)
		{
			SpanAfterIndex = i;
		}

		if (CurrentSpan.StartOffset + CurrentSpan.Num == NewFreeSpan.StartOffset)
		{
			SpanBeforeIndex = i;
		}
	}

	if (SpanBeforeIndex != INDEX_NONE)
	{
		// Merge span before with new free span
		FLinearAllocation& SpanBefore = FreeSpans[SpanBeforeIndex];
		SpanBefore.Num += NewFreeSpan.Num;

		if (SpanAfterIndex != INDEX_NONE)
		{
			// Also merge span after with span before
			FLinearAllocation SpanAfter = FreeSpans[SpanAfterIndex];
			SpanBefore.Num += SpanAfter.Num;
			FreeSpans.RemoveAtSwap(SpanAfterIndex);
		}
	}
	else if (SpanAfterIndex != INDEX_NONE)
	{
		// Merge span after with new free span
		FLinearAllocation& SpanAfter = FreeSpans[SpanAfterIndex];
		SpanAfter.StartOffset = NewFreeSpan.StartOffset;
		SpanAfter.Num += NewFreeSpan.Num;
	}
	else 
	{
		// Couldn't merge, store new free span
		FreeSpans.Add(NewFreeSpan);
	}
}

int32 FGrowOnlySpanAllocator::SearchFreeList(int32 Num)
{
	// Search free list for first matching
	for (int32 i = 0; i < FreeSpans.Num(); i++)
	{
		FLinearAllocation CurrentSpan = FreeSpans[i];

		if (CurrentSpan.Num >= Num)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void UpdateGPUScene(FRHICommandList& RHICmdList, FScene& Scene)
{
	if (UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()))
	{
		if (GGPUSceneUploadEveryFrame || Scene.GPUScene.bUpdateAllPrimitives)
		{
			Scene.GPUScene.PrimitivesToUpdate.Reset();

			for (int32 i = 0; i < Scene.Primitives.Num(); i++)
			{
				Scene.GPUScene.PrimitivesToUpdate.Add(i);
			}

			Scene.GPUScene.bUpdateAllPrimitives = false;
		}

		{
			const int32 NumPrimitiveEntries = Scene.Primitives.Num();
			const uint32 PrimitiveSceneNumFloat4s = NumPrimitiveEntries * FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s;
			// Reserve enough space
			ResizeBufferIfNeeded(RHICmdList, Scene.GPUScene.PrimitiveBuffer, FMath::RoundUpToPowerOfTwo(PrimitiveSceneNumFloat4s));
		}
		
		{
			const int32 NumLightmapDataEntries = Scene.GPUScene.LightmapDataAllocator.GetMaxSize();
			const uint32 LightmapDataNumFloat4s = NumLightmapDataEntries * FLightmapSceneShaderData::LightmapDataStrideInFloat4s;
			ResizeBufferIfNeeded(RHICmdList, Scene.GPUScene.LightmapDataBuffer, FMath::RoundUpToPowerOfTwo(LightmapDataNumFloat4s));
		}

		const int32 NumPrimitiveDataUploads = Scene.GPUScene.PrimitivesToUpdate.Num();

		if (NumPrimitiveDataUploads > 0)
		{
			SCOPED_DRAW_EVENTF(RHICmdList, UpdateGPUScene, TEXT("UpdateGPUScene PrimitivesToUpdate = %u"), NumPrimitiveDataUploads);

			FScatterUploadBuilder PrimitivesUploadBuilder(NumPrimitiveDataUploads, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s, Scene.GPUScene.PrimitivesUploadScatterBuffer, Scene.GPUScene.PrimitivesUploadDataBuffer);

			int32 NumLightmapDataUploads = 0;

			for (int32 Index : Scene.GPUScene.PrimitivesToUpdate)
			{
				if (Index < Scene.PrimitiveSceneProxies.Num())
				{
					FPrimitiveSceneProxy* PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[Index];
					NumLightmapDataUploads += PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetNumLightmapDataEntries();

					FPrimitiveSceneShaderData PrimitiveSceneData(PrimitiveSceneProxy);
					PrimitivesUploadBuilder.Add(Index, &PrimitiveSceneData.Data[0]);
				}
				else
				{
					// Dirty index belongs to a primitive that is now out of bounds
					// Upload identity data to the dirty index to expose any incorrect shader indexing, even though no primitive should be referencing it
					FPrimitiveSceneShaderData PrimitiveSceneData;
					PrimitivesUploadBuilder.Add(Index, &PrimitiveSceneData.Data[0]);
				}
			}

			RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, Scene.GPUScene.PrimitiveBuffer.UAV);

			PrimitivesUploadBuilder.UploadTo(RHICmdList, Scene.GPUScene.PrimitiveBuffer);

			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, Scene.GPUScene.PrimitiveBuffer.UAV);

			if (GGPUSceneValidatePrimitiveBuffer && Scene.GPUScene.PrimitiveBuffer.NumBytes > 0)
			{
				UE_LOG(LogRenderer, Warning, TEXT("r.GPUSceneValidatePrimitiveBuffer enabled, doing slow readback from GPU"));
				FPrimitiveSceneShaderData* InstanceBufferCopy = (FPrimitiveSceneShaderData*)RHILockStructuredBuffer(Scene.GPUScene.PrimitiveBuffer.Buffer, 0, Scene.GPUScene.PrimitiveBuffer.NumBytes, RLM_ReadOnly);

				for (int32 Index = 0; Index < Scene.PrimitiveSceneProxies.Num(); Index++)
				{
					FPrimitiveSceneShaderData PrimitiveSceneData(Scene.PrimitiveSceneProxies[Index]);

					check(FMemory::Memcmp(&InstanceBufferCopy[Index], &PrimitiveSceneData, sizeof(FPrimitiveSceneShaderData)) == 0);
				}

				RHIUnlockStructuredBuffer(Scene.GPUScene.PrimitiveBuffer.Buffer);
			}

			if (NumLightmapDataUploads > 0)
			{
				FScatterUploadBuilder LightmapDataUploadBuilder(NumLightmapDataUploads, FLightmapSceneShaderData::LightmapDataStrideInFloat4s, Scene.GPUScene.LightmapUploadScatterBuffer, Scene.GPUScene.LightmapUploadDataBuffer);

				for (int32 Index : Scene.GPUScene.PrimitivesToUpdate)
				{
					if (Index < Scene.PrimitiveSceneProxies.Num())
					{
						FPrimitiveSceneProxy* PrimitiveSceneProxy = Scene.PrimitiveSceneProxies[Index];

						FPrimitiveSceneProxy::FLCIArray LCIs;
						PrimitiveSceneProxy->GetLCIs(LCIs);
					
						check(LCIs.Num() == PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetNumLightmapDataEntries());
						const int32 LightmapDataOffset = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetLightmapDataOffset();

						for (int32 i = 0; i < LCIs.Num(); i++)
						{
							FLightmapSceneShaderData LightmapSceneData(LCIs[i], Scene.GetFeatureLevel());
							LightmapDataUploadBuilder.Add(LightmapDataOffset + i, &LightmapSceneData.Data[0]);
						}
					}
				}

				RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, Scene.GPUScene.LightmapDataBuffer.UAV);

				LightmapDataUploadBuilder.UploadTo(RHICmdList, Scene.GPUScene.LightmapDataBuffer);

				RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, Scene.GPUScene.LightmapDataBuffer.UAV);
			}

			Scene.GPUScene.PrimitivesToUpdate.Reset();

			if (Scene.GPUScene.PrimitivesUploadDataBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize || Scene.GPUScene.PrimitivesUploadScatterBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize)
			{
				Scene.GPUScene.PrimitivesUploadDataBuffer.Release();
				Scene.GPUScene.PrimitivesUploadScatterBuffer.Release();
			}

			if (Scene.GPUScene.LightmapUploadDataBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize || Scene.GPUScene.LightmapUploadScatterBuffer.NumBytes > (uint32)GGPUSceneMaxPooledUploadBufferSize)
			{
				Scene.GPUScene.LightmapUploadDataBuffer.Release();
				Scene.GPUScene.LightmapUploadScatterBuffer.Release();
			}
		}
	}

	checkSlow(Scene.GPUScene.PrimitivesToUpdate.Num() == 0);
}

void UploadDynamicPrimitiveShaderDataForView(FRHICommandList& RHICmdList, FScene& Scene, FViewInfo& View)
{
	if (UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()))
	{
		FRWBufferStructured& ViewPrimitiveShaderDataBuffer = View.ViewState ? View.ViewState->PrimitiveShaderDataBuffer : View.OneFramePrimitiveShaderDataBuffer;

		const int32 NumPrimitiveEntries = Scene.Primitives.Num() + View.DynamicPrimitiveShaderData.Num();
		const uint32 PrimitiveSceneNumFloat4s = NumPrimitiveEntries * FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s;

		// Reserve enough space
		ResizeBufferIfNeeded(RHICmdList, ViewPrimitiveShaderDataBuffer, FMath::RoundUpToPowerOfTwo(PrimitiveSceneNumFloat4s));

		// Copy scene primitive data into view primitive data
		MemcpyBuffer(RHICmdList, Scene.GPUScene.PrimitiveBuffer, ViewPrimitiveShaderDataBuffer, Scene.Primitives.Num() * FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s);

		const int32 NumPrimitiveDataUploads = View.DynamicPrimitiveShaderData.Num();

		// Append View.DynamicPrimitiveShaderData to the end of the view primitive data buffer
		if (NumPrimitiveDataUploads > 0)
		{
			FScatterUploadBuilder PrimitivesUploadBuilder(NumPrimitiveDataUploads, FPrimitiveSceneShaderData::PrimitiveDataStrideInFloat4s, Scene.GPUScene.PrimitivesUploadScatterBuffer, Scene.GPUScene.PrimitivesUploadDataBuffer);

			for (int32 DynamicUploadIndex = 0; DynamicUploadIndex < View.DynamicPrimitiveShaderData.Num(); DynamicUploadIndex++)
			{
				FPrimitiveSceneShaderData PrimitiveSceneData(View.DynamicPrimitiveShaderData[DynamicUploadIndex]);
				// Place dynamic primitive shader data just after scene primitive data
				PrimitivesUploadBuilder.Add(Scene.Primitives.Num() + DynamicUploadIndex, &PrimitiveSceneData.Data[0]);
			}

			RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, ViewPrimitiveShaderDataBuffer.UAV);

			PrimitivesUploadBuilder.UploadTo(RHICmdList, ViewPrimitiveShaderDataBuffer);
		}

		// Update view uniform buffer
		View.CachedViewUniformShaderParameters->LightmapSceneData = Scene.GPUScene.LightmapDataBuffer.SRV;
		View.CachedViewUniformShaderParameters->PrimitiveSceneData = ViewPrimitiveShaderDataBuffer.SRV;
		View.ViewUniformBuffer.UpdateUniformBufferImmediate(*View.CachedViewUniformShaderParameters);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, ViewPrimitiveShaderDataBuffer.UAV);

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_PrimitiveIdBufferEmulation);

			//@todo MeshCommandPipeline - remove PrimitiveIdBufferEmulation once drawing policies are gone
			View.OneFramePrimitiveIdBufferEmulation.Release();
			View.OneFramePrimitiveIdBufferEmulation.Initialize(sizeof(uint32), NumPrimitiveEntries, PF_R32_UINT);
			uint32* LockedPrimitiveIds = (uint32*)RHILockVertexBuffer(View.OneFramePrimitiveIdBufferEmulation.Buffer, 0, View.OneFramePrimitiveIdBufferEmulation.NumBytes, RLM_WriteOnly);

			for (int32 i = 0; i < NumPrimitiveEntries; i++)
			{
				LockedPrimitiveIds[i] = i;
			}

			RHIUnlockVertexBuffer(View.OneFramePrimitiveIdBufferEmulation.Buffer);
		}
	}
}

void AddPrimitiveToUpdateGPU(FScene& Scene, int32 PrimitiveId)
{
	if (UseGPUScene(GMaxRHIShaderPlatform, Scene.GetFeatureLevel()))
	{
		Scene.GPUScene.PrimitivesToUpdate.Add(PrimitiveId);
	}
}
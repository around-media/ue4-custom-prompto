// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneCore.cpp: Core scene implementation.
=============================================================================*/

#include "SceneCore.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "DepthRendering.h"
#include "SceneHitProxyRendering.h"
#include "VelocityRendering.h"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "RendererModule.h"
#include "ScenePrivate.h"
#include "Containers/AllocatorFixedSizeFreeList.h"
#include "MaterialShared.h"

int32 GUnbuiltPreviewShadowsInGame = 1;
FAutoConsoleVariableRef CVarUnbuiltPreviewShadowsInGame(
	TEXT("r.Shadow.UnbuiltPreviewInGame"),
	GUnbuiltPreviewShadowsInGame,
	TEXT("Whether to render unbuilt preview shadows in game.  When enabled and lighting is not built, expensive preview shadows will be rendered in game.  When disabled, lighting in game and editor won't match which can appear to be a bug."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

/**
 * Fixed Size pool allocator for FLightPrimitiveInteractions
 */
#define FREE_LIST_GROW_SIZE ( 16384 / sizeof(FLightPrimitiveInteraction) )
TAllocatorFixedSizeFreeList<sizeof(FLightPrimitiveInteraction), FREE_LIST_GROW_SIZE> GLightPrimitiveInteractionAllocator;

uint32 FRendererModule::GetNumDynamicLightsAffectingPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FLightCacheInterface* LCI)
{
	uint32 NumDynamicLights = 0;

	FLightPrimitiveInteraction *LightList = PrimitiveSceneInfo->LightList;
	while ( LightList )
	{
		const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

		// Determine the interaction type between the mesh and the light.
		FLightInteraction LightInteraction = FLightInteraction::Dynamic();
		if(LCI)
		{
			LightInteraction = LCI->GetInteraction(LightSceneInfo->Proxy);
		}

		// Don't count light-mapped or irrelevant lights.
		if(LightInteraction.GetType() != LIT_CachedIrrelevant && LightInteraction.GetType() != LIT_CachedLightMap)
		{
			++NumDynamicLights;
		}

		LightList = LightList->GetNextLight();
	}

	return NumDynamicLights;
}

/*-----------------------------------------------------------------------------
	FLightPrimitiveInteraction
-----------------------------------------------------------------------------*/

/**
 * Custom new
 */
void* FLightPrimitiveInteraction::operator new(size_t Size)
{
	// doesn't support derived classes with a different size
	checkSlow(Size == sizeof(FLightPrimitiveInteraction));
	return GLightPrimitiveInteractionAllocator.Allocate();
	//return FMemory::Malloc(Size);
}

/**
 * Custom delete
 */
void FLightPrimitiveInteraction::operator delete(void *RawMemory)
{
	GLightPrimitiveInteractionAllocator.Free(RawMemory);
	//FMemory::Free(RawMemory);
}	

/**
 * Initialize the memory pool with a default size from the ini file.
 * Called at render thread startup. Since the render thread is potentially
 * created/destroyed multiple times, must make sure we only do it once.
 */
void FLightPrimitiveInteraction::InitializeMemoryPool()
{
	static bool bAlreadyInitialized = false;
	if (!bAlreadyInitialized)
	{
		bAlreadyInitialized = true;
		int32 InitialBlockSize = 0;
		GConfig->GetInt(TEXT("MemoryPools"), TEXT("FLightPrimitiveInteractionInitialBlockSize"), InitialBlockSize, GEngineIni);
		GLightPrimitiveInteractionAllocator.Grow(InitialBlockSize);
	}
}

/**
* Returns current size of memory pool
*/
uint32 FLightPrimitiveInteraction::GetMemoryPoolSize()
{
	return GLightPrimitiveInteractionAllocator.GetAllocatedSize();
}

void FLightPrimitiveInteraction::Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// Attach the light to the primitive's static meshes.
	bool bDynamic = true;
	bool bRelevant = false;
	bool bIsLightMapped = true;
	bool bShadowMapped = false;

	// Determine the light's relevance to the primitive.
	check(PrimitiveSceneInfo->Proxy && LightSceneInfo->Proxy);
	PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo->Proxy, bDynamic, bRelevant, bIsLightMapped, bShadowMapped);

	if (bRelevant && bDynamic
		// Don't let lights with static shadowing or static lighting affect primitives that should use static lighting, but don't have valid settings (lightmap res 0, etc)
		// This prevents those components with invalid lightmap settings from causing lighting to remain unbuilt after a build
		&& !(LightSceneInfo->Proxy->HasStaticShadowing() && PrimitiveSceneInfo->Proxy->HasStaticLighting() && !PrimitiveSceneInfo->Proxy->HasValidSettingsForStaticLighting()))
	{
		const bool bTranslucentObjectShadow = LightSceneInfo->Proxy->CastsTranslucentShadows() && PrimitiveSceneInfo->Proxy->CastsVolumetricTranslucentShadow();
		const bool bInsetObjectShadow = 
			// Currently only supporting inset shadows on directional lights, but could be made to work with any whole scene shadows
			LightSceneInfo->Proxy->GetLightType() == LightType_Directional
			&& PrimitiveSceneInfo->Proxy->CastsInsetShadow();

		// Movable directional lights determine shadow relevance dynamically based on the view and CSM settings. Interactions are only required for per-object cases.
		if (LightSceneInfo->Proxy->GetLightType() != LightType_Directional || LightSceneInfo->Proxy->HasStaticShadowing() || bTranslucentObjectShadow || bInsetObjectShadow)
		{
			// Create the light interaction.
			FLightPrimitiveInteraction* Interaction = new FLightPrimitiveInteraction(LightSceneInfo, PrimitiveSceneInfo, bDynamic, bIsLightMapped, bShadowMapped, bTranslucentObjectShadow, bInsetObjectShadow);
		} //-V773
	}
}

void FLightPrimitiveInteraction::Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction)
{
	delete LightPrimitiveInteraction;
}

FLightPrimitiveInteraction::FLightPrimitiveInteraction(
	FLightSceneInfo* InLightSceneInfo,
	FPrimitiveSceneInfo* InPrimitiveSceneInfo,
	bool bInIsDynamic,
	bool bInLightMapped,
	bool bInIsShadowMapped,
	bool bInHasTranslucentObjectShadow,
	bool bInHasInsetObjectShadow
	) :
	LightSceneInfo(InLightSceneInfo),
	PrimitiveSceneInfo(InPrimitiveSceneInfo),
	LightId(InLightSceneInfo->Id),
	bLightMapped(bInLightMapped),
	bIsDynamic(bInIsDynamic),
	bIsShadowMapped(bInIsShadowMapped),
	bUncachedStaticLighting(false),
	bHasTranslucentObjectShadow(bInHasTranslucentObjectShadow),
	bHasInsetObjectShadow(bInHasInsetObjectShadow),
	bSelfShadowOnly(false),
	bES2DynamicPointLight(false)
{
	// Determine whether this light-primitive interaction produces a shadow.
	if(PrimitiveSceneInfo->Proxy->HasStaticLighting())
	{
		const bool bHasStaticShadow =
			LightSceneInfo->Proxy->HasStaticShadowing() &&
			LightSceneInfo->Proxy->CastsStaticShadow() &&
			PrimitiveSceneInfo->Proxy->CastsStaticShadow();
		const bool bHasDynamicShadow =
			!LightSceneInfo->Proxy->HasStaticLighting() &&
			LightSceneInfo->Proxy->CastsDynamicShadow() &&
			PrimitiveSceneInfo->Proxy->CastsDynamicShadow();
		bCastShadow = bHasStaticShadow || bHasDynamicShadow;
	}
	else
	{
		bCastShadow = LightSceneInfo->Proxy->CastsDynamicShadow() && PrimitiveSceneInfo->Proxy->CastsDynamicShadow();
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(bCastShadow && bIsDynamic)
	{
		// Determine the type of dynamic shadow produced by this light.
		if (PrimitiveSceneInfo->Proxy->HasStaticLighting()
			&& PrimitiveSceneInfo->Proxy->CastsStaticShadow()
			// Don't mark unbuilt for movable primitives which were built with lightmaps but moved into a new light's influence
			&& PrimitiveSceneInfo->Proxy->GetLightmapType() != ELightmapType::ForceSurface
			&& (LightSceneInfo->Proxy->HasStaticLighting() || (LightSceneInfo->Proxy->HasStaticShadowing() && !bInIsShadowMapped)))
		{
			// Update the game thread's counter of number of uncached static lighting interactions.
			bUncachedStaticLighting = true;

			if (!GUnbuiltPreviewShadowsInGame && !InLightSceneInfo->Scene->IsEditorScene())
			{
				bCastShadow = false;
			}
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			LightSceneInfo->NumUnbuiltInteractions++;
	#endif

	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FPlatformAtomics::InterlockedIncrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);
	#endif

#if WITH_EDITOR
			PrimitiveSceneInfo->Proxy->NumUncachedStaticLightingInteractions++;
#endif
		}
	}
#endif

	bSelfShadowOnly = PrimitiveSceneInfo->Proxy->CastsSelfShadowOnly();

	if (bIsDynamic)
	{
		// Add the interaction to the light's interaction list.
		PrevPrimitiveLink = PrimitiveSceneInfo->Proxy->IsMeshShapeOftenMoving() ? &LightSceneInfo->DynamicInteractionOftenMovingPrimitiveList : &LightSceneInfo->DynamicInteractionStaticPrimitiveList;

		// mobile movable point lights
		if (PrimitiveSceneInfo->Scene->GetFeatureLevel() < ERHIFeatureLevel::SM4 && LightSceneInfo->Proxy->GetLightType() == LightType_Point && LightSceneInfo->Proxy->IsMovable())
		{
			bES2DynamicPointLight = true;
			PrimitiveSceneInfo->NumMobileMovablePointLights++;
			// enable dynamic path for primitives affected by dynamic point lights
			PrimitiveSceneInfo->Proxy->bHasMobileMovablePointLightInteraction = (PrimitiveSceneInfo->NumMobileMovablePointLights != 0);
			// The mobile renderer needs to use a different shader for movable point lights, so we have to update any static meshes in drawlists
			PrimitiveSceneInfo->BeginDeferredUpdateStaticMeshes();
		}
	}

	FlushCachedShadowMapData();

	NextPrimitive = *PrevPrimitiveLink;
	if(*PrevPrimitiveLink)
	{
		(*PrevPrimitiveLink)->PrevPrimitiveLink = &NextPrimitive;
	}
	*PrevPrimitiveLink = this;

	// Add the interaction to the primitives' interaction list.
	PrevLightLink = &PrimitiveSceneInfo->LightList;
	NextLight = *PrevLightLink;
	if(*PrevLightLink)
	{
		(*PrevLightLink)->PrevLightLink = &NextLight;
	}
	*PrevLightLink = this;
}

FLightPrimitiveInteraction::~FLightPrimitiveInteraction()
{
	check(IsInRenderingThread());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Update the game thread's counter of number of uncached static lighting interactions.
	if(bUncachedStaticLighting)
	{
		LightSceneInfo->NumUnbuiltInteractions--;
		FPlatformAtomics::InterlockedDecrement(&PrimitiveSceneInfo->Scene->NumUncachedStaticLightingInteractions);
#if WITH_EDITOR
		PrimitiveSceneInfo->Proxy->NumUncachedStaticLightingInteractions--;
#endif
	}
#endif

	FlushCachedShadowMapData();

	// Track mobile movable point light count
	if (bES2DynamicPointLight)
	{
		PrimitiveSceneInfo->NumMobileMovablePointLights--;
		// enable dynamic path for primitives affected by dynamic point lights
		PrimitiveSceneInfo->Proxy->bHasMobileMovablePointLightInteraction = (PrimitiveSceneInfo->NumMobileMovablePointLights != 0);
		// The mobile renderer needs to use a different shader for movable point lights, so we have to update any static meshes in drawlists
		PrimitiveSceneInfo->BeginDeferredUpdateStaticMeshes();
	}

	// Remove the interaction from the light's interaction list.
	if(NextPrimitive)
	{
		NextPrimitive->PrevPrimitiveLink = PrevPrimitiveLink;
	}
	*PrevPrimitiveLink = NextPrimitive;

	// Remove the interaction from the primitive's interaction list.
	if(NextLight)
	{
		NextLight->PrevLightLink = PrevLightLink;
	}
	*PrevLightLink = NextLight;
}

void FLightPrimitiveInteraction::FlushCachedShadowMapData()
{
	if (LightSceneInfo && PrimitiveSceneInfo && PrimitiveSceneInfo->Proxy && PrimitiveSceneInfo->Scene)
	{
		if (bCastShadow && !PrimitiveSceneInfo->Proxy->IsMeshShapeOftenMoving())
		{
			FCachedShadowMapData* CachedShadowMapData = PrimitiveSceneInfo->Scene->CachedShadowMaps.Find(LightSceneInfo->Id);

			if (CachedShadowMapData)
			{
				CachedShadowMapData->ShadowMap.Release();
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FStaticMesh
-----------------------------------------------------------------------------*/

void FStaticMesh::LinkDrawList(FStaticMesh::FDrawListElementLink* Link)
{
	check(IsInRenderingThread());
	check(!DrawListLinks.Contains(Link));
	DrawListLinks.Add(Link);
}

void FStaticMesh::UnlinkDrawList(FStaticMesh::FDrawListElementLink* Link)
{
	check(IsInRenderingThread());
	verify(DrawListLinks.RemoveSingleSwap(Link) == 1);
}

void FStaticMesh::AddToDrawLists(FRHICommandListImmediate& RHICmdList, FScene* Scene)
{
	const auto FeatureLevel = Scene->GetFeatureLevel();
	
	// TEMP: optionally allow to switch off mesh command caching for cooked mobile
	extern int32 MeshDrawCommandPipelineMobileCooked();
	const int32 CacheMeshCommandsVal = MeshDrawCommandPipelineMobileCooked();

	if (SupportsCachingMeshDrawCommands(VertexFactory, PrimitiveSceneInfo->Proxy) && CacheMeshCommandsVal > 0)
	{
		CacheMeshDrawCommands(Scene);
	}

	if (!PrimitiveSceneInfo->Proxy->ShouldRenderInMainPass() || !ShouldIncludeDomainInMeshPass(MaterialRenderProxy->GetMaterial(FeatureLevel)->GetMaterialDomain()))
	{
		return;
	}

	if (CastShadow && CacheMeshCommandsVal < 2)
	{
		FShadowDepthDrawingPolicyFactory::AddStaticMesh(Scene, this);
	}

	if (IsTranslucent(FeatureLevel))
	{
		return;
	}

	if (Scene->GetShadingPath() == EShadingPath::Mobile && CacheMeshCommandsVal < 2)
	{
		if (bUseForMaterial)
		{
			// Add the static mesh to the DPG's base pass draw list.
			FMobileBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(RHICmdList, Scene, this);
		}
	}
}

void FStaticMesh::CacheMeshDrawCommands(FScene* Scene)
{
	//@todo - only need material uniform buffers to be created since we are going to cache pointers to them
	// Any updates (after initial creation) don't need to be forced here
	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

	QUICK_SCOPE_CYCLE_COUNTER(STAT_CacheMeshDrawCommands);
	FMemMark Mark(FMemStack::Get());

	EShadingPath ShadingPath = Scene->GetShadingPath();

	for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
	{
		EMeshPass::Type PassType = (EMeshPass::Type)PassIndex;

		if ((FPassProcessorManager::GetPassFlags(ShadingPath, PassType) & EMeshPassFlags::CachedMeshCommands) != EMeshPassFlags::None)
		{
			FCachedPassMeshDrawList& SceneDrawList = Scene->CachedDrawLists[PassType];
			FCachedPassMeshDrawListContext CachedPassMeshDrawListContext(PassMeshCommandIndices[PassType], SceneDrawList, *Scene);

			PassProcessorCreateFunction CreateFunction = FPassProcessorManager::GetCreateFunction(ShadingPath, PassType);
			FMeshPassProcessor* PassMeshProcessor = CreateFunction(Scene, nullptr, CachedPassMeshDrawListContext);

			check(!bRequiresPerElementVisibility);
			uint64 BatchElementMask = ~0ull;
			PassMeshProcessor->AddMeshBatch(*this, BatchElementMask, PrimitiveSceneInfo->Proxy);
		
			PassMeshProcessor->~FMeshPassProcessor();
		}
	}
}

void FStaticMesh::RemoveFromDrawLists(bool bMeshIsBeingDestroyed)
{
	if (bMeshIsBeingDestroyed)
	{
		// This is cheaper than calling RemoveFromDrawLists, since it 
		// doesn't unlink meshes which are about to be destroyed
		for (int32 i = 0; i < DrawListLinks.Num(); i++)
		{
			DrawListLinks[i]->Remove(false);
		}
	}
	else
	{
		// Remove the mesh from all draw lists.
		while(DrawListLinks.Num())
		{
			TRefCountPtr<FStaticMesh::FDrawListElementLink> Link = DrawListLinks[0];
			const int32 OriginalNumLinks = DrawListLinks.Num();
			// This will call UnlinkDrawList.
			Link->Remove(true);
			check(DrawListLinks.Num() == OriginalNumLinks - 1);
			if(DrawListLinks.Num())
			{
				check(DrawListLinks[0] != Link);
			}
		}
	}

	FScene* Scene = PrimitiveSceneInfo->Scene;

	for (int32 PassIndex = 0; PassIndex < ARRAY_COUNT(PassMeshCommandIndices); PassIndex++)
	{
		const int32 CommandIndex = PassMeshCommandIndices[PassIndex];
		if (CommandIndex != -1)
		{
			FCachedPassMeshDrawList& PassDrawList = Scene->CachedDrawLists[PassIndex];

			FCachedMeshDrawCommandInfo& CachedMeshDrawCommandInfo = PassDrawList.MeshDrawCommandInfo[CommandIndex];
			const FSetElementId StateBucketId = FSetElementId::FromInteger(CachedMeshDrawCommandInfo.StateBucketId);
			checkSlow(StateBucketId.IsValidId());
			FMeshDrawCommandStateBucket& StateBucket = Scene->CachedMeshDrawCommandStateBuckets[StateBucketId];

			if (StateBucket.Num == 1)
			{
				Scene->CachedMeshDrawCommandStateBuckets.Remove(StateBucketId);
			}
			else
			{
				StateBucket.Num--;
			}
		
			PassDrawList.MeshDrawCommandInfo.RemoveAt(CommandIndex);
			PassDrawList.MeshDrawCommands.RemoveAt(CommandIndex);
		}

		PassMeshCommandIndices[PassIndex] = -1;
	}
}

FStaticMesh::~FStaticMesh()
{
	FScene* Scene = PrimitiveSceneInfo->Scene;
	// Remove this static mesh from the scene's list.
	Scene->StaticMeshes.RemoveAt(Id);

	if (BatchVisibilityId != INDEX_NONE)
	{
		Scene->StaticMeshBatchVisibility.RemoveAt(BatchVisibilityId);
	}

	RemoveFromDrawLists(true);
}

/** Initialization constructor. */
FExponentialHeightFogSceneInfo::FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent):
	Component(InComponent),
	FogHeight(InComponent->GetComponentLocation().Z),
	// Scale the densities back down to their real scale
	// Artists edit the densities scaled up so they aren't entering in minuscule floating point numbers
	FogDensity(InComponent->FogDensity / 1000.0f),
	FogHeightFalloff(InComponent->FogHeightFalloff / 1000.0f),
	FogMaxOpacity(InComponent->FogMaxOpacity),
	StartDistance(InComponent->StartDistance),
	FogCutoffDistance(InComponent->FogCutoffDistance),
	LightTerminatorAngle(0),
	DirectionalInscatteringExponent(InComponent->DirectionalInscatteringExponent),
	DirectionalInscatteringStartDistance(InComponent->DirectionalInscatteringStartDistance),
	DirectionalInscatteringColor(InComponent->DirectionalInscatteringColor)
{
	FogColor = InComponent->InscatteringColorCubemap ? InComponent->InscatteringTextureTint : InComponent->FogInscatteringColor;
	InscatteringColorCubemap = InComponent->InscatteringColorCubemap;
	InscatteringColorCubemapAngle = InComponent->InscatteringColorCubemapAngle * (PI / 180.f);
	FullyDirectionalInscatteringColorDistance = InComponent->FullyDirectionalInscatteringColorDistance;
	NonDirectionalInscatteringColorDistance = InComponent->NonDirectionalInscatteringColorDistance;

	bEnableVolumetricFog = InComponent->bEnableVolumetricFog;
	VolumetricFogScatteringDistribution = FMath::Clamp(InComponent->VolumetricFogScatteringDistribution, -.99f, .99f);
	VolumetricFogAlbedo = FLinearColor(InComponent->VolumetricFogAlbedo);
	VolumetricFogEmissive = InComponent->VolumetricFogEmissive;

	// Apply a scale so artists don't have to work with tiny numbers.  
	const float UnitScale = 1.0f / 10000.0f;
	VolumetricFogEmissive.R = FMath::Max(VolumetricFogEmissive.R * UnitScale, 0.0f);
	VolumetricFogEmissive.G = FMath::Max(VolumetricFogEmissive.G * UnitScale, 0.0f);
	VolumetricFogEmissive.B = FMath::Max(VolumetricFogEmissive.B * UnitScale, 0.0f);
	VolumetricFogExtinctionScale = FMath::Max(InComponent->VolumetricFogExtinctionScale, 0.0f);
	VolumetricFogDistance = FMath::Max(InComponent->VolumetricFogDistance, 0.0f);
	VolumetricFogStaticLightingScatteringIntensity = FMath::Max(InComponent->VolumetricFogStaticLightingScatteringIntensity, 0.0f);
	bOverrideLightColorsWithFogInscatteringColors = InComponent->bOverrideLightColorsWithFogInscatteringColors;
}

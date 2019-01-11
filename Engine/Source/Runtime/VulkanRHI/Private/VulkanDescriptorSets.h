// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipelineState.h: Vulkan pipeline state definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanMemory.h"
#include "VulkanGlobalUniformBuffer.h"
#include "VulkanShaderResources.h"

class FVulkanCommandBufferPool;

// (AlwaysCompareData == true) because of CRC-32 hash collisions
class FVulkanDSetKey : public TDataKey<FVulkanDSetKey, true> {};
class FVulkanDSetsKey : public TDataKey<FVulkanDSetsKey, true> {};

struct FUniformBufferGatherInfo
{
	FUniformBufferGatherInfo()
	{
		FMemory::Memzero(CodeHeaders);
	}

	// These maps are used to find UBs that are used on multiple stages
	TMap<uint32, VkShaderStageFlags>	UBLayoutsToUsedStageMap;
	TMap<uint32, VkShaderStageFlags>	CommonUBLayoutsToStageMap;

	const FVulkanShaderHeader*	CodeHeaders[ShaderStage::NumStages];
};

struct FInputAttachmentData
{
	uint16	BindingIndex = UINT16_MAX;
	uint8	DescriptorSet = UINT8_MAX;
	FVulkanShaderHeader::EAttachmentType Type = FVulkanShaderHeader::EAttachmentType::Count;
};

// Information for remapping descriptor sets when combining layouts
struct FDescriptorSetRemappingInfo
{
	struct FRemappingInfo
	{
		uint16	NewDescriptorSet = UINT16_MAX;
		uint16	NewBindingIndex = UINT16_MAX;
	};
	struct FUBRemappingInfo
	{
		// Remapping is only valid if there is constant data
		FRemappingInfo			Remapping;

		bool					bHasConstantData = false;
		const bool				bPadding = false;	// padding is need on memcmp/MemCrc to make sure mem align
		//bool					bIsRedundant = false;
	};

	struct FSetInfo
	{
		TArray<VkDescriptorType>	Types;
		uint16						NumImageInfos = 0;
		uint16						NumBufferInfos = 0;
	};
	TArray<FSetInfo>	SetInfos;

	struct FStageInfo
	{
		TArray<FRemappingInfo>		Globals;
		TArray<FUBRemappingInfo>	UniformBuffers;
		TArray<uint16>				PackedUBBindingIndices;
		uint16						PackedUBDescriptorSet = UINT16_MAX;
		uint16						Pad0 = 0;

		inline bool IsEmpty() const
		{
			if (Globals.Num() != 0)
			{
				return false;
			}

			if (UniformBuffers.Num() != 0)
			{
				return false;
			}

			if (PackedUBBindingIndices.Num() != 0)
			{
				return false;
			}

			return true;
		}
	};
	TStaticArray<FStageInfo, ShaderStage::NumStages>	StageInfos;

	TArray<FInputAttachmentData>						InputAttachmentData;

	inline bool operator==(const FDescriptorSetRemappingInfo& In) const
	{
		if (InputAttachmentData.Num() != In.InputAttachmentData.Num())
		{
			return false;
		}

		if (SetInfos.Num() != In.SetInfos.Num())
		{
			return false;
		}

		if (FMemory::Memcmp(InputAttachmentData.GetData(), In.InputAttachmentData.GetData(), sizeof(FInputAttachmentData) * InputAttachmentData.Num()))
		{
			return false;
		}

		for (int32 SetInfosIndex = 0; SetInfosIndex < SetInfos.Num(); ++SetInfosIndex)
		{
			int32 SetInfosNums = SetInfos[SetInfosIndex].Types.Num();
			if (SetInfos[SetInfosIndex].NumBufferInfos != In.SetInfos[SetInfosIndex].NumBufferInfos ||
				SetInfos[SetInfosIndex].NumImageInfos != In.SetInfos[SetInfosIndex].NumImageInfos ||
				SetInfosNums != In.SetInfos[SetInfosIndex].Types.Num() ||
				(SetInfosNums != 0 && FMemory::Memcmp(SetInfos[SetInfosIndex].Types.GetData(), In.SetInfos[SetInfosIndex].Types.GetData(), sizeof(VkDescriptorType) * SetInfosNums)))
			{
				return false;
			}
		}

		for (uint32 StageInfosIndex = 0; StageInfosIndex < ShaderStage::NumStages; ++StageInfosIndex)
		{
			if (StageInfos[StageInfosIndex].PackedUBDescriptorSet != In.StageInfos[StageInfosIndex].PackedUBDescriptorSet ||
				StageInfos[StageInfosIndex].Pad0 != In.StageInfos[StageInfosIndex].Pad0 ||
				StageInfos[StageInfosIndex].Globals.Num() != In.StageInfos[StageInfosIndex].Globals.Num() ||
				StageInfos[StageInfosIndex].PackedUBBindingIndices.Num() != In.StageInfos[StageInfosIndex].PackedUBBindingIndices.Num() ||
				StageInfos[StageInfosIndex].UniformBuffers.Num() != In.StageInfos[StageInfosIndex].UniformBuffers.Num() ||
				FMemory::Memcmp(StageInfos[StageInfosIndex].Globals.GetData(), In.StageInfos[StageInfosIndex].Globals.GetData(), sizeof(FRemappingInfo) * StageInfos[StageInfosIndex].Globals.Num()) ||
				FMemory::Memcmp(StageInfos[StageInfosIndex].PackedUBBindingIndices.GetData(), In.StageInfos[StageInfosIndex].PackedUBBindingIndices.GetData(), sizeof(uint16) * StageInfos[StageInfosIndex].PackedUBBindingIndices.Num()) ||
				FMemory::Memcmp(StageInfos[StageInfosIndex].UniformBuffers.GetData(), In.StageInfos[StageInfosIndex].UniformBuffers.GetData(), sizeof(FUBRemappingInfo) * StageInfos[StageInfosIndex].UniformBuffers.Num()))
			{
				return false;
			}
		}

		return true;
	}

	inline bool operator != (const FDescriptorSetRemappingInfo& In) const
	{
		return !(*this == In);
	}

	inline bool IsEmpty() const
	{
		if (SetInfos.Num() == 0)
		{
			for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
			{
				if (!StageInfos[Index].IsEmpty())
				{
					return false;
				}
			}

			return true;
		}

		return false;
	}

	uint32 AddGlobal(uint32 Stage, int32 GlobalIndex, uint32 NewDescriptorSet, VkDescriptorType InType, uint16 CombinedSamplerStateAlias)
	{
		// Combined Image Samplers point both the texture and the sampler to the same descriptor
		check(CombinedSamplerStateAlias == UINT16_MAX || InType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		uint32 NewBindingIndex = CombinedSamplerStateAlias == UINT16_MAX ? SetInfos[NewDescriptorSet].Types.Add(InType) : StageInfos[Stage].Globals[CombinedSamplerStateAlias].NewBindingIndex;

		int32 RemappingIndex = StageInfos[Stage].Globals.AddDefaulted();
		check(RemappingIndex == GlobalIndex);
		FDescriptorSetRemappingInfo::FRemappingInfo& Remapping = StageInfos[Stage].Globals[RemappingIndex];
		Remapping.NewDescriptorSet = NewDescriptorSet;
		Remapping.NewBindingIndex = NewBindingIndex;

		return NewBindingIndex;
	}

	uint32 AddPackedUB(uint32 Stage, int32 PackUBIndex, uint32 NewDescriptorSet, VkDescriptorType InType)
	{
		uint32 NewBindingIndex = SetInfos[NewDescriptorSet].Types.Add(InType);
		if (StageInfos[Stage].PackedUBDescriptorSet == UINT16_MAX)
		{
			StageInfos[Stage].PackedUBDescriptorSet = NewDescriptorSet;
		}
		else
		{
			ensure(StageInfos[Stage].PackedUBDescriptorSet == NewDescriptorSet);
		}
		int32 RemappingIndex = StageInfos[Stage].PackedUBBindingIndices.Add(NewBindingIndex);
		check(RemappingIndex == PackUBIndex);

		return NewBindingIndex;
	}

	FDescriptorSetRemappingInfo::FUBRemappingInfo AddUBWithData(uint32 Stage, int32 UniformBufferIndex, uint32 NewDescriptorSet, VkDescriptorType InType, uint32& OutNewBindingIndex)
	{
		OutNewBindingIndex = SetInfos[NewDescriptorSet].Types.Add(InType);

		int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.AddDefaulted();
		check(UBRemappingIndex == UniformBufferIndex);
		FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemapping = StageInfos[Stage].UniformBuffers[UBRemappingIndex];
		UBRemapping.bHasConstantData = true;
		UBRemapping.Remapping.NewDescriptorSet = NewDescriptorSet;
		UBRemapping.Remapping.NewBindingIndex = OutNewBindingIndex;

		return UBRemapping;
	}

	void AddRedundantUB(uint32 Stage, int32 UniformBufferIndex, const FDescriptorSetRemappingInfo::FUBRemappingInfo* InExistingUBInfo)
	{
		int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.AddDefaulted();
		check(UBRemappingIndex == UniformBufferIndex);
		FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemapping = StageInfos[Stage].UniformBuffers[UBRemappingIndex];
		//UBRemapping.bIsRedundant = true;
		UBRemapping.bHasConstantData = InExistingUBInfo->bHasConstantData;
/*
		UBRemapping.EntriesRemappingInfo = InExistingUBInfo->EntriesRemappingInfo;
*/
		UBRemapping.Remapping = InExistingUBInfo->Remapping;
	}

	void AddUBResourceOnly(uint32 Stage, int32 UniformBufferIndex)
	{
		int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.AddDefaulted();
		check(UBRemappingIndex == UniformBufferIndex);
		FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemapping = StageInfos[Stage].UniformBuffers[UBRemappingIndex];
		UBRemapping.bHasConstantData = false;
	}

/*
	uint32 AddUBResourceEntryToSetAndRemapping(int32 Stage, int32 UniformBufferIndex, int32 ResourceEntryIndex, uint32 NewDescriptorSet, VkDescriptorType InType, uint8 CombinedSamplerStateAlias)
	{
		// Combined Image Samplers point both the texture and the sampler to the same descriptor
		check(CombinedSamplerStateAlias == UINT8_MAX || InType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		uint32 NewBindingIndex = CombinedSamplerStateAlias == UINT8_MAX ? SetInfos[NewDescriptorSet].Types.Add(InType) : StageInfos[Stage].UniformBuffers[UniformBufferIndex].EntriesRemappingInfo[CombinedSamplerStateAlias].NewBindingIndex;

		int32 UBEntryRemappingIndex = StageInfos[Stage].UniformBuffers[UniformBufferIndex].EntriesRemappingInfo.AddDefaulted();
		check(UBEntryRemappingIndex == ResourceEntryIndex);
		FDescriptorSetRemappingInfo::FRemappingInfo& UBEntryRemapping = StageInfos[Stage].UniformBuffers[UniformBufferIndex].EntriesRemappingInfo[UBEntryRemappingIndex];
		UBEntryRemapping.NewDescriptorSet = NewDescriptorSet;
		UBEntryRemapping.NewBindingIndex = NewBindingIndex;

		return NewBindingIndex;
	}
*/


	static_assert(
		sizeof(FRemappingInfo) == (sizeof(FRemappingInfo::NewDescriptorSet) + sizeof(FRemappingInfo::NewBindingIndex)),
		"FRemappingInfo should not have padding! structure is used for MemCrc32/Memcmp");

	static_assert(
		sizeof(FUBRemappingInfo) == (sizeof(FRemappingInfo) + sizeof(FUBRemappingInfo::bHasConstantData) + sizeof(FUBRemappingInfo::bPadding)),
		"FUBRemappingInfo should not have padding! structure is used for MemCrc32/Memcmp");
};

// Information for the layout of descriptor sets; does not hold runtime objects
class FVulkanDescriptorSetsLayoutInfo
{
public:
	FVulkanDescriptorSetsLayoutInfo()
	{
		FMemory::Memzero(LayoutTypes);
	}

	inline uint32 GetTypesUsed(VkDescriptorType Type) const
	{
		return LayoutTypes[Type];
	}

	struct FSetLayout
	{
		TArray<VkDescriptorSetLayoutBinding> LayoutBindings;
		uint32 Hash;

		inline void GenerateHash()
		{
			Hash = FCrc::MemCrc32(LayoutBindings.GetData(), sizeof(VkDescriptorSetLayoutBinding) * LayoutBindings.Num());
		}

		friend uint32 GetTypeHash(const FSetLayout& In)
		{
			return In.Hash;
		}

		inline bool operator == (const FSetLayout& In) const
		{
			if (In.Hash != Hash)
			{
				return false;
			}

			const int32 NumBindings = LayoutBindings.Num();
			if (In.LayoutBindings.Num() != NumBindings)
			{
				return false;
			}

			if (NumBindings != 0 && FMemory::Memcmp(In.LayoutBindings.GetData(), LayoutBindings.GetData(), NumBindings * sizeof(VkDescriptorSetLayoutBinding)) != 0)
			{
				return false;
			}

			return true;
		}

		inline bool operator != (const FSetLayout& In) const
		{
			return !(*this == In);
		}
	};

	const TArray<FSetLayout>& GetLayouts() const
	{
		return SetLayouts;
	}

	void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSetStage, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const;

	template<bool bIsCompute>
	void FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo, const TArrayView<const FSamplerStateRHIParamRef>& ImmutableSamplers);

	void GenerateHash(const TArrayView<const FSamplerStateRHIParamRef>& ImmutableSamplers);

	friend uint32 GetTypeHash(const FVulkanDescriptorSetsLayoutInfo& In)
	{
		return In.Hash;
	}

	inline bool operator == (const FVulkanDescriptorSetsLayoutInfo& In) const
	{
		if (In.Hash != Hash)
		{
			return false;
		}

		if (In.SetLayouts.Num() != SetLayouts.Num())
		{
			return false;
		}

		if (In.TypesUsageID != TypesUsageID)
		{
			return false;
		}

		for (int32 Index = 0; Index < In.SetLayouts.Num(); ++Index)
		{
			if (In.SetLayouts[Index] != SetLayouts[Index])
			{
				return false;
			}
		}

		if (RemappingInfo != In.RemappingInfo)
		{
			return false;
		}

		return true;
	}

	void CopyFrom(const FVulkanDescriptorSetsLayoutInfo& Info)
	{
		FMemory::Memcpy(LayoutTypes, Info.LayoutTypes, sizeof(LayoutTypes));
		Hash = Info.Hash;
		TypesUsageID = Info.TypesUsageID;
		SetLayouts = Info.SetLayouts;
		RemappingInfo = Info.RemappingInfo;
	}

	inline const uint32* GetLayoutTypes() const
	{
		return LayoutTypes;
	}

	inline uint32 GetTypesUsageID() const
	{
		return TypesUsageID;
	}

	inline bool HasInputAttachments() const
	{
		return LayoutTypes[VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT] > 0;
	}

protected:
	uint32 LayoutTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	TArray<FSetLayout> SetLayouts;

	uint32 Hash = 0;

	uint32 TypesUsageID = ~0;

	void CompileTypesUsageID();

	void AddDescriptor(int32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor);

	FDescriptorSetRemappingInfo	RemappingInfo;

	friend class FVulkanPipelineStateCacheManager;
	friend class FVulkanCommonPipelineDescriptorState;
	friend class FVulkanGfxPipelineDescriptorInfo;
	friend class FVulkanComputePipelineDescriptorInfo;
	friend class FVulkanLayout;
};

struct FVulkanDescriptorSetLayoutEntry
{
	VkDescriptorSetLayout Handle = 0;
	uint32 HandleId = 0;
};

using FVulkanDescriptorSetLayoutMap = TMap<FVulkanDescriptorSetsLayoutInfo::FSetLayout, FVulkanDescriptorSetLayoutEntry>;

// The actual run-time descriptor set layouts
class FVulkanDescriptorSetsLayout : public FVulkanDescriptorSetsLayoutInfo
{
public:
	FVulkanDescriptorSetsLayout(FVulkanDevice* InDevice);
	~FVulkanDescriptorSetsLayout();

	// Can be called only once, the idea is that the Layout remains fixed.
	void Compile(FVulkanDescriptorSetLayoutMap& DSetLayoutMap);

	inline const TArray<VkDescriptorSetLayout>& GetHandles() const
	{
		return LayoutHandles;
	}

	inline const TArray<uint32>& GetHandleIds() const
	{
		return LayoutHandleIds;
	}

	inline const VkDescriptorSetAllocateInfo& GetAllocateInfo() const
	{
		return DescriptorSetAllocateInfo;
	}

	inline uint32 GetHash() const
	{
		return Hash;
	}

private:
	FVulkanDevice* Device;
	//uint32 Hash = 0;
	TArray<VkDescriptorSetLayout> LayoutHandles;
	TArray<uint32> LayoutHandleIds;
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo;
};

class FVulkanDescriptorPool
{
public:
	FVulkanDescriptorPool(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& Layout, uint32 MaxSetsAllocations);
	~FVulkanDescriptorPool();

	inline VkDescriptorPool GetHandle() const
	{
		return DescriptorPool;
	}

	inline bool CanAllocate(const FVulkanDescriptorSetsLayout& InLayout) const
	{
		return MaxDescriptorSets > NumAllocatedDescriptorSets + InLayout.GetLayouts().Num();
	}

	void TrackAddUsage(const FVulkanDescriptorSetsLayout& InLayout);
	void TrackRemoveUsage(const FVulkanDescriptorSetsLayout& InLayout);

	inline bool IsEmpty() const
	{
		return NumAllocatedDescriptorSets == 0;
	}

	void Reset();
	bool AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, VkDescriptorSet* OutSets);
	inline uint32 GetNumAllocatedDescriptorSets() const
	{
		return NumAllocatedDescriptorSets;
	}

private:
	FVulkanDevice* Device;

	uint32 MaxDescriptorSets;
	uint32 NumAllocatedDescriptorSets;
	uint32 PeakAllocatedDescriptorSets;

	// Tracks number of allocated types, to ensure that we are not exceeding our allocated limit
	const FVulkanDescriptorSetsLayout& Layout;
	VkDescriptorPool DescriptorPool;

	friend class FVulkanCommandListContext;
};


class FVulkanTypedDescriptorPoolSet
{
	typedef TList<FVulkanDescriptorPool*> FPoolList;

	FVulkanDescriptorPool* GetFreePool(bool bForceNewPool = false);
	FVulkanDescriptorPool* PushNewPool();

protected:
	friend class FVulkanDescriptorPoolSetContainer;
	friend class FVulkanCmdBuffer;

	FVulkanTypedDescriptorPoolSet(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& InLayout)
		: Device(InDevice)
		, Layout(InLayout)
		, PoolsCount(0)
	{
		PushNewPool();
	};

	~FVulkanTypedDescriptorPoolSet();

	void Reset();

public:
	bool AllocateDescriptorSets(const FVulkanDescriptorSetsLayout& Layout, VkDescriptorSet* OutSets);

private:
	FVulkanDevice* Device;
	const FVulkanDescriptorSetsLayout& Layout;
	uint32 PoolsCount;

	FPoolList* PoolListHead = nullptr;
	FPoolList* PoolListCurrent = nullptr;
};

class FVulkanDescriptorPoolSetContainer
{
public:
	FVulkanDescriptorPoolSetContainer(FVulkanDevice* InDevice)
		: Device(InDevice)
		, LastFrameUsed(GFrameNumberRenderThread)
		, bUsed(true)
	{
	}

	~FVulkanDescriptorPoolSetContainer();

	FVulkanTypedDescriptorPoolSet* AcquireTypedPoolSet(const FVulkanDescriptorSetsLayout& Layout);

	void Reset();

	inline void SetUsed(bool bInUsed)
	{
		bUsed = bInUsed;
		LastFrameUsed = bUsed ? GFrameNumberRenderThread : LastFrameUsed;
	}

	inline bool IsUnused() const
	{
		return !bUsed;
	}

	inline uint32 GetLastFrameUsed() const
	{
		return LastFrameUsed;
	}

private:
	FVulkanDevice* Device;

	TMap<uint32, FVulkanTypedDescriptorPoolSet*> TypedDescriptorPools;

	uint32 LastFrameUsed;
	bool bUsed;
};

class FVulkanDescriptorPoolsManager
{
	class FVulkanAsyncPoolSetDeletionWorker : public FNonAbandonableTask
	{
		FVulkanDescriptorPoolSetContainer* PoolSet;

	public:
		FVulkanAsyncPoolSetDeletionWorker(FVulkanDescriptorPoolSetContainer* InPoolSet)
			: PoolSet(InPoolSet)
		{};

		void DoWork()
		{
			check(PoolSet != nullptr);

			delete PoolSet;

			PoolSet = nullptr;
		}

		void SetPoolSet(FVulkanDescriptorPoolSetContainer* InPoolSet)
		{
			check(PoolSet == nullptr);
			PoolSet = InPoolSet;
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FVulkanAsyncPoolSetDeletionWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

public:
	~FVulkanDescriptorPoolsManager();

	void Init(FVulkanDevice* InDevice)
	{
		Device = InDevice;
	}

	FVulkanDescriptorPoolSetContainer& AcquirePoolSetContainer();
	void ReleasePoolSet(FVulkanDescriptorPoolSetContainer& PoolSet);
	void GC();

private:
	FVulkanDevice* Device = nullptr;
	FAsyncTask<FVulkanAsyncPoolSetDeletionWorker>* AsyncDeletionTask = nullptr;

	FCriticalSection CS;
	TArray<FVulkanDescriptorPoolSetContainer*> PoolSets;
};

union FVulkanHashableDescriptorInfo
{
	struct
	{
		uint32 Max0;
		uint32 Max1;
		uint32 LayoutId;
	} Layout;
	struct
	{
		uint32 Id;
		uint32 Offset;
		uint32 Range;
	} Buffer;
	struct
	{
		uint32 SamplerId;
		uint32 ImageViewId;
		uint32 ImageLayout;
	} Image;
	struct
	{
		uint32 Id;
		uint32 Zero1;
		uint32 Zero2;
	} BufferView;
};

// This container holds the actual VkWriteDescriptorSet structures; a Compute pipeline uses the arrays 'as-is', whereas a 
// Gfx PSO will have one big array and chunk it depending on the stage (eg Vertex, Pixel).
struct FVulkanDescriptorSetWriteContainer
{
	TArray<FVulkanHashableDescriptorInfo> HashableDescriptorInfo;
	TArray<VkDescriptorImageInfo> DescriptorImageInfo;
	TArray<VkDescriptorBufferInfo> DescriptorBufferInfo;
	TArray<VkWriteDescriptorSet> DescriptorWrites;
	TArray<uint8> BindingToDynamicOffsetMap;
};


// Smaller data structure for runtime information about descriptor sets and bindings for a pipeline
class FVulkanComputePipelineDescriptorInfo
{
public:
	FVulkanComputePipelineDescriptorInfo()
		: RemappingUBInfos(nullptr)
		, RemappingGlobalInfos(nullptr)
		, RemappingPackedUBInfos(nullptr)
		, HasDescriptorsInSetMask(0)
		, RemappingInfo(nullptr)
		, bInitialized(false)
	{
		//		FMemory::Memzero(CodeHeaders);
	}

	inline bool GetDescriptorSetAndBindingIndex(const FVulkanShaderHeader::EType Type, int32 ParameterIndex, uint8& OutDescriptorSet, uint32& OutBindingIndex) const
	{
		switch (Type)
		{
		case FVulkanShaderHeader::UniformBuffer:
			//ensure(RemappingInfo->StageInfos[0].UniformBuffers[ParameterIndex].bHasConstantData);
			ensure(RemappingUBInfos[ParameterIndex].bHasConstantData);
			//OutDescriptorSet = RemappingInfo->StageInfos[0].UniformBuffers[ParameterIndex].Remapping.NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[0].UniformBuffers[ParameterIndex].Remapping.NewBindingIndex;
			OutDescriptorSet = RemappingUBInfos[ParameterIndex].Remapping.NewDescriptorSet;
			OutBindingIndex = RemappingUBInfos[ParameterIndex].Remapping.NewBindingIndex;
			break;
		case FVulkanShaderHeader::Global:
			//OutDescriptorSet = RemappingInfo->StageInfos[0].Globals[ParameterIndex].NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[0].Globals[ParameterIndex].NewBindingIndex;
			OutDescriptorSet = RemappingGlobalInfos[ParameterIndex].NewDescriptorSet;
			OutBindingIndex = RemappingGlobalInfos[ParameterIndex].NewBindingIndex;
			break;
		default:
			check(0);
			return false;
		}
		return true;
	}

	inline const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& GetGlobalRemappingInfo() const
	{
		//OutDescriptorSet = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewDescriptorSet;
		return RemappingInfo->StageInfos[0].Globals;
	}

	inline bool IsInitialized() const
	{
		return bInitialized;
	}

	void Initialize(const FDescriptorSetRemappingInfo& InRemappingInfo);

protected:
	// Cached data from FDescriptorSetRemappingInfo
	const FDescriptorSetRemappingInfo::FUBRemappingInfo*	RemappingUBInfos;
	const FDescriptorSetRemappingInfo::FRemappingInfo*		RemappingGlobalInfos;
	const uint16*											RemappingPackedUBInfos;
	uint32													HasDescriptorsInSetMask;
	const FDescriptorSetRemappingInfo*						RemappingInfo;
	bool													bInitialized;

	friend class FVulkanComputePipelineDescriptorState;
};

// Smaller data structure for runtime information about descriptor sets and bindings for a pipeline
class FVulkanGfxPipelineDescriptorInfo
{
public:
	FVulkanGfxPipelineDescriptorInfo()
		: HasDescriptorsInSetMask(0)
		, RemappingInfo(nullptr)
		, bInitialized(false)
	{
		FMemory::Memzero(RemappingUBInfos);
		FMemory::Memzero(RemappingGlobalInfos);
		FMemory::Memzero(RemappingPackedUBInfos);
	}

	inline bool GetDescriptorSetAndBindingIndex(const FVulkanShaderHeader::EType Type, const ShaderStage::EStage Stage, int32 ParameterIndex, uint8& OutDescriptorSet, uint32& OutBindingIndex) const
	{
		switch (Type)
		{
		case FVulkanShaderHeader::UniformBuffer:
			ensure(RemappingUBInfos[Stage][ParameterIndex].bHasConstantData);
			//ensure(RemappingInfo->StageInfos[Stage].UniformBuffers[ParameterIndex].bHasConstantData);
			//OutDescriptorSet = RemappingInfo->StageInfos[Stage].UniformBuffers[ParameterIndex].Remapping.NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[Stage].UniformBuffers[ParameterIndex].Remapping.NewBindingIndex;
			OutDescriptorSet = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewDescriptorSet;
			OutBindingIndex = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewBindingIndex;
			break;
		case FVulkanShaderHeader::Global:
			//OutDescriptorSet = RemappingInfo->StageInfos[Stage].Globals[ParameterIndex].NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[Stage].Globals[ParameterIndex].NewBindingIndex;
			OutDescriptorSet = RemappingGlobalInfos[Stage][ParameterIndex].NewDescriptorSet;
			OutBindingIndex = RemappingGlobalInfos[Stage][ParameterIndex].NewBindingIndex;
			break;
		default:
			check(0);
			return false;
		}
		return true;
	}

	inline const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& GetGlobalRemappingInfo(ShaderStage::EStage Stage) const
	{
		//OutDescriptorSet = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewDescriptorSet;
		return RemappingInfo->StageInfos[Stage].Globals;
	}

	inline bool IsInitialized() const
	{
		return bInitialized;
	}

	inline const TArray<FInputAttachmentData>& GetInputAttachmentData() const
	{
		return RemappingInfo->InputAttachmentData;
	}

	void Initialize(const FDescriptorSetRemappingInfo& InRemappingInfo);

protected:
	// Cached data from FDescriptorSetRemappingInfo
	const FDescriptorSetRemappingInfo::FUBRemappingInfo*	RemappingUBInfos[ShaderStage::NumStages];
	const FDescriptorSetRemappingInfo::FRemappingInfo*		RemappingGlobalInfos[ShaderStage::NumStages];
	const uint16*											RemappingPackedUBInfos[ShaderStage::NumStages];
	uint32													HasDescriptorsInSetMask;

	const FDescriptorSetRemappingInfo*						RemappingInfo;
	bool													bInitialized;

	friend class FVulkanGraphicsPipelineDescriptorState;
};


// Layout for a Pipeline, also includes DescriptorSets layout
class FVulkanLayout : public VulkanRHI::FDeviceChild
{
public:
	FVulkanLayout(FVulkanDevice* InDevice);
	virtual ~FVulkanLayout();

	virtual bool IsGfxLayout() const = 0;

	inline const FVulkanDescriptorSetsLayout& GetDescriptorSetsLayout() const
	{
		return DescriptorSetLayout;
	}

	inline VkPipelineLayout GetPipelineLayout() const
	{
		return PipelineLayout;
	}

	inline bool HasDescriptors() const
	{
		return DescriptorSetLayout.GetLayouts().Num() > 0;
	}

	inline uint32 GetDescriptorSetLayoutHash() const
	{
		return DescriptorSetLayout.GetHash();
	}

	VkShaderModule CreatePatchedPatchSpirvModule(TArray<uint32>& Spirv, EShaderFrequency Frequency, const FVulkanShaderHeader& CodeHeader, VkShaderStageFlagBits InStageFlag) const;

protected:
	FVulkanDescriptorSetsLayout	DescriptorSetLayout;
	VkPipelineLayout			PipelineLayout;

	template <bool bIsCompute>
	inline void FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo)
	{
		// Setting descriptor is only allowed prior to compiling the layout
		check(DescriptorSetLayout.GetHandles().Num() == 0);

		DescriptorSetLayout.FinalizeBindings<bIsCompute>(UBGatherInfo);
	}

	inline void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSet, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const
	{
		// Setting descriptor is only allowed prior to compiling the layout
		check(DescriptorSetLayout.GetHandles().Num() == 0);

		DescriptorSetLayout.ProcessBindingsForStage(StageFlags, DescSet, CodeHeader, OutUBGatherInfo);
	}

	void Compile(FVulkanDescriptorSetLayoutMap& DSetLayoutMap);

	friend class FVulkanComputePipeline;
	friend class FVulkanGfxPipeline;
	friend class FVulkanPipelineStateCacheManager;
};

class FVulkanGfxLayout : public FVulkanLayout
{
public:
	FVulkanGfxLayout(FVulkanDevice* InDevice)
		: FVulkanLayout(InDevice)
	{
	}

	virtual bool IsGfxLayout() const final override
	{
		return true;
	}

	inline const FVulkanGfxPipelineDescriptorInfo& GetGfxPipelineDescriptorInfo() const
	{
		return GfxPipelineDescriptorInfo;
	}

protected:
	FVulkanGfxPipelineDescriptorInfo		GfxPipelineDescriptorInfo;
	friend class FVulkanPipelineStateCacheManager;
};

class FVulkanComputeLayout : public FVulkanLayout
{
public:
	FVulkanComputeLayout(FVulkanDevice* InDevice)
		: FVulkanLayout(InDevice)
	{
	}

	virtual bool IsGfxLayout() const final override
	{
		return false;
	}

	inline const FVulkanComputePipelineDescriptorInfo& GetComputePipelineDescriptorInfo() const
	{
		return ComputePipelineDescriptorInfo;
	}

protected:
	FVulkanComputePipelineDescriptorInfo		ComputePipelineDescriptorInfo;
	friend class FVulkanPipelineStateCacheManager;
};

// This class encapsulates updating VkWriteDescriptorSet structures (but doesn't own them), and their flags for dirty ranges; it is intended
// to be used to access a sub-region of a long array of VkWriteDescriptorSet (ie FVulkanDescriptorSetWriteContainer)
class FVulkanDescriptorSetWriter
{
public:
	FVulkanDescriptorSetWriter()
		: WriteDescriptors(nullptr)
		, BindingToDynamicOffsetMap(nullptr)
		, DynamicOffsets(nullptr)
		, NumWrites(0)
		, HashableDescriptorInfos(nullptr)
		, bIsKeyDirty(true)
	{
	}

	const FVulkanDSetKey& GetKey() const
	{
		check(UseVulkanDescriptorCache());
		if (bIsKeyDirty)
		{
			Key.GenerateFromData(HashableDescriptorInfos, sizeof(FVulkanHashableDescriptorInfo) * (NumWrites + 1)); // Add 1 for the Layout
			bIsKeyDirty = false;
		}
		return Key;
	}

	const VkWriteDescriptorSet* GetWriteDescriptors() const
	{
		return WriteDescriptors;
	}

	const uint32 GetNumWrites() const
	{
		return NumWrites;
	}

	bool WriteUniformBuffer(uint32 DescriptorIndex, const FBufferAllocation& BufferAllocation, VkDeviceSize Offset, VkDeviceSize Range)
	{
		return WriteBuffer<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(DescriptorIndex, BufferAllocation, Offset, Range);
	}

	bool WriteDynamicUniformBuffer(uint32 DescriptorIndex, const FBufferAllocation& BufferAllocation, VkDeviceSize Offset, VkDeviceSize Range, uint32 DynamicOffset)
	{
		return WriteBuffer<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>(DescriptorIndex, BufferAllocation, Offset, Range, DynamicOffset);
	}

	bool WriteSampler(uint32 DescriptorIndex, const FVulkanSamplerState& Sampler)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);
		
		bool bChanged = false;
		if (UseVulkanDescriptorCache())
		{
			
			FVulkanHashableDescriptorInfo& HeshableInfo = HashableDescriptorInfos[DescriptorIndex];
			check(Sampler.SamplerId > 0);
			if (HeshableInfo.Image.SamplerId != Sampler.SamplerId)
			{
				HeshableInfo.Image.SamplerId = Sampler.SamplerId;
				ImageInfo->sampler = Sampler.Sampler;
				bChanged = true;
			}
			bIsKeyDirty |= bChanged;
		}
		else
		{
			bChanged = CopyAndReturnNotEqual(ImageInfo->sampler, Sampler.Sampler);
		}

		return bChanged;
	}

	bool WriteImage(uint32 DescriptorIndex, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		return WriteTextureView<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE>(DescriptorIndex, TextureView, Layout);
	}

	bool WriteInputAttachment(uint32 DescriptorIndex, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		return WriteTextureView<VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT>(DescriptorIndex, TextureView, Layout);
	}

	bool WriteStorageImage(uint32 DescriptorIndex, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		return WriteTextureView<VK_DESCRIPTOR_TYPE_STORAGE_IMAGE>(DescriptorIndex, TextureView, Layout);
	}

	bool WriteStorageTexelBuffer(uint32 DescriptorIndex, const FVulkanBufferView* View)
	{
		return WriteBufferView<VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER>(DescriptorIndex, View);
	}

	bool WriteStorageBuffer(uint32 DescriptorIndex, const FBufferAllocation& BufferAllocation, VkDeviceSize Offset, VkDeviceSize Range)
	{
		return WriteBuffer<VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>(DescriptorIndex, BufferAllocation, Offset, Range);
	}

	bool WriteUniformTexelBuffer(uint32 DescriptorIndex, const FVulkanBufferView* View)
	{
		return WriteBufferView<VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER>(DescriptorIndex, View);
	}

	void ClearBufferView(uint32 DescriptorIndex)
	{
		BufferViewReferences[DescriptorIndex] = nullptr;
	}

	void SetDescriptorSet(VkDescriptorSet DescriptorSet)
	{
		for (uint32 Index = 0; Index < NumWrites; ++Index)
		{
			WriteDescriptors[Index].dstSet = DescriptorSet;
		}
	}

protected:
	template <VkDescriptorType DescriptorType>
	bool WriteBuffer(uint32 DescriptorIndex, const FBufferAllocation& BufferAllocation, VkDeviceSize Offset, VkDeviceSize Range, uint32 DynamicOffset = 0)
	{
		check(DescriptorIndex < NumWrites);
		if (DescriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		{
			check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
		}
		else
		{
			check(WriteDescriptors[DescriptorIndex].descriptorType == DescriptorType);
		}
		VkDescriptorBufferInfo* BufferInfo = const_cast<VkDescriptorBufferInfo*>(WriteDescriptors[DescriptorIndex].pBufferInfo);
		check(BufferInfo);
		
		bool bChanged = false;
		if (UseVulkanDescriptorCache())
		{
			FVulkanHashableDescriptorInfo& HeshableInfo = HashableDescriptorInfos[DescriptorIndex];
			check(BufferAllocation.GetHandleId() > 0);
			if (HeshableInfo.Buffer.Id != BufferAllocation.GetHandleId())
			{
				HeshableInfo.Buffer.Id = BufferAllocation.GetHandleId();
				BufferInfo->buffer = BufferAllocation.GetHandle();
				bChanged = true;
			}
			if (HeshableInfo.Buffer.Offset != static_cast<uint32>(Offset))
			{
				HeshableInfo.Buffer.Offset = static_cast<uint32>(Offset);
				BufferInfo->offset = Offset;
				bChanged = true;
			}
			if (HeshableInfo.Buffer.Range != static_cast<uint32>(Range))
			{
				HeshableInfo.Buffer.Range = static_cast<uint32>(Range);
				BufferInfo->range = Range;
				bChanged = true;
			}
			bIsKeyDirty |= bChanged;
		}
		else
		{
			bChanged = CopyAndReturnNotEqual(BufferInfo->buffer, BufferAllocation.GetHandle());
			bChanged |= CopyAndReturnNotEqual(BufferInfo->offset, Offset);
			bChanged |= CopyAndReturnNotEqual(BufferInfo->range, Range);
		}

		if (DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		{
			const uint8 DynamicOffsetIndex = BindingToDynamicOffsetMap[DescriptorIndex];
			DynamicOffsets[DynamicOffsetIndex] = DynamicOffset;
		}
		return bChanged;
	}

	template <VkDescriptorType DescriptorType>
	bool WriteTextureView(uint32 DescriptorIndex, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		check(DescriptorIndex < NumWrites);
		if (DescriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
		{
			check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		}
		else
		{
			check(WriteDescriptors[DescriptorIndex].descriptorType == DescriptorType);
		}
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);

		bool bChanged = false;
		if (UseVulkanDescriptorCache())
		{
			FVulkanHashableDescriptorInfo& HeshableInfo = HashableDescriptorInfos[DescriptorIndex];
			check(TextureView.ViewId > 0);
			if (HeshableInfo.Image.ImageViewId != TextureView.ViewId)
			{
				HeshableInfo.Image.ImageViewId = TextureView.ViewId;
				ImageInfo->imageView = TextureView.View;
				bChanged = true;
			}
			if (HeshableInfo.Image.ImageLayout != static_cast<uint32>(Layout))
			{
				HeshableInfo.Image.ImageLayout = static_cast<uint32>(Layout);
				ImageInfo->imageLayout = Layout;
				bChanged = true;
			}
			bIsKeyDirty |= bChanged;
		}
		else
		{
			bChanged = CopyAndReturnNotEqual(ImageInfo->imageView, TextureView.View);
			bChanged |= CopyAndReturnNotEqual(ImageInfo->imageLayout, Layout);
		}

		return bChanged;
	}

	template <VkDescriptorType DescriptorType>
	bool WriteBufferView(uint32 DescriptorIndex, const FVulkanBufferView* View)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == DescriptorType);
		WriteDescriptors[DescriptorIndex].pTexelBufferView = &View->View;
		BufferViewReferences[DescriptorIndex] = View;
		
		if (UseVulkanDescriptorCache())
		{
			bool bChanged = false;
			FVulkanHashableDescriptorInfo& HeshableInfo = HashableDescriptorInfos[DescriptorIndex];
			check(View->ViewId > 0);
			if (HeshableInfo.BufferView.Id != View->ViewId)
			{
				HeshableInfo.BufferView.Id = View->ViewId;
				bChanged = true;
			}
			bIsKeyDirty |= bChanged;
			return bChanged;
		}
		else
		{
			return true;
		}
	}

protected:
	// A view into someone else's descriptors
	VkWriteDescriptorSet* WriteDescriptors;

	// A view into the mapping from binding index to dynamic uniform buffer offsets
	uint8* BindingToDynamicOffsetMap;

	// A view into someone else's dynamic uniform buffer offsets
	uint32* DynamicOffsets;

	uint32 NumWrites;
	TArray<TRefCountPtr<const FVulkanBufferView>> BufferViewReferences;

	FVulkanHashableDescriptorInfo* HashableDescriptorInfos;
	mutable FVulkanDSetKey Key;
	mutable bool bIsKeyDirty;

	uint32 SetupDescriptorWrites(const TArray<VkDescriptorType>& Types,
		FVulkanHashableDescriptorInfo* InHashableDescriptorInfos,
		VkWriteDescriptorSet* InWriteDescriptors, VkDescriptorImageInfo* InImageInfo,
		VkDescriptorBufferInfo* InBufferInfo, uint8* InBindingToDynamicOffsetMap,
		const FVulkanSamplerState& DefaultSampler, const FVulkanTextureView& DefaultImageView);

	friend class FVulkanCommonPipelineDescriptorState;
};

class FVulkanGenericDescriptorPool : FNoncopyable
{
public:
	FVulkanGenericDescriptorPool(FVulkanDevice* InDevice, uint32 InMaxDescriptorSets);
	~FVulkanGenericDescriptorPool();

	FVulkanDevice* GetDevice() const
	{
		return Device;
	}

	uint32 GetMaxDescriptorSets() const
	{
		return MaxDescriptorSets;
	}

	void Reset();
	bool AllocateDescriptorSet(VkDescriptorSetLayout Layout, VkDescriptorSet& OutSet);

private:
	FVulkanDevice* const Device;
	const uint32 MaxDescriptorSets;
	VkDescriptorPool DescriptorPool;
};

class FVulkanDescriptorSetCache : FNoncopyable
{
public:
	FVulkanDescriptorSetCache(FVulkanDevice* InDevice);
	~FVulkanDescriptorSetCache();

	void GetDescriptorSets(const FVulkanDSetsKey& DSetsKey, const FVulkanDescriptorSetsLayout& SetsLayout,
		TArray<FVulkanDescriptorSetWriter>& DSWriters, VkDescriptorSet* OutSets);
	void GC();

private:
	void UpdateAllocRatio();
	void AddCachedPool();

private:
	struct FSetsEntry
	{
		TStaticArray<VkDescriptorSet, ShaderStage::MaxNumSets> Sets;
		int32 NumSets;
	};

	class FCachedPool : FNoncopyable
	{
	public:
		FCachedPool(FVulkanDevice* InDevice, uint32 InMaxDescriptorSets)
			: SetCapacity(FMath::RoundToZero(InMaxDescriptorSets * MaxAllocRatio))
			, Pool(InDevice, InMaxDescriptorSets)
			, RecentFrame(0)
		{
		}

		uint32 GetMaxDescriptorSets() const
		{
			return Pool.GetMaxDescriptorSets();
		}

		void Reset()
		{
			Pool.Reset();
			SetsCache.Reset();
			SetCache.Reset();
		}

		bool CanGC() const;
		float CalcAllocRatio() const;

		bool FindDescriptorSets(const FVulkanDSetsKey& DSetsKey, VkDescriptorSet* OutSets);
		bool CreateDescriptorSets(const FVulkanDSetsKey& DSetsKey, const FVulkanDescriptorSetsLayout& SetsLayout,
			TArray<FVulkanDescriptorSetWriter>& DSWriters, VkDescriptorSet* OutSets);

	private:
		static const float MinAllocRatio;
		static const float MaxAllocRatio;

	private:
		const uint32 SetCapacity;
		FVulkanGenericDescriptorPool Pool;
		TMap<FVulkanDSetsKey, FSetsEntry> SetsCache;
		TMap<FVulkanDSetKey, VkDescriptorSet> SetCache;
		uint32 RecentFrame;
	};

private:
	FVulkanDevice* const Device;
	TArray<TUniquePtr<FCachedPool>> CachedPools;
	TUniquePtr<FCachedPool> FreePool;
	float PoolAllocRatio;
};


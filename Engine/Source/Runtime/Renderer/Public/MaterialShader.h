// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "MaterialShaderType.h"
#include "AtmosphereTextureParameters.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcessParameters.h"
#include "GlobalDistanceFieldParameters.h"

template<typename ParameterType> 
struct TUniformParameter
{
	int32 Index;
	ParameterType ShaderParameter;
	friend FArchive& operator<<(FArchive& Ar,TUniformParameter<ParameterType>& P)
	{
		return Ar << P.Index << P.ShaderParameter;
	}
};


/**
 * Debug information related to uniform expression sets.
 */
class FDebugUniformExpressionSet
{
public:
	/** The number of each type of expression contained in the set. */
	int32 NumVectorExpressions;
	int32 NumScalarExpressions;
	int32 Num2DTextureExpressions;
	int32 NumCubeTextureExpressions;
	int32 NumPerFrameScalarExpressions;
	int32 NumPerFrameVectorExpressions;
	FRHIUniformBufferLayout* DebugLayout;

	FDebugUniformExpressionSet()
		: NumVectorExpressions(0)
		, NumScalarExpressions(0)
		, Num2DTextureExpressions(0)
		, NumCubeTextureExpressions(0)
		, NumPerFrameScalarExpressions(0)
		, NumPerFrameVectorExpressions(0)
		, DebugLayout(nullptr)
	{
	}

	explicit FDebugUniformExpressionSet(const FUniformExpressionSet& InUniformExpressionSet)
	{
		InitFromExpressionSet(InUniformExpressionSet);
	}

	~FDebugUniformExpressionSet()
	{
		delete DebugLayout;
	}

	/** Initialize from a uniform expression set. */
	void InitFromExpressionSet(const FUniformExpressionSet& InUniformExpressionSet)
	{
		NumVectorExpressions = InUniformExpressionSet.UniformVectorExpressions.Num();
		NumScalarExpressions = InUniformExpressionSet.UniformScalarExpressions.Num();
		Num2DTextureExpressions = InUniformExpressionSet.Uniform2DTextureExpressions.Num();
		NumCubeTextureExpressions = InUniformExpressionSet.UniformCubeTextureExpressions.Num();
		NumPerFrameScalarExpressions = InUniformExpressionSet.PerFrameUniformScalarExpressions.Num();
		NumPerFrameVectorExpressions = InUniformExpressionSet.PerFrameUniformVectorExpressions.Num();
		DebugLayout = InUniformExpressionSet.CreateDebugLayout();
	}

	/** Returns true if the number of uniform expressions matches those with which the debug set was initialized. */
	bool Matches(const FUniformExpressionSet& InUniformExpressionSet) const
	{
		return NumVectorExpressions == InUniformExpressionSet.UniformVectorExpressions.Num()
			&& NumScalarExpressions == InUniformExpressionSet.UniformScalarExpressions.Num()
			&& Num2DTextureExpressions == InUniformExpressionSet.Uniform2DTextureExpressions.Num()
			&& NumCubeTextureExpressions == InUniformExpressionSet.UniformCubeTextureExpressions.Num()
			&& NumPerFrameScalarExpressions == InUniformExpressionSet.PerFrameUniformScalarExpressions.Num()
			&& NumPerFrameVectorExpressions == InUniformExpressionSet.PerFrameUniformVectorExpressions.Num();
	}
};

/** Serialization for debug uniform expression sets. */
inline FArchive& operator<<(FArchive& Ar, FDebugUniformExpressionSet& DebugExpressionSet)
{
	Ar << DebugExpressionSet.NumVectorExpressions;
	Ar << DebugExpressionSet.NumScalarExpressions;
	Ar << DebugExpressionSet.NumPerFrameScalarExpressions;
	Ar << DebugExpressionSet.NumPerFrameVectorExpressions;
	Ar << DebugExpressionSet.Num2DTextureExpressions;
	Ar << DebugExpressionSet.NumCubeTextureExpressions;
	return Ar;
}


/** Base class of all shaders that need material parameters. */
class RENDERER_API FMaterialShader : public FShader
{
public:
	FMaterialShader() {}

	FMaterialShader(const FMaterialShaderType::CompiledShaderInitializerType& Initializer);

	typedef void (*ModifyCompilationEnvironmentType)(EShaderPlatform, const FMaterial*, FShaderCompilerEnvironment&);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	FUniformBufferRHIParamRef GetParameterCollectionBuffer(const FGuid& Id, const FSceneInterface* SceneInterface) const;

	template<typename ShaderRHIParamRef>
	void SetParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI,const FSceneView& View)
	{
		CheckShaderIsValid();
		const auto& ViewUBParameter = GetUniformBufferParameter<FViewUniformShaderParameters>();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ViewUBParameter, View.UniformBuffer);

		const auto& BuiltinSamplersUBParameter = GetUniformBufferParameter<FBuiltinSamplersParameters>();
		SetUniformBufferParameter(RHICmdList, ShaderRHI, BuiltinSamplersUBParameter, GBuiltinSamplersUniformBuffer.GetUniformBufferRHI());
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Sets pixel parameters that are material specific but not FMeshBatch specific. */
	template< typename ShaderRHIParamRef >
	void SetParameters(
		FRHICommandList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& Material,
		const FSceneView& View, 
		bool bDeferredPass, 
		ESceneRenderTargetsMode::Type TextureMode );

	FTextureRHIRef& GetEyeAdaptation(FRHICommandList& RHICmdList, const FSceneView& View);

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override;
	virtual uint32 GetAllocatedSize() const override;

private:

	FShaderUniformBufferParameter MaterialUniformBuffer;
	TArray<FShaderUniformBufferParameter> ParameterCollectionUniformBuffers;
	TArray<FShaderParameter> PerFrameScalarExpressions;
	TArray<FShaderParameter> PerFrameVectorExpressions;
	TArray<FShaderParameter> PerFramePrevScalarExpressions;
	TArray<FShaderParameter> PerFramePrevVectorExpressions;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter LightAttenuation;
	FShaderResourceParameter LightAttenuationSampler;

	// For materials using atmospheric fog color 
	FAtmosphereShaderTextureParameters AtmosphericFogTextureParameters;

	//Use of the eye adaptation texture here is experimental and potentially dangerous as it can introduce a feedback loop. May be removed.
	FShaderResourceParameter EyeAdaptation;

	/** The PerlinNoiseGradientTexture parameter for materials that use GradientNoise */
	FShaderResourceParameter PerlinNoiseGradientTexture;
	FShaderResourceParameter PerlinNoiseGradientTextureSampler;
	/** The PerlinNoise3DTexture parameter for materials that use GradientNoise */
	FShaderResourceParameter PerlinNoise3DTexture;
	FShaderResourceParameter PerlinNoise3DTextureSampler;

	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;

	FDebugUniformExpressionSet DebugUniformExpressionSet;
	FString DebugDescription;

	/** If true, cached uniform expressions are allowed. */
	static int32 bAllowCachedUniformExpressions;
	/** Console variable ref to toggle cached uniform expressions. */
	static FAutoConsoleVariableRef CVarAllowCachedUniformExpressions;
};

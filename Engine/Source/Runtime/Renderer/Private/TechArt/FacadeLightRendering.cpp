/*=============================================================================
	FacadeLightRendering.cpp: Facade Light rendering implementation.
	//JOHNGAMECODER_BEGIN //Facade-Light
=============================================================================*/

#include "FacadeLightRendering.h"
#include "LightRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "Engine/SubsurfaceProfile.h"
#include "ShowFlags.h"

/*------------------------------------------------------------------------------------------------------
	Configuratio
------------------------------------------------------------------------------------------------------*/

static int32 GFacadeLightEnable = 1;
static FAutoConsoleVariableRef CVarFacadeLightEnable(
	TEXT("n.FacadeLight.Enable"),
	GFacadeLightEnable,
	TEXT("1 to use facade-light."));

static FVector GetLightVector(const FSceneView* View)
{
	//static FVector ForwardVector(0.0f, 0.0f, -1.0f);
	FVector ForwardVector;
	{
		float Theta = View->FinalPostProcessSettings.FacadeLighting_LightDirectionX * PI - HALF_PI;
		float Omega = View->FinalPostProcessSettings.FacadeLighting_LightDirectionY * PI;
		ForwardVector.Y = FMath::Cos(Omega);
		ForwardVector.X = FMath::Cos(Theta) * ForwardVector.Y;
		ForwardVector.Z = FMath::Sin(Theta) * ForwardVector.Y;
		ForwardVector.Y = FMath::Sin(Omega);
	}

	FVector LightDirection = View->ViewMatrices.GetInvViewMatrix().TransformVector(ForwardVector);
	return LightDirection;
}

bool FFacadeLight::IsEnable()
{
	return GFacadeLightEnable != 0;
}

static void GetLightParameters(FLightShaderParameters& LightParameters, const FLinearColor& Color, const FVector Direction, float SpecularScale)
{
	LightParameters.Position = FVector4(0, 0, 0, 0);
	LightParameters.InvRadius = 0;

	LightParameters.Color = FVector4(Color.R, Color.G, Color.B, 0);
	LightParameters.FalloffExponent = 0;

	LightParameters.Direction = Direction;

	LightParameters.SpotAngles = FVector2D(0, 0);
	LightParameters.SpecularScale = SpecularScale;
	LightParameters.SourceRadius = 0; // FMath::Sin(0.5f * FMath::DegreesToRadians(LightSOurceAngle));
	LightParameters.SoftSourceRadius = 0; // FMath::Sin(0.5f * FMath::DegreesToRadians(LightSoftSourceRadius));
	LightParameters.SourceLength = 0;
	LightParameters.SourceTexture = (FRHITexture*)GWhiteTexture;
}


static void SetupLightUniforms(FDeferredLightUniformStruct& DeferredLightUniformsValue, const FSceneView* View, FLinearColor Color, float SpecularScale, float ContactShadowLength, float IsContactShadowInWS, float IndirectLightingScale)
{
	FVector LightDirection = GetLightVector(View);

	FLightShaderParameters LightParameters;
	GetLightParameters(LightParameters, Color, LightDirection, SpecularScale);

	DeferredLightUniformsValue.LightParameters = LightParameters;

	const FVector2D FadeParams = FVector2D(0, 0); // LightSceneInfo->Proxy->GetDirectionalLightDistanceFaceParameters(View.GetFeatureLevel(), LightSceneInfo->IsPrecomputedLightingValid(), View,NaxSgadiwCascades);

	// use MAD for efficiency in the sader
	DeferredLightUniformsValue.DistanceFadeMAD = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);

	int32 ShadowMapCHannel = 0; // LightSceneInfo->Proxy->GetShadowMapChannel();

	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ALlowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

	if (!bAllowStaticLighting)
	{
		ShadowMapCHannel = INDEX_NONE;
	}

	DeferredLightUniformsValue.ShadowMapChannelMask = FVector4(
		ShadowMapCHannel == 0 ? 1 : 0,
		ShadowMapCHannel == 1 ? 1 : 0,
		ShadowMapCHannel == 2 ? 1 : 0,
		ShadowMapCHannel == 3 ? 1 : 0
	);

	const bool bDynamicShadows = View->Family->EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	const bool bHasLightFunction = false; // LightSceneInfo->Proxy->GetLightFunctionMaterial() != nullptr;
	DeferredLightUniformsValue.ShadowedBits = 0; // LightSceneInfo->Proxy->CastStaticShadow() || bHasLightFunction ? 1 : 0;
	DeferredLightUniformsValue.ShadowedBits |= 0; // LightSceneInfo->Proxy->CastStaticShadow() || View->Family->EngineSHowFlags.DynamicShadows ? 3 : 0;

	DeferredLightUniformsValue.VolumetricScatteringIntensity = 0; // LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();

	static auto* ContacctShadowsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ContactShadows"));
	DeferredLightUniformsValue.ContactShadowLength = 0;

	if (ContacctShadowsCVar && ContacctShadowsCVar->GetValueOnRenderThread() != 0 && View->Family->EngineShowFlags.ContactShadows)
	{
		DeferredLightUniformsValue.ContactShadowLength = ContactShadowLength; // LightSceneInfo->Proxy->GetContactShadowLength();
		// Sign indicates if contact shadow length is in world space or screen space.
		// Multiply byh 2 for screen space in order to preserved old values after introudcing multiply by View.ClipToView[1][1] in shader.
		DeferredLightUniformsValue.ContactShadowLength *= IsContactShadowInWS ? -1.0f : 2.0f; // LightSceneInfo->Proxy->IsContactShadowLengthInWS() ? -1.0f : 2.0f;
	}

	// When renderign reflection captures, the direct lighting of the light is actually the indirect specular from the main view
	if (View->bIsReflectionCapture)
	{
		DeferredLightUniformsValue.LightParameters.Color *= IndirectLightingScale; //LightSceneInfo->Proxy->GetIndirectLightingScale();
	}
	DeferredLightUniformsValue.LightingChannelMask = 0; //LightSceneInfo->Proxy->GetLightingChannelMask();
}

/*------------------------------------------------------------------------------------------------------
	Shader
------------------------------------------------------------------------------------------------------*/

// A vertex shader for rendering the camera light in a deferred pass.
class FDeferredFacadeLightVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDeferredFacadeLightVS, Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FDeferredFacadeLightVS() {}
	FDeferredFacadeLightVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(), View.ViewUniformBuffer);
	}
};

IMPLEMENT_SHADER_TYPE(, FDeferredFacadeLightVS, TEXT("/Engine/TechArt/DeferredFacadeLightVertexShader.usf"), TEXT("DirectionalVertexMain"), SF_Vertex);



// A pixel shader for rendering the camera light in a deferred pass.
// copied from LightRendering.cpp
class FDeferredFacadeLightPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDeferredFacadeLightPS)

	class FVisualizeCullingDim : SHADER_PERMUTATION_BOOL("VISUALIZE_LIGHT_CULLING");
	using FPermutationDomain = TShaderPermutationDomain<FVisualizeCullingDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	FDeferredFacadeLightPS() {}
	FDeferredFacadeLightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		LightAttenuationTexture.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTexture"));
		LightAttenuationTextureSampler.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTextureSampler"));
		TransmissionProfilesLinearSampler.Bind(Initializer.ParameterMap, TEXT("TransmissionProfilesLinearSampler"));
	}

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter LightAttenuationTexture;
	FShaderResourceParameter LightAttenuationTextureSampler;
	FShaderResourceParameter TransmissionProfilesTexture;
	FShaderResourceParameter TransmissionProfilesLinearSampler;

public:
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		SetParametersBase(RHICmdList, ShaderRHI, View);
		SetDeferredLightParameters(RHICmdList, ShaderRHI, View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << LightAttenuationTexture;
		Ar << LightAttenuationTextureSampler;
		Ar << TransmissionProfilesTexture;
		Ar << TransmissionProfilesLinearSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	void SetParametersBase(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI, const FSceneView& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);

		FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);

		if (LightAttenuationTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				LightAttenuationTexture,
				LightAttenuationTextureSampler,
				TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI(),
				GWhiteTexture->TextureRHI
			);
		}

		if (TransmissionProfilesTexture.IsBound())
		{
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
			const IPooledRenderTarget* PooledRT = GetSubsufaceProfileTexture_RT((FRHICommandListImmediate&)RHICmdList);

			if (!PooledRT)
			{
				// no subsurface profile was used yet
				PooledRT = GSystemTextures.BlackDummy;
			}

			const FSceneRenderTargetItem& Item = PooledRT->GetRenderTargetItem();

			SetTextureParameter(RHICmdList,
				ShaderRHI,
				TransmissionProfilesTexture,
				TransmissionProfilesLinearSampler,
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
				Item.ShaderResourceTexture);
		}
	}


	void SetDeferredLightParameters(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI, const FSceneView& View)
	{
		float SpecularScale = 1;
		float ContactShadowLength = 1;
		bool IsContactShadowInWS = true;
		float IndirectLightingScale = 1;
		FDeferredLightUniformStruct DeferredLightUniformsValue;

		SetupLightUniforms(DeferredLightUniformsValue, &View, View.FinalPostProcessSettings.FacadeLighting_LightColor * View.FinalPostProcessSettings.FacadeLighting_Intensity, SpecularScale, ContactShadowLength, IsContactShadowInWS, IndirectLightingScale);

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FDeferredLightUniformStruct>(), DeferredLightUniformsValue);
	}
};

IMPLEMENT_SHADER_TYPE(, FDeferredFacadeLightPS, TEXT("/Engine/TechArt/DeferredFacadeLightPixelShader.usf"), TEXT("DeferredLightPixelMain"), SF_Pixel);


/*------------------------------------------------------------------------------------------------------
	Facade Light
------------------------------------------------------------------------------------------------------*/

void FFacadeLight::RenderLight(FRHICommandList& RHICmdList, const TArray<FViewInfo>& Views, bool bIssueDrawEvent)
{
	// Ensure the light is valid for this view
	if (!IsEnable())
		return;

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// Use additive blending for color
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	for (const FViewInfo& View : Views)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		GraphicsPSOInit.bDepthBounds = false;
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		// shader
		TShaderMapRef<FDeferredFacadeLightVS> VertexShader(View.ShaderMap);

		FDeferredFacadeLightPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDeferredFacadeLightPS::FVisualizeCullingDim>(View.Family->EngineShowFlags.VisualizeLightCulling);
		TShaderMapRef<FDeferredFacadeLightPS> PixelShader(View.ShaderMap, PermutationVector);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		VertexShader->SetParameters(RHICmdList, View);
		PixelShader->SetParameters(RHICmdList, View);

		// Apply the directional light as a full screen quad
		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Size(),
			FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
}

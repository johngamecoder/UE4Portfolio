/*=============================================================================
	FacadeLightRendering.h: Facade Light rendering declarations.
	//JOHNGAMECODER_BEGIN //Facade-Light
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"

/*------------------------------------------------------------------------------------------------------
	Facade Light
------------------------------------------------------------------------------------------------------*/

class FFacadeLight
{
public:
	static bool IsEnable();
	static void RenderLight(FRHICommandList& RHICmdList, const TArray<FViewInfo>& Views, bool bIssueDrawEvent);
};
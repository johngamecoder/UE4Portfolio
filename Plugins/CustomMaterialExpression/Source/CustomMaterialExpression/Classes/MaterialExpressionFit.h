/*=============================================================================
	MaterialExpressionFit.h : Fit expression declaration
	//JOHNGAMECODER
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionFit.generated.h"

UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionFit : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "true"))
	FExpressionInput Value;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstRangeA_Min' if not specified"))
	FExpressionInput RangeA_Min;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstRangeA_Max' if not specified"))
	FExpressionInput RangeA_Max;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstRangeB_Min' if not specified"))
	FExpressionInput RangeB_Min;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstRangeB_Max' if not specified"))
	FExpressionInput RangeB_Max;

	UPROPERTY(EditAnywhere, Category= UMaterialExpressionRemapRange, meta=(OverridingInputProperty = "RangeA_Min"))
	float ConstRangeA_Min;

	UPROPERTY(EditAnywhere, Category= UMaterialExpressionRemapRange, meta=(OverridingInputProperty = "RangeA_Max"))
	float ConstRangeA_Max;

	UPROPERTY(EditAnywhere, Category= UMaterialExpressionRemapRange, meta=(OverridingInputProperty = "RangeB_Min"))
	float ConstRangeB_Min;

	UPROPERTY(EditAnywhere, Category= UMaterialExpressionRemapRange, meta=(OverridingInputProperty = "RangeB_Max"))
	float ConstRangeB_Max;

#if WITH_EDITOR
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
#endif
};
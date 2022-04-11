/*=============================================================================
	MaterialExpressionFit.cpp:Fit expression implementation
	//JOHNGAMECODER
=============================================================================*/

#include "MaterialExpressionFit.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "MaterialCompiler.h"
#if WITH_EDITOR
#include "MaterialGraph/MaterialGraphNode.h"
#endif

#define LOCTEXT_NAMESPACE "MaterialExpression"

UMaterialExpressionFit::UMaterialExpressionFit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConstRangeA_Min = -1;
	ConstRangeA_Max = 1;
	ConstRangeB_Min = 0;
	ConstRangeB_Max = 1;
}

void UMaterialExpressionFit::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Fit : JohnGameCoder");
	OutCaptions.Add(ret);
}

int32 UMaterialExpressionFit::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 ValueCode = Value.Compile(Compiler);
	if (ValueCode == INDEX_NONE)
	{
		Compiler->Errorf(TEXT("Value input has wrong."));
		return INDEX_NONE;
	}

	int32 RangeA_MinCode = RangeA_Min.GetTracedInput().Expression ? RangeA_Min.Compile(Compiler) : Compiler->Constant(ConstRangeA_Min);
	int32 RangeA_MaxCode = RangeA_Max.GetTracedInput().Expression ? RangeA_Max.Compile(Compiler) : Compiler->Constant(ConstRangeA_Max);
	int32 RangeB_MinCode = RangeB_Min.GetTracedInput().Expression ? RangeB_Min.Compile(Compiler) : Compiler->Constant(ConstRangeB_Min);
	int32 RangeB_MaxCode = RangeB_Max.GetTracedInput().Expression ? RangeB_Max.Compile(Compiler) : Compiler->Constant(ConstRangeB_Max);

	int32 InputDivider = Compiler->Div(Compiler->Constant(1), Compiler->Sub(RangeA_MaxCode, RangeA_MinCode)); // InputDivider = 1 / (RangeA_Max-RangeA_Min)
	int32 MulCode = Compiler->Mul(Compiler->Sub(RangeB_MaxCode, RangeB_MinCode), InputDivider); // Mul = (RangeB_Max-RangeB_Min) * InputDivider;
	int32 AddCode; //((RangeA_Max*RangeB_Min) - (RangeA_Min*RangeB_Max))*InputDivider
	{
		AddCode = Compiler->Sub(Compiler->Mul(RangeA_MaxCode, RangeB_MinCode), Compiler->Mul(RangeA_MinCode, RangeB_MaxCode));
		AddCode = Compiler->Mul(AddCode, InputDivider);
	}

	int32 OutputCode = Compiler->Add(Compiler->Mul(ValueCode, MulCode), AddCode); // Value * Mul + Add
	return OutputCode;
}

void UMaterialExpressionFit::GetExpressionToolTip(TArray<FString>& OutToolTip)
{
	ConvertToMultilineToolTip(TEXT("Convert a number range to another range, maintaining ratio."), 40, OutToolTip);
}

#undef LOCTEXT_NAMESPACE
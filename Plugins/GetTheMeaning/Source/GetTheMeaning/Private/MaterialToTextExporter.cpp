// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialToTextExporter.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialParameterCollection.h"
#include "MaterialShared.h"
#include "Engine/Texture.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

namespace
{
	static FString GetObjectPathSafe(const UObject* Obj)
	{
		if (!Obj)
			return TEXT("");
		// /Game/...Asset.Asset
		return Obj->GetPathName();
	}

	static FString GetMaterialTypeLabel(const UMaterialInterface* MI)
	{
		if (!MI)
			return TEXT("MaterialInterface");
		if (MI->IsA<UMaterial>())
			return TEXT("Material");
		if (MI->IsA<UMaterialInstance>())
			return TEXT("MaterialInstance");
		return TEXT("MaterialInterface");
	}

	template <typename TEnum>
	static FString EnumToString(TEnum Value)
	{
		if (const UEnum* Enum = StaticEnum<TEnum>())
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		return FString::Printf(TEXT("%d"), static_cast<int32>(Value));
	}

	template <typename TEnum>
	static FString EnumToString(TEnumAsByte<TEnum> Value)
	{
		return EnumToString<TEnum>(static_cast<TEnum>(Value.GetValue()));
	}

	static FString ShadingModelsToString(const FMaterialShadingModelField& Field)
	{
		TArray<FString> Names;
		for (int32 ModelInt = 0; ModelInt < static_cast<int32>(MSM_MAX); ++ModelInt)
		{
			const EMaterialShadingModel Model = static_cast<EMaterialShadingModel>(ModelInt);
			if (Field.HasShadingModel(Model))
			{
				if (const UEnum* Enum = StaticEnum<EMaterialShadingModel>())
				{
					Names.Add(Enum->GetNameStringByValue(static_cast<int64>(Model)));
				}
				else
				{
					Names.Add(FString::Printf(TEXT("MSM_%d"), ModelInt));
				}
			}
		}
		return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("Unknown");
	}

	static const FScalarParameterValue* FindScalarOverride(const UMaterialInstance* MI, const FMaterialParameterInfo& Info)
	{
		if (!MI) return nullptr;
		for (const FScalarParameterValue& V : MI->ScalarParameterValues)
		{
			if (V.ParameterInfo == Info)
				return &V;
		}
		return nullptr;
	}

	static const FVectorParameterValue* FindVectorOverride(const UMaterialInstance* MI, const FMaterialParameterInfo& Info)
	{
		if (!MI) return nullptr;
		for (const FVectorParameterValue& V : MI->VectorParameterValues)
		{
			if (V.ParameterInfo == Info)
				return &V;
		}
		return nullptr;
	}

	static const FTextureParameterValue* FindTextureOverride(const UMaterialInstance* MI, const FMaterialParameterInfo& Info)
	{
		if (!MI) return nullptr;
		for (const FTextureParameterValue& V : MI->TextureParameterValues)
		{
			if (V.ParameterInfo == Info)
				return &V;
		}
		return nullptr;
	}

	static FString GetValueSourceLabel(const UObject* Obj)
	{
		return Obj ? Obj->GetName() : TEXT("<null>");
	}

	static FString EscapeForQuotedString(const FString& In)
	{
		// 输出到伪代码/文档中，尽量保持可读且不破坏引号结构
		FString Out = In;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Out.ReplaceInline(TEXT("\r"), TEXT(" "));
		Out.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Out;
	}

	static FString SanitizeForMarkdownCell(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("\r"), TEXT(" "));
		Out.ReplaceInline(TEXT("\n"), TEXT(" "));
		Out.ReplaceInline(TEXT("|"), TEXT("\\|"));
		return Out;
	}

	struct FCollectedRefs
	{
		TArray<const UTexture*> Textures;
		TArray<TPair<const UMaterialParameterCollection*, FName>> MPCParameters;
		TArray<const UMaterialFunctionInterface*> Functions;
	};

	static void AddUniqueTexture(TArray<const UTexture*>& Arr, const UTexture* Tex)
	{
		if (!Tex) return;
		Arr.AddUnique(Tex);
	}

	static void AddUniqueFunction(TArray<const UMaterialFunctionInterface*>& Arr, const UMaterialFunctionInterface* Func)
	{
		if (!Func) return;
		Arr.AddUnique(Func);
	}

	static void CollectRefsFromMaterial(const UMaterial* Mat, FCollectedRefs& OutRefs)
	{
		if (!Mat) return;
		for (const UMaterialExpression* Expr : Mat->GetExpressions())
		{
			if (!Expr) continue;

			if (const UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expr))
			{
				AddUniqueTexture(OutRefs.Textures, TexSample->Texture);
				continue;
			}
			if (const UMaterialExpressionTextureObject* TexObj = Cast<UMaterialExpressionTextureObject>(Expr))
			{
				AddUniqueTexture(OutRefs.Textures, TexObj->Texture);
				continue;
			}
			if (const UMaterialExpressionCollectionParameter* Col = Cast<UMaterialExpressionCollectionParameter>(Expr))
			{
				if (Col->Collection)
				{
					OutRefs.MPCParameters.AddUnique(TPair<const UMaterialParameterCollection*, FName>(Col->Collection, Col->ParameterName));
				}
				continue;
			}
			if (const UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
			{
				AddUniqueFunction(OutRefs.Functions, FuncCall->MaterialFunction);
				continue;
			}
		}
	}

	static void CollectRefsFromFunction(const UMaterialFunctionInterface* Func, FCollectedRefs& OutRefs)
	{
		if (!Func) return;
		for (const UMaterialExpression* Expr : Func->GetExpressions())
		{
			if (!Expr) continue;

			if (const UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expr))
			{
				AddUniqueTexture(OutRefs.Textures, TexSample->Texture);
				continue;
			}
			if (const UMaterialExpressionTextureObject* TexObj = Cast<UMaterialExpressionTextureObject>(Expr))
			{
				AddUniqueTexture(OutRefs.Textures, TexObj->Texture);
				continue;
			}
			if (const UMaterialExpressionCollectionParameter* Col = Cast<UMaterialExpressionCollectionParameter>(Expr))
			{
				if (Col->Collection)
				{
					OutRefs.MPCParameters.AddUnique(TPair<const UMaterialParameterCollection*, FName>(Col->Collection, Col->ParameterName));
				}
				continue;
			}
			if (const UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr))
			{
				AddUniqueFunction(OutRefs.Functions, FuncCall->MaterialFunction);
				continue;
			}
		}
	}

	struct FEmitContext
	{
		TMap<const UMaterialExpression*, FString> VarByExpr;
		TArray<FString> Lines;
		int32 TempIndex = 0;

		struct FUnknownExprInfo
		{
			FString Var;
			FString ClassName;
			FString NodeName;
			FString TitleOrDesc;
			FString Guid;
			int32 NumInputs = 0;
		};

		// 跨属性口汇总 Unknown（由调用者提供容器）
		TArray<FUnknownExprInfo>* UnknownSink = nullptr;
	};

	static FString EmitLiteralFloat(float V)
	{
		// 统一小数格式，避免 1.000000
		return FString::SanitizeFloat(V);
	}

	static FString EmitLiteralVec4(const FLinearColor& C)
	{
		return FString::Printf(TEXT("float4(%s, %s, %s, %s)"),
			*FString::SanitizeFloat(C.R),
			*FString::SanitizeFloat(C.G),
			*FString::SanitizeFloat(C.B),
			*FString::SanitizeFloat(C.A));
	}

	static FString EmitLiteralVec3(const FLinearColor& C)
	{
		return FString::Printf(TEXT("float3(%s, %s, %s)"),
			*FString::SanitizeFloat(C.R),
			*FString::SanitizeFloat(C.G),
			*FString::SanitizeFloat(C.B));
	}

	static FString EmitValueForInput(FEmitContext& Ctx, const FExpressionInput* Input);
	static FString EmitValue(FEmitContext& Ctx, const UMaterialExpression* Expr);

	static FString EmitValueForInput(FEmitContext& Ctx, const FExpressionInput* Input)
	{
		if (!Input || !Input->Expression)
			return TEXT("0");
		return EmitValue(Ctx, Input->Expression);
	}

	static FString MakeTempName(FEmitContext& Ctx, const UMaterialExpression* Expr)
	{
		return FString::Printf(TEXT("t%d"), Ctx.TempIndex++);
	}

	static FString EmitTextureSampleExpr(const UMaterialExpressionTextureSample* Tex, const FString& UV)
	{
		const UTexture* T = Tex ? Tex->Texture : nullptr;
		const FString TexPath = GetObjectPathSafe(T);
		// 这里用“语义函数”而不是 UE 内部的 Texture2DSample/SamplerState，便于 AI 按目标环境改写
		return FString::Printf(TEXT("SampleTexture(\"%s\", %s)"), *TexPath, *UV);
	}

	static FString EmitValue(FEmitContext& Ctx, const UMaterialExpression* Expr)
	{
		if (!Expr)
			return TEXT("0");

		if (const FString* Found = Ctx.VarByExpr.Find(Expr))
			return *Found;

		// 尽量对“常量类”做内联（不生成临时变量），提高可读性
		if (const UMaterialExpressionConstant* K = Cast<UMaterialExpressionConstant>(Expr))
			return EmitLiteralFloat(K->R);
		if (const UMaterialExpressionConstant3Vector* V3 = Cast<UMaterialExpressionConstant3Vector>(Expr))
			return EmitLiteralVec3(V3->Constant);
		if (const UMaterialExpressionConstant4Vector* V4 = Cast<UMaterialExpressionConstant4Vector>(Expr))
			return EmitLiteralVec4(V4->Constant);

		// 其余节点生成临时变量，保证依赖顺序稳定
		const FString Var = MakeTempName(Ctx, Expr);
		Ctx.VarByExpr.Add(Expr, Var);

		FString RHS = TEXT("0");

		if (const UMaterialExpressionScalarParameter* SP = Cast<UMaterialExpressionScalarParameter>(Expr))
		{
			RHS = FString::Printf(TEXT("ParamScalar(\"%s\")"), *SP->ParameterName.ToString());
		}
		else if (const UMaterialExpressionVectorParameter* VP = Cast<UMaterialExpressionVectorParameter>(Expr))
		{
			RHS = FString::Printf(TEXT("ParamVector(\"%s\")"), *VP->ParameterName.ToString());
		}
		else if (const UMaterialExpressionTextureSampleParameter2D* TP = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
		{
			const FString UV = EmitValueForInput(Ctx, &TP->Coordinates);
			// Parameter 2D 可能在 MI 中被覆盖；这里用参数名 +（可选）当前绑定纹理路径
			const FString TexPath = GetObjectPathSafe(TP->Texture);
			RHS = FString::Printf(TEXT("SampleTextureParam2D(\"%s\", \"%s\", %s)"), *TP->ParameterName.ToString(), *TexPath, *UV);
		}
		else if (const UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr))
		{
			const FString UV = EmitValueForInput(Ctx, &TS->Coordinates);
			RHS = EmitTextureSampleExpr(TS, UV);
		}
		else if (const UMaterialExpressionTextureObject* TO = Cast<UMaterialExpressionTextureObject>(Expr))
		{
			const FString TexPath = GetObjectPathSafe(TO->Texture);
			RHS = FString::Printf(TEXT("TextureObject(\"%s\")"), *EscapeForQuotedString(TexPath));
		}
		else if (const UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &Add->A);
			const FString B = EmitValueForInput(Ctx, &Add->B);
			RHS = FString::Printf(TEXT("(%s + %s)"), *A, *B);
		}
		else if (const UMaterialExpressionSubtract* Sub = Cast<UMaterialExpressionSubtract>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &Sub->A);
			const FString B = EmitValueForInput(Ctx, &Sub->B);
			RHS = FString::Printf(TEXT("(%s - %s)"), *A, *B);
		}
		else if (const UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &Mul->A);
			const FString B = EmitValueForInput(Ctx, &Mul->B);
			RHS = FString::Printf(TEXT("(%s * %s)"), *A, *B);
		}
		else if (const UMaterialExpressionDivide* Div = Cast<UMaterialExpressionDivide>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &Div->A);
			const FString B = EmitValueForInput(Ctx, &Div->B);
			RHS = FString::Printf(TEXT("(%s / %s)"), *A, *B);
		}
		else if (const UMaterialExpressionLinearInterpolate* Lerp = Cast<UMaterialExpressionLinearInterpolate>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &Lerp->A);
			const FString B = EmitValueForInput(Ctx, &Lerp->B);
			const FString Alpha = EmitValueForInput(Ctx, &Lerp->Alpha);
			RHS = FString::Printf(TEXT("lerp(%s, %s, %s)"), *A, *B, *Alpha);
		}
		else if (const UMaterialExpressionClamp* Clamp = Cast<UMaterialExpressionClamp>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &Clamp->Input);
			const FString MinV = EmitValueForInput(Ctx, &Clamp->Min);
			const FString MaxV = EmitValueForInput(Ctx, &Clamp->Max);
			RHS = FString::Printf(TEXT("clamp(%s, %s, %s)"), *In, *MinV, *MaxV);
		}
		else if (const UMaterialExpressionSaturate* Sat = Cast<UMaterialExpressionSaturate>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &Sat->Input);
			RHS = FString::Printf(TEXT("saturate(%s)"), *In);
		}
		else if (const UMaterialExpressionOneMinus* OneMinus = Cast<UMaterialExpressionOneMinus>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &OneMinus->Input);
			RHS = FString::Printf(TEXT("(1 - %s)"), *In);
		}
		else if (const UMaterialExpressionNormalize* Norm = Cast<UMaterialExpressionNormalize>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &Norm->VectorInput);
			RHS = FString::Printf(TEXT("normalize(%s)"), *In);
		}
		else if (const UMaterialExpressionDotProduct* Dot = Cast<UMaterialExpressionDotProduct>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &Dot->A);
			const FString B = EmitValueForInput(Ctx, &Dot->B);
			RHS = FString::Printf(TEXT("dot(%s, %s)"), *A, *B);
		}
		else if (const UMaterialExpressionFresnel* Fres = Cast<UMaterialExpressionFresnel>(Expr))
		{
			const FString Normal = EmitValueForInput(Ctx, &Fres->Normal);
			const FString Exponent = EmitValueForInput(Ctx, &Fres->ExponentIn);
			const FString BaseReflect = EmitValueForInput(Ctx, &Fres->BaseReflectFractionIn);
			RHS = FString::Printf(TEXT("FresnelApprox(%s, %s, %s)"), *Normal, *Exponent, *BaseReflect);
		}
		else if (const UMaterialExpressionPanner* Pan = Cast<UMaterialExpressionPanner>(Expr))
		{
			const FString UV = EmitValueForInput(Ctx, &Pan->Coordinate);
			const FString Time = EmitValueForInput(Ctx, &Pan->Time);
			const FString Speed = Pan->Speed.Expression
				? EmitValueForInput(Ctx, &Pan->Speed)
				: FString::Printf(TEXT("float2(%s, %s)"), *EmitLiteralFloat(Pan->SpeedX), *EmitLiteralFloat(Pan->SpeedY));
			RHS = FString::Printf(TEXT("Panner(%s, %s, %s)"), *UV, *Time, *Speed);
		}
		else if (const UMaterialExpressionTime* T = Cast<UMaterialExpressionTime>(Expr))
		{
			(void)T;
			RHS = TEXT("Time()");
		}
		else if (const UMaterialExpressionTextureCoordinate* TC = Cast<UMaterialExpressionTextureCoordinate>(Expr))
		{
			// 语义函数：把平铺/通道信息显式暴露给 AI
			RHS = FString::Printf(TEXT("TexCoord(%d, float2(%s, %s), float2(%s, %s))"),
				TC->CoordinateIndex,
				*EmitLiteralFloat(TC->UTiling),
				*EmitLiteralFloat(TC->VTiling),
				*EmitLiteralFloat(TC->UnMirrorU ? 1.0f : 0.0f),
				*EmitLiteralFloat(TC->UnMirrorV ? 1.0f : 0.0f));
		}
		else if (const UMaterialExpressionIf* IfExpr = Cast<UMaterialExpressionIf>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &IfExpr->A);
			const FString B = EmitValueForInput(Ctx, &IfExpr->B);
			const FString AGT = EmitValueForInput(Ctx, &IfExpr->AGreaterThanB);
			const FString AEq = EmitValueForInput(Ctx, &IfExpr->AEqualsB);
			const FString ALT = EmitValueForInput(Ctx, &IfExpr->ALessThanB);
			const FString Th = EmitLiteralFloat(IfExpr->EqualsThreshold);
			// 保持语义清晰：If(A,B,GT,EQ,LT,Threshold)
			RHS = FString::Printf(TEXT("If(%s, %s, %s, %s, %s, %s)"), *A, *B, *AGT, *AEq, *ALT, *Th);
		}
		else if (const UMaterialExpressionPower* PowExpr = Cast<UMaterialExpressionPower>(Expr))
		{
			const FString Base = EmitValueForInput(Ctx, &PowExpr->Base);
			const FString Exp = EmitValueForInput(Ctx, &PowExpr->Exponent);
			RHS = FString::Printf(TEXT("pow(%s, %s)"), *Base, *Exp);
		}
		else if (const UMaterialExpressionAbs* AbsExpr = Cast<UMaterialExpressionAbs>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &AbsExpr->Input);
			RHS = FString::Printf(TEXT("abs(%s)"), *In);
		}
		else if (const UMaterialExpressionMin* MinExpr = Cast<UMaterialExpressionMin>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &MinExpr->A);
			const FString B = EmitValueForInput(Ctx, &MinExpr->B);
			RHS = FString::Printf(TEXT("min(%s, %s)"), *A, *B);
		}
		else if (const UMaterialExpressionMax* MaxExpr = Cast<UMaterialExpressionMax>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &MaxExpr->A);
			const FString B = EmitValueForInput(Ctx, &MaxExpr->B);
			RHS = FString::Printf(TEXT("max(%s, %s)"), *A, *B);
		}
		else if (const UMaterialExpressionSine* SinExpr = Cast<UMaterialExpressionSine>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &SinExpr->Input);
			RHS = FString::Printf(TEXT("sin(%s)"), *In);
		}
		else if (const UMaterialExpressionCosine* CosExpr = Cast<UMaterialExpressionCosine>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &CosExpr->Input);
			RHS = FString::Printf(TEXT("cos(%s)"), *In);
		}
		else if (const UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expr))
		{
			const FString In = EmitValueForInput(Ctx, &Mask->Input);
			FString Swizzle;
			if (Mask->R) Swizzle += TEXT("r");
			if (Mask->G) Swizzle += TEXT("g");
			if (Mask->B) Swizzle += TEXT("b");
			if (Mask->A) Swizzle += TEXT("a");
			if (Swizzle.IsEmpty()) Swizzle = TEXT("r");
			RHS = FString::Printf(TEXT("(%s.%s)"), *In, *Swizzle);
		}
		else if (const UMaterialExpressionAppendVector* App = Cast<UMaterialExpressionAppendVector>(Expr))
		{
			const FString A = EmitValueForInput(Ctx, &App->A);
			const FString B = EmitValueForInput(Ctx, &App->B);
			RHS = FString::Printf(TEXT("Append(%s, %s)"), *A, *B);
		}
		else
		{
			TArray<FString> Args;
			UMaterialExpression* MutableExpr = const_cast<UMaterialExpression*>(Expr);
			const int32 NumInputs = MutableExpr->CountInputs();
			Args.Reserve(NumInputs);
			for (int32 i = 0; i < NumInputs; ++i)
			{
				const FExpressionInput* In = MutableExpr->GetInput(i);
				Args.Add(EmitValueForInput(Ctx, In));
			}

			const FString ClassName = Expr->GetClass()->GetName();
			const FString TitleOrDesc = Expr->Desc.IsEmpty() ? Expr->GetName() : Expr->Desc;
			const FString EscTitle = EscapeForQuotedString(TitleOrDesc);
			const FString JoinedArgs = FString::Join(Args, TEXT(", "));
			RHS = FString::Printf(TEXT("UnknownExpr(\"%s\", \"%s\"%s%s)"),
				*EscapeForQuotedString(ClassName),
				*EscTitle,
				Args.Num() > 0 ? TEXT(", ") : TEXT(""),
				*JoinedArgs);

			if (Ctx.UnknownSink)
			{
				FEmitContext::FUnknownExprInfo Info;
				Info.Var = Var;
				Info.ClassName = ClassName;
				Info.NodeName = Expr->GetName();
				Info.TitleOrDesc = TitleOrDesc;
				Info.Guid = Expr->MaterialExpressionGuid.IsValid() ? Expr->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("");
				Info.NumInputs = NumInputs;
				Ctx.UnknownSink->Add(MoveTemp(Info));
			}
		}

		Ctx.Lines.Add(FString::Printf(TEXT("%s = %s;"), *Var, *RHS));
		return Var;
	}

	static void EmitMaterialPropertyBlock(UMaterial* Mat, EMaterialProperty Prop, const TCHAR* Label, FEmitContext& OutCtx, FString& OutText)
	{
		if (!Mat) return;
		const FExpressionInput* Input = Mat->GetExpressionInputForProperty(Prop);
		if (!Input || !Input->Expression)
			return;

		OutCtx.VarByExpr.Reset();
		OutCtx.Lines.Reset();
		OutCtx.TempIndex = 0;

		const FString Root = EmitValueForInput(OutCtx, Input);

		OutText += FString::Printf(TEXT("### %s\n\n"), Label);
		OutText += TEXT("```hlsl\n");
		OutText += TEXT("// Helper conventions (pseudo):\n");
		OutText += TEXT("// - ParamScalar(name), ParamVector(name)\n");
		OutText += TEXT("// - SampleTexture(path, uv)\n");
		OutText += TEXT("// - SampleTextureParam2D(paramName, boundPath, uv)\n\n");
		for (const FString& Line : OutCtx.Lines)
		{
			OutText += Line + TEXT("\n");
		}
		OutText += FString::Printf(TEXT("%s = %s;\n"), Label, *Root);
		OutText += TEXT("```\n\n");
	}

	static void EmitFunctionGraphIndex(const UMaterialFunctionInterface* Func, FString& OutText)
	{
		if (!Func) return;

		TConstArrayView<TObjectPtr<UMaterialExpression>> AllExprs = Func->GetExpressions();
		TMap<const UMaterialExpression*, int32> IdByExpr;
		for (int32 i = 0; i < AllExprs.Num(); ++i)
		{
			if (UMaterialExpression* E = AllExprs[i])
				IdByExpr.Add(E, i);
		}

		OutText += TEXT("\n### Nodes\n\n");
		OutText += TEXT("| Id | Class | Desc |\n");
		OutText += TEXT("|---:|-------|------|\n");
		for (int32 i = 0; i < AllExprs.Num(); ++i)
		{
			UMaterialExpression* Expr = AllExprs[i];
			if (!Expr) continue;
			const FString Desc = Expr->Desc.IsEmpty() ? Expr->GetName() : Expr->Desc;
			OutText += FString::Printf(TEXT("| %d | %s | %s |\n"), i, *Expr->GetClass()->GetName(), *Desc.Replace(TEXT("\n"), TEXT(" ")));
		}

		OutText += TEXT("\n### Links\n\n");
		OutText += TEXT("| FromId | From | ToId | ToOutputIndex |\n");
		OutText += TEXT("|------:|------|----:|-------------:|\n");
		for (int32 i = 0; i < AllExprs.Num(); ++i)
		{
			UMaterialExpression* Expr = AllExprs[i];
			if (!Expr) continue;

			const int32 NumInputs = Expr->CountInputs();
			for (int32 InIdx = 0; InIdx < NumInputs; ++InIdx)
			{
				const FExpressionInput* In = Expr->GetInput(InIdx);
				if (!In || !In->Expression) continue;
				const int32* ToId = IdByExpr.Find(In->Expression);
				const FString FromName = Expr->GetInputName(InIdx).ToString();
				OutText += FString::Printf(TEXT("| %d | %s | %d | %d |\n"), i, *FromName, ToId ? *ToId : -1, In->OutputIndex);
			}
		}
	}
}

FString FMaterialToTextExporter::ExportMaterialToText(UMaterialInterface* MaterialInterface)
{
	if (!MaterialInterface)
		return FString();

	FString Out;
	Out += TEXT("# Material Export (for AI)\n");
	Out += TEXT("# Name: ") + MaterialInterface->GetName() + TEXT("\n");
	Out += TEXT("# Type: ") + GetMaterialTypeLabel(MaterialInterface) + TEXT("\n");
	Out += TEXT("# ObjectPath: ") + GetObjectPathSafe(MaterialInterface) + TEXT("\n\n");

	Out += TEXT("---\n");
	Out += TEXT("## Material Summary\n");
	{
		UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		if (!BaseMaterial)
		{
			Out += TEXT("- BaseMaterial: <null>\n");
		}
		else
		{
			Out += TEXT("- BaseMaterial: ") + GetObjectPathSafe(BaseMaterial) + TEXT("\n");
			Out += TEXT("- MaterialDomain: ") + EnumToString(BaseMaterial->MaterialDomain) + TEXT("\n");
			Out += TEXT("- BlendMode: ") + EnumToString(BaseMaterial->BlendMode) + TEXT("\n");
			Out += TEXT("- ShadingModels: ") + ShadingModelsToString(BaseMaterial->GetShadingModels()) + TEXT("\n");
			Out += TEXT("- TwoSided: ") + FString(BaseMaterial->TwoSided ? TEXT("true") : TEXT("false")) + TEXT("\n");
			Out += TEXT("- UsesMaterialAttributes: ") + FString(BaseMaterial->bUseMaterialAttributes ? TEXT("true") : TEXT("false")) + TEXT("\n");
			Out += TEXT("- OpacityMaskClipValue: ") + FString::SanitizeFloat(BaseMaterial->OpacityMaskClipValue) + TEXT("\n");
		}

		if (UMaterialInstance* AsMI = Cast<UMaterialInstance>(MaterialInterface))
		{
			Out += TEXT("- Parent: ") + GetObjectPathSafe(AsMI->Parent) + TEXT("\n");
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## Render Pass Participation (Typical)\n");
	{
		UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		if (!BaseMaterial)
		{
			Out += TEXT("- BaseMaterial: <null>\n");
		}
		else
		{
			const EMaterialDomain Domain = BaseMaterial->MaterialDomain;
			const EBlendMode Blend = BaseMaterial->BlendMode;
			const bool bMasked = (Blend == BLEND_Masked);
			const bool bTranslucent = (Blend == BLEND_Translucent || Blend == BLEND_Additive || Blend == BLEND_Modulate || Blend == BLEND_AlphaComposite || Blend == BLEND_AlphaHoldout);

			Out += TEXT("### Pass Order (Typical)\n\n");

			if (Domain == MD_PostProcess)
			{
				Out += TEXT("1. **PostProcess**\n");
				Out += TEXT("   - 材质域为 PostProcess，通常作为后处理阶段的一部分执行（取决于具体挂载位置/Blendable）。\n");
				Out += TEXT("   - 主要关注：SceneTexture 读取、屏幕空间 UV、以及与后处理链路的混合方式。\n");
			}
			else if (bTranslucent)
			{
				Out += TEXT("1. **Translucency / SeparateTranslucency**\n");
				Out += TEXT("   - 透明材质通常在不透明物体之后渲染；大多数情况下不写入深度（依设置而定），依赖深度测试排序。\n");
				Out += TEXT("   - 主要关注：BlendMode 对混合方程影响、透明光照模式、是否参与阴影（通常受限）。\n\n");
				Out += TEXT("2. **(Optional) DepthOnly / CustomDepth**\n");
				Out += TEXT("   - 若启用写入 CustomDepth/Stencil 或特殊深度策略，会在对应深度 pass 额外渲染。\n");
			}
			else
			{
				Out += TEXT("1. **(Optional) DepthPrepass / EarlyZ**\n");
				Out += TEXT("   - 典型用于提前写入深度，提升后续 BasePass 的 Early-Z 命中率。\n");
				if (bMasked)
				{
					Out += TEXT("   - Masked 材质在深度相关 pass 里会执行 AlphaTest（`OpacityMask` 与 `ClipValue`），决定是否写深度。\n");
				}
				Out += TEXT("\n2. **ShadowDepth / ShadowMap**\n");
				Out += TEXT("   - 参与阴影深度渲染；Masked 会做 AlphaTest，影响投影轮廓。\n");
				Out += TEXT("\n3. **BasePass**\n");
				Out += TEXT("   - 主光照/材质计算阶段：读取 BaseColor/Normal/Roughness/Metallic/Emissive 等。\n");
				Out += TEXT("   - Opaque/Masked 通常在此阶段写入 GBuffer 或前向路径数据。\n");
				Out += TEXT("\n4. **(Optional) Velocity**\n");
				Out += TEXT("   - 若输出运动矢量（TAA/MotionBlur），会有速度 pass 或在 BasePass 输出 velocity。\n");
				Out += TEXT("\n5. **(Optional) CustomDepth/Stencil**\n");
				Out += TEXT("   - 若材质/网格开启 CustomDepth，则在该 pass 额外渲染用于描边/遮罩等效果。\n");
			}

			Out += TEXT("\n### Notes\n\n");
			Out += TEXT("- 以上为 **UE 典型/规范流程解释**（面向理解与复写），具体项目可能因渲染设置、平台、前向/延迟路径、Nanite 等产生差异。\n");
			Out += TEXT("- Masked 的关键点是 **AlphaTest**：`OpacityMask >= ClipValue` 才会写入深度/阴影。\n");
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## Parameters\n");
	{
		TArray<FMaterialParameterInfo> ScalarInfos;
		TArray<FMaterialParameterInfo> VectorInfos;
		TArray<FMaterialParameterInfo> TextureInfos;
		TArray<FGuid> DummyIds;

		MaterialInterface->GetAllScalarParameterInfo(ScalarInfos, DummyIds);
		MaterialInterface->GetAllVectorParameterInfo(VectorInfos, DummyIds);
		MaterialInterface->GetAllTextureParameterInfo(TextureInfos, DummyIds);

		UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		UMaterialInstance* LeafMI = Cast<UMaterialInstance>(MaterialInterface);

		auto DescribeParamInfo = [](const FMaterialParameterInfo& Info) -> FString
		{
			// 目前主要用 Name；Association/Index 对 Layered 材质后续再扩展
			return Info.Name.ToString();
		};

		Out += TEXT("### Scalars\n\n");
		Out += TEXT("| Name | FinalValue | Source | Notes |\n");
		Out += TEXT("|------|-----------:|--------|-------|\n");
		for (const FMaterialParameterInfo& Info : ScalarInfos)
		{
			float FinalValue = 0.0f;
			const bool bHasFinal = MaterialInterface->GetScalarParameterValue(Info, FinalValue);

			FString Source = TEXT("BaseMaterial");
			if (LeafMI)
			{
				const UMaterialInstance* Cursor = LeafMI;
				while (Cursor)
				{
					if (FindScalarOverride(Cursor, Info))
					{
						Source = GetValueSourceLabel(Cursor);
						break;
					}
					Cursor = Cast<UMaterialInstance>(Cursor->Parent);
				}
			}

			Out += TEXT("| ") + DescribeParamInfo(Info) + TEXT(" | ")
				+ (bHasFinal ? FString::SanitizeFloat(FinalValue) : TEXT("<unset>"))
				+ TEXT(" | ") + Source + TEXT(" | |\n");
		}

		Out += TEXT("\n### Vectors\n\n");
		Out += TEXT("| Name | FinalValue | Source | Notes |\n");
		Out += TEXT("|------|------------|--------|-------|\n");
		for (const FMaterialParameterInfo& Info : VectorInfos)
		{
			FLinearColor FinalValue = FLinearColor::Black;
			const bool bHasFinal = MaterialInterface->GetVectorParameterValue(Info, FinalValue);

			FString Source = TEXT("BaseMaterial");
			if (LeafMI)
			{
				const UMaterialInstance* Cursor = LeafMI;
				while (Cursor)
				{
					if (FindVectorOverride(Cursor, Info))
					{
						Source = GetValueSourceLabel(Cursor);
						break;
					}
					Cursor = Cast<UMaterialInstance>(Cursor->Parent);
				}
			}

			FString ValueStr = bHasFinal
				? FString::Printf(TEXT("(%.6g, %.6g, %.6g, %.6g)"), FinalValue.R, FinalValue.G, FinalValue.B, FinalValue.A)
				: TEXT("<unset>");

			Out += TEXT("| ") + DescribeParamInfo(Info) + TEXT(" | ")
				+ ValueStr + TEXT(" | ") + Source + TEXT(" | |\n");
		}

		Out += TEXT("\n### Textures\n\n");
		Out += TEXT("| Name | FinalValue(ObjectPath) | Source | Notes |\n");
		Out += TEXT("|------|------------------------|--------|-------|\n");
		for (const FMaterialParameterInfo& Info : TextureInfos)
		{
			UTexture* FinalTex = nullptr;
			const bool bHasFinal = MaterialInterface->GetTextureParameterValue(Info, FinalTex);

			FString Source = TEXT("BaseMaterial");
			if (LeafMI)
			{
				const UMaterialInstance* Cursor = LeafMI;
				while (Cursor)
				{
					if (FindTextureOverride(Cursor, Info))
					{
						Source = GetValueSourceLabel(Cursor);
						break;
					}
					Cursor = Cast<UMaterialInstance>(Cursor->Parent);
				}
			}

			FString ValueStr = bHasFinal && FinalTex ? GetObjectPathSafe(FinalTex) : TEXT("<unset>");
			Out += TEXT("| ") + DescribeParamInfo(Info) + TEXT(" | ")
				+ ValueStr + TEXT(" | ") + Source + TEXT(" | |\n");
		}

		Out += TEXT("\n");
		Out += TEXT("- Parameter override chain: Leaf MaterialInstance → Parent(s) → BaseMaterial\n");
		if (LeafMI)
		{
			const UMaterialInstance* Cursor = LeafMI;
			int32 Depth = 0;
			while (Cursor && Depth < 32)
			{
				Out += TEXT("  - ") + GetObjectPathSafe(Cursor) + TEXT("\n");
				Cursor = Cast<UMaterialInstance>(Cursor->Parent);
				Depth++;
			}
			if (BaseMaterial)
				Out += TEXT("  - ") + GetObjectPathSafe(BaseMaterial) + TEXT("\n");
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## External References\n");
	{
		FCollectedRefs Refs;
		UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		CollectRefsFromMaterial(BaseMaterial, Refs);

		Out += TEXT("### Material Parameter Collections (MPC)\n\n");
		if (Refs.MPCParameters.Num() == 0)
		{
			Out += TEXT("- (none)\n");
		}
		else
		{
			for (const auto& Pair : Refs.MPCParameters)
			{
				const UMaterialParameterCollection* Collection = Pair.Key;
				const FName ParamName = Pair.Value;
				Out += TEXT("- Collection: ") + GetObjectPathSafe(Collection)
					+ TEXT("  Param: ") + ParamName.ToString() + TEXT("\n");
			}
		}

		Out += TEXT("\n### Material Functions\n\n");
		if (Refs.Functions.Num() == 0)
		{
			Out += TEXT("- (none)\n");
		}
		else
		{
			for (const UMaterialFunctionInterface* Func : Refs.Functions)
			{
				Out += TEXT("- ") + GetObjectPathSafe(Func) + TEXT("\n");
			}
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## Textures & Samplers\n");
	{
		FCollectedRefs Refs;
		UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		CollectRefsFromMaterial(BaseMaterial, Refs);

		if (Refs.Textures.Num() == 0)
		{
			Out += TEXT("- (none)\n");
		}
		else
		{
			Out += TEXT("| Texture | ObjectPath | sRGB | Notes |\n");
			Out += TEXT("|---------|------------|:----:|-------|\n");
			for (const UTexture* Tex : Refs.Textures)
			{
				const bool bSRGB = Tex ? Tex->SRGB : false;
				Out += TEXT("| ") + (Tex ? Tex->GetName() : TEXT("<null>"))
					+ TEXT(" | ") + GetObjectPathSafe(Tex)
					+ TEXT(" | ") + (bSRGB ? TEXT("true") : TEXT("false"))
					+ TEXT(" | |\n");
			}
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## Pseudo HLSL (Code-First)\n");
	{
		UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		if (!BaseMaterial)
		{
			Out += TEXT("- BaseMaterial: <null>\n");
		}
		else
		{
			TArray<FEmitContext::FUnknownExprInfo> Unknowns;
			FEmitContext Ctx;
			Ctx.UnknownSink = &Unknowns;

			EmitMaterialPropertyBlock(BaseMaterial, MP_BaseColor, TEXT("BaseColor"), Ctx, Out);
			EmitMaterialPropertyBlock(BaseMaterial, MP_Normal, TEXT("Normal"), Ctx, Out);
			EmitMaterialPropertyBlock(BaseMaterial, MP_EmissiveColor, TEXT("Emissive"), Ctx, Out);
			EmitMaterialPropertyBlock(BaseMaterial, MP_OpacityMask, TEXT("OpacityMask"), Ctx, Out);

			Out += TEXT("- 注：此处为“材质图语义级伪 HLSL”，非 UE 最终编译产物；未覆盖节点会以 `UnknownExpr(\"Class\", \"Title\", ...)` 形式保留。\n");

			Out += TEXT("\n---\n");
			Out += TEXT("## Unknown Expressions (Fallback)\n");
			if (Unknowns.Num() == 0)
			{
				Out += TEXT("- (none)\n");
			}
			else
			{
				// 去重（跨多个属性口时，同一个表达式可能被重复记录）
				TSet<FString> SeenKeys;
				TArray<FEmitContext::FUnknownExprInfo> Unique;
				Unique.Reserve(Unknowns.Num());
				for (const auto& U : Unknowns)
				{
					const FString Key = U.Guid.IsEmpty()
						? (U.ClassName + TEXT("|") + U.NodeName + TEXT("|") + U.TitleOrDesc + TEXT("|") + U.Var)
						: (U.Guid + TEXT("|") + U.Var);
					if (SeenKeys.Contains(Key))
						continue;
					SeenKeys.Add(Key);
					Unique.Add(U);
				}

				TMap<FString, int32> CountByClass;
				for (const auto& U : Unique)
				{
					int32& C = CountByClass.FindOrAdd(U.ClassName);
					++C;
				}

				Out += TEXT("\n### Summary (by Class)\n\n");
				Out += TEXT("| Class | Count |\n");
				Out += TEXT("|-------|------:|\n");
				for (const auto& Pair : CountByClass)
				{
					Out += FString::Printf(TEXT("| %s | %d |\n"), *SanitizeForMarkdownCell(Pair.Key), Pair.Value);
				}

				Out += TEXT("\n### Details\n\n");
				Out += TEXT("| Var | Class | NodeName | Title/Desc | Inputs | Guid |\n");
				Out += TEXT("|-----|-------|----------|------------|------:|------|\n");
				for (const auto& U : Unique)
				{
					Out += FString::Printf(TEXT("| %s | %s | %s | %s | %d | %s |\n"),
						*SanitizeForMarkdownCell(U.Var),
						*SanitizeForMarkdownCell(U.ClassName),
						*SanitizeForMarkdownCell(U.NodeName),
						*SanitizeForMarkdownCell(U.TitleOrDesc),
						U.NumInputs,
						*SanitizeForMarkdownCell(U.Guid));
				}
			}
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## Graph Index\n");
	{
		UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		if (!BaseMaterial)
		{
			Out += TEXT("- BaseMaterial: <null>\n");
		}
		else
		{
			TConstArrayView<TObjectPtr<UMaterialExpression>> AllExprs = BaseMaterial->GetExpressions();

			TMap<const UMaterialExpression*, int32> IdByExpr;
			for (int32 i = 0; i < AllExprs.Num(); ++i)
			{
				if (UMaterialExpression* E = AllExprs[i])
					IdByExpr.Add(E, i);
			}

			Out += TEXT("\n### Nodes\n\n");
			Out += TEXT("| Id | Class | Desc |\n");
			Out += TEXT("|---:|-------|------|\n");
			for (int32 i = 0; i < AllExprs.Num(); ++i)
			{
				UMaterialExpression* Expr = AllExprs[i];
				if (!Expr) continue;
				const FString Desc = Expr->Desc.IsEmpty() ? Expr->GetName() : Expr->Desc;
				Out += FString::Printf(TEXT("| %d | %s | %s |\n"), i, *Expr->GetClass()->GetName(), *Desc.Replace(TEXT("\n"), TEXT(" ")));
			}

			Out += TEXT("\n### Links\n\n");
			Out += TEXT("| FromId | From | ToId | ToOutputIndex |\n");
			Out += TEXT("|------:|------|----:|-------------:|\n");
			for (int32 i = 0; i < AllExprs.Num(); ++i)
			{
				UMaterialExpression* Expr = AllExprs[i];
				if (!Expr) continue;

				const int32 NumInputs = Expr->CountInputs();
				for (int32 InIdx = 0; InIdx < NumInputs; ++InIdx)
				{
					const FExpressionInput* In = Expr->GetInput(InIdx);
					if (!In || !In->Expression) continue;
					const int32* ToId = IdByExpr.Find(In->Expression);
					const FString FromName = Expr->GetInputName(InIdx).ToString();
					Out += FString::Printf(TEXT("| %d | %s | %d | %d |\n"), i, *FromName, ToId ? *ToId : -1, In->OutputIndex);
				}
			}
		}
	}
	Out += TEXT("\n");

	return Out;
}

FString FMaterialToTextExporter::ExportMaterialFunctionToText(UMaterialFunctionInterface* MaterialFunction)
{
	if (!MaterialFunction)
		return FString();

	FString Out;
	Out += TEXT("# Material Function Export (for AI)\n");
	Out += TEXT("# Name: ") + MaterialFunction->GetName() + TEXT("\n");
	Out += TEXT("# Type: MaterialFunction\n");
	Out += TEXT("# ObjectPath: ") + GetObjectPathSafe(MaterialFunction) + TEXT("\n\n");

	Out += TEXT("---\n");
	Out += TEXT("## Function Summary\n");
	Out += TEXT("- 注：Material Function 本身不直接对应 BasePass/属性口，它是材质图的可复用子图。\n\n");

	Out += TEXT("---\n");
	Out += TEXT("## External References\n");
	{
		FCollectedRefs Refs;
		CollectRefsFromFunction(MaterialFunction, Refs);

		Out += TEXT("### Material Parameter Collections (MPC)\n\n");
		if (Refs.MPCParameters.Num() == 0)
		{
			Out += TEXT("- (none)\n");
		}
		else
		{
			for (const auto& Pair : Refs.MPCParameters)
			{
				const UMaterialParameterCollection* Collection = Pair.Key;
				const FName ParamName = Pair.Value;
				Out += TEXT("- Collection: ") + GetObjectPathSafe(Collection)
					+ TEXT("  Param: ") + ParamName.ToString() + TEXT("\n");
			}
		}

		Out += TEXT("\n### Material Functions (Calls)\n\n");
		if (Refs.Functions.Num() == 0)
		{
			Out += TEXT("- (none)\n");
		}
		else
		{
			for (const UMaterialFunctionInterface* Func : Refs.Functions)
			{
				Out += TEXT("- ") + GetObjectPathSafe(Func) + TEXT("\n");
			}
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## Textures & Samplers\n");
	{
		FCollectedRefs Refs;
		CollectRefsFromFunction(MaterialFunction, Refs);

		if (Refs.Textures.Num() == 0)
		{
			Out += TEXT("- (none)\n");
		}
		else
		{
			Out += TEXT("| Texture | ObjectPath | sRGB | Notes |\n");
			Out += TEXT("|---------|------------|:----:|-------|\n");
			for (const UTexture* Tex : Refs.Textures)
			{
				const bool bSRGB = Tex ? Tex->SRGB : false;
				Out += TEXT("| ") + (Tex ? Tex->GetName() : TEXT("<null>"))
					+ TEXT(" | ") + GetObjectPathSafe(Tex)
					+ TEXT(" | ") + (bSRGB ? TEXT("true") : TEXT("false"))
					+ TEXT(" | |\n");
			}
		}
	}
	Out += TEXT("\n");

	Out += TEXT("---\n");
	Out += TEXT("## Graph Index\n");
	EmitFunctionGraphIndex(MaterialFunction, Out);
	Out += TEXT("\n");

	return Out;
}


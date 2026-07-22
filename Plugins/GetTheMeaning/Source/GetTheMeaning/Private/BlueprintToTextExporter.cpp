// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintToTextExporter.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallDataTableFunction.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_SpawnActor.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_StructOperation.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_MacroInstance.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "EdGraphSchema_K2.h"
#include "Blueprint/BlueprintSupport.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/DataTable.h"
#include "Engine/Blueprint.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneTrack.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "BlueprintToTextExporter"

namespace
{
	using FJsonWriterRef = TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>>;

	struct FDataTableUsageInfo
	{
		FString NodeId;
		FString NodeTitle;
		FString DataTableName;
		FString DataTablePath;
		FString RowName;
		FString RowNameSource;
		FString RowStructName;
		FString RowStructPath;
	};

	struct FStructUsageInfo
	{
		FString Name;
		FString Path;
		TArray<FString> Fields;
	};

	struct FAssetReferenceInfo
	{
		FString Purpose;
		FString NodeId;
		FString NodeTitle;
		FString PinName;
		FString AssetName;
		FString AssetPath;
		FString AssetClass;
	};

	struct FCommentBoxContainedNodeInfo
	{
		FString NodeId;
		FString NodeTitle;
		FString NodeClass;
	};

	struct FCommentBoxInfo
	{
		FString GraphName;
		FString NodeId;
		FString Text;
		FString Details;
		FString MoveMode;
		FString Color;
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;
		int32 Depth = 0;
		int32 FontSize = 0;
		TArray<FCommentBoxContainedNodeInfo> ContainedNodes;
	};

	struct FWidgetEventBindingInfo
	{
		FString WidgetName;
		FString DelegateName;
		FString DelegateDisplayName;
		FString DelegateOwnerClass;
		FString FunctionName;
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
	};

	struct FWidgetSnapshotProperty
	{
		FString Name;
		FString Value;
		FString Category;
	};

	struct FWidgetCanvasSlotSnapshot
	{
		bool bHasCanvasSlot = false;
		FString SlotClass;
		FMargin Offsets;
		FAnchors Anchors;
		FVector2D Alignment = FVector2D::ZeroVector;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		bool bAutoSize = false;
		int32 ZOrder = 0;
	};

	struct FWidgetSnapshotInfo
	{
		FString Name;
		FString ClassName;
		FString Path;
		FString SlotClass;
		TArray<FWidgetSnapshotProperty> Properties;
		FWidgetCanvasSlotSnapshot CanvasSlot;
	};

	FString GetNodeId(const UEdGraphNode* Node);
	FString GetPinId(const UEdGraphPin* Pin);

	struct FLogicEntryPointInfo
	{
		FString GraphName;
		FString NodeId;
		FString Name;
		FString Type;
		FString Replication;
		bool bReliable = false;
	};

	struct FLogicControlFlowStep
	{
		FString GraphName;
		FString FromNodeId;
		FString FromNodeTitle;
		FString FromPinName;
		FString ToNodeId;
		FString ToNodeTitle;
		FString Kind;
		int32 Depth = 0;
	};

	struct FLogicBranchConditionInfo
	{
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
		FString Condition;
		FString TrueTargetNodeId;
		FString TrueTargetNodeTitle;
		FString FalseTargetNodeId;
		FString FalseTargetNodeTitle;
		TArray<FString> Sources;
		TArray<FString> Trace;
	};

	struct FLogicDataFlowInfo
	{
		FString GraphName;
		FString TargetNodeId;
		FString TargetNodeTitle;
		FString TargetPinName;
		FString SourceNodeId;
		FString SourceNodeTitle;
		FString SourcePinName;
		FString SourceExpression;
		FString ExpandedExpression;
		TArray<FString> Trace;
	};

	struct FLogicStateChangeInfo
	{
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
		FString Kind;
		FString Target;
		FString TargetScope;
		FString TargetScopeName;
		FString TargetGuid;
		FString ValueSource;
		TArray<FString> ValueTrace;
	};

	struct FLogicCallParameterInfo
	{
		FString Name;
		FString Value;
		FString DefaultValue;
		bool bLinked = false;
		TArray<FString> Trace;
	};

	struct FLogicCallInfo
	{
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
		FString FunctionName;
		FString OwnerClass;
		FString TargetSource;
		FString Replication;
		bool bReliable = false;
		bool bPure = false;
		bool bLatent = false;
		TArray<FString> ParameterSources;
		TArray<FLogicCallParameterInfo> Parameters;
		TMap<FString, TArray<FString>> ParameterTraces;
	};

	struct FLogicLoopInfo
	{
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
		FString MacroName;
		TArray<FString> DataSources;
		TMap<FString, TArray<FString>> DataTraces;
	};

	struct FLogicSummaryInfo
	{
		TArray<FLogicEntryPointInfo> EntryPoints;
		TArray<FLogicControlFlowStep> ControlFlows;
		TArray<FLogicBranchConditionInfo> BranchConditions;
		TArray<FLogicDataFlowInfo> DataFlows;
		TArray<FLogicStateChangeInfo> StateChanges;
		TArray<FLogicCallInfo> CallGraph;
		TArray<FLogicLoopInfo> Loops;
	};

	struct FRiskCallParameterTableEntry
	{
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
		FString FunctionName;
		FString OwnerClass;
		FString Replication;
		TArray<FLogicCallParameterInfo> Parameters;
	};

	struct FRiskWarningInfo
	{
		FString Severity;
		FString Category;
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
		FString Message;
		FString Details;
	};

	struct FRiskBranchRouteInfo
	{
		FString GraphName;
		FString NodeId;
		FString NodeTitle;
		FString Condition;
		FString TrueTarget;
		FString FalseTarget;
	};

	struct FRiskSummaryInfo
	{
		TArray<FRiskCallParameterTableEntry> CallParameterTable;
		TArray<FRiskBranchRouteInfo> BranchRoutes;
		TArray<FRiskWarningInfo> Warnings;
	};

	/** 是否为 exec 引脚（执行流） */
	bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	/** 获取节点的所有“执行输出”引脚（用于继续往下走），按引脚名排序以保证顺序稳定 */
	void GetExecOutputPins(UEdGraphNode* Node, TArray<UEdGraphPin*>& OutPins)
	{
		OutPins.Reset();
		if (!Node) return;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output) continue;
			if (!IsExecPin(Pin)) continue;
			OutPins.Add(Pin);
		}
		// 排序：Then 0, Then 1, ... True, False 等顺序稳定
		OutPins.Sort([](const UEdGraphPin& A, const UEdGraphPin& B) { return A.PinName.Compare(B.PinName) < 0; });
	}

	/** 获取通过 exec 连接到的下一个节点（可能有多个，如 Branch 的 True/False） */
	void GetNextNodesByExec(UEdGraphPin* ExecOutPin, TArray<UEdGraphNode*>& OutNodes)
	{
		OutNodes.Reset();
		if (!ExecOutPin) return;
		for (UEdGraphPin* Linked : ExecOutPin->LinkedTo)
		{
			if (!Linked) continue;
			UEdGraphNode* Next = Linked->GetOwningNode();
			if (Next) OutNodes.AddUnique(Next);
		}
	}

	/** 判断节点是否为“执行入口”（无 exec 输入或 exec 输入未连接） */
	bool IsExecutionRoot(UEdGraphNode* Node)
	{
		if (!Node) return false;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input) continue;
			if (!IsExecPin(Pin)) continue;
			// 有 exec 输入且已连接 -> 不是根
			if (Pin->LinkedTo.Num() > 0) return false;
		}
		// 没有 exec 输入，或 exec 输入未连接 -> 可作为根（如 Event、FunctionEntry）
		return true;
	}

	/** 判断是否为“事件”或“函数入口”等天然根节点 */
	bool IsNaturalExecutionRoot(UEdGraphNode* Node)
	{
		if (!Node) return false;
		if (Cast<UK2Node_Event>(Node)) return true;
		if (Cast<UK2Node_FunctionEntry>(Node)) return true;
		return false;
	}

	FString GetBlueprintObjectPathSafe(const UObject* Obj)
	{
		return Obj ? Obj->GetPathName() : FString();
	}

	const UObject* GetClassBlueprintAsset(const UClass* Class)
	{
		return Class ? Class->ClassGeneratedBy.Get() : nullptr;
	}

	FString GetClassBlueprintPathSafe(const UClass* Class)
	{
		if (const UObject* BlueprintAsset = GetClassBlueprintAsset(Class))
		{
			return GetBlueprintObjectPathSafe(BlueprintAsset);
		}
		return FString();
	}

	FString GetClassReadablePath(const UClass* Class)
	{
		if (!Class)
		{
			return FString();
		}

		const FString BlueprintPath = GetClassBlueprintPathSafe(Class);
		return BlueprintPath.IsEmpty() ? GetBlueprintObjectPathSafe(Class) : BlueprintPath;
	}

	FString GetBlueprintTypeString(EBlueprintType BlueprintType)
	{
		if (const UEnum* Enum = StaticEnum<EBlueprintType>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(BlueprintType));
		}
		return FString::Printf(TEXT("%d"), static_cast<int32>(BlueprintType));
	}

	FString GetPinDirectionString(EEdGraphPinDirection Direction)
	{
		switch (Direction)
		{
		case EGPD_Input:
			return TEXT("Input");
		case EGPD_Output:
			return TEXT("Output");
		default:
			return TEXT("Unknown");
		}
	}

	FString GetContainerTypeString(EPinContainerType ContainerType)
	{
		switch (ContainerType)
		{
		case EPinContainerType::Array:
			return TEXT("Array");
		case EPinContainerType::Set:
			return TEXT("Set");
		case EPinContainerType::Map:
			return TEXT("Map");
		case EPinContainerType::None:
		default:
			return TEXT("None");
		}
	}

	FString GetPinTypeString(const FEdGraphPinType& PinType)
	{
		FString BaseType = PinType.PinCategory.ToString();
		if (!PinType.PinSubCategory.IsNone())
		{
			BaseType += TEXT(":") + PinType.PinSubCategory.ToString();
		}
		if (UObject* SubCategoryObject = PinType.PinSubCategoryObject.Get())
		{
			BaseType += TEXT("<") + SubCategoryObject->GetName() + TEXT(">");
		}

		switch (PinType.ContainerType)
		{
		case EPinContainerType::Array:
			return TEXT("TArray<") + BaseType + TEXT(">");
		case EPinContainerType::Set:
			return TEXT("TSet<") + BaseType + TEXT(">");
		case EPinContainerType::Map:
		{
			FString ValueType = PinType.PinValueType.TerminalCategory.ToString();
			if (!PinType.PinValueType.TerminalSubCategory.IsNone())
			{
				ValueType += TEXT(":") + PinType.PinValueType.TerminalSubCategory.ToString();
			}
			if (UObject* ValueObject = PinType.PinValueType.TerminalSubCategoryObject.Get())
			{
				ValueType += TEXT("<") + ValueObject->GetName() + TEXT(">");
			}
			return TEXT("TMap<") + BaseType + TEXT(", ") + ValueType + TEXT(">");
		}
		case EPinContainerType::None:
		default:
			return BaseType;
		}
	}

	FString GetPinDefaultString(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}
		if (Pin->DefaultObject)
		{
			return GetBlueprintObjectPathSafe(Pin->DefaultObject);
		}
		if (!Pin->DefaultTextValue.IsEmpty())
		{
			return Pin->DefaultTextValue.ToString();
		}
		return Pin->DefaultValue;
	}

	FString FormatPinValueForReadable(const FString& Value)
	{
		return Value.IsEmpty() ? FString(TEXT("<empty>")) : Value;
	}

	struct FExecLinkInfo
	{
		FString NodeId;
		FString NodeTitle;
		FString PinId;
		FString PinName;
	};

	TArray<FExecLinkInfo> CollectIncomingExecLinks(const UEdGraphNode* Node)
	{
		TArray<FExecLinkInfo> Links;
		if (!Node)
		{
			return Links;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || !IsExecPin(Pin))
			{
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				const UEdGraphNode* SourceNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!LinkedPin || !SourceNode)
				{
					continue;
				}

				FExecLinkInfo Info;
				Info.NodeId = GetNodeId(SourceNode);
				Info.NodeTitle = SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				if (Info.NodeTitle.IsEmpty())
				{
					Info.NodeTitle = SourceNode->GetName();
				}
				Info.PinId = GetPinId(LinkedPin);
				Info.PinName = LinkedPin->PinName.ToString();
				Links.Add(MoveTemp(Info));
			}
		}

		Links.Sort([](const FExecLinkInfo& A, const FExecLinkInfo& B)
		{
			return A.NodeTitle == B.NodeTitle ? A.PinName < B.PinName : A.NodeTitle < B.NodeTitle;
		});
		return Links;
	}

	struct FVariableAccessScopeInfo
	{
		FString Scope;
		FString ScopeName;
		FString MemberGuid;
	};

	FVariableAccessScopeInfo GetVariableAccessScopeInfo(const UK2Node_Variable* VariableNode)
	{
		FVariableAccessScopeInfo Info;
		if (!VariableNode)
		{
			Info.Scope = TEXT("Unknown");
			return Info;
		}

		const FMemberReference& VariableReference = VariableNode->VariableReference;
		Info.ScopeName = VariableReference.GetMemberScopeName();
		const FGuid MemberGuid = VariableReference.GetMemberGuid();
		if (MemberGuid.IsValid())
		{
			Info.MemberGuid = MemberGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
		}

		if (VariableReference.IsLocalScope())
		{
			Info.Scope = TEXT("Local");
		}
		else if (VariableReference.IsSelfContext())
		{
			Info.Scope = TEXT("Member");
		}
		else if (VariableReference.GetMemberParentClass() || VariableReference.GetMemberParentPackage())
		{
			Info.Scope = TEXT("External");
			if (Info.ScopeName.IsEmpty())
			{
				if (const UClass* ParentClass = VariableReference.GetMemberParentClass())
				{
					Info.ScopeName = ParentClass->GetName();
				}
				else if (const UPackage* ParentPackage = VariableReference.GetMemberParentPackage())
				{
					Info.ScopeName = ParentPackage->GetName();
				}
			}
		}
		else
		{
			Info.Scope = TEXT("Unknown");
		}

		return Info;
	}

	FString FormatVariableScopeLabel(const FVariableAccessScopeInfo& Info)
	{
		if (Info.Scope.IsEmpty() || Info.Scope == TEXT("Unknown"))
		{
			return TEXT("[Unknown Scope]");
		}
		if (!Info.ScopeName.IsEmpty())
		{
			return FString::Printf(TEXT("[%s: %s]"), *Info.Scope, *Info.ScopeName);
		}
		return FString::Printf(TEXT("[%s]"), *Info.Scope);
	}

	FString GetNodeId(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return FString();
		}
		if (Node->NodeGuid.IsValid())
		{
			return Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
		}
		return Node->GetName();
	}

	FString GetPinId(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}
		if (Pin->PinId.IsValid())
		{
			return Pin->PinId.ToString(EGuidFormats::DigitsWithHyphensLower);
		}
		return Pin->PinName.ToString();
	}

	uint32 GetEventNetFlags(const UK2Node_Event* EventNode)
	{
		if (!EventNode)
		{
			return 0;
		}
		if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(EventNode))
		{
			return CustomEvent->GetNetFlags();
		}
		if (UFunction* Function = EventNode->FindEventSignatureFunction())
		{
			return Function->FunctionFlags & FUNC_NetFuncFlags;
		}
		return 0;
	}

	FString GetReplicationString(uint32 FunctionFlags)
	{
		if ((FunctionFlags & FUNC_Net) == 0)
		{
			return TEXT("Local");
		}
		if ((FunctionFlags & FUNC_NetServer) != 0)
		{
			return TEXT("RunOnServer");
		}
		if ((FunctionFlags & FUNC_NetClient) != 0)
		{
			return TEXT("RunOnOwningClient");
		}
		if ((FunctionFlags & FUNC_NetMulticast) != 0)
		{
			return TEXT("Multicast");
		}
		return TEXT("Replicated");
	}

	void WriteObjectPathField(FJsonWriterRef Writer, const TCHAR* FieldName, const UObject* Obj)
	{
		Writer->WriteValue(FieldName, GetBlueprintObjectPathSafe(Obj));
	}

	void WritePinTypeObject(FJsonWriterRef Writer, const TCHAR* FieldName, const FEdGraphPinType& PinType)
	{
		Writer->WriteObjectStart(FieldName);
		Writer->WriteValue(TEXT("display"), GetPinTypeString(PinType));
		Writer->WriteValue(TEXT("category"), PinType.PinCategory.ToString());
		Writer->WriteValue(TEXT("subCategory"), PinType.PinSubCategory.ToString());
		WriteObjectPathField(Writer, TEXT("subCategoryObject"), PinType.PinSubCategoryObject.Get());
		Writer->WriteValue(TEXT("container"), GetContainerTypeString(PinType.ContainerType));
		Writer->WriteValue(TEXT("isReference"), !!PinType.bIsReference);
		Writer->WriteValue(TEXT("isConst"), !!PinType.bIsConst);
		if (PinType.ContainerType == EPinContainerType::Map)
		{
			Writer->WriteObjectStart(TEXT("mapValueType"));
			Writer->WriteValue(TEXT("category"), PinType.PinValueType.TerminalCategory.ToString());
			Writer->WriteValue(TEXT("subCategory"), PinType.PinValueType.TerminalSubCategory.ToString());
			WriteObjectPathField(Writer, TEXT("subCategoryObject"), PinType.PinValueType.TerminalSubCategoryObject.Get());
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectEnd();
	}

	struct FVariableNetworkInfo
	{
		bool bReplicated = false;
		bool bRepNotify = false;
		FString Replication;
		FString RepNotify;
		FString Condition;
		int32 ConditionValue = 0;
	};

	struct FVariableFlagInfo
	{
		bool bPrivate = false;
		bool bEditable = false;
		bool bInstanceEditable = false;
		bool bExposeOnSpawn = false;
	};

	bool IsTruthyMetadataValue(const FString& Value)
	{
		return Value.IsEmpty()
			|| Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Value.Equals(TEXT("1"), ESearchCase::IgnoreCase);
	}

	bool HasTruthyVariableMetadata(const FBPVariableDescription& Var, const FName MetadataKey)
	{
		return Var.HasMetaData(MetadataKey) && IsTruthyMetadataValue(Var.GetMetaData(MetadataKey));
	}

	FVariableFlagInfo GetVariableFlagInfo(const FBPVariableDescription& Var)
	{
		FVariableFlagInfo Info;
		Info.bPrivate = (Var.PropertyFlags & CPF_NativeAccessSpecifierPrivate) != 0
			|| HasTruthyVariableMetadata(Var, FBlueprintMetadata::MD_Private);
		Info.bEditable = (Var.PropertyFlags & CPF_Edit) != 0
			&& (Var.PropertyFlags & CPF_EditConst) == 0;
		Info.bInstanceEditable = Info.bEditable
			&& (Var.PropertyFlags & CPF_DisableEditOnInstance) == 0;
		Info.bExposeOnSpawn = (Var.PropertyFlags & CPF_ExposeOnSpawn) != 0
			|| HasTruthyVariableMetadata(Var, FBlueprintMetadata::MD_ExposeOnSpawn);
		return Info;
	}

	FString GetLifetimeConditionString(ELifetimeCondition Condition)
	{
		if (const UEnum* Enum = StaticEnum<ELifetimeCondition>())
		{
			FString ValueName = Enum->GetNameStringByValue(static_cast<int64>(Condition));
			ValueName.RemoveFromStart(TEXT("COND_"));
			return ValueName;
		}
		return FString::Printf(TEXT("%d"), static_cast<int32>(Condition));
	}

	FVariableNetworkInfo GetVariableNetworkInfo(const FBPVariableDescription& Var)
	{
		FVariableNetworkInfo Info;
		Info.bReplicated = (Var.PropertyFlags & CPF_Net) != 0;
		Info.bRepNotify = (Var.PropertyFlags & CPF_RepNotify) != 0 || !Var.RepNotifyFunc.IsNone();
		Info.RepNotify = Var.RepNotifyFunc.IsNone() ? FString() : Var.RepNotifyFunc.ToString();
		Info.ConditionValue = static_cast<int32>(Var.ReplicationCondition.GetValue());
		Info.Condition = GetLifetimeConditionString(Var.ReplicationCondition.GetValue());

		if (!Info.bReplicated && !Info.bRepNotify)
		{
			Info.Replication = TEXT("NotReplicated");
		}
		else if (Info.bRepNotify)
		{
			Info.Replication = TEXT("RepNotify");
			Info.bReplicated = true;
		}
		else
		{
			Info.Replication = TEXT("Replicated");
		}
		return Info;
	}

	void WriteVariableNetworkObject(FJsonWriterRef Writer, const FBPVariableDescription& Var)
	{
		const FVariableNetworkInfo Info = GetVariableNetworkInfo(Var);
		Writer->WriteObjectStart(TEXT("network"));
		Writer->WriteValue(TEXT("replication"), Info.Replication);
		Writer->WriteValue(TEXT("replicated"), Info.bReplicated);
		Writer->WriteValue(TEXT("repNotify"), Info.RepNotify);
		Writer->WriteValue(TEXT("usesRepNotify"), Info.bRepNotify);
		Writer->WriteValue(TEXT("condition"), Info.Condition);
		Writer->WriteValue(TEXT("conditionValue"), Info.ConditionValue);
		Writer->WriteObjectEnd();
	}

	void WriteVariableDescriptionObject(FJsonWriterRef Writer, const FBPVariableDescription& Var, const FString& Scope, const FString& ScopeName = FString())
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Var.VarName.ToString());
		Writer->WriteValue(TEXT("scope"), Scope);
		Writer->WriteValue(TEXT("scopeName"), ScopeName);
		Writer->WriteValue(TEXT("guid"), Var.VarGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
		Writer->WriteValue(TEXT("friendlyName"), Var.FriendlyName);
		Writer->WriteValue(TEXT("category"), Var.Category.ToString());
		WritePinTypeObject(Writer, TEXT("type"), Var.VarType);
		Writer->WriteValue(TEXT("defaultValue"), Var.DefaultValue);
		Writer->WriteValue(TEXT("propertyFlags"), FString::Printf(TEXT("0x%016llx"), static_cast<unsigned long long>(Var.PropertyFlags)));
		Writer->WriteValue(TEXT("repNotify"), Var.RepNotifyFunc.ToString());
		Writer->WriteValue(TEXT("replicationCondition"), static_cast<int32>(Var.ReplicationCondition.GetValue()));
		WriteVariableNetworkObject(Writer, Var);
		const FVariableFlagInfo Flags = GetVariableFlagInfo(Var);
		Writer->WriteObjectStart(TEXT("flags"));
		Writer->WriteValue(TEXT("private"), Flags.bPrivate);
		Writer->WriteValue(TEXT("editable"), Flags.bEditable);
		Writer->WriteValue(TEXT("instanceEditable"), Flags.bInstanceEditable);
		Writer->WriteValue(TEXT("exposeOnSpawn"), Flags.bExposeOnSpawn);
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
	}

	FString FormatVariableDescriptionText(const FBPVariableDescription& Var)
	{
		FString Line = FString::Printf(TEXT("- %s: %s"), *Var.VarName.ToString(), *GetPinTypeString(Var.VarType));
		if (!Var.DefaultValue.IsEmpty())
		{
			Line += TEXT(" = ") + Var.DefaultValue;
		}
		if (!Var.RepNotifyFunc.IsNone())
		{
			Line += TEXT("  RepNotify: ") + Var.RepNotifyFunc.ToString();
		}
		const FVariableNetworkInfo Network = GetVariableNetworkInfo(Var);
		Line += TEXT("  Replication: ") + Network.Replication;
		const FVariableFlagInfo Flags = GetVariableFlagInfo(Var);
		Line += FString::Printf(
			TEXT("  Private: %s  Editable: %s  InstanceEditable: %s  ExposeOnSpawn: %s"),
			Flags.bPrivate ? TEXT("true") : TEXT("false"),
			Flags.bEditable ? TEXT("true") : TEXT("false"),
			Flags.bInstanceEditable ? TEXT("true") : TEXT("false"),
			Flags.bExposeOnSpawn ? TEXT("true") : TEXT("false")
		);
		if (!Network.Condition.IsEmpty() && Network.Condition != TEXT("None"))
		{
			Line += TEXT("  Condition: ") + Network.Condition;
		}
		return Line;
	}

	UK2Node_FunctionEntry* FindFunctionEntryNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				return EntryNode;
			}
		}
		return nullptr;
	}

	TArray<UEdGraph*> GetAllBlueprintGraphs(UBlueprint* Blueprint)
	{
		TArray<UEdGraph*> Graphs;
		if (!Blueprint)
		{
			return Graphs;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				Graphs.Add(Graph);
			}
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph)
			{
				Graphs.Add(Graph);
			}
		}
		for (const FBPInterfaceDescription& InterfaceDesc : Blueprint->ImplementedInterfaces)
		{
			for (UEdGraph* Graph : InterfaceDesc.Graphs)
			{
				if (Graph)
				{
					Graphs.AddUnique(Graph);
				}
			}
		}
		return Graphs;
	}

	void WritePinObject(FJsonWriterRef Writer, const UEdGraphPin* Pin, bool bIncludeLinks)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("id"), GetPinId(Pin));
		Writer->WriteValue(TEXT("name"), Pin ? Pin->PinName.ToString() : FString());
		Writer->WriteValue(TEXT("direction"), Pin ? GetPinDirectionString(Pin->Direction) : FString());
		Writer->WriteValue(TEXT("isExec"), IsExecPin(Pin));
		if (Pin)
		{
			WritePinTypeObject(Writer, TEXT("type"), Pin->PinType);
			Writer->WriteValue(TEXT("defaultValue"), GetPinDefaultString(Pin));
		}
		if (bIncludeLinks)
		{
			Writer->WriteArrayStart(TEXT("links"));
			if (Pin)
			{
				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin) continue;
					const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
					Writer->WriteObjectStart();
					Writer->WriteValue(TEXT("nodeId"), GetNodeId(LinkedNode));
					Writer->WriteValue(TEXT("nodeTitle"), LinkedNode ? LinkedNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString());
					Writer->WriteValue(TEXT("pinId"), GetPinId(LinkedPin));
					Writer->WriteValue(TEXT("pinName"), LinkedPin->PinName.ToString());
					Writer->WriteObjectEnd();
				}
			}
			Writer->WriteArrayEnd();
		}
		Writer->WriteObjectEnd();
	}

	FString GetCommentMoveModeString(ECommentBoxMode::Type MoveMode)
	{
		switch (MoveMode)
		{
		case ECommentBoxMode::GroupMovement:
			return TEXT("GroupMovement");
		case ECommentBoxMode::NoGroupMovement:
			return TEXT("NoGroupMovement");
		default:
			return FString::Printf(TEXT("%d"), static_cast<int32>(MoveMode));
		}
	}

	FString GetLinearColorString(const FLinearColor& Color)
	{
		return FString::Printf(TEXT("R=%.3f G=%.3f B=%.3f A=%.3f"), Color.R, Color.G, Color.B, Color.A);
	}

	bool IsNodeInsideCommentBox(const UEdGraphNode_Comment* CommentNode, const UEdGraphNode* CandidateNode)
	{
		if (!CommentNode || !CandidateNode || CandidateNode == CommentNode || Cast<UEdGraphNode_Comment>(CandidateNode))
		{
			return false;
		}

		const float CommentLeft = static_cast<float>(CommentNode->NodePosX);
		const float CommentTop = static_cast<float>(CommentNode->NodePosY);
		const float CommentRight = CommentLeft + static_cast<float>(CommentNode->NodeWidth);
		const float CommentBottom = CommentTop + static_cast<float>(CommentNode->NodeHeight);
		const float NodeCenterX = static_cast<float>(CandidateNode->NodePosX) + static_cast<float>(CandidateNode->NodeWidth) * 0.5f;
		const float NodeCenterY = static_cast<float>(CandidateNode->NodePosY) + static_cast<float>(CandidateNode->NodeHeight) * 0.5f;

		return NodeCenterX >= CommentLeft
			&& NodeCenterX <= CommentRight
			&& NodeCenterY >= CommentTop
			&& NodeCenterY <= CommentBottom;
	}

	void AddContainedCommentNode(FCommentBoxInfo& Info, const UEdGraphNode* Node)
	{
		if (!Node || Cast<UEdGraphNode_Comment>(Node))
		{
			return;
		}

		const FString NodeId = GetNodeId(Node);
		for (const FCommentBoxContainedNodeInfo& Existing : Info.ContainedNodes)
		{
			if (Existing.NodeId == NodeId)
			{
				return;
			}
		}

		FCommentBoxContainedNodeInfo Contained;
		Contained.NodeId = NodeId;
		Contained.NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		Contained.NodeClass = Node->GetClass() ? Node->GetClass()->GetName() : FString();
		Info.ContainedNodes.Add(MoveTemp(Contained));
	}

	FCommentBoxInfo MakeCommentBoxInfo(UEdGraph* Graph, UEdGraphNode_Comment* CommentNode)
	{
		FCommentBoxInfo Info;
		if (!CommentNode)
		{
			return Info;
		}

		Info.GraphName = Graph ? Graph->GetName() : FString();
		Info.NodeId = GetNodeId(CommentNode);
		Info.Text = CommentNode->NodeComment.IsEmpty() ? CommentNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : CommentNode->NodeComment;
		Info.Details = CommentNode->NodeDetails.ToString();
		Info.MoveMode = GetCommentMoveModeString(CommentNode->MoveMode.GetValue());
		Info.Color = GetLinearColorString(CommentNode->CommentColor);
		Info.X = CommentNode->NodePosX;
		Info.Y = CommentNode->NodePosY;
		Info.Width = CommentNode->NodeWidth;
		Info.Height = CommentNode->NodeHeight;
		Info.Depth = CommentNode->CommentDepth;
		Info.FontSize = CommentNode->GetFontSize();

		for (UObject* Object : CommentNode->GetNodesUnderComment())
		{
			AddContainedCommentNode(Info, Cast<UEdGraphNode>(Object));
		}

		if (Graph)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (IsNodeInsideCommentBox(CommentNode, Node))
				{
					AddContainedCommentNode(Info, Node);
				}
			}
		}

		Info.ContainedNodes.Sort([](const FCommentBoxContainedNodeInfo& A, const FCommentBoxContainedNodeInfo& B)
		{
			return A.NodeTitle < B.NodeTitle;
		});
		return Info;
	}

	TArray<FCommentBoxInfo> CollectCommentBoxes(UBlueprint* Blueprint)
	{
		TArray<FCommentBoxInfo> CommentBoxes;
		if (!Blueprint)
		{
			return CommentBoxes;
		}

		for (UEdGraph* Graph : GetAllBlueprintGraphs(Blueprint))
		{
			if (!Graph)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
				{
					CommentBoxes.Add(MakeCommentBoxInfo(Graph, CommentNode));
				}
			}
		}

		CommentBoxes.Sort([](const FCommentBoxInfo& A, const FCommentBoxInfo& B)
		{
			if (A.GraphName != B.GraphName)
			{
				return A.GraphName < B.GraphName;
			}
			return A.Text < B.Text;
		});
		return CommentBoxes;
	}

	FString ExportCommentBoxesToText(UBlueprint* Blueprint)
	{
		const TArray<FCommentBoxInfo> CommentBoxes = CollectCommentBoxes(Blueprint);
		if (CommentBoxes.Num() == 0)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("---\n\n## Comment Boxes\n\n");
		for (const FCommentBoxInfo& Comment : CommentBoxes)
		{
			Out += FString::Printf(TEXT("- [%s] %s (NodeId: %s)\n"), *Comment.GraphName, *Comment.Text, *Comment.NodeId);
			if (!Comment.Details.IsEmpty())
			{
				Out += TEXT("  - Details: ") + Comment.Details + TEXT("\n");
			}
			Out += FString::Printf(
				TEXT("  - Bounds: X=%d Y=%d W=%d H=%d MoveMode=%s Depth=%d\n"),
				Comment.X,
				Comment.Y,
				Comment.Width,
				Comment.Height,
				*Comment.MoveMode,
				Comment.Depth);
			if (Comment.ContainedNodes.Num() == 0)
			{
				Out += TEXT("  - Contains: (none detected)\n");
			}
			else
			{
				Out += FString::Printf(TEXT("  - Contains %d node(s):\n"), Comment.ContainedNodes.Num());
				const int32 MaxShownNodes = 20;
				for (int32 Index = 0; Index < Comment.ContainedNodes.Num() && Index < MaxShownNodes; ++Index)
				{
					const FCommentBoxContainedNodeInfo& Node = Comment.ContainedNodes[Index];
					Out += FString::Printf(TEXT("    - %s [%s] NodeId=%s\n"), *Node.NodeTitle, *Node.NodeClass, *Node.NodeId);
				}
				if (Comment.ContainedNodes.Num() > MaxShownNodes)
				{
					Out += FString::Printf(TEXT("    - ... %d more\n"), Comment.ContainedNodes.Num() - MaxShownNodes);
				}
			}
		}
		Out += TEXT("\n");
		return Out;
	}

	void WriteCommentBoxesJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		const TArray<FCommentBoxInfo> CommentBoxes = CollectCommentBoxes(Blueprint);
		Writer->WriteArrayStart(TEXT("commentBoxes"));
		for (const FCommentBoxInfo& Comment : CommentBoxes)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Comment.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Comment.NodeId);
			Writer->WriteValue(TEXT("text"), Comment.Text);
			Writer->WriteValue(TEXT("details"), Comment.Details);
			Writer->WriteValue(TEXT("moveMode"), Comment.MoveMode);
			Writer->WriteValue(TEXT("color"), Comment.Color);
			Writer->WriteValue(TEXT("depth"), Comment.Depth);
			Writer->WriteValue(TEXT("fontSize"), Comment.FontSize);
			Writer->WriteObjectStart(TEXT("bounds"));
			Writer->WriteValue(TEXT("x"), Comment.X);
			Writer->WriteValue(TEXT("y"), Comment.Y);
			Writer->WriteValue(TEXT("width"), Comment.Width);
			Writer->WriteValue(TEXT("height"), Comment.Height);
			Writer->WriteObjectEnd();
			Writer->WriteArrayStart(TEXT("containedNodes"));
			for (const FCommentBoxContainedNodeInfo& Node : Comment.ContainedNodes)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("nodeId"), Node.NodeId);
				Writer->WriteValue(TEXT("title"), Node.NodeTitle);
				Writer->WriteValue(TEXT("class"), Node.NodeClass);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
	}

	void WriteSignatureFromPins(FJsonWriterRef Writer, UEdGraphNode* Node)
	{
		Writer->WriteArrayStart(TEXT("inputs"));
		if (Node)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin)) continue;
				WritePinObject(Writer, Pin, false);
			}
		}
		Writer->WriteArrayEnd();
	}

	void WriteFunctionSignatureFromGraph(FJsonWriterRef Writer, UEdGraph* Graph)
	{
		UK2Node_FunctionEntry* EntryNode = nullptr;
		UK2Node_FunctionResult* ResultNode = nullptr;
		if (Graph)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!EntryNode)
				{
					EntryNode = Cast<UK2Node_FunctionEntry>(Node);
				}
				if (!ResultNode)
				{
					ResultNode = Cast<UK2Node_FunctionResult>(Node);
				}
			}
		}

		Writer->WriteArrayStart(TEXT("inputs"));
		if (EntryNode)
		{
			for (UEdGraphPin* Pin : EntryNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin)) continue;
				WritePinObject(Writer, Pin, false);
			}
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("outputs"));
		if (ResultNode)
		{
			for (UEdGraphPin* Pin : ResultNode->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin)) continue;
				WritePinObject(Writer, Pin, false);
			}
		}
		Writer->WriteArrayEnd();
	}

	void WriteNodeObject(FJsonWriterRef Writer, UEdGraphNode* Node)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("id"), GetNodeId(Node));
		Writer->WriteValue(TEXT("class"), Node ? Node->GetClass()->GetName() : FString());
		Writer->WriteValue(TEXT("name"), Node ? Node->GetName() : FString());
		Writer->WriteValue(TEXT("title"), Node ? Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString());
		Writer->WriteValue(TEXT("summary"), FBlueprintToTextExporter::ExportNodeToText(Node));
		if (Node)
		{
			Writer->WriteObjectStart(TEXT("position"));
			Writer->WriteValue(TEXT("x"), Node->NodePosX);
			Writer->WriteValue(TEXT("y"), Node->NodePosY);
			Writer->WriteObjectEnd();
		}

		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			if (UFunction* Function = CallNode->GetTargetFunction())
			{
				Writer->WriteObjectStart(TEXT("call"));
				Writer->WriteValue(TEXT("function"), Function->GetName());
				Writer->WriteValue(TEXT("owner"), Function->GetOwnerClass() ? Function->GetOwnerClass()->GetName() : FString());
				WriteObjectPathField(Writer, TEXT("path"), Function);
				Writer->WriteObjectEnd();
			}
		}

		if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
		{
			const FVariableAccessScopeInfo ScopeInfo = GetVariableAccessScopeInfo(VariableNode);
			Writer->WriteObjectStart(TEXT("variableAccess"));
			Writer->WriteValue(TEXT("name"), VariableNode->GetVarNameString());
			Writer->WriteValue(TEXT("mode"), Cast<UK2Node_VariableSet>(Node) ? TEXT("Set") : TEXT("Get"));
			Writer->WriteValue(TEXT("scope"), ScopeInfo.Scope);
			Writer->WriteValue(TEXT("scopeName"), ScopeInfo.ScopeName);
			Writer->WriteValue(TEXT("memberGuid"), ScopeInfo.MemberGuid);
			if (FProperty* Property = VariableNode->GetPropertyForVariable())
			{
				Writer->WriteValue(TEXT("propertyClass"), Property->GetClass()->GetName());
				Writer->WriteValue(TEXT("cppType"), Property->GetCPPType());
			}
			Writer->WriteObjectEnd();
		}

		if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
		{
			Writer->WriteObjectStart(TEXT("commentBox"));
			Writer->WriteValue(TEXT("text"), CommentNode->NodeComment);
			Writer->WriteValue(TEXT("details"), CommentNode->NodeDetails.ToString());
			Writer->WriteValue(TEXT("moveMode"), GetCommentMoveModeString(CommentNode->MoveMode.GetValue()));
			Writer->WriteValue(TEXT("color"), GetLinearColorString(CommentNode->CommentColor));
			Writer->WriteValue(TEXT("depth"), CommentNode->CommentDepth);
			Writer->WriteValue(TEXT("fontSize"), CommentNode->GetFontSize());
			Writer->WriteObjectStart(TEXT("bounds"));
			Writer->WriteValue(TEXT("x"), CommentNode->NodePosX);
			Writer->WriteValue(TEXT("y"), CommentNode->NodePosY);
			Writer->WriteValue(TEXT("width"), CommentNode->NodeWidth);
			Writer->WriteValue(TEXT("height"), CommentNode->NodeHeight);
			Writer->WriteObjectEnd();
			Writer->WriteObjectEnd();
		}

		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			const uint32 NetFlags = GetEventNetFlags(EventNode);
			Writer->WriteObjectStart(TEXT("event"));
			Writer->WriteValue(TEXT("name"), EventNode->GetFunctionName().ToString());
			Writer->WriteValue(TEXT("replication"), GetReplicationString(NetFlags));
			Writer->WriteValue(TEXT("reliable"), (NetFlags & FUNC_NetReliable) != 0);
			WriteSignatureFromPins(Writer, EventNode);
			Writer->WriteObjectEnd();
		}

		Writer->WriteArrayStart(TEXT("incomingExecLinks"));
		for (const FExecLinkInfo& Link : CollectIncomingExecLinks(Node))
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("nodeId"), Link.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Link.NodeTitle);
			Writer->WriteValue(TEXT("pinId"), Link.PinId);
			Writer->WriteValue(TEXT("pinName"), Link.PinName);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("pins"));
		if (Node)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				WritePinObject(Writer, Pin, true);
			}
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}

	void WriteGraphObject(FJsonWriterRef Writer, UEdGraph* Graph, const FString& Kind)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Graph ? Graph->GetName() : FString());
		Writer->WriteValue(TEXT("kind"), Kind);

		if (Kind == TEXT("Function"))
		{
			WriteFunctionSignatureFromGraph(Writer, Graph);
		}

		Writer->WriteArrayStart(TEXT("nodes"));
		if (Graph)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					WriteNodeObject(Writer, Node);
				}
			}
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("links"));
		if (Graph)
		{
			TSet<FString> SeenLinks;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin || Pin->Direction != EGPD_Output) continue;
					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (!LinkedPin) continue;
						UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
						const FString Key = GetPinId(Pin) + TEXT("->") + GetPinId(LinkedPin);
						if (SeenLinks.Contains(Key)) continue;
						SeenLinks.Add(Key);

						Writer->WriteObjectStart();
						Writer->WriteValue(TEXT("fromNodeId"), GetNodeId(Node));
						Writer->WriteValue(TEXT("fromPinId"), GetPinId(Pin));
						Writer->WriteValue(TEXT("fromPinName"), Pin->PinName.ToString());
						Writer->WriteValue(TEXT("toNodeId"), GetNodeId(LinkedNode));
						Writer->WriteValue(TEXT("toPinId"), GetPinId(LinkedPin));
						Writer->WriteValue(TEXT("toPinName"), LinkedPin->PinName.ToString());
						Writer->WriteValue(TEXT("isExec"), IsExecPin(Pin));
						Writer->WriteObjectEnd();
					}
				}
			}
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}

	FString GetGraphNameSafe(const UEdGraph* Graph)
	{
		return Graph ? Graph->GetName() : FString();
	}

	FString GetNodeTitleSafe(const UEdGraphNode* Node)
	{
		return Node ? Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString();
	}

	FString GetPinValueOrSourceLabel(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}

		if (Pin->LinkedTo.Num() > 0)
		{
			TArray<FString> Sources;
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				const UEdGraphNode* SourceNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!LinkedPin || !SourceNode)
				{
					continue;
				}
				Sources.Add(GetNodeTitleSafe(SourceNode) + TEXT(".") + LinkedPin->PinName.ToString());
			}
			return FString::Join(Sources, TEXT(", "));
		}

		return GetPinDefaultString(Pin);
	}

	FString GetVariableNodeName(const UEdGraphNode* Node)
	{
		const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
		if (!VariableNode)
		{
			return FString();
		}
		return VariableNode->GetVarNameString();
	}

	FString GetCallNodeFunctionName(const UEdGraphNode* Node)
	{
		const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
		if (!CallNode)
		{
			return FString();
		}
		const UFunction* Function = CallNode->GetTargetFunction();
		return Function ? Function->GetName() : GetNodeTitleSafe(Node);
	}

	FString DescribeSourceNodeOutput(const UEdGraphNode* Node, const UEdGraphPin* OutputPin)
	{
		if (!Node)
		{
			return FString();
		}

		if (const UK2Node_VariableGet* VariableGet = Cast<UK2Node_VariableGet>(Node))
		{
			return TEXT("Get ") + VariableGet->GetVarNameString();
		}
		if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			return TEXT("Event ") + EventNode->GetFunctionName().ToString() + TEXT(".") + (OutputPin ? OutputPin->PinName.ToString() : FString());
		}
		if (Cast<UK2Node_FunctionEntry>(Node))
		{
			return TEXT("FunctionInput ") + (OutputPin ? OutputPin->PinName.ToString() : FString());
		}
		if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			return TEXT("Call ") + GetCallNodeFunctionName(CallNode) + TEXT(".") + (OutputPin ? OutputPin->PinName.ToString() : FString());
		}
		if (const UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
		{
			const UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
			return TEXT("Macro ") + (MacroGraph ? MacroGraph->GetName() : GetNodeTitleSafe(Node)) + TEXT(".") + (OutputPin ? OutputPin->PinName.ToString() : FString());
		}
		return GetNodeTitleSafe(Node) + TEXT(".") + (OutputPin ? OutputPin->PinName.ToString() : FString());
	}

	FString TracePinSourceRecursive(const UEdGraphPin* Pin, int32 Depth, TSet<FString>& Visited, TArray<FString>& OutTrace)
	{
		if (!Pin)
		{
			return FString();
		}

		const FString PinKey = GetPinId(Pin);
		if (Visited.Contains(PinKey))
		{
			return TEXT("<cycle>");
		}
		Visited.Add(PinKey);

		if (Pin->LinkedTo.Num() == 0)
		{
			const FString DefaultValue = GetPinDefaultString(Pin);
			if (!DefaultValue.IsEmpty())
			{
				OutTrace.Add(Pin->PinName.ToString() + TEXT(" = ") + DefaultValue);
			}
			return DefaultValue;
		}

		TArray<FString> Expressions;
		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			const UEdGraphNode* SourceNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (!LinkedPin || !SourceNode)
			{
				continue;
			}

			const FString SourceLabel = DescribeSourceNodeOutput(SourceNode, LinkedPin);
			OutTrace.Add(Pin->PinName.ToString() + TEXT(" <- ") + SourceLabel);

			if (Depth <= 0)
			{
				Expressions.Add(SourceLabel);
				continue;
			}

			TArray<FString> SourceInputs;
			for (const UEdGraphPin* SourceInputPin : SourceNode->Pins)
			{
				if (!SourceInputPin || SourceInputPin->Direction != EGPD_Input || IsExecPin(SourceInputPin))
				{
					continue;
				}

				TArray<FString> ChildTrace;
				const FString ChildExpression = TracePinSourceRecursive(SourceInputPin, Depth - 1, Visited, ChildTrace);
				if (!ChildExpression.IsEmpty())
				{
					SourceInputs.Add(SourceInputPin->PinName.ToString() + TEXT("=") + ChildExpression);
					for (const FString& TraceLine : ChildTrace)
					{
						OutTrace.Add(TEXT("  ") + TraceLine);
					}
				}
			}

			if (SourceInputs.Num() > 0)
			{
				Expressions.Add(SourceLabel + TEXT("(") + FString::Join(SourceInputs, TEXT(", ")) + TEXT(")"));
			}
			else
			{
				Expressions.Add(SourceLabel);
			}
		}

		return FString::Join(Expressions, TEXT(", "));
	}

	FString TracePinSource(const UEdGraphPin* Pin, TArray<FString>& OutTrace, int32 MaxDepth = 4)
	{
		TSet<FString> Visited;
		return TracePinSourceRecursive(Pin, MaxDepth, Visited, OutTrace);
	}

	FString GetPinValueOrExpandedSourceLabel(const UEdGraphPin* Pin, TArray<FString>& OutTrace)
	{
		const FString Expanded = TracePinSource(Pin, OutTrace);
		if (!Expanded.IsEmpty())
		{
			return Expanded;
		}
		return GetPinValueOrSourceLabel(Pin);
	}

	FString GetFirstInputSourceLabel(UEdGraphNode* Node, const TArray<FName>& PreferredPinNames)
	{
		if (!Node)
		{
			return FString();
		}

		for (const FName& PinName : PreferredPinNames)
		{
			if (const UEdGraphPin* Pin = Node->FindPin(PinName, EGPD_Input))
			{
				const FString Value = GetPinValueOrSourceLabel(Pin);
				if (!Value.IsEmpty())
				{
					return Value;
				}
			}
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin))
			{
				continue;
			}
			const FString Value = GetPinValueOrSourceLabel(Pin);
			if (!Value.IsEmpty())
			{
				return Pin->PinName.ToString() + TEXT(" = ") + Value;
			}
		}

		return FString();
	}

	TArray<FString> GetInputSourceDescriptions(UEdGraphNode* Node, bool bOnlyConnected)
	{
		TArray<FString> Sources;
		if (!Node)
		{
			return Sources;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin))
			{
				continue;
			}
			if (bOnlyConnected && Pin->LinkedTo.Num() == 0)
			{
				continue;
			}

			const FString Value = GetPinValueOrSourceLabel(Pin);
			if (!Value.IsEmpty())
			{
				Sources.Add(Pin->PinName.ToString() + TEXT(" = ") + Value);
			}
		}
		return Sources;
	}

	FString GetControlFlowKind(const UEdGraphNode* FromNode, const UEdGraphPin* FromPin)
	{
		if (!FromNode || !FromPin)
		{
			return FString();
		}

		if (Cast<UK2Node_IfThenElse>(FromNode))
		{
			return FromPin->PinName.ToString() == TEXT("True") ? TEXT("BranchTrue")
				: FromPin->PinName.ToString() == TEXT("False") ? TEXT("BranchFalse")
				: TEXT("Branch");
		}
		if (Cast<UK2Node_ExecutionSequence>(FromNode))
		{
			return TEXT("Sequence.") + FromPin->PinName.ToString();
		}
		return FromPin->PinName.ToString().IsEmpty() ? TEXT("Then") : FromPin->PinName.ToString();
	}

	void CollectLogicControlFlowFromRoot(UEdGraphNode* Root, const FString& GraphName, TArray<FLogicControlFlowStep>& OutSteps)
	{
		if (!Root)
		{
			return;
		}

		TArray<UEdGraphNode*> Stack;
		TArray<int32> DepthStack;
		TSet<UEdGraphNode*> Visited;
		Stack.Push(Root);
		DepthStack.Push(0);

		while (Stack.Num() > 0)
		{
			UEdGraphNode* Node = Stack.Pop();
			const int32 Depth = DepthStack.Pop();
			if (!Node || Visited.Contains(Node))
			{
				continue;
			}
			Visited.Add(Node);

			TArray<UEdGraphPin*> ExecOuts;
			GetExecOutputPins(Node, ExecOuts);
			for (int32 Index = ExecOuts.Num() - 1; Index >= 0; --Index)
			{
				UEdGraphPin* ExecPin = ExecOuts[Index];
				TArray<UEdGraphNode*> NextNodes;
				GetNextNodesByExec(ExecPin, NextNodes);
				for (int32 NextIndex = NextNodes.Num() - 1; NextIndex >= 0; --NextIndex)
				{
					UEdGraphNode* NextNode = NextNodes[NextIndex];
					if (!NextNode)
					{
						continue;
					}

					FLogicControlFlowStep Step;
					Step.GraphName = GraphName;
					Step.FromNodeId = GetNodeId(Node);
					Step.FromNodeTitle = GetNodeTitleSafe(Node);
					Step.FromPinName = ExecPin ? ExecPin->PinName.ToString() : FString();
					Step.ToNodeId = GetNodeId(NextNode);
					Step.ToNodeTitle = GetNodeTitleSafe(NextNode);
					Step.Kind = GetControlFlowKind(Node, ExecPin);
					Step.Depth = Depth;
					OutSteps.Add(MoveTemp(Step));

					Stack.Push(NextNode);
					DepthStack.Push(Depth + 1);
				}
			}
		}
	}

	void FindLogicEntryRoots(UEdGraph* Graph, TArray<UEdGraphNode*>& OutRoots)
	{
		if (!Graph)
		{
			return;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && IsNaturalExecutionRoot(Node))
			{
				OutRoots.Add(Node);
			}
		}
		if (OutRoots.Num() > 0)
		{
			return;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && IsExecutionRoot(Node))
			{
				OutRoots.AddUnique(Node);
			}
		}
	}

	void AddLogicEntryPoint(UEdGraphNode* Node, const FString& GraphName, FLogicSummaryInfo& OutSummary)
	{
		if (!Node)
		{
			return;
		}

		FLogicEntryPointInfo Entry;
		Entry.GraphName = GraphName;
		Entry.NodeId = GetNodeId(Node);
		Entry.Name = GetNodeTitleSafe(Node);
		Entry.Type = Node->GetClass() ? Node->GetClass()->GetName() : FString();

		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			const uint32 NetFlags = GetEventNetFlags(EventNode);
			Entry.Name = EventNode->GetFunctionName().ToString();
			Entry.Type = Cast<UK2Node_CustomEvent>(EventNode) ? TEXT("CustomEvent") : TEXT("Event");
			Entry.Replication = GetReplicationString(NetFlags);
			Entry.bReliable = (NetFlags & FUNC_NetReliable) != 0;
		}
		else if (Cast<UK2Node_FunctionEntry>(Node))
		{
			Entry.Type = TEXT("Function");
			Entry.Replication = TEXT("Local");
		}
		else
		{
			Entry.Replication = TEXT("Local");
		}

		OutSummary.EntryPoints.Add(MoveTemp(Entry));
	}

	void AddLogicDataFlowsForNode(UEdGraphNode* Node, const FString& GraphName, FLogicSummaryInfo& OutSummary)
	{
		if (!Node)
		{
			return;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin) || Pin->LinkedTo.Num() == 0)
			{
				continue;
			}

			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				const UEdGraphNode* SourceNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!LinkedPin || !SourceNode)
				{
					continue;
				}

				FLogicDataFlowInfo Flow;
				Flow.GraphName = GraphName;
				Flow.TargetNodeId = GetNodeId(Node);
				Flow.TargetNodeTitle = GetNodeTitleSafe(Node);
				Flow.TargetPinName = Pin->PinName.ToString();
				Flow.SourceNodeId = GetNodeId(SourceNode);
				Flow.SourceNodeTitle = GetNodeTitleSafe(SourceNode);
				Flow.SourcePinName = LinkedPin->PinName.ToString();
				Flow.SourceExpression = GetNodeTitleSafe(SourceNode) + TEXT(".") + LinkedPin->PinName.ToString();
				Flow.ExpandedExpression = GetPinValueOrExpandedSourceLabel(Pin, Flow.Trace);
				OutSummary.DataFlows.Add(MoveTemp(Flow));
			}
		}
	}

	void AddLogicBranchCondition(UEdGraphNode* Node, const FString& GraphName, FLogicSummaryInfo& OutSummary)
	{
		if (!Cast<UK2Node_IfThenElse>(Node))
		{
			return;
		}

		const UEdGraphPin* ConditionPin = Node->FindPin(UEdGraphSchema_K2::PN_Condition, EGPD_Input);
		FLogicBranchConditionInfo Branch;
		Branch.GraphName = GraphName;
		Branch.NodeId = GetNodeId(Node);
		Branch.NodeTitle = GetNodeTitleSafe(Node);
		Branch.Condition = GetPinValueOrExpandedSourceLabel(ConditionPin, Branch.Trace);

		const auto SetBranchTarget = [](const UEdGraphPin* ExecPin, FString& OutNodeId, FString& OutNodeTitle)
		{
			if (!ExecPin || ExecPin->LinkedTo.Num() == 0)
			{
				return;
			}
			const UEdGraphPin* LinkedPin = ExecPin->LinkedTo[0];
			const UEdGraphNode* TargetNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (!TargetNode)
			{
				return;
			}
			OutNodeId = GetNodeId(TargetNode);
			OutNodeTitle = GetNodeTitleSafe(TargetNode);
		};

		SetBranchTarget(Node->FindPin(FName(TEXT("True")), EGPD_Output), Branch.TrueTargetNodeId, Branch.TrueTargetNodeTitle);
		SetBranchTarget(Node->FindPin(FName(TEXT("False")), EGPD_Output), Branch.FalseTargetNodeId, Branch.FalseTargetNodeTitle);

		if (ConditionPin)
		{
			for (const UEdGraphPin* LinkedPin : ConditionPin->LinkedTo)
			{
				const UEdGraphNode* SourceNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!LinkedPin || !SourceNode)
				{
					continue;
				}
				Branch.Sources.Add(GetNodeTitleSafe(SourceNode) + TEXT(".") + LinkedPin->PinName.ToString());
			}
		}

		OutSummary.BranchConditions.Add(MoveTemp(Branch));
	}

	void AddLogicStateChange(UEdGraphNode* Node, const FString& GraphName, FLogicSummaryInfo& OutSummary)
	{
		if (!Node)
		{
			return;
		}

		if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
		{
			const FVariableAccessScopeInfo ScopeInfo = GetVariableAccessScopeInfo(SetNode);
			FLogicStateChangeInfo Change;
			Change.GraphName = GraphName;
			Change.NodeId = GetNodeId(Node);
			Change.NodeTitle = GetNodeTitleSafe(Node);
			Change.Kind = TEXT("SetVariable");
			Change.Target = SetNode->GetVarNameString();
			Change.TargetScope = ScopeInfo.Scope;
			Change.TargetScopeName = ScopeInfo.ScopeName;
			Change.TargetGuid = ScopeInfo.MemberGuid;
			const UEdGraphPin* ValuePin = Node->FindPin(FName(*Change.Target), EGPD_Input);
			if (!ValuePin)
			{
				ValuePin = Node->FindPin(TEXT("Value"), EGPD_Input);
			}
			Change.ValueSource = GetPinValueOrExpandedSourceLabel(ValuePin, Change.ValueTrace);
			if (Change.ValueSource.IsEmpty())
			{
				Change.ValueSource = GetFirstInputSourceLabel(Node, { FName(*Change.Target), TEXT("Value") });
			}
			OutSummary.StateChanges.Add(MoveTemp(Change));
			return;
		}

		if (UK2Node_AssignmentStatement* AssignmentNode = Cast<UK2Node_AssignmentStatement>(Node))
		{
			FLogicStateChangeInfo Change;
			Change.GraphName = GraphName;
			Change.NodeId = GetNodeId(Node);
			Change.NodeTitle = GetNodeTitleSafe(Node);
			Change.Kind = TEXT("Assignment");
			Change.Target = GetPinValueOrSourceLabel(AssignmentNode->GetVariablePin());
			Change.ValueSource = GetPinValueOrExpandedSourceLabel(AssignmentNode->GetValuePin(), Change.ValueTrace);
			OutSummary.StateChanges.Add(MoveTemp(Change));
			return;
		}

		UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
		UFunction* Function = CallNode ? CallNode->GetTargetFunction() : nullptr;
		if (!CallNode || !Function)
		{
			return;
		}

		const FString FunctionName = Function->GetName();
		const bool bLooksLikeMutation =
			FunctionName.StartsWith(TEXT("Set"))
			|| FunctionName.StartsWith(TEXT("Add"))
			|| FunctionName.StartsWith(TEXT("Remove"))
			|| FunctionName.StartsWith(TEXT("Clear"))
			|| FunctionName.StartsWith(TEXT("Save"))
			|| FunctionName.Contains(TEXT("SaveGame"))
			|| FunctionName.Contains(TEXT("Write"))
			|| FunctionName.Contains(TEXT("Update"));
		if (!bLooksLikeMutation)
		{
			return;
		}

		FLogicStateChangeInfo Change;
		Change.GraphName = GraphName;
		Change.NodeId = GetNodeId(Node);
		Change.NodeTitle = GetNodeTitleSafe(Node);
		Change.Kind = TEXT("MutationCall");
		Change.Target = FunctionName;
		TArray<FString> ExpandedInputs;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin))
			{
				continue;
			}
			TArray<FString> Trace;
			const FString Value = GetPinValueOrExpandedSourceLabel(Pin, Trace);
			if (!Value.IsEmpty())
			{
				ExpandedInputs.Add(Pin->PinName.ToString() + TEXT(" = ") + Value);
				for (const FString& TraceLine : Trace)
				{
					Change.ValueTrace.Add(Pin->PinName.ToString() + TEXT(": ") + TraceLine);
				}
			}
		}
		Change.ValueSource = FString::Join(ExpandedInputs, TEXT("; "));
		OutSummary.StateChanges.Add(MoveTemp(Change));
	}

	void AddLogicCall(UEdGraphNode* Node, const FString& GraphName, FLogicSummaryInfo& OutSummary)
	{
		UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
		if (!CallNode)
		{
			return;
		}

		UFunction* Function = CallNode->GetTargetFunction();
		FLogicCallInfo Call;
		Call.GraphName = GraphName;
		Call.NodeId = GetNodeId(Node);
		Call.NodeTitle = GetNodeTitleSafe(Node);
		Call.FunctionName = Function ? Function->GetName() : FString(TEXT("?"));
		Call.OwnerClass = Function && Function->GetOwnerClass() ? Function->GetOwnerClass()->GetName() : FString();
		Call.Replication = Function ? GetReplicationString(Function->FunctionFlags & FUNC_NetFuncFlags) : FString();
		Call.bReliable = Function ? ((Function->FunctionFlags & FUNC_NetReliable) != 0) : false;
		Call.bPure = CallNode->IsNodePure();
		Call.bLatent = Function ? Function->HasMetaData(FBlueprintMetadata::MD_Latent) : false;

		if (const UEdGraphPin* SelfPin = Node->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input))
		{
			Call.TargetSource = GetPinValueOrSourceLabel(SelfPin);
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin))
			{
				continue;
			}

			const FString PinName = Pin->PinName.ToString();
			TArray<FString> Trace;
			const FString Value = GetPinValueOrExpandedSourceLabel(Pin, Trace);
			const FString DefaultValue = GetPinDefaultString(Pin);

			FLogicCallParameterInfo Parameter;
			Parameter.Name = PinName;
			Parameter.Value = Value;
			Parameter.DefaultValue = DefaultValue;
			Parameter.bLinked = Pin->LinkedTo.Num() > 0;
			Parameter.Trace = Trace;
			Call.Parameters.Add(MoveTemp(Parameter));

			if (!Value.IsEmpty() || !DefaultValue.IsEmpty() || Pin->LinkedTo.Num() == 0)
			{
				const FString ReadableValue = Value.IsEmpty() ? DefaultValue : Value;
				Call.ParameterSources.Add(PinName + TEXT(" = ") + FormatPinValueForReadable(ReadableValue));
				if (Trace.Num() > 0)
				{
					Call.ParameterTraces.Add(PinName, Trace);
				}
			}
		}
		OutSummary.CallGraph.Add(MoveTemp(Call));
	}

	void AddLogicLoop(UEdGraphNode* Node, const FString& GraphName, FLogicSummaryInfo& OutSummary)
	{
		UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node);
		if (!MacroNode)
		{
			return;
		}

		const FString Title = GetNodeTitleSafe(Node);
		const bool bLooksLikeLoop =
			Title.Contains(TEXT("ForLoop"))
			|| Title.Contains(TEXT("For Each"))
			|| Title.Contains(TEXT("ForEach"))
			|| Title.Contains(TEXT("While"))
			|| Title.Contains(TEXT("Loop"));
		if (!bLooksLikeLoop)
		{
			return;
		}

		FLogicLoopInfo Loop;
		Loop.GraphName = GraphName;
		Loop.NodeId = GetNodeId(Node);
		Loop.NodeTitle = Title;
		Loop.MacroName = MacroNode->GetMacroGraph() ? MacroNode->GetMacroGraph()->GetName() : Title;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin))
			{
				continue;
			}
			TArray<FString> Trace;
			const FString Value = GetPinValueOrExpandedSourceLabel(Pin, Trace);
			if (!Value.IsEmpty())
			{
				const FString PinName = Pin->PinName.ToString();
				Loop.DataSources.Add(PinName + TEXT(" = ") + Value);
				if (Trace.Num() > 0)
				{
					Loop.DataTraces.Add(PinName, MoveTemp(Trace));
				}
			}
		}
		OutSummary.Loops.Add(MoveTemp(Loop));
	}

	void CollectLogicSummaryFromGraph(UEdGraph* Graph, FLogicSummaryInfo& OutSummary)
	{
		if (!Graph)
		{
			return;
		}

		const FString GraphName = GetGraphNameSafe(Graph);
		TArray<UEdGraphNode*> Roots;
		FindLogicEntryRoots(Graph, Roots);
		for (UEdGraphNode* Root : Roots)
		{
			AddLogicEntryPoint(Root, GraphName, OutSummary);
			CollectLogicControlFlowFromRoot(Root, GraphName, OutSummary.ControlFlows);
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			AddLogicBranchCondition(Node, GraphName, OutSummary);
			AddLogicDataFlowsForNode(Node, GraphName, OutSummary);
			AddLogicStateChange(Node, GraphName, OutSummary);
			AddLogicCall(Node, GraphName, OutSummary);
			AddLogicLoop(Node, GraphName, OutSummary);
		}
	}

	FLogicSummaryInfo CollectLogicSummary(UBlueprint* Blueprint)
	{
		FLogicSummaryInfo Summary;
		if (!Blueprint)
		{
			return Summary;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			CollectLogicSummaryFromGraph(Graph, Summary);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			CollectLogicSummaryFromGraph(Graph, Summary);
		}
		return Summary;
	}

	FString ExportLogicSummaryToText(UBlueprint* Blueprint)
	{
		const FLogicSummaryInfo Summary = CollectLogicSummary(Blueprint);
		if (Summary.EntryPoints.Num() == 0
			&& Summary.ControlFlows.Num() == 0
			&& Summary.BranchConditions.Num() == 0
			&& Summary.StateChanges.Num() == 0
			&& Summary.CallGraph.Num() == 0
			&& Summary.Loops.Num() == 0)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("---\n\n## Logic Flow Summary\n\n");

		Out += TEXT("### Entry Points\n\n");
		if (Summary.EntryPoints.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FLogicEntryPointInfo& Entry : Summary.EntryPoints)
			{
				Out += FString::Printf(
					TEXT("- %s: %s [%s] Replication=%s Reliable=%s NodeId=%s\n"),
					*Entry.GraphName,
					*Entry.Name,
					*Entry.Type,
					*Entry.Replication,
					Entry.bReliable ? TEXT("true") : TEXT("false"),
					*Entry.NodeId);
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Control Flow\n\n");
		if (Summary.ControlFlows.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FLogicControlFlowStep& Step : Summary.ControlFlows)
			{
				FString Indent;
				for (int32 Index = 0; Index < Step.Depth; ++Index)
				{
					Indent += TEXT("  ");
				}
				Out += FString::Printf(
					TEXT("%s- [%s] %s --%s--> %s\n"),
					*Indent,
					*Step.GraphName,
					*Step.FromNodeTitle,
					*Step.Kind,
					*Step.ToNodeTitle);
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Branch Conditions\n\n");
		if (Summary.BranchConditions.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FLogicBranchConditionInfo& Branch : Summary.BranchConditions)
			{
				Out += FString::Printf(TEXT("- [%s] %s\n"), *Branch.GraphName, *Branch.NodeTitle);
				Out += FString::Printf(TEXT("  - Condition: %s\n"), *Branch.Condition);
				Out += FString::Printf(TEXT("  - True -> %s\n"), *(Branch.TrueTargetNodeTitle.IsEmpty() ? FString(TEXT("(unconnected)")) : Branch.TrueTargetNodeTitle));
				Out += FString::Printf(TEXT("  - False -> %s\n"), *(Branch.FalseTargetNodeTitle.IsEmpty() ? FString(TEXT("(unconnected)")) : Branch.FalseTargetNodeTitle));
				for (const FString& Source : Branch.Sources)
				{
					Out += TEXT("  - Source: ") + Source + TEXT("\n");
				}
				for (const FString& TraceLine : Branch.Trace)
				{
					Out += TEXT("  - Trace: ") + TraceLine + TEXT("\n");
				}
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### State Changes\n\n");
		if (Summary.StateChanges.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FLogicStateChangeInfo& Change : Summary.StateChanges)
			{
				FString TargetLabel = Change.Target;
				if (!Change.TargetScope.IsEmpty())
				{
					FVariableAccessScopeInfo ScopeInfo;
					ScopeInfo.Scope = Change.TargetScope;
					ScopeInfo.ScopeName = Change.TargetScopeName;
					TargetLabel += TEXT(" ") + FormatVariableScopeLabel(ScopeInfo);
				}
				Out += FString::Printf(
					TEXT("- [%s] %s: %s <- %s (NodeId: %s)\n"),
					*Change.GraphName,
					*Change.Kind,
					*TargetLabel,
					*Change.ValueSource,
					*Change.NodeId);
				for (const FString& TraceLine : Change.ValueTrace)
				{
					Out += TEXT("  - Trace: ") + TraceLine + TEXT("\n");
				}
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Call Graph\n\n");
		if (Summary.CallGraph.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FLogicCallInfo& Call : Summary.CallGraph)
			{
				Out += FString::Printf(
					TEXT("- [%s] %s.%s Replication=%s Pure=%s Latent=%s Reliable=%s\n"),
					*Call.GraphName,
					*Call.OwnerClass,
					*Call.FunctionName,
					*Call.Replication,
					Call.bPure ? TEXT("true") : TEXT("false"),
					Call.bLatent ? TEXT("true") : TEXT("false"),
					Call.bReliable ? TEXT("true") : TEXT("false"));
				if (!Call.TargetSource.IsEmpty())
				{
					Out += TEXT("  - Target: ") + Call.TargetSource + TEXT("\n");
				}
				for (const FLogicCallParameterInfo& Parameter : Call.Parameters)
				{
					Out += FString::Printf(
						TEXT("  - Param: %s = %s [%s defaultValue=%s]\n"),
						*Parameter.Name,
						*FormatPinValueForReadable(Parameter.Value),
						Parameter.bLinked ? TEXT("linked") : TEXT("unlinked"),
						*FormatPinValueForReadable(Parameter.DefaultValue));
				}
				for (const FString& ParameterSource : Call.ParameterSources)
				{
					Out += TEXT("  - ParamSummary: ") + ParameterSource + TEXT("\n");
				}
				for (const TPair<FString, TArray<FString>>& TracePair : Call.ParameterTraces)
				{
					for (const FString& TraceLine : TracePair.Value)
					{
						Out += TEXT("  - Trace ") + TracePair.Key + TEXT(": ") + TraceLine + TEXT("\n");
					}
				}
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Loops\n\n");
		if (Summary.Loops.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FLogicLoopInfo& Loop : Summary.Loops)
			{
				Out += FString::Printf(TEXT("- [%s] %s Macro=%s\n"), *Loop.GraphName, *Loop.NodeTitle, *Loop.MacroName);
				for (const FString& DataSource : Loop.DataSources)
				{
					Out += TEXT("  - Input: ") + DataSource + TEXT("\n");
				}
				for (const TPair<FString, TArray<FString>>& TracePair : Loop.DataTraces)
				{
					for (const FString& TraceLine : TracePair.Value)
					{
						Out += TEXT("  - Trace ") + TracePair.Key + TEXT(": ") + TraceLine + TEXT("\n");
					}
				}
			}
			Out += TEXT("\n");
		}

		return Out;
	}

	void WriteStringArray(FJsonWriterRef Writer, const TCHAR* FieldName, const TArray<FString>& Values)
	{
		Writer->WriteArrayStart(FieldName);
		for (const FString& Value : Values)
		{
			Writer->WriteValue(Value);
		}
		Writer->WriteArrayEnd();
	}

	void WriteLogicSummaryJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		const FLogicSummaryInfo Summary = CollectLogicSummary(Blueprint);
		Writer->WriteObjectStart(TEXT("logicSummary"));

		Writer->WriteArrayStart(TEXT("entryPoints"));
		for (const FLogicEntryPointInfo& Entry : Summary.EntryPoints)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Entry.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Entry.NodeId);
			Writer->WriteValue(TEXT("name"), Entry.Name);
			Writer->WriteValue(TEXT("type"), Entry.Type);
			Writer->WriteValue(TEXT("replication"), Entry.Replication);
			Writer->WriteValue(TEXT("reliable"), Entry.bReliable);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("controlFlows"));
		for (const FLogicControlFlowStep& Step : Summary.ControlFlows)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Step.GraphName);
			Writer->WriteValue(TEXT("fromNodeId"), Step.FromNodeId);
			Writer->WriteValue(TEXT("fromNodeTitle"), Step.FromNodeTitle);
			Writer->WriteValue(TEXT("fromPinName"), Step.FromPinName);
			Writer->WriteValue(TEXT("toNodeId"), Step.ToNodeId);
			Writer->WriteValue(TEXT("toNodeTitle"), Step.ToNodeTitle);
			Writer->WriteValue(TEXT("kind"), Step.Kind);
			Writer->WriteValue(TEXT("depth"), Step.Depth);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("branchConditions"));
		for (const FLogicBranchConditionInfo& Branch : Summary.BranchConditions)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Branch.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Branch.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Branch.NodeTitle);
			Writer->WriteValue(TEXT("condition"), Branch.Condition);
			Writer->WriteValue(TEXT("trueTargetNodeId"), Branch.TrueTargetNodeId);
			Writer->WriteValue(TEXT("trueTargetNodeTitle"), Branch.TrueTargetNodeTitle);
			Writer->WriteValue(TEXT("falseTargetNodeId"), Branch.FalseTargetNodeId);
			Writer->WriteValue(TEXT("falseTargetNodeTitle"), Branch.FalseTargetNodeTitle);
			WriteStringArray(Writer, TEXT("sources"), Branch.Sources);
			WriteStringArray(Writer, TEXT("trace"), Branch.Trace);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("dataFlows"));
		for (const FLogicDataFlowInfo& Flow : Summary.DataFlows)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Flow.GraphName);
			Writer->WriteValue(TEXT("targetNodeId"), Flow.TargetNodeId);
			Writer->WriteValue(TEXT("targetNodeTitle"), Flow.TargetNodeTitle);
			Writer->WriteValue(TEXT("targetPinName"), Flow.TargetPinName);
			Writer->WriteValue(TEXT("sourceNodeId"), Flow.SourceNodeId);
			Writer->WriteValue(TEXT("sourceNodeTitle"), Flow.SourceNodeTitle);
			Writer->WriteValue(TEXT("sourcePinName"), Flow.SourcePinName);
			Writer->WriteValue(TEXT("sourceExpression"), Flow.SourceExpression);
			Writer->WriteValue(TEXT("expandedExpression"), Flow.ExpandedExpression);
			WriteStringArray(Writer, TEXT("trace"), Flow.Trace);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("stateChanges"));
		for (const FLogicStateChangeInfo& Change : Summary.StateChanges)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Change.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Change.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Change.NodeTitle);
			Writer->WriteValue(TEXT("kind"), Change.Kind);
			Writer->WriteValue(TEXT("target"), Change.Target);
			Writer->WriteValue(TEXT("targetScope"), Change.TargetScope);
			Writer->WriteValue(TEXT("targetScopeName"), Change.TargetScopeName);
			Writer->WriteValue(TEXT("targetGuid"), Change.TargetGuid);
			Writer->WriteValue(TEXT("valueSource"), Change.ValueSource);
			WriteStringArray(Writer, TEXT("valueTrace"), Change.ValueTrace);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("callGraph"));
		for (const FLogicCallInfo& Call : Summary.CallGraph)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Call.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Call.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Call.NodeTitle);
			Writer->WriteValue(TEXT("functionName"), Call.FunctionName);
			Writer->WriteValue(TEXT("ownerClass"), Call.OwnerClass);
			Writer->WriteValue(TEXT("targetSource"), Call.TargetSource);
			Writer->WriteValue(TEXT("replication"), Call.Replication);
			Writer->WriteValue(TEXT("reliable"), Call.bReliable);
			Writer->WriteValue(TEXT("pure"), Call.bPure);
			Writer->WriteValue(TEXT("latent"), Call.bLatent);
			WriteStringArray(Writer, TEXT("parameterSources"), Call.ParameterSources);
			Writer->WriteArrayStart(TEXT("parameters"));
			for (const FLogicCallParameterInfo& Parameter : Call.Parameters)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), Parameter.Name);
				Writer->WriteValue(TEXT("value"), Parameter.Value);
				Writer->WriteValue(TEXT("defaultValue"), Parameter.DefaultValue);
				Writer->WriteValue(TEXT("linked"), Parameter.bLinked);
				WriteStringArray(Writer, TEXT("trace"), Parameter.Trace);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectStart(TEXT("parameterTraces"));
			for (const TPair<FString, TArray<FString>>& TracePair : Call.ParameterTraces)
			{
				WriteStringArray(Writer, *TracePair.Key, TracePair.Value);
			}
			Writer->WriteObjectEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("loops"));
		for (const FLogicLoopInfo& Loop : Summary.Loops)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Loop.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Loop.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Loop.NodeTitle);
			Writer->WriteValue(TEXT("macroName"), Loop.MacroName);
			WriteStringArray(Writer, TEXT("dataSources"), Loop.DataSources);
			Writer->WriteObjectStart(TEXT("dataTraces"));
			for (const TPair<FString, TArray<FString>>& TracePair : Loop.DataTraces)
			{
				WriteStringArray(Writer, *TracePair.Key, TracePair.Value);
			}
			Writer->WriteObjectEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteObjectEnd();
	}

	void AddRiskWarning(FRiskSummaryInfo& Summary, const FString& Severity, const FString& Category, const FString& GraphName, UEdGraphNode* Node, const FString& Message, const FString& Details = FString())
	{
		FRiskWarningInfo Warning;
		Warning.Severity = Severity;
		Warning.Category = Category;
		Warning.GraphName = GraphName;
		Warning.NodeId = GetNodeId(Node);
		Warning.NodeTitle = GetNodeTitleSafe(Node);
		Warning.Message = Message;
		Warning.Details = Details;
		Summary.Warnings.Add(MoveTemp(Warning));
	}

	bool IsCreateWidgetCall(const UK2Node_CallFunction* CallNode)
	{
		const UFunction* Function = CallNode ? CallNode->GetTargetFunction() : nullptr;
		const FString FunctionName = Function ? Function->GetName() : FString();
		return FunctionName.Contains(TEXT("CreateWidget"));
	}

	bool IsAddToViewportCall(const UK2Node_CallFunction* CallNode)
	{
		const UFunction* Function = CallNode ? CallNode->GetTargetFunction() : nullptr;
		return Function && Function->GetName().Contains(TEXT("AddToViewport"));
	}

	const UEdGraphPin* FindFirstOutputPinByName(UEdGraphNode* Node, const FName& PinName)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName == PinName)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	bool IsOutputPinUsedByExecDecisionOrStorage(const UEdGraphPin* Pin)
	{
		if (!Pin || Pin->LinkedTo.Num() == 0)
		{
			return false;
		}

		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			const UEdGraphNode* TargetNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (!LinkedPin || !TargetNode)
			{
				continue;
			}
			if (Cast<UK2Node_IfThenElse>(TargetNode) || Cast<UK2Node_VariableSet>(TargetNode) || Cast<UK2Node_AssignmentStatement>(TargetNode))
			{
				return true;
			}
		}
		return false;
	}

	bool IsBoolLikeOutputPin(const UEdGraphPin* Pin)
	{
		return Pin
			&& Pin->Direction == EGPD_Output
			&& !IsExecPin(Pin)
			&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean;
	}

	bool IsLoopWithBreakNode(const UK2Node_MacroInstance* MacroNode)
	{
		if (!MacroNode)
		{
			return false;
		}
		const UEdGraph* MacroGraph = MacroNode->GetMacroGraph();
		const FString MacroName = MacroGraph ? MacroGraph->GetName() : GetNodeTitleSafe(MacroNode);
		return MacroName.Contains(TEXT("WithBreak")) || GetNodeTitleSafe(MacroNode).Contains(TEXT("With Break"));
	}

	const UEdGraphPin* FindBreakExecPin(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || !IsExecPin(Pin))
			{
				continue;
			}
			const FString PinName = Pin->PinName.ToString();
			if (PinName.Contains(TEXT("Break")))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	void CollectRiskSummaryFromGraph(UEdGraph* Graph, FRiskSummaryInfo& Risk)
	{
		if (!Graph)
		{
			return;
		}

		const FString GraphName = GetGraphNameSafe(Graph);
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				for (const UEdGraphPin* Pin : Node->Pins)
				{
					if (IsBoolLikeOutputPin(Pin) && !IsOutputPinUsedByExecDecisionOrStorage(Pin))
					{
						AddRiskWarning(
							Risk,
							TEXT("Warning"),
							TEXT("UnusedReturnValue"),
							GraphName,
							Node,
							TEXT("Bool output pin is not connected to a Branch, variable set, or assignment."),
							TEXT("Pin: ") + Pin->PinName.ToString());
					}
				}

				if (IsCreateWidgetCall(CallNode))
				{
					const UEdGraphPin* ReturnPin = FindFirstOutputPinByName(Node, UEdGraphSchema_K2::PN_ReturnValue);
					if (!ReturnPin || ReturnPin->LinkedTo.Num() == 0)
					{
						AddRiskWarning(Risk, TEXT("Hint"), TEXT("Widget"), GraphName, Node, TEXT("CreateWidget return value is not used or cached."));
					}

					const UEdGraphPin* OwningPlayerPin = Node->FindPin(TEXT("OwningPlayer"), EGPD_Input);
					if (OwningPlayerPin && OwningPlayerPin->LinkedTo.Num() == 0 && GetPinDefaultString(OwningPlayerPin).IsEmpty())
					{
						AddRiskWarning(Risk, TEXT("Hint"), TEXT("Widget"), GraphName, Node, TEXT("CreateWidget OwningPlayer is unlinked."));
					}
				}
				else if (IsAddToViewportCall(CallNode))
				{
					const UEdGraphPin* SelfPin = Node->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
					if (SelfPin && SelfPin->LinkedTo.Num() > 0)
					{
						for (const UEdGraphPin* LinkedPin : SelfPin->LinkedTo)
						{
							const UEdGraphNode* SourceNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
							if (SourceNode && Cast<UK2Node_CallFunction>(SourceNode) && IsCreateWidgetCall(Cast<UK2Node_CallFunction>(SourceNode)))
							{
								AddRiskWarning(
									Risk,
									TEXT("Hint"),
									TEXT("Widget"),
									GraphName,
									Node,
									TEXT("AddToViewport is fed directly from CreateWidget."),
									TEXT("Consider caching the widget reference or guarding against repeated AddToViewport calls."));
							}
						}
					}
				}
			}

			if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
			{
				if (IsLoopWithBreakNode(MacroNode))
				{
					const UEdGraphPin* BreakPin = FindBreakExecPin(Node);
					if (BreakPin && BreakPin->LinkedTo.Num() == 0)
					{
						AddRiskWarning(
							Risk,
							TEXT("Warning"),
							TEXT("LoopWithBreak"),
							GraphName,
							Node,
							TEXT("Loop With Break has an unconnected Break pin."),
							TEXT("This loop will not stop early through the Break input."));
					}
				}
			}
		}
	}

	FRiskSummaryInfo CollectRiskSummary(UBlueprint* Blueprint)
	{
		FRiskSummaryInfo Risk;
		if (!Blueprint)
		{
			return Risk;
		}

		const FLogicSummaryInfo LogicSummary = CollectLogicSummary(Blueprint);
		for (const FLogicCallInfo& Call : LogicSummary.CallGraph)
		{
			FRiskCallParameterTableEntry Entry;
			Entry.GraphName = Call.GraphName;
			Entry.NodeId = Call.NodeId;
			Entry.NodeTitle = Call.NodeTitle;
			Entry.FunctionName = Call.FunctionName;
			Entry.OwnerClass = Call.OwnerClass;
			Entry.Replication = Call.Replication;
			Entry.Parameters = Call.Parameters;
			Risk.CallParameterTable.Add(MoveTemp(Entry));
		}

		for (const FLogicBranchConditionInfo& Branch : LogicSummary.BranchConditions)
		{
			FRiskBranchRouteInfo Route;
			Route.GraphName = Branch.GraphName;
			Route.NodeId = Branch.NodeId;
			Route.NodeTitle = Branch.NodeTitle;
			Route.Condition = Branch.Condition;
			Route.TrueTarget = Branch.TrueTargetNodeTitle;
			Route.FalseTarget = Branch.FalseTargetNodeTitle;
			Risk.BranchRoutes.Add(MoveTemp(Route));
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			CollectRiskSummaryFromGraph(Graph, Risk);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			CollectRiskSummaryFromGraph(Graph, Risk);
		}
		return Risk;
	}

	FString ExportRiskSummaryToText(UBlueprint* Blueprint)
	{
		const FRiskSummaryInfo Risk = CollectRiskSummary(Blueprint);
		if (Risk.CallParameterTable.Num() == 0 && Risk.BranchRoutes.Num() == 0 && Risk.Warnings.Num() == 0)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("---\n\n## Risk Summary\n\n");

		Out += TEXT("### Function/RPC Call Parameters\n\n");
		if (Risk.CallParameterTable.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FRiskCallParameterTableEntry& Entry : Risk.CallParameterTable)
			{
				Out += FString::Printf(TEXT("- [%s] %s.%s Replication=%s NodeId=%s\n"), *Entry.GraphName, *Entry.OwnerClass, *Entry.FunctionName, *Entry.Replication, *Entry.NodeId);
				for (const FLogicCallParameterInfo& Parameter : Entry.Parameters)
				{
					Out += FString::Printf(
						TEXT("  - %s: linked=%s value=%s defaultValue=%s\n"),
						*Parameter.Name,
						Parameter.bLinked ? TEXT("true") : TEXT("false"),
						*FormatPinValueForReadable(Parameter.Value),
						*FormatPinValueForReadable(Parameter.DefaultValue));
				}
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Branch Routes\n\n");
		if (Risk.BranchRoutes.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FRiskBranchRouteInfo& Route : Risk.BranchRoutes)
			{
				Out += FString::Printf(TEXT("- [%s] %s Condition=%s\n"), *Route.GraphName, *Route.NodeTitle, *FormatPinValueForReadable(Route.Condition));
				Out += FString::Printf(TEXT("  - True -> %s\n"), *(Route.TrueTarget.IsEmpty() ? FString(TEXT("(unconnected)")) : Route.TrueTarget));
				Out += FString::Printf(TEXT("  - False -> %s\n"), *(Route.FalseTarget.IsEmpty() ? FString(TEXT("(unconnected)")) : Route.FalseTarget));
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Warnings\n\n");
		if (Risk.Warnings.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FRiskWarningInfo& Warning : Risk.Warnings)
			{
				Out += FString::Printf(TEXT("- [%s/%s] [%s] %s: %s (NodeId: %s)\n"), *Warning.Severity, *Warning.Category, *Warning.GraphName, *Warning.NodeTitle, *Warning.Message, *Warning.NodeId);
				if (!Warning.Details.IsEmpty())
				{
					Out += TEXT("  - Details: ") + Warning.Details + TEXT("\n");
				}
			}
			Out += TEXT("\n");
		}

		return Out;
	}

	void WriteRiskSummaryJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		const FRiskSummaryInfo Risk = CollectRiskSummary(Blueprint);
		Writer->WriteObjectStart(TEXT("riskSummary"));

		Writer->WriteArrayStart(TEXT("callParameterTable"));
		for (const FRiskCallParameterTableEntry& Entry : Risk.CallParameterTable)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Entry.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Entry.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Entry.NodeTitle);
			Writer->WriteValue(TEXT("functionName"), Entry.FunctionName);
			Writer->WriteValue(TEXT("ownerClass"), Entry.OwnerClass);
			Writer->WriteValue(TEXT("replication"), Entry.Replication);
			Writer->WriteArrayStart(TEXT("parameters"));
			for (const FLogicCallParameterInfo& Parameter : Entry.Parameters)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), Parameter.Name);
				Writer->WriteValue(TEXT("value"), Parameter.Value);
				Writer->WriteValue(TEXT("defaultValue"), Parameter.DefaultValue);
				Writer->WriteValue(TEXT("linked"), Parameter.bLinked);
				WriteStringArray(Writer, TEXT("trace"), Parameter.Trace);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("branchRoutes"));
		for (const FRiskBranchRouteInfo& Route : Risk.BranchRoutes)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("graphName"), Route.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Route.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Route.NodeTitle);
			Writer->WriteValue(TEXT("condition"), Route.Condition);
			Writer->WriteValue(TEXT("trueTarget"), Route.TrueTarget);
			Writer->WriteValue(TEXT("falseTarget"), Route.FalseTarget);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("warnings"));
		for (const FRiskWarningInfo& Warning : Risk.Warnings)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("severity"), Warning.Severity);
			Writer->WriteValue(TEXT("category"), Warning.Category);
			Writer->WriteValue(TEXT("graphName"), Warning.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Warning.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Warning.NodeTitle);
			Writer->WriteValue(TEXT("message"), Warning.Message);
			Writer->WriteValue(TEXT("details"), Warning.Details);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteObjectEnd();
	}

	FString GetWidgetDisplayName(const UWidget* Widget)
	{
		if (!Widget)
		{
			return FString();
		}
		return Widget->GetFName().ToString();
	}

	FString GetWidgetClassName(const UWidget* Widget)
	{
		return Widget ? Widget->GetClass()->GetName() : FString();
	}

	void AppendWidgetTreeTextRecursive(const UWidget* Widget, int32 Depth, FString& Out)
	{
		if (!Widget)
		{
			return;
		}

		FString Indent;
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Indent += TEXT("  ");
		}

		Out += Indent + TEXT("- ") + GetWidgetDisplayName(Widget) + TEXT(" [") + GetWidgetClassName(Widget) + TEXT("]\n");

		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				AppendWidgetTreeTextRecursive(Panel->GetChildAt(ChildIndex), Depth + 1, Out);
			}
		}
	}

	FString ExportWidgetTreeToText(UBlueprint* Blueprint)
	{
		const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("---\n\n## Widget Tree\n\n");
		const UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;
		if (!RootWidget)
		{
			Out += TEXT("- (no root widget)\n\n");
			return Out;
		}

		AppendWidgetTreeTextRecursive(RootWidget, 0, Out);
		Out += TEXT("\n");
		return Out;
	}

	void WriteWidgetJsonContents(FJsonWriterRef Writer, const UWidget* Widget)
	{
		Writer->WriteValue(TEXT("name"), GetWidgetDisplayName(Widget));
		Writer->WriteValue(TEXT("class"), GetWidgetClassName(Widget));
		Writer->WriteValue(TEXT("path"), GetBlueprintObjectPathSafe(Widget));

		Writer->WriteArrayStart(TEXT("children"));
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				if (const UWidget* Child = Panel->GetChildAt(ChildIndex))
				{
					Writer->WriteObjectStart();
					WriteWidgetJsonContents(Writer, Child);
					Writer->WriteObjectEnd();
				}
			}
		}
		Writer->WriteArrayEnd();
	}

	void WriteWidgetTreeJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
		Writer->WriteObjectStart(TEXT("widgetTree"));
		Writer->WriteValue(TEXT("isWidgetBlueprint"), WidgetBlueprint != nullptr);
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !WidgetBlueprint->WidgetTree->RootWidget)
		{
			Writer->WriteNull(TEXT("root"));
			Writer->WriteObjectEnd();
			return;
		}

		TArray<UWidget*> Widgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
		Writer->WriteValue(TEXT("widgetCount"), Widgets.Num());
		Writer->WriteObjectStart(TEXT("root"));
		WriteWidgetJsonContents(Writer, WidgetBlueprint->WidgetTree->RootWidget);
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
	}

	bool IsUsefulWidgetSnapshotProperty(const FProperty* Property)
	{
		if (!Property)
		{
			return false;
		}

		static const TSet<FName> IncludedNames =
		{
			TEXT("Text"),
			TEXT("HintText"),
			TEXT("ColorAndOpacity"),
			TEXT("BackgroundColor"),
			TEXT("BrushColor"),
			TEXT("ContentColorAndOpacity"),
			TEXT("FillColorAndOpacity"),
			TEXT("Opacity"),
			TEXT("RenderOpacity"),
			TEXT("Visibility"),
			TEXT("bIsEnabled"),
			TEXT("ToolTipText"),
			TEXT("RenderTransform"),
			TEXT("RenderTransformPivot"),
			TEXT("Brush"),
			TEXT("Background"),
			TEXT("WidgetStyle"),
			TEXT("Font"),
			TEXT("StrikeBrush"),
			TEXT("ShadowOffset"),
			TEXT("ShadowColorAndOpacity"),
			TEXT("TextTransformPolicy"),
			TEXT("TextOverflowPolicy"),
			TEXT("Justification"),
			TEXT("WrappingPolicy"),
			TEXT("AutoWrapText"),
			TEXT("WrapTextAt"),
			TEXT("Margin"),
			TEXT("LineHeightPercentage"),
			TEXT("MinDesiredWidth"),
			TEXT("MinimumDesiredWidth"),
			TEXT("Percent"),
			TEXT("BarFillType"),
			TEXT("BarFillStyle"),
			TEXT("bIsMarquee"),
			TEXT("BorderPadding"),
			TEXT("Padding"),
			TEXT("HorizontalAlignment"),
			TEXT("VerticalAlignment"),
			TEXT("DesiredSizeScale"),
			TEXT("bShowEffectWhenDisabled"),
			TEXT("IsFocusable"),
			TEXT("ClickMethod"),
			TEXT("TouchMethod"),
			TEXT("PressMethod"),
			TEXT("IsReadOnly"),
			TEXT("IsPassword"),
			TEXT("CheckedState"),
			TEXT("ComboBoxStyle"),
			TEXT("ItemStyle"),
			TEXT("ScrollBarVisibility"),
			TEXT("Orientation"),
			TEXT("ActiveWidgetIndex")
		};

		const FName PropertyName = Property->GetFName();
		if (IncludedNames.Contains(PropertyName))
		{
			return true;
		}

		const FString Name = PropertyName.ToString();
		return Name.EndsWith(TEXT("Color"))
			|| Name.EndsWith(TEXT("ColorAndOpacity"))
			|| Name.EndsWith(TEXT("Brush"))
			|| Name.EndsWith(TEXT("Style"));
	}

	FString ExportObjectPropertyToText(const UObject* Object, const FProperty* Property)
	{
		if (!Object || !Property)
		{
			return FString();
		}

		FString Value;
		Property->ExportText_InContainer(0, Value, Object, nullptr, const_cast<UObject*>(Object), PPF_None);
		return Value;
	}

	void AddWidgetSnapshotProperty(const UObject* Object, const FProperty* Property, TArray<FWidgetSnapshotProperty>& OutProperties)
	{
		if (!Object || !Property || !IsUsefulWidgetSnapshotProperty(Property))
		{
			return;
		}

		const FString Value = ExportObjectPropertyToText(Object, Property);
		if (Value.IsEmpty() || Value == TEXT("()") || Value == TEXT("None"))
		{
			return;
		}

		FWidgetSnapshotProperty Info;
		Info.Name = Property->GetName();
		Info.Value = Value;
		Info.Category = Property->GetMetaData(TEXT("Category"));
		OutProperties.Add(MoveTemp(Info));
	}

	void CollectSnapshotPropertiesFromObject(const UObject* Object, TArray<FWidgetSnapshotProperty>& OutProperties)
	{
		if (!Object)
		{
			return;
		}

		TSet<FString> SeenNames;
		for (TFieldIterator<FProperty> PropertyIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (!Property || SeenNames.Contains(Property->GetName()))
			{
				continue;
			}
			SeenNames.Add(Property->GetName());
			AddWidgetSnapshotProperty(Object, Property, OutProperties);
		}

		OutProperties.Sort([](const FWidgetSnapshotProperty& A, const FWidgetSnapshotProperty& B)
		{
			return A.Name < B.Name;
		});
	}

	FWidgetCanvasSlotSnapshot MakeCanvasSlotSnapshot(const UPanelSlot* Slot)
	{
		FWidgetCanvasSlotSnapshot Snapshot;
		Snapshot.SlotClass = Slot ? Slot->GetClass()->GetName() : FString();

		const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
		if (!CanvasSlot)
		{
			return Snapshot;
		}

		Snapshot.bHasCanvasSlot = true;
		Snapshot.Offsets = CanvasSlot->GetOffsets();
		Snapshot.Anchors = CanvasSlot->GetAnchors();
		Snapshot.Alignment = CanvasSlot->GetAlignment();
		Snapshot.Position = CanvasSlot->GetPosition();
		Snapshot.Size = CanvasSlot->GetSize();
		Snapshot.bAutoSize = CanvasSlot->GetAutoSize();
		Snapshot.ZOrder = CanvasSlot->GetZOrder();
		return Snapshot;
	}

	void CollectWidgetSnapshots(const UWidgetBlueprint* WidgetBlueprint, TArray<FWidgetSnapshotInfo>& OutSnapshots)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return;
		}

		TArray<UWidget*> Widgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
		for (const UWidget* Widget : Widgets)
		{
			if (!Widget)
			{
				continue;
			}

			FWidgetSnapshotInfo Snapshot;
			Snapshot.Name = GetWidgetDisplayName(Widget);
			Snapshot.ClassName = GetWidgetClassName(Widget);
			Snapshot.Path = GetBlueprintObjectPathSafe(Widget);
			Snapshot.SlotClass = Widget->Slot ? Widget->Slot->GetClass()->GetName() : FString();
			Snapshot.CanvasSlot = MakeCanvasSlotSnapshot(Widget->Slot);
			CollectSnapshotPropertiesFromObject(Widget, Snapshot.Properties);
			OutSnapshots.Add(MoveTemp(Snapshot));
		}

		OutSnapshots.Sort([](const FWidgetSnapshotInfo& A, const FWidgetSnapshotInfo& B)
		{
			return A.Name < B.Name;
		});
	}

	void WriteVector2DJson(FJsonWriterRef Writer, const TCHAR* FieldName, const FVector2D& Value)
	{
		Writer->WriteObjectStart(FieldName);
		Writer->WriteValue(TEXT("x"), Value.X);
		Writer->WriteValue(TEXT("y"), Value.Y);
		Writer->WriteObjectEnd();
	}

	void WriteMarginJson(FJsonWriterRef Writer, const TCHAR* FieldName, const FMargin& Value)
	{
		Writer->WriteObjectStart(FieldName);
		Writer->WriteValue(TEXT("left"), Value.Left);
		Writer->WriteValue(TEXT("top"), Value.Top);
		Writer->WriteValue(TEXT("right"), Value.Right);
		Writer->WriteValue(TEXT("bottom"), Value.Bottom);
		Writer->WriteObjectEnd();
	}

	void WriteAnchorsJson(FJsonWriterRef Writer, const TCHAR* FieldName, const FAnchors& Value)
	{
		Writer->WriteObjectStart(FieldName);
		WriteVector2DJson(Writer, TEXT("minimum"), Value.Minimum);
		WriteVector2DJson(Writer, TEXT("maximum"), Value.Maximum);
		Writer->WriteObjectEnd();
	}

	void WriteWidgetSnapshotsJson(FJsonWriterRef Writer, const TArray<FWidgetSnapshotInfo>& Snapshots)
	{
		Writer->WriteArrayStart(TEXT("widgetSnapshots"));
		for (const FWidgetSnapshotInfo& Snapshot : Snapshots)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Snapshot.Name);
			Writer->WriteValue(TEXT("class"), Snapshot.ClassName);
			Writer->WriteValue(TEXT("path"), Snapshot.Path);
			Writer->WriteValue(TEXT("slotClass"), Snapshot.SlotClass);

			Writer->WriteArrayStart(TEXT("properties"));
			for (const FWidgetSnapshotProperty& Property : Snapshot.Properties)
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("name"), Property.Name);
				Writer->WriteValue(TEXT("value"), Property.Value);
				Writer->WriteValue(TEXT("category"), Property.Category);
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();

			if (Snapshot.CanvasSlot.bHasCanvasSlot)
			{
				Writer->WriteObjectStart(TEXT("canvasSlot"));
				Writer->WriteValue(TEXT("class"), Snapshot.CanvasSlot.SlotClass);
				WriteVector2DJson(Writer, TEXT("position"), Snapshot.CanvasSlot.Position);
				WriteVector2DJson(Writer, TEXT("size"), Snapshot.CanvasSlot.Size);
				WriteMarginJson(Writer, TEXT("offsets"), Snapshot.CanvasSlot.Offsets);
				WriteAnchorsJson(Writer, TEXT("anchors"), Snapshot.CanvasSlot.Anchors);
				WriteVector2DJson(Writer, TEXT("alignment"), Snapshot.CanvasSlot.Alignment);
				Writer->WriteValue(TEXT("autoSize"), Snapshot.CanvasSlot.bAutoSize);
				Writer->WriteValue(TEXT("zOrder"), Snapshot.CanvasSlot.ZOrder);
				Writer->WriteObjectEnd();
			}
			else
			{
				Writer->WriteNull(TEXT("canvasSlot"));
			}

			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
	}

	void AppendWidgetSnapshotsToText(const TArray<FWidgetSnapshotInfo>& Snapshots, FString& Out)
	{
		Out += TEXT("### Widget Snapshots\n\n");
		if (Snapshots.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
			return;
		}

		for (const FWidgetSnapshotInfo& Snapshot : Snapshots)
		{
			Out += FString::Printf(TEXT("- %s [%s]"), *Snapshot.Name, *Snapshot.ClassName);
			if (!Snapshot.SlotClass.IsEmpty())
			{
				Out += TEXT(" Slot=") + Snapshot.SlotClass;
			}
			Out += TEXT("\n");

			if (Snapshot.CanvasSlot.bHasCanvasSlot)
			{
				Out += FString::Printf(
					TEXT("  - CanvasSlot: Pos=(%.2f, %.2f) Size=(%.2f, %.2f) Offsets=(L=%.2f,T=%.2f,R=%.2f,B=%.2f) Anchors=(Min=%.2f,%.2f Max=%.2f,%.2f) Align=(%.2f, %.2f) AutoSize=%s Z=%d\n"),
					Snapshot.CanvasSlot.Position.X,
					Snapshot.CanvasSlot.Position.Y,
					Snapshot.CanvasSlot.Size.X,
					Snapshot.CanvasSlot.Size.Y,
					Snapshot.CanvasSlot.Offsets.Left,
					Snapshot.CanvasSlot.Offsets.Top,
					Snapshot.CanvasSlot.Offsets.Right,
					Snapshot.CanvasSlot.Offsets.Bottom,
					Snapshot.CanvasSlot.Anchors.Minimum.X,
					Snapshot.CanvasSlot.Anchors.Minimum.Y,
					Snapshot.CanvasSlot.Anchors.Maximum.X,
					Snapshot.CanvasSlot.Anchors.Maximum.Y,
					Snapshot.CanvasSlot.Alignment.X,
					Snapshot.CanvasSlot.Alignment.Y,
					Snapshot.CanvasSlot.bAutoSize ? TEXT("true") : TEXT("false"),
					Snapshot.CanvasSlot.ZOrder);
			}

			for (const FWidgetSnapshotProperty& Property : Snapshot.Properties)
			{
				Out += FString::Printf(TEXT("  - %s: %s\n"), *Property.Name, *Property.Value);
			}
		}
		Out += TEXT("\n");
	}

	FString GetBindingKindString(EBindingKind Kind)
	{
		switch (Kind)
		{
		case EBindingKind::Function:
			return TEXT("Function");
		case EBindingKind::Property:
			return TEXT("Property");
		default:
			return FString::Printf(TEXT("%d"), static_cast<int32>(Kind));
		}
	}

	FString GetMovieSceneDynamicBindingFunctionName(const FMovieSceneDynamicBinding& DynamicBinding)
	{
		if (DynamicBinding.Function)
		{
			return DynamicBinding.Function->GetName();
		}
#if WITH_EDITORONLY_DATA
		if (!DynamicBinding.CompiledFunctionName.IsNone())
		{
			return DynamicBinding.CompiledFunctionName.ToString();
		}
#endif
		return FString();
	}

	void CollectWidgetEventBindingsFromGraph(UEdGraph* Graph, TArray<FWidgetEventBindingInfo>& OutBindings)
	{
		if (!Graph)
		{
			return;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_ComponentBoundEvent* EventNode = Cast<UK2Node_ComponentBoundEvent>(Node);
			if (!EventNode)
			{
				continue;
			}

			FWidgetEventBindingInfo Info;
			Info.WidgetName = EventNode->GetComponentPropertyName().ToString();
			Info.DelegateName = EventNode->DelegatePropertyName.ToString();
			Info.DelegateDisplayName = EventNode->GetTargetDelegateDisplayName().ToString();
			Info.DelegateOwnerClass = EventNode->DelegateOwnerClass ? EventNode->DelegateOwnerClass->GetName() : FString();
			Info.FunctionName = EventNode->GetFunctionName().ToString();
			Info.GraphName = Graph->GetName();
			Info.NodeId = GetNodeId(EventNode);
			Info.NodeTitle = EventNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			OutBindings.Add(MoveTemp(Info));
		}
	}

	void CollectWidgetEventBindings(UBlueprint* Blueprint, TArray<FWidgetEventBindingInfo>& OutBindings)
	{
		if (!Blueprint)
		{
			return;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			CollectWidgetEventBindingsFromGraph(Graph, OutBindings);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			CollectWidgetEventBindingsFromGraph(Graph, OutBindings);
		}
	}

	FString ExportWidgetDetailsToText(UBlueprint* Blueprint)
	{
		const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
		if (!WidgetBlueprint)
		{
			return FString();
		}

		TArray<FWidgetEventBindingInfo> EventBindings;
		CollectWidgetEventBindings(Blueprint, EventBindings);
		TArray<FWidgetSnapshotInfo> WidgetSnapshots;
		CollectWidgetSnapshots(WidgetBlueprint, WidgetSnapshots);

		FString Out;
		Out += TEXT("---\n\n## Widget Details\n\n");
		AppendWidgetSnapshotsToText(WidgetSnapshots, Out);

		Out += TEXT("### Property Bindings\n\n");
		if (WidgetBlueprint->Bindings.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
			{
				const FString Kind = GetBindingKindString(Binding.Kind);
				const FString SourcePath = Binding.SourcePath.GetDisplayText().ToString();
				Out += FString::Printf(
					TEXT("- %s.%s -> %s %s"),
					*Binding.ObjectName,
					*Binding.PropertyName.ToString(),
					*Kind,
					*Binding.FunctionName.ToString());
				if (!Binding.SourceProperty.IsNone())
				{
					Out += TEXT(" SourceProperty=") + Binding.SourceProperty.ToString();
				}
				if (!SourcePath.IsEmpty())
				{
					Out += TEXT(" SourcePath=") + SourcePath;
				}
				if (Binding.MemberGuid.IsValid())
				{
					Out += TEXT(" Guid=") + Binding.MemberGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
				}
				Out += TEXT("\n");
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Event Bindings\n\n");
		if (EventBindings.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FWidgetEventBindingInfo& Info : EventBindings)
			{
				Out += FString::Printf(
					TEXT("- %s.%s -> %s (Graph: %s, NodeId: %s)"),
					*Info.WidgetName,
					*Info.DelegateName,
					*Info.FunctionName,
					*Info.GraphName,
					*Info.NodeId);
				if (!Info.DelegateDisplayName.IsEmpty() && Info.DelegateDisplayName != Info.DelegateName)
				{
					Out += TEXT(" DisplayName=") + Info.DelegateDisplayName;
				}
				if (!Info.DelegateOwnerClass.IsEmpty())
				{
					Out += TEXT(" Owner=") + Info.DelegateOwnerClass;
				}
				Out += TEXT("\n");
			}
			Out += TEXT("\n");
		}

		Out += TEXT("### Animations\n\n");
		if (WidgetBlueprint->Animations.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const UWidgetAnimation* Animation : WidgetBlueprint->Animations)
			{
				if (!Animation)
				{
					continue;
				}

				Out += FString::Printf(
					TEXT("- %s (%s): %.3fs -> %.3fs\n"),
					*Animation->GetName(),
					*Animation->GetDisplayName().ToString(),
					Animation->GetStartTime(),
					Animation->GetEndTime());

				const TArray<FWidgetAnimationBinding>& AnimationBindings = Animation->GetBindings();
				if (AnimationBindings.Num() == 0)
				{
					Out += TEXT("  - Binds: (none)\n");
				}
				else
				{
					for (const FWidgetAnimationBinding& Binding : AnimationBindings)
					{
						Out += FString::Printf(
							TEXT("  - Binds: %s Slot=%s Root=%s Guid=%s"),
							*Binding.WidgetName.ToString(),
							*Binding.SlotWidgetName.ToString(),
							Binding.bIsRootWidget ? TEXT("true") : TEXT("false"),
							*Binding.AnimationGuid.ToString(EGuidFormats::DigitsWithHyphensLower));

						const FString DynamicBindingFunction = GetMovieSceneDynamicBindingFunctionName(Binding.DynamicBinding);
						if (!DynamicBindingFunction.IsEmpty())
						{
							Out += TEXT(" DynamicBinding=") + DynamicBindingFunction;
						}
						Out += TEXT("\n");
					}
				}

				if (const UMovieScene* MovieScene = Animation->GetMovieScene())
				{
					Out += FString::Printf(
						TEXT("  - MovieScene: Tracks=%d ObjectBindings=%d\n"),
						MovieScene->GetTracks().Num(),
						MovieScene->GetBindings().Num());

					for (const UMovieSceneTrack* Track : MovieScene->GetTracks())
					{
						if (!Track)
						{
							continue;
						}
						Out += FString::Printf(
							TEXT("    - Track: %s [%s] Sections=%d\n"),
							*Track->GetDisplayName().ToString(),
							*Track->GetClass()->GetName(),
							Track->GetAllSections().Num());
					}

					for (const FMovieSceneBinding& ObjectBinding : MovieScene->GetBindings())
					{
						Out += FString::Printf(
							TEXT("    - ObjectBinding: %s Guid=%s Tracks=%d\n"),
							*ObjectBinding.GetName(),
							*ObjectBinding.GetObjectGuid().ToString(EGuidFormats::DigitsWithHyphensLower),
							ObjectBinding.GetTracks().Num());
						for (const UMovieSceneTrack* Track : ObjectBinding.GetTracks())
						{
							if (!Track)
							{
								continue;
							}
							Out += FString::Printf(
								TEXT("      - Track: %s [%s] Sections=%d\n"),
								*Track->GetDisplayName().ToString(),
								*Track->GetClass()->GetName(),
								Track->GetAllSections().Num());
						}
					}
				}
			}
			Out += TEXT("\n");
		}

		return Out;
	}

	void WriteWidgetDetailsJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint);
		Writer->WriteObjectStart(TEXT("widgetDetails"));
		Writer->WriteValue(TEXT("isWidgetBlueprint"), WidgetBlueprint != nullptr);
		if (!WidgetBlueprint)
		{
			Writer->WriteArrayStart(TEXT("widgetSnapshots"));
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("propertyBindings"));
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("eventBindings"));
			Writer->WriteArrayEnd();
			Writer->WriteArrayStart(TEXT("animations"));
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
			return;
		}

		TArray<FWidgetSnapshotInfo> WidgetSnapshots;
		CollectWidgetSnapshots(WidgetBlueprint, WidgetSnapshots);
		WriteWidgetSnapshotsJson(Writer, WidgetSnapshots);

		Writer->WriteArrayStart(TEXT("propertyBindings"));
		for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("objectName"), Binding.ObjectName);
			Writer->WriteValue(TEXT("propertyName"), Binding.PropertyName.ToString());
			Writer->WriteValue(TEXT("kind"), GetBindingKindString(Binding.Kind));
			Writer->WriteValue(TEXT("functionName"), Binding.FunctionName.ToString());
			Writer->WriteValue(TEXT("sourceProperty"), Binding.SourceProperty.ToString());
			Writer->WriteValue(TEXT("sourcePath"), Binding.SourcePath.GetDisplayText().ToString());
			Writer->WriteValue(TEXT("memberGuid"), Binding.MemberGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		TArray<FWidgetEventBindingInfo> EventBindings;
		CollectWidgetEventBindings(Blueprint, EventBindings);
		Writer->WriteArrayStart(TEXT("eventBindings"));
		for (const FWidgetEventBindingInfo& Info : EventBindings)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("widgetName"), Info.WidgetName);
			Writer->WriteValue(TEXT("delegateName"), Info.DelegateName);
			Writer->WriteValue(TEXT("delegateDisplayName"), Info.DelegateDisplayName);
			Writer->WriteValue(TEXT("delegateOwnerClass"), Info.DelegateOwnerClass);
			Writer->WriteValue(TEXT("functionName"), Info.FunctionName);
			Writer->WriteValue(TEXT("graphName"), Info.GraphName);
			Writer->WriteValue(TEXT("nodeId"), Info.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Info.NodeTitle);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("animations"));
		for (const UWidgetAnimation* Animation : WidgetBlueprint->Animations)
		{
			if (!Animation)
			{
				continue;
			}

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Animation->GetName());
			Writer->WriteValue(TEXT("displayName"), Animation->GetDisplayName().ToString());
			Writer->WriteValue(TEXT("path"), GetBlueprintObjectPathSafe(Animation));
			Writer->WriteValue(TEXT("startTime"), Animation->GetStartTime());
			Writer->WriteValue(TEXT("endTime"), Animation->GetEndTime());

			Writer->WriteArrayStart(TEXT("bindings"));
			for (const FWidgetAnimationBinding& Binding : Animation->GetBindings())
			{
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("widgetName"), Binding.WidgetName.ToString());
				Writer->WriteValue(TEXT("slotWidgetName"), Binding.SlotWidgetName.ToString());
				Writer->WriteValue(TEXT("animationGuid"), Binding.AnimationGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
				Writer->WriteValue(TEXT("isRootWidget"), Binding.bIsRootWidget);
				Writer->WriteValue(TEXT("dynamicBindingFunction"), GetMovieSceneDynamicBindingFunctionName(Binding.DynamicBinding));
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();

			if (const UMovieScene* MovieScene = Animation->GetMovieScene())
			{
				Writer->WriteObjectStart(TEXT("movieScene"));
				Writer->WriteValue(TEXT("path"), GetBlueprintObjectPathSafe(MovieScene));

				Writer->WriteArrayStart(TEXT("tracks"));
				for (const UMovieSceneTrack* Track : MovieScene->GetTracks())
				{
					if (!Track)
					{
						continue;
					}
					Writer->WriteObjectStart();
					Writer->WriteValue(TEXT("name"), Track->GetDisplayName().ToString());
					Writer->WriteValue(TEXT("class"), Track->GetClass()->GetName());
					Writer->WriteValue(TEXT("sectionCount"), Track->GetAllSections().Num());
					Writer->WriteObjectEnd();
				}
				Writer->WriteArrayEnd();

				Writer->WriteArrayStart(TEXT("objectBindings"));
				for (const FMovieSceneBinding& ObjectBinding : MovieScene->GetBindings())
				{
					Writer->WriteObjectStart();
					Writer->WriteValue(TEXT("name"), ObjectBinding.GetName());
					Writer->WriteValue(TEXT("guid"), ObjectBinding.GetObjectGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
					Writer->WriteArrayStart(TEXT("tracks"));
					for (const UMovieSceneTrack* Track : ObjectBinding.GetTracks())
					{
						if (!Track)
						{
							continue;
						}
						Writer->WriteObjectStart();
						Writer->WriteValue(TEXT("name"), Track->GetDisplayName().ToString());
						Writer->WriteValue(TEXT("class"), Track->GetClass()->GetName());
						Writer->WriteValue(TEXT("sectionCount"), Track->GetAllSections().Num());
						Writer->WriteObjectEnd();
					}
					Writer->WriteArrayEnd();
					Writer->WriteObjectEnd();
				}
				Writer->WriteArrayEnd();
				Writer->WriteObjectEnd();
			}
			else
			{
				Writer->WriteNull(TEXT("movieScene"));
			}

			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}

	FString GetPinLinkedSourceLabel(const UEdGraphPin* Pin)
	{
		if (!Pin || Pin->LinkedTo.Num() == 0)
		{
			return FString();
		}

		const UEdGraphPin* SourcePin = Pin->LinkedTo[0];
		const UEdGraphNode* SourceNode = SourcePin ? SourcePin->GetOwningNode() : nullptr;
		if (!SourcePin || !SourceNode)
		{
			return FString();
		}

		return SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString() + TEXT(".") + SourcePin->PinName.ToString();
	}

	FString GetRowNameDescription(const UEdGraphPin* RowNamePin)
	{
		if (!RowNamePin)
		{
			return FString();
		}
		if (RowNamePin->LinkedTo.Num() > 0)
		{
			return GetPinLinkedSourceLabel(RowNamePin);
		}
		return GetPinDefaultString(RowNamePin);
	}

	FDataTableUsageInfo MakeDataTableUsageInfo(UEdGraphNode* Node, const UEdGraphPin* DataTablePin, const UEdGraphPin* RowNamePin, const UScriptStruct* RowStruct)
	{
		FDataTableUsageInfo Info;
		Info.NodeId = GetNodeId(Node);
		Info.NodeTitle = Node ? Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString();
		Info.RowName = GetRowNameDescription(RowNamePin);
		Info.RowNameSource = RowNamePin && RowNamePin->LinkedTo.Num() > 0 ? TEXT("Linked") : TEXT("Default");

		const UDataTable* DataTable = DataTablePin ? Cast<UDataTable>(DataTablePin->DefaultObject) : nullptr;
		if (DataTable)
		{
			Info.DataTableName = DataTable->GetName();
			Info.DataTablePath = GetBlueprintObjectPathSafe(DataTable);
			if (!RowStruct)
			{
				RowStruct = DataTable->RowStruct;
			}
		}
		else if (DataTablePin && DataTablePin->LinkedTo.Num() > 0)
		{
			Info.DataTableName = GetPinLinkedSourceLabel(DataTablePin);
			Info.DataTablePath = TEXT("<linked>");
		}

		if (RowStruct)
		{
			Info.RowStructName = RowStruct->GetName();
			Info.RowStructPath = GetBlueprintObjectPathSafe(RowStruct);
		}
		return Info;
	}

	void CollectDataTableUsageFromNode(UEdGraphNode* Node, TArray<FDataTableUsageInfo>& OutUsages)
	{
		if (UK2Node_GetDataTableRow* GetRowNode = Cast<UK2Node_GetDataTableRow>(Node))
		{
			OutUsages.Add(MakeDataTableUsageInfo(
				Node,
				GetRowNode->GetDataTablePin(),
				GetRowNode->GetRowNamePin(),
				GetRowNode->GetDataTableRowStructType()
			));
			return;
		}

		if (UK2Node_CallDataTableFunction* DataTableFunctionNode = Cast<UK2Node_CallDataTableFunction>(Node))
		{
			if (UFunction* Function = DataTableFunctionNode->GetTargetFunction())
			{
				const FString DataTablePinName = Function->GetMetaData(FBlueprintMetadata::MD_DataTablePin);
				const UEdGraphPin* DataTablePin = DataTablePinName.IsEmpty() ? nullptr : DataTableFunctionNode->FindPin(FName(*DataTablePinName));
				const UEdGraphPin* RowNamePin = DataTableFunctionNode->FindPin(TEXT("RowName"));
				OutUsages.Add(MakeDataTableUsageInfo(Node, DataTablePin, RowNamePin, nullptr));
			}
		}
	}

	void AddStructUsage(TMap<FString, FStructUsageInfo>& StructsByPath, const UScriptStruct* Struct)
	{
		if (!Struct)
		{
			return;
		}

		const FString Path = GetBlueprintObjectPathSafe(Struct);
		FStructUsageInfo& Info = StructsByPath.FindOrAdd(Path);
		Info.Name = Struct->GetName();
		Info.Path = Path;

		if (Info.Fields.Num() == 0)
		{
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				const FProperty* Property = *It;
				if (!Property) continue;
				Info.Fields.Add(Property->GetName() + TEXT(": ") + Property->GetCPPType());
			}
			Info.Fields.Sort();
		}
	}

	void CollectStructUsageFromPin(TMap<FString, FStructUsageInfo>& StructsByPath, const FEdGraphPinType& PinType)
	{
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			AddStructUsage(StructsByPath, Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()));
		}
		if (PinType.ContainerType == EPinContainerType::Map && PinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Struct)
		{
			AddStructUsage(StructsByPath, Cast<UScriptStruct>(PinType.PinValueType.TerminalSubCategoryObject.Get()));
		}
	}

	void CollectBlueprintDataSummaries(UBlueprint* Blueprint, TArray<FDataTableUsageInfo>& OutDataTables, TArray<FStructUsageInfo>& OutStructs)
	{
		TMap<FString, FStructUsageInfo> StructsByPath;
		if (!Blueprint)
		{
			return;
		}

		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			CollectStructUsageFromPin(StructsByPath, Var.VarType);
		}

		auto VisitGraph = [&OutDataTables, &StructsByPath](UEdGraph* Graph)
		{
			if (!Graph) return;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;
				CollectDataTableUsageFromNode(Node, OutDataTables);

				if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(Node))
				{
					AddStructUsage(StructsByPath, StructNode->StructType);
				}

				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin)
					{
						CollectStructUsageFromPin(StructsByPath, Pin->PinType);
					}
				}
			}
		};

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			VisitGraph(Graph);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			VisitGraph(Graph);
		}

		StructsByPath.GenerateValueArray(OutStructs);
		OutStructs.Sort([](const FStructUsageInfo& A, const FStructUsageInfo& B)
		{
			return A.Name < B.Name;
		});
	}

	FString ExportDataTableAndStructSummariesToText(UBlueprint* Blueprint)
	{
		TArray<FDataTableUsageInfo> DataTables;
		TArray<FStructUsageInfo> Structs;
		CollectBlueprintDataSummaries(Blueprint, DataTables, Structs);

		if (DataTables.Num() == 0 && Structs.Num() == 0)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("---\n\n## Data Tables\n\n");
		if (DataTables.Num() == 0)
		{
			Out += TEXT("- (none)\n");
		}
		else
		{
			for (const FDataTableUsageInfo& Info : DataTables)
			{
				Out += FString::Printf(TEXT("- Node: %s\n"), *Info.NodeTitle);
				Out += FString::Printf(TEXT("  - DataTable: %s\n"), *(Info.DataTablePath.IsEmpty() ? Info.DataTableName : Info.DataTablePath));
				Out += FString::Printf(TEXT("  - RowName: %s (%s)\n"), *Info.RowName, *Info.RowNameSource);
				Out += FString::Printf(TEXT("  - RowStruct: %s\n"), *(Info.RowStructPath.IsEmpty() ? Info.RowStructName : Info.RowStructPath));
			}
		}

		Out += TEXT("\n---\n\n## Structs\n\n");
		if (Structs.Num() == 0)
		{
			Out += TEXT("- (none)\n\n");
		}
		else
		{
			for (const FStructUsageInfo& Info : Structs)
			{
				Out += FString::Printf(TEXT("- %s (%s)\n"), *Info.Name, *Info.Path);
				for (const FString& Field : Info.Fields)
				{
					Out += TEXT("  - ") + Field + TEXT("\n");
				}
			}
			Out += TEXT("\n");
		}

		return Out;
	}

	void WriteDataTableAndStructSummariesJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		TArray<FDataTableUsageInfo> DataTables;
		TArray<FStructUsageInfo> Structs;
		CollectBlueprintDataSummaries(Blueprint, DataTables, Structs);

		Writer->WriteArrayStart(TEXT("dataTables"));
		for (const FDataTableUsageInfo& Info : DataTables)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("nodeId"), Info.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Info.NodeTitle);
			Writer->WriteValue(TEXT("dataTableName"), Info.DataTableName);
			Writer->WriteValue(TEXT("dataTablePath"), Info.DataTablePath);
			Writer->WriteValue(TEXT("rowName"), Info.RowName);
			Writer->WriteValue(TEXT("rowNameSource"), Info.RowNameSource);
			Writer->WriteValue(TEXT("rowStructName"), Info.RowStructName);
			Writer->WriteValue(TEXT("rowStructPath"), Info.RowStructPath);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();

		Writer->WriteArrayStart(TEXT("structs"));
		for (const FStructUsageInfo& Info : Structs)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Info.Name);
			Writer->WriteValue(TEXT("path"), Info.Path);
			Writer->WriteArrayStart(TEXT("fields"));
			for (const FString& Field : Info.Fields)
			{
				Writer->WriteValue(Field);
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
	}

	struct FClassDefaultInfo
	{
		FString Name;
		const UClass* Class = nullptr;
	};

	TArray<FClassDefaultInfo> CollectGameModeClassDefaults(UBlueprint* Blueprint)
	{
		TArray<FClassDefaultInfo> Defaults;
		if (!Blueprint)
		{
			return Defaults;
		}

		const UClass* GeneratedClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : nullptr;
		if (!GeneratedClass || !GeneratedClass->IsChildOf(AGameModeBase::StaticClass()))
		{
			return Defaults;
		}

		const AGameModeBase* GameModeCDO = Cast<AGameModeBase>(GeneratedClass->GetDefaultObject());
		if (!GameModeCDO)
		{
			return Defaults;
		}

		Defaults.Add({ TEXT("GameStateClass"), GameModeCDO->GameStateClass.Get() });
		Defaults.Add({ TEXT("PlayerStateClass"), GameModeCDO->PlayerStateClass.Get() });
		Defaults.Add({ TEXT("PlayerControllerClass"), GameModeCDO->PlayerControllerClass.Get() });
		Defaults.Add({ TEXT("DefaultPawnClass"), GameModeCDO->DefaultPawnClass.Get() });
		Defaults.Add({ TEXT("HUDClass"), GameModeCDO->HUDClass.Get() });
		return Defaults;
	}

	FString ExportGameModeClassDefaultsToText(UBlueprint* Blueprint)
	{
		const TArray<FClassDefaultInfo> Defaults = CollectGameModeClassDefaults(Blueprint);
		if (Defaults.Num() == 0)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("---\n\n## GameMode Class Defaults\n\n");
		for (const FClassDefaultInfo& DefaultInfo : Defaults)
		{
			const FString ReadablePath = GetClassReadablePath(DefaultInfo.Class);
			Out += FString::Printf(
				TEXT("- %s: %s\n"),
				*DefaultInfo.Name,
				*(ReadablePath.IsEmpty() ? FString(TEXT("(none)")) : ReadablePath)
			);
			const FString NativeClassPath = GetBlueprintObjectPathSafe(DefaultInfo.Class);
			const FString BlueprintPath = GetClassBlueprintPathSafe(DefaultInfo.Class);
			if (!NativeClassPath.IsEmpty() && !BlueprintPath.IsEmpty() && NativeClassPath != BlueprintPath)
			{
				Out += TEXT("  - GeneratedClass: ") + NativeClassPath + TEXT("\n");
			}
		}
		Out += TEXT("\n");
		return Out;
	}

	void WriteClassDefaultJson(FJsonWriterRef Writer, const FClassDefaultInfo& DefaultInfo)
	{
		Writer->WriteObjectStart(DefaultInfo.Name);
		Writer->WriteValue(TEXT("name"), DefaultInfo.Class ? DefaultInfo.Class->GetName() : FString());
		Writer->WriteValue(TEXT("class"), GetBlueprintObjectPathSafe(DefaultInfo.Class));
		Writer->WriteValue(TEXT("blueprint"), GetClassBlueprintPathSafe(DefaultInfo.Class));
		Writer->WriteValue(TEXT("display"), GetClassReadablePath(DefaultInfo.Class));
		Writer->WriteObjectEnd();
	}

	void WriteGameModeClassDefaultsJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		const TArray<FClassDefaultInfo> Defaults = CollectGameModeClassDefaults(Blueprint);
		Writer->WriteObjectStart(TEXT("gameModeClassDefaults"));
		Writer->WriteValue(TEXT("isGameMode"), Defaults.Num() > 0);
		for (const FClassDefaultInfo& DefaultInfo : Defaults)
		{
			WriteClassDefaultJson(Writer, DefaultInfo);
		}
		Writer->WriteObjectEnd();
	}

	FString GetAssetRefKey(const FAssetReferenceInfo& Info)
	{
		return Info.Purpose + TEXT("|") + Info.NodeId + TEXT("|") + Info.PinName + TEXT("|") + Info.AssetPath;
	}

	void AddAssetReference(TArray<FAssetReferenceInfo>& OutReferences, TSet<FString>& Seen, const FAssetReferenceInfo& Info)
	{
		if (Info.AssetPath.IsEmpty() && Info.AssetName.IsEmpty())
		{
			return;
		}

		const FString Key = GetAssetRefKey(Info);
		if (Seen.Contains(Key))
		{
			return;
		}
		Seen.Add(Key);
		OutReferences.Add(Info);
	}

	FAssetReferenceInfo MakeAssetReferenceBase(UEdGraphNode* Node, const UEdGraphPin* Pin, const FString& Purpose)
	{
		FAssetReferenceInfo Info;
		Info.Purpose = Purpose;
		Info.NodeId = GetNodeId(Node);
		Info.NodeTitle = Node ? Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString();
		Info.PinName = Pin ? Pin->PinName.ToString() : FString();
		return Info;
	}

	void AddObjectAssetReference(TArray<FAssetReferenceInfo>& OutReferences, TSet<FString>& Seen, UEdGraphNode* Node, const UEdGraphPin* Pin, const UObject* Object, const FString& Purpose)
	{
		if (!Object)
		{
			return;
		}

		FAssetReferenceInfo Info = MakeAssetReferenceBase(Node, Pin, Purpose);
		Info.AssetName = Object->GetName();
		Info.AssetPath = GetBlueprintObjectPathSafe(Object);
		Info.AssetClass = Object->GetClass() ? Object->GetClass()->GetName() : FString();
		AddAssetReference(OutReferences, Seen, Info);
	}

	void AddClassAssetReference(TArray<FAssetReferenceInfo>& OutReferences, TSet<FString>& Seen, UEdGraphNode* Node, const UEdGraphPin* Pin, const UClass* Class, const FString& Purpose)
	{
		if (!Class)
		{
			return;
		}

		UObject* ClassGeneratedBy = Class->ClassGeneratedBy;
		AddObjectAssetReference(OutReferences, Seen, Node, Pin, ClassGeneratedBy ? ClassGeneratedBy : Class, Purpose);
	}

	void CollectAssetReferenceFromPin(TArray<FAssetReferenceInfo>& OutReferences, TSet<FString>& Seen, UEdGraphNode* Node, const UEdGraphPin* Pin, const FString& Purpose)
	{
		if (!Pin)
		{
			return;
		}

		if (Pin->DefaultObject)
		{
			if (const UClass* Class = Cast<UClass>(Pin->DefaultObject))
			{
				AddClassAssetReference(OutReferences, Seen, Node, Pin, Class, Purpose);
			}
			else
			{
				AddObjectAssetReference(OutReferences, Seen, Node, Pin, Pin->DefaultObject, Purpose);
			}
		}

		if (UObject* SubCategoryObject = Pin->PinType.PinSubCategoryObject.Get())
		{
			if (const UClass* Class = Cast<UClass>(SubCategoryObject))
			{
				AddClassAssetReference(OutReferences, Seen, Node, Pin, Class, Purpose + TEXT(" Type"));
			}
		}
	}

	void CollectAssetReferencesFromNode(UEdGraphNode* Node, TArray<FAssetReferenceInfo>& OutReferences, TSet<FString>& Seen)
	{
		if (!Node)
		{
			return;
		}

		if (UK2Node_ConstructObjectFromClass* ConstructNode = Cast<UK2Node_ConstructObjectFromClass>(Node))
		{
			const FString Purpose = Cast<UK2Node_SpawnActorFromClass>(Node) ? TEXT("SpawnActorClass") : TEXT("ConstructObjectClass");
			CollectAssetReferenceFromPin(OutReferences, Seen, Node, ConstructNode->GetClassPin(), Purpose);
		}
		else if (UK2Node_SpawnActor* SpawnActorNode = Cast<UK2Node_SpawnActor>(Node))
		{
			CollectAssetReferenceFromPin(OutReferences, Seen, Node, SpawnActorNode->GetBlueprintPin(), TEXT("SpawnActorBlueprint"));
		}

		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
		{
			const FString FunctionName = CallNode->GetTargetFunction() ? CallNode->GetTargetFunction()->GetName() : FString();
			const bool bLooksLikeCreateWidget = FunctionName.Contains(TEXT("CreateWidget"));
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input)
				{
					continue;
				}
				const FString Purpose = bLooksLikeCreateWidget && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class
					? TEXT("CreateWidgetClass")
					: TEXT("InputAsset");
				CollectAssetReferenceFromPin(OutReferences, Seen, Node, Pin, Purpose);
			}
		}
		else
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Input)
				{
					CollectAssetReferenceFromPin(OutReferences, Seen, Node, Pin, TEXT("InputAsset"));
				}
			}
		}
	}

	void CollectBlueprintAssetReferences(UBlueprint* Blueprint, TArray<FAssetReferenceInfo>& OutReferences)
	{
		TSet<FString> Seen;
		if (!Blueprint)
		{
			return;
		}

		auto VisitGraph = [&OutReferences, &Seen](UEdGraph* Graph)
		{
			if (!Graph) return;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				CollectAssetReferencesFromNode(Node, OutReferences, Seen);
			}
		};

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			VisitGraph(Graph);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			VisitGraph(Graph);
		}

		OutReferences.Sort([](const FAssetReferenceInfo& A, const FAssetReferenceInfo& B)
		{
			return A.AssetPath == B.AssetPath ? A.Purpose < B.Purpose : A.AssetPath < B.AssetPath;
		});
	}

	FString ExportAssetReferencesToText(UBlueprint* Blueprint)
	{
		TArray<FAssetReferenceInfo> References;
		CollectBlueprintAssetReferences(Blueprint, References);
		if (References.Num() == 0)
		{
			return FString();
		}

		FString Out;
		Out += TEXT("---\n\n## Asset References\n\n");
		for (const FAssetReferenceInfo& Ref : References)
		{
			Out += FString::Printf(TEXT("- %s: %s (%s)\n"), *Ref.Purpose, *Ref.AssetPath, *Ref.AssetClass);
			Out += FString::Printf(TEXT("  - Node: %s\n"), *Ref.NodeTitle);
			Out += FString::Printf(TEXT("  - Pin: %s\n"), *Ref.PinName);
		}
		Out += TEXT("\n");
		return Out;
	}

	void WriteAssetReferencesJson(FJsonWriterRef Writer, UBlueprint* Blueprint)
	{
		TArray<FAssetReferenceInfo> References;
		CollectBlueprintAssetReferences(Blueprint, References);

		Writer->WriteArrayStart(TEXT("assetReferences"));
		for (const FAssetReferenceInfo& Ref : References)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("purpose"), Ref.Purpose);
			Writer->WriteValue(TEXT("nodeId"), Ref.NodeId);
			Writer->WriteValue(TEXT("nodeTitle"), Ref.NodeTitle);
			Writer->WriteValue(TEXT("pinName"), Ref.PinName);
			Writer->WriteValue(TEXT("assetName"), Ref.AssetName);
			Writer->WriteValue(TEXT("assetPath"), Ref.AssetPath);
			Writer->WriteValue(TEXT("assetClass"), Ref.AssetClass);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
	}
}

FString FBlueprintToTextExporter::ExportBlueprintToText(UBlueprint* Blueprint)
{
	if (!Blueprint)
		return FString();

	FString ClassName = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetName() : (Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass->GetName() : TEXT("?"));
	const UClass* ParentClass = Blueprint->ParentClass ? Blueprint->ParentClass.Get() : nullptr;

	FString Out;
	Out += TEXT("# Blueprint Logic Export (for AI / C++ reference)\n");
	Out += TEXT("# Blueprint: ") + Blueprint->GetName() + TEXT("\n");
	Out += TEXT("# Class: ") + ClassName + TEXT("\n\n");
	Out += TEXT("# AssetPath: ") + GetBlueprintObjectPathSafe(Blueprint) + TEXT("\n");
	Out += TEXT("# ParentClass: ") + (ParentClass ? ParentClass->GetName() : TEXT("")) + TEXT("\n");
	Out += TEXT("# BlueprintType: ") + GetBlueprintTypeString(Blueprint->BlueprintType) + TEXT("\n\n");
	Out += TEXT("---\n");
	Out += TEXT("## 使用说明\n");
	Out += TEXT("- 下文按「执行顺序」组织：从事件/函数入口沿 exec 连线展开，便于理解“先做什么、再做什么、在什么条件下分支”\n");
	Out += TEXT("- 每个节点下方 `// 数据:` 列出关键输入的数据来源，便于对照 C++ 时的参数与条件\n");
	Out += TEXT("- 可作为答辩时讲述蓝图逻辑的提纲，或编写等价 C++ 的参考\n\n");

	Out += ExportGameModeClassDefaultsToText(Blueprint);

	Out += TEXT("---\n\n## Variables\n\n");
	Out += TEXT("### Member Variables\n\n");
	if (Blueprint->NewVariables.Num() == 0)
	{
		Out += TEXT("- (none)\n\n");
	}
	else
	{
		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			Out += FormatVariableDescriptionText(Var) + TEXT("\n");
		}
		Out += TEXT("\n");
	}

	Out += TEXT("### Local Variables\n\n");
	bool bHasLocalVariables = false;
	for (UEdGraph* Graph : GetAllBlueprintGraphs(Blueprint))
	{
		UK2Node_FunctionEntry* EntryNode = FindFunctionEntryNode(Graph);
		if (!EntryNode || EntryNode->LocalVariables.Num() == 0)
		{
			continue;
		}

		bHasLocalVariables = true;
		const FString ScopeName = Graph ? Graph->GetName() : FString(TEXT("?"));
		Out += TEXT("#### ") + ScopeName + TEXT("\n\n");
		for (const FBPVariableDescription& Var : EntryNode->LocalVariables)
		{
			Out += FormatVariableDescriptionText(Var) + TEXT("\n");
		}
		Out += TEXT("\n");
	}
	if (!bHasLocalVariables)
	{
		Out += TEXT("- (none)\n\n");
	}

	Out += ExportLogicSummaryToText(Blueprint);
	Out += ExportRiskSummaryToText(Blueprint);
	Out += ExportCommentBoxesToText(Blueprint);
	Out += ExportWidgetTreeToText(Blueprint);
	Out += ExportWidgetDetailsToText(Blueprint);
	Out += ExportDataTableAndStructSummariesToText(Blueprint);
	Out += ExportAssetReferencesToText(Blueprint);

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph)
		{
			FString GraphName = Graph->GetName();
			if (GraphName.IsEmpty()) GraphName = TEXT("EventGraph");
			Out += TEXT("---\n\n## 图: ") + GraphName + TEXT("\n\n");
			Out += ExportGraphToText(Graph, GraphName);
			Out += TEXT("\n");
		}
	}

	for (int32 i = 0; i < Blueprint->FunctionGraphs.Num(); i++)
	{
		UEdGraph* Graph = Blueprint->FunctionGraphs[i];
		if (Graph)
		{
			Out += TEXT("---\n\n## 函数: ") + Graph->GetName() + TEXT("\n\n");
			Out += ExportGraphToText(Graph, Graph->GetName());
			Out += TEXT("\n");
		}
	}

	return Out;
}

FString FBlueprintToTextExporter::ExportBlueprintToJson(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return FString();
	}

	FString Out;
	FJsonWriterRef Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);

	const UClass* GeneratedClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : nullptr;
	const UClass* SkeletonClass = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass.Get() : nullptr;
	const UClass* ParentClass = Blueprint->ParentClass ? Blueprint->ParentClass.Get() : nullptr;

	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schemaVersion"), 1);
	Writer->WriteObjectStart(TEXT("asset"));
	Writer->WriteValue(TEXT("name"), Blueprint->GetName());
	Writer->WriteValue(TEXT("path"), GetBlueprintObjectPathSafe(Blueprint));
	Writer->WriteValue(TEXT("package"), Blueprint->GetOutermost() ? Blueprint->GetOutermost()->GetName() : FString());
	Writer->WriteValue(TEXT("blueprintType"), GetBlueprintTypeString(Blueprint->BlueprintType));
	Writer->WriteValue(TEXT("generatedClass"), GeneratedClass ? GeneratedClass->GetName() : FString());
	Writer->WriteValue(TEXT("skeletonClass"), SkeletonClass ? SkeletonClass->GetName() : FString());
	Writer->WriteValue(TEXT("parentClass"), ParentClass ? ParentClass->GetName() : FString());
	WriteObjectPathField(Writer, TEXT("parentClassPath"), ParentClass);
	Writer->WriteObjectEnd();

	WriteGameModeClassDefaultsJson(Writer, Blueprint);
	WriteLogicSummaryJson(Writer, Blueprint);
	WriteRiskSummaryJson(Writer, Blueprint);
	WriteCommentBoxesJson(Writer, Blueprint);
	WriteWidgetTreeJson(Writer, Blueprint);
	WriteWidgetDetailsJson(Writer, Blueprint);
	WriteDataTableAndStructSummariesJson(Writer, Blueprint);
	WriteAssetReferencesJson(Writer, Blueprint);

	Writer->WriteArrayStart(TEXT("variables"));
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		WriteVariableDescriptionObject(Writer, Var, TEXT("Member"));
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("memberVariables"));
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		WriteVariableDescriptionObject(Writer, Var, TEXT("Member"));
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("localVariables"));
	for (UEdGraph* Graph : GetAllBlueprintGraphs(Blueprint))
	{
		UK2Node_FunctionEntry* EntryNode = FindFunctionEntryNode(Graph);
		if (!EntryNode)
		{
			continue;
		}

		const FString ScopeName = Graph ? Graph->GetName() : FString();
		for (const FBPVariableDescription& Var : EntryNode->LocalVariables)
		{
			WriteVariableDescriptionObject(Writer, Var, TEXT("Local"), ScopeName);
		}
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("interfaces"));
	for (const FBPInterfaceDescription& InterfaceDesc : Blueprint->ImplementedInterfaces)
	{
		Writer->WriteObjectStart();
		UClass* InterfaceClass = InterfaceDesc.Interface.Get();
		Writer->WriteValue(TEXT("name"), InterfaceClass ? InterfaceClass->GetName() : FString());
		WriteObjectPathField(Writer, TEXT("path"), InterfaceClass);
		Writer->WriteArrayStart(TEXT("graphs"));
		for (UEdGraph* Graph : InterfaceDesc.Graphs)
		{
			Writer->WriteValue(Graph ? Graph->GetName() : FString());
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("events"));
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				const uint32 NetFlags = GetEventNetFlags(EventNode);
				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("graph"), Graph->GetName());
				Writer->WriteValue(TEXT("nodeId"), GetNodeId(EventNode));
				Writer->WriteValue(TEXT("name"), EventNode->GetFunctionName().ToString());
				Writer->WriteValue(TEXT("replication"), GetReplicationString(NetFlags));
				Writer->WriteValue(TEXT("reliable"), (NetFlags & FUNC_NetReliable) != 0);
				WriteSignatureFromPins(Writer, EventNode);
				Writer->WriteObjectEnd();
			}
		}
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("functions"));
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph) continue;
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("name"), Graph->GetName());
		WriteFunctionSignatureFromGraph(Writer, Graph);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("graphs"));
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		WriteGraphObject(Writer, Graph, TEXT("Ubergraph"));
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		WriteGraphObject(Writer, Graph, TEXT("Function"));
	}
	Writer->WriteArrayEnd();

	Writer->WriteObjectEnd();
	Writer->Close();
	return Out;
}

FString FBlueprintToTextExporter::ExportGraphToText(UEdGraph* Graph, const FString& GraphName)
{
	if (!Graph) return FString();

	// 找出所有“执行入口”节点（事件、函数入口，或 exec 输入未连接的节点）
	TArray<UEdGraphNode*> Roots;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;
		if (!IsNaturalExecutionRoot(Node)) continue;
		Roots.Add(Node);
	}
	// 若没有天然根，则用“无 exec 输入连接”的节点作为根（兜底）
	if (Roots.Num() == 0)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (IsExecutionRoot(Node)) Roots.AddUnique(Node);
		}
	}

	TSet<UEdGraphNode*> Visited;
	FString Out;
	int32 IndentLevel = 0;
	const TCHAR* IndentUnit = TEXT("  ");

	auto AppendIndent = [&]() -> FString
	{
		FString s;
		for (int32 i = 0; i < IndentLevel; i++) s += IndentUnit;
		return s;
	};

	// 对每个入口做 DFS，沿 exec 输出遍历
	for (UEdGraphNode* Root : Roots)
	{
		TArray<UEdGraphNode*> Stack;
		TArray<int32> IndentStack;
		Stack.Push(Root);
		IndentStack.Push(0);

		while (Stack.Num() > 0)
		{
			UEdGraphNode* Node = Stack.Pop();
			int32 CurIndent = IndentStack.Pop();
			IndentLevel = CurIndent;

			if (Visited.Contains(Node)) continue;
			Visited.Add(Node);

			FString Line = ExportNodeToText(Node);
			if (!Line.IsEmpty())
			{
				Out += AppendIndent() + Line + TEXT("\n");

				for (const FExecLinkInfo& Link : CollectIncomingExecLinks(Node))
				{
					Out += AppendIndent()
						+ FString::Printf(TEXT("// Incoming Exec: %s.%s (NodeId: %s)\n"), *Link.NodeTitle, *Link.PinName, *Link.NodeId);
				}

				// 数据流：列出有连接且非 exec 的输入引脚及其来源
				TArray<FString> DataLines;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() == 0) continue;
					if (IsExecPin(Pin)) continue;
					UEdGraphPin* FromPin = Pin->LinkedTo[0];
					if (!FromPin) continue;
					UEdGraphNode* FromNode = FromPin->GetOwningNode();
					if (!FromNode) continue;
					FString FromLabel = GetNodeShortLabel(FromNode);
					DataLines.Add(FString::Printf(TEXT("%s = %s.%s"), *Pin->PinName.ToString(), *FromLabel, *FromPin->PinName.ToString()));
				}
				for (const FString& D : DataLines)
					Out += AppendIndent() + TEXT("// 数据: ") + D + TEXT("\n");
			}

			// 根据节点类型决定“下一步”顺序
			TArray<UEdGraphPin*> ExecOuts;
			GetExecOutputPins(Node, ExecOuts);

			UK2Node_IfThenElse* BranchNode = Cast<UK2Node_IfThenElse>(Node);
			UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(Node);

			if (BranchNode && ExecOuts.Num() >= 2)
			{
				// Branch: 先 True 再 False，输出成 if/else 结构（UE5.7 无 PN_True/PN_False，用引脚名查找）
				UEdGraphPin* PinTrue = Node->FindPin(FName(TEXT("True")), EGPD_Output);
				UEdGraphPin* PinFalse = Node->FindPin(FName(TEXT("False")), EGPD_Output);
				if (!PinTrue || !PinFalse)
				{
					// 兜底：按引脚顺序（通常 True 在前）
					PinTrue = ExecOuts.Num() > 0 ? ExecOuts[0] : nullptr;
					PinFalse = ExecOuts.Num() > 1 ? ExecOuts[1] : nullptr;
				}
				if (PinTrue)
				{
					Out += AppendIndent() + TEXT("// if (condition above) then:\n");
					TArray<UEdGraphNode*> NextTrue;
					GetNextNodesByExec(PinTrue, NextTrue);
					for (int32 i = NextTrue.Num() - 1; i >= 0; i--)
					{
						Stack.Push(NextTrue[i]);
						IndentStack.Push(CurIndent + 1);
					}
				}
				if (PinFalse)
				{
					Out += AppendIndent() + TEXT("// else:\n");
					TArray<UEdGraphNode*> NextFalse;
					GetNextNodesByExec(PinFalse, NextFalse);
					for (int32 i = NextFalse.Num() - 1; i >= 0; i--)
					{
						Stack.Push(NextFalse[i]);
						IndentStack.Push(CurIndent + 1);
					}
				}
				continue;
			}

			if (SeqNode)
			{
				// Sequence: 按 Then 0, Then 1, ... 顺序压栈（逆序压入以便按顺序弹出）
				TArray<UEdGraphPin*> ThenPins;
				for (UEdGraphPin* P : ExecOuts)
				{
					if (P && P->PinName.ToString().StartsWith(TEXT("Then")))
						ThenPins.Add(P);
				}
				ThenPins.Sort([](const UEdGraphPin& A, const UEdGraphPin& B) { return A.PinName.Compare(B.PinName) < 0; });
				for (int32 i = ThenPins.Num() - 1; i >= 0; i--)
				{
					TArray<UEdGraphNode*> Next;
					GetNextNodesByExec(ThenPins[i], Next);
					for (int32 j = Next.Num() - 1; j >= 0; j--)
					{
						Stack.Push(Next[j]);
						IndentStack.Push(CurIndent + 1);
					}
				}
				continue;
			}

			// 默认：按 exec 输出引脚顺序把下一批节点压栈（逆序以保持显示顺序）
			for (int32 i = ExecOuts.Num() - 1; i >= 0; i--)
			{
				TArray<UEdGraphNode*> Next;
				GetNextNodesByExec(ExecOuts[i], Next);
				for (int32 j = Next.Num() - 1; j >= 0; j--)
				{
					Stack.Push(Next[j]);
					IndentStack.Push(CurIndent + 1);
				}
			}
		}
	}

	// 未出现在任何执行链中的节点（孤立或仅数据连接）：列在末尾作为参考
	TArray<UEdGraphNode*> OrphanNodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || Visited.Contains(Node)) continue;
		OrphanNodes.Add(Node);
	}
	if (OrphanNodes.Num() > 0)
	{
		Out += TEXT("\n// --- 未在 exec 链中出现的节点（可能仅作数据或子图） ---\n");
		for (UEdGraphNode* Node : OrphanNodes)
		{
			FString Line = ExportNodeToText(Node);
			if (!Line.IsEmpty())
				Out += TEXT("// ") + Line + TEXT("\n");
		}
	}

	return Out;
}

FString FBlueprintToTextExporter::GetNodeShortLabel(UEdGraphNode* Node)
{
	if (!Node) return TEXT("?");
	FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	if (Title.IsEmpty()) Title = Node->GetName();

	if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		return FString::Printf(TEXT("Event_%s"), *EventNode->GetFunctionName().ToString());
	if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
	{
		UFunction* Func = CallNode->GetTargetFunction();
		return FString::Printf(TEXT("Call_%s"), Func ? *Func->GetName() : TEXT("?"));
	}
	if (Cast<UK2Node_IfThenElse>(Node)) return TEXT("Branch");
	if (Cast<UK2Node_ExecutionSequence>(Node)) return TEXT("Sequence");
	if (UK2Node_VariableGet* GetVar = Cast<UK2Node_VariableGet>(Node))
	{
		FProperty* Prop = static_cast<UK2Node_Variable*>(Node)->GetPropertyForVariable();
		return FString::Printf(TEXT("Get_%s"), Prop ? *Prop->GetName() : *Title);
	}
	if (UK2Node_VariableSet* SetVar = Cast<UK2Node_VariableSet>(Node))
	{
		FProperty* Prop = static_cast<UK2Node_Variable*>(Node)->GetPropertyForVariable();
		return FString::Printf(TEXT("Set_%s"), Prop ? *Prop->GetName() : *Title);
	}
	if (Cast<UK2Node_FunctionEntry>(Node)) return TEXT("Entry");
	if (Cast<UK2Node_FunctionResult>(Node)) return TEXT("Return");
	// 简短化：用类名 + 标题前几个字
	FString ClassName = Node->GetClass()->GetName();
	return ClassName + TEXT("_") + Title.Left(20);
}

FString FBlueprintToTextExporter::ExportNodeToText(UEdGraphNode* Node)
{
	if (!Node) return FString();

	UK2Node* K2Node = Cast<UK2Node>(Node);
	FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	if (Title.IsEmpty()) Title = Node->GetName();

	if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
	{
		const FString CommentText = CommentNode->NodeComment.IsEmpty() ? Title : CommentNode->NodeComment;
		return FString::Printf(TEXT("Comment Box: %s"), *CommentText);
	}
	if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		return FString::Printf(TEXT("Event: %s"), *EventNode->GetFunctionName().ToString());
	if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
	{
		UFunction* Func = CallNode->GetTargetFunction();
		return FString::Printf(TEXT("Call: %s"), Func ? *Func->GetName() : TEXT("?"));
	}
	if (Cast<UK2Node_IfThenElse>(Node))
		return TEXT("Branch ( condition ) -> True / False");
	if (Cast<UK2Node_VariableGet>(Node))
	{
		const UK2Node_Variable* VariableNode = static_cast<UK2Node_Variable*>(Node);
		FProperty* Prop = VariableNode->GetPropertyForVariable();
		FString VarName = Prop ? Prop->GetName() : Title;
		return FString::Printf(TEXT("Get Variable: %s %s"), *VarName, *FormatVariableScopeLabel(GetVariableAccessScopeInfo(VariableNode)));
	}
	if (Cast<UK2Node_VariableSet>(Node))
	{
		const UK2Node_Variable* VariableNode = static_cast<UK2Node_Variable*>(Node);
		FProperty* Prop = VariableNode->GetPropertyForVariable();
		FString VarName = Prop ? Prop->GetName() : Title;
		return FString::Printf(TEXT("Set Variable: %s %s"), *VarName, *FormatVariableScopeLabel(GetVariableAccessScopeInfo(VariableNode)));
	}
	if (Cast<UK2Node_ExecutionSequence>(Node))
		return TEXT("Sequence ( then 0, then 1, ... )");
	if (Cast<UK2Node_FunctionEntry>(Node))
		return FString::Printf(TEXT("FunctionEntry: %s"), *Title);
	if (Cast<UK2Node_FunctionResult>(Node))
		return TEXT("Return");
	if (K2Node)
		return FString::Printf(TEXT("[%s] %s"), *Node->GetClass()->GetName(), *Title);
	return FString::Printf(TEXT("[Node] %s"), *Title);
}

#undef LOCTEXT_NAMESPACE

#include "BlueprintRiskAnalyzer.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_VariableSet.h"
#include "UObject/UnrealType.h"

namespace GetTheMeaningBlueprintRisk
{
	bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	FString NodeId(const UEdGraphNode* Node)
	{
		return Node && Node->NodeGuid.IsValid() ? Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower) : FString();
	}

	FString NodeTitle(const UEdGraphNode* Node)
	{
		return Node ? Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString();
	}

	FString PinTypeString(const FEdGraphPinType& PinType)
	{
		FString Base = PinType.PinCategory.ToString();
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real && !PinType.PinSubCategory.IsNone())
		{
			Base = PinType.PinSubCategory.ToString();
		}
		if (const UObject* TypeObject = PinType.PinSubCategoryObject.Get())
		{
			if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(TypeObject))
			{
				Base = ScriptStruct->GetStructCPPName();
			}
			else
			{
				Base = TypeObject->GetName();
			}
		}
		Base = FBlueprintRiskAnalyzer::NormalizeTypeName(Base);
		switch (PinType.ContainerType)
		{
		case EPinContainerType::Array:
			return TEXT("TArray<") + Base + TEXT(">");
		case EPinContainerType::Set:
			return TEXT("TSet<") + Base + TEXT(">");
		case EPinContainerType::Map:
		{
			FString ValueType = PinType.PinValueType.TerminalCategory.ToString();
			if (const UObject* ValueObject = PinType.PinValueType.TerminalSubCategoryObject.Get())
			{
				if (const UScriptStruct* ValueStruct = Cast<UScriptStruct>(ValueObject))
				{
					ValueType = ValueStruct->GetStructCPPName();
				}
				else
				{
					ValueType = ValueObject->GetName();
				}
			}
			ValueType = FBlueprintRiskAnalyzer::NormalizeTypeName(ValueType);
			return TEXT("TMap<") + Base + TEXT(", ") + ValueType + TEXT(">");
		}
		default:
			return Base;
		}
	}

	const UEdGraphPin* ResolveLinkedSource(const UEdGraphPin* Pin)
	{
		const UEdGraphPin* SourcePin = Pin && Pin->LinkedTo.Num() > 0 ? Pin->LinkedTo[0] : nullptr;
		for (int32 Depth = 0; SourcePin && Depth < 32; ++Depth)
		{
			const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(SourcePin->GetOwningNode());
			if (!Knot)
			{
				break;
			}
			const UEdGraphPin* KnotInput = Knot->GetInputPin();
			SourcePin = KnotInput && KnotInput->LinkedTo.Num() > 0 ? KnotInput->LinkedTo[0] : nullptr;
		}
		return SourcePin;
	}

	bool IsPinSourcedFrom(const UEdGraphPin* InputPin, const UEdGraphPin* ExpectedSource)
	{
		return ResolveLinkedSource(InputPin) == ExpectedSource;
	}

	FString PinSource(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return FString();
		}
		if (const UEdGraphPin* SourcePin = ResolveLinkedSource(Pin))
		{
			const UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
			return FString::Printf(
				TEXT("%s.%s [%s]"),
				*NodeTitle(SourceNode),
				*SourcePin->PinName.ToString(),
				*NodeId(SourceNode));
		}
		if (Pin->DefaultObject)
		{
			return Pin->DefaultObject->GetPathName();
		}
		if (!Pin->DefaultTextValue.IsEmpty())
		{
			return Pin->DefaultTextValue.ToString();
		}
		return Pin->DefaultValue;
	}

	bool IsIdentityType(const FEdGraphPinType& PinType)
	{
		const UObject* TypeObject = PinType.PinSubCategoryObject.Get();
		return TypeObject && TypeObject->GetName().Equals(TEXT("Guid"), ESearchCase::IgnoreCase);
	}

	bool IsResultChecked(const UEdGraphPin* ResultPin)
	{
		if (!ResultPin)
		{
			return false;
		}
		for (const UEdGraphPin* LinkedPin : ResultPin->LinkedTo)
		{
			const UEdGraphNode* Target = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (Cast<UK2Node_IfThenElse>(Target)
				|| Cast<UK2Node_VariableSet>(Target)
				|| Cast<UK2Node_AssignmentStatement>(Target)
				|| Cast<UK2Node_FunctionResult>(Target))
			{
				return true;
			}
		}
		return false;
	}

	bool IsResultUsedAsBranchGuard(const UEdGraphPin* ResultPin)
	{
		if (!ResultPin)
		{
			return false;
		}
		for (const UEdGraphPin* LinkedPin : ResultPin->LinkedTo)
		{
			const UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(LinkedPin ? LinkedPin->GetOwningNode() : nullptr);
			if (Branch && LinkedPin == Branch->GetConditionPin())
			{
				return true;
			}
		}
		return false;
	}

	bool IsCriticalConsumer(const UEdGraphPin* SourcePin)
	{
		if (!SourcePin)
		{
			return false;
		}
		for (const UEdGraphPin* LinkedPin : SourcePin->LinkedTo)
		{
			const UEdGraphNode* Target = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (const UK2Node_VariableSet* VariableSet = Cast<UK2Node_VariableSet>(Target))
			{
				const FProperty* Property = VariableSet->GetPropertyForVariable();
				if (Property && Property->HasAnyPropertyFlags(CPF_Net | CPF_SaveGame))
				{
					return true;
				}
			}
			if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Target))
			{
				const UFunction* Function = Call->GetTargetFunction();
				const FString FunctionName = Function ? Function->GetName() : FString();
				if ((Function && Function->HasAnyFunctionFlags(FUNC_Net))
					|| FunctionName.Contains(TEXT("Map_Add"))
					|| FunctionName.Contains(TEXT("Set_Add"))
					|| FunctionName.Contains(TEXT("Array_Add")))
				{
					return true;
				}
			}
		}
		return false;
	}

	TArray<FString> TokenizeIdentifier(const FString& Name)
	{
		FString Expanded;
		Expanded.Reserve(Name.Len() * 2);
		for (int32 Index = 0; Index < Name.Len(); ++Index)
		{
			const TCHAR Character = Name[Index];
			const bool bAlphaNumeric = FChar::IsAlnum(Character);
			if (!bAlphaNumeric)
			{
				Expanded.AppendChar(TEXT(' '));
				continue;
			}
			if (Index > 0 && FChar::IsUpper(Character) && FChar::IsLower(Name[Index - 1]))
			{
				Expanded.AppendChar(TEXT(' '));
			}
			Expanded.AppendChar(Character);
		}

		TArray<FString> Tokens;
		Expanded.ParseIntoArrayWS(Tokens);
		return Tokens;
	}

	bool IsMapFindCall(const UK2Node_CallFunction* Call)
	{
		const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
		return Function && Function->GetName().Contains(TEXT("Map_Find"));
	}

	bool IsMapAddCall(const UK2Node_CallFunction* Call)
	{
		const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
		return Function && Function->GetName().Contains(TEXT("Map_Add"));
	}

	const UEdGraphPin* FindBoolOutput(const UEdGraphNode* Node)
	{
		if (!Node) return nullptr;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && !IsExecPin(Pin) && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	const UEdGraphPin* FindValueOutput(const UEdGraphNode* Node)
	{
		if (!Node) return nullptr;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && !IsExecPin(Pin) && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Boolean)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	const UEdGraphPin* FindExecInput(const UEdGraphNode* Node)
	{
		if (!Node) return nullptr;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && IsExecPin(Pin))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	const UEdGraphPin* FindExecOutput(const UEdGraphNode* Node)
	{
		if (!Node) return nullptr;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && IsExecPin(Pin))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	bool IsNodeReachableFromExecPin(
		const UEdGraphPin* StartExecPin,
		const UEdGraphNode* TargetNode,
		const UEdGraphNode* BlockedNode = nullptr)
	{
		if (!StartExecPin || !TargetNode)
		{
			return false;
		}

		TArray<const UEdGraphNode*> Queue;
		TSet<const UEdGraphNode*> Visited;
		for (const UEdGraphPin* LinkedPin : StartExecPin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode()) Queue.Add(LinkedPin->GetOwningNode());
		}

		for (int32 QueueIndex = 0; QueueIndex < Queue.Num() && QueueIndex < 512; ++QueueIndex)
		{
			const UEdGraphNode* Node = Queue[QueueIndex];
			if (!Node || Node == BlockedNode || Visited.Contains(Node)) continue;
			if (Node == TargetNode) return true;
			Visited.Add(Node);
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin)) continue;
				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode()) Queue.Add(LinkedPin->GetOwningNode());
				}
			}
		}
		return false;
	}

	TArray<const UK2Node_IfThenElse*> FindValidationBranches(const UEdGraph* Graph, const UEdGraphPin* SourcePin)
	{
		TArray<const UK2Node_IfThenElse*> Branches;
		if (!Graph || !SourcePin) return Branches;

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
			if (!Call) continue;

			bool bConsumesSource = false;
			const UFunction* Function = Call->GetTargetFunction();
			const FString FunctionName = Function ? Function->GetName() : FString();
			const UClass* OwnerClass = Function ? Function->GetOuterUClass() : nullptr;
			if (FBlueprintRiskAnalyzer::IsTrustedValidationFunction(FunctionName, OwnerClass ? OwnerClass->GetName() : FString()))
			{
				for (const UEdGraphPin* Pin : Call->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Input && !IsExecPin(Pin) && IsPinSourcedFrom(Pin, SourcePin))
					{
						bConsumesSource = true;
						break;
					}
				}
			}
			else if (IsMapFindCall(Call))
			{
				bConsumesSource = IsPinSourcedFrom(Call->FindPin(TEXT("Key"), EGPD_Input), SourcePin);
			}
			if (!bConsumesSource) continue;

			const UEdGraphPin* ResultPin = FindBoolOutput(Call);
			for (const UEdGraphNode* Candidate : Graph->Nodes)
			{
				const UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Candidate);
				if (Branch && ResolveLinkedSource(Branch->GetConditionPin()) == ResultPin)
				{
					Branches.AddUnique(Branch);
				}
			}
		}
		return Branches;
	}

	TArray<const UEdGraphNode*> CollectIdentityConsumers(
		const UEdGraphPin* SourcePin,
		const UK2Node_IfThenElse* GuardBranch)
	{
		TArray<const UEdGraphNode*> Consumers;
		TArray<const UEdGraphPin*> Queue;
		TSet<const UEdGraphPin*> VisitedPins;
		if (SourcePin) Queue.Add(SourcePin);

		for (int32 QueueIndex = 0; QueueIndex < Queue.Num() && QueueIndex < 512; ++QueueIndex)
		{
			const UEdGraphPin* OutputPin = Queue[QueueIndex];
			if (!OutputPin || VisitedPins.Contains(OutputPin)) continue;
			VisitedPins.Add(OutputPin);
			for (const UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
			{
				const UEdGraphNode* ConsumerNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!ConsumerNode || ConsumerNode == GuardBranch) continue;

				bool bHasExecPin = false;
				for (const UEdGraphPin* ConsumerPin : ConsumerNode->Pins)
				{
					bHasExecPin |= IsExecPin(ConsumerPin);
				}
				if (bHasExecPin)
				{
					Consumers.AddUnique(ConsumerNode);
					continue;
				}

				for (const UEdGraphPin* ConsumerPin : ConsumerNode->Pins)
				{
					if (ConsumerPin && ConsumerPin->Direction == EGPD_Output && !IsExecPin(ConsumerPin))
					{
						Queue.Add(ConsumerPin);
					}
				}
			}
		}
		return Consumers;
	}

	bool DoesBranchProtectConsumers(
		const UEdGraphPin* EntryExecPin,
		const UK2Node_IfThenElse* Branch,
		const TArray<const UEdGraphNode*>& Consumers)
	{
		if (!EntryExecPin || !Branch)
		{
			return false;
		}
		const bool bBranchReachable = IsNodeReachableFromExecPin(EntryExecPin, Branch);
		for (const UEdGraphNode* Consumer : Consumers)
		{
			if (!FBlueprintRiskAnalyzer::IsGuardTopologyProtected(
				bBranchReachable,
				IsNodeReachableFromExecPin(Branch->GetThenPin(), Consumer),
				IsNodeReachableFromExecPin(Branch->GetElsePin(), Consumer),
				IsNodeReachableFromExecPin(EntryExecPin, Consumer, Branch)))
			{
				return false;
			}
		}
		return true;
	}

	bool IsServerParameterValidated(const UEdGraph* Graph, const UEdGraphNode* EventNode, const UEdGraphPin* ParameterPin)
	{
		if (!Graph || !EventNode || !ParameterPin)
		{
			return false;
		}
		const UEdGraphPin* EntryExecPin = FindExecOutput(EventNode);
		for (const UK2Node_IfThenElse* Branch : FindValidationBranches(Graph, ParameterPin))
		{
			const TArray<const UEdGraphNode*> Consumers = CollectIdentityConsumers(ParameterPin, Branch);
			if (DoesBranchProtectConsumers(EntryExecPin, Branch, Consumers))
			{
				return true;
			}
		}
		return false;
	}

	bool IsRpcCallInputValidated(const UEdGraph* Graph, const UK2Node_CallFunction* RpcCall, const UEdGraphPin* InputPin)
	{
		const UEdGraphPin* SourcePin = ResolveLinkedSource(InputPin);
		if (!Graph || !RpcCall || !SourcePin) return false;

		for (const UK2Node_IfThenElse* Branch : FindValidationBranches(Graph, SourcePin))
		{
			const bool bThenReachesCall = IsNodeReachableFromExecPin(Branch->GetThenPin(), RpcCall);
			const bool bElseReachesCall = IsNodeReachableFromExecPin(Branch->GetElsePin(), RpcCall);
			if (!bThenReachesCall || bElseReachesCall)
			{
				continue;
			}

			bool bReachedFromEntry = false;
			bool bBypassExists = false;
			for (const UEdGraphNode* EntryNode : Graph->Nodes)
			{
				if (!Cast<UK2Node_Event>(EntryNode) && !Cast<UK2Node_FunctionEntry>(EntryNode)) continue;
				const UEdGraphPin* EntryExecPin = FindExecOutput(EntryNode);
				if (!IsNodeReachableFromExecPin(EntryExecPin, RpcCall)) continue;
				bReachedFromEntry = true;
				if (IsNodeReachableFromExecPin(EntryExecPin, RpcCall, Branch))
				{
					bBypassExists = true;
					break;
				}
			}
			if (FBlueprintRiskAnalyzer::IsGuardTopologyProtected(bReachedFromEntry, bThenReachesCall, bElseReachesCall, bBypassExists))
			{
				return true;
			}
		}
		return false;
	}

	void CollectStructFacts(UBlueprint* Blueprint, UEdGraph* Graph, UEdGraphNode* Node, const TArray<FString>& Patterns, FBlueprintRiskFacts& Facts)
	{
		const UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(Node);
		if (!StructNode || !StructNode->StructType)
		{
			return;
		}

		const bool bBreak = Cast<UK2Node_BreakStruct>(Node) != nullptr;
		const bool bWrite = Cast<UK2Node_MakeStruct>(Node) != nullptr || Cast<UK2Node_SetFieldsInStruct>(Node) != nullptr;
		if (!bBreak && !bWrite)
		{
			return;
		}

		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || IsExecPin(Pin)) continue;
			const bool bDirectionMatches = bBreak ? Pin->Direction == EGPD_Output : Pin->Direction == EGPD_Input;
			if (!bDirectionMatches || Pin->PinName == TEXT("StructRef") || Pin->PinName == TEXT("StructOut") || Pin->PinName == UEdGraphSchema_K2::PN_Self)
			{
				continue;
			}

			const FProperty* Property = FindFProperty<FProperty>(StructNode->StructType, Pin->PinName);
			if (!Property)
			{
				continue;
			}
			if (bBreak && Pin->LinkedTo.Num() == 0)
			{
				continue;
			}

			FStructFieldAccessFact Fact;
			Fact.AssetPath = Blueprint->GetPathName();
			Fact.GraphName = Graph->GetName();
			Fact.NodeId = NodeId(Node);
			Fact.NodeTitle = NodeTitle(Node);
			Fact.StructPath = StructNode->StructType->GetPathName();
			Fact.FieldName = Property->GetName();
			Fact.FieldType = FBlueprintRiskAnalyzer::NormalizeTypeName(Property->GetCPPType());
			Fact.SourceExpression = PinSource(Pin);
			Fact.bIdentityLike = IsIdentityType(Pin->PinType) || FBlueprintRiskAnalyzer::IsIdentityLikeName(Fact.FieldName, Patterns);
			Fact.bCriticalSink = bBreak && IsCriticalConsumer(Pin);
			Fact.AccessKind = bBreak
				? EProjectRiskAccessKind::Read
				: (Pin->LinkedTo.Num() > 0 ? EProjectRiskAccessKind::ExplicitWrite : EProjectRiskAccessKind::DefaultWrite);
			Facts.StructFieldAccesses.Add(MoveTemp(Fact));
		}
	}

	void CollectMapFindFact(UBlueprint* Blueprint, UEdGraph* Graph, UK2Node_CallFunction* Call, FBlueprintRiskFacts& Facts)
	{
		if (!IsMapFindCall(Call)) return;
		FMapFindFact Fact;
		Fact.AssetPath = Blueprint->GetPathName();
		Fact.GraphName = Graph->GetName();
		Fact.NodeId = NodeId(Call);
		Fact.NodeTitle = NodeTitle(Call);
		Fact.MapExpression = PinSource(Call->FindPin(TEXT("TargetMap"), EGPD_Input));
		Fact.KeyExpression = PinSource(Call->FindPin(TEXT("Key"), EGPD_Input));
		const UEdGraphPin* ValuePin = FindValueOutput(Call);
		Fact.ValueType = ValuePin ? PinTypeString(ValuePin->PinType) : FString();
		Fact.bResultChecked = IsResultChecked(FindBoolOutput(Call));
		if (ValuePin)
		{
			for (const UEdGraphPin* LinkedPin : ValuePin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode()) Fact.ValueConsumers.AddUnique(NodeTitle(LinkedPin->GetOwningNode()));
			}
		}
		Facts.MapFinds.Add(MoveTemp(Fact));
	}

	void CollectContainerFact(UBlueprint* Blueprint, UEdGraph* Graph, UK2Node_CallFunction* Call, const TArray<FString>& Patterns, FBlueprintRiskFacts& Facts)
	{
		if (!IsMapAddCall(Call)) return;
		const UEdGraphPin* MapPin = Call->FindPin(TEXT("TargetMap"), EGPD_Input);
		const UEdGraphPin* KeyPin = Call->FindPin(TEXT("Key"), EGPD_Input);
		const UEdGraphPin* ValuePin = Call->FindPin(TEXT("Value"), EGPD_Input);
		if (!MapPin || !KeyPin || !ValuePin || MapPin->PinType.ContainerType != EPinContainerType::Map)
		{
			return;
		}

		const UEdGraphPin* ValueSourcePin = ResolveLinkedSource(ValuePin);
		const UEdGraphNode* ValueNode = ValueSourcePin ? ValueSourcePin->GetOwningNode() : nullptr;
		const UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(ValueNode);
		const UScriptStruct* ValueStruct = StructNode
			? StructNode->StructType.Get()
			: Cast<UScriptStruct>(ValuePin->PinType.PinSubCategoryObject.Get());
		if (!ValueStruct)
		{
			return;
		}

		const FString KeyType = PinTypeString(KeyPin->PinType);
		for (TFieldIterator<const FProperty> It(ValueStruct); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property) continue;
			const UEdGraphPin* FieldPin = StructNode ? ValueNode->FindPin(Property->GetFName(), EGPD_Input) : nullptr;
			const FString FieldType = FBlueprintRiskAnalyzer::NormalizeTypeName(Property->GetCPPType());
			const bool bIdentityLike = (FieldPin && IsIdentityType(FieldPin->PinType)) || FBlueprintRiskAnalyzer::IsIdentityLikeName(Property->GetName(), Patterns);
			if (!bIdentityLike || KeyType != FieldType) continue;

			FContainerIdentityFact Fact;
			Fact.AssetPath = Blueprint->GetPathName();
			Fact.GraphName = Graph->GetName();
			Fact.NodeId = NodeId(Call);
			Fact.NodeTitle = NodeTitle(Call);
			Fact.ContainerExpression = PinSource(MapPin);
			Fact.KeyType = KeyType;
			Fact.ValueStructPath = ValueStruct->GetPathName();
			Fact.ValueFieldName = Property->GetName();
			Fact.ValueFieldType = FieldType;
			Fact.KeySource = PinSource(KeyPin);
			Fact.ValueSource = FieldPin ? PinSource(FieldPin) : FString();
			Fact.bIdentityLike = true;
			Fact.bValueUsesDefault = FBlueprintRiskAnalyzer::IsContainerValueDefault(
				ValuePin->LinkedTo.Num() > 0,
				FieldPin != nullptr,
				FieldPin && FieldPin->LinkedTo.Num() > 0);
			Facts.ContainerIdentities.Add(MoveTemp(Fact));
		}
	}

	void CollectRpcFacts(UBlueprint* Blueprint, UEdGraph* Graph, UK2Node_CustomEvent* Event, const TArray<FString>& Patterns, FBlueprintRiskFacts& Facts)
	{
		if (!Event || (Event->GetNetFlags() & FUNC_NetServer) == 0)
		{
			return;
		}
		for (const UEdGraphPin* Pin : Event->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin)) continue;
			const bool bIdentityLike = IsIdentityType(Pin->PinType) || FBlueprintRiskAnalyzer::IsIdentityLikeName(Pin->PinName.ToString(), Patterns);
			if (!bIdentityLike) continue;

			FRpcIdentityFact Fact;
			Fact.AssetPath = Blueprint->GetPathName();
			Fact.GraphName = Graph->GetName();
			Fact.NodeId = NodeId(Event);
			Fact.NodeTitle = NodeTitle(Event);
			Fact.RpcName = Event->CustomFunctionName.ToString();
			Fact.ParameterName = Pin->PinName.ToString();
			Fact.ParameterType = PinTypeString(Pin->PinType);
			Fact.bRunOnServer = true;
			Fact.bIdentityLike = true;
			Fact.bServerValidated = IsServerParameterValidated(Graph, Event, Pin);
			Facts.RpcIdentities.Add(MoveTemp(Fact));
		}
	}

	void CollectRpcCallFacts(UBlueprint* Blueprint, UEdGraph* Graph, UK2Node_CallFunction* Call, const TArray<FString>& Patterns, FBlueprintRiskFacts& Facts)
	{
		const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
		if (!Function || !Function->HasAnyFunctionFlags(FUNC_NetServer)) return;

		for (const UEdGraphPin* Pin : Call->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin) || Pin->PinName == UEdGraphSchema_K2::PN_Self) continue;
			const bool bIdentityLike = IsIdentityType(Pin->PinType) || FBlueprintRiskAnalyzer::IsIdentityLikeName(Pin->PinName.ToString(), Patterns);
			if (!bIdentityLike) continue;

			FRpcIdentityFact Fact;
			Fact.AssetPath = Blueprint->GetPathName();
			Fact.GraphName = Graph->GetName();
			Fact.NodeId = NodeId(Call);
			Fact.NodeTitle = NodeTitle(Call);
			Fact.RpcName = Function->GetName();
			Fact.ParameterName = Pin->PinName.ToString();
			Fact.ParameterType = PinTypeString(Pin->PinType);
			Fact.SourceExpression = PinSource(Pin);
			Fact.bRunOnServer = true;
			Fact.bCallSite = true;
			Fact.bIdentityLike = true;
			Fact.bCallerValidated = IsRpcCallInputValidated(Graph, Call, Pin);
			Facts.RpcIdentities.Add(MoveTemp(Fact));
		}
	}
}

FString FBlueprintRiskAnalyzer::NormalizeTypeName(const FString& TypeName)
{
	FString Normalized;
	FString Token;
	Normalized.Reserve(TypeName.Len() + 1);

	auto FlushToken = [&Normalized, &Token]()
	{
		if (Token.IsEmpty())
		{
			return;
		}
		if (Token.Equals(TEXT("Guid"), ESearchCase::IgnoreCase)) Normalized += TEXT("FGuid");
		else if (Token.Equals(TEXT("int"), ESearchCase::IgnoreCase)) Normalized += TEXT("int32");
		else if (Token.Equals(TEXT("name"), ESearchCase::IgnoreCase)) Normalized += TEXT("FName");
		else if (Token.Equals(TEXT("string"), ESearchCase::IgnoreCase)) Normalized += TEXT("FString");
		else if (Token.Equals(TEXT("text"), ESearchCase::IgnoreCase)) Normalized += TEXT("FText");
		else if (Token.Equals(TEXT("byte"), ESearchCase::IgnoreCase)) Normalized += TEXT("uint8");
		else Normalized += Token;
		Token.Reset();
	};

	for (const TCHAR Character : TypeName)
	{
		if (FChar::IsAlnum(Character) || Character == TEXT('_'))
		{
			Token.AppendChar(Character);
		}
		else
		{
			FlushToken();
			Normalized.AppendChar(Character);
		}
	}
	FlushToken();
	return Normalized;
}

bool FBlueprintRiskAnalyzer::IsTrustedValidationFunction(const FString& FunctionName, const FString& OwnerClassName)
{
	return (FunctionName.Equals(TEXT("IsValid_Guid"), ESearchCase::CaseSensitive)
			&& OwnerClassName.Equals(TEXT("KismetGuidLibrary"), ESearchCase::CaseSensitive))
		|| (FunctionName.Equals(TEXT("IsValid"), ESearchCase::CaseSensitive)
			&& OwnerClassName.Equals(TEXT("KismetSystemLibrary"), ESearchCase::CaseSensitive));
}

bool FBlueprintRiskAnalyzer::IsGuardTopologyProtected(
	bool bBranchReachable,
	bool bThenReachesConsumer,
	bool bElseReachesConsumer,
	bool bBypassExists)
{
	return bBranchReachable && bThenReachesConsumer && !bElseReachesConsumer && !bBypassExists;
}

bool FBlueprintRiskAnalyzer::IsContainerValueDefault(bool bValueConnected, bool bHasFieldPin, bool bFieldConnected)
{
	return !bValueConnected || (bHasFieldPin && !bFieldConnected);
}

bool FBlueprintRiskAnalyzer::IsIdentityLikeName(const FString& Name, const TArray<FString>& IdentityNamePatterns)
{
	const TArray<FString> Tokens = GetTheMeaningBlueprintRisk::TokenizeIdentifier(Name);
	for (const FString& Pattern : IdentityNamePatterns)
	{
		for (const FString& Token : Tokens)
		{
			if (Token.Equals(Pattern, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}
	return false;
}

FBlueprintRiskFacts FBlueprintRiskAnalyzer::Collect(UBlueprint* Blueprint, const TArray<FString>& IdentityNamePatterns)
{
	using namespace GetTheMeaningBlueprintRisk;

	FBlueprintRiskFacts Facts;
	if (!Blueprint)
	{
		return Facts;
	}
	Facts.AssetPath = Blueprint->GetPathName();

	TArray<UEdGraph*> Graphs;
	Graphs.Append(Blueprint->UbergraphPages);
	Graphs.Append(Blueprint->FunctionGraphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			CollectStructFacts(Blueprint, Graph, Node, IdentityNamePatterns, Facts);
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				CollectMapFindFact(Blueprint, Graph, Call, Facts);
				CollectContainerFact(Blueprint, Graph, Call, IdentityNamePatterns, Facts);
				CollectRpcCallFacts(Blueprint, Graph, Call, IdentityNamePatterns, Facts);
			}
			if (UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node))
			{
				CollectRpcFacts(Blueprint, Graph, Event, IdentityNamePatterns, Facts);
			}
		}
	}
	return Facts;
}

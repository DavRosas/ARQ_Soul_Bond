#include "MCPRegistry.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "EdGraphSchema_K2.h"

namespace
{


// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogUnrealMCP, Log, All);

TSharedPtr<FJsonObject> HandleSetupClimbZone(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleSetupSimpleClimb(const TSharedPtr<FJsonObject>& Params);

TSharedPtr<FJsonObject> HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
    }

    FString TargetNodeId;
    if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
    }

    FString SourcePinName;
    if (!Params->TryGetStringField(TEXT("source_pin"), SourcePinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_pin' parameter"));
    }

    FString TargetPinName;
    if (!Params->TryGetStringField(TEXT("target_pin"), TargetPinName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == SourceNodeId)
        {
            SourceNode = Node;
        }
        else if (Node->NodeGuid.ToString() == TargetNodeId)
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Source or target node not found"));
    }

    // Connect the nodes
    if (FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName))
    {
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("source_node_id"), SourceNodeId);
        ResultObj->SetStringField(TEXT("target_node_id"), TargetNodeId);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to connect nodes"));
}

TSharedPtr<FJsonObject> HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }
    
    // We'll skip component verification since the GetAllNodes API may have changed in UE5.5
    
    // Create the variable get node directly
    UK2Node_VariableGet* GetComponentNode = NewObject<UK2Node_VariableGet>(EventGraph);
    if (!GetComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create get component node"));
    }
    
    // Set up the variable reference properly for UE5.5
    FMemberReference& VarRef = GetComponentNode->VariableReference;
    VarRef.SetSelfMember(FName(*ComponentName));
    
    // Set node position
    GetComponentNode->NodePosX = NodePosition.X;
    GetComponentNode->NodePosY = NodePosition.Y;
    
    // Add to graph
    EventGraph->AddNode(GetComponentNode);
    GetComponentNode->CreateNewGuid();
    GetComponentNode->PostPlacedNewNode();
    GetComponentNode->AllocateDefaultPins();
    
    // Explicitly reconstruct node for UE5.5
    GetComponentNode->ReconstructNode();
    
    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), GetComponentNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the event node
    UK2Node_Event* EventNode = FUnrealMCPCommonUtils::CreateEventNode(EventGraph, EventName, NodePosition);
    if (!EventNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    if (FunctionName == TEXT("SetupClimbZoneGraph") || FunctionName == TEXT("SetupSimpleClimbNow"))
    {
        UE_LOG(LogTemp, Display, TEXT("setup_climb_SIMPLE_v2"));
        return HandleSetupSimpleClimb(Params);
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Check for target parameter (optional)
    FString Target;
    Params->TryGetStringField(TEXT("target"), Target);

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the function
    UFunction* Function = nullptr;
    UK2Node_CallFunction* FunctionNode = nullptr;
    
    // Add extensive logging for debugging
    UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in target '%s'"), 
           *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target);
    
    // Check if we have a target class specified
    if (!Target.IsEmpty())
    {
        // Try to find the target class
        UClass* TargetClass = nullptr;
        
        // First try without a prefix
        TargetClass = FindFirstObject<UClass>(*Target);
        UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
               *Target, TargetClass ? TEXT("Found") : TEXT("Not found"));
        
        // If not found, try with U prefix (common convention for UE classes)
        if (!TargetClass && !Target.StartsWith(TEXT("U")))
        {
            FString TargetWithPrefix = FString(TEXT("U")) + Target;
            TargetClass = FindFirstObject<UClass>(*TargetWithPrefix);
            UE_LOG(LogTemp, Display, TEXT("Tried to find class '%s': %s"), 
                   *TargetWithPrefix, TargetClass ? TEXT("Found") : TEXT("Not found"));
        }
        
        // If still not found, try with common component names
        if (!TargetClass)
        {
            // Try some common component class names
            TArray<FString> PossibleClassNames;
            PossibleClassNames.Add(FString(TEXT("U")) + Target + TEXT("Component"));
            PossibleClassNames.Add(Target + TEXT("Component"));
            
            for (const FString& ClassName : PossibleClassNames)
            {
                TargetClass = FindFirstObject<UClass>(*ClassName);
                if (TargetClass)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found class using alternative name '%s'"), *ClassName);
                    break;
                }
            }
        }
        
        // Special case handling for common classes like UGameplayStatics
        if (!TargetClass && Target == TEXT("UGameplayStatics"))
        {
            // For UGameplayStatics, use a direct reference to known class
            TargetClass = FindFirstObject<UClass>(TEXT("UGameplayStatics"));
            if (!TargetClass)
            {
                // Try loading it from its known package
                TargetClass = LoadObject<UClass>(nullptr, TEXT("/Script/Engine.GameplayStatics"));
                UE_LOG(LogTemp, Display, TEXT("Explicitly loading GameplayStatics: %s"), 
                       TargetClass ? TEXT("Success") : TEXT("Failed"));
            }
        }
        
        // If we found a target class, look for the function there
        if (TargetClass)
        {
            UE_LOG(LogTemp, Display, TEXT("Looking for function '%s' in class '%s'"), 
                   *FunctionName, *TargetClass->GetName());
                   
            // First try exact name
            Function = TargetClass->FindFunctionByName(*FunctionName);
            
            // If not found, try class hierarchy
            UClass* CurrentClass = TargetClass;
            while (!Function && CurrentClass)
            {
                UE_LOG(LogTemp, Display, TEXT("Searching in class: %s"), *CurrentClass->GetName());
                
                // Try exact match
                Function = CurrentClass->FindFunctionByName(*FunctionName);
                
                // Try case-insensitive match
                if (!Function)
                {
                    for (TFieldIterator<UFunction> FuncIt(CurrentClass); FuncIt; ++FuncIt)
                    {
                        UFunction* AvailableFunc = *FuncIt;
                        UE_LOG(LogTemp, Display, TEXT("  - Available function: %s"), *AvailableFunc->GetName());
                        
                        if (AvailableFunc->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive match: %s"), *AvailableFunc->GetName());
                            Function = AvailableFunc;
                            break;
                        }
                    }
                }
                
                // Move to parent class
                CurrentClass = CurrentClass->GetSuperClass();
            }
            
            // Special handling for known functions
            if (!Function)
            {
                if (TargetClass->GetName() == TEXT("GameplayStatics") && 
                    (FunctionName == TEXT("GetActorOfClass") || FunctionName.Equals(TEXT("GetActorOfClass"), ESearchCase::IgnoreCase)))
                {
                    UE_LOG(LogTemp, Display, TEXT("Using special case handling for GameplayStatics::GetActorOfClass"));
                    
                    // Create the function node directly
                    FunctionNode = NewObject<UK2Node_CallFunction>(EventGraph);
                    if (FunctionNode)
                    {
                        // Direct setup for known function
                        FunctionNode->FunctionReference.SetExternalMember(
                            FName(TEXT("GetActorOfClass")), 
                            TargetClass
                        );
                        
                        FunctionNode->NodePosX = NodePosition.X;
                        FunctionNode->NodePosY = NodePosition.Y;
                        EventGraph->AddNode(FunctionNode);
                        FunctionNode->CreateNewGuid();
                        FunctionNode->PostPlacedNewNode();
                        FunctionNode->AllocateDefaultPins();
                        
                        UE_LOG(LogTemp, Display, TEXT("Created GetActorOfClass node directly"));
                        
                        // List all pins
                        for (UEdGraphPin* Pin : FunctionNode->Pins)
                        {
                            UE_LOG(LogTemp, Display, TEXT("  - Pin: %s, Direction: %d, Category: %s"), 
                                   *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
                        }
                    }
                }
            }
        }
    }
    
    // If we still haven't found the function, try in the blueprint's class
    if (!Function && !FunctionNode)
    {
        UE_LOG(LogTemp, Display, TEXT("Trying to find function in blueprint class"));
        Function = Blueprint->GeneratedClass->FindFunctionByName(*FunctionName);
    }
    
    // Create the function call node if we found the function
    if (Function && !FunctionNode)
    {
        FunctionNode = FUnrealMCPCommonUtils::CreateFunctionCallNode(EventGraph, Function, NodePosition);
    }

    // Property setters / typed casts that are K2 nodes, not UFunctions.
    if (!Function && !FunctionNode)
    {
        auto ParseParamNumber = [](const TSharedPtr<FJsonObject>& AllParams, const FString& Key, FString& OutVal) -> bool
        {
            const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
            if (!AllParams->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj || !(*ParamsObj).IsValid())
            {
                return false;
            }
            double Num = 0.0;
            if ((*ParamsObj)->TryGetNumberField(Key, Num) || (*ParamsObj)->TryGetNumberField(TEXT("Value"), Num))
            {
                OutVal = FString::SanitizeFloat(Num);
                return true;
            }
            FString AsString;
            if ((*ParamsObj)->TryGetStringField(Key, AsString) || (*ParamsObj)->TryGetStringField(TEXT("Value"), AsString))
            {
                OutVal = AsString;
                return true;
            }
            return false;
        };

        auto FinishNewNode = [&](UEdGraphNode* NewNode) -> TSharedPtr<FJsonObject>
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString());
            return ResultObj;
        };

        if (FunctionName == TEXT("CastToCharacterMovementComponent"))
        {
            UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(EventGraph);
            CastNode->TargetType = UCharacterMovementComponent::StaticClass();
            CastNode->NodePosX = NodePosition.X;
            CastNode->NodePosY = NodePosition.Y;
            EventGraph->AddNode(CastNode);
            CastNode->CreateNewGuid();
            CastNode->PostPlacedNewNode();
            CastNode->AllocateDefaultPins();
            CastNode->SetPurity(true);
            CastNode->ReconstructNode();
            return FinishNewNode(CastNode);
        }

        if (FunctionName == TEXT("SetGravityScale") || FunctionName == TEXT("SetAirControl"))
        {
            const FName VarName = (FunctionName == TEXT("SetGravityScale"))
                ? FName(TEXT("GravityScale"))
                : FName(TEXT("AirControl"));

            UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(EventGraph);
            SetNode->VariableReference.SetExternalMember(VarName, UCharacterMovementComponent::StaticClass());
            SetNode->NodePosX = NodePosition.X;
            SetNode->NodePosY = NodePosition.Y;
            EventGraph->AddNode(SetNode);
            SetNode->CreateNewGuid();
            SetNode->PostPlacedNewNode();
            SetNode->AllocateDefaultPins();
            SetNode->ReconstructNode();

            FString DefaultVal;
            if (ParseParamNumber(Params, VarName.ToString(), DefaultVal))
            {
                if (UEdGraphPin* ValuePin = SetNode->FindPin(VarName))
                {
                    if (const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema()))
                    {
                        K2Schema->TrySetDefaultValue(*ValuePin, DefaultVal);
                    }
                    else
                    {
                        ValuePin->DefaultValue = DefaultVal;
                    }
                }
            }

            return FinishNewNode(SetNode);
        }
    }
    
    if (!FunctionNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s in target %s"), *FunctionName, Target.IsEmpty() ? TEXT("Blueprint") : *Target));
    }

    // Set parameters if provided
    if (Params->HasField(TEXT("params")))
    {
        const TSharedPtr<FJsonObject>* ParamsObj;
        if (Params->TryGetObjectField(TEXT("params"), ParamsObj))
        {
            // Process parameters
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Param : (*ParamsObj)->Values)
            {
                const FString& ParamName = Param.Key;
                const TSharedPtr<FJsonValue>& ParamValue = Param.Value;
                
                // Find the parameter pin
                UEdGraphPin* ParamPin = FUnrealMCPCommonUtils::FindPin(FunctionNode, ParamName, EGPD_Input);
                if (ParamPin)
                {
                    UE_LOG(LogTemp, Display, TEXT("Found parameter pin '%s' of category '%s'"), 
                           *ParamName, *ParamPin->PinType.PinCategory.ToString());
                    UE_LOG(LogTemp, Display, TEXT("  Current default value: '%s'"), *ParamPin->DefaultValue);
                    if (ParamPin->PinType.PinSubCategoryObject.IsValid())
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Pin subcategory: '%s'"), 
                               *ParamPin->PinType.PinSubCategoryObject->GetName());
                    }
                    
                    // Set parameter based on type
                    if (ParamValue->Type == EJson::String)
                    {
                        FString StringVal = ParamValue->AsString();
                        UE_LOG(LogTemp, Display, TEXT("  Setting string parameter '%s' to: '%s'"), 
                               *ParamName, *StringVal);
                        
                        // Handle class reference parameters (e.g., ActorClass in GetActorOfClass)
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
                        {
                            // For class references, we require the exact class name with proper prefix
                            // - Actor classes must start with 'A' (e.g., ACameraActor)
                            // - Non-actor classes must start with 'U' (e.g., UObject)
                            const FString& ClassName = StringVal;
                            
                            // TODO: This likely won't work in UE5.5+, so don't rely on it.
                            UClass* Class = FindFirstObject<UClass>(*ClassName);

                            if (!Class)
                            {
                                Class = LoadObject<UClass>(nullptr, *ClassName);
                                UE_LOG(LogUnrealMCP, Display, TEXT("FindObject<UClass> failed. Assuming soft path  path: %s"), *ClassName);
                            }
                            
                            // If not found, try with Engine module path
                            if (!Class)
                            {
                                FString EngineClassName = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
                                Class = LoadObject<UClass>(nullptr, *EngineClassName);
                                UE_LOG(LogUnrealMCP, Display, TEXT("Trying Engine module path: %s"), *EngineClassName);
                            }
                            
                            if (!Class)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to find class '%s'. Make sure to use the exact class name with proper prefix (A for actors, U for non-actors)"), *ClassName);
                                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find class '%s'"), *ClassName));
                            }

                            const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema());
                            if (!K2Schema)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to get K2Schema"));
                                return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get K2Schema"));
                            }

                            K2Schema->TrySetDefaultObject(*ParamPin, Class);
                            if (ParamPin->DefaultObject != Class)
                            {
                                UE_LOG(LogUnrealMCP, Error, TEXT("Failed to set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                                return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to set class reference for pin '%s'"), *ParamPin->PinName.ToString()));
                            }

                            UE_LOG(LogUnrealMCP, Log, TEXT("Successfully set class reference for pin '%s' to '%s'"), *ParamPin->PinName.ToString(), *ClassName);
                            continue;
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
                        {
                            bool BoolValue = ParamValue->AsBool();
                            ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                            UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                                   *ParamName, *ParamPin->DefaultValue);
                        }
                        else if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
                        {
                            // Handle array parameters - like Vector parameters
                            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                            if (ParamValue->TryGetArray(ArrayValue))
                            {
                                // Check if this could be a vector (array of 3 numbers)
                                if (ArrayValue->Num() == 3)
                                {
                                    // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                    float X = (*ArrayValue)[0]->AsNumber();
                                    float Y = (*ArrayValue)[1]->AsNumber();
                                    float Z = (*ArrayValue)[2]->AsNumber();
                                    
                                    FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                    ParamPin->DefaultValue = VectorString;
                                    
                                    UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                           *ParamName, *VectorString);
                                    UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                           *ParamPin->DefaultValue);
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                                }
                            }
                        }
                        else
                        {
                            // Name, String, Byte/enum, FKey, and other pin defaults.
                            // TrySetDefaultValue is required for FKey / Name; raw DefaultValue is not enough.
                            ParamPin->DefaultValue = StringVal;
                            if (const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema()))
                            {
                                K2Schema->TrySetDefaultValue(*ParamPin, StringVal);
                            }
                            UE_LOG(LogTemp, Display, TEXT("  Set generic default for '%s' to: '%s' (final '%s')"),
                                   *ParamName, *StringVal, *ParamPin->DefaultValue);
                        }
                    }
                    else if (ParamValue->Type == EJson::Number)
                    {
                        // Handle integer vs float parameters correctly
                        if (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
                        {
                            // Ensure we're using an integer value (no decimal)
                            int32 IntValue = FMath::RoundToInt(ParamValue->AsNumber());
                            ParamPin->DefaultValue = FString::FromInt(IntValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set integer parameter '%s' to: %d (string: '%s')"), 
                                   *ParamName, IntValue, *ParamPin->DefaultValue);
                        }
                        else
                        {
                            // For other numeric types
                            float FloatValue = ParamValue->AsNumber();
                            ParamPin->DefaultValue = FString::SanitizeFloat(FloatValue);
                            UE_LOG(LogTemp, Display, TEXT("  Set float parameter '%s' to: %f (string: '%s')"), 
                                   *ParamName, FloatValue, *ParamPin->DefaultValue);
                        }
                    }
                    else if (ParamValue->Type == EJson::Boolean)
                    {
                        bool BoolValue = ParamValue->AsBool();
                        ParamPin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
                        UE_LOG(LogTemp, Display, TEXT("  Set boolean parameter '%s' to: %s"), 
                               *ParamName, *ParamPin->DefaultValue);
                    }
                    else if (ParamValue->Type == EJson::Array)
                    {
                        UE_LOG(LogTemp, Display, TEXT("  Processing array parameter '%s'"), *ParamName);
                        // Handle array parameters - like Vector parameters
                        const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
                        if (ParamValue->TryGetArray(ArrayValue))
                        {
                            // Check if this could be a vector (array of 3 numbers)
                            if (ArrayValue->Num() == 3 && 
                                (ParamPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) &&
                                (ParamPin->PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get()))
                            {
                                // Create a proper vector string: (X=0.0,Y=0.0,Z=1000.0)
                                float X = (*ArrayValue)[0]->AsNumber();
                                float Y = (*ArrayValue)[1]->AsNumber();
                                float Z = (*ArrayValue)[2]->AsNumber();
                                
                                FString VectorString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), X, Y, Z);
                                ParamPin->DefaultValue = VectorString;
                                
                                UE_LOG(LogTemp, Display, TEXT("  Set vector parameter '%s' to: %s"), 
                                       *ParamName, *VectorString);
                                UE_LOG(LogTemp, Display, TEXT("  Final pin value: '%s'"), 
                                       *ParamPin->DefaultValue);
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Array parameter type not fully supported yet"));
                            }
                        }
                    }
                    // Add handling for other types as needed
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Parameter pin '%s' not found"), *ParamName);
                }
            }
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), FunctionNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_type' parameter"));
    }

    // Get optional parameters
    bool IsExposed = false;
    if (Params->HasField(TEXT("is_exposed")))
    {
        IsExposed = Params->GetBoolField(TEXT("is_exposed"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create variable based on type
    FEdGraphPinType PinType;
    
    // Set up pin type based on variable_type string
    if (VariableType == TEXT("Boolean"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (VariableType == TEXT("Integer") || VariableType == TEXT("Int"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType == TEXT("Float"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType == TEXT("String"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType == TEXT("Vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unsupported variable type: %s"), *VariableType));
    }

    // Create the variable
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType);

    // Set variable properties
    FBPVariableDescription* NewVar = nullptr;
    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == FName(*VariableName))
        {
            NewVar = &Variable;
            break;
        }
    }

    if (NewVar)
    {
        // Set exposure in editor
        if (IsExposed)
        {
            NewVar->PropertyFlags |= CPF_Edit;
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("variable_name"), VariableName);
    ResultObj->SetStringField(TEXT("variable_type"), VariableType);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the input action node
    UK2Node_InputAction* InputActionNode = FUnrealMCPCommonUtils::CreateInputActionNode(EventGraph, ActionName, NodePosition);
    if (!InputActionNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create input action node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), InputActionNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FUnrealMCPCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the self node
    UK2Node_Self* SelfNode = FUnrealMCPCommonUtils::CreateSelfReferenceNode(EventGraph, NodePosition);
    if (!SelfNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create self node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), SelfNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create a JSON array for the node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;
    
    // Filter nodes by the exact requested type
    if (NodeType == TEXT("Event"))
    {
        FString EventName;
        if (!Params->TryGetStringField(TEXT("event_name"), EventName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter for Event node search"));
        }
        
        // Look for nodes with exact event name (e.g., ReceiveBeginPlay)
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
            if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
            {
                UE_LOG(LogTemp, Display, TEXT("Found event node with name %s: %s"), *EventName, *EventNode->NodeGuid.ToString());
                NodeGuidArray.Add(MakeShared<FJsonValueString>(EventNode->NodeGuid.ToString()));
            }
        }
    }
    // Add other node types as needed (InputAction, etc.)
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("node_guids"), NodeGuidArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleSetupSimpleClimb(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName = TEXT("BP_ZonaEscaleraSimple");
    Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(EventGraph->GetSchema());
    EventGraph->Modify();
    Blueprint->Modify();

    TArray<UEdGraphNode*> ExistingNodes = EventGraph->Nodes;
    for (UEdGraphNode* Node : ExistingNodes)
    {
        if (Node)
        {
            EventGraph->RemoveNode(Node);
        }
    }

    auto FindFn = [](UClass* Class, const TCHAR* Name) -> UFunction*
    {
        return Class ? Class->FindFunctionByName(Name) : nullptr;
    };

    auto AddCall = [&](UClass* Class, const TCHAR* Name, float X, float Y) -> UK2Node_CallFunction*
    {
        UFunction* Fn = FindFn(Class, Name);
        if (!Fn)
        {
            UE_LOG(LogTemp, Error, TEXT("setup_climb_zone: missing function %s on %s"), Name, *Class->GetName());
            return nullptr;
        }
        return FUnrealMCPCommonUtils::CreateFunctionCallNode(EventGraph, Fn, FVector2D(X, Y));
    };

    auto SetDefault = [&](UEdGraphNode* Node, const TCHAR* PinName, const FString& Value)
    {
        if (!Node || !Schema)
        {
            return;
        }
        if (UEdGraphPin* Pin = FUnrealMCPCommonUtils::FindPin(Node, PinName, EGPD_Input))
        {
            Schema->TrySetDefaultValue(*Pin, Value);
            Pin->DefaultValue = Value;
        }
    };

    auto SetClassDefault = [&](UEdGraphNode* Node, const TCHAR* PinName, UClass* Class)
    {
        if (!Node || !Schema || !Class)
        {
            return;
        }
        if (UEdGraphPin* Pin = FUnrealMCPCommonUtils::FindPin(Node, PinName, EGPD_Input))
        {
            Schema->TrySetDefaultObject(*Pin, Class);
        }
    };

    auto Link = [&](UEdGraphNode* Source, const TCHAR* SourcePin, UEdGraphNode* Target, const TCHAR* TargetPin) -> bool
    {
        if (!Source || !Target)
        {
            return false;
        }
        return FUnrealMCPCommonUtils::ConnectGraphNodes(EventGraph, Source, SourcePin, Target, TargetPin);
    };

    auto AddEvent = [&](const TCHAR* Name, float X, float Y) -> UK2Node_Event*
    {
        UK2Node_Event* EventNode = FUnrealMCPCommonUtils::CreateEventNode(EventGraph, Name, FVector2D(X, Y));
        if (EventNode && !EventNode->NodeGuid.IsValid())
        {
            EventNode->CreateNewGuid();
        }
        return EventNode;
    };

    auto AddVarSet = [&](const TCHAR* VarName, const FString& Value, float X, float Y) -> UK2Node_VariableSet*
    {
        UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(EventGraph);
        SetNode->VariableReference.SetExternalMember(FName(VarName), UCharacterMovementComponent::StaticClass());
        SetNode->NodePosX = X;
        SetNode->NodePosY = Y;
        EventGraph->AddNode(SetNode);
        SetNode->CreateNewGuid();
        SetNode->PostPlacedNewNode();
        SetNode->AllocateDefaultPins();
        SetNode->ReconstructNode();
        SetDefault(SetNode, VarName, Value);
        return SetNode;
    };

    UK2Node_Event* BeginOverlap = AddEvent(TEXT("ReceiveActorBeginOverlap"), -600.f, 0.f);
    UK2Node_Event* Tick = AddEvent(TEXT("ReceiveTick"), -600.f, 700.f);
    UK2Node_Event* EndOverlap = AddEvent(TEXT("ReceiveActorEndOverlap"), -600.f, 350.f);

    UK2Node_CallFunction* GetPlayer = AddCall(UGameplayStatics::StaticClass(), TEXT("GetPlayerCharacter"), -200.f, 80.f);
    UK2Node_CallFunction* GetComp = AddCall(AActor::StaticClass(), TEXT("GetComponentByClass"), 80.f, 80.f);
    SetClassDefault(GetComp, TEXT("ComponentClass"), UCharacterMovementComponent::StaticClass());

    UK2Node_DynamicCast* CastCMC = NewObject<UK2Node_DynamicCast>(EventGraph);
    CastCMC->TargetType = UCharacterMovementComponent::StaticClass();
    CastCMC->NodePosX = 360;
    CastCMC->NodePosY = 40;
    EventGraph->AddNode(CastCMC);
    CastCMC->CreateNewGuid();
    CastCMC->PostPlacedNewNode();
    CastCMC->AllocateDefaultPins();
    CastCMC->SetPurity(true);
    CastCMC->ReconstructNode();

    UK2Node_CallFunction* ActOff = AddCall(UActorComponent::StaticClass(), TEXT("SetActive"), 620.f, 0.f);
    UK2Node_CallFunction* PrintIn = AddCall(UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 900.f, 0.f);
    SetDefault(ActOff, TEXT("bNewActive"), TEXT("false"));
    SetDefault(PrintIn, TEXT("InString"), TEXT("entrar"));
    UK2Node_VariableSet* Grav0 = AddVarSet(TEXT("GravityScale"), TEXT("0.0"), 1180.f, 0.f);
    UK2Node_VariableSet* Air1 = AddVarSet(TEXT("AirControl"), TEXT("1.0"), 1460.f, 0.f);

    UK2Node_CallFunction* ActOn = AddCall(UActorComponent::StaticClass(), TEXT("SetActive"), 620.f, 320.f);
    UK2Node_CallFunction* PrintOut = AddCall(UKismetSystemLibrary::StaticClass(), TEXT("PrintString"), 900.f, 320.f);
    SetDefault(ActOn, TEXT("bNewActive"), TEXT("true"));
    SetDefault(PrintOut, TEXT("InString"), TEXT("salir"));
    UK2Node_VariableSet* Grav1 = AddVarSet(TEXT("GravityScale"), TEXT("1.0"), 1180.f, 320.f);
    UK2Node_VariableSet* AirOut = AddVarSet(TEXT("AirControl"), TEXT("0.05"), 1460.f, 320.f);

    UK2Node_Self* SelfNode = FUnrealMCPCommonUtils::CreateSelfReferenceNode(EventGraph, FVector2D(-200.f, 760.f));
    UK2Node_CallFunction* IsOverlap = AddCall(AActor::StaticClass(), TEXT("IsOverlappingActor"), 40.f, 760.f);
    UK2Node_CallFunction* SelOverlap = AddCall(UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"), 320.f, 760.f);
    SetDefault(SelOverlap, TEXT("A"), TEXT("1.0"));
    SetDefault(SelOverlap, TEXT("B"), TEXT("0.0"));

    UK2Node_CallFunction* GetPC = AddCall(UGameplayStatics::StaticClass(), TEXT("GetPlayerController"), -200.f, 980.f);
    UK2Node_CallFunction* KeyW = AddCall(APlayerController::StaticClass(), TEXT("IsInputKeyDown"), 80.f, 940.f);
    UK2Node_CallFunction* KeyS = AddCall(APlayerController::StaticClass(), TEXT("IsInputKeyDown"), 80.f, 1100.f);
    SetDefault(KeyW, TEXT("Key"), TEXT("W"));
    SetDefault(KeyS, TEXT("Key"), TEXT("S"));
    UK2Node_CallFunction* SelW = AddCall(UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"), 320.f, 940.f);
    UK2Node_CallFunction* SelS = AddCall(UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"), 320.f, 1100.f);
    SetDefault(SelW, TEXT("A"), TEXT("1.0"));
    SetDefault(SelW, TEXT("B"), TEXT("0.0"));
    SetDefault(SelS, TEXT("A"), TEXT("-1.0"));
    SetDefault(SelS, TEXT("B"), TEXT("0.0"));

    UK2Node_CallFunction* AddWS = AddCall(UKismetMathLibrary::StaticClass(), TEXT("Add_DoubleDouble"), 560.f, 1000.f);
    UK2Node_CallFunction* MulOv = AddCall(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"), 780.f, 860.f);
    UK2Node_CallFunction* MulSpd = AddCall(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"), 1000.f, 860.f);
    UK2Node_CallFunction* MulDt = AddCall(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"), 1220.f, 860.f);
    SetDefault(MulSpd, TEXT("B"), TEXT("250.0"));

    UK2Node_CallFunction* GetUp = AddCall(AActor::StaticClass(), TEXT("GetActorUpVector"), 1440.f, 980.f);
    UK2Node_CallFunction* MulVec = AddCall(UKismetMathLibrary::StaticClass(), TEXT("Multiply_VectorFloat"), 1660.f, 860.f);
    if (!MulVec)
    {
        MulVec = AddCall(UKismetMathLibrary::StaticClass(), TEXT("Multiply_VectorDouble"), 1660.f, 860.f);
    }

    UK2Node_CallFunction* AddOffset = AddCall(AActor::StaticClass(), TEXT("K2_AddActorWorldOffset"), 1900.f, 700.f);
    SetDefault(AddOffset, TEXT("bSweep"), TEXT("false"));
    SetDefault(AddOffset, TEXT("bTeleport"), TEXT("true"));

    if (!BeginOverlap || !Tick || !EndOverlap || !GetPlayer || !GetComp || !CastCMC
        || !ActOff || !PrintIn || !Grav0 || !Air1 || !ActOn || !PrintOut || !Grav1 || !AirOut
        || !SelfNode || !IsOverlap || !SelOverlap || !GetPC || !KeyW || !KeyS || !SelW || !SelS || !AddWS
        || !MulOv || !MulSpd || !MulDt || !GetUp || !MulVec || !AddOffset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("setup_climb_zone: failed to create one or more nodes"));
    }

    auto AsCMC = TEXT("AsCharacter Movement Component");

    Link(BeginOverlap, TEXT("then"), ActOff, TEXT("execute"));
    Link(GetPlayer, TEXT("ReturnValue"), GetComp, TEXT("self"));
    Link(GetComp, TEXT("ReturnValue"), CastCMC, TEXT("Object"));
    Link(CastCMC, AsCMC, ActOff, TEXT("self"));
    Link(ActOff, TEXT("then"), PrintIn, TEXT("execute"));
    Link(PrintIn, TEXT("then"), Grav0, TEXT("execute"));
    Link(CastCMC, AsCMC, Grav0, TEXT("self"));
    Link(Grav0, TEXT("then"), Air1, TEXT("execute"));
    Link(CastCMC, AsCMC, Air1, TEXT("self"));

    Link(EndOverlap, TEXT("then"), ActOn, TEXT("execute"));
    Link(CastCMC, AsCMC, ActOn, TEXT("self"));
    Link(ActOn, TEXT("then"), PrintOut, TEXT("execute"));
    Link(PrintOut, TEXT("then"), Grav1, TEXT("execute"));
    Link(CastCMC, AsCMC, Grav1, TEXT("self"));
    Link(Grav1, TEXT("then"), AirOut, TEXT("execute"));
    Link(CastCMC, AsCMC, AirOut, TEXT("self"));

    Link(Tick, TEXT("then"), AddOffset, TEXT("execute"));
    Link(SelfNode, TEXT("self"), IsOverlap, TEXT("self"));
    Link(GetPlayer, TEXT("ReturnValue"), IsOverlap, TEXT("Other"));
    Link(IsOverlap, TEXT("ReturnValue"), SelOverlap, TEXT("bPickA"));
    Link(GetPC, TEXT("ReturnValue"), KeyW, TEXT("self"));
    Link(GetPC, TEXT("ReturnValue"), KeyS, TEXT("self"));
    Link(KeyW, TEXT("ReturnValue"), SelW, TEXT("bPickA"));
    Link(KeyS, TEXT("ReturnValue"), SelS, TEXT("bPickA"));
    Link(SelW, TEXT("ReturnValue"), AddWS, TEXT("A"));
    Link(SelS, TEXT("ReturnValue"), AddWS, TEXT("B"));
    Link(AddWS, TEXT("ReturnValue"), MulOv, TEXT("A"));
    Link(SelOverlap, TEXT("ReturnValue"), MulOv, TEXT("B"));
    Link(MulOv, TEXT("ReturnValue"), MulSpd, TEXT("A"));
    Link(MulSpd, TEXT("ReturnValue"), MulDt, TEXT("A"));
    Link(Tick, TEXT("DeltaSeconds"), MulDt, TEXT("B"));
    Link(SelfNode, TEXT("self"), GetUp, TEXT("self"));
    Link(GetUp, TEXT("ReturnValue"), MulVec, TEXT("A"));
    Link(MulDt, TEXT("ReturnValue"), MulVec, TEXT("B"));
    Link(MulVec, TEXT("ReturnValue"), AddOffset, TEXT("DeltaLocation"));
    Link(GetPlayer, TEXT("ReturnValue"), AddOffset, TEXT("self"));

    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (!Node)
        {
            continue;
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->LinkedTo.Num() > 0)
            {
                Node->PinConnectionListChanged(Pin);
            }
        }
    }

    if (UClass* Gen = Blueprint->GeneratedClass)
    {
        if (AActor* CDO = Cast<AActor>(Gen->GetDefaultObject()))
        {
            CDO->SetActorTickEnabled(true);
            CDO->PrimaryActorTick.bCanEverTick = true;
            CDO->PrimaryActorTick.bStartWithTickEnabled = true;
            if (UBoxComponent* Box = CDO->FindComponentByClass<UBoxComponent>())
            {
                Box->SetBoxExtent(FVector(80.f, 50.f, 280.f));
                Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Box->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
                Box->SetGenerateOverlapEvents(true);
                Box->SetHiddenInGame(false);
                Box->CanCharacterStepUpOn = ECB_No;
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("note"), TEXT("SIMPLE overlap W/S climb v2"));
    return Result;
}

TSharedPtr<FJsonObject> HandleSetupClimbZone(const TSharedPtr<FJsonObject>& Params)
{
    return HandleSetupSimpleClimb(Params);
}

}  // anonymous namespace


REGISTER_MCP_COMMAND("connect_blueprint_nodes", &HandleConnectBlueprintNodes);
REGISTER_MCP_COMMAND("add_blueprint_get_self_component_reference", &HandleAddBlueprintGetSelfComponentReference);
REGISTER_MCP_COMMAND("add_blueprint_event_node", &HandleAddBlueprintEvent);
REGISTER_MCP_COMMAND("add_blueprint_function_node", &HandleAddBlueprintFunctionCall);
REGISTER_MCP_COMMAND("add_blueprint_variable", &HandleAddBlueprintVariable);
REGISTER_MCP_COMMAND("add_blueprint_input_action_node", &HandleAddBlueprintInputActionNode);
REGISTER_MCP_COMMAND("add_blueprint_self_reference", &HandleAddBlueprintSelfReference);
REGISTER_MCP_COMMAND("find_blueprint_nodes", &HandleFindBlueprintNodes);
REGISTER_MCP_COMMAND("setup_climb_zone", &HandleSetupClimbZone);

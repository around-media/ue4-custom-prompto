// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "CompilerResultsLog.h"
#include "MessageLogModule.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "SourceCodeNavigation.h"
#include "Developer/HotReload/Public/IHotReload.h"

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "Editor.Stats"

const FName FCompilerResultsLog::Name(TEXT("CompilerResultsLog"));
FCompilerResultsLog* FCompilerResultsLog::CurrentEventTarget = nullptr;

//////////////////////////////////////////////////////////////////////////
// FCompilerResultsLog

/** Update the source backtrack map to note that NewObject was most closely generated/caused by the SourceObject */
void FBacktrackMap::NotifyIntermediateObjectCreation(UObject* NewObject, UObject* SourceObject)
{
	// Chase the source to make sure it's really a top-level ('source code') node
	while (UObject** SourceOfSource = SourceBacktrackMap.Find(Cast<UObject const>(SourceObject)))
	{
		SourceObject = *SourceOfSource;
	}

	// Record the backtrack link
	SourceBacktrackMap.Add(Cast<UObject const>(NewObject), SourceObject);
}

/** Returns the true source object for the passed in object */
UObject* FBacktrackMap::FindSourceObject(UObject* PossiblyDuplicatedObject)
{
	UObject** RemappedIfExisting = SourceBacktrackMap.Find(Cast<UObject const>(PossiblyDuplicatedObject));
	if (RemappedIfExisting != NULL)
	{
		return *RemappedIfExisting;
	}
	else
	{
		// Not in the map, must be an unduplicated object
		return PossiblyDuplicatedObject;
	}
}

UObject const* FBacktrackMap::FindSourceObject(UObject const* PossiblyDuplicatedObject)
{
	UObject** RemappedIfExisting = SourceBacktrackMap.Find(PossiblyDuplicatedObject);
	if (RemappedIfExisting != NULL)
	{
		return Cast<UObject const>(*RemappedIfExisting);
	}
	else
	{
		// Not in the map, must be an unduplicated object
		return PossiblyDuplicatedObject;
	}
}

//////////////////////////////////////////////////////////////////////////
// FCompilerResultsLog

FCompilerResultsLog::FCompilerResultsLog()
	: NumErrors(0)
	, NumWarnings(0)
	, bSilentMode(false)
	, bLogInfoOnly(false)
	, bAnnotateMentionedNodes(true)
	, bLogDetailedResults(false)
	, EventDisplayThresholdMs(0)
{
	CurrentEventScope = nullptr;
	if(CurrentEventTarget == nullptr)
	{
		CurrentEventTarget = this;
	}
}

FCompilerResultsLog::~FCompilerResultsLog()
{
	if(CurrentEventTarget == this)
	{
		CurrentEventTarget = nullptr;
	}
}

void FCompilerResultsLog::Register()
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing(Name, LOCTEXT("CompilerLog", "Compiler Log"));

	IHotReloadModule::Get().OnModuleCompilerFinished().AddStatic( &FCompilerResultsLog::GetGlobalModuleCompilerDump );
}

void FCompilerResultsLog::Unregister()
{
	IHotReloadModule::Get().OnModuleCompilerFinished().RemoveStatic( &FCompilerResultsLog::GetGlobalModuleCompilerDump );

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.UnregisterLogListing(Name);
}

void FCompilerResultsLog::InternalLogEvent(const FCompilerEvent& InEvent, int32 InDepth)
{
	const int EventTimeMs = (int)((InEvent.FinishTime - InEvent.StartTime) * 1000);
	if(EventTimeMs >= EventDisplayThresholdMs)
	{
		// Skip display of the top-most event since that time has already been logged
		if(InDepth > 0)
		{
			FString EventString = FString::Printf(TEXT("- %s"), *InEvent.Name);
			if(InEvent.Counter > 0)
			{
				EventString.Append(FString::Printf(TEXT(" (%d)"), InEvent.Counter + 1));
			}

			FFormatNamedArguments Args;
			Args.Add(TEXT("EventTimeMs"), EventTimeMs);
			EventString.Append(FText::Format(LOCTEXT("PerformanceSummaryEventTime", " [{EventTimeMs} ms]"), Args).ToString());

			FString IndentString = FString::Printf(TEXT("%*s"), (InDepth - 1) << 1, TEXT(""));
			Note(*FString::Printf(TEXT("%s%s"), *IndentString, *EventString));
		}

		const int32 NumChildEvents = InEvent.ChildEvents.Num();
		for(int32 i = 0; i < NumChildEvents; ++i)
		{
			InternalLogEvent(InEvent.ChildEvents[i].Get(), InDepth + 1);
		}
	}
}

void FCompilerResultsLog::InternalLogSummary()
{
	if(CurrentEventScope.IsValid())
	{
		const double CompileStartTime = CurrentEventScope->StartTime;
		const double CompileFinishTime = CurrentEventScope->FinishTime;

		FNumberFormattingOptions TimeFormat;
		TimeFormat.MaximumFractionalDigits = 2;
		TimeFormat.MinimumFractionalDigits = 2;
		TimeFormat.MaximumIntegralDigits = 4;
		TimeFormat.MinimumIntegralDigits = 4;
		TimeFormat.UseGrouping = false;

		FFormatNamedArguments Args;
		Args.Add(TEXT("Time"), FText::AsNumber(CompileFinishTime - GStartTime, &TimeFormat));
		Args.Add(TEXT("SourceName"), FText::FromString(SourceName));
		Args.Add(TEXT("NumWarnings"), NumWarnings);
		Args.Add(TEXT("NumErrors"), NumErrors);
		Args.Add(TEXT("TotalMilliseconds"), (int)((CompileFinishTime - CompileStartTime) * 1000));

		if (NumErrors > 0)
		{
			FString FailMsg = FText::Format(LOCTEXT("CompileFailed", "[{Time}] Compile of {SourceName} failed. {NumErrors} Fatal Issue(s) {NumWarnings} Warning(s) [in {TotalMilliseconds} ms]"), Args).ToString();
			Warning(*FailMsg);
		}
		else if(NumWarnings > 0)
		{
			FString WarningMsg = FText::Format(LOCTEXT("CompileWarning", "[{Time}] Compile of {SourceName} successful, but with {NumWarnings} Warning(s) [in {TotalMilliseconds} ms]"), Args).ToString();
			Warning(*WarningMsg);
		}
		else
		{
			FString SuccessMsg = FText::Format(LOCTEXT("CompileSuccess", "[{Time}] Compile of {SourceName} successful! [in {TotalMilliseconds} ms]"), Args).ToString();
			Note(*SuccessMsg);
		}

		if(bLogDetailedResults)
		{
			Note(*LOCTEXT("PerformanceSummaryHeading", "Performance summary:").ToString());
			InternalLogEvent(*CurrentEventScope.Get());
		}
	}
}

/** Update the source backtrack map to note that NewObject was most closely generated/caused by the SourceObject */
void FCompilerResultsLog::NotifyIntermediateObjectCreation(UObject* NewObject, UObject* SourceObject)
{
	SourceBacktrackMap.NotifyIntermediateObjectCreation(NewObject, SourceObject);
}

/** Returns the true source object for the passed in object */
UObject* FCompilerResultsLog::FindSourceObject(UObject* PossiblyDuplicatedObject)
{
	return SourceBacktrackMap.FindSourceObject(PossiblyDuplicatedObject);
}

UObject const* FCompilerResultsLog::FindSourceObject(UObject const* PossiblyDuplicatedObject)
{
	return SourceBacktrackMap.FindSourceObject(PossiblyDuplicatedObject);
}

/** Create a tokenized message record from a message containing @@ indicating where each UObject* in the ArgPtr list goes and place it in the MessageLog. */
void FCompilerResultsLog::InternalLogMessage(const EMessageSeverity::Type& Severity, const TCHAR* Message, va_list ArgPtr)
{
	UEdGraphNode* OwnerNode = nullptr;

	// Create the tokenized message
	TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create( Severity );
	Messages.Add(Line);

	const TCHAR* DelimiterStr = TEXT("@@");
	int32 DelimLength = FCString::Strlen(DelimiterStr);

	const TCHAR* Start = Message;
	if (Start && DelimLength)
	{
		while (const TCHAR* At = FCString::Strstr(Start, DelimiterStr))
		{
			// Found a delimiter, create a token from the preceding text
			Line->AddToken( FTextToken::Create( FText::FromString( FString(At - Start, Start) ) ) );
			Start += DelimLength + (At - Start);

			// And read the object and add another token for the object
			UObject* ObjectArgument = va_arg(ArgPtr, UObject*);

			FText ObjText;
			if (ObjectArgument)
			{
				// Remap object references to the source nodes
				ObjectArgument = FindSourceObject(ObjectArgument);

				if (ObjectArgument)
				{
					UEdGraphNode* Node = Cast<UEdGraphNode>(ObjectArgument);
					const UEdGraphPin* Pin = (Node? nullptr : Cast<UEdGraphPin>(ObjectArgument));

					//Get owner node reference, consider the first 
					if (OwnerNode == nullptr)
					{
						OwnerNode = (Pin ? Pin->GetOwningNodeUnchecked() : Node);
					}

					if (ObjectArgument->GetOutermost() == GetTransientPackage())
					{
						ObjText = LOCTEXT("Transient", "(transient)");					
					}
					else if (Node != NULL)
					{
						ObjText = Node->GetNodeTitle(ENodeTitleType::ListView);
					}
					else if (Pin != NULL)
					{
						ObjText = Pin->GetDisplayName();
					}
					else
					{
						ObjText = FText::FromString( ObjectArgument->GetName() );
					}
				}
				else
				{
					ObjText = LOCTEXT("None", "(none)");
				}

			}
			else
			{
				ObjText = LOCTEXT("None", "(none)");
			}
			
			Line->AddToken( FUObjectToken::Create( ObjectArgument, ObjText ) );
		}
		Line->AddToken( FTextToken::Create( FText::FromString( Start ) ) );
	}

	va_end(ArgPtr);

	// Register node error/warning.
	AnnotateNode(OwnerNode, Line);

	if( !bSilentMode && (!bLogInfoOnly || (Severity == EMessageSeverity::Info)) )
	{
		if(Severity == EMessageSeverity::CriticalError || Severity == EMessageSeverity::Error)
		{
			UE_LOG(LogBlueprint, Error, TEXT("[compiler] %s"), *Line->ToText().ToString());
		}
		else if(Severity == EMessageSeverity::Warning || Severity == EMessageSeverity::PerformanceWarning)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("[compiler] %s"), *Line->ToText().ToString());
		}
		else
		{
			UE_LOG(LogBlueprint, Log, TEXT("[compiler] %s"), *Line->ToText().ToString());
		}
	}
}

void FCompilerResultsLog::AnnotateNode(class UEdGraphNode* Node, TSharedRef<FTokenizedMessage> LogLine)
{
	if (Node != nullptr)
	{
		LogLine->SetMessageLink(FUObjectToken::Create(Node));

		if (bAnnotateMentionedNodes)
		{
			// Determine if this message is the first or more important than the previous one (only showing one error/warning per node for now)
			bool bUpdateMessage = true;
			if (Node->bHasCompilerMessage)
			{
				// Already has a message, see if we meet or trump the severity
				bUpdateMessage = LogLine->GetSeverity() <= Node->ErrorType;
			}
			else
			{
				Node->ErrorMsg.Empty();
			}

			// Update the message
			if (bUpdateMessage)
			{
				Node->ErrorType = (int32)LogLine->GetSeverity();
				Node->bHasCompilerMessage = true;

				FText FullMessage = LogLine->ToText();

				if (Node->ErrorMsg.IsEmpty())
				{
					Node->ErrorMsg = FullMessage.ToString();
				}
				else
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PreviousMessage"), FText::FromString(Node->ErrorMsg));
					Args.Add(TEXT("NewMessage"), FullMessage);
					Node->ErrorMsg = FText::Format(LOCTEXT("AggregateMessagesFormatter", "{PreviousMessage}\n{NewMessage}"), Args).ToString();
				}

				AnnotatedNodes.Add(Node);
			}
		}
	}	
}

TArray< TSharedRef<FTokenizedMessage> > FCompilerResultsLog::ParseCompilerLogDump(const FString& LogDump)
{
	TArray< TSharedRef<FTokenizedMessage> > Messages;

	TArray< FString > MessageLines;
	LogDump.ParseIntoArray(&MessageLines, TEXT("\n"), false);

	// delete any trailing empty lines
	for (int32 i = MessageLines.Num()-1; i >= 0; --i)
	{
		if (!MessageLines[i].IsEmpty())
		{
			if (i < MessageLines.Num() - 1)
			{
				MessageLines.RemoveAt(i+1, MessageLines.Num() - (i+1));
			}
			break;
		}
	}

	for (int32 i = 0; i < MessageLines.Num(); ++i)
	{
		FString Line = MessageLines[i];
		if (Line.EndsWith(TEXT("\r")))
		{
			Line = Line.LeftChop(1);
		}
		Line = Line.ConvertTabsToSpaces(4).TrimTrailing();

		// handle output line error message if applicable
		// @todo Handle case where there are parenthesis in path names
		// @todo Handle errors reported by Clang
		FString LeftStr, RightStr;
		FString FullPath, LineNumberString;
		if (Line.Split(TEXT(")"), &LeftStr, &RightStr, ESearchCase::CaseSensitive) &&
			LeftStr.Split(TEXT("("), &FullPath, &LineNumberString, ESearchCase::CaseSensitive) &&
			LineNumberString.IsNumeric() && (FCString::Strtoi(*LineNumberString, NULL, 10) > 0))
		{
			EMessageSeverity::Type Severity = EMessageSeverity::Error;
			FString FullPathTrimmed = FullPath;
			FullPathTrimmed.Trim();
			if (FullPathTrimmed.Len() != FullPath.Len()) // check for leading whitespace
			{
				Severity = EMessageSeverity::Info;
			}

			TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create( Severity );
			if ( Severity == EMessageSeverity::Info )	// add whitespace now
			{
				FString Whitespace = FullPath.Left(FullPath.Len() - FullPathTrimmed.Len());
				Message->AddToken( FTextToken::Create( FText::FromString( Whitespace ) ) );
				FullPath = FullPathTrimmed;
			}

			FString Link = FullPath + TEXT("(") + LineNumberString + TEXT(")");
			Message->AddToken( FTextToken::Create( FText::FromString( Link ) )->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&FCompilerResultsLog::OnGotoError) ) );
			Message->AddToken( FTextToken::Create( FText::FromString( RightStr ) ) );
			Messages.Add(Message);
		}
		else
		{
			EMessageSeverity::Type Severity = EMessageSeverity::Info;
			if (Line.Contains(TEXT("error LNK"), ESearchCase::CaseSensitive))
			{
				Severity = EMessageSeverity::Error;
			}

			TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create( Severity );
			Message->AddToken( FTextToken::Create( FText::FromString( Line ) ) );
			Messages.Add(Message);
		}
	}

	return Messages;
}

void FCompilerResultsLog::OnGotoError(const TSharedRef<IMessageToken>& Token)
{
	FString FullPath, LineNumberString;
	if (Token->ToText().ToString().Split(TEXT("("), &FullPath, &LineNumberString, ESearchCase::CaseSensitive))
	{
		LineNumberString = LineNumberString.LeftChop(1); // remove right parenthesis
		int32 LineNumber = FCString::Strtoi(*LineNumberString, NULL, 10);

		FSourceCodeNavigation::OpenSourceFile( FullPath, LineNumber );
	}
}

void FCompilerResultsLog::GetGlobalModuleCompilerDump(const FString& LogDump, ECompilationResult::Type CompilationResult, bool bShowLog)
{
	FMessageLog MessageLog(Name);

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("TimeStamp"), FText::AsDateTime(FDateTime::Now()));
	MessageLog.NewPage(FText::Format(LOCTEXT("CompilerLogPage", "Compilation - {TimeStamp}"), Arguments));

	if ( bShowLog )
	{
		MessageLog.Open();
	}

	MessageLog.AddMessages(ParseCompilerLogDump(LogDump));
}

// Note: Message is not a fprintf string!  It should be preformatted, but can contain @@ to indicate object references, which are the varargs
void FCompilerResultsLog::Error(const TCHAR* Message, ...)
{
	va_list ArgPtr;
	va_start(ArgPtr, Message);
	ErrorVA(Message, ArgPtr);
}

// Note: Message is not a fprintf string!  It should be preformatted, but can contain @@ to indicate object references, which are the varargs
void FCompilerResultsLog::Warning(const TCHAR* Message, ...)
{
	va_list ArgPtr;
	va_start(ArgPtr, Message);
	WarningVA(Message, ArgPtr);
}

// Note: Message is not a fprintf string!  It should be preformatted, but can contain @@ to indicate object references, which are the varargs
void FCompilerResultsLog::Note(const TCHAR* Message, ...)
{
	va_list ArgPtr;
	va_start(ArgPtr, Message);
	NoteVA(Message, ArgPtr);
}

void FCompilerResultsLog::ErrorVA(const TCHAR* Message, va_list ArgPtr)
{
	++NumErrors;
	InternalLogMessage(EMessageSeverity::Error, Message, ArgPtr);
}

void FCompilerResultsLog::WarningVA(const TCHAR* Message, va_list ArgPtr)
{
	++NumWarnings;
	InternalLogMessage(EMessageSeverity::Warning, Message, ArgPtr);
}

void FCompilerResultsLog::NoteVA(const TCHAR* Message, va_list ArgPtr)
{
	InternalLogMessage(EMessageSeverity::Info, Message, ArgPtr);
}

void FCompilerResultsLog::Append(FCompilerResultsLog const& Other)
{
	for (TSharedRef<FTokenizedMessage> const& Message : Other.Messages)
	{
		switch (Message->GetSeverity())
		{
		case EMessageSeverity::Warning:
			{
				++NumWarnings;
				break;
			}
				
		case EMessageSeverity::Error:
			{
				++NumErrors;
				break;
			}
		}
		Messages.Add(Message);
		
		UEdGraphNode* OwnerNode = nullptr;
		for (TSharedRef<IMessageToken> const& Token : Message->GetMessageTokens())
		{
			if (Token->GetType() != EMessageToken::Object)
			{
				continue;
			}
			
			FWeakObjectPtr ObjectPtr = ((FUObjectToken&)Token.Get()).GetObject();
			if (!ObjectPtr.IsValid())
			{
				continue;
			}
			UObject* ObjectArgument = ObjectPtr.Get();
			
			OwnerNode = Cast<UEdGraphNode>(ObjectArgument);
			if (UEdGraphPin const* Pin = Cast<UEdGraphPin>(ObjectArgument))
			{
				OwnerNode = Pin->GetOwningNodeUnchecked();
			}
		}
		AnnotateNode(OwnerNode, Message);
	}
}

void FCompilerResultsLog::BeginEvent(const TCHAR* InName)
{
	// Create a new event
	TSharedPtr<FCompilerEvent> ParentEventScope = CurrentEventScope;
	CurrentEventScope = MakeShareable(new FCompilerEvent(ParentEventScope));

	// Start event with given name
	CurrentEventScope->Start(InName);
}

void FCompilerResultsLog::EndEvent()
{
	if(CurrentEventScope.IsValid())
	{
		// Mark finish time
		CurrentEventScope->Finish();

		// Get the parent event scope
		TSharedPtr<FCompilerEvent> ParentEventScope = CurrentEventScope->ParentEventScope;
		if(ParentEventScope.IsValid())
		{
			// Aggregate the current event into the parent event scope
			TSharedRef<FCompilerEvent> ChildEvent = CurrentEventScope.ToSharedRef();
			AddChildEvent(ParentEventScope, ChildEvent);

			// Move current event scope back up to parent
			CurrentEventScope = ParentEventScope;
		}
		else
		{
			// Log results summary once we've ended the top-level event
			InternalLogSummary();

			// Reset current event scope
			CurrentEventScope = nullptr;
		}
	}
}

void FCompilerResultsLog::AddChildEvent(TSharedPtr<FCompilerEvent>& ParentEventScope, TSharedRef<FCompilerEvent>& ChildEventScope)
{
	if(ParentEventScope.IsValid())
	{
		// If we already have a matching parent scope, aggregate child events into the current parent scope
		if(ParentEventScope->Name == ChildEventScope->Name)
		{
			for(int i = 0; i < ChildEventScope->ChildEvents.Num(); ++i)
			{
				AddChildEvent(ParentEventScope, ChildEventScope->ChildEvents[i]);
			}
		}
		else
		{
			// Look for a matching sibling event under the current parent scope
			bool bMatchFound = false;
			for(int i = ParentEventScope->ChildEvents.Num() - 1; i >= 0 && !bMatchFound; --i)
			{
				// If we find a matching scope, combine this event with the existing one
				TSharedPtr<FCompilerEvent> ExistingChildEvent = ParentEventScope->ChildEvents[i];
				if(ExistingChildEvent->Name == ChildEventScope->Name)
				{
					bMatchFound = true;

					// Append timing and child event data to the existing event to create an aggregate event
					ExistingChildEvent->Counter += 1;
					ExistingChildEvent->FinishTime += (ChildEventScope->FinishTime - ChildEventScope->StartTime);
					for(int j = 0; j < ChildEventScope->ChildEvents.Num(); ++j)
					{
						AddChildEvent(ExistingChildEvent, ChildEventScope->ChildEvents[j]);
					}
				}
			}

			if(!bMatchFound)
			{
				// If we didn't find a matching event, append the current event to the list of child events under the current parent scope
				ParentEventScope->ChildEvents.Add(ChildEventScope);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

#endif	//#if !WITH_EDITOR

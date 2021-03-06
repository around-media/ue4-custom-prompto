// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "BlueprintEditor.h"

class KISMET_API SBlueprintEditorSelectedDebugObjectWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintEditorSelectedDebugObjectWidget){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InBlueprintEditor);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	/** Adds an object to the list of debug choices */
	void AddDebugObject(UObject* TestObject);
	void AddDebugObjectWithName(UObject* TestObject, const FString& TestObjectName);

private:
	UBlueprint* GetBlueprintObj() const { return BlueprintEditor.Pin()->GetBlueprintObj(); }

	/** Creates a list of all debug objects **/
	void GenerateDebugObjectNames(bool bRestoreSelection);

	/** Generate list of active PIE worlds to debug **/
	void GenerateDebugWorldNames(bool bRestoreSelection);

	/** Refresh the widget. **/
	void OnRefresh();

	/** Returns the name of the current debug actor */
	TSharedPtr<FString> GetDebugObjectName() const;

	/** Returns the name of the current debug actor */
	TSharedPtr<FString> GetDebugWorldName() const;

	/** Handles the selection changed event for the debug actor combo box */
	void DebugObjectSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	void DebugWorldSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Called when user clicks button to select the current object being debugged */
	void SelectedDebugObject_OnClicked();

	/** Returns true if a debug actor is currently selected */
	EVisibility IsSelectDebugObjectButtonVisible() const;

	EVisibility IsDebugWorldComboVisible() const;

	/* Returns the string to indicate no debug object is selected */
	const FString& GetNoDebugString() const;

	const FString& GetDebugAllWorldsString() const;

	TSharedRef<SWidget> OnGetActiveDetailSlotContent(bool bChangedToHighDetail);

	/** Helper method to construct a debug object label string */
	FString MakeDebugObjectLabel(UObject* TestObject, bool bAddContextIfSelectedInEditor) const;

	/** Called to create a widget for each debug object item */
	TSharedRef<SWidget> CreateDebugObjectItemWidget(TSharedPtr<FString> InItem);

	/** Returns the combo button label to use for the currently-selected debug object item */
	FText GetSelectedDebugObjectTextLabel() const;

private:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FBlueprintEditor> BlueprintEditor;

	/** Lists of actors of a given blueprint type and their names */
	TArray< TWeakObjectPtr<UObject> > DebugObjects;
	TArray< TSharedPtr<FString> > DebugObjectNames;

	/** PIE worlds that we can debug */
	TArray< TWeakObjectPtr<UWorld> > DebugWorlds;
	TArray< TSharedPtr<FString> > DebugWorldNames;

	/** Widget containing the names of all possible debug actors. This is a "generic" SComboBox rather than an STextComboBox so that we can customize the label on the combo button widget. */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> DebugObjectsComboBox;

	TSharedPtr<STextComboBox> DebugWorldsComboBox;

	TWeakObjectPtr<UObject> LastObjectObserved;
};

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SteamVREditor.h"
#include "IMotionController.h"
#include "Features/IModularFeatures.h"
#include "SteamVREditorStyle.h"
#include "SteamVREditorCommands.h"
#include "SteamVRControllerKeys.h"
#include "LevelEditor.h"

static const FName SteamVREditorTabName("SteamVREditor");

#define LOCTEXT_NAMESPACE "FSteamVREditorModule"

void FSteamVREditorModule::StartupModule()
{
	FSteamVREditorStyle::Initialize();
	FSteamVREditorStyle::ReloadTextures();

	FSteamVREditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	// Dummy action for main toolbar button
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::PluginButtonClicked),
		FCanExecuteAction());
	
	// Regenerate Action Manifest
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().JsonActionManifest,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::JsonRegenerateActionManifest),
		FCanExecuteAction());

	// Regenerate Controller Bindings
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().JsonControllerBindings,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::JsonRegenerateControllerBindings),
		FCanExecuteAction());

	// Reload Action Manifest
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().ReloadActionManifest,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::ReloadActionManifest),
		FCanExecuteAction());

	// Launch Bindings URL
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().LaunchBindingsURL,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::LaunchBindingsURL),
		FCanExecuteAction());
	
	// Add Sample Inputs
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().AddSampleInputs,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::AddSampleInputs),
		FCanExecuteAction());

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
	MenuExtender->AddMenuExtension("EditLocalTabSpawners", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FSteamVREditorModule::AddMenuExtension));

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FSteamVREditorModule::ShutdownModule()
{
	FSteamVREditorStyle::Shutdown();

	FSteamVREditorCommands::Unregister();
}

void FSteamVREditorModule::PluginButtonClicked()
{
	// Empty on purpose
}

void FSteamVREditorModule::JsonRegenerateActionManifest()
{
	USteamVRInputDeviceFunctionLibrary::RegenActionManifest();
}

void FSteamVREditorModule::JsonRegenerateControllerBindings()
{
	USteamVRInputDeviceFunctionLibrary::RegenControllerBindings();
}

void FSteamVREditorModule::ReloadActionManifest()
{
	USteamVRInputDeviceFunctionLibrary::ReloadActionManifest();
}

void FSteamVREditorModule::LaunchBindingsURL()
{
	USteamVRInputDeviceFunctionLibrary::LaunchBindingsURL();
}

void FSteamVREditorModule::AddSampleInputs()
{
	//// Get Existing Input Settings
	//auto DefaultInputSettings = GetDefault<UInputSettings>();
	//TArray<FInputAxisKeyMapping> ExistingAxisKeys = DefaultInputSettings->GetAxisMappings();
	//TArray<FInputActionKeyMapping> ExistingActionKeys = DefaultInputSettings->GetActionMappings();

	//// Create new Input Settings
	//UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();

	//if (InputSettings->IsValidLowLevel())
	//{
	//	// Teleport
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::Valve_Index_Controller_Trackpad_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::HTC_Vive_Controller_Trackpad_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::HTC_Cosmos_Controller_Joystick_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::Oculus_Touch_Controller_Joystick_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::Windows_MR_Controller_Trackpad_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), FGamepadKeyNames::MotionController_Left_Thumbstick);

	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::Valve_Index_Controller_Trackpad_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::HTC_Vive_Controller_Trackpad_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::HTC_Cosmos_Controller_Joystick_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::Oculus_Touch_Controller_Joystick_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::Windows_MR_Controller_Trackpad_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), FGamepadKeyNames::MotionController_Right_Thumbstick);

	//	// Grab
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::Valve_Index_Controller_Grip_Grab_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::HTC_Vive_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::HTC_Cosmos_Controller_Grip_Click_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::Oculus_Touch_Controller_Grip_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::Windows_MR_Controller_Grip_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), FGamepadKeyNames::MotionController_Left_Trigger);

	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::Valve_Index_Controller_Grip_Grab_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::HTC_Vive_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::HTC_Cosmos_Controller_Grip_Click_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::Oculus_Touch_Controller_Grip_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::Windows_MR_Controller_Grip_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), FGamepadKeyNames::MotionController_Right_Trigger);

	//	// Fire Arrow
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::Valve_Index_Controller_Pinch_Grab_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::HTC_Vive_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::HTC_Cosmos_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::Oculus_Touch_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::Windows_MR_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), FGamepadKeyNames::MotionController_Left_Trigger);

	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::Valve_Index_Controller_Pinch_Grab_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::HTC_Vive_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::HTC_Cosmos_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::Oculus_Touch_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::Windows_MR_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), FGamepadKeyNames::MotionController_Right_Trigger);

	//	// HMD Proximity
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("HeadsetOn")), GenericKeys::SteamVR_HMD_Proximity);

	//	// Move Right
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::Valve_Index_Controller_Thumbstick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::Windows_MR_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), FGamepadKeyNames::MotionController_Right_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::Valve_Index_Controller_Thumbstick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::Windows_MR_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), FGamepadKeyNames::MotionController_Right_Thumbstick_X);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::Valve_Index_Controller_Thumbstick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::Windows_MR_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), FGamepadKeyNames::MotionController_Left_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::Valve_Index_Controller_Thumbstick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::Windows_MR_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), FGamepadKeyNames::MotionController_Left_Thumbstick_X);

	//	// Teleport Direction
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::Valve_Index_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::Windows_MR_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), FGamepadKeyNames::MotionController_Right_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::Valve_Index_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::Windows_MR_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), FGamepadKeyNames::MotionController_Right_Thumbstick_X);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::Valve_Index_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::Windows_MR_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), FGamepadKeyNames::MotionController_Left_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::Valve_Index_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::Windows_MR_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), FGamepadKeyNames::MotionController_Left_Thumbstick_X);

	//	// Squeeze
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::Valve_Index_Controller_GripForce_Axis_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::HTC_Vive_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::HTC_Cosmos_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::Oculus_Touch_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::Windows_MR_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), FGamepadKeyNames::MotionController_Left_TriggerAxis);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::Valve_Index_Controller_GripForce_Axis_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::HTC_Vive_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::HTC_Cosmos_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::Oculus_Touch_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::Windows_MR_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), FGamepadKeyNames::MotionController_Right_TriggerAxis);
	//	

	//	// Update the config file
	//	InputSettings->SaveKeyMappings();
	//	InputSettings->UpdateDefaultConfigFile();
	//}
}

bool FSteamVREditorModule::AddUniqueAxisMapping(TArray<FInputAxisKeyMapping> ExistingAxisKeys, UInputSettings* InputSettings, FName ActionName, FKey ActionKey)
{
	// Create new axis mapping
	FInputAxisKeyMapping NewAxisMapping = FInputAxisKeyMapping(ActionName, ActionKey);

	// Check if this mapping already exists in the project
	if (ExistingAxisKeys.Find(NewAxisMapping) < 1)
	{
		// If none, create a new one
		InputSettings->AddAxisMapping(NewAxisMapping);
		return true;
	}
	
return false;
}

bool FSteamVREditorModule::AddUniqueActionMapping(TArray<FInputActionKeyMapping> ExistingActionKeys, UInputSettings* InputSettings, FName ActionName, FKey ActionKey)
{
	// Create new action mapping
	FInputActionKeyMapping NewActionMapping = FInputActionKeyMapping(ActionName, ActionKey);

	// Check if this mapping already exists in the project
	if (ExistingActionKeys.Find(NewActionMapping) < 1)
	{
		// If none, create a new one
		InputSettings->AddActionMapping(NewActionMapping);
		return true;
	}

	return false;
}

void FSteamVREditorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	FSteamVREditorStyle MenuStyle = FSteamVREditorStyle();
	MenuStyle.Initialize();

	Builder.AddSubMenu(
		LOCTEXT("SteamVRInputBtn", "SteamVR Input"),
		LOCTEXT("SteamVRInputBtnTootlip", "SteamVR Input"),
		FNewMenuDelegate::CreateRaw(this, &FSteamVREditorModule::MakeMenu),
		false, FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.PluginAction")
	);
}

void FSteamVREditorModule::MakeMenu(FMenuBuilder& MenuBuilder)
{
	//MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().PluginAction);
	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().JsonActionManifest, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.JsonActionManifest"));
	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().JsonControllerBindings, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.JsonControllerBindings"));
	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().ReloadActionManifest, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.ReloadActionManifest"));
	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().LaunchBindingsURL, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.LaunchBindingsURL"));
	//MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().AddSampleInputs, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.AddSampleInputs"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSteamVREditorModule, SteamVREditor)
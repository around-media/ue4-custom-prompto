// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AlembicHairTranslatorModule.h"

#include "AlembicHairTranslator.h"
#include "HairStrandsEditor.h"

IMPLEMENT_MODULE(FAlembicHairTranslatorModule, AlembicHairTranslatorModule);

void FAlembicHairTranslatorModule::StartupModule()
{
	FHairStrandsEditor::Get().RegisterHairTranslator<FAlembicHairTranslator>();
}

void FAlembicHairTranslatorModule::ShutdownModule()
{
}


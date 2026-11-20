#include "CkGridEditor_Module.h"

#include "CkGridEditor_Log.h"

#include "CkGridEditor/EdMode/Ck2dGridSystem_EdMode.h"
#include "CkGridEditor/EdMode/CkHexGrid_EdMode.h"
#include "CkGridEditor/EdMode/CkHexGrid_PaintService.h"
#include "CkGridEditor/Visualizer/Ck2dGridSystem_SpawnerVisualizer.h"

#include "Components/SceneComponent.h"
#include "CkGridEditor/Draw/Ck2dGridSystem_AuthoredOverlay.h"
#include "CkEntitySpawner/CkEntitySpawner_Actor.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "Editor/UnrealEdEngine.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "FCkGridEditorModule"

// --------------------------------------------------------------------------------------------------------------------

void FCkGridEditorModule::StartupModule()
{
    // The UEdMode registers by AUTO-DISCOVERY (UAssetEditorSubsystem::RegisterEditorModes iterates every
    // non-abstract UEdMode CDO at OnAllModuleLoadingPhasesComplete) — there is no RegisterMode() call for
    // UEdMode. This StaticClass() reference exists only so the linker cannot strip the TU, and with it the CDO.
    (void)UCk_2dGridSystem_EdMode::StaticClass();
    (void)UCk_HexGrid_EdMode::StaticClass();

    // GUnrealEd does not exist yet at the "Default" loading phase, so defer registration when it is absent.
    if (GUnrealEd != nullptr)
    {
        RegisterSpawnerVisualizer();
    }
    else
    {
        _PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(
            this, &FCkGridEditorModule::RegisterSpawnerVisualizer);
    }

    UE_LOG(CkGridEditor, Log, TEXT("CkGridEditor module started (Grid Paint mode auto-discovered)"));
}

void FCkGridEditorModule::RegisterSpawnerVisualizer()
{
    if (GUnrealEd == nullptr)
    { return; }

    // Idempotent: OnPostEngineInit may fire after a direct StartupModule registration in some configs.
    if (_SpawnerVisualizer.IsValid())
    { return; }

    ck::hex_grid_editor::StartupPainting();
    _SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(
        this, &FCkGridEditorModule::OnActorSelectionChanged);

    _SpawnerVisualizerName = USceneComponent::StaticClass()->GetFName();
    _SpawnerVisualizer     = MakeShareable(new FCk_2dGridSystem_SpawnerVisualizer());

    GUnrealEd->RegisterComponentVisualizer(_SpawnerVisualizerName, _SpawnerVisualizer);
    _SpawnerVisualizer->OnRegister();
}

auto FCkGridEditorModule::OnActorSelectionChanged(UObject* InSelection) -> void
{
    if (_SwitchingPaintMode || GEditor == nullptr || InSelection != GEditor->GetSelectedActors())
    { return; }
    auto& Modes = GLevelEditorModeTools();
    const auto SquareMode = Modes.IsModeActive(UCk_2dGridSystem_EdMode::EM_Ck2dGridSystemPaintModeId);
    const auto HexMode = Modes.IsModeActive(UCk_HexGrid_EdMode::EM_CkHexGridPaintModeId);
    if (NOT SquareMode && NOT HexMode)
    { return; }
    const auto SwitchingGuard = TGuardValue<bool>(_SwitchingPaintMode, true);
    const auto* Selected = GEditor->GetSelectedActors();
    for (auto Index = 0; Index < Selected->Num(); ++Index)
    {
        const auto* Spawner = Cast<ACk_EntitySpawner_UE>(Selected->GetSelectedObject(Index));
        if (ck::hex_grid_editor::ResolveSelection(Spawner).IsValid())
        {
            if (NOT HexMode)
            {
                Modes.DeactivateMode(UCk_2dGridSystem_EdMode::EM_Ck2dGridSystemPaintModeId);
                Modes.ActivateMode(UCk_HexGrid_EdMode::EM_CkHexGridPaintModeId);
            }
            return;
        }
        if (ck::grid_editor::Resolve_SpecFromSpawner(Spawner) != nullptr)
        {
            if (NOT SquareMode)
            {
                Modes.DeactivateMode(UCk_HexGrid_EdMode::EM_CkHexGridPaintModeId);
                Modes.ActivateMode(UCk_2dGridSystem_EdMode::EM_Ck2dGridSystemPaintModeId);
            }
            return;
        }
    }
}
void FCkGridEditorModule::ShutdownModule()
{
    USelection::SelectionChangedEvent.Remove(_SelectionChangedHandle);
    _SelectionChangedHandle.Reset();
    ck::hex_grid_editor::ShutdownPainting();

    if (_PostEngineInitHandle.IsValid())
    {
        FCoreDelegates::OnPostEngineInit.Remove(_PostEngineInitHandle);
        _PostEngineInitHandle.Reset();
    }

    if (GUnrealEd != nullptr && ! _SpawnerVisualizerName.IsNone())
    {
        GUnrealEd->UnregisterComponentVisualizer(_SpawnerVisualizerName);
    }
    _SpawnerVisualizer.Reset();

    // The auto-discovered UEdMode is unregistered by UAssetEditorSubsystem on OnEnginePreExit.
    UE_LOG(CkGridEditor, Log, TEXT("CkGridEditor module shut down"));
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkGridEditorModule, CkGridEditor)

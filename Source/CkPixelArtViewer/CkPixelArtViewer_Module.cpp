#include "CkPixelArtViewer_Module.h"

#include "CkPixelArtViewer/CkPixelArtViewer_Window.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkPixelSpriteEditor/CkPixelSpriteEditor_ViewerBridge.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "CkPixelArtViewer"

auto
    FCkPixelArtViewerModule::
    Get_TabId()
    -> FName
{
    return FName{TEXT("CkPixelArtViewer")};
}

auto
    FCkPixelArtViewerModule::
    Get()
    -> FCkPixelArtViewerModule&
{
    return FModuleManager::GetModuleChecked<FCkPixelArtViewerModule>(TEXT("CkPixelArtViewer"));
}

auto
    FCkPixelArtViewerModule::
    StartupModule()
    -> void
{
    auto& Spawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        Get_TabId(), FOnSpawnTab::CreateRaw(this, &FCkPixelArtViewerModule::Spawn_Tab))
        .SetDisplayName(LOCTEXT("TabTitle", "CK Pixel Art Viewer"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Inspect imported indexed pixel art and animation"));
    Spawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());

    _RegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkPixelArtViewer"), Get_TabId(),
        LOCTEXT("LauncherTitle", "[CK] Pixel Art Viewer"),
        LOCTEXT("LauncherTooltip", "Inspect authored pixel sprite frames, timing and palette"),
        ECk_Icon::TextureAsset, ECkDebuggerToolCategory::Tools, 55}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        {
            return Spawn_Tab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{Get_TabId()}});
        })));

    ck::pixel_sprite_editor::Get_OpenViewer().BindRaw(this, &FCkPixelArtViewerModule::Open_Asset);
    _PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkPixelArtViewerModule::Handle_PreExit);
}

auto
    FCkPixelArtViewerModule::
    ShutdownModule()
    -> void
{
    ck::pixel_sprite_editor::Get_OpenViewer().Unbind();
    if (_PreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_PreExitHandle);
        _PreExitHandle.Reset();
    }
    FCkDebuggerToolRegistry::Get().Unregister(Get_TabId(), _RegistrationId);
    _RegistrationId = 0;
    Close_Viewer();
    if (FGlobalTabmanager::Get()->HasTabSpawner(Get_TabId()))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Get_TabId());
    }
}

auto
    FCkPixelArtViewerModule::
    Open_Asset(
        UCk_PixelSpriteAsset_UE& InAsset,
        const FAssetOpenArgs&
    )
    -> bool
{
    _PendingAsset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{&InAsset};
    if (NOT Open_Viewer())
    {
        return false;
    }
    if (NOT _Window.IsValid())
    {
        return false;
    }
    if (_Window->Get_Asset() == &InAsset)
    {
        return true;
    }
    return _Window->Set_Asset(InAsset);
}

auto
    FCkPixelArtViewerModule::
    Open_Viewer()
    -> bool
{
    return ck::debugger_tabs::Invoke_DebuggerTab(Get_TabId()).IsValid();
}

auto
    FCkPixelArtViewerModule::
    Close_Viewer()
    -> void
{
    ck::debugger_tabs::Release_DebuggerTab(_Tab, true);
    _Window.Reset();
    _PendingAsset.Reset();
}

auto
    FCkPixelArtViewerModule::
    Get_Window() const
    -> TSharedPtr<SCk_PixelArtViewer_Window>
{
    return _Window;
}

auto
    FCkPixelArtViewerModule::
    Handle_PreExit()
    -> void
{
    ck::pixel_sprite_editor::Get_OpenViewer().Unbind();
    ck::debugger_tabs::Release_DebuggerTab(_Tab, false);
    _Window.Reset();
    _PendingAsset.Reset();
}

auto
    FCkPixelArtViewerModule::
    Spawn_Tab(
        const FSpawnTabArgs&
    )
    -> TSharedRef<SDockTab>
{
    _Window = SNew(SCk_PixelArtViewer_Window);
    if (_PendingAsset.IsValid())
    {
        _Window->Set_Asset(*_PendingAsset);
    }
    _Tab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("TabLabel", "CK Pixel Art Viewer"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _Window.Reset();
            _Tab.Reset();
            _PendingAsset.Reset();
        })
        [_Window.ToSharedRef()];
    return _Tab.ToSharedRef();
}

IMPLEMENT_MODULE(FCkPixelArtViewerModule, CkPixelArtViewer)

#undef LOCTEXT_NAMESPACE

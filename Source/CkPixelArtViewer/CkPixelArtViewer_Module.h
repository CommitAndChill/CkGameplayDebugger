#pragma once

#include "CoreMinimal.h"
#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"

class SCk_PixelArtViewer_Window;
class SDockTab;
struct FAssetOpenArgs;
class FSpawnTabArgs;

class FCkPixelArtViewerModule : public IModuleInterface
{
public:
    virtual auto
        StartupModule()
        -> void override;
    virtual auto
        ShutdownModule()
        -> void override;

    static auto
        Get()
        -> FCkPixelArtViewerModule&;
    static auto
        Get_TabId()
        -> FName;
    auto
        Open_Asset(
            UCk_PixelSpriteAsset_UE& InAsset,
            const FAssetOpenArgs& InArgs
        )
        -> bool;
    auto
        Open_Viewer()
        -> bool;
    auto
        Close_Viewer()
        -> void;
    auto
        Get_Window() const
        -> TSharedPtr<SCk_PixelArtViewer_Window>;

private:
    auto
        Spawn_Tab(
            const FSpawnTabArgs& InArgs
        )
        -> TSharedRef<SDockTab>;
    auto
        Handle_PreExit()
        -> void;

    TStrongObjectPtr<UCk_PixelSpriteAsset_UE> _PendingAsset;
    TSharedPtr<SCk_PixelArtViewer_Window> _Window;
    TSharedPtr<SDockTab> _Tab;
    FDelegateHandle _PreExitHandle;
    uint64 _RegistrationId = 0;
};

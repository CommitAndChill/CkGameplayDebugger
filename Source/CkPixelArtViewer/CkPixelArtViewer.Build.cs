using UnrealBuildTool;

public class CkPixelArtViewer : CkModuleRules
{
    public CkPixelArtViewer(ReadOnlyTargetRules Target) : base(Target, false)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore",
            "UnrealEd", "WorkspaceMenuStructure", "PropertyEditor", "DeveloperSettings", "DesktopPlatform", "CkCore", "CkPixelSprite",
            "CkPixelSpriteEditor", "CkDebuggerCommon", "CkEditorTools",
        });
        PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry", "Projects" });
    }
}

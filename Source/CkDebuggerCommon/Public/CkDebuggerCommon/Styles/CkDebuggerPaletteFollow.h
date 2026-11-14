#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;

// ====================================================================================================================
// A registered style set bakes the CkStyle palette into its brushes and widget styles when it is created. Following
// rebuilds a fresh set from the same factory when the palette flips between Light and Dark, and copies it INTO the
// live set's existing objects, so pointers widgets already hold stay valid. Procedural brushes only: an image brush
// keeps its resource, so whatever patched it in place keeps its patch.
// ====================================================================================================================

namespace ck::debug_style
{
    using FStyleSetFactory = TFunction<TSharedRef<FSlateStyleSet>()>;

    CKDEBUGGERCOMMON_API auto Follow_Palette(
        const TSharedRef<FSlateStyleSet>& InLiveStyle,
        FStyleSetFactory InFactory) -> void;

    // Idempotent; a window calls it before rebuilding on a flip so its text styles copy the new palette.
    CKDEBUGGERCOMMON_API auto Sync_Palette() -> void;

    CKDEBUGGERCOMMON_API auto Unfollow_Palette(
        const FSlateStyleSet& InLiveStyle) -> void;

    CKDEBUGGERCOMMON_API auto Request_CopyStyleInPlace(
        FSlateStyleSet& InLiveStyle,
        const FSlateStyleSet& InFreshStyle) -> void;
}

// ====================================================================================================================

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;

// ====================================================================================================================
// On a palette flip a fresh set is built and copied INTO the live set's objects, so pointers widgets hold stay valid.
// Procedural brushes only: an image brush keeps its resource, so an in-place patch survives.
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

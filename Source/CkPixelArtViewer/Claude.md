# CkPixelArtViewer

Editor-only pixel art inspection module. The asset definition in CkPixelSpriteEditor
opens this module through `ck::pixel_sprite_editor::Get_OpenViewer()`; the module
owns the `CkPixelArtViewer` nomad tab and a CkDebuggerCommon launcher entry.
Shutdown and engine pre-exit release the tab and unbind the bridge.

The window pins the imported asset, immutable manifest snapshot and a complete
transient `UCk_PixelSpriteResources_UE` texture generation. It stages a new
generation before replacing the visible one. Successful asset revisions refresh
all textures and the displayed brush; playback only switches source textures.
Failed rebuilds retain the last valid view. The canvas uses a persistent Slate
brush and pinned texture while drawing the image, checker, optional grid,
axes and crosshair. It clips drawing to its own bounds; side-by-side Fit includes
both sprites and their fixed gap. The palette readout comes from the immutable
source grid.

The presentation state keeps an absolute millisecond clock separate from authored
durations. Speed scales elapsed time; sampling quantizes presentation time; source
decimation selects every Nth frame after timing selection. Explicit frame steps
and thumbnail clicks pin the selected authored index until seek or playback,
including frames too brief to represent at the accumulated double time. Reverse
one-shot playback clamps and stops at zero. Asset revisions preserve an animation
selection by its case-sensitive name. The window retains one prepared immutable
timeline for its current snapshot revision, animation index and playback mode.
Seek, sampling, manual stepping and playback reuse it; selection, mode or asset
revision prepares a candidate before replacing the visible timeline.

The canvas previews cached textures with optional previous/following onion layers,
source-specific palette overrides, selection highlighting, and side-by-side,
overlay or pixel-difference comparison. Counts and exclusive difference bounds
come from the runtime grid comparator. A small transient indexed asset passes
each derived difference or selected-source override through the shared
`UCk_PixelSpriteResources_UE` upload path. Override generations are cached by
source until its override, render settings or asset revision changes; playback
never uploads on each tick.

The left panel lists animations and navigable source folders built from each
frame's relative authored source path. Clicking a leaf selects its first
referencing animation/frame; unreferenced leaves leave selection unchanged.
Absolute source paths appear in leaf tooltips. The three main panels have
resizable, normalized per-user widths; malformed saved widths fall back to a
usable layout. The right panel shows frame
anchors, source information, diagnostics, palette swatches and usage counts,
comparison statistics, onion offset controls and temporary RGBA overrides.
The thumbnail timeline shows frame numbers, authored durations and the playhead;
the adjacent readout and slider show total/current time and allow scrubbing.
The toolbar exposes render mode, scale, outline, background, onion and comparison
controls. The native editor tab can be maximized through Unreal docking; Fit and
Center/reset control the canvas view separately: Center keeps the current zoom
and centers the whole side-by-side comparison, while Reset restores the default
zoom and pan. Shortcuts are Space, arrows, Home, End,
F, G, O and R. Per-user canvas/onion/render preferences use GameUserSettings;
temporary palette overrides and authored data stay out of config.

Export frame calls the native new-file PNG API, with selected-source temporary
palette overrides. Export sheet calls the native new-file PNG+JSON API and
excludes those temporary overrides. Both surface structured export diagnostics.
The viewer remains an inspection tool and does not edit source XPM or JSON.

`Ck.PixelArtViewer.State` checks pure presentation operations. The
`Ck.PixelArtViewer.AuthoredWindow` NonNullRHI test imports the 16 by 16 Character
through the production factory, opens it through the asset definition bridge,
physically clicks viewer controls, checks known rendered sprite colors, and
checks that both compared sprites fit the canvas. It writes Slate screenshots to
`Saved/PixelSpriteAcceptance/Viewer-Advanced-Character.png` and
`Saved/PixelSpriteAcceptance/Viewer-Advanced-Clip.png`; the latter pans authored
cyan beyond the preview and checks the sidebar stays unpainted by the sprite.
The same mounted-widget test physically drags a splitter and reconstructs its
saved layout, exercises Center and Reset, navigates a nested source path, checks
unreferenced-source selection safety, and writes
`Saved/PixelSpriteAcceptance/Viewer-SourceHierarchy.png` for visual review.
A test discovery
result or NullRHI run alone does not establish visual correctness.

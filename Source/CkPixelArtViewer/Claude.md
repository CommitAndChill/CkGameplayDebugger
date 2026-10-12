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
Absolute source paths appear in leaf tooltips. Studio keeps three resizable panels; Focused uses the same preview widgets with
the browser hidden and an optional inspector. A layout dropdown preserves the
asset, animation, frame, clock, prepared timeline, palette overrides and texture
generation while changing the panel slots. Studio has normalized per-user widths; malformed saved widths fall back to a
usable layout. The right panel shows frame
anchors, source information, diagnostics, palette swatches and usage counts,
comparison statistics, onion offset controls and temporary RGBA overrides.
The time track uses authored duration-proportional blocks, fixed 56 px thumbnails
clipped per block, a seconds ruler and a single playhead. Its separate Fit/2x/4x/8x
zoom expands a horizontally scrolling track without changing canvas zoom. Click
and drag scrub the authored axis, preserving the current PingPong return visit
where that source frame has one. Loop wraps the displayed position; PingPong
return visits traverse the interior authored intervals backward. The absolute
playback clock and endpoint hold semantics remain unchanged. Sampling quantizes
the cursor; frame retention does not retime it. Manual steps pin the cursor at
the selected authored prefix. The adjacent readout shows authored current/total time.
The top asset toolbar exposes browse, reimport, export, animation and layout menus.
Viewport Show, Zoom, onion and comparison menus sit above the canvas. Rendering
settings live in a collapsible inspector; Details and Palette tabs share it.
Icon playback controls sit under the center timeline with mode, speed and overflow
menus for sampling, frame retention, Stop and Restart. Inspector visibility is
separate from the layout selection; switching to Studio restores its three panels. The native editor tab can be maximized through Unreal docking; Fit and
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

`Ck.PixelArtViewer.LayoutSwitch` physically uses the layout dropdown and inspector
toggle, verifies shared preview state and captures
`Saved/PixelSpriteAcceptance/Viewer-Studio.png` and `Viewer-Focused.png`.

`Ck.PixelArtViewer.TimeTrack` imports the eight-frame Sproutling through the
production factory, checks authored block proportions, fixed thumbnail size and
clipping/full reveal, physical zoom menu and scrub input, horizontal scrolling,
independent canvas zoom, layout retention, Loop wrap and PingPong return motion.
It captures `Sproutling-TimeTrack-Fit.png` and `Sproutling-TimeTrack-Zoom.png`
under `Saved/PixelSpriteAcceptance/`. Invalid timeline candidates leave the
current frames unchanged. The focused viewer gate passed 5/5 in TimelineGate6;
four mounted tests used the real renderer.

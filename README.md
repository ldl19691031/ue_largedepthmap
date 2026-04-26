# LargeDepthMap

LargeDepthMap is an Unreal Engine plugin for displaying very large height maps or depth maps inside UMG/Slate UI without loading the entire image into GPU memory.

It was built for cases where a scene capture or offline process produces a huge grayscale depth texture, for example `16384 x 16384` or `32768 x 32768`, and the UI needs to interact with it like a large map: pan, zoom, inspect detail, and switch mip levels smoothly.

<video src="Docs/Media/demo.mp4" controls width="100%"></video>

## Why This Exists

The original problem was straightforward: using a massive texture directly in UI is expensive and fragile. A single large `UTexture2D` can exceed practical import limits, consume excessive VRAM, and make zooming or panning inefficient because the renderer has to treat the image as one large resource.

Unreal's Virtual Texture system is useful in world rendering, but it is not a complete fit for this UMG/Slate use case. In UI, we needed more explicit control over which region is visible, which mip is active, when tile textures are loaded, and how much GPU memory is retained.

LargeDepthMap solves this by using a tiled data format and a custom Slate widget. The UI still behaves like one continuous image, but only the visible and recently used tiles are resident as small runtime textures.

## What It Does

- Imports a `.ldm.json` manifest into a `ULargeDepthMapAsset`.
- Packs tile binary data into the Data Asset bulk data.
- Asynchronously loads visible tiles from packed data.
- Creates small runtime `UTexture2D` resources on demand.
- Reuses textures through a simple texture pool.
- Keeps a configurable low-resolution fallback mip resident.
- Draws visible tiles in Slate with mip fallback.
- Supports artist-controlled UI materials through a `DepthTexture` texture parameter.
- Provides debug overlays for mip, tile state, GPU memory estimate, cache usage, and loading status.
- Supports both bilinear and point sampling for visual quality checks and pixel-resolution tests.

## Test Results

In local stress testing:

| Input size | Packed source data | Peak estimated GPU memory | Average estimated GPU memory |
| --- | ---: | ---: | ---: |
| `32768 x 32768` | `2.86 GB` | about `140 MB` | about `44 MB` |
| `16000+ x 16000+` | not recorded | about `120 MB` | about `44 MB` |

The important result is that runtime GPU memory stays tied to visible tile count and cache policy, not to the full source image size.

## Plugin Layout

```text
LargeDepthMap.uplugin
Source/
  LargeDepthMap/
  LargeDepthMapEditor/
Content/
  M_DepthVisualizeMat.uasset
  TestHeightMaps/
Docs/
  TechnicalReport.zh.md
  Media/demo.mp4
```

`LargeDepthMap` is the runtime module. `LargeDepthMapEditor` contains the manifest importer.

## Basic Workflow

1. Generate or prepare tiled depth-map data and a `.ldm.json` manifest.
2. Import the `.ldm.json` file in Unreal Editor.
3. Assign the imported `ULargeDepthMapAsset` to a `Large Depth Map View` widget.
4. Optionally assign a UI material that samples the `DepthTexture` parameter.
5. Use the widget in UMG and enable the debug overlay while tuning memory and streaming behavior.

The Python generator used during development lives in the original test project under `Tools/GenerateLargeDepthMapTiles.py`. It supports chunked generation for very large sizes, so it does not need to allocate a full `32768 x 32768` float array in memory.

## Material Notes

The material path is the recommended display path. The plugin creates a raw tile texture and assigns it to the material parameter named `DepthTexture` by default.

For UI materials, use the UI UV convention. In practice, the texture sample should use the Slate/UI UV, such as `GetUserInterfaceUV`'s `9-Slice UV` output, or the default UI texture sample UV. Do not use `Normalized UV` as the depth texture sampling coordinate when relying on tiled fallback, because that coordinate only describes the local Slate quad.

## Debugging

The status overlay reports:

- current mip
- visible, candidate, and ready tile counts
- retained, resident, active, free, pending, loading, and missing tile counts
- current, peak, and average estimated GPU memory
- packed data size
- request count, cache hits, created textures, and reused textures

Tile debug labels can also show the mip and tile coordinate currently being drawn, which is useful for finding fallback or UV mapping issues.

## Notes

The repository uses Git LFS for Unreal binary assets. The `32768 x 32768` stress-test Data Asset is not included because the generated `.uasset` is larger than GitHub's single-file LFS limit. The technical report in `Docs/TechnicalReport.zh.md` contains more implementation details and the recorded stress-test numbers.


# Offline loading (fork addition)

Goal: read assets straight from the game files on disk, without launching or attaching to the game.

## Survey: how each title is loaded today

Every title Greyhound supports is **memory-attach only** for its asset tables: `CoDAssets::BeginGameMode` finds the running
exe (`CoDAssets.cpp`, "find game database"), and each `Game*.cpp` reads the XAsset pools through `ProcessReader`.
Package files are only used for *payload* data next to the in-memory headers. The sole file-first path is
`CoDAssets::LoadFile` (drag & drop of a single `.xpak` / `.ipak` / `.iwd`): images and sounds only, no models, no anims.

| Title | Asset tables | Payload files | Offline today |
|---|---|---|---|
| WaW, BO1, MW, MW2, MW3 | game memory | `.iwd` (loose files) | `.iwd` drop (images/sounds) |
| BO2 | game memory | `.ipak` | `.ipak` drop (images) |
| **BO3**, BO4, BOCW | game memory | `.xpak` (BO3 streamed model/image data) | `.xpak` drop (images); **zone manifest: new, see below** |
| Ghosts, AW, MWR, MW2R, IW, WWII | game memory | `.pak` / `.ipak` | `.pak`/`.ipak` drop |
| MW4 (2019), MW5, MW6, Vanguard | game memory (+ Cordycep/"Parasyte" handler) | CASC / `.xpak` / CDN | CDN cache for payloads only |

Installed on the dev machine (read only survey): BO3 (core + zm zones only, no mp/cp zones), BO2, MW3, CoD2, CoD HQ (cod25/cod26).

## BO3 offline: what is implemented and verified

`BO3FastFile.h` (header only, std C++17, own inflate, no zlib, no MFC, no game process):

* container: `TAff0000`, version `0x251`, zone name at `0xF8`, 16 byte block headers `{comp, uncomp, stride, own offset}` from `0x248`,
  zlib blocks (no encryption), final block with uncompressed size 0 is the signature trailer;
* zone stream head (XAssetList): script strings + the manifest of all assets (type, pointer) in load order.

Verified on the retail `core_common.ff`: 70 blocks, 16 086 560 byte zone, 4 835 script strings, 39 087 assets
(7 355 xanim, 2 937 xmodel, 11 827 ximage, 68 rawfile, 586 fx ...). `zm_zod.ff`: 71 blocks, 44 967 assets, 4 290 xanim.

```
Tests\build_offline_cli.bat <outdir>                                  :: greyhound_offline.exe, MSVC build tools only
greyhound_offline.exe --zone-dir "<BO3>\zone" [--zones core_common,zm_zod] [--report out.json]
Greyhound.exe --headless --offline --list-only --zone-dir "<BO3>\zone" [--zones ...]      :: same code, exit 8 without --list-only
Tests\run_tests.bat                                                   :: set GREYHOUND_BO3_ZONE_DIR to add the retail check
```

Input files are only opened for reading.

## Not implemented (gap list)

* **Asset bodies** (XAnimParts, XModel, bind pose, skeleton): `--offline` without `--list-only` exits 8. The zone stream after the manifest is a
  sequential, byte-unaligned serialisation (pointer `-1` = data follows inline); there is no offset table. Reaching one XAnim requires the
  loader of every asset before it (in `core_common` the first 4 883 assets are type 5, then mixed types), i.e. struct layouts for ~20 asset types.
  Probes made: a naive scan for `BO3XAnim`-shaped headers (name ptr -1, frame rate float, bone counts) found nothing, and anim names
  are not present as strings in the stream (only script strings and xmodel names), so anim names need either the loaders or a hash dictionary.
* Names of any asset (only xmodel-like names appear inline), type ids other than 3/4/9/0x26/0x2F are not named.
* `.fd` files (TAff header, flag 6, 24 MB for core_common) and `.xpak` payload streams were not analysed for anim data.
* Other titles: no offline asset-table reader; BO2/MW3/WaW (older fastfile formats) and CoD2 (`.iwd`/loose) are the next candidates.
* The GUI exe glue in `HeadlessExport.cpp` could not be compiled here (no MFC component installed); the logic it calls is compiled and tested via the standalone targets.

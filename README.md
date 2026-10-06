# Doom 64 for PSP

This repository contains a PSP port of Doom 64 built around the original Doom engine with platform-specific code for the Sony PSP.

The project targets the PSP hardware and also supports deployment directly into a PPSSPP game directory via the included Makefile.

## Features

- PSP-specific rendering and input handling
- Custom platform backend under `src/psp/`
- Build target for a PSP EBOOT and PRX
- Deployment support for PPSSPP via `make deploy`
- Asset conversion utilities for Doom 64 content (`wadtool`, `doom64_hemigen`)

## Requirements

Before building, install the PSP toolchain:

- `PSPDEV`
- `pspsdk`
- GCC and the usual build tools for your host OS

The included `environ.sh` configures the environment for PSP development:

```bash
source ./environ.sh
```

## Building

From the repository root:

```bash
source ./environ.sh
make clean
make
```

This project builds the PSP binary and package assets, then copies the generated output to the configured PPSSPP game directory when the deployment target is reached.

You can also invoke the dedicated targets directly:

```bash
make prx
make eboot
make deploy
```

The default deployment path is configured in the Makefile as:

```bash
~/.config/ppsspp/PSP/GAME/DOOM64
```

You can override it by setting `PPSSPP_GAME_DIR` before running `make`:

```bash
make PPSSPP_GAME_DIR=/path/to/your/ppsspp/game
```

## Preparing game data

This project expects Doom 64 game data to be available in the `wadtool` directory before the conversion step completes.

Typical files used by the toolchain are:

```bash
wadtool/doom64.z64
wadtool/doom64.wad
```

The `wadtool` utility converts the original Doom 64 assets into the generated files used by the PSP build. The project also includes additional helper code in `doom64_hemigen` for generated assets and texture processing.

## Repository layout

```text
.
├── README.md
├── Makefile
├── environ.sh
├── EBOOT.PBP
├── doom64.prx
├── src/
│   ├── psp/
│   └── ...
├── wadtool/
├── doom64_hemigen/
└── selfboot/
```

## Running

- On real hardware: copy the generated PSP package to your memory stick and launch it.
- In PPSSPP: the Makefile can copy the built package into a PPSSPP game directory automatically.

## Notes

- This is a homebrew/port project and depends on original Doom 64 assets for gameplay content.
- Do not redistribute copyrighted game data without the appropriate rights.
- If you are troubleshooting build issues, make sure your PSP toolchain matches the project expectations and that the required asset files are present before running the conversion step.

## Credits

This codebase is based on the Doom 64 engine and adapted for the PSP platform. The project includes custom PSP backend code and support tooling for converting and packaging the game data.

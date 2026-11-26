# PrismLauncher Offline

A fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) that lets you add and launch offline Minecraft accounts without a Microsoft account.

## Changes from upstream

- Offline accounts can be added from the account manager or the first-run wizard without linking a Microsoft account first.
- Offline accounts launch the full game. The launcher no longer falls back to demo mode or asks for a Microsoft account that owns Minecraft.
- Version downloads and instance updates work without an account.
- Game metadata, libraries, assets, and Java runtimes use [mva.quad4.io](https://mva.quad4.io) by default. Each server can be changed in the API settings.
- Instance export can include the libraries, assets, metadata, and launcher-managed Java runtime needed to play offline. Importing one of these archives restores those shared files before creating the instance.
- Launches reuse existing verified libraries and download only missing files when possible.
- Java memory defaults scale with system memory, up to 4 GiB.
- The Flatpak uses Wayland when available, X11 as a fallback, GPU device access, and controller input device access instead of full device access.
- The updater points at this repository.
- CI builds Windows x64 and Linux x64 only. Releases include a Linux AppImage, a Flatpak bundle, a Linux portable tarball, and Windows zip and installer packages.
- Upstream project files such as the Code of Conduct, contribution guide, funding metadata, issue templates, and community badges were removed.

## Install

Download a build from the [releases page](https://github.com/Sudo-Ivan/PrismLauncher/releases).

Linux builds are x86_64 only. The AppImage and portable tarball are self-contained. The Flatpak bundle can be installed with:

```sh
flatpak install --user ./PrismLauncher-Linux-x86_64.flatpak
```

Windows builds are x86_64 only. Use the installer or the portable zip.

## Build

The build uses CMake and vcpkg, same as upstream:

```sh
git submodule update --init --recursive
cmake --preset default
cmake --build --preset default
```

Upstream build dependencies and platform notes are in the [Prism Launcher build instructions](https://prismlauncher.org/wiki/development/build-instructions).

## Offline mode limits

Offline accounts work in singleplayer and on servers that run with `online-mode=false`. They cannot join servers that require Mojang or Microsoft authentication, and they cannot use services that need a real session.

This fork does not provide Minecraft game files. You still need a licensed copy of Minecraft for the version you install.

## Disclaimer

This is an unofficial fork. It is not affiliated with, endorsed by, or supported by Prism Launcher, Mojang Studios, or Microsoft.

Minecraft is a trademark of Mojang Synergies AB. Microsoft and Xbox are trademarks of Microsoft Corporation.

The software is provided as is, without warranty of any kind. Use it at your own risk. Builds are unsigned unless a signing key is configured in CI.

## License

GPL-3.0-only. See [LICENSE](LICENSE).

The Prism Launcher name, logo, and related assets are covered by their own licenses. See [COPYING.md](COPYING.md).

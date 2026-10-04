<!--
SPDX-FileCopyrightText: none
SPDX-License-Identifier: CC0-1.0
-->

# Kleopatra for macOS

This is a port of [Kleopatra](https://apps.kde.org/kleopatra/), KDE's certificate manager
and graphical front-end for [GnuPG](https://gnupg.org/), to macOS. It's based on
upstream Kleopatra from [invent.kde.org/pim/kleopatra](https://invent.kde.org/pim/kleopatra),
and the macOS work lives on the `feat/macos` branch.

## What's different from upstream

- **Self-contained app:** the app bundle includes GnuPG 2.5 and a Qt based pinentry
  for passphrase prompts, so you don't need Homebrew, MacGPG or GPG Suite. The bundled
  GnuPG uses the standard `~/.gnupg` directory, so existing keys show up as usual.
- **Same look as on Linux and Windows:** Kleopatra uses KDE's Breeze style by default.
  You can switch to the native macOS style in Settings > Application Style.
- **macOS app icon:** the Kleopatra artwork on the rounded macOS icon tile, and proper
  bundle metadata (name, identifier `org.kde.kleopatra`, version).
- **Notepad in the main window:** an optional setting (Kleopatra menu > Settings >
  Appearance > General) shows the notepad in the main window, like older Kleopatra
  versions, instead of in a separate window. This option isn't macOS specific.

Everything else is upstream Kleopatra. The app uses the macOS menu bar.

## Installing

Download the disk image for your Mac from the [releases](../../releases): arm64 for
Apple silicon, x86_64 for Intel. Open it and drag Kleopatra to your Applications
folder. You can also build it yourself as described in
[packaging/macos/README.md](packaging/macos/README.md).

The app isn't signed or notarized yet, so macOS blocks it on the first start. To open
it anyway:

1. Open Kleopatra once and close the warning.
2. In System Settings > Privacy & Security, click Open Anyway next to the message
   about Kleopatra, and confirm.

Alternatively, remove the quarantine flag in Terminal:

```sh
xattr -dr com.apple.quarantine /Applications/kleopatra.app
```

## Building

See [packaging/macos/README.md](packaging/macos/README.md). Kleopatra is built with
[KDE Craft](https://community.kde.org/Craft), which provides Qt, KDE Frameworks and
the GnuPG libraries.

## Known limitations

- The app isn't code signed or notarized.
- Smartcards, keyserver lookups and running next to a gpg-agent from another GnuPG
  installation haven't been tested much yet.
- There's no Finder integration, such as context menu entries for encrypting files.

## License

Same as upstream Kleopatra: mostly GPL-2.0-or-later. The license texts are in
[LICENSES/](LICENSES/), and `REUSE.toml` and the SPDX headers say which applies to
which file.

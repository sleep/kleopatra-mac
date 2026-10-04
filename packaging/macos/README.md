<!--
SPDX-FileCopyrightText: none
SPDX-License-Identifier: CC0-1.0
-->

# Building Kleopatra for macOS

Kleopatra is built and packaged for macOS with [KDE Craft](https://community.kde.org/Craft).
Craft downloads most dependencies (Qt, KDE Frameworks and so on) prebuilt from KDE's
binary cache and builds the rest from source.

## Requirements

- macOS on Apple silicon (other setups haven't been tried)
- Xcode with its command line tools
- Python 3
- About 6 GB of disk space for Craft and the build

## Setting up Craft

```sh
curl -sSLO https://invent.kde.org/packaging/craft/raw/master/setup/CraftBootstrap.py
python3 CraftBootstrap.py --prefix ~/CraftRoot
source ~/CraftRoot/craft/craftenv.sh
```

Kleopatra's development branch needs the development versions of the other KDE PIM
libraries, and Craft should build Kleopatra from your checkout:

```sh
craft --set version=master kde/pim
craft --set srcDir=/path/to/kleopatra kde/pim/kleopatra
```

## Craft blueprint overlay

`craft-blueprints/` contains blueprints that replace some of the blueprints of
craft-blueprints-kde on macOS:

- GnuPG 2.5, which can determine its own location on macOS and can therefore be
  relocated with `gpgconf.ctl` when it is bundled in the app bundle
- libgcrypt and libassuan versions required by GnuPG 2.5
- pinentry-qt, which is bundled for asking for passphrases
- mimetreeparser, whose development branch also needs KIO and KService, which the
  blueprint of craft-blueprints-kde doesn't list yet
- Kleopatra itself, with lists of files to leave out of the app bundle
  (`blacklist_macos.txt`) and exceptions (`whitelist_macos.txt`). Craft puts all files
  of all dependencies into the bundle by default, including Python, multimedia and QML
  libraries, build tools and unrelated translations. The lists shrink the app to about
  a third of that.

Register the overlay in `~/CraftRoot/etc/CraftSettings.ini`:

```ini
[Blueprints]
Locations = /path/to/kleopatra/packaging/macos/craft-blueprints
```

Check that Craft uses it:

```sh
craft --search libs/gnupg   # BlueprintPath should point into packaging/macos
```

## Building and packaging

```sh
craft kde/pim/kleopatra
craft --package kde/pim/kleopatra
```

The package is a disk image in `~/CraftRoot/tmp/`.

If you leave something out of the bundle, check that all library references still
resolve, for example with `otool -L`, and that the app and the bundled GnuPG still
work (see Testing).

After changing the sources, rebuild with:

```sh
craft --compile kde/pim/kleopatra && craft --install kde/pim/kleopatra && craft --qmerge kde/pim/kleopatra
```

Add `craft --configure kde/pim/kleopatra` first after changing CMake files. Building
also installs KDE's clang-format pre-commit hook in your checkout. It needs
`git clang-format`, which Craft provides, so commit with `~/CraftRoot/bin` in `PATH`.

## How the bundled GnuPG works

Craft's packaging puts the GnuPG programs into `Contents/MacOS` and GnuPG's data into
`Contents/Resources`. GnuPG expects its programs in `<root>/bin` and `<root>/libexec`,
so the bundle also has:

- `Contents/bin` with `gpgconf.ctl` and symlinks to the GnuPG programs
- `Contents/libexec`, a symlink to `MacOS`
- `Contents/share`, a symlink to `Resources`

These are installed by `src/macos/CMakeLists.txt`. `gpgconf.ctl` needs an absolute
root directory, so at startup Kleopatra sets `KLEOPATRA_GNUPG_ROOTDIR` to the bundle's
`Contents` directory and puts `Contents/bin` first in `PATH`, where GpgME looks for
`gpgconf` (see `src/main.cpp`).

The app in `~/CraftRoot/Applications/KDE/` (not packaged) has no GnuPG in the bundle,
so the symlinks dangle and Kleopatra uses the GnuPG found in `PATH`, for example from
Homebrew.

## Testing

Use a separate GnuPG home directory so that tests don't touch your keys. Keep its path
short, because macOS limits the length of the socket paths GnuPG creates in it:

```sh
mkdir -m 700 /tmp/kleo-test
open -n --env GNUPGHOME=/tmp/kleo-test /path/to/Kleopatra.app
```

To check the bundled GnuPG directly:

```sh
APP=/path/to/Kleopatra.app/Contents
env -i HOME=$HOME PATH=$APP/bin:/usr/bin:/bin GNUPGHOME=/tmp/kleo-test \
    KLEOPATRA_GNUPG_ROOTDIR=$APP gpgconf --check-programs
```

All components should be listed with paths inside the app bundle.

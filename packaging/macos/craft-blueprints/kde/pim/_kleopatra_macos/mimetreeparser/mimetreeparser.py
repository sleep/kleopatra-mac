# SPDX-FileCopyrightText: g10 Code GmbH
# SPDX-FileContributor: Carl Schwan <carl.schwan@gnupg.com>
# SPDX-License-Identifier: BSD-2-Clause

from pathlib import Path

import info
import VersionInfo
from Blueprints.CraftPackageObject import CraftPackageObject
from Package.CMakePackageBase import CMakePackageBase


class subinfo(info.infoclass):
    def setTargets(self):
        # use the versions of the other KDE PIM packages of craft-blueprints-kde
        libkleo = CraftPackageObject.get("kde/pim/libkleo")
        self.versionInfo = VersionInfo.VersionInfo(subinfo=self, fileName=Path(libkleo.source).parent.parent / "version.ini")
        self.versionInfo.setDefaultValues()
        self.svnTargets["master"] = "https://invent.kde.org/pim/mimetreeparser.git"
        self.description = "Mime parsing and viewer library"

    def setDependencies(self):
        self.runtimeDependencies["virtual/base"] = None
        self.buildDependencies["kde/frameworks/extra-cmake-modules"] = None
        self.runtimeDependencies["libs/qt/qtbase"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kconfig"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kwidgetsaddons"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kcodecs"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kcalendarcore"] = None
        self.runtimeDependencies["kde/frameworks/tier2/kmime"] = None
        self.runtimeDependencies["kde/pim/kmbox"] = None
        self.runtimeDependencies["kde/pim/libkleo"] = None
        self.runtimeDependencies["libs/gpgme/gpgme"] = None
        # the master branch of mimetreeparser also needs KIO and KService
        self.runtimeDependencies["kde/frameworks/tier3/kio"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kservice"] = None


class Package(CMakePackageBase):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.subinfo.options.configure.args += ["-DUSE_UNITY_CMAKE_SUPPORT=ON"]

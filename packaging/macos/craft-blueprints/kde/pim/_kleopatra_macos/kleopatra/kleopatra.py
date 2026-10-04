# SPDX-FileCopyrightText: Hannah von Reth <vonreth@kde.org> and others
# SPDX-License-Identifier: BSD-2-Clause

from pathlib import Path

import info
import VersionInfo
from Blueprints.CraftPackageObject import CraftPackageObject
from Blueprints.CraftVersion import CraftVersion
from Package.CMakePackageBase import CMakePackageBase


class subinfo(info.infoclass):
    def setTargets(self):
        # use the versions of the other KDE PIM packages of craft-blueprints-kde
        libkleo = CraftPackageObject.get("kde/pim/libkleo")
        self.versionInfo = VersionInfo.VersionInfo(subinfo=self, fileName=Path(libkleo.source).parent.parent / "version.ini")
        self.versionInfo.setDefaultValues()

        self.description = "Kleopatra"

    def setDependencies(self):
        self.runtimeDependencies["virtual/base"] = None
        self.buildDependencies["kde/frameworks/extra-cmake-modules"] = None
        self.runtimeDependencies["libs/qt/qtbase"] = None
        self.runtimeDependencies["kde/frameworks/tier1/ki18n"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kwidgetsaddons"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kitemmodels"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kcodecs"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kcoreaddons"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kwindowsystem"] = None
        self.runtimeDependencies["kde/frameworks/tier2/kdoctools"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kiconthemes"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kxmlgui"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kconfigwidgets"] = None
        self.runtimeDependencies["kde/frameworks/tier2/kmime"] = None
        if self.buildTarget == "master" or CraftVersion(self.buildTarget) > CraftVersion("26.04.9"):
            self.runtimeDependencies["libs/kdsingleapplication"] = None

        self.runtimeDependencies["kde/pim/libkleo"] = None
        self.runtimeDependencies["kde/pim/mimetreeparser"] = None
        self.buildDependencies["libs/assuan2"] = None
        self.runtimeDependencies["kde/plasma/breeze"] = None


class Package(CMakePackageBase):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.subinfo.options.configure.args += ["-DUSE_UNITY_CMAKE_SUPPORT=ON"]

    def createPackage(self):
        self.blacklist_file.append(self.blueprintDir() / "blacklist.txt")
        # leave out everything that Kleopatra doesn't need; the whitelist keeps exceptions
        self.blacklist_file.append(self.blueprintDir() / "blacklist_macos.txt")
        self.whitelist_file.append(self.blueprintDir() / "whitelist_macos.txt")
        self.defines["alias"] = "kleopatra"
        return super().createPackage()

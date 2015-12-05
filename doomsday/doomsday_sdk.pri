# The Doomsday Engine Project
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# qmake .pri file for projects using the Doomsday SDK.
#
# The Doomsday SDK is distributed under the GNU Lesser General Public
# License version 3 (or, at your option, any later version). Please
# visit http://www.gnu.org/licenses/lgpl.html for details.
#
# Variables:
# - DENG_CONFIG     Names of supporting libraries to use (gui, appfw, shell);
#                   symlink: deploy libs as symbolic links

DENG_SDK_DIR = $$PWD

isEmpty(PREFIX): PREFIX = $$OUT_PWD

exists($$DENG_SDK_DIR/include/doomsday) {
    INCLUDEPATH += $$DENG_SDK_DIR/include/doomsday
}
else {
    INCLUDEPATH += $$DENG_SDK_DIR/include
}

win32: LIBS += -L$$DENG_SDK_DIR/lib
 else: QMAKE_LFLAGS = -L$$DENG_SDK_DIR/lib $$QMAKE_LFLAGS

# The core library is always required.
LIBS += -ldeng_core

contains(DENG_CONFIG, appfw): DENG_CONFIG *= gui shell

# Supporting libraries are optional.
contains(DENG_CONFIG, gui):   LIBS += -ldeng_gui
contains(DENG_CONFIG, appfw): LIBS += -ldeng_appfw
contains(DENG_CONFIG, shell): LIBS += -ldeng_shell

# Appropriate packages.
DENG_PACKAGES += net.dengine.stdlib
contains(DENG_CONFIG, gui): DENG_PACKAGES += net.dengine.stdlib.gui

defineTest(dengDynamicLinkPath) {
    # 1: path to search dynamic libraries from
    *-g++*|*-gcc*|*-clang* {
        QMAKE_LFLAGS += -Wl,-rpath,$$1
        export(QMAKE_LFLAGS)
    }
    win32 {
        LIBS += -L$$1
    }
}

# Instruct the dynamic linker to load the libs from the SDK lib dir.
dengDynamicLinkPath($$DENG_SDK_DIR/lib)

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
}

# Macros ---------------------------------------------------------------------

defineTest(dengRunPython2) {
    win32: system(python $$1)       # 2.7 still expected
     else: system(/usr/bin/env python2.7 $$1)
}

defineTest(dengPostLink) {
    isEmpty(QMAKE_POST_LINK) {
        QMAKE_POST_LINK = $$1
    } else {
        QMAKE_POST_LINK = $$QMAKE_POST_LINK && $$1
    }
    export(QMAKE_POST_LINK)
}

defineTest(dengPackage) {
    # 1: path of a .pack source directory (without the ".pack")
    # 2: target directory where the zipped .pack is written
    dengRunPython2($$DENG_SDK_DIR/buildpackage.py \"$${1}.pack\" \"$$2\")

    # Automatically deploy the package.
    appPackages.files += $$2/$${1}.pack
    export(appPackages.files)
}

defineTest(dengSdkPackage) {
    # 1: identifier of the package
    exists($$DENG_SDK_DIR/packs/$${1}.pack) {
        sdkPackages.files *= $$DENG_SDK_DIR/packs/$${1}.pack
        export(sdkPackages.files)
    }
}

defineReplace(dengFindLibDir) {
    # Determines the appropriate library directory given prefix $$1
    prefix = $$1
    dir = $$prefix/lib
    contains(QMAKE_HOST.arch, x86_64) {
        exists($$prefix/lib64) {
            dir = $$prefix/lib64
        }
        exists($$prefix/lib/x86_64-linux-gnu) {
            dir = $$prefix/lib/x86_64-linux-gnu
        }
    }
    return($$dir)
}

defineReplace(dengSdkLib) {
    # 1: name of library
    win32: fn = $$DENG_SDK_DIR/lib/$${1}.dll
    else:macx {
        versions = 2 1 0
        for(vers, versions) {
            fn = $$DENG_SDK_DIR/lib/lib$${1}.$${vers}.dylib
            exists($$fn): return($$fn)
        }
    }
    else {
        fn = $$DENG_SDK_DIR/lib/lib$${1}.so
        # Apply the latest major version.
        exists($${fn}.2):      fn = $${fn}.2
        else:exists($${fn}.1): fn = $${fn}.1
        else:exists($${fn}.0): fn = $${fn}.0
    }
    return($$fn)
}

defineTest(dengDeploy) {
    # 1: app name
    # 2: install prefix

    for(i, DENG_PACKAGES): dengSdkPackage($$i)

    prefix = $$2
    INSTALLS += target denglibs appPackages sdkPackages

    denglibs.files = $$dengSdkLib(deng_core)
    contains(DENG_CONFIG, gui):   denglibs.files += $$dengSdkLib(deng_gui)
    contains(DENG_CONFIG, appfw): denglibs.files += $$dengSdkLib(deng_appfw)
    contains(DENG_CONFIG, shell): denglibs.files += $$dengSdkLib(deng_shell)

    win32 {
        denglibs.files += $$DENG_SDK_DIR/lib/zlib1.dll
        contains(DENG_CONFIG, gui) {
            denglibs.files += \
                $$DENG_SDK_DIR/lib/assimpd.dll \
                $$DENG_SDK_DIR/lib/assimp.dll
        }

        target.path      = $$prefix/bin
        denglibs.path    = $$target.path
        appPackages.path = $$prefix/data
        sdkPackages.path = $$prefix/data
    }
    else:macx {
        QMAKE_BUNDLE_DATA += $$INSTALLS
        QMAKE_BUNDLE_DATA -= target
        appPackages.path = Contents/Resources
        sdkPackages.path = Contents/Resources
        denglibs.path    = Contents/Frameworks
    }
    else {
        target.path      = $$prefix/bin
        denglibs.path    = $$dengFindLibDir($$prefix)
        appPackages.path = $$prefix/share/$$1
        sdkPackages.path = $$prefix/share/$$1
    }

    contains(DENG_CONFIG, symlink):unix {
        # Symlink the libraries rather than copy.
        macx {
            QMAKE_BUNDLE_DATA -= denglibs
            fwDir = $${TARGET}.app/$$denglibs.path
        }
        else {
            INSTALLS -= denglibs
            fwDir = $$denglibs.path
        }
        for(fn, denglibs.files) {
            dengPostLink(mkdir -p \"$$fwDir\" && ln -sf \"$$fn\" \"$$fwDir\")
        }
    }

    win32 {
        contains(DENG_CONFIG, gui): deployOpts += -opengl
        denglibs.extra = windeployqt \"$$target.path\" -network $$deployOpts
        export(denglibs.extra)
    }

    macx: export(QMAKE_BUNDLE_DATA)
    else {
        export(INSTALLS)
        export(target.path)
    }
    export(appPackages.path)
    export(sdkPackages.files)
    export(sdkPackages.path)
    export(denglibs.files)
    export(denglibs.path)
}
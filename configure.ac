#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Process this file with autoconf to produce a configure script

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([xf86-video-nested],
        [0.1.0],
        [https://gitlab.freedesktop.org/xorg/driver/xf86-video-nested/issues],
        [xf86-video-nested])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR(.)

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-xz])

# Require xorg-macros: XORG_DEFAULT_OPTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.3 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.3)
XORG_DEFAULT_OPTIONS

# Initialize libtool
LT_INIT([disable-static])

# Define a configure option for an alternate module directory
AC_ARG_WITH(xorg-module-dir, [  --with-xorg-module-dir=DIR ],
                             [ moduledir="$withval" ],
                             [ moduledir="$libdir/xorg/modules" ])
AC_SUBST(moduledir)


# Store the list of server defined optional extensions in REQUIRED_MODULES
#XORG_DRIVER_CHECK_EXT(RANDR, randrproto)

# Obtain compiler/linker options for the driver dependencies
PKG_CHECK_MODULES(XORG, xorg-server >= 1.18 xproto $REQUIRED_MODULES)

# Checks for libraries.
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(XEXT, xext)

DRIVER_NAME=nested
AC_SUBST([DRIVER_NAME])

AC_CONFIG_FILES([
                Makefile
                src/Makefile
])
AC_OUTPUT

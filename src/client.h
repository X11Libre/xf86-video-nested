/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *
 * Paulo Zanoni <pzanoni@mandriva.com>
 * Tuan Bui <tuanbui918@gmail.com>
 * Colin Cornaby <colin.cornaby@mac.com>
 * Timothy Fleck <tim.cs.pdx@gmail.com>
 * Colin Hill <colin.james.hill@gmail.com>
 * Weseung Hwang <weseung@gmail.com>
 * Nathaniel Way <nathanielcw@hotmail.com>
 */

#include <colormap.h>
#include <misc.h>
#include "xf86Cursor.h"

#include <X11/extensions/XKBstr.h>

struct NestedClientPrivate;
typedef struct NestedClientPrivate *NestedClientPrivatePtr;

Bool NestedClientCheckDisplay(const char *displayName);

Bool NestedClientValidDepth(int depth);

NestedClientPrivatePtr NestedClientCreateScreen(int    scrnIndex,
                                                const char  *displayName,
                                                int    width,
                                                int    height,
                                                int    originX,
                                                int    originY,
                                                int    depth,
                                                int    bitsPerPixel,
                                                Pixel *retRedMask,
                                                Pixel *retGreenMask,
                                                Pixel *retBlueMask);

char *NestedClientGetFrameBuffer(NestedClientPrivatePtr pPriv);

void NestedClientUpdateScreen(NestedClientPrivatePtr pPriv,
                              int16_t x1,
                              int16_t y1,
                              int16_t x2,
                              int16_t y2);

void NestedClientHideCursor(NestedClientPrivatePtr pPriv);

void NestedClientCheckEvents(NestedClientPrivatePtr pPriv);

void NestedClientCloseScreen(NestedClientPrivatePtr pPriv);

void NestedClientSetDevicePtr(NestedClientPrivatePtr pPriv, DeviceIntPtr dev);

int NestedClientGetFileDescriptor(NestedClientPrivatePtr pPriv);

Bool NestedClientGetKeyboardMappings(NestedClientPrivatePtr pPriv, KeySymsPtr keySyms, CARD8 *modmap, XkbControlsPtr ctrls);

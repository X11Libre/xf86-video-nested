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

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xorg-server.h>
#include <fb.h>
#include <micmap.h>
#include <mipointer.h>
#include <shadow.h>
#include <xf86.h>
#include <xf86Module.h>
#include <xf86str.h>
#include "xf86Xinput.h"

#include "compat-api.h"

#include "client.h"
#include "nested_input.h"

#define NESTED_VERSION 0
#define NESTED_NAME "NESTED"
#define NESTED_DRIVER_NAME "nested"

#define NESTED_MAJOR_VERSION PACKAGE_VERSION_MAJOR
#define NESTED_MINOR_VERSION PACKAGE_VERSION_MINOR
#define NESTED_PATCHLEVEL PACKAGE_VERSION_PATCHLEVEL

#define TIMER_CALLBACK_INTERVAL 20

static MODULESETUPPROTO(NestedSetup);
static void NestedIdentify(int flags);
static const OptionInfoRec *NestedAvailableOptions(int chipid, int busid);
static Bool NestedProbe(DriverPtr drv, int flags);
static Bool NestedDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
                             pointer ptr);

static Bool NestedPreInit(ScrnInfoPtr pScrn, int flags);
static Bool NestedScreenInit(SCREEN_INIT_ARGS_DECL);

static Bool NestedSwitchMode(SWITCH_MODE_ARGS_DECL);
static void NestedAdjustFrame(ADJUST_FRAME_ARGS_DECL);
static Bool NestedEnterVT(VT_FUNC_ARGS_DECL);
static void NestedLeaveVT(VT_FUNC_ARGS_DECL);
static void NestedFreeScreen(FREE_SCREEN_ARGS_DECL);
static ModeStatus NestedValidMode(SCRN_ARG_TYPE arg, DisplayModePtr mode,
                                  Bool verbose, int flags);

static Bool NestedSaveScreen(ScreenPtr pScreen, int mode);
static Bool NestedCreateScreenResources(ScreenPtr pScreen);

static void NestedShadowUpdate(ScreenPtr pScreen, shadowBufPtr pBuf);
static Bool NestedCloseScreen(CLOSE_SCREEN_ARGS_DECL);

int NestedValidateModes(ScrnInfoPtr pScrn);
Bool NestedAddMode(ScrnInfoPtr pScrn, int width, int height);
void NestedPrintPscreen(ScrnInfoPtr p);
void NestedPrintMode(ScrnInfoPtr p, DisplayModePtr m);

typedef enum {
    OPTION_DISPLAY,
    OPTION_ORIGIN
} NestedOpts;

typedef enum {
    NESTED_CHIP
} NestedType;

static SymTabRec NestedChipsets[] = {
    { NESTED_CHIP, "nested" },
    {-1,            NULL }
};

/* XXX: Shouldn't we allow NestedClient to define options too? If some day we
 * port NestedClient to something that's not Xlib/Xcb we might need to add some
 * custom options */
static OptionInfoRec NestedOptions[] = {
    { OPTION_DISPLAY, "Display", OPTV_STRING, {0}, FALSE },
    { OPTION_ORIGIN,  "Origin",  OPTV_STRING, {0}, FALSE },
    { -1,             NULL,      OPTV_NONE,   {0}, FALSE }
};

_X_EXPORT DriverRec NESTED = {
    NESTED_VERSION,
    NESTED_DRIVER_NAME,
    NestedIdentify,
    NestedProbe,
    NestedAvailableOptions,
    NULL, /* module */
    0,    /* refCount */
    NestedDriverFunc,
    NULL, /* DeviceMatch */
    0     /* PciProbe */
};

_X_EXPORT InputDriverRec NESTEDINPUT = {
    1,
    "nestedinput",
    NULL,
    NestedInputPreInit,
    NestedInputUnInit,
    NULL,
    0,
};

static XF86ModuleVersionInfo NestedVersRec = {
    NESTED_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    NESTED_MAJOR_VERSION,
    NESTED_MINOR_VERSION,
    NESTED_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0} /* checksum */
};

_X_EXPORT XF86ModuleData nestedModuleData = {
    &NestedVersRec,
    NestedSetup,
    NULL, /* teardown */
};

/* These stuff should be valid to all server generations */
typedef struct NestedPrivate {
    const char                   *displayName;
    int                          originX;
    int                          originY;
    NestedClientPrivatePtr       clientData;
    CreateScreenResourcesProcPtr CreateScreenResources;
    CloseScreenProcPtr           CloseScreen;
    ShadowUpdateProc             update;
} NestedPrivate, *NestedPrivatePtr;

#define PNESTED(p)    ((NestedPrivatePtr)((p)->driverPrivate))
#define PCLIENTDATA(p) (PNESTED(p)->clientData)

/*static ScrnInfoPtr NESTEDScrn;*/

static pointer
NestedSetup(pointer module, pointer opts, int *errmaj, int *errmin) {
    static Bool setupDone = FALSE;

    if (!setupDone) {
        setupDone = TRUE;
        
        xf86AddDriver(&NESTED, module, HaveDriverFuncs);
        xf86AddInputDriver(&NESTEDINPUT, module, 0);
        
        return (pointer)1;
    } else {
        if (errmaj)
            *errmaj = LDR_ONCEONLY;
        
        return NULL;
    }
}

static void
NestedIdentify(int flags) {
    xf86PrintChipsets(NESTED_NAME, "Driver for nested servers",
                      NestedChipsets);
}

static const OptionInfoRec *
NestedAvailableOptions(int chipid, int busid) {
    return NestedOptions;
}

static Bool
NestedProbe(DriverPtr drv, int flags) {
    Bool foundScreen = FALSE;
    int numDevSections;
    GDevPtr *devSections;
    int i;

    ScrnInfoPtr pScrn;
    int entityIndex;

    if (flags & PROBE_DETECT)
        return FALSE;

    if ((numDevSections = xf86MatchDevice(NESTED_DRIVER_NAME,
                                          &devSections)) <= 0) {
        return FALSE;
    }

    if (numDevSections > 0) {
        for(i = 0; i < numDevSections; i++) {
            pScrn = NULL;
            entityIndex = xf86ClaimNoSlot(drv, NESTED_CHIP, devSections[i],
                                          TRUE);
            pScrn = xf86AllocateScreen(drv, 0);
            if (pScrn) {
                xf86AddEntityToScreen(pScrn, entityIndex);
                pScrn->driverVersion = NESTED_VERSION;
                pScrn->driverName    = NESTED_DRIVER_NAME;
                pScrn->name          = NESTED_NAME;
                pScrn->Probe         = NestedProbe;
                pScrn->PreInit       = NestedPreInit;
                pScrn->ScreenInit    = NestedScreenInit;
                pScrn->SwitchMode    = NestedSwitchMode;
                pScrn->AdjustFrame   = NestedAdjustFrame;
                pScrn->EnterVT       = NestedEnterVT;
                pScrn->LeaveVT       = NestedLeaveVT;
                pScrn->FreeScreen    = NestedFreeScreen;
                pScrn->ValidMode     = NestedValidMode;
                foundScreen = TRUE;
            }
        }
    }

    return foundScreen;
}

#ifndef HW_SKIP_CONSOLE
#define HW_SKIP_CONSOLE 4
#endif

static Bool
NestedDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr) {
    CARD32 *flag;
    xf86Msg(X_INFO, "NestedDriverFunc\n");

    /* XXX implement */
    switch(op) {
        case GET_REQUIRED_HW_INTERFACES:
            flag = (CARD32*)ptr;
            (*flag) = HW_SKIP_CONSOLE;
            return TRUE;

        case RR_GET_INFO:
        case RR_SET_CONFIG:
        case RR_GET_MODE_MM:
        default:
            return FALSE;
    }
}

static Bool NestedAllocatePrivate(ScrnInfoPtr pScrn) {
    if (pScrn->driverPrivate != NULL) {
        xf86Msg(X_WARNING, "NestedAllocatePrivate called for an already "
                "allocated private!\n");
        return FALSE;
    }

    pScrn->driverPrivate = XNFcallocarray(sizeof(NestedPrivate), 1);
    if (pScrn->driverPrivate == NULL)
        return FALSE;
    return TRUE;
}

static void NestedFreePrivate(ScrnInfoPtr pScrn) {
    if (pScrn->driverPrivate == NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Double freeing NestedPrivate!\n");
        return;
    }

    free(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

/* Data from here is valid to all server generations */
static Bool NestedPreInit(ScrnInfoPtr pScrn, int flags) {
    NestedPrivatePtr pNested;
    const char *originString = NULL;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedPreInit\n");

    if (flags & PROBE_DETECT)
        return FALSE;

    if (!NestedAllocatePrivate(pScrn)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to allocate private\n");
        return FALSE;
    }

    pNested = PNESTED(pScrn);

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support24bppFb | Support32bppFb))
        return FALSE;
 
    xf86PrintDepthBpp(pScrn);

    if (pScrn->depth > 8) {
        rgb zeros = {0, 0, 0};
        if (!xf86SetWeight(pScrn, zeros, zeros)) {
            return FALSE;
        }
    }

    if (!xf86SetDefaultVisual(pScrn, -1))
        return FALSE;

    pScrn->monitor = pScrn->confScreen->monitor; /* XXX */

    xf86CollectOptions(pScrn, NULL);
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, NestedOptions);

    if (xf86IsOptionSet(NestedOptions, OPTION_DISPLAY)) {
        pNested->displayName = xf86GetOptValString(NestedOptions,
                                                   OPTION_DISPLAY);
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using display \"%s\"\n",
                   pNested->displayName);
    } else {
        pNested->displayName = NULL;
    }

    if (xf86IsOptionSet(NestedOptions, OPTION_ORIGIN)) {
        originString = xf86GetOptValString(NestedOptions, OPTION_ORIGIN);
        if (sscanf(originString, "%d %d", &pNested->originX,
            &pNested->originY) != 2) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Invalid value for option \"Origin\"\n");
            return FALSE;
        }
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using origin x:%d y:%d\n",
                   pNested->originX, pNested->originY);
    } else {
        pNested->originX = 0;
        pNested->originY = 0;
    }

    xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    if (!NestedClientCheckDisplay(pNested->displayName)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Can't open display: %s\n",
                   pNested->displayName);
        return FALSE;
    }

    if (!NestedClientValidDepth(pScrn->depth)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid depth: %d\n",
                   pScrn->depth);
        return FALSE;
    }

    /*if (pScrn->depth > 1) {
        Gamma zeros = {0.0, 0.0, 0.0};
        if (!xf86SetGamma(pScrn, zeros))
            return FALSE;
    }*/

    if (NestedValidateModes(pScrn) < 1) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes\n");
        return FALSE;
    }


    if (!pScrn->modes) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
        return FALSE;
    }
    xf86SetCrtcForModes(pScrn, 0);

    pScrn->currentMode = pScrn->modes;

    xf86SetDpi(pScrn, 0, 0);

    if (!xf86LoadSubModule(pScrn, "shadow"))
        return FALSE;
    if (!xf86LoadSubModule(pScrn, "fb"))
        return FALSE;

    pScrn->memPhysBase = 0;
    pScrn->fbOffset = 0;
    
    return TRUE;
}

int
NestedValidateModes(ScrnInfoPtr pScrn) {
    DisplayModePtr mode;
    int i, width, height, ret = 0;
    int maxX = 0, maxY = 0;

    /* Print useless stuff */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Monitor wants these modes:\n");
    for(mode = pScrn->monitor->Modes; mode != NULL; mode = mode->next) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  %s (%dx%d)\n", mode->name,
                   mode->HDisplay, mode->VDisplay);
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Too bad for it...\n");

    /* If user requested modes, add them. If not, use 640x480 */
    if (pScrn->display->modes != NULL) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "User wants these modes:\n");
        for(i = 0; pScrn->display->modes[i] != NULL; i++) {
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  %s\n",
                       pScrn->display->modes[i]);
            if (sscanf(pScrn->display->modes[i], "%dx%d", &width,
                       &height) != 2) {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                           "This is not the mode name I was expecting...\n");
                return 0;
            }
            if (!NestedAddMode(pScrn, width, height)) {
                return 0;
            }
        }
    } else {
        if (!NestedAddMode(pScrn, 640, 480)) {
            return 0;
        }
    }

    pScrn->modePool = NULL;

    /* Now set virtualX, virtualY and displayWidth*/

    if (pScrn->display->virtualX >= pScrn->modes->HDisplay &&
        pScrn->display->virtualY >= pScrn->modes->VDisplay) {
        pScrn->virtualX = pScrn->display->virtualX;
        pScrn->virtualY = pScrn->display->virtualY;
    } else {
        /* XXX: if not specified, make virtualX and virtualY as big as the max X
         * and Y. I'm not sure this is correct */
        mode = pScrn->modes;
        while (mode != NULL) {
            if (mode->HDisplay > maxX)
                maxX = mode->HDisplay;
       
            if (mode->VDisplay > maxY)
                maxY = mode->VDisplay;
          
            mode = mode->next;
        }
        pScrn->virtualX = maxX;
        pScrn->virtualY = maxY;
    }
    pScrn->displayWidth = pScrn->virtualX;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Virtual size: %dx%d\n",
               pScrn->virtualX, pScrn->virtualY);

    /* Calculate the return value */
    mode = pScrn->modes;
    while (mode != NULL) {
        mode = mode->next;
        ret++;
    }

    /* Finally, make the mode list circular */
    pScrn->modes->prev->next = pScrn->modes;

    return ret;
}

Bool
NestedAddMode(ScrnInfoPtr pScrn, int width, int height) {
    DisplayModePtr mode;
    char nameBuf[64];
    size_t len;
    char *modeName;

    if (snprintf(nameBuf, 64, "%dx%d", width, height) >= 64)
        return FALSE;

    mode = XNFcallocarray(sizeof(DisplayModeRec), 1);
    mode->status = MODE_OK;
    mode->type = M_T_DRIVER;
    mode->HDisplay = width;
    mode->VDisplay = height;

    len = strlen(nameBuf);
    modeName = XNFalloc(len+1);
    strcpy(modeName, nameBuf);
    mode->name = modeName;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Adding mode %s\n", mode->name);

    /* Now add mode to pScrn->modes. We'll keep the list non-circular for now,
     * but we'll maintain pScrn->modes->prev to know the last element */
    mode->next = NULL;
    if (!pScrn->modes) {
        pScrn->modes = mode;
        mode->prev = mode;
    } else {
        mode->prev = pScrn->modes->prev;
        pScrn->modes->prev->next = mode;
        pScrn->modes->prev = mode;
    }

    return TRUE;
}

// Wrapper for timed call to NestedInputLoadDriver.  Used with timer in order
// to force the initialization to wait until the input core is initialized.
static CARD32
NestedMouseTimer(OsTimerPtr timer, CARD32 time, pointer arg) {
    NestedInputLoadDriver(arg);
    return 0;
}

static void
#if ABI_VIDEODRV_VERSION >= SET_ABI_VERSION(23, 0)
NestedBlockHandler(void *data, void *wt)
#else
NestedBlockHandler(pointer data, OSTimePtr wt, pointer LastSelectMask)
#endif
{
    NestedClientPrivatePtr pNestedClient = data;
    NestedClientCheckEvents(pNestedClient);
}

static void
#if ABI_VIDEODRV_VERSION >= SET_ABI_VERSION(23, 0)
NestedWakeupHandler(void *data, int i)
#else
NestedWakeupHandler(pointer data, int i, pointer LastSelectMask)
#endif
{
}

/* Called at each server generation */
static Bool NestedScreenInit(SCREEN_INIT_ARGS_DECL)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    NestedPrivatePtr pNested;
    Pixel redMask, greenMask, blueMask;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedScreenInit\n");

    pNested = PNESTED(pScrn);
    /*NESTEDScrn = pScrn;*/

    NestedPrintPscreen(pScrn);

    /* Save state:
     * NestedSave(pScrn); */
    
    //Load_Nested_Mouse();

    pNested->clientData = NestedClientCreateScreen(pScrn->scrnIndex,
                                                   pNested->displayName,
                                                   pScrn->virtualX,
                                                   pScrn->virtualY,
                                                   pNested->originX,
                                                   pNested->originY,
                                                   pScrn->depth,
                                                   pScrn->bitsPerPixel,
                                                   &redMask, &greenMask, &blueMask);
    
    if (!pNested->clientData) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to create client screen\n");
        return FALSE;
    }
    
    // Schedule the NestedInputLoadDriver function to load once the
    // input core is initialized.
    TimerSet(NULL, 0, 1, NestedMouseTimer, pNested->clientData);

    miClearVisualTypes();
    if (!miSetVisualTypesAndMasks(pScrn->depth,
                                  miGetDefaultVisualMask(pScrn->depth),
                                  pScrn->rgbBits, pScrn->defaultVisual,
                                  redMask, greenMask, blueMask))
        return FALSE;
    
    if (!miSetPixmapDepths())
        return FALSE;

    if (!fbScreenInit(pScreen, NestedClientGetFrameBuffer(PCLIENTDATA(pScrn)),
                      pScrn->virtualX, pScrn->virtualY, pScrn->xDpi,
                      pScrn->yDpi, pScrn->displayWidth, pScrn->bitsPerPixel))
        return FALSE;

    fbPictureInit(pScreen, 0, 0);

    xf86SetBlackWhitePixels(pScreen);
    xf86SetBackingStore(pScreen);
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
    
    if (!miCreateDefColormap(pScreen))
        return FALSE;

    pNested->update = NestedShadowUpdate;
    pScreen->SaveScreen = NestedSaveScreen;

    if (!shadowSetup(pScreen))
        return FALSE;

    pNested->CreateScreenResources = pScreen->CreateScreenResources;
    pScreen->CreateScreenResources = NestedCreateScreenResources;

    pNested->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = NestedCloseScreen;

    RegisterBlockAndWakeupHandlers(NestedBlockHandler, NestedWakeupHandler, pNested->clientData);

    return TRUE;
}

static Bool
NestedCreateScreenResources(ScreenPtr pScreen) {
    xf86DrvMsg(pScreen->myNum, X_INFO, "NestedCreateScreenResources\n");
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    NestedPrivatePtr pNested = PNESTED(pScrn);
    Bool ret;

    pScreen->CreateScreenResources = pNested->CreateScreenResources;
    ret = pScreen->CreateScreenResources(pScreen);
    pScreen->CreateScreenResources = NestedCreateScreenResources;

    if(!shadowAdd(pScreen, pScreen->GetScreenPixmap(pScreen),
                  pNested->update, NULL, 0, 0)) {
        xf86DrvMsg(pScreen->myNum, X_ERROR, "NestedCreateScreenResources failed to shadowAdd.\n");
        return FALSE;
    }

    return ret;
}

static void
NestedShadowUpdate(ScreenPtr pScreen, shadowBufPtr pBuf) {
    RegionPtr pRegion = DamageRegion(pBuf->pDamage);
    NestedClientUpdateScreen(PCLIENTDATA(xf86ScreenToScrn(pScreen)),
                             pRegion->extents.x1, pRegion->extents.y1,
                             pRegion->extents.x2, pRegion->extents.y2);
}

static Bool
NestedCloseScreen(CLOSE_SCREEN_ARGS_DECL) {
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedCloseScreen\n");

    shadowRemove(pScreen, pScreen->GetScreenPixmap(pScreen));

    RemoveBlockAndWakeupHandlers(NestedBlockHandler, NestedWakeupHandler, PNESTED(pScrn)->clientData);
    NestedClientCloseScreen(PCLIENTDATA(pScrn));

    pScreen->CloseScreen = PNESTED(pScrn)->CloseScreen;
    return (*pScreen->CloseScreen)(CLOSE_SCREEN_ARGS);
}

static Bool NestedSaveScreen(ScreenPtr pScreen, int mode) {
    xf86DrvMsg(pScreen->myNum, X_INFO, "NestedSaveScreen\n");
    return TRUE;
}

static Bool NestedSwitchMode(SWITCH_MODE_ARGS_DECL) {
    SCRN_INFO_PTR(arg);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedSwitchMode\n");
    return TRUE;
}

static void NestedAdjustFrame(ADJUST_FRAME_ARGS_DECL) {
    SCRN_INFO_PTR(arg);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedAdjustFrame\n");
}

static Bool NestedEnterVT(VT_FUNC_ARGS_DECL) {
    SCRN_INFO_PTR(arg);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedEnterVT\n");
    return TRUE;
}

static void NestedLeaveVT(VT_FUNC_ARGS_DECL) {
    SCRN_INFO_PTR(arg);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedLeaveVT\n");
}

static void NestedFreeScreen(FREE_SCREEN_ARGS_DECL) {
    SCRN_INFO_PTR(arg);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedFreeScreen\n");
}

static ModeStatus NestedValidMode(SCRN_ARG_TYPE arg, DisplayModePtr mode,
                                  Bool verbose, int flags) {
    SCRN_INFO_PTR(arg);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NestedValidMode:\n");

    if (!mode)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "NULL MODE!\n");

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  name: %s\n", mode->name);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  HDisplay: %d\n", mode->HDisplay);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  VDisplay: %d\n", mode->VDisplay);
    return MODE_OK;
}

void NestedPrintPscreen(ScrnInfoPtr p) {
    /* XXX: finish implementing this someday? */
    xf86DrvMsg(p->scrnIndex, X_INFO, "Printing pScrn:\n");
    xf86DrvMsg(p->scrnIndex, X_INFO, "driverVersion: %d\n", p->driverVersion);
    xf86DrvMsg(p->scrnIndex, X_INFO, "driverName:    %s\n", p->driverName);
    xf86DrvMsg(p->scrnIndex, X_INFO, "pScreen:       %p\n", p->pScreen);
    xf86DrvMsg(p->scrnIndex, X_INFO, "scrnIndex:     %d\n", p->scrnIndex);
    xf86DrvMsg(p->scrnIndex, X_INFO, "configured:    %d\n", p->configured);
    xf86DrvMsg(p->scrnIndex, X_INFO, "origIndex:     %d\n", p->origIndex);
    xf86DrvMsg(p->scrnIndex, X_INFO, "imageByteOrder: %d\n", p->imageByteOrder);
    /*xf86DrvMsg(p->scrnIndex, X_INFO, "bitmapScanlineUnit: %d\n");
    xf86DrvMsg(p->scrnIndex, X_INFO, "bitmapScanlinePad: %d\n");
    xf86DrvMsg(p->scrnIndex, X_INFO, "bitmapBitOrder: %d\n");
    xf86DrvMsg(p->scrnIndex, X_INFO, "numFormats: %d\n");
    xf86DrvMsg(p->scrnIndex, X_INFO, "formats[]: 0x%x\n");
    xf86DrvMsg(p->scrnIndex, X_INFO, "fbFormat: 0x%x\n"); */
    xf86DrvMsg(p->scrnIndex, X_INFO, "bitsPerPixel: %d\n", p->bitsPerPixel);
    /*xf86DrvMsg(p->scrnIndex, X_INFO, "pixmap24: 0x%x\n"); */
    xf86DrvMsg(p->scrnIndex, X_INFO, "depth: %d\n", p->depth);
    NestedPrintMode(p, p->currentMode);
    /*xf86DrvMsg(p->scrnIndex, X_INFO, "depthFrom: %\n");
    xf86DrvMsg(p->scrnIndex, X_INFO, "\n");*/
}

void NestedPrintMode(ScrnInfoPtr p, DisplayModePtr m) {
    xf86DrvMsg(p->scrnIndex, X_INFO, "HDisplay   %d\n",   m->HDisplay);
    xf86DrvMsg(p->scrnIndex, X_INFO, "HSyncStart %d\n", m->HSyncStart);
    xf86DrvMsg(p->scrnIndex, X_INFO, "HSyncEnd   %d\n",   m->HSyncEnd);
    xf86DrvMsg(p->scrnIndex, X_INFO, "HTotal     %d\n",     m->HTotal);
    xf86DrvMsg(p->scrnIndex, X_INFO, "HSkew      %d\n",      m->HSkew);
    xf86DrvMsg(p->scrnIndex, X_INFO, "VDisplay   %d\n",   m->VDisplay);
    xf86DrvMsg(p->scrnIndex, X_INFO, "VSyncStart %d\n", m->VSyncStart);
    xf86DrvMsg(p->scrnIndex, X_INFO, "VSyncEnd   %d\n",   m->VSyncEnd);
    xf86DrvMsg(p->scrnIndex, X_INFO, "VTotal     %d\n",     m->VTotal);
    xf86DrvMsg(p->scrnIndex, X_INFO, "VScan      %d\n",      m->VScan);
}

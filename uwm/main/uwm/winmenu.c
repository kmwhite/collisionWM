/***  WINMENU.C: Contains routines for the hexagonal window menu  ***/

/* ########################################################################

   uwm - THE ude WINDOW MANAGER

   ########################################################################

   Copyright (c) : Christian Ruppert

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   ######################################################################## */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>

#include "uwm.h"
#include "init.h"
#include "handlers.h"
#include "workspaces.h"
#include "winmenumenu.h"
#include "windows.h"
#include "special.h"
#include "settings.h"

extern UDEScreen TheScreen;
extern Display *disp;
extern InitStruct InitS;

short selectedHex;
int hexX,hexY,move_back;
UltimateContext *TheWin;

void StartWinMenu(UltimateContext *uc,int x,int y)
{
  TheWin=uc;
  selectedHex=ICONWINS;

  hexX = x; hexY = y;
  if(x < 40) x = 40;
  if(x > (TheScreen.width - 64))  x = TheScreen.width - 64;
  if(y < 42) y = 42;
  if(y > (TheScreen.height - 42)) y = TheScreen.height - 42;
  
  XUnmapWindow(disp, TheScreen.HexMenu.icons[I_REALLY].IconWin);
  XShapeCombineMask(disp, TheScreen.HexMenu.IconParent, ShapeBounding,
                    TheScreen.HexMenu.icons[I_REALLY].x,
                    TheScreen.HexMenu.icons[I_REALLY].y,
                    TheScreen.HexMenu.icons[I_REALLY].IconShape,
                    ShapeSubtract);

  XMoveWindow(disp, TheScreen.HexMenu.IconParent, x - 40, y - 42);
  XInstallColormap(disp, TheScreen.colormap);
  XMapRaised(disp, TheScreen.HexMenu.IconParent);
  GrabPointer(TheScreen.HexMenu.IconParent,
              ButtonPressMask|ButtonReleaseMask|EnterWindowMask,
              TheScreen.Mice[C_BORDER]);

  move_back = False;
  if((x != hexX) || (y != hexY)){
    XWarpPointer(disp, None, TheScreen.root, 0, 0, 0, 0, x, y);
    move_back = True;
    hexX -= x; hexY -= y;
  }

  InstallWinMenuHandle();
}

void StopWinMenu(short selected, XEvent *event)
{
  switch(selected){
    case I_ICONIFY:
      IconifyWin(TheWin);
      break;
    case I_CLOSE:
      CloseWin(TheWin);
      break;
    case I_AUTORISE:
      if(TheWin->flags & RISEN) {
        MoveResizeWin(TheWin,TheWin->ra.x,TheWin->ra.y,\
                                TheWin->ra.w,TheWin->ra.h);
        TheWin->flags &= ~RISEN;
      } else {
        int maxw,maxh,bw,bh,wi,hi;
        maxw=TheWin->ra.maxw;maxh=TheWin->ra.maxh;
        bw=TheWin->ra.bw;bh=TheWin->ra.bh;
        wi=TheWin->ra.wi;hi=TheWin->ra.hi;
        TheWin->ra.x=TheWin->Attr.x;
        TheWin->ra.y=TheWin->Attr.y;
        TheWin->ra.w=TheWin->Attr.width;
        TheWin->ra.h=TheWin->Attr.height;
        MoveResizeWin(TheWin,0,0,(maxw>TheScreen.width)?\
         (bw+((int)((TheScreen.width-bw-1)/wi))*wi):maxw,\
                      (maxh>TheScreen.height)?(bh+((int)\
                 ((TheScreen.height-bh-1)/hi))*hi):maxh);


        TheWin->flags |= RISEN;
      }
      break;
    case I_BACK:
      LowerWin(TheWin);
      break;
    case I_KILL:
      break;
    case I_MENU:
      WinMenuMenu(TheWin,event->xbutton.x,event->xbutton.y);
      break;
    case I_REALLY:
      XKillClient(disp,TheWin->win);
      break;
    default:
      if(move_back) XWarpPointer(disp,None,None,0,0,0,0,hexX,hexY);
  }
  XUnmapWindow(disp, TheScreen.HexMenu.IconParent);
  if(TheWin) XInstallColormap(disp,TheWin->Attributes.colormap);
  ReinstallDefaultHandle();

  if(selectedHex < ICONWINS)
    XSetWindowBackgroundPixmap(disp,
                               TheScreen.HexMenu.icons[selectedHex].IconWin,
                               TheScreen.HexMenu.icons[selectedHex].IconPix);
  UngrabPointer();
}

void WinMenuEnterNotify(XEvent *event)
{
  int a;

  StampTime(event->xcrossing.time);
  if(selectedHex<ICONWINS){
    XSetWindowBackgroundPixmap(disp,
                               TheScreen.HexMenu.icons[selectedHex].IconWin,
                               TheScreen.HexMenu.icons[selectedHex].IconPix);
    XClearWindow(disp, TheScreen.HexMenu.icons[selectedHex].IconWin);
  }

  selectedHex=ICONWINS;
  for(a=0;a<ICONWINS;a++)
    if(event->xcrossing.window == TheScreen.HexMenu.icons[a].IconWin)
      selectedHex=a;

  if(selectedHex<ICONWINS){
    XSetWindowBackgroundPixmap(disp,
                               TheScreen.HexMenu.icons[selectedHex].IconWin,
                            TheScreen.HexMenu.icons[selectedHex].IconSelectPix);
    XClearWindow(disp, TheScreen.HexMenu.icons[selectedHex].IconWin);
  }

  if((selectedHex == I_KILL) || (selectedHex == I_REALLY)){
    XMapWindow(disp, TheScreen.HexMenu.icons[selectedHex].IconWin);
    XShapeCombineMask(disp, TheScreen.HexMenu.IconParent, ShapeBounding,
                      TheScreen.HexMenu.icons[I_REALLY].x,
                      TheScreen.HexMenu.icons[I_REALLY].y,
                      TheScreen.HexMenu.icons[I_REALLY].IconShape, ShapeUnion);
  } else {
    XShapeCombineMask(disp, TheScreen.HexMenu.IconParent, ShapeBounding,
                      TheScreen.HexMenu.icons[I_REALLY].x,
                      TheScreen.HexMenu.icons[I_REALLY].y,
                      TheScreen.HexMenu.icons[I_REALLY].IconShape,
                      ShapeSubtract);
    XUnmapWindow(disp, TheScreen.HexMenu.icons[I_REALLY].IconWin);
  }
}

void WinMenuButtonPress(XEvent *event)
{
  StampTime(event->xbutton.time);
}

void WinMenuVisibility(XEvent *event)
{
  XEvent dummy;

  if(event->xvisibility.window == TheScreen.HexMenu.IconParent) {
    if(event->xvisibility.state != VisibilityUnobscured) {
      RaiseWin(TheWin);
      XRaiseWindow(disp, TheScreen.HexMenu.IconParent);
    } else while(XCheckTypedWindowEvent(disp, TheScreen.HexMenu.IconParent,
                                        VisibilityNotify, &dummy));
  }
}

void ButtonAction(int a)
{
  switch(InitS.WMMenuButtons[a]){
    case 'X': WithWin2WS(TheWin,(TheWin->WorkSpace + 1) 
                         % NUMBER_OF_WORKSPACES);
              break;
    case 'Z': WithWin2WS(TheWin,(TheWin->WorkSpace 
                                 + NUMBER_OF_WORKSPACES -1)
                                 % NUMBER_OF_WORKSPACES);
              break;
    case 'M': break;
  }
}

void WinMenuButtonRelease(XEvent *event)
{
  StampTime(event->xbutton.time);
  if(ButtonCount(event->xbutton.state)>1) {
    switch(event->xbutton.button){
      case Button1: ButtonAction(0); break;
      case Button2: ButtonAction(1); break;
      case Button3: ButtonAction(2); break;
      case Button4: break;
      case Button5: break;
    }
  } else {
    StopWinMenu(selectedHex, event);
  }
  
}

void WinMenuUnmapNotify(XEvent *event)
{
  if(event->xunmap.window == TheWin->win) StopWinMenu(ICONWINS, event);
  HandleUnmapNotify(event);
}

/***  MENU.C: Contains routines for the UWM-menues  ***/

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

#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include "uwm.h"
#include "init.h"
#include "handlers.h"
#include "menu.h"
#include "nodes.h"
#include "widgets.h"
#include "special.h"
#include "settings.h"
#include "workspaces.h"

#define MENUBORDERW settings.global_settings->BevelWidth
#define MENUXOFS settings.global_settings->MenuXOffset
#define MENUYOFS settings.global_settings->MenuYOffset
#define MENUSCROLLHEIGHT 5

extern UDEScreen TheScreen;
extern Display *disp;
extern HandlerTable *Handle;
extern UltimateContext *ActiveWin;
extern InitStruct InitS;

Menu *activemen=NULL;
MenuItem *selectedMenuItem;
short keepIt;
#define KeepNonTransMenu   (1<<0)
#define KeepMenuActive     (1<<1)
Bool quittable;
void (*SpecialProc)(XEvent *event,MenuItem *selectedMenuItem);

void Menu2ws(Menu *menu,short ws)
{
  XSetWindowAttributes wattr;
  XGCValues xgcv;
  Node *mi;

  xgcv.foreground=settings.workspace_settings[ws]->BackgroundLight->pixel;
  XChangeGC(disp,TheScreen.MenuLightGC,GCForeground,&xgcv);
  xgcv.foreground=settings.workspace_settings[ws]->BackgroundShadow->pixel;
  XChangeGC(disp,TheScreen.MenuShadowGC,GCForeground,&xgcv);
  xgcv.foreground=settings.workspace_settings[ws]->BackgroundColor->pixel;
  XChangeGC(disp,TheScreen.MenuBackGC,GCForeground,&xgcv);
  xgcv.foreground=settings.workspace_settings[ws]->ForegroundColor->pixel;
  XChangeGC(disp,TheScreen.MenuTextGC,GCForeground,&xgcv);

  xgcv.foreground=settings.workspace_settings[ws]->BackgroundColor->pixel;
  XChangeWindowAttributes(disp,menu->win,CWBackPixel,&wattr);
  mi=NULL;
  while(mi=NodeNext(menu->Items,mi)){
    MenuItem *item;
    item=mi->data;
    if(item->type!=I_LINE) {
      XChangeWindowAttributes(disp,item->win,CWBackPixel,&wattr);
      if(item->type==I_SUBMENU) Menu2ws(item->data,ws);
    }
  }
}

Menu *RootMenu(Menu *men)
{
  while(men->parent) men=men->parent;
  return(men);
}

Menu *MenuCreate(char *name)
{
  Menu *menu;
  XSetWindowAttributes wattr;
  XGCValues xgcv;

  if(!(menu=malloc(sizeof(Menu)))) return (NULL);

  if(name) {
    menu->name = MyCalloc(strlen(name)+1, sizeof(char));
    strcpy(menu->name, name);
  } else {
    menu->name = NULL;
  }
  if(!(menu->Items=NodeListCreate()))
    SeeYa(1,"FATAL: out of memory!");
  menu->font = settings.global_settings->Font->xfs;
  menu->width = (menu->name
                 ? XTextWidth(menu->font, menu->name, strlen(menu->name))
		 : 0) + 4 * MENUBORDERW + 2*MENUXOFS;
  menu->ItemHeight=menu->font->ascent + menu->font->descent +
                 2 * MENUBORDERW + 2*MENUYOFS;

  menu->height = (menu->name ? menu->ItemHeight : 0) + 2 * MENUBORDERW;

  wattr.background_pixel=ActiveWSSettings->BackgroundColor->pixel;
  wattr.backing_store=WhenMapped;
  wattr.override_redirect=True;
  wattr.save_under=True;
  menu->win=XCreateWindow(disp, TheScreen.root, 0, 0, menu->width, menu->height,
                               0,CopyFromParent,InputOutput,CopyFromParent,
                               (TheScreen.DoesSaveUnders ? CWSaveUnder : 0)|
                          (TheScreen.DoesBackingStore ? CWBackingStore : 0)|
	                             CWBackPixel|CWOverrideRedirect,&wattr);
  XSelectInput(disp, menu->win, ExposureMask | LeaveWindowMask 
                                | EnterWindowMask | VisibilityChangeMask);

  XSaveContext(disp,menu->win,TheScreen.MenuFrameContext,(XPointer)menu);

  return(menu);
}

void RemoveMenuBottomLines(Menu *men)
{
  Node *n;
  if((n=NodePrev(men->Items,NULL))&&(((MenuItem *)(n->data))->type == I_LINE)) {
    free(n->data);
    NodeDelete(men->Items,n);
    men->height-=2*MENUBORDERW;
    XResizeWindow(disp,men->win,men->width,men->height);
  }
  n=NULL;
  while(n=NodeNext(men->Items,n)){
    MenuItem *mi;
    mi=n->data;
    if(mi->type==I_SUBMENU) RemoveMenuBottomLines(mi->data);
  }
}

char linename[]="line";

void AppendMenuItem(Menu *menu,char *name,void *data,short type)
{
  MenuItem *item;
  XSetWindowAttributes wattr;
  int width;

  item=MyCalloc(1,sizeof(MenuItem));
  item->y=menu->height;
  item->menu=menu;

  if((item->type=type)==I_LINE) {
    menu->height+=2*MENUBORDERW;
    item->win=None;
    item->name=linename;
  } else {
    item->name=MyCalloc(strlen(name)+1,sizeof(char));
    strcpy(item->name,name);
    item->data=data;

    width=XTextWidth(menu->font,item->name,strlen(item->name)) +\
                                    4 * MENUBORDERW + 2*MENUXOFS;
    if(item->type==I_SUBMENU) width+=7*MENUBORDERW;
    if((item->type==I_SWITCH_ON)||(item->type==I_SWITCH_OFF))
      width+=6*MENUBORDERW;
    if(width>menu->width) {
      Node *mi=NULL;
      menu->width=width;
      while(mi=NodeNext(menu->Items,mi))
        if(((MenuItem *)(mi->data))->type!=I_LINE)
          XResizeWindow(disp,((MenuItem *)(mi->data))->win,\
                menu->width-2*MENUBORDERW,menu->ItemHeight);
    }

    wattr.background_pixel=ActiveWSSettings->BackgroundColor->pixel;
    wattr.backing_store=WhenMapped;
    wattr.override_redirect=True;
    item->win=XCreateWindow(disp,menu->win,MENUBORDERW,menu->height-MENUBORDERW\
                   ,menu->width-2*MENUBORDERW,menu->ItemHeight,0,CopyFromParent,
                                                     InputOutput,CopyFromParent,
                              (TheScreen.DoesBackingStore ? CWBackingStore : 0)|
                                         CWBackPixel|CWOverrideRedirect,&wattr);

    XSelectInput(disp, item->win, EnterWindowMask|ExposureMask);

    menu->height+=menu->ItemHeight;

    XSaveContext(disp,item->win,TheScreen.MenuContext,(XPointer)item);
  }

  if(!NodeAppend(menu->Items,item))
    SeeYa(1,"FATAL: out of memory!");
  XResizeWindow(disp,menu->win,menu->width,menu->height);
}

void DestroyMenu(Menu *menu)
{
  Node *mi;

  mi=NULL;
  while(mi=NodeNext(menu->Items,mi)) {
    MenuItem *item;
    item=mi->data;
    if(item->type!=I_LINE) {
      XDeleteContext(disp,item->win,TheScreen.MenuContext);
      XDestroyWindow(disp,item->win);
    }
    if(item->type==I_SUBMENU) DestroyMenu(item->data); /***/
    if(item->name!=linename) free(item->name);
    free(item);
  }
  XDeleteContext(disp, menu->win, TheScreen.MenuFrameContext);
  XDestroyWindow(disp,menu->win);
  NodeListDelete(&(menu->Items));
  if(menu->name) free(menu->name);
  free(menu);
}

void DrawItem(MenuItem *item, short deactivate)
{
  XClearWindow(disp,item->win);
  XDrawString(disp,item->win,TheScreen.MenuTextGC,MENUXOFS+MENUBORDERW,\
            MENUYOFS+MENUBORDERW+item->menu->font->ascent,item->name,\
                                                  strlen(item->name));
  if(item->type==I_SUBMENU) {
    XDrawLine(disp,item->win,TheScreen.MenuTextGC,item->menu->width-4*\
           MENUBORDERW,item->menu->ItemHeight/2,item->menu->width-10*\
                               MENUBORDERW,item->menu->ItemHeight/2);
    XDrawLine(disp,item->win,TheScreen.MenuTextGC,item->menu->width-6.6*\
                 MENUBORDERW,item->menu->ItemHeight/2-1.5*MENUBORDERW,\
             item->menu->width-4*MENUBORDERW,item->menu->ItemHeight/2);
  }
  if(item->type==I_SWITCH_OFF) {
    DrawBevel(item->win,item->menu->width-9*MENUBORDERW,\
                 item->menu->ItemHeight/2-2*MENUBORDERW,\
                      item->menu->width-5*MENUBORDERW-1,\
               item->menu->ItemHeight/2+2*MENUBORDERW-1,\
                        MENUBORDERW,TheScreen.MenuLightGC,\
                                   TheScreen.MenuShadowGC);
  }
  if(item->type==I_SWITCH_ON) {
    DrawBevel(item->win,item->menu->width-9*MENUBORDERW,\
                 item->menu->ItemHeight/2-2*MENUBORDERW,\
                      item->menu->width-5*MENUBORDERW-1,\
               item->menu->ItemHeight/2+2*MENUBORDERW-1,\
                       MENUBORDERW,TheScreen.MenuShadowGC,\
                                    TheScreen.MenuLightGC);
    XFillRectangle(disp,item->win,TheScreen.MenuTextGC,\
                     item->menu->width-8*MENUBORDERW,\
                item->menu->ItemHeight/2-MENUBORDERW,\
                         2*MENUBORDERW,2*MENUBORDERW);
  }
  if(!deactivate && (item == selectedMenuItem))
    DrawBevel(item->win,0,0,item->menu->width-2*MENUBORDERW-1,\
                         item->menu->ItemHeight-1,MENUBORDERW,\
                     TheScreen.MenuShadowGC,TheScreen.MenuLightGC);
}

void DrawMenuFrame(Menu *menu, int items)
{
  int a;
  Node *mi;

  XClearWindow(disp,menu->win);

  DrawBevel(menu->win,0,0,menu->width-1,menu->height-1,MENUBORDERW,\
                                      TheScreen.MenuLightGC,TheScreen.MenuShadowGC);
  if(menu->name) {
    DrawBevel(menu->win,MENUBORDERW,MENUBORDERW,menu->width-MENUBORDERW-1,\
                               menu->ItemHeight+MENUBORDERW-1,MENUBORDERW,\
                                             TheScreen.MenuShadowGC,TheScreen.MenuLightGC);
    XDrawString(disp,menu->win,TheScreen.MenuTextGC,MENUXOFS+2*MENUBORDERW,MENUYOFS+\
             2*MENUBORDERW+menu->font->ascent,menu->name,strlen(menu->name));
  }

  mi=NULL;
  while(mi=NodeNext(menu->Items,mi)){
    MenuItem *item;
    item=mi->data;
    if(item->type != I_LINE) {
      if(items) DrawItem(item, 0);
    } else {
      int h;
      h=item->y;
      for(a=0;a<MENUBORDERW;a++) {
        XDrawLine(disp,menu->win,TheScreen.MenuShadowGC,a+1,h-1-a,menu->width-a,h-1-a);
        XDrawLine(disp,menu->win,TheScreen.MenuLightGC,a,a+h,menu->width-a-2,a+h);
      }
    }
  }
}

void MapMenu(Menu *menu,int x, int y)
{
  activemen=menu;
  menu->x=x;
  menu->y=y;

  XMapSubwindows(disp,menu->win);
  XMoveWindow(disp,menu->win,x,y);
  XMapRaised(disp,menu->win);
}

void RedrawMenuTreeRecursion(Menu *men)
{
  if(men->parent) RedrawMenuTreeRecursion(men->parent);
  DrawMenuFrame(men, 1);
}
void RedrawMenuTree()
{
  if(activemen) RedrawMenuTreeRecursion(activemen);
}

void DeleteMenuTree(Menu *menu)
{
  Node *mi;

  mi=NULL;
  while(mi=NodeNext(menu->Items,mi)){
    MenuItem *item;
    item=mi->data;
    if(item->type==I_SUBMENU) {
      DeleteMenuTree(item->data);
    }
  }
  XUnmapWindow(disp,menu->win);
}

void DeleteSubMenus(Menu *menu)
{
  Node *mi;

  mi=NULL;
  while(mi=NodeNext(menu->Items,mi)){
    MenuItem *item;
    item=mi->data;
    if(item->type==I_SUBMENU) {
      DeleteMenuTree(item->data);
    }
  }
}

MenuItem *StartMenu(Menu *menu, int x, int y, Bool q,
                    void (*prc)(XEvent *event, MenuItem *selectedMenuItem))
{
  selectedMenuItem=NULL;
  quittable = q;
  keepIt = ((settings.global_settings->BehaviourFlags & TRANSIENT_MENUS) 
            ? 0 : KeepNonTransMenu) | KeepMenuActive;
  SpecialProc = prc;

  if(x>(TheScreen.width-menu->width))
    x=TheScreen.width-menu->width;
  if(y>(TheScreen.height-menu->height-1))
    y=TheScreen.height-menu->height-1;

  Menu2ws(menu, ActiveWS);
  InstallMenuHandle();
  GrabPointer(TheScreen.root,ButtonPressMask|ButtonReleaseMask|LeaveWindowMask|\
                                     EnterWindowMask,TheScreen.Mice[C_DEFAULT]);

  menu->parent=NULL;
  XInstallColormap(disp,TheScreen.colormap);

  MapMenu(menu, x, y);

  while(keepIt){
    XEvent event;
    XNextEvent(disp,&event);
    if(Handle[event.type]) (*Handle[event.type])(&event);
  }

  UngrabPointer();
  ReinstallDefaultHandle();
  DeleteMenuTree(menu);
  if(ActiveWin) XInstallColormap(disp,ActiveWin->Attributes.colormap);

  activemen=NULL;
  return(selectedMenuItem);
}

void SelectItem(MenuItem *item, unsigned int state)
{
  if(selectedMenuItem){
    DrawBevel(selectedMenuItem->win, 0, 0,
              selectedMenuItem->menu->width - 2 * MENUBORDERW - 1,
              selectedMenuItem->menu->ItemHeight - 1, MENUBORDERW,
              TheScreen.MenuBackGC, TheScreen.MenuBackGC);
  }
  selectedMenuItem=item;
  if(selectedMenuItem){
    if((selectedMenuItem->y+selectedMenuItem->menu->y
        + selectedMenuItem->menu->ItemHeight) >= TheScreen.height){
      XWarpPointer(disp,None,None,0,0,0,0,0,-MENUSCROLLHEIGHT*\
                           selectedMenuItem->menu->ItemHeight);
      selectedMenuItem->menu->y -= MENUSCROLLHEIGHT 
	                           * selectedMenuItem->menu->ItemHeight;
      XMoveWindow(disp, selectedMenuItem->menu->win, selectedMenuItem->menu->x,
		  selectedMenuItem->menu->y);
    }
    if((selectedMenuItem->y+selectedMenuItem->menu->y)<=0){
      XWarpPointer(disp,None,None,0,0,0,0,0,MENUSCROLLHEIGHT*\
                                  selectedMenuItem->menu->ItemHeight);
      selectedMenuItem->menu->y += MENUSCROLLHEIGHT
	                           * selectedMenuItem->menu->ItemHeight;
      XMoveWindow(disp, selectedMenuItem->menu->win, selectedMenuItem->menu->x,
		  selectedMenuItem->menu->y);
    }
    if(ButtonCount(state)>0) MenuDontKeepItAnymore();
    if(selectedMenuItem->menu != activemen){
      DeleteSubMenus(selectedMenuItem->menu);
      activemen = selectedMenuItem->menu;
    }
    if(selectedMenuItem->type == I_SUBMENU) {
      long int x,y;
      Menu *men;
      men=selectedMenuItem->data;
      men->parent=selectedMenuItem->menu;
      x=selectedMenuItem->menu->x+selectedMenuItem->menu->width*0.8;
      if((x+men->width)>TheScreen.width) x=selectedMenuItem->menu->x-men->width;
      if(x<30) x=30;
      y=selectedMenuItem->menu->y+selectedMenuItem->y;
      if(y>(((signed long int)TheScreen.height)-((signed long int)men->height-\
                                         1))) y=TheScreen.height-men->height-1;
      if(y<0) y=0;
      MapMenu(men, x, y);
    }
    DrawBevel(item->win, 0, 0, item->menu->width - 2 * MENUBORDERW - 1,
              item->menu->ItemHeight - 1, MENUBORDERW,
              TheScreen.MenuShadowGC, TheScreen.MenuLightGC);
  }
}

Bool VisibleMenuWin(Window win)
{
  Menu *men;

  men=activemen;
  while(men) {
    if(men->win==win) return(True);
    else men=men->parent;
  }
  return(False);
}

void MenuEnterNotify(XEvent *event)
{
  MenuItem *mc;
  StampTime(event->xcrossing.time);
  if(VisibleMenuWin(event->xcrossing.window))
    XChangeActivePointerGrab(disp,ButtonPressMask|ButtonReleaseMask|\
          EnterWindowMask|LeaveWindowMask,TheScreen.Mice[C_WINDOW],TimeStamp);
  if(!XFindContext(disp,event->xcrossing.window,TheScreen.MenuContext,\
                                                     (XPointer *)&mc)){
    SelectItem(mc, event->xcrossing.state);
    XChangeActivePointerGrab(disp,ButtonPressMask|ButtonReleaseMask|\
          EnterWindowMask|LeaveWindowMask,TheScreen.Mice[C_WINDOW],TimeStamp);
  }
  else if(quittable) SelectItem(NULL, event->xcrossing.state);
}

void MenuLeaveNotify(XEvent *event)
{
  StampTime(event->xcrossing.time);
  if(VisibleMenuWin(event->xcrossing.window)) {
    XChangeActivePointerGrab(disp,ButtonPressMask|ButtonReleaseMask|\
         EnterWindowMask|LeaveWindowMask,TheScreen.Mice[C_DEFAULT],TimeStamp);
    if(quittable) SelectItem(NULL, event->xcrossing.state);
  }
}

void RaiseMenuNParents(Menu *men)
{
  if(men->parent) RaiseMenuNParents(men->parent);
  XRaiseWindow(disp,men->win);
}

void MenuExpose(XEvent *event)
{
  Menu *mc;
  MenuItem *mi;

  if(event->xexpose.count) return;
  if(!XFindContext(disp, event->xexpose.window, TheScreen.MenuContext,
                   (XPointer *)&mi)) {
    DrawItem(mi, 0);
    return;
  }
  if(!XFindContext(disp, event->xexpose.window, TheScreen.MenuFrameContext,
                   (XPointer *)&mc)) {
    DrawMenuFrame(mc, 0);
    return;
  }
  HandleExpose(event);
}

void MenuVisibility(XEvent *event)
{
  XEvent dummy;

  if(event->xvisibility.window==activemen->win) {
    if(event->xvisibility.state!=VisibilityUnobscured)
      RaiseMenuNParents(activemen);
    else while(XCheckTypedWindowEvent(disp, activemen->win, VisibilityNotify,
                                      &dummy));
  }
}

void MenuDontKeepItAnymore()
{
  keepIt &= ~KeepNonTransMenu;
}

void MenuButtonPress(XEvent *event)
{
  StampTime(event->xbutton.time);
  MenuDontKeepItAnymore();
}

void MenuButtonRelease(XEvent *event)
{
  StampTime(event->xbutton.time);
  if(ButtonCount(event->xbutton.state)>1) {
    if(SpecialProc != NULL) SpecialProc(event, selectedMenuItem);
  } else if(!(keepIt & KeepNonTransMenu)) keepIt &= ~KeepMenuActive;
}

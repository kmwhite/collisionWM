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

#define MENUBORDERW InitS.MenuBorderWidth
#define MENUXOFS InitS.MenuXOffset
#define MENUYOFS InitS.MenuYOffset
#define MENUSCROLLHEIGHT 5

extern UDEScreen TheScreen;
extern Display *disp;
extern HandlerTable *Handle;
extern UltimateContext *ActiveWin;
extern InitStruct InitS;

Menu *activemen=NULL;
MenuItem *selectedMenuItem;
extern short Buttoncount;
Bool quittable,keepIt;
void (*SpecialProc)(XEvent *event,MenuItem *selectedMenuItem);

void Menu2ws(Menu *menu,short ws)
{
  XSetWindowAttributes wattr;
  XGCValues xgcv;
  Node *mi;

  xgcv.foreground=TheScreen.Colors[ws][UDE_Light].pixel;
  XChangeGC(disp,menu->LightGC,GCForeground,&xgcv);
  xgcv.foreground=TheScreen.Colors[ws][UDE_Shadow].pixel;
  XChangeGC(disp,menu->ShadowGC,GCForeground,&xgcv);
  xgcv.foreground=TheScreen.Colors[ws][UDE_StandardText].pixel;
  XChangeGC(disp,menu->TextGC,GCForeground,&xgcv);

  wattr.background_pixel=TheScreen.Colors[ws][UDE_Back].pixel;
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

  menu->name=MyCalloc(strlen(name)+1,sizeof(char));
  strcpy(menu->name,name);
  if(!(menu->Items=NodeListCreate()))
    SeeYa(1,"FATAL: out of memory!");
  menu->font=TheScreen.MenuFont;
  menu->width=XTextWidth(menu->font,menu->name,strlen(menu->name)) +\
                 4 * MENUBORDERW + 2*MENUXOFS;
  menu->ItemHeight=menu->font->ascent + menu->font->descent +\
                 2 * MENUBORDERW + 2*MENUYOFS;
  menu->height=menu->ItemHeight + 2 * MENUBORDERW;

  wattr.background_pixel=TheScreen.Colors[TheScreen.desktop.ActiveWorkSpace]\
                                                            [UDE_Back].pixel;
  wattr.backing_store=True;
  wattr.override_redirect=True;
  wattr.save_under=True;
  menu->win=XCreateWindow(disp,TheScreen.root,0,0,menu->width,menu->height,\
                               0,CopyFromParent,InputOutput,CopyFromParent,\
          CWSaveUnder|CWBackPixel|CWBackingStore|CWOverrideRedirect,&wattr);
  XSelectInput(disp,menu->win,LeaveWindowMask|EnterWindowMask|\
                                         VisibilityChangeMask);

  xgcv.function=GXcopy;
  xgcv.foreground=TheScreen.Colors[TheScreen.desktop.ActiveWorkSpace]
                                  [UDE_Light].pixel;
  xgcv.line_width=0;
  xgcv.line_style=LineSolid;
  xgcv.cap_style=CapButt;
  menu->LightGC=XCreateGC(disp,menu->win,GCFunction|GCForeground|\
                         GCCapStyle|GCLineWidth|GCLineStyle,&xgcv);
  xgcv.function=GXcopy;
  xgcv.foreground=TheScreen.Colors[TheScreen.desktop.ActiveWorkSpace]
                                  [UDE_Shadow].pixel;
  xgcv.line_width=0;
  xgcv.line_style=LineSolid;
  xgcv.cap_style=CapButt;
  menu->ShadowGC=XCreateGC(disp,menu->win,GCFunction|GCForeground|\
                         GCCapStyle|GCLineWidth|GCLineStyle,&xgcv);
  xgcv.function=GXcopy;
  xgcv.foreground=TheScreen.Colors[TheScreen.desktop.ActiveWorkSpace]\
                                             [UDE_StandardText].pixel;
  xgcv.fill_style=FillSolid;
  xgcv.font=TheScreen.MenuFont->fid;
  menu->TextGC=XCreateGC(disp,menu->win,GCFunction|GCForeground|\
                                       GCFillStyle|GCFont,&xgcv);

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

    wattr.background_pixel=TheScreen.Colors[TheScreen.desktop.ActiveWorkSpace]\
                                                              [UDE_Back].pixel;
    wattr.backing_store=True;
    wattr.override_redirect=True;
    item->win=XCreateWindow(disp,menu->win,MENUBORDERW,menu->height-MENUBORDERW\
                  ,menu->width-2*MENUBORDERW,menu->ItemHeight,0,CopyFromParent,\
                         InputOutput,CopyFromParent,CWBackingStore|CWBackPixel|\
                                                     CWOverrideRedirect,&wattr);

    XSelectInput(disp,item->win,EnterWindowMask);

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
  XFreeGC(disp,menu->TextGC);
  XFreeGC(disp,menu->LightGC);
  XFreeGC(disp,menu->ShadowGC);
  XDestroyWindow(disp,menu->win);
  NodeListDelete(&(menu->Items));
  free(menu->name);
  free(menu);
}

void DrawItem(MenuItem *item)
{
  XClearWindow(disp,item->win);
  XDrawString(disp,item->win,item->menu->TextGC,MENUXOFS+MENUBORDERW,\
            MENUYOFS+MENUBORDERW+item->menu->font->ascent,item->name,\
                                                  strlen(item->name));
  if(item->type==I_SUBMENU) {
    XDrawLine(disp,item->win,item->menu->TextGC,item->menu->width-4*\
           MENUBORDERW,item->menu->ItemHeight/2,item->menu->width-10*\
                               MENUBORDERW,item->menu->ItemHeight/2);
    XDrawLine(disp,item->win,item->menu->TextGC,item->menu->width-6.6*\
                 MENUBORDERW,item->menu->ItemHeight/2-1.5*MENUBORDERW,\
             item->menu->width-4*MENUBORDERW,item->menu->ItemHeight/2);
  }
  if(item->type==I_SWITCH_OFF) {
    DrawBevel(item->win,item->menu->width-9*MENUBORDERW,\
                 item->menu->ItemHeight/2-2*MENUBORDERW,\
                      item->menu->width-5*MENUBORDERW-1,\
               item->menu->ItemHeight/2+2*MENUBORDERW-1,\
                        MENUBORDERW,item->menu->LightGC,\
                                   item->menu->ShadowGC);
  }
  if(item->type==I_SWITCH_ON) {
    DrawBevel(item->win,item->menu->width-9*MENUBORDERW,\
                 item->menu->ItemHeight/2-2*MENUBORDERW,\
                      item->menu->width-5*MENUBORDERW-1,\
               item->menu->ItemHeight/2+2*MENUBORDERW-1,\
                       MENUBORDERW,item->menu->ShadowGC,\
                                    item->menu->LightGC);
    XFillRectangle(disp,item->win,item->menu->TextGC,\
                     item->menu->width-8*MENUBORDERW,\
                item->menu->ItemHeight/2-MENUBORDERW,\
                         2*MENUBORDERW,2*MENUBORDERW);
  }
}

void DrawMenu(Menu *menu,int x, int y)
{
  int a,h;
  Node *mi;

  activemen=menu;
  menu->x=x;
  menu->y=y;

  XClearWindow(disp,menu->win);
  XMapSubwindows(disp,menu->win);
  XMoveWindow(disp,menu->win,x,y);
  XMapRaised(disp,menu->win);

  DrawBevel(menu->win,0,0,menu->width-1,menu->height-1,MENUBORDERW,\
                                      menu->LightGC,menu->ShadowGC);
  DrawBevel(menu->win,MENUBORDERW,MENUBORDERW,menu->width-MENUBORDERW-1,\
                             menu->ItemHeight+MENUBORDERW-1,MENUBORDERW,\
                                           menu->ShadowGC,menu->LightGC);

  XDrawString(disp,menu->win,menu->TextGC,MENUXOFS+2*MENUBORDERW,MENUYOFS+\
           2*MENUBORDERW+menu->font->ascent,menu->name,strlen(menu->name));
  mi=NULL;
  while(mi=NodeNext(menu->Items,mi)){
    MenuItem *item;
    item=mi->data;
    if(item->type!=I_LINE) {
      DrawItem(item);
    } else {
      h=item->y;
      for(a=0;a<MENUBORDERW;a++) {
        XDrawLine(disp,menu->win,menu->ShadowGC,a,h-1-a,menu->width-a,h-1-a);
        XDrawLine(disp,menu->win,menu->LightGC,a,a+h,menu->width-a,a+h);
      }
    }
  }
}

void RedrawMenuTreeRecursion(Menu *men)
{
  if(men->parent) RedrawMenuTreeRecursion(men->parent);
  DrawMenu(men,men->x,men->y);
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

MenuItem *StartMenu(Menu *menu, int x, int y, Bool q, Bool mousestarted,
                    void (*prc)(XEvent *event, MenuItem *selectedMenuItem))
{
  selectedMenuItem=NULL;
  Buttoncount=mousestarted ? 1 : 0;
  quittable = q;
  keepIt = (!TheScreen.desktop.flags & UDETransientMenus) || (!mousestarted);
  SpecialProc = prc;

  if(x>(TheScreen.width-menu->width))
    x=TheScreen.width-menu->width;
  if(y>(TheScreen.height-menu->height-1))
    y=TheScreen.height-menu->height-1;

  Menu2ws(menu,TheScreen.desktop.ActiveWorkSpace);
  InstallMenuHandle();
  GrabPointer(TheScreen.root,ButtonPressMask|ButtonReleaseMask|LeaveWindowMask|\
                                     EnterWindowMask,TheScreen.Mice[C_DEFAULT]);

  menu->parent=NULL;
  XInstallColormap(disp,TheScreen.colormap);

  DrawMenu(menu,x,y);

  while(Buttoncount || keepIt){
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

void SelectItem(MenuItem *item)
{
  if(selectedMenuItem){
    DrawItem(selectedMenuItem);
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
    if(Buttoncount) keepIt=False;
    DrawBevel(item->win,0,0,item->menu->width-2*MENUBORDERW-1,\
                         item->menu->ItemHeight-1,MENUBORDERW,\
                     item->menu->ShadowGC,item->menu->LightGC);
    if(selectedMenuItem->menu!=activemen){
      DeleteSubMenus(selectedMenuItem->menu);
      activemen=selectedMenuItem->menu;
    }
    if(selectedMenuItem->type==I_SUBMENU) {
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
      DrawMenu(men,x,y);
    }
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
    SelectItem(mc);
    XChangeActivePointerGrab(disp,ButtonPressMask|ButtonReleaseMask|\
          EnterWindowMask|LeaveWindowMask,TheScreen.Mice[C_WINDOW],TimeStamp);
  }
  else if(quittable) SelectItem(NULL);
}

void MenuLeaveNotify(XEvent *event)
{
  StampTime(event->xcrossing.time);
  if(VisibleMenuWin(event->xcrossing.window)) {
    XChangeActivePointerGrab(disp,ButtonPressMask|ButtonReleaseMask|\
         EnterWindowMask|LeaveWindowMask,TheScreen.Mice[C_DEFAULT],TimeStamp);
    if(quittable) SelectItem(NULL);
  }
}

void RaiseMenuNParents(Menu *men)
{
  if(men->parent) RaiseMenuNParents(men->parent);
  XRaiseWindow(disp,men->win);
}


void MenuVisibility(XEvent *event)
{
  XEvent dummy;

  if((event->xvisibility.window==activemen->win)&&
            (event->xvisibility.state!=VisibilityUnobscured)){
    RaiseMenuNParents(activemen);
  } else {
    while(XCheckTypedWindowEvent(disp,activemen->win,VisibilityNotify,&dummy));
  }
}

void MenuDontKeepItAnymore()
{
  keepIt=False;
  Buttoncount=1;
}

void MenuButtonPress(XEvent *event)
{
  StampTime(event->xbutton.time);
  keepIt=False;
  Buttoncount++;
}

void MenuButtonRelease(XEvent *event)
{
  StampTime(event->xbutton.time);
  Buttoncount--;
  if( Buttoncount && (SpecialProc!=NULL)) {
    SpecialProc(event,selectedMenuItem);
  }
}

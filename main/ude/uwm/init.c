/***  INIT.C: Contains routines for the initialisation of uwm  ***/

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


#define __USE_GNU

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#warning This will probably not compile without config.h
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#error nop! it will not compile.
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/xpm.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>

#include "nodes.h"

#include "uwm.h"
#include "special.h"
#include "init.h"
#include "menu.h"
#include "handlers.h"
#include "workspaces.h"
#include "uwmmenu.h"
#include "winmenumenu.h"
#include "urm.h"
#include "pix.h"
#include "MwmUtil.h"
#include "settings.h"
#include "confparse.h"
#include "uwm_intl.h"

extern char **environ;

const int iconpostab[ICONWINS][2] = {
  {0,14},
  {24,0},
  {24,56},
  {0,42},
  {48,14},
  {48,42},
  {72,28}
};

extern UDEScreen TheScreen;
extern Display *disp;
extern XContext UWMContext;
extern XContext UWMGroupContext;
extern Atom WM_STATE_PROPERTY;
extern Atom WM_CHANGE_STATE;
extern Atom WM_TAKE_FOCUS;
extern Atom WM_DELETE_WINDOW;
extern Atom WM_PROTOCOLS;
extern Atom MOTIF_WM_HINTS;

extern InitStruct InitS;

void SetupCursors(void)
{
  TheScreen.Mice[C_DEFAULT] = XCreateFontCursor(disp,XC_X_cursor);
  TheScreen.Mice[C_N] = XCreateFontCursor(disp,XC_top_side);
  TheScreen.Mice[C_NO] = XCreateFontCursor(disp,XC_top_right_corner);
  TheScreen.Mice[C_O] = XCreateFontCursor(disp,XC_right_side);
  TheScreen.Mice[C_SO] = XCreateFontCursor(disp,XC_bottom_right_corner);
  TheScreen.Mice[C_S] = XCreateFontCursor(disp,XC_bottom_side);
  TheScreen.Mice[C_SW] = XCreateFontCursor(disp,XC_bottom_left_corner);
  TheScreen.Mice[C_W] = XCreateFontCursor(disp,XC_left_side);
  TheScreen.Mice[C_NW] = XCreateFontCursor(disp,XC_top_left_corner);
  TheScreen.Mice[C_DRAG] = XCreateFontCursor(disp,XC_fleur);
  TheScreen.Mice[C_BORDER] = XCreateFontCursor(disp,XC_center_ptr);
  TheScreen.Mice[C_WINDOW] = XCreateFontCursor(disp,XC_top_left_arrow);
}

char *iconfiles[7] = {
  "iconify",
  "close",
  "autorise",
  "back",
  "kill",
  "menu",
  "really"
};

#include "shape.bitmap"

void PrepareIcons()
{
  XpmAttributes xa;
  char filename[512], dirname[300];
  XSetWindowAttributes xswa;
  int a,b;

  a = 0;
  if(InitS.HexPath[0] =='\0') sprintf(dirname, "%sgfx/", TheScreen.udedir);
  else sprintf(dirname,"%s/",InitS.HexPath);

  for(b = 0;b<7;b++){
    xa.valuemask = 0;
    sprintf(filename,"%s%s.xpm",dirname,iconfiles[b]);
    a |= XpmReadFileToPixmap(disp,TheScreen.root,filename,&TheScreen.icons.\
                                  IconPixs[b],&TheScreen.icons.shape,&xa);
    if(TheScreen.icons.shape != None) XFreePixmap(disp,TheScreen.icons.shape);
    xa.valuemask = 0;
    sprintf(filename,"%s%ss.xpm",dirname,iconfiles[b]);
    a |= XpmReadFileToPixmap(disp,TheScreen.root,filename,&TheScreen.icons.\
                            IconSelectPixs[b],&TheScreen.icons.shape,&xa);
    if(TheScreen.icons.shape != None) XFreePixmap(disp,TheScreen.icons.shape);
  }
  if(a) {
    if(InitS.HexPath[0] =='\0') SeeYa(1,"Icon pixmaps could not be loaded");
    else {
      InitS.HexPath[0] = '\0';
      fprintf(TheScreen.errout,
              "UWM: icons specified in uwmrc not found, trying to load default.\n");
      PrepareIcons();
    }
    return;
  }

  TheScreen.icons.shape = XCreateBitmapFromData(disp,TheScreen.root,\
                              shape_bits,shape_width,shape_height);

  xswa.override_redirect = True;
  xswa.save_under = True;

  TheScreen.icons.IconParent = XCreateWindow(disp,TheScreen.root,0,0,104,84,0,\
                                  CopyFromParent,InputOutput,CopyFromParent,
                               (TheScreen.DoesSaveUnders ? CWSaveUnder : 0)|
				                   CWOverrideRedirect,&xswa);
  XSelectInput(disp,TheScreen.icons.IconParent,LeaveWindowMask|\
                                          VisibilityChangeMask);
  XShapeCombineMask(disp,TheScreen.icons.IconParent,ShapeBounding,\
                                iconpostab[0][0],iconpostab[0][1],\
                                   TheScreen.icons.shape,ShapeSet);
  for(a = 0;a<ICONWINS;a++) {
    TheScreen.icons.IconWins[a] = XCreateWindow(disp,TheScreen.icons.IconParent,\
                                    iconpostab[a][0],iconpostab[a][1],32,28,0,\
           CopyFromParent,InputOutput,CopyFromParent,CWOverrideRedirect,&xswa);
    XSelectInput(disp,TheScreen.icons.IconWins[a],EnterWindowMask);
    XSetWindowBackgroundPixmap(disp,TheScreen.icons.IconWins[a],\
                                    TheScreen.icons.IconPixs[a]);
    XShapeCombineMask(disp,TheScreen.icons.IconWins[a],ShapeBounding,0,0,\
                                          TheScreen.icons.shape,ShapeSet);
    XShapeCombineMask(disp,TheScreen.icons.IconParent,ShapeBounding,\
            iconpostab[a][0],iconpostab[a][1],TheScreen.icons.shape,\
                                                         ShapeUnion);
  }
  XMapSubwindows(disp,TheScreen.icons.IconParent);
}

unsigned long AllocColor(XColor *xcol)
{
  if(XAllocColor(disp, TheScreen.colormap, xcol))
    return(xcol->pixel);
  else {
    if((xcol->red + xcol->green + xcol->blue) < (384<<8)){
      fprintf(TheScreen.errout,\
              "UWM: Cannot alloc color: %d;%d;%d. Using BlackPixel.\n",
              xcol->red, xcol->green, xcol->blue);
      return(xcol->pixel = BlackPixel(disp,TheScreen.Screen));
    } else {
      fprintf(TheScreen.errout,\
              "UWM: Cannot alloc color: %d;%d;%d. Using WhitePixel.\n",
              xcol->red, xcol->green, xcol->blue);
      return(xcol->pixel = WhitePixel(disp,TheScreen.Screen));
    }
  }
}

void FreeColor(XColor *color)
{
  if((color->pixel != BlackPixel(disp, TheScreen.Screen))
     && (color->pixel != WhitePixel(disp, TheScreen.Screen)))
    XFreeColors(disp, TheScreen.colormap, &(color->pixel), 1, 0);
}

void InitUWM()
{
  char *env;
  XGCValues xgcv;
  XSetWindowAttributes xswa;
  XEvent event;
  Window otherwmswin;

#ifdef DEVEL
  TheScreen.errout = fopen("/dev/tty10","w");
  fprintf(TheScreen.errout,"UWM: Using this display for Error output!\n");
  fflush(TheScreen.errout);
#else
  TheScreen.errout = stderr;
#endif

/*** some inits to let SeeYa know what to deallocate etc... ***/

  TheScreen.UltimateList= NULL;   /* in case we have to quit very soon */
  TheScreen.AppsMenu= NULL;
  TheScreen.UWMMenu= NULL;

/*** from now on SeeYa can be called 8-) ***/

/*** setting up general X-stuff and initializing some TheScreen-members ***/

  TheScreen.Home= getenv("HOME");
  if(!TheScreen.Home) TheScreen.Home= ".";

  disp= XOpenDisplay(NULL);
  if(!disp)
    SeeYa(1,"Cannot open display");
  /* That's it already in case we can't get a Display */

  TheScreen.Screen= DefaultScreen (disp);
  TheScreen.width= DisplayWidth (disp,TheScreen.Screen);
  TheScreen.height= DisplayHeight (disp,TheScreen.Screen);
#ifndef DISABLE_BACKING_STORE
  TheScreen.DoesSaveUnders = DoesSaveUnders(ScreenOfDisplay(disp,
                                                            TheScreen.Screen));
  TheScreen.DoesBackingStore = DoesBackingStore(ScreenOfDisplay(disp,
                                                            TheScreen.Screen));
#else
  TheScreen.DoesSaveUnders = False;
  TheScreen.DoesBackingStore = False;
#endif

  /* Mark Display as close-on-exec (important for restarts and starting other wms) */
  if(fcntl (ConnectionNumber(disp), F_SETFD, 1) == -1)
    SeeYa(1,"Looks like something is wrong with the Display I got");

  TheScreen.root= RootWindow (disp, TheScreen.Screen);
  if(TheScreen.root == None)
    SeeYa(1,"No proper root Window available");

  xswa.override_redirect = True;
  TheScreen.inputwin = XCreateWindow(disp, TheScreen.root, -2, -2, 1, 1, 0, 0,
                                     InputOnly, CopyFromParent,
                                     CWOverrideRedirect, &xswa);
  if(TheScreen.inputwin == None) SeeYa(1,"couldn't initialize keyboard input");
  XMapWindow(disp, TheScreen.inputwin);

/*** The following WM_Sx stuff is requested by icccm 2.0 ***/
  XSelectInput(disp, TheScreen.inputwin, PropertyChangeMask);
  XStoreName(disp, TheScreen.inputwin, ""); /* obtain a time stamp */
  XWindowEvent(disp, TheScreen.inputwin, PropertyChangeMask, &event);
  TheScreen.now = TheScreen.start_tstamp = event.xproperty.time;
  XSelectInput(disp, TheScreen.inputwin, INPUTWIN_EVENTS); /* set the real event mask */

  XSetErrorHandler((XErrorHandler)UWMErrorHandler);
    /* The real UWM-Error-handler */

  SetupCursors();  /* get the cursors we are going to use */
  InitHandlers();  /* set up the handler jump-tables */

  XDefineCursor(disp,TheScreen.root,TheScreen.Mice[C_DEFAULT]);
  TheScreen.colormap = DefaultColormap(disp,DefaultScreen(disp));

  /*** find out where ude is installed. ***/
  env = getenv("UDEdir");
  if(!env) {
    /* UDE_DIR is a macro that is defined in the Makefile (by automake) and
       it will contain the default ude directory which is the same pkgdatadir
       it will usually be /usr/local/share/ude */
    env = UDE_DIR;
  }
  if('/' != (*(strchr(env, '\0') - 1))) {
    TheScreen.udedir = MyCalloc(strlen(env) + 2, sizeof(char));
    sprintf(TheScreen.udedir,"%s/", env);
  } else {
    TheScreen.udedir = MyCalloc(strlen(env) + 1, sizeof(char));
    sprintf(TheScreen.udedir,"%s", env);
  }
  env = MyCalloc(strlen(TheScreen.udedir) + strlen("UDEdir = ") + 1,
                 sizeof(char));
  sprintf(env, "UDEdir = %s", TheScreen.udedir);
  putenv(env);   /* errors setting UDEdir are not fatal, so we ignore them. */

  TheScreen.cppincpaths = malloc(sizeof(char) * (1 + strlen("-I ")
         + strlen(TheScreen.Home) + strlen("/.ude/config/ -I ") 
         + strlen(TheScreen.udedir) + strlen("config/ -I ./")));
  sprintf(TheScreen.cppincpaths, "-I %s/.ude/config/ -I %sconfig/ -I ./",
          TheScreen.Home, TheScreen.udedir);

  TheScreen.urdbcppopts = Initurdbcppopts();

/*** set up some uwm-specific stuff ***/

  UWMContext = XUniqueContext();   /* Create Context to store UWM-data */
  UWMGroupContext = XUniqueContext();   /* Create Context to store group data */
  TheScreen.MenuContext = XUniqueContext();   
  TheScreen.MenuFrameContext = XUniqueContext();   
    /* Create Context to store menu-data */

  /* Atoms for selections */
  TheScreen.VERSION_ATOM = XInternAtom(disp,"VERSION",False);
  TheScreen.ATOM_PAIR = XInternAtom(disp,"ATOM_PAIR",False);
  TheScreen.TARGETS = XInternAtom(disp,"TARGETS",False);
  TheScreen.MULTIPLE = XInternAtom(disp,"MULTIPLE",False);
  TheScreen.TIMESTAMP = XInternAtom(disp,"TIMESTAMP",False);

  /* window management related Atoms */
  WM_STATE_PROPERTY = XInternAtom(disp,"WM_STATE",False);
  WM_CHANGE_STATE = XInternAtom(disp,"WM_CHANGE_STATE",False);
  WM_TAKE_FOCUS = XInternAtom(disp,"WM_TAKE_FOCUS",False);
  WM_DELETE_WINDOW = XInternAtom(disp,"WM_DELETE_WINDOW",False);
  WM_PROTOCOLS = XInternAtom(disp,"WM_PROTOCOLS",False);
  MOTIF_WM_HINTS = XInternAtom(disp,_XA_MOTIF_WM_HINTS,False);

  if(!(TheScreen.UltimateList = NodeListCreate()))
    SeeYa(1,"FATAL: out of memory!");

  /*** read configuration files ***/

  ReadConfigFile();

  PrepareIcons();

  /*** prepare menus ***/

  CreateUWMMenu();
  InitWSProcs();

  /*** set up more UWM-specific stuff ***/
  xgcv.function = GXinvert;
  xgcv.line_style = LineSolid;
  xgcv.line_width = settings.global_settings->BorderWidth;
  xgcv.cap_style = CapButt;
  xgcv.subwindow_mode = IncludeInferiors;
  TheScreen.rubbercontext = XCreateGC(disp,TheScreen.root,GCFunction|\
          GCCapStyle|GCLineStyle|GCLineWidth|GCSubwindowMode,&xgcv);
  xgcv.function = GXcopy;
  xgcv.line_style = LineSolid;
  xgcv.line_width = 0;
  xgcv.cap_style = CapButt;
  xgcv.foreground = BlackPixel(disp,TheScreen.Screen);
  TheScreen.blackcontext = XCreateGC(disp,TheScreen.root,GCFunction|\
            GCCapStyle|GCLineStyle|GCLineWidth|GCForeground,&xgcv);

  xgcv.function = GXcopy;
  xgcv.foreground = ActiveWSSettings->BackgroundLight->pixel;
  xgcv.line_width = 0;
  xgcv.line_style = LineSolid;
  xgcv.cap_style = CapButt;
  TheScreen.MenuLightGC = XCreateGC(disp, TheScreen.root, GCFunction
                                  | GCForeground | GCCapStyle | GCLineWidth
				  | GCLineStyle, &xgcv);
  xgcv.function = GXcopy;
  xgcv.foreground = ActiveWSSettings->BackgroundShadow->pixel;
  xgcv.line_width = 0;
  xgcv.line_style = LineSolid;
  xgcv.cap_style = CapButt;
  TheScreen.MenuShadowGC = XCreateGC(disp, TheScreen.root, GCFunction
                                  | GCForeground | GCCapStyle | GCLineWidth
				  | GCLineStyle, &xgcv);
  xgcv.function = GXcopy;
  xgcv.foreground = ActiveWSSettings->BackgroundColor->pixel;
  xgcv.line_width = 0;
  xgcv.line_style = LineSolid;
  xgcv.cap_style = CapButt;
  TheScreen.MenuBackGC = XCreateGC(disp, TheScreen.root, GCFunction
                                 | GCForeground | GCCapStyle | GCLineWidth
				 | GCLineStyle, &xgcv);
  xgcv.function = GXcopy;
  xgcv.foreground = ActiveWSSettings->ForegroundColor->pixel;
  xgcv.fill_style = FillSolid;
  xgcv.font = settings.global_settings->Font.xfs->fid;
  TheScreen.MenuTextGC = XCreateGC(disp,TheScreen.root, GCFunction
                                 | GCForeground | GCFillStyle | GCFont, &xgcv);

  XGrabKey(disp, XKeysymToKeycode(disp,XK_Right), UWM_MODIFIERS,
           TheScreen.root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(disp, XKeysymToKeycode(disp,XK_Left), UWM_MODIFIERS,
           TheScreen.root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(disp, XKeysymToKeycode(disp,XK_Up), UWM_MODIFIERS,
           TheScreen.root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(disp, XKeysymToKeycode(disp,XK_Down), UWM_MODIFIERS,
           TheScreen.root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(disp, XKeysymToKeycode(disp,XK_Page_Down), UWM_MODIFIERS,
           TheScreen.root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(disp, XKeysymToKeycode(disp,XK_Page_Up), UWM_MODIFIERS,
           TheScreen.root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(disp, XKeysymToKeycode(disp,XK_End), UWM_MODIFIERS,
           TheScreen.root, True, GrabModeAsync, GrabModeAsync);
  XGrabButton(disp, AnyButton, UWM_MODIFIERS, TheScreen.root,
              True, ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
	      GrabModeAsync, None, None);

  BroadcastWorkSpacesInfo();
  UpdateDesktop();

/***/XSync(disp,False);
/***/sleep(1);
/***/exit(0);

  /* Set the background for the current desktop. */
  SetWSBackground();
}

uwm_global_settings global_settings = {
	  10,			/* BorderWidth */
	  3,			/* TransientBorderWidth */
	  0,			/* TitleHeight */
	  2,			/* FrameBevelWidth */
	  39,			/* FrameFlags */
	  0,			/* LayoutFlags */
	  2,			/* BevelWidth */
	  2,			/* MenuXOffset */
	  2,			/* MenuYOffset */
	  { NULL, NULL },	/* TitleFont */
	  { NULL, NULL },	/* Font */
	  { NULL, NULL },	/* MonoFont */
	  { NULL, NULL },	/* HighlightFont */
	  { NULL, NULL },	/* InactiveFont */
	  NULL,			/* StartScript */
	  NULL,			/* StopScript */
	  NULL,			/* ResourceFile */
	  NULL,			/* HexPath */
	  5,			/* PlacementStrategy */
	  0,			/* PlacementThreshold */
	  0,			/* OpaqueMoveSize */
	  0,			/* MaxWinWidth */
	  0,			/* MaxWinHeight */
	  -1,			/* WarpPointerToNewWinH */
	  -1,			/* WarpPointerToNewWinV */
	  10,			/* SnapDistance */
	  0			/* BehaviourFlags */
	};

uwm_settings settings = {
	  &global_settings,	/* global_settings */
	  0,			/* workspace_settings_count */
	  NULL			/* workspace_settings */
	};

int ReadConfigFile()
{
  int a;

  uwm_yyparse_wrapper("uwmrc.new");

  /* initialize gettext defaults */
  strncpy(uwm_default_ws_name, _("Default Workspace"),
          UWM_DEFAULT_NAME_LENGTH);
  uwm_default_ws_name[UWM_DEFAULT_NAME_LENGTH - 1] = '\0';

  /* complete global options with defaults */
  for(a = 0; a < UWM_GLOBAL_OPTION_NR; a ++) {
    if(uwm_global_index[a].default_val_string
       && (!(*(((void **)settings.global_settings)
               + uwm_global_index[a].offset)))
       && (uwm_yy_to_setting_table[uwm_global_index[a].type][UWM_YY_STRING])) {
      YYSTYPE d;
      char *errmsg;
      d.string = MyStrdup(uwm_global_index[a].default_val_string);
      if(errmsg = uwm_yy_to_setting_table
                        [uwm_global_index[a].type][UWM_YY_STRING]
                        (&d, &(uwm_global_index[a]),
			 settings.global_settings)) {
        SeeYa(1, errmsg);
      }
    }
  }
  /* complete workspace options with defaults */
  for(a = 0; a < settings.workspace_settings_count; a++) {
    int b;
    if(!settings.workspace_settings[a]) {
      settings.workspace_settings[a] = MyCalloc(1,
						sizeof(uwm_workspace_settings));
      memset(settings.workspace_settings[a], 0, sizeof(uwm_workspace_settings));
    }
    for(b = 0; b < UWM_WORKSPACE_OPTION_NR; b ++) {
      if(uwm_workspace_index[b].default_val_string
         && (!(*(((void **)settings.workspace_settings[a])
                 + uwm_workspace_index[b].offset)))
         && (uwm_yy_to_setting_table[uwm_workspace_index[b].type]
	                            [UWM_YY_STRING])) {
        YYSTYPE d;
        char *errmsg;
        d.string = MyStrdup(uwm_workspace_index[b].default_val_string);
        if(errmsg = uwm_yy_to_setting_table
                          [uwm_workspace_index[b].type][UWM_YY_STRING]
                          (&d, &(uwm_workspace_index[b]),
			   settings.workspace_settings[a])) {
          SeeYa(1, errmsg);
        }
      }
    }
/***/fprintf(TheScreen.errout, "%s\n", settings.workspace_settings[a]->Name);
  }
}


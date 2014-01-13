/*** /

 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com

 / ***/

// gollybase
#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "jvnalgo.h"
#include "generationsalgo.h"
#include "ruleloaderalgo.h"

// gui-common
#include "algos.h"      // for InitAlgorithms, NumAlgos, etc
#include "prefs.h"      // for GetPrefs, MAX_MAG, tempdir, etc
#include "layer.h"      // for ResizeLayers, currlayer
#include "control.h"    // for SetMinimumStepExponent, NextGeneration, etc
#include "file.h"       // for NewPattern
#include "view.h"       // for fullscreen, TouchBegan, etc
#include "status.h"     // for SetMessage, etc
#include "utils.h"      // for Beep, etc
#include "undo.h"       // for currlayer->undoredo->...
#include "render.h"     // for InitOGLES2, DrawPattern

#include "webcalls.h"   // for UpdateStatus, refresh_pattern, etc

#include <stdlib.h>
#include <stdio.h>
#include <GL/glfw.h>
#include <emscripten/emscripten.h>

// -----------------------------------------------------------------------------

static int currwd = 960, currht = 960;      // initial size of viewport
static double last_time;                    // when NextGeneration was last called

// -----------------------------------------------------------------------------

static void InitPaths()
{
    //!!! set tempdir, supplieddir, etc
    
    clipfile = tempdir + "golly_clipboard";
    prefsfile = "GollyPrefs";                   // where will this be saved???
}

// -----------------------------------------------------------------------------

static int InitGL()
{
    if (glfwInit() != GL_TRUE) {
        printf("glfwInit failed!\n");
        return GL_FALSE;
    }

    if (glfwOpenWindow(currwd, currht, 8, 8, 8, 8, 0, 0, GLFW_WINDOW) != GL_TRUE) {
        printf("glfwOpenWindow failed!\n");
        return GL_FALSE;
    }
    glfwSetWindowTitle("Golly");
    
    if (!InitOGLES2()) {
        printf("InitOGLES2 failed!\n");
        return GL_FALSE;
    }

    last_time = glfwGetTime();
    return GL_TRUE;
}

// -----------------------------------------------------------------------------

static void OnSurfaceCreated() {
    // we only do 2D drawing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FOG);

    glEnable(GL_BLEND);
    // this blending function seems similar to the one used in desktop Golly
    // (ie. selected patterns look much the same)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(1.0, 1.0, 1.0, 1.0);
}

// -----------------------------------------------------------------------------

static void OnSurfaceChanged(int width, int height) {
    glViewport(0, 0, width, height);
    currwd = width;
    currht = height;
    if (currwd != currlayer->view->getwidth() ||
        currht != currlayer->view->getheight()) {
        // update size of viewport
        ResizeLayers(currwd, currht);
        UpdatePattern();
    }
}

// -----------------------------------------------------------------------------

extern "C" {

void StartStop()
{
    if (generating) {
        StopGenerating();
        // generating is now false so change button label from "Stop" to "Start"
        EM_ASM (
           Module.setButtonLabel('startStop', 'Start') ;
        ) ;
    } else if (StartGenerating()) {
        // generating is now true so change button label to "Stop"
        EM_ASM (
           Module.setButtonLabel('startStop', 'Stop') ;
        ) ;
    }
}

} // extern "C"

// -----------------------------------------------------------------------------

static void StopIfGenerating()
{
    if (generating) {
        StopGenerating();
        // generating flag is now false so change button label to "Start"
        EM_ASM (
           Module.setButtonLabel('startStop', 'Start') ;
        ) ;
    }
}

// -----------------------------------------------------------------------------

static void OneGeneration()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // previous NextGeneration() hasn't finished
        // try again after a short delay!!!???
        return;
    }

    NextGeneration(false);
    UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

static void GoSlower()
{
    if (currlayer->currexpo > minexpo) {
        currlayer->currexpo--;
        SetGenIncrement();
        UpdateStatus();
    } else {
        Beep();
    }
}

// -----------------------------------------------------------------------------

static void GoFaster()
{
    currlayer->currexpo++;
    SetGenIncrement();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

static void StepBy1()
{
    // reset step exponent to 0
    currlayer->currexpo = 0;
    SetGenIncrement();
    UpdateStatus();
}

// -----------------------------------------------------------------------------

static void ZoomOut()
{
    currlayer->view->unzoom();
    UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

static void ZoomIn()
{
    if (currlayer->view->getmag() < MAX_MAG) {
        currlayer->view->zoom();
        UpdatePatternAndStatus();
    } else {
        Beep();
    }
}

// -----------------------------------------------------------------------------

static void Scale1to1()
{
    if (currlayer->view->getmag() != 0) {
        currlayer->view->setmag(0);
        UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

static void CycleAlgo()
{
    // cycle to next algorithm
    int algoindex = currlayer->algtype + 1;
    if (algoindex == NumAlgos()) {
        algoindex = 0;  // go back to QuickLife
    }
    ChangeAlgorithm(algoindex, "xxx");
    // unknown rule will be changed to new algo's default rule
}

// -----------------------------------------------------------------------------

static void FitPattern()
{
    FitInView(1);
    UpdatePatternAndStatus();
}

// -----------------------------------------------------------------------------

static void Help()
{
    // do something else eventually!!!
    EM_ASM(
        alert('You can use these keyboard commands:\n\n' +
              'return -- start/stop generating\n' +
              'space -- do 1 generation\n' +
              '- or _ -- go slower\n' +
              '+ or = -- go faster\n' +
              '0 -- set step exponent to 0\n' +
              '1 -- set scale to 1:1\n' +
              '[ -- zoom out\n' +
              '] -- zoom in\n' +
              'a -- select all\n' +
              'A -- cycle to next algorithm\n' +
              'f -- fit\n' +
              'h -- help\n' +
              'i -- toggle icon mode\n' +
              'n -- new (empty) universe\n' +
              'r -- reset\n' +
              'R -- random pattern\n' +
              'v -- paste\n' +
              'V -- cancel paste\n' +
              'z -- undo\n' +
              'Z -- redo\n'
             );
    );
}

// -----------------------------------------------------------------------------

static void ToggleIconMode()
{
    showicons = !showicons;
    UpdatePattern();
}

// -----------------------------------------------------------------------------

static void NewUniverse()
{
    // undo/redo history is about to be cleared so no point calling RememberGenFinish
    // if we're generating a (possibly large) pattern
    bool saveundo = allowundo;
    allowundo = false;
    StopIfGenerating();
    allowundo = saveundo;
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    NewPattern();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

static void ChangePrefs()
{
    //!!! show some sort of modal dialog box that lets user change various settings???
    
    SavePrefs();    // where will this write GollyPrefs file???!!!
}

// -----------------------------------------------------------------------------

static void Reset()
{
    if (currlayer->algo->getGeneration() == currlayer->startgen) return;

    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    ResetPattern();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

static void RandomPattern()
{
    NewUniverse();
    currlayer->currsel.SetRect(-10, -10, 20, 20);
    currlayer->currsel.RandomFill();
    currlayer->currsel.Deselect();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

static void Paste()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    if (waitingforpaste) {
        //!!! SelectPasteAction();
    } else {
        PasteClipboard();
        UpdatePatternAndStatus();
    }
}

// -----------------------------------------------------------------------------

static void Undo()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    currlayer->undoredo->UndoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

static void Redo()
{
    StopIfGenerating();
    
    if (event_checker > 0) {
        // try again after a short delay!!!???
        return;
    }
    
    currlayer->undoredo->RedoChange();
    UpdateEverything();
}

// -----------------------------------------------------------------------------

static void OnCharPressed(int ch, int action)
{
    if (action != GLFW_PRESS) return;
    ClearMessage();
    switch (ch) {
        case 13  : StartStop(); break;
        case ' ' : OneGeneration(); break;
        case '_' : GoSlower(); break;
        case '-' : GoSlower(); break;
        case '+' : GoFaster(); break;
        case '=' : GoFaster(); break;
        case '0' : StepBy1(); break;
        case '1' : Scale1to1(); break;
        case '[' : ZoomOut(); break;
        case ']' : ZoomIn(); break;
        case 'a' : SelectAll(); break;
        case 'A' : CycleAlgo(); break;
        case 'f' : FitPattern(); break;
        case 'h' : Help(); break;
        case 'i' : ToggleIconMode(); break;
        case 'n' : NewUniverse(); break;
        case 'p' : ChangePrefs(); break;
        case 'r' : Reset(); break;
        case 'R' : RandomPattern(); break;
        case 'v' : Paste(); break;
        case 'V' : AbortPaste(); UpdatePattern(); break;
        case 'z' : Undo(); break;
        case 'Z' : Redo(); break;
    }
    // note that arrow keys aren't detected here!!! may need to use OnKeyPressed???
    // DEBUG: printf("char=%c (%i)\n", ch, ch);
}

// -----------------------------------------------------------------------------

/* forget this callback???!!! it returns strange results for keys like '[' and ']'
static void OnKeyPressed(int key, int action)
{
    printf("key=%i action=%i\n", key, action);
}
*/

// -----------------------------------------------------------------------------

static bool touching = false;

static void OnMouseClick(int button, int action)
{
    if (action == GLFW_PRESS) {
        int x, y;
        glfwGetMousePos(&x, &y);
        // DEBUG: printf("click at x=%d y=%d\n", x, y);
    
        ClearMessage();
        
        // check for click outside viewport
        if (x < 0 || x >= currwd || y < 0 || y >= currht) {
            if (touching) TouchEnded();
            touching = false;
            return;
        }
        
        TouchBegan(x, y);
        touching = true;
    
    } else if (action == GLFW_RELEASE) {
        if (touching) TouchEnded();
        touching = false;
    }
}

// -----------------------------------------------------------------------------

static void OnMouseMove(int x, int y)
{
    int mousestate = glfwGetMouseButton(GLFW_MOUSE_BUTTON_LEFT);
    if (mousestate == GLFW_PRESS) {
        int x, y;
        glfwGetMousePos(&x, &y);
        // DEBUG: printf("moved to x=%d y=%d\n", x, y);
        
        // ignore move outside viewport
        if (x < 0 || x >= currwd || y < 0 || y >= currht) return;
    
        TouchMoved(x, y);
    } else {
        // update XY position in status bar
        //!!!
    }
}

// -----------------------------------------------------------------------------

static void DoFrame()
{
    if (generating && event_checker == 0) {
        if (currlayer->currexpo < 0) {
            // get current delay (in secs)
            float delay = GetCurrentDelay() / 1000.0;
            double current_time = glfwGetTime();
            // check if it's time to call NextGeneration
            if (current_time - last_time >= delay) {
                NextGeneration(true);
                UpdatePatternAndStatus();
                last_time = current_time;
            }
        } else {
            NextGeneration(true);
            UpdatePatternAndStatus();
        }
    }
    
    if (refresh_pattern) {
        refresh_pattern = false;
        DrawPattern(currindex);
    }
    
    glfwSwapBuffers();
}

// -----------------------------------------------------------------------------

int EMSCRIPTEN_KEEPALIVE main()
{
    SetMessage("This is Golly 0.1 for the web.  Copyright 2014 The Golly Gang.");
    InitPaths();                // init tempdir, prefsfile, etc
    MAX_MAG = 5;                // maximum cell size = 32x32
    InitAlgorithms();           // must initialize algoinfo first
    GetPrefs();                 // load user's preferences
    SetMinimumStepExponent();   // for slowest speed
    AddLayer();                 // create initial layer (sets currlayer)
    NewPattern();               // create new, empty universe
    UpdateStatus();             // show initial message

    // show timing messages when generating stops!!!
    showtiming = true;

    // test bounded grid!!!
    // currlayer->algo->setrule("B3/S23:T10,6");

    if (InitGL() == GL_TRUE) {
        OnSurfaceCreated();
        OnSurfaceChanged(currwd, currht);
        //!!!??? glfwSetKeyCallback(OnKeyPressed);
        glfwSetCharCallback(OnCharPressed);
        glfwSetMouseButtonCallback(OnMouseClick);
        glfwSetMousePosCallback(OnMouseMove);
        emscripten_set_main_loop(DoFrame, 0, 1);
    }

    glfwTerminate();
    return 0;
}
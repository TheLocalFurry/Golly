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

#include "wx/wxprec.h"      // for compilers that support precompilation
#ifndef WX_PRECOMP
    #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "wx/rawbmp.h"      // for wxAlphaPixelData

#include "wxgolly.h"        // for mainptr, viewptr
#include "wxmain.h"         // for mainptr->...
#include "wxview.h"         // for viewptr->...
#include "wxlayer.h"        // for currlayer->...
#include "wxprefs.h"        // for showoverlay
#include "wxoverlay.h"

#include <vector>           // for std::vector
#include <cstdio>           // for FILE*, etc

// -----------------------------------------------------------------------------

Overlay* curroverlay = NULL;    // pointer to current overlay

const char* no_overlay = "overlay has not been created";

// -----------------------------------------------------------------------------

Overlay::Overlay()
{
    pixmap = NULL;
}

// -----------------------------------------------------------------------------

Overlay::~Overlay()
{
    DeleteOverlay();
}

// -----------------------------------------------------------------------------

void Overlay::DeleteOverlay()
{
    if (pixmap) {
        free(pixmap);
        pixmap = NULL;
    }
    
    std::map<std::string,Clip*>::iterator it;
    for (it = clips.begin(); it != clips.end(); ++it) {
        delete it->second;
    }
    clips.clear();
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCreate(const char* args)
{
    // don't set wd and ht until we've checked the args are valid
    int w, h;
    if (sscanf(args, " %d %d", &w, &h) != 2) {
        return OverlayError("create command requires 2 arguments");
    }
    
    if (w <= 0) return OverlayError("width of overlay must be > 0");
    if (h <= 0) return OverlayError("height of overlay must be > 0");
    
    // given width and height are ok
    wd = w;
    ht = h;
    
    // delete any existing pixmap
    DeleteOverlay();

    // use calloc so all pixels will be 100% transparent (alpha = 0)
    pixmap = (unsigned char*) calloc(wd * ht * 4, 1);
    if (pixmap == NULL) return OverlayError("not enough memory to create overlay");
    
    // initialize RGBA values to opaque white
    r = g = b = a = 255;
    
    // don't do alpha blending initially
    alphablend = false;
    
    only_draw_overlay = false;
    
    // initial position of overlay is in top left corner of current layer
    pos = topleft;
    
    ovcursor = wxSTANDARD_CURSOR;
    cursname = "arrow";

    // identity transform
    axx = 1;
    axy = 0;
    ayx = 0;
    ayy = 1;
    identity = true;
    
    // initialize current font used by text command
    currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    fontname = "default";
    fontsize = 11;
    currfont.SetPointSize(fontsize);
    
    return NULL;
}

// -----------------------------------------------------------------------------

bool Overlay::PointInOverlay(int vx, int vy, int* ox, int* oy)
{
    if (pixmap == NULL) return false;
    
    int viewwd, viewht;
    viewptr->GetClientSize(&viewwd, &viewht);
    if (viewwd <= 0 || viewht <= 0) return false;
    
    int x = 0;
    int y = 0;
    switch (pos) {
        case topleft:
            break;
        case topright:
            x = viewwd - wd;
            break;
        case bottomright:
            x = viewwd - wd;
            y = viewht - ht;
            break;
        case bottomleft:
            y = viewht - ht;
            break;
        case middle:
            x = (viewwd - wd) / 2;
            y = (viewht - ht) / 2;
            break;
    }

    if (vx < x) return false;
    if (vy < y) return false;
    if (vx >= x + wd) return false;
    if (vy >= y + ht) return false;
    
    *ox = vx - x;
    *oy = vy - y;

    return true;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoPosition(const char* args)
{
    if (strncmp(args+1, "topleft", 7) == 0) {
        pos = topleft;
        
    } else if (strncmp(args+1, "topright", 8) == 0) {
        pos = topright;
        
    } else if (strncmp(args+1, "bottomright", 11) == 0) {
        pos = bottomright;
        
    } else if (strncmp(args+1, "bottomleft", 10) == 0) {
        pos = bottomleft;
        
    } else if (strncmp(args+1, "middle", 6) == 0) {
        pos = middle;
        
    } else {
        return OverlayError("unknown position");
    }
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSetRGBA(const char* args)
{
    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("rgba command requires 4 arguments");
    }
    
    if (a1 < 0 || a1 > 255 ||
        a2 < 0 || a2 > 255 ||
        a3 < 0 || a3 > 255 ||
        a4 < 0 || a4 > 255) {
        return OverlayError("rgba values must be from 0 to 255");
    }
    
    unsigned char oldr = r;
    unsigned char oldg = g;
    unsigned char oldb = b;
    unsigned char olda = a;
    
    r = (unsigned char) a1;
    g = (unsigned char) a2;
    b = (unsigned char) a3;
    a = (unsigned char) a4;
    
    // return old values
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", oldr, oldg, oldb, olda);
    return result;
}

// -----------------------------------------------------------------------------

#define PixelInOverlay(x,y) \
    (x >= 0 && x < wd && \
     y >= 0 && y < ht)

// -----------------------------------------------------------------------------

void Overlay::DrawPixel(int x, int y)
{
    // caller must guarantee that pixel is within pixmap
    unsigned char* p = pixmap + y*wd*4 + x*4;
    if (alphablend && a < 255) {
        // source pixel is translucent so blend with destination pixel;
        // see https://en.wikipedia.org/wiki/Alpha_compositing#Alpha_blending
        unsigned char destr = p[0];
        unsigned char destg = p[1];
        unsigned char destb = p[2];
        unsigned char desta = p[3];
        float alpha = a / 255.0;
        if (desta == 255) {
            // destination pixel is opaque
            p[0] = int(alpha * (r - destr) + destr);
            p[1] = int(alpha * (g - destg) + destg);
            p[2] = int(alpha * (b - destb) + destb);
            // no need to change p[3] (alpha stays at 255)
        } else {
            // destination pixel is translucent
            float inva = 1.0 - alpha;
            float destalpha = desta / 255.0;
            float outa = alpha + destalpha * inva;
            p[3] = int(outa * 255);
            if (p[3] > 0) {
                p[0] = int((r * alpha + destr * destalpha * inva) / outa);
                p[1] = int((g * alpha + destg * destalpha * inva) / outa);
                p[2] = int((b * alpha + destb * destalpha * inva) / outa);
            }
        }
    } else {
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSetPixel(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    if (sscanf(args, "%d %d", &x, &y) != 2) {
        return OverlayError("set command requires 2 arguments");
    }
    
    // ignore pixel if outside pixmap edges
    if (PixelInOverlay(x, y)) DrawPixel(x, y);

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoGetPixel(const char* args)
{
    if (pixmap == NULL) return "";

    int x, y;
    if (sscanf(args, "%d %d", &x, &y) != 2) {
        return OverlayError("get command requires 2 arguments");
    }

    // check if x,y is outside pixmap
    if (!PixelInOverlay(x, y)) return "";
    
    unsigned char* p = pixmap + y*wd*4 + x*4;
    static char result[16];
    sprintf(result, "%hhu %hhu %hhu %hhu", p[0], p[1], p[2], p[3]);
    return result;
}

// -----------------------------------------------------------------------------

bool Overlay::TransparentPixel(int x, int y)
{
    if (pixmap == NULL) return false;

    // check if x,y is outside pixmap
    if (!PixelInOverlay(x, y)) return false;
    
    unsigned char* p = pixmap + y*wd*4 + x*4;
    
    // return true if alpha value is 0
    return p[3] == 0;
}

// -----------------------------------------------------------------------------

void Overlay::SetOverlayCursor()
{
    #ifdef __WXMAC__
        wxSetCursor(*ovcursor);
    #endif
    viewptr->SetCursor(*ovcursor);
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCursor(const char* args)
{
    if (strncmp(args+1, "arrow", 5) == 0) {
        ovcursor = wxSTANDARD_CURSOR;

    } else if (strncmp(args+1, "current", 7) == 0) {
        ovcursor = currlayer->curs;

    } else if (strncmp(args+1, "pencil", 6) == 0) {
        ovcursor = curs_pencil;

    } else if (strncmp(args+1, "pick", 4) == 0) {
        ovcursor = curs_pick;

    } else if (strncmp(args+1, "cross", 5) == 0) {
        ovcursor = curs_cross;

    } else if (strncmp(args+1, "hand", 4) == 0) {
        ovcursor = curs_hand;

    } else if (strncmp(args+1, "zoomin", 6) == 0) {
        ovcursor = curs_zoomin;

    } else if (strncmp(args+1, "zoomout", 7) == 0) {
        ovcursor = curs_zoomout;

    } else if (strncmp(args+1, "hidden", 6) == 0) {
        ovcursor = curs_hidden;

    } else {
        return OverlayError("unknown cursor");
    }

    viewptr->CheckCursor(mainptr->infront);
    
    std::string oldcursor = cursname;
    cursname = args+1;
    
    // return old cursor name
    static std::string result;
    result = oldcursor;
    return result.c_str();
}

// -----------------------------------------------------------------------------

void Overlay::CheckCursor()
{
    // the cursor needs to be checked if the pixmap data has changed, but that's
    // highly likely if we call this routine at the end of DrawOverlay
    viewptr->CheckCursor(mainptr->infront);
}

// -----------------------------------------------------------------------------

const char* Overlay::DoGetXY()
{
    if (pixmap == NULL) return "";
    if (!mainptr->infront) return "";
    
    wxPoint pt = viewptr->ScreenToClient( wxGetMousePosition() );
    
    int ox, oy;
    if (PointInOverlay(pt.x, pt.y, &ox, &oy)) {
        static char result[32];
        sprintf(result, "%d %d", ox, oy);
        return result;
    } else {
        return "";
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoLine(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);
    
    int x1, y1, x2, y2;
    if (sscanf(args, " %d %d %d %d", &x1, &y1, &x2, &y2) != 4) {
        return OverlayError("line command requires 4 arguments");
    }
    
    if (x1 == x2 && y1 == y2) {
        if (PixelInOverlay(x1, y1)) DrawPixel(x1, y1);
        return NULL;
    }
    
    // draw a line of pixels from x1,y1 to x2,y2 using Bresenham's algorithm
    int dx = x2 - x1;
    int ax = abs(dx) * 2;
    int sx = dx < 0 ? -1 : 1;
    
    int dy = y2 - y1;
    int ay = abs(dy) * 2;
    int sy = dy < 0 ? -1 : 1;
    
    if (ax > ay) {
        int d = ay - (ax / 2);
        while (x1 != x2) {
            if (PixelInOverlay(x1, y1)) DrawPixel(x1, y1);
            if (d >= 0) {
                y1 = y1 + sy;
                d = d - ax;
            }
            x1 = x1 + sx;
            d = d + ay;
        }
    } else {
        int d = ax - (ay / 2);
        while (y1 != y2) {
            if (PixelInOverlay(x1, y1)) DrawPixel(x1, y1);
            if (d >= 0) {
                x1 = x1 + sx;
                d = d - ay;
            }
            y1 = y1 + sy;
            d = d + ax;
        }
    }
    if (PixelInOverlay(x2, y2)) DrawPixel(x2, y2);
    
    return NULL;
}

// -----------------------------------------------------------------------------

void Overlay::FillRect(int x, int y, int w, int h)
{
    if (alphablend && a < 255) {
        for (int j=y; j<y+h; j++) {
            for (int i=x; i<x+w; i++) {
                // caller ensures pixel is within pixmap
                DrawPixel(i,j);
            }
        }
    } else {
        int rowbytes = wd * 4;
        unsigned char* p = pixmap + y*rowbytes + x*4;
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;
        
        // copy above pixel to remaining pixels in first row
        unsigned char* dest = p;
        for (int i=1; i<w; i++) {
            dest += 4;
            memcpy(dest, p, 4);
        }
        
        // copy first row to remaining rows
        dest = p;
        int wbytes = w * 4;
        for (int i=1; i<h; i++) {
            dest += rowbytes;
            memcpy(dest, p, wbytes);
        }
    }
}

// -----------------------------------------------------------------------------

#define RectOutsideOverlay(x,y,w,h) \
    (x >= wd || x + w <= 0 || \
     y >= ht || y + h <= 0)

#define RectInsideOverlay(x,y,w,h) \
    (x >= 0 && x + w <= wd && \
     y >= 0 && y + h <= ht)

// -----------------------------------------------------------------------------

const char* Overlay::DoFill(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);
    
    if (args[0] == ' ') {
        int x, y, w, h;
        if (sscanf(args, " %d %d %d %d", &x, &y, &w, &h) != 4) {
            return OverlayError("fill command requires 0 or 4 arguments");
        }
        
        if (w <= 0) {
            // treat non-positive width as inset from current width
            w = wd + w;
            if (w <= 0) return NULL;
        }
        if (h <= 0) {
            // treat non-positive height as inset from current height
            h = ht + h;
            if (h <= 0) return NULL;
        }
        
        // ignore rect if completely outside pixmap edges
        if (RectOutsideOverlay(x, y, w, h)) return NULL;
        
        // clip any part of rect outside pixmap edges
        int xmax = x + w - 1;
        int ymax = y + h - 1;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (xmax >= wd) xmax = wd - 1;
        if (ymax >= ht) ymax = ht - 1;
        w = xmax - x + 1;
        h = ymax - y + 1;
        
        // fill visible rect with current RGBA values
        FillRect(x, y, w, h);
        
    } else {
        // fill entire pixmap with current RGBA values
        FillRect(0, 0, wd, ht);
    }
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoCopy(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);
    
    int x, y, w, h;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %d %d %n%c", &x, &y, &w, &h, &namepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("copy command requires 5 arguments");
    }
    
    if (w <= 0) return OverlayError("copy width must be > 0");
    if (h <= 0) return OverlayError("copy height must be > 0");
    
    if (x < 0 || x+w-1 >= wd || y < 0 || y+h-1 >= ht) {
        return OverlayError("copy rectangle must be within overlay");
    }
    
    std::string name = args + namepos;
    
    // delete any existing clip data with the given name
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it != clips.end()) {
        delete it->second;
        clips.erase(it);
    }
    
    Clip* newclip = new Clip(w, h);
    if (newclip == NULL || newclip->cdata == NULL) {
        delete newclip;
        return OverlayError("not enough memory to copy pixels");
    }
    
    // fill newclip->cdata with pixel data from given rectangle in pixmap
    unsigned char* data = newclip->cdata;
    
    if (x == 0 && y == 0 && w == wd && h == ht) {
        // clip and overlay are the same size so do a fast copy
        memcpy(data, pixmap, w * h * 4);
    
    } else {
        // use memcpy to copy each row
        int rowbytes = wd * 4;
        int wbytes = w * 4;
        unsigned char* p = pixmap + y*rowbytes + x*4;
        for (int i=0; i<h; i++) {
            memcpy(data, p, wbytes);
            p += rowbytes;
            data += wbytes;
        }
    }
    
    clips[name] = newclip;      // create named clip for later use by DoPaste
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoPaste(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);
    
    int x, y;
    int namepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &x, &y, &namepos, &dummy) != 3) {
        // note that %n is not included in the count
        return OverlayError("paste command requires 3 arguments");
    }
    
    std::string name = args + namepos;
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it == clips.end()) {
        static std::string msg;
        msg = "unknown paste name (";
        msg += name;
        msg += ")";
        return OverlayError(msg.c_str());
    }
    
    Clip* clipptr = it->second;
    int w = clipptr->cwd;
    int h = clipptr->cht;
    
    // do nothing if rect is completely outside overlay
    if (RectOutsideOverlay(x, y, w, h)) return NULL;
    
    if (x == 0 && y == 0 && w == wd && h == ht && !alphablend && identity) {
        // clip and overlay are the same size and there's no alpha blending
        // or transforming so do a fast paste using a single memcpy call
        memcpy(pixmap, clipptr->cdata, w * h * 4);
    
    } else if (RectInsideOverlay(x, y, w, h) && !alphablend && identity) {
        // rect is within overlay and there's no alpha blending or transforming
        // so use memcpy to paste rows of pixels from clip data into pixmap
        unsigned char* data = clipptr->cdata;
        int rowbytes = wd * 4;
        int wbytes = w * 4;
        unsigned char* p = pixmap + y*rowbytes + x*4;
        for (int j = 0; j < h; j++) {
            memcpy(p, data, wbytes);
            p += rowbytes;
            data += wbytes;
        }
    
    } else {
        // save current RGBA values and paste pixel by pixel using DrawPixel,
        // clipping any outside the overlay, and possibly doing alpha blending
        // and an affine transformation
        unsigned char saver = r;
        unsigned char saveg = g;
        unsigned char saveb = b;
        unsigned char savea = a;

        unsigned char* data = clipptr->cdata;
        int datapos = 0;
        if (identity) {
            for (int j = 0; j < h; j++) {
                for (int i = 0; i < w; i++) {
                    r = data[datapos++];
                    g = data[datapos++];
                    b = data[datapos++];
                    a = data[datapos++];
                    if (PixelInOverlay(x, y)) DrawPixel(x, y);
                    x++;
                }
                y++;
                x -= w;
            }
        } else {
            // do an affine transformation
            int x0 = x - (x * axx + y * axy);
            int y0 = y - (x * ayx + y * ayy);
            for (int j = 0; j < h; j++) {
                for (int i = 0; i < w; i++) {
                    r = data[datapos++];
                    g = data[datapos++];
                    b = data[datapos++];
                    a = data[datapos++];
                    int newx = x0 + x * axx + y * axy;
                    int newy = y0 + x * ayx + y * ayy;
                    if (PixelInOverlay(newx, newy)) DrawPixel(newx, newy);
                    x++;
                }
                y++;
                x -= w;
            }
        }
        
        // restore saved RGBA values
        r = saver;
        g = saveg;
        b = saveb;
        a = savea;
    }
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoLoad(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);
    
    int x, y;
    int filepos;
    char dummy;
    if (sscanf(args, " %d %d %n%c", &x, &y, &filepos, &dummy) != 3) {
        // note that %n is not included in the count
        return OverlayError("load command requires 3 arguments");
    }
    
    wxString filepath = wxString(args + filepos, wxConvLocal);
    if (!wxFileExists(filepath)) {
        return OverlayError("given file does not exist");
    }
    
    wxImage image;
    if (!image.LoadFile(filepath)) {
        return OverlayError("failed to load image from given file");
    }
    
    int imgwd = image.GetWidth();
    int imght = image.GetHeight();
    if (RectOutsideOverlay(x, y, imgwd, imght)) {
        // do nothing if image rect is completely outside overlay,
        // but we still return the image dimensions so users can do things
        // like center the image within the overlay
    } else {
        // use alpha data if it exists otherwise try looking for mask
        unsigned char* alphadata = NULL;
        if (image.HasAlpha()) {
            alphadata = image.GetAlpha();
        }
        unsigned char maskr, maskg, maskb;
        bool hasmask = false;
        if (alphadata == NULL) {
            hasmask = image.GetOrFindMaskColour(&maskr, &maskg, &maskb);
        }
        
        // save current RGBA values
        unsigned char saver = r;
        unsigned char saveg = g;
        unsigned char saveb = b;
        unsigned char savea = a;

        unsigned char* rgbdata = image.GetData();
        int rgbpos = 0;
        int alphapos = 0;
        for (int j = 0; j < imght; j++) {
            for (int i = 0; i < imgwd; i++) {
                r = rgbdata[rgbpos++];
                g = rgbdata[rgbpos++];
                b = rgbdata[rgbpos++];
                if (alphadata) {
                    a = alphadata[alphapos++];
                } else if (hasmask && r == maskr && g == maskg && b == maskb) {
                    // transparent pixel
                    a = 0;
                } else {
                    a = 255;
                }
                if (PixelInOverlay(x, y)) DrawPixel(x, y);
                x++;
            }
            y++;
            x -= imgwd;
        }
        
        // restore saved RGBA values
        r = saver;
        g = saveg;
        b = saveb;
        a = savea;
    }
    
    // return image dimensions
    static char result[32];
    sprintf(result, "%d %d", imgwd, imght);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoSave(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);
    
    int x, y, w, h;
    int filepos;
    char dummy;
    if (sscanf(args, " %d %d %d %d %n%c", &x, &y, &w, &h, &filepos, &dummy) != 5) {
        // note that %n is not included in the count
        return OverlayError("save command requires 5 arguments");
    }
    
    if (w <= 0) return OverlayError("save width must be > 0");
    if (h <= 0) return OverlayError("save height must be > 0");
    
    if (x < 0 || x+w-1 >= wd || y < 0 || y+h-1 >= ht) {
        return OverlayError("save rectangle must be within overlay");
    }
    
    wxString filepath = wxString(args + filepos, wxConvLocal);
    wxString ext = filepath.AfterLast('.');
    if (!ext.IsSameAs(wxT("png"),false)) {
        return OverlayError("save file must have a .png extension");
    }
    
    unsigned char* rgbdata = (unsigned char*) malloc(w * h * 3);
    if (rgbdata== NULL) {
        return OverlayError("not enough memory to save RGB data");
    }
    unsigned char* alphadata = (unsigned char*) malloc(w * h);
    if (alphadata == NULL) {
        free(rgbdata);
        return OverlayError("not enough memory to save alpha data");
    }
    
    int rgbpos = 0;
    int alphapos = 0;
    int rowbytes = wd * 4;
    for (int j=y; j<y+h; j++) {
        for (int i=x; i<x+w; i++) {
            // get pixel at i,j
            unsigned char* p = pixmap + j*rowbytes + i*4;
            rgbdata[rgbpos++] = p[0];
            rgbdata[rgbpos++] = p[1];
            rgbdata[rgbpos++] = p[2];
            alphadata[alphapos++] = p[3];
        }
    }
    
    // create image of requested size using the given RGB and alpha data;
    // static_data flag is false so wxImage dtor will free rgbdata and alphadata
    wxImage image(w, h, rgbdata, alphadata, false);
    
    if (!image.SaveFile(filepath)) {
        return OverlayError("failed to save image in given file");
    }
    
    return NULL;
}

// -----------------------------------------------------------------------------

#define PixelsMatch(newpxl,oldr,oldg,oldb,olda) \
    (newpxl[0] == oldr && \
     newpxl[1] == oldg && \
     newpxl[2] == oldb && \
     newpxl[3] == olda)

/* above macro is faster
static bool PixelsMatch(unsigned char* newpxl,
                        // NOTE: passing in "unsigned char* oldpxl" resulted in a compiler bug
                        // on Mac OS 10.6.8, so we did this instead:
                        unsigned char oldr, unsigned char oldg, unsigned char oldb, unsigned char olda)
{
    return newpxl[0] == oldr &&
           newpxl[1] == oldg &&
           newpxl[2] == oldb &&
           newpxl[3] == olda;
}
*/

// -----------------------------------------------------------------------------

const char* Overlay::DoFlood(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int x, y;
    if (sscanf(args, " %d %d", &x, &y) != 2) {
        return OverlayError("flood command requires 2 arguments");
    }
    
    // // check if x,y is outside pixmap
    if (!PixelInOverlay(x, y)) return NULL;

    int rowbytes = wd * 4;
    unsigned char* oldpxl = pixmap + y*rowbytes + x*4;
    unsigned char oldr = oldpxl[0];
    unsigned char oldg = oldpxl[1];
    unsigned char oldb = oldpxl[2];
    unsigned char olda = oldpxl[3];
    
    // do nothing if color of given pixel matches current RGBA values
    if (oldr == r && oldg == g && oldb == b && olda == a) return NULL;

    // do flood fill using fast scanline algorithm
    // (based on code at http://lodev.org/cgtutor/floodfill.html)
    bool slowdraw = alphablend && a < 255;
    int maxy = ht - 1;
    std::vector<int> xcoord;
    std::vector<int> ycoord;
    xcoord.push_back(x);
    ycoord.push_back(y);
    while (!xcoord.empty()) {
        // get pixel coords from end of vectors
        x = xcoord.back();
        y = ycoord.back();
        xcoord.pop_back();
        ycoord.pop_back();
        
        bool above = false;
        bool below = false;
        
        unsigned char* newpxl = pixmap + y*rowbytes + x*4;
        while (x >= 0 && PixelsMatch(newpxl,oldr,oldg,oldb,olda)) {
            x--;
            newpxl -= 4;
        }
        x++;
        newpxl += 4;
        
        while (x < wd && PixelsMatch(newpxl,oldr,oldg,oldb,olda)) {
            if (slowdraw) {
                // pixel is within pixmap
                DrawPixel(x,y);
            } else {
                newpxl[0] = r;
                newpxl[1] = g;
                newpxl[2] = b;
                newpxl[3] = a;
            }
            
            if (y > 0) {
                unsigned char* apxl = newpxl - rowbytes;    // pixel at x, y-1
                
                if (!above && PixelsMatch(apxl,oldr,oldg,oldb,olda)) {
                    xcoord.push_back(x);
                    ycoord.push_back(y-1);
                    above = true;
                } else if (above && !PixelsMatch(apxl,oldr,oldg,oldb,olda)) {
                    above = false;
                }
            }
            
            if (y < maxy) {
                unsigned char* bpxl = newpxl + rowbytes;    // pixel at x, y+1
                
                if (!below && PixelsMatch(bpxl,oldr,oldg,oldb,olda)) {
                    xcoord.push_back(x);
                    ycoord.push_back(y+1);
                    below = true;
                } else if (below && !PixelsMatch(bpxl,oldr,oldg,oldb,olda)) {
                    below = false;
                }
            }
            
            x++;
            newpxl += 4;
        }
    }
    
    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoBlend(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int i;
    if (sscanf(args, " %d", &i) != 1) {
        return OverlayError("blend command requires 1 argument");
    }
    
    if (i < 0 || i > 1) {
        return OverlayError("blend value must be 0 or 1");
    }
    
    int oldblend = alphablend ? 1 : 0;
    alphablend = i > 0;
    
    // return old value
    static char result[2];
    sprintf(result, "%d", oldblend);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoFont(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);
    
    bool samename = false;  // only change font size?
    const char* newname;
    int newsize;
    int namepos;
    char dummy;
    int numargs = sscanf(args, " %d %n%c", &newsize, &namepos, &dummy);
    if (numargs == 1) {
        samename = true;
    } else if (numargs != 2) {
        // note that %n is not included in the count
        return OverlayError("font command requires 1 or 2 arguments");
    }
    
    if (newsize <= 0 || newsize >= 1000) {
        return OverlayError("font size must be > 0 and < 1000");
    }
    
    if (samename) {
        // just change the current font's size
        currfont.SetPointSize(newsize);
    
    } else {
        newname = args + namepos;
        
        // check if given font name is valid
        if (strcmp(newname, "default") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetPointSize(newsize);
            
        } else if (strcmp(newname, "default-bold") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetPointSize(newsize);
            currfont.SetWeight(wxFONTWEIGHT_BOLD);
            
        } else if (strcmp(newname, "default-italic") == 0) {
            currfont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
            currfont.SetPointSize(newsize);
            currfont.SetStyle(wxFONTSTYLE_ITALIC);
            
        } else if (strcmp(newname, "mono") == 0) {
            currfont = wxFont(newsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            
        } else if (strcmp(newname, "mono-bold") == 0) {
            currfont = wxFont(newsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
            
        } else if (strcmp(newname, "mono-italic") == 0) {
            currfont = wxFont(newsize, wxFONTFAMILY_MODERN, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);
            
        } else if (strcmp(newname, "roman") == 0) {
            currfont = wxFont(newsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            
        } else if (strcmp(newname, "roman-bold") == 0) {
            currfont = wxFont(newsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
            
        } else if (strcmp(newname, "roman-italic") == 0) {
            currfont = wxFont(newsize, wxFONTFAMILY_ROMAN, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);
        
        } else {
            return OverlayError("unknown font name");
        }
    }
    
    int oldfontsize = fontsize;
    std::string oldfontname = fontname;
    
    fontsize = newsize;
    if (!samename) fontname = newname;
    
    // return old fontsize and fontname
    char ibuff[16];
    sprintf(ibuff, "%d", oldfontsize);
    static std::string result;
    result = ibuff;
    result += " ";
    result += oldfontname;
    return result.c_str();
}

// -----------------------------------------------------------------------------

const char* Overlay::DoText(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    std::string name;
    int namepos, textpos;
    char dummy;
    if (sscanf(args, " %n%*s %n%c", &namepos, &textpos, &dummy) != 1) {
        // note that %n and %*s are not included in the count
        return OverlayError("text command requires 2 arguments");
    }
    
    name = args + namepos;
    name = name.substr(0, name.find(" "));
    
    wxString textstr = wxString(args + textpos, wxConvLocal);
    
    // draw text into an offscreen bitmap with same size as overlay
    wxBitmap bitmap(wd, ht, 32);
    wxMemoryDC dc;
    dc.SelectObject(bitmap);

    // fill background with white
    wxRect rect(0, 0, wd, ht);
    wxBrush brush(*wxWHITE);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(brush);
    dc.DrawRectangle(rect);
    dc.SetBrush(wxNullBrush);
    dc.SetPen(wxNullPen);
    
    dc.SetFont(currfont);
    
    // set text background to white (it will become transparent below)
    // and use alpha to set gray level of text (to be replaced by r,g,b below)
    dc.SetBackgroundMode(wxSOLID);
    dc.SetTextBackground(*wxWHITE);
    dc.SetTextForeground(wxColour(255-a, 255-a, 255-a, 255));
    
    int textwd, textht, descent, leading;
    dc.GetTextExtent(textstr, &textwd, &textht, &descent, &leading);
    dc.DrawText(textstr, 0, 0);
    dc.SelectObject(wxNullBitmap);
    if (textwd > wd) textwd = wd;
    if (textht > ht) textht = ht;

    // delete any existing clip data with the given name
    std::map<std::string,Clip*>::iterator it;
    it = clips.find(name);
    if (it != clips.end()) {
        delete it->second;
        clips.erase(it);
    }
    
    // create clip data with given name and big enough to enclose text
    Clip* textclip = new Clip(textwd, textht);
    if (textclip == NULL || textclip->cdata == NULL) {
        delete textclip;
        return OverlayError("not enough memory for text clip");
    }
    
    // copy text from top left corner of offscreen bitmap into clip data
    unsigned char* m = textclip->cdata;
    int j = 0;
    wxAlphaPixelData data(bitmap);
    if (data) {
        wxAlphaPixelData::Iterator p(data);
        for (int y = 0; y < textht; y++) {
            wxAlphaPixelData::Iterator rowstart = p;
            for (int x = 0; x < textwd; x++) {
                if (p.Red() == 255 && p.Green() == 255 && p.Blue() == 255) {
                    // white background becomes transparent
                    m[j++] = 0;
                    m[j++] = 0;
                    m[j++] = 0;
                    m[j++] = 0;
                } else {
                    // change non-white pixel (black or gray) to current RGB colors
                    m[j++] = r;
                    m[j++] = g;
                    m[j++] = b;
                    // set alpha based on grayness
                    m[j++] = 255 - p.Red();
                }
                p++;
            }
            p = rowstart;
            p.OffsetY(data, 1);
        }
    }
    
    clips[name] = textclip;    // create named clip for later use by DoPaste
    
    // return text info
    static char result[64];
    sprintf(result, "%d %d %d %d", textwd, textht, descent, leading);
    return result;
}

// -----------------------------------------------------------------------------

const char* Overlay::DoTransform(const char* args)
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    int a1, a2, a3, a4;
    if (sscanf(args, " %d %d %d %d", &a1, &a2, &a3, &a4) != 4) {
        return OverlayError("transform command requires 4 arguments");
    }
    
    if (a1 < -1 || a1 > 1 ||
        a2 < -1 || a2 > 1 ||
        a3 < -1 || a3 > 1 ||
        a4 < -1 || a4 > 1) {
        return OverlayError("transform values must be 0, 1 or -1");
    }

    int oldaxx = axx;
    int oldaxy = axy;
    int oldayx = ayx;
    int oldayy = ayy;

    axx = a1;
    axy = a2;
    ayx = a3;
    ayy = a4;
    identity = (axx == 1) && (axy == 0) && (ayx == 0) && (ayy == 1);
    
    // return old values
    static char result[16];
    sprintf(result, "%d %d %d %d", oldaxx, oldaxy, oldayx, oldayy);
    return result;
}

// -----------------------------------------------------------------------------

bool Overlay::OnlyDrawOverlay()
{
    if (pixmap == NULL) return false;

    if (only_draw_overlay) {
        // this flag must only be used for one refresh so reset it immediately
        only_draw_overlay = false;
        return showoverlay && !(numlayers > 1 && tilelayers);
    } else {   
        return false;
    }
}

// -----------------------------------------------------------------------------

const char* Overlay::DoUpdate()
{
    if (pixmap == NULL) return OverlayError(no_overlay);

    only_draw_overlay = true;
    viewptr->Refresh(false);
    viewptr->Update();
    // DrawView in wxrender.cpp will call OnlyDrawOverlay (see above)

    #ifdef __WXGTK__
        // needed on Linux to see update immediately
        insideYield = true;
        wxGetApp().Yield(true);
        insideYield = false;
    #endif

    return NULL;
}

// -----------------------------------------------------------------------------

const char* Overlay::OverlayError(const char* msg)
{
    static std::string err;
    err = "ERR:";
    err += msg;
    return err.c_str();
}

// -----------------------------------------------------------------------------

const char* Overlay::DoOverlayCommand(const char* cmd)
{
    if (strncmp(cmd, "set ", 4) == 0)       return DoSetPixel(cmd+4);
    if (strncmp(cmd, "get ", 4) == 0)       return DoGetPixel(cmd+4);
    if (strcmp(cmd,  "xy") == 0)            return DoGetXY();
    if (strncmp(cmd, "line", 4) == 0)       return DoLine(cmd+4);
    if (strncmp(cmd, "rgba", 4) == 0)       return DoSetRGBA(cmd+4);
    if (strncmp(cmd, "fill", 4) == 0)       return DoFill(cmd+4);
    if (strncmp(cmd, "copy", 4) == 0)       return DoCopy(cmd+4);
    if (strncmp(cmd, "paste", 5) == 0)      return DoPaste(cmd+5);
    if (strncmp(cmd, "load", 4) == 0)       return DoLoad(cmd+4);
    if (strncmp(cmd, "save", 4) == 0)       return DoSave(cmd+4);
    if (strncmp(cmd, "flood", 5) == 0)      return DoFlood(cmd+5);
    if (strncmp(cmd, "blend", 5) == 0)      return DoBlend(cmd+5);
    if (strncmp(cmd, "text", 4) == 0)       return DoText(cmd+4);
    if (strncmp(cmd, "font", 4) == 0)       return DoFont(cmd+4);
    if (strncmp(cmd, "transform", 9) == 0)  return DoTransform(cmd+9);
    if (strncmp(cmd, "position", 8) == 0)   return DoPosition(cmd+8);
    if (strncmp(cmd, "cursor", 6) == 0)     return DoCursor(cmd+6);
    if (strcmp(cmd,  "update") == 0)        return DoUpdate();
    if (strncmp(cmd, "create", 6) == 0)     return DoCreate(cmd+6);
    if (strcmp(cmd,  "delete") == 0) {
        DeleteOverlay();
        return NULL;
    }
    return OverlayError("unknown command");
}
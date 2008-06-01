                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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
/**
 *   This file is where we figure out how to draw jvn structures,
 *   no matter what the magnification or renderer.
 */
#include "ghashbase.h"
#include "util.h"
#include <vector>
#include <cstring>
#include <cstdio>
#include <algorithm>
using namespace std ;

const int logbmsize = 7 ;                 // 6=64x64  7=128x128  8=256x256
const int bmsize = (1<<logbmsize) ;
const int byteoff = (bmsize/8) ;
/* AKT
const int ibufsize = (bmsize*bmsize/32) ;
static unsigned int ibigbuf[ibufsize] ;   // a shared buffer for up to 256x256 pixels
static unsigned char *bigbuf = (unsigned char *)ibigbuf ;
*/
const int rowoff = (bmsize*3) ;           // AKT
const int ibufsize = (bmsize*bmsize*3) ;  // each pixel has 3 r,g,b values
static unsigned char ibigbuf[ibufsize] ;  // a shared buffer for up to 256x256 pixels
static unsigned char *bigbuf = ibigbuf ;

// AKT: made this a method of ghashbase so it can use cellred etc
void ghashbase::drawpixel(int x, int y) {
   int i = (bmsize-1-y) * rowoff + x*3;
   // AKT: for this test we assume all live cells are in state 1
   bigbuf[i]   = cellred[1];
   bigbuf[i+1] = cellgreen[1];
   bigbuf[i+2] = cellblue[1];
}
/*
 *   Draw a 4x4 area yielding 1x1, 2x2, or 4x4 pixels.
 */
void ghashbase::draw4x4_1(unsigned short sw, unsigned short se,
                          unsigned short nw, unsigned short ne,
			  int llx, int lly) {
   int i = (bmsize-1+lly) * rowoff - (llx*3);
   bigbuf[i]   = cellred[sw] ;
   bigbuf[i+1] = cellblue[sw] ;
   bigbuf[i+2] = cellgreen[sw] ;
   i += 3 ;
   bigbuf[i]   = cellred[se] ;
   bigbuf[i+1] = cellblue[se] ;
   bigbuf[i+2] = cellgreen[se] ;
   i += rowoff ;
   bigbuf[i]   = cellred[ne] ;
   bigbuf[i+1] = cellblue[ne] ;
   bigbuf[i+2] = cellgreen[ne] ;
   i -= 3 ;
   bigbuf[i]   = cellred[nw] ;
   bigbuf[i+1] = cellblue[nw] ;
   bigbuf[i+2] = cellgreen[nw] ;
}
void ghashbase::draw4x4_1(ghnode *n, ghnode *z, int llx, int lly) {
   int i = (bmsize-1+lly) * rowoff - (llx*3);
   bigbuf[i]   = 255;//!!!cellred[1];
   bigbuf[i+1] = 255;//!!!cellgreen[1];
   bigbuf[i+2] = 255;//!!!cellblue[1];
}
// AKT: kill all cells in bigbuf
void ghashbase::killpixels() {
   if (pmag > 1) {
      // pixblit assumes bigbuf contains bmsize*bmsize bytes where each byte
      // is a cell state, so it's easy to kill all cells
      memset(bigbuf, 0, bmsize*bmsize*3);
   } else {
      // pixblit assumes bigbuf contains 3 bytes (r,g,b) for each pixel
      if (cellred[0] == cellgreen[0] && cellgreen[0] == cellblue[0]) {
         // use fast method
         memset(bigbuf, cellred[0], sizeof(ibigbuf));
      } else {
         // use slow method
         // or create a single killed row at start of draw() and use memcpy here???
         for (int i = 0; i < ibufsize; i += 3) {
            bigbuf[i]   = cellred[0];
            bigbuf[i+1] = cellgreen[0];
            bigbuf[i+2] = cellblue[0];
         }
      }
   }
}

void ghashbase::clearrect(int minx, int miny, int w, int h) {
   // minx,miny is lower left corner
   if (w <= 0 || h <= 0)
      return ;
   if (pmag > 1) {
      minx *= pmag ;
      miny *= pmag ;
      w *= pmag ;
      h *= pmag ;
   }
   miny = uviewh - miny - h ;
   renderer->killrect(minx, miny, w, h) ;
}

void ghashbase::renderbm(int x, int y) {
   // x,y is lower left corner
   int rx = x ;
   int ry = y ;
   int rw = bmsize ;
   int rh = bmsize ;
   if (pmag > 1) {
      rx *= pmag ;
      ry *= pmag ;
      rw *= pmag ;
      rh *= pmag ;
   }
   ry = uviewh - ry - rh ;
   renderer->pixblit(rx, ry, rw, rh, (char *)bigbuf, pmag);
   killpixels();
}
/*
 *   Here, llx and lly are coordinates in screen pixels describing
 *   where the lower left pixel of the screen is.  Draw one ghnode.
 *   This is our main recursive routine.
 */
void ghashbase::drawghnode(ghnode *n, int llx, int lly, int depth, ghnode *z) {
   int sw = 1 << (depth - mag + 1) ;
   if (sw >= bmsize &&
       (llx + vieww <= 0 || lly + viewh <= 0 || llx >= sw || lly >= sw))
      return ;
   if (n == z) {
      if (sw >= bmsize)
         clearrect(-llx, -lly, sw, sw) ;
   } else if (depth > 0 && sw > 2) {
      z = z->nw ;
      sw >>= 1 ;
      depth-- ;
      if (sw == (bmsize >> 1)) {
         drawghnode(n->sw, 0, 0, depth, z) ;
         drawghnode(n->se, -(bmsize/2), 0, depth, z) ;
         drawghnode(n->nw, 0, -(bmsize/2), depth, z) ;
         drawghnode(n->ne, -(bmsize/2), -(bmsize/2), depth, z) ;
         renderbm(-llx, -lly) ;
      } else {
         drawghnode(n->sw, llx, lly, depth, z) ;
         drawghnode(n->se, llx-sw, lly, depth, z) ;
         drawghnode(n->nw, llx, lly-sw, depth, z) ;
         drawghnode(n->ne, llx-sw, lly-sw, depth, z) ;
      }
   } else if (depth > 0 && sw == 2) {
      draw4x4_1(n, z->nw, llx, lly) ;
   } else if (sw == 1) {
      drawpixel(-llx, -lly) ;
   } else {
      struct ghleaf *l = (struct ghleaf *)n ;
      sw >>= 1 ;
      if (sw == 1) {
         draw4x4_1(l->sw, l->se, l->nw, l->ne, llx, lly) ;
      } else {
	 lifefatal("Can't happen") ;
      }
   }
}
/*
 *   Fill in the llxb and llyb bits from the viewport information.
 *   Allocate if necessary.  This arithmetic should be done carefully.
 */
void ghashbase::fill_ll(int d) {
   pair<bigint, bigint> coor = view->at(0, view->getymax()) ;
   coor.second.mul_smallint(-1) ;
   bigint s = 1 ;
   s <<= d ;
   coor.first += s ;
   coor.second += s ;
   int bitsreq = coor.first.bitsreq() ;
   int bitsreq2 = coor.second.bitsreq() ;
   if (bitsreq2 > bitsreq)
      bitsreq = bitsreq2 ;
   if (bitsreq <= d)
     bitsreq = d + 1 ; // need to access llxyb[d]
   if (bitsreq > llsize) {
      if (llsize) {
         delete [] llxb ;
         delete [] llyb ;
      }
      llxb = new char[bitsreq] ;
      llyb = new char[bitsreq] ;
      llsize = bitsreq ;
   }
   llbits = bitsreq ;
   coor.first.tochararr(llxb, llbits) ;
   coor.second.tochararr(llyb, llbits) ;
}
/*
 *   This is the top-level draw routine that takes the root ghnode.
 *   It maintains four ghnodes onto which the screen fits and uses the
 *   high bits of llx/lly to project those four ghnodes as far down
 *   the tree as possible, so we know we can get away with just
 *   32-bit arithmetic in the above recursive routine.  This way
 *   we don't need any high-precision addition or subtraction to
 *   display an image.
 */
void ghashbase::draw(viewport &viewarg, liferender &rendererarg) {
   /* AKT: call killpixels below
   memset(bigbuf, 0, sizeof(ibigbuf)) ;
   */
   
   ensure_hashed() ;
   renderer = &rendererarg ;
   view = &viewarg ;
   uvieww = view->getwidth() ;
   uviewh = view->getheight() ;
   if (view->getmag() > 0) {
      pmag = 1 << (view->getmag()) ;
      mag = 0 ;
      viewh = ((uviewh - 1) >> view->getmag()) + 1 ;
      vieww = ((uvieww - 1) >> view->getmag()) + 1 ;
      uviewh += (-uviewh) & (pmag - 1) ;
   } else {
      mag = (-view->getmag()) ;
      pmag = 1 ;
      viewh = uviewh ;
      vieww = uvieww ;
   }

   // AKT: must call killpixels after setting pmag
   killpixels();

   int d = depth ;
   fill_ll(d) ;
   int maxd = vieww ;
   int i ;
   ghnode *z = zeroghnode(d) ;
   ghnode *sw = root, *nw = z, *ne = z, *se = z ;
   if (viewh > maxd)
      maxd = viewh ;
   int llx=-llxb[llbits-1], lly=-llyb[llbits-1] ;
/*   Skip down to top of tree. */
   for (i=llbits-1; i>d && i>=mag; i--) { /* go down to d, but not further than mag */
      llx = (llx << 1) + llxb[i] ;
      lly = (lly << 1) + llyb[i] ;
      if (llx > 2*maxd || lly > 2*maxd || llx < -2*maxd || lly < -2*maxd) {
         clearrect(0, 0, vieww, viewh) ;
	 goto bail ;
      }
   }
   /*  Find the lowest four we need to examine */
   while (d > 0 && d - mag >= 0 &&
	  (d - mag > 28 || (1 << (d - mag)) > 2 * maxd)) {
      llx = (llx << 1) + llxb[d] ;
      lly = (lly << 1) + llyb[d] ;
      if (llx >= 1) {
	 if (lly >= 1) {
            ne = ne->sw ;
            nw = nw->se ;
            se = se->nw ;
            sw = sw->ne ;
	    lly-- ;
         } else {
            ne = se->nw ;
            nw = sw->ne ;
            se = se->sw ;
            sw = sw->se ;
         }
	 llx-- ;
      } else {
	 if (lly >= 1) {
            ne = nw->se ;
            nw = nw->sw ;
            se = sw->ne ;
            sw = sw->nw ;
	    lly-- ;
         } else {
            ne = sw->ne ;
            nw = sw->nw ;
            se = sw->se ;
            sw = sw->sw ;
         }
      }
      if (llx > 2*maxd || lly > 2*maxd || llx < -2*maxd || lly < -2*maxd) {
         clearrect(0, 0, vieww, viewh) ;
         goto bail ;
      }
      d-- ;
   }
   /*  At this point we know we can use 32-bit arithmetic. */
   for (i=d; i>=mag; i--) {
      llx = (llx << 1) + llxb[i] ;
      lly = (lly << 1) + llyb[i] ;
   }
   /* clear the border *around* the universe if necessary */
   if (d + 1 <= mag) {
      ghnode *z = zeroghnode(d) ;
      if (llx > 0 || lly > 0 || llx + vieww <= 0 || lly + viewh <= 0 ||
          (sw == z && se == z && nw == z && ne == z)) {
         clearrect(0, 0, vieww, viewh) ;
      } else {
         clearrect(0, 1-lly, vieww, viewh-1+lly) ;
         clearrect(0, 0, vieww, -lly) ;
         clearrect(0, -lly, -llx, 1) ;
         clearrect(1-llx, -lly, vieww-1+llx, 1) ;
         drawpixel(0, 0) ;
         renderbm(-llx, -lly) ;
      }
   } else {
      z = zeroghnode(d) ;
      maxd = 1 << (d - mag + 2) ;
      clearrect(0, maxd-lly, vieww, viewh-maxd+lly) ;
      clearrect(0, 0, vieww, -lly) ;
      clearrect(0, -lly, -llx, maxd) ;
      clearrect(maxd-llx, -lly, vieww-maxd+llx, maxd) ;
      if (maxd <= bmsize) {
         maxd >>= 1 ;
         drawghnode(sw, 0, 0, d, z) ;
         drawghnode(se, -maxd, 0, d, z) ;
         drawghnode(nw, 0, -maxd, d, z) ;
         drawghnode(ne, -maxd, -maxd, d, z) ;
         renderbm(-llx, -lly) ;
      } else {
         maxd >>= 1 ;
         drawghnode(sw, llx, lly, d, z) ;
         drawghnode(se, llx-maxd, lly, d, z) ;
         drawghnode(nw, llx, lly-maxd, d, z) ;
         drawghnode(ne, llx-maxd, lly-maxd, d, z) ;
      }
   }
bail:
   renderer = 0 ;
   view = 0 ;
}
static
int getbitsfromleaves(const vector<ghnode *> &v) {
  unsigned short nw=0, ne=0, sw=0, se=0 ;
  int i;
  for (i=0; i<(int)v.size(); i++) {
    ghleaf *p = (ghleaf *)v[i] ;
    nw |= p->nw ;
    ne |= p->ne ;
    sw |= p->sw ;
    se |= p->se ;
  }
  int r = 0 ;
  // horizontal bits are least significant ones
  unsigned short w = nw | sw ;
  unsigned short e = ne | se ;
  // vertical bits are next 8
  unsigned short n = nw | ne ;
  unsigned short s = sw | se ;
  if (w)
    r |= 512 ;
  if (e)
    r |= 256 ;
  if (n)
    r |= 2 ;
  if (s)
    r |= 1 ;
  return r ;
}

/**
 *   Copy the vector, but sort it and uniquify it so we don't have a ton
 *   of duplicate ghnodes.
 */
static
void sortunique(vector<ghnode *> &dest, vector<ghnode *> &src) {
  swap(src, dest) ; // note:  this is superfast
  sort(dest.begin(), dest.end()) ;
  vector<ghnode *>::iterator new_end = unique(dest.begin(), dest.end()) ;
  dest.erase(new_end, dest.end()) ;
  src.clear() ;
}
using namespace std ;
void ghashbase::findedges(bigint *ptop, bigint *pleft, bigint *pbottom, bigint *pright) {
   // following code is from fit() but all goal/size stuff
   // has been removed so it finds the exact pattern edges
   ensure_hashed() ;
   bigint xmin = -1 ;
   bigint xmax = 1 ;
   bigint ymin = -1 ;
   bigint ymax = 1 ;
   int currdepth = depth ;
   int i;
   if (root == zeroghnode(currdepth)) {
      // return impossible edges to indicate empty pattern;
      // not really a problem because caller should check first
      *ptop = 1 ;
      *pleft = 1 ;
      *pbottom = 0 ;
      *pright = 0 ;
      return ;
   }
   vector<ghnode *> top, left, bottom, right ;
   top.push_back(root) ;
   left.push_back(root) ;
   bottom.push_back(root) ;
   right.push_back(root) ;
   int topbm = 0, bottombm = 0, rightbm = 0, leftbm = 0 ;
   while (currdepth >= -2) {
      currdepth-- ;
      if (currdepth == -1) { // we have ghleaf ghnodes; turn them into bitmasks
         topbm = getbitsfromleaves(top) & 0xff ;
         bottombm = getbitsfromleaves(bottom) & 0xff ;
         leftbm = getbitsfromleaves(left) >> 8 ;
         rightbm = getbitsfromleaves(right) >> 8 ;
      }
      if (currdepth == -1) {
          int sz = 1 << (currdepth + 2) ;
          int maskhi = (1 << sz) - (1 << (sz >> 1)) ;
          int masklo = (1 << (sz >> 1)) - 1 ;
          ymax += ymax ;
          if ((topbm & maskhi) == 0) {
             ymax.add_smallint(-2) ;
          } else {
             topbm >>= (sz >> 1) ;
          }
          ymin += ymin ;
          if ((bottombm & masklo) == 0) {
            ymin.add_smallint(2) ;
            bottombm >>= (sz >> 1) ;
          }
          xmax += xmax ;
          if ((rightbm & masklo) == 0) {
             xmax.add_smallint(-2) ;
             rightbm >>= (sz >> 1) ;
          }
          xmin += xmin ;
          if ((leftbm & maskhi) == 0) {
            xmin.add_smallint(2) ;
          } else {
            leftbm >>= (sz >> 1) ;
          }
      } else if (currdepth >= 0) {
         ghnode *z = 0 ;
         if (hashed)
            z = zeroghnode(currdepth) ;
         vector<ghnode *> newv ;
         int outer = 0 ;
         for (i=0; i<(int)top.size(); i++) {
            ghnode *t = top[i] ;
            if (!outer && (t->nw != z || t->ne != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            } else {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
	 sortunique(top, newv) ;
         ymax += ymax ;
         if (!outer) {
            ymax.add_smallint(-2) ;
         }
         outer = 0 ;
         for (i=0; i<(int)bottom.size(); i++) {
            ghnode *t = bottom[i] ;
            if (!outer && (t->sw != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            }
         }
	 sortunique(bottom, newv) ;
         ymin += ymin ;
         if (!outer) {
            ymin.add_smallint(2) ;
         }
         outer = 0 ;
         for (i=0; i<(int)right.size(); i++) {
            ghnode *t = right[i] ;
            if (!outer && (t->ne != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            }
         }
	 sortunique(right, newv) ;
         xmax += xmax ;
         if (!outer) {
            xmax.add_smallint(-2) ;
         }
         outer = 0 ;
         for (i=0; i<(int)left.size(); i++) {
            ghnode *t = left[i] ;
            if (!outer && (t->nw != z || t->sw != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            } else {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
	 sortunique(left, newv) ;
         xmin += xmin ;
         if (!outer) {
            xmin.add_smallint(2) ;
         }
      }
   }
   xmin >>= 1 ;
   xmax >>= 1 ;
   ymin >>= 1 ;
   ymax >>= 1 ;
   xmin <<= (currdepth + 3) ;
   ymin <<= (currdepth + 3) ;
   xmax <<= (currdepth + 3) ;
   ymax <<= (currdepth + 3) ;
   xmax -= 1 ;
   ymax -= 1 ;
   ymin.mul_smallint(-1) ;
   ymax.mul_smallint(-1) ;
   // set pattern edges
   *ptop = ymax ;          // due to y flip
   *pbottom = ymin ;       // due to y flip
   *pleft = xmin ;
   *pright = xmax ;
}

void ghashbase::fit(viewport &view, int force) {
   ensure_hashed() ;
   bigint xmin = -1 ;
   bigint xmax = 1 ;
   bigint ymin = -1 ;
   bigint ymax = 1 ;
   int xgoal = view.getwidth() - 2 ;
   int ygoal = view.getheight() - 2 ;
   if (xgoal < 8)
      xgoal = 8 ;
   if (ygoal < 8)
      ygoal = 8 ;
   int xsize = 2 ;
   int ysize = 2 ;
   int currdepth = depth ;
   int i;
   if (root == zeroghnode(currdepth)) {
      view.center() ;
      view.setmag(MAX_MAG) ;
      return ;
   }
   vector<ghnode *> top, left, bottom, right ;
   top.push_back(root) ;
   left.push_back(root) ;
   bottom.push_back(root) ;
   right.push_back(root) ;
   int topbm = 0, bottombm = 0, rightbm = 0, leftbm = 0 ;
   while (currdepth >= -2) {
      currdepth-- ;
      if (currdepth == -1) { // we have ghleaf ghnodes; turn them into bitmasks
         topbm = getbitsfromleaves(top) & 0xff ;
	 bottombm = getbitsfromleaves(bottom) & 0xff ;
	 leftbm = getbitsfromleaves(left) >> 8 ;
	 rightbm = getbitsfromleaves(right) >> 8 ;
      }
      if (currdepth == -1) {
	 int sz = 1 << (currdepth + 2) ;
	 int maskhi = (1 << sz) - (1 << (sz >> 1)) ;
	 int masklo = (1 << (sz >> 1)) - 1 ;
         ymax += ymax ;
	 if ((topbm & maskhi) == 0) {
	    ymax.add_smallint(-2) ;
	    ysize-- ;
	 } else {
	    topbm >>= (sz >> 1) ;
	 }
         ymin += ymin ;
	 if ((bottombm & masklo) == 0) {
	   ymin.add_smallint(2) ;
	   ysize-- ;
	   bottombm >>= (sz >> 1) ;
	 }
         xmax += xmax ;
	 if ((rightbm & masklo) == 0) {
	    xmax.add_smallint(-2) ;
	    xsize-- ;
	    rightbm >>= (sz >> 1) ;
	 }
         xmin += xmin ;
	 if ((leftbm & maskhi) == 0) {
	   xmin.add_smallint(2) ;
	   xsize-- ;
	 } else {
	   leftbm >>= (sz >> 1) ;
	 }
         xsize <<= 1 ;
         ysize <<= 1 ;
      } else if (currdepth >= 0) {
         ghnode *z = 0 ;
         if (hashed)
            z = zeroghnode(currdepth) ;
         vector<ghnode *> newv ;
         int outer = 0 ;
         for (i=0; i<(int)top.size(); i++) {
            ghnode *t = top[i] ;
            if (!outer && (t->nw != z || t->ne != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            } else {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
         top = newv ;
         newv.clear() ;
         ymax += ymax ;
         if (!outer) {
            ymax.add_smallint(-2) ;
            ysize-- ;
         }
         outer = 0 ;
         for (i=0; i<(int)bottom.size(); i++) {
            ghnode *t = bottom[i] ;
            if (!outer && (t->sw != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->sw != z)
                  newv.push_back(t->sw) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->ne != z)
                  newv.push_back(t->ne) ;
            }
         }
         bottom = newv ;
         newv.clear() ;
         ymin += ymin ;
         if (!outer) {
            ymin.add_smallint(2) ;
            ysize-- ;
         }
         ysize *= 2 ;
         outer = 0 ;
         for (i=0; i<(int)right.size(); i++) {
            ghnode *t = right[i] ;
            if (!outer && (t->ne != z || t->se != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            } else {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            }
         }
         right = newv ;
         newv.clear() ;
         xmax += xmax ;
         if (!outer) {
            xmax.add_smallint(-2) ;
            xsize-- ;
         }
         outer = 0 ;
         for (i=0; i<(int)left.size(); i++) {
            ghnode *t = left[i] ;
            if (!outer && (t->nw != z || t->sw != z)) {
               newv.clear() ;
               outer = 1 ;
            }
            if (outer) {
               if (t->nw != z)
                  newv.push_back(t->nw) ;
               if (t->sw != z)
                  newv.push_back(t->sw) ;
            } else {
               if (t->ne != z)
                  newv.push_back(t->ne) ;
               if (t->se != z)
                  newv.push_back(t->se) ;
            }
         }
         left = newv ;
         newv.clear() ;
         xmin += xmin ;
         if (!outer) {
            xmin.add_smallint(2) ;
            xsize-- ;
         }
         xsize *= 2 ;
      }
      if (xsize > xgoal || ysize > ygoal)
         break ;
   }
   if (currdepth < 0){
     xmin >>= -currdepth ;
     ymin >>= -currdepth ;
     xmax >>= -currdepth ;
     ymax >>= -currdepth ;
   } else {
     xmin <<= currdepth ;
     ymin <<= currdepth ;
     xmax <<= currdepth ;
     ymax <<= currdepth ;
   }
   xmax -= 1 ;
   ymax -= 1 ;
   ymin.mul_smallint(-1) ;
   ymax.mul_smallint(-1) ;
   if (!force) {
      // if all four of the above dimensions are in the viewport, don't change
      if (view.contains(xmin, ymin) && view.contains(xmax, ymax))
         return ;
   }
   xmin += xmax ;
   xmin >>= 1 ;
   ymin += ymax ;
   ymin >>= 1 ;
   int mag = - currdepth - 1 ;
   while (xsize <= xgoal && ysize <= ygoal && mag < MAX_MAG) {
      mag++ ;
      xsize *= 2 ;
      ysize *= 2 ;
   }
   view.setpositionmag(xmin, ymin, mag) ;
}
void ghashbase::lowerRightPixel(bigint &x, bigint &y, int mag) {
   if (mag >= 0)
     return ;
   x >>= -mag ;
   x <<= -mag ;
   y -= 1 ;
   y >>= -mag ;
   y <<= -mag ;
   y += 1 ;
}

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

#include "prefs.h"      // for showicons
#include "layer.h"      // for currlayer

#import "PatternViewController.h"   // for UpdateEditBar, CloseStatePicker
#import "StateView.h"               // for DrawOneIcon
#import "StatePickerView.h"

@implementation StatePickerView

// -----------------------------------------------------------------------------

- (id)initWithCoder:(NSCoder *)c
{
    self = [super initWithCoder:c];
    if (self) {
        [self setMultipleTouchEnabled:YES];
        
        // add gesture recognizer to this view
        UITapGestureRecognizer *tap1Gesture = [[UITapGestureRecognizer alloc]
                                               initWithTarget:self action:@selector(singleTap:)];
        tap1Gesture.numberOfTapsRequired = 1;
        tap1Gesture.numberOfTouchesRequired = 1;
        tap1Gesture.delegate = self;
        [self addGestureRecognizer:tap1Gesture];
    }
    return self;
}

// -----------------------------------------------------------------------------

- (void)drawRect:(CGRect)rect
{
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGImageRef* iconmaps = currlayer->icons31x31;

    // use white lines
    [[UIColor whiteColor] setStroke];
    CGContextSetLineWidth(context, 1.0);

    // font for drawing state numbers
    UIFont *numfont = [UIFont systemFontOfSize:10];
    
    // draw boxes showing colors or icons of all states
    int x = 0, y = 0;
    int dx, dy;
    for (int i = 0; i < currlayer->algo->NumCellStates(); i++) {
        CGRect box = CGRectMake(x+1, y+1, 31, 31);
        if (showicons && iconmaps && iconmaps[i]) {
            // fill box with background color then draw icon
            CGContextSetFillColorWithColor(context, currlayer->colorref[0]);
            CGContextFillRect(context, box);
            DrawOneIcon(context, x+1, y+1, iconmaps[i],
                        currlayer->cellr[0], currlayer->cellg[0], currlayer->cellb[0],
                        currlayer->cellr[i], currlayer->cellg[i], currlayer->cellb[i]);
        } else {
            // fill box with color of current drawing state
            CGContextSetFillColorWithColor(context, currlayer->colorref[i]);
            CGContextFillRect(context, box);
        }

        // anti-aliased text is much nicer
        CGContextSetShouldAntialias(context, true);

        // show state number in top left corner of box, as black text on white
        NSString *num = [NSString stringWithFormat:@"%d", i];
        CGRect textrect;
        textrect.size = [num sizeWithFont:numfont];
        textrect.origin.x = x+1;
        textrect.origin.y = y+1;
        textrect.size.height -= 3;
        [[UIColor whiteColor] setFill];
        CGContextFillRect(context, textrect);
        textrect.origin.y -= 2;
        [[UIColor blackColor] setFill];
        [num drawInRect:textrect withFont:numfont];
        
        // avoid fuzzy lines
        CGContextSetShouldAntialias(context, false);
        
        // draw lines around box (why do we need to add 1 to y coords???)
        CGContextMoveToPoint(context, x, y+1);
        CGContextAddLineToPoint(context, x+32, y+1);
        CGContextAddLineToPoint(context, x+32, y+33);
        CGContextAddLineToPoint(context, x, y+33);
        CGContextAddLineToPoint(context, x, y+1);
        CGContextStrokePath(context);
        
        if (i == currlayer->drawingstate) {
            // remember location of current drawing state
            dx = x;
            dy = y;
        }
        
        // move to next box
        if ((i+1) % 16 == 0) {
            x = 0;
            y += 32;
        } else {
            x += 32;
        }
    }

    /* no need to do this as current drawing state is shown in edit bar:
    // indicate the current drawing state with a black box
    [[UIColor blackColor] setStroke];
    x = dx;
    y = dy;
    CGContextMoveToPoint(context, x, y+1);
    CGContextAddLineToPoint(context, x+32, y+1);
    CGContextAddLineToPoint(context, x+32, y+33);
    CGContextAddLineToPoint(context, x, y+33);
    CGContextAddLineToPoint(context, x, y+1);
    CGContextStrokePath(context);
    */
}

// -----------------------------------------------------------------------------

- (void)singleTap:(UITapGestureRecognizer *)gestureRecognizer
{
    if ([gestureRecognizer state] == UIGestureRecognizerStateEnded) {
        CGPoint pt = [gestureRecognizer locationInView:self];
        int col = pt.x / 32;
        int row = pt.y / 32;
        int newstate = row * 16 + col;
        if (newstate >= 0 && newstate < currlayer->algo->NumCellStates()) {
            currlayer->drawingstate = newstate;
            UpdateEditBar();
            CloseStatePicker();
        }
    }
}

// -----------------------------------------------------------------------------

@end

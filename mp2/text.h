/*
 * tab:4
 *
 * text.h - font data and text to mode X conversion utility header file
 *
 * "Copyright (c) 2004-2009 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO 
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE 
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE, 
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:        Steve Lumetta
 * Version:       2
 * Creation Date: Thu Sep  9 22:08:16 2004
 * Filename:      text.h
 * History:
 *    SL    1    Thu Sep  9 22:08:16 2004
 *        First written.
 *    SL    2    Sat Sep 12 13:40:11 2009
 *        Integrated original release back into main code base.
 */
#include "modex.h"

#ifndef TEXT_H
#define TEXT_H

/* The default VGA text mode font is 8x16 pixels. */
#define FONT_WIDTH   8
#define FONT_HEIGHT  16

/* Standard VGA text font. */
extern unsigned char font_data[256][16];

/* Add for mp2  */

/* the height of status bar needs to be 2 add the height of the font    */
#define STATUS_BAR_HEIGHT  (sizeof(font_data[0])/sizeof(font_data[0][0]) + 2)

/* size of status bar = height of the bar * width of the image */
#define STATUS_BAR_SIZE    IMAGE_X_DIM * STATUS_BAR_HEIGHT

/* bar_buffer used to store all the color of the status bar */
unsigned char bar_buffer[STATUS_BAR_SIZE];

/* the status bar size of the view of plane */
#define PLANE_BAR_SIZE     STATUS_BAR_SIZE / 4

/* Transform a string into the big font based on the ascii number of each character and the memory font table.  */
void font_transform(const char * text, unsigned char bar_color, unsigned char text_color);

/* Transform the string of fruit text into font array with transparent color  */
void fruit_font_transform(const char* text, unsigned char* fruit_text);
#endif /* TEXT_H */

/*
* ColorizingDMD: Program to edit cROM colorized roms
* Programmed in plain C with Visual Studio 2022 by Zedrummer, 2022
* 
* Linked to the project Visual Pinball Engine for Unity and, as such, is licensed under the GNU General Public License v3.0 https://github.com/freezy/VisualPinball.Engine/blob/master/LICENSE
* 
* Uses OpenGL Immediate Mode for display in 2D in window as this is by far fast enough to make it works at 100+ FPS even on a low end computer with any dedicated GPU
* 
*/

#pragma region Includes

#include "framework.h"
#include "ColorizingDMD.h"
#include <strsafe.h>
#include <gdiplus.h>
using namespace Gdiplus;
#include <CommCtrl.h>
#include "resource.h"
#include <shlwapi.h>
#include "cRom.h"
#include <tchar.h>
#include <direct.h>
#include "OGL_Immediate_2D.h"
#include <windowsx.h>
#include <math.h>
#include <shlobj_core.h>
#include <crtdbg.h>
#include "LiteZip.h"

#pragma endregion Includes

#pragma region Global_Variables

#define MAJ_VERSION 1
#define MIN_VERSION 0

static TCHAR szWindowClass[] = _T("ColorizingDMD");

HINSTANCE hInst;                                // current instance
HWND hWnd = NULL, hwTB = NULL;
UINT ColSetMode = 0, preColRot = 0, acColRot = 0; // 0- displaying colsets, 1- displaying colrots
GLFWwindow * glfwframestripo, * glfwframestripc;	// handle+context of our window
bool fDone = false;

//UINT acFrame = 0, prevFrame = 0;
UINT preframe = 0, nselframes = 1;

cRom_struct MycRom = { "",0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL };
cRP_struct MycRP = { "",{FALSE},{0},0,0,{0},FALSE,0,FALSE };

UINT TxFrameStripo[2] = { (UINT)-1, (UINT)-1 }; // Framebuffer texture for the strip displaying the original frames ID
UINT TxFrameStripc[2] = { (UINT)-1, (UINT)-1 }; // Framebuffer texture for the strip displaying the colorized frames ID
UINT TxChiffres, TxcRom; // Number texture ID
UINT acFSoText = 0; // current texture displayed on the original frame strip
UINT acFScText = 0; // current texture displayed on the colorized frame strip
UINT8 Raw_Digit_Def[RAW_DIGIT_W * RAW_DIGIT_H * 10]; // buffer for raw numbers
UINT8* pFrameoStrip = NULL; // pointer to the memory to draw the original frame strips
UINT8* pFramecStrip = NULL; // pointer to the memory to draw the colorized frame strips
UINT SliderWidth, PosSlider; // Width of the frame strip slider and pos of the slider on the bar

UINT ScrW, ScrH; // client size of the main window
int MonWidth = 1920; // X resolution of the monitor

int PreFrameInStrip = 0; // First frame to display in the frame strip
UINT NFrameToDraw = 0; // how many frames to draw in the strip
UINT FS_LMargin = 0;    // left margin (same as right) before the first frame in the strip

//unsigned int nSelFrames = 1; // number of selected frames
#define MAX_SEL_FRAMES 8192 // Max number of selected frames (has consequences on the undo/redo buffer size!)
//unsigned int SelFrames[MAX_SEL_FRAMES]={0}; // list of selected frames
const UINT8 SelColor[3] = { 100,150,255 };
const UINT8 UnselColor[3] = { 255,50,50 };

DWORD timeLPress = 0, timeRPress = 0, timeUPress = 0, timeDPress = 0; // timer to know how long the key has been pressed
int MouseFrSliderlx; // previous position on the slider
bool MouseFrSliderLPressed = false; // mouse pressed on the frame strip slider

HANDLE hStdout; // for log window

bool UpdateFSneeded = false; // Do we need to update the frame strip
bool UpdateSSneeded = false; // Do we need to update the sprite strip
//bool UpdateFSneededwhenBreleased = false; // Do we need to update the frame strip after the mouse button has been released

#pragma endregion Global_Variables

#pragma region Debug_Tools

void cprintf(const char* format,...) // write to the console
{
    char tbuf[490];
    va_list argptr;
    va_start(argptr, format);
    vsprintf_s(tbuf,490, format, argptr);
    va_end(argptr);
    char tbuf2[512];
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    sprintf_s(tbuf2, 512, "%02d:%02d: %s\n\r", lt.wHour,lt.wMinute, tbuf);
    WriteFile(hStdout, tbuf2, (DWORD)strlen(tbuf2), NULL, NULL);
}

void AffLastError(char* lpszFunction)
{
    // Retrieve the system error message for the last-error code

    char* lpMsgBuf;
    char* lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf,
        0, NULL);

    // Display the error message and exit the process

    lpDisplayBuf = (char*)LocalAlloc(LMEM_ZEROINIT,
        (strlen((LPCSTR)lpMsgBuf) + strlen((LPCSTR)lpszFunction) + 40) * sizeof(char));
    StringCchPrintfA(lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(char),
        "%s failed with error %d: %s",
        lpszFunction, dw, lpMsgBuf);
    cprintf(lpDisplayBuf);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}

#pragma endregion Debug_Tools

#pragma region Memory_Tools

void Del_Buffer_Element(UINT8* pBuf, UINT* pnElt, UINT noElt, UINT Elt_Size)
{
    /* Erase the noElt-th element of the buffer pointed by pBuf.
    * Before the function, the buffer contains *pnElt element and each element is Elt_Size byte long.
    * Shift all the elements after noElt to the left and reduce the buffer size with realloc. *pnElt is decremetend at the end.*/
    if (noElt >= (*pnElt)) return;
    if ((*pnElt) == 1)
    {
        free(pBuf);
        pBuf = NULL;
        (*pnElt) = 0;
        return;
    }
    if (noElt < ((*pnElt) - 1)) memcpy(&pBuf[noElt * Elt_Size], &pBuf[(noElt + 1) * Elt_Size], ((*pnElt) - 1 - noElt) * Elt_Size);
    pBuf = (UINT8*)realloc(pBuf, ((*pnElt) - 1) * Elt_Size);
    (*pnElt)--;
}

bool Add_Buffer_Element(UINT8* pBuf, UINT* pnElt, UINT Elt_Size, UINT8* pSrc)
{
    /* Add an element to the buffer pointed by pBuf.
    * Before the function, the buffer contains *pnElt element and each element is Elt_Size byte long.
    * Increase the buffer size by Elt_Size bytes using realloc then copy the new element from pSrc copiing Elt_Size bytes at the end of the buffer.
    * if pSrc==NULL, fill the memory with 0s
    * *pnElt is incremented at the end*/
    pBuf = (UINT8*)realloc(pBuf, ((*pnElt) + 1) * Elt_Size);
    if (!pBuf)
    {
        cprintf("Unable to increase the buffer size in Add_Buffer_Element");
        return false;
    }
    if (pSrc != NULL) memcpy(&pBuf[(*pnElt) * Elt_Size], pSrc, Elt_Size); else memset(&pBuf[(*pnElt) * Elt_Size], 0, Elt_Size);
    (*pnElt)++;
    return true;
}

#pragma endregion Memory_Tools

UINT32 crc32_table[256];

void build_crc32_table(void) // initiating the CRC table, must be called at startup
{
    for (UINT32 i = 0; i < 256; i++)
    {
        UINT32 ch = i;
        UINT32 crc = 0;
        for (UINT32 j = 0; j < 8; j++)
        {
            UINT32 b = (ch ^ crc) & 1;
            crc >>= 1;
            if (b) crc = crc ^ 0xEDB88320;
            ch >>= 1;
        }
        crc32_table[i] = crc;
    }
}

UINT32 crc32_fast(const UINT8* so, size_t n) // computing buffer CRC32, "build_crc32_table()" must have been called before the first use
{
    UINT32 crc = 0xFFFFFFFF;
    for (size_t i = 0; i < n; i++)
    {
        UINT8 val = so[i];
        crc = (crc >> 8) ^ crc32_table[(val ^ crc) & 0xFF];
    }
    return ~crc;
}

#pragma region Window_Tools_And_Drawings

unsigned char draw_color[4] = { 255,255,255,255 };

void Draw_Rectangle(GLFWwindow* glfwin, int ix, int iy, int fx, int fy)
{
    glfwMakeContextCurrent(glfwin);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBegin(GL_LINE_LOOP);
    glColor4ubv(draw_color);
    glVertex2i(ix, iy);
    glVertex2i(fx, iy);
    glVertex2i(fx, fy);
    glVertex2i(ix, fy);
    glEnd();
}

void Draw_Fill_Rectangle_Text(GLFWwindow* glfwin, int ix, int iy, int fx, int fy, UINT textID, float tx0, float tx1, float ty0, float ty1)
{
    glfwMakeContextCurrent(glfwin);
    glColor4ub(255, 255, 255, 255);
    glBindTexture(GL_TEXTURE_2D, textID);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBegin(GL_TRIANGLES);
    glTexCoord2f(tx0, ty0);
    glVertex2i(ix, iy);
    glTexCoord2f(tx1, ty0);
    glVertex2i(fx, iy);
    glTexCoord2f(tx0, ty1);
    glVertex2i(ix, fy);
    glTexCoord2f(tx0, ty1);
    glVertex2i(ix, fy);
    glTexCoord2f(tx1, ty0);
    glVertex2i(fx, iy);
    glTexCoord2f(tx1, ty1);
    glVertex2i(fx, fy);
    glEnd();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void Display_Avancement(GLFWwindow* glfwin, float avancement)
{
    Draw_Fill_Rectangle_Text(glfwin, 0, 0, (int)(ScrW * avancement), ScrH, TxcRom, 0, avancement, 0, 2.15f);
    gl33_SwapBuffers(glfwin, false);
}

void Draw_Raw_Digit(UINT8 digit, UINT x, UINT y, UINT8* pbuf, UINT width, UINT height)
{
    // Draw a digit in a RGBA memory buffer pbuf at (x,y) dimension of the buffer (width,height)
    UINT8* pdig = &Raw_Digit_Def[digit * RAW_DIGIT_W];
    UINT8* pdig2;
    const UINT dwid = 10 * RAW_DIGIT_W;
    UINT mx = x + RAW_DIGIT_W;
    if (mx > width) mx = width;
    UINT my = y + RAW_DIGIT_H;
    if (my > height) my = height;
    for (UINT tj = y; tj < my; tj++)
    {
        pdig2 = pdig;
        for (UINT ti = x; ti < mx; ti++)
        {
            float lval = (float)(255-(*pdig)) / 255.0f;
            UINT val = (UINT)((1.0f - lval) * pbuf[ti * 4 + tj * 4 * width] + lval * draw_color[0]);
            if (val > 255) pbuf[ti * 4 + tj * 4 * width] = 255; else pbuf[ti * 4 + tj * 4 * width] = val;
            val = (UINT)((1.0f - lval) * pbuf[ti * 4 + tj * 4 * width + 1] + lval * draw_color[1]);
            if (val > 255) pbuf[ti * 4 + tj * 4 * width + 1] = 255; else pbuf[ti * 4 + tj * 4 * width + 1] = val;
            val = (UINT)((1.0f - lval) * pbuf[ti * 4 + tj * 4 * width + 2] + lval * draw_color[2]);
            if (val > 255) pbuf[ti * 4 + tj * 4 * width + 2] = 255; else pbuf[ti * 4 + tj * 4 * width + 2] = val;
            pbuf[ti * 4 + tj * 4 * width + 3] = 255;
            pdig++;
        }
        pdig = pdig2 + dwid;
    }
}

void Draw_Raw_Number(UINT number, UINT x, UINT y, UINT8* pbuf, UINT width, UINT height)
{
    // Draw a number to opengl from a texture giving the 10 digits
    UINT div = 1000000000;
    bool started = false;
    UINT num = number;
    UINT tx = x;
    while (div > 0)
    {
        if (started || (num / div > 0) || (div == 1))
        {
            started = true;
            UINT8 digit = (UINT8)(num / div);
            Draw_Raw_Digit(digit, tx, y, pbuf, width, height);
            num = num - div * digit;
            tx += RAW_DIGIT_W;
        }
        div = div / 10;
    }
}

void Draw_Digit(UINT8 digit, UINT x, UINT y, float zoom)
{
    // Draw a digit to opengl with a texture (to use with Draw_Number)
    glColor4ubv(draw_color);
    glTexCoord2f(digit / 10.0f, 0);
    glVertex2i(x, y);
    glTexCoord2f((digit + 1) / 10.0f, 0);
    glVertex2i(x + (int)(zoom * DIGIT_TEXTURE_W), y);
    glTexCoord2f((digit + 1) / 10.0f, 1);
    glVertex2i(x + (int)(zoom * DIGIT_TEXTURE_W), y + (int)(zoom * DIGIT_TEXTURE_H));
    glTexCoord2f(digit / 10.0f, 1);
    glVertex2i(x, y + (int)(zoom * DIGIT_TEXTURE_H));
    glTexCoord2f(digit / 10.0f, 0);
    glVertex2i(x, y);
    glTexCoord2f((digit + 1) / 10.0f, 1);
    glVertex2i(x + (int)(zoom * DIGIT_TEXTURE_W), y + (int)(zoom * DIGIT_TEXTURE_H));
}

void Draw_Number(UINT number, UINT x, UINT y, float zoom)
{
    // Draw a number to opengl from a texture giving the 10 digits
    UINT div = 1000000000;
    bool started = false;
    UINT num = number;
    UINT tx = x;
    glEnable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, TxChiffres);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    while (div > 0)
    {
        if (started || (num / div > 0) || (div == 1))
        {
            started = true;
            UINT8 digit = (UINT8)(num / div);
            Draw_Digit(digit, tx, y, zoom);
            num = num - div * digit;
            tx += (UINT)(zoom * DIGIT_TEXTURE_W);
        }
        div = div / 10;
    }
    glEnd();
}

void putpixel(int x, int y, UINT8* surface, UINT8 color, bool coloronly, byte* frame)
{
    // set a pixel in a monochrome memory surface or frame
    if ((x < 0) || (x >= (int)MycRom.fWidth) || (y < 0) || (y >= (int)MycRom.fHeight)) return;
    if (coloronly) // do we just mask the non-0 pixels
    {
        if (frame[y * MycRom.fWidth + x] == 0) return;
    }
    surface[y * MycRom.fWidth + x] = color;
}

void putpixel2(int x, int y, UINT8* surface, UINT8 color)
{
    // set a pixel in a monochrome memory surface or frame for sprites
    if ((x < 0) || (x >= (int)MAX_SPRITE_SIZE) || (y < 0) || (y >= (int)MAX_SPRITE_SIZE)) return;
    surface[y * MAX_SPRITE_SIZE + x] = color;
}

void drawline(int x, int y, int x2, int y2, UINT8* surface, UINT8 color, bool coloronly, byte* frame)
{
    int w = x2 - x;
    int h = y2 - y;
    int dx1 = 0, dy1 = 0, dx2 = 0, dy2 = 0;
    if (w < 0) dx1 = -1; else if (w > 0) dx1 = 1;
    if (h < 0) dy1 = -1; else if (h > 0) dy1 = 1;
    if (w < 0) dx2 = -1; else if (w > 0) dx2 = 1;

    int longest = abs(w);
    int shortest = abs(h);

    if (!(longest > shortest))
    {
        longest = abs(h);
        shortest = abs(w);
        if (h < 0) dy2 = -1;
        else if (h > 0) dy2 = 1;
        dx2 = 0;
    }
    int numerator = longest >> 1;
    for (int i = 0; i <= longest; i++)
    {
        putpixel(x, y, surface, color, coloronly, frame);
        numerator += shortest;
        if (!(numerator < longest))
        {
            numerator -= longest;
            x += dx1;
            y += dy1;
        }
        else {
            x += dx2;
            y += dy2;
        }
    }
}

void drawline2(int x, int y, int x2, int y2, UINT8* surface, UINT8 color)
{
    int w = x2 - x;
    int h = y2 - y;
    int dx1 = 0, dy1 = 0, dx2 = 0, dy2 = 0;
    if (w < 0) dx1 = -1; else if (w > 0) dx1 = 1;
    if (h < 0) dy1 = -1; else if (h > 0) dy1 = 1;
    if (w < 0) dx2 = -1; else if (w > 0) dx2 = 1;

    int longest = abs(w);
    int shortest = abs(h);

    if (!(longest > shortest))
    {
        longest = abs(h);
        shortest = abs(w);
        if (h < 0) dy2 = -1;
        else if (h > 0) dy2 = 1;
        dx2 = 0;
    }
    int numerator = longest >> 1;
    for (int i = 0; i <= longest; i++)
    {
        putpixel2(x, y, surface, color);
        numerator += shortest;
        if (!(numerator < longest))
        {
            numerator -= longest;
            x += dx1;
            y += dy1;
        }
        else {
            x += dx2;
            y += dy2;
        }
    }
}

void drawAllOctantsF(int xc, int yc, int xp, int yp, UINT8* surface, UINT8 color, bool coloronly, byte* frame)
{
    for (int x = 0; x <= xp; x++)
    {
        for (int y = 0; y <= yp; y++)
        {
            putpixel(xc + x, yc + y, surface, color, coloronly, frame);
            putpixel(xc - x, yc + y, surface, color, coloronly, frame);
            putpixel(xc + x, yc - y, surface, color, coloronly, frame);
            putpixel(xc - x, yc - y, surface, color, coloronly, frame);
            putpixel(xc + y, yc + x, surface, color, coloronly, frame);
            putpixel(xc - y, yc + x, surface, color, coloronly, frame);
            putpixel(xc + y, yc - x, surface, color, coloronly, frame);
            putpixel(xc - y, yc - x, surface, color, coloronly, frame);
        }
    }
}

void drawAllOctants(int xc, int yc, int x, int y, UINT8* surface, UINT8 color, bool coloronly, byte* frame)
{
    putpixel(xc + x, yc + y, surface, color, coloronly, frame);
    putpixel(xc - x, yc + y, surface, color, coloronly, frame);
    putpixel(xc + x, yc - y, surface, color, coloronly, frame);
    putpixel(xc - x, yc - y, surface, color, coloronly, frame);
    putpixel(xc + y, yc + x, surface, color, coloronly, frame);
    putpixel(xc - y, yc + x, surface, color, coloronly, frame);
    putpixel(xc + y, yc - x, surface, color, coloronly, frame);
    putpixel(xc - y, yc - x, surface, color, coloronly, frame);
}


void drawAllOctantsF2(int xc, int yc, int xp, int yp, UINT8* surface, UINT8 color)
{
    for (int x = 0; x <= xp; x++)
    {
        for (int y = 0; y <= yp; y++)
        {
            putpixel2(xc + x, yc + y, surface, color);
            putpixel2(xc - x, yc + y, surface, color);
            putpixel2(xc + x, yc - y, surface, color);
            putpixel2(xc - x, yc - y, surface, color);
            putpixel2(xc + y, yc + x, surface, color);
            putpixel2(xc - y, yc + x, surface, color);
            putpixel2(xc + y, yc - x, surface, color);
            putpixel2(xc - y, yc - x, surface, color);
        }
    }
}

void drawAllOctants2(int xc, int yc, int x, int y, UINT8* surface, UINT8 color)
{
    putpixel2(xc + x, yc + y, surface, color);
    putpixel2(xc - x, yc + y, surface, color);
    putpixel2(xc + x, yc - y, surface, color);
    putpixel2(xc - x, yc - y, surface, color);
    putpixel2(xc + y, yc + x, surface, color);
    putpixel2(xc - y, yc + x, surface, color);
    putpixel2(xc + y, yc - x, surface, color);
    putpixel2(xc - y, yc - x, surface, color);
}

void drawcircle(int xc, int yc, int r, UINT8* surface, UINT8 color, BOOL filled, bool coloronly, byte* frame)
{
    // draw a circle with the bresenham algorithm in a monochrome memory surface (same size as the frames) or frame
    int x = 0, y = r;
    int d = 3 - 2 * r;
    if (!filled) drawAllOctants(xc, yc, x, y, surface, color, coloronly, frame); else drawAllOctantsF(xc, yc, x, y, surface, color, coloronly, frame);
    while (y >= x)
    {
        x++;
        if (d > 0)
        {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else
            d = d + 4 * x + 6;
        if (!filled) drawAllOctants(xc, yc, x, y, surface, color, coloronly, frame); else drawAllOctantsF(xc, yc, x, y, surface, color, coloronly, frame);
    }
}

void drawcircle2(int xc, int yc, int r, UINT8* surface, UINT8 color, BOOL filled)
{
    // draw a circle with the bresenham algorithm in a monochrome memory surface (same size as the frames) or frame
    int x = 0, y = r;
    int d = 3 - 2 * r;
    if (!filled) drawAllOctants2(xc, yc, x, y, surface, color); else drawAllOctantsF2(xc, yc, x, y, surface, color);
    while (y >= x)
    {
        x++;
        if (d > 0)
        {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else
            d = d + 4 * x + 6;
        if (!filled) drawAllOctants2(xc, yc, x, y, surface, color); else drawAllOctantsF2(xc, yc, x, y, surface, color);
    }
}

void drawrectangle(int x, int y, int x2, int y2, UINT8* surface, UINT8 color, BOOL isfilled, bool coloronly, byte* frame)
{
    for (int tj = min(x, x2); tj <= max(x, x2); tj++)
    {
        for (int ti = min(y, y2); ti <= max(y, y2); ti++)
        {
            if ((tj != x) && (tj != x2) && (ti != y) && (ti != y2) && (!isfilled)) continue;
            if (coloronly) // do we just mask the non-0 pixels
            {
                if (frame[ti * MycRom.fWidth + tj] == 0) continue;
            }
            surface[ti * MycRom.fWidth + tj] = color;
        }
    }
}

void drawrectangle2(int x, int y, int x2, int y2, UINT8* surface, UINT8 color, BOOL isfilled)
{
    // for sprites
    for (int tj = min(x, x2); tj <= max(x, x2); tj++)
    {
        for (int ti = min(y, y2); ti <= max(y, y2); ti++)
        {
            if ((tj != x) && (tj != x2) && (ti != y) && (ti != y2) && (!isfilled)) continue;
            surface[ti * MAX_SPRITE_SIZE + tj] = color;
        }
    }
}

void Draw_Line(float x0, float y0, float x1, float y1)
{
    glVertex2i((int)x0, (int)y0);
    glVertex2i((int)x1, (int)y1);
}

void SetViewport(GLFWwindow* glfwin)
{
    // Set the OpenGL viewport in 2D according the client area of the child window
    int Resx, Resy;
    glfwMakeContextCurrent(glfwin);
    glfwGetFramebufferSize(glfwin, &Resx, &Resy);
    glViewport(0, 0, Resx, Resy);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, Resx, Resy, 0, -2, 2);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Calc_Resize_Frame(void)
{
    // Calculate the global variables depending on the main window dimension
    RECT winrect;
    GetClientRect(hWnd, &winrect);
    ScrW = winrect.right;
    ScrH = winrect.bottom;
    NFrameToDraw = (int)((float)(ScrW - FRAME_STRIP_W_MARGIN) / (float)(256 + FRAME_STRIP_W_MARGIN)); // Calculate the number of frames to display in the strip
    FS_LMargin = (ScrW - (NFrameToDraw * 256 + (NFrameToDraw - 1) * FRAME_STRIP_W_MARGIN)) / 2; // calculate the left and right margin in the strip
    if (MycRom.name[0])
    {
        if (MycRom.fWidth == 192)
        {
            NFrameToDraw = (int)((float)(ScrW - FRAME_STRIP_W_MARGIN) / (float)(192 + FRAME_STRIP_W_MARGIN)); // Calculate the number of frames to display in the strip
            FS_LMargin = (ScrW - (NFrameToDraw * 192 + (NFrameToDraw - 1) * FRAME_STRIP_W_MARGIN)) / 2; // calculate the left and right margin in the strip
        }
    }
    glfwSetWindowSize(glfwframestripo, ScrW, FRAME_STRIP_HEIGHT);
    glfwSetWindowPos(glfwframestripo, 0, ScrH - 2 * FRAME_STRIP_HEIGHT);
    SetViewport(glfwframestripo);
    glfwSetWindowSize(glfwframestripc, ScrW, FRAME_STRIP_HEIGHT);
    glfwSetWindowPos(glfwframestripc, 0, ScrH - FRAME_STRIP_HEIGHT);
    SetViewport(glfwframestripc);
}

void RenderDrawPointClip(GLFWwindow* glfwin, unsigned int x, unsigned int y, unsigned int xmax, unsigned int ymax, unsigned int zoom)
{
    // square out of the clipping zone
    if ((x > xmax) || (y > ymax)) return;
    // square entirely in the clipping zone
    if ((x + zoom - 1 <= xmax) && (y + zoom - 1 <= ymax))
    {
        glColor4ubv(draw_color);
        glTexCoord2f(0, 0);
        glVertex2i(x, y);
        glTexCoord2f(1, 0);
        glVertex2i(x + zoom - 1, y);
        glTexCoord2f(1, 1);
        glVertex2i(x + zoom - 1, y + zoom - 1);
        glTexCoord2f(0, 1);
        glVertex2i(x, y + zoom - 1);
        glTexCoord2f(0, 0);
        glVertex2i(x, y);
        glTexCoord2f(1, 1);
        glVertex2i(x + zoom - 1, y + zoom - 1);
        return;
    }
    // square partially out of the clipping zone
    unsigned int tx = x + zoom - 1;
    unsigned int ty = y + zoom - 1;
    float ttx = 1, tty = 1;
    if (tx > xmax)
    {
        tx = xmax;
        ttx = (float)(xmax - x) / (float)zoom;
    }
    if (ty > ymax)
    {
        ty = ymax;
        tty = (float)(ymax - y) / (float)zoom;
    }
    glColor4ubv(draw_color);
    glTexCoord2f(0, 0);
    glVertex2i(x, y);
    glTexCoord2f(ttx, 0);
    glVertex2i(tx, y);
    glTexCoord2f(ttx, tty);
    glVertex2i(tx, ty);
    glTexCoord2f(0, tty);
    glVertex2i(x, tx);
    glTexCoord2f(0, 0);
    glVertex2i(x, y);
    glTexCoord2f(ttx, tty);
    glVertex2i(tx, ty);
}

void RenderDrawPoint(GLFWwindow* glfwin, unsigned int x, unsigned int y, unsigned int zoom)
{
    glColor4ubv(draw_color);
    glTexCoord2f(0, 0);
    glVertex2i(x, y);
    glTexCoord2f(1, 0);
    glVertex2i(x + zoom - 1, y);
    glTexCoord2f(1, 1);
    glVertex2i(x + zoom - 1, y + zoom - 1);
    glTexCoord2f(0, 1);
    glVertex2i(x, y + zoom - 1);
    glTexCoord2f(0, 0);
    glVertex2i(x, y);
    glTexCoord2f(1, 1);
    glVertex2i(x + zoom - 1, y + zoom - 1);
}

void Draw_Frame_Strip(void)
{
    // Paste the previsouly calculated texture on the client area of the frame strip
    float RTexCoord = (float)ScrW / (float)MonWidth;
    glfwMakeContextCurrent(glfwframestripo);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, TxFrameStripo[acFSoText]);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    glColor4f(1, 1, 1, 1);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f(RTexCoord, 0);
    glVertex2i(ScrW, 0);
    glTexCoord2f(RTexCoord, 1);
    glVertex2i(ScrW, FRAME_STRIP_HEIGHT);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f(RTexCoord, 1);
    glVertex2i(ScrW, FRAME_STRIP_HEIGHT);
    glTexCoord2f(0, 1);
    glVertex2i(0, FRAME_STRIP_HEIGHT);
    glEnd();
    glfwMakeContextCurrent(glfwframestripc);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, TxFrameStripc[acFScText]);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    glColor4f(1, 1, 1, 1);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f(RTexCoord, 0);
    glVertex2i(ScrW, 0);
    glTexCoord2f(RTexCoord, 1);
    glVertex2i(ScrW, FRAME_STRIP_HEIGHT);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f(RTexCoord, 1);
    glVertex2i(ScrW, FRAME_STRIP_HEIGHT);
    glTexCoord2f(0, 1);
    glVertex2i(0, FRAME_STRIP_HEIGHT);
    glEnd();

}

void CalcPosSlider(void)
{
    float div = (float)MycRom.nFrames / (float)SliderWidth;
    PosSlider = (int)((float)PreFrameInStrip / div);
}

bool isFrameSelected(UINT noFr)   // return false if the frame is not selected, the position in the selection list if already selected
{
    if ((noFr >= min(preframe, preframe + nselframes - 1)) && (noFr <= max(preframe, preframe + nselframes - 1))) return true;
    return false;
}

void Get_Frame_Strip_Line_Color(UINT pos)
{
    // give the color of the 2 lines under the slider according to if there are selected frames in this line and similar frames to the current one
    UINT corframe = (int)((float)(pos - FRAME_STRIP_SLIDER_MARGIN) * (float)MycRom.nFrames / (float)SliderWidth);
    UINT cornextframe = (int)((float)(pos - FRAME_STRIP_SLIDER_MARGIN + 1) * (float)MycRom.nFrames / (float)SliderWidth);
    if (cornextframe == corframe) cornextframe++;
    bool issel = false;
    for (UINT ti = corframe; ti < cornextframe; ti++)
    {
        if (isFrameSelected(ti)) issel = true;
    }
    if (issel == true)
    {
        draw_color[0] = SelColor[0];
        draw_color[1] = SelColor[1];
        draw_color[2] = SelColor[2];
    }
    else
    {
        draw_color[0] = UnselColor[0];
        draw_color[1] = UnselColor[1];
        draw_color[2] = UnselColor[2];
    }
}

UINT8 originalcolors[16 * 3];

void SetRenderDrawColor(UINT8 R, UINT8 G, UINT8 B, UINT8 A)
{
    draw_color[0] = R;
    draw_color[1] = G;
    draw_color[2] = B;
    draw_color[3] = A;
}

void SetRenderDrawColorv(UINT8* col3, UINT8 A)
{
    draw_color[0] = col3[0];
    draw_color[1] = col3[1];
    draw_color[2] = col3[2];
    draw_color[3] = A;
}

void Frame_Strip_Updateo(void)
{
    glfwMakeContextCurrent(glfwframestripo);
    // Calculate the texture to display on the frame strip
    if (pFrameoStrip) memset(pFrameoStrip, 0, MonWidth * FRAME_STRIP_HEIGHT * 4);
    // Draw the frames
    if (MycRom.name[0] == 0) return;
    bool doublepixsize = true;
    if (MycRom.fHeight == 64) doublepixsize = false;
    UINT8* pfr, * pcol, * pstrip, * psmem, * psmem2, * pfro;
    UINT addrow = ScrW * 4;
    pstrip = pFrameoStrip + FS_LMargin * 4 + FRAME_STRIP_H_MARGIN * addrow;
    UINT8 frFrameColor[3] = { 255,255,255 };
    int fwid = 256;
    if (MycRom.name[0])
    {
        if (MycRom.fWidth == 192) fwid = 192;
    }
    for (int ti = 0; ti < (int)NFrameToDraw; ti++)
    {
        if ((PreFrameInStrip + ti < 0) || (PreFrameInStrip + ti >= (int)MycRom.nFrames))
        {
            pstrip += (fwid + FRAME_STRIP_W_MARGIN) * 4;
            continue;
        }
        if (isFrameSelected(PreFrameInStrip + ti))
        {
            frFrameColor[0] = SelColor[0];
            frFrameColor[1] = SelColor[1];
            frFrameColor[2] = SelColor[2];
        }
        else
        {
            frFrameColor[0] = UnselColor[0];
            frFrameColor[1] = UnselColor[1];
            frFrameColor[2] = UnselColor[2];
        }
        for (int tj = -1; tj < fwid + 1; tj++)
        {
            *(pstrip + tj * 4 + 64 * addrow) = *(pstrip - addrow + tj * 4) = frFrameColor[0];
            *(pstrip + tj * 4 + 64 * addrow + 1) = *(pstrip - addrow + tj * 4 + 1) = frFrameColor[1];
            *(pstrip + tj * 4 + 64 * addrow + 2) = *(pstrip - addrow + tj * 4 + 2) = frFrameColor[2];
            *(pstrip + tj * 4 + 64 * addrow + 3) = *(pstrip - addrow + tj * 4 + 3) = 255;
        }
        for (int tj = 0; tj < 64; tj++)
        {
            *(pstrip + tj * addrow + fwid * 4) = *(pstrip - 4 + tj * addrow) = frFrameColor[0];
            *(pstrip + tj * addrow + fwid * 4 + 1) = *(pstrip - 4 + tj * addrow + 1) = frFrameColor[1];
            *(pstrip + tj * addrow + fwid * 4 + 2) = *(pstrip - 4 + tj * addrow + 2) = frFrameColor[2];
            *(pstrip + tj * addrow + fwid * 4 + 3) = *(pstrip - 4 + tj * addrow + 3) = 255;
        }
        pfr = &MycRP.oFrames[(PreFrameInStrip + ti) * MycRom.fWidth * MycRom.fHeight];
        pcol = originalcolors;
        pfro = NULL;
        psmem = pstrip;
        for (UINT tj = 0; tj < MycRom.fHeight; tj++)
        {
            psmem2 = pstrip;
            for (UINT tk = 0; tk < MycRom.fWidth; tk++)
            {
                pstrip[0] = pcol[3 * (*pfr)];
                pstrip[1] = pcol[3 * (*pfr) + 1];
                pstrip[2] = pcol[3 * (*pfr) + 2];
                pstrip[3] = 255;
                if (doublepixsize)
                {
                    pstrip[addrow + 4] = pstrip[addrow] = pstrip[4] = pstrip[0];
                    pstrip[addrow + 5] = pstrip[addrow + 1] = pstrip[5] = pstrip[1];
                    pstrip[addrow + 6] = pstrip[addrow + 2] = pstrip[6] = pstrip[2];
                    pstrip[addrow + 7] = pstrip[addrow + 3] = pstrip[7] = 255;
                    pstrip += 4;
                }
                pstrip += 4;
                pfr++;
            }
            pstrip = psmem2 + addrow;
            if (doublepixsize) pstrip += addrow;
        }
        pstrip = psmem + (fwid + FRAME_STRIP_W_MARGIN) * 4;
    }
    // Draw the frame numbers above the frames
    for (UINT ti = 0; ti < NFrameToDraw; ti++)
    {
        if ((PreFrameInStrip + ti < 0) || (PreFrameInStrip + ti >= (int)MycRom.nFrames))
        {
            pstrip += (fwid + FRAME_STRIP_W_MARGIN) * 4;
            continue;
        }
        SetRenderDrawColor(128, 128, 128, 255);
        Draw_Raw_Number(PreFrameInStrip + ti, FS_LMargin + ti * (fwid + FRAME_STRIP_W_MARGIN), FRAME_STRIP_H_MARGIN - RAW_DIGIT_H - 3, pFrameoStrip, ScrW, FRAME_STRIP_H_MARGIN);
        SetRenderDrawColor(255, 255, 0, 255);
        Draw_Raw_Number(MycRP.FrameDuration[PreFrameInStrip + ti], FS_LMargin + ti * (fwid + FRAME_STRIP_W_MARGIN) + fwid - 5 * RAW_DIGIT_W - 1, FRAME_STRIP_H_MARGIN - RAW_DIGIT_H - 3, pFrameoStrip, ScrW, FRAME_STRIP_H_MARGIN);
    }
    SetRenderDrawColor(255, 255, 255, 255);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, TxFrameStripo[!(acFSoText)]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ScrW, FRAME_STRIP_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pFrameoStrip); //RGBA with 4 bytes alignment for efficiency
    acFSoText = !(acFSoText); // equivalent to "x xor 1"
}

void Frame_Strip_Updatec(void)
{
    glfwMakeContextCurrent(glfwframestripc);
    // Calculate the texture to display on the frame strip
    if (pFramecStrip) memset(pFramecStrip, 0, MonWidth * FRAME_STRIP_HEIGHT * 4);
    // Draw the frames
    if (MycRom.name[0] == 0) return;
    bool doublepixsize = true;
    if (MycRom.fHeight == 64) doublepixsize = false;
    UINT8* pfr, * pcol, * pstrip, * psmem, * psmem2, * pfro;
    UINT addrow = ScrW * 4;
    pstrip = pFramecStrip + FS_LMargin * 4 + FRAME_STRIP_H_MARGIN_C * addrow;
    UINT8 frFrameColor[3] = { 255,255,255 };
    int fwid = 256;
    if (MycRom.name[0])
    {
        if (MycRom.fWidth == 192) fwid = 192;
    }
    for (int ti = 0; ti < (int)NFrameToDraw; ti++)
    {
        if ((PreFrameInStrip + ti < 0) || (PreFrameInStrip + ti >= (int)MycRom.nFrames))
        {
            pstrip += (fwid + FRAME_STRIP_W_MARGIN) * 4;
            continue;
        }
        if (isFrameSelected(PreFrameInStrip + ti))
        {
            frFrameColor[0] = SelColor[0];
            frFrameColor[1] = SelColor[1];
            frFrameColor[2] = SelColor[2];
        }
        else
        {
            frFrameColor[0] = UnselColor[0];
            frFrameColor[1] = UnselColor[1];
            frFrameColor[2] = UnselColor[2];
        }
        for (int tj = -1; tj < fwid + 1; tj++)
        {
            *(pstrip + tj * 4 + 64 * addrow) = *(pstrip - addrow + tj * 4) = frFrameColor[0];
            *(pstrip + tj * 4 + 64 * addrow + 1) = *(pstrip - addrow + tj * 4 + 1) = frFrameColor[1];
            *(pstrip + tj * 4 + 64 * addrow + 2) = *(pstrip - addrow + tj * 4 + 2) = frFrameColor[2];
            *(pstrip + tj * 4 + 64 * addrow + 3) = *(pstrip - addrow + tj * 4 + 3) = 255;
        }
        for (int tj = 0; tj < 64; tj++)
        {
            *(pstrip + tj * addrow + fwid * 4) = *(pstrip - 4 + tj * addrow) = frFrameColor[0];
            *(pstrip + tj * addrow + fwid * 4 + 1) = *(pstrip - 4 + tj * addrow + 1) = frFrameColor[1];
            *(pstrip + tj * addrow + fwid * 4 + 2) = *(pstrip - 4 + tj * addrow + 2) = frFrameColor[2];
            *(pstrip + tj * addrow + fwid * 4 + 3) = *(pstrip - 4 + tj * addrow + 3) = 255;
        }
        pfr = &MycRom.cFrames[(PreFrameInStrip + ti) * MycRom.fWidth * MycRom.fHeight];
        pcol = &MycRom.cPal[(PreFrameInStrip + ti) * MycRom.ncColors * 3];
        pfro = &MycRP.oFrames[(PreFrameInStrip + ti) * MycRom.fWidth * MycRom.fHeight];


        psmem = pstrip;
        for (UINT tj = 0; tj < MycRom.fHeight; tj++)
        {
            psmem2 = pstrip;
            for (UINT tk = 0; tk < MycRom.fWidth; tk++)
            {
                pstrip[0] = pcol[3 * (*pfr)];
                pstrip[1] = pcol[3 * (*pfr) + 1];
                pstrip[2] = pcol[3 * (*pfr) + 2];
                pstrip[3] = 255;
                if (doublepixsize)
                {
                    pstrip[addrow + 4] = pstrip[addrow] = pstrip[4] = pstrip[0];
                    pstrip[addrow + 5] = pstrip[addrow + 1] = pstrip[5] = pstrip[1];
                    pstrip[addrow + 6] = pstrip[addrow + 2] = pstrip[6] = pstrip[2];
                    pstrip[addrow + 7] = pstrip[addrow + 3] = pstrip[7] = 255;
                    pstrip += 4;
                }
                pstrip += 4;
                pfr++;
                pfro++;
            }
            pstrip = psmem2 + addrow;
            if (doublepixsize) pstrip += addrow;
        }
        pstrip = psmem + (fwid + FRAME_STRIP_W_MARGIN) * 4;
    }
    // Draw the slider below the frames
    for (UINT ti = addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 - 5); ti < addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 7); ti++)
    {
        pFramecStrip[ti] = 50;
    }
    for (UINT ti = FRAME_STRIP_SLIDER_MARGIN; ti < ScrW - FRAME_STRIP_SLIDER_MARGIN; ti++)
    {
        Get_Frame_Strip_Line_Color(ti);
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 - 1) + 4 * ti] = draw_color[0];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 - 1) + 4 * ti + 1] = draw_color[1];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 - 1) + 4 * ti + 2] = draw_color[2];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 - 1) + 4 * ti + 3] = 255;
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2) + 4 * ti] = draw_color[0];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2) + 4 * ti + 1] = draw_color[1];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2) + 4 * ti + 2] = draw_color[2];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2) + 4 * ti + 3] = 255;
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 1) + 4 * ti] = draw_color[0];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 1) + 4 * ti + 1] = draw_color[1];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 1) + 4 * ti + 2] = draw_color[2];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 1) + 4 * ti + 3] = 255;
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 2) + 4 * ti] = draw_color[0];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 2) + 4 * ti + 1] = draw_color[1];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 2) + 4 * ti + 2] = draw_color[2];
        pFramecStrip[addrow * (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 2) + 4 * ti + 3] = 255;
    }
    SliderWidth = ScrW - 2 * FRAME_STRIP_SLIDER_MARGIN;
    CalcPosSlider();
    for (int ti = -5; ti <= 6; ti++)
    {
        for (int tj = 0; tj <= 2; tj++)
        {
            int offset = (FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + ti) * addrow + (FRAME_STRIP_SLIDER_MARGIN + tj + PosSlider) * 4;
            pFramecStrip[offset] = 255;
            pFramecStrip[offset + 1] = 255;
            pFramecStrip[offset + 2] = 255;
            pFramecStrip[offset + 3] = 255;
        }
    }
    SetRenderDrawColor(255, 255, 255, 255);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, TxFrameStripc[!(acFScText)]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ScrW, FRAME_STRIP_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pFramecStrip); //RGBA with 4 bytes alignment for efficiency
    acFScText = !(acFScText); // equivalent to "x xor 1"
}

#pragma endregion Window_Tools_And_Drawings

#pragma region Project_File_Functions

void Free_cRP(void)
{
    // Free buffers for MycRP
    if (MycRP.name[0] != 0)
    {
        MycRP.name[0] = 0;
        // Nothing to free for now
    }
    if (MycRP.oFrames)
    {
        free(MycRP.oFrames);
        MycRP.oFrames = NULL;
    }
    if (MycRP.FrameDuration)
    {
        free(MycRP.FrameDuration);
        MycRP.FrameDuration = NULL;
    }
}

void Free_cRom(void)
{
    // Free buffers for the MycRom
    if (MycRom.name[0] != 0)
    {
        MycRom.name[0] = 0;
        if (MycRom.HashCode)
        {
            free(MycRom.HashCode);
            MycRom.HashCode = NULL;
        }
        if (MycRom.CompMaskID)
        {
            free(MycRom.CompMaskID);
            MycRom.CompMaskID = NULL;
        }
        if (MycRom.MovRctID)
        {
            free(MycRom.MovRctID);
            MycRom.MovRctID = NULL;
        }
        if (MycRom.CompMasks)
        {
            free(MycRom.CompMasks);
            MycRom.CompMasks = NULL;
        }
        if (MycRom.MovRcts)
        {
            free(MycRom.MovRcts);
            MycRom.MovRcts = NULL;
        }
        if (MycRom.cPal)
        {
            free(MycRom.cPal);
            MycRom.cPal = NULL;
        }
        if (MycRom.cFrames)
        {
            free(MycRom.cFrames);
            MycRom.cFrames = NULL;
        }
        if (MycRom.DynaMasks)
        {
            free(MycRom.DynaMasks);
            MycRom.DynaMasks = NULL;
        }
        if (MycRom.Dyna4Cols)
        {
            free(MycRom.Dyna4Cols);
            MycRom.Dyna4Cols = NULL;
        }
        if (MycRom.FrameSprites)
        {
            free(MycRom.FrameSprites);
            MycRom.FrameSprites = NULL;
        }
        if (MycRom.TriggerID)
        {
            free(MycRom.TriggerID);
            MycRom.TriggerID = NULL;
        }
        if (MycRom.ColorRotations)
        {
            free(MycRom.ColorRotations);
            MycRom.ColorRotations = NULL;
        }
        if (MycRom.SpriteDescriptions)
        {
            free(MycRom.SpriteDescriptions);
            MycRom.SpriteDescriptions = NULL;
        }
        if (MycRom.SpriteDetDwords)
        {
            free(MycRom.SpriteDetDwords);
            MycRom.SpriteDetDwords = NULL;
        }
        if (MycRom.SpriteDetDwordPos)
        {
            free(MycRom.SpriteDetDwordPos);
            MycRom.SpriteDetDwordPos = NULL;
        }
        if (MycRom.SpriteDetAreas)
        {
            free(MycRom.SpriteDetAreas);
            MycRom.SpriteDetAreas = NULL;
        }
    }
}

void Free_Project(void)
{
    Free_cRP();
    Free_cRom();
}

void Delete_Duplicate_Frame(void)
{
    preframe = 0;
    nselframes = 1;

    int* pDelete = (int*)malloc(MycRom.nFrames * sizeof(int));
    if (!pDelete) return;
    UINT32 nDeleted = 0;
    for (UINT32 ti = 0; ti < MycRom.nFrames; ti++) pDelete[ti] = -1;
    for (UINT ti = 0; ti < MycRom.nFrames - 1; ti++)
    {
        if (pDelete[ti] >= 0) continue;
        for (UINT tj = ti + 1; tj < MycRom.nFrames; tj++)
        {
            if (pDelete[tj] >= 0) continue;
            if (MycRom.HashCode[ti] == MycRom.HashCode[tj])
            {
                pDelete[tj] = ti;
                nDeleted++;
            }
        }
    }
    UINT tl = MycRom.nFrames; // n frames remaining, so to copy
    const UINT fsize = MycRom.fWidth * MycRom.fHeight;
    for (UINT ti = 0; ti < tl; ti++)
    {
        if (pDelete[ti] >= 0)
        {
            UINT tj = tl - ti - 1;
            for (UINT tk = 0; tk < MycRP.nSections; tk++)
            {
                if (MycRP.Section_Firsts[tk] == ti) MycRP.Section_Firsts[tk] = pDelete[ti];
            }
            for (UINT tk = 0; tk < MycRom.nSprites; tk++)
            {
                if (MycRP.Sprite_Col_From_Frame[tk] == ti) MycRP.Sprite_Col_From_Frame[tk] = pDelete[ti];
            }
            memmove(&MycRom.HashCode[ti], &MycRom.HashCode[ti + 1], tj * sizeof(UINT32));
            memmove(&MycRom.CompMaskID[ti], &MycRom.CompMaskID[ti + 1], tj);
            memmove(&MycRom.ShapeCompMode[ti], &MycRom.ShapeCompMode[ti + 1], tj);
            memmove(&MycRom.MovRctID[ti], &MycRom.MovRctID[ti + 1], tj);
            memmove(&MycRom.cPal[ti * 3 * MycRom.ncColors], &MycRom.cPal[(ti + 1) * 3 * MycRom.ncColors], tj * 3 * MycRom.ncColors);
            memmove(&MycRom.cFrames[ti * fsize], &MycRom.cFrames[(ti + 1) * fsize], tj * fsize);
            memmove(&MycRom.DynaMasks[ti * fsize], &MycRom.DynaMasks[(ti + 1) * fsize], tj * fsize);
            memmove(&MycRom.Dyna4Cols[ti * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors], &MycRom.Dyna4Cols[(ti + 1) * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors], tj * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors);
            memmove(&MycRom.FrameSprites[ti * MAX_SPRITES_PER_FRAME], &MycRom.FrameSprites[(ti + 1) * MAX_SPRITES_PER_FRAME], tj * MAX_SPRITES_PER_FRAME);
            memmove(&MycRom.ColorRotations[3 * MAX_COLOR_ROTATION * ti], &MycRom.ColorRotations[3 * MAX_COLOR_ROTATION * (ti + 1)], tj);
            memmove(&MycRom.TriggerID[ti], &MycRom.TriggerID[ti + 1], tj * sizeof(UINT32));
            memmove(&MycRP.oFrames[ti * fsize], &MycRP.oFrames[(ti + 1) * fsize], tj * fsize);
            memmove(&MycRP.FrameDuration[ti], &MycRP.FrameDuration[ti + 1], tj * sizeof(UINT32));
            memmove(&pDelete[ti], &pDelete[ti + 1], tj * sizeof(int));
            tl--;
            ti--; // to retest the same rank as it is now the previously next rank
        }
    }
    free(pDelete);
    MycRom.nFrames = tl;
    MycRom.HashCode = (UINT32*)realloc(MycRom.HashCode, tl * sizeof(UINT32));
    MycRom.CompMaskID = (UINT8*)realloc(MycRom.CompMaskID, tl);
    MycRom.ShapeCompMode = (UINT8*)realloc(MycRom.ShapeCompMode, tl);
    MycRom.MovRctID = (UINT8*)realloc(MycRom.MovRctID, tl);
    MycRom.cPal = (UINT8*)realloc(MycRom.cPal, tl * 3 * MycRom.ncColors);
    MycRom.cFrames = (UINT8*)realloc(MycRom.cFrames, tl * fsize);
    MycRom.DynaMasks = (UINT8*)realloc(MycRom.DynaMasks, tl * fsize);
    MycRom.Dyna4Cols = (UINT8*)realloc(MycRom.Dyna4Cols, tl * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors);
    MycRom.FrameSprites = (UINT8*)realloc(MycRom.FrameSprites, tl * MAX_SPRITES_PER_FRAME);
    MycRom.ColorRotations = (UINT8*)realloc(MycRom.ColorRotations, tl * 3 * MAX_COLOR_ROTATION);
    MycRom.TriggerID = (UINT32*)realloc(MycRom.TriggerID, tl * sizeof(UINT32));
    MycRP.oFrames = (UINT8*)realloc(MycRP.oFrames, tl * fsize);
    MycRP.FrameDuration = (UINT32*)realloc(MycRP.FrameDuration, tl * sizeof(UINT32));
    UpdateFSneeded = true;
}

bool Save_cRom(char* path)
{
    if (MycRom.name[0] == 0) return true;
    UINT8* pactiveframes = (UINT8*)malloc(MycRom.nFrames);
    if (!pactiveframes)
    {
        cprintf("Can't get memory for active frames. Action canceled");
        return false;
    }
    for (UINT32 ti = 0; ti < MycRom.nFrames; ti++) MycRom.HashCode[ti] = 0;
    // Save the cRom file
    char tbuf[MAX_PATH];
    sprintf_s(tbuf, MAX_PATH, "%s%s.cROM", path, MycRom.name);
    FILE* pfile;
    if (fopen_s(&pfile, tbuf, "wb") != 0)
    {
        MessageBoxA(hWnd, "Impossible to access the destination directory for cRom, check that it is existing", "Error", MB_OK);
        free(pactiveframes);
        return false;
    }
    fwrite(MycRom.name, 1, 64, pfile);
    UINT lengthheader = 11 * sizeof(UINT);
    fwrite(&lengthheader, sizeof(UINT), 1, pfile);
    fwrite(&MycRom.fWidth, sizeof(UINT), 1, pfile);
    fwrite(&MycRom.fHeight, sizeof(UINT), 1, pfile);
    fwrite(&MycRom.nFrames, sizeof(UINT), 1, pfile);
    fwrite(&MycRom.noColors, sizeof(UINT), 1, pfile);
    fwrite(&MycRom.ncColors, sizeof(UINT), 1, pfile);
    MycRom.nCompMasks = 0;
    fwrite(&MycRom.nCompMasks, sizeof(UINT), 1, pfile);
    MycRom.nMovMasks = 0;
    fwrite(&MycRom.nMovMasks, sizeof(UINT), 1, pfile);
    MycRom.nSprites = 0;
    fwrite(&MycRom.nSprites, sizeof(UINT), 1, pfile);
    fwrite(MycRom.HashCode, sizeof(UINT), MycRom.nFrames, pfile);
    for (unsigned int ti = 0; ti < MycRom.nFrames; ti++)
    {
        MycRom.ShapeCompMode[ti] = 0;
        MycRom.CompMaskID[ti] = 255;
        MycRom.MovRctID[ti] = 255;
        MycRom.TriggerID[ti] = 0xFFFFFFFF;
        for (unsigned int tj = 0; tj < MycRom.fWidth * MycRom.fHeight; tj++) MycRom.DynaMasks[ti * MycRom.fWidth * MycRom.fHeight + tj] = 255;
        for (unsigned int tj = 0; tj < MAX_DYNA_SETS_PER_FRAME * MycRom.noColors; tj++) MycRom.Dyna4Cols[ti * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors + tj] = tj % MAX_DYNA_SETS_PER_FRAME;
        for (unsigned int tj = 0; tj < MAX_SPRITES_PER_FRAME; tj++) MycRom.FrameSprites[ti * MAX_SPRITES_PER_FRAME + tj] = 255;
        for (unsigned int tj = 0; tj < 3 * MAX_COLOR_ROTATION; tj++) MycRom.FrameSprites[ti * 3 * MAX_COLOR_ROTATION + tj] = 255;
    }
    fwrite(MycRom.ShapeCompMode, 1, MycRom.nFrames, pfile);
    fwrite(MycRom.CompMaskID, 1, MycRom.nFrames, pfile);
    fwrite(MycRom.MovRctID, 1, MycRom.nFrames, pfile);
    fwrite(MycRom.cPal, 1, MycRom.nFrames * 3 * MycRom.ncColors, pfile);
    fwrite(MycRom.cFrames, 1, MycRom.nFrames * MycRom.fWidth * MycRom.fHeight, pfile);
    fwrite(MycRom.DynaMasks, 1, MycRom.nFrames * MycRom.fWidth * MycRom.fHeight, pfile);
    fwrite(MycRom.Dyna4Cols, 1, MycRom.nFrames * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors, pfile);
    fwrite(MycRom.FrameSprites, 1, MycRom.nFrames * MAX_SPRITES_PER_FRAME, pfile);
    for (UINT ti = 0; ti < MycRom.nFrames; ti++)
    {
        pactiveframes[ti] = 1;
        /*if (MycRP.FrameDuration[ti] > SKIP_FRAME_DURATION) continue;
        continue;
        pactiveframes[ti] = 0;*/
    }
    fwrite(pactiveframes, 1, MycRom.nFrames, pfile);
    free(pactiveframes);
    fwrite(MycRom.ColorRotations, 1, 3 * MAX_COLOR_ROTATION * MycRom.nFrames, pfile);
    fwrite(MycRom.TriggerID, sizeof(UINT32), MycRom.nFrames, pfile);
    fclose(pfile);
    return true;
}

bool Load_cRom(char* name)
{
    FILE* pfile;
    if (fopen_s(&pfile, name, "rb") != 0)
    {
        AffLastError((char*)"Load_cRom:fopen_s");
        return false;
    }
    fread(MycRom.name, 1, 64, pfile);
    UINT lengthheader;
    fread(&lengthheader, sizeof(UINT), 1, pfile);
    fread(&MycRom.fWidth, sizeof(UINT), 1, pfile);
    fread(&MycRom.fHeight, sizeof(UINT), 1, pfile);
    fread(&MycRom.nFrames, sizeof(UINT), 1, pfile);
    fread(&MycRom.noColors, sizeof(UINT), 1, pfile);
    fread(&MycRom.ncColors, sizeof(UINT), 1, pfile);
    fread(&MycRom.nCompMasks, sizeof(UINT), 1, pfile);
    fread(&MycRom.nMovMasks, sizeof(UINT), 1, pfile);
    fread(&MycRom.nSprites, sizeof(UINT), 1, pfile);
    MycRom.HashCode = (UINT*)malloc(sizeof(UINT) * MycRom.nFrames);
    MycRom.ShapeCompMode = (UINT8*)malloc(MycRom.nFrames);
    MycRom.CompMaskID = (UINT8*)malloc(MycRom.nFrames);
    MycRom.MovRctID = (UINT8*)malloc(MycRom.nFrames);
    MycRom.CompMasks = (UINT8*)malloc(MAX_MASKS * MycRom.fWidth * MycRom.fHeight);
    MycRom.MovRcts = (UINT8*)malloc(MycRom.nMovMasks * 4);
    MycRom.cPal = (UINT8*)malloc(MycRom.nFrames * 3 * MycRom.ncColors);
    MycRom.cFrames = (UINT8*)malloc(MycRom.nFrames * MycRom.fWidth * MycRom.fHeight);
    MycRom.DynaMasks = (UINT8*)malloc(MycRom.nFrames * MycRom.fWidth * MycRom.fHeight);
    MycRom.Dyna4Cols = (UINT8*)malloc(MycRom.nFrames * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors);
    MycRom.FrameSprites = (UINT8*)malloc(MycRom.nFrames * MAX_SPRITES_PER_FRAME);
    MycRom.SpriteDescriptions = (UINT16*)malloc(MycRom.nSprites * MAX_SPRITE_SIZE * MAX_SPRITE_SIZE * sizeof(UINT16));
    MycRom.ColorRotations = (UINT8*)malloc(MycRom.nFrames * 3 * MAX_COLOR_ROTATION);
    MycRom.SpriteDetDwords = (UINT32*)malloc(MycRom.nSprites * sizeof(UINT32) * MAX_SPRITE_DETECT_AREAS);
    MycRom.SpriteDetDwordPos = (UINT16*)malloc(MycRom.nSprites * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS);
    MycRom.SpriteDetAreas = (UINT16*)malloc(MycRom.nSprites * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS * 4);
    MycRom.TriggerID = (UINT32*)malloc(MycRom.nFrames * sizeof(UINT32));
    if ((!MycRom.ShapeCompMode) || (!MycRom.HashCode) || (!MycRom.CompMaskID) || (!MycRom.MovRctID) || (!MycRom.CompMasks) ||
        (!MycRom.MovRcts) || (!MycRom.cPal) || (!MycRom.cFrames) || (!MycRom.DynaMasks) || (!MycRom.Dyna4Cols) || (!MycRom.FrameSprites) ||
        (!MycRom.SpriteDescriptions) ||
        (!MycRom.ColorRotations) || (!MycRom.SpriteDetDwords) || (!MycRom.SpriteDetDwordPos) || (!MycRom.SpriteDetAreas) || (!MycRom.TriggerID))
    {
        cprintf("Can't get the buffers in Load_cRom");
        Free_cRom(); // We free the buffers we got
        fclose(pfile);
        return false;
    }
    memset(MycRom.CompMasks, 0, MAX_MASKS * MycRom.fWidth * MycRom.fHeight);
    fread(MycRom.HashCode, sizeof(UINT), MycRom.nFrames, pfile);
    fread(MycRom.ShapeCompMode, 1, MycRom.nFrames, pfile);
    fread(MycRom.CompMaskID, 1, MycRom.nFrames, pfile);
    fread(MycRom.MovRctID, 1, MycRom.nFrames, pfile);
    if (MycRom.nCompMasks) fread(MycRom.CompMasks, 1, MycRom.nCompMasks * MycRom.fWidth * MycRom.fHeight, pfile);
    if (MycRom.nMovMasks) fread(MycRom.MovRcts, 1, MycRom.nMovMasks * 4, pfile);
    fread(MycRom.cPal, 1, MycRom.nFrames * 3 * MycRom.ncColors, pfile);
    fread(MycRom.cFrames, 1, MycRom.nFrames * MycRom.fWidth * MycRom.fHeight, pfile);
    fread(MycRom.DynaMasks, 1, MycRom.nFrames * MycRom.fWidth * MycRom.fHeight, pfile);
    fread(MycRom.Dyna4Cols, 1, MycRom.nFrames * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors, pfile);
    fread(MycRom.FrameSprites, 1, MycRom.nFrames * MAX_SPRITES_PER_FRAME, pfile);
    fread(MycRom.SpriteDescriptions, sizeof(UINT16), MycRom.nSprites * MAX_SPRITE_SIZE * MAX_SPRITE_SIZE, pfile);
    fseek(pfile, MycRom.nFrames, SEEK_CUR); // we skip the active frame content
    memset(MycRom.ColorRotations, 255, 3 * MAX_COLOR_ROTATION * MycRom.nFrames);
    memset(MycRom.SpriteDetAreas, 255, sizeof(UINT16) * 4 * MAX_SPRITE_DETECT_AREAS * MycRom.nSprites);
    if (lengthheader >= 9 * sizeof(UINT))
    {
        fread(MycRom.ColorRotations, 1, 3 * MAX_COLOR_ROTATION * MycRom.nFrames, pfile);
        if (lengthheader >= 10 * sizeof(UINT))
        {
            fread(MycRom.SpriteDetDwords, sizeof(UINT32), MycRom.nSprites * MAX_SPRITE_DETECT_AREAS, pfile);
            fread(MycRom.SpriteDetDwordPos, sizeof(UINT16), MycRom.nSprites * MAX_SPRITE_DETECT_AREAS, pfile);
            fread(MycRom.SpriteDetAreas, sizeof(UINT16), MycRom.nSprites * 4 * MAX_SPRITE_DETECT_AREAS, pfile);
            if (lengthheader >= 11 * sizeof(UINT))
            {
                fread(MycRom.TriggerID, sizeof(UINT32), MycRom.nFrames, pfile);
            }
            else memset(MycRom.TriggerID, 0xff, sizeof(UINT32) * MycRom.nFrames);
        }
    }
    fclose(pfile);
    return true;
}

/*bool Add_cRom(char* name)
{
    FILE* pfile,* pfile2;
    if (fopen_s(&pfile, name, "rb") != 0)
    {
        AffLastError((char*)"Add_cRom:fopen_s");
        return false;
    }
    char tbuf[MAX_PATH];
    strcpy_s(tbuf, MAX_PATH, name);
    tbuf[strlen(tbuf) - 2] = 'P';
    tbuf[strlen(tbuf) - 1] = 0;
    if (fopen_s(&pfile2, tbuf, "rb") != 0)
    {
        fclose(pfile);
        AffLastError((char*)"Add_cRP:fopen_s");
        return false;
    }

    fread(tbuf, 1, 64, pfile);
    if (strcmp(tbuf, MycRom.name) != 0)
    {
        MessageBoxA(hWnd, "The rom name is different, this file can't be added", "Error", MB_OK);
        fclose(pfile);
        return false;
    }
    UINT lengthheader,tvar,tnframes,tnmasks,tnmmasks,tnsprites;
    fread(&lengthheader, sizeof(UINT), 1, pfile);
    fread(&tvar, sizeof(UINT), 1, pfile);
    if (tvar != MycRom.fWidth)
    {
        MessageBoxA(hWnd, "The rom width is different, this file can't be added", "Error", MB_OK);
        fclose(pfile);
        return false;
    }
    fread(&tvar, sizeof(UINT), 1, pfile);
    if (tvar != MycRom.fHeight)
    {
        MessageBoxA(hWnd, "The rom height is different, this file can't be added", "Error", MB_OK);
        fclose(pfile);
        return false;
    }
    fread(&tnframes, sizeof(UINT), 1, pfile);
    fread(&tvar, sizeof(UINT), 1, pfile);
    fread(&tvar, sizeof(UINT), 1, pfile);
    fread(&tnmasks, sizeof(UINT), 1, pfile);
    fread(&tnmmasks, sizeof(UINT), 1, pfile);
    fread(&tnsprites, sizeof(UINT), 1, pfile);
    MycRom.HashCode = (UINT*)realloc(MycRom.HashCode, sizeof(UINT) * (MycRom.nFrames + tnframes));
    MycRom.ShapeCompMode = (UINT8*)realloc(MycRom.ShapeCompMode, MycRom.nFrames + MycRom.nFrames);
    MycRom.CompMaskID = (UINT8*)realloc(MycRom.CompMaskID, MycRom.nFrames + MycRom.nFrames);
    MycRom.MovRctID = (UINT8*)realloc(MycRom.MovRctID, MycRom.nFrames + MycRom.nFrames);
    UINT8* tCompMasks=(UINT8*)malloc(MAX_MASKS * MycRom.fWidth * MycRom.fHeight);
    if (tnmmasks>0) UINT8* tCompMasks = (UINT8*)malloc(tnmmasks * 4);
    MycRom.cPal = (UINT8*)realloc(MycRom.cPal, (MycRom.nFrames + tnframes) * 3 * MycRom.ncColors);
    MycRom.cFrames = (UINT8*)realloc(MycRom.cFrames, (MycRom.nFrames + tnframes) * MycRom.fWidth * MycRom.fHeight);
    MycRom.DynaMasks = (UINT8*)realloc(MycRom.DynaMasks, (MycRom.nFrames + tnframes) * MycRom.fWidth * MycRom.fHeight);
    MycRom.Dyna4Cols = (UINT8*)realloc(MycRom.Dyna4Cols, (MycRom.nFrames + tnframes) * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors);
    MycRom.FrameSprites = (UINT8*)realloc(MycRom.FrameSprites, (MycRom.nFrames + tnframes) * MAX_SPRITES_PER_FRAME);
    MycRom.SpriteDescriptions = (UINT16*)realloc(MycRom.SpriteDescriptions, (tnsprites + MycRom.nSprites) * MAX_SPRITE_SIZE * MAX_SPRITE_SIZE * sizeof(UINT16));
    MycRom.ColorRotations = (UINT8*)realloc(MycRom.ColorRotations, (MycRom.nFrames + tnframes) * 3 * MAX_COLOR_ROTATION);
    MycRom.SpriteDetDwords = (UINT32*)realloc(MycRom.SpriteDetDwords, (tnsprites + MycRom.nSprites) * sizeof(UINT32) * MAX_SPRITE_DETECT_AREAS);
    MycRom.SpriteDetDwordPos = (UINT16*)realloc(MycRom.SpriteDetDwordPos, (tnsprites + MycRom.nSprites) * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS);
    MycRom.SpriteDetAreas = (UINT16*)realloc(MycRom.SpriteDetAreas, (tnsprites + MycRom.nSprites) * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS * 4);
    MycRom.TriggerID = (UINT32*)realloc(MycRom.TriggerID, (MycRom.nFrames + tnframes) * sizeof(UINT32));
    if ((!MycRom.ShapeCompMode) || (!MycRom.HashCode) || (!MycRom.CompMaskID) || (!MycRom.MovRctID) || (!tCompMasks) ||
        (!MycRom.MovRcts) || (!MycRom.cPal) || (!MycRom.cFrames) || (!MycRom.DynaMasks) || (!MycRom.Dyna4Cols) || (!MycRom.FrameSprites) ||
        (!MycRom.SpriteDescriptions) ||
        (!MycRom.ColorRotations) || (!MycRom.SpriteDetDwords) || (!MycRom.SpriteDetDwordPos) || (!MycRom.SpriteDetAreas) || (!MycRom.TriggerID))
    {
        cprintf("Can't get the buffers in Load_cRom");
        Free_cRom(); // We free the buffers we got
        fclose(pfile);
        return false;
    }
    fread(&MycRom.HashCode[MycRom.nFrames], sizeof(UINT), MycRom.nFrames, pfile);
    fread(&MycRom.ShapeCompMode[MycRom.nFrames], 1, MycRom.nFrames, pfile);
    fread(&MycRom.CompMaskID[MycRom.nFrames], 1, MycRom.nFrames, pfile);
    fread(&MycRom.MovRctID[MycRom.nFrames], 1, MycRom.nFrames, pfile);
    if (tnmasks) fread(tCompMasks, 1, tnmasks * MycRom.fWidth * MycRom.fHeight, pfile);
    if (tnmmasks) fread(&MycRom.MovRcts[MycRom.nMovMasks * 4], 1, tnmmasks * 4, pfile);
    fread(&MycRom.cPal[MycRom.nFrames * 3 * MycRom.ncColors], 1, tnframes * 3 * MycRom.ncColors, pfile);
    fread(&MycRom.cFrames[MycRom.nFrames * MycRom.fWidth * MycRom.fHeight], 1, tnframes * MycRom.fWidth * MycRom.fHeight, pfile);
    fread(&MycRom.DynaMasks[MycRom.nFrames * MycRom.fWidth * MycRom.fHeight], 1, tnframes * MycRom.fWidth * MycRom.fHeight, pfile);
    fread(&MycRom.Dyna4Cols[MycRom.nFrames * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors], 1, tnframes * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors, pfile);
    fread(&MycRom.FrameSprites[MycRom.nFrames * MAX_SPRITES_PER_FRAME], 1, tnframes * MAX_SPRITES_PER_FRAME, pfile);
    fread(&MycRom.SpriteDescriptions[MycRom.nSprites * MAX_SPRITE_SIZE * MAX_SPRITE_SIZE], sizeof(UINT16), tnsprites * MAX_SPRITE_SIZE * MAX_SPRITE_SIZE, pfile);
    fseek(pfile, tnframes, SEEK_CUR); // we skip the active frame content
    memset(MycRom.ColorRotations, 255, 3 * MAX_COLOR_ROTATION * MycRom.nFrames);
    memset(MycRom.SpriteDetAreas, 255, sizeof(UINT16) * 4 * MAX_SPRITE_DETECT_AREAS * MycRom.nSprites);
    if (lengthheader >= 9 * sizeof(UINT))
    {
        fread(&MycRom.ColorRotations[3 * MAX_COLOR_ROTATION * MycRom.nFrames], 1, 3 * MAX_COLOR_ROTATION * tnframes, pfile);
        if (lengthheader >= 10 * sizeof(UINT))
        {
            fread(&MycRom.SpriteDetDwords[MycRom.nSprites * MAX_SPRITE_DETECT_AREAS], sizeof(UINT32), tnsprites * MAX_SPRITE_DETECT_AREAS, pfile);
            fread(&MycRom.SpriteDetDwordPos[MycRom.nSprites * MAX_SPRITE_DETECT_AREAS], sizeof(UINT16), tnsprites * MAX_SPRITE_DETECT_AREAS, pfile);
            fread(&MycRom.SpriteDetAreas[MycRom.nSprites * 4 * MAX_SPRITE_DETECT_AREAS], sizeof(UINT16), tnsprites * 4 * MAX_SPRITE_DETECT_AREAS, pfile);
            if (lengthheader >= 11 * sizeof(UINT))
            {
                fread(&MycRom.TriggerID[MycRom.nFrames], sizeof(UINT32), tnframes, pfile);
            }
            else memset(MycRom.TriggerID, 0xff, sizeof(UINT32) * MycRom.nFrames);
        }
    }
    fclose(pfile);
    MycRP.oFrames = (UINT8*)realloc(MycRP.oFrames, (tnframes+ MycRom.nFrames) * MycRom.fWidth * MycRom.fHeight);
    if (!MycRP.oFrames)
    {
        cprintf("Can't get the buffer in Add_cRP");
        Free_cRP(); // We free the buffers we got
        fclose(pfile2);
        return false;
    }
    MycRP.FrameDuration = (UINT32*)realloc(MycRP.FrameDuration,(tnframes+ MycRom.nFrames) * sizeof(UINT32));
    if (!MycRP.FrameDuration)
    {
        cprintf("Can't get the buffer in Add_cRP");
        Free_cRP(); // We free the buffers we got
        fclose(pfile2);
        return false;
    }
    fread(tbuf, 1, 64, pfile2);
    fread(&MycRP.oFrames[MycRom.nFrames * MycRom.fWidth * MycRom.fHeight], 1, (tnframes + MycRom.nFrames)* MycRom.fWidth* MycRom.fHeight, pfile2);
    for (UINT32 ti = MycRom.nFrames; ti < MycRom.nFrames + tnframes; ti++)
        MycRom.HashCode[ti] = crc32_fast(&MycRP.oFrames[ti * MycRom.fWidth * MycRom.fHeight], MycRom.fWidth * MycRom.fHeight);
    BOOL tactivecolset[MAX_COL_SETS];
    fread(tactivecolset, sizeof(BOOL), MAX_COL_SETS, pfile2);
    UINT8 tcolsets[MAX_COL_SETS * 16];
    fread(tcolsets, sizeof(UINT8), MAX_COL_SETS * 16, pfile2);
    UINT8 tnul;
    fread(&tnul, sizeof(UINT8), 1, pfile2);
    fread(&tnul, sizeof(UINT8), 1, pfile2);
    char tnamecolsets[MAX_COL_SETS * 64];
    fread(tnamecolsets, sizeof(char), MAX_COL_SETS * 64, pfile2);
    // We look if the col sets are similar to the ones in the first file and if not to add them
    for (int ti = 0; ti < MAX_COL_SETS; ti++)
    {
        if (tactivecolset[ti] == FALSE) continue;
        int librefound = -1, samefound = -1;
        for (int tj = 0; (tj < MAX_COL_SETS) && (samefound < 0); tj++)
        {
            if (MycRP.activeColSet[tj] == FALSE)
            {
                if (librefound < 0) librefound = tj;
                continue;
            }
            samefound = tj;
            for (UINT tk = 0; tk < MycRom.noColors; tk++)
            {
                if (tcolsets[ti * 16 + tk] != MycRP.ColSets[tj * 16 + tk])
                {
                    samefound = -1;
                    break;
                }
            }
        }
        // if the colset doesn't exist and we have found a free one in the old sets, we copy the new there
        if ((samefound < 0) && (librefound >= 0))
        {
            MycRP.activeColSet[librefound] = TRUE;
            for (UINT tk = 0; tk < MycRom.noColors; tk++) MycRP.ColSets[librefound * 16 + tk] = tcolsets[ti * 16 + tk];
            strcpy_s(&MycRP.nameColSet[librefound * 64], 64, &tnamecolsets[ti * 64]);
        }
    }
    UINT32 tnul32;
    fread(&tnul32, sizeof(UINT32), 1, pfile2);
    fread(&tnul, sizeof(UINT8), 1, pfile2);
    fread(&tnul32, sizeof(int), 1, pfile2);
    fread(&tnul32, sizeof(BOOL), 1, pfile2);
    fread(MycRP.Mask_Names, sizeof(char), MAX_MASKS * SIZE_MASK_NAME, pfile2);
    // we check if there are similar masks and add new ones if any
    for (int ti = 0; ti < tnmasks; ti++)
    {
        UINT8* newmask=&
    }
    fread(&MycRP.nSections, sizeof(UINT32), 1, pfile2);
    fread(MycRP.Section_Firsts, sizeof(UINT32), MAX_SECTIONS, pfile2);
    fread(MycRP.Section_Names, sizeof(char), MAX_SECTIONS * SIZE_SECTION_NAMES, pfile2);
    fread(MycRP.Sprite_Names, sizeof(char), 255 * SIZE_SECTION_NAMES, pfile2);
    fread(MycRP.Sprite_Col_From_Frame, sizeof(UINT32), 255, pfile2);
    for (UINT ti = 0; ti < MycRom.nFrames; ti++) MycRP.FrameDuration[ti] = 0;
    fread(MycRP.FrameDuration, sizeof(UINT32), MycRom.nFrames, pfile2);
    fread(MycRP.Sprite_Edit_Colors, 1, 16 * 255, pfile2);
    fread(MycRP.SaveDir, 1, 260, pfile2);
    memset(MycRP.SpriteRect, 255, sizeof(UINT16) * 4 * 255);
    fread(MycRP.SpriteRect, sizeof(UINT16), 4 * 255, pfile2);
    fread(MycRP.SpriteRectMirror, sizeof(BOOL), 2 * 255, pfile2);
    fclose(pfile2);
    return true;
}*/

bool Save_cRP(char* path)
{
    if (MycRP.name[0] == 0) return true;
    char tbuf[MAX_PATH];
    sprintf_s(tbuf, MAX_PATH, "%s%s.cRP", path, MycRP.name);
    FILE* pfile;
    if (fopen_s(&pfile, tbuf, "wb") != 0)
    {
        MessageBoxA(hWnd, "Impossible to access the destination directory for cRP, check that it is existing", "Error", MB_OK);
        return false;
    }
    fwrite(MycRP.name, 1, 64, pfile);
    fwrite(MycRP.oFrames, 1, MycRom.nFrames * MycRom.fWidth * MycRom.fHeight, pfile);
    fwrite(MycRP.activeColSet, sizeof(BOOL), MAX_COL_SETS, pfile);
    fwrite(MycRP.ColSets, sizeof(UINT8), MAX_COL_SETS * 16, pfile);
    fwrite(&MycRP.acColSet, sizeof(UINT8), 1, pfile);
    fwrite(&MycRP.preColSet, sizeof(UINT8), 1, pfile);
    fwrite(MycRP.nameColSet, sizeof(char), MAX_COL_SETS * 64, pfile);
    fwrite(&MycRP.DrawColMode, sizeof(UINT32), 1, pfile);
    fwrite(&MycRP.Draw_Mode, sizeof(UINT8), 1, pfile);
    fwrite(&MycRP.Mask_Sel_Mode, sizeof(int), 1, pfile);
    fwrite(&MycRP.Fill_Mode, sizeof(BOOL), 1, pfile);
    fwrite(MycRP.Mask_Names, sizeof(char), MAX_MASKS * SIZE_MASK_NAME, pfile);
    fwrite(&MycRP.nSections, sizeof(UINT32), 1, pfile);
    fwrite(MycRP.Section_Firsts, sizeof(UINT32), MAX_SECTIONS, pfile);
    fwrite(MycRP.Section_Names, sizeof(char), MAX_SECTIONS * SIZE_SECTION_NAMES, pfile);
    fwrite(MycRP.Sprite_Names, sizeof(char), 255 * SIZE_SECTION_NAMES, pfile);
    fwrite(MycRP.Sprite_Col_From_Frame, sizeof(UINT32), 255, pfile);
    fwrite(MycRP.FrameDuration, sizeof(UINT32), MycRom.nFrames, pfile);
    fwrite(MycRP.Sprite_Edit_Colors, 1, 16 * 255, pfile);
    fwrite(MycRP.SaveDir, 1, 260, pfile);
    fwrite(MycRP.SpriteRect, sizeof(UINT16), 4 * 255, pfile);
    fwrite(MycRP.SpriteRectMirror, sizeof(BOOL), 2 * 255, pfile);
    fclose(pfile);
    return true;
}

bool Load_cRP(char* name)
{
    // cRP must be loaded after cROM
    Free_cRP();
    //char tbuf[MAX_PATH];
    //sprintf_s(tbuf, MAX_PATH, "%s%s", DumpDir, name);
    FILE* pfile;
    if (fopen_s(&pfile, name, "rb") != 0)
    {
        AffLastError((char*)"Load_cRP:fopen_s");
        return false;
    }
    MycRP.oFrames = (UINT8*)malloc(MycRom.nFrames * MycRom.fWidth * MycRom.fHeight);
    if (!MycRP.oFrames)
    {
        cprintf("Can't get the buffer in Load_cRP");
        Free_cRP(); // We free the buffers we got
        fclose(pfile);
        return false;
    }
    MycRP.FrameDuration = (UINT32*)malloc(MycRom.nFrames * sizeof(UINT32));
    if (!MycRP.FrameDuration)
    {
        cprintf("Can't get the buffer in Load_cRP");
        Free_cRP(); // We free the buffers we got
        fclose(pfile);
        return false;
    }
    fread(MycRP.name, 1, 64, pfile);
    fread(MycRP.oFrames, 1, MycRom.nFrames * MycRom.fWidth * MycRom.fHeight, pfile);
    for (UINT32 ti = 0; ti < MycRom.nFrames; ti++)
        MycRom.HashCode[ti] = crc32_fast(&MycRP.oFrames[ti * MycRom.fWidth * MycRom.fHeight], MycRom.fWidth * MycRom.fHeight);
    fread(MycRP.activeColSet, sizeof(BOOL), MAX_COL_SETS, pfile);
    fread(MycRP.ColSets, sizeof(UINT8), MAX_COL_SETS * 16, pfile);
    fread(&MycRP.acColSet, sizeof(UINT8), 1, pfile);
    fread(&MycRP.preColSet, sizeof(UINT8), 1, pfile);
    fread(MycRP.nameColSet, sizeof(char), MAX_COL_SETS * 64, pfile);
    fread(&MycRP.DrawColMode, sizeof(UINT32), 1, pfile);
    fread(&MycRP.Draw_Mode, sizeof(UINT8), 1, pfile);
    fread(&MycRP.Mask_Sel_Mode, sizeof(int), 1, pfile);
    fread(&MycRP.Fill_Mode, sizeof(BOOL), 1, pfile);
    fread(MycRP.Mask_Names, sizeof(char), MAX_MASKS * SIZE_MASK_NAME, pfile);
    fread(&MycRP.nSections, sizeof(UINT32), 1, pfile);
    fread(MycRP.Section_Firsts, sizeof(UINT32), MAX_SECTIONS, pfile);
    fread(MycRP.Section_Names, sizeof(char), MAX_SECTIONS * SIZE_SECTION_NAMES, pfile);
    fread(MycRP.Sprite_Names, sizeof(char), 255 * SIZE_SECTION_NAMES, pfile);
    fread(MycRP.Sprite_Col_From_Frame, sizeof(UINT32), 255, pfile);
    for (UINT ti = 0; ti < MycRom.nFrames; ti++) MycRP.FrameDuration[ti] = 0;
    fread(MycRP.FrameDuration, sizeof(UINT32), MycRom.nFrames, pfile);
    fread(MycRP.Sprite_Edit_Colors, 1, 16 * 255, pfile);
    fread(MycRP.SaveDir, 1, 260, pfile);
    memset(MycRP.SpriteRect, 255, sizeof(UINT16) * 4 * 255);
    fread(MycRP.SpriteRect, sizeof(UINT16), 4 * 255, pfile);
    fread(MycRP.SpriteRectMirror, sizeof(BOOL), 2 * 255, pfile);
    fclose(pfile);
    return true;
}

bool Load_cDump(char* path)// , char* rom)
{
    /*char tbuf[MAX_PATH];
    sprintf_s(tbuf, MAX_PATH, "%s%s.cdump", path, rom);*/
#pragma warning(suppress : 4996)
    FILE* pf = fopen(path, "rb");
    if (pf == 0)
    {
        MessageBoxA(hWnd, "Can't open the file", "Error", MB_OK);
        return false;
    }
    Free_cRom();
    Free_cRP();
    // Initiate cROM
    MycRom.ncColors = 64;
    fread(&MycRom.nFrames, sizeof(UINT32), 1, pf);
    fread(MycRom.name, 1, 64, pf);
    fread(&MycRom.fWidth, sizeof(UINT32), 1, pf);
    fread(&MycRom.fHeight, sizeof(UINT32), 1, pf);
    fread(&MycRom.noColors, sizeof(UINT32), 1, pf);
    MycRom.ncColors = 64;
    MycRom.nCompMasks = 0;
    MycRom.nMovMasks = 0;
    MycRom.nSprites = 0;
    MycRom.HashCode = (UINT*)malloc(sizeof(UINT) * MycRom.nFrames);
    MycRom.ShapeCompMode = (UINT8*)malloc(MycRom.nFrames);
    MycRom.CompMaskID = (UINT8*)malloc(MycRom.nFrames);
    MycRom.MovRctID = (UINT8*)malloc(MycRom.nFrames);
    MycRom.CompMasks = (UINT8*)malloc(MAX_MASKS * MycRom.fWidth * MycRom.fHeight);
    MycRom.MovRcts = (UINT8*)malloc(MycRom.nMovMasks * 4);
    MycRom.cPal = (UINT8*)malloc(MycRom.nFrames * 3 * MycRom.ncColors);
    MycRom.cFrames = (UINT8*)malloc(MycRom.nFrames * MycRom.fWidth * MycRom.fHeight);
    MycRom.DynaMasks = (UINT8*)malloc(MycRom.nFrames * MycRom.fWidth * MycRom.fHeight);
    MycRom.Dyna4Cols = (UINT8*)malloc(MycRom.nFrames * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors);
    MycRom.FrameSprites = (UINT8*)malloc(MycRom.nFrames * MAX_SPRITES_PER_FRAME);
    MycRom.SpriteDescriptions = (UINT16*)malloc(MycRom.nSprites * MAX_SPRITE_SIZE * MAX_SPRITE_SIZE * sizeof(UINT16));
    MycRom.ColorRotations = (UINT8*)malloc(MycRom.nFrames * 3 * MAX_COLOR_ROTATION);
    MycRom.SpriteDetDwords = (UINT32*)malloc(MycRom.nSprites * sizeof(UINT32) * MAX_SPRITE_DETECT_AREAS);
    MycRom.SpriteDetDwordPos = (UINT16*)malloc(MycRom.nSprites * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS);
    MycRom.SpriteDetAreas = (UINT16*)malloc(MycRom.nSprites * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS * 4);
    MycRom.TriggerID = (UINT32*)malloc(MycRom.nFrames * sizeof(UINT32));
    if ((!MycRom.ShapeCompMode) || (!MycRom.HashCode) || (!MycRom.CompMaskID) || (!MycRom.CompMasks) ||
        (!MycRom.cPal) || (!MycRom.cFrames) || (!MycRom.DynaMasks) || (!MycRom.Dyna4Cols) || (!MycRom.FrameSprites) ||
        (!MycRom.SpriteDescriptions) || (!MycRom.MovRctID) ||(!MycRom.MovRcts) ||
        (!MycRom.ColorRotations) || (!MycRom.SpriteDetDwords) || (!MycRom.SpriteDetDwordPos) || (!MycRom.SpriteDetAreas) || (!MycRom.TriggerID))
    {
        cprintf("Can't get the buffers in Load_cRom");
        Free_cRom(); // We free the buffers we got
        fclose(pf);
        return false;
    }
    memset(MycRom.HashCode, 0, sizeof(UINT) * MycRom.nFrames);
    memset(MycRom.ShapeCompMode, FALSE, MycRom.nFrames);
    memset(MycRom.CompMaskID, 255, MycRom.nFrames);
    memset(MycRom.CompMasks, 0, MAX_MASKS * MycRom.fWidth * MycRom.fHeight);
    memset(MycRom.DynaMasks, 255, MycRom.nFrames * MycRom.fWidth * MycRom.fHeight);
    for (UINT tj = 0; tj < MycRom.nFrames; tj++)
    {
        for (UINT ti = 0; ti < MAX_DYNA_SETS_PER_FRAME; ti++)
        {
            for (UINT tk = 0; tk < MycRom.noColors; tk++) MycRom.Dyna4Cols[tj * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors + ti * MycRom.noColors + tk] = (UINT8)(ti * MycRom.noColors + tk);
        }
    }
    memset(MycRom.FrameSprites, 255, MycRom.nFrames * MAX_SPRITES_PER_FRAME);
    memset(MycRom.SpriteDescriptions, 0, MycRom.nSprites * MAX_SPRITE_SIZE * MAX_SPRITE_SIZE * sizeof(UINT16));
    memset(MycRom.ColorRotations, 255, MycRom.nFrames * 3 * MAX_COLOR_ROTATION);
    memset(MycRom.SpriteDetDwords, 0, MycRom.nSprites * sizeof(UINT32) * MAX_SPRITE_DETECT_AREAS);
    memset(MycRom.SpriteDetDwordPos, 0, MycRom.nSprites * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS);
    memset(MycRom.SpriteDetAreas, 255, MycRom.nSprites * sizeof(UINT16) * MAX_SPRITE_DETECT_AREAS * 4);
    memset(MycRom.TriggerID, 255, MycRom.nFrames * sizeof(UINT32));
    // Initiate cRP
    MycRP.oFrames = (UINT8*)malloc(MycRom.nFrames * MycRom.fWidth * MycRom.fHeight);
    if (!MycRP.oFrames)
    {
        cprintf("Can't get the buffer in Load_cRP");
        Free_cRom();
        Free_cRP(); // We free the buffers we got
        fclose(pf);
        return false;
    }
    MycRP.FrameDuration = (UINT32*)malloc(MycRom.nFrames * sizeof(UINT32));
    if (!MycRP.FrameDuration)
    {
        cprintf("Can't get the buffer in Load_cRP");
        Free_cRom();
        Free_cRP(); // We free the buffers we got
        fclose(pf);
        return false;
    }
    strcpy_s(MycRP.name, 64, MycRom.name);
    for (UINT ti = 0; ti < MycRom.nFrames; ti++) MycRP.activeColSet[ti] = FALSE;
    memset(MycRP.ColSets, 0, MAX_COL_SETS * 16);
    MycRP.acColSet = MycRP.preColSet = 0;
    memset(MycRP.nameColSet, 0, MAX_COL_SETS * 64);
    MycRP.DrawColMode = MycRP.Draw_Mode = MycRP.Mask_Sel_Mode = 0;
    MycRP.Fill_Mode = true;
    memset(MycRP.Mask_Names, 0, MAX_MASKS * SIZE_MASK_NAME);
    MycRP.nSections = 0;
    memset(MycRP.Section_Firsts, 0, MAX_SECTIONS * sizeof(UINT32));
    memset(MycRP.Section_Names, 0, MAX_SECTIONS * SIZE_SECTION_NAMES);
    memset(MycRP.Sprite_Names, 0, 255 * SIZE_SECTION_NAMES);
    memset(MycRP.Sprite_Col_From_Frame, 0, 255 * sizeof(UINT32));
    for (int ti = 0; ti < 255; ti++)
    {
        for (int tj = 0; tj < 16; tj++) MycRP.Sprite_Edit_Colors[ti + tj * 255] = tj;
    }
    memset(MycRP.SpriteRect, 1, 255 * 4 * sizeof(UINT16));
    memset(MycRP.SpriteRectMirror, 0, sizeof(BOOL) * 255 * 2);
    //-----------------
    for (UINT32 ti = 0; ti < MycRom.nFrames; ti++)
    {
        fread(&MycRP.FrameDuration[ti], sizeof(UINT32), 1, pf);
        if (ti > 0) MycRP.FrameDuration[ti - 1] = MycRP.FrameDuration[ti] - MycRP.FrameDuration[ti - 1];
        if (ti == MycRom.nFrames - 1) MycRP.FrameDuration[ti] = 3000;
        fread(&MycRP.oFrames[ti * MycRom.fWidth * MycRom.fHeight], 1, MycRom.fWidth * MycRom.fHeight, pf);
        fread(&MycRom.cFrames[ti * MycRom.fWidth * MycRom.fHeight], 1, MycRom.fWidth * MycRom.fHeight, pf);
        MycRom.HashCode[ti] = crc32_fast(&MycRP.oFrames[ti * MycRom.fWidth * MycRom.fHeight], MycRom.fWidth * MycRom.fHeight);
        fread(&MycRom.cPal[ti * 3 * MycRom.ncColors], 1, 3 * MycRom.ncColors, pf);
    }
    unsigned char maxocol = 0;
    for (UINT ti = 0; ti < MycRom.nFrames * MycRom.fWidth * MycRom.fHeight; ti++)
    {
        if (MycRP.oFrames[ti] > maxocol) maxocol = MycRP.oFrames[ti];
    }
    if (maxocol < 4) MycRom.noColors = 4; else MycRom.noColors = 16;
    for (unsigned int ti = 0; ti < MycRom.noColors; ti++) // shader of noColors oranges
    {
        originalcolors[ti * 3] = (UINT8)(255.0 * (float)ti / (float)(MycRom.noColors - 1));
        originalcolors[ti * 3 + 1] = (UINT8)(127.0 * (float)ti / (float)(MycRom.noColors - 1));
        originalcolors[ti * 3 + 2] = (UINT8)(0.0 * (float)ti / (float)(MycRom.noColors - 1));
    }
    fclose(pf);
    Calc_Resize_Frame();
    UpdateFSneeded = true;
    Delete_Duplicate_Frame();
    return true;
}

bool Add_cDump(char* path)//, char* rom)
{
    //char tbuf[MAX_PATH];
    //sprintf_s(tbuf, MAX_PATH, "%s%s.cdump", path, rom);
#pragma warning(suppress : 4996)
    FILE* pf = fopen(path, "rb");
    if (pf == 0)
    {
        MessageBoxA(hWnd, "Can't open the file", "Error", MB_OK);
        return false;
    }
    // Initiate cROM
    /*if (strcmp(MycRom.name, rom) != 0)
    {
        MessageBoxA(hWnd, "The rom name is not the same, this file can't be added", "Error", MB_OK);
        fclose(pf);
        return false;
    }*/
    UINT32 tnframes, tvar;
    fread(&tnframes, sizeof(UINT32), 1, pf);
    char tbuf[64];
    fread(&tbuf, 1, 64, pf);
    if (strcmp(MycRom.name, tbuf) != 0)
    {
        MessageBoxA(hWnd, "The rom name is different, this file can't be added", "Error", MB_OK);
        fclose(pf);
        return false;
    }
    fread(&tvar, sizeof(UINT32), 1, pf);
    if (tvar != MycRom.fWidth)
    {
        MessageBoxA(hWnd, "The rom width is different, this file can't be added", "Error", MB_OK);
        fclose(pf);
        return false;
    }
    fread(&tvar, sizeof(UINT32), 1, pf);
    if (tvar != MycRom.fHeight)
    {
        MessageBoxA(hWnd, "The rom height is different, this file can't be added", "Error", MB_OK);
        fclose(pf);
        return false;
    }
    fread(&tvar, sizeof(UINT32), 1, pf);
    MycRom.HashCode = (UINT*)realloc(MycRom.HashCode, sizeof(UINT) * (MycRom.nFrames + tnframes));
    MycRom.ShapeCompMode = (UINT8*)realloc(MycRom.ShapeCompMode, MycRom.nFrames + tnframes);
    MycRom.CompMaskID = (UINT8*)realloc(MycRom.CompMaskID, MycRom.nFrames + tnframes);
    MycRom.MovRctID = (UINT8*)realloc(MycRom.MovRctID, MycRom.nFrames + tnframes);
    MycRom.cPal = (UINT8*)realloc(MycRom.cPal, (MycRom.nFrames + tnframes) * 3 * MycRom.ncColors);
    MycRom.cFrames = (UINT8*)realloc(MycRom.cFrames, (MycRom.nFrames + tnframes) * MycRom.fWidth * MycRom.fHeight);
    MycRom.DynaMasks = (UINT8*)realloc(MycRom.DynaMasks, (MycRom.nFrames + tnframes) * MycRom.fWidth * MycRom.fHeight);
    MycRom.Dyna4Cols = (UINT8*)realloc(MycRom.Dyna4Cols, (MycRom.nFrames + tnframes) * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors);
    MycRom.FrameSprites = (UINT8*)realloc(MycRom.FrameSprites, (MycRom.nFrames + tnframes) * MAX_SPRITES_PER_FRAME);
    MycRom.ColorRotations = (UINT8*)realloc(MycRom.ColorRotations, (MycRom.nFrames + tnframes) * 3 * MAX_COLOR_ROTATION);
    MycRom.TriggerID = (UINT32*)realloc(MycRom.TriggerID, (MycRom.nFrames + tnframes) * sizeof(UINT32));
    if ((!MycRom.ShapeCompMode) || (!MycRom.HashCode) || (!MycRom.CompMaskID) || (!MycRom.MovRctID) ||
        (!MycRom.cPal) || (!MycRom.cFrames) || (!MycRom.DynaMasks) || (!MycRom.Dyna4Cols) || (!MycRom.FrameSprites) ||
        (!MycRom.SpriteDescriptions) ||
        (!MycRom.ColorRotations) || (!MycRom.SpriteDetDwords) || (!MycRom.SpriteDetDwordPos) || (!MycRom.SpriteDetAreas) || (!MycRom.TriggerID))
    {
        cprintf("Can't extend the buffers in Add_cDump, the whole rom is unloaded");
        Free_cRom(); // We free the buffers we got
        fclose(pf);
        return false;
    }
    memset(&MycRom.HashCode[MycRom.nFrames], 0, sizeof(UINT) * tnframes);
    memset(&MycRom.ShapeCompMode[MycRom.nFrames], FALSE, tnframes);
    memset(&MycRom.CompMaskID[MycRom.nFrames], 255, tnframes);
    memset(&MycRom.MovRctID[MycRom.nFrames], 255, tnframes);
    memset(&MycRom.DynaMasks[MycRom.nFrames * MycRom.fWidth * MycRom.fHeight], 255, tnframes * MycRom.fWidth * MycRom.fHeight);
    for (UINT tj = 0; tj < tnframes; tj++)
    {
        for (UINT ti = 0; ti < MAX_DYNA_SETS_PER_FRAME; ti++)
        {
            for (UINT tk = 0; tk < MycRom.noColors; tk++) MycRom.Dyna4Cols[MycRom.nFrames * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors + tj * MAX_DYNA_SETS_PER_FRAME * MycRom.noColors + ti * MycRom.noColors + tk] = (UINT8)(ti * MycRom.noColors + tk);
        }
    }
    memset(&MycRom.FrameSprites[MycRom.nFrames * MAX_SPRITES_PER_FRAME], 255, tnframes * MAX_SPRITES_PER_FRAME);
    memset(&MycRom.ColorRotations[MycRom.nFrames * 3 * MAX_COLOR_ROTATION], 255, tnframes * 3 * MAX_COLOR_ROTATION);
    memset(&MycRom.TriggerID[MycRom.nFrames], 255, tnframes * sizeof(UINT32));
    // Initiate cRP
    MycRP.oFrames = (UINT8*)realloc(MycRP.oFrames, (tnframes + MycRom.nFrames) * MycRom.fWidth * MycRom.fHeight);
    if (!MycRP.oFrames)
    {
        cprintf("Can't extend the buffer in Add_cDump");
        Free_cRom();
        Free_cRP(); // We free the buffers we got
        fclose(pf);
        return false;
    }
    MycRP.FrameDuration = (UINT32*)realloc(MycRP.FrameDuration, (MycRom.nFrames + tnframes) * sizeof(UINT32));
    if (!MycRP.FrameDuration)
    {
        cprintf("Can't extend the buffer in Add_cDump");
        Free_cRom();
        Free_cRP(); // We free the buffers we got
        fclose(pf);
        return false;
    }
    for (UINT ti = MycRom.nFrames; ti < MycRom.nFrames + tnframes; ti++) MycRP.activeColSet[ti] = FALSE;
    //-----------------
    for (UINT32 ti = MycRom.nFrames; ti < MycRom.nFrames + tnframes; ti++)
    {
        fread(&MycRP.FrameDuration[ti], sizeof(UINT32), 1, pf);
        if (ti > 0) MycRP.FrameDuration[ti - 1] = MycRP.FrameDuration[ti] - MycRP.FrameDuration[ti - 1];
        if (ti == MycRom.nFrames + tnframes - 1) MycRP.FrameDuration[ti] = 3000;
        fread(&MycRP.oFrames[ti * MycRom.fWidth * MycRom.fHeight], 1, MycRom.fWidth * MycRom.fHeight, pf);
        fread(&MycRom.cFrames[ti * MycRom.fWidth * MycRom.fHeight], 1, MycRom.fWidth * MycRom.fHeight, pf);
        MycRom.HashCode[ti] = crc32_fast(&MycRP.oFrames[ti * MycRom.fWidth * MycRom.fHeight], MycRom.fWidth * MycRom.fHeight);
        fread(&MycRom.cPal[ti * 3 * MycRom.ncColors], 1, 3 * MycRom.ncColors, pf);
    }
    fclose(pf);
    MycRom.nFrames += tnframes;
    Calc_Resize_Frame();
    UpdateFSneeded = true;
    Delete_Duplicate_Frame();
    return true;
}

/*#define MAX_FILES_LIST 2048
char dir_list[MAX_FILES_LIST * MAX_PATH];
char rom_list[MAX_FILES_LIST * MAX_PATH];

void get_file_list(char* folder, char* filtername, HWND hLst, int* pnfiles)
{
    HANDLE hFind;
    WIN32_FIND_DATAA data;
    char szFullPattern[MAX_PATH];
    PathCombineA(szFullPattern, folder, "*.*");
    hFind = FindFirstFileA(szFullPattern, &data);
    szFullPattern[0] = 0;

    if (hFind != INVALID_HANDLE_VALUE) 
    {
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (data.cFileName[0] != '.'))
            {
                // found a subdirectory; recurse into it
                PathCombineA(szFullPattern, folder, data.cFileName);
                strcat_s(szFullPattern, MAX_PATH, "\\");
                get_file_list(szFullPattern, filtername, hLst,pnfiles);
            }
        } while (FindNextFileA(hFind, &data));
        FindClose(hFind);
    }

    // Now we are going to look for the matching files
    PathCombineA(szFullPattern, folder, filtername);
    hFind = FindFirstFileA(szFullPattern, &data);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (*pnfiles < MAX_FILES_LIST)))
            {
                // found a file; do something with it
                PathCombineA(szFullPattern, folder, data.cFileName);
                strcpy_s(&dir_list[*pnfiles * MAX_PATH], MAX_PATH, folder);
                strcpy_s(&rom_list[*pnfiles * MAX_PATH], MAX_PATH, data.cFileName);
                rom_list[*pnfiles * MAX_PATH + strlen(&rom_list[*pnfiles * MAX_PATH]) - 6] = 0;
                SendMessageA(hLst, LB_ADDSTRING, 0, (LPARAM)szFullPattern);
                (*pnfiles)++;
            }
        } while (FindNextFileA(hFind, &data));
        FindClose(hFind);
    }
}*/


#pragma endregion Project_File_Functions


#pragma region Window_Procs

/*BOOL CALLBACK List_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            SendMessage(GetDlgItem(hDlg, IDC_LIST), LB_RESETCONTENT, 0, 0);
            int nfiles = 0;
            get_file_list(MycRP.SaveDir, (char*)"*.cdump", GetDlgItem(hDlg, IDC_LIST), &nfiles);
            if (nfiles == 0)
            {
                MessageBoxA(hWnd, "No cdump file found in the altcolor subdirectories", "Error", MB_OK);
                EndDialog(hDlg, -1);
                return 0;
            }
            return 0;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDOK:
                {
                    EndDialog(hDlg, SendMessage(GetDlgItem(hDlg, IDC_LIST), LB_GETCURSEL, 0, 0));
                    return TRUE;
                }
                case IDCANCEL:
                {
                    EndDialog(hDlg, -1);
                    return TRUE;
                }
                default:
                    return FALSE;
            }
        }
        default:
            return FALSE;
    }
    return TRUE;
}*/

LRESULT CALLBACK WndProc(HWND hWin, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        /*switch (wmId)
        {
        default:
            return DefWindowProc(hWin, message, wParam, lParam);
            break;
        }*/
    }
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 1024 + 16; // minimum 1024
        mmi->ptMinTrackSize.y = 300 + 59; // per 512
        return 0;
        break;
    }
    case WM_SIZE:
    {
        if (!IsIconic(hWnd))
        {
            Calc_Resize_Frame();
            UpdateFSneeded = true;
        }
        break;
    }
    case WM_CLOSE:
    {
        if (MessageBoxA(hWnd, "Confirm you want to exit?", "Confirm", MB_YESNO) == IDYES) DestroyWindow(hWnd);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        fDone = true;
        break;
    }
    default:
        return DefWindowProc(hWin, message, wParam, lParam);
    }
    return 0;
}

bool isSReleased = false, isEnterReleased = false, isZReleased = false, isYReleased = false, isMReleased = false, isAReleased = false, isCReleased = false, isVReleased = false, isFReleased = false, isDReleased = false, isEReleased = false;

void CheckAccelerators(void)
{
    if (!(GetKeyState('S') & 0x8000)) isSReleased = true;
    if (!(GetKeyState('Z') & 0x8000)) isZReleased = true;
    if (!(GetKeyState('Y') & 0x8000)) isYReleased = true;
    if (!(GetKeyState('M') & 0x8000)) isMReleased = true;
    if (!(GetKeyState('F') & 0x8000)) isFReleased = true;
    if (!(GetKeyState('C') & 0x8000)) isCReleased = true;
    if (!(GetKeyState('A') & 0x8000)) isAReleased = true;
    if (!(GetKeyState('D') & 0x8000)) isDReleased = true;
    if (!(GetKeyState('E') & 0x8000)) isEReleased = true;
    if (!(GetKeyState('V') & 0x8000)) isVReleased = true;
    if (!(GetKeyState(VK_RETURN) & 0x8000)) isEnterReleased = true;
    if (GetForegroundWindow() == hWnd)
    {
        if (MycRom.name[0])
        {
            if (GetKeyState(VK_CONTROL) & 0x8000)
            {
                if ((isSReleased) && (GetKeyState('S') & 0x8000))
                {
                    isSReleased = false;
                    Save_cRP(MycRP.SaveDir);
                    Save_cRom(MycRP.SaveDir);
                }
                if ((isZReleased) && (GetKeyState('Z') & 0x8000))
                {
                    isZReleased = false;
                }
                if ((isYReleased) && (GetKeyState('Y') & 0x8000))
                {
                    isYReleased = false;
                }
                if ((isMReleased) && (GetKeyState('M') & 0x8000))
                {
                    isMReleased = false;
                }
                if ((isAReleased) && (GetKeyState('A') & 0x8000))
                {
                    isAReleased = false;
                }
                if ((isDReleased) && (GetKeyState('D') & 0x8000))
                {
                    isDReleased = false;
                }
                if ((isFReleased) && (GetKeyState('F') & 0x8000))
                {
                    isFReleased = false;
                }
                if ((isCReleased) && (GetKeyState('C') & 0x8000))
                {
                    isCReleased = false;
                }
                if ((isVReleased) && (GetKeyState('V') & 0x8000))
                {
                    isVReleased = false;
                }
                if ((isEReleased) && (GetKeyState('E') & 0x8000))
                {
                    isEReleased = false;
                }
            }
        }
    }
}

void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (MycRom.name[0] == 0) return;
    short step = (short)yoffset;
    if (!(GetKeyState(VK_SHIFT) & 0x8000)) PreFrameInStrip -= step;
    else PreFrameInStrip -= step * (int)NFrameToDraw;
    if (PreFrameInStrip < 0) PreFrameInStrip = 0;
    if (PreFrameInStrip >= (int)MycRom.nFrames) PreFrameInStrip = (int)MycRom.nFrames - 1;
    UpdateFSneeded = true;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int xipos = (int)xpos, yipos = (int)ypos;

    if (MycRom.name[0] == 0) return; // we have no project loaded, ignore
    // Click on the frame strip
    if ((button == GLFW_MOUSE_BUTTON_LEFT) && (action == GLFW_RELEASE)) MouseFrSliderLPressed = false;
    if ((button == GLFW_MOUSE_BUTTON_LEFT) && (action == GLFW_PRESS))
    {
        if (window == glfwframestripc)
        {
            // are we on the slider
            if ((yipos >= FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 - 5) && (yipos <= FRAME_STRIP_HEIGHT - FRAME_STRIP_H_MARGIN / 2 + 6))
            {
                if ((xipos >= FRAME_STRIP_SLIDER_MARGIN) && (xipos <= (int)ScrW - FRAME_STRIP_SLIDER_MARGIN))
                {
                    MouseFrSliderLPressed = true;
                    MouseFrSliderlx = xipos - FRAME_STRIP_SLIDER_MARGIN;
                }
                return;
            }
        }
        // are we on a frame of the strip to select
        UINT wid = 256;
        if (MycRom.fWidth == 192) wid = 192;
        if ((yipos >= FRAME_STRIP_H_MARGIN) && (yipos < FRAME_STRIP_H_MARGIN + 128))
        {
            if ((xipos >= (int)FS_LMargin) && (xipos <= (int)ScrW - (int)FS_LMargin))
            {
                if ((xipos - FS_LMargin) % (wid + FRAME_STRIP_W_MARGIN) < wid) // check that we are not between the frames
                {
                    UINT tpFrame = PreFrameInStrip + (xipos - FS_LMargin) / (wid + FRAME_STRIP_W_MARGIN);
                    if (tpFrame >= MycRom.nFrames) tpFrame = MycRom.nFrames - 1;
                    if (tpFrame < 0) tpFrame = 0;
                    if (mods & GLFW_MOD_SHIFT)
                    {
                        // select a range
                        if (preframe > tpFrame)
                        {
                            nselframes = preframe - tpFrame + 1;
                            preframe = tpFrame;
                        }
                        else nselframes = tpFrame - preframe + 1;
                    }
                    else
                    {
                        // selecting just this frame
                        preframe = tpFrame;
                        nselframes = 1;
                    }
                    UpdateFSneeded = true;
                }
            }
        }
    }
    return;
}

bool isPressed(int nVirtKey, DWORD* timePress)
{
    // Check if a key is pressed, manage repetition (first delay longer than the next ones)
    if (GetKeyState(nVirtKey) & 0x8000)
    {
        DWORD acTime = timeGetTime();
        if (acTime > (*timePress))
        {
            if ((*timePress) == 0) (*timePress) = FIRST_KEY_TIMER_INT + acTime; else(*timePress) = NEXT_KEY_TIMER_INT + acTime;
            return true;
        }
    }
    else (*timePress) = 0;
    return false;
}

#pragma endregion Window_Procs

#pragma region Window_Creations

bool CreateTextures(void)
{
    // Create the textures for the lower strip showing the frames
    glfwMakeContextCurrent(glfwframestripo);
    MonWidth = GetSystemMetrics(SM_CXFULLSCREEN);

    pFrameoStrip = (UINT8*)malloc(MonWidth * FRAME_STRIP_HEIGHT * 4); //RGBA for efficiency and alignment
    if (!pFrameoStrip)
    {
        return false;
    }

    glGenTextures(2, TxFrameStripo);

    glBindTexture(GL_TEXTURE_2D, TxFrameStripo[0]);
    // We allocate a texture corresponding to the width of the screen in case we are fullscreen by 64-pixel (frame) + 2x16-pixel (margins) height
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MonWidth, FRAME_STRIP_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, TxFrameStripo[1]);
    // We allocate a texture corresponding to the width of the screen in case we are fullscreen by 64-pixel (frame) + 2x16-pixel (margins) height
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MonWidth, FRAME_STRIP_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Create the number texture
    Bitmap* pbmp = Bitmap::FromFile(L"textures\\chiffres.png");
    if (pbmp == NULL) return false;
    Rect rect = Rect(0, 0, pbmp->GetWidth(), pbmp->GetHeight());
    BitmapData bmpdata;
    pbmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpdata);
    glGenTextures(1, &TxChiffres);

    glBindTexture(GL_TEXTURE_2D, TxChiffres);
    // We allocate a texture corresponding to the width of the screen in case we are fullscreen by 64-pixel (frame) + 2x16-pixel (margins) height
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 200, 32, 0, GL_BGRA, GL_UNSIGNED_BYTE, bmpdata.Scan0);
    pbmp->UnlockBits(&bmpdata);
    delete pbmp;

    // Create the textures for the lower strip showing the sprite
    glfwMakeContextCurrent(glfwframestripc);
    MonWidth = GetSystemMetrics(SM_CXFULLSCREEN);

    pFramecStrip = (UINT8*)malloc(MonWidth * FRAME_STRIP_HEIGHT * 4); //RGBA for efficiency and alignment
    if (!pFramecStrip)
    {
        return false;
    }

    glGenTextures(2, TxFrameStripc);

    glBindTexture(GL_TEXTURE_2D, TxFrameStripc[0]);
    // We allocate a texture corresponding to the width of the screen in case we are fullscreen by 64-pixel (frame) + 2x16-pixel (margins) height
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MonWidth, FRAME_STRIP_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, TxFrameStripc[1]);
    // We allocate a texture corresponding to the width of the screen in case we are fullscreen by 64-pixel (frame) + 2x16-pixel (margins) height
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MonWidth, FRAME_STRIP_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    
    // Create the number texture
    pbmp = Bitmap::FromFile(L"textures\\chiffres.png");
    if (pbmp == NULL) return false;
    rect = Rect(0, 0, pbmp->GetWidth(), pbmp->GetHeight());
    pbmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpdata);
    glGenTextures(1, &TxChiffres);

    glBindTexture(GL_TEXTURE_2D, TxChiffres);
    // We allocate a texture corresponding to the width of the screen in case we are fullscreen by 64-pixel (frame) + 2x16-pixel (margins) height
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 200, 32, 0, GL_BGRA, GL_UNSIGNED_BYTE, bmpdata.Scan0);
    pbmp->UnlockBits(&bmpdata);
    delete pbmp;

    // Read the RAW digit definition
    FILE* pfile;
    if (fopen_s(&pfile, "textures\\chiffres.raw", "rb")) return false;
    fread(Raw_Digit_Def, 1, RAW_DIGIT_W * RAW_DIGIT_H * 10, pfile);
    fclose(pfile);
    return true;
}

bool SetIcon(HWND ButHWND, UINT ButIco)
{
    HICON hicon = LoadIcon(hInst, MAKEINTRESOURCE(ButIco));
    if (!hicon) AffLastError((char*)"SetIcon");
    SendMessageW(ButHWND, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hicon);
    DestroyIcon(hicon);
    return true;
}

INT_PTR CALLBACK Toolbar_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_OPENCDUMP:
                {
                    //int nrest = (int)DialogBoxA(hInst, MAKEINTRESOURCEA(IDD_LISTITEMS), hWnd, (DLGPROC)List_Proc);
                    //if (nrest >= 0)
                    //{
                    OPENFILENAMEA ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    char szFile[MAX_PATH];
                    ofn.lStructSize = sizeof(OPENFILENAMEA);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFile = szFile;
                    ofn.lpstrFile[0] = '\0';
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "cDump\0*.cDump\0\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    GetOpenFileNameA(&ofn);
                    Load_cDump(ofn.lpstrFile);
                    //}
                    return TRUE;
                }
                case IDC_ADDCDUMP:
                {
                    if (MycRom.name[0] == 0) return TRUE;
                    OPENFILENAMEA ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    char szFile[MAX_PATH];
                    ofn.lStructSize = sizeof(OPENFILENAMEA);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFile = szFile;
                    ofn.lpstrFile[0] = '\0';
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "cDump\0*.cDump\0\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    GetOpenFileNameA(&ofn);
                    Add_cDump(ofn.lpstrFile);
                    return TRUE;
                }
                case IDC_OPENCROM:
                {
                    OPENFILENAMEA ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    char szFile[MAX_PATH];
                    ofn.lStructSize = sizeof(OPENFILENAMEA);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFile = szFile;
                    ofn.lpstrFile[0] = '\0';
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "Serum(cRom)\0*.cRom\0\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    GetOpenFileNameA(&ofn);
                    if (Load_cRom(ofn.lpstrFile))
                    {
                        ofn.lpstrFile[strlen(ofn.lpstrFile) - 2] = 'P';
                        ofn.lpstrFile[strlen(ofn.lpstrFile) - 1] = 0;
                        Load_cRP(ofn.lpstrFile);
                    }
                    return TRUE;
                }
                /*case IDC_ADDCROM:
                {
                    if (MycRom.name[0] == 0) return TRUE;
                    OPENFILENAMEA ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    char szFile[MAX_PATH];
                    ofn.lStructSize = sizeof(OPENFILENAMEA);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFile = szFile;
                    ofn.lpstrFile[0] = '\0';
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "Serum(cRom)\0*.cRom\0\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    GetOpenFileNameA(&ofn);
                    Add_cRom(ofn.lpstrFile);
                    return TRUE;
                }*/
                case IDC_VPMPATH:
                {
                    BROWSEINFOA bi;
                    bi.hwndOwner = hWnd;
                    bi.pidlRoot = NULL;
                    //char tbuf2[MAX_PATH];
                    bi.pszDisplayName = MycRP.SaveDir;
                    bi.lpszTitle = "Browse for the VPinMAME directory...";
                    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                    bi.lpfn = NULL;
                    bi.iImage = 0;
                    LPITEMIDLIST piil;
                    piil = SHBrowseForFolderA(&bi);
                    if (!piil) return false;
                    SHGetPathFromIDListA(piil, MycRP.SaveDir);
                    if (MycRP.SaveDir[strlen(MycRP.SaveDir) - 1] != '\\') strcat_s(MycRP.SaveDir, MAX_PATH, "\\");
                    CoTaskMemFree(piil);
                    SetDlgItemTextA(hwTB, IDC_VPATH, MycRP.SaveDir);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}


bool CreateToolbar(void)
{
    if (hwTB)
    {
        DestroyWindow(hwTB);
    }

    hwTB = CreateDialog(hInst, MAKEINTRESOURCE(IDD_TOOLBOX), hWnd, Toolbar_Proc);
    if (!hwTB)
    {
        cprintf("Unable to create the comparison toolbar");
        return false;
    }
    ShowWindow(hwTB, TRUE);
    SetIcon(GetDlgItem(hwTB, IDC_OPENCROM), IDI_OPEN);
    SetIcon(GetDlgItem(hwTB, IDC_SAVECROM), IDI_SAVE);
    SetIcon(GetDlgItem(hwTB, IDC_OPENCDUMP), IDI_OPENCDUMP);
    SetIcon(GetDlgItem(hwTB, IDC_ADDCDUMP), IDI_ADDCDUMP);
    SetIcon(GetDlgItem(hwTB, IDC_DELFRAMES), IDI_DELFRAME);
    SetIcon(GetDlgItem(hwTB, IDC_MOVEFRAMES), IDI_MOVEFRAME);
    SetIcon(GetDlgItem(hwTB, IDC_VPMPATH), IDI_VPMPATH);
    SetIcon(GetDlgItem(hwTB, IDC_ADDCROM), IDI_ADDCROM);
    SetIcon(GetDlgItem(hwTB, IDC_OPENTXT), IDI_OPENTXT);
    SetIcon(GetDlgItem(hwTB, IDC_ADDTXT), IDI_ADDTXT);

    SetWindowLong(GetDlgItem(hwTB, IDC_STRY1), GWL_STYLE, WS_BORDER | WS_CHILD | WS_VISIBLE | SS_BLACKRECT);
    SetWindowPos(GetDlgItem(hwTB, IDC_STRY1), 0, 0, 0, 5, 100, SWP_NOMOVE | SWP_NOZORDER);

    // to avoid beeps on key down
    SetFocus(hwTB);
    PostMessage(hwTB, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hwTB, IDC_NEW), TRUE);

    SetDlgItemTextA(hwTB, IDC_VPATH, MycRP.SaveDir);

    return true;
}


#pragma endregion Window_Creations

#pragma region Main

bool isLeftReleased = false, isRightReleased = false;

bool LoadVPinMAMEDir(void)
{
    FILE* pfile;
    if (fopen_s(&pfile, "VPMDir.pos", "rb") != 0)
    {
        cprintf("No save directory file found, using default");
        strcpy_s(MycRP.SaveDir, 260, "C:\\visual pinball\\VPinMame\\");
        return false;
    }
    fread(MycRP.SaveDir, 1, MAX_PATH, pfile);
    fclose(pfile);
    return true;
}

void SaveVPinMAMEDir(void)
{
    FILE* pfile;
    if (fopen_s(&pfile, "VPMDir.pos", "wb") != 0)
    {
        cprintf("Error while saving save directory file");
        return;
    }
    fwrite(MycRP.SaveDir, 1, MAX_PATH, pfile);
    fclose(pfile);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{

    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);

    hInst = hInstance;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&icex);   
    CoInitialize(NULL);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != 0)
    {
        cprintf("Can't initialize GDI+");
        return -1;
    }

    LoadVPinMAMEDir();

    AllocConsole();
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    cprintf("ColorizingDMD started");
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_CROM)); // LoadIcon(hInstance, MAKEINTRESOURCE(IDI_COLORIZINGDMD));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;// MAKEINTRESOURCEW(IDC_COLORIZINGDMD);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = NULL; // LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    if (!RegisterClassEx(&wcex))
    {
        cprintf("Call to RegisterClassEx failed!");
        return 1;
    }
    int monwidth = GetSystemMetrics(SM_CXFULLSCREEN) + 8;
    hWnd = CreateWindow(szWindowClass, L"ColorizingDMD", WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, monwidth, 300, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd)
    {
        AffLastError((char*)"CreateWindow");
        return FALSE;
    }
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    build_crc32_table();

    if (!gl33_InitWindow(&glfwframestripo, 10, 10, "Original Frame Strip", hWnd)) return -1;
    if (!gl33_InitWindow(&glfwframestripc, 10, 10, "Colorized Frame Strip", hWnd)) return -1;
    glfwSetMouseButtonCallback(glfwframestripo, mouse_button_callback);
    glfwSetMouseButtonCallback(glfwframestripc, mouse_button_callback);
    glfwSetScrollCallback(glfwframestripo, mouse_scroll_callback);
    glfwSetScrollCallback(glfwframestripc, mouse_scroll_callback);

    CreateToolbar();

    MSG msg;

    if (!CreateTextures()) return -1;
    Calc_Resize_Frame();
    UpdateFSneeded = true;

    MycRom.name[0] = 0;
    MycRom.cFrames = NULL;
    MycRom.cPal = NULL;
    MycRP.oFrames = NULL;
    MycRP.FrameDuration = NULL;

    //Load_cDump(MycRP.SaveDir, (char*)"jy_12");
    //Load_cDump(MycRP.SaveDir, (char*)"baywatch");

    // Boucle de messages principale :
    while (!fDone)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsIconic(hWnd))
        {
            if (!(GetKeyState(VK_LEFT) & 0x8000)) isLeftReleased = true;
            if (!(GetKeyState(VK_RIGHT) & 0x8000)) isRightReleased = true;
            if (MycRom.name[0] != 0)
            {
                if (UpdateFSneeded)
                {
                    Frame_Strip_Updateo();
                    Frame_Strip_Updatec();
                    UpdateFSneeded = false;
                }
                Draw_Frame_Strip();
            }
            if (MouseFrSliderLPressed)
            {
                double x, y;
                glfwGetCursorPos(glfwframestripc, &x, &y);
                int px = (int)x;// - 2 * FRAME_STRIP_SLIDER_MARGIN;
                if (px != MouseFrSliderlx)
                {
                    MouseFrSliderlx = px;
                    if (px < 0) px = 0;
                    if (px > (int)ScrW - 2 * FRAME_STRIP_SLIDER_MARGIN) px = (int)ScrW - 2 * FRAME_STRIP_SLIDER_MARGIN;
                    PreFrameInStrip = (int)((float)px * (float)MycRom.nFrames / (float)SliderWidth);
                    if (PreFrameInStrip < 0) PreFrameInStrip = 0;
                    if (PreFrameInStrip >= (int)MycRom.nFrames) PreFrameInStrip = (int)MycRom.nFrames - 1;
                    UpdateFSneeded = true;
                }
            }
            float fps = 0;
            fps = gl33_SwapBuffers(glfwframestripo, true);
            gl33_SwapBuffers(glfwframestripc, true);
            CheckAccelerators();
            char tbuf[256];
            POINT tpt;
            GetCursorPos(&tpt);
            sprintf_s(tbuf, 256, "SortingCDump v%i.%i (by Zedrummer)     ROM name: %s      Frame: %i->%i/%i      @%.1fFPS", MAJ_VERSION, MIN_VERSION, MycRom.name, preframe, preframe + nselframes - 1, MycRom.nFrames, fps);
            SetWindowTextA(hWnd, tbuf);
            glfwPollEvents();
        }
    }
    SaveVPinMAMEDir();
    if (pFrameoStrip) free(pFrameoStrip);
    if (pFramecStrip) free(pFramecStrip);
    GdiplusShutdown(gdiplusToken);
    glfwTerminate();
    cprintf("ColorizingDMD terminated");
    return 0;
}

#pragma endregion Main
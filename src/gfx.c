#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <png.h>
#include <jpeglib.h>

#include "gfx.h"

static uint32_t fbw = 0, fbh = 0;
static tex *frameBuffer;

static inline uint32_t blend(const color px, const color fb)
{
    if(px.a == 0x00)
        return colorGetColor(fb);
    else if(px.a == 0xFF)
        return colorGetColor(px);

    uint8_t subAl = 0xFF - px.a;

    uint8_t fR = (px.r * px.a + fb.r * subAl) / 0xFF;
    uint8_t fG = (px.g * px.a + fb.g * subAl) / 0xFF;
    uint8_t fB = (px.b * px.a + fb.b * subAl) / 0xFF;

    return (0xFF << 24 | fB << 16 | fG << 8 | fR);
}

static inline uint32_t smooth(const color px1, const color px2)
{
    uint8_t fR = (px1.r + px2.r) / 2;
    uint8_t fG = (px1.g + px2.g) / 2;
    uint8_t fB = (px1.b + px2.b) / 2;
    uint8_t fA = (px1.a + px2.a) / 2;

    return (fA << 24 | fB << 16 | fG << 8 | fR);
}

bool graphicsInit(int windowWidth, int windowHeight)
{
    gfxInitResolution((uint32_t)windowWidth, (uint32_t)windowHeight);
    gfxInitDefault();
    plInitialize();
    consoleInit(NULL);

    gfxSetMode(GfxMode_LinearDouble);

    fbw = windowWidth;
    fbh = windowHeight;

    //Make a fake tex that points to framebuffer
    frameBuffer = malloc(sizeof(tex));
    frameBuffer->width = windowWidth;
    frameBuffer->height = windowHeight;
    frameBuffer->data = (uint32_t *)gfxGetFramebuffer(NULL, NULL);
    frameBuffer->size = windowWidth * windowHeight;

    return true;
}

bool graphicsExit()
{
    free(frameBuffer);

    plExit();
    gfxExit();

    return true;
}

void gfxHandleBuffs()
{
    gfxFlushBuffers();
    gfxSwapBuffers();
    gfxWaitForVsync();
}

static void drawGlyph(const FT_Bitmap *bmp, tex *target, int _x, int _y, const color c)
{
    if(bmp->pixel_mode != FT_PIXEL_MODE_GRAY)
        return;

    uint8_t *bmpPtr = bmp->buffer;
    for(int y = _y; y < _y + bmp->rows; y++)
    {
        if(y > fbh || y < 0)
            break;

        uint32_t *rowPtr = &target->data[y * target->width + _x];
        for(int x = _x; x < _x + bmp->width; x++, bmpPtr++, rowPtr++)
        {
            if(x > fbw || x < 0)
                break;

            if(*bmpPtr > 0)
            {
                color txClr, tgtClr;
                colorSetRGBA(&txClr, c.r, c.g, c.b, *bmpPtr);
                colorCreateFromU32(&tgtClr, *rowPtr);

                *rowPtr = blend(txClr, tgtClr);
            }
        }
    }
}

void drawText(const char *str, tex *target, const font *f, int x, int y, int sz, color c)
{
    int tmpX = x;
    FT_Error ret = 0;
    FT_GlyphSlot slot = f->face->glyph;
    uint32_t tmpChr = 0;
    ssize_t unitCnt = 0;

    FT_Set_Char_Size(f->face, 0, sz * 64, 90, 90);

    for(unsigned i = 0; i < strlen(str); )
    {
        unitCnt = decode_utf8(&tmpChr, (uint8_t *)&str[i]);
        if(unitCnt <= 0)
            break;

        i += unitCnt;
        if(tmpChr == '\n')
        {
            tmpX = x;
            y += sz + 8;
            continue;
        }

        ret = FT_Load_Glyph(f->face, FT_Get_Char_Index(f->face, tmpChr), FT_LOAD_RENDER);
        if(ret)
            return;

        int drawY = y + (sz - slot->bitmap_top);
        drawGlyph(&slot->bitmap, target, tmpX + slot->bitmap_left, drawY, c);

        tmpX += slot->advance.x >> 6;
    }
}

size_t textGetWidth(const char *str, const font *f, int sz)
{
    size_t width = 0;

    uint32_t untCnt = 0, tmpChr = 0;
    FT_GlyphSlot slot = f->face->glyph;
    FT_Error ret = 0;

    FT_Set_Char_Size(f->face, 0, 64 * sz, 90, 90);

    for(unsigned i = 0; i < strlen(str); )
    {
        untCnt = decode_utf8(&tmpChr, (uint8_t *)&str[i]);

        if(untCnt <= 0)
            break;

        i += untCnt;
        ret = FT_Load_Glyph(f->face, FT_Get_Char_Index(f->face, tmpChr), FT_LOAD_RENDER);
        if(ret)
            return 0;

        width += slot->advance.x >> 6;
    }

    return width;
}

void clearBufferColor(const color clr)
{
    uint32_t *fb = (uint32_t *)gfxGetFramebuffer(NULL, NULL);
    uint32_t clearClr = colorGetColor(clr);
    for(unsigned i = 0; i < gfxGetFramebufferSize() / 4; i++, fb++)
        *fb = clearClr;
}

void drawRect(tex *target, int x, int y, int w,  int h, const color c)
{
    uint32_t clr = colorGetColor(c);

    for(int tY = y; tY < y + h; tY++)
    {
        uint32_t *rowPtr = &target->data[tY * target->width + x];
        for(int tX = x; tX < x + w; tX++, rowPtr++)
            *rowPtr = clr;
    }
}

tex *texCreate(int w, int h)
{
    tex *ret = malloc(sizeof(tex));

    ret->width = w;
    ret->height = h;

    ret->data = (uint32_t *)malloc(w * h * sizeof(uint32_t));
    memset(ret->data, 0, w * h * sizeof(uint32_t));
    ret->size = ret->width * ret->height;

    return ret;
}

tex *texLoadPNGFile(const char *path)
{
    FILE *pngIn = fopen(path, "rb");
    if(pngIn != NULL)
    {
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if(png == 0)
            return NULL;

        png_infop pngInfo = png_create_info_struct(png);
        if(pngInfo == 0)
            return NULL;

        int jmp = setjmp(png_jmpbuf(png));
        if(jmp)
            return NULL;

        png_init_io(png, pngIn);
        png_read_info(png, pngInfo);

        if(png_get_color_type(png, pngInfo) != PNG_COLOR_TYPE_RGBA)
        {
            png_destroy_read_struct(&png, &pngInfo, NULL);
            return NULL;
        }

        tex *ret = malloc(sizeof(tex));
        ret->width = png_get_image_width(png, pngInfo);
        ret->height = png_get_image_height(png, pngInfo);

        ret->data = (uint32_t *)malloc((ret->width * ret->height) * sizeof(uint32_t));
        ret->size = ret->width * ret->height;

        png_bytep *rows = malloc(sizeof(png_bytep) * ret->height);
        for(int i = 0; i < ret->height; i++)
            rows[i] = malloc(png_get_rowbytes(png, pngInfo));

        png_read_image(png, rows);

        uint32_t *dataPtr = &ret->data[0];
        for(int y = 0; y < ret->height; y++)
        {
            uint32_t *rowPtr = (uint32_t *)rows[y];
            for(int x = 0; x < ret->width; x++)
                *dataPtr++ = *rowPtr++;
        }

        for(int i = 0; i < ret->height; i++)
            free(rows[i]);

        free(rows);

        png_destroy_read_struct(&png, &pngInfo, NULL);
        fclose(pngIn);

        return ret;
    }
    return NULL;
}

tex *texLoadJPEGFile(const char *path)
{
    FILE *jpegIn = fopen(path, "rb");
    if(jpegIn != NULL)
    {
        struct jpeg_decompress_struct jpegInfo;
        struct jpeg_error_mgr error;

        jpegInfo.err = jpeg_std_error(&error);

        jpeg_create_decompress(&jpegInfo);
        jpeg_stdio_src(&jpegInfo, jpegIn);
        jpeg_read_header(&jpegInfo, true);

        if(jpegInfo.jpeg_color_space == JCS_YCbCr)
            jpegInfo.out_color_space = JCS_RGB;

        tex *ret = malloc(sizeof(tex));

        ret->width = jpegInfo.image_width;
        ret->height = jpegInfo.image_height;

        ret->data = (uint32_t *)malloc((ret->width * ret->height) * sizeof(uint32_t));
        ret->size = ret->width * ret->height;

        jpeg_start_decompress(&jpegInfo);

        JSAMPARRAY row = malloc(sizeof(JSAMPROW));
        row[0] = malloc(sizeof(JSAMPLE) * ret->width * 3);

        uint32_t *dataPtr = &ret->data[0];
        for(int y = 0; y < ret->height; y++)
        {
            jpeg_read_scanlines(&jpegInfo, row, 1);
            uint8_t *jpegPtr = row[0];
            for(int x = 0; x < ret->width; x++, jpegPtr += 3)
            {
                *dataPtr++ = (0xFF << 24 | jpegPtr[2] << 16 | jpegPtr[1] << 8 | jpegPtr[0]);
            }
        }

        jpeg_finish_decompress(&jpegInfo);
        jpeg_destroy_decompress(&jpegInfo);

        free(row[0]);
        free(row);

        fclose(jpegIn);

        return ret;
    }
    return NULL;
}

tex *texLoadJPEGMem(const uint8_t *jpegData, size_t jpegSize)
{
    struct jpeg_decompress_struct jpegInfo;
    struct jpeg_error_mgr error;

    jpegInfo.err = jpeg_std_error(&error);

    jpeg_create_decompress(&jpegInfo);
    jpeg_mem_src(&jpegInfo, jpegData, jpegSize);
    jpeg_read_header(&jpegInfo, true);

    if(jpegInfo.jpeg_color_space == JCS_YCbCr)
        jpegInfo.out_color_space = JCS_RGB;

    tex *ret = malloc(sizeof(tex));
    ret->width = jpegInfo.image_width;
    ret->height = jpegInfo.image_height;

    ret->data = (uint32_t *)malloc((ret->width * ret->height) * sizeof(uint32_t));
    ret->size = ret->width * ret->height;

    jpeg_start_decompress(&jpegInfo);

    JSAMPARRAY row = malloc(sizeof(JSAMPARRAY));
    row[0] = malloc(sizeof(JSAMPLE) * ret->width * 3);

    uint32_t *dataPtr = &ret->data[0];
    for(int y = 0; y < ret->height; y++)
    {
        jpeg_read_scanlines(&jpegInfo, row, 1);
        uint8_t *jpegPtr = row[0];
        for(int x = 0; x < ret->width; x++, jpegPtr += 3)
        {
            *dataPtr++ = (0xFF << 24 | jpegPtr[2] << 16 | jpegPtr[1] << 8 | jpegPtr[0]);
        }
    }

    jpeg_finish_decompress(&jpegInfo);
    jpeg_destroy_decompress(&jpegInfo);

    free(row[0]);
    free(row);

    return ret;
}

void texDestroy(tex *t)
{
    if(t->data != NULL)
        free(t->data);

    if(t != NULL)
        free(t);
}

void texClearColor(tex *in, const color c)
{
    uint32_t *dataPtr = &in->data[0];
    for(int i = 0; i < in->width * in->height; i++)
        *dataPtr++ = colorGetColor(c);
}

void texDraw(const tex *t, tex *target, int x, int y)
{
    if(t->data != NULL)
    {
        color dataClr, fbClr;
        uint32_t *dataPtr = &t->data[0];
        for(int tY = y; tY < y + t->height; tY++)
        {
            uint32_t *rowPtr = &target->data[tY * target->width + x];
            for(int tX = x; tX < x + t->width; tX++, rowPtr++)
            {
                colorCreateFromU32(&dataClr, *dataPtr++);
                colorCreateFromU32(&fbClr, *rowPtr);

                *rowPtr = blend(dataClr, fbClr);
            }
        }
    }
}

void texDrawNoAlpha(const tex *t, tex *target, int x, int y)
{
    if(t->data != NULL)
    {
        uint32_t *dataPtr = &t->data[0];
        for(int tY = y; tY < y + t->height; tY++)
        {
            uint32_t *rowPtr = &target->data[tY * target->width + x];
            for(int tX = x; tX < x + t->width; tX++)
            {
                *rowPtr++ = *dataPtr++;
            }
        }
    }
}

void texDrawSkip(const tex *t, tex *target, int x, int y)
{
    if(t->data != NULL)
    {
        uint32_t *dataPtr = &t->data[0];
        color px1, px2, fbPx;
        for(int tY = y; tY < y + (t->height / 2); tY++, dataPtr += t->width)
        {
            uint32_t *rowPtr = &target->data[tY * target->width + x];
            for(int tX = x; tX < x + (t->width / 2); tX++, rowPtr++)
            {
                colorCreateFromU32(&px1, *dataPtr++);
                colorCreateFromU32(&px2, *dataPtr++);
                colorCreateFromU32(&fbPx, *rowPtr);

                *rowPtr = blend(colorCreateTemp(smooth(px1, px2)), fbPx);
            }
        }
    }
}

void texDrawSkipNoAlpha(const tex *t, tex *target, int x, int y)
{
    if(t->data != NULL)
    {
        uint32_t *dataPtr = &t->data[0];
        color px1, px2;
        for(int tY = y; tY < y + (t->height / 2); tY++, dataPtr += t->width)
        {
            uint32_t *rowPtr = &target->data[tY * target->width + x];
            for(int tX = x; tX < x + (t->width / 2); tX++, rowPtr++)
            {
                colorCreateFromU32(&px1, *dataPtr++);
                colorCreateFromU32(&px2, *dataPtr++);

                *rowPtr = smooth(px1, px2);
            }
        }
    }
}

void texDrawInvert(const tex *t, tex *target, int x, int y, bool alpha)
{
    if(t->data != NULL)
    {
        color dataClr, fbClr;
        uint32_t *dataPtr = &t->data[0];
        for(int tY = y; tY < y + t->height; tY++)
        {
            uint32_t *rowPtr = &target->data[tY * target->width + x];
            for(int tX = x; tX < x + t->width; tX++, rowPtr++)
            {
                colorCreateFromU32(&dataClr, *dataPtr++);
                colorInvert(&dataClr);
                if(alpha)
                    colorCreateFromU32(&fbClr, *rowPtr);

                *rowPtr = alpha ? blend(dataClr, fbClr) : colorGetColor(dataClr);
            }
        }
    }
}

void texSwapColors(tex *t, const color old, const color newColor)
{
    uint32_t oldClr = colorGetColor(old), newClr = colorGetColor(newColor);

    uint32_t *dataPtr = &t->data[0];
    for(unsigned i = 0; i < t->size; i++, dataPtr++)
    {
        if(*dataPtr == oldClr)
            *dataPtr = newClr;
    }

}

void texScaleToTex(const tex *in, tex *out, int scale)
{
    for(int y = 0; y < in->height; y++)
    {
        for(int tY = y * scale; tY < (y * scale) + scale; tY++)
        {
            uint32_t *inPtr = &in->data[y * in->width];
            for(int x = 0; x < in->width; x++, inPtr++)
            {
                for(int tX = x * scale; tX < (x * scale) + scale; tX++)
                {
                    out->data[tY * (in->width * scale) + tX] = *inPtr;
                }
            }
        }
    }
}

void texDrawDirect(const tex *in, int x, int y)
{
    uint32_t *fb = (uint32_t *)gfxGetFramebuffer(NULL, NULL);

    uint32_t *dataPtr = &in->data[0];
    for(int _y = y; _y < y + in->height; _y++)
    {
        uint32_t *rowPtr = &fb[_y * fbw + x];
        for(int _x = x; _x < x + in->width; _x++)
        {
            *rowPtr++ = *dataPtr++;
        }
    }
}

font *fontLoadSharedFont(PlSharedFontType fontType)
{
    PlFontData plFont;

    if(R_FAILED(plGetSharedFontByType(&plFont, fontType)))
        return NULL;

    font *ret = malloc(sizeof(font));

    if((ret->libRet = FT_Init_FreeType(&ret->lib)))
    {
        free(ret);
        return NULL;
    }

    if((ret->faceRet = FT_New_Memory_Face(ret->lib, plFont.address, plFont.size, 0, &ret->face)))
    {
        free(ret);
        return NULL;
    }

    ret->fntData = NULL;

    return ret;
}

font *fontLoadTTF(const char *path)
{
    font *ret = malloc(sizeof(font));
    if((ret->libRet = FT_Init_FreeType(&ret->lib)))
    {
        free(ret);
        return NULL;
    }

    FILE *ttf = fopen(path, "rb");
    fseek(ttf, 0, SEEK_END);
    size_t ttfSize = ftell(ttf);
    fseek(ttf, 0, SEEK_SET);

    ret->fntData = malloc(ttfSize);
    fread(ret->fntData, 1, ttfSize, ttf);
    fclose(ttf);

    if((ret->faceRet = FT_New_Memory_Face(ret->lib, ret->fntData, ttfSize, 0, &ret->face)))
    {
        free(ret->fntData);
        free(ret);
        return NULL;
    }

    return ret;
}

void fontDestroy(font *f)
{
    if(f->faceRet == 0)
        FT_Done_Face(f->face);
    if(f->libRet == 0)
        FT_Done_FreeType(f->lib);
    if(f->fntData != NULL)
        free(f->fntData);

    free(f);
}

tex *texGetFramebuffer()
{
    return frameBuffer;
}
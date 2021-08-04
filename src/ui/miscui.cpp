#include <cstring>
#include <cstdarg>
#include <cmath>
#include <switch.h>

#include "gfx.h"
#include "ui.h"
#include "miscui.h"
#include "util.h"
#include "type.h"

static const SDL_Color divLight  = {0x6D, 0x6D, 0x6D, 0xFF};
static const SDL_Color divDark   = {0xCC, 0xCC, 0xCC, 0xFF};
static const SDL_Color shadow    = {0x66, 0x66, 0x66, 0xFF};

static const SDL_Color fillBack  = {0x66, 0x66, 0x66, 0xFF};
static const SDL_Color fillBar   = {0x00, 0xDD, 0x00, 0xFF};

static const SDL_Color menuColorLight = {0x32, 0x50, 0xF0, 0xFF};
static const SDL_Color menuColorDark  = {0x00, 0xFF, 0xC5, 0xFF};

void ui::menu::setParams(const unsigned& _x, const unsigned& _y, const unsigned& _rW, const unsigned& _fS, const unsigned& _mL)
{
    x = _x;
    mY = _y;
    y = _y;
    tY = _y;
    rW = _rW;
    rY = _y;
    fSize = _fS;
    mL = _mL;
    rH = _fS + 32;
}

void ui::menu::editParam(int _param, unsigned newVal)
{
    switch(_param)
    {
        case MENU_X:
            x = newVal;
            break;

        case MENU_Y:
            y = newVal;
            break;

        case MENU_RECT_WIDTH:
            rW = newVal;
            break;

        case MENU_FONT_SIZE:
            fSize = newVal;
            break;

        case MENU_MAX_SCROLL:
            mL = newVal;
            break;
    }
}

int ui::menu::addOpt(SDL_Texture *_icn, const std::string& add)
{
    ui::menuOpt newOpt;
    if((int)gfx::getTextWidth(add.c_str(), fSize) < rW - 56 || rW == 0)
        newOpt.txt = add;
    else
    {
        std::string tmp;
        for(unsigned i = 0; i < add.length(); )
        {
            uint32_t tmpChr = 0;
            ssize_t untCnt = decode_utf8(&tmpChr, (uint8_t *)&add.c_str()[i]);

            tmp += add.substr(i, untCnt);
            i += untCnt;
            if((int)gfx::getTextWidth(tmp.c_str(), fSize) >= rW - 56)
            {
                newOpt.txt = tmp;
                break;
            }
        }
    }
    newOpt.icn = _icn;
    opt.push_back(newOpt);
    return opt.size() - 1;
}

void ui::menu::editOpt(int ind, SDL_Texture *_icn, const std::string& ch)
{
    if(!opt[ind].icn && _icn)
        opt[ind].icn = _icn;

    opt[ind].txt = ch;
}

void ui::menu::optAddButtonEvent(unsigned _ind, HidNpadButton _button, funcPtr _func, void *args)
{
    ui::menuOptEvent newEvent = {_func, args, _button};
    opt[_ind].events.push_back(newEvent);
}

int ui::menu::getOptPos(const std::string& txt)
{
    for(unsigned i = 0; i < opt.size(); i++)
    {
        if(opt[i].txt == txt)
            return i;
    }
    return -1;
}


ui::menu::~menu()
{
    opt.clear();
}

void ui::menu::update()
{
    if(!isActive)
        return;

    uint64_t down = ui::padKeysDown();
    uint64_t held = ui::padKeysHeld();

    if(held & HidNpadButton_AnyUp || held & HidNpadButton_AnyDown)
        ++fc;
    else
        fc = 0;

    if(fc > 10)
        fc = 0;

    int oldSel = selected;
    int mSize = opt.size() - 1, scrL = mL - 1;
    if( (down & HidNpadButton_AnyUp) || ((held & HidNpadButton_AnyUp) && fc == 10) )
    {
        if(selected > 0)
            --selected;
    }
    else if( (down & HidNpadButton_AnyDown) || ((held & HidNpadButton_AnyDown) && fc == 10))
    {
        if(selected < mSize)
            ++selected;
    }
    else if(down & HidNpadButton_AnyLeft)
    {
        selected -= mL;
        if(selected < 0)
            selected = 0;
    }
    else if(down & HidNpadButton_AnyRight)
    {
        selected += mL;
        if(selected > mSize)
            selected = mSize;
    }
    if(down && !opt[selected].events.empty())
    {
        for(ui::menuOptEvent& e : opt[selected].events)
        {
            if((down & e.button) && e.func)
                (*e.func)(e.args);
        }
    }

    if(selected <= mL)
        tY = mY;
    else if(selected >= (mSize - mL) && mSize > mL * 2)
        tY = mY + -(rH * (mSize - (mL * 2)));
    else if(selected > mL && selected < (mSize - mL))
        tY = -(rH * (selected - mL));

    if(selected != oldSel && onChange)
        (*onChange)(NULL);

    if(callback)
        (*callback)(callbackArgs);
}

void ui::menu::draw(SDL_Texture *target, const SDL_Color *textClr, bool drawText)
{
    if(opt.size() < 1)
        return;

    int tH = 0;
    SDL_QueryTexture(target, NULL, NULL, NULL, &tH);

    if(y != tY)
    {
        float add = (float)((float)tY - (float)y) / ui::animScale;
        y += ceil(add);
    }

    if(clrAdd)
    {
        clrSh += 6;
        if(clrSh >= 0x72)
            clrAdd = false;
    }
    else
    {
        clrSh -= 3;
        if(clrSh <= 0x00)
            clrAdd = true;
    }

    for(int i = 0, tY = y; i < (int)opt.size(); i++, tY += rH)
    {
        if(tY < -rH || tY > tH)
            continue;

        if(i == selected)
        {
            if(isActive)
                ui::drawBoundBox(target, x, y + (i * rH), rW, rH, clrSh);

            gfx::drawRect(target, ui::thmID == ColorSetId_Light ? &menuColorLight : &menuColorDark, x + 10, ((y + (rH / 2 - fSize / 2)) + (i * rH)) - 2, 4, fSize + 4);
            if(drawText)
                gfx::drawTextf(target, fSize, x + 20, (y + (rH / 2 - fSize / 2)) + (i * rH), ui::thmID == ColorSetId_Light ? &menuColorLight : &menuColorDark, opt[i].txt.c_str());
        }
        else
        {
            if(drawText)
                gfx::drawTextf(target, fSize, x + 20, (y + (rH / 2 - fSize / 2)) + (i * rH), textClr, opt[i].txt.c_str());
        }

        int w, h;
        if(opt[i].icn && SDL_QueryTexture(opt[i].icn, NULL, NULL, &w, &h) == 0)
        {
            float scale = (float)fSize / (float)h;
            int dW = scale * w;
            int dH = scale * h;

            gfx::texDrawStretch(target, opt[i].icn, x + 20, (y + (rH / 2 - fSize / 2)) + (i * rH), dW, dH);
        }
    }
}

void ui::menu::reset()
{
    opt.clear();
    selected = 0;
    y = mY;
    tY = mY;
    fc = 0;
}

void ui::menu::setActive(bool _set)
{
    isActive = _set;
}

void ui::progBar::update(const uint64_t& _prog)
{
    prog = _prog;

    if(max > 0)
        width = (float)((float)prog / (float)max) * 648.0f;
}

void ui::progBar::draw(const std::string& text)
{
    char progStr[64];
    sprintf(progStr, "%.2fMB / %.2fMB", (float)prog / 1024.0f / 1024.0f, (float)max / 1024.0f / 1024.0f);
    int progX = 640 - (gfx::getTextWidth(progStr, 18) / 2);

    ui::drawTextbox(NULL, 280, 262, 720, 256);
    gfx::drawLine(NULL, &ui::rectSh, 280, 454, 999, 454);
    gfx::drawRect(NULL, &fillBack, 312, 471, 648, 32);
    gfx::drawRect(NULL, &fillBar, 312, 471, (int)width, 32);
    gfx::drawTextf(NULL, 18, progX, 478, &ui::txtCont, progStr);
    gfx::drawTextfWrap(NULL, 16, 312, 288, 648, &ui::txtCont, text.c_str());
}

ui::popMessageMngr::~popMessageMngr()
{
    message.clear();
}

void ui::popMessageMngr::update()
{
    //Anything that needs graphics/text must be processed on main thread
    for(ui::popMessage& p : popQueue)
    {
        p.rectWidth = gfx::getTextWidth(p.message.c_str(), 24) + 32;
        message.push_back(p);
    }
    popQueue.clear();

    for(unsigned i = 0; i < message.size(); i++)
    {
        message[i].frames--;
        if(message[i].frames <= 0)
            message.erase(message.begin() + i);
    }
}

void ui::popMessageMngr::popMessageAdd(const std::string& mess, int frameTime)
{
    ui::popMessage newPop = {mess, 0, frameTime};
    popQueue.push_back(newPop);
}

void ui::popMessageMngr::draw()
{
    int y = 640;
    for(auto& p : message)
    {
        y -= 48;
        if(p.y != y)
            p.y += (y - p.y) / ui::animScale;

        gfx::drawRect(NULL, &ui::tboxClr, 64, p.y, p.rectWidth, 40);
        gfx::drawTextf(NULL, 24, 80, p.y + 8, &ui::txtCont, p.message.c_str());
    }
}

ui::confirmArgs *ui::confirmArgsCreate(bool _hold, funcPtr _func, void *_funcArgs, bool _cleanup, const char *fmt, ...)
{
    char tmp[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf(tmp, fmt, args);
    va_end(args);

    ui::confirmArgs *ret = new confirmArgs;
    ret->hold = _hold;
    ret->func = _func;
    ret->args = _funcArgs;
    ret->cleanup = _cleanup;
    ret->text = tmp;

    return ret;
}

//todo: more inline with original
void confirm_t(void *a)
{
    threadInfo *t = (threadInfo *)a;
    ui::confirmArgs *c = (ui::confirmArgs *)t->argPtr;

    while(true)
    {
        uint64_t down = ui::padKeysDown();
        uint64_t held = ui::padKeysHeld();

        if((down & HidNpadButton_A) && !c->hold)
        {
            ui::newThread(c->func, c->args, NULL);
            break;
        }
        else if((held & HidNpadButton_A) && c->hold)
        {
            if(c->frameCount % 4 == 0 && ++c->lgFrame > 7)
                c->lgFrame = 0;

            if(c->frameCount >= 120)
            {
                ui::newThread(c->func, c->args, NULL);
                break;
            }
        }
        else if(down & HidNpadButton_B)
            break;

        svcSleepThread(10000000);//Close enough
    }
    if(c->cleanup)
        delete c;

    t->finished = true;
}

//For now this will work
void confirmDrawFunc(void *a)
{
    threadInfo *t = (threadInfo *)a;
    ui::confirmArgs *c = (ui::confirmArgs *)t->argPtr;
    if(!t->finished)
    {
        std::string yesTxt = ui::yt;
        unsigned yesX = 0;

        if((ui::padKeysHeld() & HidNpadButton_A) && c->hold)
        {
            c->frameCount++;
            if(c->frameCount <= 40)
                yesTxt = ui::holdingText[0];
            else if(c->frameCount <= 80)
                yesTxt = ui::holdingText[1];
            else if(c->frameCount <= 120)
                yesTxt = ui::holdingText[2];

            yesTxt += ui::loadGlyphArray[c->lgFrame];
        }
        else
            c->frameCount = 0;


        //I positioned these by eye. They're probably off.
        yesX = 458 - (gfx::getTextWidth(yesTxt.c_str(), 18) / 2);
        ui::drawTextbox(NULL, 280, 262, 720, 256);
        gfx::drawTextfWrap(NULL, 16, 312, 288, 656, &ui::txtCont, c->text.c_str());
        gfx::drawLine(NULL, &ui::rectSh, 280, 454, 999, 454);
        gfx::drawLine(NULL, &ui::rectSh, 640, 454, 640, 518);
        gfx::drawTextf(NULL, 18, yesX, 478, &ui::txtCont, yesTxt.c_str());
        gfx::drawTextf(NULL, 18, 782, 478, &ui::txtCont, ui::nt.c_str());
    }
}

void ui::confirm(void *a)
{
    ui::newThread(confirm_t, a, confirmDrawFunc);
}

void ui::drawTextbox(SDL_Texture *target, int x, int y, int w, int h)
{
    //Top
    gfx::texDraw(target, ui::cornerTopLeft, x, y);
    gfx::drawRect(target, &ui::tboxClr, x + 32, y, w - 64, 32);
    gfx::texDraw(target, ui::cornerTopRight, (x + w) - 32, y);

    //middle
    gfx::drawRect(target, &ui::tboxClr, x, y + 32,  w, h - 64);

    //bottom
    gfx::texDraw(target, ui::cornerBottomLeft, x, (y + h) - 32);
    gfx::drawRect(target, &ui::tboxClr, x + 32, (y + h) - 32, w - 64, 32);
    gfx::texDraw(target, ui::cornerBottomRight, (x + w) - 32, (y + h) - 32);
}

void ui::drawBoundBox(SDL_Texture *target, int x, int y, int w, int h, uint8_t clrSh)
{
    SDL_SetRenderTarget(gfx::render, target);
    SDL_Color rectClr;

    if(ui::thmID == ColorSetId_Light)
        rectClr = {0xFD, 0xFD, 0xFD, 0xFF};
    else
        rectClr = {0x21, 0x22, 0x21, 0xFF};

    gfx::drawRect(target, &rectClr, x + 4, y + 4, w - 8, h - 8);

    rectClr = {0x00, (uint8_t)(0x88 + clrSh), (uint8_t)(0xC5 + (clrSh / 2)), 0xFF};

    SDL_SetTextureColorMod(mnuTopLeft, rectClr.r, rectClr.g, rectClr.b);
    SDL_SetTextureColorMod(mnuTopRight, rectClr.r, rectClr.g, rectClr.b);
    SDL_SetTextureColorMod(mnuBotLeft, rectClr.r, rectClr.g, rectClr.b);
    SDL_SetTextureColorMod(mnuBotRight, rectClr.r, rectClr.g, rectClr.b);

    //top
    gfx::texDraw(target, mnuTopLeft, x, y);
    gfx::drawRect(target, &rectClr, x + 4, y, w - 8, 4);
    gfx::texDraw(target, mnuTopRight, (x + w) - 4, y);

    //mid
    gfx::drawRect(target, &rectClr, x, y + 4, 4, h - 8);
    gfx::drawRect(target, &rectClr, (x + w) - 4, y + 4, 4, h - 8);

    //bottom
    gfx::texDraw(target, mnuBotLeft, x, (y + h) - 4);
    gfx::drawRect(target, &rectClr, x + 4, (y + h) - 4, w - 8, 4);
    gfx::texDraw(target, mnuBotRight, (x + w) - 4, (y + h) - 4);
}

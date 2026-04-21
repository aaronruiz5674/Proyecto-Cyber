#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define SCREEN_W    1920   // Cambia a tu resolución
#define SCREEN_H    1080
#define IMG_W       150
#define IMG_H       150
#define MAX_IMAGES  20
#define SECRET_ANSWER "gato"

// ── Hook global del ratón ─────────────────────────────────────────────────────
static volatile bool g_clicked = false;
static HHOOK g_mouseHook = NULL;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_LBUTTONDOWN)
        g_clicked = true;
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

// ── Imagen rebotante ──────────────────────────────────────────────────────────
typedef struct {
    float x, y, vx, vy;
    bool active;
} BouncingImg;

// ── Popup ─────────────────────────────────────────────────────────────────────
typedef struct {
    bool visible;
    char input[64];
    int  len;
    bool wrong;
} Popup;

void drawText(SDL_Renderer *r, TTF_Font *f, const char *t, int x, int y, SDL_Color c) {
    if (!f || !t || !t[0]) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(f, t, c);
    if (!s) return;
    SDL_Texture *tx = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect d = {x, y, s->w, s->h};
    SDL_RenderCopy(r, tx, NULL, &d);
    SDL_DestroyTexture(tx);
    SDL_FreeSurface(s);
}

void drawPopup(SDL_Renderer *r, TTF_Font *f, Popup *p) {
    if (!p->visible) return;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_Rect overlay = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(r, &overlay);

    // Caja centrada
    int bx = SCREEN_W/2 - 220, by = SCREEN_H/2 - 100;
    SDL_Rect box = {bx, by, 440, 200};
    SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
    SDL_RenderDrawRect(r, &box);

    SDL_Color black = {20,20,20,255};
    SDL_Color red   = {200,0,0,255};
    SDL_Color gray  = {100,100,100,255};

    drawText(r, f, "¿Cual es el animal favorito?", bx+70, by+20, black);
    drawText(r, f, "Responde correctamente para continuar.", bx+30, by+55, gray);

    SDL_Rect inp = {bx+20, by+95, 400, 38};
    SDL_SetRenderDrawColor(r, 255,255,255,255);
    SDL_RenderFillRect(r, &inp);
    SDL_SetRenderDrawColor(r, 100,100,200,255);
    SDL_RenderDrawRect(r, &inp);

    char display[70];
    snprintf(display, sizeof(display), "%s|", p->input);
    drawText(r, f, display, bx+28, by+100, black);

    if (p->wrong)
        drawText(r, f, "Incorrecto. Intentalo de nuevo.", bx+80, by+148, red);
    else
        drawText(r, f, "ENTER para confirmar", bx+140, by+148, gray);
}

int main(void) {
    srand((unsigned)time(NULL));

    // Instalar hook global ANTES de SDL
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    

    // ── Ventana transparente, sin bordes, siempre encima ─────────────────────
    SDL_Window *win = SDL_CreateWindow("",
        0, 0, SCREEN_W, SCREEN_H,
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_ALWAYS_ON_TOP |
        SDL_WINDOW_SKIP_TASKBAR);          // no aparece en la barra de tareas

    // Hacer la ventana click-through EXCEPTO cuando el popup está activo
    // Usamos WinAPI para poner la ventana encima de todo
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(win, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    // Siempre en primer plano y transparente al ratón inicialmente
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, SCREEN_W, SCREEN_H,
                 SWP_NOMOVE | SWP_NOSIZE);

    // Ventana con transparencia por color clave (negro puro = transparente)
    SetWindowLong(hwnd, GWL_EXSTYLE,
        GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // Recursos
    SDL_Texture *imgTex = NULL;
    SDL_Surface *surf = IMG_Load("goat.png");
    if (surf) { imgTex = SDL_CreateTextureFromSurface(ren, surf); SDL_FreeSurface(surf); }

    Mix_Music *music = Mix_LoadMUS("Balada.wav");
    if (music) Mix_PlayMusic(music, -1);

    TTF_Font *font = TTF_OpenFont("font.ttf", 20);
    if (!font) font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 20);

    // Estado
    BouncingImg imgs[MAX_IMAGES] = {0};
    int imgCount = 0;
    Popup popup = {false, "", 0, false};
    bool running = true;
    bool popupMode = false; // cuando el popup está activo, ventana recibe clicks

    SDL_Event e;
    SDL_StartTextInput();

    while (running) {
        // Procesar mensajes de Windows para que el hook funcione
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // ── Click global detectado → nueva imagen ────────────────────────────
        if (g_clicked && !popup.visible) {
            g_clicked = false;
            if (imgCount < MAX_IMAGES) {
                // Posición aleatoria
                imgs[imgCount].x  = (float)(rand() % (SCREEN_W - IMG_W));
                imgs[imgCount].y  = (float)(rand() % (SCREEN_H - IMG_H));
                // Velocidad aleatoria con dirección aleatoria
                float spd = 2.5f + (rand() % 30) / 10.0f;
                float ang = (float)(rand() % 360) * 3.14159f / 180.0f;
                imgs[imgCount].vx = cosf(ang) * spd;
                imgs[imgCount].vy = sinf(ang) * spd;
                imgs[imgCount].active = true;
                imgCount++;
            }
        }

        // ── Eventos SDL ──────────────────────────────────────────────────────
        while (SDL_PollEvent(&e)) {
            // BLOQUEAR cierre total — solo se puede salir por la pregunta
            if (e.type == SDL_QUIT) { /* ignorado */ }

            if (e.type == SDL_KEYDOWN) {
                SDL_Keymod mod = SDL_GetModState();

                // CTRL+Q → abrir popup
                if (e.key.keysym.sym == SDLK_q && (mod & KMOD_CTRL) && !popup.visible) {
                    popup.visible = true;
                    popup.len = 0;
                    popup.input[0] = '\0';
                    popup.wrong = false;
                    popupMode = true;

                    // Quitar WS_EX_TRANSPARENT para recibir clicks en el popup
                    SetWindowLong(hwnd, GWL_EXSTYLE,
                        GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
                }

                if (popup.visible) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && popup.len > 0) {
                        popup.input[--popup.len] = '\0';
                        popup.wrong = false;
                    }
                    if (e.key.keysym.sym == SDLK_RETURN) {
                        if (strcmp(popup.input, SECRET_ANSWER) == 0) {
                            running = false;
                        } else {
                            popup.wrong = true;
                            popup.len = 0;
                            popup.input[0] = '\0';
                        }
                    }
                }
            }

            if (e.type == SDL_TEXTINPUT && popup.visible) {
                int add = (int)strlen(e.text.text);
                if (popup.len + add < 63) {
                    strcat(popup.input, e.text.text);
                    popup.len += add;
                    popup.wrong = false;
                }
            }
        }

        // ── Física de rebote ─────────────────────────────────────────────────
        for (int i = 0; i < imgCount; i++) {
            if (!imgs[i].active) continue;
            imgs[i].x += imgs[i].vx;
            imgs[i].y += imgs[i].vy;
            if (imgs[i].x <= 0)             { imgs[i].x = 0;              imgs[i].vx =  fabsf(imgs[i].vx); }
            if (imgs[i].x >= SCREEN_W-IMG_W){ imgs[i].x = SCREEN_W-IMG_W; imgs[i].vx = -fabsf(imgs[i].vx); }
            if (imgs[i].y <= 0)             { imgs[i].y = 0;              imgs[i].vy =  fabsf(imgs[i].vy); }
            if (imgs[i].y >= SCREEN_H-IMG_H){ imgs[i].y = SCREEN_H-IMG_H; imgs[i].vy = -fabsf(imgs[i].vy); }
        }

        // ── Render ───────────────────────────────────────────────────────────
        // Negro puro = transparente (por LWA_COLORKEY)
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        for (int i = 0; i < imgCount; i++) {
            if (!imgs[i].active) continue;
            SDL_Rect dst = {(int)imgs[i].x, (int)imgs[i].y, IMG_W, IMG_H};
            if (imgTex) {
                SDL_RenderCopy(ren, imgTex, NULL, &dst);
            } else {
                SDL_SetRenderDrawColor(ren, 220, 50, 50, 255);
                SDL_RenderFillRect(ren, &dst);
            }
        }

        drawPopup(ren, font, &popup);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    // Limpieza
    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
    SDL_StopTextInput();
    if (imgTex) SDL_DestroyTexture(imgTex);
    if (music)  { Mix_HaltMusic(); Mix_FreeMusic(music); }
    if (font)   TTF_CloseFont(font);
    Mix_CloseAudio();
    TTF_Quit(); IMG_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
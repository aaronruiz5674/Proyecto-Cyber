// dependencias
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// resolución de la pantalla (anchura, altura)
#define SCREEN_W    1920
#define SCREEN_H    1080

// tamaño de la imagen (anchura,, altura)
#define IMG_W       150
#define IMG_H       150
#define MAX_IMAGES  20

// palabra secreta a adivinar para cerrar el programa
#define SECRET_ANSWER "vinicius"

// funciones para ajustar el volumen del sistema
DWORD WINAPI volumeWatcherThread(LPVOID lpParam) {
    CoInitialize(NULL);

    IMMDeviceEnumerator *pEnum = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;

    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                     &IID_IMMDeviceEnumerator, (void**)&pEnum);
    if (FAILED(hr) || !pEnum) { CoUninitialize(); return 1; }

    hr = pEnum->lpVtbl->GetDefaultAudioEndpoint(pEnum, eRender, eConsole, &pDevice);
    if (FAILED(hr) || !pDevice) { pEnum->lpVtbl->Release(pEnum); CoUninitialize(); return 1; }

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioEndpointVolume,
                               CLSCTX_ALL, NULL, (void**)&pVolume);
    if (FAILED(hr) || !pVolume) { pDevice->lpVtbl->Release(pDevice); pEnum->lpVtbl->Release(pEnum); CoUninitialize(); return 1; }

    while (1) {
        pVolume->lpVtbl->SetMasterVolumeLevelScalar(pVolume, 0.05f, NULL);
        pVolume->lpVtbl->SetMute(pVolume, FALSE, NULL);
        Sleep(200);
    }

    pVolume->lpVtbl->Release(pVolume);
    pDevice->lpVtbl->Release(pDevice);
    pEnum->lpVtbl->Release(pEnum);
    CoUninitialize();
    return 0;
}

// funciones para detectar clicks del ratón
static volatile bool g_clicked = false;
static HHOOK g_mouseHook = NULL;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_LBUTTONDOWN)
        g_clicked = true;
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

// atributos de la imagen (para que rebote)
typedef struct {
    float x, y, vx, vy;
    float angle;
    float va;
    bool active;
} BouncingImg;

typedef struct {
    bool visible;
    char input[64];
    int  len;
    bool wrong;
} Popup;

// funciones para dibujar el texto 
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

    int bx = SCREEN_W/2 - 220, by = SCREEN_H/2 - 100;
    SDL_Rect box = {bx, by, 440, 200};
    SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
    SDL_RenderFillRect(r, &box);

    SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
    SDL_RenderDrawRect(r, &box);

    SDL_Color black = {20,20,20,255};
    SDL_Color red   = {200,0,0,255};
    SDL_Color gray  = {100,100,100,255};

    // pregunta a responder 
    drawText(r, f, "¿Cual es el mejor del mundo?", bx+70, by+20, black);
    drawText(r, f, "Responde correctamente para parar el virus.", bx+30, by+55, gray);

    SDL_Rect inp = {bx+20, by+95, 400, 38};
    SDL_SetRenderDrawColor(r, 255,255,255,255);
    SDL_RenderFillRect(r, &inp);

    SDL_SetRenderDrawColor(r, 100,100,200,255);
    SDL_RenderDrawRect(r, &inp);

    char display[70];
    snprintf(display, sizeof(display), "%s|", p->input);
    drawText(r, f, display, bx+28, by+100, black);

    // si la respuesta es incorrecta mostrar mensaje de error
    if (p->wrong)
        drawText(r, f, "Incorrecto. Intentalo de nuevo.", bx+80, by+148, red);
    else
        drawText(r, f, "ENTER para confirmar", bx+140, by+148, gray);
}

// función principal
int main(void) {
    srand((unsigned)time(NULL));

    // iniciar el hilo 
    CreateThread(NULL, 0, volumeWatcherThread, NULL, 0, NULL);

    // detectar clicks del ratón 
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    // crear ventana sin bordes, siempre encima, y que no aparezca en la barra de tareas
    SDL_Window *win = SDL_CreateWindow("",
        0, 0, SCREEN_W, SCREEN_H,
        SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_ALWAYS_ON_TOP |
        SDL_WINDOW_SKIP_TASKBAR);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(win, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, SCREEN_W, SCREEN_H,
                 SWP_NOMOVE | SWP_NOSIZE);

    // ── Color clave: negro casi puro (1,1,1) para que el fondo sea transparente
    SetWindowLong(hwnd, GWL_EXSTYLE,
        GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(hwnd, RGB(1,1,1), 0, LWA_COLORKEY);

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    // cargar imagen del balón de playa
    SDL_Texture *imgTex = NULL;
    SDL_Surface *surf = IMG_Load("goat.png");
    if (surf) { imgTex = SDL_CreateTextureFromSurface(ren, surf); SDL_FreeSurface(surf); }

    // cargar canción 
    Mix_Music *music = Mix_LoadMUS("Balada.wav");
    if (music) {
        Mix_PlayMusic(music, -1);
        Mix_VolumeMusic(MIX_MAX_VOLUME);
    }

    // atributos del texto en pantalla ...
    TTF_Font *font = TTF_OpenFont("font.ttf", 20);
    if (!font) font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 20);

    BouncingImg imgs[MAX_IMAGES] = {0};
    int imgCount = 0;

    Popup popup = {false, "", 0, false};
    bool running = true;

    SDL_Event e;
    SDL_StartTextInput();

    while (running) {

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_clicked && !popup.visible) {
            g_clicked = false;

            if (imgCount < MAX_IMAGES) {
                imgs[imgCount].x = rand() % (SCREEN_W - IMG_W);
                imgs[imgCount].y = rand() % (SCREEN_H - IMG_H);

                float spd = 2.5f + (rand() % 30) / 10.0f;
                float ang = (rand() % 360) * 3.14159f / 180.0f;

                imgs[imgCount].vx = cosf(ang) * spd;
                imgs[imgCount].vy = sinf(ang) * spd;

                imgs[imgCount].angle = 0;
                imgs[imgCount].va = ((rand() % 200) / 100.0f) - 3.0f;

                imgs[imgCount].active = true;
                imgCount++;
            }
        }

        while (SDL_PollEvent(&e)) {

            if (e.type == SDL_KEYDOWN) {
                SDL_Keymod mod = SDL_GetModState();

                if (e.key.keysym.sym == SDLK_q && (mod & KMOD_CTRL) && !popup.visible) {
                    popup.visible = true;
                    popup.len = 0;
                    popup.input[0] = '\0';
                    popup.wrong = false;

                    // ── Quitar transparencia para que el popup sea clickeable ──
                    SetWindowLong(hwnd, GWL_EXSTYLE,
                        GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);

                    // ── Quitar color clave para que el fondo opaco se vea ─────
                    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
                }

                if (popup.visible) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && popup.len > 0)
                        popup.input[--popup.len] = '\0';

                    if (e.key.keysym.sym == SDLK_RETURN) {
                        if (strcmp(popup.input, SECRET_ANSWER) == 0)
                            running = false;
                        else {
                            popup.wrong = true;
                            popup.len = 0;
                            popup.input[0] = '\0';
                        }
                    }
                }
            }

            if (e.type == SDL_TEXTINPUT && popup.visible) {
                strcat(popup.input, e.text.text);
                popup.len += strlen(e.text.text);
            }
        }

        // actualizar posición de las imágenes
        for (int i = 0; i < imgCount; i++) {
            if (!imgs[i].active) continue;

            imgs[i].x += imgs[i].vx;
            imgs[i].y += imgs[i].vy;

            imgs[i].angle += imgs[i].va;

            if (imgs[i].angle > 15 || imgs[i].angle < -15)
                imgs[i].va *= -1;

            if (imgs[i].x <= 0 || imgs[i].x >= SCREEN_W - IMG_W)
                imgs[i].vx *= -1;

            if (imgs[i].y <= 0 || imgs[i].y >= SCREEN_H - IMG_H)
                imgs[i].vy *= -1;
        }

        // ── Fondo: (1,1,1) = transparente, no negro puro ─────────────────
        if (popup.visible)
            SDL_SetRenderDrawColor(ren, 30, 30, 30, 255); // fondo oscuro visible
        else
            SDL_SetRenderDrawColor(ren, 1, 1, 1, 255);    // transparente

        SDL_RenderClear(ren);

        for (int i = 0; i < imgCount; i++) {
            SDL_Rect dst = {(int)imgs[i].x, (int)imgs[i].y, IMG_W, IMG_H};

            if (imgTex) {
                SDL_RenderCopyEx(
                    ren,
                    imgTex,
                    NULL,
                    &dst,
                    imgs[i].angle,
                    NULL,
                    SDL_FLIP_NONE
                );
            }
        }

        drawPopup(ren, font, &popup);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);

    SDL_StopTextInput();
    SDL_DestroyTexture(imgTex);
    Mix_HaltMusic();
    Mix_FreeMusic(music);
    TTF_CloseFont(font);

    Mix_CloseAudio();
    TTF_Quit(); IMG_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
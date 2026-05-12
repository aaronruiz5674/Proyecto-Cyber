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

// tamaño de la imagen (anchura, altura)
#define IMG_W       150
#define IMG_H       150
#define MAX_IMAGES  20

// Dimensiones del popup
#define POPUP_W     480
#define POPUP_H     220

// palabra secreta a adivinar para cerrar el programa
#define SECRET_ANSWER "mbappe"

// =============================================================================
// Hilo que fuerza el volumen del sistema al 5%
// =============================================================================
DWORD WINAPI volumeWatcherThread(LPVOID lpParam) {
    CoInitialize(NULL);

    IMMDeviceEnumerator  *pEnum   = NULL;
    IMMDevice            *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnum);
    if (FAILED(hr) || !pEnum) { CoUninitialize(); return 1; }

    hr = pEnum->lpVtbl->GetDefaultAudioEndpoint(pEnum, eRender, eConsole, &pDevice);
    if (FAILED(hr) || !pDevice) { pEnum->lpVtbl->Release(pEnum); CoUninitialize(); return 1; }

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioEndpointVolume,
                                   CLSCTX_ALL, NULL, (void**)&pVolume);
    if (FAILED(hr) || !pVolume) {
        pDevice->lpVtbl->Release(pDevice);
        pEnum->lpVtbl->Release(pEnum);
        CoUninitialize();
        return 1;
    }

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

// =============================================================================
// Hook global de raton para detectar clics (spawneo de imagenes)
// =============================================================================
static volatile bool g_clicked   = false;
static HHOOK         g_mouseHook = NULL;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_LBUTTONDOWN)
        g_clicked = true;
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

// =============================================================================
// Estructuras
// =============================================================================
typedef struct {
    float x, y, vx, vy;
    float angle, va;
    bool  active;
} BouncingImg;

typedef struct {
    bool visible;
    char input[64];
    int  len;
    bool wrong;
} Popup;

// =============================================================================
// Helpers de renderizado
// =============================================================================
void drawText(SDL_Renderer *r, TTF_Font *f, const char *t, int x, int y, SDL_Color c) {
    if (!f || !t || !t[0]) return;
    SDL_Surface *s  = TTF_RenderUTF8_Blended(f, t, c);
    if (!s) return;
    SDL_Texture *tx = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect     d  = {x, y, s->w, s->h};
    SDL_RenderCopy(r, tx, NULL, &d);
    SDL_DestroyTexture(tx);
    SDL_FreeSurface(s);
}

// Dibuja el popup en su propio renderer (ventana separada con bordes de Windows)
void drawPopupWindow(SDL_Renderer *r, TTF_Font *f, Popup *p) {
    // Fondo claro
    SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
    SDL_RenderClear(r);

    SDL_Color black = {20,  20,  20,  255};
    SDL_Color red   = {200, 0,   0,   255};
    SDL_Color gray  = {100, 100, 100, 255};

    drawText(r, f, "¿Quién es el responsable del problema del Real Madrid?",       20, 20,  black);
    drawText(r, f, "Responde correctamente para parar el virus.", 10, 55,  gray);

    // Campo de texto
    SDL_Rect inp = {10, 95, POPUP_W - 20, 40};
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderFillRect(r, &inp);
    SDL_SetRenderDrawColor(r, 100, 100, 200, 255);
    SDL_RenderDrawRect(r, &inp);

    char display[70];
    snprintf(display, sizeof(display), "%s|", p->input);
    drawText(r, f, display, 18, 103, black);

    if (p->wrong)
        drawText(r, f, "Incorrecto. Intentalo de nuevo.", 90, 155, red);
    else
        drawText(r, f, "Pulsa ENTER para confirmar",      110, 155, gray);

    SDL_RenderPresent(r);
}

// =============================================================================
// main
// =============================================================================
int main(void) {
    srand((unsigned)time(NULL));

    // Iniciar hilo de volumen
    CreateThread(NULL, 0, volumeWatcherThread, NULL, 0, NULL);

    // Hook de raton
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);

    // ── SDL init ──────────────────────────────────────────────────────────────
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    // ── Ventana principal: sin bordes, siempre encima, sin taskbar ───────────
    // Esta es la capa de imágenes flotantes. Tiene WS_EX_TRANSPARENT, por lo
    // que los clics la atraviesan y llegan al escritorio → el PC es totalmente
    // usable mientras las imágenes rebotan por encima.
    SDL_Window *win = SDL_CreateWindow("",
        0, 0, SCREEN_W, SCREEN_H,
        SDL_WINDOW_BORDERLESS      |
        SDL_WINDOW_ALWAYS_ON_TOP   |
        SDL_WINDOW_SKIP_TASKBAR);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(win, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, SCREEN_W, SCREEN_H,
                 SWP_NOMOVE | SWP_NOSIZE);

    // LAYERED + TRANSPARENT: visible pero no intercepta clics del raton
    DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    // Color clave (1,1,1): los píxeles de ese color se vuelven transparentes
    SetLayeredWindowAttributes(hwnd, RGB(1, 1, 1), 0, LWA_COLORKEY);

    // ── Ventana del popup: ventana NORMAL con bordes de Windows ──────────────
    // Aparece como una ventana más en la barra de tareas, se puede mover,
    // tiene título y recibe el foco de teclado directamente.
    SDL_Window *popupWin = SDL_CreateWindow(
        "Alerta del sistema",
        SCREEN_W / 2 - POPUP_W / 2,
        SCREEN_H / 2 - POPUP_H / 2,
        POPUP_W, POPUP_H,
        SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_INPUT_FOCUS);

    SDL_SysWMinfo popupWMInfo;
    SDL_VERSION(&popupWMInfo.version);
    SDL_GetWindowWMInfo(popupWin, &popupWMInfo);
    HWND popupHwnd = popupWMInfo.info.win.window;
    SetWindowPos(popupHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // ── Renderers ─────────────────────────────────────────────────────────────
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    SDL_Renderer *popupRen = SDL_CreateRenderer(popupWin, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // ── Assets ────────────────────────────────────────────────────────────────
    SDL_Texture *imgTex = NULL;
    SDL_Surface *surf   = IMG_Load("goat.png");
    if (surf) { imgTex = SDL_CreateTextureFromSurface(ren, surf); SDL_FreeSurface(surf); }

    Mix_Music *music = Mix_LoadMUS("Balada.wav");
    if (music) {
        Mix_PlayMusic(music, -1);
        Mix_VolumeMusic(MIX_MAX_VOLUME);
    }

    TTF_Font *font = TTF_OpenFont("font.ttf", 20);
    if (!font) font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 20);

    // ── Estado ────────────────────────────────────────────────────────────────
    BouncingImg imgs[MAX_IMAGES] = {0};
    int imgCount = 0;

    Popup popup = {true, "", 0, false};

    bool running = true;
    SDL_Event e;
    SDL_StartTextInput();

    // Imágenes iniciales visibles desde el arranque
    for (int i = 0; i < 6; i++) {
        imgs[imgCount].x  = (float)(rand() % (SCREEN_W - IMG_W));
        imgs[imgCount].y  = (float)(rand() % (SCREEN_H - IMG_H));
        float spd = 2.5f + (rand() % 30) / 10.0f;
        float ang = (rand() % 360) * 3.14159f / 180.0f;
        imgs[imgCount].vx     = cosf(ang) * spd;
        imgs[imgCount].vy     = sinf(ang) * spd;
        imgs[imgCount].angle  = 0;
        imgs[imgCount].va     = ((rand() % 200) / 100.0f) - 3.0f;
        imgs[imgCount].active = true;
        imgCount++;
    }

    // ── Bucle principal ───────────────────────────────────────────────────────
    while (running) {

        // Mensajes Win32 para que el hook de raton funcione
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Spawneo extra de imagenes con clic
        // El hook captura todos los clics aunque la ventana principal sea
        // WS_EX_TRANSPARENT, así que esto sigue funcionando.
        if (g_clicked) {
            g_clicked = false;
            if (imgCount < MAX_IMAGES) {
                imgs[imgCount].x  = (float)(rand() % (SCREEN_W - IMG_W));
                imgs[imgCount].y  = (float)(rand() % (SCREEN_H - IMG_H));
                float spd = 2.5f + (rand() % 30) / 10.0f;
                float ang = (rand() % 360) * 3.14159f / 180.0f;
                imgs[imgCount].vx     = cosf(ang) * spd;
                imgs[imgCount].vy     = sinf(ang) * spd;
                imgs[imgCount].angle  = 0;
                imgs[imgCount].va     = ((rand() % 200) / 100.0f) - 3.0f;
                imgs[imgCount].active = true;
                imgCount++;
            }
        }

        // Eventos SDL — el popup recibe el foco de teclado
        while (SDL_PollEvent(&e)) {

            // Ignorar intento de cerrar el popup con la X
            if (e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_CLOSE &&
                SDL_GetWindowFromID(e.window.windowID) == popupWin) {
                // No hacemos nada: la X no cierra el programa
            }

            if (e.type == SDL_KEYDOWN && popup.visible) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && popup.len > 0)
                    popup.input[--popup.len] = '\0';

                if (e.key.keysym.sym == SDLK_RETURN) {
                    if (strcmp(popup.input, SECRET_ANSWER) == 0) {
                        running = false;
                    } else {
                        popup.wrong    = true;
                        popup.len      = 0;
                        popup.input[0] = '\0';
                    }
                }
            }

            if (e.type == SDL_TEXTINPUT && popup.visible) {
                int addLen = (int)strlen(e.text.text);
                if (popup.len + addLen < 63) {
                    strcat(popup.input, e.text.text);
                    popup.len += addLen;
                }
            }
        }

        // Actualizar imagenes rebotantes
        for (int i = 0; i < imgCount; i++) {
            if (!imgs[i].active) continue;
            imgs[i].x     += imgs[i].vx;
            imgs[i].y     += imgs[i].vy;
            imgs[i].angle += imgs[i].va;

            if (imgs[i].angle >  15 || imgs[i].angle < -15) imgs[i].va *= -1;
            if (imgs[i].x <= 0      || imgs[i].x >= SCREEN_W - IMG_W)  imgs[i].vx *= -1;
            if (imgs[i].y <= 0      || imgs[i].y >= SCREEN_H - IMG_H)  imgs[i].vy *= -1;
        }

        // ── Render capa de imágenes (ventana transparente) ────────────────────
        // Fondo = color clave (1,1,1) → se vuelve invisible
        // Solo se ven las imágenes flotando sobre el escritorio
        SDL_SetRenderDrawColor(ren, 1, 1, 1, 255);
        SDL_RenderClear(ren);

        for (int i = 0; i < imgCount; i++) {
            if (!imgs[i].active) continue;
            SDL_Rect dst = {(int)imgs[i].x, (int)imgs[i].y, IMG_W, IMG_H};
            if (imgTex)
                SDL_RenderCopyEx(ren, imgTex, NULL, &dst,
                                 imgs[i].angle, NULL, SDL_FLIP_NONE);
        }

        SDL_RenderPresent(ren);

        // ── Render popup (ventana separada con bordes normales de Windows) ────
        if (popup.visible)
            drawPopupWindow(popupRen, font, &popup);

        SDL_Delay(16);
    }

    // ── Limpieza ──────────────────────────────────────────────────────────────
    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);

    SDL_StopTextInput();
    if (imgTex) SDL_DestroyTexture(imgTex);
    Mix_HaltMusic();
    if (music)  Mix_FreeMusic(music);
    if (font)   TTF_CloseFont(font);

    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_DestroyRenderer(popupRen);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(popupWin);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
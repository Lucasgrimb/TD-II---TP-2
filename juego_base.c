#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ========= CONSTANTES DEL JUEGO =========
#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 500

#define GRID_OFFSET_X 220
#define GRID_OFFSET_Y 59
#define GRID_WIDTH 650
#define GRID_HEIGHT 425

#define GRID_ROWS 5
#define GRID_COLS 9
#define CELL_WIDTH (GRID_WIDTH / GRID_COLS)
#define CELL_HEIGHT (GRID_HEIGHT / GRID_ROWS)

#define PEASHOOTER_FRAME_WIDTH 177
#define PEASHOOTER_FRAME_HEIGHT 166
#define PEASHOOTER_TOTAL_FRAMES 31
#define PEASHOOTER_ANIMATION_SPEED 4
#define PEASHOOTER_SHOOT_FRAME 18

#define ZOMBIE_FRAME_WIDTH 164
#define ZOMBIE_FRAME_HEIGHT 203
#define ZOMBIE_TOTAL_FRAMES 90
#define ZOMBIE_ANIMATION_SPEED 2
#define ZOMBIE_DISTANCE_PER_CYCLE 40.0f

#define MAX_ARVEJAS 100
#define MAX_ZOMBIES 20
#define PEA_SPEED 5
#define ZOMBIE_SPAWN_RATE 300

// ========= ESTRUCTURAS DE DATOS =========
typedef struct {
    int row, col;
} Cursor;

typedef struct {
    SDL_Rect rect;
    int activo;
    int cooldown;
    int current_frame;
    int frame_timer;
    int debe_disparar;
} Planta;

typedef struct {
    SDL_Rect rect;
    int activo;
} Arveja;

typedef struct {
    SDL_Rect rect;
    int activo;
    int vida;
    int row;
    int current_frame;
    int frame_timer;
    float pos_x;
} Zombie;

// ========= VARIABLES GLOBALES DEL JUEGO =========
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* tex_background = NULL;
SDL_Texture* tex_peashooter_sheet = NULL;
SDL_Texture* tex_zombie_sheet = NULL;
SDL_Texture* tex_pea = NULL;

Cursor cursor = {0, 0};
Planta grid[GRID_ROWS][GRID_COLS] = {0};
Arveja arvejas[MAX_ARVEJAS] = {0};
Zombie zombies[MAX_ZOMBIES] = {0};
int zombie_spawn_timer = ZOMBIE_SPAWN_RATE;

// ========= FUNCIONES =========

SDL_Texture* cargarTextura(const char* path) {
    SDL_Texture* newTexture = IMG_LoadTexture(renderer, path);
    if (newTexture == NULL) printf("No se pudo cargar la textura %s! SDL_image Error: %s\n", path, IMG_GetError());
    return newTexture;
}

int inicializar() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 0;
    window = SDL_CreateWindow("Plantas vs Zombies - Base para TP", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) return 0;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) return 0;
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) return 0;
    tex_background = cargarTextura("res/Frontyard.png");
    tex_peashooter_sheet = cargarTextura("res/peashooter_sprite_sheet.png");
    tex_zombie_sheet = cargarTextura("res/zombie_sprite_sheet.png");
    tex_pea = cargarTextura("res/pea.png");
    return 1;
}

void cerrar() {
    SDL_DestroyTexture(tex_background);
    SDL_DestroyTexture(tex_peashooter_sheet);
    SDL_DestroyTexture(tex_zombie_sheet);
    SDL_DestroyTexture(tex_pea);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}


void colocarPlanta() {
    if (!grid[cursor.row][cursor.col].activo) {
        Planta* p = &grid[cursor.row][cursor.col];
        p->rect.x = GRID_OFFSET_X + (cursor.col * CELL_WIDTH);
        p->rect.y = GRID_OFFSET_Y + (cursor.row * CELL_HEIGHT);
        p->rect.w = CELL_WIDTH;
        p->rect.h = CELL_HEIGHT;
        p->activo = 1;
        p->cooldown = rand() % 100;
        p->current_frame = 0;
        p->frame_timer = 0;
        p->debe_disparar = 0;
    }
}

void dispararArveja(int row, int col) {
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (!arvejas[i].activo) {
            arvejas[i].rect.x = grid[row][col].rect.x + (CELL_WIDTH / 2);
            arvejas[i].rect.y = grid[row][col].rect.y + (CELL_HEIGHT / 4);
            arvejas[i].rect.w = 20;
            arvejas[i].rect.h = 20;
            arvejas[i].activo = 1;
            break;
        }
    }
}

void generarZombie() {
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        if (!zombies[i].activo) {
            Zombie* z = &zombies[i];
            z->row = rand() % GRID_ROWS;
            z->pos_x = SCREEN_WIDTH;
            z->rect.x = (int)z->pos_x;
            z->rect.y = GRID_OFFSET_Y + (z->row * CELL_HEIGHT);
            z->rect.w = CELL_WIDTH;
            z->rect.h = CELL_HEIGHT;
            z->vida = 100;
            z->activo = 1;
            z->current_frame = 0;
            z->frame_timer = 0;
            break;
        }
    }
}

void actualizarEstado() {
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        if (zombies[i].activo) {
            Zombie* z = &zombies[i];
            float distance_per_tick = ZOMBIE_DISTANCE_PER_CYCLE / (float)(ZOMBIE_TOTAL_FRAMES * ZOMBIE_ANIMATION_SPEED);
            z->pos_x -= distance_per_tick;
            z->rect.x = (int)z->pos_x;
            z->frame_timer++;
            if (z->frame_timer >= ZOMBIE_ANIMATION_SPEED) {
                z->frame_timer = 0;
                z->current_frame = (z->current_frame + 1) % ZOMBIE_TOTAL_FRAMES;
            }
        }
    }

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (grid[r][c].activo) {
                Planta* p = &grid[r][c];
                if (p->cooldown <= 0) {
                    p->debe_disparar = 1;
                } else {
                    p->cooldown--;
                }
                p->frame_timer++;
                if (p->frame_timer >= PEASHOOTER_ANIMATION_SPEED) {
                    p->frame_timer = 0;
                    p->current_frame = (p->current_frame + 1) % PEASHOOTER_TOTAL_FRAMES;
                    if (p->debe_disparar && p->current_frame == PEASHOOTER_SHOOT_FRAME) {
                        dispararArveja(r, c);
                        p->cooldown = 120;
                        p->debe_disparar = 0;
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (arvejas[i].activo) {
            arvejas[i].rect.x += PEA_SPEED;
            if (arvejas[i].rect.x > SCREEN_WIDTH) arvejas[i].activo = 0;
        }
    }
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        if (!zombies[i].activo) continue;
        for (int j = 0; j < MAX_ARVEJAS; j++) {
            if (!arvejas[j].activo) continue;
            int arveja_row = (arvejas[j].rect.y - GRID_OFFSET_Y) / CELL_HEIGHT;
            if (zombies[i].row == arveja_row) {
                if (SDL_HasIntersection(&arvejas[j].rect, &zombies[i].rect)) {
                    arvejas[j].activo = 0;
                    zombies[i].vida -= 25;
                    if (zombies[i].vida <= 0) zombies[i].activo = 0;
                }
            }
        }
    }

    zombie_spawn_timer--;
    if (zombie_spawn_timer <= 0) {
        generarZombie();
        zombie_spawn_timer = ZOMBIE_SPAWN_RATE;
    }
}

void dibujar() {
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex_background, NULL, NULL);

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (grid[r][c].activo) {
                Planta* p = &grid[r][c];
                SDL_Rect src_rect = { p->current_frame * PEASHOOTER_FRAME_WIDTH, 0, PEASHOOTER_FRAME_WIDTH, PEASHOOTER_FRAME_HEIGHT };
                SDL_RenderCopy(renderer, tex_peashooter_sheet, &src_rect, &p->rect);
            }
        }
    }
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (arvejas[i].activo) SDL_RenderCopy(renderer, tex_pea, NULL, &arvejas[i].rect);
    }
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        if (zombies[i].activo) {
            Zombie* z = &zombies[i];
            SDL_Rect src_rect = { z->current_frame * ZOMBIE_FRAME_WIDTH, 0, ZOMBIE_FRAME_WIDTH, ZOMBIE_FRAME_HEIGHT };
            SDL_RenderCopy(renderer, tex_zombie_sheet, &src_rect, &z->rect);
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
    SDL_Rect cursor_rect = {GRID_OFFSET_X + cursor.col * CELL_WIDTH, GRID_OFFSET_Y + cursor.row * CELL_HEIGHT, CELL_WIDTH, CELL_HEIGHT};
    SDL_RenderDrawRect(renderer, &cursor_rect);

    SDL_RenderPresent(renderer);
}

int main(int argc, char* args[]) {
    srand(time(NULL));
    if (!inicializar()) return 1;

    SDL_Event e;
    int game_over = 0;

    while (!game_over) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) game_over = 1;
            if (e.type == SDL_MOUSEMOTION) {
                int mouse_x = e.motion.x;
                int mouse_y = e.motion.y;
                if (mouse_x >= GRID_OFFSET_X && mouse_x < GRID_OFFSET_X + GRID_WIDTH &&
                    mouse_y >= GRID_OFFSET_Y && mouse_y < GRID_OFFSET_Y + GRID_HEIGHT) {
                    cursor.col = (mouse_x - GRID_OFFSET_X) / CELL_WIDTH;
                    cursor.row = (mouse_y - GRID_OFFSET_Y) / CELL_HEIGHT;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                colocarPlanta();
            }
        }

        actualizarEstado();
        dibujar();

        for (int i = 0; i < MAX_ZOMBIES; i++) {
            if (zombies[i].activo && zombies[i].rect.x < GRID_OFFSET_X - zombies[i].rect.w) {
                printf("GAME OVER - Un zombie llego a tu casa!\n");
                game_over = 1;
                break;
            }
        }

        SDL_Delay(16);
    }

    cerrar();
    return 0;
}

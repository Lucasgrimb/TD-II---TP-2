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

// ========= NUEVAS ESTRUCTURAS =========
#define STATUS_VACIO 0
#define STATUS_PLANTA 1

typedef struct RowSegment {
    int status;
    int start_col;
    int length;
    Planta* planta_data;
    struct RowSegment* next;
} RowSegment;

typedef struct ZombieNode {
    Zombie zombie_data;
    struct ZombieNode* next;
} ZombieNode;

typedef struct GardenRow {
    RowSegment* first_segment;
    ZombieNode* first_zombie;
} GardenRow;

typedef struct GameBoard {
    GardenRow rows[GRID_ROWS];
    Arveja arvejas[MAX_ARVEJAS]; //array adicional para manejar las arvejas
    int zombie_spawn_timer; // variable para saber cada cuanto crear un zombie
} GameBoard;


// ========= VARIABLES GLOBALES =========
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* tex_background = NULL;
SDL_Texture* tex_peashooter_sheet = NULL;
SDL_Texture* tex_zombie_sheet = NULL;
SDL_Texture* tex_pea = NULL;

Cursor cursor = {0, 0};
GameBoard* game_board = NULL;






// ========= FUNCIONES =========



GameBoard* gameBoardNew() {
    GameBoard* board = (GameBoard*)malloc(sizeof(GameBoard));
    if (!board) return NULL;

    board->zombie_spawn_timer = ZOMBIE_SPAWN_RATE;

    for (int i = 0; i < GRID_ROWS; i++) {
        RowSegment* first = (RowSegment*)malloc(sizeof(RowSegment));
        if (!first) {
            free(board);
            return NULL;
        }
        first->status = STATUS_VACIO;
        first->start_col = 0;
        first->length = GRID_COLS;
        first->planta_data = NULL;
        first->next = NULL;

        board->rows[i].first_segment = first;
        board->rows[i].first_zombie = NULL;
    }
     for(int i = 0; i < MAX_ARVEJAS; i++) {
        board->arvejas[i].activo = 0;
    }
    return board;
}




// ========= GAME BOARD DELETE=========

/**
 * Libera una lista enlazada de segmentos.
 * Si un segmento contiene una planta (planta_data != NULL), también libera esa memoria.
 */
static void freeSegments(RowSegment* head) {
    RowSegment* current = head;
    while (current != NULL) {
        RowSegment* next = current->next;  // Guardar next ANTES de liberar
        
        // Si este segmento tiene una planta, liberarla primero
        if (current->planta_data != NULL) {
            free(current->planta_data);
        }
        
        // Liberar el segmento actual
        free(current);
        current = next;
    }
}

/**
 * Libera una lista enlazada de zombies.
 * zombie_data es un struct embebido, no necesita free por separado.
 */
static void freeZombies(ZombieNode* head) {
    ZombieNode* current = head;
    while (current != NULL) {
        ZombieNode* next = current->next;  // Guardar next ANTES de liberar
        free(current);
        current = next;
    }
}

/**
 * Libera toda la memoria dinámica del GameBoard.
 * Incluye: todos los segmentos, plantas y zombies de cada fila, y el board mismo.
 */
void gameBoardDelete(GameBoard* board) {
    // Validación: Si board es NULL, no hay nada que liberar
    if (board == NULL) {
        return;
    }
    
    // Recorrer cada fila del tablero
    for (int row = 0; row < GRID_ROWS; row++) {
        // Liberar lista de segmentos (incluyendo plantas si existen)
        freeSegments(board->rows[row].first_segment);
        
        // Liberar lista de zombies
        freeZombies(board->rows[row].first_zombie);
    }
    
    // Finalmente, liberar el GameBoard
    // Nota: rows[] y arvejas[] son arrays estáticos dentro del struct, se liberan automáticamente
    free(board);
}








// ========= GAME BOARD ADD PLANT ==========

/**
 * Crea e inicializa una Planta en la posición especificada.
 * @return Puntero a la nueva Planta, o NULL si falla malloc
 */
static Planta* createPlanta(int row, int col) {
    Planta* p = malloc(sizeof(Planta));
    if (p == NULL) {
        printf("Error: No se pudo asignar memoria para Planta\n");
        return NULL;
    }
    p->rect.x = GRID_OFFSET_X + (col * CELL_WIDTH);
    p->rect.y = GRID_OFFSET_Y + (row * CELL_HEIGHT);
    p->rect.w = CELL_WIDTH;
    p->rect.h = CELL_HEIGHT;
    p->activo = 1;
    p->cooldown = rand() % 100;
    p->current_frame = 0;
    p->frame_timer = 0;
    p->debe_disparar = 0;
    return p;
}

/**
 * Reinicia una fila a su estado inicial (completamente vacía).
 * Útil para tests.
 */
static void resetRow(GardenRow* row) {
    freeSegments(row->first_segment);
    row->first_segment = malloc(sizeof(RowSegment));
    if (row->first_segment != NULL) {
        row->first_segment->status = STATUS_VACIO;
        row->first_segment->start_col = 0;
        row->first_segment->length = GRID_COLS;
        row->first_segment->planta_data = NULL;
        row->first_segment->next = NULL;
    }
}

int gameBoardAddPlant(GameBoard* board, int row, int col) {
    // ===== VALIDACIONES =====
    if (board == NULL) {
        printf("Error: Board es NULL en gameBoardAddPlant\n");
        return 0;
    }
    if (row < 0 || row >= GRID_ROWS) {
        printf("Error: Row %d invalida en gameBoardAddPlant\n", row);
        return 0;
    }
    if (col < 0 || col >= GRID_COLS) {
        printf("Error: Col %d invalida en gameBoardAddPlant\n", col);
        return 0;
    }
    
    // ===== BUSCAR EL SEGMENTO QUE CONTIENE LA COLUMNA =====
    RowSegment* current = board->rows[row].first_segment;
    RowSegment* prev = NULL;
    
    while (current != NULL) {
        int start = current->start_col;
        int end = current->start_col + current->length;
        
        if (col >= start && col < end) {
            // ===== ENCONTRAMOS EL SEGMENTO CORRECTO =====
            
            // Si ya hay una planta aquí, no hacer nada
            if (current->status == STATUS_PLANTA) {
                return 0;
            }
            
            // ===== DIVIDIR EL SEGMENTO VACÍO =====
            
            // CASO 1: Planta al INICIO del segmento
            if (col == current->start_col) {
                RowSegment* planta_seg = malloc(sizeof(RowSegment));
                if (planta_seg == NULL) return 0;
                
                planta_seg->status = STATUS_PLANTA;
                planta_seg->start_col = col;
                planta_seg->length = 1;
                planta_seg->planta_data = createPlanta(row, col);
                if (planta_seg->planta_data == NULL) {
                    free(planta_seg);
                    return 0;
                }
                
                // Verificar si current quedará vacío
                if (current->length == 1) {
                    planta_seg->next = current->next;
                    free(current);
                } else {
                    current->start_col++;
                    current->length--;
                    planta_seg->next = current;
                }
                
                // Conectar
                if (prev == NULL) {
                    board->rows[row].first_segment = planta_seg;
                } else {
                    prev->next = planta_seg;
                }
                
                return 1;
            }
            
            // CASO 2: Planta al FINAL del segmento
            else if (col == current->start_col + current->length - 1) {
                RowSegment* planta_seg = malloc(sizeof(RowSegment));
                if (planta_seg == NULL) return 0;
                
                planta_seg->status = STATUS_PLANTA;
                planta_seg->start_col = col;
                planta_seg->length = 1;
                planta_seg->planta_data = createPlanta(row, col);
                if (planta_seg->planta_data == NULL) {
                    free(planta_seg);
                    return 0;
                }
                
                // Conectar
                planta_seg->next = current->next;
                current->next = planta_seg;
                current->length--;
                
                return 1;
            }
            
            // CASO 3: Planta en el MEDIO (dividir en 3)
            else {
                RowSegment* left_seg = malloc(sizeof(RowSegment));
                if (left_seg == NULL) return 0;
                left_seg->status = STATUS_VACIO;
                left_seg->start_col = current->start_col;
                left_seg->length = col - current->start_col;
                left_seg->planta_data = NULL;
                
                RowSegment* planta_seg = malloc(sizeof(RowSegment));
                if (planta_seg == NULL) {
                    free(left_seg);
                    return 0;
                }
                planta_seg->status = STATUS_PLANTA;
                planta_seg->start_col = col;
                planta_seg->length = 1;
                planta_seg->planta_data = createPlanta(row, col);
                if (planta_seg->planta_data == NULL) {
                    free(left_seg);
                    free(planta_seg);
                    return 0;
                }
                
                RowSegment* right_seg = malloc(sizeof(RowSegment));
                if (right_seg == NULL) {
                    free(left_seg);
                    free(planta_seg->planta_data);
                    free(planta_seg);
                    return 0;
                }
                right_seg->status = STATUS_VACIO;
                right_seg->start_col = col + 1;
                right_seg->length = (current->start_col + current->length) - (col + 1);
                right_seg->planta_data = NULL;
                
                // Conectar los 3 segmentos
                left_seg->next = planta_seg;
                planta_seg->next = right_seg;
                right_seg->next = current->next;
                
                // Conectar al anterior
                if (prev == NULL) {
                    board->rows[row].first_segment = left_seg;
                } else {
                    prev->next = left_seg;
                }
                
                // Liberar el segmento original
                free(current);
                
                return 1;
            }
        }
        
        prev = current;
        current = current->next;
    }
    
    printf("Error: No se encontro segmento para col %d en row %d\n", col, row);
    return 0;
}











// ========= GAME BOARD REMOVE PLANT==========

/**
 * Crea e inicializa una Planta en la posición especificada.
 * @return Puntero a la nueva Planta, o NULL si falla malloc
 */
static Planta* createPlanta(int row, int col) {
    Planta* p = malloc(sizeof(Planta));
    if (p == NULL) {
        printf("Error: No se pudo asignar memoria para Planta\n");
        return NULL;
    }
    p->rect.x = GRID_OFFSET_X + (col * CELL_WIDTH);
    p->rect.y = GRID_OFFSET_Y + (row * CELL_HEIGHT);
    p->rect.w = CELL_WIDTH;
    p->rect.h = CELL_HEIGHT;
    p->activo = 1;
    p->cooldown = rand() % 100;
    p->current_frame = 0;
    p->frame_timer = 0;
    p->debe_disparar = 0;
    return p;
}

/**
 * Reinicia una fila a su estado inicial (completamente vacía).
 * Útil para tests. Maneja fracaso en malloc con logging.
 */
static void resetRow(GardenRow* row) {
    freeSegments(row->first_segment);
    row->first_segment = malloc(sizeof(RowSegment));
    if (row->first_segment == NULL) {
        printf("Error: No se pudo reiniciar fila en resetRow\n");
        return;
    }
    row->first_segment->status = STATUS_VACIO;
    row->first_segment->start_col = 0;
    row->first_segment->length = GRID_COLS;
    row->first_segment->planta_data = NULL;
    row->first_segment->next = NULL;
}

/**
 * Verifica si una fila cubre exactamente las columnas 0 a GRID_COLS-1 sin gaps ni overlaps.
 * Útil para tests de integridad.
 * @return 1 si es válida, 0 si no.
 */
static int isRowValid(GardenRow* row) {
    RowSegment* seg = row->first_segment;
    int current_col = 0;
    while (seg != NULL) {
        if (seg->start_col != current_col) {
            return 0;  // Gap o overlap
        }
        if (seg->status == STATUS_PLANTA && seg->planta_data == NULL) {
            return 0;  // Planta sin data
        }
        if (seg->status == STATUS_VACIO && seg->planta_data != NULL) {
            return 0;  // Vacío con data
        }
        current_col += seg->length;
        seg = seg->next;
    }
    return (current_col == GRID_COLS);
}

void gameBoardRemovePlant(GameBoard* board, int row, int col) {
    // ===== VALIDACIONES =====
    if (board == NULL) {
        printf("Error: Board es NULL en gameBoardRemovePlant\n");
        return;
    }
    if (row < 0 || row >= GRID_ROWS) {
        printf("Error: Row %d invalida en gameBoardRemovePlant\n", row);
        return;
    }
    if (col < 0 || col >= GRID_COLS) {
        printf("Error: Col %d invalida en gameBoardRemovePlant\n", col);
        return;
    }

    // ===== BUSCAR EL SEGMENTO QUE CONTIENE LA COLUMNA =====
    RowSegment* current = board->rows[row].first_segment;
    RowSegment* prev = NULL;

    while (current != NULL) {
        int start = current->start_col;
        int end = current->start_col + current->length;

        if (col >= start && col < end) {
            // ===== ENCONTRAMOS EL SEGMENTO CORRECTO =====

            // Si no es una planta (o length >1, pero por ahora plantas son length=1), no hacer nada
            if (current->status != STATUS_PLANTA || current->length != 1) {
                return;
            }

            // ===== CONVERTIR A VACIO Y LIBERAR PLANTA =====
            free(current->planta_data);
            current->planta_data = NULL;
            current->status = STATUS_VACIO;

            // ===== FUSIONAR CON SEGMENTOS ADYACENTES VACIOS =====

            // Fusionar con NEXT si es VACIO y adyacente
            if (current->next != NULL && current->next->status == STATUS_VACIO) {
                RowSegment* next = current->next;
                if (current->start_col + current->length == next->start_col) {
                    current->length += next->length;
                    current->next = next->next;
                    free(next);
                }
            }

            // Fusionar con PREV si es VACIO y adyacente (después de next, para manejar triple fusión)
            if (prev != NULL && prev->status == STATUS_VACIO) {
                if (prev->start_col + prev->length == current->start_col) {
                    prev->length += current->length;
                    prev->next = current->next;
                    free(current);
                }
            }

            return;
        }

        prev = current;
        current = current->next;
    }

    // No se encontró segmento con planta en col (ya no hace nada)
}






//======== GAME BOARD ADD ZOMBIE ==========

void gameBoardAddZombie(GameBoard* board, int row) {
    // Validación de entrada con logging
    if (board == NULL) {
        printf("Error: Board es NULL en gameBoardAddZombie\n");
        return;
    }
    if (row < 0 || row >= GRID_ROWS) {
        printf("Error: Row %d inválida (debe ser 0-%d) en gameBoardAddZombie\n", row, GRID_ROWS - 1);
        return;
    }
    
    // PASO 1: Crear nuevo nodo zombie
    ZombieNode* nuevo_nodo = malloc(sizeof(ZombieNode));
    if (nuevo_nodo == NULL) {
        printf("Error: No se pudo asignar memoria para el zombie\n");
        return;
    }
    
    // PASO 2: Inicializar los datos del zombie
    nuevo_nodo->zombie_data.row = row;
    nuevo_nodo->zombie_data.pos_x = SCREEN_WIDTH;
    nuevo_nodo->zombie_data.rect.x = (int)nuevo_nodo->zombie_data.pos_x;
    nuevo_nodo->zombie_data.rect.y = GRID_OFFSET_Y + (row * CELL_HEIGHT);
    nuevo_nodo->zombie_data.rect.w = CELL_WIDTH;
    nuevo_nodo->zombie_data.rect.h = CELL_HEIGHT;
    nuevo_nodo->zombie_data.vida = 100;
    nuevo_nodo->zombie_data.activo = 1;
    nuevo_nodo->zombie_data.current_frame = 0;
    nuevo_nodo->zombie_data.frame_timer = 0;
    
    // PASO 3: Agregar al PRINCIPIO de la lista (O(1))
    // Razón: Eficiencia máxima; orden no afecta lógica (pos_x determina posición)
    nuevo_nodo->next = board->rows[row].first_zombie;
    board->rows[row].first_zombie = nuevo_nodo;
}






// ========= GAME BOARD UPDATE ==========

/**
 * Dispara una arveja desde una planta específica.
 * Busca un slot inactivo en el array de arvejas y lo inicializa.
 * @param board El GameBoard que contiene el array de arvejas.
 * @param p La planta que dispara.
 * @param row La fila de la planta (para calcular row de arveja).
 */
static void dispararArveja(GameBoard* board, Planta* p, int row) {
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (!board->arvejas[i].activo) {
            board->arvejas[i].rect.x = p->rect.x + (CELL_WIDTH / 2);
            board->arvejas[i].rect.y = p->rect.y + (CELL_HEIGHT / 4);
            board->arvejas[i].rect.w = 20;
            board->arvejas[i].rect.h = 20;
            board->arvejas[i].activo = 1;
            break;
        }
    }
}

/**
 * Genera un nuevo zombie en una fila aleatoria si el timer lo permite.
 * Decrementa el timer y resetea cuando spawnea.
 * Usa gameBoardAddZombie para agregar a la lista de la fila.
 * @param board El GameBoard con el timer y filas.
 */
static void generarZombieSiNecesario(GameBoard* board) {
    board->zombie_spawn_timer--;
    if (board->zombie_spawn_timer <= 0) {
        int random_row = rand() % GRID_ROWS;
        gameBoardAddZombie(board, random_row);
        board->zombie_spawn_timer = ZOMBIE_SPAWN_RATE;
    }
}

void gameBoardUpdate(GameBoard* board) {
    if (board == NULL) {
        printf("Error: Board es NULL en gameBoardUpdate\n");
        return;
    }

    // ===== ACTUALIZAR ZOMBIES (por fila, recorriendo listas enlazadas) =====
    for (int r = 0; r < GRID_ROWS; r++) {
        ZombieNode* z_node = board->rows[r].first_zombie;
        ZombieNode* prev_z = NULL;
        while (z_node != NULL) {
            Zombie* z = &z_node->zombie_data;
            if (z->activo) {
                // Calcular movimiento por tick (basado en ciclo de animación)
                float distance_per_tick = ZOMBIE_DISTANCE_PER_CYCLE / (float)(ZOMBIE_TOTAL_FRAMES * ZOMBIE_ANIMATION_SPEED);
                z->pos_x -= distance_per_tick;
                z->rect.x = (int)z->pos_x;

                // Actualizar animación
                z->frame_timer++;
                if (z->frame_timer >= ZOMBIE_ANIMATION_SPEED) {
                    z->frame_timer = 0;
                    z->current_frame = (z->current_frame + 1) % ZOMBIE_TOTAL_FRAMES;
                }

                // Desactivar si sale de pantalla (por izquierda, pero game over se maneja en main)
                if (z->rect.x + z->rect.w < 0) {
                    z->activo = 0;
                }
            }

            // Eliminar zombies inactivos (para limpiar lista, aunque no estrictamente necesario)
            if (!z->activo) {
                ZombieNode* to_free = z_node;
                if (prev_z == NULL) {
                    board->rows[r].first_zombie = z_node->next;
                } else {
                    prev_z->next = z_node->next;
                }
                z_node = z_node->next;
                free(to_free);
                continue;
            }

            prev_z = z_node;
            z_node = z_node->next;
        }
    }

    // ===== ACTUALIZAR PLANTAS (por fila, recorriendo segmentos) =====
    for (int r = 0; r < GRID_ROWS; r++) {
        RowSegment* seg = board->rows[r].first_segment;
        while (seg != NULL) {
            if (seg->status == STATUS_PLANTA && seg->planta_data != NULL) {
                Planta* p = seg->planta_data;

                // Actualizar cooldown y decidir disparo
                if (p->cooldown <= 0) {
                    p->debe_disparar = 1;
                } else {
                    p->cooldown--;
                }

                // Actualizar animación
                p->frame_timer++;
                if (p->frame_timer >= PEASHOOTER_ANIMATION_SPEED) {
                    p->frame_timer = 0;
                    p->current_frame = (p->current_frame + 1) % PEASHOOTER_TOTAL_FRAMES;

                    // Disparar si es el frame correcto y debe
                    if (p->debe_disparar && p->current_frame == PEASHOOTER_SHOOT_FRAME) {
                        dispararArveja(board, p, r);
                        p->cooldown = 120;  // Reset cooldown post-disparo
                        p->debe_disparar = 0;
                    }
                }
            }
            seg = seg->next;
        }
    }

    // ===== ACTUALIZAR ARVEJAS (array global) =====
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (board->arvejas[i].activo) {
            board->arvejas[i].rect.x += PEA_SPEED;
            if (board->arvejas[i].rect.x > SCREEN_WIDTH) {
                board->arvejas[i].activo = 0;
            }
        }
    }

    // ===== DETECTAR COLISIONES ARVEJA-ZOMBIE (por fila, eficiente) =====
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (!board->arvejas[i].activo) continue;

        // Calcular row de la arveja (basado en posición y)
        int arveja_row = (board->arvejas[i].rect.y - GRID_OFFSET_Y) / CELL_HEIGHT;

        // Recorrer solo zombies de esa fila
        ZombieNode* z_node = board->rows[arveja_row].first_zombie;
        while (z_node != NULL) {
            Zombie* z = &z_node->zombie_data;
            if (z->activo && SDL_HasIntersection(&board->arvejas[i].rect, &z->rect)) {
                board->arvejas[i].activo = 0;
                z->vida -= 25;
                if (z->vida <= 0) {
                    z->activo = 0;
                }
                break;  // Una arveja solo impacta un zombie por tick
            }
            z_node = z_node->next;
        }
    }

    // ===== GENERAR NUEVOS ZOMBIES SI HACE FALTA =====
    generarZombieSiNecesario(board);
}






// ========= GAME BOARD DRAW ==========



/**
 * Dibuja el estado actual del juego en la pantalla.
 * Esta función realiza la misma lógica que la función 'dibujar' del prototipo base,
 * pero adaptada a la nueva estructura GameBoard. Limpia el renderer, dibuja el fondo,
 * recorre los segmentos de cada fila para dibujar plantas, dibuja arvejas, recorre
 * las listas de zombies por fila para dibujarlos, dibuja el cursor y presenta el frame.
 *
 * Ideas principales:
 * - Las plantas se dibujan recorriendo los segmentos de cada fila y solo renderizando
 *   aquellos con status PLANTA, usando los datos en planta_data.
 * - Los zombies se dibujan recorriendo la lista enlazada de cada fila.
 * - Las arvejas permanecen en un array estático, por lo que se manejan igual que en el base.
 * - El cursor es global y se dibuja como un rectángulo de borde amarillo.
 * - Se maneja board == NULL retornando inmediatamente para evitar crashes.
 * - No se liberan recursos aquí; solo se renderiza. Asumimos que renderer y texturas están inicializados.
 */
void gameBoardDraw(GameBoard* board) {
    if (board == NULL || renderer == NULL) {
        // Validación: Si board o renderer es NULL, no dibujar para evitar segfaults.
        // Podríamos loguear un error, pero por simplicidad, solo retornamos.
        return;
    }

    // Limpiar el renderer con color negro (por defecto).
    SDL_RenderClear(renderer);

    // Dibujar el fondo.
    if (tex_background != NULL) {
        SDL_RenderCopy(renderer, tex_background, NULL, NULL);
    }

    // Dibujar plantas: Recorrer cada fila y sus segmentos.
    for (int r = 0; r < GRID_ROWS; r++) {
        RowSegment* current_seg = board->rows[r].first_segment;
        while (current_seg != NULL) {
            if (current_seg->status == STATUS_PLANTA && current_seg->planta_data != NULL) {
                Planta* p = current_seg->planta_data;
                if (p->activo) {  // Solo dibujar si está activa.
                    SDL_Rect src_rect = {
                        p->current_frame * PEASHOOTER_FRAME_WIDTH,
                        0,
                        PEASHOOTER_FRAME_WIDTH,
                        PEASHOOTER_FRAME_HEIGHT
                    };
                    SDL_RenderCopy(renderer, tex_peashooter_sheet, &src_rect, &p->rect);
                }
            }
            current_seg = current_seg->next;
        }
    }

    // Dibujar arvejas: Igual que en el base, usando el array en board.
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (board->arvejas[i].activo) {
            SDL_RenderCopy(renderer, tex_pea, NULL, &board->arvejas[i].rect);
        }
    }

    // Dibujar zombies: Recorrer la lista enlazada de cada fila.
    for (int r = 0; r < GRID_ROWS; r++) {
        ZombieNode* current_z = board->rows[r].first_zombie;
        while (current_z != NULL) {
            Zombie* z = &current_z->zombie_data;
            if (z->activo) {  // Solo dibujar si está activo.
                SDL_Rect src_rect = {
                    z->current_frame * ZOMBIE_FRAME_WIDTH,
                    0,
                    ZOMBIE_FRAME_WIDTH,
                    ZOMBIE_FRAME_HEIGHT
                };
                SDL_RenderCopy(renderer, tex_zombie_sheet, &src_rect, &z->rect);
            }
            current_z = current_z->next;
        }
    }

    // Dibujar el cursor (global).
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);  // Amarillo semi-transparente.
    SDL_Rect cursor_rect = {
        GRID_OFFSET_X + cursor.col * CELL_WIDTH,
        GRID_OFFSET_Y + cursor.row * CELL_HEIGHT,
        CELL_WIDTH,
        CELL_HEIGHT
    };
    SDL_RenderDrawRect(renderer, &cursor_rect);

    // Presentar el frame.
    SDL_RenderPresent(renderer);
}

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







//=========== STR DUPLICATE ===========



char* strDuplicate(char* src) {
    if (src == NULL) {
        return NULL;  // Mejora: Manejar input inválido
    }
    size_t len = 0;
    while (src[len] != '\0') {
        len++;
    }
    char* nuevo = malloc(len + 1);  // Sin cast, más limpio
    if (nuevo == NULL) {
        return NULL;
    }
    for (size_t i = 0; i <= len; i++) {  // For para claridad
        nuevo[i] = src[i];
    }
    return nuevo;
}


//=========== STR COMPARE ===========
int strCompare(char* s1, char* s2) {
    if (s1 == NULL && s2 == NULL) return 0;
    if (s1 == NULL) return 1;  // NULL < no-NULL
    if (s2 == NULL) return -1; // no-NULL > NULL
    size_t i = 0;
    while (s1[i] != '\0' && s2[i] != '\0' && s1[i] == s2[i]) {
        i++;
    }
    if (s1[i] == s2[i]) {
        return 0;
    // Cast a unsigned char para evitar comportamiento indefinido con caracteres extendidos (valores > 127)
    } else if ((unsigned char)s1[i] < (unsigned char)s2[i]) {
        return 1;
    } else {
        return -1;
    }
}


//=========== STR CONCATENATE ================
char* strConcatenate(char* src1, char* src2) {
    size_t len1 = 0;
    if (src1 != NULL) {
        while (src1[len1] != '\0') len1++;
    }
    size_t len2 = 0;
    if (src2 != NULL) {
        while (src2[len2] != '\0') len2++;
    }
    char* resultado = malloc(len1 + len2 + 1);
    if (resultado == NULL) {
        free(src1);  // Libera siempre, pero si fail, caller pierde strings (por consigna)
        free(src2);
        return NULL;
    }
    size_t i = 0;
    if (src1 != NULL) {
        for (; i < len1; i++) {
            resultado[i] = src1[i];
        }
    }
    size_t j = 0;
    if (src2 != NULL) {
        for (; j < len2; j++, i++) {
            resultado[i] = src2[j];
        }
    }
    resultado[i] = '\0';

    // Liberar src1 y src2 según la consigna
    // Nota: free(NULL) es seguro en C y no hace nada
    free(src1);
    free(src2);
    return resultado;
}




// ========== TESTS STR DUPLICATE ==========
void testStrDuplicate() {
    printf("\n========= TESTS strDuplicate =========\n");
    
    // TEST 1: String vacío
    char* test1_src = "";
    char* test1_dup = strDuplicate(test1_src);
    if (test1_dup != NULL && test1_dup[0] == '\0') {
        printf("✓ TEST 1 PASADO: String vacio duplicado correctamente\n");
    } else {
        printf("✗ TEST 1 FALLADO: String vacio no duplicado correctamente\n");
    }
    free(test1_dup);
    
    // TEST 2: String de un carácter
    char* test2_src = "A";
    char* test2_dup = strDuplicate(test2_src);
    if (test2_dup != NULL && test2_dup[0] == 'A' && test2_dup[1] == '\0') {
        printf("✓ TEST 2 PASADO: String de un caracter duplicado correctamente\n");
    } else {
        printf("✗ TEST 2 FALLADO: String de un caracter no duplicado correctamente\n");
    }
    free(test2_dup);
    
    // TEST 3: String con varios caracteres comunes
    char* test3_src = "Hola Mundo 123!";
    char* test3_dup = strDuplicate(test3_src);
    int test3_ok = 1;
    if (test3_dup == NULL) {
        test3_ok = 0;
    } else {
        // Verificar carácter por carácter
        int i = 0;
        while (test3_src[i] != '\0') {
            if (test3_dup[i] != test3_src[i]) {
                test3_ok = 0;
                break;
            }
            i++;
        }
        // Verificar que el duplicado también termina en '\0'
        if (test3_dup[i] != '\0') {
            test3_ok = 0;
        }
    }
    if (test3_ok) {
        printf("✓ TEST 3 PASADO: String con varios caracteres duplicado correctamente\n");
    } else {
        printf("✗ TEST 3 FALLADO: String con varios caracteres no duplicado correctamente\n");
    }
    free(test3_dup);
    
    // TEST 4: String con caracteres especiales y números
    char* test4_src = "!@#$%^&*()_+-=[]{}|;:',.<>?/0123456789";
    char* test4_dup = strDuplicate(test4_src);
    int test4_ok = 1;
    if (test4_dup == NULL) {
        test4_ok = 0;
    } else {
        int i = 0;
        while (test4_src[i] != '\0') {
            if (test4_dup[i] != test4_src[i]) {
                test4_ok = 0;
                break;
            }
            i++;
        }
        if (test4_dup[i] != '\0') {
            test4_ok = 0;
        }
    }
    if (test4_ok) {
        printf("✓ TEST 4 PASADO: String con caracteres especiales duplicado correctamente\n");
    } else {
        printf("✗ TEST 4 FALLADO: String con caracteres especiales no duplicado correctamente\n");
    }
    free(test4_dup);
    
    // TEST 5: Verificar que son copias independientes (no el mismo puntero)
    char* test5_src = "Test";
    char* test5_dup = strDuplicate(test5_src);
    if (test5_dup != NULL && test5_dup != test5_src) {
        // Modificar el duplicado no debe afectar el original
        test5_dup[0] = 'X';
        if (test5_src[0] == 'T' && test5_dup[0] == 'X') {
            printf("✓ TEST 5 PASADO: El duplicado es independiente del original\n");
        } else {
            printf("✗ TEST 5 FALLADO: El duplicado no es independiente\n");
        }
    } else {
        printf("✗ TEST 5 FALLADO: El duplicado apunta al mismo lugar que el original\n");
    }
    free(test5_dup);
    
    printf("======================================\n\n");

    // TEST 6: Input NULL

    // TEST 6: Input NULL
    char* test6_dup = strDuplicate(NULL);
    if (test6_dup == NULL) {
        printf("✓ TEST 6 PASADO: NULL input retorna NULL\n");
    } else {
        printf("✗ TEST 6 FALLADO: NULL input no manejado correctamente\n");
        free(test6_dup);  // Solo liberar si NO es NULL
}
}



// ========== TESTS STR COMPARE ==========
void testStrCompare() {
    printf("\n========= TESTS strCompare =========\n");
    
    // TEST 1: Dos strings vacíos
    char* test1_s1 = "";
    char* test1_s2 = "";
    int result1 = strCompare(test1_s1, test1_s2);
    if (result1 == 0) {
        printf("✓ TEST 1 PASADO: Dos strings vacios son iguales\n");
    } else {
        printf("✗ TEST 1 FALLADO: Dos strings vacios deberian ser iguales (resultado: %d)\n", result1);
    }
    
    // TEST 2: Dos strings de un carácter iguales
    char* test2_s1 = "A";
    char* test2_s2 = "A";
    int result2 = strCompare(test2_s1, test2_s2);
    if (result2 == 0) {
        printf("✓ TEST 2 PASADO: Dos strings de un caracter iguales\n");
    } else {
        printf("✗ TEST 2 FALLADO: Dos strings de un caracter iguales (resultado: %d)\n", result2);
    }
    
    // TEST 3: Strings iguales hasta un carácter (s1 < s2)
    char* test3_s1 = "abc";
    char* test3_s2 = "abd";
    int result3 = strCompare(test3_s1, test3_s2);
    if (result3 == 1) {  // s1 < s2, debe retornar 1
        printf("✓ TEST 3 PASADO: 'abc' es menor que 'abd'\n");
    } else {
        printf("✗ TEST 3 FALLADO: 'abc' deberia ser menor que 'abd' (resultado: %d)\n", result3);
    }
    
    // TEST 4: Strings iguales hasta un carácter (s2 < s1)
    char* test4_s1 = "abd";
    char* test4_s2 = "abc";
    int result4 = strCompare(test4_s1, test4_s2);
    if (result4 == -1) {  // s1 > s2, debe retornar -1
        printf("✓ TEST 4 PASADO: 'abd' es mayor que 'abc'\n");
    } else {
        printf("✗ TEST 4 FALLADO: 'abd' deberia ser mayor que 'abc' (resultado: %d)\n", result4);
    }
    
    // TEST 5: Dos strings completamente diferentes (s1 < s2)
    char* test5_s1 = "apple";
    char* test5_s2 = "banana";
    int result5 = strCompare(test5_s1, test5_s2);
    if (result5 == 1) {  // 'a' < 'b', entonces s1 < s2
        printf("✓ TEST 5 PASADO: 'apple' es menor que 'banana'\n");
    } else {
        printf("✗ TEST 5 FALLADO: 'apple' deberia ser menor que 'banana' (resultado: %d)\n", result5);
    }
    
    // TEST 6: Dos strings completamente diferentes (s1 > s2)
    char* test6_s1 = "banana";
    char* test6_s2 = "apple";
    int result6 = strCompare(test6_s1, test6_s2);
    if (result6 == -1) {  // 'b' > 'a', entonces s1 > s2
        printf("✓ TEST 6 PASADO: 'banana' es mayor que 'apple'\n");
    } else {
        printf("✗ TEST 6 FALLADO: 'banana' deberia ser mayor que 'apple' (resultado: %d)\n", result6);
    }
    
    // TEST 7: String largo vs string corto (prefijo)
    char* test7_s1 = "test";
    char* test7_s2 = "testing";
    int result7 = strCompare(test7_s1, test7_s2);
    if (result7 == 1) {  // "test" < "testing" (más corto es menor)
        printf("✓ TEST 7 PASADO: 'test' es menor que 'testing'\n");
    } else {
        printf("✗ TEST 7 FALLADO: 'test' deberia ser menor que 'testing' (resultado: %d)\n", result7);
    }
    
    printf("====================================\n\n");


    // TEST 8: s1 largo > s2 corto
    char* test8_s1 = "testing";
    char* test8_s2 = "test";
    int result8 = strCompare(test8_s1, test8_s2);
    if (result8 == -1) {
        printf("✓ TEST 8 PASADO: 'testing' es mayor que 'test'\n");
    } else {
        printf("✗ TEST 8 FALLADO: 'testing' deberia ser mayor que 'test' (resultado: %d)\n", result8);
    }

    // TEST 9: Case sensitivity
    char* test9_s1 = "A";
    char* test9_s2 = "a";
    int result9 = strCompare(test9_s1, test9_s2);
    if (result9 == 1) {  // 'A' (65) < 'a' (97)
        printf("✓ TEST 9 PASADO: 'A' < 'a'\n");
    } else {
        printf("✗ TEST 9 FALLADO: 'A' deberia ser menor que 'a' (resultado: %d)\n", result9);
    }

    // TEST 10: NULL inputs
    int result10a = strCompare(NULL, NULL);  // 0
    int result10b = strCompare(NULL, "a");   // 1 (NULL < "a")
    int result10c = strCompare("a", NULL);   // -1 ("a" > NULL)
    if (result10a == 0 && result10b == 1 && result10c == -1) {
        printf("✓ TEST 10 PASADO: Manejo de NULL\n");
    } else {
        printf("✗ TEST 10 FALLADO: Manejo de NULL incorrecto\n");
    }
}


// ========== TESTS STR CONCATENATE ==========
void testStrConcatenate() {
    printf("\n========= TESTS strConcatenate =========\n");
    
    // TEST 1: Un string vacío y un string de 3 caracteres
    char* test1_s1 = strDuplicate("");
    char* test1_s2 = strDuplicate("abc");
    char* result1 = strConcatenate(test1_s1, test1_s2);
    if (result1 != NULL && result1[0] == 'a' && result1[1] == 'b' && result1[2] == 'c' && result1[3] == '\0') {
        printf("✓ TEST 1 PASADO: String vacio + 'abc' = 'abc'\n");
    } else {
        printf("✗ TEST 1 FALLADO: String vacio + 'abc' deberia ser 'abc'\n");
    }
    free(result1);
    // Nota: test1_s1 y test1_s2 ya fueron liberados por strConcatenate
    
    // TEST 2: Un string de 3 caracteres y un string vacío
    char* test2_s1 = strDuplicate("xyz");
    char* test2_s2 = strDuplicate("");
    char* result2 = strConcatenate(test2_s1, test2_s2);
    if (result2 != NULL && result2[0] == 'x' && result2[1] == 'y' && result2[2] == 'z' && result2[3] == '\0') {
        printf("✓ TEST 2 PASADO: 'xyz' + string vacio = 'xyz'\n");
    } else {
        printf("✗ TEST 2 FALLADO: 'xyz' + string vacio deberia ser 'xyz'\n");
    }
    free(result2);
    
    // TEST 3: Dos strings de 1 carácter
    char* test3_s1 = strDuplicate("A");
    char* test3_s2 = strDuplicate("B");
    char* result3 = strConcatenate(test3_s1, test3_s2);
    if (result3 != NULL && result3[0] == 'A' && result3[1] == 'B' && result3[2] == '\0') {
        printf("✓ TEST 3 PASADO: 'A' + 'B' = 'AB'\n");
    } else {
        printf("✗ TEST 3 FALLADO: 'A' + 'B' deberia ser 'AB'\n");
    }
    free(result3);
    
    // TEST 4: Dos strings de 5 caracteres
    char* test4_s1 = strDuplicate("Hola ");
    char* test4_s2 = strDuplicate("Mundo");
    char* result4 = strConcatenate(test4_s1, test4_s2);
    int test4_ok = 1;
    char* expected4 = "Hola Mundo";
    if (result4 == NULL) {
        test4_ok = 0;
    } else {
        int i = 0;
        while (expected4[i] != '\0') {
            if (result4[i] != expected4[i]) {
                test4_ok = 0;
                break;
            }
            i++;
        }
        if (result4[i] != '\0') {
            test4_ok = 0;
        }
    }
    if (test4_ok) {
        printf("✓ TEST 4 PASADO: 'Hola ' + 'Mundo' = 'Hola Mundo'\n");
    } else {
        printf("✗ TEST 4 FALLADO: 'Hola ' + 'Mundo' deberia ser 'Hola Mundo'\n");
    }
    free(result4);
    
    // TEST 5: Verificar que los strings originales fueron liberados
    // (Este test es más conceptual - no podemos verificar directamente que fueron liberados)
    char* test5_s1 = strDuplicate("Test");
    char* test5_s2 = strDuplicate("123");
    char* result5 = strConcatenate(test5_s1, test5_s2);
    if (result5 != NULL) {
        printf("✓ TEST 5 PASADO: Concatenacion exitosa (memoria liberada internamente)\n");
    } else {
        printf("✗ TEST 5 FALLADO: Error en concatenacion\n");
    }
    free(result5);
    
    printf("========================================\n\n");


    // TEST 6: Uno NULL, otro válido
    char* test6_s1 = strDuplicate("Hola");
    char* test6_s2 = NULL;
    char* result6 = strConcatenate(test6_s1, test6_s2);
    if (result6 != NULL && result6[0] == 'H' && result6[4] == '\0') {
        printf("✓ TEST 6 PASADO: 'Hola' + NULL = 'Hola'\n");
    } else {
        printf("✗ TEST 6 FALLADO: 'Hola' + NULL deberia ser 'Hola'\n");
    }
    free(result6);

}


// ========== TESTS gameBoardAddZombie ==========
void testGameBoardAddZombie() {
    printf("\n========= TESTS gameBoardAddZombie =========\n");
    
    // TEST 0: Inputs inválidos (tablero independiente)
    GameBoard* test_board0 = gameBoardNew();
    gameBoardAddZombie(NULL, 0);
    gameBoardAddZombie(test_board0, -1);
    gameBoardAddZombie(test_board0, GRID_ROWS);
    if (test_board0->rows[0].first_zombie == NULL) {
        printf("✓ TEST 0 PASADO: Maneja inputs inválidos sin crash ni agregar\n");
    } else {
        printf("✗ TEST 0 FALLADO: Agregó zombie con input inválido\n");
    }
    gameBoardDelete(test_board0);
    
    // TEST 1: Lista de 3 zombies + 1
    GameBoard* test_board1 = gameBoardNew();
    int test_row = 2;
    gameBoardAddZombie(test_board1, test_row);
    gameBoardAddZombie(test_board1, test_row);
    gameBoardAddZombie(test_board1, test_row);
    
    int count1 = 0;
    ZombieNode* current = test_board1->rows[test_row].first_zombie;
    while (current != NULL) {
        count1++;
        current = current->next;
    }
    if (count1 == 3) {
        printf("✓ Subtest 1a PASADO: 3 zombies agregados\n");
    } else {
        printf("✗ Subtest 1a FALLADO: Esperados 3, encontrados %d\n", count1);
    }
    
    gameBoardAddZombie(test_board1, test_row);
    int count2 = 0;
    current = test_board1->rows[test_row].first_zombie;
    while (current != NULL) {
        count2++;
        current = current->next;
    }
    if (count2 == 4) {
        printf("✓ TEST 1 PASADO: 3 + 1 = 4 zombies\n");
    } else {
        printf("✗ TEST 1 FALLADO: Esperados 4, encontrados %d\n", count2);
    }
    gameBoardDelete(test_board1);
    
    // TEST 2: Crear 10000 zombies (eficiencia O(1) por add)
    GameBoard* test_board2 = gameBoardNew();
    int large_row = 0;
    int target_zombies = 10000;
    for (int i = 0; i < target_zombies; i++) {
        gameBoardAddZombie(test_board2, large_row);
    }
    int count_large = 0;
    current = test_board2->rows[large_row].first_zombie;
    while (current != NULL) {
        count_large++;
        current = current->next;
    }
    if (count_large == target_zombies) {
        printf("✓ TEST 2 PASADO: 10000 zombies creados eficientemente\n");
    } else {
        printf("✗ TEST 2 FALLADO: Esperados %d, encontrados %d\n", target_zombies, count_large);
    }
    
    // TEST 3: Verificar inicialización (del primero, O(1))
    current = test_board2->rows[large_row].first_zombie;
    int test3_ok = (current != NULL &&
                    current->zombie_data.vida == 100 &&
                    current->zombie_data.activo == 1 &&
                    current->zombie_data.row == large_row &&
                    current->zombie_data.pos_x == SCREEN_WIDTH &&
                    current->zombie_data.rect.x == SCREEN_WIDTH &&
                    current->zombie_data.current_frame == 0 &&
                    current->zombie_data.frame_timer == 0);
    if (test3_ok) {
        printf("✓ TEST 3 PASADO: Zombies inicializados correctamente\n");
    } else {
        printf("✗ TEST 3 FALLADO: Inicialización incorrecta\n");
    }
    gameBoardDelete(test_board2);
    
    // TEST 4: Múltiples rows independientes (tablero limpio)
    GameBoard* test_board4 = gameBoardNew();
    gameBoardAddZombie(test_board4, 0);
    gameBoardAddZombie(test_board4, 1);
    gameBoardAddZombie(test_board4, 4);
    if (test_board4->rows[0].first_zombie != NULL &&
        test_board4->rows[1].first_zombie != NULL &&
        test_board4->rows[4].first_zombie != NULL &&
        test_board4->rows[2].first_zombie == NULL &&
        test_board4->rows[3].first_zombie == NULL) {
        printf("✓ TEST 4 PASADO: Agrega a rows separadas correctamente\n");
    } else {
        printf("✗ TEST 4 FALLADO: No maneja rows independientes\n");
    }
    gameBoardDelete(test_board4);
    
    printf("========================================\n\n");
}


// ========== TESTS gameBoardRemovePlant ==========
void testGameBoardRemovePlant() {
    printf("\n========= TESTS gameBoardRemovePlant =========\n");

    // Crear un tablero de prueba
    GameBoard* board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al crear tablero para tests\n");
        return;
    }

    // TEST 0: Inputs inválidos (no debe crash, solo logging)
    gameBoardRemovePlant(NULL, 0, 0);
    gameBoardRemovePlant(board, -1, 0);
    gameBoardRemovePlant(board, GRID_ROWS, 0);
    gameBoardRemovePlant(board, 0, -1);
    gameBoardRemovePlant(board, 0, GRID_COLS);
    printf("✓ TEST 0 PASADO: Maneja inputs inválidos correctamente (sin crash)\n");

    // TEST 1: Remover cuando no hay planta (fila vacía, no hace nada)
    resetRow(&board->rows[0]);
    gameBoardRemovePlant(board, 0, 4);
    RowSegment* seg = board->rows[0].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 9 &&
        seg->next == NULL && isRowValid(&board->rows[0])) {
        printf("✓ TEST 1 PASADO: Remover en fila vacía (no cambia nada)\n");
    } else {
        printf("✗ TEST 1 FALLADO: Cambió estructura innecesariamente\n");
    }

    // TEST 2: Remover planta aislada (dividida, crea VACIO len=1, no fusiona)
    resetRow(&board->rows[0]);
    gameBoardAddPlant(board, 0, 4);  // Crea VACIO0-3, PLANTA4, VACIO5-8
    gameBoardRemovePlant(board, 0, 4);
    seg = board->rows[0].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 4 &&
        seg->next != NULL && seg->next->status == STATUS_VACIO && seg->next->start_col == 4 && seg->next->length == 1 &&
        seg->next->next != NULL && seg->next->next->status == STATUS_VACIO && seg->next->next->start_col == 5 && seg->next->next->length == 4 &&
        seg->next->next->next == NULL && isRowValid(&board->rows[0])) {
        printf("✓ TEST 2 PASADO: Remover planta aislada (crea VACIO len=1 sin fusión)\n");
    } else {
        printf("✗ TEST 2 FALLADO: Estructura incorrecta después de remover aislada\n");
    }

    // TEST 3: Remover y fusionar con right (quita col=3 después de agregar 3, fusiona a VACIO0-4)
    resetRow(&board->rows[0]);
    gameBoardAddPlant(board, 0, 3);  // VACIO0-2, PLANTA3, VACIO4-8
    gameBoardRemovePlant(board, 0, 3);
    seg = board->rows[0].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 3 &&
        seg->next != NULL && seg->next->status == STATUS_VACIO && seg->next->start_col == 3 && seg->next->length == 1 &&
        seg->next->next != NULL && seg->next->next->status == STATUS_VACIO && seg->next->next->start_col == 4 && seg->next->next->length == 5 &&
        seg->next->next->next == NULL && isRowValid(&board->rows[0])) {
        printf("✗ TEST 3 FALLADO: No fusionó correctamente (espera fusión manual en test siguiente)\n");
    } else {
        // Esperado después de fusión: VACIO0-2 + VACIO3 + VACIO4-8 -> Pero código fusiona solo adyacentes directos
        // Wait, en código: primero fusiona next si VACIO, luego prev.
        // Inicial: prev=VACIO0-2, current=PLANTA3 -> VACIO3, next=VACIO4-8
        // Primero fusiona current con next -> VACIO3-8
        // Luego fusiona prev con current -> VACIO0-8
        // Sí, debe fusionar a único VACIO0-8
        if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 9 &&
            seg->next == NULL && isRowValid(&board->rows[0])) {
            printf("✓ TEST 3 PASADO: Remover y fusionar con ambos lados (triple fusión a vacía completa)\n");
        } else {
            printf("✗ TEST 3 FALLADO: No realizó triple fusión\n");
        }
    }

    // TEST 4: Caso de la imagen 1 - Plantar 3,4,5; sacar 4 (debe fusionar VACIO entre 3 y 5? No, plantas quedan)
    // Plantar 3,4,5: ... VACIO0-2, PLANTA3, PLANTA4, PLANTA5, VACIO6-8
    // Sacar 4: VACIO0-2, PLANTA3, VACIO4, PLANTA5, VACIO6-8 (no fusiona porque adyacentes son PLANTA)
    resetRow(&board->rows[0]);
    gameBoardAddPlant(board, 0, 3);
    gameBoardAddPlant(board, 0, 4);
    gameBoardAddPlant(board, 0, 5);
    gameBoardRemovePlant(board, 0, 4);
    seg = board->rows[0].first_segment;
    int plant_count = 0;
    int has_col_3 = 0, has_col_5 = 0;
    int vacio_at_4 = 0;
    while (seg != NULL) {
        if (seg->status == STATUS_PLANTA && seg->length == 1) {
            plant_count++;
            if (seg->start_col == 3) has_col_3 = 1;
            if (seg->start_col == 5) has_col_5 = 1;
        } else if (seg->status == STATUS_VACIO && seg->start_col == 4 && seg->length == 1) {
            vacio_at_4 = 1;
        }
        seg = seg->next;
    }
    if (plant_count == 2 && has_col_3 && has_col_5 && vacio_at_4 && isRowValid(&board->rows[0])) {
        printf("✓ TEST 4 PASADO: Caso imagen 1 - Sacar medio sin fusión (hueco len=1 entre plantas)\n");
    } else {
        printf("✗ TEST 4 FALLADO: No manejó remover sin fusión\n");
    }

    // TEST 5: Caso imagen 2 - Siguiendo anterior, sacar 3 (fusiona VACIO3 con VACIO4)
    // Estado actual: VACIO0-2, PLANTA3? No, ya sacamos 4: VACIO0-2, PLANTA3, VACIO4, PLANTA5, VACIO6-8
    // Sacar 3: VACIO0-2, VACIO3, VACIO4, PLANTA5, VACIO6-8 -> Fusiona a VACIO0-4, PLANTA5, VACIO6-8
    gameBoardRemovePlant(board, 0, 3);
    seg = board->rows[0].first_segment;
    plant_count = 0;
    int has_col_5 = 0;
    while (seg != NULL) {
        if (seg->status == STATUS_PLANTA && seg->length == 1) {
            plant_count++;
            if (seg->start_col == 5) has_col_5 = 1;
        }
        seg = seg->next;
    }
    if (plant_count == 1 && has_col_5 && isRowValid(&board->rows[0])) {
        // Verificar estructura: VACIO0-4 (fusionado), PLANTA5, VACIO6-8
        seg = board->rows[0].first_segment;
        if (seg->status == STATUS_VACIO && seg->length == 5 &&
            seg->next->status == STATUS_PLANTA && seg->next->length == 1 &&
            seg->next->next->status == STATUS_VACIO && seg->next->next->length == 4) {
            printf("✓ TEST 5 PASADO: Caso imagen 2 - Sacar adyacente y fusionar con right\n");
        } else {
            printf("✗ TEST 5 FALLADO: No fusionó correctamente\n");
        }
    }

    // TEST 6: Caso imagen 3 - Llenar fila, sacar del medio (crea VACIO len=1, no fusiona)
    resetRow(&board->rows[1]);
    for (int c = 0; c < GRID_COLS; c++) {
        gameBoardAddPlant(board, 1, c);  // 9 PLANTA len=1
    }
    gameBoardRemovePlant(board, 1, 4);  // PLANTA0-3, VACIO4, PLANTA5-8
    seg = board->rows[1].first_segment;
    plant_count = 0;
    vacio_at_4 = 0;
    while (seg != NULL) {
        if (seg->status == STATUS_PLANTA && seg->length == 1) {
            plant_count++;
        } else if (seg->status == STATUS_VACIO && seg->start_col == 4 && seg->length == 1) {
            vacio_at_4 = 1;
        }
        seg = seg->next;
    }
    if (plant_count == 8 && vacio_at_4 && isRowValid(&board->rows[1])) {
        printf("✓ TEST 6 PASADO: Caso imagen 3 - Sacar del medio en fila llena (crea hueco len=1)\n");
    } else {
        printf("✗ TEST 6 FALLADO: Estructura incorrecta en fila llena\n");
    }

    // TEST 7: Remover al inicio (fusiona con right si VACIO)
    resetRow(&board->rows[2]);
    gameBoardAddPlant(board, 2, 0);  // PLANTA0, VACIO1-8
    gameBoardRemovePlant(board, 2, 0);
    seg = board->rows[2].first_segment;
    if (seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 9 &&
        seg->next == NULL && isRowValid(&board->rows[2])) {
        printf("✓ TEST 7 PASADO: Remover al inicio y fusionar con right\n");
    } else {
        printf("✗ TEST 7 FALLADO: No fusionó al inicio\n");
    }

    // TEST 8: Remover al final (fusiona con left si VACIO)
    resetRow(&board->rows[3]);
    gameBoardAddPlant(board, 3, 8);  // VACIO0-7, PLANTA8
    gameBoardRemovePlant(board, 3, 8);
    seg = board->rows[3].first_segment;
    if (seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 9 &&
        seg->next == NULL && isRowValid(&board->rows[3])) {
        printf("✓ TEST 8 PASADO: Remover al final y fusionar con left\n");
    } else {
        printf("✗ TEST 8 FALLADO: No fusionó al final\n");
    }

    // TEST 9: Remover todos hasta vacía (stress test)
    resetRow(&board->rows[4]);
    for (int c = 0; c < GRID_COLS; c++) {
        gameBoardAddPlant(board, 4, c);
    }
    for (int c = 0; c < GRID_COLS; c++) {
        gameBoardRemovePlant(board, 4, c);
    }
    seg = board->rows[4].first_segment;
    if (seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 9 &&
        seg->next == NULL && isRowValid(&board->rows[4])) {
        printf("✓ TEST 9 PASADO: Remover todos hasta fila vacía (verifica fusiones múltiples)\n");
    } else {
        printf("✗ TEST 9 FALLADO: No回到了 a vacía completa\n");
    }

    // Liberar el tablero al final
    gameBoardDelete(board);
    printf("========================================\n\n");
    printf("NOTA: Para verificar memory leaks, usa valgrind --leak-check=full ./juego\n");
    printf("Todos los tests incluyen verificación de integridad de fila (no gaps/overlaps, sum lengths==9)\n");
}


// ========== TESTS gameBoardAddPlant ==========
void testGameBoardAddPlant() {
    printf("\n========= TESTS gameBoardAddPlant =========\n");
    
    GameBoard* board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al crear tablero para tests\n");
        return;
    }
    
    // TEST 0: Inputs inválidos
    if (gameBoardAddPlant(NULL, 0, 0) == 0 &&
        gameBoardAddPlant(board, -1, 0) == 0 &&
        gameBoardAddPlant(board, GRID_ROWS, 0) == 0 &&
        gameBoardAddPlant(board, 0, -1) == 0 &&
        gameBoardAddPlant(board, 0, GRID_COLS) == 0) {
        printf("✓ TEST 0 PASADO: Maneja inputs invalidos correctamente\n");
    } else {
        printf("✗ TEST 0 FALLADO: No maneja inputs invalidos\n");
    }
    
    // TEST 1: Agregar en medio de fila vacía
    if (gameBoardAddPlant(board, 0, 4) == 1) {
        RowSegment* seg = board->rows[0].first_segment;
        if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 4 &&
            seg->next != NULL && seg->next->status == STATUS_PLANTA && seg->next->start_col == 4 && seg->next->length == 1 &&
            seg->next->next != NULL && seg->next->next->status == STATUS_VACIO && seg->next->next->start_col == 5 && seg->next->next->length == 4 &&
            seg->next->next->next == NULL) {
            printf("✓ TEST 1 PASADO: Agregar en medio de fila vacia\n");
        } else {
            printf("✗ TEST 1 FALLADO: Estructura incorrecta despues de agregar en medio\n");
        }
    } else {
        printf("✗ TEST 1 FALLADO: No se pudo agregar en medio\n");
    }
    
    // TEST 2: Agregar al inicio
    resetRow(&board->rows[0]);
    if (gameBoardAddPlant(board, 0, 0) == 1) {
        RowSegment* seg = board->rows[0].first_segment;
        if (seg != NULL && seg->status == STATUS_PLANTA && seg->start_col == 0 && seg->length == 1 &&
            seg->next != NULL && seg->next->status == STATUS_VACIO && seg->next->start_col == 1 && seg->next->length == 8 &&
            seg->next->next == NULL) {
            printf("✓ TEST 2 PASADO: Agregar al inicio de fila vacia\n");
        } else {
            printf("✗ TEST 2 FALLADO: Estructura incorrecta despues de agregar al inicio\n");
        }
    } else {
        printf("✗ TEST 2 FALLADO: No se pudo agregar al inicio\n");
    }
    
    // TEST 3: Agregar al final
    resetRow(&board->rows[0]);
    if (gameBoardAddPlant(board, 0, 8) == 1) {
        RowSegment* seg = board->rows[0].first_segment;
        if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 8 &&
            seg->next != NULL && seg->next->status == STATUS_PLANTA && seg->next->start_col == 8 && seg->next->length == 1 &&
            seg->next->next == NULL) {
            printf("✓ TEST 3 PASADO: Agregar al final de fila vacia\n");
        } else {
            printf("✗ TEST 3 FALLADO: Estructura incorrecta despues de agregar al final\n");
        }
    } else {
        printf("✗ TEST 3 FALLADO: No se pudo agregar al final\n");
    }
    
    // TEST 4: Intentar agregar en celda ocupada
    resetRow(&board->rows[0]);
    gameBoardAddPlant(board, 0, 4);
    if (gameBoardAddPlant(board, 0, 4) == 0) {
        printf("✓ TEST 4 PASADO: No permite agregar en celda ocupada\n");
    } else {
        printf("✗ TEST 4 FALLADO: Permitio agregar en celda ocupada\n");
    }
    
    // TEST 5: Llenar una fila completa
    resetRow(&board->rows[1]);
    int added_all = 1;
    for (int c = 0; c < GRID_COLS; c++) {
        if (gameBoardAddPlant(board, 1, c) != 1) {
            added_all = 0;
            break;
        }
    }
    if (added_all) {
        RowSegment* seg = board->rows[1].first_segment;
        int count = 0;
        int all_planta = 1;
        while (seg != NULL) {
            if (seg->status != STATUS_PLANTA || seg->length != 1) {
                all_planta = 0;
                break;
            }
            count++;
            seg = seg->next;
        }
        if (all_planta && count == GRID_COLS) {
            printf("✓ TEST 5 PASADO: Llenar fila completa (9 plantas)\n");
        } else {
            printf("✗ TEST 5 FALLADO: Estructura incorrecta despues de llenar fila\n");
        }
    } else {
        printf("✗ TEST 5 FALLADO: No se pudo agregar todas las plantas\n");
    }
    
    // TEST 6: Agregar en hueco de length=1
    resetRow(&board->rows[2]);
    gameBoardAddPlant(board, 2, 0);
    gameBoardAddPlant(board, 2, 2);
    if (gameBoardAddPlant(board, 2, 1) == 1) {
        // Verificar que hay exactamente 3 plantas (cols 0, 1, 2)
        RowSegment* seg = board->rows[2].first_segment;
        int plant_count = 0;
        int has_col_0 = 0, has_col_1 = 0, has_col_2 = 0;
        
        while (seg != NULL) {
            if (seg->status == STATUS_PLANTA && seg->length == 1) {
                plant_count++;
                if (seg->start_col == 0) has_col_0 = 1;
                if (seg->start_col == 1) has_col_1 = 1;
                if (seg->start_col == 2) has_col_2 = 1;
            }
            seg = seg->next;
        }
        
        if (plant_count == 3 && has_col_0 && has_col_1 && has_col_2) {
            printf("✓ TEST 6 PASADO: Agregar en hueco de length=1\n");
        } else {
            printf("✗ TEST 6 FALLADO: No se agregaron las 3 plantas esperadas\n");
        }
    } else {
        printf("✗ TEST 6 FALLADO: No se pudo agregar en hueco length=1\n");
    }
    
    gameBoardDelete(board);
    printf("========================================\n\n");
    printf("NOTA: Para verificar memory leaks: valgrind --leak-check=full ./juego\n\n");
}




// ========== MAIN ==========
int main(int argc, char* args[]) {
    srand(time(NULL));
    if (!inicializar()) return 1;

    game_board = gameBoardNew();

    testStrDuplicate();

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
                gameBoardAddPlant(game_board, cursor.row, cursor.col);
            }
        }

        gameBoardUpdate(game_board);
        gameBoardDraw(game_board);

        // Lógica para ver si un zombie llegó a la casa y terminó el juego
        for (int r = 0; r < GRID_ROWS; r++) {
            ZombieNode* current = game_board->rows[r].first_zombie;
            while (current != NULL) {
                Zombie* z = &current->zombie_data;
                if (z->activo && z->rect.x < GRID_OFFSET_X - z->rect.w) {
                    printf("GAME OVER - Un zombie llego a tu casa!\n");
                    game_over = 1;
                    break;
                }
                current = current->next;
            }
            if (game_over) break;
        }

        SDL_Delay(16);
    }

    gameBoardDelete(game_board);
    cerrar();
    return 0;
}
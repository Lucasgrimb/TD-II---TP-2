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



//========= GAME BOARD NEW =========
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
 * Libera una lista enlazada de segmentos (RowSegment).
 * La idea principal es recorrer la lista y liberar cada nodo.
 * Resuelve el problema de la memoria anidada: si el segmento
 * es de tipo PLANTA (planta_data != NULL), libera esa Planta
 * *antes* de liberar el segmento mismo.
 */
static void freeSegments(RowSegment* head) {
    RowSegment* current = head;
    while (current != NULL) {
        RowSegment* next = current->next;  // Guardo el siguiente antes de liberar
        
        // Si el segmento contenía una planta, libero esa memoria primero
        if (current->planta_data != NULL) {
            free(current->planta_data);
        }
        
        // Libero el nodo (segmento) actual
        free(current);
        current = next;
    }
}

/**
 * Libera una lista enlazada simple de zombies (ZombieNode).
 * La idea es recorrer la lista y liberar cada nodo.
 * Se guarda el puntero 'next' antes de liberar 'current'
 * para no perder la referencia al resto de la lista.
 */
static void freeZombies(ZombieNode* head) {
    ZombieNode* current = head;
    while (current != NULL) {
        ZombieNode* next = current->next;  // Guardo el siguiente
        free(current);
        current = next;
    }
}

/**
 * Libera toda la memoria dinámica del GameBoard.
 * Esta es la función principal de limpieza que cumple la consigna.
 * La idea es aplicar una limpieza "de abajo hacia arriba":
 * 1. Recorre cada fila.
 * 2. Libera las listas de Segmentos y Zombies de esa fila (usando helpers).
 * 3. Finalmente, libera el GameBoard.
 */
void gameBoardDelete(GameBoard* board) {
    // Si el puntero es NULL, no hay nada que hacer.
    if (board == NULL) {
        return;
    }
    
    // Recorro cada fila del tablero
    for (int row = 0; row < GRID_ROWS; row++) {
        // Libero las listas enlazadas de esta fila
        freeSegments(board->rows[row].first_segment);
        freeZombies(board->rows[row].first_zombie);
    }
    
    // Una vez liberado todo el contenido, libero el contenedor principal
    free(board);
}






// ========= GAME BOARD ADD PLANT ==========

/**
 * Helper para crear y asignar memoria para una estructura Planta.
 * La idea es encapsular la inicialización de la planta, pidiendo memoria
 * para ella en el heap y seteando sus valores iniciales.
 */
static Planta* createPlanta(int row, int col) {
    Planta* p = malloc(sizeof(Planta));
    if (p == NULL) {
        printf("Error: No se pudo asignar memoria para Planta\n");
        return NULL;
    }
    // Inicializa los datos de la planta según su posición en la grilla
    p->rect.x = GRID_OFFSET_X + (col * CELL_WIDTH);
    p->rect.y = GRID_OFFSET_Y + (row * CELL_HEIGHT);
    p->rect.w = CELL_WIDTH;
    p->rect.h = CELL_HEIGHT;
    p->activo = 1;
    p->cooldown = rand() % 100; // Cooldown inicial aleatorio
    p->current_frame = 0;
    p->frame_timer = 0;
    p->debe_disparar = 0;
    return p;
}

/**
 * Helper para reiniciar una fila a un solo segmento VACIO.
 * Resuelve la necesidad de los tests de empezar con una fila limpia,
 * liberando la lista de segmentos existente y creando una nueva.
 */
static void resetRow(GardenRow* row) {
    freeSegments(row->first_segment); // Libera la lista vieja
    row->first_segment = malloc(sizeof(RowSegment)); // Crea la nueva
    if (row->first_segment != NULL) {
        row->first_segment->status = STATUS_VACIO;
        row->first_segment->start_col = 0;
        row->first_segment->length = GRID_COLS;
        row->first_segment->planta_data = NULL;
        row->first_segment->next = NULL;
    }
}

/**
 * Agrega una planta en la grilla (fila y columna).
 * La idea es buscar en la lista de RowSegment el segmento VACIO que
 * contiene la 'col' deseada. Una vez encontrado, ese segmento se
 * divide en 1, 2 o 3 segmentos nuevos (VACIO, PLANTA, VACIO),
 * manejando la reconexión de punteros y la memoria.
 */
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
    // 'current' avanza por la lista, 'prev' se queda atrás para reconexiones
    RowSegment* current = board->rows[row].first_segment;
    RowSegment* prev = NULL;
    
    while (current != NULL) {
        int start = current->start_col;
        int end = current->start_col + current->length;
        
        // Si la 'col' está dentro de este segmento...
        if (col >= start && col < end) {
            
            // Consigna: Si ya hay una planta, no hacer nada.
            if (current->status == STATUS_PLANTA) {
                return 0; // Celda ocupada
            }
            
            // El segmento es VACIO. Creamos el nuevo segmento PLANTA.
            RowSegment* planta_seg = malloc(sizeof(RowSegment));
            if (planta_seg == NULL) return 0; // Falla malloc
            
            planta_seg->status = STATUS_PLANTA;
            planta_seg->start_col = col;
            planta_seg->length = 1;
            // Creamos la planta anidada (con su propio malloc)
            planta_seg->planta_data = createPlanta(row, col);
            if (planta_seg->planta_data == NULL) {
                free(planta_seg); // Limpieza si falla el malloc anidado
                return 0;
            }
            
            // --- Lógica de División de Segmentos ---

            // CASO 1: La planta se inserta al INICIO del segmento vacío
            if (col == current->start_col) {
                
                // CASO 1.1: El segmento vacío era de tamaño 1.
                // Lo reemplazamos completamente.
                if (current->length == 1) {
                    planta_seg->next = current->next;
                    // Reconecto la lista (prev o el head) para que apunte a planta_seg
                    if (prev == NULL) {
                        board->rows[row].first_segment = planta_seg;
                    } else {
                        prev->next = planta_seg;
                    }
                    free(current); // Libero el segmento vacío reemplazado
                } 
                // CASO 1.2: El segmento vacío era > 1.
                // Achicamos el segmento vacío y ponemos la planta antes.
                else {
                    current->start_col++; // El vacío empieza una col después
                    current->length--;    // y tiene un largo menos
                    planta_seg->next = current;
                    // Reconecto la lista (prev o el head) para que apunte a planta_seg
                    if (prev == NULL) {
                        board->rows[row].first_segment = planta_seg;
                    } else {
                        prev->next = planta_seg;
                    }
                }
                return 1; // Éxito
            }
            
            // CASO 2: La planta se inserta al FINAL del segmento vacío
            else if (col == current->start_col + current->length - 1) {
                // Achicamos el segmento vacío
                current->length--;
                // Insertamos planta_seg *después* de current
                planta_seg->next = current->next;
                current->next = planta_seg;
                return 1; // Éxito
            }
            
            // CASO 3: La planta se inserta en el MEDIO (División en 3)
            // (Ej: [VACIO 0-8] + planta en 3 -> [VACIO 0-2] [PLANTA 3-3] [VACIO 4-8])
            else {
                // 1. Modifico 'current' para que sea la parte IZQUIERDA
                int original_end = current->start_col + current->length;
                current->length = col - current->start_col;
                
                // 2. Creo un nuevo segmento 'right_seg' para la parte DERECHA
                RowSegment* right_seg = malloc(sizeof(RowSegment));
                if (right_seg == NULL) {
                    // Si falla, limpio la planta_data y planta_seg que ya cree
                    free(planta_seg->planta_data);
                    free(planta_seg);
                    return 0;
                }
                right_seg->status = STATUS_VACIO;
                right_seg->start_col = col + 1;
                right_seg->length = original_end - (col + 1);
                right_seg->planta_data = NULL;
                
                // 3. Conecto todo: current -> planta_seg -> right_seg -> (lo que seguía)
                right_seg->next = current->next;
                planta_seg->next = right_seg;
                current->next = planta_seg;
                
                return 1; // Éxito
            }
        }
        
        // Si 'col' no estaba en 'current', avanzo en la lista
        prev = current;
        current = current->next;
    }
    
    // Si salgo del while, es un error (no debería pasar si la fila está bien)
    printf("Error: No se encontro segmento para col %d en row %d\n", col, row);
    return 0;
}







// ========= GAME BOARD REMOVE PLANT==========

/**
 * Elimina una planta de la grilla (fila y columna).
 * La idea principal es encontrar el segmento PLANTA correspondiente,
 * liberar la memoria de sus datos (planta_data) y convertirlo a STATUS_VACIO.
 *
 * Resuelve el problema de la fragmentación de la lista implementando la
 * lógica de FUSIÓN: si el nuevo segmento vacío tiene vecinos también vacíos
 * (izquierda o derecha), se unen en un solo segmento más grande.
 */
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

        // Si la columna está en este segmento...
        if (col >= start && col < end) {
            
            // Consigna: Si no es una planta, no hacer nada.
            if (current->status != STATUS_PLANTA) {
                return;
            }

            // 1. CONVERTIR A VACIO Y LIBERAR PLANTA
            // Libero la memoria anidada de la planta
            free(current->planta_data);
            current->planta_data = NULL;
            // Convierto el segmento en VACIO
            current->status = STATUS_VACIO;

            // 2. FUSIONAR CON SEGMENTOS ADYACENTES VACIOS

            // Fusionar con NEXT (derecha) si es VACIO y adyacente
            if (current->next != NULL && current->next->status == STATUS_VACIO) {
                RowSegment* next_seg = current->next; // Guardo el nodo a eliminar
                
                // Verifico adyacencia (aunque en esta lógica siempre debería serlo)
                if (current->start_col + current->length == next_seg->start_col) {
                    current->length += next_seg->length; // Absorbo su longitud
                    current->next = next_seg->next;      // Salteo el nodo 'next'
                    free(next_seg);                      // Libero el nodo absorbido
                }
            }

            // Fusionar con PREV (izquierda) si es VACIO y adyacente
            // (Esto maneja la fusión triple, ya que 'current' puede
            // haber absorbido ya a 'next')
            if (prev != NULL && prev->status == STATUS_VACIO) {
                if (prev->start_col + prev->length == current->start_col) {
                    prev->length += current->length; // 'prev' absorbe a 'current'
                    prev->next = current->next;      // 'prev' saltea a 'current'
                    free(current);                   // Libero 'current' (absorbido)
                }
            }

            return; // Terminado
        }

        // Si no era este segmento, avanzo en la lista
        prev = current;
        current = current->next;
    }
}





//======== GAME BOARD ADD ZOMBIE ==========

/**
 * Crea un nuevo zombie (ZombieNode) con memoria dinámica y lo
 * agrega a la lista enlazada de la fila correspondiente.
 *
 * Esta función resuelve la limitación del array estático del juego base,
 * permitiendo un número virtualmente ilimitado de zombies.
 * La idea principal es usar la inserción al principio de la lista (O(1)),
 * que es la forma más eficiente de agregar un nodo.
 */
void gameBoardAddZombie(GameBoard* board, int row) {
    // Validación: chequeo de puntero NULL e índice de fila
    if (board == NULL) {
        printf("Error: Board es NULL en gameBoardAddZombie\n");
        return;
    }
    if (row < 0 || row >= GRID_ROWS) {
        printf("Error: Row %d inválida (debe ser 0-%d) en gameBoardAddZombie\n", 
               row, GRID_ROWS - 1);
        return;
    }
    
    // 1. Asigno memoria en el heap para el nuevo nodo (el "contenedor")
    ZombieNode* nuevo_nodo = malloc(sizeof(ZombieNode));
    if (nuevo_nodo == NULL) {
        printf("Error: No se pudo asignar memoria para el zombie\n");
        return;
    }
    
    // 2. Inicializo los datos del zombie (el "contenido")
    nuevo_nodo->zombie_data.row = row;
    nuevo_nodo->zombie_data.pos_x = SCREEN_WIDTH;  // Spawnea fuera de pantalla
    nuevo_nodo->zombie_data.rect.x = (int)nuevo_nodo->zombie_data.pos_x;
    
    // Ajuste para que el rect se ajuste al tamaño de la celda (como en el original)
    nuevo_nodo->zombie_data.rect.y = GRID_OFFSET_Y + (row * CELL_HEIGHT);
    
    // Dimensiones ajustadas a la celda para escalado automático en render
    nuevo_nodo->zombie_data.rect.w = CELL_WIDTH;
    nuevo_nodo->zombie_data.rect.h = CELL_HEIGHT;
    
    // Valores iniciales estándar
    nuevo_nodo->zombie_data.vida = 100;
    nuevo_nodo->zombie_data.activo = 1;
    nuevo_nodo->zombie_data.current_frame = 0;
    nuevo_nodo->zombie_data.frame_timer = 0;
    
    // 3. Agrego el nodo al PRINCIPIO de la lista (Head Insertion)
    nuevo_nodo->next = board->rows[row].first_zombie;
    board->rows[row].first_zombie = nuevo_nodo;
}






// ========= GAME BOARD UPDATE ==========

/**
 * Helper para crear una nueva arveja.
 */
static void dispararArveja(GameBoard* board, Planta* p, int row) {
    
    // Busca un 'slot' de arveja inactivo
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (!board->arvejas[i].activo) {
            board->arvejas[i].rect.x = p->rect.x + (CELL_WIDTH / 2); // Centrado
            board->arvejas[i].rect.y = p->rect.y + (CELL_HEIGHT / 4); // Centrado
            
            board->arvejas[i].rect.w = 20;
            board->arvejas[i].rect.h = 20;

            board->arvejas[i].activo = 1;
            break; // Solo dispara una arveja a la vez
        }
    }
}

/**
 * Helper para manejar el spawn de zombies.
 */
static void generarZombieSiNecesario(GameBoard* board) {
    board->zombie_spawn_timer--;
    if (board->zombie_spawn_timer <= 0) {
        int random_row = rand() % GRID_ROWS;
        gameBoardAddZombie(board, random_row); // Llama a la función de inserción
        board->zombie_spawn_timer = ZOMBIE_SPAWN_RATE; // Resetea el timer
    }
}

/**
 * Avanza el estado del juego un "tick".
 */
void gameBoardUpdate(GameBoard* board) {
    if (board == NULL) {
        return;
    }

    // ===== 1. ACTUALIZAR ZOMBIES =====
    for (int r = 0; r < GRID_ROWS; r++) {
        ZombieNode* z_node = board->rows[r].first_zombie;
        ZombieNode* prev_z = NULL;
        
        while (z_node != NULL) {
            Zombie* z = &z_node->zombie_data;
            
            if (z->activo) {
                float distance_per_tick = ZOMBIE_DISTANCE_PER_CYCLE / 
                    (float)(ZOMBIE_TOTAL_FRAMES * ZOMBIE_ANIMATION_SPEED);
                z->pos_x -= distance_per_tick;
                z->rect.x = (int)z->pos_x;

                z->frame_timer++;
                if (z->frame_timer >= ZOMBIE_ANIMATION_SPEED) {
                    z->frame_timer = 0;
                    z->current_frame = (z->current_frame + 1) % ZOMBIE_TOTAL_FRAMES;
                }
            }

            if (!z->activo && z->vida <= 0) {
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

    // ===== 2. ACTUALIZAR PLANTAS =====
    for (int r = 0; r < GRID_ROWS; r++) {
        RowSegment* seg = board->rows[r].first_segment;
        
        while (seg != NULL) {
            if (seg->status == STATUS_PLANTA && seg->planta_data != NULL) {
                Planta* p = seg->planta_data;

                // Lógica de cooldown y disparo
                if (p->cooldown > 0) {
                    p->cooldown--;
                } else {
                    p->debe_disparar = 1;
                }

                // Lógica de animación
                p->frame_timer++;
                if (p->frame_timer >= PEASHOOTER_ANIMATION_SPEED) {
                    p->frame_timer = 0;
                    p->current_frame = (p->current_frame + 1) % PEASHOOTER_TOTAL_FRAMES;

                    if (p->debe_disparar && p->current_frame == PEASHOOTER_SHOOT_FRAME) {
                        dispararArveja(board, p, r);
                        p->cooldown = 120; // Reinicio cooldown
                        p->debe_disparar = 0;
                    }
                }
            }
            seg = seg->next;
        }
    }

    // ===== 3. ACTUALIZAR ARVEJAS =====
    // (Esta sección no cambia)
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (board->arvejas[i].activo) {
            board->arvejas[i].rect.x += PEA_SPEED;
            
            if (board->arvejas[i].rect.x > SCREEN_WIDTH) {
                board->arvejas[i].activo = 0;
            }
        }
    }

    // ===== 4. DETECTAR COLISIONES ARVEJA-ZOMBIE =====
    for (int i = 0; i < MAX_ARVEJAS; i++) {
        if (!board->arvejas[i].activo) continue;

        int arveja_y_center = board->arvejas[i].rect.y + board->arvejas[i].rect.h / 2;
        int arveja_row = (arveja_y_center - GRID_OFFSET_Y) / CELL_HEIGHT;
        
        if (arveja_row < 0 || arveja_row >= GRID_ROWS) continue;

        ZombieNode* z_node = board->rows[arveja_row].first_zombie;
        while (z_node != NULL) {
            Zombie* z = &z_node->zombie_data;
            
            if (z->activo && SDL_HasIntersection(&board->arvejas[i].rect, &z->rect)) {
                board->arvejas[i].activo = 0;
                z->vida -= 25;
                
                if (z->vida <= 0) {
                    z->activo = 0;
                }
                break;
            }
            z_node = z_node->next;
        }
    }

    // ===== 5. GENERAR NUEVOS ZOMBIES =====
    generarZombieSiNecesario(board);
}




// ========= GAME BOARD DRAW ==========


/**
 * Dibuja el estado actual del juego en la pantalla.
 * Esta función implementa la lógica de la consigna,
 * adaptando la función 'dibujar' del juego base a las nuevas
 * estructuras de listas enlazadas.
 *
 * La idea principal es dibujar en "capas", de atrás hacia adelante:
 * 1. Fondo -> 2. Plantas -> 3. Arvejas -> 4. Zombies -> 5. UI (Cursor)
 */
void gameBoardDraw(GameBoard* board) {
    // Validaciones básicas
    if (board == NULL || renderer == NULL) {
        return;
    }

    // 1. Limpiar el frame anterior
    SDL_RenderClear(renderer);

    // 2. Dibujar el fondo
    if (tex_background != NULL) {
        SDL_RenderCopy(renderer, tex_background, NULL, NULL);
    }

    // 3. DIBUJAR PLANTAS
    // Recorro las 5 filas
    for (int r = 0; r < GRID_ROWS; r++) {
        // Recorro la lista de segmentos de cada fila
        RowSegment* seg = board->rows[r].first_segment;
        while (seg != NULL) {
            // Si el segmento es una planta y está activa, la dibujo
            if (seg->status == STATUS_PLANTA && seg->planta_data != NULL) {
                Planta* p = seg->planta_data;
                if (p->activo) {
                    // Calculo el 'src_rect' para tomar el cuadro de animación correcto
                    // de la hoja de sprites (sprite sheet).
                    SDL_Rect src_rect = {
                        p->current_frame * PEASHOOTER_FRAME_WIDTH,
                        0,
                        PEASHOOTER_FRAME_WIDTH,
                        PEASHOOTER_FRAME_HEIGHT
                    };
                    SDL_RenderCopy(renderer, tex_peashooter_sheet, &src_rect, &p->rect);
                }
            }
            seg = seg->next;
        }
    }

    // 4. DIBUJAR ARVEJAS
    // (Las arvejas siguen en un array estático)
    if (tex_pea != NULL) {
        for (int i = 0; i < MAX_ARVEJAS; i++) {
            if (board->arvejas[i].activo) {
                // El 'src_rect' es NULL porque 'pea.png' no es una hoja de sprites
                SDL_RenderCopy(renderer, tex_pea, NULL, &board->arvejas[i].rect);
            }
        }
    }

    // 5. DIBUJAR ZOMBIES
    // Recorro las 5 filas
    for (int r = 0; r < GRID_ROWS; r++) {
        // Recorro la lista de zombies de cada fila
        ZombieNode* z_node = board->rows[r].first_zombie;
        while (z_node != NULL) {
            Zombie* z = &z_node->zombie_data;
            if (z->activo) {
                // Calculo el 'src_rect' para la animación del zombie
                SDL_Rect src_rect = {
                    z->current_frame * ZOMBIE_FRAME_WIDTH,
                    0,
                    ZOMBIE_FRAME_WIDTH,
                    ZOMBIE_FRAME_HEIGHT
                };
                SDL_RenderCopy(renderer, tex_zombie_sheet, &src_rect, &z->rect);
            }
            z_node = z_node->next;
        }
    }

    // 6. DIBUJAR CURSOR (UI)
    // Es importante guardar y restaurar el color de dibujo
    // para no afectar otros elementos de la UI.
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a); // Guardo color
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200); // Amarillo
    SDL_Rect cursor_rect = {
        GRID_OFFSET_X + (cursor.col * CELL_WIDTH),
        GRID_OFFSET_Y + (cursor.row * CELL_HEIGHT),
        CELL_WIDTH,
        CELL_HEIGHT
    };
    SDL_RenderDrawRect(renderer, &cursor_rect);
    
    SDL_SetRenderDrawColor(renderer, r, g, b, a); // Restauro color

    // 7. Muestro el frame terminado en la pantalla
    SDL_RenderPresent(renderer);
}



//=========== STR DUPLICATE ===========
/**
 * Duplica un string en una nueva área de memoria.
 * La idea es calcular la longitud de src, pedir memoria con malloc
 * (longitud + 1 para el '\0') y copiar todo el contenido.
 * Esto resuelve el problema de necesitar copias independientes.
 */
char* strDuplicate(char* src) {
    // Valido la entrada
    if (src == NULL) {
        return NULL;  
    }

    //Calculo la longitud de src
    size_t len = 0;
    while (src[len] != '\0') {
        len++;
    }

    // Asigno memoria para el nuevo string (longitud + 1)
    char* nuevo = malloc(len + 1);  
    if (nuevo == NULL) {    // Fallo en malloc
        return NULL;
    }

    // Copio el contenido de src a nuevo, incluyendo el '\0'
    for (size_t i = 0; i <= len; i++) {  
        nuevo[i] = src[i];
    }
    return nuevo;
}


//=========== STR COMPARE ===========
/**
 * Compara dos strings lexicográficamente (orden ASCII).
 * Resuelve la necesidad de ordenar o comparar strings siguiendo
 * las reglas de la consigna:
 * 0 si son iguales,
 * 1 si s1 < s2,
 * -1 si s1 > s2.
 */
int strCompare(char* s1, char* s2) {
    // Manejo robusto de casos NULL
    if (s1 == NULL && s2 == NULL) return 0; // Iguales
    if (s1 == NULL) return 1;  // NULL se considera "menor"
    if (s2 == NULL) return -1; // "mayor" que NULL
    
    // Itero hasta encontrar la primera diferencia o el fin de un string
    size_t i = 0;
    while (s1[i] != '\0' && s2[i] != '\0' && s1[i] == s2[i]) {
        i++;
    }
    
    // Analizo el carácter donde frenó
    if (s1[i] == s2[i]) {
        return 0;  // Son iguales (ambos llegaron a '\0')
    } else if ((unsigned char)s1[i] < (unsigned char)s2[i]) {
        return 1;  // s1 es menor
    } else {
        return -1; // s1 es mayor
    }
}


//=========== STR CONCATENATE ================
/**
 * Concatena dos strings (src1 y src2) en uno nuevo.
 * La idea es calcular las longitudes, pedir memoria para la suma
 * de ambas (más el '\0') y copiar ambos contenidos.
 * Resuelve el requisito de la consigna de liberar la memoria
 * de los strings originales.
 */
char* strConcatenate(char* src1, char* src2) {
    // Calcular longitudes de src1 y src2, manejando NULLs
    size_t len1 = 0;
    if (src1 != NULL) {
        while (src1[len1] != '\0') len1++;
    }
    size_t len2 = 0;
    if (src2 != NULL) {
        while (src2[len2] != '\0') len2++;
    }

    // Asignar memoria para el string resultante
    char* resultado = malloc(len1 + len2 + 1);
    
    // Si falla malloc, libero la memoria original (como pide la consigna)
    if (resultado == NULL) {
        free(src1);  
        free(src2);
        return NULL;
    }

    // Copio src1 en resultado
    size_t i = 0;
    if (src1 != NULL) {
        for (; i < len1; i++) {
            resultado[i] = src1[i];
        }
    }

    // Copio src2 a continuación
    size_t j = 0;
    if (src2 != NULL) {
        for (; j < len2; j++, i++) {
            resultado[i] = src2[j];
        }
    }
    // Termino el nuevo string
    resultado[i] = '\0';

    // Libero la memoria de los strings originales (consigna)
    free(src1);
    free(src2);
    return resultado;
}




// ========== TESTS STR DUPLICATE ==========


/**
 * Pruebas para strDuplicate.
 * El objetivo es validar que la copia funcione en casos borde
 * (vacío, NULL) y que la copia sea independiente en memoria.
 */
void testStrDuplicate() {
    printf("\n========= TESTS strDuplicate =========\n");
    
    // TEST 1: Pruebo el caso base: un string vacío
    char* test1_src = "";
    char* test1_dup = strDuplicate(test1_src);
    if (test1_dup != NULL && test1_dup[0] == '\0') {
        printf("✓ TEST 1 PASADO: String vacio duplicado correctamente\n");
    } else {
        printf("✗ TEST 1 FALLADO: String vacio no duplicado correctamente\n");
    }
    free(test1_dup);
    
    // TEST 2: Pruebo el caso mínimo con contenido
    char* test2_src = "A";
    char* test2_dup = strDuplicate(test2_src);
    if (test2_dup != NULL && test2_dup[0] == 'A' && test2_dup[1] == '\0') {
        printf("✓ TEST 2 PASADO: String de un caracter duplicado correctamente\n");
    } else {
        printf("✗ TEST 2 FALLADO: String de un caracter no duplicado correctamente\n");
    }
    free(test2_dup);
    
    // TEST 3: Pruebo un string normal
    char* test3_src = "Hola Mundo 123!";
    char* test3_dup = strDuplicate(test3_src);
    int test3_ok = 1;
    if (test3_dup == NULL) {
        test3_ok = 0;
    } else {
        // Verifico contenido y terminador
        int i = 0;
        while (test3_src[i] != '\0') {
            if (test3_dup[i] != test3_src[i]) {
                test3_ok = 0;
                break;
            }
            i++;
        }
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
    
    // TEST 4: Pruebo todos los caracteres pedidos por la consigna
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
    
    // TEST 5: Pruebo la independencia de la memoria.
    // Modifico la copia y verifico que el original no cambie.
    char* test5_src = "Test";
    char* test5_dup = strDuplicate(test5_src);
    if (test5_dup != NULL && test5_dup != test5_src) {
        test5_dup[0] = 'X'; // Modifico la copia
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

    // TEST 6: Pruebo la robustez de la función ante un NULL
    char* test6_dup = strDuplicate(NULL);
    if (test6_dup == NULL) {
        printf("✓ TEST 6 PASADO: NULL input retorna NULL\n");
    } else {
        printf("✗ TEST 6 FALLADO: NULL input no manejado correctamente\n");
        free(test6_dup);
    }
}



// ========== TESTS STR COMPARE ==========


/**
 * Pruebas para strCompare.
 * El objetivo es validar la lógica de comparación lexicográfica
 * y los valores de retorno pedidos por la consigna (0, 1, -1).
 */
void testStrCompare() {
    printf("\n========= TESTS strCompare =========\n");
    
    // TEST 1: Pruebo el caso base: dos strings vacíos
    char* test1_s1 = "";
    char* test1_s2 = "";
    int result1 = strCompare(test1_s1, test1_s2);
    if (result1 == 0) {
        printf("✓ TEST 1 PASADO: Dos strings vacios son iguales\n");
    } else {
        printf("✗ TEST 1 FALLADO: Dos strings vacios deberian ser iguales (resultado: %d)\n", result1);
    }
    
    // TEST 2: Pruebo igualdad con un carácter
    char* test2_s1 = "A";
    char* test2_s2 = "A";
    int result2 = strCompare(test2_s1, test2_s2);
    if (result2 == 0) {
        printf("✓ TEST 2 PASADO: Dos strings de un caracter iguales\n");
    } else {
        printf("✗ TEST 2 FALLADO: Dos strings de un caracter iguales (resultado: %d)\n", result2);
    }
    
    // TEST 3: Pruebo (s1 < s2)
    char* test3_s1 = "abc";
    char* test3_s2 = "abd";
    int result3 = strCompare(test3_s1, test3_s2);
    if (result3 == 1) { // Consigna pide 1
        printf("✓ TEST 3 PASADO: 'abc' es menor que 'abd'\n");
    } else {
        printf("✗ TEST 3 FALLADO: 'abc' deberia ser menor que 'abd' (resultado: %d)\n", result3);
    }
    
    // TEST 4: Pruebo (s1 > s2)
    char* test4_s1 = "abd";
    char* test4_s2 = "abc";
    int result4 = strCompare(test4_s1, test4_s2);
    if (result4 == -1) { // Consigna pide -1
        printf("✓ TEST 4 PASADO: 'abd' es mayor que 'abc'\n");
    } else {
        printf("✗ TEST 4 FALLADO: 'abd' deberia ser mayor que 'abc' (resultado: %d)\n", result4);
    }
    
    // TEST 5: Pruebo strings diferentes (s1 < s2)
    char* test5_s1 = "apple";
    char* test5_s2 = "banana";
    int result5 = strCompare(test5_s1, test5_s2);
    if (result5 == 1) { 
        printf("✓ TEST 5 PASADO: 'apple' es menor que 'banana'\n");
    } else {
        printf("✗ TEST 5 FALLADO: 'apple' deberia ser menor que 'banana' (resultado: %d)\n", result5);
    }
    
    // TEST 6: Pruebo strings diferentes (s1 > s2)
    char* test6_s1 = "banana";
    char* test6_s2 = "apple";
    int result6 = strCompare(test6_s1, test6_s2);
    if (result6 == -1) { 
        printf("✓ TEST 6 PASADO: 'banana' es mayor que 'apple'\n");
    } else {
        printf("✗ TEST 6 FALLADO: 'banana' deberia ser mayor que 'apple' (resultado: %d)\n", result6);
    }
    
    // TEST 7: Pruebo el caso de prefijo (s1 < s2)
    char* test7_s1 = "test";
    char* test7_s2 = "testing";
    int result7 = strCompare(test7_s1, test7_s2);
    if (result7 == 1) { 
        printf("✓ TEST 7 PASADO: 'test' es menor que 'testing'\n");
    } else {
        printf("✗ TEST 7 FALLADO: 'test' deberia ser menor que 'testing' (resultado: %d)\n", result7);
    }
    
    printf("====================================\n\n");

    // TEST 8: Pruebo el caso de prefijo (s1 > s2)
    char* test8_s1 = "testing";
    char* test8_s2 = "test";
    int result8 = strCompare(test8_s1, test8_s2);
    if (result8 == -1) { 
        printf("✓ TEST 8 PASADO: 'testing' es mayor que 'test'\n");
    } else {
        printf("✗ TEST 8 FALLADO: 'testing' deberia ser mayor que 'test' (resultado: %d)\n", result8);
    }

    // TEST 9: Pruebo la sensibilidad a mayúsculas
    char* test9_s1 = "A";
    char* test9_s2 = "a";
    int result9 = strCompare(test9_s1, test9_s2);
    if (result9 == 1) { 
        printf("✓ TEST 9 PASADO: 'A' < 'a'\n");
    } else {
        printf("✗ TEST 9 FALLADO: 'A' deberia ser menor que 'a' (resultado: %d)\n", result9);
    }

    // TEST 10: Pruebo la robustez de la función ante NULLs
    int result10a = strCompare(NULL, NULL);  // 0
    int result10b = strCompare(NULL, "a");   // 1
    int result10c = strCompare("a", NULL);   // -1
    if (result10a == 0 && result10b == 1 && result10c == -1) {
        printf("✓ TEST 10 PASADO: Manejo de NULL\n");
    } else {
        printf("✗ TEST 10 FALLADO: Manejo de NULL incorrecto\n");
    }
}




// ========== TESTS STR CONCATENATE ==========


/**
 * Pruebas para strConcatenate.
 * El objetivo es validar la correcta unión de strings en casos
 * borde (vacíos, NULL) y la correcta gestión de la memoria,
 * verificando que los 'src' se liberen.
 */
void testStrConcatenate() {
    printf("\n========= TESTS strConcatenate =========\n");
    
    // TEST 1: Pruebo string vacío + string
    char* test1_s1 = strDuplicate("");
    char* test1_s2 = strDuplicate("abc");
    char* result1 = strConcatenate(test1_s1, test1_s2);
    if (result1 != NULL && result1[0] == 'a' && result1[1] == 'b' && result1[2] == 'c' && result1[3] == '\0') {
        printf("✓ TEST 1 PASADO: String vacio + 'abc' = 'abc'\n");
    } else {
        printf("✗ TEST 1 FALLADO: String vacio + 'abc' deberia ser 'abc'\n");
    }
    free(result1);
    
    // TEST 2: Pruebo string + string vacío
    char* test2_s1 = strDuplicate("xyz");
    char* test2_s2 = strDuplicate("");
    char* result2 = strConcatenate(test2_s1, test2_s2);
    if (result2 != NULL && result2[0] == 'x' && result2[1] == 'y' && result2[2] == 'z' && result2[3] == '\0') {
        printf("✓ TEST 2 PASADO: 'xyz' + string vacio = 'xyz'\n");
    } else {
        printf("✗ TEST 2 FALLADO: 'xyz' + string vacio deberia ser 'xyz'\n");
    }
    free(result2);
    
    // TEST 3: Pruebo string de 1 char + 1 char
    char* test3_s1 = strDuplicate("A");
    char* test3_s2 = strDuplicate("B");
    char* result3 = strConcatenate(test3_s1, test3_s2);
    if (result3 != NULL && result3[0] == 'A' && result3[1] == 'B' && result3[2] == '\0') {
        printf("✓ TEST 3 PASADO: 'A' + 'B' = 'AB'\n");
    } else {
        printf("✗ TEST 3 FALLADO: 'A' + 'B' deberia ser 'AB'\n");
    }
    free(result3);
    
    // TEST 4: Pruebo dos strings normales
    char* test4_s1 = strDuplicate("Hola ");
    char* test4_s2 = strDuplicate("Mundo");
    char* result4 = strConcatenate(test4_s1, test4_s2);
    int test4_ok = 1;
    char* expected4 = "Hola Mundo";
    if (result4 == NULL) {
        test4_ok = 0;
    } else {
        // Verifico contenido
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
    
    // TEST 5: Prueba conceptual de la gestión de memoria.
    // Al usar strDuplicate, me aseguro que los 'src' están en el heap.
    // Si strConcatenate los libera (como pide la consigna) y puedo
    // liberar el 'resultado' sin crasheos, la memoria se manejó bien.
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

    // TEST 6: Pruebo la robustez ante un NULL
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



// ========== TESTS gameBoardAddPlant ==========


/**
 * Pruebas para gameBoardAddPlant.
 * La idea es validar exhaustivamente la lógica de "división de segmentos",
 * que es el núcleo de la gestión de la grilla.
 */
void testGameBoardAddPlant() {
    printf("\n========= TESTS gameBoardAddPlant =========\n");
    
    GameBoard* board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al crear tablero para tests\n");
        return;
    }
    
    // TEST 0: Pruebo la robustez ante inputs inválidos.
    // La función no debería crashear ni agregar nada.
    if (gameBoardAddPlant(NULL, 0, 0) == 0 &&
        gameBoardAddPlant(board, -1, 0) == 0 &&
        gameBoardAddPlant(board, GRID_ROWS, 0) == 0 &&
        gameBoardAddPlant(board, 0, -1) == 0 &&
        gameBoardAddPlant(board, 0, GRID_COLS) == 0) {
        printf("✓ TEST 0 PASADO: Maneja inputs invalidos correctamente\n");
    } else {
        printf("✗ TEST 0 FALLADO: No maneja inputs invalidos\n");
    }
    
    // TEST 1: Pruebo agregar en el medio (Consigna 1)
    // Valida la lógica de división en 3 segmentos: VACIO-PLANTA-VACIO.
    if (gameBoardAddPlant(board, 0, 4) == 1) {
        RowSegment* seg = board->rows[0].first_segment;
        // Reviso la estructura de la lista: [VACIO, 0, 4] -> [PLANTA, 4, 1] -> [VACIO, 5, 4]
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
    
    // TEST 2: Pruebo agregar al inicio (Consigna 1)
    // Valida la lógica de división en 2 segmentos al inicio: PLANTA-VACIO.
    resetRow(&board->rows[0]);
    if (gameBoardAddPlant(board, 0, 0) == 1) {
        RowSegment* seg = board->rows[0].first_segment;
        // Reviso la estructura: [PLANTA, 0, 1] -> [VACIO, 1, 8]
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
    
    // TEST 3: Pruebo agregar al final (Consigna 1)
    // Valida la lógica de división en 2 segmentos al final: VACIO-PLANTA.
    resetRow(&board->rows[0]);
    if (gameBoardAddPlant(board, 0, 8) == 1) {
        RowSegment* seg = board->rows[0].first_segment;
        // Reviso la estructura: [VACIO, 0, 8] -> [PLANTA, 8, 1]
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
    
    // TEST 4: Pruebo celda ocupada (Consigna 3)
    // La función debe retornar 0 (fallo) si se planta sobre una PLANTA existente.
    resetRow(&board->rows[0]);
    gameBoardAddPlant(board, 0, 4); // Planto la primera vez
    if (gameBoardAddPlant(board, 0, 4) == 0) { // Intento plantar de nuevo
        printf("✓ TEST 4 PASADO: No permite agregar en celda ocupada\n");
    } else {
        printf("✗ TEST 4 FALLADO: Permitio agregar en celda ocupada\n");
    }
    
    // TEST 5: Pruebo llenar la fila (Consigna 2)
    // Valida que la lista de segmentos maneje el caso de 9 nodos PLANTA seguidos.
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
        int all_plants = 1;
        while (seg != NULL) {
            if (seg->status != STATUS_PLANTA || seg->length != 1) {
                all_plants = 0;
                break;
            }
            count++;
            seg = seg->next;
        }
        if (all_plants && count == GRID_COLS) {
            printf("✓ TEST 5 PASADO: Llenar fila completa (9 plantas)\n");
        } else {
            printf("✗ TEST 5 FALLADO: Estructura incorrecta (encontradas %d plantas)\n", count);
        }
    } else {
        printf("✗ TEST 5 FALLADO: No se pudo llenar la fila completa\n");
    }
    
    // TEST 6: Prueba avanzada de llenado de huecos.
    // Simula un patrón de plantado no lineal (pares, luego impares).
    resetRow(&board->rows[2]);
    int pattern_ok = 1;
    for (int c = 0; c < GRID_COLS; c += 2) { // Columnas pares
        if (gameBoardAddPlant(board, 2, c) != 1) pattern_ok = 0;
    }
    for (int c = 1; c < GRID_COLS; c += 2) { // Columnas impares (huecos)
        if (gameBoardAddPlant(board, 2, c) != 1) pattern_ok = 0;
    }
    
    if (pattern_ok) {
        // Al final, la fila debe estar llena (9 segmentos PLANTA)
        RowSegment* seg = board->rows[2].first_segment;
        int plant_count = 0;
        while (seg != NULL) {
            if (seg->status == STATUS_PLANTA) plant_count++;
            seg = seg->next;
        }
        if (plant_count == GRID_COLS) {
            printf("✓ TEST 6 PASADO: Patron alternado y llenado de huecos\n");
        } else {
            printf("✗ TEST 6 FALLADO: Esperaba %d plantas, encontre %d\n", GRID_COLS, plant_count);
        }
    } else {
        printf("✗ TEST 6 FALLADO: No se pudo crear patron alternado\n");
    }
    
    // TEST 7: Prueba avanzada de caso borde: plantar en un hueco (length=1) al inicio.
    resetRow(&board->rows[3]);
    gameBoardAddPlant(board, 3, 1); // Deja hueco en col 0
    if (gameBoardAddPlant(board, 3, 0) == 1) { // Llena el hueco
        RowSegment* seg = board->rows[3].first_segment;
        // Valida que los dos primeros segmentos sean PLANTA
        if (seg != NULL && seg->status == STATUS_PLANTA && seg->start_col == 0 &&
            seg->next != NULL && seg->next->status == STATUS_PLANTA && seg->next->start_col == 1) {
            printf("✓ TEST 7 PASADO: Agregar en segmento length=1 al inicio\n");
        } else {
            printf("✗ TEST 7 FALLADO: Estructura incorrecta en segmento length=1\n");
        }
    } else {
        printf("✗ TEST 7 FALLADO: No se pudo agregar en segmento length=1\n");
    }
    
    // TEST 8: Prueba avanzada de caso borde: plantar en un hueco (length=1) al final.
    resetRow(&board->rows[3]);
    gameBoardAddPlant(board, 3, 7); // Deja hueco en col 8
    if (gameBoardAddPlant(board, 3, 8) == 1) { // Llena el hueco
        RowSegment* seg = board->rows[3].first_segment;
        while (seg != NULL && seg->next != NULL && seg->next->next != NULL) {
            seg = seg->next; // Avanzo hasta el anteúltimo segmento
        }
        // Valida que los dos últimos segmentos sean PLANTA
        if (seg != NULL && seg->status == STATUS_PLANTA && seg->start_col == 7 &&
            seg->next != NULL && seg->next->status == STATUS_PLANTA && seg->next->start_col == 8) {
            printf("✓ TEST 8 PASADO: Agregar en segmento length=1 al final\n");
        } else {
            printf("✗ TEST 8 FALLADO: Estructura incorrecta en segmento length=1 al final\n");
        }
    } else {
        printf("✗ TEST 8 FALLADO: No se pudo agregar en segmento length=1 al final\n");
    }
    
    // TEST 9: Prueba de robustez: verifica que planta_data (memoria anidada)
    // se haya asignado e inicializado correctamente.
    resetRow(&board->rows[4]);
    if (gameBoardAddPlant(board, 4, 3) == 1) {
        RowSegment* seg = board->rows[4].first_segment;
        while (seg != NULL && seg->status != STATUS_PLANTA) {
            seg = seg->next; // Busco el segmento planta
        }
        if (seg != NULL && seg->planta_data != NULL) {
            // Verifico que los datos de la planta sean correctos
            if (seg->planta_data->activo == 1 &&
                seg->planta_data->rect.x == GRID_OFFSET_X + (3 * CELL_WIDTH) &&
                seg->planta_data->rect.y == GRID_OFFSET_Y + (4 * CELL_HEIGHT)) {
                printf("✓ TEST 9 PASADO: planta_data inicializada correctamente\n");
            } else {
                printf("✗ TEST 9 FALLADO: planta_data mal inicializada\n");
            }
        } else {
            printf("✗ TEST 9 FALLADO: planta_data es NULL\n");
        }
    } else {
        printf("✗ TEST 9 FALLADO: No se pudo agregar planta\n");
    }
    
    // Libero toda la memoria usada en los tests
    gameBoardDelete(board);
    printf("========================================\n");
    printf("Tests completados para gameBoardAddPlant\n");
    printf("========================================\n\n");
}





// ========== TESTS gameBoardRemovePlant ==========


/**
 * Pruebas para gameBoardRemovePlant.
 * La idea es validar la lógica de "fusión de segmentos",
 * que es el problema central de esta función. Los tests
 * cubren los casos mínimos de la consigna y varios
 * casos borde (fusión al inicio, al final y triple).
 */
void testGameBoardRemovePlant() {
    printf("\n========= TESTS gameBoardRemovePlant =========\n");

    GameBoard* board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al crear tablero para tests\n");
        return;
    }

    // TEST 0: Pruebo la robustez ante inputs inválidos.
    gameBoardRemovePlant(NULL, 0, 0);
    gameBoardRemovePlant(board, -1, 0);
    gameBoardRemovePlant(board, GRID_ROWS, 0);
    gameBoardRemovePlant(board, 0, -1);
    gameBoardRemovePlant(board, 0, GRID_COLS);
    printf("✓ TEST 0 PASADO: Maneja inputs invalidos sin crash\n");

    // TEST 1: Pruebo remover de una celda ya vacía.
    // La lista de segmentos debe quedar intacta (1 solo nodo VACIO).
    resetRow(&board->rows[0]);
    gameBoardRemovePlant(board, 0, 4);
    RowSegment* seg = board->rows[0].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && 
        seg->length == GRID_COLS && seg->next == NULL) {
        printf("✓ TEST 1 PASADO: Remover en celda vacia no cambia nada\n");
    } else {
        printf("✗ TEST 1 FALLADO: Modifico estructura sin planta\n");
    }

    // TEST 2: Pruebo la Consigna 1 (Plantar 3, 4, 5 y sacar 4).
    // Esto prueba que al sacar la planta del medio, se crea un hueco (VACIO, length=1)
    // que NO se fusiona con sus vecinos (PLANTA).
    resetRow(&board->rows[0]);
    gameBoardAddPlant(board, 0, 3);
    gameBoardAddPlant(board, 0, 4);
    gameBoardAddPlant(board, 0, 5);
    gameBoardRemovePlant(board, 0, 4);
    
    // Verifico el estado: PLANTA en 3, VACIO en 4, PLANTA en 5.
    seg = board->rows[0].first_segment;
    int found_plant_3 = 0, found_vacio_4 = 0, found_plant_5 = 0;
    while (seg != NULL) {
        if (seg->status == STATUS_PLANTA && seg->start_col == 3) found_plant_3 = 1;
        if (seg->status == STATUS_VACIO && seg->start_col == 4 && seg->length == 1) found_vacio_4 = 1;
        if (seg->status == STATUS_PLANTA && seg->start_col == 5) found_plant_5 = 1;
        seg = seg->next;
    }
    
    if (found_plant_3 && found_vacio_4 && found_plant_5) {
        printf("✓ TEST 2 PASADO: Sacar planta del medio (no fusiona con plantas adyacentes)\n");
    } else {
        printf("✗ TEST 2 FALLADO: Estructura incorrecta al sacar planta del medio\n");
    }

    // TEST 3: Pruebo la Consigna 2 (Seguir del caso anterior y sacar 3).
    // Estado actual: [VACIO 0-2] [PLANTA 3] [VACIO 4] [PLANTA 5] [VACIO 6-8]
    // Al sacar 3, [VACIO 3] debe fusionarse con [VACIO 4] y con [VACIO 0-2].
    // El resultado debe ser un gran segmento [VACIO 0-4].
    gameBoardRemovePlant(board, 0, 3);
    
    seg = board->rows[0].first_segment;
    int found_fused_vacio = 0;
    int still_has_plant_5 = 0;
    while (seg != NULL) {
        // Busco un segmento VACIO que cubra el rango [0-4]
        if (seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 5) {
            found_fused_vacio = 1;
        }
        if (seg->status == STATUS_PLANTA && seg->start_col == 5) {
            still_has_plant_5 = 1;
        }
        seg = seg->next;
    }
    
    if (found_fused_vacio && still_has_plant_5) {
        printf("✓ TEST 3 PASADO: Fusion de segmentos vacios adyacentes (Triple Fusión)\n");
    } else {
        printf("✗ TEST 3 FALLADO: No fusiono correctamente segmentos vacios\n");
    }

    // TEST 4: Pruebo la Consigna 3 (Llenar fila y sacar del medio).
    // Esto prueba que se crea un hueco (VACIO, length=1) aislado entre Plantas.
    resetRow(&board->rows[1]);
    for (int c = 0; c < GRID_COLS; c++) {
        gameBoardAddPlant(board, 1, c);
    }
    gameBoardRemovePlant(board, 1, 4); // Saco la del medio
    
    seg = board->rows[1].first_segment;
    int plant_count = 0;
    int has_vacio_at_4 = 0;
    while (seg != NULL) {
        if (seg->status == STATUS_PLANTA) plant_count++;
        if (seg->status == STATUS_VACIO && seg->start_col == 4 && seg->length == 1) {
            has_vacio_at_4 = 1;
        }
        seg = seg->next;
    }
    
    if (plant_count == 8 && has_vacio_at_4) {
        printf("✓ TEST 4 PASADO: Llenar fila y sacar del medio\n");
    } else {
        printf("✗ TEST 4 FALLADO: Error al sacar planta de fila llena\n");
    }

    // TEST 5: Prueba avanzada: Remover planta aislada (Triple Fusión).
    // Planto en 4 (VACIO 0-3, PLANTA 4, VACIO 5-8).
    // Al remover 4, los 3 segmentos deben fusionarse en uno solo (VACIO 0-8).
    resetRow(&board->rows[2]);
    gameBoardAddPlant(board, 2, 4);
    gameBoardRemovePlant(board, 2, 4);
    
    seg = board->rows[2].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && 
        seg->length == GRID_COLS && seg->next == NULL) {
        printf("✓ TEST 5 PASADO: Remover planta aislada (triple fusion)\n");
    } else {
        printf("✗ TEST 5 FALLADO: No fusiono correctamente en fila vacia\n");
    }

    // TEST 6: Prueba avanzada: Fusión al inicio.
    // Planto en 0 (PLANTA 0, VACIO 1-8).
    // Al remover 0, debe fusionarse con el segmento 'next' y quedar (VACIO 0-8).
    resetRow(&board->rows[2]);
    gameBoardAddPlant(board, 2, 0);
    gameBoardRemovePlant(board, 2, 0);
    
    seg = board->rows[2].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && 
        seg->length == GRID_COLS && seg->next == NULL) {
        printf("✓ TEST 6 PASADO: Remover al inicio y fusionar\n");
    } else {
        printf("✗ TEST 6 FALLADO: No fusiono al remover del inicio\n");
    }

    // TEST 7: Prueba avanzada: Fusión al final.
    // Planto en 8 (VACIO 0-7, PLANTA 8).
    // Al remover 8, debe fusionarse con el segmento 'prev' y quedar (VACIO 0-8).
    resetRow(&board->rows[3]);
    gameBoardAddPlant(board, 3, 8);
    gameBoardRemovePlant(board, 3, 8);
    
    seg = board->rows[3].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && 
        seg->length == GRID_COLS && seg->next == NULL) {
        printf("✓ TEST 7 PASADO: Remover al final y fusionar\n");
    } else {
        printf("✗ TEST 7 FALLADO: No fusiono al remover del final\n");
    }

    // TEST 8: Prueba avanzada: Fusiones múltiples complejas.
    resetRow(&board->rows[4]);
    // Dejo huecos en columnas impares
    for (int c = 0; c < GRID_COLS; c += 2) {
        gameBoardAddPlant(board, 4, c);
    }
    // Remuevo plantas para forzar fusiones
    gameBoardRemovePlant(board, 4, 0);  // Fusiona con [VACIO 1]
    gameBoardRemovePlant(board, 4, 2);  // Fusiona [VACIO 0-1] con [VACIO 2] y [VACIO 3]
    
    seg = board->rows[4].first_segment;
    int found_large_vacio = 0;
    while (seg != NULL) {
        if (seg->status == STATUS_VACIO && seg->start_col == 0 && seg->length == 4) {
            found_large_vacio = 1; // Debería existir un [VACIO 0-3]
        }
        seg = seg->next;
    }
    
    if (found_large_vacio) {
        printf("✓ TEST 8 PASADO: Fusiones multiples complejas\n");
    } else {
        printf("✗ TEST 8 FALLADO: No manejo fusiones complejas\n");
    }

    // TEST 9: Stress test: Llenar y vaciar la fila.
    // Esto asegura que la lógica de fusión es robusta y puede
    // devolver la fila a su estado inicial (1 solo segmento VACIO).
    resetRow(&board->rows[4]);
    for (int c = 0; c < GRID_COLS; c++) {
        gameBoardAddPlant(board, 4, c);
    }
    for (int c = GRID_COLS - 1; c >= 0; c--) { // Vacio de derecha a izquierda
        gameBoardRemovePlant(board, 4, c);
    }
    
    seg = board->rows[4].first_segment;
    if (seg != NULL && seg->status == STATUS_VACIO && seg->start_col == 0 && 
        seg->length == GRID_COLS && seg->next == NULL) {
        printf("✓ TEST 9 PASADO: Stress test - volver a fila vacia\n");
    } else {
        printf("✗ TEST 9 FALLADO: No volvio a estado inicial\n");
    }

    gameBoardDelete(board);
    printf("========================================\n");
    printf("Tests completados para gameBoardRemovePlant\n");
    printf("========================================\n\n");
}






// ========== TESTS gameBoardAddZombie ==========


/**
 * Pruebas para gameBoardAddZombie.
 * La idea es validar que la inserción en la lista de zombies
 * funcione, cumpla con los casos de la consigna (escalabilidad)
 * y que los datos se inicialicen correctamente.
 */
void testGameBoardAddZombie() {
    printf("\n========= TESTS gameBoardAddZombie =========\n");
    
    // TEST 0: Pruebo la robustez de la función ante inputs inválidos.
    GameBoard* board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al crear tablero para tests\n");
        return;
    }
    
    // Pruebo con board NULL y filas fuera de rango
    gameBoardAddZombie(NULL, 0);
    gameBoardAddZombie(board, -1);
    gameBoardAddZombie(board, GRID_ROWS);
    
    // Verifico que la lista siga vacía
    int all_empty = 1;
    for (int i = 0; i < GRID_ROWS; i++) {
        if (board->rows[i].first_zombie != NULL) {
            all_empty = 0;
            break;
        }
    }
    
    if (all_empty) {
        printf("✓ TEST 0 PASADO: Maneja inputs invalidos sin agregar zombies\n");
    } else {
        printf("✗ TEST 0 FALLADO: Agrego zombie con input invalido\n");
    }
    
    // TEST 1: Pruebo la Consigna 1 (Agregar a lista existente).
    // Agrego 3 zombies y cuento. Luego agrego 1 más y vuelvo a contar.
    int test_row = 2;
    for (int i = 0; i < 3; i++) {
        gameBoardAddZombie(board, test_row);
    }
    
    int count = 0;
    ZombieNode* current = board->rows[test_row].first_zombie;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    
    if (count == 3) {
        gameBoardAddZombie(board, test_row); // Agrego el cuarto
        
        count = 0;
        current = board->rows[test_row].first_zombie;
        while (current != NULL) {
            count++;
            current = current->next;
        }
        
        if (count == 4) {
            printf("✓ TEST 1 PASADO: Tomar lista de 3 zombies y agregar 1 mas\n");
        } else {
            printf("✗ TEST 1 FALLADO: Esperados 4, encontrados %d\n", count);
        }
    } else {
        printf("✗ TEST 1 FALLADO: No se crearon los 3 zombies iniciales\n");
    }
    
    // TEST 2: Pruebo la Consigna 2 (Test de escalabilidad).
    // La idea es verificar que la lista enlazada puede crecer
    // mucho más que un array estático, probando con 10000 nodos.
    printf("TEST 2: Creando 10000 zombies (puede tomar un momento)...\n");
    
    gameBoardDelete(board);
    board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al recrear tablero\n");
        return;
    }
    
    int large_count = 10000;
    int row_test = 1;
    
    for (int i = 0; i < large_count; i++) {
        gameBoardAddZombie(board, row_test);
    }
    
    // Cuento los zombies agregados
    count = 0;
    current = board->rows[row_test].first_zombie;
    while (current != NULL && count <= large_count) {
        count++;
        current = current->next;
    }
    
    if (count == large_count) {
        printf("✓ TEST 2 PASADO: Crear lista de 10000 zombies\n");
    } else {
        printf("✗ TEST 2 FALLADO: Esperados %d, encontrados %d\n", large_count, count);
    }
    
    // TEST 3: Prueba avanzada: Verifico la inicialización de datos.
    // Me aseguro que un zombie nuevo tenga los valores correctos
    // (vida, posición, estado, etc.).
    gameBoardDelete(board);
    board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al recrear tablero\n");
        return;
    }
    
    gameBoardAddZombie(board, 0);
    ZombieNode* zombie = board->rows[0].first_zombie;
    
    if (zombie != NULL) {
        int init_ok = 1;
        if (zombie->zombie_data.vida != 100) init_ok = 0;
        if (zombie->zombie_data.activo != 1) init_ok = 0;
        if (zombie->zombie_data.row != 0) init_ok = 0;
        if (zombie->zombie_data.pos_x != SCREEN_WIDTH) init_ok = 0;
        
        if (init_ok) {
            printf("✓ TEST 3 PASADO: Zombie inicializado correctamente\n");
        } else {
            printf("✗ TEST 3 FALLADO: Inicializacion incorrecta\n");
        }
    } else {
        printf("✗ TEST 3 FALLADO: No se creo el zombie\n");
    }
    
    // TEST 4: Prueba avanzada: Verifico la independencia de las filas.
    // Agrego zombies en distintas filas y me aseguro que no se mezclen.
    gameBoardDelete(board);
    board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al recrear tablero\n");
        return;
    }
    
    gameBoardAddZombie(board, 0);
    gameBoardAddZombie(board, 2);
    gameBoardAddZombie(board, 4);
    
    int test4_ok = 1;
    for (int i = 0; i < GRID_ROWS; i++) {
        int expected = (i == 0 || i == 2 || i == 4) ? 1 : 0;
        
        count = 0;
        current = board->rows[i].first_zombie;
        while (current != NULL) {
            count++;
            current = current->next;
        }
        
        if (count != expected) test4_ok = 0;
    }
    
    if (test4_ok) {
        printf("✓ TEST 4 PASADO: Zombies en filas independientes\n");
    } else {
        printf("✗ TEST 4 FALLADO: Error en distribucion por filas\n");
    }
    
    // TEST 5: Prueba avanzada: Verifico la lógica de inserción (LIFO).
    // La función debe insertar al *principio* de la lista (head insertion).
    gameBoardDelete(board);
    board = gameBoardNew();
    if (board == NULL) {
        printf("✗ Error al recrear tablero\n");
        return;
    }
    
    // Agrego 3 zombies con vidas distintas para identificarlos
    gameBoardAddZombie(board, 3);
    board->rows[3].first_zombie->zombie_data.vida = 50;  // 1ro
    gameBoardAddZombie(board, 3);
    board->rows[3].first_zombie->zombie_data.vida = 75;  // 2do
    gameBoardAddZombie(board, 3);
    board->rows[3].first_zombie->zombie_data.vida = 90;  // 3ro (debe ser el 1ro en la lista)
    
    // Verifico el orden: 90 -> 75 -> 50
    current = board->rows[3].first_zombie;
    if (current && current->zombie_data.vida == 90 &&
        current->next && current->next->zombie_data.vida == 75 &&
        current->next->next && current->next->next->zombie_data.vida == 50) {
        printf("✓ TEST 5 PASADO: Zombies se agregan al inicio (LIFO)\n");
    } else {
        printf("✗ TEST 5 FALLADO: Orden incorrecto en la lista\n");
    }
    
    gameBoardDelete(board);
    printf("========================================\n");
    printf("Tests completados para gameBoardAddZombie\n");
    printf("========================================\n\n");
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



// ========== MAIN ==========

/**
 * La idea es inicializar SDL, crear el GameBoard dinámico,
 * correr los tests, y luego entrar en el bucle principal (Game Loop).
 * El bucle maneja eventos (input), actualiza el estado (Update)
 * y dibuja (Draw), hasta que 'game_over' es verdadero.
 */
int main(int argc, char* args[]) {
    // Inicialización de SDL y texturas
    srand(time(NULL));
    if (!inicializar()) return 1;

    // Creo el GameBoard dinámico
    game_board = gameBoardNew();

    // --- Ejecución de Tests ---
    // antes de arrancar el juego.
    testStrDuplicate();
    testStrCompare(); 
    testStrConcatenate();  
    testGameBoardAddZombie();  
    testGameBoardRemovePlant();   
    testGameBoardAddPlant();


    SDL_Event e;
    int game_over = 0;

    // Inicia el Game Loop
    while (!game_over) {
        
        // 1. MANEJO DE EVENTOS (Input)
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                game_over = 1;
            }
            // Actualizo la posición del cursor en la grilla
            if (e.type == SDL_MOUSEMOTION) {
                int mouse_x = e.motion.x;
                int mouse_y = e.motion.y;
                if (mouse_x >= GRID_OFFSET_X && mouse_x < GRID_OFFSET_X + GRID_WIDTH &&
                    mouse_y >= GRID_OFFSET_Y && mouse_y < GRID_OFFSET_Y + GRID_HEIGHT) {
                    cursor.col = (mouse_x - GRID_OFFSET_X) / CELL_WIDTH;
                    cursor.row = (mouse_y - GRID_OFFSET_Y) / CELL_HEIGHT;
                }
            }
            // Agrego una planta al hacer click
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                gameBoardAddPlant(game_board, cursor.row, cursor.col);
            }
        }

        // 2. ACTUALIZAR ESTADO (Update)
        gameBoardUpdate(game_board);
        
        // 3. DIBUJAR (Draw)
        gameBoardDraw(game_board);

        // 4. CHEQUEO DE GAME OVER (Consigna)
        // La idea es replicar la lógica del juego base, pero
        // iterando sobre las nuevas listas de zombies de cada fila.
        for (int r = 0; r < GRID_ROWS; r++) {
            ZombieNode* current = game_board->rows[r].first_zombie;
            while (current != NULL) {
                Zombie* z = &current->zombie_data;
                
                // La condición es la misma que el juego base:
                // Si el zombie está activo y su 'x' supera la línea de la casa.
                if (z->activo && z->rect.x < GRID_OFFSET_X - z->rect.w) {
                    printf("GAME OVER - Un zombie llego a tu casa!\n");
                    game_over = 1;
                    break; // Dejo de chequear zombies en esta fila
                }
                current = current->next;
            }
            if (game_over) break; // Dejo de chequear las otras filas
        }

        // Control de FPS (aprox 60 FPS)
        SDL_Delay(16);
    }

    // 5. LIMPIEZA
    // Libero toda la memoria dinámica pedida
    gameBoardDelete(game_board);
    cerrar(); // Libero texturas y cierro SDL
    return 0;
}






// Uso de la IA en el desarrollo del tp: 

/**
 * En una primera instancia utilizamos la IA para ayudarnos a evaluar si realmente entendíamos los conceptos vistos en clase sobre programación en C. 
 * Una vez teníamos los conceptos claros, usamos la IA para que nos ayude a entender a fondo la consigna y para armar un plan estructurado para comenezar a desarrollar el código. 
 * Luego, por cada función/tests que debíamos implementar, primero la intentamos desarrollar nosotros mismos sin ayuda de IA haciendo primero un pseudocódigo y usando díagramas para armar toda la lógica de las funciones.
 * Luego le pedimos que nos corriga lo que hicimos. 
 * En caso de encontrar algún error o inconsistencia, le pedimos que nos explique el por qué y nos ayude a corregirlo. Para realemente aprender como se debe implementar cada funcion. 
 * Sin embargo, en varias ocaciones la IA nos daba soluciones que no eran correctas o que no cumplían con todos los requisitos de la consigna por eso aprendimos que es muy importante siempre revisar y entender a fondo el código que la IA nos provee.
 * Finalmente una vez que teniamos todo el codigo desarrollado, verificamos en sea correcto chequeando con tres IAs diferentes cada función (Grok, Claude, y Gemini). 
 * Por último utilizamos IA para que nos ayude a emprolijar todo el código.
*/
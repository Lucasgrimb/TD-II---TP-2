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

void gameBoardDelete(GameBoard* board) {
    // TODO: Liberar toda la memoria dinámica.
    // TODO: Recorrer cada GardenRow.
    // TODO: Liberar todos los RowSegment (y los planta_data si existen).
    // TODO: Liberar todos los ZombieNode.
    // TODO: Finalmente, liberar el GameBoard.
    printf("Función gameBoardDelete no implementada.\n");
}

int gameBoardAddPlant(GameBoard* board, int row, int col) {
    // TODO: Encontrar la GardenRow correcta.
    // TODO: Recorrer la lista de RowSegment hasta encontrar el segmento VACIO que contenga a `col`.
    // TODO: Si se encuentra y tiene espacio, realizar la lógica de DIVISIÓN de segmento.
    // TODO: Crear la nueva `Planta` con memoria dinámica y asignarla al `planta_data` del nuevo segmento.
    printf("Función gameBoardAddPlant no implementada.\n");
    return 0;
}

void gameBoardRemovePlant(GameBoard* board, int row, int col) {
    // TODO: Similar a AddPlant, encontrar el segmento que contiene `col`.
    // TODO: Si es un segmento de tipo PLANTA, convertirlo a VACIO y liberar el `planta_data`.
    // TODO: Implementar la lógica de FUSIÓN con los segmentos vecinos si también son VACIO.
    printf("Función gameBoardRemovePlant no implementada.\n");
}

void gameBoardAddZombie(GameBoard* board, int row) {
    // TODO: Crear un nuevo ZombieNode con memoria dinámica.
    // TODO: Inicializar sus datos (posición, vida, animación, etc.).
    // TODO: Agregarlo a la lista enlazada simple de la GardenRow correspondiente.
    printf("Función gameBoardAddZombie no implementada.\n");
}

void gameBoardUpdate(GameBoard* board) {
    if (!board) return;
    // TODO: Re-implementar la lógica de `actualizarEstado` usando las nuevas estructuras.
    // TODO: Recorrer las listas de zombies de cada fila para moverlos y animarlos.
    // TODO: Recorrer las listas de segmentos de cada fila para gestionar los cooldowns y animaciones de las plantas.
    // TODO: Actualizar la lógica de disparo, colisiones y spawn de zombies.
}

void gameBoardDraw(GameBoard* board) {
    if (!board) return;
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex_background, NULL, NULL);

    // TODO: Re-implementar la lógica de `dibujar` usando las nuevas estructuras.
    // TODO: Recorrer las listas de segmentos para dibujar las plantas.
    // TODO: Recorrer las listas de zombies para dibujarlos.
    // TODO: Dibujar las arvejas y el cursor.

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

        // TODO: Agregar la lógica para ver si un zombie llegó a la casa y terminó el juego

        SDL_Delay(16);
    }

    gameBoardDelete(game_board);
    cerrar();
    return 0;
}

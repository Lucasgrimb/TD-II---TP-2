# Makefile TP2
# Compilador de C a utilizar
CC = clang
# Flags para el compilador:
# -g: Incluye información de debugging (para usar con gdb)
# -Wall: Para que aparezcan todos los warnings
# `pkg-config ...`: Obtiene automáticamente las rutas de los headers de SDL2
CFLAGS = -g -Wall `pkg-config --cflags sdl2 SDL2_image`
# Flags para el linker:
LIBS = `pkg-config --libs sdl2 SDL2_image`
# Nombre del archivo ejecutable que se generará
EXEC = juego
# Archivos fuente (.c) a compilar.
# Cambien esta línea para compilar el archivo que quieran:
#SRCS = juego_base.c
#SRCS = solucion_esqueleto.c
SRCS = tp_resuelto.c
# --- Reglas de Compilación ---
# La regla por defecto: se ejecuta cuando escribís solo "make"
# Compila el ejecutable.
all: $(EXEC)
# Regla para crear el archivo ejecutable
# Depende de los archivos fuente.
$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) -o $(EXEC) $(SRCS) $(LIBS)
# Regla para limpiar el proyecto: se ejecuta al escribir "make clean"
# Borra el archivo ejecutable generado.
clean:
	rm -f $(EXEC)

# Regla extra para ejecutar el juego
run: $(EXEC)
	./$(EXEC)

# Declara que "all", "clean" y "run" no son nombres de archivos
.PHONY: all clean run
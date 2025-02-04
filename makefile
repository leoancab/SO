# Compilador y opciones
CC = gcc
CFLAGS = -pthread

# Programas a generar
all: publicador desenfocador realzador

# Regla para compilar el publicador
publicador: publicador.c bmp.c filter.c bmp.h filter.h
	$(CC) publicador.c bmp.c filter.c -o publicador $(CFLAGS) -lrt

# Regla para compilar el desenfocador
desenfocador: desenfocador.c bmp.c filter.c bmp.h filter.h
	$(CC) desenfocador.c bmp.c filter.c -o desenfocador $(CFLAGS) -lrt

# Regla para compilar el realzador
realzador: realzador.c bmp.c filter.c bmp.h filter.h
	$(CC) realzador.c bmp.c filter.c -o realzador $(CFLAGS) -lrt

# Limpieza de los binarios generados
clean:
	rm -f publicador desenfocador realzador

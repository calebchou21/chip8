#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#include <SDL2/SDL.h>

#define MEMORY_SIZE 4096
#define PROGRAM_START 0x200
#define MAX_PROGRAM_SIZE (MEMORY_SIZE - PROGRAM_START)
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define PIXEL_SCALE 10

typedef struct {
    uint8_t memory[MEMORY_SIZE];
    uint8_t V[16];
    uint16_t I;
    uint16_t PC;
    uint8_t SP;
    uint16_t stack[16];
    uint8_t display[SCREEN_WIDTH * SCREEN_HEIGHT];
    int debug;
} Chip8;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
} SDLContext;

int sdl_init(SDLContext *context);
void sdl_cleanup(SDLContext *context);
void draw_display(SDL_Renderer *renderer, const Chip8 *chip8);
void chip8_init(Chip8 *chip8);
void chip8_debug(Chip8 *chip8, uint16_t instruction);
int load_rom(const char *path, Chip8 *chip8);
void chip8_cycle(Chip8 *chip8);

#endif

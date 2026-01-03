#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

#include <SDL2/SDL.h>

#define CYCLES_PER_FRAME 10
#define MEMORY_SIZE 4096
#define PROGRAM_START 0x200
#define MAX_PROGRAM_SIZE (MEMORY_SIZE - PROGRAM_START)
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define PIXEL_SCALE 10
#define SAMPLE_RATE 44100
#define BEEP_FREQ 440

typedef struct {
    uint8_t memory[MEMORY_SIZE];
    uint8_t V[16];
    uint16_t I;
    uint16_t PC;
    uint8_t SP;
    uint16_t stack[16];
    uint8_t display[SCREEN_WIDTH * SCREEN_HEIGHT];
    uint8_t keypad[16];
    uint8_t delay_timer;
    uint8_t sound_timer;
    int sound_playing;
    int debug;
} Chip8;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    const uint8_t *key_states;
} SDLContext;

static int audio_phase = 0;
void audio_callback(void *userdata, uint8_t *stream, int len);

int sdl_init(SDLContext *context, Chip8 *chip8);
void sdl_cleanup(SDLContext *context);
void handle_input(Chip8 *chip8, const uint8_t *key_states);
void draw_display(SDL_Renderer *renderer, const Chip8 *chip8);
void chip8_init(Chip8 *chip8);
void chip9_update_timers(Chip8 *chip8);
void chip8_debug(Chip8 *chip8, uint16_t instruction);
int load_rom(const char *path, Chip8 *chip8);
void chip8_cycle(Chip8 *chip8);

#endif

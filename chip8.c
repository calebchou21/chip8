#include "chip8.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int audio_phase = 0;

static const uint8_t FONTSET[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void audio_callback(void *userdata, uint8_t *stream, int len) {
    Chip8 *chip8 = (Chip8 *)userdata;
    int16_t *buffer = (int16_t *)stream;
    int samples = len / sizeof(int16_t);

    if (!chip8->sound_playing) {
        memset(stream, 0, len);
        return;
    }

    int period = SAMPLE_RATE / BEEP_FREQ;

    for (int i = 0; i < samples; i++) {
        buffer[i] = (audio_phase < period / 2) ? 8000 : -8000;
        audio_phase = (audio_phase + 1) % period;
    }
}

int sdl_init(SDLContext *context, Chip8 *chip8) {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "Error initializing SDL %s\n", SDL_GetError());
        return -1;
    }

    context->window = SDL_CreateWindow(
            "chip8", 
            SDL_WINDOWPOS_UNDEFINED, 
            SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH * PIXEL_SCALE,
            SCREEN_HEIGHT * PIXEL_SCALE,
            SDL_WINDOW_SHOWN
        );
    if (!context->window) {
        fprintf(stderr, "Error creating window: %s\n", SDL_GetError());
        return -1;
    }

    context->renderer = SDL_CreateRenderer(
            context->window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );
    if(!context->renderer) {
        fprintf(stderr, "Error getting renderer: %s\n", SDL_GetError());
        return -1;
    }

    const uint8_t *key_states = SDL_GetKeyboardState(NULL);
    context->key_states = key_states;

    SDL_AudioSpec spec = {0};
    spec.freq = SAMPLE_RATE;
    spec.format = AUDIO_S16SYS;
    spec.channels = 1;
    spec.samples = 512;
    spec.callback = audio_callback;
    spec.userdata = chip8;

    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);

    return 1;
}

void draw_display(SDL_Renderer *renderer, const Chip8 *chip8) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (chip8->display[y * SCREEN_WIDTH + x]) {
                SDL_Rect pixel = {
                    x * PIXEL_SCALE,
                    y * PIXEL_SCALE,
                    PIXEL_SCALE,
                    PIXEL_SCALE
                };
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

void sdl_cleanup(SDLContext *context) {
    SDL_DestroyWindow(context->window);
    SDL_Quit();
}

void handle_input(Chip8 *chip8, const uint8_t *key_states) {
    static const SDL_Scancode scancodes[16] = {
        SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,
        SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C,
        SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V
    }; 

    for (int i = 0; i < 16; i++) {
        chip8->keypad[i] = key_states[scancodes[i]];
    }
}

void chip8_init(Chip8 *chip8) {
    memset(chip8, 0, sizeof(Chip8));
    chip8->PC = 0x200;

    for (int i = 0; i < 80; i++) {
        chip8->memory[0x050 + i] = FONTSET[i];
    }
}

void chip8_update_timers(Chip8 *chip8) {
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }

    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;

        if (!chip8->sound_playing) {
            chip8->sound_playing = 1;
        }
    } else {
        chip8->sound_playing = 0;
    }
}

void chip8_debug(Chip8 *chip8, uint16_t instruction) {
    printf("PC: %X | Instr: %04X | SP: %d | I: %X | V: [",
            chip8->PC, instruction, chip8->SP, chip8->I);
    for (int i = 0; i < 16; i++) {
        printf("%X", chip8->V[i]);
        if (i != 15) printf(" ");
    }
    printf("]\n");
}

int load_rom(const char *path, Chip8 *chip8) {
    FILE* fptr;
    fptr = fopen(path, "rb");

    if (fptr == NULL) {
        perror("File not found.");
        return 0;
    }

    uint8_t buffer[MAX_PROGRAM_SIZE];
    int len = fread(buffer, sizeof (uint8_t), MAX_PROGRAM_SIZE, fptr);
    
    for (int i = 0; i < len; i++) {
        chip8->memory[PROGRAM_START + i] = buffer[i];
    }

    fclose(fptr);
    return 1;
}

void chip8_cycle(Chip8 *chip8) {
    uint16_t current_instruction = (chip8->memory[chip8->PC] << 8) | chip8->memory[chip8->PC + 1];
    
    if (chip8->debug) {
        chip8_debug(chip8, current_instruction);
    }

    chip8->PC += 2;

    uint8_t F = (current_instruction & 0xf000) >> 12;
    uint8_t X = (current_instruction & 0x0f00) >> 8;
    uint8_t Y = (current_instruction & 0x00f0) >> 4;
    uint8_t N = (current_instruction & 0x000f);
    uint8_t NN = current_instruction & 0x00ff;
    uint16_t NNN = current_instruction & 0x0fff;

    switch(F) {
        case 0x0:
            switch(NNN) {
                case 0x0e0: // Clear display
                    memset(chip8->display, 0, sizeof(chip8->display));
                    break;
                case 0x0ee: // Return from subroutine
                    if (chip8->SP == 0) {
                        return;
                    }
                    chip8->SP -= 1;
                    chip8->PC = chip8->stack[chip8->SP];
                    break;
            }
            break;
        case 0x1: // Jump
           chip8->PC = NNN;
           break;
        case 0x2: // Call subroutine at NNN
            chip8->stack[chip8->SP] = chip8->PC;
            chip8->SP++;
            chip8->PC = NNN;
            break;
        case 0x3: // Skip if VX == NN
            if (chip8->V[X] == NN) {
                chip8->PC += 2;
            }
            break;
        case 0x4: // Skip if VX != NN
            if (chip8->V[X] != NN) {
                chip8->PC += 2;
            }
            break;
        case 0x5: // Skip if VX == VY
            if (chip8->V[X] == chip8->V[Y]) {
                chip8->PC += 2;
            }
            break;
        case 0x9: // Skip if VX != VY
            if (chip8->V[X] != chip8->V[Y]) {
                chip8->PC += 2;
            }
            break;
        case 0x6: // Set VX to NN
            chip8->V[X] = NN;
            break;
        case 0x7: // Add NN to VX
            chip8->V[X] += NN;
            break;
        case 0x8:
            switch(N) {
                case 0x0: // Set VX to VY
                    chip8->V[X] = chip8->V[Y];
                    break;
                case 0x1: // Set VX to VX | VY
                    chip8->V[X] = chip8->V[X] | chip8->V[Y];
                    break;
                case 0x2: // Set VX to VX & VY
                    chip8->V[X] = chip8->V[X] & chip8->V[Y];
                    break;
                case 0x3: // Set VX to VX ^ VY
                    chip8->V[X] = chip8->V[X] ^ chip8->V[Y];
                    break;
                case 0x4: { // Set VX to VX + VY
                    uint16_t sum = chip8->V[X] + chip8->V[Y];
                    chip8->V[X] = sum & 0xff;
                    chip8->V[0xf] = (sum > 0xff);
                    break;
                }
                case 0x5: { // Set VX to VX - VY
                    int diff = chip8->V[X] - chip8->V[Y];
                    chip8->V[X] = diff;
                    if (diff >= 0) {
                        chip8->V[0xf] = 1;
                    } else {
                        chip8->V[0xf] = 0;
                    }
                    break;
                }
                case 0x7: { // Set VX to VY - VX
                    int diff = chip8->V[Y] - chip8->V[X];
                    chip8->V[X] = diff;
                    if (diff >= 0) {
                        chip8->V[0xf] = 1;
                    } else {
                        chip8->V[0xf] = 0;
                    }
                    break;
                }
                case 0x6: { // Shift VX 1 bit right
                    uint8_t bit = chip8->V[X] & 0x01;
                    chip8->V[X] >>= 1;
                    chip8->V[0xf] = bit;
                    break;
                }
                case 0xe: { // Shift VX 1 bit left
                    uint8_t bit = (chip8->V[X] & 0x80) >> 7;               
                    chip8->V[X] <<= 1;
                    chip8->V[0xf] = bit;
                    break;
                }
            }
            break;
        case 0xa: // Set I to NNN
            chip8->I = NNN;
            break;
        case 0xb: // Jump to NNN + V[0]
            chip8->PC = NNN + chip8->V[0];
            break;
        case 0xC: { // Random number
            int8_t rand_num = rand();
            chip8->V[X] = NN & rand_num;
            break;
        }
        case 0xD: { // Display
            uint8_t start_x = chip8->V[X] % SCREEN_WIDTH;
            uint8_t start_y = chip8->V[Y] % SCREEN_HEIGHT;
            chip8->V[0xf] = 0;

            for (int row = 0; row < N; row++) {
                if (start_y + row >= SCREEN_HEIGHT) break;

                uint8_t sprite_data = chip8->memory[chip8->I + row];
                for (int bit = 0; bit < 8; bit++) {
                    if (start_x + bit >= SCREEN_WIDTH) break;

                    uint8_t pixel = (sprite_data & (0x80 >> bit)) != 0;
                    uint16_t index = (start_y + row) * SCREEN_WIDTH + (start_x + bit);
                    if (pixel) {
                        if (chip8->display[index]) {
                            chip8->V[0xf] = 1;
                        }
                        chip8->display[index] ^= 1;
                    }
                }
            }
            break;
        }
        case 0xE: 
            switch (NN) {
                case 0x9E: // Skip if key VX pressed
                    if (chip8->keypad[chip8->V[X]]) {
                        chip8->PC += 2;
                    }
                    break;
                case 0xA1: // Skip if key VX not pressed
                    if (!chip8->keypad[chip8->V[X]]) {
                        chip8->PC += 2;
                    }
                    break;
            }
            break;
        case 0xF: 
            switch (NN) {
                case 0x07: // Set VX to delay timer
                    chip8->V[X] = chip8->delay_timer;
                    break;
                case 0x15: // Set delay timer to VX
                    chip8->delay_timer = chip8->V[X];
                    break;
                case 0x18: // Set sound timer to VX
                    chip8->sound_timer = chip8->V[X];
                    break;
                case 0x1E: { // Add VX to I
                    uint16_t result = chip8->I + chip8->V[X];
                    chip8->V[0xf] = (result > 0x0fff);
                    chip8->I = result;
                    break;
                }
                case 0x0A: { // Block until keypress
                    int key_pressed = -1;
                    
                    for (int i = 0; i < 16; i++) {
                        if (chip8->keypad[i]) {
                            key_pressed = i;
                            break;
                        }
                    }

                    if (key_pressed == -1) {
                        chip8->PC -= 2;
                    } else {
                        chip8->V[X] = key_pressed;
                    }
                    break;
                }
                case 0x29: { // Font character
                    uint8_t digit = chip8->V[X] & 0x0f;
                    chip8->I = 0x050 + digit * 5;
                    break;
                }
                case 0x33: // BCD
                    chip8->memory[chip8->I] = chip8->V[X] / 100;
                    chip8->memory[chip8->I + 1] = (chip8->V[X] % 100) / 10;
                    chip8->memory[chip8->I + 2] = chip8->V[X] % 10;
                    break;
                case 0x55: // Store memory
                    for (int i = 0; i <= X; i++) {
                        chip8->memory[chip8->I + i] = chip8->V[i];
                    }
                    break;
                case 0x65: // Load memory
                    for (int i = 0; i <= X; i++) {
                        chip8->V[i] = chip8->memory[chip8->I + i]; 
                    }
                    break;
            }
            break;
        default: 
            fprintf(stderr, "Invalid instruction: %X\n", current_instruction);
    }
}

int main(int argc, char *argv[]) {
    int debug = 0;
    if (argc > 3) {
        fprintf(stderr, "Usage: %s <rom_file> [-d]\n", argv[0]);
        return -1;
    }

    if (argc == 3 && strcmp(argv[2], "-d") == 0) {
        debug = 1;
    }
    
    srand(time(NULL));

    Chip8 chip8;
    chip8_init(&chip8);
    chip8.debug = debug;

    int success = load_rom(argv[1], &chip8);
    if (!success) {
        return -1;
    }

    SDLContext context;
    success = sdl_init(&context, &chip8);
    if(!success) {
        return -1;
    }

    SDL_Event e;
    int running = 1;

    const uint32_t TIMER_INTERVAL = 1000 / 60;
    unsigned int last_timer_tick = SDL_GetTicks();

    while(running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
        }

        SDL_PumpEvents();
        
        handle_input(&chip8, context.key_states);

        for (int i = 0; i < CYCLES_PER_FRAME; i++) {
            chip8_cycle(&chip8);
        }
        
        uint32_t now = SDL_GetTicks();
        if (now - last_timer_tick >= TIMER_INTERVAL) {
            chip8_update_timers(&chip8);
            last_timer_tick += TIMER_INTERVAL;
        }
        
        draw_display(context.renderer, &chip8);
    }

    sdl_cleanup(&context);
}

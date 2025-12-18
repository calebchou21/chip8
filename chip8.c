#include "chip8.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

int sdl_init(SDLContext *context) {
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

void chip8_init(Chip8 *chip8) {
    memset(chip8, 0, sizeof(Chip8));
    chip8->PC = 0x200;

    for (int i = 0; i < 80; i++) {
        chip8->memory[0x050 + i] = FONTSET[i];
    }
}

void chip8_debug(Chip8 *chip8, uint16_t instruction) {
    printf("PC: %X | Instr: %X | SP: %d | I: %X | V: [",
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
    int len = fread(buffer, sizeof buffer[0], MAX_PROGRAM_SIZE, fptr);
    
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
        printf("Press enter to continue...");
        getchar();
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
                // Clear display
                case 0x0e0:
                    memset(chip8->display, 0, sizeof(chip8->display));
                    break;
                // Return from subroutine
                case 0x0ee:
                    if (chip8->SP == 0) {
                        return;
                    }
                    chip8->SP -= 1;
                    chip8->PC = chip8->stack[chip8->SP];
                    break;
            }
            break;
        // Jump
        case 0x1:
           chip8->PC = NNN;
           break;
        // Call subroutine at location NNN
        case 0x2:
            chip8->stack[chip8->SP] = chip8->PC;
            chip8->SP++;
            chip8->PC = NNN;
            break;
        // 0x3XNN: Skips one instruction if the value in VX is equal to NN
        case 0x3:
            if (chip8->V[X] == NN) {
                chip8->PC += 2;
            }
            break;
        // 0x4XNN Skips one instruction if the value in VX is not equal to NN
        case 0x4:
            if (chip8->V[X] != NN) {
                chip8->PC += 2;
            }
            break;
        // 0x5XY0 Skips one instruction if the values in VX and VY are equal
        case 0x5:
            if (chip8->V[X] == chip8->V[Y]) {
                chip8->PC += 2;
            }
            break;
        // 0x9XY0 Skips one instruction if the values in VX and VY are not equal
        case 0x9:
            if (chip8->V[X] != chip8->V[Y]) {
                chip8->PC += 2;
            }
            break;
        // 0x6XNN Sets register VX to value NN
        case 0x6:
            chip8->V[X] = NN;
            break;
        // 0x7XNN Adds the value NN to VX
        case 0x7:
            chip8->V[X] += NN;
            break;
        // Logical and arithmetic instructions
        case 0x8:
            switch(N) {
                // 0x8XY0 sets VX to the value of VY
                case 0x0:
                    chip8->V[X] = chip8->V[Y];
                    break;
                // 0x8XY1 sets VX to the bitwise OR of VX and VY
                case 0x1:
                    chip8->V[X] = chip8->V[X] | chip8->V[Y];
                    break;
                // 0x8XY2 sets VX to the bitwise AND of VX and VY
                case 0x2:
                    chip8->V[X] = chip8->V[X] & chip8->V[Y];
                    break;
                // 0x8XY3 sets VX to the bitwise XOR of VX and VY
                case 0x3:
                    chip8->V[X] = chip8->V[X] ^ chip8->V[Y];
                    break;
                // 0x8XY4 sets VX to the value of VX + VY
                case 0x4: {
                    uint16_t sum = chip8->V[X] + chip8->V[Y];
                    chip8->V[X] = sum & 0xff;
                    chip8->V[0xf] = (sum > 0xff);
                    break;
                }
                // 0x8XY5 sets VX to VX - VY
                case 0x5:
                    chip8->V[0xf] = (chip8->V[X] > chip8->V[Y]);
                    chip8->V[X] -= chip8->V[Y];
                    break;
                // 0x8XY7 sets VX to VY - VX
                case 0x7:
                    chip8->V[0xf] = (chip8->V[X] > chip8->V[Y]);
                    chip8->V[X] -= chip8->V[Y];
                    break;
                // 0x8XY6 shifts value in VX 1 bit to the right
                // TODO: ambigous instruction but I will ignore Y for now (fix later)
                case 0x6:
                    chip8->V[0xf] = chip8->V[X] & 0x01;
                    chip8->V[X] >>= 1;
                    break;
                // 08XYE shifts value in VX 1 bit to the left 
                // TODO: Same not as previous shift instruction
                case 0xe: 
                    chip8->V[0xf] = chip8->V[X] & 0x01;
                    chip8->V[X] <<= 1;
                    break;
            }
            break;
        // 0xANNN sets index register I to NNN
        case 0xa:
            chip8->I = NNN;
            break;
        // 0xBNNN jumps to address the address NNN plus the value in V0
        // TODO: Ambigious instruction -- going with original implementation but should make configurable
        case 0xb:
            chip8->PC = NNN + chip8->V[0];
            break;
        // 0xCXNN generates a random number and binarys ANDs it with NN, putting the result in VX
        case 0xC: {
            int8_t rand_num = rand();
            chip8->V[X] = NN & rand_num;
            break;
        }
        // Draws an N pixel tall sprite held at location I to the screen at position (X, Y) held in VX and VY
        case 0xD: {
            X = chip8->V[X] % SCREEN_WIDTH;
            Y = chip8->V[Y] % SCREEN_HEIGHT;
            chip8->V[0xf] = 0;

            for (int i = 0; i < N; i++) {
                if (Y > SCREEN_HEIGHT) break;

                uint8_t sprite_data = chip8->memory[chip8->I + i];
                uint8_t mask = 0x80;
                for (int j = 0; j < 8; j++) {
                    uint8_t pixel = sprite_data & mask;
                    if (X > SCREEN_WIDTH) break;
                    if (pixel && chip8->display[SCREEN_WIDTH * Y + X]) {
                        chip8->display[Y * X] = 0;
                        chip8->V[0xf] = 1;
                    }
                    else if (pixel && !chip8->display[SCREEN_WIDTH * Y + X]) {
                        chip8->display[SCREEN_WIDTH * Y + X] = 0;
                    }
                    X++;
                    mask >>= 1;
                }
                Y++;
            }
            break;
        }
        default: {
            fprintf(stderr, "Invalid instruction: %X\n", current_instruction);
        }
    }
}

int main(int argc, char *argv[]) {
    int debug;
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
    success = sdl_init(&context);
    if(!success) {
        return -1;
    }

    SDL_Event e;
    int running = 1;

    while(running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
        }

        chip8_cycle(&chip8);
        draw_display(context.renderer, &chip8);

        SDL_Delay(1); // Eventually implement timer instead.
    }

    sdl_cleanup(&context);
}

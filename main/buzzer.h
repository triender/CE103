// buzzer.h
#ifndef BUZZER_H
#define BUZZER_H

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Note structure
typedef struct {
    int frequency;
    int duration;
} Note;

// Function declarations
void buzzer_init(int gpio_num);
void play_tone(int frequency, int duration);
void play_melody(Note* melody, int length);
void play_melody_alt(int notes[], int durations[], int length);

#endif // BUZZER_H

// buzzer.c
#include "buzzer.h"

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_HIGH_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. ((2^13)-1) * 50% = 4096

static int buzzer_gpio_num;

void buzzer_init(int gpio_num) {
    buzzer_gpio_num = gpio_num;

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 2000,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL,
        .duty       = 0,
        .gpio_num   = buzzer_gpio_num,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER
    };
    ledc_channel_config(&ledc_channel);

    ledc_fade_func_install(0);
}

void play_tone(int frequency, int duration) {
    if (frequency == 0) {
        vTaskDelay(duration / portTICK_PERIOD_MS);
        return;
    }
    ledc_set_freq(LEDC_MODE, LEDC_TIMER, frequency);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    vTaskDelay(duration / portTICK_PERIOD_MS);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    vTaskDelay(50 / portTICK_PERIOD_MS); // Short pause between notes
}

void play_melody(Note* melody, int length) {
    for (int i = 0; i < length; i++)
        play_tone(melody[i].frequency, melody[i].duration);
}

void play_melody_alt(int notes[], int durations[], int num_notes) {
    for (int i = 0; i < num_notes; i++) {
        int note = notes[i];
        int duration = durations[i];

        if (note == 0)
            vTaskDelay(duration / portTICK_PERIOD_MS);
        else
            play_tone(note, duration);
    }
}
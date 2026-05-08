#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <cjson/cJSON.h>

#define COOLING_DEV_PATH "/sys/class/thermal/cooling_device5/cur_state"
#define CONFIG_PATH "/etc/fan-control.json"

typedef struct {
    int temp;
    int state;
} FanStage;

FanStage stages[4];

void load_config() {
    FILE *f = fopen(CONFIG_PATH, "rb");
    if (!f) {
        perror("Config file not found");
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc(len + 1);
    fread(data, 1, len, f);
    fclose(f);

    cJSON *json = cJSON_Parse(data);
    cJSON *s_array = cJSON_GetObjectItem(json, "stages");
    for (int i = 0; i < 4; i++) {
        cJSON *item = cJSON_GetArrayItem(s_array, i);
        stages[i].temp = cJSON_GetObjectItem(item, "temp")->valueint;
        stages[i].state = cJSON_GetObjectItem(item, "state")->valueint;
    }
    cJSON_Delete(json);
    free(data);
}

float read_w1_temp() {
    glob_t gstruct;
    if (glob("/sys/bus/w1/devices/28-*/w1_slave", 0, NULL, &gstruct) != 0) {
        if (gstruct.gl_pathc > 0) globfree(&gstruct);
        return -100.0;
    }

    FILE *f = fopen(gstruct.gl_pathv[0], "r");
    if (!f) {
        globfree(&gstruct);
        return -100.0;
    }

    char buf[256];
    float temp = 0;
    while (fgets(buf, sizeof(buf), f)) {
        char *t_ptr = strstr(buf, "t=");
        if (t_ptr) {
            temp = atof(t_ptr + 2) / 1000.0;
        }
    }

    fclose(f);
    globfree(&gstruct);
    return temp;
}

void set_fan_state(int state) {
    FILE *f = fopen(COOLING_DEV_PATH, "w");
    if (!f) return;
    fprintf(f, "%d", state);
    fclose(f);
}

int main() {
    load_config();
    int last_state = -1;

    while (1) {
        float current_temp = read_w1_temp();
        int target_state = 0;

        if (current_temp > -50.0) {
            for (int i = 3; i >= 0; i--) {
                if (current_temp >= stages[i].temp) {
                    target_state = stages[i].state;
                    break;
                }
            }

            if (target_state != last_state) {
                set_fan_state(target_state);
                last_state = target_state;
            }
        }

        sleep(5);
    }
    return 0;
}
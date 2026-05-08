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
    printf("[DEBUG] Apertura file config: %s\n", CONFIG_PATH);
    FILE *f = fopen(CONFIG_PATH, "rb");
    if (!f) {
        perror("Errore in reading config file");
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = malloc(len + 1);
    if (!data) {
        printf("Memory allocation failed\n");
        fclose(f);
        exit(1);
    }

    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("Parse error: %s\n", error_ptr);
        }
        free(data);
        exit(1);
    }

    cJSON *s_array = cJSON_GetObjectItem(json, "stages");
    if (!cJSON_IsArray(s_array)) {
        printf("Parse error: 'stages' is not a JSON array!\n");
        cJSON_Delete(json);
        free(data);
        exit(1);
    }

    int array_size = cJSON_GetArraySize(s_array);

    for (int i = 0; i < 4 && i < array_size; i++) {
        cJSON *item = cJSON_GetArrayItem(s_array, i);
        cJSON *temp = cJSON_GetObjectItem(item, "temp");
        cJSON *state = cJSON_GetObjectItem(item, "state");

        if (cJSON_IsNumber(temp) && cJSON_IsNumber(state)) {
            stages[i].temp = temp->valueint;
            stages[i].state = state->valueint;
            printf("  -> Loaded Stage %d: T >= %d, State %d\n", i, stages[i].temp, stages[i].state);
        } else {
            printf("  -> Error: Stage %d has missing or invalid data!\n", i);
        }
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
    printf("=== Fan Control Rock5B ===\n");
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
                printf("Temperature: %.2f°C, setting fan state to %d\n", current_temp, target_state);
                set_fan_state(target_state);
                last_state = target_state;
            }
        }

        sleep(5);
    }
    return 0;
}
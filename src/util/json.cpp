#include "json.h"

#include "core/asserts.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

size_t json_parse(const char* json_str, Json** json);
size_t json_parse_string(const char* json_str, Json** json);

Json* json_object() {
    Json* json = (Json*)malloc(sizeof(Json));
    json->type = JSON_TYPE_OBJECT;
    json->object.capacity = 1;
    json->object.length = 0;
    json->object.keys = (char**)malloc(json->object.capacity * sizeof(char**));
    json->object.values = (Json**)malloc(json->object.capacity * sizeof(Json*));

    return json;
}

Json* json_array() {
    Json* json = (Json*)malloc(sizeof(Json));
    json->type = JSON_TYPE_ARRAY;
    json->array.capacity = 1;
    json->array.length = 0;
    json->array.values = (Json**)malloc(json->array.capacity * sizeof(Json*));

    return json;
}

Json* json_string(const char* value) {
    Json* json = (Json*)malloc(sizeof(Json));
    json->type = JSON_TYPE_STRING;
    json->string.length = strlen(value);
    json->string.value = (char*)malloc((json->string.length + 1) * sizeof(char));
    strcpy(json->string.value, value);

    return json;
}

Json* json_number(double value) {
    Json* json = (Json*)malloc(sizeof(Json));
    json->type = JSON_TYPE_NUMBER;
    json->number.value = value;

    return json;
}

Json* json_boolean(bool value) {
    Json* json = (Json*)malloc(sizeof(Json));
    json->type = JSON_TYPE_BOOLEAN;
    json->boolean.value = value;
    
    return json;
}

Json* json_null() {
    Json* json = (Json*)malloc(sizeof(Json));
    json->type = JSON_TYPE_NULL;

    return json;
}

Json* json_from_ivec2(ivec2 value) {
    Json* json = json_object();
    json_object_set_number(json, "x", value.x);
    json_object_set_number(json, "y", value.y);
    return json;
}

ivec2 json_to_ivec2(Json* json) {
    return ivec2(
        (int)json_object_get_number(json, "x"),
        (int)json_object_get_number(json, "y")
    );
}

void json_free(Json* json) {
    if (json->type == JSON_TYPE_OBJECT) {
        for (size_t index = 0; index < json->object.length; index++) {
            free(json->object.keys[index]);
            json_free(json->object.values[index]);
        }
        free(json->object.keys);
        free(json->object.values);
    } 
    if (json->type == JSON_TYPE_ARRAY) {
        for (size_t index = 0; index < json->array.length; index++) {
            json_free(json->array.values[index]);
        }
        free(json->array.values);
    }
    if (json->type == JSON_TYPE_STRING) {
        free(json->string.value);
    }

    free(json);
}

const char* json_get_string(const Json* json) {
    GOLD_ASSERT(json != NULL && json->type == JSON_TYPE_STRING);
    return json->string.value;
}

double json_get_number(const Json* json) {
    GOLD_ASSERT(json != NULL && json->type == JSON_TYPE_NUMBER);
    return json->number.value;
}

bool json_get_boolean(const Json* json) {
    GOLD_ASSERT(json != NULL && json->type == JSON_TYPE_BOOLEAN);
    return json->boolean.value;
}

Json* json_object_get(const Json* json, const char* key) {
    GOLD_ASSERT(json->type == JSON_TYPE_OBJECT);
    for (size_t index = 0; index < json->object.length; index++) {
        if (strcmp(json->object.keys[index], key) == 0) {
            return json->object.values[index];
        }
    }

    return NULL;
}

const char* json_object_get_string(const Json* json, const char* key) {
    return json_get_string(json_object_get(json, key));
}

double json_object_get_number(Json* json, const char* key) {
    return json_get_number(json_object_get(json, key));
}

bool json_object_get_boolean(Json* json, const char* key) {
    return json_get_boolean(json_object_get(json, key));
}

void json_object_set(Json* json, const char* key, Json* value) {
    GOLD_ASSERT(json->type == JSON_TYPE_OBJECT);
    size_t index;
    for (index = 0; index < json->object.length; index++) {
        if (strcmp(json->object.keys[index], key) == 0) {
            break;
        }
    }

    // If we are about to overwrite an existing value, free the old value
    if (index < json->object.length) {
        json_free(json->object.values[index]);
    }

    // If index == object length it means that this is a new key
    if (index == json->object.length) {
        // Check if we need to grow the capacity
        if (json->object.length == json->object.capacity) {
            json->object.capacity *= 2;

            char** temp_keys = json->object.keys;
            json->object.keys = (char**)malloc(json->object.capacity * sizeof(char*));
            memcpy(json->object.keys, temp_keys, json->object.length * sizeof(char*));
            free(temp_keys);

            Json** temp_values = json->object.values;
            json->object.values = (Json**)malloc(json->object.capacity * sizeof(Json*));
            memcpy(json->object.values, temp_values, json->object.length * sizeof(Json*));
            free(temp_values);
        }

        // Increment the object length
        json->object.length++;

        // Allocate and store the key (this only happens on insertion)
        size_t key_length = strlen(key);
        json->object.keys[index] = (char*)malloc((key_length + 1) * sizeof(char));
        strcpy(json->object.keys[index], key);
    }

    json->object.values[index] = value;
}

void json_object_set_string(Json* json, const char* key, const char* value) {
    json_object_set(json, key, json_string(value));
}

void json_object_set_number(Json* json, const char* key, double value) {
    json_object_set(json, key, json_number(value));
}

void json_object_set_boolean(Json* json, const char* key, bool value) {
    json_object_set(json, key, json_boolean(value));
}

Json* json_array_get(const Json* json, size_t index) {
    GOLD_ASSERT(json->type == JSON_TYPE_ARRAY && index < json->array.length);
    return json->array.values[index];
}

const char* json_array_get_string(const Json* json, size_t index) {
    return json_get_string(json_array_get(json, index));
}

double json_array_get_number(const Json* json, size_t index) {
    return json_get_number(json_array_get(json, index));
}

bool json_array_get_boolean(const Json* json, size_t index) {
    return json_get_boolean(json_array_get(json, index));
}

void json_array_push(Json* json, Json* value) {
    GOLD_ASSERT(json->type == JSON_TYPE_ARRAY);

    if (json->array.length == json->array.capacity) {
        json->array.capacity *= 2;

        Json** temp_values = json->array.values;
        json->array.values = (Json**)malloc(json->array.capacity * sizeof(Json*));
        memcpy(json->array.values, temp_values, json->array.length * sizeof(Json*));
        free(temp_values);
    }

    json->array.values[json->array.length] = value;
    json->array.length++;
}

void json_array_push_string(Json* json, const char* value) {
    json_array_push(json, json_string(value));
}

void json_array_push_number(Json* json, double value) {
    json_array_push(json, json_number(value));
}

void json_array_push_boolean(Json* json, bool value) {
    json_array_push(json, json_boolean(value));
}

void json_array_set(Json* json, size_t index, Json* value) {
    GOLD_ASSERT(json->type == JSON_TYPE_ARRAY);

    if (index < json->array.length) {
        json_free(json->array.values[index]);
        json->array.values[index] = value;
    } else {
        json_array_push(json, value);
    }
}

void json_array_set_string(Json* json, size_t index, const char* value) {
    json_array_set(json, index, json_string(value));
}

void json_array_set_number(Json* json, size_t index, double value) {
    json_array_set(json, index, json_number(value));
}

void json_array_set_boolean(Json* json, size_t index, bool value) {
    json_array_set(json, index, json_boolean(value));
}

void json_fprintf(FILE* file, const Json* json, size_t depth) {
    switch (json->type) {
        case JSON_TYPE_OBJECT: {
            if (json->object.length == 0) {
                fprintf(file, "{}");
                break;
            }

            fprintf(file, "{\n");

            for (size_t index = 0; index < json->object.length; index++) {
                for (size_t depth_index = 0; depth_index < depth; depth_index++) {
                    fprintf(file, "\t");
                }
                fprintf(file, "\"%s\": ", json->object.keys[index]);
                json_fprintf(file, json->object.values[index], depth + 1);
                if (index < json->object.length - 1) {
                    fprintf(file, ",");
                }
                fprintf(file, "\n");
            }

            for (size_t depth_index = 0; depth_index < depth - 1; depth_index++) {
                fprintf(file, "\t");
            }
            fprintf(file, "}");
            break;
        }
        case JSON_TYPE_ARRAY: {
            if (json->array.length == 0) {
                fprintf(file, "[]");
                break;
            }

            fprintf(file, "[\n");

            for (size_t index = 0; index < json->array.length; index++) {
                for (size_t depth_index = 0; depth_index < depth; depth_index++) {
                    fprintf(file, "\t");
                }
                json_fprintf(file, json_array_get(json, index), depth + 1);
                if (index < json->array.length - 1) {
                    fprintf(file, ",");
                }
                fprintf(file, "\n");
            }

            for (size_t depth_index = 0; depth_index < depth - 1; depth_index++) {
                fprintf(file, "\t");
            }
            fprintf(file, "]");
            break;
        }
        case JSON_TYPE_STRING: {
            fprintf(file, "\"%s\"", json->string.value);
            break;
        }
        case JSON_TYPE_NUMBER: {
            if ((double)((int)json->number.value) == json->number.value) {
                fprintf(file, "%i", (int)json->number.value);
            } else {
                fprintf(file, "%f", json->number.value);
            }
            break;
        }
        case JSON_TYPE_BOOLEAN: {
            fprintf(file, json->boolean.value ? "true" : "false");
            break;
        }
        case JSON_TYPE_NULL: {
            fprintf(file, "null");
            break;
        }
    }
}

bool json_write(const Json* json, const char* path) {
    FILE* file = fopen(path, "w");
    if (file == NULL) {
        return false;
    }

    json_fprintf(file, json, 1);

    fclose(file);

    return true;
}

size_t json_parse_object(const char* json_str, Json** json) {
    GOLD_ASSERT(*json_str == '{');

    *json = json_object();

    const char* json_str_ptr = json_str + 1;
    while (*json_str_ptr != '\0' && *json_str_ptr != '}') {
        Json* key_json;
        json_str_ptr += json_parse_string(json_str_ptr, &key_json);

        GOLD_ASSERT(*json_str_ptr == ':');
        json_str_ptr++;

        Json* value_json;
        json_str_ptr += json_parse(json_str_ptr, &value_json);

        json_object_set(*json, json_get_string(key_json), value_json);
        json_free(key_json);

        GOLD_ASSERT(*json_str_ptr == ',' || *json_str_ptr == '}');

        if (*json_str_ptr == ',') {
            json_str_ptr++;
        }
    }

    GOLD_ASSERT(*json_str_ptr == '}');
    json_str_ptr++;

    return json_str_ptr - json_str;
}

size_t json_parse_array(const char* json_str, Json** json) {
    GOLD_ASSERT(*json_str == '[');

    *json = json_array();
    const char* json_str_ptr = json_str + 1;

    while (*json_str_ptr != '\0' && *json_str_ptr != ']') {
        Json* value;
        json_str_ptr += json_parse(json_str_ptr, &value);

        json_array_push(*json, value);

        GOLD_ASSERT(*json_str_ptr == ',' || *json_str_ptr == ']');
        if (*json_str_ptr == ',') {
            json_str_ptr++;
        }
    }

    GOLD_ASSERT(*json_str_ptr == ']');
    json_str_ptr++;

    return json_str_ptr - json_str;
}

size_t json_parse_string(const char* json_str, Json** json) {
    GOLD_ASSERT(json_str[0] == '"');

    size_t length = 0;
    while (json_str[length + 1] != '\0' && json_str[length + 1] != '"') {
        length++;
    }

    GOLD_ASSERT(json_str[length + 1] == '"');

    *json = (Json*)malloc(sizeof(Json));
    (*json)->type = JSON_TYPE_STRING;
    (*json)->string.length = length;
    (*json)->string.value = (char*)malloc(length + 1);
    strncpy((*json)->string.value, json_str + 1, length);
    (*json)->string.value[length] = '\0';

    return length + 2;
}

size_t json_parse_number(const char* json_str, Json** json) {
    GOLD_ASSERT(json_str[0] >= '0' && json_str[0] <= '9');

    size_t length = 0;
    while (json_str[length] != '\0' && json_str[length] != ',' && json_str[length] != '}' && json_str[length] != ']') {
        length++;
    }

    GOLD_ASSERT(length != 0 && (json_str[length] == '\0' || json_str[length] == ',' || json_str[length] == '}' || json_str[length] == ']'));

    double value = strtod(json_str, NULL);
    *json = json_number(value);

    return length;
}

bool json_parse_str_begins_with(const char* json_str, size_t length, const char* value) {
    size_t value_length = strlen(value);
    if (length != value_length) {
        return false;
    }

    return strncmp(json_str, value, length) == 0;
}

size_t json_parse_boolean_and_null(const char* json_str, Json** json) {
    size_t length = 0;
    while (json_str[length] != '\0' && json_str[length] != ',' && json_str[length] != '}' && json_str[length] != ']') {
        length++;
    }

    GOLD_ASSERT(length != 0 && (json_str[length] == '\0' || json_str[length] == ',' || json_str[length] == '}' || json_str[length] == ']'));

    *json = NULL;
    if (json_parse_str_begins_with(json_str, length, "true")) {
        *json = json_boolean(true);
    }
    if (json_parse_str_begins_with(json_str, length, "false")) {
        *json = json_boolean(false);
    }
    if (json_parse_str_begins_with(json_str, length, "null")) {
        *json = json_null();
    }
    GOLD_ASSERT(*json != NULL);

    return length;
}

size_t json_parse(const char* json_str, Json** json) {
    if (*json_str == '{') {
        return json_parse_object(json_str, json);
    }
    if (*json_str == '[') {
        return json_parse_array(json_str, json);
    }
    if (*json_str == '"') {
        return json_parse_string(json_str, json);
    }
    if (*json_str >= '0' && *json_str <= '9') {
        return json_parse_number(json_str, json);
    }
    return json_parse_boolean_and_null(json_str, json);
}

bool json_read_is_char_whitespace(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
            return true;
        default:
            return false;
    }
}

Json* json_read(const char* path) {
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    size_t buffer_length = 0;
    size_t buffer_capacity = 1024;
    char* buffer = (char*)malloc(buffer_capacity * sizeof(char));

    char c;
    bool is_inside_quotes = false;
    while ((c = fgetc(file)) != EOF) {
        if (c == '"') {
            is_inside_quotes = !is_inside_quotes;
        }
        if (json_read_is_char_whitespace(c) && !is_inside_quotes) {
            continue;
        }

        if (buffer_length == buffer_capacity) {
            buffer_capacity *= 2;

            char* temp = buffer;
            buffer = (char*)malloc(buffer_capacity * sizeof(char));
            memcpy(buffer, temp, buffer_length * sizeof(char));
            free(temp);
        }

        buffer[buffer_length] = c;
        buffer_length++;
    }

    fclose(file);

    Json* json;
    json_parse(buffer, &json);

    free(buffer);

    return json;
}
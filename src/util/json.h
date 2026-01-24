#pragma once

#include "math/gmath.h"

enum JsonType {
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY,
    JSON_TYPE_STRING,
    JSON_TYPE_NUMBER,
    JSON_TYPE_BOOLEAN,
    JSON_TYPE_NULL
};

struct Json;

struct JsonObject {
    char** keys;
    Json** values;
    size_t length;
    size_t capacity;
};

struct JsonArray {
    Json** values;
    size_t length;
    size_t capacity;
};

struct JsonString {
    char* value;
    size_t length;
};

struct JsonNumber {
    double value;
};

struct JsonBoolean {
    bool value;
};

struct Json {
    JsonType type;
    union {
        JsonObject object;
        JsonArray array;
        JsonString string;
        JsonNumber number;
        JsonBoolean boolean;
    };
};

Json* json_object();
Json* json_array();
Json* json_string(const char* value);
Json* json_number(double value);
Json* json_boolean(bool value);
Json* json_null();

Json* json_from_ivec2(ivec2 value);
ivec2 json_to_ivec2(Json* json);

void json_free(Json* json);

const char* json_get_string(const Json* json);
double json_get_number(const Json* json);
bool json_get_boolean(const Json* json);

Json* json_object_get(const Json* json, const char* key);
const char* json_object_get_string(const Json* json, const char* key);
double json_object_get_number(Json* json, const char* key);
bool json_object_get_boolean(Json* json, const char* key);

void json_object_set(Json* json, const char* key, Json* value);
void json_object_set_string(Json* json, const char* key, const char* value);
void json_object_set_number(Json* json, const char* key, double value);
void json_object_set_boolean(Json* json, const char* key, bool value);

Json* json_array_get(const Json* json, size_t index);
const char* json_array_get_string(const Json* json, size_t index);
double json_array_get_number(const Json* json, size_t index);
bool json_array_get_boolean(const Json* json, size_t index);

void json_array_push(Json* json, Json* value);
void json_array_push_string(Json* json, const char* value);
void json_array_push_number(Json* json, double value);
void json_array_push_boolean(Json* json, bool value);

void json_array_set(Json* json, size_t index, Json* value);
void json_array_set_string(Json* json, size_t index, const char* value);
void json_array_set_number(Json* json, size_t index, double value);
void json_array_set_boolean(Json* json, size_t index, bool value);

bool json_write(const Json* json, const char* path);
Json* json_read(const char* path);
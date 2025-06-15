#include "replay.h"

#include "util.h"
#include "network/types.h"
#include "core/filesystem.h"
#include "core/logger.h"
#include "feedback/feedback.h"

static const uint8_t REPLAY_FILE_VERSION = 0;

#ifdef GOLD_DEBUG
    static char arg_replay_file[128];
    static bool use_arg_replay_file = false;

    void replay_debug_set_file_name(char* argv) {
        strcpy(arg_replay_file, argv);
        use_arg_replay_file = true;
    }
#endif

FILE* replay_file_open(int32_t lcg_seed, const Noise& noise, MatchPlayer players[MAX_PLAYERS]) {
    char replay_file_path[256];
    char* replay_file_ptr = replay_file_path;
    replay_file_ptr += sprintf(replay_file_ptr, "replays/");
    replay_file_ptr += sprintf_timestamp(replay_file_ptr);
    replay_file_ptr += sprintf(replay_file_ptr, ".rep");
    #ifdef GOLD_DEBUG
        if (use_arg_replay_file) {
            filesystem_get_replay_path(replay_file_path, arg_replay_file);
        }
    #endif

    FILE* file = fopen(replay_file_path, "wb");
    if (file == NULL) {
        log_error("Could not open replay file for writing with path %s.", replay_file_path);
        return NULL;
    }

    feedback_set_replay_path(replay_file_path);

    // Version byte
    fwrite(&REPLAY_FILE_VERSION, 1, sizeof(uint8_t), file);

    // LCG seed
    fwrite(&lcg_seed, 1, sizeof(int32_t), file);

    // Map
    fwrite(&noise.width, 1, sizeof(uint32_t), file);
    fwrite(&noise.height, 1, sizeof(uint32_t), file);
    fwrite(noise.map, 1, noise.width * noise.height * sizeof(int8_t), file);

    // Players
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        fwrite(&players[player_id].active, 1, sizeof(bool), file);
        fwrite(players[player_id].name, 1, sizeof(players[player_id].name), file);
        fwrite(&players[player_id].team, 1, sizeof(uint32_t), file);
        fwrite(&players[player_id].recolor_id, 1, sizeof(int32_t), file);
    }

    return file;
}

void replay_file_close(FILE* file) {
    if (file == NULL) {
        return;
    }
    fclose(file);
}

void replay_file_write_inputs(FILE* file, uint8_t player_id, const std::vector<MatchInput>* inputs) {
    if (file == NULL) {
        return;
    }

    fwrite(&player_id, 1, sizeof(uint8_t), file);
    uint8_t out_buffer[NETWORK_INPUT_BUFFER_SIZE];
    size_t out_buffer_length = 0;

    // This means the player was invalid / not in the game during this turn
    if (inputs == NULL) {
        fwrite(&out_buffer_length, 1, sizeof(size_t), file);
        return;
    }

    for (const MatchInput& input : *inputs) {
        match_input_serialize(out_buffer, out_buffer_length, input);
    }
    fwrite(&out_buffer_length, 1, sizeof(size_t), file);
    fwrite(out_buffer, 1, out_buffer_length, file);
}

bool replay_file_read(std::vector<std::vector<MatchInput>>* match_inputs, MatchState* state, const char* path) {
    char replay_file_path[256];
    filesystem_get_replay_path(replay_file_path, path);

    FILE* file = fopen(replay_file_path, "rb");
    if (file == NULL) {
        log_error("Could not open replay file for reading with path %s.", replay_file_path);
        return false;
    }

    feedback_set_replay_path(replay_file_path);

    // Version byte
    uint8_t replay_version;
    fread(&replay_version, 1, sizeof(uint8_t), file);
    log_trace("Replay version: %u", replay_version);

    // LCG seed
    int32_t lcg_seed;
    fread(&lcg_seed, 1, sizeof(int32_t), file);

    // Map
    Noise noise;
    fread(&noise.width, 1, sizeof(uint32_t), file);
    fread(&noise.height, 1, sizeof(uint32_t), file);
    noise.map = (int8_t*)malloc(noise.width * noise.height * sizeof(int8_t));
    fread(noise.map, 1, noise.width * noise.height * sizeof(int8_t), file);

    // Players
    MatchPlayer players[MAX_PLAYERS];
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        fread(&players[player_id].active, 1, sizeof(bool), file);
        fread(players[player_id].name, 1, sizeof(players[player_id].name), file);
        fread(&players[player_id].team, 1, sizeof(uint32_t), file);
        fread(&players[player_id].recolor_id, 1, sizeof(int32_t), file);
    }

    // Init state
    *state = match_init(lcg_seed, noise, players);

    while (true) {
        // The first byte tells us the type of the next block
        // For types 0-3, this is a set of turn inputs from the player_id 0-3
        uint8_t block_type;
        size_t bytes_read = fread(&block_type, 1, sizeof(uint8_t), file);
        if (bytes_read == 0) {
            break;
        }

        // The next type tells us the length of the next block
        size_t block_length;
        fread(&block_length, 1, sizeof(size_t), file);

        if (block_type < MAX_PLAYERS) {
            std::vector<MatchInput> inputs;

            if (block_length == 0) {
                // There was no input from this user this turn
                // So just pass an empty input
                inputs.push_back((MatchInput) { .type = MATCH_INPUT_NONE });
            } else {
                // Read the rest of the block
                uint8_t in_buffer[NETWORK_INPUT_BUFFER_SIZE];
                fread(in_buffer, 1, block_length, file);
                // Deserialize input
                size_t in_buffer_head = 0;
                while (in_buffer_head < block_length) {
                    inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
                }
            }

            match_inputs[block_type].push_back(inputs);
        } else {
            log_warn("Unhandled block type %u", block_type);
        }
    }

    free(noise.map);
    fclose(file);

    return true;
}
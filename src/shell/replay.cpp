#include "replay.h"

#include "network/types.h"
#include "core/filesystem.h"
#include "core/logger.h"

static const uint32_t REPLAY_FILE_SIGNATURE = 0x46591214;
static const uint32_t REPLAY_FILE_VERSION = 0;

// The header of each replay block is made up of the type (first four bits) followed by the player id (second four bits)
static const uint8_t REPLAY_BLOCK_HEADER_TYPE_MASK = 0xF0;
static const uint8_t REPLAY_BLOCK_HEADER_PLAYER_MASK = 0x0F;
static const uint8_t REPLAY_BLOCK_TYPE_INPUTS = 0;
static const uint8_t REPLAY_BLOCK_TYPE_CHAT = 1 << 4;

#ifdef GOLD_DEBUG
    static char arg_replay_file[128];
    static bool use_arg_replay_file = false;

    void replay_debug_set_file_name(char* argv) {
        strcpy(arg_replay_file, argv);
        use_arg_replay_file = true;
    }
#endif

FILE* replay_file_open(int32_t lcg_seed, const Noise& noise, MatchPlayer players[MAX_PLAYERS]) {
    std::string replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + filesystem_get_timestamp_str() + ".rep";
    #ifdef GOLD_DEBUG
        if (use_arg_replay_file) {
            replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + arg_replay_file + ".rep";
        }
    #endif

    FILE* file = fopen(replay_path.c_str(), "wb");
    if (file == NULL) {
        log_error("Could not open replay file for writing with path %s.", replay_path.c_str());
        return NULL;
    }

    // Signature
    fwrite(&REPLAY_FILE_SIGNATURE, 1, sizeof(uint32_t), file);

    // Version byte
    fwrite(&REPLAY_FILE_VERSION, 1, sizeof(uint32_t), file);

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

    uint8_t block_header = REPLAY_BLOCK_TYPE_INPUTS + player_id;
    fwrite(&block_header, 1, sizeof(uint8_t), file);
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

void replay_file_write_chat(FILE* file, uint8_t player_id, uint32_t turn, const char* message) {
    if (file == NULL) {
        return;
    }

    // Header
    uint8_t block_header = REPLAY_BLOCK_TYPE_CHAT + player_id;
    fwrite(&block_header, 1, sizeof(uint8_t), file);

    // Length
    size_t message_byte_length = strlen(message) + 1;
    size_t block_length = message_byte_length + sizeof(uint32_t);
    fwrite(&block_length, 1, sizeof(size_t), file);

    // Content
    fwrite(&turn, 1, sizeof(uint32_t), file);
    fwrite(message, 1, message_byte_length, file);
}

bool replay_file_read(const char* path, MatchState* state, std::vector<std::vector<MatchInput>>* match_inputs, std::vector<ReplayChatMessage>* match_chatlog) {
    std::string replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + path;
    FILE* file = fopen(replay_path.c_str(), "rb");
    if (file == NULL) {
        log_error("Could not open replay file for reading with path %s.", replay_path.c_str());
        return false;
    }

    // Signature
    uint32_t replay_signature;
    fread(&replay_signature, 1, sizeof(uint32_t), file);
    if (replay_signature != REPLAY_FILE_SIGNATURE) {
        log_error("Replay file signature was invalid %s.", replay_path.c_str());
        fclose(file);
        return false;
    }

    // Version
    uint32_t replay_version;
    fread(&replay_version, 1, sizeof(uint32_t), file);
    log_debug("Replay version: %u", replay_version);

    // LCG seed
    int32_t lcg_seed;
    fread(&lcg_seed, 1, sizeof(int32_t), file);

    // Map
    Noise noise;
    fread(&noise.width, 1, sizeof(uint32_t), file);
    fread(&noise.height, 1, sizeof(uint32_t), file);
    noise.map = (uint8_t*)malloc(noise.width * noise.height * sizeof(uint8_t));
    fread(noise.map, 1, noise.width * noise.height * sizeof(uint8_t), file);

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

    bool read_successful = true;
    while (read_successful) {
        // The first byte tells us the type of the next block
        // For types 0-3, this is a set of turn inputs from the player_id 0-3
        uint8_t block_header;
        size_t bytes_read = fread(&block_header, 1, sizeof(uint8_t), file);
        if (bytes_read == 0) {
            break;
        }
        uint8_t block_type = block_header & REPLAY_BLOCK_HEADER_TYPE_MASK;
        uint8_t block_player = block_header & REPLAY_BLOCK_HEADER_PLAYER_MASK;

        // The next type tells us the length of the next block
        size_t block_length;
        fread(&block_length, 1, sizeof(size_t), file);

        switch (block_type) {
            case REPLAY_BLOCK_TYPE_INPUTS: {
                if (block_player >= MAX_PLAYERS) {
                    log_error("Replay file corruption: Unrecognized player id %u in replay block of type inputs", block_player);
                    read_successful = false;
                    break;
                }

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

                match_inputs[block_player].push_back(inputs);
                break;
            }
            case REPLAY_BLOCK_TYPE_CHAT: {
                if (block_player > MAX_PLAYERS) {
                    log_error("Replay file corruption: Unhandled player id of %u in block type chat.", block_player);
                    read_successful = false;
                    break;
                }

                ReplayChatMessage message;
                message.player_id = block_player;
                fread(&message.turn, 1, sizeof(uint32_t), file);
                fread(message.message, 1, block_length - sizeof(uint32_t), file);

                match_chatlog->push_back(message);

                break;
            }
            default: {
                log_error("Replay file corruption: Unhandled block type %u", block_type);
                read_successful = false;
                break;
            }
        }
    }

    free(noise.map);
    fclose(file);

    return read_successful;
}
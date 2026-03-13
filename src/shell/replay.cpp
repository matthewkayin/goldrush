#include "replay.h"

#include "network/types.h"
#include "core/filesystem.h"
#include "core/logger.h"
#include "profile/profile.h"

static const uint32_t REPLAY_FILE_SIGNATURE = 0x46591214;
static const uint32_t REPLAY_FILE_VERSION = 0;

#ifdef GOLD_DEBUG

static char arg_replay_file[128];
static bool use_arg_replay_file = false;

void replay_set_filename(const char* filename) {
    strcpy(arg_replay_file, filename);
    use_arg_replay_file = true;
}

#endif

FILE* replay_file_open(int32_t lcg_seed, MapType map_type, const Noise* noise, MatchPlayer players[MAX_PLAYERS]) {
    std::string replay_path = filesystem_get_data_path() + FILESYSTEM_REPLAY_FOLDER_NAME + FILESYSTEM_REPLAY_AUTOSAVE_PREFIX + filesystem_get_timestamp_str() + ".rep";
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

    // Map Type
    uint8_t map_type_byte = (uint8_t)map_type;
    fwrite(&map_type_byte, 1, sizeof(uint8_t), file);

    // Map
    noise_fwrite(noise, file);

    // Players
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        fwrite(&players[player_id], 1, sizeof(MatchPlayer), file);
    }

    return file;
}

void replay_file_close(FILE* file) {
    if (file == NULL) {
        return;
    }
    fclose(file);
}

void replay_file_write_entry(FILE* file, const ReplayEntry& entry) {
    if (file == NULL) {
        return;
    }

    fwrite(&entry.type, 1, sizeof(entry.type), file);

    switch (entry.type) {
        case REPLAY_ENTRY_INPUT: {
            uint8_t out_buffer[NETWORK_INPUT_BUFFER_SIZE];
            size_t out_buffer_length = 0;

            match_input_serialize(out_buffer, out_buffer_length, entry.input);
            fwrite(&out_buffer_length, 1, sizeof(size_t), file);
            fwrite(out_buffer, 1, out_buffer_length, file);

            break;
        }
        case REPLAY_ENTRY_CHAT: {
            fwrite(&entry.chat_message, 1, sizeof(entry.chat_message), file);
            break;
        }
        case REPLAY_ENTRY_DISCONNECT: {
            fwrite(&entry.disconnect_player_id, 1, sizeof(entry.disconnect_player_id), file);
            break;
        }
        case REPLAY_ENTRY_NEW_TURN: {
            break;
        }
    }
}

bool replay_file_read(const char* path, MatchState& state, std::vector<std::vector<ReplayEntry>>* replay_entries) {
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

    // Map type
    uint8_t map_type_byte;
    fread(&map_type_byte, 1, sizeof(uint8_t), file);
    MapType map_type = (MapType)map_type_byte;

    // Map
    Noise* noise = noise_fread(file);

    // Players
    MatchPlayer players[MAX_PLAYERS];
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        fread(&players[player_id], 1, sizeof(MatchPlayer), file);
    }

    // Init state
    match_init(state, lcg_seed, players, (MatchInitMapParams) {
        .type = MATCH_INIT_MAP_FROM_NOISE,
        .noise = (MatchInitMapParamsNoise) {
            .type = map_type,
            .noise = noise
        }
    });

    while (true) {
        uint8_t entry_type;
        size_t bytes_read = fread(&entry_type, 1, sizeof(uint8_t), file);
        if (bytes_read == 0) {
            goto success;
        }

        switch (entry_type) {
            case REPLAY_ENTRY_INPUT: {
                size_t in_buffer_length;
                uint8_t in_buffer[NETWORK_INPUT_BUFFER_SIZE];

                bytes_read = fread(&in_buffer_length, 1, sizeof(size_t), file);
                if (bytes_read != sizeof(size_t)) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, sizeof(size_t));
                    goto fail;
                }
                bytes_read = fread(in_buffer, 1, in_buffer_length, file);
                if (bytes_read != in_buffer_length) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, in_buffer_length);
                    goto fail;
                }

                size_t in_buffer_head = 0;
                MatchInput input = match_input_deserialize(in_buffer, in_buffer_head);

                if (input.type != MATCH_INPUT_NONE) {
                    char print_buffer[1024];
                    match_input_print(print_buffer, input);
                    log_debug("Replay entry turn %u input %s", (uint32_t)replay_entries->size() - 1U, print_buffer);
                }

                replay_entries->back().push_back((ReplayEntry) {
                    .type = REPLAY_ENTRY_INPUT,
                    .input = input
                });

                break;
            }
            case REPLAY_ENTRY_CHAT: {
                ReplayEntry entry;
                entry.type = REPLAY_ENTRY_CHAT;

                bytes_read = fread(&entry.chat_message, 1, sizeof(entry.chat_message), file);
                if (bytes_read != sizeof(entry.chat_message)) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, sizeof(entry.chat_message));
                    goto fail;
                }

                log_debug("Replay entry turn %u chat message %s: %s", (uint32_t)replay_entries->size() - 1U, entry.chat_message.prefix, entry.chat_message.message);

                replay_entries->back().push_back(entry);

                break;
            }
            case REPLAY_ENTRY_DISCONNECT: {
                ReplayEntry entry;
                entry.type = REPLAY_ENTRY_DISCONNECT;

                bytes_read = fread(&entry.disconnect_player_id, 1, sizeof(entry.disconnect_player_id), file);
                if (bytes_read != sizeof(entry.disconnect_player_id)) {
                    log_error("Replay file bytes read mismatch. Read %u Expected %u", bytes_read, sizeof(entry.disconnect_player_id));
                    goto fail;
                }

                log_debug("Replay entry turn %u disconnect %u", (uint32_t)replay_entries->size() - 1U, entry.disconnect_player_id);

                replay_entries->back().push_back(entry);

                break;
            }
            case REPLAY_ENTRY_NEW_TURN: {
                replay_entries->push_back(std::vector<ReplayEntry>());
                break;
            }
            default: {
                log_error("Replay file has unexpected entry type %u.", entry_type);
                goto fail;
            }
        }
    }

success:
    noise_free(noise);
    fclose(file);

    return true;

fail:
    noise_free(noise);
    fclose(file);

    return false;
}
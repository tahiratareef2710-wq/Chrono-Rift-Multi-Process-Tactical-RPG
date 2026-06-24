// shared_memory.h - Complete header for Chrono Rift
#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <signal.h>

#define MAX_PLAYERS 4
#define MAX_ENEMIES 9

// ===== REPLACE 240721 WITH YOUR ACTUAL ROLL NUMBER =====
#define ROLL_NUMBER 240566  // CHANGE THIS TO YOUR ROLL NUMBER!
#define ROLL_LAST_TWO (ROLL_NUMBER % 100)
#define ROLL_LAST_ONE (ROLL_NUMBER % 10)
#define ROLL_SECOND_LAST ((ROLL_NUMBER / 10) % 10)

// Weapon structure (Section 10)
struct Weapon {
    int id;
    char name[20];
    int slot_size;
    int damage;
};

// Weapon database - from assignment table
const Weapon WEAPON_DB[] = {
    {0, "Solar Core", 10, 95},
    {1, "Lunar Blade", 10, 90},
    {2, "Iron Halberd", 7, 55},
    {3, "Venom Dagger", 4, 30},
    {4, "Thunderstaff", 6, 50},
    {5, "Obsidian Axe", 5, 45},
    {6, "Frostbow", 6, 48},
    {7, "Splinter Stick", 2, 12}
};
const int NUM_WEAPONS = 8;

// Artifact tracking (Section 7)
struct ArtifactEntry {
    char name[20];
    bool is_locked;
    int held_by;
    int entity_type;  // 0=player, 1=enemy
};

// Main Game State Structure
struct GameState {
    // Game status
    bool game_running;
    int game_winner;  // 0=playing, 1=players won, 2=enemies won
    int turn_number;
    int total_kills;
    int player1_kills;   // For multiplayer competition
    int player2_kills;   // For multiplayer competition
    char last_action_message[256];
    bool is_stun;
    time_t ultimate_start_time;
    bool artifacts_waiting_for_solar[MAX_PLAYERS + MAX_ENEMIES];
    bool artifacts_waiting_for_lunar[MAX_PLAYERS + MAX_ENEMIES];
    bool artifacts_waiting_for_eclipse[MAX_PLAYERS + MAX_ENEMIES];
    bool hip_ready;   // HIP sets this true after initialization
    bool asp_ready;
    bool hip2_ready;
    int eclipse_relic_spawn_turn;   
    // HIP2 sets this true after initialization (Bonus)

    // Synchronization primitives
    sem_t global_mutex;
    sem_t turn_mutex;
    sem_t artifact_mutex;
    sem_t hip_signal;
    sem_t asp_signal;
    sem_t hip2_signal; 

   
    // Bonus multiplayer
    pid_t hip2_pid;
    bool hip2_connected;
    int hip2_player_start;  
    int hip2_num_players;
    bool multiplayer_mode;

    // Player data (max 4)
    int num_players;
    struct Player {
        int id;
        int hp;
        int max_hp;
        int stamina;
        int max_stamina;  // Always 100
        int speed;
        int damage;
        bool is_alive;
        bool is_stunned;
        int stun_end_time;
        int inventory[20];  // -1 = empty, otherwise weapon ID
        int long_term_storage[100];
        int long_term_count;
    } players[4];

    // Enemy data (max 9)
    int num_enemies;
    struct Enemy {
        int id;
        int hp;
        int max_hp;
        int stamina;
        int max_stamina;  // Always 150
        int speed;
        int damage;
        bool is_alive;
        bool is_stunned;
        int stun_end_time;
        int held_weapon;  // -1 if none
    } enemies[9];

    // Turn scheduling
    int current_entity_type;  // 0=player, 1=enemy
    int current_entity_id;
    int active_player_id;

    // Process PIDs for signal delivery (Section 5 & 8)
    pid_t arbiter_pid;
    pid_t hip_pid;
    pid_t asp_pid;

    // Artifacts (Section 7)
    ArtifactEntry artifacts[3];
    int num_artifacts;

    bool weapon_dropped;
    int dropped_weapon_id;
    int drop_for_player;
    int drop_turn_number;  // Track when drop occurred

    // Ultimate Ability tracking (Section 8)
    int stun_target_npc_id;
    bool ultimate_active;
    time_t ultimate_end_time;

    // Eclipse Relic (Section 7) - dynamic artifact
    int stunned_npc_id;
    int stunned_player_id;
    bool eclipse_relic_available;
    bool eclipse_relic_introduced;
    int eclipse_relic_holder;
    int eclipse_relic_holder_type;

    // Pending action
    struct PendingAction {
        int type;
        int actor_type;
        int actor_id;
        int target_type;
        int target_id;
        int weapon_id;
        bool is_stun;
    } pending_action;
    bool action_pending;
};

// =============================================
// ARTIFACT FUNCTIONS (inline so all processes can use them)
// =============================================

inline bool lock_artifact(GameState* state, int artifact_id, int entity_id, int entity_type) {
    sem_wait(&state->artifact_mutex);

    if (state->artifacts[artifact_id].is_locked) {
        if (artifact_id == 0) {
            state->artifacts_waiting_for_solar[entity_id] = true;
        }
        else if (artifact_id == 1) {
            state->artifacts_waiting_for_lunar[entity_id] = true;
        }
        else if (artifact_id == 2) {
            state->artifacts_waiting_for_eclipse[entity_id] = true;
        }
        sem_post(&state->artifact_mutex);
        return false;
    }

    state->artifacts[artifact_id].is_locked = true;
    state->artifacts[artifact_id].held_by = entity_id;
    state->artifacts[artifact_id].entity_type = entity_type;

    state->artifacts_waiting_for_solar[entity_id] = false;
    state->artifacts_waiting_for_lunar[entity_id] = false;
    state->artifacts_waiting_for_eclipse[entity_id] = false;

    sem_post(&state->artifact_mutex);
    return true;
}

inline void release_artifact(GameState* state, int artifact_id) {
    sem_wait(&state->artifact_mutex);

    int holder = state->artifacts[artifact_id].held_by;

    if (holder != -1) {
        state->artifacts_waiting_for_solar[holder] = false;
        state->artifacts_waiting_for_lunar[holder] = false;
        state->artifacts_waiting_for_eclipse[holder] = false;
    }

    state->artifacts[artifact_id].is_locked = false;
    state->artifacts[artifact_id].held_by = -1;
    state->artifacts[artifact_id].entity_type = -1;

    sem_post(&state->artifact_mutex);
}

// Helper function to attach to shared memory
inline GameState* attach_shared_memory() {
    std::string shm_name = "/chrono_rift_" + std::to_string(ROLL_NUMBER);
    int shm_fd = shm_open(shm_name.c_str(), O_RDWR, 0666);

    if (shm_fd == -1) {
        std::cerr << "Cannot attach to shared memory. Is arbiter running?" << std::endl;
        exit(1);
    }

    GameState* state = (GameState*)mmap(0, sizeof(GameState),
        PROT_READ | PROT_WRITE,
        MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    return state;
}

// Helper to unlink shared memory
inline void unlink_shared_memory() {
    std::string shm_name = "/chrono_rift_" + std::to_string(ROLL_NUMBER);
    shm_unlink(shm_name.c_str());
}

#endif
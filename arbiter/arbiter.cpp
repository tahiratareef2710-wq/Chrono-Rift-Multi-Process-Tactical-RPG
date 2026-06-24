// arbiter/arbiter.cpp
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <sys/wait.h>
#include "../shared_memory.h"
#include <vector>      
using namespace std;

// Global variables for signal handling
GameState* g_state = nullptr;
pid_t g_hip_pid = -1;
pid_t g_asp_pid = -1;
bool g_multiplayer_mode = false;

// =============================================
// SIGNAL HANDLERS (Section 5 & 8)
// =============================================

// SIGTERM handler - player chose to quit (Section 10)
void sigterm_handler(int sig) {
    cout << "\n[Arbiter] SIGTERM received - Player quit the game!" << endl;
    if (g_state) {
        g_state->game_running = false;
        g_state->game_winner = 0;
    }
}

// SIGALRM handler - Ultimate Ability 10 second window expired (Section 8)
void sigalrm_handler(int sig) {
    cout << "\n[Arbiter] SIGALRM - Ultimate Ability window expired!" << endl;
    
    if (g_state && g_state->ultimate_active) {
        cout << "[Arbiter] Updating enemy stamina after 10 seconds..." << endl;
        
        double time_passed = 10.0;
        
        for (int i = 0; i < g_state->num_enemies; i++) {
            if (g_state->enemies[i].is_alive && !g_state->enemies[i].is_stunned) {
                int old_stamina = g_state->enemies[i].stamina;
                g_state->enemies[i].stamina += (int)(g_state->enemies[i].speed * time_passed);
                if (g_state->enemies[i].stamina > g_state->enemies[i].max_stamina) {
                    g_state->enemies[i].stamina = g_state->enemies[i].max_stamina;
                }
                cout << "[Arbiter] Enemy " << i+1 << " stamina: " << old_stamina 
                     << " -> " << g_state->enemies[i].stamina << endl;
            }
        }
    }
    
    cout << "[Arbiter] Resuming ASP process..." << endl;
    if (g_asp_pid != -1) {
        kill(g_asp_pid, SIGCONT);
        cout << "[Arbiter] ASP resumed with SIGCONT" << endl;
    }
}

// SIGUSR1 handler - used for stun notifications (Section 5)
void sigusr1_handler(int sig) {
    cout << "\n[Arbiter] SIGUSR1 received - Stun event!" << endl;
}

// Cleanup child processes on exit
void cleanup_child_processes() {
    cout << "\n[Arbiter] Cleaning up child processes..." << endl;

    if (g_hip_pid > 0) {
        cout << "[Arbiter] Sending SIGTERM to HIP (PID: " << g_hip_pid << ")" << endl;
        kill(g_hip_pid, SIGTERM);
    }

    if (g_asp_pid > 0) {
        cout << "[Arbiter] Sending SIGTERM to ASP (PID: " << g_asp_pid << ")" << endl;
        kill(g_asp_pid, SIGTERM);
    }

    usleep(200000);

    if (g_hip_pid > 0 && kill(g_hip_pid, 0) == 0) {
        cout << "[Arbiter] Force killing HIP" << endl;
        kill(g_hip_pid, SIGKILL);
    }

    if (g_asp_pid > 0 && kill(g_asp_pid, 0) == 0) {
        cout << "[Arbiter] Force killing ASP" << endl;
        kill(g_asp_pid, SIGKILL);
    }
}

// Custom SIGTERM handler for arbiter
void arbiter_sigterm_handler(int sig) {
    cout << "\n[Arbiter] SIGTERM received - shutting down game..." << endl;

    if (g_state) {
        g_state->game_running = false;
    }

    cleanup_child_processes();

    if (g_state) {
        sem_destroy(&g_state->global_mutex);
        sem_destroy(&g_state->turn_mutex);
        sem_destroy(&g_state->artifact_mutex);
        sem_destroy(&g_state->hip_signal);
        sem_destroy(&g_state->asp_signal);
        sem_destroy(&g_state->hip2_signal);

        munmap(g_state, sizeof(GameState));
    }

    string shm_name = "/chrono_rift_" + to_string(ROLL_NUMBER);
    shm_unlink(shm_name.c_str());

    cout << "[Arbiter] Cleanup complete. Exiting." << endl;
    exit(0);
}

// Setup all signal handlers
void setup_signals() {
    struct sigaction sa_term, sa_alrm, sa_usr1;

    sa_term.sa_handler = arbiter_sigterm_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    sigaction(SIGTERM, &sa_term, nullptr);

    sa_alrm.sa_handler = sigalrm_handler;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alrm, nullptr);

    sa_usr1.sa_handler = sigusr1_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, nullptr);

    cout << "[Arbiter] Signal handlers registered." << endl;
}

// =============================================
// STUN MECHANIC (Section 5)
// =============================================
void apply_stun(GameState* state, int entity_type, int entity_id) {
    time_t stun_start = time(nullptr);
    time_t stun_end = stun_start + 3;

    char start_buf[32], end_buf[32];
    strftime(start_buf, sizeof(start_buf), "%H:%M:%S", localtime(&stun_start));
    strftime(end_buf,   sizeof(end_buf),   "%H:%M:%S", localtime(&stun_end));

    if (entity_type == 0) {
        cout << "[Arbiter] Player " << entity_id + 1
             << " STUNNED at " << start_buf
             << " — expires at " << end_buf << endl;
        state->players[entity_id].is_stunned = true;
        state->players[entity_id].stun_end_time = stun_end;
        if (g_hip_pid != -1) kill(g_hip_pid, SIGUSR1);
    }
    else {
        cout << "[Arbiter] Enemy " << entity_id + 1
             << " STUNNED at " << start_buf
             << " — expires at " << end_buf << endl;
        state->enemies[entity_id].is_stunned = true;
        state->enemies[entity_id].stun_end_time = stun_end;
        if (g_asp_pid != -1) kill(g_asp_pid, SIGUSR1);
    }
}
void update_stuns(GameState* state) {
    int now = time(nullptr);
    for (int i = 0; i < state->num_players; i++) {
        if (state->players[i].is_stunned && now >= state->players[i].stun_end_time) {
            state->players[i].is_stunned = false;
        }
    }
    for (int i = 0; i < state->num_enemies; i++) {
        if (state->enemies[i].is_stunned && now >= state->enemies[i].stun_end_time) {
            state->enemies[i].is_stunned = false;
        }
    }
}

// =============================================
// ULTIMATE ABILITY (Section 8)
// =============================================
void trigger_ultimate(GameState* state) {
    cout << "\n🌟🌟🌟 ULTIMATE ABILITY TRIGGERED! 🌟🌟🌟" << endl;
    cout << "[Arbiter] Suspending ASP for 10 seconds via SIGSTOP..." << endl;

    if (g_asp_pid != -1) {
        kill(g_asp_pid, SIGSTOP);
        cout << "[Arbiter] ASP suspended! Player has 10 second window!" << endl;
        alarm(10);
    }
}

// =============================================
// ARTIFACT SYSTEM (Section 7)
// =============================================
void init_artifacts(GameState* state) {
    strcpy(state->artifacts[0].name, "Solar Core");
    state->artifacts[0].is_locked = false;
    state->artifacts[0].held_by = -1;
    state->artifacts[0].entity_type = -1;

    strcpy(state->artifacts[1].name, "Lunar Blade");
    state->artifacts[1].is_locked = false;
    state->artifacts[1].held_by = -1;
    state->artifacts[1].entity_type = -1;

    strcpy(state->artifacts[2].name, "Eclipse Relic");
    state->artifacts[2].is_locked = false;
    state->artifacts[2].held_by = -1;
    state->artifacts[2].entity_type = -1;

    state->num_artifacts = 2;
    state->eclipse_relic_available = false;
    state->eclipse_relic_introduced = false;
    state->eclipse_relic_holder = -1;
    state->eclipse_relic_holder_type = -1;
}

// =============================================
// DEADLOCK DETECTION THREAD (Section 7)
// =============================================
void* deadlock_detection_thread(void* arg) {
    GameState* state = (GameState*)arg;
    static bool last_deadlock_state = false;
    
    while (state->game_running) {
        this_thread::sleep_for(chrono::milliseconds(500));
        
        if (sem_trywait(&state->artifact_mutex) != 0) continue;
        
        int solar_holder = -1, lunar_holder = -1, eclipse_holder = -1;
        
        for (int i = 0; i < state->num_artifacts; i++) {
            if (state->artifacts[i].is_locked) {
                if (strcmp(state->artifacts[i].name, "Solar Core") == 0) {
                    solar_holder = state->artifacts[i].held_by;
                }
                else if (strcmp(state->artifacts[i].name, "Lunar Blade") == 0) {
                    lunar_holder = state->artifacts[i].held_by;
                }
                else if (strcmp(state->artifacts[i].name, "Eclipse Relic") == 0) {
                    eclipse_holder = state->artifacts[i].held_by;
                }
            }
        }
        
        bool deadlock_detected = false;
        
        if (solar_holder != -1 && lunar_holder != -1 && solar_holder != lunar_holder) {
            if (state->artifacts_waiting_for_lunar[solar_holder] && 
                state->artifacts_waiting_for_solar[lunar_holder]) {
                deadlock_detected = true;
                state->artifacts[0].is_locked = false;
                state->artifacts[0].held_by = -1;
                cout << "\n⚠️ DEADLOCK DETECTED! Released Solar Core." << endl;
            }
        }
        
        if (!deadlock_detected && solar_holder != -1 && eclipse_holder != -1 && solar_holder != eclipse_holder) {
            if (state->artifacts_waiting_for_eclipse[solar_holder] && 
                state->artifacts_waiting_for_solar[eclipse_holder]) {
                deadlock_detected = true;
                state->artifacts[0].is_locked = false;
                state->artifacts[0].held_by = -1;
                cout << "\n⚠️ DEADLOCK DETECTED! Released Solar Core." << endl;
            }
        }
        
        if (!deadlock_detected && lunar_holder != -1 && eclipse_holder != -1 && lunar_holder != eclipse_holder) {
            if (state->artifacts_waiting_for_eclipse[lunar_holder] && 
                state->artifacts_waiting_for_lunar[eclipse_holder]) {
                deadlock_detected = true;
                state->artifacts[1].is_locked = false;
                state->artifacts[1].held_by = -1;
                cout << "\n⚠️ DEADLOCK DETECTED! Released Lunar Blade." << endl;
            }
        }
        
        last_deadlock_state = deadlock_detected;
        sem_post(&state->artifact_mutex);
    }
    return nullptr;
}

void spawn_new_enemy(GameState* state) {
    for (int i = 0; i < 9; i++) {
        if (!state->enemies[i].is_alive) {
            state->enemies[i].is_alive = true;
	    state->enemies[i].is_stunned = false;
            state->enemies[i].hp = ROLL_LAST_TWO + 50 + (rand() % 151);
            state->enemies[i].max_hp = state->enemies[i].hp;
            state->enemies[i].stamina = 150;
            state->enemies[i].max_stamina = 150;
            state->enemies[i].speed = 10 + (rand() % 21);
            state->enemies[i].damage = ROLL_SECOND_LAST + 10;
            state->enemies[i].held_weapon = -1;
            cout << "🔄 A NEW ENEMY APPEARS! (Enemy " << i + 1 << ")" << endl;
            return;
        }
    }
}

// =============================================
// ACTION PROCESSING (Section 3 & 10)
// =============================================
void process_action(GameState* state) {
    if (!state->action_pending) return;

    auto& action = state->pending_action;
    cout << "\n[Arbiter] Processing action type " << action.type << "..." << endl;

    if (action.type == 0) {
        if (action.actor_type == 0) {
            int dmg = state->players[action.actor_id].damage;
            state->enemies[action.target_id].hp -= dmg;
            cout << "⚔️  Player " << action.actor_id + 1 << " deals " << dmg
                << " damage to Enemy " << action.target_id + 1 << endl;
            
            if (action.is_stun) {
                apply_stun(state, 1, action.target_id);
            }
           
            if (state->enemies[action.target_id].hp <= 0) {
                state->enemies[action.target_id].is_alive = false;
                state->total_kills++;
                cout << "💀 Enemy defeated! Total kills: " << state->total_kills << " / 10" << endl;

                // =============================================
                // MULTIPLAYER KILL TRACKING
                // =============================================
                if (g_multiplayer_mode && action.actor_type == 0) {
                    if (action.actor_id == 0) {
                        state->player1_kills++;
                        cout << "🏆 PLAYER 1 HAS " << state->player1_kills << " KILLS!" << endl;
                        if (state->player1_kills >= 5) {
                            cout << "\n🏆🏆🏆 PLAYER 1 WINS! 🏆🏆🏆" << endl;
                            state->game_running = false;
                            state->game_winner = 1;
                        }
                    } else if (action.actor_id == 1) {
                        state->player2_kills++;
                        cout << "🏆 PLAYER 2 HAS " << state->player2_kills << " KILLS!" << endl;
                        if (state->player2_kills >= 5) {
                            cout << "\n🏆🏆🏆 PLAYER 2 WINS! 🏆🏆🏆" << endl;
                            state->game_running = false;
                            state->game_winner = 2;
                        }
                    }
                }

                // Weapon drop from held weapon
                if (state->enemies[action.target_id].held_weapon != -1) {
                    int dropped_weapon = state->enemies[action.target_id].held_weapon;
                    cout << "📦 Enemy dropped: " << WEAPON_DB[dropped_weapon].name << "!" << endl;
                    state->weapon_dropped = true;
                    state->dropped_weapon_id = dropped_weapon;
                    state->drop_for_player = action.actor_id;
                    state->enemies[action.target_id].held_weapon = -1;
                }
                else {
                    int drop = rand() % 100;
                    if (drop < 50) {
                        int weapon_id = rand() % NUM_WEAPONS;
                        cout << "📦 Enemy dropped: " << WEAPON_DB[weapon_id].name << "!" << endl;
                        state->weapon_dropped = true;
                        state->dropped_weapon_id = weapon_id;
                        state->drop_for_player = action.actor_id;
                    }
                    else {
                        state->weapon_dropped = false;
                    }
                }
                spawn_new_enemy(state);
            }
        }
        else {
            int dmg = state->enemies[action.actor_id].damage;
            state->players[action.target_id].hp -= dmg;
            cout << "⚔️  Enemy " << action.actor_id + 1 << " deals " << dmg
                << " damage to Player " << action.target_id + 1 << endl;
            if (state->players[action.target_id].hp <= 0) {
                state->players[action.target_id].is_alive = false;
                release_artifact(state, 0);
                release_artifact(state, 1);
                release_artifact(state, 2);
                cout << "💀 Player " << action.target_id + 1 << " DEFEATED!" << endl;
            }
        }
    }
else if (action.type == 1) {
    if (action.actor_type == 0) {
        int dmg = state->players[action.actor_id].damage;
        state->enemies[action.target_id].stamina -= dmg;
        if (state->enemies[action.target_id].stamina < 0)
            state->enemies[action.target_id].stamina = 0;
        cout << "🌊 Player " << action.actor_id + 1 << " drains Enemy "
            << action.target_id + 1 << " stamina by " << dmg << endl;
        cout << "   Enemy " << action.target_id + 1 << " stamina now: "
            << state->enemies[action.target_id].stamina << endl;
        // Mark enemy as having just been exhausted so scheduler skips their regen
        state->enemies[action.target_id].is_stunned = true;
        state->enemies[action.target_id].stun_end_time = time(nullptr) + 1;
    }
}    else if (action.type == 2) {
        if (action.actor_type == 0 && action.weapon_id >= 0) {
            int dmg = WEAPON_DB[action.weapon_id].damage;
            state->enemies[action.target_id].hp -= dmg;
            cout << "🗡️  Player " << action.actor_id + 1 << " uses "
                << WEAPON_DB[action.weapon_id].name << " for " << dmg << " damage!" << endl;
        }
    }
    else if (action.type == 3) {
        cout << "📦 Player " << action.actor_id + 1 << " swaps in weapon" << endl;
    }
    else if (action.type == 4) {
        int heal = state->players[action.actor_id].max_hp / 10;
        state->players[action.actor_id].hp += heal;
        if (state->players[action.actor_id].hp > state->players[action.actor_id].max_hp)
            state->players[action.actor_id].hp = state->players[action.actor_id].max_hp;
        cout << "💊 Player " << action.actor_id + 1 << " heals " << heal << " HP!" << endl;
    }
    else if (action.type == 5) {
        cout << "⏭️  " << (action.actor_type == 0 ? "Player" : "Enemy")
            << " " << action.actor_id + 1 << " skips turn" << endl;
        if (action.actor_type == 0)
            state->players[action.actor_id].stamina = state->players[action.actor_id].max_stamina / 2;
        else
            state->enemies[action.actor_id].stamina = state->enemies[action.actor_id].max_stamina / 2;
    }
    else if (action.type == 6) {
        trigger_ultimate(state);
        state->ultimate_active = true;
        state->ultimate_start_time = time(nullptr);
        state->ultimate_end_time = time(nullptr) + 10;
    }

    if (action.type != 5) {
        if (action.actor_type == 0)
            state->players[action.actor_id].stamina = 0;
        else
            state->enemies[action.actor_id].stamina = 0;
    }

    state->action_pending = false;
    state->turn_number++;
}

// =============================================
// WIN CONDITIONS (Section 10)
// =============================================
void check_win_conditions(GameState* state) {
    int alive_players = 0;
    int alive_enemies = 0;
    static int last_enemy_count = 0;

    for (int i = 0; i < state->num_players; i++) {
        if (state->players[i].is_alive) alive_players++;
    }

    for (int i = 0; i < state->num_enemies; i++) {
        if (state->enemies[i].is_alive) alive_enemies++;
    }

    if (alive_enemies < last_enemy_count) {
        int killed = last_enemy_count - alive_enemies;
        cout << "\n========================================" << endl;
        cout << "💀 ENEMY DEFEATED! Total kills: " << state->total_kills << " / 10 💀" << endl;
        cout << "========================================" << endl;
    }
    last_enemy_count = alive_enemies;

    sem_wait(&state->global_mutex);
    if (state->total_kills >= 3 && !state->eclipse_relic_introduced && !state->eclipse_relic_available) {
        state->eclipse_relic_available = true;
        state->eclipse_relic_introduced = true;
        state->eclipse_relic_spawn_turn = state->turn_number;
        cout << "\n✨✨✨ THE ECLIPSE RELIC HAS APPEARED! ✨✨✨" << endl;
         }
        sem_post(&state->global_mutex);
    // Single player win condition
    if (!g_multiplayer_mode && state->total_kills >= 10) {
        cout << "\n╔══════════════════════════════════════════════════╗" << endl;
        cout << "║   🏆🏆🏆   VICTORY!   🏆🏆🏆                      ║" << endl;
        cout << "║   Your party has defeated 10 enemies!           ║" << endl;
        cout << "╚══════════════════════════════════════════════════╝" << endl;
        state->game_running = false;
        state->game_winner = 1;
    }

    if (alive_players == 0) {
        cout << "\n╔══════════════════════════════════════════════════╗" << endl;
        cout << "║   💀💀💀   GAME OVER!   💀💀💀                    ║" << endl;
        cout << "║   All players have been defeated!               ║" << endl;
        cout << "╚══════════════════════════════════════════════════╝" << endl;
        state->game_running = false;
        state->game_winner = 2;
    }
}

// =============================================
// STAMINA SCHEDULER (Section 3)
// =============================================
void calculate_next_actor(GameState* state) {
    // Check for entities with FULL stamina first
    for (int i = 0; i < state->num_players; i++) {
        if (state->players[i].is_alive && !state->players[i].is_stunned && 
            state->players[i].stamina >= state->players[i].max_stamina) {
            state->current_entity_type = 0;
            state->current_entity_id = i;
            return;
        }
    }

    for (int i = 0; i < state->num_enemies; i++) {
        if (state->enemies[i].is_alive && !state->enemies[i].is_stunned && 
            state->enemies[i].stamina >= state->enemies[i].max_stamina) {
            state->current_entity_type = 1;
            state->current_entity_id = i;
            return;
        }
    }

    float min_time = 999999;
    int next_type = -1, next_id = -1;

    for (int i = 0; i < state->num_players; i++) {
        if (!state->players[i].is_alive || state->players[i].is_stunned) continue;
        int needed = state->players[i].max_stamina - state->players[i].stamina;
        float t = (float)needed / state->players[i].speed;
        if (t < min_time) { min_time = t; next_type = 0; next_id = i; }
    }

    for (int i = 0; i < state->num_enemies; i++) {
        if (!state->enemies[i].is_alive || state->enemies[i].is_stunned) continue;
        int needed = state->enemies[i].max_stamina - state->enemies[i].stamina;
        float t = (float)needed / state->enemies[i].speed;
        if (t < min_time) { min_time = t; next_type = 1; next_id = i; }
    }

    // Advance time for ALL entities EXCEPT the next actor
    for (int i = 0; i < state->num_players; i++) {
        if (!state->players[i].is_alive || state->players[i].is_stunned) continue;
        if (next_type == 0 && next_id == i) continue;
        int gain = (int)(state->players[i].speed * min_time + 0.5f);  // Manual rounding
        state->players[i].stamina += gain;
        if (state->players[i].stamina > state->players[i].max_stamina)
            state->players[i].stamina = state->players[i].max_stamina;
    }

    for (int i = 0; i < state->num_enemies; i++) {
        if (!state->enemies[i].is_alive || state->enemies[i].is_stunned) continue;
        if (next_type == 1 && next_id == i) continue;
        int gain = (int)(state->enemies[i].speed * min_time + 0.5f);  // Manual rounding
        state->enemies[i].stamina += gain;
        if (state->enemies[i].stamina > state->enemies[i].max_stamina)
            state->enemies[i].stamina = state->enemies[i].max_stamina;
    }

    state->current_entity_type = next_type;
    state->current_entity_id = next_id;

    if (next_type == 0) {
        state->players[next_id].stamina = state->players[next_id].max_stamina;
    } else {
        state->enemies[next_id].stamina = state->enemies[next_id].max_stamina;
    }
}

// =============================================
// MAIN
// =============================================
int main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "   CHRONO RIFT - GAME ARBITER" << endl;
    cout << "========================================" << endl;
    cout << "Roll Number Seed: " << ROLL_NUMBER << endl;
    
    // Check for multiplayer flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--multiplayer") == 0) {
            g_multiplayer_mode = true;
            cout << "🎮 MULTIPLAYER MODE ENABLED" << endl;
            break;
        }
    }
    cout << "========================================\n" << endl;

    setup_signals();

    string shm_name = "/chrono_rift_" + to_string(ROLL_NUMBER);
    shm_unlink(shm_name.c_str());

    int shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        cerr << "Failed to create shared memory" << endl;
        return 1;
    }

    ftruncate(shm_fd, sizeof(GameState));
    g_state = (GameState*)mmap(0, sizeof(GameState),
        PROT_READ | PROT_WRITE,
        MAP_SHARED, shm_fd, 0);

    // Initialize state
    g_state->game_running = true;
    g_state->game_winner = 0;
    g_state->turn_number = 0;
    g_state->action_pending = false;
    g_state->weapon_dropped = false;
    g_state->dropped_weapon_id = -1;
    g_state->drop_for_player = -1;
    g_state->num_players = 0;
    g_state->num_enemies = 0;
    g_state->current_entity_type = 0;
    g_state->current_entity_id = 0;
    g_state->ultimate_active = false;
    g_state->ultimate_end_time = 0;
    g_state->player1_kills = 0;
    g_state->player2_kills = 0;
    g_state->multiplayer_mode = g_multiplayer_mode;
    g_state->hip_ready = false;
    g_state->asp_ready = false;
    g_state->hip2_ready = false;
    g_state->hip2_connected = false;
    strcpy(g_state->last_action_message, "Game Started");
    sem_init(&g_state->global_mutex, 1, 1);
    sem_init(&g_state->turn_mutex, 1, 1);
    sem_init(&g_state->artifact_mutex, 1, 1);
    sem_init(&g_state->hip_signal, 1, 0);
    sem_init(&g_state->asp_signal, 1, 0);
    sem_init(&g_state->hip2_signal, 1, 0);
    g_state->hip2_player_start = 1;
    init_artifacts(g_state);

    g_state->arbiter_pid = getpid();
    g_state->hip_pid = -1;
    g_state->asp_pid = -1;

    for (int i = 0; i < MAX_PLAYERS + MAX_ENEMIES; i++) {
        g_state->artifacts_waiting_for_solar[i] = false;
        g_state->artifacts_waiting_for_lunar[i] = false;
        g_state->artifacts_waiting_for_eclipse[i] = false;
    }

    pthread_t deadlock_thread;
    pthread_create(&deadlock_thread, nullptr, deadlock_detection_thread, g_state);

    cout << "Shared memory created!" << endl;
    cout << "Waiting for HIP and ASP to connect..." << endl;

    while (!g_state->hip_ready || !g_state->asp_ready) {
        this_thread::sleep_for(chrono::milliseconds(500));
        cout << "." << flush;
    }

    if (g_multiplayer_mode) {
        cout << "\nWaiting for HIP2 to connect..." << endl;
        while (!g_state->hip2_ready) {
            this_thread::sleep_for(chrono::milliseconds(500));
            cout << "." << flush;
        }
        cout << "\n HIP2 connected!" << endl;
    }

    g_hip_pid = g_state->hip_pid;
    g_asp_pid = g_state->asp_pid;

    cout << "\n Players: " << g_state->num_players
         << " | Enemies: " << g_state->num_enemies << endl;
    cout << " HIP PID: " << g_hip_pid
         << " | ASP PID: " << g_asp_pid << endl;
    if (g_multiplayer_mode) {
        cout << " HIP2 PID: " << g_state->hip2_pid << endl;
    }

    cout << "\n GAME START! \n" << endl;

    while (g_state->game_running) {
        update_stuns(g_state);

        if (g_state->ultimate_active && time(nullptr) >= g_state->ultimate_end_time) {
            g_state->ultimate_active = false;
            cout << "[Arbiter] Ultimate window expired - normal scheduling resumed!" << endl;
        }

        calculate_next_actor(g_state);

        if (g_state->ultimate_active && g_state->current_entity_type == 1) {
            cout << "[Arbiter] Ultimate active - skipping Enemy "
                 << g_state->current_entity_id + 1 << "'s turn!" << endl;
            
            sem_wait(&g_state->global_mutex);
            g_state->action_pending = true;
            g_state->pending_action.type = 5;
            g_state->pending_action.actor_type = 1;
            g_state->pending_action.actor_id = g_state->current_entity_id;
            sem_post(&g_state->global_mutex);
        }
        else if (g_state->current_entity_type == 0) {
            int player_id = g_state->current_entity_id;
            
            if (g_multiplayer_mode && player_id >= g_state->hip2_player_start) {
                cout << "[Arbiter] Player " << player_id + 1 << " (HIP2)'s turn" << endl;
                sem_post(&g_state->hip2_signal);
            } else {
                cout << "[Arbiter] Player " << player_id + 1 << " (HIP1)'s turn" << endl;
                sem_post(&g_state->hip_signal);
            }
        }
        else {
            cout << "[Arbiter] Enemy " << g_state->current_entity_id + 1 << "'s turn" << endl;
            sem_post(&g_state->asp_signal);
        }

        int timeout = (g_state->current_entity_type == 0) ? 300 : 30;
        int wait = 0;
        bool action_seen = false;
        
        while (wait < timeout) {
            sem_wait(&g_state->global_mutex);
            action_seen = g_state->action_pending;
            sem_post(&g_state->global_mutex);
            if (action_seen) break;
            this_thread::sleep_for(chrono::milliseconds(100));
            wait++;
        }

        if (wait >= timeout) {
            cout << "\n Turn timeout! Forcing skip..." << endl;
            sem_wait(&g_state->global_mutex);
            g_state->action_pending = true;
            g_state->pending_action.type = 5;
            g_state->pending_action.actor_type = g_state->current_entity_type;
            g_state->pending_action.actor_id = g_state->current_entity_id;
            sem_post(&g_state->global_mutex);
        }

        process_action(g_state);

        sem_wait(&g_state->global_mutex);
        g_state->action_pending = false;   // ← add this
        sem_post(&g_state->global_mutex);

        check_win_conditions(g_state);
    }

    cout << "\n Cleaning up..." << endl;
    pthread_cancel(deadlock_thread);
    sem_destroy(&g_state->global_mutex);
    sem_destroy(&g_state->turn_mutex);
    sem_destroy(&g_state->artifact_mutex);
    sem_destroy(&g_state->hip_signal);
    sem_destroy(&g_state->asp_signal);
    sem_destroy(&g_state->hip2_signal);
    munmap(g_state, sizeof(GameState));
    shm_unlink(shm_name.c_str());

    cout << " Arbiter shutting down." << endl;
    return 0;
}

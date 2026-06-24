// asp/asp.cpp - Complete Automated Strategic Process (Enemy AI)
#include <iostream>
#include <pthread.h>
#include <vector>
#include <random>
#include <unistd.h>
#include <ctime>
#include <signal.h>
#include "../shared_memory.h"

using namespace std;


bool g_asp_running = true;


void asp_sigterm_handler(int sig) {
    cout << "\n[ASP] Received SIGTERM - shutting down gracefully..." << endl;
    g_asp_running = false;
    
    // Detach from shared memory
    GameState* state = attach_shared_memory();
    if (state != MAP_FAILED && state != nullptr) {
        munmap(state, sizeof(GameState));
    }
    
    exit(0);
}
// Which NPC is being stunned
GameState* g_asp_state = nullptr;

void sigusr1_handler(int sig) {
    // Just print - no sleep! Let threads handle timing
    cout << "\n[ASP] SIGUSR1 received - Stun signal delivered!" << endl;
}
// Thread function for each NPC
void* npc_thread_function(void* arg) {
    int npc_id = *(int*)arg;
    GameState* state = attach_shared_memory();

    cout << "[NPC " << npc_id + 1 << "] Thread ready!" << endl;

    // Random number generator for AI decisions
    mt19937 rng(ROLL_NUMBER + npc_id);

    // =============================================
    // SETUP SIGNAL MASK FOR THIS THREAD
    // Block SIGUSR1 so we can wait for it with sigtimedwait
    // =============================================
    sigset_t stun_mask;
    sigemptyset(&stun_mask);
    sigaddset(&stun_mask, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &stun_mask, NULL);

    // Stun tracking for this thread
    bool is_stunned = false;
    time_t stun_end_time = 0;

    while (g_asp_running && state->game_running && state->enemies[npc_id].is_alive) {

        // =============================================
        // CHECK FOR PENDING STUN SIGNAL (NON-BLOCKING)
        // This uses sigtimedwait - NO BUSY WAITING!
        // =============================================
        sigset_t wait_mask;
        sigemptyset(&wait_mask);
        sigaddset(&wait_mask, SIGUSR1);

        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;  // Non-blocking check

siginfo_t sig_info;
int ret = sigtimedwait(&wait_mask, &sig_info, &timeout);
        if (ret != -1 && sig_info.si_signo == SIGUSR1) {
            // Stun signal received! Enter stunned state
            is_stunned = true;
            stun_end_time = time(nullptr) + 3;
            time_t stun_start = time(nullptr);
            char time_buf[32];
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&stun_start));
            cout << "\n[NPC " << npc_id + 1 << "] STUNNED at " << time_buf
                << " — will recover at ";
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&stun_end_time));
            cout << time_buf << endl;

            // Update shared memory stun status
            state->enemies[npc_id].is_stunned = true;
            state->enemies[npc_id].stun_end_time = stun_end_time;
        }

        // =============================================
        // HANDLE STUNNED STATE (ASYNC - NO POLLING!)
        // =============================================
        if (is_stunned) {
            // Check if stun has expired
            if (time(nullptr) >= stun_end_time) {
                is_stunned = false;
                state->enemies[npc_id].is_stunned = false;
                time_t recover_time = time(nullptr);
                char time_buf[32];
                strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&recover_time));
                cout << "[NPC " << npc_id + 1 << "] Stun EXPIRED at " << time_buf
                    << " — resuming normal logic." << endl;
            }
            else {
                // Stunned - wait for the rest of the stun duration
                // This uses sleep() - true blocking, NO CPU WASTE!
                int remaining = stun_end_time - time(nullptr);
                if (remaining > 0) {
                    sleep(remaining);  // ✅ Thread sleeps - no busy waiting!
                }
                // Skip this turn
                sem_wait(&state->global_mutex);
                state->action_pending = true;
                state->pending_action.type = 5;  // Skip
                state->pending_action.actor_type = 1;
                state->pending_action.actor_id = npc_id;
                sem_post(&state->global_mutex);
                continue;
            }
        }

        // =============================================
        // WAIT FOR ARBITER TO SIGNAL TURN
        // =============================================
        while (true) {
            sem_wait(&state->asp_signal);
            if (state->current_entity_type == 1 &&
                state->current_entity_id == npc_id) {
                break;  // It's my turn!
            }
            // Not my turn - put signal back for the right thread
            sem_post(&state->asp_signal);
            usleep(10000);  // 10ms yield
        }

        // =============================================
        // CHECK STUN AGAIN (in case stun arrived while waiting)
        // =============================================
        if (state->enemies[npc_id].is_stunned || is_stunned) {
            cout << "\n[NPC " << npc_id + 1 << "] is STUNNED! Turn skipped." << endl;
            sem_wait(&state->global_mutex);
            state->enemies[npc_id].stamina = 0;
            state->action_pending = true;
            state->pending_action.type = 5;  // Skip
            state->pending_action.actor_type = 1;
            state->pending_action.actor_id = npc_id;
            sem_post(&state->global_mutex);
            continue;
        }

        // =============================================
     // ECLIPSE RELIC AUTO-PICKUP
     // =============================================
        if (state->eclipse_relic_available &&
            state->eclipse_relic_holder == -1 &&
            state->turn_number >= state->eclipse_relic_spawn_turn + 2) {
            sem_wait(&state->global_mutex);
            state->eclipse_relic_holder = npc_id;
            state->eclipse_relic_holder_type = 1;  // 1 = enemy
            state->eclipse_relic_available = false;
            cout << "\n[NPC " << npc_id + 1 << "] picked up the Eclipse Relic!" << endl;
            sem_post(&state->global_mutex);
        }
    
        // =============================================
        // AI DECISION - 70% attack, 30% skip
        // =============================================
        uniform_int_distribution<int> attack_dist(0, 9);
        int action_choice = attack_dist(rng);
        int action_type;
        if (action_choice < 5)      action_type = 0;  // 50% attack
        else if (action_choice < 7) action_type = 1;  // 20% stamina drain
        else if (action_choice < 9) action_type = 5;  // 20% skip
        else  action_type = 0; // 70% attack, 30% skip

        sem_wait(&state->global_mutex);

        state->action_pending = true;
        state->pending_action.type = action_type;
        state->pending_action.actor_type = 1;  // Enemy
        state->pending_action.actor_id = npc_id;
        state->pending_action.is_stun = false;  // Enemies don't have stun attack

        if (action_type == 0) {
            // Attack strike - choose random alive player
            vector<int> alive_players;
            for (int i = 0; i < state->num_players; i++) {
                if (state->players[i].is_alive) {
                    alive_players.push_back(i);
                }
            }
            if (!alive_players.empty()) {
                uniform_int_distribution<int> player_dist(0, alive_players.size() - 1);
                int target = alive_players[player_dist(rng)];
                state->pending_action.target_type = 0;  // Player
                state->pending_action.target_id = target;
                cout << "\n[NPC " << npc_id + 1 << "] DECIDES TO ATTACK Player " << target + 1 << "!" << endl;
            }
            else {
                // No players alive, skip
                state->pending_action.type = 5;
                cout << "\n[NPC " << npc_id + 1 << "] No targets available! Skipping." << endl;
            }
        }
        else {
            cout << "\n[NPC " << npc_id + 1 << "] DECIDES TO SKIP TURN!" << endl;
        }

        sem_post(&state->global_mutex);

        cout << "[NPC " << npc_id + 1 << "] Action submitted." << endl;
    }

    cout << "[NPC " << npc_id + 1 << "] Thread exiting." << endl;
    return nullptr;
}

int main() {
    cout << "========================================" << endl;
    cout << "   CHRONO RIFT - AUTOMATED STRATEGIC PROCESS" << endl;
    cout << "========================================" << endl;
    
    // Attach to shared memory
    GameState* state = attach_shared_memory();
    if (state == MAP_FAILED) {
        cerr << "❌ Failed to attach to shared memory. Is arbiter running?" << endl;
        return 1;
    }
    state->asp_pid = getpid();
    // Register signal handlers
signal(SIGTERM, asp_sigterm_handler);

    signal(SIGUSR1, sigusr1_handler);
    sem_wait(&state->global_mutex);
    
    // Random number of enemies (2-9) using roll number as seed
    mt19937 rng(ROLL_NUMBER);
    uniform_int_distribution<int> enemy_count_dist(2, 9);
    state->num_enemies = enemy_count_dist(rng);
    
    cout << "\n👾 Creating " << state->num_enemies << " enemies..." << endl;
    cout << "(Using Roll Number: " << ROLL_NUMBER << ")" << endl;
    
    uniform_int_distribution<int> hp_dist(50, 200);
    uniform_int_distribution<int> speed_dist(10, 30);
    
    for (int i = 0; i < state->num_enemies; i++) {
        state->enemies[i].id = i;
        state->enemies[i].is_alive = true;
        state->enemies[i].is_stunned = false;
        state->enemies[i].max_stamina = 150;
        state->enemies[i].stamina = 150;
        state->enemies[i].damage = ROLL_SECOND_LAST + 10;
        state->enemies[i].speed = speed_dist(rng);
        state->enemies[i].held_weapon = -1;
        
        // HP = last 2 digits of roll number + random(50-200)
        int random_hp = hp_dist(rng);
        state->enemies[i].max_hp = ROLL_LAST_TWO + random_hp;
        state->enemies[i].hp = state->enemies[i].max_hp;
        
        cout << "   👹 Enemy " << i+1 << ": HP=" << state->enemies[i].max_hp 
             << ", DMG=" << state->enemies[i].damage 
             << ", SPD=" << state->enemies[i].speed << endl;
    }
    
    sem_post(&state->global_mutex);
    
    // Create NPC threads (REQUIREMENT: one thread per NPC)
    vector<pthread_t> threads(state->num_enemies);
    vector<int> npc_ids(state->num_enemies);
    
    cout << "\n🧵 Creating NPC threads..." << endl;
    for (int i = 0; i < state->num_enemies; i++) {
        npc_ids[i] = i;
        if (pthread_create(&threads[i], nullptr, npc_thread_function, &npc_ids[i]) != 0) {
            cerr << "❌ Failed to create thread for NPC " << i << endl;
            return 1;
        }
        cout << "   ✅ NPC " << i+1 << " thread created" << endl;
    }
    state->asp_ready = true;
    
    cout << "\n⏳ All NPC threads created! Waiting for game to start..." << endl;
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" << endl;
    
    // Wait for all threads to complete
    for (int i = 0; i < state->num_enemies; i++) {
        pthread_join(threads[i], nullptr);
    }
    
    munmap(state, sizeof(GameState));
    cout << "\n👋 ASP shutting down." << endl;
    
    return 0;
}

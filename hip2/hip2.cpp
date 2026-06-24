// hip2/hip2.cpp - Second Human Interface Process (Bonus Multiplayer)
// Controls Player 2 automatically (no prompts)
#include <iostream>
#include <pthread.h>
#include <vector>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <signal.h>
#include <sys/types.h>
#include "../shared_memory.h"

using namespace std;

GameState* g_state = nullptr;

void sigterm_handler(int sig) {
    cout << "\n[HIP2] Received SIGTERM - shutting down..." << endl;
    if (g_state) {
        g_state->game_running = false;
        sem_post(&g_state->hip2_signal);
    }
    exit(0);
}

// =============================================
// INVENTORY ALLOCATOR (same as HIP)
// =============================================
int find_contiguous_slots(GameState* state, int player_id, int size) {
    int* inv = state->players[player_id].inventory;
    for (int i = 0; i <= 20 - size; i++) {
        bool found = true;
        for (int j = i; j < i + size; j++) {
            if (inv[j] != -1) { found = false; break; }
        }
        if (found) return i;
    }
    return -1;
}

void place_weapon(GameState* state, int player_id, int weapon_id, int start_slot) {
    int size = WEAPON_DB[weapon_id].slot_size;
    for (int i = start_slot; i < start_slot + size; i++)
        state->players[player_id].inventory[i] = weapon_id;
    cout << "✅ " << WEAPON_DB[weapon_id].name
        << " placed at slots " << start_slot
        << "-" << start_slot + size - 1 << endl;
}

void swap_to_long_term(GameState* state, int player_id, int start_slot) {
    int weapon_id = state->players[player_id].inventory[start_slot];
    if (weapon_id == -1) return;
    int size = WEAPON_DB[weapon_id].slot_size;
    for (int i = start_slot; i < start_slot + size; i++)
        state->players[player_id].inventory[i] = -1;
    int count = state->players[player_id].long_term_count;
    state->players[player_id].long_term_storage[count] = weapon_id;
    state->players[player_id].long_term_count++;
    cout << "📦 " << WEAPON_DB[weapon_id].name << " moved to long term storage!" << endl;
}

void pickup_weapon(GameState* state, int player_id, int weapon_id) {
    int size = WEAPON_DB[weapon_id].slot_size;

    cout << "\n[Inventory] Trying to pick up "
        << WEAPON_DB[weapon_id].name
        << " (size " << size << ")" << endl;

    bool has_solar = false, has_lunar = false;
    for (int i = 0; i < 20; i++) {
        if (state->players[player_id].inventory[i] == 0) has_solar = true;
        if (state->players[player_id].inventory[i] == 1) has_lunar = true;
    }
    if (has_solar && has_lunar) {
        cout << "❌ You already have Solar Core AND Lunar Blade (20/20 slots)." << endl;
        return;
    }

    if (weapon_id == 0) {
        if (!lock_artifact(state, 0, player_id, 0)) {
            cout << "❌ Solar Core is already held by someone else!" << endl;
            state->artifacts_waiting_for_solar[player_id] = true;
            return;
        }
        state->artifacts_waiting_for_solar[player_id] = false;
    }
    else if (weapon_id == 1) {
        if (!lock_artifact(state, 1, player_id, 0)) {
            cout << "❌ Lunar Blade is already held by someone else!" << endl;
            state->artifacts_waiting_for_lunar[player_id] = true;
            return;
        }
        state->artifacts_waiting_for_lunar[player_id] = false;
    }

    int slot = find_contiguous_slots(state, player_id, size);
    if (slot != -1) {
        place_weapon(state, player_id, weapon_id, slot);
        return;
    }

    cout << "[Inventory] Not enough contiguous space! Swapping weapons to LTS..." << endl;

    int i = 0;
    while (i < 20) {
        slot = find_contiguous_slots(state, player_id, size);
        if (slot != -1) break;

        int wid = state->players[player_id].inventory[i];
        if (wid != -1 && wid != 0 && wid != 1) {
            swap_to_long_term(state, player_id, i);
        }
        else {
            i++;
        }
    }

    slot = find_contiguous_slots(state, player_id, size);
    if (slot != -1) {
        place_weapon(state, player_id, weapon_id, slot);
    }
    else {
        cout << "❌ Cannot free enough space. " << WEAPON_DB[weapon_id].name
            << " sent directly to Long Term Storage." << endl;
        state->players[player_id].long_term_storage[state->players[player_id].long_term_count] = weapon_id;
        state->players[player_id].long_term_count++;
    }
}

void display_inventory(GameState* state, int player_id) {
    cout << "\n📦 INVENTORY (20 slots):\n   [";
    int i = 0;
    while (i < 20) {
        int wid = state->players[player_id].inventory[i];
        if (wid == -1) { cout << "_"; i++; }
        else {
            int size = WEAPON_DB[wid].slot_size;
            string name = WEAPON_DB[wid].name;
            if (name.length() > (size_t)size) name = name.substr(0, size);
            cout << name; i += size;
        }
        if (i < 20) cout << "|";
    }
    cout << "]" << endl;
    bool has_weapon = false;
    for (int j = 0; j < 20; j++) {
        if (state->players[player_id].inventory[j] != -1) {
            int wid = state->players[player_id].inventory[j];
            bool already = false;
            for (int k = 0; k < j; k++)
                if (state->players[player_id].inventory[k] == wid) { already = true; break; }
            if (!already) {
                cout << "   - " << WEAPON_DB[wid].name
                    << " (slots: " << WEAPON_DB[wid].slot_size
                    << ", dmg: " << WEAPON_DB[wid].damage << ")" << endl;
                has_weapon = true;
            }
        }
    }
    if (!has_weapon) cout << "   (empty)" << endl;
    if (state->players[player_id].long_term_count > 0) {
        cout << "\n🗄️  LONG TERM STORAGE:" << endl;
        for (int j = 0; j < state->players[player_id].long_term_count; j++) {
            int wid = state->players[player_id].long_term_storage[j];
            cout << "   " << j + 1 << ". " << WEAPON_DB[wid].name
                << " (dmg: " << WEAPON_DB[wid].damage << ")" << endl;
        }
    }
}

void swap_in_weapon(GameState* state, int player_id) {
    if (state->players[player_id].long_term_count == 0) {
        cout << "❌ Long term storage is empty!" << endl; return;
    }
    cout << "\n🗄️  LONG TERM STORAGE:" << endl;
    for (int i = 0; i < state->players[player_id].long_term_count; i++) {
        int wid = state->players[player_id].long_term_storage[i];
        cout << "   " << i + 1 << ". " << WEAPON_DB[wid].name
            << " (size: " << WEAPON_DB[wid].slot_size
            << ", dmg: " << WEAPON_DB[wid].damage << ")" << endl;
    }
    cout << "Choose weapon to swap in (1-"
        << state->players[player_id].long_term_count << "): ";
    int choice; cin >> choice; choice--;
    if (choice < 0 || choice >= state->players[player_id].long_term_count) {
        cout << "❌ Invalid choice!" << endl; return;
    }
    int weapon_id = state->players[player_id].long_term_storage[choice];
    for (int i = choice; i < state->players[player_id].long_term_count - 1; i++)
        state->players[player_id].long_term_storage[i] =
        state->players[player_id].long_term_storage[i + 1];
    state->players[player_id].long_term_count--;
    pickup_weapon(state, player_id, weapon_id);
    cout << "✅ " << WEAPON_DB[weapon_id].name << " swapped in! Cannot use until next turn." << endl;
}

// =============================================
// PLAYER THREAD FUNCTION (uses hip2_signal)
// =============================================
void* player_thread_function(void* arg) {
    int player_id = *(int*)arg;
    cout << "[HIP2 Player " << player_id + 1 << "] Thread ready!" << endl;

    while (g_state->game_running && g_state->players[player_id].is_alive) {
        while (true) {
            sem_wait(&g_state->hip2_signal);
            if (g_state->current_entity_type == 0 &&
                g_state->current_entity_id == player_id) {
                break;
            }
            sem_post(&g_state->hip2_signal);
            usleep(10000);
        }

        if (g_state->players[player_id].is_stunned) {
            cout << "\n[HIP2 Player " << player_id + 1 << "] is STUNNED! Turn skipped." << endl;
            sem_wait(&g_state->global_mutex);
            g_state->players[player_id].stamina = 0;
            g_state->pending_action.type = 5;
            g_state->pending_action.actor_type = 0;
            g_state->pending_action.actor_id = player_id;
            g_state->pending_action.is_stun = false;
            g_state->action_pending = true;
            sem_post(&g_state->global_mutex);
            continue;
        }

        cout << "\n========================================" << endl;
        cout << "🟣 PLAYER " << player_id + 1 << "'S TURN (HIP2) 🟣" << endl;
        cout << "========================================" << endl;
        cout << "❤️  HP: " << g_state->players[player_id].hp
            << "/" << g_state->players[player_id].max_hp << endl;
        cout << "⚡ Stamina: " << g_state->players[player_id].stamina
            << "/" << g_state->players[player_id].max_stamina << endl;
        cout << "🗡️  Damage: " << g_state->players[player_id].damage << endl;

        display_inventory(g_state, player_id);

        cout << "\n🎯 ENEMIES:" << endl;
        for (int i = 0; i < g_state->num_enemies; i++) {
            if (g_state->enemies[i].is_alive)
                cout << "   " << i + 1 << ". Enemy " << i + 1
                << " (HP: " << g_state->enemies[i].hp
                << ", Stamina: " << g_state->enemies[i].stamina << ")" << endl;
        }

        bool has_solar = false, has_lunar = false;
        for (int j = 0; j < 20; j++) {
            if (g_state->players[player_id].inventory[j] == 0) has_solar = true;
            if (g_state->players[player_id].inventory[j] == 1) has_lunar = true;
        }
        bool can_ultimate = has_solar && has_lunar;
        bool weapon_available = (g_state->weapon_dropped && g_state->drop_for_player == player_id);
        bool eclipse_available = (g_state->eclipse_relic_available && g_state->eclipse_relic_holder == -1);

        cout << "\n📋 ACTIONS:" << endl;
        cout << "   0. ⚡ Stun Attack" << endl;
        cout << "   1. ⚔️  Attack (Strike)" << endl;
        cout << "   2. 🌊 Attack (Exhaust)" << endl;
        cout << "   3. 🗡️  Use Weapon" << endl;
        cout << "   4. 📦 Swap In" << endl;
        cout << "   5. 💊 Heal" << endl;
        cout << "   6. ⏭️  Skip" << endl;
        if (weapon_available)
            cout << "   7. 🎁 Pick Up: " << WEAPON_DB[g_state->dropped_weapon_id].name << endl;
        if (can_ultimate)
            cout << "   8. 🌟 ULTIMATE ABILITY!" << endl;
        if (eclipse_available)
            cout << "   9. ✨ Pick Up ECLIPSE RELIC!" << endl;

        cout << "\nChoose action: ";
        int choice; cin >> choice;

        sem_wait(&g_state->global_mutex);
        
        g_state->pending_action.actor_type = 0;
        g_state->pending_action.actor_id = player_id;

        if (choice == 0) {
            g_state->pending_action.type = 0;
            g_state->pending_action.is_stun = true;
            cout << "Select target (1-" << g_state->num_enemies << "): ";
            int target; cin >> target;
            g_state->pending_action.target_type = 1;
            g_state->pending_action.target_id = target - 1;
        }
        else if (choice == 1 || choice == 2) {
            g_state->pending_action.type = choice - 1;
            g_state->pending_action.is_stun = false;
            cout << "Select target (1-" << g_state->num_enemies << "): ";
            int target; cin >> target;
            g_state->pending_action.target_type = 1;
            g_state->pending_action.target_id = target - 1;
        }
        else if (choice == 3) {
            display_inventory(g_state, player_id);
            bool has_weapon = false;
            for (int j = 0; j < 20; j++)
                if (g_state->players[player_id].inventory[j] != -1) { has_weapon = true; break; }
            if (!has_weapon) {
                cout << "❌ No weapons! Skipping." << endl;
                g_state->pending_action.type = 5;
            }
            else {
                cout << "Select weapon slot (0-19): "; int slot; cin >> slot;
                if (slot >= 0 && slot < 20 && g_state->players[player_id].inventory[slot] != -1) {
                    g_state->pending_action.weapon_id = g_state->players[player_id].inventory[slot];
                    g_state->pending_action.type = 2;
                    cout << "Select target (1-" << g_state->num_enemies << "): ";
                    int target; cin >> target;
                    g_state->pending_action.target_type = 1;
                    g_state->pending_action.target_id = target - 1;
                }
                else {
                    g_state->pending_action.type = 5;
                }
            }
        }
        else if (choice == 4) {
            sem_post(&g_state->global_mutex);
            swap_in_weapon(g_state, player_id);
            sem_wait(&g_state->global_mutex);
            g_state->pending_action.type = 3;
        }
        else if (choice == 5) {
            g_state->pending_action.type = 4;
        }
        else if (choice == 6) {
            g_state->pending_action.type = 5;
        }
        else if (choice == 7 && weapon_available) {
            int weapon_id = g_state->dropped_weapon_id;
            sem_post(&g_state->global_mutex);
            pickup_weapon(g_state, player_id, weapon_id);
            g_state->weapon_dropped = false;
            g_state->dropped_weapon_id = -1;
            sem_wait(&g_state->global_mutex);
            g_state->pending_action.type = 5;
        }
        else if (choice == 8 && can_ultimate) {
            cout << "\n🌟 ULTIMATE ABILITY ACTIVATED!" << endl;
            g_state->pending_action.type = 6;
        }
        else if (choice == 9 && eclipse_available) {
            sem_post(&g_state->global_mutex);
            bool locked = lock_artifact(g_state, 2, player_id, 0);
            sem_wait(&g_state->global_mutex);
            if (locked) {
                cout << "\n✨ You picked up the Eclipse Relic!" << endl;
                g_state->eclipse_relic_holder = player_id;
                g_state->eclipse_relic_holder_type = 0;  // 0 = player
                g_state->eclipse_relic_available = false;
                g_state->num_artifacts = 3;
            }
            else {
                cout << "❌ Eclipse Relic already held!" << endl;
            }
            g_state->pending_action.type = 5;
        }
        else {
            g_state->pending_action.type = 5;
        }
        g_state->action_pending = true;
        sem_post(&g_state->global_mutex);
        cout << "\n⏳ Action submitted!" << endl;
    }
    return nullptr;
}

// =============================================
// MAIN - Creates Player 2 automatically
// =============================================
int main() {
    cout << "========================================" << endl;
    cout << "   CHRONO RIFT - HIP2 (PLAYER 2)" << endl;
    cout << "========================================" << endl;

    g_state = attach_shared_memory();
    if (g_state == MAP_FAILED) {
        cerr << "❌ Failed to attach to shared memory!" << endl;
        return 1;
    }

    signal(SIGTERM, sigterm_handler);
    signal(SIGUSR1, SIG_IGN);

    g_state->hip2_pid = getpid();
    

    cout << "✅ Connected to shared memory!" << endl;

    // Wait for HIP1 to set its players first
    while (!g_state->hip_ready) {
        usleep(100000);
    }

    // PLAYER 2 is at index = current number of players
    int player_id = g_state->num_players;  // If HIP1 has 1 player, this will be 1

    // Update total player count
    g_state->num_players = player_id + 1;
    for (int i = 0; i < g_state->num_players; i++) {
        g_state->players[i].speed = 100 / g_state->num_players;
    }
    // Initialize Player 2 with correct stats
    g_state->players[player_id].id = player_id;
    g_state->players[player_id].is_alive = true;
    g_state->players[player_id].is_stunned = false;
    g_state->players[player_id].max_stamina = 100;
    g_state->players[player_id].stamina = 100;
    g_state->players[player_id].damage = ROLL_LAST_ONE + 10;
    g_state->players[player_id].speed = 100 / g_state->num_players;
    g_state->players[player_id].long_term_count = 0;

    int random_hp = 100 + (rand() % 901);
    g_state->players[player_id].max_hp = ROLL_NUMBER + random_hp;
    g_state->players[player_id].hp = g_state->players[player_id].max_hp;

    for (int j = 0; j < 20; j++) {
        g_state->players[player_id].inventory[j] = -1;
    }
    g_state->hip2_ready = true;  // ← here

    cout << "\n🎮 Player 2 created automatically!" << endl;
    cout << "   HP: " << g_state->players[player_id].max_hp
        << ", DMG: " << g_state->players[player_id].damage
        << ", SPD: " << g_state->players[player_id].speed << endl;
    cout << "   Total players: " << g_state->num_players << endl;

    // Create thread for Player 2
    pthread_t player_thread;
    pthread_create(&player_thread, nullptr, player_thread_function, &player_id);

    pthread_join(player_thread, nullptr);

    cout << "👋 HIP2 shutting down." << endl;
    return 0;
}
// render/render.cpp - SFML GUI for Chrono Rift (Section 9)
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include "../shared_memory.h"

using namespace std;

// =============================================
// CONSTANTS
// =============================================
const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 800;
const int FPS = 30;

// Colors
const sf::Color BG_COLOR(15, 15, 30);
const sf::Color PANEL_COLOR(30, 30, 60);
const sf::Color BORDER_COLOR(80, 80, 150);
const sf::Color PLAYER_COLOR(50, 150, 255);
const sf::Color ENEMY_COLOR(255, 80, 80);
const sf::Color HP_COLOR(50, 200, 80);
const sf::Color HP_LOW_COLOR(255, 80, 80);
const sf::Color STAMINA_COLOR(255, 200, 50);
const sf::Color STUN_COLOR(200, 50, 255);
const sf::Color TEXT_COLOR(220, 220, 255);
const sf::Color GOLD_COLOR(255, 215, 0);
const sf::Color ACTIVE_COLOR(100, 255, 100);

// Action log
vector<string> action_log;
const int MAX_LOG = 12;

void add_log(const string& msg) {
    action_log.push_back(msg);
    if ((int)action_log.size() > MAX_LOG)
        action_log.erase(action_log.begin());
}

// =============================================
// DRAWING HELPERS
// =============================================

// Draw a bar (HP or Stamina)
void draw_bar(sf::RenderWindow& window, float x, float y,
    float width, float height,
    float current, float maximum,
    sf::Color fill_color, sf::Color bg_color,
    sf::Font& font, const string& label) {

    // Background
    sf::RectangleShape bg(sf::Vector2f(width, height));
    bg.setPosition(x, y);
    bg.setFillColor(bg_color);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1);
    window.draw(bg);

    // Fill
    float ratio = (maximum > 0) ? (current / maximum) : 0;
    if (ratio > 1) ratio = 1;
    if (ratio < 0) ratio = 0;

    sf::RectangleShape fill(sf::Vector2f(width * ratio, height));
    fill.setPosition(x, y);
    fill.setFillColor(fill_color);
    window.draw(fill);

    // Label text
    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(11);
    text.setFillColor(sf::Color::White);
    text.setString(label + ": " + to_string((int)current) + "/" + to_string((int)maximum));
    text.setPosition(x + 4, y + 2);
    window.draw(text);
}

// Draw a rounded panel
void draw_panel(sf::RenderWindow& window, float x, float y,
    float w, float h, sf::Color color, bool highlighted = false) {
    sf::RectangleShape panel(sf::Vector2f(w, h));
    panel.setPosition(x, y);
    panel.setFillColor(color);
    panel.setOutlineColor(highlighted ? ACTIVE_COLOR : BORDER_COLOR);
    panel.setOutlineThickness(highlighted ? 2 : 1);
    window.draw(panel);
}

// Draw text
void draw_text(sf::RenderWindow& window, sf::Font& font,
    const string& str, float x, float y,
    int size, sf::Color color) {
    sf::Text text;
    text.setFont(font);
    text.setString(str);
    text.setCharacterSize(size);
    text.setFillColor(color);
    text.setPosition(x, y);
    window.draw(text);
}

// =============================================
// DRAW PLAYER PANEL
// =============================================
void draw_player_panel(sf::RenderWindow& window, sf::Font& font,
    GameState* state, int idx, float x, float y) {
    auto& p = state->players[idx];
    bool is_active = (state->current_entity_type == 0 &&
        state->current_entity_id == idx);
    bool is_stunned = p.is_stunned;

    float pw = 260, ph = 160;

    // Panel background
    sf::Color panel_bg = is_stunned ? sf::Color(60, 20, 80) :
        (is_active ? sf::Color(20, 50, 80) : PANEL_COLOR);
    draw_panel(window, x, y, pw, ph, panel_bg, is_active);

    // Title
    string title = "Player " + to_string(idx + 1);
    if (!p.is_alive) title += " [DEAD]";
    else if (is_stunned) title += " [STUNNED]";
    else if (is_active) title += " << TURN";
    draw_text(window, font, title, x + 8, y + 6, 13,
        is_active ? ACTIVE_COLOR :
        (is_stunned ? STUN_COLOR :
            (p.is_alive ? PLAYER_COLOR : sf::Color(100, 100, 100))));

    if (!p.is_alive) return;

    // HP bar
    sf::Color hp_col = ((float)p.hp / p.max_hp < 0.3f) ? HP_LOW_COLOR : HP_COLOR;
    draw_bar(window, x + 8, y + 28, pw - 16, 16,
        p.hp, p.max_hp, hp_col,
        sf::Color(40, 40, 40), font, "HP");

    // Stamina bar
    draw_bar(window, x + 8, y + 50, pw - 16, 16,
        p.stamina, p.max_stamina, STAMINA_COLOR,
        sf::Color(40, 40, 40), font, "SP");

    // Stats
    draw_text(window, font, "DMG: " + to_string(p.damage) +
        "  SPD: " + to_string(p.speed),
        x + 8, y + 74, 11, TEXT_COLOR);

    // Inventory summary
    string inv_str = "INV: ";
    bool has_item = false;
    for (int i = 0; i < 20; i++) {
        if (p.inventory[i] != -1) {
            int wid = p.inventory[i];
            bool already = false;
            for (int j = 0; j < i; j++)
                if (p.inventory[j] == wid) { already = true; break; }
            if (!already) {
                if (has_item) inv_str += ", ";
                string wname = WEAPON_DB[wid].name;
                if (wname.length() > 8) wname = wname.substr(0, 8);
                inv_str += wname;
                has_item = true;
            }
        }
    }
    if (!has_item) inv_str += "empty";
    draw_text(window, font, inv_str, x + 8, y + 92, 10, GOLD_COLOR);

    // Long term storage count
    if (p.long_term_count > 0) {
        draw_text(window, font,
            "LTS: " + to_string(p.long_term_count) + " weapon(s)",
            x + 8, y + 108, 10, sf::Color(180, 180, 100));
    }

    // Ultimate eligibility
    bool has_solar = false, has_lunar = false;
    for (int i = 0; i < 20; i++) {
        if (p.inventory[i] == 0) has_solar = true;
        if (p.inventory[i] == 1) has_lunar = true;
    }
    if (has_solar && has_lunar) {
        draw_text(window, font, "*** ULTIMATE READY ***",
            x + 8, y + 124, 11, GOLD_COLOR);
    }
}

// =============================================
// DRAW ENEMY PANEL
// =============================================
void draw_enemy_panel(sf::RenderWindow& window, sf::Font& font,
    GameState* state, int idx, float x, float y) {
    auto& e = state->enemies[idx];
    bool is_active = (state->current_entity_type == 1 &&
        state->current_entity_id == idx);
    bool is_stunned = e.is_stunned;

    float ew = 110, eh = 100;

    sf::Color panel_bg = !e.is_alive ? sf::Color(20, 20, 20) :
        is_stunned ? sf::Color(60, 20, 80) :
        (is_active ? sf::Color(80, 20, 20) : PANEL_COLOR);
    draw_panel(window, x, y, ew, eh, panel_bg, is_active);

    // Title
    string title = "E" + to_string(idx + 1);
    if (!e.is_alive) title += " DEAD";
    else if (is_stunned) title += " STUN";
    else if (is_active) title += " <--";
    draw_text(window, font, title, x + 6, y + 5, 12,
        is_active ? ACTIVE_COLOR :
        (is_stunned ? STUN_COLOR :
            (e.is_alive ? ENEMY_COLOR : sf::Color(80, 80, 80))));

    if (!e.is_alive) return;

    // HP bar
    sf::Color hp_col = ((float)e.hp / e.max_hp < 0.3f) ? HP_LOW_COLOR : HP_COLOR;
    draw_bar(window, x + 4, y + 24, ew - 8, 14,
        e.hp, e.max_hp, hp_col,
        sf::Color(40, 40, 40), font, "HP");

    // Stamina bar
    draw_bar(window, x + 4, y + 44, ew - 8, 14,
        e.stamina, e.max_stamina, STAMINA_COLOR,
        sf::Color(40, 40, 40), font, "SP");

    // Stats
    draw_text(window, font,
        "DMG:" + to_string(e.damage),
        x + 6, y + 64, 10, TEXT_COLOR);
    draw_text(window, font,
        "SPD:" + to_string(e.speed),
        x + 6, y + 78, 10, TEXT_COLOR);
}

// =============================================
// DRAW ACTION LOG
// =============================================
void draw_action_log(sf::RenderWindow& window, sf::Font& font,
    GameState* state, float x, float y, float w, float h) {
    draw_panel(window, x, y, w, h, sf::Color(20, 20, 40));
    draw_text(window, font, "ACTION LOG", x + 8, y + 6, 13, GOLD_COLOR);

    float ly = y + 26;

    // ✅ ADD THIS - Display last action message first ✅
    if (state && strlen(state->last_action_message) > 0) {
        draw_text(window, font, state->last_action_message, x + 8, ly, 11, sf::Color(255, 200, 100));
        ly += 16;
    }

    // Display the action log history
    for (int i = (int)action_log.size() - 1; i >= 0; i--) {
        draw_text(window, font, action_log[i], x + 8, ly, 11, TEXT_COLOR);
        ly += 16;
        if (ly > y + h - 10) break;
    }
}

// =============================================
// DRAW ARTIFACT TABLE
// =============================================
void draw_artifact_table(sf::RenderWindow& window, sf::Font& font,
    GameState* state, float x, float y,
    float w, float h) {
    draw_panel(window, x, y, w, h, sf::Color(20, 20, 40));
    draw_text(window, font, "ARTIFACT TABLE", x + 8, y + 6, 13, GOLD_COLOR);

    float ay = y + 26;
    for (int i = 0; i < state->num_artifacts; i++) {
        auto& art = state->artifacts[i];
        string status = art.is_locked ?
            "Held by " + string(art.entity_type == 0 ? "P" : "E") +
            to_string(art.held_by + 1) : "FREE";
        sf::Color col = art.is_locked ? sf::Color(255, 150, 50) : sf::Color(100, 255, 100);
        draw_text(window, font,
            string(art.name) + ": " + status,
            x + 8, ay, 11, col);
        ay += 18;
    }
}

// =============================================
// DRAW GAME STATUS
// =============================================
void draw_game_status(sf::RenderWindow& window, sf::Font& font,
    GameState* state, float x, float y,
    float w, float h) {
    draw_panel(window, x, y, w, h, sf::Color(20, 20, 40));

    draw_text(window, font, "CHRONO RIFT", x + 8, y + 6, 16, GOLD_COLOR);
    draw_text(window, font,
        "Turn: " + to_string(state->turn_number),
        x + 8, y + 28, 12, TEXT_COLOR);
    draw_text(window, font,
        "Kills: " + to_string(state->total_kills) + " / 10",
        x + 8, y + 46, 12,
        state->total_kills >= 10 ? ACTIVE_COLOR : TEXT_COLOR);

    // Current turn indicator
    string turn_str = "NOW: ";
    if (state->current_entity_type == 0)
        turn_str += "Player " + to_string(state->current_entity_id + 1);
    else
        turn_str += "Enemy " + to_string(state->current_entity_id + 1);
    draw_text(window, font, turn_str, x + 8, y + 64, 12, ACTIVE_COLOR);

    // Ultimate active
    if (state->ultimate_active) {
        draw_text(window, font, "*** ULTIMATE ACTIVE ***",
            x + 8, y + 82, 12, GOLD_COLOR);
    }

    // Eclipse Relic
    if (state->eclipse_relic_available) {
        draw_text(window, font, "✨ ECLIPSE RELIC AVAILABLE!",
            x + 8, y + 100, 11, sf::Color(150, 100, 255));
    }

    // Game over
    if (!state->game_running) {
        string result = state->game_winner == 1 ? "VICTORY!" : "GAME OVER!";
        sf::Color result_col = state->game_winner == 1 ?
            ACTIVE_COLOR : HP_LOW_COLOR;
        draw_text(window, font, result, x + 8, y + 118, 18, result_col);
    }
}

// =============================================
// RENDERING THREAD (Section 9)
// =============================================
void rendering_thread(GameState* state) {
    sf::RenderWindow window(
        sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
        "Chrono Rift - Game Monitor",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(FPS);

    // Load font
    sf::Font font;
    if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")) {
        if (!font.loadFromFile("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf")) {
            cerr << "❌ Could not load font!" << endl;
            return;
        }
    }

    add_log("=== CHRONO RIFT STARTED ===");
    add_log("Roll Number: " + to_string(ROLL_NUMBER));

    int last_turn = -1;

    while (window.isOpen()) {
        // Handle events
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }
        sem_wait(&state->global_mutex);
        // Update action log when turn changes
        if (state->turn_number != last_turn) {
            last_turn = state->turn_number;
            string entity = state->current_entity_type == 0 ?
                "Player " + to_string(state->current_entity_id + 1) :
                "Enemy " + to_string(state->current_entity_id + 1);
            add_log("Turn " + to_string(state->turn_number) +
                ": " + entity + "'s turn");

            if (state->ultimate_active)
                add_log(">>> ULTIMATE ABILITY ACTIVE <<<");
            if (state->eclipse_relic_available)
                add_log(">>> ECLIPSE RELIC APPEARED <<<");
        }

        // Clear
        window.clear(BG_COLOR);

        // ---- HEADER ----
        draw_text(window, font,
            "CHRONO RIFT  |  Roll: " + to_string(ROLL_NUMBER) +
            "  |  Turn: " + to_string(state->turn_number) +
            "  |  Kills: " + to_string(state->total_kills) + "/10",
            10, 8, 14, GOLD_COLOR);

        // Divider
        sf::RectangleShape divider(sf::Vector2f(WINDOW_WIDTH - 20, 1));
        divider.setPosition(10, 28);
        divider.setFillColor(BORDER_COLOR);
        window.draw(divider);

        // ---- PLAYERS SECTION ----
        draw_text(window, font, "PLAYERS", 10, 35, 13, PLAYER_COLOR);
        for (int i = 0; i < state->num_players; i++) {
            draw_player_panel(window, font, state, i,
                10 + i * 270, 55);
        }

        // ---- ENEMIES SECTION ----
        draw_text(window, font, "ENEMIES", 10, 230, 13, ENEMY_COLOR);
        for (int i = 0; i < state->num_enemies; i++) {
            draw_enemy_panel(window, font, state, i,
                10 + i * 118, 250);
        }

        // ---- RIGHT PANELS ----
        // Game status
        draw_game_status(window, font, state,
            10, 370, 300, 160);

        // Artifact table
        draw_artifact_table(window, font, state,
            320, 370, 300, 160);

        draw_action_log(window, font, state, 10, 545, 610, 240);

        // ---- INVENTORY DISPLAY ----
        draw_text(window, font, "INVENTORY", 640, 370, 13, GOLD_COLOR);
        float inv_y = 390;
        for (int p = 0; p < state->num_players; p++) {
            draw_text(window, font,
                "P" + to_string(p + 1) + ": [",
                640, inv_y, 11, PLAYER_COLOR);
            float slot_x = 680;
            for (int s = 0; s < 20; s++) {
                int wid = state->players[p].inventory[s];
                sf::RectangleShape slot(sf::Vector2f(24, 18));
                slot.setPosition(slot_x + s * 26, inv_y);
                slot.setFillColor(wid == -1 ?
                    sf::Color(40, 40, 40) : sf::Color(80, 60, 120));
                slot.setOutlineColor(BORDER_COLOR);
                slot.setOutlineThickness(1);
                window.draw(slot);
                if (wid != -1) {
                    string wname = string(1, WEAPON_DB[wid].name[0]);
                    draw_text(window, font, wname,
                        slot_x + s * 26 + 7, inv_y + 2,
                        11, GOLD_COLOR);
                }
            }
            inv_y += 26;
        }
        sem_post(&state->global_mutex);
        window.display();
    }
}

// =============================================
// MAIN
// =============================================
int main() {
    cout << "========================================" << endl;
    cout << "   CHRONO RIFT - RENDER PROCESS" << endl;
    cout << "========================================" << endl;
    cout << "Attaching to shared memory..." << endl;

    // Wait for arbiter to create shared memory
    GameState* state = nullptr;
    int attempts = 0;
    while (attempts < 30) {
        state = attach_shared_memory();
        if (state != MAP_FAILED && state != nullptr) break;
        cout << "Waiting for arbiter..." << endl;
        sleep(1);
        attempts++;
    }

    if (state == MAP_FAILED || state == nullptr) {
        cerr << "❌ Could not attach to shared memory!" << endl;
        cerr << "Make sure arbiter is running first!" << endl;
        return 1;
    }

    cout << "✅ Connected to shared memory!" << endl;
    cout << "🎮 Opening game window..." << endl;

    // Run rendering in dedicated thread (Section 9)
    thread render_thread(rendering_thread, state);
    render_thread.join();

    cout << "👋 Render process shutting down." << endl;
    return 0;
}

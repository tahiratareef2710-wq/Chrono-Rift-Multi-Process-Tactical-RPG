// main_menu.cpp - Standalone launcher for Chrono Rift
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string>

using namespace std;

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

pid_t arbiter_pid = -1, hip_pid = -1, hip2_pid = -1, asp_pid = -1, render_pid = -1;

void clear_screen() {
    cout << "\033[2J\033[H";
}

void print_ascii_art() {
    cout << CYAN << R"(
   ╔═══════════════════════════════════════════════════════════════╗
   ║                                                               ║
   ║      ██████╗██╗  ██╗██████╗  ██████╗ ███╗   ██╗ ██████╗       ║
   ║     ██╔════╝██║  ██║██╔══██╗██╔═══██╗████╗  ██║██╔═══██╗      ║
   ║     ██║     ███████║██████╔╝██║   ██║██╔██╗ ██║██║   ██║      ║
   ║     ██║     ██╔══██║██╔══██╗██║   ██║██║╚██╗██║██║   ██║      ║
   ║     ╚██████╗██║  ██║██║  ██║╚██████╔╝██║ ╚████║╚██████╔╝      ║
   ║      ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝ ╚═════╝       ║
   ║                                                               ║
   ║                    ╔══════════════════════╗                   ║
   ║                    ║    C H R O N O       ║                   ║
   ║                    ║      R I F T         ║                   ║
   ║                    ╚══════════════════════╝                   ║
   ║                                                               ║
   ║              ┌─────────────────────────────────┐              ║
   ║              │    OPERATING SYSTEMS PROJECT    │              ║
   ║              │         SPRING 2026             │              ║
   ║              └─────────────────────────────────┘              ║
   ║                                                               ║
   ╚═══════════════════════════════════════════════════════════════╝
   )" << RESET << endl;
}

void print_menu() {
    cout << "\n";
    cout << "   ╔════════════════════════════════════════════════════════╗\n";
    cout << "   ║" << BOLD << "M A I N   M E N U" << RESET << "          ║\n";
    cout << "   ╠════════════════════════════════════════════════════════╣\n";
    cout << "   ║                                                        ║\n";
    cout << "   ║   " << GREEN << "1" << RESET << ". 🎮 Start New Game   ║\n";
    cout << "   ║   " << GREEN << "2" << RESET << ". 📖 How to Play      ║\n";
    cout << "   ║   " << GREEN << "3" << RESET << ". 🏆 Credits          ║\n";
    cout << "   ║   " << RED << "4" << RESET << ". 🚪 Exit Game          ║\n";
    cout << "   ║                                                        ║\n";
    cout << "   ╚════════════════════════════════════════════════════════╝\n";
    cout << "\n   👉 " << YELLOW << "Enter your choice (1-4): " << RESET;
}

void show_how_to_play() {
    clear_screen();
    cout << BLUE << "\n   ╔══════════════════════════════════════════════════════════════╗\n";
    cout << "             ║ " << BOLD << "H O W   T O   P L A Y" << RESET << BLUE << "   ║\n";
    cout << "             ╚══════════════════════════════════════════════════════════════╝\n" << RESET;
    cout << "\n   " << YELLOW << "📌 GAME OVERVIEW" << RESET << endl;
    cout << "   ───────────────────────────────────────────────────────────────\n";
    cout << "   Chrono Rift is a turn-based tactical RPG where your party\n";
    cout << "   fights against computer-controlled enemies.\n\n";

    cout << "   " << YELLOW << "⚔️  ACTIONS (Your Turn)" << RESET << endl;
    cout << "   ───────────────────────────────────────────────────────────────\n";
    cout << "   1. Attack (Strike)  - Deal damage to enemy HP\n";
    cout << "   2. Attack (Exhaust) - Drain enemy stamina\n";
    cout << "   3. Use Weapon       - Attack using a weapon from inventory\n";
    cout << "   4. Swap In          - Bring weapon from long term storage\n";
    cout << "   5. Heal             - Restore 10%% of your HP\n";
    cout << "   6. Skip             - Skip turn (stamina goes to 50%%)\n";
    cout << "   0. Stun Attack      - Stun enemy for 3 seconds\n\n";

    cout << "   " << YELLOW << "🗡️  WEAPON SYSTEM" << RESET << endl;
    cout << "   ───────────────────────────────────────────────────────────────\n";
    cout << "   • Weapons take up contiguous inventory slots (2-10 slots)\n";
    cout << "   • Inventory has 20 slots total\n";
    cout << "   • Solar Core + Lunar Blade = Ultimate Ability!\n";
    cout << "   • Eclipse Relic appears after 3 kills\n\n";

    cout << "   " << YELLOW << "🏆 WIN CONDITIONS" << RESET << endl;
    cout << "   ───────────────────────────────────────────────────────────────\n";
    cout << "   • Kill 10 enemies → VICTORY!\n";
    cout << "   • All players die → GAME OVER\n";
    cout << "   • Type 0 in action menu to quit\n\n";

    cout << "\n   " << CYAN << "Press Enter to return to main menu..." << RESET;
    cin.ignore();
    cin.get();
}

void show_credits() {
    clear_screen();
    cout << MAGENTA << "\n   ╔══════════════════════════════════════════════════════════════╗\n";
    cout << "                ║" << BOLD << "C R E D I T S" << RESET << MAGENTA << "         ║\n";
    cout << "                ╚══════════════════════════════════════════════════════════════╝\n" << RESET;
    cout << "\n   " << YELLOW << "🎓 PROJECT INFORMATION" << RESET << endl;
    cout << "   ───────────────────────────────────────────────────────────────\n";
    cout << "   Course        : CS 2006 - Operating Systems\n";
    cout << "   Semester      : Spring 2026\n";
    cout << "   Project       : Chrono Rift\n\n";

    cout << "   " << YELLOW << "👥 DEVELOPERS" << RESET << endl;
    cout << "   ───────────────────────────────────────────────────────────────\n";
    cout << "   Student 1     : Sukaina Zainab (Roll No: 240566)\n";
    cout << "   Student 2     : Tahira Tareef (Roll No: 240721)\n\n";

    cout << "   " << YELLOW << "🔧 TECHNICAL FEATURES" << RESET << endl;
    cout << "   ───────────────────────────────────────────────────────────────\n";
    cout << "   • Process Isolation (Arbiter, HIP, ASP)\n";
    cout << "   • Shared Memory IPC (No pipes!)\n";
    cout << "   • Multi-threading (One thread per entity)\n";
    cout << "   • Stamina-based Scheduling\n";
    cout << "   • Signal-based Stun (SIGUSR1)\n";
    cout << "   • Signal-only Ultimate (SIGSTOP/SIGCONT/SIGALRM)\n";
    cout << "   • Artifact Deadlock Detection\n";
    cout << "   • Contiguous Inventory Allocator\n";
    cout << "   • SFML Real-time GUI\n";
    cout << "   • Local Multiplayer (Bonus) - Two separate HIP processes!\n\n";

    cout << "\n   " << CYAN << "Press Enter to return to main menu..." << RESET;
    cin.ignore();
    cin.get();
}

void cleanup_game_processes() {
    cout << "\n   🧹 Cleaning up game processes..." << endl;

    if (arbiter_pid > 0) kill(arbiter_pid, SIGTERM);
    if (hip_pid > 0) kill(hip_pid, SIGTERM);
    if (hip2_pid > 0) kill(hip2_pid, SIGTERM);
    if (asp_pid > 0) kill(asp_pid, SIGTERM);
    if (render_pid > 0) kill(render_pid, SIGTERM);

    usleep(500000);

    if (arbiter_pid > 0) kill(arbiter_pid, SIGKILL);
    if (hip_pid > 0) kill(hip_pid, SIGKILL);
    if (hip2_pid > 0) kill(hip2_pid, SIGKILL);
    if (asp_pid > 0) kill(asp_pid, SIGKILL);
    if (render_pid > 0) kill(render_pid, SIGKILL);
}

void start_game() {
    clear_screen();
    cout << GREEN << "\n   ╔══════════════════════════════════════════════════════════════╗\n";
    cout << "              ║" << BOLD << "L A U N C H I N G" << RESET << GREEN << "       ║\n";
    cout << "              ╚══════════════════════════════════════════════════════════════╝\n" << RESET;

    // Ask game mode FIRST
    cout << "\n ╔════════════════════════════════════╗\n";
    cout << "   ║        SELECT GAME MODE            ║\n";
    cout << "   ╠════════════════════════════════════╣\n";
    cout << "   ║  1. Single Player                  ║\n";
    cout << "   ║  2. 🎮 Local Multiplayer (Bonus!)  ║\n";
    cout << "   ╚════════════════════════════════════╝\n";
    cout << "   Choice: ";
    int mode;
    cin >> mode;

    bool is_multiplayer = (mode == 2);

    if (is_multiplayer) {
        cout << "\n   " << YELLOW << "⚠️  MULTIPLAYER MODE INSTRUCTIONS:" << RESET << endl;
        cout << "   • When HIP1 asks for party size, enter " << BOLD << "1" << RESET << endl;
        cout << "   • HIP2 will automatically create Player 2" << endl;
        cout << "   • Both players compete to kill 10 enemies first!" << endl;
        cout << "\n   Press Enter to continue...";
        cin.ignore();
        cin.get();
        clear_screen();
        cout << GREEN << "\n   🚀 Starting Chrono Rift in MULTIPLAYER mode...\n\n" << RESET;
    }
    else {
        cout << "\n   🚀 Starting Chrono Rift in SINGLE PLAYER mode...\n\n" << RESET;
    }

    // Start Arbiter with multiplayer flag
    arbiter_pid = fork();
    if (arbiter_pid == 0) {
        if (is_multiplayer) {
            execl("./arbiter_out", "arbiter_out", "--multiplayer", nullptr);
        }
        else {
            execl("./arbiter_out", "arbiter_out", nullptr);
        }
        exit(1);
    }
    sleep(1);

    // Start ASP
    asp_pid = fork();
    if (asp_pid == 0) {
        execl("./asp_out", "asp_out", nullptr);
        exit(1);
    }
    sleep(1);

    // Start HIP1 (Human Interface Process)
    hip_pid = fork();
    if (hip_pid == 0) {
        execlp("xterm", "xterm", "-title", "Player 1", "-e", "./hip_out", nullptr);
        exit(1);
    }
    sleep(1);

    // Start HIP2 if multiplayer
    if (is_multiplayer) {
        cout << "   🎮 Starting HIP2 (Player 2)..." << endl;
        hip2_pid = fork();
        if (hip2_pid == 0) {
            execlp("xterm", "xterm", "-title", "Player 2", "-e", "./hip2_out", nullptr);
            exit(1);
        }
        sleep(1);
        cout << "   ✅ HIP2 started! Player 2 is now active." << endl;
    }

    // Start Render
    render_pid = fork();
    if (render_pid == 0) {
        execl("./render_out", "render_out", nullptr);
        exit(1);
    }

    cout << "   ✅ All processes started!\n";
    cout << "   📺 SFML window should appear shortly.\n";
    cout << "   🎮 Game is running.\n\n";
    cout << "   " << YELLOW << "⚠️  IMPORTANT: When game ends, press Enter to return to menu." << RESET << "\n\n";

    // Wait for arbiter to finish (game ends)
    int status;
    waitpid(arbiter_pid, &status, 0);

    cout << GREEN << "\n   🏁 Game has ended!" << RESET << endl;

    // Cleanup child processes
    cleanup_game_processes();

    cout << "\n   " << CYAN << "Press Enter to return to main menu..." << RESET;
    cin.ignore();
    cin.get();
}

int main() {
    // Check if executables exist
    if (access("./arbiter_out", F_OK) != 0 ||
        access("./hip_out", F_OK) != 0 ||
        access("./asp_out", F_OK) != 0 ||
        access("./render_out", F_OK) != 0) {
        cout << RED << "\n❌ Executables not found! Please run 'make' first.\n" << RESET;
        cout << "   Run: make clean && make\n\n";
        return 1;
    }

    int choice;

    while (true) {
        clear_screen();
        print_ascii_art();
        print_menu();

        cin >> choice;

        switch (choice) {
        case 1:
            start_game();
            break;

        case 2:
            show_how_to_play();
            break;

        case 3:
            show_credits();
            break;

        case 4:
            clear_screen();
            cout << RED << "\n   ╔═══════════════════════════════════════════════════════════════════════╗\n";
            cout << "            ║                                                                       ║\n";
            cout << "            ║" << BOLD << "Thank you for playing Chrono Rift!" << RESET << RED << " ║\n";
            cout << "            ║                                                                       ║\n";
            cout << "            ║                " << BOLD << "Goodbye!" << RESET << RED << "           ║\n";
            cout << "            ║                                                                       ║\n";
            cout << "            ╚═══════════════════════════════════════════════════════════════════════╝\n" << RESET;
            cout << endl;
            return 0;

        default:
            cout << RED << "\n   ❌ Invalid choice! Please enter 1-4.\n" << RESET;
            sleep(1);
        }
    }

    return 0;
}
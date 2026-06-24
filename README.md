# Chrono Rift

> A multi-process tactical RPG built in C++ using core Linux Operating Systems concepts.

![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)
![Docker](https://img.shields.io/badge/Docker-2496ED?style=flat&logo=docker&logoColor=white)


---

## About

Chrono Rift is a turn-based tactical RPG where a human player engages in 
combat against computer-controlled enemies. Entities compete for execution 
time based on a stamina-driven scheduling system inspired by classic RPG mechanics.

---
## Team
1. Tahira Tareef
2. Sukaina Zainab

## Key Features

| Feature | Description |
|---|---|
| Multi-Process Architecture | Separate processes for Arbiter, Human Interface, and AI |
| Stamina Scheduling | Dynamic turn system based on entity speed and stamina |
| Deadlock Detection | Background thread monitors and resolves circular waits |
| Async Stun Mechanic | UNIX signals halt enemy execution instantly |
| Inventory System | 20-slot linear array with memory allocation logic |
| Real-Time GUI | Live stamina bars, health stats, and action log |
| NPC AI | Dedicated thread per enemy for concurrent execution |

---

## Technologies

- **Language:** C++
- **OS:** Linux (Ubuntu)
- **IPC:** Shared Memory, Semaphores, POSIX Signals
- **Threading:** Pthreads
- **GUI:** SFML / ncurses
- **Environment:** Docker

---

## How to Run

```bash
# Clone the repository
git clone https://github.com/tahiratareef2710-wq/Chrono-Rift-Multi-Process-Tactical-RPG.git

# Navigate to project directory
cd Chrono-Rift-Multi-Process-Tactical-RPG

# Build using Makefile
make

# Run the game
./chronorift
```

---




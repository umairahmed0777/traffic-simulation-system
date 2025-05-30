# 🚦 Smart Traffic Simulation System (C++ with SFML & Pthreads)

This project is a simulation of a smart traffic intersection using **Object-Oriented Programming (OOP)** in **C++**, built with **SFML for graphics** and **Pthreads** for concurrency. It simulates real-world traffic behavior including traffic lights, emergency vehicle priority, vehicle breakdowns, and resource management using Banker's Algorithm.

---

## 📌 Features

- 🚗 Vehicle generation with types: Regular, Heavy, Emergency
- 🚦 Dynamic traffic light control with emergency priority
- ⚠️ Vehicle breakdown detection and analytics
- 💸 Speeding challan system with mock Stripe payment
- 🧠 Deadlock prevention using Banker's Algorithm
- 📊 Analytics and logs saved to `analytics.txt`
- 👤 User portal for challan details and payment
- 🎨 Real-time graphical simulation using **SFML**

---

## 🧰 Technologies Used

- **C++** with STL
- **SFML** for graphics
- **Pthreads** for multithreading
- **Banker's Algorithm** for resource safety
- File I/O for analytics saving

---

## 🖥️ How to Run

### 1. Prerequisites

- C++ compiler (e.g., g++)
- [SFML 2.x](https://www.sfml-dev.org/download.php) installed
- POSIX system (Linux/macOS or WSL for Windows)

### 2. Compile

```bash
g++ -o traffic_simulation SmartTrafficIntersection.cpp -lsfml-graphics -lsfml-window -lsfml-system -lpthread

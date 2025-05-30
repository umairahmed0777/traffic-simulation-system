// SmartTrafficIntersection.cpp

#include <SFML/Graphics.hpp>
#include <iostream>
#include <pthread.h>
#include <queue>
#include <cstdlib>
#include <unistd.h>
#include <ctime>
#include <map>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <fstream>


using namespace std;

// Constants
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int LANE_WIDTH = 100;
const int VEHICLE_SIZE = 15;
const int TRAFFIC_LIGHT_RADIUS = 20;
const int GREEN_LIGHT_DURATION = 10; // Seconds for green light
const int YELLOW_LIGHT_DURATION = 3; // Seconds for yellow light
const int SPEED_LIMIT = 10;          // Speed limit in pixels per frame
const float EMERGENCY_PRIORITY_TIME = 2.0; // Reduced time for emergency lights
const int MAX_LANE_CAPACITY = 10; // Maximum vehicles per lane
const int BREAKDOWN_PROBABILITY = 5; // Probability of breakdown (in percentage)
const int MAX_RESOURCES = 10; // Example resource limit for Banker's Algorithm

// Directions
enum Direction { NORTH = 0, SOUTH, EAST, WEST };

// Lanes
enum Lane { LANE1 = 0, LANE2 }; // LANE1: Incoming, LANE2: Outgoing

// Vehicle types
enum VehicleType { REGULAR, HEAVY, EMERGENCY };

// Vehicle structure
struct Vehicle {
    sf::RectangleShape shape;
    Direction direction;
    Lane lane;
    VehicleType type;
    string vehicleNumber;
    bool challanIssued;
    float speed; // Speed in pixels per frame
    bool breakdown;
};

// Traffic light structure
struct TrafficLight {
    sf::CircleShape red;
    sf::CircleShape yellow;
    sf::CircleShape green;
    bool emergencyPriority;
};

// Challan structure
struct Challan {
    string challanID;
    string vehicleNumber;
    float amount;
    string issueDate;
    string dueDate;
    bool paid;
};

// Shared variables
queue<Vehicle> trafficQueues[4][2];         // Separate queues for each direction and lane
pthread_mutex_t queueLocks[4][2];          // Mutex for each direction and lane
TrafficLight trafficLights[4];
Direction currentGreenDirection = NORTH;
pthread_mutex_t trafficLightLock;

// Analytics
map<string, int> analytics = {
    {"totalVehicles", 0},
    {"emergencyVehicles", 0},
    {"challansIssued", 0},
    {"totalFineAmount", 0},
    {"breakdowns", 0}
};

// Challan and Stripe variables
map<string, Challan> challans;

// Banker's Algorithm data structures
int availableResources = MAX_RESOURCES;
map<string, int> allocatedResources;
map<string, int> maximumResources;

pthread_mutex_t resourceLock;

// Mock time
time_t mockTime = time(nullptr);
pthread_mutex_t timeLock;

// SFML window for graphics
sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Smart Traffic Intersection");

// Utility function to format time
string formatTime(time_t rawTime) {
    struct tm* timeInfo = localtime(&rawTime);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
    return string(buffer);
}

// Function to initialize traffic lights
void initializeTrafficLights() {
    for (int i = 0; i < 4; ++i) {
        trafficLights[i].red.setRadius(TRAFFIC_LIGHT_RADIUS);
        trafficLights[i].yellow.setRadius(TRAFFIC_LIGHT_RADIUS);
        trafficLights[i].green.setRadius(TRAFFIC_LIGHT_RADIUS);

        trafficLights[i].red.setFillColor(sf::Color::Red);
        trafficLights[i].yellow.setFillColor(sf::Color::Black);
        trafficLights[i].green.setFillColor(sf::Color::Black);

        trafficLights[i].emergencyPriority = false;
    }

    // Position traffic lights
    // NORTH
    trafficLights[NORTH].red.setPosition(WINDOW_WIDTH / 2 - 40, 50);
    trafficLights[NORTH].yellow.setPosition(WINDOW_WIDTH / 2, 50);
    trafficLights[NORTH].green.setPosition(WINDOW_WIDTH / 2 + 40, 50);

    // SOUTH
    trafficLights[SOUTH].red.setPosition(WINDOW_WIDTH / 2 - 40, WINDOW_HEIGHT - 100);
    trafficLights[SOUTH].yellow.setPosition(WINDOW_WIDTH / 2, WINDOW_HEIGHT - 100);
    trafficLights[SOUTH].green.setPosition(WINDOW_WIDTH / 2 + 40, WINDOW_HEIGHT - 100);

    // WEST
    trafficLights[WEST].red.setPosition(50, WINDOW_HEIGHT / 2 - 40);
    trafficLights[WEST].yellow.setPosition(50, WINDOW_HEIGHT / 2);
    trafficLights[WEST].green.setPosition(50, WINDOW_HEIGHT / 2 + 40);

    // EAST
    trafficLights[EAST].red.setPosition(WINDOW_WIDTH - 100, WINDOW_HEIGHT / 2 - 40);
    trafficLights[EAST].yellow.setPosition(WINDOW_WIDTH - 100, WINDOW_HEIGHT / 2);
    trafficLights[EAST].green.setPosition(WINDOW_WIDTH - 100, WINDOW_HEIGHT / 2 + 40);
}

// Function to issue challans for speeding
void issueChallan(Vehicle& vehicle) {
    if (vehicle.challanIssued || vehicle.type == EMERGENCY) return;

    vehicle.challanIssued = true;
    analytics["challansIssued"]++;

    // Generate a challan
    string challanID = "CH" + to_string(rand() % 10000);
    string issueDate = formatTime(mockTime);
    string dueDate = formatTime(mockTime + (7 * 24 * 60 * 60)); // Due date after 7 days
    float fineAmount = 1.17*((vehicle.speed - SPEED_LIMIT) * 100);
    fineAmount = max(0.0f, fineAmount); // Ensure no negative fines

    Challan challan = {challanID, vehicle.vehicleNumber, fineAmount, issueDate, dueDate, false};
    challans[vehicle.vehicleNumber] = challan;

    analytics["totalFineAmount"] += fineAmount;

    cout << "Challan issued! Vehicle Number: " << vehicle.vehicleNumber << " Fine Amount: $" << fineAmount << endl;
}

// Stripe payment simulation
bool stripePayment(string challanID, float amountPaid) {
    if (challans.find(challanID) == challans.end()) return false;

    Challan& challan = challans[challanID];
    if (amountPaid >= challan.amount) {
        challan.paid = true;
        cout << "Payment successful for Challan ID: " << challanID << endl;
        return true;
    } else {
        cout << "Insufficient payment for Challan ID: " << challanID << endl;
        return false;
    }
}


// User portal to display challan details
void userPortal() {
    string vehicleNumber;
    cout << "Enter vehicle number: ";
    cin >> vehicleNumber;

    if (challans.find(vehicleNumber) == challans.end()) {
        cout << "No challans found for Vehicle Number: " << vehicleNumber << endl;
        return;
    }

    Challan& challan = challans[vehicleNumber];

    // Display challan details and payment options
    cout << "Challan Details for Vehicle Number: " << vehicleNumber << endl;
    cout << "Challan ID: " << challan.challanID << endl;
    cout << "Fine Amount: $" << challan.amount << endl;
    cout << "Issue Date: " << challan.issueDate << endl;
    cout << "Due Date: " << challan.dueDate << endl;
    cout << "Payment Status: " << (challan.paid ? "Paid" : "Unpaid") << endl;

    if (!challan.paid) {
        cout << "Do you want to pay the challan? (y/n): ";
        char choice;
        cin >> choice;

        if (choice == 'y' || choice == 'Y') {
            float amountPaid;
            cout << "Enter amount to pay: ";
            cin >> amountPaid;

            if (amountPaid >= challan.amount) {
                challan.paid = true;
                cout << "Payment successful!" << endl;
            } else {
                cout << "Insufficient payment. Please try again." << endl;
            }
        }
    }
}
// Function to handle breakdowns
void handleBreakdown(Vehicle& vehicle) {
    if (vehicle.breakdown) return;

    vehicle.breakdown = true;
    analytics["breakdowns"]++;

    cout << "Vehicle breakdown! Vehicle Number: " << vehicle.vehicleNumber << endl;
}

// Banker's Algorithm for deadlock prevention
bool isSafeState() {
    int work = availableResources;
    map<string, bool> finish;
    for (auto& [vehicle, allocated] : allocatedResources) {
        finish[vehicle] = false;
    }

    bool progress = true;
    while (progress) {
        progress = false;
        for (auto& [vehicle, allocated] : allocatedResources) {
            if (!finish[vehicle] && allocated <= work) {
                work += allocated;
                finish[vehicle] = true;
                progress = true;
            }
        }
    }

    for (auto& [vehicle, completed] : finish) {
        if (!completed) return false;
    }
    return true;
}

bool requestResources(string vehicleId, int request) {
    pthread_mutex_lock(&resourceLock);

    if (request > availableResources) {
        pthread_mutex_unlock(&resourceLock);
        return false; // Request cannot be granted immediately
    }

    availableResources -= request;
    allocatedResources[vehicleId] += request;

    if (!isSafeState()) {
        availableResources += request;
        allocatedResources[vehicleId] -= request;
        pthread_mutex_unlock(&resourceLock);
        return false;
    }

    pthread_mutex_unlock(&resourceLock);
    return true;
}

void releaseResources(string vehicleId, int release) {
    pthread_mutex_lock(&resourceLock);

    allocatedResources[vehicleId] -= release;
    availableResources += release;

    pthread_mutex_unlock(&resourceLock);
}

// Function to manage queues and enforce lane capacity
void manageQueues(Direction direction) {
    for (int lane = 0; lane < 2; ++lane) {
        pthread_mutex_lock(&queueLocks[direction][lane]);
        while (trafficQueues[direction][lane].size() > MAX_LANE_CAPACITY) {
            trafficQueues[direction][lane].pop();
            cout << "Queue overflow! Vehicle removed from " << direction << " lane " << lane << endl;
            analytics["totalVehicles"]--; // Adjust total vehicles count
        }
        pthread_mutex_unlock(&queueLocks[direction][lane]);
    }
}

// Function to simulate vehicle arrival
void generateVehicle(Direction direction, Lane lane) {
    pthread_mutex_lock(&queueLocks[direction][lane]);
    Vehicle vehicle;
    // Randomly assign vehicle type
    int randType = rand() % 100;
    if (randType < 10) {
        vehicle.type = EMERGENCY;
    } else if (randType < 30) {
        vehicle.type = HEAVY;
    } else {
        vehicle.type = REGULAR;
    }

    // Determine if the vehicle has a breakdown
    vehicle.breakdown = (rand() % 100 < BREAKDOWN_PROBABILITY);

    // Assign speed based on vehicle type
    if (vehicle.type == REGULAR) {
        vehicle.speed = SPEED_LIMIT + (rand() % 5); // 10-14
    } else if (vehicle.type == HEAVY) {
        vehicle.speed = SPEED_LIMIT - 2 + (rand() % 3); // 8-10
    } else { // EMERGENCY
        vehicle.speed = SPEED_LIMIT + 5 + (rand() % 5); // 15-19
    }

    vehicle.direction = direction;
    vehicle.lane = lane;
    vehicle.vehicleNumber = "VEH" + to_string(rand() % 1000);
    vehicle.challanIssued = false;

    vehicle.shape.setSize(sf::Vector2f(VEHICLE_SIZE, VEHICLE_SIZE));
    if (vehicle.type == EMERGENCY) {
        vehicle.shape.setFillColor(sf::Color::Red);
    } else if (vehicle.type == HEAVY) {
        vehicle.shape.setFillColor(sf::Color(128, 0, 128)); // Purple for heavy vehicles
    } else {
        vehicle.shape.setFillColor(sf::Color::Blue);
    }

    // Set initial position based on direction and lane
    if (direction == NORTH) {
        if (lane == LANE1) { // Incoming
            vehicle.shape.setPosition(WINDOW_WIDTH / 2 + lane * LANE_WIDTH / 2, -VEHICLE_SIZE);
        } else { // Outgoing
            vehicle.shape.setPosition(WINDOW_WIDTH / 2 + lane * LANE_WIDTH / 2, WINDOW_HEIGHT / 2 + LANE_WIDTH / 2);
        }
    } else if (direction == SOUTH) {
        if (lane == LANE1) { // Incoming
            vehicle.shape.setPosition(WINDOW_WIDTH / 2 - lane * LANE_WIDTH / 2, WINDOW_HEIGHT);
        } else { // Outgoing
            vehicle.shape.setPosition(WINDOW_WIDTH / 2 - lane * LANE_WIDTH / 2, WINDOW_HEIGHT / 2 - LANE_WIDTH / 2);
        }
    } else if (direction == EAST) {
        if (lane == LANE1) { // Incoming
            vehicle.shape.setPosition(WINDOW_WIDTH, WINDOW_HEIGHT / 2 + lane * LANE_WIDTH / 2);
        } else { // Outgoing
            vehicle.shape.setPosition(WINDOW_WIDTH / 2 - LANE_WIDTH / 2, WINDOW_HEIGHT / 2 + lane * LANE_WIDTH / 2);
        }
    } else if (direction == WEST) {
        if (lane == LANE1) { // Incoming
            vehicle.shape.setPosition(-VEHICLE_SIZE, WINDOW_HEIGHT / 2 - lane * LANE_WIDTH / 2);
        } else { // Outgoing
            vehicle.shape.setPosition(WINDOW_WIDTH / 2 + LANE_WIDTH / 2, WINDOW_HEIGHT / 2 - lane * LANE_WIDTH / 2);
        }
    }

    trafficQueues[direction][lane].push(vehicle);
    analytics["totalVehicles"]++;
    if (vehicle.type == EMERGENCY) {
        analytics["emergencyVehicles"]++;
    }
    if (vehicle.breakdown) {
        handleBreakdown(vehicle);
    }

    pthread_mutex_unlock(&queueLocks[direction][lane]);
}

// Function to handle traffic light control
void* trafficLightController(void* arg) {
    while (window.isOpen()) {
        pthread_mutex_lock(&trafficLightLock);

        // Check for emergency vehicles and prioritize their direction
        bool emergencyFound = false;
        for (int dir = 0; dir < 4 && !emergencyFound; ++dir) {
            for (int lane = 0; lane < 2 && !emergencyFound; ++lane) {
                pthread_mutex_lock(&queueLocks[dir][lane]);
                if (!trafficQueues[dir][lane].empty() && trafficQueues[dir][lane].front().type == EMERGENCY) {
                    currentGreenDirection = static_cast<Direction>(dir);
                    trafficLights[dir].emergencyPriority = true;
                    emergencyFound = true;
                }
                pthread_mutex_unlock(&queueLocks[dir][lane]);
            }
        }

        if (!emergencyFound) {
            trafficLights[currentGreenDirection].emergencyPriority = false;
            // Round-robin traffic light switching
            currentGreenDirection = static_cast<Direction>((currentGreenDirection + 1) % 4);
        }

        // Update traffic light states
        for (int i = 0; i < 4; ++i) {
            if (i == currentGreenDirection) {
                trafficLights[i].red.setFillColor(sf::Color::Black);
                trafficLights[i].green.setFillColor(sf::Color::Green);
                trafficLights[i].yellow.setFillColor(sf::Color::Black);
            } else {
                trafficLights[i].red.setFillColor(sf::Color::Red);
                trafficLights[i].green.setFillColor(sf::Color::Black);
                trafficLights[i].yellow.setFillColor(sf::Color::Black);
            }
        }

        pthread_mutex_unlock(&trafficLightLock);

        // Simulate green light duration
        sleep(GREEN_LIGHT_DURATION);

        // Transition to yellow light
        pthread_mutex_lock(&trafficLightLock);
        if (trafficLights[currentGreenDirection].green.getFillColor() == sf::Color::Green) {
            trafficLights[currentGreenDirection].green.setFillColor(sf::Color::Black);
            trafficLights[currentGreenDirection].yellow.setFillColor(sf::Color::Yellow);
        }
        pthread_mutex_unlock(&trafficLightLock);

        // Simulate yellow light duration
        sleep(YELLOW_LIGHT_DURATION);

        // Transition to red light
        pthread_mutex_lock(&trafficLightLock);
        trafficLights[currentGreenDirection].yellow.setFillColor(sf::Color::Black);
        trafficLights[currentGreenDirection].red.setFillColor(sf::Color::Red);
        pthread_mutex_unlock(&trafficLightLock);
    }
    return nullptr;
}

// Function to move vehicles based on traffic light state
void moveVehicles(Direction direction) {
    for (int lane = 0; lane < 2; ++lane) {
        pthread_mutex_lock(&queueLocks[direction][lane]);
        queue<Vehicle>& vehicleQueue = trafficQueues[direction][lane];
        size_t size = vehicleQueue.size();

        for (size_t i = 0; i < size; ++i) {
            Vehicle vehicle = vehicleQueue.front();
            vehicleQueue.pop();

            // Check if the traffic light is green for this direction
            bool isGreen = false;
            pthread_mutex_lock(&trafficLightLock);
            if (trafficLights[direction].green.getFillColor() == sf::Color::Green) {
                isGreen = true;
            }
            pthread_mutex_unlock(&trafficLightLock);

            if (isGreen) {
                // Move vehicle forward based on direction and lane
                if (direction == NORTH) {
                    if (lane == LANE1) { // Incoming
                        vehicle.shape.move(0, vehicle.speed);
                    } else { // Outgoing
                        vehicle.shape.move(0, vehicle.speed);
                    }
                } else if (direction == SOUTH) {
                    if (lane == LANE1) { // Incoming
                        vehicle.shape.move(0, -vehicle.speed);
                    } else { // Outgoing
                        vehicle.shape.move(0, -vehicle.speed);
                    }
                } else if (direction == EAST) {
                    if (lane == LANE1) { // Incoming
                        vehicle.shape.move(-vehicle.speed, 0);
                    } else { // Outgoing
                        vehicle.shape.move(-vehicle.speed, 0);
                    }
                } else if (direction == WEST) {
                    if (lane == LANE1) { // Incoming
                        vehicle.shape.move(vehicle.speed, 0);
                    } else { // Outgoing
                        vehicle.shape.move(vehicle.speed, 0);
                    }
                }

                // Update vehicle's speed and enforce speed limit
                if (vehicle.speed > SPEED_LIMIT && vehicle.type != EMERGENCY) {
                    issueChallan(vehicle);
                }
            }

            // Simulate breakdown handling
            if (vehicle.breakdown) {
                handleBreakdown(vehicle);
                
            }

            // Re-add the vehicle if it's still within bounds
            sf::Vector2f position = vehicle.shape.getPosition();
            bool inBounds = false;
            if (direction == NORTH && position.y < WINDOW_HEIGHT / 2 + LANE_WIDTH) {
                inBounds = true;
            } else if (direction == SOUTH && position.y > WINDOW_HEIGHT / 2 - LANE_WIDTH) {
                inBounds = true;
            } else if (direction == EAST && position.x > WINDOW_WIDTH / 2 - LANE_WIDTH) {
                inBounds = true;
            } else if (direction == WEST && position.x < WINDOW_WIDTH / 2 + LANE_WIDTH) {
                inBounds = true;
            }

            if (inBounds) {
                vehicleQueue.push(vehicle);
            } else {
                cout << "Vehicle exited! Vehicle Number: " << vehicle.vehicleNumber << endl;
                releaseResources(vehicle.vehicleNumber, 1); // Release resources upon exit
            }
        }
        pthread_mutex_unlock(&queueLocks[direction][lane]);
    }
}

// Function to draw lanes
void drawLanes() {
    // Draw horizontal lanes (East-West)
    sf::RectangleShape horizontalLane1(sf::Vector2f(WINDOW_WIDTH, LANE_WIDTH / 2));
    horizontalLane1.setFillColor(sf::Color(200, 200, 200));
    horizontalLane1.setPosition(0, (WINDOW_HEIGHT / 2) - LANE_WIDTH / 2);
    window.draw(horizontalLane1);

    sf::RectangleShape horizontalLane2(sf::Vector2f(WINDOW_WIDTH, LANE_WIDTH / 2));
    horizontalLane2.setFillColor(sf::Color(200, 200, 200));
    horizontalLane2.setPosition(0, (WINDOW_HEIGHT / 2));
    window.draw(horizontalLane2);

    // Draw vertical lanes (North-South)
    sf::RectangleShape verticalLane1(sf::Vector2f(LANE_WIDTH / 2, WINDOW_HEIGHT));
    verticalLane1.setFillColor(sf::Color(200, 200, 200));
    verticalLane1.setPosition((WINDOW_WIDTH / 2) - LANE_WIDTH / 2, 0);
    window.draw(verticalLane1);

    sf::RectangleShape verticalLane2(sf::Vector2f(LANE_WIDTH / 2, WINDOW_HEIGHT));
    verticalLane2.setFillColor(sf::Color(200, 200, 200));
    verticalLane2.setPosition((WINDOW_WIDTH / 2), 0);
    window.draw(verticalLane2);
}

// Function to draw the entire scene
void drawScene() {
    window.clear(sf::Color::White);

    // Draw lanes
    drawLanes();

    // Draw traffic lights
    for (int i = 0; i < 4; ++i) {
        window.draw(trafficLights[i].red);
        window.draw(trafficLights[i].yellow);
        window.draw(trafficLights[i].green);
    }

    // Draw vehicles
    for (int dir = 0; dir < 4; dir++) {
        for (int lane = 0; lane < 2; lane++) {
            pthread_mutex_lock(&queueLocks[dir][lane]);
            queue<Vehicle> tempQueue = trafficQueues[dir][lane];
            while (!tempQueue.empty()) {
                Vehicle vehicle = tempQueue.front();
                tempQueue.pop();
                window.draw(vehicle.shape);
            }
            pthread_mutex_unlock(&queueLocks[dir][lane]);
        }
    }

    window.display();
}

// Function to simulate vehicle arrival at intervals
void* vehicleGenerator(void* arg) {
    Direction direction = *(Direction*)arg;
    srand(time(NULL) + direction); // Seed based on direction to diversify

    while (window.isOpen()) {
        sleep(rand() % 3 + 1); // Random interval between vehicle arrivals (1-3 seconds)

        // Generate vehicles for both lanes
        generateVehicle(direction, LANE1); // Incoming
        generateVehicle(direction, LANE2); // Outgoing
        manageQueues(direction);
    }
    return nullptr;
}
void saveAnalyticsToFile(const string& filename) {
    ofstream file(filename);
    if (file.is_open()) {
        file << "Traffic Simulation Analytics\n";
        file << "-----------------------------\n";
        for (const auto& [key, value] : analytics) {
            file << key << ": " << value << endl;
        }
        file.close();
        cout << "Analytics saved to " << filename << endl;
    } else {
        cerr << "Error: Unable to open file " << filename << endl;
    }
}

// Entry point
int main() {
      srand(time(NULL)); // Seed the random number generator

    // Initialize mutexes
    for (int dir = 0; dir < 4; ++dir) {
        for (int lane = 0; lane < 2; ++lane) {
            pthread_mutex_init(&queueLocks[dir][lane], nullptr);
        }
    }
    pthread_mutex_init(&trafficLightLock, nullptr);
    pthread_mutex_init(&resourceLock, nullptr);
    pthread_mutex_init(&timeLock, nullptr);

    // Initialize traffic lights
    initializeTrafficLights();

    // Launch traffic light controller thread
    pthread_t trafficLightThread;
    pthread_create(&trafficLightThread, nullptr, trafficLightController, nullptr);

    // Launch vehicle generator threads for each direction
    pthread_t vehicleThreads[4];
    Direction directions[4] = {NORTH, SOUTH, EAST, WEST};
    for (int i = 0; i < 4; ++i) {
        pthread_create(&vehicleThreads[i], nullptr, vehicleGenerator, &directions[i]);
    }
 bool simulationRunning = true;

    while (true) {
        cout << "Choose an option:\n";
        cout << "1. View Traffic Simulation\n";
        cout << "2. Access User Portal\n";
        cout << "3. Exit\n";

        int choice;
        cin >> choice;

        switch (choice) {
            case 1:
                    while (window.isOpen() && simulationRunning) {
                    sf::Event event;
                    while (window.pollEvent(event)) {
                        if (event.type == sf::Event::Closed) {
                            window.close();
                            simulationRunning = false; // Ensure the simulation stops
                        } else if (event.type == sf::Event::KeyPressed) {
                            if (event.key.code == sf::Keyboard::Space) {
                                simulationRunning = !simulationRunning;
                                cout << "Simulation " << (simulationRunning ? "resumed" : "paused") << endl;
                            }
                        }
                    }
                    if (simulationRunning) {
                        moveVehicles(NORTH);
                        moveVehicles(SOUTH);
                        moveVehicles(EAST);
                        moveVehicles(WEST);
                         // Draw the updated scene
                        drawScene();

                         usleep(50000); // 50ms delay for smooth animation (~20 FPS)
                }

       

                }
                break;
                
            case 2:
                userPortal();
                break;
            case 3:
                saveAnalyticsToFile("analytics.txt");
                pthread_cancel(trafficLightThread);
    pthread_join(trafficLightThread, nullptr);

    for (int i = 0; i < 4; ++i) {
        pthread_cancel(vehicleThreads[i]);
        pthread_join(vehicleThreads[i], nullptr);
    }

    // Destroy mutexes
    for (int dir = 0; dir < 4; ++dir) {
        for (int lane = 0; lane < 2; ++lane) {
            pthread_mutex_destroy(&queueLocks[dir][lane]);
        }
    }
    pthread_mutex_destroy(&trafficLightLock);
    pthread_mutex_destroy(&resourceLock);
    pthread_mutex_destroy(&timeLock);

                return 0;
            default:
                cout << "Invalid choice. Please try again.\n";
        }
    }
}

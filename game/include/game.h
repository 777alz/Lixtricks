#pragma once
#include "raylib.h"
#include "raymath.h"
#include <vector>

struct Player {
    Vector3 position;
    float speed;
    float walkSpeed;
    float runSpeed;
    float slideSpeed;
    float crouchSpeed;
    float slideDuration;
    float slideTimer;
    bool isSliding;
    float sprintTimer;
    bool sprintExhausted;
    bool prevCrouching;
    float airborneSpeed;
    int jumpCount;
    bool wasOnGround;
    bool slideQueued;
    float cameraYaw;
    float cameraPitch;
    float radius;
    float velocityY;
};

struct Platform {
    Vector3 position;
    Vector3 size;
    Color colour;
};

struct Projectile {
    Vector3 position;
    Vector3 velocity;
    float radius;
    float lifetime;
};

struct Enemy {
    Vector3 position;
    Vector3 size;
    int hitCount;
    float flashTimer;
};

struct World {
    Camera3D camera;
    std::vector<Platform> platforms;
    std::vector<Projectile> projectiles;
    std::vector<Enemy> enemies;
    float enemySpawnTimer;
};

// Expose world and player
extern World world;
extern Player player;

// Game lifecycle functions
void GameInit();
void GameCleanup();
bool GameUpdate();
void GameDraw();

#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <algorithm>
#include <random>

// -----------------------------------------------------------------------------
// Global / State
// -----------------------------------------------------------------------------
Camera3D camera = { 0 };

Vector3 playerPosition;
float  playerSpeed      = 5.0f;
float  walkSpeed        = 5.0f;
float  runSpeed         = 10.0f;
float  slideSpeed       = 16.0f;
float  crouchSpeed      = 2.5f;
float  slideDuration    = 0.5f;
float  slideTimer       = 0.0f;
bool   isSliding        = false;
bool   isCrouching      = false;
float  sprintTimer      = 0.0f;
constexpr float maxSprintTime = 10.0f;
bool   sprintExhausted  = false;
bool   prevCrouching    = false;
float  airborneSpeed    = 0.0f;

int    jumpCount        = 0;
constexpr int maxJumpCount = 2;
bool   wasOnGround      = false;
bool   slideQueued      = false; // prevent holding crouch from retriggering slide

float  cameraYaw        = 0.0f;
float  cameraPitch      = 0.0f;
float  playerRadius     = 0.5f;

// Gravity / jump
float  playerVelocityY  = 0.0f;
constexpr float gravity      = 9.81f;
constexpr float jumpVelocity = 5.0f;

struct Platform {
    Vector3 position;
    Vector3 size;
    Color   colour;
};

std::vector<Platform> platforms = {
    { {0, 0, 0},     {50, 1, 50}, DARKGREEN },
    { {0, 1.5f, 10}, { 2, 2,  2}, WHITE }
};

struct Projectile {
    Vector3 position;
    Vector3 velocity;
    float   radius;
    float   lifetime;
};

std::vector<Projectile> projectiles;
constexpr float projectileSpeed    = 100.0f;
constexpr float projectileRadius   = 0.01f;
constexpr float projectileLifetime = 2.0f;

inline float GetPlatformTopY(const Platform& p) {
    return p.position.y + p.size.y * 0.5f;
}

struct Enemy {
    Vector3 position;
    Vector3 size;
    int     hitCount;
    float   flashTimer;
};

std::vector<Enemy> enemies;

constexpr int   enemyMaxHits        = 5;
constexpr float enemyFlashDuration  = 0.1f;
constexpr float enemyRadius         = 0.5f;
constexpr float enemySpawnInterval  = 2.0f;
constexpr int   enemyMaxCount       = 10;
float           enemySpawnTimer     = 0.0f;

// -----------------------------------------------------------------------------
// Utility / Collision
// -----------------------------------------------------------------------------
inline bool SphereVsAABB(const Vector3& spherePos, float r,
                         const Vector3& boxPos, const Vector3& boxSize) {
    const float hx = boxSize.x * 0.5f;
    const float hy = boxSize.y * 0.5f;
    const float hz = boxSize.z * 0.5f;

    const float minX = boxPos.x - hx;
    const float maxX = boxPos.x + hx;
    const float minY = boxPos.y - hy;
    const float maxY = boxPos.y + hy;
    const float minZ = boxPos.z - hz;
    const float maxZ = boxPos.z + hz;

    const float cx = std::clamp(spherePos.x, minX, maxX);
    const float cy = std::clamp(spherePos.y, minY, maxY);
    const float cz = std::clamp(spherePos.z, minZ, maxZ);

    const float dx = spherePos.x - cx;
    const float dy = spherePos.y - cy;
    const float dz = spherePos.z - cz;

    return (dx*dx + dy*dy + dz*dz) <= (r * r);
}

inline void GetExpandedPlatformBounds(const Platform& p,
                                      float& minX, float& maxX,
                                      float& minY, float& maxY,
                                      float& minZ, float& maxZ) {
    const float hx = p.size.x * 0.5f + playerRadius;
    const float hy = p.size.y * 0.5f + playerRadius;
    const float hz = p.size.z * 0.5f + playerRadius;
    minX = p.position.x - hx; maxX = p.position.x + hx;
    minY = p.position.y - hy; maxY = p.position.y + hy;
    minZ = p.position.z - hz; maxZ = p.position.z + hz;
}

inline bool isCollidingPlatform(const Vector3& pos) {
    for (const auto& p : platforms) {
        float minX, maxX, minY, maxY, minZ, maxZ;
        GetExpandedPlatformBounds(p, minX, maxX, minY, maxY, minZ, maxZ);
        if (pos.x > minX && pos.x < maxX &&
            pos.y > minY && pos.y < maxY &&
            pos.z > minZ && pos.z < maxZ) {
            return true;
        }
    }
    return false;
}

inline float isOnPlatform(const Vector3& pos) {
    constexpr float tolerance = 0.05f;
    for (const auto& p : platforms) {
        float minX, maxX, minY, maxY, minZ, maxZ;
        GetExpandedPlatformBounds(p, minX, maxX, minY, maxY, minZ, maxZ);
        if (pos.x > minX && pos.x < maxX &&
            pos.z > minZ && pos.z < maxZ) {
            const float topY  = GetPlatformTopY(p);
            const float feetY = pos.y - playerRadius;
            if (feetY >= topY - tolerance && feetY <= topY + tolerance) {
                return topY + playerRadius;
            }
        }
    }
    return -1.0f;
}

inline bool ProjectileHitsPlatform(const Projectile& pr, const Platform& plat) {
    return SphereVsAABB(pr.position, pr.radius, plat.position, plat.size);
}

float getRandomFloat(float min, float max) {
    thread_local static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

// -----------------------------------------------------------------------------
// Enemy spawning
// -----------------------------------------------------------------------------
void spawnEnemy() {
    if (enemies.size() >= static_cast<size_t>(enemyMaxCount)) return;

    const Platform& floor = platforms[0];
    const float minX = floor.position.x - floor.size.x * 0.5f + enemyRadius;
    const float maxX = floor.position.x + floor.size.x * 0.5f - enemyRadius;
    const float minZ = floor.position.z - floor.size.z * 0.5f + enemyRadius;
    const float maxZ = floor.position.z + floor.size.z * 0.5f - enemyRadius;
    const float y    = GetPlatformTopY(floor) + enemyRadius;

    enemies.push_back({
        { getRandomFloat(minX, maxX), y, getRandomFloat(minZ, maxZ) },
        { 1, 2, 1 },
        0,
        0.0f
    });
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void GameInit() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Lixtricks");
    SetTargetFPS(144);
    DisableCursor();

    if (!platforms.empty()) {
        const Platform& floor = platforms[0];
        playerPosition = {
            floor.position.x,
            GetPlatformTopY(floor) + playerRadius,
            floor.position.z
        };
    }

    camera.position   = playerPosition;
    camera.target     = Vector3Add(playerPosition, { 0.0f, 0.0f, 1.0f });
    camera.up         = { 0.0f, 1.0f, 0.0f };
    camera.fovy       = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const Vector3 forward = Vector3Subtract(camera.target, camera.position);
    cameraYaw   = atan2f(forward.x, forward.z);
    cameraPitch = asinf(forward.y / Vector3Length(forward));

    enemies.clear();
    projectiles.clear();
    projectiles.reserve(256);
    enemies.reserve(enemyMaxCount);

    enemySpawnTimer = 0.0f;
}

void GameCleanup() {
    CloseWindow();
}

// -----------------------------------------------------------------------------
// Frame Update
// -----------------------------------------------------------------------------
bool GameUpdate() {
    const float dt = GetFrameTime();

    // Enemy spawn timer
    enemySpawnTimer += dt;
    if (enemySpawnTimer >= enemySpawnInterval) {
        spawnEnemy();
        enemySpawnTimer = 0.0f;
    }

    // Mouse look
    const Vector2 mouseDelta = GetMouseDelta();
    constexpr float sensitivity = 0.003f;
    cameraYaw   -= mouseDelta.x * sensitivity;
    cameraPitch -= mouseDelta.y * sensitivity;

    constexpr float pitchLimit = 89.0f * DEG2RAD;
    cameraPitch = std::clamp(cameraPitch, -pitchLimit, pitchLimit);

    // Precompute yaw/pitch trig
    const float cosPitch = cosf(cameraPitch);
    const float sinPitch = sinf(cameraPitch);
    const float cosYaw   = cosf(cameraYaw);
    const float sinYaw   = sinf(cameraYaw);

    const Vector3 forward = {
        cosPitch * sinYaw,
        sinPitch,
        cosPitch * cosYaw
    };
    const Vector3 left = {
        cosYaw,
        0.0f,
        -sinYaw
    };

    Vector3 nextPos = playerPosition;

    const bool running   = IsKeyDown(KEY_LEFT_SHIFT);
    const bool crouching = IsKeyDown(KEY_C);

    // Movement speed used this frame (based on previous state's playerSpeed)
    const float cameraSpeed = playerSpeed * dt;

    // Horizontal movement with simple sliding along blocking faces
    auto tryMove = [&](const Vector3& dir, float scale) {
        const Vector3 delta    = Vector3Scale(dir, scale);
        Vector3 candidate      = nextPos;
        candidate.x += delta.x;
        candidate.z += delta.z;
        candidate.y  = nextPos.y;

        if (!isCollidingPlatform(candidate)) {
            nextPos = candidate;
            return;
        }

        // Try slide X
        Vector3 slideX = nextPos;
        slideX.x += delta.x;
        if (!isCollidingPlatform(slideX)) {
            nextPos.x = slideX.x;
        }

        // Try slide Z
        Vector3 slideZ = nextPos;
        slideZ.z += delta.z;
        if (!isCollidingPlatform(slideZ)) {
            nextPos.z = slideZ.z;
        }
    };

    if (IsKeyDown(KEY_W)) tryMove(forward,  cameraSpeed);
    if (IsKeyDown(KEY_S)) tryMove(forward, -cameraSpeed);
    if (IsKeyDown(KEY_D)) tryMove(left,    -cameraSpeed);
    if (IsKeyDown(KEY_A)) tryMove(left,     cameraSpeed);

    // Ground check
    const float platformY = isOnPlatform(nextPos);
    const bool  onGround  = platformY >= 0.0f;

    if (onGround && !wasOnGround) {
        jumpCount = 0; // landed
    }

    // Movement mode -> speed
    if (onGround) {
        if (isSliding) {
            playerSpeed = slideSpeed;
        } else if (running && !crouching && !sprintExhausted) {
            playerSpeed = runSpeed;
        } else if (crouching) {
            playerSpeed = crouchSpeed;
        } else {
            playerSpeed = walkSpeed;
        }
        airborneSpeed = playerSpeed;
    } else {
        playerSpeed = airborneSpeed;
    }

    // Ground snap only when descending
    if (onGround && playerVelocityY < 0.0f) {
        nextPos.y       = platformY;
        playerVelocityY = 0.0f;
    }

    // Jump
    if (IsKeyPressed(KEY_SPACE) && (onGround || jumpCount < maxJumpCount)) {
        playerVelocityY = jumpVelocity;
        ++jumpCount;
    }

    // Gravity
    if (!onGround)
        playerVelocityY -= gravity * dt;

    // Vertical integrate
    nextPos.y += playerVelocityY * dt;

    // Sliding start (crouch press while running)
    const bool crouchPressed = crouching && !prevCrouching;
    if (running && crouchPressed && !isSliding && onGround && !slideQueued) {
        isSliding  = true;
        slideTimer = slideDuration;
        slideQueued = true;
    }

    // Sliding update
    if (isSliding) {
        slideTimer -= dt;
        if (slideTimer <= 0.0f) {
            isSliding = false;
        }
    }
    if (!crouching) {
        slideQueued = false;
    }

    // Sprint resource
    if (running && !sprintExhausted) {
        sprintTimer += dt;
        if (sprintTimer >= maxSprintTime) {
            sprintExhausted = true;
            sprintTimer     = maxSprintTime;
        }
    } else {
        sprintTimer -= dt * 2.0f;
        if (sprintTimer < 0.0f) sprintTimer = 0.0f;
        if (sprintTimer <= 0.0f) sprintExhausted = false;
    }

    playerPosition = nextPos;

    // Fire projectile
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        projectiles.push_back({
            playerPosition,
            Vector3Scale(forward, projectileSpeed),
            projectileRadius,
            projectileLifetime
        });
    }

    // Update enemies flash timers
    for (auto& enemy : enemies) {
        if (enemy.flashTimer > 0.0f) {
            enemy.flashTimer -= dt;
            if (enemy.flashTimer < 0.0f) enemy.flashTimer = 0.0f;
        }
    }

    // Update projectiles + collision (platform + enemies) in one pass
    for (auto& proj : projectiles) {
        if (proj.lifetime <= 0.0f) continue;

        proj.position.x += proj.velocity.x * dt;
        proj.position.y += proj.velocity.y * dt;
        proj.position.z += proj.velocity.z * dt;
        proj.lifetime   -= dt;
        if (proj.lifetime <= 0.0f) continue;

        // Platform collision
        for (const auto& plat : platforms) {
            if (ProjectileHitsPlatform(proj, plat)) {
                proj.lifetime = 0.0f;
                break;
            }
        }
        if (proj.lifetime <= 0.0f) continue;

        // Enemy collision
        for (auto& enemy : enemies) {
            if (SphereVsAABB(proj.position, proj.radius, enemy.position, enemy.size)) {
                ++enemy.hitCount;
                enemy.flashTimer = enemyFlashDuration;
                proj.lifetime = 0.0f;
                break;
            }
        }
    }

    // Compact expired projectiles
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
                       [](const Projectile& p) { return p.lifetime <= 0.0f; }),
        projectiles.end()
    );

    // Remove dead enemies
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
                       [](const Enemy& e) { return e.hitCount >= enemyMaxHits; }),
        enemies.end()
    );

    // Camera
    camera.position = playerPosition;
    camera.target   = Vector3Add(playerPosition, forward);

    prevCrouching = crouching;
    wasOnGround   = onGround;
    return true;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------
void GameDraw() {
    BeginDrawing();
    ClearBackground(SKYBLUE);

    BeginMode3D(camera);

    for (const auto& p : platforms)
        DrawCube(p.position, p.size.x, p.size.y, p.size.z, p.colour);

    for (const auto& e : enemies) {
        const Color col = (e.flashTimer > 0.0f) ? RED : DARKPURPLE;
        DrawCube(e.position, e.size.x, e.size.y, e.size.z, col);
    }

    for (const auto& p : projectiles)
        DrawSphere(p.position, p.radius, YELLOW);

    EndMode3D();

    const int screenWidth  = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const int cx = screenWidth / 2;
    const int cy = screenHeight / 2;
    const int crosshairSize = 12;
    const int crosshairThickness = 2;

    DrawRectangle(cx - crosshairSize / 2,  cy - crosshairThickness / 2,
                  crosshairSize,           crosshairThickness, RAYWHITE);
    DrawRectangle(cx - crosshairThickness / 2, cy - crosshairSize / 2,
                  crosshairThickness,         crosshairSize,   RAYWHITE);

    DrawText("Lixtricks", 10, 10, 12, RAYWHITE);
    DrawFPS(10, 30);
    EndDrawing();
}
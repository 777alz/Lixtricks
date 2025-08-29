#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <algorithm>
#include <random>

// -----------------------------------------------------------------------------
// Global / State
// -----------------------------------------------------------------------------
constexpr float gravity      = 9.81f;
constexpr float jumpVelocity = 5.0f;
constexpr float maxSprintTime = 10.0f;
constexpr int maxJumpCount = 2;
constexpr float projectileSpeed    = 100.0f;
constexpr float projectileRadius   = 0.01f;
constexpr float projectileLifetime = 2.0f;
constexpr int   enemyMaxHits        = 5;
constexpr float enemyFlashDuration  = 0.1f;
constexpr float enemyRadius         = 0.5f;
constexpr float enemySpawnInterval  = 2.0f;
constexpr int   enemyMaxCount       = 10;

World world = {
    {}, // camera
    { // platforms
        { {0, 0, 0},     {50, 1, 50}, DARKGREEN },
        { {0, 1.5f, 10}, { 2, 2,  2}, WHITE }
    },
    {}, // projectiles
    {}, // enemies
    0.0f // enemySpawnTimer
};

Player player = {
    {}, // position
    5.0f, // speed
    5.0f, // walkSpeed
    10.0f, // runSpeed
    16.0f, // slideSpeed
    2.5f, // crouchSpeed
    0.5f, // slideDuration
    0.0f, // slideTimer
    false, // isSliding
    0.0f, // sprintTimer
    false, // sprintExhausted
    false, // prevCrouching
    0.0f, // airborneSpeed
    0, // jumpCount
    false, // wasOnGround
    false, // slideQueued
    0.0f, // cameraYaw
    0.0f, // cameraPitch
    0.5f, // radius
    0.0f // velocityY
};

// -----------------------------------------------------------------------------
// Utility / Collision
// -----------------------------------------------------------------------------
inline float GetPlatformTopY(const Platform& p) {
    return p.position.y + p.size.y * 0.5f;
}

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
    const float hx = p.size.x * 0.5f + player.radius;
    const float hy = p.size.y * 0.5f + player.radius;
    const float hz = p.size.z * 0.5f + player.radius;
    minX = p.position.x - hx; maxX = p.position.x + hx;
    minY = p.position.y - hy; maxY = p.position.y + hy;
    minZ = p.position.z - hz; maxZ = p.position.z + hz;
}

inline bool isCollidingPlatform(const Vector3& pos) {
    for (const auto& p : world.platforms) {
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
    for (const auto& p : world.platforms) {
        float minX, maxX, minY, maxY, minZ, maxZ;
        GetExpandedPlatformBounds(p, minX, maxX, minY, maxY, minZ, maxZ);
        if (pos.x > minX && pos.x < maxX &&
            pos.z > minZ && pos.z < maxZ) {
            const float topY  = GetPlatformTopY(p);
            const float feetY = pos.y - player.radius;
            if (feetY >= topY - tolerance && feetY <= topY + tolerance) {
                return topY + player.radius;
            }
        }
    }
    return -1.0f;
}

inline bool ProjectileHitsPlatform(const Projectile& pr, const Platform& plat) {
    return SphereVsAABB(pr.position, pr.radius, plat.position, plat.size);
}

inline bool SweptSphereVsAABB(const Vector3& start, const Vector3& end, float radius, const Vector3& boxPos, const Vector3& boxSize)
{
    // Expand AABB by sphere radius
    const float hx = boxSize.x * 0.5f + radius;
    const float hy = boxSize.y * 0.5f + radius;
    const float hz = boxSize.z * 0.5f + radius;
    const Vector3 min = { boxPos.x - hx, boxPos.y - hy, boxPos.z - hz };
    const Vector3 max = { boxPos.x + hx, boxPos.y + hy, boxPos.z + hz };

    // Ray vs AABB (slab method)
    Vector3 dir = { end.x - start.x, end.y - start.y, end.z - start.z };
    float tmin = 0.0f, tmax = 1.0f;
    for (int i = 0; i < 3; ++i) {
        float s = (&start.x)[i], d = (&dir.x)[i], mn = (&min.x)[i], mx = (&max.x)[i];
        if (fabsf(d) < 1e-8f) {
            if (s < mn || s > mx) return false;
        }
        else {
            float ood = 1.0f / d;
            float t1 = (mn - s) * ood;
            float t2 = (mx - s) * ood;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    return true;
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
    if (world.enemies.size() >= static_cast<size_t>(enemyMaxCount)) return;

    const Platform& floor = world.platforms[0];
    const float minX = floor.position.x - floor.size.x * 0.5f + enemyRadius;
    const float maxX = floor.position.x + floor.size.x * 0.5f - enemyRadius;
    const float minZ = floor.position.z - floor.size.z * 0.5f + enemyRadius;
    const float maxZ = floor.position.z + floor.size.z * 0.5f - enemyRadius;
    const float y    = GetPlatformTopY(floor) + enemyRadius;

    world.enemies.push_back({
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

    if (!world.platforms.empty()) {
        const Platform& floor = world.platforms[0];
        player.position = {
            floor.position.x,
            GetPlatformTopY(floor) + player.radius,
            floor.position.z
        };
    }

    world.camera.position   = player.position;
    world.camera.target     = Vector3Add(player.position, { 0.0f, 0.0f, 1.0f });
    world.camera.up         = { 0.0f, 1.0f, 0.0f };
    world.camera.fovy       = 60.0f;
    world.camera.projection = CAMERA_PERSPECTIVE;

    const Vector3 forward = Vector3Subtract(world.camera.target, world.camera.position);
    player.cameraYaw   = atan2f(forward.x, forward.z);
    player.cameraPitch = asinf(forward.y / Vector3Length(forward));

    world.enemies.clear();
    world.projectiles.clear();
    world.projectiles.reserve(256);
    world.enemies.reserve(enemyMaxCount);

    world.enemySpawnTimer = 0.0f;
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
    world.enemySpawnTimer += dt;
    if (world.enemySpawnTimer >= enemySpawnInterval) {
        spawnEnemy();
        world.enemySpawnTimer = 0.0f;
    }

    // Mouse look
    const Vector2 mouseDelta = GetMouseDelta();
    constexpr float sensitivity = 0.003f;
    player.cameraYaw   -= mouseDelta.x * sensitivity;
    player.cameraPitch -= mouseDelta.y * sensitivity;

    constexpr float pitchLimit = 89.0f * DEG2RAD;
    player.cameraPitch = std::clamp(player.cameraPitch, -pitchLimit, pitchLimit);

    // Precompute yaw/pitch trig
    const float cosPitch = cosf(player.cameraPitch);
    const float sinPitch = sinf(player.cameraPitch);
    const float cosYaw   = cosf(player.cameraYaw);
    const float sinYaw   = sinf(player.cameraYaw);

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

    Vector3 nextPos = player.position;

    const bool running   = IsKeyDown(KEY_LEFT_SHIFT);
    const bool crouching = IsKeyDown(KEY_C);

    const float cameraSpeed = player.speed * dt;

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

        Vector3 slideX = nextPos;
        slideX.x += delta.x;
        if (!isCollidingPlatform(slideX)) {
            nextPos.x = slideX.x;
        }

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

    if (onGround && !player.wasOnGround) {
        player.jumpCount = 0; // landed
    }

    // Movement mode -> speed
    if (onGround) {
        if (player.isSliding) {
            player.speed = player.slideSpeed;
        } else if (running && !crouching && !player.sprintExhausted) {
            player.speed = player.runSpeed;
        } else if (crouching) {
            player.speed = player.crouchSpeed;
        } else {
            player.speed = player.walkSpeed;
        }
        player.airborneSpeed = player.speed;
    } else {
        player.speed = player.airborneSpeed;
    }

    // Ground snap only when descending
    if (onGround && player.velocityY < 0.0f) {
        nextPos.y       = platformY;
        player.velocityY = 0.0f;
    }

    // Jump
    if (IsKeyPressed(KEY_SPACE) && (onGround || player.jumpCount < maxJumpCount)) {
        player.velocityY = jumpVelocity;
        ++player.jumpCount;
    }

    // Gravity
    if (!onGround)
        player.velocityY -= gravity * dt;

    // Vertical integrate
    nextPos.y += player.velocityY * dt;

    // Sliding start (crouch press while running)
    const bool crouchPressed = crouching && !player.prevCrouching;
    if (running && crouchPressed && !player.isSliding && onGround && !player.slideQueued) {
        player.isSliding  = true;
        player.slideTimer = player.slideDuration;
        player.slideQueued = true;
    }

    // Sliding update
    if (player.isSliding) {
        player.slideTimer -= dt;
        if (player.slideTimer <= 0.0f) {
            player.isSliding = false;
        }
    }
    if (!crouching) {
        player.slideQueued = false;
    }

    // Sprint resource
    if (running && !player.sprintExhausted) {
        player.sprintTimer += dt;
        if (player.sprintTimer >= maxSprintTime) {
            player.sprintExhausted = true;
            player.sprintTimer     = maxSprintTime;
        }
    } else {
        player.sprintTimer -= dt * 2.0f;
        if (player.sprintTimer < 0.0f) player.sprintTimer = 0.0f;
        if (player.sprintTimer <= 0.0f) player.sprintExhausted = false;
    }

    player.position = nextPos;

    // Fire projectile
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        constexpr float spawnOffset = 0.6f;
        Vector3 spawnPos = {
            player.position.x + forward.x * spawnOffset,
            player.position.y + forward.y * spawnOffset,
            player.position.z + forward.z * spawnOffset
        };
        world.projectiles.push_back({
            spawnPos,
            Vector3Scale(forward, projectileSpeed),
            projectileRadius,
            projectileLifetime
        });
    }

    // Update enemy flash timers
    for (auto& enemy : world.enemies) {
        enemy.flashTimer -= dt;
        if (enemy.flashTimer < 0.0f) enemy.flashTimer = 0.0f;
    }

    // Update projectiles + collision (platform + enemies)
    for (auto& proj : world.projectiles) {
        if (proj.lifetime <= 0.0f) continue;

        Vector3 prevPos = proj.position;
        proj.position.x += proj.velocity.x * dt;
        proj.position.y += proj.velocity.y * dt;
        proj.position.z += proj.velocity.z * dt;
        proj.lifetime   -= dt;
        if (proj.lifetime <= 0.0f) continue;

        for (const auto& plat : world.platforms) {
            if (SweptSphereVsAABB(prevPos, proj.position, proj.radius, plat.position, plat.size)) {
                proj.lifetime = 0.0f;
                break;
            }
        }
        if (proj.lifetime <= 0.0f) continue;

        for (auto& enemy : world.enemies) {
            if (SweptSphereVsAABB(prevPos, proj.position, proj.radius, enemy.position, enemy.size)) {
                ++enemy.hitCount;
                enemy.flashTimer = enemyFlashDuration;
                proj.lifetime = 0.0f;
                break;
            }
        }
    }

    // Compact expired projectiles
    world.projectiles.erase(
        std::remove_if(world.projectiles.begin(), world.projectiles.end(),
                       [](const Projectile& p) { return p.lifetime <= 0.0f; }),
        world.projectiles.end()
    );

    // Remove dead enemies
    world.enemies.erase(
        std::remove_if(world.enemies.begin(), world.enemies.end(),
                       [](const Enemy& e) { return e.hitCount >= enemyMaxHits; }),
        world.enemies.end()
    );

    // Camera
    world.camera.position = player.position;
    world.camera.target   = Vector3Add(player.position, forward);

    player.prevCrouching = crouching;
    player.wasOnGround   = onGround;
    return true;
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------
void GameDraw() {
    BeginDrawing();
    ClearBackground(SKYBLUE);

    BeginMode3D(world.camera);

    for (const auto& p : world.platforms)
        DrawCube(p.position, p.size.x, p.size.y, p.size.z, p.colour);

    for (const auto& e : world.enemies) {
        const Color col = (e.flashTimer > 0.0f) ? RED : DARKPURPLE;
        DrawCube(e.position, e.size.x, e.size.y, e.size.z, col);
    }

    for (const auto& p : world.projectiles)
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
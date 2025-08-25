#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <algorithm>

//camera
Camera3D camera = { 0 };
Vector3 playerPosition;
float playerSpeed = 5.0f;
float cameraYaw = 0.0f;
float cameraPitch = 0.0f;
float playerRadius = 0.5f;

//gravity and jump
float playerVelocityY = 0.0f;
constexpr float gravity = 9.81f;
constexpr float jumpVelocity = 8.0f;

struct Platform {
    Vector3 position;
    Vector3 size;
    Color colour;
};

std::vector<Platform> platforms = {
    {Vector3{0, 0, 0}, Vector3{50, 1, 50}, WHITE},
    {Vector3{0, 1.5f, 10}, Vector3{2, 2, 2}, GREEN},
};

struct Projectile {
    Vector3 position;
    Vector3 velocity;
    float radius;
    float lifetime;
};

std::vector<Projectile> projectiles;
constexpr float projectileSpeed = 100.0f;
constexpr float projectileRadius = 0.01f;
constexpr float projectileLifetime = 2.0f;

auto getPlatformTopY = [](const Platform& p) {
    return p.position.y + p.size.y / 2;
	};

struct Enemy {
    Vector3 position;
    Vector3 size;
    int hitCount;
    float flashTimer; // seconds left to flash red
};

std::vector<Enemy> enemies;

constexpr int enemyMaxHits = 5;
constexpr float enemyFlashDuration = 0.1f;
constexpr float enemyRadius = 0.5f;
constexpr float enemySpawnInterval = 2.0f;
constexpr int enemyMaxCount = 10;
float enemySpawnTimer = 0.0f;

//collision for projectile/enemy
inline bool isProjectileCollidingAABB(const Projectile& proj, const Vector3& boxPos, const Vector3& boxSize) {
    float minX = boxPos.x - boxSize.x / 2;
    float maxX = boxPos.x + boxSize.x / 2;
    float minY = boxPos.y - boxSize.y / 2;
    float maxY = boxPos.y + boxSize.y / 2;
    float minZ = boxPos.z - boxSize.z / 2;
    float maxZ = boxPos.z + boxSize.z / 2;

    float closestX = std::clamp(proj.position.x, minX, maxX);
    float closestY = std::clamp(proj.position.y, minY, maxY);
    float closestZ = std::clamp(proj.position.z, minZ, maxZ);

    float distSq = (proj.position.x - closestX) * (proj.position.x - closestX)
        + (proj.position.y - closestY) * (proj.position.y - closestY)
        + (proj.position.z - closestZ) * (proj.position.z - closestZ);

    return distSq <= (proj.radius * proj.radius);
}

inline void getPlatformBounds(const Platform& p, float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) {
    minX = p.position.x - p.size.x / 2 - playerRadius;
    maxX = p.position.x + p.size.x / 2 + playerRadius;
    minY = p.position.y - p.size.y / 2 - playerRadius;
    maxY = p.position.y + p.size.y / 2 + playerRadius;
    minZ = p.position.z - p.size.z / 2 - playerRadius;
    maxZ = p.position.z + p.size.z / 2 + playerRadius;
}

// is given position inside any platform bounds?
inline bool isCollidingPlatform(const Vector3& pos) {
    for (const auto& p : platforms) {
        float minX, maxX, minY, maxY, minZ, maxZ;
        getPlatformBounds(p, minX, maxX, minY, maxY, minZ, maxZ);
        if (pos.x > minX && pos.x < maxX &&
            pos.y > minY && pos.y < maxY &&
            pos.z > minZ && pos.z < maxZ)
            return true;
    }
    return false;
}

// is given position on top of any platform?
inline float isOnPlatform(const Vector3& pos) {
    for (const auto& p : platforms) {
        float topY = getPlatformTopY(p);
        float minX, maxX, minY, maxY, minZ, maxZ;
        getPlatformBounds(p, minX, maxX, minY, maxY, minZ, maxZ);

        if (pos.x > minX && pos.x < maxX &&
            pos.z > minZ && pos.z < maxZ)
        {
            constexpr float toleranceAbove = 0.2f;
            constexpr float toleranceBelow = 0.05f;
            float feetY = pos.y - playerRadius;
            if (feetY >= topY - toleranceBelow && feetY <= topY + toleranceAbove) {
                return topY + playerRadius;
            }
        }
    }
    return -1.0f;
}

inline bool isProjectileCollidingPlatform(const Projectile& proj, const Platform& plat) {
    float minX, maxX, minY, maxY, minZ, maxZ;
    minX = plat.position.x - plat.size.x / 2;
    maxX = plat.position.x + plat.size.x / 2;
    minY = plat.position.y - plat.size.y / 2;
    maxY = plat.position.y + plat.size.y / 2;
    minZ = plat.position.z - plat.size.z / 2;
    maxZ = plat.position.z + plat.size.z / 2;

    float closestX = std::clamp(proj.position.x, minX, maxX);
    float closestY = std::clamp(proj.position.y, minY, maxY);
    float closestZ = std::clamp(proj.position.z, minZ, maxZ);

    float distSq = (proj.position.x - closestX) * (proj.position.x - closestX)
        + (proj.position.y - closestY) * (proj.position.y - closestY)
        + (proj.position.z - closestZ) * (proj.position.z - closestZ);

    return distSq <= (proj.radius * proj.radius);
}

void spawnEnemy()
{
	if (enemies.size() >= enemyMaxCount) return;

    //spawn at a random position on floor
    const Platform& floor = platforms[0];
    float minX = floor.position.x - floor.size.x / 2 + enemyRadius;
    float maxX = floor.position.x + floor.size.x / 2 - enemyRadius;
    float minZ = floor.position.z - floor.size.z / 2 + enemyRadius;
    float maxZ = floor.position.z + floor.size.z / 2 - enemyRadius;
    float y = getPlatformTopY(floor) + enemyRadius;

    float x = minX + static_cast<float>(rand()) / RAND_MAX * (maxX - minX);
    float z = minZ + static_cast<float>(rand()) / RAND_MAX * (maxZ - minZ);

    enemies.push_back({ Vector3{ x, y, z }, Vector3{ 1, 2, 1 }, 0, 0.0f });
}

void GameInit()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Lixtricks");
    SetTargetFPS(144);

	DisableCursor();

    if (!platforms.empty()) {
        //spawn on floor (first platform)
        const Platform& floor = platforms[0];
        float floorTopY = getPlatformTopY(floor);
        playerPosition = {
            floor.position.x,
            floorTopY + playerRadius,
            floor.position.z
        };
    }

    camera.position = playerPosition;
    camera.target = Vector3Add(playerPosition, Vector3{ 0.0f, 0.0f, 1.0f });
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    cameraYaw = atan2f(forward.x, forward.z);
    cameraPitch = asinf(forward.y / Vector3Length(forward));

    enemies.clear();
    enemySpawnTimer = 0.0f;
}

void GameCleanup()
{
    CloseWindow();
}

bool GameUpdate()
{
    float dt = GetFrameTime();
	float cameraSpeed = playerSpeed * dt;

	//spawn enemies over time
	enemySpawnTimer += dt;
	if (enemySpawnTimer >= enemySpawnInterval) {
		spawnEnemy();
        enemySpawnTimer = 0.0f;
	}

    //mouse look
    Vector2 mouseDelta = GetMouseDelta();
    constexpr float sensitivity = 0.003f;

    cameraYaw -= mouseDelta.x * sensitivity;
    cameraPitch -= mouseDelta.y * sensitivity;

    constexpr float pitchLimit = 89.0f * DEG2RAD;
    cameraPitch = std::clamp(cameraPitch, -pitchLimit, pitchLimit);

    //calculate directions
    float cosPitch = cosf(cameraPitch);
    float sinPitch = sinf(cameraPitch);
    float cosYaw = cosf(cameraYaw);
    float sinYaw = sinf(cameraYaw);

    Vector3 forward = {
        cosPitch * sinYaw,
        sinPitch,
        cosPitch * cosYaw
    };
    forward = Vector3Normalize(forward);

    Vector3 left = {
        cosYaw,
        0.0f,
        -sinYaw
    };
    left = Vector3Normalize(left);

	Vector3 nextPos = playerPosition;

    //horizontal movement
    auto tryMove = [&](const Vector3& dir, float scale) {
        Vector3 candidate = Vector3Add(nextPos, Vector3Scale(dir, scale));
        candidate.y = nextPos.y;
        if (!isCollidingPlatform(candidate)) nextPos = candidate;
        };

    if (IsKeyDown(KEY_W)) tryMove(forward, cameraSpeed);
    if (IsKeyDown(KEY_S)) tryMove(forward, -cameraSpeed);
    if (IsKeyDown(KEY_D)) tryMove(left, -cameraSpeed);
    if (IsKeyDown(KEY_A)) tryMove(left, cameraSpeed);

    //gravity and jump
    float platformY = isOnPlatform(nextPos);
    bool onGround = platformY >= 0.0f;
    if (onGround) {
        //only snap and reset velocity if falling
        if (playerVelocityY < 0.0f) {
            nextPos.y = platformY;
            playerVelocityY = 0.0f;
        }
        //allow jump if not already moving up
        if (IsKeyPressed(KEY_SPACE) && playerVelocityY == 0.0f) {
            playerVelocityY = jumpVelocity;
        }
    }
    else {
        playerVelocityY -= gravity * dt;
    }

	//apply vertical velocity
	nextPos.y += playerVelocityY * dt;

    //prevent falling below lowest platform
    float lowestY = getPlatformTopY(platforms[0]);
    if (nextPos.y < lowestY) {
        nextPos.y = lowestY;
        playerVelocityY = 0.0f;
    }

    //prevent going through platforms vertically
    if (playerVelocityY < 0.0f && isCollidingPlatform(nextPos)) {
        nextPos.y = playerPosition.y;
        playerVelocityY = 0.0f;
    }

    playerPosition = nextPos;

	//shoot projectiles
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Projectile proj;
        proj.position = playerPosition;
        proj.velocity = Vector3Scale(forward, projectileSpeed);
        proj.radius = projectileRadius;
        proj.lifetime = projectileLifetime;
        projectiles.push_back(proj);
	}

	//update projectiles
    for (auto& proj : projectiles) {
        proj.position = Vector3Add(proj.position, Vector3Scale(proj.velocity, dt));
        proj.lifetime -= dt;
	}

    //enemy hit detection
    for (auto& enemy : enemies) {
        if (enemy.flashTimer > 0.0f)
            enemy.flashTimer -= dt;
    }

    for (auto& proj : projectiles) {
        for (auto& enemy : enemies) {
            if (isProjectileCollidingAABB(proj, enemy.position, enemy.size)) {
                enemy.hitCount++;
                enemy.flashTimer = enemyFlashDuration;
                proj.lifetime = 0.0f;
                break;
            }
        }
    }

	//remove expired projectiles
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& p) {
                if (p.lifetime <= 0.0f) return true;
                for (const auto& plat : platforms) {
                    if (isProjectileCollidingPlatform(p, plat)) return true;
                }
                return false;
            }),
        projectiles.end()
    );

    //remove dead enemies
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
            [](const Enemy& e) { return e.hitCount >= enemyMaxHits; }),
        enemies.end()
    );

    camera.position = playerPosition;
    camera.target = Vector3Add(playerPosition, forward);

    return true;
}

void GameDraw()
{
    BeginDrawing();
    ClearBackground(DARKGRAY);

    BeginMode3D(camera);

    //draw platforms
    for (const auto& p : platforms) {
        DrawCube(p.position, p.size.x, p.size.y, p.size.z, p.colour);
    }

	//draw enemies
    for (const auto& e : enemies) {
        Color col = (e.flashTimer > 0.0f) ? RED : BLUE;
        DrawCube(e.position, e.size.x, e.size.y, e.size.z, col);
	}

	//draw projectiles
    for (const auto& p : projectiles) {
        DrawSphere(p.position, p.radius, YELLOW);
	}

    EndMode3D();

    //draw crosshair at centre of screen
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    int cx = screenWidth / 2;
    int cy = screenHeight / 2;
    int crosshairSize = 12;
    int crosshairThickness = 2;
    //horizontal line
    DrawRectangle(cx - crosshairSize / 2, cy - crosshairThickness / 2, crosshairSize, crosshairThickness, RAYWHITE);
    //vertical line
    DrawRectangle(cx - crosshairThickness / 2, cy - crosshairSize / 2, crosshairThickness, crosshairSize, RAYWHITE);

    DrawText("Lixtricks", 10, 10, 12, RAYWHITE);
	DrawFPS(10, 30);
    

    EndDrawing();
}
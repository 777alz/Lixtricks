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
const float gravity = 9.81f;
const float jumpVelocity = 8.0f;

//platform struct
struct Platform {
    Vector3 position;
    Vector3 size;
    Color colour;
};

//list of platforms
std::vector<Platform> platforms = {
    {Vector3{0, 0, 0}, Vector3{50, 1, 50}, WHITE},
    {Vector3{0, 1.5f, 10}, Vector3{2, 2, 2}, GREEN},
};

auto getPlatformTopY = [](const Platform& p) {
    return p.position.y + p.size.y / 2;
	};

auto getPlatformBounds = [&](const Platform& p) {
    float minX = p.position.x - p.size.x / 2 - playerRadius;
    float maxX = p.position.x + p.size.x / 2 + playerRadius;
    float minY = p.position.y - p.size.y / 2 - playerRadius;
    float maxY = p.position.y + p.size.y / 2 + playerRadius;
    float minZ = p.position.z - p.size.z / 2 - playerRadius;
    float maxZ = p.position.z + p.size.z / 2 + playerRadius;
    return std::make_tuple(minX, maxX, minY, maxY, minZ, maxZ);
    };

// is given position inside any platform bounds?
auto isCollidingPlatform = [&](Vector3 pos) {
    for (const auto& p : platforms) {
        auto [minX, maxX, minY, maxY, minZ, maxZ] = getPlatformBounds(p);
        if (pos.x > minX && pos.x < maxX &&
            pos.y > minY && pos.y < maxY &&
            pos.z > minZ && pos.z < maxZ)
            return true;
    }
    return false;
    };

// is given position on top of any platform?
auto isOnPlatform = [&](Vector3 pos) {
    for (const auto& p : platforms) {
        float topY = getPlatformTopY(p);
        auto [minX, maxX, minY, maxY, minZ, maxZ] = getPlatformBounds(p);

        if (pos.x > minX && pos.x < maxX &&
            pos.z > minZ && pos.z < maxZ)
        {
            float toleranceAbove = 0.2f;
            float toleranceBelow = 0.05f;
            float feetY = pos.y - playerRadius;
            if (feetY >= topY - toleranceBelow && feetY <= topY + toleranceAbove) {
                return topY + playerRadius;
            }
        }
    }
    return -1.0f;
    };

void GameInit()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Lixtricks");
    SetTargetFPS(144);

	DisableCursor();

    if (!platforms.empty()) {
        // Spawn at the center of the floor (first platform)
        const Platform& floor = platforms[0];
        float floorTopY = getPlatformTopY(floor);
        playerPosition = {
            floor.position.x,           // center X of the floor
            floorTopY + playerRadius,   // just above the floor
            floor.position.z            // center Z of the floor
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
}

void GameCleanup()
{
    CloseWindow();
}

bool GameUpdate()
{
    float dt = GetFrameTime();
	float cameraSpeed = playerSpeed * dt;

    //mouse look
    Vector2 mouseDelta = GetMouseDelta();
    float sensitivity = 0.003f;

    cameraYaw -= mouseDelta.x * sensitivity;
    cameraPitch -= mouseDelta.y * sensitivity;

    float pitchLimit = 89.0f * DEG2RAD;
    cameraPitch = std::clamp(cameraPitch, -pitchLimit, pitchLimit);

    //calculate directions
    Vector3 forward = {
        cosf(cameraPitch) * sinf(cameraYaw),
        sinf(cameraPitch),
        cosf(cameraPitch) * cosf(cameraYaw)
    };
    forward = Vector3Normalize(forward);

    Vector3 left = {
        cosf(cameraYaw),
        0.0f,
        -sinf(cameraYaw)
    };
    left = Vector3Normalize(left);

    //movement with collision check
    Vector3 nextPos = playerPosition;

    //horizontal movement (ignore y for floor collision)
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

    EndMode3D();

    DrawText("Lixtricks", 10, 10, 12, RAYWHITE);
	DrawFPS(10, 30);

    EndDrawing();
}
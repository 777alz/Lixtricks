#include "game.h"
#include "raylib.h"
#include "raymath.h"

Camera3D camera = { 0 };
Vector3 playerPosition = { 0.0f, 1.8f, 0.0f };
float playerSpeed = 5.0f;
float cameraYaw = 0.0f;
float cameraPitch = 0.0f;

void GameInit()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Lixtricks");
    SetTargetFPS(144);

	DisableCursor();

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
	float cameraSpeed = playerSpeed * GetFrameTime();

    //mouse look
    Vector2 mouseDelta = GetMouseDelta();
    float sensitivity = 0.003f; // Adjust as needed

    cameraYaw -= mouseDelta.x * sensitivity;
    cameraPitch -= mouseDelta.y * sensitivity;

    //clamp pitch to avoid flipping
    float pitchLimit = 89.0f * DEG2RAD;
    if (cameraPitch > pitchLimit) cameraPitch = pitchLimit;
    if (cameraPitch < -pitchLimit) cameraPitch = -pitchLimit;

    //calculate forward direction
    Vector3 forward;
    forward.x = cosf(cameraPitch) * sinf(cameraYaw);
    forward.y = sinf(cameraPitch);
    forward.z = cosf(cameraPitch) * cosf(cameraYaw);
	forward = Vector3Normalize(forward);

    //calculate left direction
	Vector3 left;
    left.x = cosf(cameraYaw);
    left.y = 0.0f;
    left.z = -sinf(cameraYaw);
    left = Vector3Normalize(left);


    //player Movement
    if (IsKeyDown(KEY_W)) playerPosition = Vector3Add(playerPosition, Vector3Scale(forward, cameraSpeed));
    if (IsKeyDown(KEY_S)) playerPosition = Vector3Add(playerPosition, Vector3Scale(forward, -cameraSpeed));
    if (IsKeyDown(KEY_D)) playerPosition = Vector3Add(playerPosition, Vector3Scale(left, -cameraSpeed));
    if (IsKeyDown(KEY_A)) playerPosition = Vector3Add(playerPosition, Vector3Scale(left, cameraSpeed));
    if (IsKeyDown(KEY_SPACE)) playerPosition.y += cameraSpeed;
    if (IsKeyDown(KEY_LEFT_CONTROL)) playerPosition.y -= cameraSpeed;

    //calculate new camera direction
    Vector3 lookDir;
	lookDir.x = cosf(cameraPitch) * sinf(cameraYaw);
	lookDir.y = sinf(cameraPitch);
	lookDir.z = cosf(cameraPitch) * cosf(cameraYaw);

    camera.position = playerPosition;
    camera.target = Vector3Add(playerPosition, lookDir);

    return true;
}

void GameDraw()
{
    BeginDrawing();
    ClearBackground(DARKGRAY);

    BeginMode3D(camera);
    DrawCube(Vector3{ 0, 0, 10 }, 2, 2, 2, GREEN); // Example object
    EndMode3D();

    DrawText("Lixtricks", 10, 10, 12, RAYWHITE);
	DrawFPS(10, 30);

    EndDrawing();
}
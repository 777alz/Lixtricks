#pragma once
#include "raylib.h"
#include "raymath.h"

// Expose camera and player position if needed elsewhere
extern Camera3D camera;
extern Vector3 playerPosition;
extern float playerSpeed;

// Game lifecycle functions
void GameInit();
void GameCleanup();
bool GameUpdate();
void GameDraw();

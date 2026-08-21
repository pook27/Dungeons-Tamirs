#ifndef WORLD_H
#define WORLD_H

#include "game_types.h"

extern Room roomGrid[ROOM_GRID_ROWS][ROOM_GRID_COLS];
extern int currentRoomRow, currentRoomCol;
extern int bossRoomRow, bossRoomCol; // recomputed by GenerateFloorShape every floor
extern int dungeonDepth;             // floors cleared so far this run - drives difficulty scaling

void InitRooms(void);       // (re)generates the floor shape and every room in it - called on reset and each floor descent
void LoadRoom(GameAssets *assets); // spawns the current room's enemies/pot the first time it's entered
void DrawRoom(Room *room, GameAssets *assets, int roomRow, int roomCol);
void DrawMinimap(void);

int TryChangeRoom(Sprite *player); // returns 1 (and updates current room) if the player walked through a doorway
void UpdateDoors(Room *room);      // opens the room's doors once its enemies are dead
void ResolveWallCollision(Sprite *s, Room *room);
int AllRoomsCleared(void);
int TileTypeAt(Room *room, float worldX, float worldY);

#endif // WORLD_H

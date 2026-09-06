# Boat System

UE5 C++ 기반 2인 협동 Boat Gameplay System입니다.

## Core Features

- 2-Player Cooperative Boarding
- Client Prediction / Server Validation
- Server-Authoritative Paddle Physics
- Custom Network Smoothing
- River Flow / Spring-Damper Buoyancy
- Checkpoint / Respawn
- Hazard / Death Handling

## Architecture

Player Boarding  
→ Server Seat Validation  
→ Both Players Seated  
→ Physics Simulation  
→ Server Paddle Physics  
→ Boat Net State  
→ Client Interpolation / Extrapolation

## Main Source Files

- `Boat.h / Boat.cpp`
- `BoatPlayerController.h / BoatPlayerController.cpp`
- `BoatSavePoint.h / BoatSavePoint.cpp`
- `RiverFlowVolume.h / RiverFlowVolume.cpp`
- `BoatTurretBullet.h / BoatTurretBullet.cpp`

#pragma once

#include "World/Camera.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace H3D {

// windows.all_workspaces: every workspace of the monitor hangs on the inside
// of one rectangular room, each as a monitor-shaped frame, and the player
// stands in the middle of it. Pure geometry, no Hyprland dependencies --
// main.cpp hands in the workspace list and the frame size and seats windows
// on what comes back.
//
// Room coordinates: the floor is y = 0, the room is centred on the origin,
// and the walls are numbered clockwise as seen from above starting with the
// one the spawn faces at yaw 0:
//
//   0 front  z = -halfZ    1 right  x = +halfX
//   2 back   z = +halfZ    3 left   x = -halfX
//
// Frames fill the walls in that order, each wall left to right as seen from
// inside, so walking the room clockwise reads the workspaces in id order.
namespace WallLayout {

// A frame's border around the workspace area.
constexpr float kBezel = 0.12f;

// How far a frame's face stands off its wall. Window seats are measured from
// the face, so everything mounted on it clears the wall by at least this.
constexpr float kFrameDepth = 0.06f;

// Between neighbouring frames on a wall, and between a frame and the corner.
constexpr float kGap = 1.0f;

// Floor to the bottom edge of a frame's bezel.
constexpr float kSill = 0.55f;

// Above the frame's top edge: room for the workspace label, and air.
constexpr float kHeadroom = 0.9f;

// No room is smaller than this along either axis, so a wall with no frame
// on it still leaves somewhere to stand back from the ones that have.
constexpr float kMinSpan = 6.0f;

constexpr float kMinHeight = 3.2f;

struct SWorkspace {
    int64_t     id = 0;
    std::string name;
};

struct SFrame {
    int64_t     id = 0;   // workspace id
    std::string name;
    int         wall = 0; // 0..3, see above

    // Centre of the workspace area, ON the frame face.
    Vec3 center{};
    Vec3 normal{}; // into the room
    Vec3 right{};  // in-plane +x: the monitor's left-to-right

    // The workspace area: the monitor's logical size times the wall scale.
    float width  = 0.f;
    float height = 0.f;

    // Window orientation that faces into the room, in the entity convention
    // (normal = (sin yaw, 0, cos yaw)).
    float yaw = 0.f;
};

struct SLayout {
    // Inside faces of the walls, and the ceiling.
    float halfX  = 0.f;
    float halfZ  = 0.f;
    float height = 0.f;

    std::array<int, 4>  perWall{};
    std::vector<SFrame> frames;

    bool empty() const {
        return frames.empty();
    }

    const SFrame* find(int64_t id) const;
};

// One frame per workspace, in the order given. `frameWidth`/`frameHeight`
// are the workspace area in world units.
SLayout compute(
    const std::vector<SWorkspace>& workspaces,
    float                          frameWidth,
    float                          frameHeight
);

// Inward normal of wall `wall` (0..3).
Vec3 wallNormal(int wall);

// Where a point of the monitor lands on `frame`, `lift` off its face. The
// frame is the monitor scaled, so (x, y) in monitor-local logical px of a
// `monW` x `monH` monitor maps straight onto its right and up axes.
Vec3 seat(const SFrame& frame, float x, float y, float monW, float monH,
          float lift);

} // namespace WallLayout

} // namespace H3D

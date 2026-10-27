#pragma once

#include "World/WallLayout.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace H3D {

// world.house: a room built around the all-workspaces frames -- wood floor,
// plastered walls with trim, a ceiling with lights, every frame a dark screen
// in a bezel with the workspace's name above it, and windows cut into
// whatever wall is left over, looking out at the panorama. Pure geometry
// like WallLayout: GLScene draws the mesh, main.cpp turns the solids into
// Jolt bodies.
namespace House {

// What the house shader paints, per vertex. Patterns are procedural from
// the world position, so the mesh carries no UVs.
enum EMaterial : uint8_t {
    MAT_FLOOR = 0,
    MAT_WALL,
    MAT_CEILING,
    MAT_TRIM,
    MAT_BEZEL,
    MAT_LABEL,
    MAT_LIGHT,
};

// Interleaved vertex layout of SHouse::vertices.
constexpr int kVertexFloats = 8; // position 3, normal 3, material, group

// The screen inside a frame: drawn separately, textured with the wallpaper,
// so each wall reads as that workspace's desktop.
struct SScreen {
    Vec3  center{};
    float yaw    = 0.f; // entity convention, as WallLayout::SFrame::yaw
    float width  = 0.f;
    float height = 0.f;
};

// An axis-aligned solid, for collision.
struct SSolid {
    Vec3 lo{}, hi{};
};

struct SHouse {
    // kVertexFloats per vertex, triangle list. `group` is the frame index
    // for a frame's bezel and label (the active workspace's light up) and
    // -1 for everything else.
    std::vector<float> vertices;

    std::vector<SScreen> screens; // index-aligned with layout.frames

    // Walls (around the openings), floor, ceiling, and a pane in every
    // opening: the openings are wider and taller than the player, and
    // without glass they would be doors out into the void.
    std::vector<SSolid> solids;

    // Ceiling lights: xyz position, w intensity.
    std::vector<std::array<float, 4>> lights;

    bool empty() const {
        return vertices.empty();
    }
};

SHouse build(const WallLayout::SLayout& layout);

} // namespace House

} // namespace H3D

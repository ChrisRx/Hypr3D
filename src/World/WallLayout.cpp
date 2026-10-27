#include "World/WallLayout.hpp"

#include <algorithm>
#include <cmath>

namespace H3D::WallLayout {

namespace {

// Along-wall length `count` frames need: a gap at each corner and one
// between every neighbouring pair.
float wallSpan(int count, float slot) {
    if (count <= 0)
        return 0.f;

    return count * slot + (count + 1) * kGap;
}

} // namespace

Vec3 wallNormal(int wall) {
    switch (wall) {
        case 0: return {0.f, 0.f, 1.f};
        case 1: return {-1.f, 0.f, 0.f};
        case 2: return {0.f, 0.f, -1.f};
        default: return {1.f, 0.f, 0.f};
    }
}

Vec3 seat(const SFrame& frame, float x, float y, float monW, float monH,
          float lift) {
    const float PER_PX = monW > 0.f ? frame.width / monW : 0.f;

    return frame.center + frame.right * ((x - 0.5f * monW) * PER_PX) +
        Vec3{0.f, 1.f, 0.f} * ((0.5f * monH - y) * PER_PX) + frame.normal * lift;
}

const SFrame* SLayout::find(int64_t id) const {
    for (const auto& F : frames)
        if (F.id == id)
            return &F;

    return nullptr;
}

SLayout compute(
    const std::vector<SWorkspace>& workspaces,
    float                          frameWidth,
    float                          frameHeight
) {
    SLayout out;

    const int COUNT = static_cast<int>(workspaces.size());

    if (COUNT <= 0 || !(frameWidth > 0.f) || !(frameHeight > 0.f))
        return out;

    // Evenly around the room. The remainder goes to the front wall first,
    // then the back, then the right: two workspaces end up face to face, and
    // a short wall never carries more frames than the long ones.
    const int BASE = COUNT / 4;
    out.perWall    = {BASE, BASE, BASE, BASE};

    constexpr int EXTRA_ORDER[3] = {0, 2, 1};
    for (int i = 0; i < COUNT % 4; ++i)
        ++out.perWall[EXTRA_ORDER[i]];

    const float SLOT     = frameWidth + 2.f * kBezel;
    const float MIN_SPAN = std::max(kMinSpan, SLOT + 2.f * kGap);

    out.halfX = 0.5f * std::max({wallSpan(out.perWall[0], SLOT),
                                 wallSpan(out.perWall[2], SLOT), MIN_SPAN});
    out.halfZ = 0.5f * std::max({wallSpan(out.perWall[1], SLOT),
                                 wallSpan(out.perWall[3], SLOT), MIN_SPAN});
    out.height = std::max(kMinHeight,
                          kSill + 2.f * kBezel + frameHeight + kHeadroom);

    const Vec3  UP{0.f, 1.f, 0.f};
    const float CENTER_Y = kSill + kBezel + frameHeight * 0.5f;

    size_t next = 0;

    for (int wall = 0; wall < 4; ++wall) {
        const int  ON_WALL = out.perWall[wall];
        const Vec3 NORMAL  = wallNormal(wall);
        const Vec3 RIGHT   = cross(UP, NORMAL);

        // The point of the wall plane level with the room centre.
        const Vec3 PLANE =
            NORMAL * -(wall % 2 == 0 ? out.halfZ : out.halfX);

        const float TOTAL = ON_WALL * SLOT + (ON_WALL - 1) * kGap;

        for (int i = 0; i < ON_WALL; ++i) {
            const auto& WS = workspaces[next++];

            const float ALONG = -0.5f * TOTAL + 0.5f * SLOT + i * (SLOT + kGap);

            SFrame frame;
            frame.id     = WS.id;
            frame.name   = WS.name;
            frame.wall   = wall;
            frame.center = PLANE + RIGHT * ALONG + UP * CENTER_Y +
                NORMAL * kFrameDepth;
            frame.normal = NORMAL;
            frame.right  = RIGHT;
            frame.width  = frameWidth;
            frame.height = frameHeight;
            frame.yaw    = std::atan2(NORMAL.x, NORMAL.z);

            out.frames.push_back(std::move(frame));
        }
    }

    return out;
}

} // namespace H3D::WallLayout

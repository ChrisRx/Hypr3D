#include "World/House.hpp"

#include "../../third_party/font8x8_basic.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace H3D::House {

namespace {

using WallLayout::SFrame;
using WallLayout::SLayout;

constexpr float kWallThickness = 0.25f;
constexpr float kSlab          = 0.3f; // floor and ceiling

// Window openings, cut into any stretch of wall a frame does not use.
constexpr float kOpeningSill   = 0.8f;
constexpr float kOpeningTop    = 3.4f; // and never closer than 0.8 to the ceiling
constexpr float kOpeningMaxW   = 3.0f;
constexpr float kOpeningMinW   = 1.2f;
constexpr float kOpeningMargin = 0.6f; // plain wall kept either side
constexpr float kFrameClear    = 0.3f; // ... and around every frame

constexpr float kTrim     = 0.07f; // opening frame bar width
constexpr float kMullion  = 0.05f;
constexpr float kBaseH    = 0.12f; // baseboard
constexpr float kCrownH   = 0.10f; // crown moulding
constexpr float kTrimProj = 0.03f; // how far trim stands off the wall

// Labels: one font8x8 pixel in world units, and the gap above the bezel.
constexpr float kGlyphPixel = 0.045f;
constexpr float kLabelGap   = 0.14f;

// Ceiling lights: one per this many metres of room, at most four a side.
constexpr float kLightSpacing = 5.0f;
constexpr int   kLightsPerSide = 4;

const Vec3 UP{0.f, 1.f, 0.f};

class CMesh {
  public:
    explicit CMesh(std::vector<float>& out) : m_out(out) {}

    // a-b-c-d counter-clockwise seen from where `n` points.
    void quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
              const Vec3& n, EMaterial mat, int group) {
        for (const Vec3* P : {&a, &b, &c, &a, &c, &d})
            m_out.insert(m_out.end(),
                         {P->x, P->y, P->z, n.x, n.y, n.z,
                          static_cast<float>(mat), static_cast<float>(group)});
    }

    void box(const SSolid& s, EMaterial mat, int group = -1) {
        const float X0 = s.lo.x, Y0 = s.lo.y, Z0 = s.lo.z;
        const float X1 = s.hi.x, Y1 = s.hi.y, Z1 = s.hi.z;

        quad({X1, Y0, Z1}, {X1, Y0, Z0}, {X1, Y1, Z0}, {X1, Y1, Z1}, {1, 0, 0}, mat, group);
        quad({X0, Y0, Z0}, {X0, Y0, Z1}, {X0, Y1, Z1}, {X0, Y1, Z0}, {-1, 0, 0}, mat, group);
        quad({X0, Y1, Z1}, {X1, Y1, Z1}, {X1, Y1, Z0}, {X0, Y1, Z0}, {0, 1, 0}, mat, group);
        quad({X0, Y0, Z0}, {X1, Y0, Z0}, {X1, Y0, Z1}, {X0, Y0, Z1}, {0, -1, 0}, mat, group);
        quad({X0, Y0, Z1}, {X1, Y0, Z1}, {X1, Y1, Z1}, {X0, Y1, Z1}, {0, 0, 1}, mat, group);
        quad({X1, Y0, Z0}, {X0, Y0, Z0}, {X0, Y1, Z0}, {X1, Y1, Z0}, {0, 0, -1}, mat, group);
    }

  private:
    std::vector<float>& m_out;
};

// One wall's own coordinates: `a` runs along it (left to right seen from
// inside, i.e. along the frames' right axis), `y` up from the floor, `d` into
// the wall from its inside face (negative stands out into the room).
struct SWallFrame {
    Vec3  normal, right;
    float plane = 0.f;  // distance of the inside face from the room centre
    float extent = 0.f; // half length along `a`, corners included

    Vec3 at(float a, float y, float d) const {
        return right * a + UP * y + normal * -(plane + d);
    }

    SSolid solid(float a0, float a1, float y0, float y1, float d0, float d1) const {
        const Vec3 P = at(a0, y0, d0);
        const Vec3 Q = at(a1, y1, d1);

        return {
            {std::min(P.x, Q.x), std::min(P.y, Q.y), std::min(P.z, Q.z)},
            {std::max(P.x, Q.x), std::max(P.y, Q.y), std::max(P.z, Q.z)},
        };
    }
};

SWallFrame wallFrame(const SLayout& layout, int wall) {
    SWallFrame w;
    w.normal = WallLayout::wallNormal(wall);
    w.right  = cross(UP, w.normal);

    // Front and back run the full width, corners included; the side walls
    // fit between them, so no two wall slabs overlap.
    if (wall % 2 == 0) {
        w.plane  = layout.halfZ;
        w.extent = layout.halfX + kWallThickness;
    } else {
        w.plane  = layout.halfX;
        w.extent = layout.halfZ;
    }

    return w;
}

struct SSpan {
    float a0 = 0.f, a1 = 0.f;
};

// Where frame `f` sits along its wall.
float alongWall(const SWallFrame& w, const SFrame& f) {
    return dot(f.center, w.right);
}

// The openings of one wall: one per stretch of free wall that is long
// enough, centred in it.
std::vector<SSpan> openingsFor(const SLayout& layout, int wall, const SWallFrame& w) {
    const float INSIDE = wall % 2 == 0 ? layout.halfX : layout.halfZ;

    std::vector<SSpan> used;
    for (const auto& F : layout.frames) {
        if (F.wall != wall)
            continue;

        const float A    = alongWall(w, F);
        const float HALF = 0.5f * F.width + WallLayout::kBezel + kFrameClear;
        used.push_back({A - HALF, A + HALF});
    }

    std::sort(used.begin(), used.end(),
              [](const SSpan& x, const SSpan& y) { return x.a0 < y.a0; });

    std::vector<SSpan> out;
    float cursor = -INSIDE;

    const auto TRY = [&](float s0, float s1) {
        const float WIDTH = std::min(kOpeningMaxW, (s1 - s0) - 2.f * kOpeningMargin);

        if (WIDTH < kOpeningMinW)
            return;

        const float MID = 0.5f * (s0 + s1);
        out.push_back({MID - 0.5f * WIDTH, MID + 0.5f * WIDTH});
    };

    for (const auto& U : used) {
        TRY(cursor, U.a0);
        cursor = std::max(cursor, U.a1);
    }

    TRY(cursor, INSIDE);
    return out;
}

void buildWall(CMesh& mesh, SHouse& house, const SLayout& layout, int wall) {
    const SWallFrame W = wallFrame(layout, wall);
    const float      H = layout.height;
    const float      T = kWallThickness;

    const float SILL = kOpeningSill;
    const float TOP  = std::min(kOpeningTop, H - 0.8f);

    const auto OPENINGS = TOP - SILL > 1.0f ? openingsFor(layout, wall, W) :
                                              std::vector<SSpan>{};

    const auto SLAB = [&](float a0, float a1, float y0, float y1) {
        if (a1 - a0 <= 1e-4f || y1 - y0 <= 1e-4f)
            return;

        const SSolid S = W.solid(a0, a1, y0, y1, 0.f, T);
        mesh.box(S, MAT_WALL);
        house.solids.push_back(S);
    };

    float cursor = -W.extent;

    for (const auto& O : OPENINGS) {
        SLAB(cursor, O.a0, 0.f, H);
        SLAB(O.a0, O.a1, 0.f, SILL);
        SLAB(O.a0, O.a1, TOP, H);
        cursor = O.a1;

        // The glass: collision only. Every opening is taller than the
        // player, so without it the room has doors into the void.
        house.solids.push_back(W.solid(O.a0, O.a1, SILL, TOP, 0.4f * T, 0.6f * T));

        // Casing on the inside face, a deeper sill, and a cross of
        // mullions in the opening itself.
        mesh.box(W.solid(O.a0 - kTrim, O.a0, SILL - kTrim, TOP + kTrim, -kTrimProj, 0.f), MAT_TRIM);
        mesh.box(W.solid(O.a1, O.a1 + kTrim, SILL - kTrim, TOP + kTrim, -kTrimProj, 0.f), MAT_TRIM);
        mesh.box(W.solid(O.a0, O.a1, TOP, TOP + kTrim, -kTrimProj, 0.f), MAT_TRIM);
        mesh.box(W.solid(O.a0 - 2.f * kTrim, O.a1 + 2.f * kTrim, SILL - 0.05f, SILL, -0.1f, 0.5f * T), MAT_TRIM);

        const float MID_A = 0.5f * (O.a0 + O.a1);
        const float MID_Y = SILL + 0.62f * (TOP - SILL);
        mesh.box(W.solid(MID_A - 0.5f * kMullion, MID_A + 0.5f * kMullion, SILL, TOP, 0.45f * T, 0.55f * T), MAT_TRIM);
        mesh.box(W.solid(O.a0, O.a1, MID_Y - 0.5f * kMullion, MID_Y + 0.5f * kMullion, 0.45f * T, 0.55f * T), MAT_TRIM);
    }

    SLAB(cursor, W.extent, 0.f, H);

    // Baseboard and crown moulding, inside corner to inside corner.
    const float INSIDE = wall % 2 == 0 ? layout.halfX : layout.halfZ;
    mesh.box(W.solid(-INSIDE, INSIDE, 0.f, kBaseH, -kTrimProj * 0.5f, 0.f), MAT_TRIM);
    mesh.box(W.solid(-INSIDE, INSIDE, H - kCrownH, H, -kTrimProj, 0.f), MAT_TRIM);
}

// The workspace name in font8x8, one quad per lit pixel, centred over the
// frame and standing just off the wall.
void buildLabel(CMesh& mesh, const SWallFrame& w, const SFrame& f, int group) {
    std::string text = f.name.empty() ? std::to_string(f.id) : f.name;

    const float MAX_W = f.width + 2.f * WallLayout::kBezel;
    const float PIXEL = std::min(kGlyphPixel, MAX_W / (8.f * std::max<size_t>(text.size(), 1)));

    const float A_LEFT = alongWall(w, f) - 0.5f * PIXEL * 8.f * text.size();
    const float Y_TOP  = WallLayout::kSill + 2.f * WallLayout::kBezel + f.height +
        kLabelGap + 8.f * PIXEL;
    const float D      = -0.006f;

    for (size_t ci = 0; ci < text.size(); ++ci) {
        const auto  C    = static_cast<unsigned char>(text[ci]);
        const auto* ROWS = font8x8_basic[C < 128 ? C : '?'];

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (!(ROWS[row] & (1u << col)))
                    continue;

                const float A0 = A_LEFT + (static_cast<float>(ci) * 8.f + col) * PIXEL;
                const float Y1 = Y_TOP - row * PIXEL;

                mesh.quad(w.at(A0, Y1 - PIXEL, D), w.at(A0 + PIXEL, Y1 - PIXEL, D),
                          w.at(A0 + PIXEL, Y1, D), w.at(A0, Y1, D), w.normal,
                          MAT_LABEL, group);
            }
        }
    }
}

} // namespace

SHouse build(const SLayout& layout) {
    SHouse house;

    if (layout.empty())
        return house;

    CMesh mesh{house.vertices};

    const float HX = layout.halfX + kWallThickness;
    const float HZ = layout.halfZ + kWallThickness;
    const float H  = layout.height;

    const SSolid FLOOR{{-HX, -kSlab, -HZ}, {HX, 0.f, HZ}};
    const SSolid CEILING{{-HX, H, -HZ}, {HX, H + kSlab, HZ}};

    mesh.box(FLOOR, MAT_FLOOR);
    mesh.box(CEILING, MAT_CEILING);
    house.solids.push_back(FLOOR);
    house.solids.push_back(CEILING);

    for (int wall = 0; wall < 4; ++wall)
        buildWall(mesh, house, layout, wall);

    for (size_t i = 0; i < layout.frames.size(); ++i) {
        const auto& F     = layout.frames[i];
        const auto  W     = wallFrame(layout, F.wall);
        const int   GROUP = static_cast<int>(i);
        const float A     = alongWall(W, F);
        const float HALF  = 0.5f * F.width + WallLayout::kBezel;

        mesh.box(W.solid(A - HALF, A + HALF, WallLayout::kSill,
                         WallLayout::kSill + 2.f * WallLayout::kBezel + F.height,
                         -WallLayout::kFrameDepth, 0.f),
                 MAT_BEZEL, GROUP);

        buildLabel(mesh, W, F, GROUP);

        // A hair in front of the bezel face; windows seat further out still
        // (kSeatBase in main.cpp).
        house.screens.push_back({F.center + F.normal * 0.003f, F.yaw, F.width, F.height});
    }

    const int NX = std::clamp(static_cast<int>(std::lround(2.f * layout.halfX / kLightSpacing)), 1, kLightsPerSide);
    const int NZ = std::clamp(static_cast<int>(std::lround(2.f * layout.halfZ / kLightSpacing)), 1, kLightsPerSide);

    for (int i = 0; i < NX; ++i) {
        for (int j = 0; j < NZ; ++j) {
            const float X = -layout.halfX + (i + 0.5f) * (2.f * layout.halfX / NX);
            const float Z = -layout.halfZ + (j + 0.5f) * (2.f * layout.halfZ / NZ);

            mesh.box({{X - 0.45f, H - 0.04f, Z - 0.45f}, {X + 0.45f, H, Z + 0.45f}}, MAT_LIGHT);

            // The more lamps, the dimmer each, so a big hall is not lit
            // brighter than a small room.
            house.lights.push_back({X, H - 0.8f, Z, 1.1f / std::sqrt(static_cast<float>(NX * NZ))});
        }
    }

    return house;
}

} // namespace H3D::House

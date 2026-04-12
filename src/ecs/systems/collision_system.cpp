#include "ecs/systems/collision_system.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "ecs/components.hpp"
#include "game/items.hpp"

namespace dreadcast::ecs::collision {

namespace {

bool circleRectOverlap(Vector2 center, float r, Rectangle rect) {
    const float cx = std::clamp(center.x, rect.x, rect.x + rect.width);
    const float cy = std::clamp(center.y, rect.y, rect.y + rect.height);
    const float dx = center.x - cx;
    const float dy = center.y - cy;
    return dx * dx + dy * dy <= r * r;
}

Rectangle spriteWorldBounds(const Transform &t, const Sprite &s) {
    return {t.position.x - s.width * 0.5F, t.position.y - s.height * 0.5F, s.width, s.height};
}

void applyEnemyKnockbackDir(entt::registry &registry, entt::entity target, Vector2 dir,
                            float strength) {
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len <= 0.001F || strength <= 0.001F) {
        return;
    }
    const Vector2 nd{dir.x / len, dir.y / len};
    auto &vel = registry.get_or_emplace<Velocity>(target);
    vel.value.x = nd.x * strength;
    vel.value.y = nd.y * strength;
    registry.emplace_or_replace<KnockbackState>(target,
                                                KnockbackState{config::KNOCKBACK_DURATION, 0.0F});
}

bool pierceListHas(const PierceHitRecord *rec, entt::entity e) {
    if (rec == nullptr) {
        return false;
    }
    return std::find(rec->hit.begin(), rec->hit.end(), e) != rec->hit.end();
}

void snareResolveHit(entt::registry &registry, entt::entity snareEntity,
                     entt::entity firstHitEnemy, const SnareProjectile &snare,
                     Vector2 *snareImpactWorld, float *snareImpactFlashTimer) {
    if (!registry.valid(firstHitEnemy) || !registry.all_of<Transform>(firstHitEnemy)) {
        return;
    }
    const Vector2 anchor = registry.get<Transform>(firstHitEnemy).position;
    const float r = snare.pullRadius;
    const float rSq = r * r;

    std::vector<entt::entity> group;
    const auto enemies = registry.view<Enemy, Transform>();
    for (const auto e : enemies) {
        const auto &t = registry.get<Transform>(e);
        const float dx = t.position.x - anchor.x;
        const float dy = t.position.y - anchor.y;
        if (dx * dx + dy * dy <= rSq) {
            group.push_back(e);
        }
    }
    if (group.empty()) {
        group.push_back(firstHitEnemy);
    }

    Vector2 centroid{0.0F, 0.0F};
    for (const auto e : group) {
        const auto &t = registry.get<Transform>(e);
        centroid.x += t.position.x;
        centroid.y += t.position.y;
    }
    const float inv = 1.0F / static_cast<float>(group.size());
    centroid.x *= inv;
    centroid.y *= inv;

    for (const auto e : group) {
        if (!registry.valid(e) || !registry.all_of<Transform>(e)) {
            continue;
        }
        auto &t = registry.get<Transform>(e);
        t.position.x += (centroid.x - t.position.x) * 0.88F;
        t.position.y += (centroid.y - t.position.y) * 0.88F;
        registry.emplace_or_replace<StunnedState>(
            e, StunnedState{snare.stunDuration, 0.0F});
    }
    if (snareImpactWorld != nullptr) {
        *snareImpactWorld = anchor;
    }
    if (snareImpactFlashTimer != nullptr) {
        *snareImpactFlashTimer = 0.55F;
    }
    if (registry.valid(snareEntity)) {
        registry.destroy(snareEntity);
    }
}

} // namespace

void projectile_hits(entt::registry &registry, entt::entity playerEntity,
                       dreadcast::InventoryState *inventory, Vector2 *snareImpactWorld,
                       float *snareImpactFlashTimer) {
    // Deadlight Snare: first enemy hit triggers pull + stun.
    std::vector<entt::entity> snares;
    for (const auto e : registry.view<Projectile, SnareProjectile, Transform, Sprite>()) {
        snares.push_back(e);
    }
    for (const auto projEntity : snares) {
        if (!registry.valid(projEntity)) {
            continue;
        }
        const auto &proj = registry.get<Projectile>(projEntity);
        const auto &snare = registry.get<SnareProjectile>(projEntity);
        const auto &pt = registry.get<Transform>(projEntity);
        const auto &ps = registry.get<Sprite>(projEntity);
        const Vector2 center = {pt.position.x, pt.position.y};
        const float radius = std::max(ps.width, ps.height) * 0.5F;
        if (!proj.fromPlayer) {
            continue;
        }
        const auto targets = registry.view<Enemy, Transform, Sprite, Health>();
        for (const auto target : targets) {
            const auto &tt = registry.get<Transform>(target);
            const auto &ts = registry.get<Sprite>(target);
            const Rectangle rect = spriteWorldBounds(tt, ts);
            if (circleRectOverlap(center, std::max(radius, config::PROJECTILE_RADIUS), rect)) {
                snareResolveHit(registry, projEntity, target, snare, snareImpactWorld,
                                snareImpactFlashTimer);
                break;
            }
        }
    }

    std::vector<entt::entity> projectiles;
    for (const auto e : registry.view<Projectile, Transform, Sprite>()) {
        if (registry.all_of<SnareProjectile>(e)) {
            continue;
        }
        projectiles.push_back(e);
    }
    for (const auto projEntity : projectiles) {
        if (!registry.valid(projEntity)) {
            continue;
        }
        const auto &proj = registry.get<Projectile>(projEntity);
        const auto &pt = registry.get<Transform>(projEntity);
        const auto &ps = registry.get<Sprite>(projEntity);
        const Vector2 center = {pt.position.x, pt.position.y};
        const float radius = std::max(ps.width, ps.height) * 0.5F;
        const bool isSlug = registry.all_of<SlugProjectile>(projEntity);

        if (proj.fromPlayer) {
            const auto targets = registry.view<Enemy, Transform, Sprite, Health>();
            for (const auto target : targets) {
                const auto &tt = registry.get<Transform>(target);
                const auto &ts = registry.get<Sprite>(target);
                const Rectangle rect = spriteWorldBounds(tt, ts);
                if (!circleRectOverlap(center, std::max(radius, config::PROJECTILE_RADIUS), rect)) {
                    continue;
                }
                if (isSlug) {
                    const PierceHitRecord *pierceRec = registry.try_get<PierceHitRecord>(projEntity);
                    if (pierceListHas(pierceRec, target)) {
                        continue;
                    }
                    auto &hp = registry.get<Health>(target);
                    hp.current -= proj.damage;
                    if (registry.all_of<Agitation>(target)) {
                        registry.get<Agitation>(target).agitationRange += 100.0F;
                    }
                    const float kb = registry.get<SlugProjectile>(projEntity).sideKnockback;
                    const Vector2 d = proj.direction;
                    const float dx = tt.position.x - pt.position.x;
                    const float dy = tt.position.y - pt.position.y;
                    const float cross = dx * d.y - dy * d.x;
                    const Vector2 perp = cross >= 0.0F ? Vector2{-d.y, d.x} : Vector2{d.y, -d.x};
                    applyEnemyKnockbackDir(registry, target, perp, kb);
                    auto &rec = registry.get_or_emplace<PierceHitRecord>(projEntity);
                    rec.hit.push_back(target);
                    continue;
                }

                auto &hp = registry.get<Health>(target);
                hp.current -= proj.damage;
                if (registry.all_of<Agitation>(target)) {
                    registry.get<Agitation>(target).agitationRange += 100.0F;
                }
                if (proj.knockbackOnHit > 0.001F) {
                    applyEnemyKnockbackDir(registry, target, proj.direction, proj.knockbackOnHit);
                }
                if (!proj.pierce) {
                    registry.destroy(projEntity);
                    break;
                }
            }
        } else {
            if (registry.valid(playerEntity) &&
                registry.all_of<Transform, Sprite, Health>(playerEntity)) {
                const auto &tt = registry.get<Transform>(playerEntity);
                const auto &ts = registry.get<Sprite>(playerEntity);
                const Rectangle rect = spriteWorldBounds(tt, ts);
                if (circleRectOverlap(center, std::max(radius, config::PROJECTILE_RADIUS), rect)) {
                    if (registry.all_of<ManicEffect>(playerEntity)) {
                        registry.destroy(projEntity);
                        continue;
                    }
                    auto &hp = registry.get<Health>(playerEntity);
                    const float dealt = proj.damage;
                    hp.current -= dealt;
                    registry.destroy(projEntity);
                    const float rf =
                        inventory ? inventory->totalEquippedDamageReflect() : 0.0F;
                    if (rf > 0.001F && registry.valid(proj.source) &&
                        registry.all_of<Health>(proj.source)) {
                        auto &sh = registry.get<Health>(proj.source);
                        sh.current -= dealt * rf;
                    }
                }
            }
        }
    }
}

void player_pickup_mana_shards(entt::registry &registry, entt::entity player) {
    if (!registry.valid(player) || !registry.all_of<Transform, Sprite, Mana>(player)) {
        return;
    }
    const auto &pt = registry.get<Transform>(player);
    const auto &ps = registry.get<Sprite>(player);
    const Rectangle playerRect = spriteWorldBounds(pt, ps);

    const auto shards = registry.view<ManaShard, Transform, Sprite>();
    for (const auto shard : shards) {
        const auto &st = registry.get<Transform>(shard);
        const auto &ss = registry.get<Sprite>(shard);
        const Rectangle shardRect = spriteWorldBounds(st, ss);
        if (CheckCollisionRecs(playerRect, shardRect)) {
            auto &mana = registry.get<Mana>(player);
            const float amount = registry.get<ManaShard>(shard).manaAmount;
            mana.current = std::min(mana.max, mana.current + amount);
            registry.destroy(shard);
        }
    }
}

entt::entity find_item_pickup_under_point(entt::registry &registry, Vector2 worldPoint) {
    entt::entity hit = entt::null;
    const auto pickups = registry.view<ItemPickup, Transform, Sprite>();
    for (const auto p : pickups) {
        const auto &it = registry.get<Transform>(p);
        const auto &is = registry.get<Sprite>(p);
        const Rectangle itemRect = spriteWorldBounds(it, is);
        if (CheckCollisionPointRec(worldPoint, itemRect)) {
            hit = p;
            break;
        }
    }
    return hit;
}

entt::entity find_nearest_item_pickup_in_range(entt::registry &registry, Vector2 worldPos,
                                               float maxRange) {
    entt::entity best = entt::null;
    const float maxSq = maxRange * maxRange;
    float bestDistSq = maxSq + 1.0F;
    const auto pickups = registry.view<ItemPickup, Transform, Sprite>();
    for (const auto p : pickups) {
        const auto &it = registry.get<Transform>(p);
        const float dx = it.position.x - worldPos.x;
        const float dy = it.position.y - worldPos.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 > maxSq) {
            continue;
        }
        if (best == entt::null || d2 < bestDistSq) {
            bestDistSq = d2;
            best = p;
        }
    }
    return best;
}

entt::entity find_item_pickup_hover_in_range(entt::registry &registry, Vector2 playerWorldPos,
                                             Vector2 worldMouse, float maxRange) {
    const auto under = find_item_pickup_under_point(registry, worldMouse);
    if (under == entt::null || !registry.all_of<Transform>(under)) {
        return entt::null;
    }
    const auto &it = registry.get<Transform>(under);
    const float dx = it.position.x - playerWorldPos.x;
    const float dy = it.position.y - playerWorldPos.y;
    const float d2 = dx * dx + dy * dy;
    if (d2 > maxRange * maxRange) {
        return entt::null;
    }
    return under;
}

entt::entity find_interactable_hover_in_range(entt::registry &registry, Vector2 playerWorldPos,
                                              Vector2 worldMouse, float maxRange) {
    const float maxSq = maxRange * maxRange;
    const auto view = registry.view<Interactable, Transform, Sprite>();
    for (const auto e : view) {
        auto &inter = registry.get<Interactable>(e);
        if (inter.opened) {
            continue;
        }
        const auto &transform = registry.get<Transform>(e);
        const auto &sprite = registry.get<Sprite>(e);
        const Rectangle rect = spriteWorldBounds(transform, sprite);
        if (!CheckCollisionPointRec(worldMouse, rect)) {
            continue;
        }
        const float dx = transform.position.x - playerWorldPos.x;
        const float dy = transform.position.y - playerWorldPos.y;
        if (dx * dx + dy * dy > maxSq) {
            continue;
        }
        return e;
    }
    return entt::null;
}

void rewrite_ground_pickup_indices_after_remove(entt::registry &registry, int removedIdx,
                                                int oldLastIdx) {
    if (removedIdx == oldLastIdx) {
        return;
    }
    const auto view = registry.view<ItemPickup>();
    for (const auto e : view) {
        auto &ip = view.get<ItemPickup>(e);
        if (ip.itemIndex == oldLastIdx) {
            ip.itemIndex = removedIdx;
        }
    }
}

bool try_pickup_item_entity(entt::registry &registry, entt::entity p, InventoryState &inventory,
                            float &inventoryFullFlashTimer) {
    if (!registry.valid(p) || !registry.all_of<ItemPickup, Transform, Sprite>(p)) {
        return false;
    }

    const int pickupIndex = registry.get<ItemPickup>(p).itemIndex;
    if (pickupIndex < 0 || pickupIndex >= static_cast<int>(inventory.items.size())) {
        registry.destroy(p);
        return true;
    }

    ItemData &pickupItem = inventory.items[static_cast<size_t>(pickupIndex)];

    auto tryMergeIntoStack = [&](int dstPoolIdx) -> bool {
        if (dstPoolIdx < 0 || dstPoolIdx >= static_cast<int>(inventory.items.size())) {
            return false;
        }
        ItemData &dst = inventory.items[static_cast<size_t>(dstPoolIdx)];
        if (!pickupItem.canStackWith(dst)) {
            return false;
        }
        const int space = dst.maxStack - dst.stackCount;
        if (space <= 0) {
            return false;
        }
        if (pickupItem.stackCount > space) {
            return false;
        }
        dst.stackCount += pickupItem.stackCount;
        const int removeIdx = pickupIndex;
        const int oldLast = static_cast<int>(inventory.items.size()) - 1;
        registry.destroy(p);
        inventory.removeItemAtIndex(removeIdx);
        rewrite_ground_pickup_indices_after_remove(registry, removeIdx, oldLast);
        return true;
    };

    if (pickupItem.isStackable && pickupItem.isConsumable) {
        for (size_t i = 0; i < inventory.bagSlots.size(); ++i) {
            const int bi = inventory.bagSlots[i];
            if (bi >= 0 && tryMergeIntoStack(bi)) {
                return true;
            }
        }
        for (size_t i = 0; i < inventory.consumableSlots.size(); ++i) {
            const int ci = inventory.consumableSlots[i];
            if (ci >= 0 && tryMergeIntoStack(ci)) {
                return true;
            }
        }
    }

    const int bag = inventory.firstEmptyBagSlot();
    if (bag < 0) {
        inventoryFullFlashTimer = 1.2F;
        return false;
    }

    inventory.bagSlots[static_cast<size_t>(bag)] = pickupIndex;
    registry.destroy(p);
    return true;
}

} // namespace dreadcast::ecs::collision

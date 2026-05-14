#include "AnimationFactory.h"
#include "Animations.h"
#include <sodium/randombytes.h>

// helper to visit the variant
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

std::unique_ptr<Animation> createAnimation(const GameEvent& event, const AnimationContext& ctx) {
    return std::visit(overloaded{
        [&](const CardPlayedEvent& e) -> std::unique_ptr<Animation> {
            auto it = ctx.cardTextures.find(e.card);
            if (it == ctx.cardTextures.end()) return nullptr;

            // pick a random slot from the playing player's hand. ctx still reflects the pre-event snapshot
            // TODO: update to be non-random for local player
            bool isLocal = (e.player == ctx.localPlayer);
            int preActionHandSize = std::max(1, isLocal ? ctx.myHandSize : ctx.oppHandSize);
            int slotIndex = static_cast<int>(randombytes_uniform(static_cast<uint32_t>(preActionHandSize)));
            auto boundsFunc = isLocal ? ctx.getPlayerCardBounds : ctx.getOpponentCardBounds;
            auto startBounds = boundsFunc(slotIndex, preActionHandSize, ctx.logicalWidth, ctx.cardWidth, ctx.cardHeight);
            sf::Vector2f endPos = {ctx.logicalWidth / 2.f - ctx.cardWidth / 2.f, ctx.logicalHeight / 2.f - ctx.cardHeight / 2.f};

            return std::make_unique<CardSlideAnimation>(
                e.card, false, startBounds.position, endPos,
                it->second, ctx.cardWidth, ctx.cardHeight);
        },

        [&](const CardDiscardedEvent& e) -> std::unique_ptr<Animation> {
            auto it = ctx.cardTextures.find(e.card);
            if (it == ctx.cardTextures.end()) return nullptr;

            // pick start position from the discarding player's hand area. ctx is pre-event
            bool isLocal = (e.player == ctx.localPlayer);
            int preActionHandSize = std::max(1, isLocal ? ctx.myHandSize : ctx.oppHandSize);
            int slotIndex = static_cast<int>(randombytes_uniform(static_cast<uint32_t>(preActionHandSize)));
            auto boundsFunc = isLocal ? ctx.getPlayerCardBounds : ctx.getOpponentCardBounds;
            auto startBounds = boundsFunc(slotIndex, preActionHandSize, ctx.logicalWidth, ctx.cardWidth, ctx.cardHeight);
            sf::Vector2f endPos = {ctx.logicalWidth / 2.f - ctx.cardWidth / 2.f, ctx.logicalHeight / 2.f - ctx.cardHeight / 2.f};

            return std::make_unique<CardSlideAnimation>(
                e.card, true, startBounds.position, endPos,
                it->second, ctx.cardWidth, ctx.cardHeight);
        },

        [&](const DamageDealtEvent& e) -> std::unique_ptr<Animation> {
            // opponent stats at top (y = 40), local stats at bottom (y = logicalHeight - 225)
            bool isLocal = (e.target == ctx.localPlayer);
            sf::Vector2f pos = isLocal
                ? sf::Vector2f{100.f, ctx.logicalHeight - 225.f}
                : sf::Vector2f{100.f, 40.f};
            return std::make_unique<DamageNumberAnimation>(e.amount, pos, ctx.font);
        },

        [&](const LifeGainedEvent& e) -> std::unique_ptr<Animation> {
            bool isLocal = (e.target == ctx.localPlayer);
            sf::Vector2f pos = isLocal
                ? sf::Vector2f{100.f, ctx.logicalHeight - 225.f}
                : sf::Vector2f{100.f, 40.f};
            return std::make_unique<LifeGainAnimation>(e.amount, pos, ctx.font);
        },

        [&](const CardsMilledEvent& e) -> std::unique_ptr<Animation> {
            bool isLocal = (e.player == ctx.localPlayer);
            sf::Vector2f pos = isLocal
                ? sf::Vector2f{200.f, ctx.logicalHeight - 210.f}
                : sf::Vector2f{200.f, 55.f};
            return std::make_unique<MillAnimation>(e.count, pos, ctx.font);
        },
    }, event);
}

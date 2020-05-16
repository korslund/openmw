#ifndef GAME_MWMECHANICS_AIBREATHE_H
#define GAME_MWMECHANICS_AIBREATHE_H

#include "typedaipackage.hpp"

namespace MWMechanics
{
    /// \brief AiPackage to have an actor resurface to breathe
    // The AI will go up if lesser than half breath left
    class AiBreathe final : public TypedAiPackage<AiBreathe>
    {
        public:
            bool execute (const MWWorld::Ptr& actor, CharacterController& characterController, AiState& state, float duration) final;

            static constexpr TypeId getTypeId() { return TypeIdBreathe; }

            static constexpr unsigned int defaultPriority() { return 2; }

            static constexpr bool defaultCanCancel() { return false; }

            static constexpr bool defaultShouldCancelPreviousAi() { return false; }
    };
}
#endif

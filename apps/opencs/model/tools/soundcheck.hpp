#ifndef CSM_TOOLS_SOUNDCHECK_H
#define CSM_TOOLS_SOUNDCHECK_H

#include <components/esm/loadsoun.hpp>

#include "../world/idcollection.hpp"

#include "stage.hpp"

namespace CSMTools
{
    /// \brief VerifyStage: make sure that sound records are internally consistent
    class SoundCheckStage : public Stage
    {
            const CSMWorld::IdCollection<ESM::Sound>& mSounds;

        public:

            SoundCheckStage (const CSMWorld::IdCollection<ESM::Sound>& sounds);

            virtual int setup();
            ///< \return number of steps

            virtual void perform (int stage, std::vector<std::string>& messages);
            ///< Messages resulting from this tage will be appended to \a messages.
    };
}

#endif

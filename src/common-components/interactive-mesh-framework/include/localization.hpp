#ifndef LOCALIZATION_HPP_
#define LOCALIZATION_HPP_

#include "location_defs.h"

namespace imf{
    class Localization{
        public:
            virtual bool start() = 0;
            virtual void stop() = 0;
            virtual void singleRun() = 0;
    };
}

#endif
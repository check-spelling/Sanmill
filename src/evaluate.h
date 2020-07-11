﻿/*
  Sanmill, a mill game playing engine derived from NineChess 1.5
  Copyright (C) 2015-2018 liuweilhy (NineChess author)
  Copyright (C) 2019-2020 Calcitem <calcitem@outlook.com>

  Sanmill is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Sanmill is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EVALUATE_H
#define EVALUATE_H

#include "config.h"

#include <string>

#include "types.h"

class Position;

namespace Eval {

std::string trace(Position *pos);

Value evaluate(Position *pos);

#ifdef EVALUATE_ENABLE

#ifdef EVALUATE_MATERIAL
    Value evaluateMaterial()
    {
        return 0;
    }
#endif

#ifdef EVALUATE_SPACE
    Value evaluateSpace()
    {
        return 0;
    }
#endif

#ifdef EVALUATE_MOBILITY
    Value evaluateMobility()
    {
        return 0;
    }
#endif

#ifdef EVALUATE_TEMPO
    Value evaluateTempo()
    {
        return 0;
    }
#endif

#ifdef EVALUATE_THREAT
    Value evaluateThreat()
    {
        return 0;
    }
#endif

#ifdef EVALUATE_SHAPE
    Value evaluateShape()
    {
        return 0;
    }
#endif

#ifdef EVALUATE_MOTIF
    Value AIAlgorithm::evaluateMotif()
    {
        return 0;
    }
#endif
#endif /* EVALUATE_ENABLE */
};

#endif /* EVALUATE_H */
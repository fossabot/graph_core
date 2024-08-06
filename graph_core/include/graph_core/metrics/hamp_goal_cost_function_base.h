#pragma once
/*
Copyright (c) 2024, Manuel Beschi and Cesare Tonola, JRL-CARI CNR-STIIMA/UNIBS, manuel.beschi@unibs.it, c.tonola001@unibs.it
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <graph_core/metrics/goal_cost_function_base.h>
#include <Eigen/Dense>

namespace graph
{
namespace core
{

/**
 * @class HampGoalCostFunctionBase
 * @brief Base class for defining goal cost functions.
 *
 * The HampGoalCostFunctionBase class provides an interface for defining cost functions
 * associated with goal configurations in HAMP path planning. Users can derive from
 * this class to implement custom cost functions.
 */
class HampGoalCostFunctionBase: public GoalCostFunctionBase
{

  /**
   * @brief human_positions_ Matrix 3xn with the human positions.
   */
  Eigen::Matrix<double,3,-1> human_positions_;

  /**
   * @brief human_velocities_ Matrix 3xn with the human velocities.
   */
  Eigen::Matrix<double,3,-1> human_velocities_;

public:

  /**
   * @brief Default constructor for GoalCostFunctionBase.
   */
  HampGoalCostFunctionBase()
  {

  }

  /**
   * @brief Set human position.
   *
   * @param human_positions Matrix 3xn with the human positions
   */
  virtual void setHumanPositions(const Eigen::Matrix<double,3,-1>& human_positions)
  {
    human_positions_=human_positions;
  }

  /**
   * @brief Set human position.
   *
   * @param human_velocities Matrix 3xn with the human velocities
   */
  virtual void setHumanVelocities(const Eigen::Matrix<double,3,-1>& human_velocities)
  {
    human_velocities_=human_velocities;
  }

};

typedef std::shared_ptr<HampGoalCostFunctionBase> HampGoalCostFunctionPtr;

} //end namespace core
} // end namespace graph

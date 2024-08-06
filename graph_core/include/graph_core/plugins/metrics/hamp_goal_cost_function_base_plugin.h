#pragma once
/*
Copyright (c) 2024, Manuel Beschi and Cesare Tonola, UNIBS and CNR-STIIMA, manuel.beschi@unibs.it, c.tonola001@unibs.it
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

#include <graph_core/metrics/hamp_goal_cost_function_base.h>
#include <cnr_class_loader/class_loader.hpp>

namespace graph
{
namespace core
{

/**
 * @class GoalCostFunctionBasePlugin
 * @brief This class implements a wrapper to graph::core::GoalCostFunctionBase to allow its plugin to be defined.
 * The class can be loaded as a plugin and builds a graph::core::GoalCostFunctionBase object.
 */
class HampGoalCostFunctionBasePlugin;
typedef std::shared_ptr<HampGoalCostFunctionBasePlugin> HampGoalCostFunctionPluginPtr;

class HampGoalCostFunctionBasePlugin: public std::enable_shared_from_this<HampGoalCostFunctionBasePlugin>
{
protected:

  /**
   * @brief metrics_ is the graph::core::GoalCostFunctionBase object built and initialized by this plugin class.
   */
  graph::core::HampGoalCostFunctionPtr goal_cost_fcn_;

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  /**
   * @brief Empty constructor for HampGoalCostFunctionBasePlugin. The function init() must be called afterwards.
   */
  HampGoalCostFunctionBasePlugin()
  {
    goal_cost_fcn_ = nullptr;
  }

  /**
   * @brief Destructor for GoalCostFunctionBasePlugin.
   */
  virtual ~HampGoalCostFunctionBasePlugin()
  {
    goal_cost_fcn_ = nullptr;
  }

  /**
   * @brief getCostFunction return the graph::core::GoalCostFunctionPtr object built by the plugin.
   * @return the graph::core::GoalCostFunctionPtr object built.
   */
  virtual graph::core::GoalCostFunctionPtr getCostFunction()
  {
    return goal_cost_fcn_;
  }

  /**
   * @brief init Initialise the graph::core::GoalCostFunctionBase object, defining its main attributes.
   * @param param_ns defines the namespace under which parameter are searched for using cnr_param library.
   * @param logger Pointer to a TraceLogger for logging.
   * @return True if correctly initialised, False if already initialised.
   */
  virtual bool init(const std::string& param_ns, const cnr_logger::TraceLoggerPtr& logger) = 0;
};

} //namespace core
} //namespace graph

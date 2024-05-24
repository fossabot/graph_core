#pragma once
/*
Copyright (c) 2019, Manuel Beschi CNR-STIIMA manuel.beschi@stiima.cnr.it
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

#include <graph_core/samplers/sampler_base.h>
#include <cnr_class_loader/register_macro.hpp>

namespace graph
{
namespace core
{

/**
 * @class SamplerBasePlugin
 * @brief This class implements a wrapper to graph::core::SamplerBase to allow its plugin to be defined.
 * The class can be loaded as a plugin and builds a graph::core::SamplerBase object.
 */
class SamplerBasePlugin;
typedef std::shared_ptr<SamplerBasePlugin> SamplerPluginPtr;

class SamplerBasePlugin: public std::enable_shared_from_this<SamplerBasePlugin>
{
protected:

  /**
   * @brief sampler_ is the graph::core::SamplerBase object built and initialized by this plugin class.
   */
  graph::core::SamplerPtr sampler_;

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  /**
   * @brief Empty constructor for SamplerBasePlugin. The function init() must be called afterwards.
   */
  SamplerBasePlugin()
  {
    sampler_ = nullptr;
  }

  /**
   * @brief Destructor for SamplerBasePlugin.
   */
  virtual ~SamplerBasePlugin()
  {
    sampler_ = nullptr;
  }

  /**
   * @brief getSampler return the grraph::core::SamplerPtr object built by the plugin.
   * @return the graph::core::SamplerPtr object built.
   */
  virtual graph::core::SamplerPtr getSampler()
  {
    return sampler_;
  }

  /**
   * @brief init Initialise the object graph::core::SamplerBase, defining its main attributes.
   * @param param_ns defines the namespace under which parameter are searched for using cnr_param library.
   * @param focus_1 focus 1 for the ellipse.
   * @param focus_2 focus 2 for the ellipse.
   * @param lower_bound Lower bounds for each dimension.
   * @param upper_bound Upper bounds for each dimension.
   * @param scale Scaling factors for each dimension.
   * @param logger TraceLogger for logging.
   * @param cost Cost of the path (default: infinity).
   * @return True if correctly initialised, False if already initialised.
   */
  virtual bool init(const std::string& param_ns,
                    const Eigen::VectorXd& focus_1,
                    const Eigen::VectorXd& focus_2,
                    const Eigen::VectorXd& lower_bound,
                    const Eigen::VectorXd& upper_bound,
                    const Eigen::VectorXd& scale,
                    const cnr_logger::TraceLoggerPtr& logger,
                    const double& cost = std::numeric_limits<double>::infinity()) = 0;
};

} //namespace core
} //namespace graph

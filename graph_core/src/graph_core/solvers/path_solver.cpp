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

#include <graph_core/solvers/path_solver.h>

namespace pathplan {

PathLocalOptimizer::PathLocalOptimizer(const CollisionCheckerPtr &checker, const MetricsPtr &metrics):
  checker_(checker),
  metrics_(metrics)
{

}
void PathLocalOptimizer::config(ros::NodeHandle &nh)
{
  max_stall_gen_=10;
  stall_gen_=0;
}

void PathLocalOptimizer::setPath(const PathPtr &path)
{
  assert(path);
  solved_=false;
  stall_gen_=0;
  path_=path;
}

bool PathLocalOptimizer::step(PathPtr& solution)
{
  solution=path_;

  if (solved_)
    return true;

  double cost=path_->cost();

  bool solved=!path_->warp();
  solved=!path_->slipParent() && solved;
  solved=!path_->slipChild() && solved;

  if (cost<=(1.001*path_->cost()))
  {
    if (stall_gen_==0)
    {
      ROS_FATAL("simplify path");
      if (!path_->simplify())
      {
        stall_gen_++;
      }
      else
      {
        solved=false;
      }
    }
    else
    {
      stall_gen_++;
    }
  }
  else
    stall_gen_=0;
  solved_=solved || (stall_gen_>=max_stall_gen_);
  return solved;

}

bool PathLocalOptimizer::solve(PathPtr& solution, const unsigned int &max_iteration)
{
  unsigned int iter=0;
  solution=path_;
  while (iter++<max_iteration)
  {
    if (solved_)
    {
      ROS_FATAL("solved in %u iterations",iter);
      return true;
    }
    step(solution);
  }
  return solved_;
}


}
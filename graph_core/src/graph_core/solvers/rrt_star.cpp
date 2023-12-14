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

#include <graph_core/solvers/rrt_star.h>

namespace pathplan
{

bool RRTStar::addStartTree(const TreePtr &start_tree, const double &max_time)
{
  assert(start_tree);
  start_tree_ = start_tree;
  return setProblem(max_time);
}

bool RRTStar::config(const ros::NodeHandle& nh)
{
  RRT::config(nh);
  solved_ = false;
  if (!nh.getParam("rewire_radius",r_rewire_))
  {
    ROS_DEBUG("%s/rewire_radius is not set. using 2.0*max_distance",nh.getNamespace().c_str());
    r_rewire_=2.0*max_distance_;
  }
  return true;
}

bool RRTStar::update(PathPtr& solution)
{
//  PATH_COMMENT("RRT*::update");

  return update(sampler_->sample(), solution);
}

bool RRTStar::update(const Eigen::VectorXd& configuration, PathPtr& solution)
{
  PATH_COMMENT("RRT*::update");

  if (!init_)
  {
    PATH_COMMENT("RRT* -> not init");

    return false;
  }
  if (cost_ <= utopia_tolerance_ * best_utopia_)
  {
    PATH_COMMENT("RRT*:: Solution already optimal");
    solution=solution_;
    completed_=true;
    return true;
  }

  if(not solved_)
  {
    PATH_COMMENT("RRT* -> solving");
    NodePtr new_node;
    if(start_tree_->rewire(configuration,r_rewire_,new_node))
    {
      if((new_node->getConfiguration() - goal_node_->getConfiguration()).norm() < max_distance_)
      {
        if(checker_->checkPath(new_node->getConfiguration(), goal_node_->getConfiguration()))
        {
          ConnectionPtr conn = std::make_shared<Connection>(new_node, goal_node_);
          conn->setCost(metrics_->cost(new_node, goal_node_));
          conn->add();

          solution_ = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node_), metrics_, checker_);
          solution_->setTree(start_tree_);
          solution = solution_;

          start_tree_->addNode(goal_node_);

          path_cost_ = solution_->cost();
          cost_=path_cost_+goal_cost_;
          sampler_->setCost(path_cost_);

          solved_ = true;

          return true;
        }
      }
    }
    return false;
  }
  else
  {
    PATH_COMMENT("RRT* -> improving");
    bool improved = start_tree_->rewire(configuration, r_rewire_);
    if(improved)
    {
      if (start_tree_->costToNode(goal_node_) >= (solution_->cost() - 1e-8))
        return false;

      solution_ = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node_), metrics_, checker_);
      solution_->setTree(start_tree_);

      path_cost_ = solution_->cost();
      cost_ = path_cost_+goal_cost_;
      sampler_->setCost(path_cost_);
    }
    solution = solution_;
    return improved;
  }
}


bool RRTStar::update(const NodePtr& n, PathPtr& solution)
{
  PATH_COMMENT("RRT*::update");

  if (!init_)
    return false;
  if (cost_ <= utopia_tolerance_ * best_utopia_)
  {
    completed_=true;
    solution=solution_;
    return true;
  }
  
  double old_path_cost = solution_->cost();
  bool improved = start_tree_->rewireToNode(n, r_rewire_);

  if(solution_ == nullptr)
  {
    ROS_INFO("RRT* -> solving");
    NodePtr new_node;
    if(start_tree_->rewireToNode(n,r_rewire_,new_node))
    {
      if((new_node->getConfiguration() - goal_node_->getConfiguration()).norm() < max_distance_)
      {
        if(checker_->checkPath(new_node->getConfiguration(), goal_node_->getConfiguration()))
        {
          ConnectionPtr conn = std::make_shared<Connection>(new_node, goal_node_);
          conn->setCost(metrics_->cost(new_node, goal_node_));
          conn->add();

          solution_ = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node_), metrics_, checker_);
          solution_->setTree(start_tree_);
          solution = solution_;

          start_tree_->addNode(goal_node_);

          path_cost_ = solution_->cost();
          cost_=path_cost_+goal_cost_;
          sampler_->setCost(path_cost_);

          solved_ = true;

          return true;
        }
      }
    }
    return false;
  }
  else
  {
    PATH_COMMENT("RRT* -> improving");

    //double r_rewire = std::min(start_tree_->getMaximumDistance(), r_rewire_factor_ * sampler_->getSpecificVolume() * std::pow(std::log(start_tree_->getNumberOfNodes())/start_tree_->getNumberOfNodes(),1./dof_));
    bool improved = start_tree_->rewireToNode(n, r_rewire_);

    if (improved)
    {
      if (start_tree_->costToNode(goal_node_) >= (solution_->cost() - 1e-8))
        return false;

      solution_ = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node_), metrics_, checker_);
      solution_->setTree(start_tree_);

      path_cost_ = solution_->cost();
      cost_ = path_cost_+goal_cost_;
      sampler_->setCost(path_cost_);
    }
    solution = solution_;
    return improved;
  }
}

bool RRTStar::solve(PathPtr &solution, const unsigned int& max_iter, const double &max_time)
{
  ros::WallTime tic = ros::WallTime::now();
  bool solved = false;
  for (unsigned int iter = 0; iter < max_iter; iter++)
  {
    if(update(solution))
    {
      ROS_DEBUG("Improved or solved in %u iterations", iter);
      solved_ = true;  //if solution_ was set externally, solved_ was altready true -> use solved to know if solve() completed successfully
      solved = true;
    }
    if((ros::WallTime::now()-tic).toSec()>=0.98*max_time)
      break;
  }
  return solved;
}

}

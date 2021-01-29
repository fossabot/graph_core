/*
Copyright (c) 2020, JRL-CARI CNR-STIIMA/UNIBS
Manuel Beschi manuel.beschi@unibs.it
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


#include <graph_core/solvers/multigoal.h>


namespace pathplan
{

bool MultigoalSolver::addStart(const NodePtr& start_node)
{
  if (!configured_)
  {
    ROS_ERROR("Solver is not configured.");
    return false;
  }
  solved_ = false;
  start_tree_ = std::make_shared<Tree>(start_node, Forward, max_distance_, checker_, metrics_);
  setProblem();
  ROS_DEBUG("Add start goal");
  return true;
}

bool MultigoalSolver::addGoal(const NodePtr& goal_node)
{
  if (!start_tree_)
  {
    ROS_ERROR(("You have to specify first the start goal"));
    return false;
  }

  double utopia=metrics_->utopia(goal_node,start_tree_->getRoot());
//  double utopia=(goal_node->getConfiguration() - start_tree_->getRoot()->getConfiguration()).norm();
  if (utopia>path_cost_)
  {
    ROS_DEBUG("the utopia cost of this goal is already worst than the actual solution, skip it");
    return false;
  }
  if (!checker_->check(goal_node->getConfiguration()))
  {
    ROS_DEBUG("goal collides, skip it");
    return false;
  }

  if (utopia<best_utopia_)
    best_utopia_=utopia;

  PathPtr solution;
  double cost=std::numeric_limits<double>::infinity();
  TreePtr goal_tree;
  GoalStatus status;
  NodePtr new_node;

  unsigned int index=goal_nodes_.size();
  if (start_tree_->connectToNode(goal_node, new_node))
  {
    solution = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node), metrics_, checker_);
    solution->setTree(start_tree_);

    cost = solution->cost();
    start_tree_->addNode(goal_node);

    solved_ = true;

    if (cost<=(utopia+1e-8))
    {
      ROS_DEBUG("Goal %u reaches its utopia.", index);
      status=GoalStatus::done;
    }
    else
    {
      ROS_DEBUG("Goal %u has a solution with cost %f",index,cost);
      status=GoalStatus::refine;
    }

    PATH_COMMENT_STREAM("A direct solution is found\n" << *solution);
  }
  else
  {
    cost = std::numeric_limits<double>::infinity();
    goal_tree = std::make_shared<Tree>(goal_node, Backward, max_distance_, checker_, metrics_);
    status=GoalStatus::search;
  }

  TubeInformedSamplerPtr tube_sampler = std::make_shared<TubeInformedSampler>(start_tree_->getRoot()->getConfiguration(),goal_node->getConfiguration(),sampler_->getLB(),sampler_->getUB(),path_cost_);
  tube_sampler->setLocalBias(local_bias_);
  tube_sampler->setRadius(tube_radius_);
  if (solution)
    tube_sampler->setPath(solution);

  goal_nodes_.push_back(goal_node);
  goal_trees_.push_back(goal_tree);
  costs_.push_back(cost);
  utopias_.push_back(utopia);
  solutions_.push_back(solution);
  tube_samplers_.push_back(tube_sampler);
  status_.push_back(status);

  if (isBestSolution(goal_nodes_.size()-1))
    ROS_DEBUG_STREAM("Goal "<<goal_node->getConfiguration().transpose() << " is the best one with a cost = "  << cost);

  init_=true;
  return true;
}

bool MultigoalSolver::isBestSolution(const int &index)
{
  assert(status_.at(index)!=GoalStatus::discard);

  if (costs_.at(index)>=(path_cost_-1e-8))
    return false;
  path_cost_=costs_.at(index);
  best_goal_index=index;
  solution_=solutions_.at(index);

  for (unsigned int igoal=0;igoal<goal_nodes_.size();igoal++)
  {
    if (status_.at(igoal)==GoalStatus::discard)
      continue;
    if (utopias_.at(igoal)>path_cost_)
    {
      status_.at(igoal)=GoalStatus::discard;
      ROS_DEBUG("Goal %u is discarded. utopia=%f, best cost=%f",igoal,utopias_.at(igoal),path_cost_);
      cleanTree();
      continue;
    }
    if (status_.at(igoal)==GoalStatus::done)
      continue;

    tube_samplers_.at(igoal)->setCost(path_cost_);
  }

  if ((path_cost_<0.9999*cost_at_last_clean) || start_tree_->needCleaning())
  {
    cost_at_last_clean=path_cost_;
    cleanTree();
  }

  return true;
}

void MultigoalSolver::resetProblem()
{
  goal_nodes_.clear();
  goal_trees_.clear();
  costs_.clear();
  utopias_.clear();
  solutions_.clear();
  tube_samplers_.clear();
  status_.clear();
  start_tree_.reset();
  solved_=false;
}

bool MultigoalSolver::setProblem()
{
  if (!start_tree_)
    return false;
  if (goal_nodes_.size()==0)
    return false;

  return true;
}

bool MultigoalSolver::config(const ros::NodeHandle& nh)
{
  max_distance_ = 1;
  if (!nh.getParam("max_distance",max_distance_))
  {
    ROS_WARN("%s/max_distance is not set. using 1.0",nh.getNamespace().c_str());
    max_distance_=1.0;
  }
  if (!nh.getParam("extend",extend_))
  {
    ROS_WARN("%s/extend is not set. using false (connect algorithm)",nh.getNamespace().c_str());
    max_distance_=1.0;
  }
  if (!nh.getParam("local_bias",local_bias_))
  {
    ROS_WARN("%s/local_bias is not set. using 0.3",nh.getNamespace().c_str());
    local_bias_=0.3;
  }
  if (local_bias_<0)
  {
    ROS_WARN("%s/local_bias cannot be negative, set equal to 0",nh.getNamespace().c_str());
    local_bias_=0.0;
  }
  if (local_bias_>1)
  {
    ROS_WARN("%s/local_bias cannot be greater than 1.0, set equal to 1.0",nh.getNamespace().c_str());
    local_bias_=1.0;
  }

  if (!nh.getParam("tube_radius",tube_radius_))
  {
    ROS_WARN("%s/tube_radius is not set. using 0.01",nh.getNamespace().c_str());
    tube_radius_=0.3;
  }
  if (tube_radius_<=0)
  {
    ROS_WARN("%s/local_bias cannot be negative, set equal to 0.01",nh.getNamespace().c_str());
    tube_radius_=0.01;
  }


  configured_=true;
  return true;
}


bool MultigoalSolver::update(PathPtr& solution)
{
  if (!init_)
    return false;
  if (path_cost_ <= 1.003 * best_utopia_)
  {
    solution=solution_;
    completed_=true;
    return false;
  }


  bool global_improvement=false;
  double r_rewire = start_tree_->getMaximumDistance();
  double old_cost=path_cost_;


  for (unsigned int igoal=0;igoal<goal_nodes_.size();igoal++)
  {
    NodePtr new_start_node, new_goal_node;
    bool add_to_start, add_to_goal;
    Eigen::VectorXd configuration = tube_samplers_.at(igoal)->sample();

    double prob=1.0;
    if ((costs_.at(igoal)-path_cost_)>2.0*path_cost_)
    {
      prob=0.1;
    }
    else
    {
      prob=1.0-0.9*((costs_.at(igoal)-path_cost_))/(2.0*path_cost_);
    }
    if (ud_(gen_)>prob)
      continue;
    switch (status_.at(igoal))
    {

    case GoalStatus::search:


      if (extend_)
        add_to_start = start_tree_->extend(configuration, new_start_node);
      else
        add_to_start = start_tree_->connect(configuration, new_start_node);


      if (add_to_start)
      {
        if (extend_)
          add_to_goal = goal_trees_.at(igoal)->extendToNode(new_start_node, new_goal_node);
        else
          add_to_goal = goal_trees_.at(igoal)->connectToNode(new_start_node, new_goal_node);
      }
      else
      {
        if (extend_)
          add_to_goal = goal_trees_.at(igoal)->extend(configuration, new_goal_node);
        else
          add_to_goal = goal_trees_.at(igoal)->connect(configuration, new_goal_node);
      }

      if (add_to_start && add_to_goal && new_goal_node == new_start_node)
      {
        goal_trees_.at(igoal)->keepOnlyThisBranch(goal_trees_.at(igoal)->getConnectionToNode(new_goal_node));
        start_tree_->addBranch(goal_trees_.at(igoal)->getConnectionToNode(new_goal_node));

        solutions_.at(igoal) = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_nodes_.at(igoal)), metrics_, checker_);
        solutions_.at(igoal)->setTree(start_tree_);
        tube_samplers_.at(igoal)->setPath(solutions_.at(igoal));
        tube_samplers_.at(igoal)->setRadius(tube_radius_*solutions_.at(igoal)->cost());
        costs_.at(igoal) = solutions_.at(igoal)->cost();

        solved_ = true;
        if (costs_.at(igoal)<=(utopias_.at(igoal)+1e-8))
        {
          ROS_DEBUG("Goal %u reaches its utopia. cost = %f, utopia =%f",igoal,costs_.at(igoal),utopias_.at(igoal));
          status_.at(igoal)=GoalStatus::done;
        }
        else
        {
          ROS_DEBUG("Goal %u has a solution with cost %f",igoal,costs_.at(igoal));
          status_.at(igoal)=GoalStatus::refine;
        }
        global_improvement=isBestSolution(igoal) || global_improvement;
      }

      break;
    case GoalStatus::refine:
      if (start_tree_->rewire(tube_samplers_.at(igoal)->sample(), r_rewire))
      {
        if (start_tree_->costToNode(goal_nodes_.at(igoal)) >= (costs_.at(igoal) - 1e-8))
          continue;

        solutions_.at(igoal) = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_nodes_.at(igoal)), metrics_, checker_);
        solutions_.at(igoal)->setTree(start_tree_);
        tube_samplers_.at(igoal)->setPath(solutions_.at(igoal));
        tube_samplers_.at(igoal)->setRadius(tube_radius_*solutions_.at(igoal)->cost());
        costs_.at(igoal) = solutions_.at(igoal)->cost();
        if (costs_.at(igoal)<=(utopias_.at(igoal)+1e-8))
        {
          ROS_DEBUG("Goal %u reaches its utopia. cost = %f, utopia =%f",igoal,costs_.at(igoal),utopias_.at(igoal));
          cleanTree();
          status_.at(igoal)=GoalStatus::done;
        }
        else
          ROS_DEBUG("Goal %u refines solution with cost %f",igoal,costs_.at(igoal));
        global_improvement=isBestSolution(igoal) || global_improvement;
      }
      break;
    case GoalStatus::discard:
    case GoalStatus::done:
      break;
    }
  }

  if (solved_)
  {
    local_bias_=std::min(forgetting_factor_*local_bias_+reward_*(old_cost-path_cost_)/(old_cost-best_utopia_),1.0);
    for (TubeInformedSamplerPtr& sampler: tube_samplers_)
    {
      sampler->setLocalBias(local_bias_);
    }
  }
  solution = solution_;
  return global_improvement;
}


void MultigoalSolver::printMyself(std::ostream &os) const
{
  os << "Best cost: " << path_cost_ << ". Nodes of start tree: " << start_tree_->getNumberOfNodes() << "\nGoals:\n";

  for (unsigned int igoal=0;igoal<goal_nodes_.size();igoal++)
  {
    os << igoal <<". Status: ";
    switch (status_.at(igoal))
    {
    case GoalStatus::done:
      os << "done";
      break;
    case GoalStatus::refine:
      os << "refine";
      break;
    case GoalStatus::search:
      os << "search";
      break;
    case GoalStatus::discard:
      os << "discard";
      break;
    }
    os << ". cost = " << costs_.at(igoal);
    os << ". utopia = " << utopias_.at(igoal);
    os << ". volume = " << std::scientific <<tube_samplers_.at(igoal)->getSpecificVolume()<< std::defaultfloat;

    os<<std::endl;
  }
}

void MultigoalSolver::cleanTree()
{
  std::vector<NodePtr> white_list=goal_nodes_;
  white_list.push_back(start_tree_->getRoot());
  std::vector<SamplerPtr> samplers;
  for (unsigned int igoal=0;igoal<goal_nodes_.size();igoal++)
  {
    if ((status_.at(igoal)==GoalStatus::refine)||(status_.at(igoal)==GoalStatus::search))
      samplers.push_back(tube_samplers_.at(igoal));
  }


  for (const PathPtr& sol: solutions_)
  {
    if (sol)
      for (const ConnectionPtr& conn: sol->getConnections())
        white_list.push_back(conn->getChild());
  }
  start_tree_->purgeNodesOutsideEllipsoids(samplers,white_list);
}

}  // namespace pathplan

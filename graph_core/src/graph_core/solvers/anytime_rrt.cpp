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

#include <graph_core/solvers/anytime_rrt.h>

namespace graph_core
{

void AnytimeRRT::importFromSolver(const AnytimeRRTPtr& solver)
{
  CNR_DEBUG(logger_,"Import from AnytimeRRT solver");

  RRT::importFromSolver(std::static_pointer_cast<RRT>(solver));

  bias_            = solver->getBias();
  delta_           = solver->getDelta();
  new_tree_        = solver->getNewTree();
  cost_impr_       = solver->getCostImpr();
}

void AnytimeRRT::importFromSolver(const TreeSolverPtr& solver)
{
  if(std::dynamic_pointer_cast<graph_core::AnytimeRRT>(solver) != nullptr)
  {
    AnytimeRRT::importFromSolver(std::static_pointer_cast<AnytimeRRT>(solver));
  }
  else
  {
    TreeSolver::importFromSolver(solver);
  }
}

bool AnytimeRRT::solveWithRRT(PathPtr& solution,
                              const unsigned int& max_iter,
                              const double &max_time)
{
  return RRT::solve(solution,max_iter,max_time);
}

bool AnytimeRRT::solve(PathPtr &solution, const unsigned int& max_iter, const double& max_time)
{
  std::chrono::time_point<std::chrono::system_clock> tic = std::chrono::system_clock::now();
  double time;

  if(solved_)
  {
    solution = solution_;

    if (cost_ <= utopia_tolerance_ * best_utopia_)
    {
      CNR_DEBUG(logger_,"Utopia reached!");
      completed_=true;
      return true;
    }
  }

  if(max_time <=0.0)
    return false;

  // RRT to find quickly a first sub-optimal solution
  bool success;
  unsigned int n_failed_iter = 0;

  time = (std::chrono::duration<double> (std::chrono::system_clock::now()-tic)).count();
  while(time<0.98*max_time && (not solved_) && n_failed_iter<FAILED_ITER)
  {
    success = AnytimeRRT::solveWithRRT(solution,max_iter,(max_time-time));

    CNR_DEBUG(logger_,"Tree has "<<start_tree_->getNumberOfNodes()<<" nodes");

    if(!success)
      n_failed_iter++;

    time = (std::chrono::duration<double> (std::chrono::system_clock::now()-tic)).count();
  }

  if(not solved_)
    return false;

  CNR_DEBUG(logger_,"Path cost: "<<path_cost_);

  if (cost_ <= utopia_tolerance_ * best_utopia_)
  {
    CNR_DEBUG(logger_,"Utopia reached!");
    completed_=true;
    return true;
  }

  // Informed trees to find better solutions
  NodePtr start_node = start_tree_->getRoot();
  NodePtr goal_node  = goal_node_;

  bool improved;
  n_failed_iter = 0;

  time = (std::chrono::duration<double> (std::chrono::system_clock::now()-tic)).count();
  while(time<0.98*max_time && completed_ == false && n_failed_iter<FAILED_ITER)
  {
    NodePtr tmp_start_node = std::make_shared<Node>(start_node->getConfiguration(),logger_);
    NodePtr tmp_goal_node  = std::make_shared<Node>(goal_node->getConfiguration(),logger_);
    improved = AnytimeRRT::improve(tmp_start_node,tmp_goal_node,solution,max_iter,(max_time-time));

    CNR_DEBUG(logger_,"New tree has "<<new_tree_->getNumberOfNodes()<<" nodes");

    improved? (n_failed_iter = 0):
              (n_failed_iter++);

    assert(not (improved) or start_tree_==new_tree_);

    if(cost_ <= utopia_tolerance_ * best_utopia_)
    {
      CNR_DEBUG(logger_,"Utopia reached!");
      completed_=true;
      break;
    }

    time = (std::chrono::duration<double> (std::chrono::system_clock::now()-tic)).count();
  }

  if(goal_node_ != goal_node)
  {
    //Rewire the tree goal (set goal_node)
    goal_node->disconnect();

    ConnectionPtr conn = std::make_shared<Connection>(solution_->getConnections().back()->getParent(), goal_node, logger_);
    conn->setCost(solution_->getConnections().back()->getCost());
    conn->setTimeCostUpdate(solution_->getConnections().back()->getTimeCostUpdate());
    conn->add();

    start_tree_->removeNode(goal_node_);
    goal_node_ = goal_node;
    start_tree_->addNode(goal_node_);

    solution_ = solution = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node_), metrics_, checker_, logger_);
    solution->setTree(start_tree_);
  }

  if(start_tree_->getRoot() != start_node)
  {
    //Rewire the tree root (set start_node)
    start_node->disconnect();
    NodePtr root = start_tree_->getRoot();  //tmp_start_node

    std::vector<NodePtr> root_children;

    ConnectionPtr conn_root_child_on_path = start_tree_->getConnectionToNode(goal_node_).front();
    NodePtr root_child_on_path = conn_root_child_on_path->getChild();
    double cost_first_conn_on_path = conn_root_child_on_path->getCost();

    ConnectionPtr conn2child;
    for(unsigned int i=0;i<root->getChildConnectionsSize();i++)
    {
      conn2child = root->childConnection(i);
      assert(conn2child->getParent() == start_tree_->getRoot());

      if(conn2child->getChild() != root_child_on_path)
      {
        root_children.push_back(conn2child->getChild());
      }
    }

    start_tree_->changeRoot(goal_node_); //change root before removing the old root
    start_tree_->removeNode(root);

    for(unsigned int i=0; i<root_children.size(); i++)
    {
      ConnectionPtr conn = std::make_shared<Connection>(start_node,root_children.at(i),logger_);  //the parent of start_node is the node on the path, the others are children
      conn->setCost(root_children.at(i)->getParentConnections().front()->getCost());
      conn->setTimeCostUpdate(root_children.at(i)->getParentConnections().front()->getTimeCostUpdate());
      conn->add();
    }

    ConnectionPtr conn2node_on_path = std::make_shared<Connection>(root_child_on_path,start_node,logger_);
    conn2node_on_path->setCost(cost_first_conn_on_path);
    conn2node_on_path->add();

    start_tree_->addNode(start_node);
    start_tree_->changeRoot(start_node);

    solution_ = solution = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node_), metrics_, checker_, logger_);
    solution->setTree(start_tree_);
  }

  CNR_DEBUG(logger_,"Final tree has "<<start_tree_->getNumberOfNodes()<<" nodes");

  return solved_;
}

bool AnytimeRRT::improve(NodePtr& start_node, PathPtr& solution, const unsigned int& max_iter, const double &max_time)
{
  double cost2beat = (1.0-cost_impr_)*path_cost_;
  return improve(start_node, solution, cost2beat, max_iter, max_time);
}

bool AnytimeRRT::improve(NodePtr& start_node, PathPtr& solution, const double& cost2beat, const unsigned int& max_iter, const double &max_time)
{
  NodePtr tmp_goal_node = std::make_shared<Node>(goal_node_->getConfiguration(),logger_);
  return improve(start_node, tmp_goal_node, solution, cost2beat, max_iter, max_time);
}

bool AnytimeRRT::improve(NodePtr& start_node, NodePtr& goal_node, PathPtr& solution, const unsigned int& max_iter, const double &max_time)
{
  double cost2beat = (1.0-cost_impr_)*path_cost_;
  return improve(start_node, goal_node, solution, cost2beat, max_iter, max_time);
}

bool AnytimeRRT::improve(NodePtr& start_node, NodePtr& goal_node, PathPtr& solution, const double& cost2beat, const unsigned int& max_iter, const double &max_time)
{
  std::chrono::time_point<std::chrono::system_clock> tic = std::chrono::system_clock::now();

  if(max_time <=0.0)
    return false;

  double utopia = (goal_node->getConfiguration() - start_node->getConfiguration()).norm(); //start and goal may be different from the previous ones
  completed_ = false;

  if(cost_ <= utopia_tolerance_ * utopia) //also if start and/or goal are changed, the old path is better to follow
  {
    CNR_DEBUG(logger_,"Utopia reached! Utopia: "<<utopia_tolerance_*utopia<<" path cost: "<<path_cost_);
    completed_=true;
    return false;
  }

  if(cost2beat <= utopia)
  {
    CNR_DEBUG(logger_,"The cost to beat is less than utopia, impossible to reach! Utopia: "<<utopia<<" cost to beat: "<<cost2beat);
    return false;
  }

  new_tree_ = std::make_shared<Tree>(start_node, max_distance_, checker_, metrics_, logger_, use_kdtree_);

  tmp_goal_node_ = goal_node;
  cost2beat_ = cost2beat;

  bias_ = bias_-delta_;
  if(bias_<0.1) bias_ = 0.1;

//  improve_sampler_ = std::make_shared<pathplan::InformedSampler>(start_node->getConfiguration(),goal_node->getConfiguration(),sampler_->getLB(),sampler_->getUB());
  improve_sampler_->setCost(path_cost_); //(1-cost_impr_)*path_cost_

  for (unsigned int iter = 0; iter < max_iter; iter++)
  {
    if(AnytimeRRT::improveUpdate(solution))
    {
      CNR_DEBUG(logger_,"Improved path cost: "<<path_cost_);

      if(solution->getTree() != solution_->getTree())
        assert(0);
      if(start_tree_ != new_tree_)
        assert(0);

      return true;
    }

    if((std::chrono::duration<double> (std::chrono::system_clock::now()-tic)).count()>=0.98*max_time)
      break;
  }

  return false;
}

bool AnytimeRRT::config(const YAML::Node& config)
{
  setParameters();

  return RRT::config(config);
}

void AnytimeRRT::resetProblem()
{
  new_tree_.reset();
  completed_ = false;

  RRT::resetProblem();
}

bool AnytimeRRT::improveUpdate(PathPtr &solution)
{
  CNR_DEBUG(logger_,"AnytimeRRT::improveUpdate");

  if(completed_)
  {
    CNR_DEBUG(logger_,"already found the best solution");
    solution = solution_;
    return true;
  }

  if (improve_sampler_->collapse())
    return false;

  return AnytimeRRT::improveUpdate(improve_sampler_->sample(), solution);
}

bool AnytimeRRT::improveUpdate(const Eigen::VectorXd& point, PathPtr &solution)
{
  CNR_DEBUG(logger_,"AnytimeRRT::improveUpdate");

  if (completed_)
  {
    CNR_DEBUG(logger_,"already found the best solution");
    solution = solution_;
    return true;
  }

  NodePtr new_node;
  if(new_tree_->informedExtend(point,new_node,tmp_goal_node_->getConfiguration(),cost2beat_,bias_))
  {
    if((new_node->getConfiguration() - tmp_goal_node_->getConfiguration()).norm() < max_distance_)
    {
      std::vector<ConnectionPtr> conn2node = new_tree_->getConnectionToNode(new_node);
      double cost_node2goal = metrics_->cost(new_node, tmp_goal_node_);
      double new_solution_cost = cost_node2goal;

      for(const ConnectionPtr& conn: conn2node)
        new_solution_cost += conn->getCost();

      if(new_solution_cost<solution_->cost())
      {
        if(checker_->checkConnection(new_node->getConfiguration(), tmp_goal_node_->getConfiguration()))
        {
          assert(tmp_goal_node_->getParentConnectionsSize() == 0);

          start_tree_->removeNode(goal_node_);
          goal_node_ = tmp_goal_node_;
          goal_cost_=goal_cost_fcn_->cost(goal_node_);

          ConnectionPtr conn_node2goal = std::make_shared<Connection>(new_node, goal_node_, logger_);
          conn_node2goal->setCost(cost_node2goal);
          conn_node2goal->add();

          new_tree_->addNode(goal_node_);
          start_tree_ = new_tree_;

          solution = std::make_shared<Path>(start_tree_->getConnectionToNode(goal_node_), metrics_, checker_, logger_);
          solution->setTree(start_tree_);
          solution_ = solution;

          best_utopia_ = goal_cost_+metrics_->utopia(start_tree_->getRoot(),goal_node_);;

          path_cost_ = solution_->cost();
          cost_ = path_cost_+goal_cost_;

          improve_sampler_->setCost(path_cost_);

          return true;
        }
      }
    }
  }
  return false;
}

bool AnytimeRRT::update(const NodePtr& n, PathPtr &solution)
{
  CNR_ERROR(logger_,"Update to node not yet available");
  return false;
}


}



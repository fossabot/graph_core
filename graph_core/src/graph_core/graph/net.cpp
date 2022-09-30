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

#include <graph_core/graph/net.h>

namespace pathplan
{

bool Net::purgeSuccessors(NodePtr& node, const std::vector<NodePtr>& white_list, unsigned int& removed_nodes)
{
  if (std::find(white_list.begin(), white_list.end(), node) != white_list.end())
  {
    ROS_INFO_STREAM("Node in white list: "<<*node);
    return false;
  }
  assert(node);

  bool purged;
  bool disconnect = true;
  do
  {
    purged = false;

    std::vector<NodePtr> successors = node->getChildren();
    successors.insert(successors.end(),node->getNetChildrenConst().begin(),node->getNetChildrenConst().end());

    for (NodePtr& n : successors)
    {
      assert(n.get()!=node.get());

      if((n->getNetParentConnectionsSize())>0 || n == linked_tree_->getRoot())
        continue;
      else
      {
        if(not purgeSuccessors(n,white_list,removed_nodes))
          disconnect = false;
        else
          purged = true;
      }
    }
  }while(purged);

  if(disconnect)
  {
    ConnectionPtr conn2convert;
    std::vector<NodePtr> children = node->getChildren();
    for(NodePtr& successor2save: children)
    {
      assert(successor2save->getNetParentConnectionsSize()>0);
      assert(successor2save->getParentConnectionsSize() == 1);
      assert(successor2save->parentConnection(0)->getParent() == node);

      conn2convert = successor2save->netParentConnection(0); //the successor2save must be still part of the tree, so you convert one of its net parent connection into a parent connection
      assert(conn2convert->isNet());
      conn2convert->convertToConnection();

      assert(successor2save->getParentConnectionsSize() == 1);
    }

    linked_tree_->purgeThisNode(node,removed_nodes);
  }

  return disconnect;
}

bool Net::purgeFromHere(ConnectionPtr& conn2node, const std::vector<NodePtr>& white_list, unsigned int& removed_nodes)
{
  NodePtr node = conn2node->getChild();

  if((node->getNetParentConnectionsSize())>0 || node == linked_tree_->getRoot())
  {
    if(not conn2node->isNet())
    {
      ConnectionPtr conn2convert = node->netParentConnection(0);
      assert(conn2convert->isNet());
      conn2convert->convertToConnection();
      removed_nodes = 0;
    }

    conn2node->remove();
    return false;
  }
  else
    return purgeSuccessors(node,white_list,removed_nodes);
}

std::multimap<double,std::vector<ConnectionPtr>>& Net::getConnectionBetweenNodes(const NodePtr &start_node, const NodePtr& goal_node, const std::vector<NodePtr>& black_list, const double& max_time)
{
  double cost2beat = std::numeric_limits<double>::infinity();
  return getConnectionBetweenNodes(start_node,goal_node,cost2beat,black_list,max_time);
}

std::multimap<double,std::vector<ConnectionPtr>>& Net::getConnectionBetweenNodes(const NodePtr &start_node, const NodePtr& goal_node, const double& cost2beat, const std::vector<NodePtr>& black_list, const double& max_time, const bool& search_in_tree)
{
  double cost2here = 0.0;

  black_list_.clear();
  black_list_ = black_list;

  visited_nodes_.clear();
  visited_nodes_.push_back(goal_node);

  map_.clear();
  connections2parent_.clear();

  max_time_ = max_time;

  time_vector_.clear();
  curse_of_dimensionality_ = 0;
  search_in_tree_ = search_in_tree;

  tic_search_ = ros::WallTime::now();
  computeConnectionFromNodeToNode(start_node,goal_node,cost2here,cost2beat);

  return map_;
}

std::multimap<double,std::vector<ConnectionPtr>>& Net::getConnectionToNode(const NodePtr &node, const std::vector<NodePtr>& black_list, const double& max_time)
{
  double cost2beat = std::numeric_limits<double>::infinity();
  return getConnectionToNode(node,cost2beat,black_list,max_time);
}

std::multimap<double,std::vector<ConnectionPtr>>& Net::getConnectionToNode(const NodePtr &node, const double& cost2beat, const std::vector<NodePtr>& black_list, const double& max_time)
{
  double cost2here = 0.0;

  black_list_.clear();
  black_list_ = black_list;

  visited_nodes_.clear();
  visited_nodes_.push_back(node);

  map_.clear();
  connections2parent_.clear();

  max_time_ = max_time;

  time_vector_.clear();
  curse_of_dimensionality_ = 0;

  tic_search_ = ros::WallTime::now();
  computeConnectionFromNodeToNode(linked_tree_->getRoot(),node,cost2here,cost2beat);

  return map_;
}

void Net::computeConnectionFromNodeToNode(const NodePtr& start_node, const NodePtr& goal_node)
{
  double cost2here = 0.0;
  double cost2beat = std::numeric_limits<double>::infinity();
  return computeConnectionFromNodeToNode(start_node, goal_node, cost2here, cost2beat);
}

void Net::computeConnectionFromNodeToNode(const NodePtr& start_node, const NodePtr& goal_node, const double& cost2here, const double& cost2beat)
{
  cost_to_beat_ = cost2beat;
  return computeConnectionFromNodeToNode(start_node,goal_node,cost2here);
}

void Net::computeConnectionFromNodeToNode(const NodePtr& start_node, const NodePtr& goal_node, const double& cost2here)
{
  ros::WallTime time_black_list_check, time_visited_list_check;
  ros::WallTime now = ros::WallTime::now();
  ros::WallTime tic_tot = now;

  if(verbose_)
    ROS_INFO_STREAM("time in: "<<(now-tic_search_).toSec());

  //Depth-first search
  curse_of_dimensionality_++;

  long double time_tot = 0.0;

  NodePtr parent;

  if(goal_node == linked_tree_->getRoot() || goal_node == start_node)
  {
    now = ros::WallTime::now();
    time_tot = (now-tic_tot).toSec();
    if(verbose_)
      ROS_INFO_STREAM("time return: "<<time_tot);

    return;
  }
  else
  {
    std::vector<ConnectionPtr> all_parent_connections = goal_node->getParentConnectionsConst();
    std::vector<ConnectionPtr> net_parent_connections = goal_node->getNetParentConnectionsConst();
    all_parent_connections.insert(all_parent_connections.end(),net_parent_connections.begin(),net_parent_connections.end());

    ros::WallTime tic_cycle;

    now = ros::WallTime::now();
    time_tot = (now-tic_tot).toSec();

    double time2now;
    double cost2parent;
    for(const ConnectionPtr& conn2parent:all_parent_connections)
    {
      tic_cycle = ros::WallTime::now();
      time2now = (tic_cycle-tic_search_).toSec();

      if(verbose_)
        ROS_INFO_STREAM("Available time: "<<max_time_-time2now);

      if(time2now>0.9*max_time_)
      {
        if(verbose_)
        {
          now = ros::WallTime::now();
          ROS_INFO_STREAM("Net max time exceeded! Time: "<<time2now<<" max time: "<<max_time_);
          ROS_INFO_STREAM("time return: "<<(now-tic_cycle).toSec());
        }
        return;
      }

      parent = conn2parent->getParent();

      if(search_in_tree_)
      {
        if(not linked_tree_->isInTree(parent))
          continue;
      }

      cost2parent = cost2here+conn2parent->getCost();

      if(cost2parent == std::numeric_limits<double>::infinity() || cost2parent>=cost_to_beat_ || std::abs(cost2parent-cost_to_beat_)<=NET_ERROR_TOLERANCE) //NET_ERROR_TOLERANCE to cope with machine errors
      {
        now = ros::WallTime::now();
        time_tot += (now-tic_cycle).toSec();

        if(verbose_)
        {
          ROS_INFO("cost up to now %lf, cost to beat %f -> don't follow this branch!",cost2parent,cost_to_beat_);
          ROS_INFO_STREAM("time don't follow branch: "<<(now-tic_cycle).toSec());
        }
        continue;
      }

      assert([&]() ->bool{
               if(not(cost2parent<cost_to_beat_))
               {
                 ROS_INFO("cost to parent %f, cost to beat %f ",cost2parent,cost_to_beat_);
                 return false;
               }
               return true;
             }());

      double cost_heuristics = cost2parent+(parent->getConfiguration()-start_node->getConfiguration()).norm();
      if(cost_heuristics>=cost_to_beat_ || std::abs(cost_heuristics-cost_to_beat_)<=NET_ERROR_TOLERANCE )
      {
        now = ros::WallTime::now();
        time_tot += (now-tic_cycle).toSec();

        if(verbose_)
        {
          ROS_INFO("cost heuristic through this node %lf, cost to beat %f -> don't follow this branch!",cost_heuristics,cost_to_beat_);
          ROS_INFO_STREAM("time cost heuristics: "<<(now-tic_cycle).toSec());
        }
        continue;
      }

      assert([&]() ->bool{
               if(not(cost_heuristics<cost_to_beat_))
               {
                 ROS_INFO("cost heuristics %f, cost to beat %f ",cost_heuristics,cost_to_beat_);
                 return false;
               }
               return true;
             }());

      if(parent == start_node)
      {
        //When the start node is reached, a solution is found -> insert into the map

        std::vector<ConnectionPtr> connections2start = connections2parent_;
        connections2start.push_back(conn2parent);

        std::reverse(connections2start.begin(),connections2start.end());

        std::pair<double,std::vector<ConnectionPtr>> pair;
        pair.first = cost2parent;
        pair.second = connections2start;

        if(not search_every_solution_) //update cost_to_beat_ -> search only for better solutions than this one
          cost_to_beat_ = cost2parent;

        if(verbose_)
        {
          ROS_INFO_STREAM("New conn inserted: "<<conn2parent<<" "<<*conn2parent<<" cost up to now: "<<cost2parent<<" cost to beat: "<<cost_to_beat_);
          ROS_INFO_STREAM("Start node reached! Cost: "<<cost2parent<<" (cost to beat updated)");
        }

        map_.insert(pair);

        time_tot += (ros::WallTime::now()-tic_cycle).toSec();
      }
      else
      {
        time_black_list_check = ros::WallTime::now();
        if(std::find(black_list_.begin(),black_list_.end(),parent)<black_list_.end())
        {
          now = ros::WallTime::now();
          time_tot += (now-tic_cycle).toSec();

          if(verbose_)
          {
            ROS_INFO_STREAM("parent belongs to black list, skipping..");
            ROS_INFO_STREAM("time black list: "<<(now-tic_cycle).toSec()<<" check: "<<(now-time_black_list_check).toSec());
          }
          continue;
        }

        now = ros::WallTime::now();
        if(verbose_)
          ROS_INFO_STREAM("time black list check: "<<(now-time_black_list_check).toSec());

        time_visited_list_check = ros::WallTime::now();
        if(std::find(visited_nodes_.begin(),visited_nodes_.end(),parent)<visited_nodes_.end())
        {
          now = ros::WallTime::now();
          time_tot += (now-tic_cycle).toSec();

          if(verbose_)
          {
            ROS_INFO_STREAM("avoiding cycles...");
            ROS_INFO_STREAM("time visited nodes: "<<(now-tic_cycle).toSec()<<" check: "<<(now-time_visited_list_check).toSec());
          }

          continue;
        }
        else
          visited_nodes_.push_back(parent);

        now = ros::WallTime::now();
        if(verbose_)
          ROS_INFO_STREAM("time visited list check: "<<(now-time_visited_list_check).toSec());

        //        std::vector<ConnectionPtr> connections2parent = connections2here_;
        //        connections2parent.push_back(conn2parent);

        connections2parent_.push_back(conn2parent);

        now = ros::WallTime::now();
        time_tot += (now-tic_cycle).toSec();

        if(verbose_)
        {
          ROS_INFO_STREAM("New conn inserted: "<<conn2parent<<" "<<*conn2parent<<" cost up to now: "<<cost2parent<<" cost to beat: "<<cost_to_beat_);
          ROS_INFO_STREAM("time before: "<<(now-tic_search_).toSec()<<" time cycle "<<(now-tic_cycle).toSec());
        }

        computeConnectionFromNodeToNode(start_node,parent,cost2parent);

        ros::WallTime tic_cycle2 = ros::WallTime::now();
        visited_nodes_.pop_back();
        connections2parent_.pop_back();

        now = ros::WallTime::now();
        time_tot += (now-tic_cycle2).toSec();
      }
    }

    time_vector_.push_back(time_tot);
    return;
  }
}

}  // end namespace pathplan

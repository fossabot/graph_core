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
#include <iostream>
#include <graph_core/graph/path.h>
namespace pathplan
{

Path::Path(std::vector<ConnectionPtr> connections,
           const MetricsPtr& metrics,
           const CollisionCheckerPtr& checker):
  connections_(connections),
  metrics_(metrics),
  checker_(checker)
{
  assert(connections_.size() > 0);

  cost_ = 0;

  for (const ConnectionPtr& conn : connections_)
  {
    cost_ += conn->getCost();
    change_warp_.push_back(true);
  }
  change_warp_.at(0) = false;
}

Path::Path(std::vector<NodePtr> nodes,
           const MetricsPtr& metrics,
           const CollisionCheckerPtr& checker):
  metrics_(metrics),
  checker_(checker)
{
  assert(nodes.size() > 0);

  connections_.clear();
  cost_ = 0;
  for(unsigned int i=0;i<nodes.size()-1;i++)
  {
    NodePtr parent = nodes.at(i);
    NodePtr child  = nodes.at(i+1);

    ConnectionPtr conn;
    (child->parent_connections_.size() == 0)? (conn = std::make_shared<Connection>(parent,child,false)):
                                              (conn = std::make_shared<Connection>(parent,child,true));
    double cost = metrics->cost(parent,child);
    conn->setCost(cost);
    conn->add();

    connections_.push_back(conn);

    cost_ += cost;
    change_warp_.push_back(true);
  }

  change_warp_.at(0) = false;
}

PathPtr Path::clone()
{
  std::vector<NodePtr> nodes;
  std::vector<ConnectionPtr> new_conn_vector;
  std::vector<Eigen::VectorXd> wp = getWaypoints();

  nodes.push_back(std::make_shared<Node>(wp.at(0)));

  for(unsigned int i = 1;i<wp.size();i++)
  {
    nodes.push_back(std::make_shared<Node>(wp.at(i)));

    ConnectionPtr conn = std::make_shared<Connection>(nodes.at(i-1),nodes.at(i));
    conn->setCost(connections_.at(i-1)->getCost());
    conn->add();

    new_conn_vector.push_back(conn);
  }

  PathPtr new_path = std::make_shared<Path>(new_conn_vector,metrics_,checker_);

  new_path->setChangeWarp(change_warp_);
  //new_path->setTree(tree_);  the nodes are cloned, the path does not match the path of the tree but is superimposed on it

  return new_path;
}

double Path::computeEuclideanNorm()
{
  double euclidean_norm = 0;

  for (const ConnectionPtr& conn : connections_)
    euclidean_norm += conn->norm();
  return euclidean_norm;
}


Eigen::VectorXd Path::pointOnCurvilinearAbscissa(const double& abscissa)
{
  assert(connections_.size() > 0);
  if (abscissa <= 0)
    return connections_.at(0)->getParent()->getConfiguration();
  double euclidean_norm = 0;
  for (const ConnectionPtr& conn : connections_)
  {
    if ((euclidean_norm + conn->norm()) > abscissa)
    {
      double ratio = (abscissa - euclidean_norm) / conn->norm();
      return conn->getParent()->getConfiguration() + ratio * (conn->getChild()->getConfiguration() - conn->getParent()->getConfiguration());
    }
    euclidean_norm += conn->norm();

  }
  return connections_.back()->getChild()->getConfiguration();
}

double Path::curvilinearAbscissaOfPoint(const Eigen::VectorXd& conf, int& idx)
{
  double abscissa = std::numeric_limits<double>::infinity();

  ConnectionPtr connection = findConnection(conf,idx);
  if(connection == nullptr)
  {
    ROS_ERROR("The configuration does not belong to the path -> the curvilinear abscissa can not be computed");
    ROS_INFO_STREAM("conf: "<<conf.transpose());
    assert(0);
    return abscissa;
  }
  else
  {
    return curvilinearAbscissaOfPointGivenConnection(conf,idx);
  }
}

double Path::curvilinearAbscissaOfPoint(const Eigen::VectorXd& conf)
{
  int idx;
  return curvilinearAbscissaOfPoint(conf,idx);
}

double Path::curvilinearAbscissaOfPointGivenConnection(const Eigen::VectorXd& conf, const int& conn_idx)
{
  double abscissa = std::numeric_limits<double>::infinity();

  if(conn_idx < 0 || conn_idx >= int(connections_.size()))
  {
    ROS_ERROR("The connection does not belong to the path -> the curvilinear abscissa can not be computed");
    ROS_INFO_STREAM("conn_idx: "<<conn_idx);
    assert(0);
    return abscissa;
  }

  double euclidean_norm = 0.0;
  double euclidean_norm_to_parent = -1.0;
  for(int i=0;i<int(connections_.size());i++)
  {
    if(i == conn_idx)
      euclidean_norm_to_parent = euclidean_norm;

    euclidean_norm += connections_.at(i)->norm();
  }

  assert(euclidean_norm_to_parent >= 0);

  double dist = (connections_.at(conn_idx)->getParent()->getConfiguration()-conf).norm();
  abscissa = (euclidean_norm_to_parent+dist)/euclidean_norm;

  return abscissa;
}

double Path::getCostFromConf(const Eigen::VectorXd &conf)
{
  computeCost();
  double cost = 0;
  int idx;
  ConnectionPtr this_conn = findConnection(conf,idx);

  if(this_conn == nullptr)
  {
    ROS_ERROR("cost can't be computed");
    return 0;
  }
  else
  {
    if(conf == connections_.at(0)->getParent()->getConfiguration())
      return cost_;

    if(idx < connections_.size()-1)
    {
      for(unsigned int i=idx+1;i<connections_.size();i++)
      {
        cost += connections_.at(i)->getCost();
        if(cost == std::numeric_limits<double>::infinity())
          return std::numeric_limits<double>::infinity();
      }
    }

    if(conf == this_conn->getParent()->getConfiguration())
      cost += this_conn->getCost();
    else if (conf == this_conn->getChild()->getConfiguration())
      cost += 0;
    else
    {
      if(this_conn->getCost() == std::numeric_limits<double>::infinity())
      {
        ConnectionPtr conn = std::make_shared<Connection>(std::make_shared<Node>(conf),this_conn->getChild());
        if(checker_->checkConnection(conn))
          cost += metrics_->cost(conf,this_conn->getChild()->getConfiguration());
        else
          cost = std::numeric_limits<double>::infinity();
      }
      else
        cost += metrics_->cost(conf,this_conn->getChild()->getConfiguration());
    }
  }
  return cost;
}

double Path::getNormFromConf(const Eigen::VectorXd &conf)
{
  double norm = 0;
  int idx;
  ConnectionPtr this_conn = findConnection(conf,idx);

  if(this_conn == nullptr)
  {
    ROS_ERROR("norm can't be computed");
    assert(0);
    return 0;
  }
  else
  {
    norm += (conf-this_conn->getChild()->getConfiguration()).norm();
    if(idx < connections_.size()-1)
    {
      for(unsigned int i=idx+1;i<connections_.size();i++) norm += connections_.at(i)->norm();
    }
  }

  return norm;
}

void Path::computeCost()
{
  cost_ = 0;

  for (const ConnectionPtr& conn : connections_)
    cost_ += conn->getCost();
}

void Path::setChanged(const unsigned int &connection_idx)
{
  change_warp_.at(connection_idx) = 1;
}

bool Path::bisection(const unsigned int &connection_idx,
                     const Eigen::VectorXd &center,
                     const Eigen::VectorXd &direction,
                     double max_distance,
                     double min_distance)
{
  assert(connection_idx < connections_.size());
  assert(connection_idx > 0);
  ConnectionPtr& conn12 = connections_.at(connection_idx - 1);
  ConnectionPtr& conn23 = connections_.at(connection_idx);

  NodePtr parent = conn12->getParent();
  NodePtr child = conn23->getChild();

  bool improved = false;
  double cost = conn12->getCost() + conn23->getCost();

  unsigned int iter = 0;
  double distance;

  while (iter++ < 5 & (max_distance - min_distance) > min_length_)
  {
    if (iter > 0)
      distance = 0.5 * (max_distance + min_distance);
    else
      distance = min_distance;

    Eigen::VectorXd p = center + direction * distance;
    assert(p.size() == center.size());
    double cost_pn = metrics_->cost(parent->getConfiguration(), p);
    double cost_nc = metrics_->cost(p, child->getConfiguration());
    double cost_n = cost_pn + cost_nc;

    if (cost_n >= cost)
    {
      min_distance = distance;
      continue;
    }
    bool is_valid = checker_->checkPath(parent->getConfiguration(), p) && checker_->checkPath(p, child->getConfiguration());
    if (!is_valid)
    {
      min_distance = distance;
      continue;
    }

    improved = true;
    max_distance = distance;
    cost = cost_n;
    //    conn12->remove(); // keep this connection, it could be useful in case of tree

    bool is_net = conn23->isNet();
    conn23->remove();

    NodePtr n = std::make_shared<Node>(p);
    conn12 = std::make_shared<Connection>(parent, n);

    is_net? (conn23 = std::make_shared<Connection>(n, child, true)):
            (conn23 = std::make_shared<Connection>(n, child, false));

    conn12->setCost(cost_pn);
    conn23->setCost(cost_nc);
    conn12->add();
    conn23->add();

    if (tree_)
      tree_->addNode(n, false);
  }

  if (improved)
    computeCost();
  return improved;
}

bool Path::warp(const double &min_dist, const double &max_time)
{
  if(max_time > 0)
  {
    ros::WallTime tic = ros::WallTime::now();
    for (unsigned int idx = 1; idx < connections_.size(); idx++)
    {
      if(connections_.at(idx-1)->norm()>min_dist && connections_.at(idx)->norm()>min_dist)
      {
        if (change_warp_.at(idx - 1) || change_warp_.at(idx))
        {
          Eigen::VectorXd center = 0.5 * (connections_.at(idx - 1)->getParent()->getConfiguration() +
                                          connections_.at(idx)->getChild()->getConfiguration());
          Eigen::VectorXd direction = connections_.at(idx - 1)->getChild()->getConfiguration() - center;
          double max_distance = direction.norm();
          double min_distance = 0;

          direction.normalize();

          if(!bisection(idx, center, direction, max_distance, min_distance))
          {
            change_warp_.at(idx) = 0;
          }
          else
            setChanged(idx);
        }
      }

      if((ros::WallTime::now()-tic).toSec()>=0.98*max_time) break;
    }
  }

  return std::any_of(change_warp_.cbegin(), change_warp_.cend(), [](bool i)
  {
    return i;
  });
}

bool Path::resample(const double &distance)
{
  bool resampled = false;
  unsigned int ic = 0;
  while (ic < connections_.size())
  {
    if (connections_.at(ic)->norm() <= distance)
      continue;

    ROS_ERROR("still unimplemented");
    resampled = true;
  }

  return resampled;

}

std::vector<NodePtr> Path::getNodes()
{
  std::vector<NodePtr> nodes;
  if (connections_.size() == 0)
    return nodes;

  nodes.push_back(connections_.at(0)->getParent());
  for (const ConnectionPtr& conn : connections_)
    nodes.push_back(conn->getChild());

  return nodes;
}


std::vector<Eigen::VectorXd> Path::getWaypoints()
{
  std::vector<Eigen::VectorXd> wp;
  if (connections_.size() == 0)
    return wp;

  wp.push_back(connections_.at(0)->getParent()->getConfiguration());
  for (const ConnectionPtr& conn : connections_)
    wp.push_back(conn->getChild()->getConfiguration());

  return wp;
}


ConnectionPtr Path::findConnection(const Eigen::VectorXd& configuration)
{
  int idx;
  return findConnection(configuration,idx);
}

ConnectionPtr Path::findConnection(const Eigen::VectorXd& configuration, int& idx)
{
  ConnectionPtr conn = nullptr;
  idx = -1;

  Eigen::VectorXd parent;
  Eigen::VectorXd child;

  double dist, distP, distC;

  for(unsigned int i=0; i<connections_.size(); i++)
  {
    parent = connections_.at(i)->getParent()->getConfiguration();
    child =  connections_.at(i)->getChild()->getConfiguration();

    dist = (parent-child).norm();
    distP = (parent - configuration).norm();
    distC = (configuration-child).norm();

    if(std::abs(dist-distP-distC)<1.0e-05)
    {
      conn = connections_.at(i);
      idx = i;
      return conn;
    }
  }

  // -->ERROR MSG<--
  ROS_ERROR("Connection not found");
  ROS_INFO_STREAM("conf: "<<configuration.transpose());

  for(unsigned int i=0; i<connections_.size(); i++)
  {
    parent = connections_.at(i)->getParent()->getConfiguration();
    child =  connections_.at(i)->getChild()->getConfiguration();

    dist = (parent-child).norm();
    distP = (parent - configuration).norm();
    distC = (configuration-child).norm();

    double err = std::abs(dist-distP-distC);

    ROS_INFO("conn n %d, length %f, dist from parent %f, dist from child %f, error %f",i,dist,distP,distC,err);
  }
  // -->         <--

  return nullptr;
}

Eigen::VectorXd Path::projectOnConnection(const Eigen::VectorXd& point, const ConnectionPtr &conn, double& distance, bool& in_conn, bool verbose)
{ 
  Eigen::VectorXd parent = conn->getParent()->getConfiguration();
  Eigen::VectorXd child  = conn->getChild() ->getConfiguration();

  if(point == parent)
  {
    if(verbose)
      ROS_INFO("POINT == PARENT");

    in_conn = true;
    distance = 0.0;
    return parent;
  }
  else if(point == child)
  {
    if(verbose)
      ROS_INFO("POINT == CHILD");

    in_conn = true;
    distance = 0.0;
    return child;
  }
  else
  {
    Eigen::VectorXd conn_vector  = child-parent;
    Eigen::VectorXd point_vector = point-parent;

    double conn_length = (conn_vector).norm();
    assert(conn_length>0.0);

    double point_length = (point_vector).norm();
    assert(point_length>0);

    Eigen::VectorXd conn_versor = conn_vector/conn_length;
    double s = point_vector.dot(conn_versor);

    Eigen::VectorXd projection = parent + s*conn_versor;

    distance = (point-projection).norm();
    assert(not std::isnan(distance));
    assert((point-projection).dot(conn_vector)<1e-06);

    ((s>=0.0) && (s<=conn_length))? (in_conn = true):
                                    (in_conn = false);

    if((conn == connections_.front()) && (s<=0.0))  //if the point is before the start it is projected on the start
    {
      if(verbose)
        ROS_INFO("CONN == CONNECTIONS.front()");

      projection = parent; //the start
      in_conn = true;
    }

    if((conn == connections_.back()) && (s>=conn_length))  //if the point is before the start it is projected on the start
    {
      if(verbose)
        ROS_INFO("CONN == CONNECTIONS.back()");

      projection = child;  //the goal
      in_conn = true;
    }

    if(verbose)
      ROS_INFO_STREAM("in_conn: "<<in_conn<<" dist: "<<distance<<" s: "<<s<<" point_length: "<<point_length<<" conn_length: "<<conn_length<< " projection: "<<projection.transpose()<<" parent: "<<parent.transpose()<<" child: "<<child.transpose());

    return projection;
  }
}

Eigen::VectorXd Path::projectOnClosestConnection(const Eigen::VectorXd& point)
{
  Eigen::VectorXd pr;
  Eigen::VectorXd projection;
  double min_distance = std::numeric_limits<double>::infinity();

  for(const ConnectionPtr& conn:connections_)
  {
    double distance;
    bool in_connection;
    pr = projectOnConnection(point,conn,distance,in_connection);

    if(in_connection)
    {
      if(distance<min_distance)
      {
        min_distance = distance;
        projection = pr;
      }
    }
  }

  if(min_distance == std::numeric_limits<double>::infinity())
  {
    projection = findCloserNode(point)->getConfiguration();
    PATH_COMMENT("projection on path not found");
  }

  return projection;
}

Eigen::VectorXd Path::projectOnClosestConnectionKeepingPastPrj(const Eigen::VectorXd& point, const Eigen::VectorXd &past_prj, int& n_conn, int delta_n_conn)
{
  int idx;
  Eigen::VectorXd prj;
  Eigen::VectorXd projection;
  double distance_from_past_prj;
  double min_distance_from_past_prj = std::numeric_limits<double>::infinity();

  ConnectionPtr conn;
  for(unsigned int i=0;i<connections_.size();i++)
  {
    conn = connections_.at(i);

    double distance;
    bool in_connection;
    prj = projectOnConnection(point,conn,distance,in_connection);

    if(in_connection)
    {
      distance_from_past_prj = (prj-past_prj).norm();
      if(distance_from_past_prj<min_distance_from_past_prj)
      {
        if(i>=unsigned(n_conn) && i<=unsigned(n_conn+delta_n_conn))
        {
          min_distance_from_past_prj = distance_from_past_prj;
          projection = prj;
          idx = i;
        }
      }
    }
  }

  if(min_distance_from_past_prj == std::numeric_limits<double>::infinity())
  {
    projection = past_prj;
    PATH_COMMENT("projection on path not found");
  }
  else
    n_conn = idx;

  return projection;
}

Eigen::VectorXd Path::projectOnClosestConnectionKeepingCurvilinearAbscissa(const Eigen::VectorXd& point, Eigen::VectorXd &past_prj, double &new_abscissa, double &past_abscissa, int &n_conn, int delta_n_conn)
{
  int idx;
  Eigen::VectorXd prj;
  Eigen::VectorXd projection;
  double distance_from_past_abs;
  double min_distance_from_past_abs = std::numeric_limits<double>::infinity();

  ConnectionPtr conn;
  for(unsigned int i=0;i<connections_.size();i++)
  {
    conn = connections_.at(i);

    double distance;
    bool in_connection;
    prj = projectOnConnection(point,conn,distance,in_connection);

    if(in_connection)
    {
      if(i>=unsigned(n_conn) && i<=unsigned(n_conn+delta_n_conn))
      {
        double abscissa = curvilinearAbscissaOfPointGivenConnection(prj,i);
        distance_from_past_abs = abscissa-past_abscissa;

        if(abscissa>=past_abscissa && distance_from_past_abs<min_distance_from_past_abs)
        {
          new_abscissa = abscissa;
          min_distance_from_past_abs = distance_from_past_abs;
          projection = prj;
          idx = i;
        }
      }
    }
  }

  if(min_distance_from_past_abs == std::numeric_limits<double>::infinity())
  {
    new_abscissa = past_abscissa;
    projection = past_prj;
    PATH_COMMENT("projection on path not found");
  }
  else
    n_conn = idx;

  return projection;
}

bool Path::removeNodes()
{
  std::vector<NodePtr> deleted_nodes, white_list;
  return removeNodes(white_list, deleted_nodes);
}

bool Path::removeNodes(const std::vector<NodePtr> &white_list)
{
  std::vector<NodePtr> deleted_nodes;
  return removeNodes(white_list, deleted_nodes);
}

bool Path::removeNode(NodePtr& node, const std::vector<NodePtr> &white_list)
{
  if(node == connections_.front()->getParent() || node == connections_.back()->getChild())
  {
    ROS_ERROR_STREAM("FIRST OR LAST NODE "<<*node); //ELIMINA
    return false;
  }

  ConnectionPtr conn_parent_node,conn_node_child;
  int idx = -1;

  for(unsigned int i=0;i<connections_.size()-1;i++)
  {
    if(node == connections_.at(i)->getChild())
    {
      conn_parent_node = connections_.at(i);
      conn_node_child = connections_.at(i+1);
      idx = i;
    }
  }

  if(idx<0)
  {
    ROS_ERROR("Node does not belong to the path");
    return false;
  }

  return removeNode(node,idx,white_list);
}

bool Path::removeNode(NodePtr& node, const int& idx_conn, const std::vector<NodePtr> &white_list)
{
  if(node == connections_.front()->getParent() || node == connections_.back()->getChild())
  {
    ROS_ERROR_STREAM("FIRST OR LAST NODE "<<*node); //ELIMINA
    return false;
  }

  if(std::find(white_list.begin(),white_list.end(),node)<white_list.end())
  {
    ROS_ERROR("WHITE LIST"); //elimina
    return false;
  }

  if(idx_conn<0 || idx_conn > (connections_.size()-1))
  {
    ROS_ERROR("Node does not belong to the path");
    return false;
  }

  ConnectionPtr conn_parent_node = connections_.at(idx_conn);
  ConnectionPtr conn_node_child  = connections_.at(idx_conn+1);

  if(conn_parent_node->isParallel(conn_node_child)
     && ((node->parent_connections_.size()+node->net_parent_connections_.size()) <= 1)
     && ((node->child_connections_ .size()+node->net_child_connections_ .size()) <= 1)
     )
  {
    assert(not tree_ || node != tree_->getRoot()); //node must have 1 parent (root must have zero) and 1 child (root may have many)

    bool net = conn_node_child->isNet();
    ConnectionPtr new_conn = std::make_shared<pathplan::Connection>(conn_parent_node->getParent(),conn_node_child->getChild(),net);
    double cost = conn_parent_node->getCost()+conn_node_child->getCost();
    new_conn->setCost(cost);
    new_conn->add();

    node->disconnect();

    if(tree_)
      tree_->removeNode(node);

    std::vector<ConnectionPtr> new_connections;

    if(idx_conn>0)
      new_connections.insert(new_connections.begin(),connections_.begin(),connections_.begin()+idx_conn);

    new_connections.push_back(new_conn);

    if(idx_conn+2<connections_.size())
      new_connections.insert(new_connections.end(),connections_.begin()+(idx_conn+2),connections_.end());

    setConnections(new_connections);

    return true;
  }
  else
  {
    if(not conn_parent_node->isParallel(conn_node_child))
      ROS_ERROR("NOT PARALLEL");
    if(not ((node->parent_connections_.size()+node->net_parent_connections_.size()) <= 1))
      ROS_ERROR("PARENT >1");

    if(not ((node->child_connections_ .size()+node->net_child_connections_ .size()) <= 1))
      ROS_ERROR_STREAM("CHILD >1 "<<*node);

    return false;
  }
}

bool Path::removeNodes(const std::vector<NodePtr> &white_list, std::vector<NodePtr> &deleted_nodes)
{
  bool removed;
  std::vector<NodePtr> void_list;
  bool at_least_one_removed = false;

  do
  {
    removed = false;

    for(unsigned int i=0;i<connections_.size()-1;i++)
    {
      ConnectionPtr conn_parent_node = connections_.at(i);
      ConnectionPtr conn_node_child  = connections_.at(i+1);

      NodePtr node = conn_parent_node->getChild();
      assert(node == conn_node_child->getParent());

      if(std::find(white_list.begin(),white_list.end(),node)<white_list.end())
        continue;

      if(removeNode(node,i,void_list))
      {
        removed = true;
        at_least_one_removed = true;
        deleted_nodes.push_back(node);

        break; //because connections_ is changed
      }
    }
  }while(removed);

  return at_least_one_removed;
}

NodePtr Path::addNodeAtCurrentConfig(const Eigen::VectorXd& configuration, ConnectionPtr &conn, const bool &rewire)
{
  bool is_a_new_node;
  return addNodeAtCurrentConfig(configuration,conn,rewire,is_a_new_node);
}

NodePtr Path::addNodeAtCurrentConfig(const Eigen::VectorXd& configuration, const bool& rewire)
{
  ConnectionPtr conn = findConnection(configuration);
  return addNodeAtCurrentConfig(configuration,conn,rewire);

}

NodePtr Path::addNodeAtCurrentConfig(const Eigen::VectorXd& configuration, ConnectionPtr& conn, const bool& rewire, bool& is_a_new_node)
{
  is_a_new_node = false;

  if(rewire && !tree_)
  {
    ROS_ERROR("Tree not set, the new node can't be added to the path");
    assert(0);
    return nullptr;
  }

  if(conn)
  {
    std::vector<ConnectionPtr>::iterator it = find(connections_.begin(),connections_.end(),conn);
    if(it == connections_.end())
    {
      ROS_ERROR("Connection not member of connections_");
      return nullptr;
    }

    NodePtr parent = conn->getParent();
    NodePtr child = conn->getChild();

    if((parent->getConfiguration() - configuration).norm()<1e-06)
    {
      is_a_new_node = false;
      return parent;
    }
    else if((child ->getConfiguration() - configuration).norm()<1e-06)
    {
      is_a_new_node = false;
      return child;
    }
    else
    {
      //controlla se devi mettere connessione se rewire==false
      NodePtr actual_node = std::make_shared<Node>(configuration);
      is_a_new_node = true;

      //ELIMINA
      if(not (((configuration-parent->getConfiguration()).norm()>1e-06) && ((configuration-child->getConfiguration()).norm()>1e-06)))
      {
        ROS_INFO_STREAM("DIST PARENT: "<<(configuration-parent->getConfiguration()).norm() <<" DIST CHILD: "<<(configuration-child->getConfiguration()).norm());
        assert(0);
      }
      //

      if(rewire)
      {
        double cost_parent, cost_child;
        if(conn->getCost() == std::numeric_limits<double>::infinity())
        {
          if(not checker_->check(actual_node->getConfiguration()))
          {
            cost_parent = std::numeric_limits<double>::infinity();
            cost_child  = std::numeric_limits<double>::infinity();
          }
          else
          {
            if(not checker_->checkPath(actual_node->getConfiguration(),child->getConfiguration()))
            {
              cost_child = std::numeric_limits<double>::infinity();

              checker_->checkPath(actual_node->getConfiguration(),parent->getConfiguration())?
                    (cost_parent = metrics_->cost(parent->getConfiguration(),actual_node->getConfiguration())):
                    (cost_parent = std::numeric_limits<double>::infinity());
            }
            else
            {
              cost_parent = std::numeric_limits<double>::infinity();
              cost_child  = metrics_->cost(actual_node->getConfiguration(),child->getConfiguration());
            }
          }
        }
        else
        {
          cost_parent = metrics_->cost(parent->getConfiguration(), actual_node->getConfiguration());
          cost_child  = conn->getCost() - cost_parent;
        }

        bool net = conn->isNet();
        conn->remove();

        if(tree_)
          tree_->addNode(actual_node);

        ConnectionPtr conn_parent = std::make_shared<Connection>(parent, actual_node,false);
        conn_parent->setCost(cost_parent);
        conn_parent->add();

        ConnectionPtr conn_child = std::make_shared<Connection>(actual_node,child,net);
        conn_child->setCost(cost_child);
        conn_child->add();

        unsigned int size_before = connections_.size();

        //Insert the conn parent (do not flip these two inserts)
        *it = conn_parent;

        //Insert the conn child
        if((it+1) == connections_.end())
          connections_.push_back(conn_child);
        else
          connections_.insert(it+1,conn_child);

        assert(connections_.size() == (size_before+1));

        setConnections(connections_); //it updates some class members
      }
      return actual_node;
    }
  }

  ROS_ERROR("Connection not found, the node can't be created");
  assert(0);

  return nullptr;
}

NodePtr Path::findCloserNode(const Eigen::VectorXd& configuration, double &dist)
{
  if(connections_.size()<1)
  {
    ROS_ERROR("No connections");
    throw std::invalid_argument("No connections");
  }

  NodePtr closest_node=connections_.at(0)->getParent();
  double min_dist = (closest_node->getConfiguration()-configuration).norm();
  for (const ConnectionPtr& conn: connections_)
  {
    dist = (conn->getChild()->getConfiguration()-configuration).norm();
    if(dist<min_dist)
    {
      closest_node=conn->getChild();
      min_dist=dist;
    }
  }
  dist = min_dist;
  return closest_node;
}

NodePtr Path::findCloserNode(const Eigen::VectorXd& configuration)
{
  double distance;
  return findCloserNode(configuration,distance);
}

NodePtr Path::findCloserNode(const NodePtr& node)
{
  Eigen::VectorXd configuration = node->getConfiguration();
  return findCloserNode(configuration);
}

NodePtr Path::findCloserNode(const NodePtr& node, double &dist)
{
  Eigen::VectorXd configuration = node->getConfiguration();
  return findCloserNode(configuration,dist);
}

PathPtr Path::getSubpathToConf(const Eigen::VectorXd& conf, const bool get_copy)
{
  //If get_copy, the node is not real added to the path and a copy of the subpath is returned
  //If !get_copy, the node is really added to the path, the path and the tree are rewired and the real subpath is returned

  //Conf is a path waypoint
  for(const Eigen::VectorXd& wp: getWaypoints())
  {
    if(conf == wp)
    {
      if(not get_copy)
        return getSubpathToNode(conf);
      else
        return getSubpathToNode(conf)->clone();
    }
  }

  //Conf is not a path waypoint
  PathPtr subpath;
  NodePtr node;
  int idx_conn;
  ConnectionPtr conn = findConnection(conf,idx_conn);
  bool is_net = conn->isNet();

  if(not conn)
  {
    ROS_ERROR("Conf does not belong to the path, subpath to conf can not be computed");
    assert(0);
  }

  if(not get_copy)
  {
    node = addNodeAtCurrentConfig(conf,conn,true);
    subpath = getSubpathToNode(node);
  }
  else  //a copy of the subpath will be created (nodes, connections..)
  {
    node = addNodeAtCurrentConfig(conf,conn,false);

    NodePtr parent;
    std::vector<ConnectionPtr> connections_to_parent;
    if(idx_conn > 0)
    {
      connections_to_parent = ((getSubpathToNode(conn->getParent()))->clone())->getConnections();
      parent = connections_to_parent.back()->getChild();
    }
    else
      parent = std::make_shared<Node>(conn->getParent()->getConfiguration());

    double cost;
    if(conn->getCost() == std::numeric_limits<double>::infinity())
    {
      checker_->checkPath(conn->getParent()->getConfiguration(),node->getConfiguration())?
            (cost  = metrics_->cost(conn->getParent()->getConfiguration(),node->getConfiguration())):
            (cost = std::numeric_limits<double>::infinity());
    }
    else
      cost  = metrics_->cost(conn->getParent()->getConfiguration(),node->getConfiguration());

    ConnectionPtr conn_parent;
    conn_parent = std::make_shared<Connection>(parent,node,is_net);
    conn_parent->setCost(cost);
    conn_parent->add();

    std::vector<ConnectionPtr> connections_vector;
    if(not connections_to_parent.empty())
      connections_vector = connections_to_parent;

    connections_vector.push_back(conn_parent);

    subpath = std::make_shared<Path>(connections_vector,metrics_,checker_);
  }

  return subpath;
}

PathPtr Path::getSubpathFromConf(const Eigen::VectorXd& conf, const bool get_copy)
{
  //If get_copy, the node is not real added to the path and a copy of the subpath is returned
  //If !get_copy, the node is really added to the path, the path and the tree are rewired and the real subpath is returned

  //Conf is a path waypoint
  for(const Eigen::VectorXd& wp: getWaypoints())
  {
    if(conf == wp)
    {
      if(not get_copy)
        return getSubpathFromNode(conf);
      else
        return getSubpathFromNode(conf)->clone();
    }
  }

  //Conf is not a path waypoint
  PathPtr subpath;
  NodePtr node;
  int idx_conn;
  ConnectionPtr conn = findConnection(conf,idx_conn);
  if(not conn)
  {
    ROS_ERROR("Conf does not belong to the path, subpath from conf can not be computed");
    assert(0);
  }

  bool is_net = conn->isNet();

  if(not get_copy)
  {
    node = addNodeAtCurrentConfig(conf,conn,true);
    subpath = getSubpathFromNode(node);
  }
  else  //a copy of the subpath will be created (nodes, connections..)
  {
    node = addNodeAtCurrentConfig(conf,conn,false);

    NodePtr child;
    std::vector<ConnectionPtr> connections_from_child;
    if(idx_conn < connections_.size()-1)
    {
      connections_from_child = (getSubpathFromNode(conn->getChild())->clone())->getConnections();
      child = connections_from_child.front()->getParent();
    }
    else
    {
      child = std::make_shared<Node>(conn->getChild()->getConfiguration());
    }

    double cost;
    if(conn->getCost() == std::numeric_limits<double>::infinity())
    {
      checker_->checkPath(node->getConfiguration(),conn->getChild()->getConfiguration())?
            (cost  = metrics_->cost(node->getConfiguration(),conn->getChild()->getConfiguration())):
            (cost = std::numeric_limits<double>::infinity());
    }
    else
      cost  = metrics_->cost(node->getConfiguration(),conn->getChild()->getConfiguration());

    ConnectionPtr conn_child = std::make_shared<Connection>(node,child,is_net);
    conn_child->setCost(cost);
    conn_child->add();

    std::vector<ConnectionPtr> connections_vector;
    connections_vector.push_back(conn_child);

    if(not connections_from_child.empty())
      connections_vector.insert(connections_vector.end(),connections_from_child.begin(),connections_from_child.end());

    subpath = std::make_shared<Path>(connections_vector,metrics_,checker_);
  }

  return subpath;
}

PathPtr Path::getSubpathToNode(const NodePtr& node)
{
  return getSubpathToNode(node->getConfiguration());
}

PathPtr Path::getSubpathToNode(const Eigen::VectorXd& conf)
{
  if((conf-connections_.front()->getParent()->getConfiguration()).norm()<1e-06)
  {
    ROS_ERROR("No subpath available, the node is equal to the first node of the path");
    ROS_INFO_STREAM("configuration: "<<conf.transpose());
    for(const Eigen::VectorXd& wp:getWaypoints())
      ROS_INFO_STREAM("path wp: "<<wp.transpose());
    throw std::invalid_argument("No subpath available, the node is equal to the first node of the path");
  }

  if((conf-connections_.back()->getChild()->getConfiguration()).norm()<1e-06)
  {
    return this->pointer();
  }

  PathPtr subpath;
  for(unsigned int idx=0; idx<connections_.size(); idx++)
  {
    if((conf-connections_.at(idx)->getChild()->getConfiguration()).norm()<1e-06)
    {
      std::vector<ConnectionPtr> conn;
      conn.assign(connections_.begin(), connections_.begin()+idx+1); // to save the idx connections

      subpath = std::make_shared<Path>(conn, metrics_,checker_);
      return subpath;
    }
  }

  ROS_ERROR("The node doesn't belong to this path");
  ROS_INFO_STREAM("configuration: "<<conf.transpose());

  for(const Eigen::VectorXd& wp:getWaypoints())
    ROS_INFO_STREAM("wp: "<<wp.transpose());

  throw std::invalid_argument("The node doesn't belong to this path");
}

PathPtr Path::getSubpathFromNode(const NodePtr& node)
{
  return getSubpathFromNode(node->getConfiguration());
}

PathPtr Path::getSubpathFromNode(const Eigen::VectorXd& conf)
{
  if((conf-connections_.back()->getChild()->getConfiguration()).norm()<1e-06)
  {
    ROS_ERROR("No subpath available, the node is equal to the last node of the path");
    ROS_INFO_STREAM("configuration: "<<conf.transpose());
    throw std::invalid_argument("No subpath available, the node is equal to the last node of the path");
  }

  if((conf-connections_.front()->getParent()->getConfiguration()).norm()<1e-06)
  {
    return this->pointer();
  }

  PathPtr subpath;
  for(unsigned int idx=0; idx<connections_.size(); idx++)
  {
    if((conf-connections_.at(idx)->getChild()->getConfiguration()).norm()<1e-06)
    {
      std::vector<ConnectionPtr> conn;
      conn.assign(connections_.begin()+idx+1, connections_.end());

      subpath = std::make_shared<Path>(conn, metrics_,checker_);
      return subpath;
    }
  }

  ROS_ERROR("The node doesn't belong to this path");
  ROS_INFO_STREAM("configuration: "<<conf.transpose());

  for(const Eigen::VectorXd& wp:getWaypoints())
    ROS_INFO_STREAM("wp: "<<wp.transpose());

  throw std::invalid_argument("The node doesn't belong to this path");
}

bool Path::simplify(const double& distance)
{
  bool simplified=false;
  bool reconnect_first_conn = false;

  if(connections_.size()>1)
  {
    double dist = (connections_.at(0)->getParent()->getConfiguration() - connections_.at(0)->getChild()->getConfiguration()).norm();
    if(dist < distance) reconnect_first_conn = true;
  }

  unsigned int ic = 1;
  while (ic < connections_.size())
  {
    double dist = (connections_.at(ic)->getParent()->getConfiguration() - connections_.at(ic)->getChild()->getConfiguration()).norm();
    if (dist > distance )
    {
      if(!(ic == 1 && reconnect_first_conn))
      {
        ic++;
        continue;
      }
    }
    if (checker_->checkPath(connections_.at(ic - 1)->getParent()->getConfiguration(),
                            connections_.at(ic)->getChild()->getConfiguration()))
    {
      simplified = true;
      double cost = metrics_->cost(connections_.at(ic - 1)->getParent(),
                                   connections_.at(ic)->getChild());

      bool is_net = connections_.at(ic)->isNet();
      ConnectionPtr conn;
      is_net? (conn = std::make_shared<Connection>(connections_.at(ic - 1)->getParent(),connections_.at(ic)->getChild(),true)):
              (conn = std::make_shared<Connection>(connections_.at(ic - 1)->getParent(),connections_.at(ic)->getChild(),false));
      conn->setCost(cost);
      conn->add();

      connections_.at(ic)->remove();
      connections_.erase(connections_.begin() + (ic - 1), connections_.begin() + ic + 1);
      connections_.insert(connections_.begin() + (ic - 1), conn);

      change_warp_.erase(change_warp_.begin() + ic);
      if (ic>1)
      {
        change_warp_.at(ic - 1) = 1;
      }
    }
    else
      ic++;
  }

  return simplified;
}


bool Path::isValid(const CollisionCheckerPtr &this_checker)
{
  CollisionCheckerPtr checker = checker_;
  if(this_checker != nullptr)
    checker = this_checker;

  bool valid = isValidFromConn(connections_.front(),checker);

  if(not valid)
    cost_ = std::numeric_limits<double>::infinity();
  else
    computeCost();

  return valid;
}

bool Path::isValidFromConn(const ConnectionPtr& this_conn, const CollisionCheckerPtr &this_checker)
{
  CollisionCheckerPtr checker = checker_;
  if(this_checker != nullptr)
    checker = this_checker;

  bool valid = true;
  Eigen::VectorXd parent;
  Eigen::VectorXd child;
  double cost;
  bool from_here = false;

  for(const ConnectionPtr &conn : connections_)
  {
    if(this_conn == conn)
      from_here = true;

    if(from_here)
    {
      if(not checker->checkConnection(conn))
      {
        conn->setCost(std::numeric_limits<double>::infinity());
        valid = false;
      }
      else
      {
        parent = conn->getParent()->getConfiguration();
        child = conn->getChild()->getConfiguration();

        cost = metrics_->cost(parent,child);
        conn->setCost(cost);
      }
    }
  }

  return valid;
}

bool Path::isValidFromConf(const Eigen::VectorXd &conf, const CollisionCheckerPtr &this_checker)
{
  int pos_closest_obs_from_goal = -1;
  bool validity = isValidFromConf(conf,pos_closest_obs_from_goal,this_checker);
  return validity;
}

bool Path::isValidFromConf(const Eigen::VectorXd &conf, int &pos_closest_obs_from_goal, const CollisionCheckerPtr &this_checker)
{
  CollisionCheckerPtr checker = checker_;
  if(this_checker != nullptr)
    checker = this_checker;

  pos_closest_obs_from_goal = -1;
  bool validity = true;
  int idx;
  ConnectionPtr conn = findConnection(conf,idx);
  assert(conn != nullptr);

  if(conf == conn->getParent()->getConfiguration())
  {
    validity = isValidFromConn(conn,checker);

    if(not validity)
    {
      for(int i = (connections_.size()-1);i>=idx;i--)
      {
        if(connections_.at(i)->getCost() == std::numeric_limits<double>::infinity()) pos_closest_obs_from_goal = connections_.size()-1-i;
      }
    }
  }
  else if(conf == conn->getChild()->getConfiguration())
  {
    if(idx<connections_.size()-1)
    {
      conn = connections_.at(idx+1);
      validity = isValidFromConn(conn,checker);

      if(not validity)
      {
        for(int i = (connections_.size()-1);i>=idx+1;i--)
        {
          if(connections_.at(i)->getCost() == std::numeric_limits<double>::infinity()) pos_closest_obs_from_goal = connections_.size()-1-i;
        }
      }
    }
    else
    {
      ROS_INFO_STREAM("Conf is equal to goal, no connection to validate from here. Conf: "<<conf.transpose()<<" goal: "<<connections_.back()->getChild()->getConfiguration().transpose());
      validity = true;
      assert(0);
    }
  }
  else
  {
    if(not checker->checkConnFromConf(conn,conf))
    {
      validity = false;
      conn->setCost(std::numeric_limits<double>::infinity());
      pos_closest_obs_from_goal = connections_.size()-1-idx;
    }

    if(idx<connections_.size()-1)  //also if the checker has failed, this check is important to update the cost of all the connections
    {
      if(!isValidFromConn(connections_.at(idx+1),checker))
      {
        validity = false;
        for(int i = (connections_.size()-1);i>=idx+1;i--)
        {
          if(connections_.at(i)->getCost() == std::numeric_limits<double>::infinity()) pos_closest_obs_from_goal = connections_.size()-1-i;
        }
      }
    }
  }

  return validity;
}

XmlRpc::XmlRpcValue Path::toXmlRpcValue(bool reverse) const
{
  XmlRpc::XmlRpcValue x;
  if (connections_.size()==0)
    return x;
  x.setSize(connections_.size()+1);

  if (not reverse)
  {
    x[0]=connections_.at(0)->getParent()->toXmlRpcValue();
    for (size_t idx=0;idx<connections_.size();idx++)
      x[idx+1]=connections_.at(idx)->getChild()->toXmlRpcValue();
  }
  else
  {
    x[0]=connections_.back()->getChild()->toXmlRpcValue();
    for (size_t idx=0;idx<connections_.size();idx++)
      x[idx+1]=connections_.at(connections_.size()-idx-1)->getParent()->toXmlRpcValue();
  }
  return x;
}

void Path::flip()
{
  for (ConnectionPtr conn: connections_)
    conn->flip();

  std::reverse(connections_.begin(),connections_.end());
}

std::ostream& operator<<(std::ostream& os, const Path& path)
{
  os << "cost = " << path.cost_ << std::endl;
  if (path.connections_.size() == 0)
    os << "no waypoints";

  os << "waypoints= " << std::endl << "[";

  for (const ConnectionPtr& conn : path.connections_)
  {
    os << conn->getParent()->getConfiguration().transpose() << ";" << std::endl;
  }
  os << path.connections_.back()->getChild()->getConfiguration().transpose() << "];" << std::endl;

  return os;
}

}

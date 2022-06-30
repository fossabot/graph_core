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

#include <graph_core/graph/connection.h>

namespace pathplan
{

Connection::Connection(const NodePtr& parent, const NodePtr& child, const bool is_net):
  parent_(parent),
  child_(child),
  is_net_(is_net)
{
  euclidean_norm_ = (child->getConfiguration() - parent->getConfiguration()).norm();
}

void Connection::add(const bool is_net)
{
  assert((is_net && getChild()->parent_connections_.size() == 1) || ((not is_net) && getChild()->parent_connections_.size() == 0));

  is_net_ = is_net;
  add();
}

void Connection::add()
{
  assert(not valid_);
  valid_ = true;

  if(is_net_)
  {
    getParent()->addNetChildConnection(pointer());
    getChild()->addNetParentConnection(pointer());
  }
  else
  {
    getParent()->addChildConnection(pointer());
    getChild()->addParentConnection(pointer());
  }
}
void Connection::remove()
{
  if (!valid_)
  {
    //ROS_ERROR("not valid");
    return;
  }

  valid_ = false;
  if (not parent_.expired())
  {
    if(is_net_)
      getParent()->remoteNetChildConnection(pointer());
    else
      getParent()->remoteChildConnection(pointer());
  }
  else
    ROS_FATAL("parent already destroied");

  if (not child_.expired())
  {
    if(is_net_)
      getChild()->remoteNetParentConnection(pointer());
    else
      getChild()->remoteParentConnection(pointer());
  }
  else
    ROS_FATAL("child already destroied");
}

void Connection::flip()
{
  remove(); // remove connection from parent and child
  parent_.swap(child_);
  add();   // add new connection from new parent and child
}
Connection::~Connection()
{
}

bool Connection::isParallel(const ConnectionPtr& conn, const double& toll)
{
  if(euclidean_norm_ == 0.0 || conn->norm() == 0.0)
  {
    ROS_INFO_STREAM("A connection has norm zero");
    assert(0);
    return false;
  }
  // v1 dot v2 = norm(v1)*norm(v2)*cos(angle)
  double scalar= (getChild()->getConfiguration()-getParent()->getConfiguration()).dot(
        conn->getChild()->getConfiguration()-conn->getParent()->getConfiguration());

  // v1 dot v2 = norm(v1)*norm(v2) if v1 // v2

  return (scalar>(euclidean_norm_*conn->norm())-toll);
}

bool Connection::convertToConnection()
{
  if(is_net_)
  {
    remove();
    is_net_ = false;
    add();
    return true;
  }
  return false;
}

bool Connection::convertToNetConnection()
{
  if(not is_net_)
  {
    remove();
    is_net_ = true;
    add();
    return true;
  }
  return false;
}

void Connection::changeConnectionType()
{
  if(not convertToConnection())
    convertToNetConnection();
}

std::ostream& operator<<(std::ostream& os, const Connection& connection)
{
//  os << connection.parent_->getConfiguration().transpose() << " -->" << std::endl;
//  os << "-->" << connection.child_->getConfiguration().transpose() << std::endl;

  os << connection.getParent()->getConfiguration().transpose()
     <<" ("
     <<connection.getParent()
     << ") --> "
     << connection.getChild()->getConfiguration().transpose()
     <<" ("
     <<connection.getChild()
     << ")"
     << " | cost: " << connection.cost_
     << " | net: "<<connection.is_net_;

  return os;
}

}

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

#include <graph_core/util.h>
#include <graph_core/graph/node.h>
namespace pathplan
{
class Connection : public std::enable_shared_from_this<Connection>
{
  /**
   * Add here your reserved flags.
   * Increment number_reserved_flags_ accordingly!
   * Initialize flags_ in the constructor accordingly!
   * If you need to modify or read these flags externally, implement getter and setter functions!
   */
  const static unsigned int idx_valid_ = 0;
  const static unsigned int idx_net_ = 1;
  const static unsigned int idx_recently_checked_ = 2;

  static const unsigned int number_reserved_flags_ = 3;

protected:
  NodeWeakPtr parent_;
  NodePtr child_;
  double cost_;
  double euclidean_norm_;
  double time_cost_update_;

  /**
   * @brief flags_ is a vector of flags. By default, the first three positions are reserved for valid flag, net flag and recently checked flag.
   * You can add new flags specific to your algorithm using function setFlag and passing the vector-index to store the flag.
   * getReservedFlagsNumber allows you to know how many positions are reserved for the defaults. setFlag doesn't allow you to overwrite these positions.
   * To overwrite them, you should use the flag-specific function (like setRecentlyChecked).
   */
  std::vector<bool> flags_;

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  Connection(const NodePtr& parent, const NodePtr& child, const bool is_net = false);

  ConnectionPtr pointer()
  {
    return shared_from_this();
  }

  bool isNet()
  {
    return flags_[idx_net_];
  }

  bool isRecentlyChecked()
  {
    return flags_[idx_recently_checked_];
  }

  void setRecentlyChecked(bool checked)
  {
    flags_[idx_recently_checked_] = checked;
  }

  bool isValid()
  {
    return flags_[idx_valid_];
  }

  void setCost(const double& cost)
  {
    cost_ = cost;
    time_cost_update_ = ros::WallTime::now().toSec();
  }
  const double& getCost()
  {
    return cost_;
  }

  const double& getTimeCostUpdate()
  {
    return time_cost_update_;
  }

  void setTimeCostUpdate(const double& time)
  {
    time_cost_update_ = time;
  }

  double norm()
  {
    return euclidean_norm_;
  }
  NodePtr getParent() const
  {
    assert(not parent_.expired());
    return parent_.lock();
  }
  NodePtr getChild() const
  {
    return child_;
  }

  /**
   * @brief setFlag sets your custome flag. Ue this function only if the flag was not created before because it creates a new one flag in flags_ vector
   * @param flag the NEW flag to set
   * @return the index of the flag in flags_ vector to use when you want to change the value
   */
  unsigned int setFlag(const bool flag);

  /**
   * @brief setFlag sets the custom flag at index idx. The flag should be already present in the flags_ vector.
   * If not, it add the new flag if and only if the index idx is equal to flags_ size
   * @param idx the index of the flag
   * @param flag the value of the flag
   * @return true if the flag is set correctly, flase otherwise
   */
  bool setFlag(const int& idx, const bool flag);

  /**
   * @brief getFlag returns the value of the flag at position idx. It returns the value if the flag exists, otherwise return the default value.
   * @param idx the index of the flag you are asking for.
   * @param default_value the default value returned if the flag doesn't exist.
   * @return the flag if it exists, the default value otherwise.
   */
  bool getFlag(const int& idx, const bool default_value);

  void add();
  void add(const bool is_net);
  void remove();
  void flip();
  bool convertToConnection();
  bool convertToNetConnection();
  void changeConnectionType();
  bool isParallel(const ConnectionPtr& conn, const double& toll = 1e-06);
  Eigen::VectorXd projectOnConnection(const Eigen::VectorXd& point, double& distance, bool& in_conn, const bool& verbose = false);

  /**
   * @brief getReservedFlagsNumber tells you how many positions are occupied by the defaults. Use this to know where you can sve your new flags.
   * @return the first free position in flags_ vector, so the idx next to the defaults.
   */
  static unsigned int getReservedFlagsNumber()
  {
    return number_reserved_flags_;
  }

  ~Connection();
  friend std::ostream& operator<<(std::ostream& os, const Connection& connection);
};

std::ostream& operator<<(std::ostream& os, const Connection& connection);

}

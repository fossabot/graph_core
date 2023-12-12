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


#include <graph_core/informed_sampler.h>

namespace pathplan
{

void InformedSampler::init()
{
  if(cost_ < 0.0)
  {
    CNR_FATAL(logger_,"cost should be >= 0");
    throw std::invalid_argument("cost should be >= 0");
  }

  ndof_ = lower_bound_.rows();

  if(start_configuration_.rows() != ndof_)
  {
    CNR_FATAL(logger_,"start configuration should have the same size of ndof");
    throw std::invalid_argument("start configuration should have the same size of ndof");
  }
  if(stop_configuration_.rows() != ndof_)
  {
    CNR_FATAL(logger_,"stop configuration should have the same size of ndof");
    throw std::invalid_argument("stop configuration should have the same size of ndof");
  }
  if(upper_bound_.rows() != ndof_)
  {
    CNR_FATAL(logger_,"upper bound should have the same size of ndof");
    throw std::invalid_argument("upper bound should have the same size of ndof");
  }
  if(lower_bound_.rows() != ndof_)
  {
    CNR_FATAL(logger_,"lower bound should have the same size of ndof");
    throw std::invalid_argument("lower bound should have the same size of ndof");
  }
  if(scale_.rows() != ndof_)
  {
    CNR_FATAL(logger_,"scale should have the same size of ndof");
    throw std::invalid_argument("scale should have the same size of ndof");
  }

  ud_ = std::uniform_real_distribution<double>(0, 1);

  inv_scale_=scale_.cwiseInverse();

  start_configuration_ = start_configuration_.cwiseProduct(scale_);
  stop_configuration_  = stop_configuration_ .cwiseProduct(scale_);

  lower_bound_ = lower_bound_.cwiseProduct(scale_);
  upper_bound_ = upper_bound_.cwiseProduct(scale_);

  ellipse_center_ = 0.5 * (start_configuration_ + stop_configuration_);
  focii_distance_ = (start_configuration_ - stop_configuration_).norm();
  center_bound_ = 0.5 * (lower_bound_ + upper_bound_);
  bound_width_ = 0.5 * (lower_bound_ - upper_bound_);
  ellipse_axis_.resize(ndof_);

  rot_matrix_ = computeRotationMatrix(start_configuration_, stop_configuration_);

  CNR_DEBUG(logger_,"rot_matrix_:\n" << rot_matrix_);
  CNR_DEBUG(logger_,"ellipse center" << ellipse_center_.transpose());
  CNR_DEBUG(logger_,"focii_distance_" << focii_distance_);
  CNR_DEBUG(logger_,"center_bound_" << center_bound_.transpose());
  CNR_DEBUG(logger_,"bound_width_" << bound_width_.transpose());

  if (cost_ < std::numeric_limits<double>::infinity())
  {
    inf_cost_ = false;
    setCost(cost_);
  }
  else
    inf_cost_ = true;
}

Eigen::MatrixXd InformedSampler::computeRotationMatrix(const Eigen::VectorXd& x1, const Eigen::VectorXd& x2)
{
  assert(x1.size() == x2.size());
  unsigned int dof = x1.size();
  Eigen::MatrixXd rot_matrix(dof, dof);
  rot_matrix.setIdentity();
  Eigen::VectorXd main_versor = (x1 - x2) / (x1 - x2).norm();

  bool is_standard_base = false;
  for (unsigned int ic = 0; ic < rot_matrix.cols(); ic++)
  {
    if (std::abs(main_versor.dot(rot_matrix.col(ic))) > 0.999)
    {
      is_standard_base = true;
      // rot_matrix is already orthonormal, put this direction as first
      Eigen::VectorXd tmp = rot_matrix.col(ic);
      rot_matrix.col(ic) = rot_matrix.col(0);
      rot_matrix.col(0) = tmp;
      break;
    }
  }

  if (!is_standard_base)
  {
    rot_matrix.col(0) = main_versor;
    // orthonormalization
    for (unsigned int ic = 1; ic < rot_matrix.cols(); ic++)
    {
      for (unsigned int il = 0; il < ic; il++)
      {
        rot_matrix.col(ic) -= (rot_matrix.col(ic).dot(rot_matrix.col(il))) * rot_matrix.col(il);
      }
      rot_matrix.col(ic) /= rot_matrix.col(ic).norm();
    }
  }
  return rot_matrix;
}

Eigen::VectorXd InformedSampler::sample()
{
  if(inf_cost_)
  {
    return center_bound_ + Eigen::MatrixXd::Random(ndof_, 1).cwiseProduct(bound_width_);
  }
  else
  {
    Eigen::VectorXd ball(ndof_);
    for (int iter = 0; iter < 100; iter++)
    {
      ball.setRandom();
      ball *= std::pow(ud_(gen_), 1.0 / (double)ndof_) / ball.norm();

      Eigen::VectorXd q = (rot_matrix_ * ellipse_axis_.asDiagonal() * ball) + ellipse_center_; //q_scaled

      bool in_of_bounds = true;
      for (unsigned int iax = 0; iax < ndof_; iax++)
      {
        if (q(iax) > upper_bound_(iax) || q(iax) < lower_bound_(iax))
        {
          in_of_bounds = false;
          break;
        }
      }

      if(in_of_bounds)
        return q.cwiseProduct(inv_scale_); //q = q_scaled/scale
    }
    //    ROS_DEBUG_THROTTLE(0.1, "unable to find a feasible point in the ellipse");

    return center_bound_ + Eigen::MatrixXd::Random(ndof_, 1).cwiseProduct(bound_width_);
  }
}

bool InformedSampler::inBounds(const Eigen::VectorXd& q)
{
  Eigen::VectorXd q_scaled = q.cwiseProduct(scale_);
  for (unsigned int iax = 0; iax < ndof_; iax++)
  {
    if (q_scaled(iax) > upper_bound_(iax) || q_scaled(iax) < lower_bound_(iax))
    {
      return false;
    }
  }
  if (inf_cost_)
    return true;
  else
    return ((q_scaled - start_configuration_).norm() + (q_scaled  - stop_configuration_).norm()) < cost_;

}

void InformedSampler::setCost(const double &cost)
{
  cost_ = cost;
  inf_cost_ = cost_ >= std::numeric_limits<double>::infinity();

  if (cost_ < focii_distance_)
  {
    CNR_WARN(logger_,"cost is "<<cost_<<" focci distance is "<< focii_distance_);
    CNR_WARN(logger_,"start_configuration: " << start_configuration_.transpose());
    CNR_WARN(logger_,"stop_configuration: " << stop_configuration_.transpose());
    cost_ = focii_distance_;
    min_radius_ = 0.0;
  }
  else
  {
    min_radius_ = 0.5 * std::sqrt(std::pow(cost_, 2) - std::pow(focii_distance_, 2));
  }
  max_radius_ = 0.5 * cost_;
  ellipse_axis_.setConstant(min_radius_);
  ellipse_axis_(0) = max_radius_;

  if (inf_cost_)
  {
    specific_volume_=std::tgamma(ndof_*0.5+1)/std::pow(M_PI,ndof_*0.5);  // inverse of the volume of unit ball

    for (unsigned int idx=0;idx<ndof_;idx++)
    {
      specific_volume_*=(upper_bound_(idx)-lower_bound_(idx));
    }
  }
  else
    specific_volume_=max_radius_*std::pow(min_radius_,ndof_-1);

  if (specific_volume_>0.0)
    specific_volume_=std::pow(specific_volume_,1./ndof_);
}

double InformedSampler::getSpecificVolume()
{
  return specific_volume_;
}

}

// Copyright 2023 Eugen Kaltenegger

#include <tuw_iwos_odometer/odometer_motor.h>

#include <algorithm>
#include <map>
#include <memory>

using tuw_iwos_odometer::OdometerMotor;
using tuw_iwos_tools::Side;

OdometerMotor::OdometerMotor(double wheelbase, double wheeloffset)
{
  this->wheelbase_ = wheelbase;
  this->wheeloffset_ = wheeloffset;

  this->icc_tool_ = std::make_unique<tuw_iwos_tools::IccTool>(this->wheelbase_, this->wheeloffset_, 0.0, 0.0, 0.0);

  this->revolute_velocity_ = std::make_shared<std::map<tuw_iwos_tools::Side, double>>();
  this->steering_position_ = std::make_shared<std::map<tuw_iwos_tools::Side, double>>();
  this->icc_pointer_ = std::make_shared<tuw::Point2D>(0.0, 0.0, 0.0);
  this->r_pointer_ = std::make_shared<std::map<tuw_iwos_tools::Side, double>>();
  this->v_pointer_ = std::make_shared<std::map<tuw_iwos_tools::Side, double>>();
  this->w_pointer_ = std::make_shared<std::map<tuw_iwos_tools::Side, double>>();
}

bool OdometerMotor::update(const sensor_msgs::JointStateConstPtr &this_joint_state)
{
  if (this->previous_joint_state_ == nullptr)
  {
    this->current_joint_state_ = this_joint_state;
    this->previous_joint_state_ = this_joint_state;
    return false;
  }
  else
  {
    this->current_joint_state_ = this_joint_state;
    this->update(this->previous_joint_state_,
                 this->current_joint_state_,
                 this->pose_);
    this->previous_joint_state_ = this_joint_state;
    return true;
  }
}

bool OdometerMotor::update(const sensor_msgs::JointStateConstPtr &joint_state_start,
                           const sensor_msgs::JointStateConstPtr &joint_state_end,
                           const std::shared_ptr<tuw::Pose2D> &pose_pointer)
{
  const sensor_msgs::JointStateConstPtr &joint_state = joint_state_end;

  // manage joint state
  std::shared_ptr<tuw_nav_msgs::JointsIWS> joints =
          tuw_iwos_tools::MessageTransformer::toJointsIWSPointer(*joint_state);

  (*this->revolute_velocity_)[tuw_iwos_tools::Side::LEFT] = joints->revolute[0];
  (*this->revolute_velocity_)[tuw_iwos_tools::Side::RIGHT] = joints->revolute[1];

  (*this->steering_position_)[tuw_iwos_tools::Side::LEFT] = joints->steering[0];
  (*this->steering_position_)[tuw_iwos_tools::Side::RIGHT] = joints->steering[1];

  // manage time
  ros::Time previous_time = joint_state_start->header.stamp;
  ros::Time current_time = joint_state_end->header.stamp;

  ros::Duration time = current_time - previous_time;

  double dt = time.toSec() / static_cast<double>(this->calculation_iterations_);

  // calculate pose
  try
  {
    this->icc_tool_->calculateIcc(this->revolute_velocity_,
                                  this->steering_position_,
                                  this->icc_pointer_,
                                  this->r_pointer_,
                                  this->v_pointer_,
                                  this->w_pointer_);
  }
  catch (...)
  {
    return false;
  }

  double v = this->v_pointer_->at(Side::CENTER);
  double w = this->w_pointer_->at(Side::CENTER);
  double x = pose_pointer->x();
  double y = pose_pointer->y();
  double theta = pose_pointer->theta();

  cv::Vec<double, 3> step{v * dt, 0.0, w * dt};
  cv::Vec<double, 3> pose{x, y, theta};
  cv::Matx<double, 3, 3> transform = cv::Matx<double, 3, 3>().eye();

  for (int i = 0; i < this->calculation_iterations_; i++)
  {
    transform(0, 0) = +cos(pose[2]);
    transform(0, 1) = -sin(pose[2]);
    transform(1, 0) = +sin(pose[2]);
    transform(1, 1) = +cos(pose[2]);
    pose = pose + (transform * step);
  }

  pose_pointer->set_x(pose[0]);
  pose_pointer->set_y(pose[1]);
  pose_pointer->set_theta(pose[2]);

  this->updateOdometerMessage(current_time);
  this->updateOdometerTransform(current_time);

  return true;
}

void OdometerMotor::updateOdometerMessage(ros::Time time)
{
  this->odometer_message_->header.seq = this->odometer_message_->header.seq + 1;
  this->odometer_message_->header.stamp = time;

  this->odometer_message_->pose.pose.position.x = this->pose_->x();
  this->odometer_message_->pose.pose.position.y = this->pose_->y();
  this->odometer_message_->pose.pose.orientation = tf::createQuaternionMsgFromYaw(this->pose_->theta());

  this->odometer_message_->twist.twist.linear.x = this->v_pointer_->at(Side::CENTER);
  this->odometer_message_->twist.twist.angular.z = this->w_pointer_->at(Side::CENTER);
}

void OdometerMotor::updateOdometerTransform(ros::Time time)
{
  this->transform_message_->header.seq = this->odometer_message_->header.seq + 1;
  this->odometer_message_->header.stamp = time;

  this->transform_message_->transform.translation.x = this->pose_->x();
  this->transform_message_->transform.translation.y = this->pose_->y();
  this->transform_message_->transform.rotation = tf::createQuaternionMsgFromYaw(this->pose_->theta());
}

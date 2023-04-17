// Copyright 2023 Eugen Kaltenegger

#ifndef ODOMETER_NODE_H
#define ODOMETER_NODE_H

#include <memory>

#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <ros/ros.h>
#include <tf/transform_broadcaster.h>

#include <tuw_iwos_odometer/encoder_odometer.h>
#include <tuw_iwos_odometer/mixed_odometer.h>

using message_filters::sync_policies::ApproximateTime;
using message_filters::Synchronizer;

namespace tuw_iwos_odometer
{
class OdometerNode
{
public:
  OdometerNode();
  ~OdometerNode() = default;
  void run();
  void updateEncoderOdometer(const sensor_msgs::JointStateConstPtr& joint_state);
  void synchronizedUpdateMixedOdometer(const sensor_msgs::JointStateConstPtr& joint_state,
                                       const sensor_msgs::ImuConstPtr& imu);
private:
  std::shared_ptr<ros::NodeHandle> node_handle_;

  std::unique_ptr<message_filters::Subscriber<sensor_msgs::JointState>> encoder_subscriber_;
  std::unique_ptr<message_filters::Subscriber<sensor_msgs::Imu>> raw_imu_subscriber_;
  std::unique_ptr<message_filters::Subscriber<sensor_msgs::Imu>> rpy_imu_subscriber_;

  // atp: Approximate Time Policy
  typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::JointState, sensor_msgs::Imu> atp;
  // ats: Approximate Time Synchronizer
  typedef message_filters::Synchronizer<atp> ats;
  boost::shared_ptr<ats> ats_;

  std::unique_ptr<EncoderOdometer> encoder_odometer_;
  std::unique_ptr<MixedOdometer> mixed_odometer_;
};
}  // namespace tuw_iwos_odometer

#endif  // ODOMETER_NODE_H

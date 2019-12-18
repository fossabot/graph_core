#include <human_probablistic_occupancy/human_probablistic_occupancy.h>
#include <subscription_notifier/subscription_notifier.h>
#include <rosdyn_core/primitives.h>

int main(int argc, char **argv)
{
  ros::init(argc, argv, "human_occupancy");
  ros::NodeHandle nh;
  ros::AsyncSpinner spinner(4);
  spinner.start();

  ros::Publisher pc_pub=nh.advertise<sensor_msgs::PointCloud>("occupancy",1);

  Eigen::Vector3d x_min;
  Eigen::Vector3d x_max;
  unsigned int npnt=50;
  x_min.setConstant(-3);
  x_max.setConstant(3);
  human_occupancy::OccupancyGrid grid(x_min,x_max,npnt);

  double t=0;
  double st=1./12.5;
  ros::Rate lp(1./st);

  std::vector<double> center(3);
  double radius;
  if (!nh.getParam("occ_center",center))
  {
    center.at(0)=1;
    center.at(1)=2;
    center.at(2)=1;
  }
  radius=0.5;
  if (!nh.getParam("occ_radius",radius))
  {
    radius=0.5;
  }

  bool store=true;
  double varying=0;
  while (ros::ok())
  {
    double tmp_radius=(0.9+0.1*varying)*radius;
    varying+=0.5*st;
    if (varying>1)
      varying=0;

    geometry_msgs::PoseArray array;
    for (double r=0;r<=tmp_radius;r+=0.05*tmp_radius)
    {
      for (double a1=0;a1<2*M_PI;a1+=0.05*M_PI)
      {
        for (double a2=0;a2<M_PI;a2+=0.05*M_PI)
        {
          geometry_msgs::Pose pose;
          pose.position.x=center.at(0)+r*std::sin(a1)*std::cos(a2);
          pose.position.y=center.at(1)+r*std::sin(a1)*std::sin(a2);
          pose.position.z=center.at(2)+r*std::cos(a1);
          array.poses.push_back(pose);
        }
      }
    }

    grid.update(array);
    pc_pub.publish(grid.toPointCloud());

    t+=st;
    lp.sleep();
    if (t>5 && store)
    {
      grid.toYaml(nh);
      store=false;;
    }
  }

  return 0;
}

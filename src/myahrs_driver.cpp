//------------------------------------------------------------------------------
#include <myahrs_driver/myahrs_driver.hpp>

#include <sensor_msgs/Imu.h>
#include <tf/transform_broadcaster.h>

//------------------------------------------------------------------------------
using namespace WithRobot;

//------------------------------------------------------------------------------
static const int BAUDRATE = 115200;
static const char* DIVIDER = "1";  // 100 Hz

#define DEG2RAD (M_PI/180.0)
//------------------------------------------------------------------------------
void handle_error(const char* error_msg)
{
  fprintf(stderr, "ERROR: %s\n", error_msg);
  exit(1);
}

//------------------------------------------------------------------------------
class UserDefinedAhrs : public iMyAhrsPlus
{
private:
  ros::NodeHandle nh;
  ros::NodeHandle nh_priv;

  void OnSensorData(int sensor_id, SensorData data)
  {
    sample_count++;
    {
      LockGuard _l(lock);
      sensor_data = data;
      sensor_data.euler_angle = sensor_data.quaternion.to_euler_angle();
    }

    do_something(sensor_id);
  }

  void OnAttributeChange(int sensor_id, std::string attribute_name, std::string value)
  {
    printf("OnAttributeChange(id %d, %s, %s)\n", sensor_id, attribute_name.c_str(), value.c_str());
  }

public:
  int sample_count;
  ros::Publisher data_pub;
  sensor_msgs::Imu imu_data;
  Platform::Mutex lock;
  SensorData sensor_data;
  tf::TransformBroadcaster broadcaster;

  UserDefinedAhrs(std::string port="", unsigned int baudrate=115200)
  : iMyAhrsPlus(port, baudrate),
    sample_count(0),
    nh_priv("~")
  {
    data_pub = nh_priv.advertise<sensor_msgs::Imu>("imu_publisher", 10);
  }

  ~UserDefinedAhrs()
  {}

  bool initialize()
  {
    bool ok = false;

    do
    {
      if(start() == false) break;
      if(cmd_binary_data_format("QUATERNION, IMU") == false) break;
      if(cmd_divider(DIVIDER) == false) break;
      if(cmd_mode("BC") == false) break;
      ok = true;
    } while(0);

    return ok;
  }

  inline void get_data(SensorData& data)
  {
    LockGuard _l(lock);
    data = sensor_data;
  }

  inline SensorData get_data()
  {
    LockGuard _l(lock);
    return sensor_data;
  }

  void do_something(int sensor_id)
  {
    std::string line(50, '-');
    printf("%s\n", line.c_str());

    Quaternion& q = sensor_data.quaternion;
    EulerAngle& e = sensor_data.euler_angle;
    ImuData<float>& imu = sensor_data.imu;

    printf("%04d) sensor_id %d, Quaternion(xyzw)=%.4f,%.4f,%.4f,%.4f, Angle(rpy)=%.1f, %.1f, %.1f, Accel(xyz)=%.4f,%.4f,%.4f, Gyro(xyz)=%.4f,%.4f,%.4f, Magnet(xyz)=%.2f,%.2f,%.2f\n",
      sample_count,
      sensor_id,
      q.x, q.y, q.z, q.w,
      e.roll, e.pitch, e.yaw,
      imu.ax, imu.ay, imu.az,
      imu.gx, imu.gy, imu.gz,
      imu.mx, imu.my, imu.mz);

    ros::Time now = ros::Time::now();
    sensor_msgs::Imu imu_msg;
    imu_msg.header.stamp = now;

    imu_msg.header.frame_id = "imu_base";

    imu_msg.orientation.x = q.x;
    imu_msg.orientation.y = q.y;
    imu_msg.orientation.z = q.z;
    imu_msg.orientation.w = q.w;

    imu_msg.angular_velocity.x = imu.gx * DEG2RAD;
    imu_msg.angular_velocity.y = imu.gy * DEG2RAD;
    imu_msg.angular_velocity.z = imu.gz * DEG2RAD;

    imu_msg.linear_acceleration.x = imu.ax * 9.80665;
    imu_msg.linear_acceleration.y = imu.ay * 9.80665;
    imu_msg.linear_acceleration.z = imu.az * 9.80665;

    data_pub.publish(imu_msg);

    broadcaster.sendTransform(tf::StampedTransform(tf::Transform(tf::createQuaternionFromRPY(e.roll, -e.pitch, -e.yaw),
                                                                 tf::Vector3(0.0, 0.0, 0.1)),
                                                   ros::Time::now(), "imu_base", "imu"));
  }
};

//------------------------------------------------------------------------------
void test(const char* serial_device, int baudrate)
{
  printf("\n### %s() ###\n", __FUNCTION__);

  UserDefinedAhrs sensor(serial_device, baudrate);

  if(sensor.initialize() == false)
  {
    handle_error("initialize() returns false");
  }

  while(1)
  {
    Platform::msleep(100);
  }

  printf("END OF TEST(%s)\n\n", __FUNCTION__);
}

//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  ros::init(argc, argv, "myahrs_driver");

  test("/dev/ttyACM1", BAUDRATE);

//  ros::spin();
  return 0;
}

//------------------------------------------------------------------------------

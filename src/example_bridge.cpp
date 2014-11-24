#include <queue>

#include <boost/thread.hpp>

#include <ros/ros.h>
#include <sensor_msgs/JointState.h>


template<class T>
class MyBuffer {
  public:
    std::queue<T> m_queue;
    boost::mutex m_mutex;
    boost::condition_variable m_condition;

    void push(const T& item) {
      {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        m_queue.push(item);
      }
      m_condition.notify_one();
    }

    T pop() {
      T item;
      {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        item = m_queue.front();
        m_queue.pop();
      }
      return item;
    }

    void waitForItem() {
      boost::unique_lock<boost::mutex> lock(m_mutex);
      while(m_queue.size() == 0) {
        m_condition.wait(lock);
      }
    }
};


/* NOTE: real bridge should be written in good OOP */
MyBuffer<double> fromOtherBuffer;
MyBuffer<double> toOtherBuffer;
ros::Publisher pub;



void
jointsCallback(const sensor_msgs::JointState::ConstPtr& msg) {
  ROS_INFO_STREAM("Subscribe from ROS:\n" /* << *msg */);

  toOtherBuffer.push(msg->position[0]);
}


void
inputFromOtherRun()
{
  while (true) {
    std::cout << "Input from other: ";
    double val;
    std::cin >> val;

    fromOtherBuffer.push(val);
  }
}


void
outputToOtherRun()
{
  while (true) {
    toOtherBuffer.waitForItem();
    double val = toOtherBuffer.pop();
    std::cout << "Output to other: " << val << std::endl;
  }
}


void
outputToRosRun()
{
  while (true) {
    fromOtherBuffer.waitForItem();
    double val = fromOtherBuffer.pop();
    ROS_INFO_STREAM("Publish to ROS:\n" /* << *msg */);
    sensor_msgs::JointState msg;
    msg.position.push_back(val);
    pub.publish(msg);
  }
}


int
main(int argc, char** argv)
{
  ros::init(argc, argv, "example_bridge");
  ros::NodeHandle nh;

  boost::shared_ptr<boost::thread> inputFromOtherThread = boost::make_shared<boost::thread>(inputFromOtherRun);
  boost::shared_ptr<boost::thread> outputToOtherThread = boost::make_shared<boost::thread>(outputToOtherRun);
  boost::shared_ptr<boost::thread> outputToRosThread = boost::make_shared<boost::thread>(outputToRosRun);

  ros::Subscriber sub = nh.subscribe<sensor_msgs::JointState>("set_joints", 1, jointsCallback);
  pub = nh.advertise<sensor_msgs::JointState>("get_joints", 1);

  ROS_INFO("Spinning");
  ros::spin();
}

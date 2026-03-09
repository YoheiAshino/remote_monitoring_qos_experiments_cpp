#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class TelemetryPublisher : public rclcpp::Node
{
public:
  TelemetryPublisher() : Node("telemetry_publisher"),
    fast_count_(0), cmd_count_(0), diag_count_(0)
  {
    // QoSはまず固定（Day20-2で切替）
    auto fast_qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    auto cmd_qos  = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    auto diag_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();

    pub_fast_ = create_publisher<geometry_msgs::msg::Vector3Stamped>("/telemetry/fast_state", fast_qos);
    pub_cmd_  = create_publisher<geometry_msgs::msg::Vector3Stamped>("/cmd/teleop", cmd_qos);
    pub_diag_ = create_publisher<std_msgs::msg::String>("/telemetry/diagnostics", diag_qos);

    // 周期：fast=10ms, cmd=50ms, diag=1000ms
    t_fast_ = create_wall_timer(10ms, std::bind(&TelemetryPublisher::tick_fast, this));
    t_cmd_  = create_wall_timer(50ms, std::bind(&TelemetryPublisher::tick_cmd, this));
    t_diag_ = create_wall_timer(1000ms, std::bind(&TelemetryPublisher::tick_diag, this));

    RCLCPP_INFO(get_logger(), "telemetry_publisher started");
  }

private:
  void tick_fast()
  {
    geometry_msgs::msg::Vector3Stamped msg;
    msg.header.stamp = now();
    msg.header.frame_id = "base_link";  // 監視例なので意味は薄いが“それっぽく”
    msg.vector.x = static_cast<double>(fast_count_++);  // 値
    msg.vector.y = 0.0;
    msg.vector.z = 0.0;
    pub_fast_->publish(msg);
  }

  void tick_cmd()
  {
    geometry_msgs::msg::Vector3Stamped msg;
    msg.header.stamp = now();
    msg.header.frame_id = "remote_operator";
    msg.vector.x = static_cast<double>(cmd_count_++);  // コマンドID
    msg.vector.y = 0.0;
    msg.vector.z = 0.0;
    pub_cmd_->publish(msg);
  }

  void tick_diag()
  {
    std_msgs::msg::String msg;
    msg.data = "diag#" + std::to_string(diag_count_++) + " OK";
    pub_diag_->publish(msg);
  }

  int fast_count_, cmd_count_, diag_count_;
  rclcpp::Publisher<geometry_msgs::msg::Vector3Stamped>::SharedPtr pub_fast_;
  rclcpp::Publisher<geometry_msgs::msg::Vector3Stamped>::SharedPtr pub_cmd_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_diag_;
  rclcpp::TimerBase::SharedPtr t_fast_, t_cmd_, t_diag_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TelemetryPublisher>());
  rclcpp::shutdown();
  return 0;
}
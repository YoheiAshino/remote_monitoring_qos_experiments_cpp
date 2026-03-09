#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/vector3_stamped.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

static double latency_ms(const rclcpp::Time & sent, const rclcpp::Time & recv)
{
  return (recv - sent).seconds() * 1000.0;
}

class RemoteMonitor : public rclcpp::Node
{
public:
  RemoteMonitor() : Node("remote_monitor"),
    fast_delay_ms_(this->declare_parameter<int>("fast_delay_ms", 0)),
    cmd_delay_ms_(this->declare_parameter<int>("cmd_delay_ms", 0)),
    diag_delay_ms_(this->declare_parameter<int>("diag_delay_ms", 0))
  {
    // QoSはまずpublisherと合わせて固定（Day20-2で切替）
    auto fast_qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
    auto cmd_qos  = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
    auto diag_qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();

    sub_fast_ = create_subscription<geometry_msgs::msg::Vector3Stamped>(
      "/telemetry/fast_state", fast_qos,
      std::bind(&RemoteMonitor::on_fast, this, std::placeholders::_1));

    sub_cmd_ = create_subscription<geometry_msgs::msg::Vector3Stamped>(
      "/cmd/teleop", cmd_qos,
      std::bind(&RemoteMonitor::on_cmd, this, std::placeholders::_1));

    sub_diag_ = create_subscription<std_msgs::msg::String>(
      "/telemetry/diagnostics", diag_qos,
      std::bind(&RemoteMonitor::on_diag, this, std::placeholders::_1));

    RCLCPP_INFO(get_logger(),
      "remote_monitor started (delays ms: fast=%d cmd=%d diag=%d)",
      fast_delay_ms_, cmd_delay_ms_, diag_delay_ms_);
  }

private:
  void on_fast(const geometry_msgs::msg::Vector3Stamped::SharedPtr msg)
{
  if (fast_delay_ms_ > 0) std::this_thread::sleep_for(std::chrono::milliseconds(fast_delay_ms_));

  int id = static_cast<int>(msg->vector.x);
  if (last_fast_id_ != -1 && id != last_fast_id_ + 1) {
    RCLCPP_WARN(get_logger(), "FAST DROP? expected=%d got=%d", last_fast_id_ + 1, id);
  }
  last_fast_id_ = id;

  auto recv = now();
  const double lat = latency_ms(msg->header.stamp, recv);
  RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 500,
    "FAST  id=%d latency=%.2fms", id, lat);
}

  void on_cmd(const geometry_msgs::msg::Vector3Stamped::SharedPtr msg)
{
  if (cmd_delay_ms_ > 0) std::this_thread::sleep_for(std::chrono::milliseconds(cmd_delay_ms_));

  int id = static_cast<int>(msg->vector.x);
  if (last_cmd_id_ != -1 && id != last_cmd_id_ + 1) {
    RCLCPP_WARN(get_logger(), "CMD DROP? expected=%d got=%d", last_cmd_id_ + 1, id);
  }
  last_cmd_id_ = id;

  auto recv = now();
  const double lat = latency_ms(msg->header.stamp, recv);
  RCLCPP_INFO(get_logger(), "CMD   id=%d latency=%.2fms", id, lat);
}

  void on_diag(const std_msgs::msg::String::SharedPtr msg)
  {
    if (diag_delay_ms_ > 0) std::this_thread::sleep_for(std::chrono::milliseconds(diag_delay_ms_));

    RCLCPP_INFO(get_logger(), "DIAG  %s", msg->data.c_str());
  }

  int fast_delay_ms_, cmd_delay_ms_, diag_delay_ms_;
  int last_fast_id_ = -1;
  int last_cmd_id_  = -1;
  rclcpp::Subscription<geometry_msgs::msg::Vector3Stamped>::SharedPtr sub_fast_;
  rclcpp::Subscription<geometry_msgs::msg::Vector3Stamped>::SharedPtr sub_cmd_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_diag_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RemoteMonitor>());
  rclcpp::shutdown();
  return 0;
}
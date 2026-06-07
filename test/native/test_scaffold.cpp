#include <unity.h>
#include "queue_types.h"
#include "device_config.h"
#include "mqtt_topics.h"

void setUp(void) {}
void tearDown(void) {}

void test_device_id_is_defined() {
  TEST_ASSERT(strlen(device_config::kDeviceId) > 0);
}

void test_mqtt_prefix_is_defined() {
  TEST_ASSERT(strlen(device_config::kMqttTopicPrefix) > 0);
}

void test_command_message_size() {
  // CommandMessage should be small enough for the 8-slot queue
  TEST_ASSERT(sizeof(CommandMessage) <= 256);
}

void test_event_message_size() {
  // EventMessage should be small enough for the 16-slot queue
  TEST_ASSERT(sizeof(EventMessage) <= 256);
}

void test_command_subscribe_topic() {
  char buf[128];
  mqtt_topics::commandSubscribeAll(buf);
  TEST_ASSERT(strlen(buf) > 0);
  // Should contain the device ID and /#
  TEST_ASSERT(strstr(buf, device_config::kDeviceId) != nullptr);
  TEST_ASSERT(strstr(buf, "/#") != nullptr);
}

void test_event_heartbeat_topic() {
  char buf[128];
  mqtt_topics::eventHeartbeat(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "heartbeat") != nullptr);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_device_id_is_defined);
  RUN_TEST(test_mqtt_prefix_is_defined);
  RUN_TEST(test_command_message_size);
  RUN_TEST(test_event_message_size);
  RUN_TEST(test_command_subscribe_topic);
  RUN_TEST(test_event_heartbeat_topic);

  return UNITY_END();
}

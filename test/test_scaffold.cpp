#include <unity.h>
#include <string.h>
#include "queue_types.h"
#include "device_config.h"
#include "mqtt_topics.h"

void setUp(void) {}
void tearDown(void) {}

// ── Device config ────────────────────────────────────────────────────────────

void test_device_id_is_defined() {
  TEST_ASSERT(strlen(device_config::kDeviceId) > 0);
  TEST_ASSERT(strlen(device_config::kDeviceId) <= 64);
}

void test_mqtt_prefix_is_defined() {
  TEST_ASSERT(strlen(device_config::kMqttTopicPrefix) > 0);
  TEST_ASSERT(strlen(device_config::kMqttTopicPrefix) <= 32);
}

void test_timezone_offset_is_valid() {
  // Must be between -12 and +14 (valid UTC offsets)
  TEST_ASSERT(device_config::kTimezoneOffsetHours >= -12);
  TEST_ASSERT(device_config::kTimezoneOffsetHours <= 14);
  // Derived seconds must match
  TEST_ASSERT(device_config::kTimezoneOffsetSeconds ==
              (long)device_config::kTimezoneOffsetHours * 3600L);
}

void test_default_brightness_in_range() {
  TEST_ASSERT(device_config::kDefaultBrightness <= 100);
}

// ── Queue struct sizes ───────────────────────────────────────────────────────

void test_command_message_size() {
  // CommandMessage should be small enough for the 8-slot queue
  TEST_ASSERT(sizeof(CommandMessage) <= 256);
}

void test_event_message_size() {
  // EventMessage should be small enough for the 16-slot queue
  TEST_ASSERT(sizeof(EventMessage) <= 256);
}

void test_event_message_union_member_sizes() {
  // Ensure no single event variant blows the union
  EventMessage evt;
  static_assert(sizeof(evt) <= 256, "EventMessage exceeds 256 bytes");
}

// ── Topic building ───────────────────────────────────────────────────────────

void test_command_subscribe_topic() {
  char buf[128];
  mqtt_topics::commandSubscribeAll(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, device_config::kDeviceId) != nullptr);
  TEST_ASSERT(strstr(buf, "/#") != nullptr);
}

void test_event_heartbeat_topic() {
  char buf[128];
  mqtt_topics::eventHeartbeat(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "heartbeat") != nullptr);
  TEST_ASSERT(strstr(buf, device_config::kDeviceId) != nullptr);
}

void test_event_alarm_triggered_topic() {
  char buf[128];
  mqtt_topics::eventAlarmTriggered(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "alarm_triggered") != nullptr);
}

void test_event_alarm_acknowledged_topic() {
  char buf[128];
  mqtt_topics::eventAlarmAcknowledged(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "alarm_acknowledged") != nullptr);
}

void test_event_command_result_topic() {
  char buf[128];
  mqtt_topics::eventCommandResult(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "command_result") != nullptr);
}

void test_state_presence_topic() {
  char buf[128];
  mqtt_topics::statePresence(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "presence") != nullptr);
  TEST_ASSERT(strstr(buf, device_config::kDeviceId) != nullptr);
}

void test_state_display_topic() {
  char buf[128];
  mqtt_topics::stateDisplay(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "display") != nullptr);
}

void test_state_alarm_topic() {
  char buf[128];
  mqtt_topics::stateAlarm(buf);
  TEST_ASSERT(strlen(buf) > 0);
  TEST_ASSERT(strstr(buf, "alarm") != nullptr);
}

void test_topic_buffer_does_not_overflow() {
  // The longest possible topic is:
  // clocks/events/{deviceId}/alarm_acknowledged
  // = 6 + 1 + 6 + 1 + 64 + 1 + 18 + 1 = ~98 chars max
  // Verify with the configured device ID
  char buf[128];
  mqtt_topics::eventAlarmAcknowledged(buf);
  TEST_ASSERT(strlen(buf) < 128);
  mqtt_topics::statePresence(buf);
  TEST_ASSERT(strlen(buf) < 128);
}

// ── Command message field sizes ──────────────────────────────────────────────

void test_alarm_label_bounds() {
  CommandMessage cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.type = CommandMessage::Type::kSetAlarm;
  cmd.alarmTimeUtc = 2000000000; // some future time
  // Fill with max content
  memset(cmd.alarmLabel, 'A', sizeof(cmd.alarmLabel) - 1);
  cmd.alarmLabel[sizeof(cmd.alarmLabel) - 1] = '\0';
  TEST_ASSERT(strlen(cmd.alarmLabel) == sizeof(cmd.alarmLabel) - 1);
}

void test_message_field_bounds() {
  CommandMessage cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.type = CommandMessage::Type::kDisplayMessage;
  memset(cmd.message, 'M', sizeof(cmd.message) - 1);
  cmd.message[sizeof(cmd.message) - 1] = '\0';
  TEST_ASSERT(strlen(cmd.message) == sizeof(cmd.message) - 1);
  TEST_ASSERT(cmd.durationSeconds == 0);
  cmd.durationSeconds = 300;
  TEST_ASSERT(cmd.durationSeconds == 300);
}

// ── Enum consistency ─────────────────────────────────────────────────────────

void test_command_message_types_are_distinct() {
  // Verify no unintentional overlap
  CommandMessage::Type alarm     = CommandMessage::Type::kSetAlarm;
  CommandMessage::Type message   = CommandMessage::Type::kDisplayMessage;
  CommandMessage::Type brightness = CommandMessage::Type::kSetBrightness;
  TEST_ASSERT(alarm != message);
  TEST_ASSERT(alarm != brightness);
  TEST_ASSERT(message != brightness);
}

void test_event_message_types_are_distinct() {
  EventMessage::Type hb     = EventMessage::Type::kHeartbeat;
  EventMessage::Type cr     = EventMessage::Type::kCommandResult;
  EventMessage::Type at     = EventMessage::Type::kAlarmTriggered;
  EventMessage::Type aa     = EventMessage::Type::kAlarmAcknowledged;
  EventMessage::Type ds     = EventMessage::Type::kDisplayState;
  EventMessage::Type as_    = EventMessage::Type::kAlarmState;
  TEST_ASSERT(hb != cr && hb != at && hb != aa && hb != ds && hb != as_);
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_device_id_is_defined);
  RUN_TEST(test_mqtt_prefix_is_defined);
  RUN_TEST(test_timezone_offset_is_valid);
  RUN_TEST(test_default_brightness_in_range);

  RUN_TEST(test_command_message_size);
  RUN_TEST(test_event_message_size);
  RUN_TEST(test_event_message_union_member_sizes);

  RUN_TEST(test_command_subscribe_topic);
  RUN_TEST(test_event_heartbeat_topic);
  RUN_TEST(test_event_alarm_triggered_topic);
  RUN_TEST(test_event_alarm_acknowledged_topic);
  RUN_TEST(test_event_command_result_topic);
  RUN_TEST(test_state_presence_topic);
  RUN_TEST(test_state_display_topic);
  RUN_TEST(test_state_alarm_topic);
  RUN_TEST(test_topic_buffer_does_not_overflow);

  RUN_TEST(test_alarm_label_bounds);
  RUN_TEST(test_message_field_bounds);

  RUN_TEST(test_command_message_types_are_distinct);
  RUN_TEST(test_event_message_types_are_distinct);

  return UNITY_END();
}

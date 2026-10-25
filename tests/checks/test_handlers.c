#include "test.h"

static void queue_fifo_and_overflow(void) {
  BLB_Handlers *handlers = calloc(1, sizeof(*handlers));

  TEST_REQUIRE(handlers);
  BLB_HandlersSetMode(handlers, BLB_HANDLER_POLL);
  TEST_CHECK(BLB_HandlersGetMode(handlers) == BLB_HANDLER_POLL);

  for (size_t i = 0; i < BLB_HANDLER_EVENT_QUEUE_CAPACITY + 7; ++i) {
    BLB_HandlerEvent event = {.type = BLB_EVENT_TEXT};

    event.data.text.codepoint = (uint32_t)i;
    BLB_HandlersQueueEvent(handlers, event);
  }

  TEST_CHECK(BLB_HandlersPendingEvents(handlers) == BLB_HANDLER_EVENT_QUEUE_CAPACITY);
  TEST_CHECK(BLB_HandlersDroppedEvents(handlers) == 7);
  BLB_HandlerEvent event;

  TEST_CHECK(BLB_HandlersNextEvent(handlers, &event));
  TEST_CHECK(event.data.text.codepoint == 7 && event.serial == 8);
  uint64_t serial = event.serial;

  while (BLB_HandlersNextEvent(handlers, &event)) {
    TEST_CHECK(event.serial == ++serial);
  }
  TEST_CHECK(BLB_HandlersPendingEvents(handlers) == 0);
  TEST_CHECK(!BLB_HandlersNextEvent(handlers, &event));
  free(handlers);
}

static void count_event(const BLB_HandlerEvent *event, void *userdata) {
  if (event->type == BLB_EVENT_TEXT)
    ++*(int *)userdata;
}

static void callback_registration(void) {
  BLB_Handlers *handlers = calloc(1, sizeof(*handlers));

  TEST_REQUIRE(handlers);
  int received = 0;

  TEST_CHECK(BLB_HandlersSetCallback(handlers, BLB_EVENT_TEXT, count_event, &received));
  BLB_HandlersQueueEvent(handlers, (BLB_HandlerEvent){.type = BLB_EVENT_TEXT});
  BLB_HandlersProcess(handlers);
  TEST_CHECK(received == 1);
  BLB_HandlersClearCallbacks(handlers, BLB_EVENT_TEXT);
  BLB_HandlersQueueEvent(handlers, (BLB_HandlerEvent){.type = BLB_EVENT_TEXT});
  BLB_HandlersProcess(handlers);
  TEST_CHECK(received == 1);
  free(handlers);
}

static const Test_Case cases[] = {TEST_CASE(queue_fifo_and_overflow), TEST_CASE(callback_registration)};
const Test_Suite test_handlers = {"handlers", cases, sizeof(cases) / sizeof(cases[0])};

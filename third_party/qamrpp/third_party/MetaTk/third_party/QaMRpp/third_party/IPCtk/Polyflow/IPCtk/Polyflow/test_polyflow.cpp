#include "Polyflow.hpp"

#include <cassert>
#include <chrono>
#include <thread>

int main() {
  using namespace polyflow;

  task_graph g;

  auto t1 = std::make_shared<task_base>(1, task_metadata{"seed", priority::high, std::nullopt},
    [](cancellation_token&) -> zethamem::Value { return std::int64_t{5}; });

  auto t2 = std::make_shared<task_base>(2, task_metadata{"double", priority::normal, std::nullopt},
    [](cancellation_token&) -> zethamem::Value { return std::int64_t{10}; });
  t2->add_dependency(1);

  auto t3 = t2->then(3, task_metadata{"plus1", priority::normal, std::nullopt},
    [](const zethamem::Value& in, cancellation_token&) -> zethamem::Value {
      return std::get<std::int64_t>(in) + 1;
    });

  g.add_task(t1);
  g.add_task(t2);
  g.add_task(t3);

  auto order = g.schedule();
  assert(order.size() == 3);

  execution_context ctx(std::move(g), 2);
  ctx.run_all();

  auto v3 = ctx.wait(3);
  assert(std::get<std::int64_t>(v3) == 11);

  auto t4 = std::make_shared<task_base>(4, task_metadata{"cancelled", priority::low, std::nullopt},
    [](cancellation_token& tok) -> zethamem::Value {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      if (tok.is_cancelled()) throw task_error("cancelled");
      return std::int64_t{1};
    });
  t4->cancel();
  assert(t4->state() == task_state::cancelled);

  return 0;
}

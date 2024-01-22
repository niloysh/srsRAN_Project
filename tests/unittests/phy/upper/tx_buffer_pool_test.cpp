/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "srsran/phy/upper/log_likelihood_ratio.h"
#include "srsran/phy/upper/tx_buffer_pool.h"
#include "srsran/phy/upper/unique_tx_buffer.h"
#include "srsran/support/executors/task_worker.h"
#include "srsran/support/executors/task_worker_pool.h"
#include <gtest/gtest.h>

using namespace srsran;

namespace srsran {

bool operator==(span<const bool> left, span<const bool> right)
{
  return std::equal(left.begin(), left.end(), right.begin(), right.end());
}

} // namespace srsran

// Tests that the pool returns nullptr when the limit of buffers is reached.
TEST(tx_buffer_pool, buffer_limit)
{
  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 16;
  pool_config.nof_buffers          = 4;
  pool_config.nof_codeblocks       = 4;
  pool_config.expire_timeout_slots = 10;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Create as many buffers as the limit is set.
  std::vector<unique_tx_buffer> buffers;
  for (unsigned rnti = 0; rnti != pool_config.nof_buffers; ++rnti) {
    trx_buffer_identifier buffer_id(rnti, 0);

    // Reserve buffer, it shall not fail.
    buffers.emplace_back(pool->get_pool().reserve(slot, buffer_id, 1));
    ASSERT_TRUE(buffers.back().is_valid());
  }

  // Create one more buffer. No buffers are available. It must fail to reserve.
  trx_buffer_identifier buffer_id(static_cast<uint16_t>(pool_config.nof_buffers), 0);
  ASSERT_FALSE(pool->get_pool().reserve(slot, buffer_id, 1).is_valid());
}

// Tests that the pool returns nullptr when the limit of codeblocks is reached.
TEST(tx_buffer_pool, codeblock_limit)
{
  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 16;
  pool_config.nof_buffers          = 2;
  pool_config.nof_codeblocks       = 1;
  pool_config.expire_timeout_slots = 10;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Reserve buffer with all the codeblocks, it shall not fail.
  trx_buffer_identifier buffer_id0(0x1234, 0x3);
  unique_tx_buffer      buffer = pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks);
  ASSERT_TRUE(buffer.is_valid());

  // Create one more buffer. No codeblocks are available. It must fail to reserve.
  trx_buffer_identifier buffer_id1(0x1234, buffer_id0.get_harq() + 1);
  ASSERT_FALSE(pool->get_pool().reserve(slot, buffer_id1, pool_config.nof_codeblocks).is_valid());
}

// Tests that the pool frees reserved buffer.
TEST(tx_buffer_pool, buffer_free)
{
  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 16;
  pool_config.nof_buffers          = 1;
  pool_config.nof_codeblocks       = 1;
  pool_config.expire_timeout_slots = 10;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Reserve buffer with all the codeblocks, it shall not fail.
  trx_buffer_identifier buffer_id0(0x1234, 0x3);
  unique_tx_buffer      buffer = pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks);
  ASSERT_TRUE(buffer.is_valid());

  // Unlock buffer. It is still reserved.
  buffer = unique_tx_buffer();

  // Reserve buffer with the same identifier. It shall not fail.
  buffer = pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks);
  ASSERT_TRUE(buffer.is_valid());

  // Reserve buffer with a different identifier. It shall fail.
  trx_buffer_identifier buffer_id1(0x1234, buffer_id0.get_harq() + 1);
  ASSERT_FALSE(pool->get_pool().reserve(slot, buffer_id1, pool_config.nof_codeblocks).is_valid());

  // Free the first buffer identifier.
  buffer = unique_tx_buffer();

  // Run slot for clearing the buffer.
  pool->get_pool().run_slot(slot);

  // Reserve buffer with all the codeblocks, it shall not fail.
  buffer = pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks);
  ASSERT_TRUE(buffer.is_valid());
}

// Tests that the pool expires buffers after the last reserved slot.
TEST(tx_buffer_pool, buffer_expire)
{
  unsigned delay = 3;

  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 16;
  pool_config.nof_buffers          = 1;
  pool_config.nof_codeblocks       = 1;
  pool_config.expire_timeout_slots = 4;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Reserve buffer with all the codeblocks, it shall not fail.
  trx_buffer_identifier buffer_id0(0x1234, 0x3);
  ASSERT_TRUE(pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks).is_valid());

  // Run slot and reserve the same buffer.
  slot += delay;
  pool->get_pool().run_slot(slot);
  ASSERT_TRUE(pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks).is_valid());

  // Run for each slot until it expires.
  do {
    // Try to reserve another buffer. As there are no buffers available it shall fail.
    trx_buffer_identifier buffer_id1(0x1234, buffer_id0.get_harq() + 1);
    ASSERT_FALSE(pool->get_pool().reserve(slot, buffer_id1, pool_config.nof_codeblocks).is_valid());
    ++slot;
    pool->get_pool().run_slot(slot);
  } while (slot.system_slot() < pool_config.expire_timeout_slots + delay);

  // After the first buffer expired, buffer reservation shall not fail.
  trx_buffer_identifier buffer_id2(0x1234, buffer_id0.get_harq() + 2);
  ASSERT_TRUE(pool->get_pool().reserve(slot, buffer_id2, pool_config.nof_codeblocks).is_valid());
}

// Tests that the pool renews buffer expiration if they are locked.
TEST(tx_buffer_pool, buffer_renew_expire)
{
  unsigned expire_timeout_slots = 4;

  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 16;
  pool_config.nof_buffers          = 2;
  pool_config.nof_codeblocks       = 1;
  pool_config.expire_timeout_slots = expire_timeout_slots;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Reserve buffer with all the codeblocks, it shall not fail.
  trx_buffer_identifier buffer_id0(0x1234, 0x3);
  unique_tx_buffer      buffer = pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks);
  ASSERT_TRUE(buffer.is_valid());

  // Advance slots. As the buffer is locked, the expiration shall be renewed.
  slot += expire_timeout_slots;
  pool->get_pool().run_slot(slot);

  // Try to get the same buffer. It must fail as the buffer is locked.
  unique_tx_buffer locked_buffer = pool->get_pool().reserve(slot, buffer_id0, pool_config.nof_codeblocks);
  ASSERT_FALSE(locked_buffer.is_valid());

  // Unlock buffer.
  buffer = unique_tx_buffer();

  // Run for each slot until it expires.
  do {
    // Try to reserve another buffer. As there are no buffers available it shall fail.
    trx_buffer_identifier buffer_id1(0x1234, buffer_id0.get_harq() + 1);
    unique_tx_buffer      invalid_buffer = pool->get_pool().reserve(slot, buffer_id1, pool_config.nof_codeblocks);
    ASSERT_FALSE(invalid_buffer.is_valid());
    ++slot;
    pool->get_pool().run_slot(slot);
  } while (slot.system_slot() < pool_config.expire_timeout_slots + expire_timeout_slots);

  // After the first buffer expired, buffer reservation shall not fail.
  trx_buffer_identifier buffer_id2(0x1234, buffer_id0.get_harq() + 2);
  ASSERT_TRUE(pool->get_pool().reserve(slot, buffer_id2, pool_config.nof_codeblocks).is_valid());
}

// Tests that the pool renews buffer expiration if they are locked.
TEST(tx_buffer_pool, buffer_resize)
{
  static constexpr unsigned nof_codeblocks = 4;

  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 16;
  pool_config.nof_buffers          = 1;
  pool_config.nof_codeblocks       = nof_codeblocks;
  pool_config.expire_timeout_slots = 4;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Reserve buffer with nof_codeblocks - 1 codeblocks, it shall not fail.
  trx_buffer_identifier buffer_id0(0x1234, 0x3);
  unique_tx_buffer      buffer = pool->get_pool().reserve(slot, buffer_id0, nof_codeblocks);
  ASSERT_TRUE(buffer.is_valid());

  // Check the number of codeblock matches.
  ASSERT_EQ(buffer.get().get_nof_codeblocks(), nof_codeblocks);

  // Unlock the buffer.
  buffer = unique_tx_buffer();

  // Reserve the same buffer with more codeblocks.
  buffer = pool->get_pool().reserve(slot, buffer_id0, nof_codeblocks - 1);

  // Check the number of codeblock matches.
  ASSERT_EQ(buffer.get().get_nof_codeblocks(), nof_codeblocks - 1);
}

// Tests buffer soft bits contents persists between retransmissions.
TEST(tx_buffer_pool, buffer_contents)
{
  unsigned nof_cb_x_buffer = 2;
  unsigned cb_size         = 16;
  // Data size cannot be larger than cb_size / 3 (recall that 1/3 is the maximum coding rate).
  unsigned data_size = 5;

  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = cb_size;
  pool_config.nof_buffers          = 4;
  pool_config.nof_codeblocks       = pool_config.nof_buffers * nof_cb_x_buffer;
  pool_config.expire_timeout_slots = 10;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Create as many buffers as the limit is set.
  trx_buffer_identifier buffer_id(0x1234, 0x3);

  // Temporal storage of buffer codeblock information.
  std::vector<span<log_likelihood_ratio>> cb_soft_bits;
  std::vector<bit_buffer>                 cb_data_bits;

  // Note: two buffers with the same identifier cannot be simultaneously in scope.
  {
    // Reserve buffer, it shall not fail.
    unique_tx_buffer rm_buffer = pool->get_pool().reserve(slot, buffer_id, nof_cb_x_buffer);
    ASSERT_TRUE(rm_buffer.is_valid());

    // For each codeblock...
    for (unsigned cb_id = 0; cb_id != nof_cb_x_buffer; ++cb_id) {
      // Get codeblock soft and data bits.
      bit_buffer data_buffer = rm_buffer.get().get_codeblock(cb_id, data_size);

      cb_data_bits.emplace_back(data_buffer);

      // Make sure size matches.
      ASSERT_TRUE(data_buffer.size() == data_size);

      // Write data in codeblock.
      for (unsigned bit_idx = 0; bit_idx != data_size; ++bit_idx) {
        data_buffer.insert(bit_idx & 1U, bit_idx, 1);
      }
    }
  }

  // Reserve buffer, it shall not fail.
  unique_tx_buffer buffer = pool->get_pool().reserve(slot, buffer_id, nof_cb_x_buffer);
  ASSERT_TRUE(buffer.is_valid());

  // For each codeblock...
  for (unsigned cb_id = 0; cb_id != nof_cb_x_buffer; ++cb_id) {
    // Get codeblock soft bits.
    bit_buffer data_buffer0 = cb_data_bits[cb_id];
    bit_buffer data_buffer1 = buffer.get().get_codeblock(cb_id, data_size);

    // Make sure absolute codeblock indexes match.
    ASSERT_EQ(buffer.get().get_absolute_codeblock_id(cb_id), cb_id);

    // Make sure the data pointers match.
    ASSERT_TRUE(data_buffer0.get_buffer().data() == data_buffer1.get_buffer().data());

    // Validate data persists in the codeblock.
    for (unsigned bit_idx = 0; bit_idx != data_size; ++bit_idx) {
      ASSERT_EQ(data_buffer0.extract(bit_idx, 1), bit_idx & 1U);
    }
  }
}

// Tests buffer pool fails to reserve after stopping.
TEST(tx_buffer_pool, reserve_after_stop)
{
  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 1;
  pool_config.nof_buffers          = 4;
  pool_config.nof_codeblocks       = pool_config.nof_buffers;
  pool_config.expire_timeout_slots = 10;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Stop the pool operation.
  pool->stop();

  // Try to reserve a buffer.
  unique_tx_buffer buffer = pool->get_pool().reserve(slot, trx_buffer_identifier(0, 0), pool_config.nof_codeblocks);

  // The buffer must be invalid.
  ASSERT_FALSE(buffer.is_valid());
}

// Tests buffer pool waits to stop.
TEST(tx_buffer_pool, wait_to_stop)
{
  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = 1;
  pool_config.nof_buffers          = 4;
  pool_config.nof_codeblocks       = pool_config.nof_buffers;
  pool_config.expire_timeout_slots = 10;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  // Try to reserve a buffer.
  unique_tx_buffer buffer = pool->get_pool().reserve(slot, trx_buffer_identifier(0, 0), pool_config.nof_codeblocks);

  // Create asynchronous task for unlocking the buffer.
  std::thread async_unlock([&buffer]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    buffer = unique_tx_buffer();
  });

  // Stop the pool operation.
  pool->stop();

  // Make sure the asynchronous thread joined.
  async_unlock.join();

  // The buffer must be invalid.
  ASSERT_FALSE(buffer.is_valid());
}

TEST(tx_buffer_pool, concurrent)
{
  unsigned nof_repetitions     = 100;
  unsigned nof_release_threads = 4;
  unsigned max_nof_buffers     = 16;
  unsigned nof_cb_x_buffer     = 8;
  unsigned cb_size             = 16;

  task_worker_pool<concurrent_queue_policy::lockfree_mpmc> release_worker_pool(
      nof_release_threads, nof_repetitions * max_nof_buffers, "release");

  // Create pool configuration for the test.
  tx_buffer_pool_config pool_config;
  pool_config.max_codeblock_size   = cb_size;
  pool_config.nof_buffers          = max_nof_buffers;
  pool_config.nof_codeblocks       = max_nof_buffers * nof_cb_x_buffer;
  pool_config.expire_timeout_slots = 10;
  pool_config.external_soft_bits   = false;

  // Current slot.
  slot_point slot(0, 0);

  // Create buffer pool.
  std::unique_ptr<tx_buffer_pool_controller> pool = create_tx_buffer_pool(pool_config);
  ASSERT_TRUE(pool);

  for (unsigned i_repetition = 0; i_repetition != nof_repetitions; ++i_repetition) {
    for (unsigned i_buffer = 0; i_buffer != max_nof_buffers; ++i_buffer) {
      // Reserve buffer.
      unique_tx_buffer buffer =
          pool->get_pool().reserve(slot, trx_buffer_identifier(0x1234, i_buffer), nof_cb_x_buffer);

      // The reservation should be successful for the first time.
      ASSERT_TRUE((i_repetition > 0) || buffer.is_valid());

      // Release or unlock buffer asynchronously in the worker pool.
      if (buffer.is_valid()) {
        ASSERT_TRUE(
            release_worker_pool.push_task([buffer2 = std::move(buffer)]() mutable { buffer2 = unique_tx_buffer(); }));
      }
    }

    // Process housekeeping asynchronously.
    pool->get_pool().run_slot(slot++);
  }

  // Wait for tasks to finish.
  release_worker_pool.wait_pending_tasks();

  // Stop workers before destroying them.
  release_worker_pool.stop();
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);

  // Make sure logger is enabled and use /dev/null as sink.
  srslog::set_default_sink(*srslog::create_file_sink("/dev/null"));
  srslog::init();
  srslog::basic_logger& logger = srslog::fetch_basic_logger("PHY", true);
  logger.set_level(srslog::basic_levels::debug);

  int ret = RUN_ALL_TESTS();

  srslog::flush();

  return ret;
}

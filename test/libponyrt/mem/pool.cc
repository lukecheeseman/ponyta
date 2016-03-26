#include <platform.h>

#include <mem/pool.h>

#include <gtest/gtest.h>

typedef char block_t[32];

TEST(Pool, Fifo)
{
  void* p1 = ponyint_pool_alloc(0);
  ponyint_pool_free(0, p1);

  void* p2 = POOL_ALLOC(block_t);
  ASSERT_EQ(p1, p2);
  POOL_FREE(block_t, p2);
}

TEST(Pool, Size)
{
  void* p1 = ponyint_pool_alloc(0);
  ponyint_pool_free(0, p1);

  void* p2 = ponyint_pool_alloc_size(17);
  ASSERT_EQ(p1, p2);
  ponyint_pool_free_size(17, p2);

  void* p3 = ponyint_pool_alloc(0);
  ASSERT_EQ(p2, p3);
  ponyint_pool_free(0, p3);
}

TEST(Pool, ExceedBlock)
{
  void* p[2048];

  for(int i = 0; i < 2048; i++)
  {
    p[i] = POOL_ALLOC(block_t);

    for(int j = 0; j < i; j++)
    {
      ASSERT_NE(p[i], p[j]);
    }
  }

  for(int i = 2047; i >= 0; i--)
    POOL_FREE(block_t, p[i]);

  void* q[2048];

  for(int i = 0; i < 2048; i++)
  {
    q[i] = POOL_ALLOC(block_t);
    ASSERT_EQ(p[i], q[i]);

    for(int j = 0; j < i; j++)
    {
      ASSERT_NE(q[i], q[j]);
    }
  }

  for(int i = 2047; i >= 0; i--)
    POOL_FREE(block_t, q[i]);
}

TEST(Pool, LargeAlloc)
{
  void* p = ponyint_pool_alloc_size(1 << 20);
  ASSERT_TRUE(p != NULL);
  ponyint_pool_free_size(1 << 20, p);
}

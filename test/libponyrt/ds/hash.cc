#include <platform.h>
#include <gtest/gtest.h>

#include <stdlib.h>
#include <stdint.h>

#include <ds/fun.h>
#include <ds/hash.h>

#define INITIAL_SIZE 8
#define BELOW_HALF (INITIAL_SIZE + (2 - 1)) / 2

typedef struct hash_elem_t hash_elem_t;

DECLARE_HASHMAP(testmap, testmap_t, hash_elem_t);

class HashMapTest: public testing::Test
{
  protected:
    testmap_t _map;

    virtual void SetUp();
    virtual void TearDown();

    hash_elem_t* get_element();
    void put_elements(size_t count);

  public:
    static size_t hash_tst(hash_elem_t* p);
    static bool cmp_tst(hash_elem_t* a, hash_elem_t* b);
    static void free_elem(hash_elem_t* p);
    static void free_buckets(size_t size, void* p);
};

DEFINE_HASHMAP(testmap, testmap_t, hash_elem_t, HashMapTest::hash_tst,
  HashMapTest::cmp_tst, malloc, HashMapTest::free_buckets,
  HashMapTest::free_elem);

struct hash_elem_t
{
  size_t key;
  size_t val;
};

void HashMapTest::SetUp()
{
  testmap_init(&_map, 1);
}

void HashMapTest::TearDown()
{
  testmap_destroy(&_map);
}

void HashMapTest::put_elements(size_t count)
{
  hash_elem_t* curr = NULL;

  for(size_t i = 0; i < count; i++)
  {
    curr = get_element();
    curr->key = i;

    testmap_put(&_map, curr);
  }
}

hash_elem_t* HashMapTest::get_element()
{
  return (hash_elem_t*) malloc(sizeof(hash_elem_t));
}

size_t HashMapTest::hash_tst(hash_elem_t* p)
{
  return ponyint_hash_size(p->key);
}

bool HashMapTest::cmp_tst(hash_elem_t* a, hash_elem_t* b)
{
  return a->key == b->key;
}

void HashMapTest::free_elem(hash_elem_t* p)
{
  free(p);
}

void HashMapTest::free_buckets(size_t len, void* p)
{
  (void)len;
  free(p);
}

/** The default size of a map is 0 or at least 8,
 *  i.e. a full cache line of void* on 64-bit systems.
 */
TEST_F(HashMapTest, InitialSizeCacheLine)
{
  ASSERT_EQ((size_t)INITIAL_SIZE, _map.contents.size);
}

/** The size of a list is the number of distinct elements
 *  that have been added to the list.
 */
TEST_F(HashMapTest, HashMapSize)
{
  put_elements(100);

  ASSERT_EQ((size_t)100, testmap_size(&_map));
}

/** Hash maps are resized by (size << 3)
 *  once a size threshold of 0.5 is exceeded.
 */
TEST_F(HashMapTest, Resize)
{
  put_elements(BELOW_HALF);

  ASSERT_EQ((size_t)BELOW_HALF, testmap_size(&_map));
  // the map was not resized yet.
  ASSERT_EQ((size_t)INITIAL_SIZE, _map.contents.size);

  hash_elem_t* curr = get_element();
  curr->key = BELOW_HALF;

  testmap_put(&_map, curr);

  ASSERT_EQ((size_t)BELOW_HALF+1, testmap_size(&_map));
  ASSERT_EQ((size_t)INITIAL_SIZE << 3, _map.contents.size);
}

/** After having put an element with a
 *  some key, it should be possible
 *  to retrieve that element using the key.
 */
TEST_F(HashMapTest, InsertAndRetrieve)
{
  hash_elem_t* e = get_element();
  e->key = 1;
  e->val = 42;

  testmap_put(&_map, e);

  hash_elem_t* n = testmap_get(&_map, e);

  ASSERT_EQ(e->val, n->val);
}

/** Getting an element which is not in the HashMap
 *  should result in NULL.
 */
TEST_F(HashMapTest, TryGetNonExistent)
{
  hash_elem_t* e1 = get_element();
  hash_elem_t* e2 = get_element();

  e1->key = 1;
  e2->key = 2;

  testmap_put(&_map, e1);

  hash_elem_t* n = testmap_get(&_map, e2);

  ASSERT_EQ(NULL, n);
}

/** Replacing elements with equivalent keys
 *  returns the previous one.
 */
TEST_F(HashMapTest, ReplacingElementReturnsReplaced)
{
  hash_elem_t* e1 = get_element();
  hash_elem_t* e2 = get_element();

  e1->key = 1;
  e2->key = 1;

  testmap_put(&_map, e1);

  hash_elem_t* n = testmap_put(&_map, e2);
  ASSERT_EQ(n, e1);

  hash_elem_t* m = testmap_get(&_map, e2);
  ASSERT_EQ(m, e2);
}

/** Deleting an element in a hash map returns it.
 *  The element cannot be retrieved anymore after that.
 *
 *  All other elements remain within the map.
 */
TEST_F(HashMapTest, DeleteElement)
{
  hash_elem_t* e1 = get_element();
  hash_elem_t* e2 = get_element();

  e1->key = 1;
  e2->key = 2;

  testmap_put(&_map, e1);
  testmap_put(&_map, e2);

  size_t l = testmap_size(&_map);

  ASSERT_EQ(l, (size_t)2);

  hash_elem_t* n1 = testmap_remove(&_map, e1);

  l = testmap_size(&_map);

  ASSERT_EQ(n1, e1);
  ASSERT_EQ(l, (size_t)1);

  hash_elem_t* n2 = testmap_get(&_map, e2);

  ASSERT_EQ(n2, e2);
}

/** Iterating over a hash map returns every element
 *  in it.
 */
TEST_F(HashMapTest, MapIterator)
{
  hash_elem_t* curr = NULL;
  size_t expect = 0;

  for(uint32_t i = 0; i < 100; i++)
  {
    expect += i;
    curr = get_element();

    curr->key = i;
    curr->val = i;

    testmap_put(&_map, curr);
  }

  size_t s = HASHMAP_BEGIN;
  size_t l = testmap_size(&_map);
  size_t c = 0; //executions
  size_t e = 0; //sum

  ASSERT_EQ(l, (size_t)100);

  while((curr = testmap_next(&_map, &s)) != NULL)
  {
    c++;
    e += curr->val;
  }

  ASSERT_EQ(e, expect);
  ASSERT_EQ(c, l);
}

/** An element removed by index cannot be retrieved
 *  after being removed.
 */
TEST_F(HashMapTest, RemoveByIndex)
{
  hash_elem_t* curr = NULL;
  put_elements(100);

  size_t i = HASHMAP_BEGIN;
  hash_elem_t* p = NULL;

  while((curr = testmap_next(&_map, &i)) != NULL)
  {
    if(curr->key == 20)
    {
      p = curr;
      break;
    }
  }

  hash_elem_t* n = testmap_removeindex(&_map, i);

  ASSERT_EQ(n, p);
  ASSERT_EQ(NULL, testmap_get(&_map, p));
}

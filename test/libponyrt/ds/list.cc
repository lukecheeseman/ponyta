#include <platform.h>
#include <gtest/gtest.h>

#include <stdlib.h>

#include <ds/list.h>

typedef struct elem_t elem_t;

DECLARE_LIST(testlist, testlist_t, elem_t);

class ListTest: public testing::Test
{
  public:
    static elem_t* times2(elem_t* e, void* arg);
    static bool compare(elem_t* a, elem_t* b);
};

DEFINE_LIST(testlist, testlist_t, elem_t, ListTest::compare, NULL);

struct elem_t
{
  uint32_t val; /* only needed for map_fn test */
};

bool ListTest::compare(elem_t* a, elem_t* b)
{
  return a == b;
}

elem_t* ListTest::times2(elem_t* e, void* arg)
{
  (void)arg;
  e->val = e->val << 1;
  return e;
}

/** Calling ponyint_list_append with NULL as argument
 *  returns a new list with a single element.
 */
TEST_F(ListTest, InitializeList)
{
  elem_t e;

  testlist_t* list = testlist_append(NULL, &e);

  ASSERT_TRUE(list != NULL);
  ASSERT_EQ((size_t)1, testlist_length(list));

  testlist_free(list);
}

/** A call to ponyint_list_length returns the number of elements
 *  until the end of the list is reached.
 */
TEST_F(ListTest, ListLength)
{
  elem_t e1;
  elem_t e2;
  elem_t e3;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  ASSERT_EQ((size_t)3, testlist_length(list));

  testlist_free(list);
}

/** A call to ponyint_list_append appends an element to
 *  the end of the list.
 *
 *  After pushing an element, the resulting
 *  pointer still points to the head of the list.
 *
 *  If the end is reached, testlist_next returns NULL.
 */
TEST_F(ListTest, AppendElement)
{
  elem_t e1;
  elem_t e2;

  testlist_t* list = testlist_append(NULL, &e1);
  testlist_t* n = testlist_append(list, &e2);

  ASSERT_EQ(n, list);
  ASSERT_EQ(&e1, testlist_data(n));

  n = testlist_next(n);

  ASSERT_EQ(&e2, testlist_data(n));

  n = testlist_next(n);

  ASSERT_EQ(NULL, n);

  testlist_free(list);
}

/** A call to ponyint_list_push prepends an element to the list.
 *
 *  The pointer returned points to the new head.
 */
TEST_F(ListTest, PushElement)
{
  elem_t e1;
  elem_t e2;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_push(list, &e2);

  ASSERT_EQ(&e2, testlist_data(list));

  testlist_t* n = testlist_next(list);

  ASSERT_EQ(&e1, testlist_data(n));

  testlist_free(list);
}

/** A call to ponyint_list_pop provides the head of the list.
 *  The second element prior to ponyint_list_pop becomes the new head.
 */
TEST_F(ListTest, ListPop)
{
  elem_t* p = NULL;

  elem_t e1;
  elem_t e2;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);
  list = testlist_pop(list, &p);

  ASSERT_EQ(p, &e1);

  ASSERT_EQ(
    &e2,
    testlist_data(
      testlist_index(list, 0)
    )
  );

  testlist_free(list);
}

/** A call to ponyint_list_index (with index > 0) advances
 *  the list pointer to the element at position
 *  "index" (starting from 0).
 */
TEST_F(ListTest, ListIndexAbsolute)
{
  size_t i = 1;

  elem_t e1;
  elem_t e2;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);

  testlist_t* n = testlist_index(list, i);

  ASSERT_EQ(&e2, testlist_data(n));
  testlist_free(list);
}

/** A call to ponyint_list_index with a negative index
 *  allows to traverse the list relative to its end.
 */
TEST_F(ListTest, ListIndexRelative)
{
  ssize_t i = -2;

  elem_t e1;
  elem_t e2;
  elem_t e3;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  //should advance list to the second-to-last element
  testlist_t* n = testlist_index(list, i);

  ASSERT_EQ(&e2, testlist_data(n));

  testlist_free(list);
}

/** A call to ponyint_list_find returns a matching element according
 *  to the provided compare function.
 *
 *  If an element is not in the list, ponyint_list_find returns NULL.
 */
TEST_F(ListTest, ListFind)
{
  elem_t e1;
  elem_t e2;
  elem_t e3;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);

  ASSERT_EQ(
    &e2,
    testlist_find(list, &e2)
  );

  ASSERT_EQ(
    NULL,
    testlist_find(list, &e3)
  );

  testlist_free(list);
}

/** A call to ponyint_list_findindex returns the position
 *  of an element within the list, -1 otherwise.
 */
TEST_F(ListTest, ListFindIndex)
{
  elem_t e1;
  elem_t e2;
  elem_t e3;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);

  ASSERT_EQ(
    1,
    testlist_findindex(list, &e2)
  );

  ASSERT_EQ(
    -1,
     testlist_findindex(list, &e3)
  );

  testlist_free(list);
}

/** Lists where elements are pair-wise equivalent
 *  (starting from the provided head, according to
 *  the compare function), are equivalent.
 */
TEST_F(ListTest, ListEquals)
{
  testlist_t* a = NULL;
  testlist_t* b = NULL;

  elem_t e1;
  elem_t e2;

  a = testlist_append(a, &e1);
  a = testlist_append(a, &e2);

  b = testlist_append(b, &e1);
  b = testlist_append(b, &e2);

  ASSERT_TRUE(
    testlist_equals(a,b)
  );

  testlist_free(a);
  testlist_free(b);
}

/** Lists where one is a pair-wise prefix of
 *  the other, are subsets.
 */
TEST_F(ListTest, ListSubset)
{
  testlist_t* a = NULL;
  testlist_t* b = NULL;

  elem_t e1;
  elem_t e2;

  a = testlist_append(a, &e1);
  a = testlist_append(a, &e2);
  b = testlist_append(b, &e1);

  ASSERT_TRUE(
    testlist_subset(b, a)
  );

  testlist_free(a);
  testlist_free(b);
}

/** A call to ponyint_list_reverse the returns a pointer
 *  to a new list of reversed order.
 */
TEST_F(ListTest, ListReverse)
{
  elem_t* e = NULL;

  elem_t e1;
  elem_t e2;
  elem_t e3;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  testlist_t* r = testlist_reverse(list);

  r = testlist_pop(r, &e);

  ASSERT_EQ(e, &e3);

  r = testlist_pop(r, &e);

  ASSERT_EQ(e, &e2);

  testlist_pop(r, &e);

  ASSERT_EQ(e, &e1);
}

/** A call to ponyint_list_map invokes the provided map function
 *  for each element within the list.
 */
TEST_F(ListTest, ListMap)
{
  elem_t* c = NULL;

  elem_t e1;
  elem_t e2;
  elem_t e3;

  e1.val = 1;
  e2.val = 2;
  e3.val = 3;

  testlist_t* list = testlist_append(NULL, &e1);
  list = testlist_append(list, &e2);
  list = testlist_append(list, &e3);

  testlist_t* mapped = testlist_map(list, times2, NULL);
  testlist_t* m = mapped;

  for(uint32_t i = 1; i < 3; i++)
  {
    c = testlist_data(m);
    ASSERT_EQ(i << 1, c->val);
    m = testlist_next(m);
  }

  testlist_free(list);
  testlist_free(mapped);
}

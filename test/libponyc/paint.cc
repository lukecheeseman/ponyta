#include <gtest/gtest.h>
#include <platform.h>

#include <reach/paint.h>
#include <ast/stringtab.h>
#include <../libponyrt/mem/pool.h>

#include "util.h"

class PaintTest: public testing::Test
{
protected:
  reachable_types_t* _set;
  reachable_type_t* _current_type;

  virtual void SetUp()
  {
    _set = reach_new();
    _current_type = NULL;
  }

  virtual void TearDown()
  {
    reach_free(_set);
  }

  void add_type(const char* name)
  {
    _current_type = POOL_ALLOC(reachable_type_t);
    memset(_current_type, 0, sizeof(reachable_type_t));
    _current_type->name = stringtab(name);
    _current_type->ast = NULL;
    reachable_method_names_init(&_current_type->methods, 0);
    reachable_type_cache_init(&_current_type->subtypes, 0);
    _current_type->vtable_size = 0;

    reachable_types_put(_set, _current_type);
  }

  void add_method(const char* name)
  {
    reachable_method_name_t* mn = POOL_ALLOC(reachable_method_name_t);
    memset(mn, 0, sizeof(reachable_method_name_t));
    mn->name = stringtab(name);
    reachable_methods_init(&mn->r_methods, 0);

    reachable_method_names_put(&_current_type->methods, mn);

    reachable_method_t* method = POOL_ALLOC(reachable_method_t);
    memset(method, 0, sizeof(reachable_method_t));
    method->name = stringtab(name);
    method->typeargs = NULL;
    method->r_fun = NULL;
    method->vtable_index = (uint32_t)-1;

    reachable_methods_put(&mn->r_methods, method);
  }

  void do_paint()
  {
    paint(_set);
  }

  void check_vtable_size(const char* name, uint32_t min_expected,
    uint32_t max_expected, uint32_t* actual)
  {
    reachable_type_t t;
    t.name = stringtab(name);
    reachable_type_t* type = reachable_types_get(_set, &t);
    ASSERT_NE((void*)NULL, type);

    ASSERT_LE(min_expected, type->vtable_size);
    ASSERT_GE(max_expected, type->vtable_size);

    if(actual != NULL)
      *actual = type->vtable_size;

    // Find max colour
    size_t i = HASHMAP_BEGIN;
    reachable_method_name_t* mn;
    uint32_t max_colour = 0;

    while((mn = reachable_method_names_next(&type->methods, &i)) != NULL)
    {
      reachable_method_t m2;
      memset(&m2, 0, sizeof(reachable_method_t));
      m2.name = mn->name;
      reachable_method_t* method = reachable_methods_get(&mn->r_methods, &m2);

      assert(method != NULL);

      if(method->vtable_index > max_colour)
        max_colour = method->vtable_index;
    }

    ASSERT_EQ(max_colour + 1, type->vtable_size);
  }

  void check_method_colour(const char* name, uint32_t max_expected,
    uint32_t* actual)
  {
    size_t i = HASHMAP_BEGIN;
    reachable_type_t* type;
    uint32_t colour = (uint32_t)-1;

    while((type = reachable_types_next(_set, &i)) != NULL)
    {
      reachable_method_name_t m1;
      m1.name = stringtab(name);
      reachable_method_name_t* mn =
        reachable_method_names_get(&type->methods, &m1);

      if(mn != NULL)
      {
        reachable_method_t m2;
        memset(&m2, 0, sizeof(reachable_method_t));
        m2.name = stringtab(name);
        reachable_method_t* method =
          reachable_methods_get(&mn->r_methods, &m2);

        ASSERT_NE((void*)NULL, method);

        if(colour == (uint32_t)-1)
        {
          colour = method->vtable_index;
        }
        else
        {
          ASSERT_EQ(colour, method->vtable_index);
        }
      }
    }

    ASSERT_GE(max_expected, colour);

    if(actual != NULL)
      *actual = colour;
  }
};


TEST_F(PaintTest, SingleTypeSingleFunction)
{
  add_type("Foo");
  add_method("foo");

  do_paint();

  DO(check_vtable_size("Foo", 1, 1, NULL));
  DO(check_method_colour("foo", 0, NULL));
}


TEST_F(PaintTest, MultiTypeMultiFunction)
{
  add_type("Foo");
  add_method("foo");
  add_method("bar");

  add_type("Bar");
  add_method("bar");
  add_method("foo");

  do_paint();

  uint32_t foo_colour;
  uint32_t bar_colour;

  DO(check_vtable_size("Foo", 2, 2, NULL));
  DO(check_vtable_size("Bar", 2, 2, NULL));

  DO(check_method_colour("foo", 1, &foo_colour));
  DO(check_method_colour("bar", 1, &bar_colour));

  ASSERT_NE(foo_colour, bar_colour);
}


TEST_F(PaintTest, ThreeTypeTwoFunctionForcedSparse)
{
  add_type("A");
  add_method("d");
  add_method("e");

  add_type("B");
  add_method("e");
  add_method("f");

  add_type("C");
  add_method("d");
  add_method("f");

  do_paint();

  uint32_t a_size;
  uint32_t b_size;
  uint32_t c_size;

  DO(check_vtable_size("A", 2, 3, &a_size));
  DO(check_vtable_size("B", 2, 3, &b_size));
  DO(check_vtable_size("C", 2, 3, &c_size));

  ASSERT_TRUE(a_size == 2 || b_size == 2 || c_size == 2);
  ASSERT_TRUE(a_size == 3 || b_size == 3 || c_size == 3);

  uint32_t d_colour;
  uint32_t e_colour;
  uint32_t f_colour;

  DO(check_method_colour("d", 2, &d_colour));
  DO(check_method_colour("e", 2, &e_colour));
  DO(check_method_colour("f", 2, &f_colour));

  ASSERT_NE(d_colour, e_colour);
  ASSERT_NE(d_colour, f_colour);
  ASSERT_NE(e_colour, f_colour);
}


TEST_F(PaintTest, MultiNoCollision)
{
  add_type("T1");
  add_method("m1");
  add_method("m2");

  add_type("T2");
  add_method("m3");
  add_method("m4");

  add_type("T3");
  add_method("m5");
  add_method("m6");

  add_type("T4");
  add_method("m7");
  add_method("m8");

  add_type("T5");
  add_method("m9");

  add_type("T6");
  add_method("m10");

  add_type("T7");
  add_method("m11");

  add_type("T8");
  add_method("m12");

  do_paint();

  DO(check_vtable_size("T1", 2, 2, NULL));
  DO(check_vtable_size("T2", 2, 2, NULL));
  DO(check_vtable_size("T3", 2, 2, NULL));
  DO(check_vtable_size("T4", 2, 2, NULL));
  DO(check_vtable_size("T5", 1, 1, NULL));
  DO(check_vtable_size("T6", 1, 1, NULL));
  DO(check_vtable_size("T7", 1, 1, NULL));
  DO(check_vtable_size("T8", 1, 1, NULL));

  DO(check_method_colour("m1", 1, NULL));
  DO(check_method_colour("m2", 1, NULL));
  DO(check_method_colour("m3", 1, NULL));
  DO(check_method_colour("m4", 1, NULL));
  DO(check_method_colour("m5", 1, NULL));
  DO(check_method_colour("m6", 1, NULL));
  DO(check_method_colour("m7", 1, NULL));
  DO(check_method_colour("m8", 1, NULL));
  DO(check_method_colour("m9", 0, NULL));
  DO(check_method_colour("m10", 0, NULL));
  DO(check_method_colour("m11", 0, NULL));
  DO(check_method_colour("m12", 0, NULL));
}


TEST_F(PaintTest, MultiComplex)
{
  add_type("T1");
  add_method("m1");
  add_method("m2");
  add_method("m3");
  add_method("m4");

  add_type("T2");
  add_method("m2");
  add_method("m5");
  add_method("m6");

  add_type("T3");
  add_method("m3");
  add_method("m5");
  add_method("m7");

  add_type("T4");
  add_method("m4");
  add_method("m5");
  add_method("m6");
  add_method("m7");

  add_type("T5");
  add_method("m1");
  add_method("m2");
  add_method("m5");
  add_method("m6");

  add_type("T6");
  add_method("m6");

  add_type("T7");
  add_method("m1");
  add_method("m2");
  add_method("m3");
  add_method("m4");
  add_method("m7");
  add_method("m8");

  add_type("T8");
  add_method("m3");
  add_method("m7");
  add_method("m8");

  do_paint();

  DO(check_vtable_size("T1", 4, 8, NULL));
  DO(check_vtable_size("T2", 3, 8, NULL));
  DO(check_vtable_size("T3", 3, 8, NULL));
  DO(check_vtable_size("T4", 4, 8, NULL));
  DO(check_vtable_size("T5", 4, 8, NULL));
  DO(check_vtable_size("T6", 1, 8, NULL));
  DO(check_vtable_size("T7", 6, 8, NULL));
  DO(check_vtable_size("T8", 3, 8, NULL));

  uint32_t m1_colour;
  uint32_t m2_colour;
  uint32_t m3_colour;
  uint32_t m4_colour;
  uint32_t m5_colour;
  uint32_t m6_colour;
  uint32_t m7_colour;
  uint32_t m8_colour;

  DO(check_method_colour("m1", 7, &m1_colour));
  DO(check_method_colour("m2", 7, &m2_colour));
  DO(check_method_colour("m3", 7, &m3_colour));
  DO(check_method_colour("m4", 7, &m4_colour));
  DO(check_method_colour("m5", 7, &m5_colour));
  DO(check_method_colour("m6", 7, &m6_colour));
  DO(check_method_colour("m7", 7, &m7_colour));
  DO(check_method_colour("m8", 7, &m8_colour));

  // T1
  ASSERT_NE(m1_colour, m2_colour);
  ASSERT_NE(m1_colour, m3_colour);
  ASSERT_NE(m1_colour, m4_colour);
  ASSERT_NE(m2_colour, m3_colour);
  ASSERT_NE(m2_colour, m4_colour);
  ASSERT_NE(m3_colour, m5_colour);

  // T2
  ASSERT_NE(m2_colour, m5_colour);
  ASSERT_NE(m2_colour, m6_colour);
  ASSERT_NE(m2_colour, m6_colour);

  // T3
  ASSERT_NE(m3_colour, m5_colour);
  ASSERT_NE(m3_colour, m7_colour);
  ASSERT_NE(m5_colour, m7_colour);

  // T4
  ASSERT_NE(m4_colour, m5_colour);
  ASSERT_NE(m4_colour, m6_colour);
  ASSERT_NE(m4_colour, m7_colour);
  ASSERT_NE(m5_colour, m6_colour);
  ASSERT_NE(m5_colour, m7_colour);
  ASSERT_NE(m6_colour, m7_colour);

  // T5
  ASSERT_NE(m1_colour, m2_colour);
  ASSERT_NE(m1_colour, m5_colour);
  ASSERT_NE(m1_colour, m6_colour);
  ASSERT_NE(m2_colour, m5_colour);
  ASSERT_NE(m2_colour, m6_colour);
  ASSERT_NE(m5_colour, m6_colour);

  // T6

  // T7
  ASSERT_NE(m1_colour, m2_colour);
  ASSERT_NE(m1_colour, m3_colour);
  ASSERT_NE(m1_colour, m4_colour);
  ASSERT_NE(m1_colour, m7_colour);
  ASSERT_NE(m1_colour, m8_colour);
  ASSERT_NE(m2_colour, m3_colour);
  ASSERT_NE(m2_colour, m4_colour);
  ASSERT_NE(m2_colour, m7_colour);
  ASSERT_NE(m2_colour, m8_colour);
  ASSERT_NE(m3_colour, m4_colour);
  ASSERT_NE(m3_colour, m7_colour);
  ASSERT_NE(m3_colour, m8_colour);
  ASSERT_NE(m4_colour, m7_colour);
  ASSERT_NE(m4_colour, m8_colour);
  ASSERT_NE(m7_colour, m8_colour);

  // T8
  ASSERT_NE(m3_colour, m7_colour);
  ASSERT_NE(m3_colour, m8_colour);
  ASSERT_NE(m7_colour, m8_colour);
}

#include "gentrace.h"
#include "gencall.h"
#include "gendesc.h"
#include "genname.h"
#include "genprim.h"
#include "../type/cap.h"
#include "../type/matchtype.h"
#include "../type/subtype.h"
#include <assert.h>

/**
 * type: The type of the element being traced.
 *
 * orig: The original top level type being traced.
 *
 * tuple: This begins as NULL. If we trace through a tuple, this becomes a
 * tuple of "don't care" of the right cardinality, with the element of the
 * tuple being tested substituted into the right place. In other words, (A, B)
 * becomes the tuple (A, _) when tracing the first element, and (_, B) when
 * tracing the second element.
 */

// Arranged from most specific to least specific.
typedef enum
{
  TRACE_NONE,
  TRACE_MAYBE,
  TRACE_MACHINE_WORD,
  TRACE_PRIMITIVE,
  TRACE_VAL_KNOWN,
  TRACE_VAL_UNKNOWN,
  TRACE_MUT_KNOWN,
  TRACE_MUT_UNKNOWN,
  TRACE_TAG_KNOWN,
  TRACE_TAG_UNKNOWN,
  TRACE_DYNAMIC,
  TRACE_TUPLE
} trace_t;

static void trace_dynamic(compile_t* c, LLVMValueRef ctx, LLVMValueRef object,
  ast_t* type, ast_t* orig, ast_t* tuple, LLVMBasicBlockRef next_block);

static trace_t trace_type(ast_t* type);

static trace_t trace_union_machine_word(trace_t a)
{
  switch(a)
  {
    case TRACE_MACHINE_WORD:
    case TRACE_PRIMITIVE:
    case TRACE_VAL_KNOWN:
    case TRACE_VAL_UNKNOWN:
      return TRACE_VAL_UNKNOWN;

    case TRACE_MUT_KNOWN:
    case TRACE_MUT_UNKNOWN:
      return TRACE_MUT_UNKNOWN;

    case TRACE_TAG_KNOWN:
    case TRACE_TAG_UNKNOWN:
      return TRACE_TAG_UNKNOWN;

    case TRACE_DYNAMIC:
    case TRACE_TUPLE:
      return TRACE_DYNAMIC;

    default: {}
  }

  assert(0);
  return TRACE_NONE;
}

static trace_t trace_union_primitive(trace_t a)
{
  switch(a)
  {
    case TRACE_PRIMITIVE:
      return TRACE_PRIMITIVE;

    case TRACE_MACHINE_WORD:
    case TRACE_VAL_KNOWN:
    case TRACE_VAL_UNKNOWN:
      return TRACE_VAL_UNKNOWN;

    case TRACE_MUT_KNOWN:
    case TRACE_MUT_UNKNOWN:
      return TRACE_MUT_UNKNOWN;

    case TRACE_TAG_KNOWN:
    case TRACE_TAG_UNKNOWN:
      return TRACE_TAG_UNKNOWN;

    case TRACE_DYNAMIC:
    case TRACE_TUPLE:
      return TRACE_DYNAMIC;

    default: {}
  }

  assert(0);
  return TRACE_NONE;
}

static trace_t trace_union_val(trace_t a)
{
  switch(a)
  {
    case TRACE_MACHINE_WORD:
    case TRACE_PRIMITIVE:
    case TRACE_VAL_KNOWN:
    case TRACE_VAL_UNKNOWN:
      return TRACE_VAL_UNKNOWN;

    case TRACE_MUT_KNOWN:
    case TRACE_MUT_UNKNOWN:
    case TRACE_TAG_KNOWN:
    case TRACE_TAG_UNKNOWN:
    case TRACE_DYNAMIC:
    case TRACE_TUPLE:
      return TRACE_DYNAMIC;

    default: {}
  }

  assert(0);
  return TRACE_NONE;
}

static trace_t trace_union_mut(trace_t a)
{
  switch(a)
  {
    case TRACE_MACHINE_WORD:
    case TRACE_PRIMITIVE:
    case TRACE_MUT_KNOWN:
    case TRACE_MUT_UNKNOWN:
      return TRACE_MUT_UNKNOWN;

    case TRACE_VAL_KNOWN:
    case TRACE_VAL_UNKNOWN:
    case TRACE_TAG_KNOWN:
    case TRACE_TAG_UNKNOWN:
    case TRACE_DYNAMIC:
    case TRACE_TUPLE:
      return TRACE_DYNAMIC;

    default: {}
  }

  assert(0);
  return TRACE_NONE;
}

static trace_t trace_union_tag(trace_t a)
{
  switch(a)
  {
    case TRACE_MACHINE_WORD:
    case TRACE_PRIMITIVE:
    case TRACE_TAG_KNOWN:
    case TRACE_TAG_UNKNOWN:
      return TRACE_TAG_UNKNOWN;

    case TRACE_VAL_KNOWN:
    case TRACE_VAL_UNKNOWN:
    case TRACE_MUT_KNOWN:
    case TRACE_MUT_UNKNOWN:
    case TRACE_DYNAMIC:
    case TRACE_TUPLE:
      return TRACE_DYNAMIC;

    default: {}
  }

  assert(0);
  return TRACE_NONE;
}

static trace_t trace_type_union(ast_t* type)
{
  trace_t trace = TRACE_NONE;

  for(ast_t* child = ast_child(type);
    child != NULL;
    child = ast_sibling(child))
  {
    trace_t t = trace_type(child);

    switch(trace)
    {
      case TRACE_NONE:
        trace = t;
        break;

      case TRACE_MAYBE:
        // Can't be in a union.
        assert(0);
        return TRACE_NONE;

      case TRACE_MACHINE_WORD:
        trace = trace_union_machine_word(t);
        break;

      case TRACE_PRIMITIVE:
        trace = trace_union_primitive(t);
        break;

      case TRACE_MUT_KNOWN:
      case TRACE_MUT_UNKNOWN:
        trace = trace_union_mut(t);
        break;

      case TRACE_VAL_KNOWN:
      case TRACE_VAL_UNKNOWN:
        trace = trace_union_val(t);
        break;

      case TRACE_TAG_KNOWN:
      case TRACE_TAG_UNKNOWN:
        trace = trace_union_tag(t);
        break;

      case TRACE_DYNAMIC:
      case TRACE_TUPLE:
        return TRACE_DYNAMIC;
    }
  }

  return trace;
}

static trace_t trace_type_isect(ast_t* type)
{
  trace_t trace = TRACE_DYNAMIC;

  for(ast_t* child = ast_child(type);
    child != NULL;
    child = ast_sibling(child))
  {
    trace_t t = trace_type(child);

    switch(t)
    {
      case TRACE_NONE:
      case TRACE_MAYBE:
        // Can't be in an isect.
        assert(0);
        return TRACE_NONE;

      case TRACE_PRIMITIVE:
        return TRACE_PRIMITIVE;

      case TRACE_MACHINE_WORD:
      case TRACE_VAL_KNOWN:
      case TRACE_VAL_UNKNOWN:
        trace = TRACE_VAL_UNKNOWN;
        break;

      case TRACE_MUT_KNOWN:
      case TRACE_MUT_UNKNOWN:
        if(trace != TRACE_VAL_UNKNOWN)
          trace = TRACE_MUT_UNKNOWN;
        break;

      case TRACE_TAG_KNOWN:
      case TRACE_TAG_UNKNOWN:
        if((trace != TRACE_MUT_UNKNOWN) && (trace != TRACE_VAL_UNKNOWN))
          trace = TRACE_TAG_UNKNOWN;
        break;

      case TRACE_DYNAMIC:
      case TRACE_TUPLE:
        break;
    }
  }

  return trace;
}

static trace_t trace_type_nominal(ast_t* type)
{
  switch(ast_id((ast_t*)ast_data(type)))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      switch(cap_single(type))
      {
        case TK_VAL:
          return TRACE_VAL_UNKNOWN;

        case TK_TAG:
          return TRACE_TAG_UNKNOWN;

        default: {}
      }

      return TRACE_MUT_UNKNOWN;

    case TK_PRIMITIVE:
    {
      if(is_machine_word(type))
        return TRACE_MACHINE_WORD;

      return TRACE_PRIMITIVE;
    }

    case TK_STRUCT:
    case TK_CLASS:
      if(is_maybe(type))
        return TRACE_MAYBE;

      switch(cap_single(type))
      {
        case TK_VAL:
          return TRACE_VAL_KNOWN;

        case TK_TAG:
          return TRACE_TAG_KNOWN;

        default: {}
      }

      return TRACE_MUT_KNOWN;

    case TK_ACTOR:
      return TRACE_TAG_KNOWN;

    default: {}
  }

  assert(0);
  return TRACE_NONE;
}

static trace_t trace_type(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return trace_type_union(type);

    case TK_ISECTTYPE:
      return trace_type_isect(type);

    case TK_TUPLETYPE:
      return TRACE_TUPLE;

    case TK_NOMINAL:
      return trace_type_nominal(type);

    default: {}
  }

  assert(0);
  return TRACE_DYNAMIC;
}

static void trace_maybe(compile_t* c, LLVMValueRef ctx, LLVMValueRef object,
  ast_t* type)
{
  // Only trace the element if it isn't NULL.
  ast_t* type_args = ast_childidx(type, 2);
  ast_t* elem = ast_child(type_args);

  LLVMValueRef test = LLVMBuildIsNull(c->builder, object, "");
  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  LLVMPositionBuilderAtEnd(c->builder, is_false);
  gentrace(c, ctx, object, elem);
  LLVMBuildBr(c->builder, is_true);

  LLVMPositionBuilderAtEnd(c->builder, is_true);
}

static void trace_known(compile_t* c, LLVMValueRef ctx, LLVMValueRef object,
  ast_t* type, int mutability)
{
  reach_type_t* t = reach_type(c->reach, type, c->opt);

  LLVMValueRef args[4];
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, object, c->object_ptr, "");
  args[2] = LLVMBuildBitCast(c->builder, t->desc, c->descriptor_ptr, "");
  args[3] = LLVMConstInt(c->i32, mutability, false);

  gencall_runtime(c, "pony_traceknown", args, 4, "");
}

static void trace_unknown(compile_t* c, LLVMValueRef ctx, LLVMValueRef object,
  int mutability)
{
  LLVMValueRef args[3];
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, object, c->object_ptr, "");
  args[2] = LLVMConstInt(c->i32, mutability, false);

  gencall_runtime(c, "pony_traceunknown", args, 3, "");
}

static void trace_tuple(compile_t* c, LLVMValueRef ctx, LLVMValueRef value,
  ast_t* type)
{
  int i = 0;

  // We're a tuple, determined statically.
  for(ast_t* child = ast_child(type);
    child != NULL;
    child = ast_sibling(child))
  {
    // Extract each element and trace it.
    LLVMValueRef elem = LLVMBuildExtractValue(c->builder, value, i, "");
    gentrace(c, ctx, elem, child);
    i++;
  }
}

static void trace_dynamic_union_or_isect(compile_t* c, LLVMValueRef ctx,
  LLVMValueRef object, ast_t* type, ast_t* orig, ast_t* tuple,
  LLVMBasicBlockRef next_block)
{
  // Trace as every type. This results in maximal tracing. It also results in
  // all tuple elements eventually being maximally traced.
  for(ast_t* child = ast_child(type);
    child != NULL;
    child = ast_sibling(child))
  {
    trace_dynamic(c, ctx, object, child, orig, tuple, next_block);
  }

  // No type matched. This may be a boxed primitive: trace it here.
  trace_unknown(c, ctx, object, PONY_TRACE_OPAQUE);
}

static void trace_dynamic_tuple(compile_t* c, LLVMValueRef ctx,
  LLVMValueRef ptr, LLVMValueRef desc, ast_t* type, ast_t* orig, ast_t* tuple)
{
  // Build a "don't care" type of our cardinality.
  size_t cardinality = ast_childcount(type);
  ast_t* dontcare = ast_from(type, TK_TUPLETYPE);

  for(size_t i = 0; i < cardinality; i++)
    ast_append(dontcare, ast_from(type, TK_DONTCARE));

  // Replace our type in the tuple type with the "don't care" type.
  bool in_tuple = (tuple != NULL);

  if(in_tuple)
    ast_swap(type, dontcare);
  else
    tuple = dontcare;

  // If the original type is a subtype of the test type, then we are always
  // the correct cardinality. Otherwise, we need to dynamically check
  // cardinality.
  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBasicBlockRef is_false = codegen_block(c, "");

  if(!is_subtype(orig, tuple, NULL, c->opt))
  {
    LLVMValueRef dynamic_count = gendesc_fieldcount(c, desc);
    LLVMValueRef static_count = LLVMConstInt(c->i32, cardinality, false);
    LLVMValueRef test = LLVMBuildICmp(c->builder, LLVMIntEQ, static_count,
      dynamic_count, "");

    // Skip if not the right cardinality.
    LLVMBuildCondBr(c->builder, test, is_true, is_false);
  } else {
    LLVMBuildBr(c->builder, is_true);
  }

  LLVMPositionBuilderAtEnd(c->builder, is_true);

  size_t index = 0;
  ast_t* child = ast_child(type);
  ast_t* dc_child = ast_child(dontcare);

  while(child != NULL)
  {
    switch(trace_type(child))
    {
      case TRACE_MACHINE_WORD:
      case TRACE_PRIMITIVE:
        // Skip this element.
        break;

      case TRACE_MUT_KNOWN:
      case TRACE_MUT_UNKNOWN:
      case TRACE_VAL_KNOWN:
      case TRACE_VAL_UNKNOWN:
      case TRACE_TAG_KNOWN:
      case TRACE_TAG_UNKNOWN:
      case TRACE_DYNAMIC:
      {
        // If we are (A, B), turn (_, _) into (A, _).
        ast_t* swap = ast_dup(child);
        ast_swap(dc_child, swap);

        // Create a next block.
        LLVMBasicBlockRef next_block = codegen_block(c, "");

        // Load the object from the tuple field.
        LLVMValueRef field_info = gendesc_fieldinfo(c, desc, index);
        LLVMValueRef object = gendesc_fieldload(c, ptr, field_info);

        // Trace dynamic, even if the tuple thinks the field isn't dynamic.
        trace_dynamic(c, ctx, object, swap, orig, tuple, next_block);

        // Continue into the next block.
        LLVMBuildBr(c->builder, next_block);
        LLVMPositionBuilderAtEnd(c->builder, next_block);

        // Restore (A, _) to (_, _).
        ast_swap(swap, dc_child);
        ast_free_unattached(swap);
        break;
      }

      case TRACE_TUPLE:
      {
        // If we are (A, B), turn (_, _) into (A, _).
        ast_t* swap = ast_dup(child);
        ast_swap(dc_child, swap);

        // Get a pointer to the unboxed tuple and it's descriptor.
        LLVMValueRef field_info = gendesc_fieldinfo(c, desc, index);
        LLVMValueRef field_ptr = gendesc_fieldptr(c, ptr, field_info);
        LLVMValueRef field_desc = gendesc_fielddesc(c, field_info);

        // Trace the tuple dynamically.
        trace_dynamic_tuple(c, ctx, field_ptr, field_desc, swap, orig, tuple);

        // Restore (A, _) to (_, _).
        ast_swap(swap, dc_child);
        ast_free_unattached(swap);
        break;
      }

      default: {}
    }

    index++;
    child = ast_sibling(child);
    dc_child = ast_sibling(dc_child);
  }

  // Restore the tuple type.
  if(in_tuple)
    ast_swap(dontcare, type);

  ast_free_unattached(dontcare);

  // Continue with other possible tracings.
  LLVMBuildBr(c->builder, is_false);
  LLVMPositionBuilderAtEnd(c->builder, is_false);
}

static void trace_dynamic_nominal(compile_t* c, LLVMValueRef ctx,
  LLVMValueRef object, ast_t* type, ast_t* orig, ast_t* tuple,
  LLVMBasicBlockRef next_block)
{
  // Skip if a primitive.
  ast_t* def = (ast_t*)ast_data(type);

  if(ast_id(def) == TK_PRIMITIVE)
    return;

  // If it's not possible to use match or as to extract this type from the
  // original type, there's no need to trace as this type.
  if(tuple != NULL)
  {
    // We are a tuple element. Our type is in the correct position in the
    // tuple, everything else is TK_DONTCARE.
    if(is_matchtype(orig, tuple, c->opt) != MATCHTYPE_ACCEPT)
      return;
  } else {
    // We aren't a tuple element.
    if(is_matchtype(orig, type, c->opt) != MATCHTYPE_ACCEPT)
      return;
  }

  // We aren't always this type. We need to check dynamically.
  LLVMValueRef desc = gendesc_fetch(c, object);
  LLVMValueRef test = gendesc_isnominal(c, desc, type);

  LLVMBasicBlockRef is_true = codegen_block(c, "");
  LLVMBasicBlockRef is_false = codegen_block(c, "");
  LLVMBuildCondBr(c->builder, test, is_true, is_false);

  // Trace as this type.
  LLVMPositionBuilderAtEnd(c->builder, is_true);
  gentrace(c, ctx, object, type);

  // If we have traced as mut or val, we're done with this element. Otherwise,
  // continue tracing this as if the match had been unsuccessful.
  switch(trace_type(type))
  {
    case TRACE_MUT_KNOWN:
    case TRACE_MUT_UNKNOWN:
    case TRACE_VAL_KNOWN:
    case TRACE_VAL_UNKNOWN:
      LLVMBuildBr(c->builder, next_block);
      break;

    default:
      LLVMBuildBr(c->builder, is_false);
      break;
  }

  // Carry on, whether we have traced or not.
  LLVMPositionBuilderAtEnd(c->builder, is_false);
}

static void trace_dynamic(compile_t* c, LLVMValueRef ctx, LLVMValueRef object,
  ast_t* type, ast_t* orig, ast_t* tuple, LLVMBasicBlockRef next_block)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      trace_dynamic_union_or_isect(c, ctx, object, type, orig, tuple,
        next_block);
      break;

    case TK_TUPLETYPE:
    {
      // This is a boxed tuple. Trace the box, then handle the elements.
      trace_unknown(c, ctx, object, PONY_TRACE_OPAQUE);

      LLVMValueRef desc = gendesc_fetch(c, object);
      LLVMValueRef ptr = gendesc_ptr_to_fields(c, object, desc);
      trace_dynamic_tuple(c, ctx, ptr, desc, type, orig, tuple);
      break;
    }

    case TK_NOMINAL:
      trace_dynamic_nominal(c, ctx, object, type, orig, tuple, next_block);
      break;

    default: {}
  }
}

bool gentrace_needed(ast_t* type)
{
  switch(trace_type(type))
  {
    case TRACE_NONE:
      assert(0);
      return false;

    case TRACE_MACHINE_WORD:
    case TRACE_PRIMITIVE:
      return false;

    case TRACE_TUPLE:
    {
      for(ast_t* child = ast_child(type);
        child != NULL;
        child = ast_sibling(child))
      {
        if(gentrace_needed(child))
          return true;
      }

      return false;
    }

    default:
      break;
  }

  return true;
}

void gentrace_prototype(compile_t* c, reach_type_t* t)
{
  switch(t->underlying)
  {
    case TK_CLASS:
    case TK_ACTOR:
      break;

    default:
      return;
  }

  bool need_trace = false;

  // Vectors and matrices don't have fields but may need tracing
  if(is_vector(t->ast))
  {
    ast_t* typeargs = ast_childidx(t->ast, 2);
    need_trace = gentrace_needed(ast_child(typeargs));
  }
  else
  {
    for(uint32_t i = 0; i < t->field_count; i++)
    {
      if(gentrace_needed(t->fields[i].ast))
      {
        need_trace = true;
        break;
      }
    }
  }

  if(!need_trace)
    return;

  t->trace_fn = codegen_addfun(c, genname_trace(t->name), c->trace_type);
}

void gentrace(compile_t* c, LLVMValueRef ctx, LLVMValueRef value, ast_t* type)
{
  switch(trace_type(type))
  {
    case TRACE_NONE:
      assert(0);
      return;

    case TRACE_MACHINE_WORD:
    case TRACE_PRIMITIVE:
      return;

    case TRACE_MAYBE:
      trace_maybe(c, ctx, value, type);
      return;

    case TRACE_VAL_KNOWN:
      trace_known(c, ctx, value, type, PONY_TRACE_IMMUTABLE);
      return;

    case TRACE_VAL_UNKNOWN:
      trace_unknown(c, ctx, value, PONY_TRACE_IMMUTABLE);
      return;

    case TRACE_MUT_KNOWN:
      trace_known(c, ctx, value, type, PONY_TRACE_MUTABLE);
      return;

    case TRACE_MUT_UNKNOWN:
      trace_unknown(c, ctx, value, PONY_TRACE_MUTABLE);
      return;

    case TRACE_TAG_KNOWN:
      trace_known(c, ctx, value, type, PONY_TRACE_OPAQUE);
      return;

    case TRACE_TAG_UNKNOWN:
      trace_unknown(c, ctx, value, PONY_TRACE_OPAQUE);
      return;

    case TRACE_DYNAMIC:
    {
      LLVMBasicBlockRef next_block = codegen_block(c, "");
      trace_dynamic(c, ctx, value, type, type, NULL, next_block);
      LLVMBuildBr(c->builder, next_block);
      LLVMPositionBuilderAtEnd(c->builder, next_block);
      return;
    }

    case TRACE_TUPLE:
      trace_tuple(c, ctx, value, type);
      return;
  }
}
